#include "grid.h"
#include "../build/internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode {

QPoint Neighbour(const QPoint& p, Side s)
{
	switch(s)
	{
	case North: return p + QPoint(0, -1);
	case East: return p + QPoint(1, 0);
	case South: return p + QPoint(0, 1);
	default: return p + QPoint(-1, 0);
	}
}

Side Opposite(Side s)
{
	static const Side opposite[] = {South, West, North, East};
	return opposite[s];
}

void QuarterEdge(const QPoint& cell, int quarter, int k, QPoint& otherCell, int& otherQuarter, Line& line)
{
	QuarterAcross(cell, quarter, k, otherCell, otherQuarter);
	if(otherCell != cell)
		line = (quarter == North || quarter == South) ? Horizontal : Vertical;
	else
	{
		int a = qMin(quarter, otherQuarter), b = qMax(quarter, otherQuarter);
		line = ((a == North && b == West) || (a == East && b == South)) ? Backslash : Slash;
	}
}

void QuarterCornerPoints(const QPoint& cell, int quarter, QPoint corners[3])
{
	QuarterCorners(cell, quarter, corners);
}

bool QuarterAtPoint(const Plan& plan, double x, double y, QPoint& cell, int& quarter)
{
	double fx = (x - plan.X0(0)) / plan.cell, fy = (plan.Y1(0) - y) / plan.cell;
	int c = qFloor(fx), r = qFloor(fy);
	cell = QPoint(c, r);
	if(!plan.OnGrid(cell))
		return false;
	double lx = fx - c, ly = fy - r;
	if(ly < lx && ly < 1 - lx)
		quarter = North;
	else if(ly > lx && ly > 1 - lx)
		quarter = South;
	else if(lx < ly && lx < 1 - ly)
		quarter = West;
	else
		quarter = East;
	return true;
}

}
