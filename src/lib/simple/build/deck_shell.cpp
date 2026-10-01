#include "planbuilder.h"
#include "internal.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace SimpleMode::detail {

void PlanBuilder::BuildHull()
{
	for(int r = 0; r < plan.Rows(); r++)
		for(int c = 0; c < plan.Cols(); c++)
		{
			QPoint cell(c, r);
			if(whole(cell) == '.')
				continue;
			auto hulled = [&](int q) { return plan.Quarter(D, cell, q) != '.' && !IsOpenAirQuarter(plan, D, cell, q); };
			auto skyAbove = [&](int q) {
				return plan.Quarter(D + 1, cell, q) == '.' || (!plan.interiors && IsOpenAirQuarter(plan, D + 1, cell, q));
			};
			AuScFaceList faces;
			bool allFloor = true, allRoof = true;
			for(int q = 0; q < 4; q++)
			{
				bool here = hulled(q);
				allFloor = allFloor && here && (plan.Quarter(D - 1, cell, q) == '.' || IsOpenAirQuarter(plan, D - 1, cell, q))
					&& !IsOpenQuarter(plan, D, cell, q);
				allRoof = allRoof && here && skyAbove(q);
			}
			double x0 = plan.X0(c), x1 = plan.X1(c), y0 = plan.Y0(r), y1 = plan.Y1(r);
			if(allFloor)
				AddQuad(faces, Ft(x0,y0,0), Ft(x0,y1,0), Ft(x1,y1,0), Ft(x1,y0,0), AuPoint(0,0,-1));
			const bool roofHatch = allRoof && roofHatchAt(D, cell);
			if(allRoof && (SkylightAt(plan, D, cell) || roofHatch))
			{
				double hx0, hy0, hx1, hy1;
				if(roofHatch)
				{
					const double hh = HATCH_SIZE / 2;
					hx0 = midX(cell) - hh, hx1 = midX(cell) + hh, hy0 = midY(cell) - hh, hy1 = midY(cell) + hh;
				}
				else
					SkylightHole(plan, cell, hx0, hy0, hx1, hy1);
				auto quad = [&](double a0, double b0, double a1, double b1) {
					AddQuad(faces, Ft(a0,b0,H), Ft(a0,b1,H), Ft(a1,b1,H), Ft(a1,b0,H), AuPoint(0,0,1));
				};
				quad(x0, y0, x1, hy0);
				quad(x0, hy1, x1, y1);
				quad(x0, hy0, hx0, hy1);
				quad(hx1, hy0, x1, hy1);
			}
			else if(allRoof)
				AddQuad(faces, Ft(x0,y0,H), Ft(x0,y1,H), Ft(x1,y1,H), Ft(x1,y0,H), AuPoint(0,0,1));
			for(int q = 0; q < 4; q++)
			{
				if(!hulled(q))
					continue;
				QVector<V2> t = QuarterPoly(plan, cell, q);
				if(!allFloor && (plan.Quarter(D - 1, cell, q) == '.' || IsOpenAirQuarter(plan, D - 1, cell, q))
						&& !IsOpenQuarter(plan, D, cell, q))
					AddTri(faces, Ft(t[0], 0), Ft(t[1], 0), Ft(t[2], 0), AuPoint(0,0,-1));
				if(!allRoof && skyAbove(q))
					AddTri(faces, Ft(t[0], H), Ft(t[1], H), Ft(t[2], H), AuPoint(0,0,1));
				for(int k = 0; k < 3; k++)
				{
					QPoint oc;
					int oq;
					QuarterAcross(cell, q, k, oc, oq);
					if(plan.Quarter(D, oc, oq) != '.' && !IsOpenAirQuarter(plan, D, oc, oq))
						continue;
					V2 a = t[k], b = t[(k + 1) % 3];
					V2 out = (b - a).Unit().Left() * -1;
					AddWall(faces, a, b, 0, H, out, openings(a, b, a, b, '.', true), windowHoles(a, b, a, b, '.', true));
				}
			}
			if(faces.isEmpty())
				continue;
			textureBySide(faces, BuildingTexture(plan, "top"), BuildingTexture(plan, "outside"), BuildingTexture(plan, "outside"),
				BuildingTextureScale(plan, "roofing"), BuildingTextureScale(plan, "outside"), BuildingTextureScale(plan, "outside"));
			AuScHull hull;
			hull.AddFaces(faces);
			add(hull, QString("hull %1,%2 deck %3").arg(c).arg(r).arg(D));
		}

}

void PlanBuilder::BuildRooms()
{
	for(const Room& room : plan.rooms)
	{
		QList<QList<Edge>> loops = RoomOutlines(plan, D, room.letter);
		if(loops.isEmpty())
			continue;
		AuScFaceList faces;
		AuScFaceList trim;
		AuScFaceList panels;
		const Room* spaceOf = &room;
		for(int r = 0; r < plan.Rows() && spaceOf == &room; r++)
			for(int c = 0; c < plan.Cols() && spaceOf == &room; c++)
				for(int q = 0; q < 4 && spaceOf == &room; q++)
					if(plan.Quarter(D, QPoint(c, r), q) == room.letter && room.type == "opentobelow"
							&& IsOpenQuarter(plan, D, QPoint(c, r), q))
						spaceOf = &plan.rooms[SpaceRoom(plan, D, QPoint(c, r), q)];
		const Room& space = *spaceOf;
		const bool air = IsOpenAirRoom(plan, room.letter);
		const bool lift = IsLiftRoom(plan, room.letter);
		const QList<VoidBox> nearR = air ? nearRects(room.letter) : QList<VoidBox>();
		AuScFaceList farFaces;
		const QString design = RoomDesign(space);
		const int trims = air || lift || design == "none" ? 0 : TrimsOf(design);
		auto openAlong = [&](const V2& a, const V2& u, double len, const V2& inward) {
			QList<QPair<double, double>> list;
			const double step = plan.cell / 4;
			for(double s = 0; s < len - 1e-6; s += step)
			{
				double s1 = qMin(len, s + step);
				V2 p = a + u * ((s + s1) / 2) + inward * 0.15;
				QPoint cell;
				int q;
				if(!QuarterAtPoint(plan, p.x, p.y, cell, q) || !IsOpenQuarter(plan, D, cell, q))
					continue;
				if(!list.isEmpty() && qAbs(list.last().second - s) < 1e-6)
					list.last().second = s1;
				else
					list << qMakePair(s, s1);
			}
			return list;
		};
		const double hh = HATCH_SIZE / 2;
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint cell(c, r);
				if(whole(cell) == room.letter)
				{
					double x0 = plan.X0(c), x1 = plan.X1(c), y0 = plan.Y0(r), y1 = plan.Y1(r), hx = midX(cell), hy = midY(cell);
					auto flat = [&](double z, const AuPoint& n, double half) {
						if(half <= 0)
						{
							AddQuad(faces, Ft(x0,y0,z), Ft(x0,y1,z), Ft(x1,y1,z), Ft(x1,y0,z), n);
							return;
						}
						auto quad = [&](double a0, double b0, double a1, double b1) {
							AddQuad(faces, Ft(a0,b0,z), Ft(a0,b1,z), Ft(a1,b1,z), Ft(a1,b0,z), n);
						};
						quad(x0, y0, x1, hy - half);
						quad(x0, hy + half, x1, y1);
						quad(x0, hy - half, hx - half, hy + half);
						quad(hx + half, hy - half, x1, hy + half);
					};
					const bool skylight = SkylightAt(plan, D, cell) != nullptr;
					const double sh = plan.cell / 4;
					if(air)
					{
						if(hatchAt(D, cell))
							flat(0, AuPoint(0,0,1), hh);
						else
						{
							double fi = openFloorInset(room.letter, cell);
							auto narrowed = [&](Side s) { return IsOpenEdge(plan, room.letter, facing(cell, s)) && !stairArrivingAt(cell, s); };
							double fx0 = x0 + (narrowed(West) ? fi : 0);
							double fx1 = x1 - (narrowed(East) ? fi : 0);
							double fy0 = y0 + (narrowed(South) ? fi : 0);
							double fy1 = y1 - (narrowed(North) ? fi : 0);
							for(const QPair<VoidBox, bool>& pc : splitNear(VoidBox{fx0, fy0, fx1, fy1}, nearR))
							{
								const VoidBox& b = pc.first;
								AddQuad(pc.second ? faces : farFaces, Ft(b.x0,b.y0,0), Ft(b.x0,b.y1,0), Ft(b.x1,b.y1,0), Ft(b.x1,b.y0,0),
									AuPoint(0,0,1));
							}
						}
						for(int q = 0; q < 4; q++)
							if(coveredAbove(cell, q))
							{
								QVector<V2> t = QuarterPoly(plan, cell, q);
								AddTri(faces, Ft(t[0], HC), Ft(t[1], HC), Ft(t[2], HC), AuPoint(0,0,-1));
							}
						continue;
					}
					const bool shaftBelow = lift && D > 0 && plan.At(D - 1, cell) == room.letter;
					const bool shaftAbove = lift && D + 1 < plan.Decks() && plan.At(D + 1, cell) == room.letter;
					if(!IsOpenCell(plan, D, cell) && !shaftBelow)
						flat(0, AuPoint(0,0,1), hatchAt(D, cell) ? hh : 0);
					bool someOpenAbove = false;
					for(int q = 0; q < 4; q++)
						someOpenAbove = someOpenAbove || IsOpenQuarter(plan, D + 1, cell, q);
					const bool roofHatch = roofHatchAt(D, cell);
					if(!someOpenAbove && !shaftAbove)
						flat(HC, AuPoint(0,0,-1), hatchAt(D + 1, cell) || roofHatch ? hh : skylight ? sh : 0);
					if(roofHatch)
					{
						AddQuad(faces, Ft(hx-hh,hy-hh,HC), Ft(hx-hh,hy-hh,H), Ft(hx-hh,hy+hh,H), Ft(hx-hh,hy+hh,HC), AuPoint(1,0,0));
						AddQuad(faces, Ft(hx+hh,hy-hh,HC), Ft(hx+hh,hy-hh,H), Ft(hx+hh,hy+hh,H), Ft(hx+hh,hy+hh,HC), AuPoint(-1,0,0));
						AddQuad(faces, Ft(hx-hh,hy-hh,HC), Ft(hx-hh,hy-hh,H), Ft(hx+hh,hy-hh,H), Ft(hx+hh,hy-hh,HC), AuPoint(0,1,0));
						AddQuad(faces, Ft(hx-hh,hy+hh,HC), Ft(hx-hh,hy+hh,H), Ft(hx+hh,hy+hh,H), Ft(hx+hh,hy+hh,HC), AuPoint(0,-1,0));
					}
					else if(skylight)
					{
						AddQuad(faces, Ft(hx-sh,hy-sh,HC), Ft(hx-sh,hy-sh,H), Ft(hx-sh,hy+sh,H), Ft(hx-sh,hy+sh,HC), AuPoint(1,0,0));
						AddQuad(faces, Ft(hx+sh,hy-sh,HC), Ft(hx+sh,hy-sh,H), Ft(hx+sh,hy+sh,H), Ft(hx+sh,hy+sh,HC), AuPoint(-1,0,0));
						AddQuad(faces, Ft(hx-sh,hy-sh,HC), Ft(hx-sh,hy-sh,H), Ft(hx+sh,hy-sh,H), Ft(hx+sh,hy-sh,HC), AuPoint(0,1,0));
						AddQuad(faces, Ft(hx-sh,hy+sh,HC), Ft(hx-sh,hy+sh,H), Ft(hx+sh,hy+sh,H), Ft(hx+sh,hy+sh,HC), AuPoint(0,-1,0));
					}
					else if(!shaftAbove)
						for(int q = 0; q < 4; q++)
							if(!IsOpenQuarter(plan, D + 1, cell, q))
							{
								QVector<V2> t = QuarterPoly(plan, cell, q);
								AddTri(faces, Ft(t[0], HC), Ft(t[1], HC), Ft(t[2], HC), AuPoint(0,0,-1));
							}
					if(hatchAt(D, cell))
					{
						double zb = -plan.wall;
						AddQuad(faces, Ft(hx-hh,hy-hh,zb), Ft(hx-hh,hy-hh,0), Ft(hx-hh,hy+hh,0), Ft(hx-hh,hy+hh,zb), AuPoint(1,0,0));
						AddQuad(faces, Ft(hx+hh,hy-hh,zb), Ft(hx+hh,hy-hh,0), Ft(hx+hh,hy+hh,0), Ft(hx+hh,hy+hh,zb), AuPoint(-1,0,0));
						AddQuad(faces, Ft(hx-hh,hy-hh,zb), Ft(hx-hh,hy-hh,0), Ft(hx+hh,hy-hh,0), Ft(hx+hh,hy-hh,zb), AuPoint(0,1,0));
						AddQuad(faces, Ft(hx-hh,hy+hh,zb), Ft(hx-hh,hy+hh,0), Ft(hx+hh,hy+hh,0), Ft(hx+hh,hy+hh,zb), AuPoint(0,-1,0));
					}
				}
				else if(whole(cell) == '*')
					for(int q = 0; q < 4; q++)
					{
						if(plan.Quarter(D, cell, q) != room.letter)
							continue;
						QVector<V2> t = QuarterPoly(plan, cell, q);
						if(!IsOpenQuarter(plan, D, cell, q))
							AddTri(faces, Ft(t[0], 0), Ft(t[1], 0), Ft(t[2], 0), AuPoint(0,0,1));
						if(air ? coveredAbove(cell, q) : !IsOpenQuarter(plan, D + 1, cell, q))
							AddTri(faces, Ft(t[0], HC), Ft(t[1], HC), Ft(t[2], HC), AuPoint(0,0,-1));
					}
			}
		for(const QList<Edge>& loop : loops)
		{
			int n = loop.size();
			QVector<V2> starts(n), ends(n);
			QVector<bool> step(n, false);
			for(int i = 0; i < n; i++)
			{
				const Edge& prev = loop[(i + n - 1) % n];
				const Edge& e = loop[i];
				V2 up = (prev.b - prev.a).Unit(), ue = (e.b - e.a).Unit();
				V2 pp = prev.a + up.Left() * prev.inset, pe = e.a + ue.Left() * e.inset;
				if(qAbs(up.Cross(ue)) < 1e-9)
				{
					ends[(i + n - 1) % n] = e.a + up.Left() * prev.inset;
					starts[i] = pe;
					step[i] = true;
				}
				else
				{
					V2 x = Intersect(pp, up, pe, ue);
					ends[(i + n - 1) % n] = x;
					starts[i] = x;
				}
			}
			for(int i = 0; i < n; i++)
			{
				const Edge& e = loop[i];
				V2 ue = (e.b - e.a).Unit(), inward = ue.Left();
				const double foot = D > 0 ? -plan.wall : 0;
				if(step[i] && (ends[(i + n - 1) % n] - starts[i]).Length() > 1e-6)
				{
					V2 from = ends[(i + n - 1) % n], to = starts[i];
					AddWall(faces, from, to, foot, air ? H : HC, (to - from).Unit().Left(), {});
				}
				if(e.open || e.shell)
					continue;
				if(air)
				{
					const double len = (ends[i] - starts[i]).Length(), st = plan.cell / 4;
					for(double s = 0; s < len - 1e-6; s += st)
					{
						double s1 = qMin(len, s + st);
						V2 p = starts[i] + ue * ((s + s1) / 2) + inward * 0.15;
						QPoint cell;
						int q;
						if(QuarterAtPoint(plan, p.x, p.y, cell, q) && coveredAbove(cell, q))
							continue;
						V2 w0 = starts[i] + ue * s, w1 = starts[i] + ue * s1;
						AddWall(faces, w0, w1, HC, H, inward, {});
						AddQuad(faces, Ft(w0, H), Ft(w0 - inward * e.inset, H), Ft(w1 - inward * e.inset, H), Ft(w1, H), AuPoint(0,0,1));
					}
				}
				QList<QPair<double, double>> gaps = openings(starts[i], ends[i], e.a, e.b, room.letter, false);
				QList<Hole> holes = windowHoles(starts[i], ends[i], e.a, e.b, room.letter, false);
				AddWall(faces, starts[i], ends[i], 0, HC, inward, gaps, holes);
				if(D > 0)
					AddWall(faces, starts[i], ends[i], foot, 0, inward, {});
				const double len = (ends[i] - starts[i]).Length();
				QList<QPair<double, double>> open = D > 0 ? openAlong(starts[i], ue, len, inward) : QList<QPair<double, double>>();
				WallRun run = wallRun(starts[i], ends[i], inward, HC, gaps);
				run.onFloor = true;
				run.panels = &panels;
				for(const QPair<double, double>& o : open)
				{
					V2 i0 = starts[i] + ue * o.first, i1 = starts[i] + ue * o.second;
					AddQuad(faces, Ft(i0, foot), Ft(i0 - inward * e.inset, foot), Ft(i1 - inward * e.inset, foot), Ft(i1, foot),
						AuPoint(0,0,-1));
					run.keep << WallRun::Keep{o.first, o.second, -1, 1.0};
				}
				auto openAt = [&](double s) {
					for(const QPair<double, double>& o : open)
						if(s >= o.first - 1e-6 && s <= o.second + 1e-6)
							return true;
					return false;
				};
				TrimRoomWall(trim, run, theme, trims);
				for(const Hole& h : holes)
				{
					V2 i0 = starts[i] + ue * h.s0, i1 = starts[i] + ue * h.s1, o0 = i0 - inward * e.inset, o1 = i1 - inward * e.inset;
					AddQuad(faces, Ft(i0, h.z0), Ft(i0, h.z1), Ft(o0, h.z1), Ft(o0, h.z0), AuPoint(ue.x, ue.y, 0));
					AddQuad(faces, Ft(i1, h.z0), Ft(i1, h.z1), Ft(o1, h.z1), Ft(o1, h.z0), AuPoint(-ue.x, -ue.y, 0));
					if(h.z0 > 1e-6 || openAt((h.s0 + h.s1) / 2))
						AddQuad(faces, Ft(i0, h.z0), Ft(o0, h.z0), Ft(o1, h.z0), Ft(i1, h.z0), AuPoint(0,0,1));
					if(h.z1 < HC - 1e-6)
						AddQuad(faces, Ft(i0, h.z1), Ft(o0, h.z1), Ft(o1, h.z1), Ft(i1, h.z1), AuPoint(0,0,-1));
				}
				for(const QPair<double, double>& g : gaps)
				{
					for(int side = 0; side < 2; side++)
					{
						double s = side ? g.second : g.first;
						V2 inner = starts[i] + ue * s, outer = inner - inward * e.inset;
						V2 face = side ? ue * -1 : ue;
						AddQuad(faces, Ft(inner, 0), Ft(inner, DOOR_HEIGHT), Ft(outer, DOOR_HEIGHT), Ft(outer, 0),
							AuPoint(face.x, face.y, 0));
					}
					V2 i0 = starts[i] + ue * g.first, i1 = starts[i] + ue * g.second;
					AddQuad(faces, Ft(i0, DOOR_HEIGHT), Ft(i0 - inward * e.inset, DOOR_HEIGHT),
						Ft(i1 - inward * e.inset, DOOR_HEIGHT), Ft(i1, DOOR_HEIGHT), AuPoint(0,0,-1));
				}
			}
		}
		if(D > 0)
			for(int r = 0; r < plan.Rows(); r++)
				for(int c = 0; c < plan.Cols(); c++)
				{
					QPoint p(c, r);
					if(whole(p) != room.letter || !IsOpenCell(plan, D, p))
						continue;
					for(int s = 0; s < 4; s++)
					{
						QPoint q = Neighbour(p, Side(s));
						if(whole(q) != room.letter || IsOpenCell(plan, D, q))
							continue;
						static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
						V2 m(midX(p), midY(p)), out = dirs[s], along(out.y, -out.x);
						V2 a = m + out * (plan.cell / 2) - along * (plan.cell / 2), b = a + along * plan.cell;
						AddQuad(faces, Ft(a, -plan.wall), Ft(a, 0), Ft(b, 0), Ft(b, -plan.wall), AuPoint(-out.x, -out.y, 0));
					}
				}
		for(AuScFaceList* list : {&faces, &farFaces})
		{
			textureBySide(*list, RoomTexture(plan, space, "floor"), RoomTexture(plan, space, "walls"), RoomTexture(plan, space, "ceiling"),
				RoomTextureScale(space, "floor"), RoomTextureScale(space, "walls"), RoomTextureScale(space, "ceiling"));
			for(AuScFace* f : *list)
			{
				const double nz = f->FaceNormal().z;
				const QString surface = nz >= 0.7 ? "floor" : nz <= -0.7 ? "ceiling" : "walls";
				const QColor c = RoomColor(space, surface);
				if(c.isValid())
					f->SetPaint(Tint(c, RoomColorStrength(space, surface)));
			}
		}
		AuScRoom part;
		part.AddFaces(faces);
		part.SetGroupName(space.name);
		part.SetInteriorLighting(space.sunlit, LightOf(space.lights));
		add(part, QString("room %1 deck %2").arg(room.name).arg(D));
		if(!farFaces.isEmpty())
		{
			AuScRoom away;
			away.AddFaces(farFaces);
			away.SetGroupName(space.name);
			away.SetInteriorLighting(space.sunlit, LightOf(space.lights));
			add(away, QString("room %1 deck %2 away from the building").arg(room.name).arg(D));
		}

		double beamD = qMin(theme == Stone ? 0.16 : theme == Metal ? 0.2 : 0.18, plan.voidGap - 0.03);
		if((trims & Beams) && theme != Plain && beamD >= 0.05)
		{
			auto beamAt = [&](const QPoint& cell) {
				if(!plan.OnGrid(cell))
					return false;
				bool someOpenAbove = false;
				for(int q = 0; q < 4; q++)
					someOpenAbove = someOpenAbove || IsOpenQuarter(plan, D + 1, cell, q);
				return !(whole(cell) != room.letter || hatchAt(D + 1, cell) || someOpenAbove || IsOpenCell(plan, D, cell)
					|| SkylightAt(plan, D, cell) || roofHatchAt(D, cell));
			};
			for(int r = 0; r < plan.Rows(); r++)
				for(int c = 0; c < plan.Cols(); c++)
				{
					QPoint cell(c, r);
					if(!beamAt(cell))
						continue;
					const QPoint west(c - 1, r), east(c + 1, r);
					const int skip = BoxTop | (beamAt(west) || plan.Quarter(D, west, East) != room.letter ? BoxBack : 0)
						| (beamAt(east) || plan.Quarter(D, east, West) != room.letter ? BoxFront : 0);
					double y = midY(cell), bw = theme == Stone ? 0.22 : theme == Metal ? 0.06 : 0.16;
					AddBox(trim, plan.X0(c), y - bw / 2, HC - beamD, plan.X1(c), y + bw / 2, HC, skip);
					if(theme == Metal)
						AddBox(trim, plan.X0(c), y - 0.08, HC - beamD, plan.X1(c), y + 0.08, HC - beamD + 0.025, skip & ~BoxTop);
				}
		}
		if(!trim.isEmpty() || !panels.isEmpty())
		{
			AuScMesh mesh;
			FinishTrim(plan, trim, TRIM_PAINT, RoomTrimTexture(plan, space), RoomColor(space, "trim"), RoomColorStrength(space, "trim"),
				RoomTextureScale(space, "trim"));
			Paint(panels, Tint(RoomColor(space, "trim").isValid() ? RoomColor(space, "trim") : TRIM_PAINT, LOW_POLY_TINT));
			while(!panels.isEmpty())
				trim.append(panels.takeFirst());
			mesh.AddFaces(trim);
			mesh.SetGroupName(space.name);
			add(mesh, QString("trim %1 deck %2").arg(room.name).arg(D));
		}
	}

}

void PlanBuilder::BuildOutsideTrims()
{
	const Theme dressing = OutsideDressing(plan) == "none" ? Plain : ThemeOf(OutsideDressing(plan));
	if(dressing != Plain)
	{
		Plan outline = plan;
		for(QString& row : outline.decks[D])
			for(QChar& ch : row)
				if(ch != '.')
					ch = IsOpenAirRoom(plan, ch) ? QChar('.') : QChar('X');
		AuScFaceList faces, panels;
		QList<QSharedPointer<RoofRegion>> roofRegions;
		for(const Roof& roof : plan.roofs)
			roofRegions << QSharedPointer<RoofRegion>::create(plan, D, roof, theme, H);
		auto soffit = [&](const V2& p) {
			double z = 1e9;
			for(const QSharedPointer<RoofRegion>& region : roofRegions)
				z = qMin(z, region->Soffit(p));
			return z + ROOF_THICKNESS - 0.015;
		};
		const double proud = 0.07, cornerProud = 0.13;
		auto openAirAt = [&](const V2& p) {
			QPoint cell;
			int q;
			return QuarterAtPoint(plan, p.x, p.y, cell, q) && IsOpenAirQuarter(plan, D, cell, q);
		};
		for(const QList<Edge>& loop : RoomOutlines(outline, D, 'X'))
			for(int i = 0; i < loop.size(); i++)
			{
				const Edge& e = loop[i];
				V2 u = (e.b - e.a).Unit(), n = u.Left() * -1;
				double len = (e.b - e.a).Length();
				QList<QPair<double, double>> stretches;
				QList<bool> onBalcony;
				{
					const bool axis = qAbs(u.x) < 1e-6 || qAbs(u.y) < 1e-6;
					const int count = qMax(1, qRound(len / (axis ? plan.cell / 2 : plan.cell * M_SQRT1_2)));
					for(int k = 0; k < count; k++)
					{
						double s0 = k == 0 ? 0.0 : len * k / count, s1 = k + 1 == count ? len : len * (k + 1) / count;
						bool air = openAirAt(e.a + u * ((s0 + s1) / 2) + n * 0.05);
						if(!stretches.isEmpty() && onBalcony.last() == air)
							stretches.last().second = s1;
						else
						{
							stretches << qMakePair(s0, s1);
							onBalcony << air;
						}
					}
				}
				for(int k = 0; k < stretches.size(); k++)
				{
					const double off = 0.0;
					const double s0 = stretches[k].first, s1e = stretches[k].second;
					const V2 a = (s0 == 0 && off == 0) ? e.a : e.a + u * s0 + n * off;
					const V2 b = (s1e == len && off == 0) ? e.b : e.a + u * s1e + n * off;
					const double sl = s1e - s0;
					QList<QPair<double, double>> gaps;
					for(const QPair<double, double>& g : openings(a, b, e.a, e.b, '.', true))
						if(g.second > -1e-6 && g.first < sl + 1e-6)
							gaps << g;
					WallRun run = wallRun(a, b, n, H, gaps);
					run.onFloor = D == 0 || onBalcony[k];
					run.panels = &panels;
					for(double s = 0; s < sl - 1e-6; s += 0.25)
					{
						double s1 = qMin(sl, s + 0.25);
						double z = qMin(soffit(a + u * s + n * proud), qMin(soffit(a + u * ((s + s1) / 2) + n * proud),
							soffit(a + u * s1 + n * proud)));
						if(z < 1e8)
							run.ceilings << WallRun::Keep{s, s1, qMin(z, H + 1), H + 1};
					}
					QList<QPair<double, double>> eaves;
					const double step = 0.25;
					for(double s = 0; s < sl - 1e-6; s += step)
					{
						double s1 = qMin(sl, s + step);
						V2 p = a + u * ((s + s1) / 2) - n * (0.3 + off);
						QPoint cell;
						int q;
						bool found = QuarterAtPoint(plan, p.x, p.y, cell, q);
						bool covered = D + 1 < plan.Decks() && found && plan.Quarter(D + 1, cell, q) != '.';
						QChar roof = found ? plan.RoofAt(D, cell) : QChar('.');
						if(roof != '.' && plan.roofs.contains(roof))
						{
							const QString& style = plan.roofs[roof].style;
							covered = covered || (style != "flat" && style != "dome" && style != "spire");
						}
						if(covered)
							continue;
						if(!eaves.isEmpty() && qAbs(eaves.last().second - s) < 1e-6)
							eaves.last().second = s1;
						else
							eaves << qMakePair(s, s1);
					}
					TrimOutsideWall(faces, run, dressing, D == 0 && !onBalcony[k], eaves);
				}
				const Edge& next = loop[(i + 1) % loop.size()];
				V2 u2 = (next.b - next.a).Unit();
				if(u.Cross(u2) > 1e-9 && !openAirAt(e.b - u * 0.05 + n * 0.05) && !openAirAt(e.b + u2 * 0.05 - u2.Left() * 0.05))
					TrimOutsideCorner(faces, e.b, u, u2, qMin(H, soffit(e.b + (n + u2.Left() * -1) * cornerProud)), D == 0, dressing);
			}
		if(!faces.isEmpty() || !panels.isEmpty())
		{
			AuScMesh mesh;
			FinishOutsideTrim(plan, faces, OUTSIDE_TRIM_PAINT);
			Paint(panels, Tint(OutsideTrimColor(plan).isValid() ? OutsideTrimColor(plan) : OUTSIDE_TRIM_PAINT, LOW_POLY_TINT));
			while(!panels.isEmpty())
				faces.append(panels.takeFirst());
			mesh.AddFaces(faces);
			add(mesh, QString("outside trim deck %1").arg(D));
		}
	}

}

}
