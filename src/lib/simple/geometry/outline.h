#pragma once

#include "../build/common.h"
#include "v2.h"

namespace SimpleMode::detail {

struct Edge
{
	V2 a, b;
	QChar other;
	double inset;
	bool open = false;
	bool shell = false;
};

void QuarterCorners(const QPoint& cell, int q, QPoint corners[3]);

void QuarterAcross(const QPoint& cell, int q, int k, QPoint& otherCell, int& otherQ);

V2 LatticeM(const Plan& plan, const QPoint& p);

QList<QList<Edge>> RoomOutlines(const Plan& plan, int deck, QChar room);

QVector<V2> QuarterPoly(const Plan& plan, const QPoint& cell, int q);

}
