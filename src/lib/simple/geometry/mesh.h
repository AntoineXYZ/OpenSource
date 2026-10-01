#pragma once

#include "../build/common.h"
#include "v2.h"

namespace SimpleMode::detail {

AuPoint Ft(double x, double y, double z);

AuPoint Ft(const V2& p, double z);

AuPoint FtP(const AuPoint& p);

AuPoint Unit3(double x, double y, double z);

V2 Intersect(const V2& a, const V2& u, const V2& b, const V2& v);

QVector<V2> ClipHalf(const QVector<V2>& poly, const V2& n, double k);

QVector<V2> ClipLeftOf(const QVector<V2>& poly, const V2& a, const V2& u, double dist);

double Area(const QVector<V2>& poly);

void AddQuad(AuScFaceList& faces, const AuPoint& ll, const AuPoint& ul, const AuPoint& ur, const AuPoint& lr, const AuPoint& n);

void AddTri(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& n);

enum BoxSide { BoxBottom = 1, BoxTop = 2, BoxBack = 4, BoxFront = 8, BoxStart = 16, BoxEnd = 32 };

void AddBox(AuScFaceList& faces, double x0, double y0, double z0, double x1, double y1, double z1, int skip = 0);

void AddWallBox(AuScFaceList& faces, const V2& o, const V2& u, const V2& n, double from, double to, double w, double z0, double z1,
	int skip = 0);

void AddPrism(AuScFaceList& faces, const QVector<V2>& poly, double z0, double z1);

struct Hole
{
	double s0, s1, z0, z1;
};

void AddWall(AuScFaceList& faces, const V2& a, const V2& b, double z0, double z1, const V2& n, QList<QPair<double, double>> openings,
	const QList<Hole>& windows = QList<Hole>());

void Paint(AuScFaceList& faces, const QColor& color);

void TextureFaces(AuScFaceList& faces, int slot, double size);
void TextureBySide(AuScFaceList& faces, int up, double upSize, int side, double sideSize, int down, double downSize);
QColor Tint(const QColor& color, double amount);

}
