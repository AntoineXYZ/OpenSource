#pragma once

#include <QPoint>
#include "types.h"

namespace SimpleMode {

struct Plan;

QPoint Neighbour(const QPoint& p, Side s);
Side Opposite(Side s);

void QuarterEdge(const QPoint& cell, int quarter, int k, QPoint& otherCell, int& otherQuarter, Line& line);

void QuarterCornerPoints(const QPoint& cell, int quarter, QPoint corners[3]);

bool QuarterAtPoint(const Plan& plan, double x, double y, QPoint& cell, int& quarter);

}
