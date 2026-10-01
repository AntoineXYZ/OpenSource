#include "planbuilder.h"
#include "internal.h"

#include <QSet>

namespace SimpleMode::detail {

void PlanBuilder::BuildOpenAir()
{
	QList<QPair<RailingGap, DoorPlace>> gaps;
	for(const RailingGap& g : plan.gaps)
	{
		if(g.deck != D)
			continue;
		DoorPlace gp = PlaceGap(plan, g);
		if(!gp.ok)
		{
			log << "  Skipping railing gap at " << g.at.x() << "," << g.at.y() << " on deck " << D << ": " << gp.problem << "\n";
			continue;
		}
		if(D > 0)
			log << "  Railing gap at " << g.at.x() << "," << g.at.y() << " on deck " << D << " leads out into thin air, as a door to "
				<< "the outside does above the ground deck (unless a stair comes up there).\n";
		gaps << qMakePair(g, gp);
	}
	for(const Room& room : plan.rooms)
	{
		if(!IsOpenAirRoom(plan, room.letter))
			continue;
		const QList<QList<Edge>>& loops = outlineOf(D, room.letter);
		if(loops.isEmpty())
			continue;
		const bool railed = HasOpenAirRailing(plan, D, room.letter);
		const bool catwalk = room.type == "catwalk", platform = room.type == "platform";
		const double T = catwalk ? 0.1 : 0.22;
		const QString style = catwalk ? "bars" : DefaultRailingStyle(plan.theme);
		auto somethingAt = [&](int deck, const V2& p) {
			QPoint cell;
			int q;
			return QuarterAtPoint(plan, p.x, p.y, cell, q) && plan.Quarter(deck, cell, q) != '.';
		};
		struct Arrival { V2 c, u, F; double half, gap; bool reach = true, ground = false; };
		QList<Arrival> arrivals;
		for(const PlacedStair& ps : placedStairs)
		{
			if(ps.stair.deck != D - 1 || whole(ps.layout.arrival) != room.letter || !arrivesOnOpenAir(ps))
				continue;
			static const V2 sideDir[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
			V2 out = sideDir[Opposite(Side(ps.layout.arrivalSide))];
			const StairFrame& fr = ps.frames.last();
			arrivals << Arrival{V2(midX(ps.layout.arrival), midY(ps.layout.arrival)) + out * (S / 2), V2(out.y, -out.x), fr.F,
				fr.w / 2, fr.w / 2 + 0.05};
		}
		for(const QPair<RailingGap, DoorPlace>& g : gaps)
		{
			if(g.second.here != room.letter)
				continue;
			const double w = GapWidth(plan, g.first);
			Arrival ar{V2(g.second.mx, g.second.my), V2(g.second.ux, g.second.uy), V2(-g.second.nx, -g.second.ny),
				g.first.full ? qMax(0.3, w / 2 - 0.2) : w / 2, w / 2};
			ar.reach = D == 0;
			ar.ground = D == 0;
			arrivals << ar;
		}
		AuScFaceList rail, glass, dress, legs;
		QList<AuScFaceList> blocks;
		QSet<QPair<int, int>> legAt, postAt;

		for(const QList<Edge>& loop : loops)
		{
			int n = loop.size();
			QVector<V2> starts(n), ends(n);
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
				if(!e.open)
					continue;
				V2 u = (e.b - e.a).Unit(), inward = u.Left(), out = inward * -1;
				V2 a = starts[i], b = ends[i], m = (a + b) * 0.5;
				double len = (b - a).Length();
				if(len < 0.05)
					continue;
				if(railed)
				{
					QList<QPair<double, double>> pieces{qMakePair(0.0, len)};
					for(const Arrival& ar : arrivals)
					{
						double along = u.Dot(ar.c - e.a);
						if(qAbs(u.Cross(ar.c - e.a)) > 1e-6 || along < 0 || along > (e.b - e.a).Length())
							continue;
						double s = u.Dot(ar.c - a);
						QList<QPair<double, double>> cut;
						for(const QPair<double, double>& pc : pieces)
						{
							if(pc.first < s - ar.gap)
								cut << qMakePair(pc.first, qMin(pc.second, s - ar.gap));
							if(pc.second > s + ar.gap)
								cut << qMakePair(qMax(pc.first, s + ar.gap), pc.second);
						}
						pieces = cut;
					}
					for(const QPair<double, double>& pc : pieces)
					{
						if(pc.second - pc.first < 0.15)
							continue;
						V2 pa = a + u * pc.first, pb = a + u * pc.second, pm = (pa + pb) * 0.5;
						double pl = pc.second - pc.first;
						RailingMesh(rail, glass, pa, pb, out, style, 0, OpenEdgeInset(plan, room.letter));
						blocks << AuScFaceList();
						AddWallBox(blocks.last(), pm, u, out, -0.05, 0.05, pl, 0, HC);
						railingLines << OpenSpan{D, pm, u, pl / 2};
					}
				}
				{
					int count = qMax(1, int(std::ceil(len / (2 * S) - 1e-6)));
					for(int k = 0; k <= count; k++)
					{
						V2 pt = a + (b - a) * (double(k) / count), probe = pt + inward * 0.15;
						QPoint pc;
						int pq;
						if(!QuarterAtPoint(plan, probe.x, probe.y, pc, pq) || !coveredAbove(pc, pq))
							continue;
						QPair<int, int> key(qRound(pt.x * 100), qRound(pt.y * 100));
						if(postAt.contains(key))
							continue;
						postAt.insert(key);
						AddWallBox(legs, pt, u, out, -0.07, 0.07, 0.14, 0, HC);
					}
				}
				if(platform && D > 0)
				{
					int count = qMax(1, int(std::ceil(len / (2 * S) - 1e-6)));
					for(int k = 0; k <= count; k++)
					{
						V2 pt = a + (b - a) * (double(k) / count);
						QPair<int, int> key(qRound(pt.x * 100), qRound(pt.y * 100));
						if(legAt.contains(key))
							continue;
						legAt.insert(key);
						double bottom = -D * H;
						for(int d = D - 1; d >= 0; d--)
							if(somethingAt(d, pt + inward * 0.05))
							{
								bottom = (d + 1 - D) * H;
								break;
							}
						if(bottom < -T - 0.05)
							AddWallBox(legs, pt, u, out, -0.08, 0.08, 0.16, bottom, -T);
					}
				}
			}
		}

		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint cell(c, r);
				if(whole(cell) == '*')
				{
					for(int q = 0; q < 4; q++)
						if(plan.Quarter(D, cell, q) == room.letter && D > 0 && plan.Quarter(D - 1, cell, q) == '.')
						{
							QVector<V2> t = QuarterPoly(plan, cell, q);
							AddTri(dress, Ft(t[0], -T), Ft(t[1], -T), Ft(t[2], -T), AuPoint(0,0,-1));
						}
					continue;
				}
				if(whole(cell) != room.letter)
					continue;
				auto openSide = [&](Side s) { return IsOpenEdge(plan, room.letter, facing(cell, s)) && !stairArrivingAt(cell, s); };
				const double fi = openFloorInset(room.letter, cell);
				double x0 = plan.X0(c) + (openSide(West) ? fi : 0), x1 = plan.X1(c) - (openSide(East) ? fi : 0);
				double y0 = plan.Y0(r) + (openSide(South) ? fi : 0), y1 = plan.Y1(r) - (openSide(North) ? fi : 0);
				V2 mid(midX(cell), midY(cell));
				const V2 corner[4][2] = {{V2(x1, y1), V2(x0, y1)}, {V2(x1, y0), V2(x1, y1)}, {V2(x0, y0), V2(x1, y0)},
					{V2(x0, y1), V2(x0, y0)}};
				static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
				const bool slab = D > 0 && plan.At(D - 1, cell) == '.';
				const bool roofed = coveredAbove(cell);
				if(slab)
					AddQuad(dress, Ft(x0,y0,-T), Ft(x0,y1,-T), Ft(x1,y1,-T), Ft(x1,y0,-T), AuPoint(0,0,-1));
				for(int s = 0; s < 4; s++)
				{
					QPoint nb = Neighbour(cell, Side(s));
					bool sameRoom = whole(nb) == room.letter;
					const V2& p0 = corner[s][0];
					const V2& p1 = corner[s][1];
					AuPoint nOut(dirs[s].x, dirs[s].y, 0);
					if(slab && !sameRoom && plan.At(D - 1, nb) == '.')
						AddQuad(dress, Ft(p0, -T), Ft(p0, 0), Ft(p1, 0), Ft(p1, -T), nOut);
					if(roofed && (whole(nb) == '.' || (sameRoom && !coveredAbove(nb))))
					{
						V2 e0 = mid + dirs[s] * (S / 2) + V2(dirs[s].y, -dirs[s].x) * (S / 2);
						V2 e1 = mid + dirs[s] * (S / 2) - V2(dirs[s].y, -dirs[s].x) * (S / 2);
						AddQuad(dress, Ft(e0, HC), Ft(e0, H), Ft(e1, H), Ft(e1, HC), nOut);
					}
				}
			}

		auto addMesh = [&](AuScFaceList& faces, const QColor& color, const QString& what) {
			if(faces.isEmpty())
				return;
			Paint(faces, color);
			AuScMesh mesh;
			mesh.AddFaces(faces);
			add(mesh, QString("%1 %2 deck %3").arg(what).arg(room.name).arg(D));
		};
		if(!rail.isEmpty())
		{
			Paint(rail, style == "bars" ? QColor(0x2c, 0x2d, 0x31) : style == "glass" ? QColor(0x9a, 0xa2, 0xaa) : TRIM_PAINT);
			Paint(glass, QColor(0xb0, 0xd0, 0xe0));
			glass.SetOpacity(0.3f);
			AuScMesh mesh;
			mesh.AddFaces(rail);
			mesh.AddFaces(glass);
			add(mesh, QString("railing %1 deck %2").arg(room.name).arg(D));
		}
		addMesh(dress, OUTSIDE_TRIM_PAINT, "underside");
		addMesh(legs, OUTSIDE_TRIM_PAINT, "legs");
		for(AuScFaceList& block : blocks)
		{
			AuScBarrier barrier;
			barrier.AddFaces(block);
			barrier.SetFaceOpacity(VOID_OPACITY);
			add(barrier, QString("railing barrier %1").arg(room.name));
		}
		blocks.clear();

		for(const Arrival& ar : arrivals)
		{
			if(!ar.reach)
				continue;
			QPoint cell;
			int q;
			V2 inside = ar.c + ar.F * 0.3;
			if(!QuarterAtPoint(plan, inside.x, inside.y, cell, q))
				continue;
			static const V2 sideDir[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
			Side side = North;
			for(int s = 0; s < 4; s++)
				if(sideDir[s].Dot(ar.F) < -0.9)
					side = Side(s);
			double reachIn = voidInset(cell, side) + VOID_OVERLAP;
			V2 p0 = ar.c - ar.F * 0.8 - ar.u * ar.half, p1 = ar.c + ar.F * reachIn + ar.u * ar.half;
			AuScFaceList faces;
			AddBox(faces, qMin(p0.x, p1.x), qMin(p0.y, p1.y), ar.ground ? 0.0 : VF, qMax(p0.x, p1.x), qMax(p0.y, p1.y), VC);
			addRoomVoid(room, faces, QString("%1 void %2 deck %3").arg(ar.ground ? "railing gap" : "stair arrival").arg(room.name).arg(D));
			openSpans << OpenSpan{D, ar.c, ar.u, ar.gap};
			if(ar.ground)
				groundSpots << GroundSpot{D, ar.c, ar.u, ar.F * -1, -reachIn, ar.half};
			ctx.arrivalChecks << ArrivalCheck{D, ar.c, ar.F, ar.half, ar.ground};
		}

		if(!railed)
			continue;
		const QList<VoidBox> nearR = nearRects(room.letter);
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint cell(c, r);
				AuScFaceList box;
				if(whole(cell) == room.letter)
				{
					auto openSide = [&](Side s) { return IsOpenEdge(plan, room.letter, facing(cell, s)) && !stairArrivingAt(cell, s); };
					const double fi = openFloorInset(room.letter, cell);
					const VoidBox all{plan.X0(c) + (openSide(West) ? fi : 0), plan.Y0(r) + (openSide(South) ? fi : 0),
						plan.X1(c) - (openSide(East) ? fi : 0), plan.Y1(r) - (openSide(North) ? fi : 0)};
					for(const QPair<VoidBox, bool>& pc : splitNear(all, nearR))
					{
						VoidBox b = pc.first;
						b.x0 -= b.x0 > all.x0 + 1e-6 ? 0.01 : 0;
						b.y0 -= b.y0 > all.y0 + 1e-6 ? 0.01 : 0;
						b.x1 += b.x1 < all.x1 - 1e-6 ? 0.01 : 0;
						b.y1 += b.y1 < all.y1 - 1e-6 ? 0.01 : 0;
						AuScFaceList piece;
						AddBox(piece, b.x0, b.y0, 0, b.x1, b.y1, H);
						piece.SetOpacity(0.f);
						AuScWindow enclosure;
						enclosure.AddFaces(piece);
						enclosure.SetGroupName(room.name);
						add(enclosure, QString("open air enclosure %1,%2 deck %3%4").arg(c).arg(r).arg(D).arg(pc.second ? "" : " (away)"));
					}
					continue;
				}
				else if(whole(cell) == '*')
				{
					for(int q = 0; q < 4; q++)
						if(plan.Quarter(D, cell, q) == room.letter)
							AddPrism(box, QuarterPoly(plan, cell, q), 0, H);
				}
				if(box.isEmpty())
					continue;
				box.SetOpacity(0.f);
				AuScWindow enclosure;
				enclosure.AddFaces(box);
				enclosure.SetGroupName(room.name);
				add(enclosure, QString("open air enclosure %1,%2 deck %3").arg(c).arg(r).arg(D));
			}
	}
}

void PlanBuilder::BuildUnderVoids()
{
	if(D == 0)
		return;
	const double off = 0.5, fringe = 1.0, top = -0.7;
	if(D * H + top < 1.5)
		return;
	auto builtBelow = [&](const QPoint& cell) {
		for(int k = 0; k < D; k++)
			if(plan.At(k, cell) != '.')
				return true;
		for(const PlacedStair& ps : placedStairs)
			if(ps.stair.deck < D && ps.layout.cells.contains(cell))
				return true;
		return false;
	};
	for(int r = 0; r < plan.Rows(); r++)
		for(int c = 0; c < plan.Cols(); c++)
		{
			QPoint cell(c, r);
			if(whole(cell) == '.' || builtBelow(cell))
				continue;
			double b[4];
			for(int s = 0; s < 4; s++)
			{
				QPoint nb = Neighbour(cell, Side(s));
				b[s] = builtBelow(nb) ? -off : whole(nb) != '.' ? 0.0 : fringe;
			}
			double x0 = plan.X0(c) - b[West], x1 = plan.X1(c) + b[East], y0 = plan.Y0(r) - b[South], y1 = plan.Y1(r) + b[North];
			for(int dx : {-1, 1})
				for(int dy : {-1, 1})
				{
					if(!builtBelow(QPoint(c + dx, r - dy)))
						continue;
					if(dx > 0)
						x1 = qMin(x1, plan.X1(c) - (b[East] < 0 ? off : 0));
					else
						x0 = qMax(x0, plan.X0(c) + (b[West] < 0 ? off : 0));
					if(dy > 0)
						y1 = qMin(y1, plan.Y1(r) - (b[North] < 0 ? off : 0));
					else
						y0 = qMax(y0, plan.Y0(r) + (b[South] < 0 ? off : 0));
				}
			AuScFaceList faces;
			AddBox(faces, x0, y0, -D * H, x1, y1, top);
			AuScHullVoid hv;
			hv.AddFaces(faces);
			hv.SetFaceOpacity(VOID_OPACITY);
			add(hv, QString("under %1,%2 deck %3").arg(c).arg(r).arg(D));
		}
}

}
