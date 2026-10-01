#include "internal.h"

namespace SimpleMode::detail {

AuPoint BuildContext::up(const AuPoint& p, int deck) const
{
	return p + AuPoint(0, 0, deck * H * FeetPerMeter());
}

QChar BuildContext::whole(const QPoint& p) const
{
	return plan.At(D, p);
}

QChar BuildContext::facing(const QPoint& p, Side s) const
{
	return plan.Quarter(D, Neighbour(p, s), Opposite(s));
}

double BuildContext::thickness(QChar other) const
{
	return other == '.' || IsOpenAirRoom(plan, other) ? plan.wall : plan.partition / 2;
}

double BuildContext::voidInset(const QPoint& p, Side s) const
{
	QChar other = facing(p, s);
	if(IsOpenEdge(plan, whole(p), other))
		return OpenEdgeInset(plan, whole(p)) + VOID_INSET;
	if(IsOpenAirRoom(plan, whole(p)) && other != '.' && !IsOpenAirRoom(plan, other))
		return SHELL_INSET + VOID_INSET;
	return other == whole(p) ? 0.0 : thickness(other) + VOID_INSET;
}

bool BuildContext::stairCellOn(int deck, const QPoint& p) const
{
	for(const PlacedStair& ps : placedStairs)
		if(ps.stair.deck == deck && ps.layout.cells.contains(p))
			return true;
	return false;
}

double BuildContext::stairSurface(int deck, const V2& p) const
{
	double best = -1;
	for(const PlacedStair& ps : placedStairs)
	{
		if(ps.stair.deck != deck)
			continue;
		for(int i = 0; i < ps.frames.size(); i++)
		{
			const StairFrame& fr = ps.frames[i];
			double t = (p - fr.s0).Dot(fr.F), a = (p - fr.s0).Dot(fr.R);
			const StairLayout::Flight& f = ps.layout.flights[i];
			if(t >= 0 && t <= fr.len && qAbs(a) <= fr.w / 2 + 0.05)
				best = qMax(best, f.z0 + (f.z1 - f.z0) * t / fr.len);
		}
		for(const PlacedStair::Landing& l : ps.landings)
			if(p.x >= l.x0 && p.x <= l.x1 && p.y >= l.y0 && p.y <= l.y1)
				best = qMax(best, l.z);
	}
	return best;
}

double BuildContext::midX(const QPoint& p) const
{
	return (plan.X0(p.x()) + plan.X1(p.x())) / 2;
}

double BuildContext::midY(const QPoint& p) const
{
	return (plan.Y0(p.y()) + plan.Y1(p.y())) / 2;
}

QList<Piece> BuildContext::piecesOf(int deck) const
{
	QList<Piece> pieces;
	for(int r = 0; r < plan.Rows(); r++)
		for(int c = 0; c < plan.Cols(); c++)
		{
			QPoint cell(c, r);
			QChar w = plan.At(deck, cell);
			if(w == '.' || IsOpenCell(plan, deck, cell) || stairCellOn(deck, cell) || IsLiftRoom(plan, w))
				continue;
			if(w != '*')
			{
				pieces << Piece{cell, -1, w, V2(midX(cell), midY(cell))};
				continue;
			}
			for(int q = 0; q < 4; q++)
			{
				QChar ch = plan.Quarter(deck, cell, q);
				if(ch == '.' || IsOpenQuarter(plan, deck, cell, q))
					continue;
				QVector<V2> t = QuarterPoly(plan, cell, q);
				pieces << Piece{cell, q, ch, (t[0] + t[1] + t[2]) * (1.0 / 3)};
			}
		}
	return pieces;
}

const QList<QList<Edge>>& BuildContext::outlineOf(int deck, QChar room)
{
	QPair<int, QChar> key(deck, room);
	if(!outlines.contains(key))
		outlines.insert(key, RoomOutlines(plan, deck, room));
	return outlines[key];
}

}
