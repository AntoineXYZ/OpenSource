#include <aubdreport.h>
#include "internal.h"

#include <memory>
#include <QRegularExpression>

namespace SimpleMode::detail {

int RunSelfChecks(BuildContext& ctx, const QString& outPath, QTextStream& log)
{
	Plan& plan = ctx.plan;
	int& D = ctx.D;
	const double H = ctx.H, VF = ctx.VF;
	const auto& doorChecks = ctx.doorChecks;
	const auto& hatchChecks = ctx.hatchChecks;
	const auto& openSpans = ctx.openSpans;
	const auto& railingLines = ctx.railingLines;
	const auto& groundSpots = ctx.groundSpots;
	const auto& stairChecks = ctx.stairChecks;
	const auto& placedStairs = ctx.placedStairs;
	const int roofPoints = ctx.roofPoints, roofBad = ctx.roofBad;
	QTextStream& roofLog = ctx.roofLog;
	const QString& roofText = ctx.roofText;
	auto up = [&](const AuPoint& p, int deck) { return ctx.up(p, deck); };
	auto whole = [&](const QPoint& p) { return ctx.whole(p); };
	auto voidInset = [&](const QPoint& p, Side s) { return ctx.voidInset(p, s); };
	auto stairCellOn = [&](int deck, const QPoint& p) { return ctx.stairCellOn(deck, p); };
	auto stairSurface = [&](int deck, const V2& p) { return ctx.stairSurface(deck, p); };
	auto piecesOf = [&](int deck) { return ctx.piecesOf(deck); };
	auto outlineOf = [&](int deck, QChar room) -> const QList<QList<Edge>>& { return ctx.outlineOf(deck, room); };

	std::unique_ptr<AuDesign> back(AuDesign::LoadFromFile(outPath, SOH_FILE_TYPE));
	if(!back)
	{
		log << "SELF-CHECK FAILED: the written file does not load.\n";
		return 1;
	}
	int ok = 0, bad = 0;
	const AuScParts& parts = back->Parts();
	for(int d = 0; d < plan.Decks() && plan.interiors; d++)
		for(const Piece& pc : piecesOf(d))
		{
			quint16 vid = parts.RoomVoidContainingLoc(Ft(pc.mid, d * H + 1.0));
			const AuScPart* v = vid ? parts.value(vid) : nullptr;
			const QString& name = plan.rooms[pc.room].name;
			if(v && v->GroupName() == name)
				ok++;
			else
			{
				bad++;
				log << "  Piece at " << pc.cell.x() << "," << pc.cell.y() << (pc.quarter < 0 ? "" : " (a quarter)") << " on deck " << d
					<< " of " << name << ": " << (v ? QString("inside void of %1").arg(v->GroupName()) : QString("not inside any room void"))
					<< "\n";
			}
		}
	if(plan.interiors)
		log << "Self-check: " << ok << " of " << (ok + bad) << " cells and quarters have their middle inside their room's void.\n";

	{
		const QStringList& keys = ThemePalette(plan.theme);
		bool palette = true;
		for(int i = 0; i < SCTEXI_Count && i < keys.size(); i++)
		{
			const TextureKind* k = FindTextureKind(keys[i]);
			palette = palette && back->TexturePaletteId(i) == quint16(k && k->texlib >= 0 ? k->texlib : MaterialTexlib(plan.theme));
		}
		QMap<QString, const Room*> byName;
		for(const Room& r : plan.rooms)
			byName.insert(r.name, &r);
		int faces = 0, facesBad = 0;
		for(AuScParts::const_iterator pi = parts.cbegin(); pi != parts.cend(); pi++)
		{
			const QString type = (*pi)->TypeName();
			const bool room = type == "Room", hull = type == "Hull";
			if(!room && !hull)
				continue;
			const Room* r = room ? byName.value((*pi)->GroupName()) : nullptr;
			if(room && !r)
				continue;
			AuScFaceList list;
			(*pi)->Faces(list);
			for(const AuScFace* f : list)
			{
				const double nz = f->FaceNormal().z;
				const QString surface = room ? (nz >= 0.7 ? "floor" : nz <= -0.7 ? "ceiling" : "walls") : (nz >= 0.7 ? "top" : "outside");
				const QString key = room ? RoomTexture(plan, *r, surface) : BuildingTexture(plan, surface);
				if(room)
				{
					const QColor want = RoomColor(*r, surface), got = f->Paint();
					const bool colourOk = want.isValid() ? got.rgb() == want.rgb()
						&& qAbs(got.alphaF() - RoomColorStrength(*r, surface)) < 0.01 : got.alpha() == 0;
					if(!colourOk)
					{
						faces++;
						if(facesBad++ < 5)
							log << "  " << (*pi)->GroupName() << " face on its " << surface << ": colour " << got.name(QColor::HexArgb) << "\n";
						continue;
					}
				}
				if(f->Paint().alpha() == 255)
					continue;
				faces++;
				const int slot = f->TextureIndex();
				const bool outer = slot == TextureSlot(plan, BuildingTexture(plan, "outside")) || slot == TextureSlot(plan, BuildingTexture(plan, "top"))
					|| slot == TextureSlot(plan, BuildingTexture(plan, "roofing"));
				if(slot != TextureSlot(plan, key) && (room || !outer))
				{
					if(facesBad++ < 5)
						log << "  " << (*pi)->GroupName() << " " << type << " face on its " << surface << ": slot " << f->TextureIndex()
							<< ", not " << key << "'s\n";
				}
			}
		}
		log << "Self-check: the texture palette is the " << plan.theme << " theme's" << (palette ? "" : " - NOT") << "; " << (faces - facesBad)
			<< " of " << faces << " room and hull faces show their texture.\n";
		bad += facesBad + (palette ? 0 : 1);
	}
	if(roofPoints)
	{
		roofLog.flush();
		log << roofText << "Self-check: " << (roofPoints - roofBad) << " of " << roofPoints << " points over the roofs and their eaves "
			<< "are covered once, the roof above the deck's top.\n";
		bad += roofBad;
	}

	if(!plan.interiors)
	{
		int inner = 0, windows = 0, windowsBad = 0;
		for(AuScParts::const_iterator pi = parts.cbegin(); pi != parts.cend(); pi++)
		{
			const AuScPart* part = *pi;
			if(part->IsRoomMesh() || part->IsRoomVoid() || part->IsVoid() || part->IsDoor() || part->IsPath() || part->IsTurboLiftMesh()
					|| (part->Type() >= AuScPart::Part_StationCommand && part->Type() <= AuScPart::Part_ConsoleTransporter))
			{
				if(inner++ < 5)
					log << "  A " << part->TypeName() << " part (" << part->GroupName() << ") in a shell\n";
				continue;
			}
			if(!part->IsWindow())
				continue;
			AuScFaceList faces;
			part->Faces(faces);
			bool black = !faces.isEmpty();
			for(const AuScFace* f : faces)
				black = black && f->Paint().rgb() == qRgb(0, 0, 0) && f->Paint().alpha() == 255 && f->Opacity() >= 0.999f;
			windows++;
			windowsBad += !black;
		}
		log << "Self-check: a shell with no interiors: " << inner << " parts to go into (none wanted); " << (windows - windowsBad)
			<< " of " << windows << " windows and skylights black.\n";
		bad += inner + windowsBad;
		std::unique_ptr<AuBdDesign> preview(dynamic_cast<AuBdDesign*>(back->Clone()));
		if(preview)
		{
			preview->FinalizePreview();
			AuBdReport report;
			preview->ReportDesign(report);
			QStringList msgs;
			report.OkForAssembly(msgs, *preview);
			int errors = 0;
			for(const QString& m : msgs)
				if(m.contains("Error:"))
				{
					QString text = m;
					log << "  The design studio: " << text.remove(QRegularExpression("<[^>]*>")) << "\n";
					errors++;
				}
			log << "Preview: the design studio finds " << errors << " errors in it.\n";
			bad += errors;
		}
		return bad ? 1 : 0;
	}

	AuDoorStateHash closed, open;
	for(const DoorCheck& dc : doorChecks)
	{
		if(dc.always)
			continue;
		closed.insert(dc.id, AuScDoorState(AuScDoor::Closed));
		open.insert(dc.id, AuScDoorState(AuScDoor::Open));
	}
	for(const HatchCheck& hc : hatchChecks)
	{
		closed.insert(hc.id, AuScDoorState(AuScDoor::Closed));
		open.insert(hc.id, AuScDoorState(AuScDoor::Open));
	}
	int doorOk = 0, doorBad = 0;
	for(const DoorCheck& dc : doorChecks)
	{
		AuPoint inGap = up(dc.inGap, dc.deck);
		quint16 whenClosed = parts.RoomVoidContainingLoc(inGap, closed, AuLandingGearStateHash(), AuLiftHash());
		quint16 whenOpen = parts.RoomVoidContainingLoc(inGap, open, AuLandingGearStateHash(), AuLiftHash());
		bool good = dc.always ? whenClosed && whenOpen : !whenClosed && whenOpen;
		QString outsideNote;
		if(dc.hull)
		{
			AuPoint outside = up(dc.outside, dc.deck);
			quint16 outClosed = parts.VoidContainingLoc(outside, closed, AuLandingGearStateHash(), AuLiftHash(), 0);
			quint16 outOpen = parts.VoidContainingLoc(outside, open, AuLandingGearStateHash(), AuLiftHash(), 0);
			good = good && (dc.always ? outClosed && outOpen : !outClosed && outOpen);
			outsideNote = QString(", outside: closed %1 / open %2").arg(outClosed ? "void" : "none").arg(outOpen ? "void" : "none");
		}
		(good ? doorOk : doorBad)++;
		log << "  " << (dc.always ? QString("Opening") : QString("Door %1").arg(dc.id)) << " on deck " << dc.deck
			<< (dc.hull ? " (outside)" : " (interior)") << ": doorway closed "
			<< (whenClosed ? "void" : "none") << " / open " << (whenOpen ? "void" : "none") << outsideNote
			<< (good ? "  ok" : "  WRONG") << "\n";
	}
	for(const HatchCheck& hc : hatchChecks)
	{
		const int lower = hc.roof ? hc.deck + 1 : hc.deck;
		AuPoint inFloor = Ft(hc.x, hc.y, lower * H - plan.wall / 2);
		quint16 whenClosed = parts.RoomVoidContainingLoc(inFloor, closed, AuLandingGearStateHash(), AuLiftHash());
		quint16 whenOpen = parts.RoomVoidContainingLoc(inFloor, open, AuLandingGearStateHash(), AuLiftHash());
		AuMatrix4x4 m;
		bool below = parts.FindLadderToGrab(Ft(hc.x + 0.5, hc.y, (lower - 1) * H + 1.0), m, open, AuLandingGearStateHash(), AuLiftHash());
		bool above = parts.FindLadderToGrab(Ft(hc.x + 0.5, hc.y, lower * H + 1.0), m, open, AuLandingGearStateHash(), AuLiftHash());
		bool whenShut = parts.FindLadderToGrab(Ft(hc.x + 0.5, hc.y, (lower - 1) * H + 1.0), m, closed, AuLandingGearStateHash(),
			AuLiftHash());
		bool good = !whenClosed && whenOpen && below && above && !whenShut;
		(good ? doorOk : doorBad)++;
		log << "  " << (hc.roof ? "Hull hatch " : "Hatch ") << hc.id << (hc.roof ? " in the roof over deck " : " in the floor of deck ")
			<< hc.deck << ": opening closed " << (whenClosed ? "void" : "none")
			<< " / open " << (whenOpen ? "void" : "none") << "; ladder within reach from below " << (below ? "yes" : "no")
			<< ", from above " << (above ? "yes" : "no") << ", while shut " << (whenShut ? "yes" : "no")
			<< (good ? "  ok" : "  WRONG") << "\n";
	}
	log << "Self-check: " << doorOk << " of " << (doorOk + doorBad) << " doorways and hatches open and close correctly (openings "
		<< "always open).\n";
	bad += doorBad;

	int wallOk = 0, wallBad = 0;
	for(int d = 0; d < plan.Decks(); d++)
		for(const Room& room : plan.rooms)
			for(const QList<Edge>& loop : RoomOutlines(plan, d, room.letter))
				for(const Edge& e : loop)
				{
					V2 u = (e.b - e.a).Unit();
					double len = (e.b - e.a).Length();
					int steps = qMax(2, int(len / 0.1));
					for(int i = 0; i <= steps; i++)
						for(double in : {0.0, 0.5})
							for(double z : {1.0, 2.0})
							{
								V2 p = e.a + u * (0.01 + i * (len - 0.02) / steps) + u.Left() * (e.inset * in);
								bool open = false;
								for(const OpenSpan& os : openSpans)
									open = open || (os.deck == d && qAbs(os.u.Cross(p - os.m)) < e.inset + 0.02
										&& qAbs(os.u.Dot(p - os.m)) < os.half + 0.05);
								if(open)
									continue;
								quint16 vid = parts.VoidContainingLoc(Ft(p, d * H + z), closed, AuLandingGearStateHash(), AuLiftHash(), 0);
								if(!vid)
								{
									wallOk++;
									continue;
								}
								if(wallBad++ < 10)
									log << "  Wall of " << room.name << " on deck " << d << " at " << p.x << ", " << p.y << " (" << z
										<< " m up) is inside void " << vid << " (" << parts.value(vid)->GroupName() << ")\n";
							}
				}
	log << "Self-check: " << wallOk << " of " << (wallOk + wallBad) << " points inside the walls are solid with the doors shut.\n";
	bad += wallBad;

	int statesOk = 0, statesBad = 0, animated = 0;
	for(AuScParts::const_iterator pi = parts.cbegin(); pi != parts.cend(); pi++)
	{
		if(!(*pi)->IsDoor())
			continue;
		bool ok = true;
		for(int s : {AuScDoor::Closed, AuScDoor::Open, AuScDoor::StuckClosed, AuScDoor::StuckOpen, AuScDoor::LockedClosed,
				AuScDoor::LockedOpen})
			ok = ok && (*pi)->HasState(s);
		bool frames = true;
		for(int s = AuScDoor::Open10; s <= AuScDoor::Open90; s++)
			frames = frames && (*pi)->HasState(s);
		animated += frames;
		(ok ? statesOk : statesBad)++;
		if(!ok)
			log << "  Door " << pi.key() << " has no mesh for a state it can be in, so it would vanish then\n";
	}
	log << "Self-check: " << statesOk << " of " << (statesOk + statesBad) << " doors and hatches have a mesh for every state they "
		<< "can be in; " << animated << " animate open and shut.\n";
	bad += statesBad;

	std::unique_ptr<AuBdDesign> preview(dynamic_cast<AuBdDesign*>(back->Clone()));
	if(preview)
	{
		preview->FinalizePreview();
		const AuScParts& pparts = preview->Parts();
		AuDoorStateHash states;
		pparts.DoorStates(states);
		AuLiftHash lifts;
		pparts.TurboLifts(lifts);
		QHash<quint16, AuLiftHash> liftAtDoor;
		for(AuScParts::const_iterator pi = pparts.cbegin(); pi != pparts.cend(); pi++)
		{
			const AuScDoor* door = (*pi)->IsDoor() ? dynamic_cast<const AuScDoor*>(*pi) : nullptr;
			if(!door || !door->IsTurboLiftDoor())
				continue;
			AuLiftHash at = lifts;
			if(quint16 liftId = pparts.TurboLiftForDoor(pi.key(), *door))
				at[liftId] = door->StopName();
			liftAtDoor.insert(pi.key(), at);
		}
		auto liftsFor = [&](quint16 doorId) { return liftAtDoor.value(doorId, lifts); };
		auto liftAt = [&](QChar room, int deck, AuLiftHash& at) {
			const QString stop = LiftStopName(plan, room, deck);
			for(AuScParts::const_iterator pi = pparts.cbegin(); pi != pparts.cend(); pi++)
				if((*pi)->IsTurboLiftMesh() && (*pi)->GroupName() == plan.rooms[room].name && (*pi)->StopNames().contains(stop))
				{
					at = lifts;
					at[pi.key()] = stop;
					return true;
				}
			return false;
		};
		int doorsClosed = 0, doorsManual = 0;
		for(AuDoorStateHash::const_iterator si = states.cbegin(); si != states.cend(); si++)
		{
			if(!(*si).IsOpen())
				doorsClosed++;
			if(!(*si).IsAutomaticDoor())
				doorsManual++;
		}
		log << "Preview: " << doorsClosed << " of " << states.size() << " doors and hatches start closed, " << doorsManual
			<< " manual.\n";
		if(doorsClosed != states.size())
			bad++;

		QStringList roomNames;
		pparts.RoomVoidNames(roomNames);
		int lit = 0;
		bool allDefault = true;
		for(const Room& room : plan.rooms)
			allDefault = allDefault && room.lights == "bright" && room.sunlit;
		for(const QString& name : roomNames)
		{
			const Room* given = nullptr;
			for(const Room& room : plan.rooms)
				if(room.name == name)
					given = &room;
			INTERIOR_LIGHT want = given ? LightOf(given->lights) : INTLIGHT_Bright;
			bool sun = given ? given->sunlit : true;
			if(pparts.RoomLight(name) == want && pparts.RoomIsSunlit(name) == sun)
				lit++;
			else
				log << "  " << name << " starts with other lights or sunlight than it was given\n";
		}
		if(allDefault)
			log << "Preview: " << lit << " of " << roomNames.size() << " rooms start lit and sunlit.\n";
		else
			log << "Preview: " << lit << " of " << roomNames.size() << " rooms start with the lights and sunlight they were given.\n";
		if(lit != roomNames.size())
			bad++;

		int stationsOk = 0;
		for(const StationCheck& sc : ctx.stationChecks)
		{
			AuScParts::const_iterator pi = pparts.find(sc.id);
			quint16 rid = pi != pparts.cend() ? pparts.RoomVoidContainingLoc((*pi)->OperatorLoc(), .5) : 0;
			bool good = rid && pparts.value(rid)->GroupName() == sc.room;
			for(const AuPoint& spot : sc.spots)
			{
				quint16 sid = pparts.RoomVoidContainingLoc(spot, .5);
				good = good && sid && pparts.value(sid)->GroupName() == sc.room;
			}
			stationsOk += good;
			if(!good)
				log << "  Station at " << sc.cell.x() << "," << sc.cell.y() << " on deck " << sc.deck << ": the game does not find it (or its pads or patient) in "
					<< sc.room << (rid ? " (it finds it in " + pparts.value(rid)->GroupName() + ")" : QString()) << "\n";
		}
		int reported = 0;
		if(!ctx.stationChecks.isEmpty())
		{
			AuBdReport report;
			preview->ReportDesign(report);
			for(const StationCheck& sc : ctx.stationChecks)
			{
				bool good = false;
				if(report.CommandStations().contains(sc.id))
					good = report.CommandStations()[sc.id].RoomId() != 0;
				else if(report.CrewStations().contains(sc.id))
					good = report.CrewStations()[sc.id].RoomId() != 0;
				else if(report.Transporters().contains(sc.id))
					good = report.Transporters()[sc.id].RoomId() != 0 && report.Transporters()[sc.id].Pads() > 0;
				reported += good;
				if(!good)
					log << "  Station at " << sc.cell.x() << "," << sc.cell.y() << " on deck " << sc.deck << " is not in the design's "
						<< "report as a station in a room\n";
			}
		}
		if(!ctx.stationChecks.isEmpty())
		{
			log << "Preview: " << stationsOk << " of " << ctx.stationChecks.size() << " control stations are in their room, for "
				<< "someone there to use.\n";
			bad += ctx.stationChecks.size() - stationsOk;
			log << "Preview: " << reported << " of " << ctx.stationChecks.size() << " control stations are in the design's report "
				<< "(command, crew or transporter), in their room.\n";
			bad += ctx.stationChecks.size() - reported;
		}

		if(!ctx.fixtureChecks.isEmpty())
		{
			AuBdReport report;
			preview->ReportDesign(report);
			int berths = 0, spots = 0, spotsOk = 0;
			QMap<QString, int> made;
			for(const BuildContext::FixtureCheck& fc : ctx.fixtureChecks)
			{
				if(fc.type == "berth")
				{
					berths++;
					continue;
				}
				if(fc.type != "parking")
					continue;
				made[fc.kind]++;
				spots++;
				AuScParts::const_iterator pi = pparts.find(fc.id);
				if(fc.kind == "spacecraft" || pi == pparts.cend())
				{
					spotsOk += fc.kind == "spacecraft";
					continue;
				}
				AuPoint at = (*pi)->PositionMatrix().Translation();
				quint16 rid = pparts.RoomVoidContainingLoc(at);
				bool good = rid && pparts.value(rid)->GroupName() == fc.room;
				spotsOk += good;
				if(!good)
					log << "  Parking spot at " << fc.cell.x() << "," << fc.cell.y() << " on deck " << fc.deck << ": its middle is not in "
						<< fc.room << "'s void\n";
			}
			bool counted = report.Beds() == berths && report.ParkingSpotsGround() == made.value("ground")
				&& report.ParkingSpotsWater() == made.value("water") && report.ParkingSpotsRocket() == made.value("rocket")
				&& report.ParkingSpotsSpace() == made.value("space") && report.ParkingSpotsSpacecraft() == made.value("spacecraft");
			if(berths || spots)
			{
				log << "Preview: " << berths << " berths and " << spotsOk << " of " << spots << " parking spots where the game finds them; "
					<< (counted ? "the design's report counts them all" : "the design's report counts them differently") << ".\n";
				bad += (spots - spotsOk) + (counted ? 0 : 1);
			}

			int turrets = 0, turretsOk = 0, tables = 0;
			for(const BuildContext::FixtureCheck& fc : ctx.fixtureChecks)
			{
				tables += fc.type == "gametable";
				if(fc.type != "turret")
					continue;
				turrets++;
				QString wrong;
				if(!report.Turrets().contains(fc.id))
					wrong = "not in the report";
				else if(report.Turrets()[fc.id].Module() == CID_Invalid)
					wrong = "no module";
				else
				{
					quint16 rid = report.Turrets()[fc.id].RoomId();
					QString in = rid && pparts.contains(rid) ? pparts.value(rid)->GroupName() : QString();
					if(in != fc.room)
						wrong = QString("worked from %1, not %2").arg(in.isEmpty() ? QString("nowhere") : in)
							.arg(fc.room.isEmpty() ? QString("nowhere") : fc.room);
				}
				if(wrong.isEmpty())
					turretsOk++;
				else
					log << "  Turret at " << fc.cell.x() << "," << fc.cell.y() << " on deck " << fc.deck << ": " << wrong << "\n";
			}
			if(turrets || tables)
			{
				QStringList msgs;
				int studioErrors = 0;
				if(turrets)
				{
					report.OkForAssembly(msgs, *preview);
					for(const QString& m : msgs)
						if(m.contains("urret"))
						{
							QString text = m;
							log << "  The design studio: " << text.remove(QRegularExpression("<[^>]*>")) << "\n";
							studioErrors += m.contains("Error:");
						}
				}
				const bool tablesCounted = report.GameTables().size() == tables;
				log << "Preview: " << turretsOk << " of " << turrets << " turrets are in the design's report, armed, and worked from "
					<< "their gunner's room (or by the crew); " << tables << " game tables, "
					<< (tablesCounted ? "all in the report" : "not all in the report") << ".\n";
				bad += (turrets - turretsOk) + studioErrors + (tablesCounted ? 0 : 1);
			}
		}

		const int steps = 12;
		int points = 0, pointsBad = 0;
		const double nudge = 0.0137;
		AuLiftHash pointLifts = lifts;
		auto testPoint = [&](const V2& p, int deck, QChar room, double expect = 1.0, double locZ = -1e9) {
			for(const OpenSpan& rl : railingLines)
				if(rl.deck == deck && qAbs(rl.u.Cross(p - rl.m)) < 0.08 && qAbs(rl.u.Dot(p - rl.m)) < rl.half + 0.08)
					return;
			AuPoint loc = Ft(p + V2(0.0071, 0.0053), (locZ > -1e8 ? locZ : deck * H + VF + 1.0) + nudge);
			expect += nudge;
			quint16 vid = pparts.RoomVoidContainingLoc(loc, .1, states, AuLandingGearStateHash(), pointLifts);
			AuPoint floorN;
			bool inWell;
			double ht = preview->HeightAboveFloor(loc, AuPoint(0,0,1), floorN, inWell, states, pointLifts) / FeetPerMeter();
			const QString& name = plan.rooms[room].name;
			bool ground = false;
			for(const GroundSpot& g : groundSpots)
				ground = ground || (g.deck == deck && g.n.Dot(p + V2(0.0071, 0.0053) - g.m) >= g.from - 0.01
					&& qAbs(g.u.Dot(p + V2(0.0071, 0.0053) - g.m)) <= g.half + 0.01);
			bool good = vid && pparts.value(vid)->GroupName() == name && (qAbs(ht - expect) < 0.01 || (ground && qAbs(ht - expect - VF) < 0.01));
			points++;
			if(!good)
			{
				if(pointsBad < 5)
					log << "  At (" << p.x << ", " << p.y << ") on deck " << deck << " in " << name << ": "
						<< (vid ? pparts.value(vid)->GroupName() : QString("no void")) << ", floor " << ht << " m below\n";
				pointsBad++;
			}
		};
		auto nearStairEdge = [&](int deck, const V2& p) {
			for(const PlacedStair& ps : placedStairs)
			{
				if(ps.stair.deck != deck)
					continue;
				for(const StairFrame& fr : ps.frames)
				{
					double t = (p - fr.s0).Dot(fr.F), a = qAbs((p - fr.s0).Dot(fr.R));
					if(t > -0.08 && t < fr.len + 0.28 && qAbs(a - fr.w / 2 - 0.05) < 0.08)
						return true;
					if(a < fr.w / 2 + 0.13 && (qAbs(t) < 0.08 || qAbs(t - fr.len) < 0.08 || qAbs(t - fr.len - 0.2) < 0.08))
						return true;
				}
				for(const PlacedStair::Landing& l : ps.landings)
					if(p.x > l.x0 - 0.08 && p.x < l.x1 + 0.08 && p.y > l.y0 - 0.08 && p.y < l.y1 + 0.08
							&& !(p.x > l.x0 + 0.08 && p.x < l.x1 - 0.08 && p.y > l.y0 + 0.08 && p.y < l.y1 - 0.08))
						return true;
			}
			return false;
		};
		auto clearOfWalls = [&](QChar room, const V2& p) {
			const double margin = 0.04;
			const QList<QList<Edge>>& loops = outlineOf(D, room);
			for(const QList<Edge>& loop : loops)
				for(int i = 0; i < loop.size(); i++)
				{
					const Edge& e = loop[i];
					V2 u = (e.b - e.a).Unit();
					double along = u.Dot(p - e.a), across = u.Cross(p - e.a);
					if(along >= 0 && along <= (e.b - e.a).Length() && across < e.inset + VOID_INSET + margin)
						return false;
					const Edge& e2 = loop[(i + 1) % loop.size()];
					V2 u2 = (e2.b - e2.a).Unit();
					if(u.Cross(u2) < -1e-9 && u.Cross(p - e.b) < e.inset + VOID_INSET + margin
							&& u2.Cross(p - e2.a) < e2.inset + VOID_INSET + margin
							&& u.Dot(p - e.b) >= -(e.b - e.a).Length() && u2.Dot(p - e2.a) <= (e2.b - e2.a).Length())
						return false;
				}
			return true;
		};
		for(D = 0; D < plan.Decks(); D++)
			for(int r = 0; r < plan.Rows(); r++)
				for(int c = 0; c < plan.Cols(); c++)
				{
					QPoint p(c, r);
					QChar w = whole(p);
					if(w == '.')
						continue;
					if(IsOpenCell(plan, D, p))
					{
						QChar space = SpaceRoom(plan, D, p);
						if(!plan.rooms.contains(space))
							continue;
						int floorDeck = D;
						while(IsOpenCell(plan, floorDeck, p))
							floorDeck--;
						for(int i = 0; i <= steps; i++)
							for(int j = 0; j <= steps; j++)
							{
								V2 pt(plan.X0(c) + plan.cell * i / steps, plan.Y0(r) + plan.cell * j / steps);
								const int saved = D;
								bool clear = true;
								for(D = saved; D >= floorDeck && clear; D--)
									clear = clearOfWalls(whole(p), pt);
								D = saved;
								if(!clear || nearStairEdge(floorDeck, pt))
									continue;
								double surface = stairSurface(floorDeck, pt);
								double floorZ = floorDeck * H + (surface >= 0 ? surface + VF : VF);
								testPoint(pt, D, space, D * H + VF + 1.0 - floorZ);
							}
						continue;
					}
					if(stairCellOn(D, p))
					{
						for(int i = 0; i <= steps; i++)
							for(int j = 0; j <= steps; j++)
							{
								V2 pt(plan.X0(c) + plan.cell * i / steps, plan.Y0(r) + plan.cell * j / steps);
								if(!clearOfWalls(w, pt) || nearStairEdge(D, pt))
									continue;
								double surface = stairSurface(D, pt);
								testPoint(pt, D, w, 1.0, D * H + (surface >= 0 ? surface + VF : VF) + 1.0);
							}
						continue;
					}
					if(w != '*')
					{
						if(IsLiftRoom(plan, w) && !liftAt(w, D, pointLifts))
							continue;
						double x0 = plan.X0(c) + voidInset(p, West) + 0.05, x1 = plan.X1(c) - voidInset(p, East) - 0.05;
						double y0 = plan.Y0(r) + voidInset(p, South) + 0.05, y1 = plan.Y1(r) - voidInset(p, North) - 0.05;
						for(int i = 0; i <= steps; i++)
							for(int j = 0; j <= steps; j++)
							{
								V2 pt(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * j / steps);
								if(clearOfWalls(w, pt))
									testPoint(pt, D, w);
							}
						pointLifts = lifts;
						continue;
					}
					for(int q = 0; q < 4; q++)
					{
						QChar room = plan.Quarter(D, p, q);
						if(room == '.')
							continue;
						if(IsOpenQuarter(plan, D, p, q))
						{
							QChar space = SpaceRoom(plan, D, p, q);
							if(!plan.rooms.contains(space))
								continue;
							int floorDeck = D;
							while(IsOpenQuarter(plan, floorDeck, p, q))
								floorDeck--;
							QVector<V2> t = QuarterPoly(plan, p, q);
							for(int i = 0; i <= 12; i++)
								for(int j = 0; i + j <= 12; j++)
								{
									double a = i / 12.0, b = j / 12.0, g = 1 - a - b;
									V2 pt = t[0] * a + t[1] * b + t[2] * g;
									const int saved = D;
									bool clear = true;
									for(D = saved; D >= floorDeck && clear; D--)
										clear = clearOfWalls(plan.Quarter(D, p, q), pt);
									D = saved;
									if(clear)
										testPoint(pt, D, space, D * H + VF + 1.0 - (floorDeck * H + VF));
								}
							continue;
						}
						QVector<V2> t = QuarterPoly(plan, p, q);
						for(int i = 0; i <= 12; i++)
							for(int j = 0; i + j <= 12; j++)
							{
								double a = i / 12.0, b = j / 12.0, g = 1 - a - b;
								V2 pt = t[0] * a + t[1] * b + t[2] * g;
								if(clearOfWalls(room, pt))
									testPoint(pt, D, room);
							}
					}
				}
		D = 0;
		log << "Preview: " << (points - pointsBad) << " of " << points << " floor points are in their room, 1 m above the floor.\n";
		bad += pointsBad;

		int airPoints = 0, airBad = 0;
		for(int d = 0; d < plan.Decks(); d++)
			for(const Piece& pc : piecesOf(d))
			{
				if(!HasOpenAirRailing(plan, d, pc.room))
					continue;
				QList<V2> spots{pc.mid};
				if(pc.quarter < 0)
				{
					const int saved = D;
					D = d;
					double x0 = plan.X0(pc.cell.x()) + voidInset(pc.cell, West) + 0.03, x1 = plan.X1(pc.cell.x()) - voidInset(pc.cell, East) - 0.03;
					double y0 = plan.Y0(pc.cell.y()) + voidInset(pc.cell, South) + 0.03, y1 = plan.Y1(pc.cell.y()) - voidInset(pc.cell, North) - 0.03;
					D = saved;
					spots = {pc.mid, V2(x0, y0), V2(x0, y1), V2(x1, y0), V2(x1, y1)};
				}
				for(const V2& p : spots)
					for(double z : {0.1, 1.0, 1.7})
					{
						airPoints++;
						if(preview->IsInsideHull(Ft(p, d * H + VF + z), states, lifts))
							continue;
						if(airBad++ < 5)
							log << "  At (" << p.x << ", " << p.y << ") on deck " << d << ", " << z << " m up, in "
								<< plan.rooms[pc.room].name << ": outside the building\n";
					}
			}
		int underPoints = 0, underBad = 0;
		for(int d = 1; d < plan.Decks(); d++)
			for(int r = 0; r < plan.Rows(); r++)
				for(int c = 0; c < plan.Cols(); c++)
				{
					QPoint cell(c, r);
					QChar w = plan.At(d, cell);
					if(w == '.' || w == '*' || d * H - 0.7 < 1.5)
						continue;
					bool clear = true;
					for(int k = 0; k < d; k++)
						clear = clear && plan.At(k, cell) == '.';
					for(const PlacedStair& ps : placedStairs)
						clear = clear && !(ps.stair.deck < d && ps.layout.cells.contains(cell));
					if(!clear)
						continue;
					for(int i = 0; i <= 4; i++)
						for(int j = 0; j <= 4; j++)
						{
							V2 p(plan.X0(c) + plan.cell * (0.1 + 0.2 * i), plan.Y0(r) + plan.cell * (0.1 + 0.2 * j));
							bool nearIt = false;
							for(int k = 0; k < d && !nearIt; k++)
								for(int dr = -1; dr <= 1 && !nearIt; dr++)
									for(int dc = -1; dc <= 1 && !nearIt; dc++)
									{
										QPoint o(c + dc, r + dr);
										if(plan.At(k, o) == '.')
											continue;
										double ox = qMax(0.0, qMax(plan.X0(o.x()) - p.x, p.x - plan.X1(o.x())));
										double oy = qMax(0.0, qMax(plan.Y0(o.y()) - p.y, p.y - plan.Y1(o.y())));
										nearIt = ox * ox + oy * oy < 0.65 * 0.65;
									}
							for(const PlacedStair& ps : placedStairs)
								for(const QPoint& o : ps.layout.cells)
									if(ps.stair.deck < d && !nearIt)
									{
										double ox = qMax(0.0, qMax(plan.X0(o.x()) - p.x, p.x - plan.X1(o.x())));
										double oy = qMax(0.0, qMax(plan.Y0(o.y()) - p.y, p.y - plan.Y1(o.y())));
										nearIt = ox * ox + oy * oy < 0.95 * 0.95;
									}
							if(nearIt)
								continue;
							for(double z : {0.3, 1.0, 1.8})
							{
								underPoints++;
								if(!preview->IsInsideHull(Ft(p, z), states, lifts))
									continue;
								if(underBad++ < 5)
									log << "  Under " << plan.rooms[w].name << " at (" << p.x << ", " << p.y << "), " << z
										<< " m up: inside the building, so nobody can walk there\n";
							}
						}
				}
		if(underPoints)
		{
			log << "Preview: " << (underPoints - underBad) << " of " << underPoints << " points under balconies, platforms, catwalks "
				"and rooms built out over the ground are outside the building (free to walk under).\n";
			bad += underBad;
		}
		if(airPoints)
		{
			log << "Preview: " << (airPoints - airBad) << " of " << airPoints << " points on balconies, platforms and catwalks "
				"are inside the building.\n";
			bad += airBad;
		}

		int climbPoints = 0, climbBad = 0;
		for(const StairCheck& sc : stairChecks)
		{
			for(const QPair<AuPoint, AuPoint>& f : sc.flights)
				for(int i = 0; i <= 20; i++)
				{
					AuPoint loc = up(f.first + (f.second - f.first) * (i / 20.0), sc.deck) + AuPoint(0, 0, nudge * FeetPerMeter());
					AuPoint floorN;
					bool inWell;
					double ht = preview->HeightAboveFloor(loc, AuPoint(0,0,1), floorN, inWell, states, lifts) / FeetPerMeter();
					climbPoints++;
					if(qAbs(ht - (PATH_HEIGHT - VF + nudge)) > 0.03)
					{
						if(climbBad++ < 5)
							log << "  Stair on deck " << sc.deck << ": at " << loc.x / FeetPerMeter() << ", " << loc.y / FeetPerMeter() << ", "
								<< loc.z / FeetPerMeter() << " the floor is " << ht << " m below\n";
					}
				}
			for(int k = 0; k + 1 < sc.path.size(); k++)
				for(int i = 1; i <= 10; i++)
				{
					AuPoint a = up(sc.path[k] + (sc.path[k + 1] - sc.path[k]) * ((i - 1) / 10.0), sc.deck);
					AuPoint b = up(sc.path[k] + (sc.path[k + 1] - sc.path[k]) * (i / 10.0), sc.deck);
					AuPoint hit, hitN;
					climbPoints++;
					if(preview->IsPathBlocked(a, b, hit, hitN, states, lifts))
					{
						if(climbBad++ < 5)
							log << "  Stair on deck " << sc.deck << ": blocked at " << hit.x / FeetPerMeter() << ", " << hit.y / FeetPerMeter()
								<< ", " << hit.z / FeetPerMeter() << "\n";
					}
				}
		}
		if(!stairChecks.isEmpty())
			log << "Preview: " << (climbPoints - climbBad) << " of " << climbPoints << " points climbing the stairs have the stair "
				<< "under them and nothing in the way.\n";
		bad += climbBad;

		AuDoorStateHash opened(states);
		for(AuDoorStateHash::iterator si = opened.begin(); si != opened.end(); si++)
			(*si).Open();
		int walkPoints = 0, walkBad = 0;
		for(const DoorCheck& dc : doorChecks)
			for(int i = 0; i <= 40; i++)
			{
				AuPoint flat = dc.walkFrom + (dc.walkTo - dc.walkFrom) * (i / 40.);
				AuPoint loc = up(flat, dc.deck) + Ft(0.0071, 0.0053, nudge);
				V2 pm(flat.x / FeetPerMeter(), flat.y / FeetPerMeter());
				double across = ((flat.x - dc.door.x) * dc.outward.x + (flat.y - dc.door.y) * dc.outward.y) / FeetPerMeter();
				QPoint cell;
				int q;
				D = dc.deck;
				if(qAbs(across) > plan.wall + VOID_INSET + 0.05 && QuarterAtPoint(plan, pm.x, pm.y, cell, q)
						&& plan.Quarter(D, cell, q) != '.' && !clearOfWalls(plan.Quarter(D, cell, q), pm))
					continue;
				AuPoint floorN;
				bool inWell;
				double ht = preview->HeightAboveFloor(loc, AuPoint(0,0,1), floorN, inWell, opened, liftsFor(dc.id)) / FeetPerMeter();
				walkPoints++;
				double floorZ = loc.z / FeetPerMeter() - ht - dc.deck * H;
				if(ht <= 0 || (qAbs(floorZ) >= 0.01 && qAbs(floorZ - VF) >= 0.01))
				{
					if(walkBad < 5)
						log << "  Door " << dc.id << " step " << i << ": floor " << ht << " m below\n";
					walkBad++;
				}
			}
		log << "Preview: " << (walkPoints - walkBad) << " of " << walkPoints
			<< " points walking through the open doorways have a floor under them.\n";
		bad += walkBad;

		int entries = 0, entriesBad = 0;
		for(const DoorCheck& dc : doorChecks)
		{
			if(!dc.hull || dc.deck)
				continue;
			for(double h : {0.05, 1.0, 1.6})
			{
				bool wasIn = false, joined = false, blockedAfter = false;
				AuPoint prev;
				for(int i = 0; i <= 63; i++)
				{
					double dist = 3.0 - i * 0.05;
					AuPoint loc = dc.door + dc.outward * (dist * FeetPerMeter());
					loc.z = h * FeetPerMeter();
					bool in = preview->IsInsideHull(loc, opened, liftsFor(dc.id));
					AuPoint hit, hitN;
					bool blocked = i && preview->IsPathBlocked(prev, loc, hit, hitN, opened, liftsFor(dc.id));
					if(i && !wasIn && in && !blocked)
						joined = true;
					if(joined && blocked)
						blockedAfter = true;
					wasIn = in;
					prev = loc;
				}
				entries++;
				if(!joined || blockedAfter)
				{
					entriesBad++;
					log << "  Door " << dc.id << " at " << h << " m up: " << (joined ? "blocked after entering" : "never enters")
						<< "\n";
				}
			}
		}
		log << "Preview: " << (entries - entriesBad) << " of " << entries
			<< " walks in through the outside doors enter the building cleanly.\n";
		bad += entriesBad;

		int ups = 0, upsBad = 0;
		for(const ArrivalCheck& ac : ctx.arrivalChecks)
		{
			V2 R(ac.F.y, -ac.F.x);
			for(double across : {-(ac.half - 0.2), 0.0, ac.half - 0.2})
				for(double h : ac.ground ? std::initializer_list<double>{0.05, 1.0, 1.6} : std::initializer_list<double>{1.0, 1.6})
				{
					bool wasIn = false, joined = false, blockedAfter = false, first = true;
					AuPoint prev;
					for(int i = 0; i <= (ac.ground ? 43 : 60); i++)
					{
						double t = -2.0 + i * 0.05;
						V2 p = ac.edge + ac.F * t + R * across;
						double z = ac.ground ? ac.deck * H + h : ac.deck * H + VF + h;
						if(t < 0 && !ac.ground)
						{
							double surface = stairSurface(ac.deck - 1, p);
							if(surface < 0)
								continue;
							z = (ac.deck - 1) * H + surface + VF + h;
						}
						AuPoint loc = Ft(p, z);
						bool in = preview->IsInsideHull(loc, opened, lifts);
						AuPoint hit, hitN;
						bool blocked = !first && preview->IsPathBlocked(prev, loc, hit, hitN, opened, lifts);
						if(first && in)
							joined = true;
						if(!first && !wasIn && in && !blocked)
							joined = true;
						if(joined && blocked)
							blockedAfter = true;
						wasIn = in;
						prev = loc;
						first = false;
					}
					ups++;
					if(!joined || blockedAfter || !wasIn)
					{
						upsBad++;
						log << "  " << (ac.ground ? "Railing gap" : "Stair") << " onto deck " << ac.deck << " at " << ac.edge.x << ", " << ac.edge.y << ", " << across << " m aside, "
							<< h << " m up: " << (!joined ? "never steps on" : blockedAfter ? "blocked after stepping on" : "ends up outside")
							<< "\n";
					}
				}
		}
		if(ups)
		{
			log << "Preview: " << (ups - upsBad) << " of " << ups << " walks up outside stairs, and in through railing gaps, onto "
				<< "balconies step on cleanly.\n";
			bad += upsBad;
		}

		int climbs = 0, climbsBad = 0;
		for(const HatchCheck& hc : hatchChecks)
		{
			if(!hc.roof)
				continue;
			for(int shut = 0; shut < 2; shut++)
			{
				const AuDoorStateHash& with = shut ? states : opened;
				bool wasIn = false, joined = false, blockedAfter = false, blocked = false;
				AuPoint prev;
				const double top = (hc.deck + 1) * H + 2.0, bottom = hc.deck * H + VF + 1.0;
				const int steps = int((top - bottom) / 0.05);
				for(int i = 0; i <= steps; i++)
				{
					AuPoint loc = Ft(hc.bx + 0.0071, hc.by + 0.0053, top - (top - bottom) * i / steps);
					bool in = preview->IsInsideHull(loc, with, lifts);
					AuPoint hit, hitN;
					bool b = i && preview->IsPathBlocked(prev, loc, hit, hitN, with, lifts);
					blocked = blocked || b;
					if(i && !wasIn && in && !b)
						joined = true;
					if(joined && b)
						blockedAfter = true;
					wasIn = in;
					prev = loc;
				}
				climbs++;
				bool good = shut ? (blocked || !joined) : (joined && !blockedAfter && wasIn);
				if(!good)
				{
					climbsBad++;
					log << "  Hull hatch " << hc.id << (shut ? " shut: gets in through the roof" : !joined ? ": never taken in"
						: blockedAfter ? ": blocked after being taken in" : ": ends up outside") << "\n";
				}
			}
			AuPoint off = Ft(hc.sx, hc.sy, (hc.deck + 1) * H + PATH_HEIGHT), hit = off - Ft(0, 0, 3.0), hitN;
			quint16 hitPart = 0;
			double ht = preview->Sight(off, hit, hitPart, opened, lifts, hitN) ? (off.z - hit.z) / FeetPerMeter() : -1;
			bool in = preview->IsInsideHull(off, opened, lifts);
			climbs++;
			if(qAbs(ht - PATH_HEIGHT) > 0.05 || in)
			{
				climbsBad++;
				log << "  Hull hatch " << hc.id << ": stepping off onto the roof at " << hc.sx << ", " << hc.sy << ", the floor is " << ht << " m below"
					<< (in ? ", inside the hull" : "") << "\n";
			}
		}
		if(climbs)
		{
			log << "Preview: " << (climbs - climbsBad) << " of " << climbs << " climbs through hull hatches, and steps off them onto "
				<< "the roof, go right.\n";
			bad += climbsBad;
		}

		int liftStops = 0, liftStopsOk = 0;
		AuBdReport liftReport;
		bool reportMade = false;
		for(const Room& room : plan.rooms)
		{
			if(!IsLiftRoom(plan, room.letter))
				continue;
			QList<int> stopDecks;
			for(int d : LiftDecks(plan, room.letter))
				for(const Door& door : plan.doors)
				{
					DoorPlace dp = door.deck == d ? PlaceDoor(plan, door) : DoorPlace();
					if(dp.ok && LiftOf(plan, dp) == room.letter)
					{
						stopDecks << d;
						break;
					}
				}
			if(stopDecks.isEmpty())
				continue;
			if(!reportMade)
			{
				preview->ReportDesign(liftReport);
				reportMade = true;
			}
			quint16 liftId = 0;
			for(AuScParts::const_iterator pi = pparts.cbegin(); pi != pparts.cend(); pi++)
				if((*pi)->IsTurboLiftMesh() && (*pi)->GroupName() == room.name)
					liftId = pi.key();
			const AuScPart* liftPart = liftId ? pparts.value(liftId) : nullptr;
			V2 mid(0, 0);
			int cells = 0;
			for(int r = 0; r < plan.Rows(); r++)
				for(int c = 0; c < plan.Cols(); c++)
					if(plan.At(stopDecks.first(), QPoint(c, r)) == room.letter)
					{
						mid = mid + V2((plan.X0(c) + plan.X1(c)) / 2, (plan.Y0(r) + plan.Y1(r)) / 2);
						cells++;
					}
			mid = mid * (1.0 / qMax(1, cells));
			for(int k = 0; k < stopDecks.size(); k++)
			{
				const int d = stopDecks[k];
				const QString stop = LiftStopName(plan, room.letter, d);
				liftStops++;
				QStringList wrong;
				quint16 doorId = liftId ? liftReport.TurboLiftStopDoor(liftId, stop) : 0;
				if(!liftId)
					wrong << "no lift";
				else if(!liftReport.TurboLifts().contains(liftId))
					wrong << "not in the report";
				else if(!doorId)
					wrong << "no door in the report";
				else if(!states.IsTurboLiftDoor(doorId) || states.DoorAssociatedPart(doorId) != liftId)
					wrong << "its door is not the lift's";
				if(liftId)
				{
					AuLiftHash here = lifts, away = lifts;
					here[liftId] = stop;
					away[liftId] = LiftStopName(plan, room.letter, stopDecks[(k + 1) % stopDecks.size()]);
					AuPoint loc = Ft(mid + V2(0.0071, 0.0053), d * H + VF + 1.0 + nudge);
					quint16 vid = pparts.RoomVoidContainingLoc(loc, .1, states, AuLandingGearStateHash(), here);
					AuPoint floorN;
					bool inWell;
					double ht = preview->HeightAboveFloor(loc, AuPoint(0,0,1), floorN, inWell, states, here) / FeetPerMeter();
					if(vid != liftId)
						wrong << QString("in the car, void %1").arg(vid);
					else if(qAbs(ht - 1.0 - nudge) > 0.01)
						wrong << QString("in the car, the floor %1 m below").arg(ht);
					if(stopDecks.size() > 1 && pparts.RoomVoidContainingLoc(loc, .1, states, AuLandingGearStateHash(), away))
						wrong << "a void in the shaft with the car away";
				}
				if(wrong.isEmpty())
					liftStopsOk++;
				else
					log << "  " << room.name << ", stop " << stop << ": " << wrong.join("; ") << "\n";
			}
			if(liftPart && stopDecks.size() > 1 && liftPart->PathSegments() < stopDecks.size() - 1)
			{
				liftStops++;
				log << "  " << room.name << ": " << liftPart->PathSegments() << " paths between its " << stopDecks.size() << " stops\n";
			}
		}
		if(liftStops)
		{
			QStringList msgs;
			liftReport.OkForAssembly(msgs, *preview);
			for(const QString& m : msgs)
				if(m.contains("urbo lift") || m.contains("Lift"))
				{
					QString text = m;
					log << "  The design studio: " << text.remove(QRegularExpression("<[^>]*>")) << "\n";
					liftStops += m.contains("Error:");
				}
			log << "Preview: " << liftStopsOk << " of " << liftStops << " turbo lift stops have their door and the car's room, "
				<< "in the design's report.\n";
			bad += liftStops - liftStopsOk;
		}
	}
	return bad ? 1 : 0;
}

}
