#include "outline.h"
#include "../build/internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

void QuarterCorners(const QPoint& cell, int q, QPoint corners[3])
{
	int c = cell.x() * 2, r = cell.y() * 2;
	QPoint tl(c, r), tr(c + 2, r), bl(c, r + 2), br(c + 2, r + 2), mid(c + 1, r + 1);
	switch(q)
	{
	case North: corners[0] = tl; corners[1] = mid; corners[2] = tr; break;
	case East: corners[0] = mid; corners[1] = br; corners[2] = tr; break;
	case South: corners[0] = bl; corners[1] = br; corners[2] = mid; break;
	default: corners[0] = tl; corners[1] = bl; corners[2] = mid; break;
	}
}

void QuarterAcross(const QPoint& cell, int q, int k, QPoint& otherCell, int& otherQ)
{
	static const int same[4][3] = {
		{West, East, -1},
		{South, -1, North},
		{-1, East, West},
		{-1, South, North},
	};
	int s = same[q][k];
	if(s >= 0)
	{
		otherCell = cell;
		otherQ = s;
	}
	else
	{
		otherCell = Neighbour(cell, (Side)q);
		otherQ = Opposite((Side)q);
	}
}

V2 LatticeM(const Plan& plan, const QPoint& p)
{
	return V2(plan.X0(0) + p.x() * plan.cell / 2, plan.Y1(0) - p.y() * plan.cell / 2);
}

QList<QList<Edge>> RoomOutlines(const Plan& plan, int deck, QChar room)
{
	struct Seg { QPoint a, b; QChar other; bool used; };
	QVector<Seg> segs;
	for(int r = 0; r < plan.Rows(); r++)
		for(int c = 0; c < plan.Cols(); c++)
			for(int q = 0; q < 4; q++)
			{
				QPoint cell(c, r);
				if(plan.Quarter(deck, cell, q) != room)
					continue;
				QPoint corners[3];
				QuarterCorners(cell, q, corners);
				for(int k = 0; k < 3; k++)
				{
					QPoint oc;
					int oq;
					QuarterAcross(cell, q, k, oc, oq);
					QChar other = plan.Quarter(deck, oc, oq);
					if(other != room)
						segs << Seg{corners[k], corners[(k + 1) % 3], other, false};
				}
			}
	QMultiHash<QPair<int, int>, int> starts;
	for(int i = 0; i < segs.size(); i++)
		starts.insert(qMakePair(segs[i].a.x(), segs[i].a.y()), i);

	QList<QList<Edge>> loops;
	for(int first = 0; first < segs.size(); first++)
	{
		if(segs[first].used)
			continue;
		QList<int> chain;
		int cur = first;
		while(cur >= 0 && !segs[cur].used)
		{
			segs[cur].used = true;
			chain << cur;
			V2 inDir = LatticeM(plan, segs[cur].b) - LatticeM(plan, segs[cur].a);
			int best = -1;
			double bestTurn = -10;
			for(int j : starts.values(qMakePair(segs[cur].b.x(), segs[cur].b.y())))
			{
				if(segs[j].used)
					continue;
				V2 outDir = LatticeM(plan, segs[j].b) - LatticeM(plan, segs[j].a);
				double turn = qAtan2(inDir.Cross(outDir), inDir.Dot(outDir));
				if(turn > bestTurn)
				{
					bestTurn = turn;
					best = j;
				}
			}
			cur = best;
		}
		QList<Edge> loop;
		for(int i : chain)
		{
			Edge e{LatticeM(plan, segs[i].a), LatticeM(plan, segs[i].b), segs[i].other,
				segs[i].other == '.' ? plan.wall : plan.partition / 2};
			if(IsOpenEdge(plan, room, segs[i].other))
			{
				e.open = true;
				e.inset = OpenEdgeInset(plan, room);
			}
			else if(IsOpenAirRoom(plan, room) && segs[i].other != '.' && !IsOpenAirRoom(plan, segs[i].other))
			{
				e.shell = true;
				e.inset = SHELL_INSET;
			}
			else if(!IsOpenAirRoom(plan, room) && IsOpenAirRoom(plan, segs[i].other))
				e.inset = plan.wall;
			if(!loop.isEmpty())
			{
				Edge& last = loop.last();
				V2 u0 = (last.b - last.a).Unit(), u1 = (e.b - e.a).Unit();
				if(qAbs(u0.Cross(u1)) < 1e-9 && u0.Dot(u1) > 0 && qAbs(last.inset - e.inset) < 1e-9 && last.open == e.open && last.shell == e.shell && (last.b - e.a).Length() < 1e-9)
				{
					last.b = e.b;
					continue;
				}
			}
			loop << e;
		}
		if(loop.size() > 1)
		{
			Edge& last = loop.last();
			Edge& head = loop.first();
			V2 u0 = (last.b - last.a).Unit(), u1 = (head.b - head.a).Unit();
			if(qAbs(u0.Cross(u1)) < 1e-9 && u0.Dot(u1) > 0 && qAbs(last.inset - head.inset) < 1e-9 && last.open == head.open && last.shell == head.shell)
			{
				head.a = last.a;
				loop.removeLast();
			}
		}
		if(!loop.isEmpty())
			loops << loop;
	}
	return loops;
}

QVector<V2> QuarterPoly(const Plan& plan, const QPoint& cell, int q)
{
	QPoint corners[3];
	QuarterCorners(cell, q, corners);
	return {LatticeM(plan, corners[0]), LatticeM(plan, corners[1]), LatticeM(plan, corners[2])};
}

}
