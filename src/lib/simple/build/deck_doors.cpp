#include "planbuilder.h"
#include "internal.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace SimpleMode::detail {

AuPoint PlanBuilder::pieceMid(const V2& at, QChar room)
{
	QPoint cell;
	int q;
	if(!QuarterAtPoint(plan, at.x, at.y, cell, q))
		return AuPoint::Invalid();
	int i = pieceIndex(pieces, cell, q);
	if(i < 0 || pieces[i].room != room)
		return AuPoint::Invalid();
	return Ft(pieces[i].mid, PATH_HEIGHT);
}

void PlanBuilder::BuildWindows()
{
	for(const PlacedWindow& pw : windows)
	{
		const DoorPlace& wp = pw.place;
		bool hull = onShell(wp);
		if(pw.kind == KindRailing)
		{
			V2 m(wp.mx, wp.my), u(wp.ux, wp.uy), n(wp.nx, wp.ny);
			const QString style = pw.railing.style.isEmpty() ? DefaultRailingStyle(plan.theme) : pw.railing.style;
			AuScFaceList main, pane;
			RailingMesh(main, pane, m - u * (pw.width / 2), m + u * (pw.width / 2), n, style);
			Paint(main, style == "bars" ? QColor(0x2c, 0x2d, 0x31) : style == "glass" ? QColor(0x9a, 0xa2, 0xaa) : TRIM_PAINT);
			Paint(pane, QColor(0xb0, 0xd0, 0xe0));
			pane.SetOpacity(0.3f);
			AuScMesh mesh;
			mesh.AddFaces(main);
			mesh.AddFaces(pane);
			mesh.SetGroupName(plan.rooms.value(wp.here).name);
			add(mesh, "railing");
			AuScBarrier barrier;
			AuScFaceList faces;
			AddWallBox(faces, m, u, n, -0.05, 0.05, pw.width, 0, HC);
			barrier.AddFaces(faces);
			barrier.SetFaceOpacity(VOID_OPACITY);
			add(barrier, "railing barrier");
			openSpans << OpenSpan{D, m, u, pw.width / 2};
			railingLines << OpenSpan{D, m, u, pw.width / 2};
			continue;
		}
		if(pw.kind != KindWindow)
			continue;
		WindowGeo g{V2(wp.mx, wp.my), V2(wp.ux, wp.uy), V2(wp.nx, wp.ny), pw.width, pw.sill, pw.head, thickness(wp.there),
			hull ? 0 : plan.partition / 2, hull, pw.tech, theme};
		AuScWindow glass;
		AuScFaceList pane;
		WindowGlass(pane, g);
		pane.SetOpacity(pw.tech == 1 ? 0.f : pw.tech == 5 ? 0.15f : 0.25f);
		if(!plan.interiors)
		{
			Paint(pane, QColor(0, 0, 0));
			pane.SetOpacity(1.f);
		}
		glass.AddFaces(pane);
		if(plan.interiors)
			glass.SetGroupName(plan.rooms[wp.here].name);
		if(add(glass, "window"))
			windowCount++;
		for(int side : {-1, 1})
		{
			if(!plan.interiors && side < 0)
				continue;
			WindowMesh wm;
			WindowFrames(wm, g, side);
			PaintWindowMesh(wm, g);
			if(wm.frame.isEmpty() && wm.dark.isEmpty() && wm.shutter.isEmpty() && wm.glow.isEmpty())
				continue;
			AuScMesh mesh;
			for(AuScFaceList* list : {&wm.frame, &wm.dark, &wm.shutter, &wm.glow})
				mesh.AddFaces(*list);
			if(side < 0)
				mesh.SetGroupName(plan.rooms[wp.here].name);
			else if(!hull)
				mesh.SetGroupName(plan.rooms[wp.there].name);
			add(mesh, "window frame");
		}
	}

}

void PlanBuilder::BuildDoors()
{
	pieces = piecesOf(D);
	for(int di = 0; di < doors.size(); di++)
	{
		const PlacedDoor& pd = doors[di];
		const DoorPlace& dp = pd.place;
		const QString hereName = plan.rooms[dp.here].name;
		bool hullDoor = dp.there == '.';
		V2 m(dp.mx, dp.my), u(dp.ux, dp.uy), n(dp.nx, dp.ny);
		double inHere = thickness(dp.there);
		const bool shellDoor = onShell(dp);
		double inThere = shellDoor ? 0 : plan.partition / 2;
		double doorMid = shellDoor ? -plan.wall / 2 : 0;
		auto at = [&](double outward, double z, double aside = 0) { return Ft(m + n * outward + u * aside, z); };

		if(pd.door.opening)
		{
			double spring, top;
			OpeningHeights(plan, pd.door, spring, top);
			const double width = DoorWidth(plan, pd.door);
			const double through = pd.door.style == "full" ? qMax(0.6, width - 2 * (plan.wall + VOID_INSET)) : width;
			if(top - spring > 0.05)
			{
				AuScFaceList arch;
				ArchSpandrels(arch, m, u, n, width, spring, top, -inHere, inThere);
				Paint(arch, hullDoor ? OUTSIDE_TRIM_PAINT : TRIM_PAINT);
				AuScMesh mesh;
				mesh.AddFaces(arch);
				mesh.SetGroupName(hereName);
				add(mesh, "opening arch");
			}
			const double from = -(inHere + VOID_INSET + 0.15), to = hullDoor ? doorMid + VOID_OVERLAP : inThere + VOID_INSET + 0.15;
			if(hullDoor)
				groundSpots << GroundSpot{D, m, u, n, from, through / 2};
			for(int half = 0; half < (hullDoor ? 1 : 2); half++)
			{
				AuScRoomVoid v;
				AuScFaceList faces;
				if(hullDoor)
					AddWallBox(faces, m, u, n, from, to, through, 0, DOOR_HEIGHT);
				else if(half == 0)
					AddWallBox(faces, m, u, n, from, VOID_OVERLAP, through, VF, DOOR_HEIGHT);
				else
					AddWallBox(faces, m, u, n, -VOID_OVERLAP, to, through, VF, DOOR_HEIGHT);
				v.AddFaces(faces);
				v.SetFaceOpacity(VOID_OPACITY);
				v.SetGroupName(half == 0 ? hereName : plan.rooms[dp.there].name);
				v.SetHoldAccess(HOLD_Inaccessible);
				v.SetPersonnelAccess(PERSONNEL_Everyone);
				add(v, "opening void");
			}
			if(hullDoor)
			{
				for(int piece = 0; piece < 2; piece++)
				{
					AuScHullVoid hv;
					AuScFaceList hf;
					if(piece == 0)
						AddWallBox(hf, m, u, n, doorMid - VOID_OVERLAP, 0.02 + VOID_OVERLAP, through, 0, DOOR_HEIGHT);
					else
						AddWallBox(hf, m, u, n, 0.02, HULL_MARGIN + 0.4, through + 0.8, 0, DOOR_HEIGHT);
					hv.AddFaces(hf);
					hv.SetFaceOpacity(VOID_OPACITY);
					add(hv, "outside opening void");
				}
			}
			addPath(at(-0.8, PATH_HEIGHT), at(hullDoor ? HULL_MARGIN + 0.4 : 0.8, PATH_HEIGHT), 0);
			AuPoint hereMid = pieceMid(m - n * 1.0, dp.here);
			if(hereMid != AuPoint::Invalid())
				addPath(hereMid, at(-0.8, PATH_HEIGHT), 0);
			AuPoint thereMid = hullDoor ? AuPoint::Invalid() : pieceMid(m + n * 1.0, dp.there);
			if(hullDoor)
				addPath(at(HULL_MARGIN + 0.4, PATH_HEIGHT), at(2.5, PATH_HEIGHT), 0);
			else if(thereMid != AuPoint::Invalid())
				addPath(at(0.8, PATH_HEIGHT), thereMid, 0);
			DoorCheck check{0, D, at(hullDoor ? -(plan.wall + VOID_INSET) / 2 : 0, 1.0), at(0.4, 1.0), hullDoor,
				at(-1.2, 1.0), at(hullDoor ? doorMid : 1.2, 1.0), at(doorMid, 0), AuPoint(n.x, n.y, 0)};
			check.always = true;
			doorChecks << check;
			openSpans << OpenSpan{D, m, u, width / 2};
			continue;
		}

		V2 L = n.Left();
		const QString& style = doorStyles[di];
		AuScDoor door;
		BuildDoorStates(door, DoorGeo{m, n, L, inHere, inThere, doorMid, H, theme, style, DoorWidth(plan, pd.door)});
		if(!plan.interiors)
		{
			door.SetState(AuScDoor::Closed);
			AuScFaceList shut;
			door.Faces(shut);
			AuScMesh mesh;
			mesh.AddFaces(shut);
			if(add(mesh, "door, shut"))
				doorCount++;
			continue;
		}
		door.SetHullDoor(hullDoor);
		const QChar liftRoom = LiftOf(plan, dp);
		bool automatic = pd.door.automatic && !hullDoor && liftRoom.isNull();
		door.SetDoorProperties(automatic ? Qt::Checked : Qt::Unchecked, pd.door.locked && !hullDoor ? Qt::Checked : Qt::Unchecked,
			pd.door.vehicles ? Qt::Checked : Qt::Unchecked, pd.door.vehicles ? Qt::Checked : Qt::Unchecked);
		V2 switchSide = switchSides[di];
		const QList<int>& markers = switchMarkers[di];
		double aside = DoorWidth(plan, pd.door) / 2 + SWITCH_ASIDE;
		AuPoint hereSwitch = Ft(m + n * (-inHere - SWITCH_OUT) + switchSide * aside, SWITCH_HEIGHT);
		AuPoint thereSwitch = Ft(m + n * (inThere + SWITCH_OUT) + switchSide * aside, SWITCH_HEIGHT);
		const ConsoleSpot* console = consoleOf(pd);
		if(pd.door.operation == "door")
			door.SetDoorOpLocs(at(-inHere - SWITCH_OUT, SWITCH_HEIGHT), at(inThere + SWITCH_OUT, SWITCH_HEIGHT));
		else if(pd.door.operation == "console" && console)
		{
			AuPoint opLoc = Ft(console->at + console->inward * 0.135, 1.25);
			door.SetDoorOpLocs(opLoc, console->room == dp.here ? thereSwitch : hereSwitch);
		}
		else
		{
			if(pd.door.operation == "console")
				log << "  Door at " << pd.door.at.x() << "," << pd.door.at.y() << " on deck " << D
					<< ": its console is not in a room on this deck; it has switches instead.\n";
			door.SetDoorOpLocs(hereSwitch, thereSwitch);
		}
		door.SetGroupName(hereName);
		if(!liftRoom.isNull())
			door.SetStopName(LiftStopName(plan, liftRoom, D));
		quint16 doorId = add(door, "door");
		if(!doorId)
			continue;
		doorCount++;
		for(int side : markers)
		{
			AuScMesh marker;
			AuScFaceList faces;
			double face = side < 0 ? -inHere : inThere;
			AddWallBox(faces, m + switchSide * aside, u, n, face, face + side * 0.04, 0.12, SWITCH_HEIGHT - 0.09,
				SWITCH_HEIGHT + 0.09);
			marker.AddFaces(faces);
			if(side < 0)
				marker.SetGroupName(hereName);
			else if(!hullDoor)
				marker.SetGroupName(plan.rooms[dp.there].name);
			add(marker, "door switch marker");
		}
		if(pd.door.operation == "console" && console)
		{
			AuScMesh mesh;
			AuScFaceList panel, screen, go, stop;
			AddWallBox(panel, console->at, console->u, console->inward, 0, 0.07, 0.36, 0.95, 1.55);
			AddWallBox(screen, console->at, console->u, console->inward, 0.07, 0.076, 0.26, 1.28, 1.46);
			AddWallBox(go, console->at - console->u * 0.07, console->u, console->inward, 0.07, 0.085, 0.06, 1.05, 1.12);
			AddWallBox(stop, console->at + console->u * 0.07, console->u, console->inward, 0.07, 0.085, 0.06, 1.05, 1.12);
			Paint(panel, QColor(0x5a, 0x60, 0x6a));
			Paint(screen, QColor(0x2a, 0x5a, 0x9a));
			Paint(go, QColor(0x3a, 0xb0, 0x4a));
			Paint(stop, QColor(0xc8, 0x28, 0x20));
			for(AuScFaceList* list : {&panel, &screen, &go, &stop})
				mesh.AddFaces(*list);
			mesh.SetGroupName(plan.rooms[console->room].name);
			add(mesh, "door console");
		}
		doorChecks << DoorCheck{doorId, D, at(hullDoor ? -(plan.wall + VOID_INSET) / 2 : 0, 1.0), at(0.4, 1.0), hullDoor,
			at(-1.2, 1.0), at(hullDoor ? doorMid : 1.2, 1.0), at(doorMid, 0), AuPoint(n.x, n.y, 0)};

		{
			AuScRoomVoid v;
			AuScFaceList faces;
			double from = -(inHere + VOID_INSET + 0.15);
			double to = hullDoor ? doorMid + VOID_OVERLAP : inThere + VOID_INSET + 0.15;
			AddWallBox(faces, m, u, n, from, to, DoorWidth(plan, pd.door), 0, DOOR_HEIGHT);
			v.AddFaces(faces);
			v.SetFaceOpacity(VOID_OPACITY);
			v.SetGroupName(hereName);
			v.SetHoldAccess(HOLD_Inaccessible);
			v.SetPersonnelAccess(PERSONNEL_Everyone);
			v.SetAssociatedPart(doorId, AuScDoor::Open);
			add(v, "doorway void");
		}
		if(hullDoor)
		{
			AuScHullVoid v;
			AuScFaceList faces;
			AddWallBox(faces, m, u, n, doorMid - VOID_OVERLAP, HULL_MARGIN + 0.4, DoorWidth(plan, pd.door) + 0.8, 0, DOOR_HEIGHT);
			v.AddFaces(faces);
			v.SetFaceOpacity(VOID_OPACITY);
			v.SetAssociatedPart(doorId, AuScDoor::Open);
			add(v, "outside door void");
		}
		addPath(at(-0.8, PATH_HEIGHT), at(hullDoor ? HULL_MARGIN + 0.4 : 0.8, PATH_HEIGHT), doorId);
		AuPoint hereMid = pieceMid(m - n * 1.0, dp.here);
		if(hereMid != AuPoint::Invalid())
			addPath(hereMid, at(-0.8, PATH_HEIGHT), 0);
		if(hullDoor)
			addPath(at(HULL_MARGIN + 0.4, PATH_HEIGHT), at(2.5, PATH_HEIGHT), 0);
		else
		{
			AuPoint thereMid = pieceMid(m + n * 1.0, dp.there);
			if(thereMid != AuPoint::Invalid())
				addPath(at(0.8, PATH_HEIGHT), thereMid, 0);
		}
	}

}

void PlanBuilder::BuildHatches()
{
	for(const Hatch& h : plan.hatches)
	{
		if(h.deck != D || h.roof)
			continue;
		if(!hatchAt(D, h.cell))
		{
			log << "  Skipping hatch at " << h.cell.x() << "," << h.cell.y() << " on deck " << D
				<< ": it needs a whole room cell there and on the deck below.\n";
			continue;
		}
		const QString upperName = plan.rooms[whole(h.cell)].name;
		const QString lowerName = plan.rooms[plan.At(D - 1, h.cell)].name;
		const double hx = midX(h.cell), hy = midY(h.cell), hh = HATCH_SIZE / 2;

		QString hatchStyle = h.style.isEmpty() ? DefaultHatchStyle(plan.theme) : h.style;
		if(hatchStyle == "slide" && S / 2 < hh + HATCH_SIZE)
		{
			log << "  Hatch at " << h.cell.x() << "," << h.cell.y() << " on deck " << D << ": the cell is too small for it to "
				<< "slide into the floor; it opens up instead.\n";
			hatchStyle = "flap-up";
		}
		V2 fwd, right;
		HatchAxes(h.side, fwd, right);
		const HatchGeo geo{hx, hy, HATCH_SIZE, 0, -plan.wall, theme, hatchStyle, fwd, right};
		AuScDoor hatch;
		BuildHatchStates(hatch, geo);
		hatch.SetHullDoor(false);
		hatch.SetDoorProperties(Qt::Unchecked, Qt::Unchecked, Qt::Unchecked, Qt::Unchecked);
		hatch.SetDoorOpLocs(Ft(hx, hy, -plan.wall - 0.25), Ft(hx, hy, 0.25));
		hatch.SetGroupName(upperName);
		quint16 hatchId = add(hatch, "hatch");
		if(!hatchId)
			continue;
		hatchCount++;
		hatchChecks << HatchCheck{hatchId, D, hx, hy};

		{
			AuScRoomVoid v;
			AuScFaceList faces;
			AddBox(faces, hx - hh + 0.05, hy - hh + 0.05, -H + VC - VOID_OVERLAP, hx + hh - 0.05, hy + hh - 0.05, VF + VOID_OVERLAP);
			v.AddFaces(faces);
			v.SetFaceOpacity(VOID_OPACITY);
			v.SetGroupName(upperName);
			v.SetHoldAccess(HOLD_Inaccessible);
			v.SetPersonnelAccess(PERSONNEL_Everyone);
			v.SetAssociatedPart(hatchId, AuScDoor::Open);
			add(v, "hatch void");
		}
		{
			AuScPathLadder ladder;
			AuPoint body = geo.At(0, HATCH_SIZE / 2 - LADDER_IN - CLIMBER_BACK, 0);
			ladder.MakePath(Ft(body.x, body.y, -H + PATH_HEIGHT), Ft(body.x, body.y, PATH_HEIGHT));
			ladder.SetFacing(AuPoint(fwd.x, fwd.y, 0));
			ladder.SetAssociatedPart(hatchId, AuScDoor::Open);
			add(ladder, "ladder path");
		}
		{
			QString ladderStyle = h.ladder.isEmpty() ? DefaultLadderStyle(plan.theme) : h.ladder;
			AuScFaceList rails, rungs, extra;
			BuildLadder(rails, rungs, extra, geo, -H, 0, 0.9, ladderStyle);
			QColor material = theme == Stone ? QColor(0x3e, 0x3e, 0x44) : theme == Metal ? QColor(0x9a, 0xa2, 0xaa)
				: QColor(0x8a, 0x5a, 0x32);
			QColor rope(0xc8, 0xb0, 0x80), wood(0x8a, 0x5a, 0x32);
			if(theme != Plain)
			{
				Paint(rails, ladderStyle == "rope" ? rope : material);
				Paint(rungs, ladderStyle == "rope" ? wood : material);
				Paint(extra, theme == Metal ? QColor(0xd8, 0xb0, 0x20) : material);
			}
			AuScMesh ladder;
			for(AuScFaceList* list : {&rails, &rungs, &extra})
				if(!list->isEmpty())
					ladder.AddFaces(*list);
			ladder.SetGroupName(lowerName);
			add(ladder, "ladder");
		}
	}

	for(int i = 0; i < plan.hatches.size(); i++)
	{
		const Hatch& h = plan.hatches[i];
		if(h.deck != D || !h.roof)
			continue;
		if(!roofHatchAt(D, h.cell))
		{
			log << "  Skipping hull hatch at " << h.cell.x() << "," << h.cell.y() << " on deck " << D << ": "
				<< RoofHatchProblem(plan, h, i) << "\n";
			continue;
		}
		const QString roomName = plan.rooms[whole(h.cell)].name;
		const double hx = midX(h.cell), hy = midY(h.cell), hh = HATCH_SIZE / 2;
		QString hatchStyle = h.style.isEmpty() ? DefaultHatchStyle(plan.theme) : h.style;
		if(hatchStyle == "slide" && S / 2 < hh + HATCH_SIZE)
		{
			log << "  Hull hatch at " << h.cell.x() << "," << h.cell.y() << " on deck " << D << ": the cell is too small for it "
				<< "to slide into the roof; it opens up instead.\n";
			hatchStyle = "flap-up";
		}
		V2 fwd, right;
		HatchAxes(h.side, fwd, right);
		const HatchGeo geo{hx, hy, HATCH_SIZE, H, HC, theme, hatchStyle, fwd, right};
		AuScDoor hatch;
		BuildHatchStates(hatch, geo);
		hatch.SetHullDoor(true);
		hatch.SetDoorProperties(Qt::Unchecked, Qt::Unchecked, Qt::Unchecked, Qt::Unchecked);
		hatch.SetDoorOpLocs(Ft(hx, hy, HC - 0.25), Ft(hx, hy, H + 0.25));
		hatch.SetGroupName(roomName);
		quint16 hatchId = add(hatch, "hull hatch");
		if(!hatchId)
			continue;
		hatchCount++;
		AuPoint body = geo.At(0, hh - LADDER_IN - CLIMBER_BACK, 0), off = geo.At(0, hh + 0.8, H + PATH_HEIGHT);
		hatchChecks << HatchCheck{hatchId, D, hx, hy, true, body.x, body.y, off.x, off.y};

		{
			AuScRoomVoid v;
			AuScFaceList faces;
			AddBox(faces, hx - hh + 0.05, hy - hh + 0.05, VC - VOID_OVERLAP, hx + hh - 0.05, hy + hh - 0.05, H + 0.3);
			v.AddFaces(faces);
			v.SetFaceOpacity(VOID_OPACITY);
			v.SetGroupName(roomName);
			v.SetHoldAccess(HOLD_Inaccessible);
			v.SetPersonnelAccess(PERSONNEL_Everyone);
			v.SetAssociatedPart(hatchId, AuScDoor::Open);
			add(v, "hull hatch void");
		}
		{
			AuScHullVoid v;
			AuScFaceList faces;
			const double reach = hh + 1.2;
			AuPoint a = geo.At(-hh - 0.3, -hh - 0.3, 0), b = geo.At(hh + 0.3, reach, 0);
			AddBox(faces, qMin(a.x, b.x), qMin(a.y, b.y), H - 0.02, qMax(a.x, b.x), qMax(a.y, b.y), H + 2.2);
			v.AddFaces(faces);
			v.SetFaceOpacity(VOID_OPACITY);
			v.SetAssociatedPart(hatchId, AuScDoor::Open);
			add(v, "hull hatch outside void");
		}
		{
			AuScPathLadder ladder;
			ladder.MakePath(Ft(body.x, body.y, PATH_HEIGHT), Ft(body.x, body.y, H + PATH_HEIGHT));
			ladder.SetFacing(AuPoint(fwd.x, fwd.y, 0));
			ladder.SetAssociatedPart(hatchId, AuScDoor::Open);
			add(ladder, "ladder path");
			addPath(Ft(body.x, body.y, H + PATH_HEIGHT), Ft(off.x, off.y, off.z), hatchId);
		}
		{
			QString ladderStyle = h.ladder.isEmpty() ? DefaultLadderStyle(plan.theme) : h.ladder;
			AuScFaceList rails, rungs, extra;
			BuildLadder(rails, rungs, extra, geo, 0, H, 0.9, ladderStyle);
			QColor material = theme == Stone ? QColor(0x3e, 0x3e, 0x44) : theme == Metal ? QColor(0x9a, 0xa2, 0xaa)
				: QColor(0x8a, 0x5a, 0x32);
			QColor rope(0xc8, 0xb0, 0x80), wood(0x8a, 0x5a, 0x32);
			if(theme != Plain)
			{
				Paint(rails, ladderStyle == "rope" ? rope : material);
				Paint(rungs, ladderStyle == "rope" ? wood : material);
				Paint(extra, theme == Metal ? QColor(0xd8, 0xb0, 0x20) : material);
			}
			AuScMesh ladder;
			for(AuScFaceList* list : {&rails, &rungs, &extra})
				if(!list->isEmpty())
					ladder.AddFaces(*list);
			ladder.SetGroupName(roomName);
			add(ladder, "ladder");
		}
	}

}

}
