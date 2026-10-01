#include "planbuilder.h"
#include "internal.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace SimpleMode::detail {

void PlanBuilder::addRoomVoid(const Room& room, AuScFaceList& faces, const QString& what)
{
	AuScRoomVoid v;
	v.AddFaces(faces);
	v.SetFaceOpacity(VOID_OPACITY);
	v.SetGroupName(room.name);
	v.SetHoldAccess(HoldOf(room.type));
	v.SetPersonnelAccess(PERSONNEL_Everyone);
	add(v, what);
	voidCount++;
}

void PlanBuilder::addCarvedVoid(const Room& room, const QVector<V2>& poly, const QVector<V2>& base, const QString& what)
{
	QList<QVector<V2>> carved = carveVoid(room.letter, poly, base);
	bool unchanged = carved.size() == 1 && carved[0].size() == poly.size();
	for(int k = 0; unchanged && k < poly.size(); k++)
		unchanged = (carved[0][k] - poly[k]).Length() < 1e-9;
	for(int k = 0; k < carved.size(); k++)
	{
		AuScFaceList faces;
		if(unchanged && poly.size() == 4)
			AddBox(faces, poly[0].x, poly[0].y, VF, poly[2].x, poly[2].y, VC);
		else
			AddPrism(faces, carved[k], VF, VC);
		addRoomVoid(room, faces, carved.size() == 1 ? what : QString("%1 part %2").arg(what).arg(k + 1));
	}
}

void PlanBuilder::BuildRoomVoids()
{
	QVector<int> rectOf(plan.Rows() * plan.Cols(), -1);
	auto rectAt = [&](const QPoint& p) -> int& { return rectOf[p.y() * plan.Cols() + p.x()]; };
	for(const Room& room : plan.rooms)
	{
		if(IsLiftRoom(plan, room.letter))
			continue;
		const bool air = IsOpenAirRoom(plan, room.letter);
		const bool perCell = air;
		const QList<VoidBox> nearR = air ? nearRects(room.letter) : QList<VoidBox>();
		auto splitVoid = [&](const VoidBox& box, const QString& what) {
			for(const QPair<VoidBox, bool>& pc : splitNear(box, nearR))
			{
				VoidBox b = pc.first;
				b.x0 -= b.x0 > box.x0 + 1e-6 ? VOID_OVERLAP : 0;
				b.y0 -= b.y0 > box.y0 + 1e-6 ? VOID_OVERLAP : 0;
				b.x1 += b.x1 < box.x1 - 1e-6 ? VOID_OVERLAP : 0;
				b.y1 += b.y1 < box.y1 - 1e-6 ? VOID_OVERLAP : 0;
				AuScFaceList faces;
				AddBox(faces, b.x0, b.y0, VF, b.x1, b.y1, VC);
				addRoomVoid(room, faces, what);
			}
		};
		QList<VoidBox> boxes, boxCells;
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint p(c, r);
				if(whole(p) != room.letter || rectAt(p) >= 0 || IsOpenCell(plan, D, p))
					continue;
				int c1 = c;
				while(true)
				{
					QPoint q(c1 + 1, r);
					if(perCell || whole(q) != room.letter || rectAt(q) >= 0 || IsOpenCell(plan, D, q) || voidInset(q, North) != voidInset(p, North)
							|| voidInset(q, South) != voidInset(p, South))
						break;
					c1++;
				}
				int r1 = r;
				while(true)
				{
					int rr = r1 + 1;
					bool ok = !perCell && voidInset(QPoint(c, r1), South) == 0;
					for(int k = c; ok && k <= c1; k++)
					{
						QPoint q(k, rr);
						ok = whole(q) == room.letter && rectAt(q) < 0 && !IsOpenCell(plan, D, q)
							&& voidInset(q, South) == voidInset(QPoint(c, rr), South)
							&& voidInset(q, North) == 0;
					}
					ok = ok && voidInset(QPoint(c, rr), West) == voidInset(p, West)
						&& voidInset(QPoint(c1, rr), East) == voidInset(QPoint(c1, r), East);
					if(!ok)
						break;
					r1 = rr;
				}
				for(int rr = r; rr <= r1; rr++)
					for(int k = c; k <= c1; k++)
						rectAt(QPoint(k, rr)) = boxes.size();
				boxes << VoidBox{plan.X0(c) + voidInset(p, West), plan.Y0(r1) + voidInset(QPoint(c, r1), South),
					plan.X1(c1) - voidInset(QPoint(c1, r), East), plan.Y1(r) - voidInset(p, North)};
				boxCells << VoidBox{plan.X0(c), plan.Y0(r1), plan.X1(c1), plan.Y1(r)};
			}
		for(int i = 0; i < boxes.size() && air; i++)
			splitVoid(boxes[i], QString("room void %1 deck %2 box %3").arg(room.name).arg(D).arg(i + 1));
		for(int i = 0; i < boxes.size() && !air; i++)
			addCarvedVoid(room, rectPoly(boxes[i].x0, boxes[i].y0, boxes[i].x1, boxes[i].y1),
				rectPoly(boxCells[i].x0, boxCells[i].y0, boxCells[i].x1, boxCells[i].y1),
				QString("room void %1 deck %2 box %3").arg(room.name).arg(D).arg(i + 1));

		QMap<QString, VoidBox> connectors;
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint p(c, r);
				if(whole(p) != room.letter || IsOpenCell(plan, D, p))
					continue;
				for(Side s : {East, South})
				{
					QPoint q = Neighbour(p, s);
					if(whole(q) != room.letter || rectAt(p) == rectAt(q) || IsOpenCell(plan, D, q))
						continue;
					double bnd = s == East ? plan.X1(c) : plan.Y0(r);
					QString key = QString("%1-%2-%3-%4").arg(rectAt(p)).arg(rectAt(q)).arg(s).arg(bnd);
					VoidBox b;
					if(s == East)
					{
						double lo = qMax(plan.Y0(p.y()) + voidInset(p, South), plan.Y0(q.y()) + voidInset(q, South));
						double hi = qMin(plan.Y1(p.y()) - voidInset(p, North), plan.Y1(q.y()) - voidInset(q, North));
						b = VoidBox{bnd - VOID_OVERLAP, lo, bnd + VOID_OVERLAP, hi};
					}
					else
					{
						double lo = qMax(plan.X0(p.x()) + voidInset(p, West), plan.X0(q.x()) + voidInset(q, West));
						double hi = qMin(plan.X1(p.x()) - voidInset(p, East), plan.X1(q.x()) - voidInset(q, East));
						b = VoidBox{lo, bnd - VOID_OVERLAP, hi, bnd + VOID_OVERLAP};
					}
					if(connectors.contains(key))
					{
						VoidBox& cb = connectors[key];
						cb = VoidBox{qMin(cb.x0, b.x0), qMin(cb.y0, b.y0), qMax(cb.x1, b.x1), qMax(cb.y1, b.y1)};
					}
					else
						connectors.insert(key, b);
				}
			}
		for(const VoidBox& b : connectors)
		{
			if(air)
			{
				splitVoid(b, QString("room void connector %1 deck %2").arg(room.name).arg(D));
				continue;
			}
			QVector<V2> poly = rectPoly(b.x0, b.y0, b.x1, b.y1);
			addCarvedVoid(room, poly, poly, QString("room void connector %1 deck %2").arg(room.name).arg(D));
		}

		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint cell(c, r);
				if(whole(cell) != '*')
					continue;
				for(int q = 0; q < 4; q++)
				{
					if(plan.Quarter(D, cell, q) != room.letter || IsOpenQuarter(plan, D, cell, q))
						continue;
					addCarvedVoid(room, quarterVoidUncut(cell, q), QuarterPoly(plan, cell, q),
						QString("room void %1 deck %2 quarter %3,%4").arg(room.name).arg(D).arg(c).arg(r));
				}
			}
	}

}

void PlanBuilder::BuildOpenVoids()
{
	if(D > 0)
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint p(c, r);
				if(!IsOpenCell(plan, D, p))
					continue;
				const QChar spaceLetter = SpaceRoom(plan, D, p), belowLetter = plan.At(D - 1, p);
				if(!plan.rooms.contains(spaceLetter))
					continue;
				double inset[4];
				for(int s = 0; s < 4; s++)
				{
					QPoint q = Neighbour(p, Side(s));
					if(IsOpenQuarter(plan, D, q, Opposite(Side(s))) && SpaceRoom(plan, D, q, Opposite(Side(s))) == spaceLetter)
					{
						inset[s] = -VOID_OVERLAP;
						continue;
					}
					QChar here = facing(p, Side(s)), under = plan.Quarter(D - 1, q, Opposite(Side(s)));
					double onThis = here == whole(p) ? 0 : thickness(here) + VOID_INSET;
					double onBelow = under == belowLetter ? 0 : thickness(under) + VOID_INSET;
					inset[s] = qMax(onThis, onBelow);
				}
				AuScFaceList faces;
				AddBox(faces, plan.X0(c) + inset[West], plan.Y0(r) + inset[South], -H + VC - VOID_OVERLAP + ((c + r) % 2) * 0.0431,
					plan.X1(c) - inset[East], plan.Y1(r) - inset[North], VC);
				const Room& space = plan.rooms[spaceLetter];
				addRoomVoid(space, faces, QString("open void %1 deck %2 cell %3,%4").arg(space.name).arg(D).arg(c).arg(r));
			}

	if(D > 0)
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
				for(int q = 0; q < 4; q++)
				{
					QPoint cell(c, r);
					if(whole(cell) != '*' || !IsOpenQuarter(plan, D, cell, q))
						continue;
					const QChar spaceLetter = SpaceRoom(plan, D, cell, q);
					if(!plan.rooms.contains(spaceLetter))
						continue;
					const QChar here = plan.Quarter(D, cell, q), under = plan.Quarter(D - 1, cell, q);
					QVector<V2> t = QuarterPoly(plan, cell, q);
					V2 base[3], dir[3];
					for(int k = 0; k < 3; k++)
					{
						QPoint oc;
						int oq;
						QuarterAcross(cell, q, k, oc, oq);
						double shift;
						if(IsOpenQuarter(plan, D, oc, oq) && SpaceRoom(plan, D, oc, oq) == spaceLetter)
							shift = -VOID_OVERLAP;
						else
						{
							QChar other = plan.Quarter(D, oc, oq), otherBelow = plan.Quarter(D - 1, oc, oq);
							double onThis = other == here ? 0 : thickness(other) + VOID_INSET;
							double onBelow = otherBelow == under ? 0 : thickness(otherBelow) + VOID_INSET;
							shift = qMax(onThis, onBelow);
						}
						dir[k] = (t[(k + 1) % 3] - t[k]).Unit();
						base[k] = t[k] + dir[k].Left() * shift;
					}
					QVector<V2> v;
					for(int k = 0; k < 3; k++)
						v << Intersect(base[(k + 2) % 3], dir[(k + 2) % 3], base[k], dir[k]);
					AuScFaceList faces;
					const bool eastWest = q == East || q == West;
					const double stagger = eastWest ? ((c + r) % 2 ? 0.0647 : 0.0213) : ((c + r) % 2 ? 0.0431 : 0);
					AddPrism(faces, v, -H + VC - VOID_OVERLAP + stagger, VC);
					const Room& space = plan.rooms[spaceLetter];
					addRoomVoid(space, faces, QString("open void %1 deck %2 quarter %3,%4").arg(space.name).arg(D).arg(c).arg(r));
				}

	for(const PlacedStair& ps : placedStairs)
	{
		if(ps.stair.deck != D - 1)
			continue;
		const StairFrame& fr = ps.frames.last();
		const QChar wellLetter = IsOpenCell(plan, D, ps.layout.top) ? SpaceRoom(plan, D, ps.layout.top) : QChar('.');
		const QChar arrivalLetter = whole(ps.layout.arrival);
		AuScFaceList back, onto;
		SlopedBar(back, fr, -fr.w / 2 - 0.05, fr.w / 2 + 0.05, fr.len - (plan.wall + VOID_INSET + VOID_OVERLAP), fr.len,
			-0.05, VC, -0.05, VC);
		SlopedBar(onto, fr, -fr.w / 2 - 0.05, fr.w / 2 + 0.05, fr.len - 0.02, fr.len + VOID_OVERLAP, -0.05, VC, -0.05, VC);
		if(plan.rooms.contains(wellLetter))
		{
			const Room& well = plan.rooms[wellLetter];
			addRoomVoid(well, back, QString("stair top void %1 deck %2").arg(well.name).arg(D));
		}
		if(plan.rooms.contains(arrivalLetter))
		{
			const Room& arrival = plan.rooms[arrivalLetter];
			addRoomVoid(arrival, onto, QString("stair top void %1 deck %2").arg(arrival.name).arg(D));
		}
	}

}

}
