#include "mesh.h"
#include "../build/internal.h"

#define EPS 1e-6

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

AuPoint Ft(double x, double y, double z)
{
	const double f = FeetPerMeter();
	return AuPoint(x * f, y * f, z * f);
}

AuPoint Ft(const V2& p, double z)
{
	return Ft(p.x, p.y, z);
}

AuPoint FtP(const AuPoint& p)
{
	return Ft(p.x, p.y, p.z);
}

AuPoint Unit3(double x, double y, double z)
{
	double l = qSqrt(x * x + y * y + z * z);
	return AuPoint(x / l, y / l, z / l);
}

V2 Intersect(const V2& a, const V2& u, const V2& b, const V2& v)
{
	double d = u.Cross(v);
	double s = (b - a).Cross(v) / d;
	return a + u * s;
}

QVector<V2> ClipHalf(const QVector<V2>& poly, const V2& n, double k)
{
	auto side = [&](const V2& p) { return n.Dot(p) - k; };
	QVector<V2> out;
	for(int i = 0; i < poly.size(); i++)
	{
		const V2& p = poly[i];
		const V2& q = poly[(i + 1) % poly.size()];
		double sp = side(p), sq = side(q);
		if(sp >= 0)
			out << p;
		if((sp >= 0) != (sq >= 0))
			out << p + (q - p) * (sp / (sp - sq));
	}
	return out;
}

QVector<V2> ClipLeftOf(const QVector<V2>& poly, const V2& a, const V2& u, double dist)
{
	return ClipHalf(poly, u.Left(), u.Left().Dot(a) + dist);
}

double Area(const QVector<V2>& poly)
{
	double a = 0;
	for(int i = 0; i < poly.size(); i++)
		a += poly[i].Cross(poly[(i + 1) % poly.size()]);
	return a / 2;
}

void AddQuad(AuScFaceList& faces, const AuPoint& ll, const AuPoint& ul, const AuPoint& ur, const AuPoint& lr, const AuPoint& n)
{
	auto make = [&](const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d) {
		const double f = FeetPerMeter();
		double w = (d - a).Length() / f;
		double h = (b - a).Length() / f;
		return new AuScFaceQuad(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 0.f, float(h), n),
				AuScPoint(c, float(w), float(h), n), AuScPoint(d, float(w), 0.f, n));
	};
	AuScFace* face = make(ll, ul, ur, lr);
	AuScTriList tris;
	face->Triangles(tris);
	if(!tris.isEmpty() && tris.first().Normal().Dot(n) < 0)
	{
		delete face;
		face = make(lr, ur, ul, ll);
	}
	faces.append(face);
}

void AddTri(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& n)
{
	auto make = [&](const AuPoint& p, const AuPoint& q, const AuPoint& r) {
		const double f = FeetPerMeter();
		return new AuScFaceTri(AuScPoint(p, 0.f, 0.f, n), AuScPoint(q, float((q - p).Length() / f), 0.f, n),
				AuScPoint(r, 0.f, float((r - p).Length() / f), n));
	};
	AuScFace* face = make(a, b, c);
	AuScTriList tris;
	face->Triangles(tris);
	if(!tris.isEmpty() && tris.first().Normal().Dot(n) < 0)
	{
		delete face;
		face = make(a, c, b);
	}
	faces.append(face);
}

void AddBox(AuScFaceList& faces, double x0, double y0, double z0, double x1, double y1, double z1, int skip)
{
	if(!(skip & BoxBottom))
		AddQuad(faces, Ft(x0,y0,z0), Ft(x0,y1,z0), Ft(x1,y1,z0), Ft(x1,y0,z0), AuPoint(0,0,-1));
	if(!(skip & BoxTop))
		AddQuad(faces, Ft(x0,y0,z1), Ft(x0,y1,z1), Ft(x1,y1,z1), Ft(x1,y0,z1), AuPoint(0,0,1));
	if(!(skip & BoxStart))
		AddQuad(faces, Ft(x0,y0,z0), Ft(x0,y0,z1), Ft(x1,y0,z1), Ft(x1,y0,z0), AuPoint(0,-1,0));
	if(!(skip & BoxEnd))
		AddQuad(faces, Ft(x0,y1,z0), Ft(x0,y1,z1), Ft(x1,y1,z1), Ft(x1,y1,z0), AuPoint(0,1,0));
	if(!(skip & BoxBack))
		AddQuad(faces, Ft(x0,y0,z0), Ft(x0,y0,z1), Ft(x0,y1,z1), Ft(x0,y1,z0), AuPoint(-1,0,0));
	if(!(skip & BoxFront))
		AddQuad(faces, Ft(x1,y0,z0), Ft(x1,y0,z1), Ft(x1,y1,z1), Ft(x1,y1,z0), AuPoint(1,0,0));
}

void AddWallBox(AuScFaceList& faces, const V2& o, const V2& u, const V2& n, double from, double to, double w, double z0, double z1,
	int skip)
{
	double lo = qMin(from, to), hi = qMax(from, to);
	V2 a = o + u * (-w / 2), b = o + u * (w / 2);
	V2 a0 = a + n * lo, a1 = a + n * hi, b0 = b + n * lo, b1 = b + n * hi;
	AuPoint nu(u.x, u.y, 0), nn(n.x, n.y, 0);
	if(!(skip & BoxBottom))
		AddQuad(faces, Ft(a0,z0), Ft(a1,z0), Ft(b1,z0), Ft(b0,z0), AuPoint(0,0,-1));
	if(!(skip & BoxTop))
		AddQuad(faces, Ft(a0,z1), Ft(a1,z1), Ft(b1,z1), Ft(b0,z1), AuPoint(0,0,1));
	if(!(skip & BoxBack))
		AddQuad(faces, Ft(a0,z0), Ft(a0,z1), Ft(b0,z1), Ft(b0,z0), nn * -1);
	if(!(skip & BoxFront))
		AddQuad(faces, Ft(a1,z0), Ft(a1,z1), Ft(b1,z1), Ft(b1,z0), nn);
	if(!(skip & BoxStart))
		AddQuad(faces, Ft(a0,z0), Ft(a0,z1), Ft(a1,z1), Ft(a1,z0), nu * -1);
	if(!(skip & BoxEnd))
		AddQuad(faces, Ft(b0,z0), Ft(b0,z1), Ft(b1,z1), Ft(b1,z0), nu);
}

void AddPrism(AuScFaceList& faces, const QVector<V2>& poly, double z0, double z1)
{
	for(int i = 1; i + 1 < poly.size(); i++)
	{
		AddTri(faces, Ft(poly[0], z0), Ft(poly[i], z0), Ft(poly[i + 1], z0), AuPoint(0,0,-1));
		AddTri(faces, Ft(poly[0], z1), Ft(poly[i], z1), Ft(poly[i + 1], z1), AuPoint(0,0,1));
	}
	for(int i = 0; i < poly.size(); i++)
	{
		const V2& a = poly[i];
		const V2& b = poly[(i + 1) % poly.size()];
		V2 out = (b - a).Unit().Left() * -1;
		AddQuad(faces, Ft(a, z0), Ft(a, z1), Ft(b, z1), Ft(b, z0), AuPoint(out.x, out.y, 0));
	}
}

void AddWall(AuScFaceList& faces, const V2& a, const V2& b, double z0, double z1, const V2& n, QList<QPair<double, double>> openings,
	const QList<Hole>& windows)
{
	V2 u = (b - a).Unit();
	double len = (b - a).Length();
	AuPoint nn(n.x, n.y, 0);
	auto piece = [&](double s0, double s1, double h0, double h1) {
		if(s1 - s0 < EPS || h1 - h0 < EPS)
			return;
		V2 p0 = a + u * s0, p1 = a + u * s1;
		AddQuad(faces, Ft(p0, h0), Ft(p0, h1), Ft(p1, h1), Ft(p1, h0), nn);
	};
	QList<Hole> holes = windows;
	for(const QPair<double, double>& o : openings)
		holes << Hole{o.first, o.second, z0, z0 + DOOR_HEIGHT};
	QList<double> cuts = {0, len};
	for(const Hole& h : holes)
		cuts << qBound(0.0, h.s0, len) << qBound(0.0, h.s1, len);
	std::sort(cuts.begin(), cuts.end());
	for(int i = 0; i + 1 < cuts.size(); i++)
	{
		double c0 = cuts[i], c1 = cuts[i + 1];
		if(c1 - c0 < 1e-6)
			continue;
		QList<QPair<double, double>> gaps;
		for(const Hole& h : holes)
			if(h.s0 < (c0 + c1) / 2 && h.s1 > (c0 + c1) / 2)
				gaps << qMakePair(qMax(z0, h.z0), qMin(z1, h.z1));
		std::sort(gaps.begin(), gaps.end());
		double z = z0;
		for(const QPair<double, double>& g : gaps)
		{
			piece(c0, c1, z, g.first);
			z = qMax(z, g.second);
		}
		piece(c0, c1, z, z1);
	}
}

void Paint(AuScFaceList& faces, const QColor& color)
{
	for(AuScFace* face : faces)
		face->SetPaint(color);
}

static void TextureFace(AuScFace* face, int slot, double size)
{
	static const AuMatrix4x4 fromAbove, fromEast(AuPoint(0, 0, 0), AuPoint(0, 1, 0), AuPoint(0, 0, 1)),
		fromSouth(AuPoint(0, 0, 0), AuPoint(1, 0, 0), AuPoint(0, 0, 1));
	const AuPoint n = face->FaceNormal();
	const AuMatrix4x4& plane = qAbs(n.z) >= 0.7 ? fromAbove : qAbs(n.x) >= qAbs(n.y) ? fromEast : fromSouth;
	face->TextureProject(quint8(slot), size * FeetPerMeter(), plane);
}

void TextureFaces(AuScFaceList& faces, int slot, double size)
{
	if(slot < 0)
		return;
	for(AuScFace* face : faces)
		TextureFace(face, slot, size);
}

void TextureBySide(AuScFaceList& faces, int up, double upSize, int side, double sideSize, int down, double downSize)
{
	for(AuScFace* face : faces)
	{
		const double nz = face->FaceNormal().z;
		const int slot = nz >= 0.7 ? up : nz <= -0.7 ? down : side;
		if(slot >= 0)
			TextureFace(face, slot, nz >= 0.7 ? upSize : nz <= -0.7 ? downSize : sideSize);
	}
}

QColor Tint(const QColor& color, double amount)
{
	QColor c = color;
	c.setAlphaF(qBound(0.0, amount, 1.0));
	return c;
}

}
