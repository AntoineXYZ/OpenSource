#include "roof.h"
#include "internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

QColor RoofPaint(Theme theme)
{
	return theme == Stone ? QColor(0x55, 0x5c, 0x66) : theme == Metal ? QColor(0x7c, 0x88, 0x92) : QColor(0x6a, 0x4a, 0x3c);
}

const Skylight* SkylightAt(const Plan& plan, int deck, const QPoint& cell)
{
	for(int i = 0; i < plan.skylights.size(); i++)
		if(plan.skylights[i].deck == deck && plan.skylights[i].cell == cell && SkylightProblem(plan, plan.skylights[i], i).isEmpty())
			return &plan.skylights[i];
	return nullptr;
}

void SkylightHole(const Plan& plan, const QPoint& cell, double& x0, double& y0, double& x1, double& y1)
{
	const double m = plan.cell / 4;
	x0 = plan.X0(cell.x()) + m;
	x1 = plan.X1(cell.x()) - m;
	y0 = plan.Y0(cell.y()) + m;
	y1 = plan.Y1(cell.y()) - m;
}

}
