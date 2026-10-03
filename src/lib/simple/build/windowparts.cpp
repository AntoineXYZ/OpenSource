#include "windowparts.h"
#include "internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

double GlassAcross(const WindowGeo& g)
{
	return g.hull ? g.inThere - 0.03 : 0;
}

void PaintWindowMesh(WindowMesh& m, const WindowGeo& g)
{
	QColor frame = g.tech == 1 ? QColor(0x6e, 0x46, 0x26)
		: g.tech == 2 ? (g.theme == Stone ? QColor(0x9a, 0x96, 0x8c) : g.theme == Metal ? QColor(0x7a, 0x80, 0x88) : QColor(0x7a, 0x52, 0x30))
		: g.tech == 3 ? QColor(0xe8, 0xe4, 0xda) : QColor(0x3a, 0x3e, 0x44);
	Paint(m.frame, frame);
	Paint(m.dark, QColor(0x2c, 0x2d, 0x31));
	Paint(m.shutter, QColor(0x3e, 0x6a, 0x4a));
	Paint(m.glow, QColor(0x40, 0xe0, 0xff));
	for(AuScFace* face : m.glow)
		face->SetSelfIllumination(1.f);
}

void WindowFrames(WindowMesh& out, const WindowGeo& g, int side)
{
	const double w = g.width, h0 = g.sill, h1 = g.head, face = side < 0 ? -g.inHere : g.inThere;
	const double glass = GlassAcross(g);
	auto box = [&](AuScFaceList& f, double s0, double s1, double z0, double z1, double a0, double a1) {
		AddWallBox(f, g.m + g.u * ((s0 + s1) / 2), g.u, g.n, a0, a1, s1 - s0, z0, z1);
	};
	auto out1 = [&](double depth) { return face + side * depth; };
	const bool outer = side > 0;
	switch(g.tech)
	{
	case 1:
		box(out.frame, -w / 2 - 0.1, w / 2 + 0.1, h1, h1 + 0.12, face, out1(0.05));
		if(outer)
		{
			box(out.frame, -w / 2 - 0.05, w / 2 + 0.05, h0 - 0.06, h0, face, out1(0.06));
			if(w >= 0.5 && w <= 1.2)
				for(double end : {-1.0, 1.0})
				{
					double s0 = end * (w / 2 + 0.02), s1 = end * (w / 2 + 0.02 + w / 2);
					box(out.shutter, qMin(s0, s1), qMax(s0, s1), h0, h1, out1(0.02), out1(0.05));
					for(double k : {0.25, 0.75})
						box(out.frame, qMin(s0, s1) + 0.03, qMax(s0, s1) - 0.03, h0 + k * (h1 - h0) - 0.04, h0 + k * (h1 - h0) + 0.04,
							out1(0.05), out1(0.065));
				}
		}
		break;
	case 2:
		if(outer)
		{
			box(out.frame, -w / 2 - 0.1, -w / 2, h0 - 0.08, h1 + 0.1, face, out1(0.05));
			box(out.frame, w / 2, w / 2 + 0.1, h0 - 0.08, h1 + 0.1, face, out1(0.05));
			box(out.frame, -w / 2 - 0.1, w / 2 + 0.1, h1, h1 + 0.12, face, out1(0.06));
			box(out.frame, -w / 2 - 0.12, w / 2 + 0.12, h0 - 0.08, h0, face, out1(0.07));
		}
		for(double s = -w / 2 + 0.15; s < w / 2 - 0.05; s += 0.15)
			box(out.dark, s - 0.006, s + 0.006, h0, h1, glass + side * 0.004, glass + side * 0.012);
		for(double z = h0 + 0.15; z < h1 - 0.05; z += 0.15)
			box(out.dark, -w / 2, w / 2, z - 0.006, z + 0.006, glass + side * 0.004, glass + side * 0.012);
		break;
	case 3:
	{
		if(outer)
		{
			const double fw = 0.06;
			box(out.frame, -w / 2, -w / 2 + fw, h0, h1, glass - 0.04, glass + 0.04);
			box(out.frame, w / 2 - fw, w / 2, h0, h1, glass - 0.04, glass + 0.04);
			box(out.frame, -w / 2, w / 2, h1 - fw, h1, glass - 0.04, glass + 0.04);
			box(out.frame, -w / 2, w / 2, h0, h0 + fw, glass - 0.04, glass + 0.04);
			int cols = qMax(1, qRound(w / 0.5)), rows = qMax(1, qRound((h1 - h0) / 0.6));
			for(int c = 1; c < cols; c++)
			{
				double s = -w / 2 + w * c / cols;
				box(out.frame, s - 0.015, s + 0.015, h0, h1, glass - 0.025, glass + 0.025);
			}
			for(int r = 1; r < rows; r++)
			{
				double z = h0 + (h1 - h0) * r / rows;
				box(out.frame, -w / 2, w / 2, z - 0.015, z + 0.015, glass - 0.025, glass + 0.025);
			}
			if(g.hull)
				box(out.frame, -w / 2 - 0.05, w / 2 + 0.05, h0 - 0.05, h0, face, out1(0.07));
		}
		else
			box(out.frame, -w / 2 - 0.03, w / 2 + 0.03, h0 - 0.03, h0, face, out1(0.06));
		break;
	}
	case 4:
		if(outer)
		{
			const double fw = 0.035;
			box(out.frame, -w / 2, -w / 2 + fw, h0, h1, glass - 0.03, glass + 0.03);
			box(out.frame, w / 2 - fw, w / 2, h0, h1, glass - 0.03, glass + 0.03);
			box(out.frame, -w / 2, w / 2, h1 - fw, h1, glass - 0.03, glass + 0.03);
			box(out.frame, -w / 2, w / 2, h0, h0 + fw, glass - 0.03, glass + 0.03);
			if(w > 1.5)
				box(out.frame, -0.02, 0.02, h0, h1, glass - 0.03, glass + 0.03);
			if(g.hull)
				box(out.frame, -w / 2 - 0.02, w / 2 + 0.02, h0 - 0.03, h0, face, out1(0.05));
		}
		break;
	default:
		for(double z : {h0, h1 - 0.02})
			box(out.glow, -w / 2, w / 2, z, z + 0.02, glass + side * 0.004, glass + side * 0.02);
		for(double s : {-w / 2, w / 2 - 0.02})
			box(out.glow, s, s + 0.02, h0, h1, glass + side * 0.004, glass + side * 0.02);
		break;
	}
}

void WindowGlass(AuScFaceList& glass, const WindowGeo& g)
{
	V2 p = g.m + g.n * GlassAcross(g), a = p - g.u * (g.width / 2), b = p + g.u * (g.width / 2);
	for(double side : {-1.0, 1.0})
		AddQuad(glass, Ft(a, g.sill), Ft(a, g.head), Ft(b, g.head), Ft(b, g.sill), AuPoint(g.n.x * side, g.n.y * side, 0));
	QColor tint = g.tech == 2 ? QColor(0x9a, 0xc8, 0xa8) : g.tech == 3 ? QColor(0xb0, 0xd0, 0xe0)
		: g.tech == 4 ? QColor(0xc8, 0xe0, 0xf0) : QColor(0x60, 0x98, 0xe0);
	Paint(glass, tint);
}

void ArchSpandrels(AuScFaceList& f, const V2& m, const V2& u, const V2& n, double width, double spring, double top, double from,
	double to)
{
	const double a = width / 2, rise = top - spring;
	if(rise < 0.05)
		return;
	const double R = (a * a + rise * rise) / (2 * rise), zc = top - R;
	const int segments = 16;
	auto curve = [&](double s) { return zc + qSqrt(qMax(0.0, R * R - s * s)); };
	auto at = [&](double s, double across, double z) { return Ft(m + u * s + n * across, z); };
	for(int k = 0; k < segments; k++)
	{
		double s0 = -a + 2 * a * k / segments, s1 = -a + 2 * a * (k + 1) / segments, z0 = curve(s0), z1 = curve(s1);
		AddQuad(f, at(s0, from, z0), at(s0, from, top), at(s1, from, top), at(s1, from, z1), AuPoint(-n.x, -n.y, 0));
		AddQuad(f, at(s0, to, z0), at(s0, to, top), at(s1, to, top), at(s1, to, z1), AuPoint(n.x, n.y, 0));
		double sm = (s0 + s1) / 2, zm = (z0 + z1) / 2;
		AuPoint down(-u.x * sm, -u.y * sm, -(zm - zc));
		down = down * (1 / qMax(1e-9, down.Length()));
		AddQuad(f, at(s0, from, z0), at(s1, from, z1), at(s1, to, z1), at(s0, to, z0), down);
	}
}

void RailingMesh(AuScFaceList& main, AuScFaceList& glass, const V2& a, const V2& b, const V2& n, QString style,
	double base, double floorPast)
{
	const V2 u = (b - a).Unit();
	const double L = (b - a).Length();
	auto box = [&](double s0, double s1, double w, double z0, double z1, int skip) {
		if(z0 < 1e-9 && w / 2 > floorPast + 1e-9)
			skip &= ~BoxBottom;
		AddWallBox(main, a + u * ((s0 + s1) / 2), u, n, -w / 2, w / 2, s1 - s0, base + z0, base + z1, skip);
	};
	auto uprights = [&](double s0, double s1, double gap, double w, double z0, double z1, int skip) {
		int count = qMax(1, int((s1 - s0) / gap));
		for(int i = 1; i < count; i++)
		{
			double s = s0 + (s1 - s0) * i / count;
			box(s - w / 2, s + w / 2, w, z0, z1, skip);
		}
	};
	const int ends = BoxStart | BoxEnd, between = BoxBottom | BoxTop;
	if(style == "balustrade")
	{
		box(0, L, 0.22, 0, 0.18, BoxBottom);
		box(0, 0.24, 0.24, 0.18, 1.05, BoxBottom);
		box(L - 0.24, L, 0.24, 0.18, 1.05, BoxBottom);
		uprights(0.24, L - 0.24, 0.22, 0.08, 0.18, 0.85, between);
		uprights(0.24, L - 0.24, 0.22, 0.12, 0.42, 0.6, 0);
		box(0.24, L - 0.24, 0.22, 0.85, 1.0, ends);
	}
	else if(style == "bars")
	{
		box(0, 0.05, 0.05, 0, 1.05, BoxBottom);
		box(L - 0.05, L, 0.05, 0, 1.05, BoxBottom);
		box(0.05, L - 0.05, 0.06, 0.98, 1.02, 0);
		box(0.05, L - 0.05, 0.03, 0.05, 0.08, ends);
		uprights(0.05, L - 0.05, 0.1, 0.016, 0.08, 0.98, between);
	}
	else if(style == "glass")
	{
		int panels = qMax(1, int(std::ceil((L - 0.05) / 1.2)));
		for(int i = 0; i <= panels; i++)
		{
			double s = qBound(0.025, L * i / panels, L - 0.025);
			box(s - 0.025, s + 0.025, 0.05, 0, 1.03, BoxBottom);
		}
		AddWallBox(glass, a + u * (L / 2), u, n, -0.006, 0.006, L - 0.05, base + 0.08, base + 0.98);
		box(0, L, 0.05, 0.98, 1.03, 0);
	}
	else if(style == "half-wall")
	{
		box(0, L, 0.15, 0, 0.95, between);
		box(0, L, 0.2, 0.95, 1.02, 0);
	}
	else
	{
		box(0, 0.1, 0.1, 0, 1.1, BoxBottom);
		box(L - 0.1, L, 0.1, 0, 1.1, BoxBottom);
		box(0.1, L - 0.1, 0.07, 0.95, 1.02, ends);
		box(0.1, L - 0.1, 0.05, 0.08, 0.13, ends);
		uprights(0.1, L - 0.1, 0.12, 0.035, 0.13, 0.95, between);
	}
}

}
