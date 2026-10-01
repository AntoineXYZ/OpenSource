#include "doors.h"
#include "internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

AuPoint FrameAt(const Frame3& f, double a, double b, double c)
{
	return f.o + f.x * a + f.y * b + f.z * c;
}

Frame3 LeafFrame(const V2& o, const V2& x, double z)
{
	return Frame3{AuPoint(o.x, o.y, z), AuPoint(x.x, x.y, 0), AuPoint(-x.y, x.x, 0), AuPoint(0, 0, 1)};
}

void FrameBox(AuScFaceList& faces, const Frame3& f, double x0, double x1, double y0, double y1, double z0, double z1)
{
	if(x1 - x0 < 1e-4 || y1 - y0 < 1e-4 || z1 - z0 < 1e-4)
		return;
	auto p = [&](double a, double b, double c) { return FtP(FrameAt(f, a, b, c)); };
	AddQuad(faces, p(x0, y0, z0), p(x0, y1, z0), p(x1, y1, z0), p(x1, y0, z0), f.z * -1);
	AddQuad(faces, p(x0, y0, z1), p(x0, y1, z1), p(x1, y1, z1), p(x1, y0, z1), f.z);
	AddQuad(faces, p(x0, y0, z0), p(x0, y0, z1), p(x1, y0, z1), p(x1, y0, z0), f.y * -1);
	AddQuad(faces, p(x0, y1, z0), p(x0, y1, z1), p(x1, y1, z1), p(x1, y1, z0), f.y);
	AddQuad(faces, p(x0, y0, z0), p(x0, y0, z1), p(x0, y1, z1), p(x0, y1, z0), f.x * -1);
	AddQuad(faces, p(x1, y0, z0), p(x1, y0, z1), p(x1, y1, z1), p(x1, y1, z0), f.x);
}

void AddDoorMesh(AuScStatePart& door, DoorMesh& m, Theme theme)
{
	Q_UNUSED(theme);
	Paint(m.leaf, DOOR_PAINT);
	Paint(m.battens, DOOR_BATTEN_PAINT);
	Paint(m.iron, QColor(0x2c, 0x2d, 0x31));
	Paint(m.accent, QColor(0xd8, 0xb0, 0x20));
	Paint(m.lock, QColor(0xc8, 0x28, 0x20));
	for(AuScFaceList* list : {&m.leaf, &m.battens, &m.iron, &m.accent, &m.lock})
		if(!list->isEmpty())
			door.AddFaces(*list);
}

void Leaf(DoorMesh& m, const Frame3& f, double w, double z0, double z1, double t, Theme theme, double handleX)
{
	FrameBox(m.leaf, f, 0.005, w - 0.005, -t / 2, t / 2, z0, z1);
	double h = z1 - z0;
	for(int side : {-1, 1})
	{
		auto on = [&](double depth) { return side < 0 ? qMakePair(-t / 2 - depth, -t / 2) : qMakePair(t / 2, t / 2 + depth); };
		if(theme == Wood && h > 0.5)
		{
			QPair<double, double> y = on(0.02);
			double zb = z0 + h * 0.12, zt = z1 - h * 0.18;
			FrameBox(m.battens, f, 0.06, w - 0.06, y.first, y.second, zb, zb + 0.12);
			FrameBox(m.battens, f, 0.06, w - 0.06, y.first, y.second, zt - 0.12, zt);
			double dx = w - 0.24, dz = zt - zb - 0.24, len = qSqrt(dx * dx + dz * dz), c = dx / len, s = dz / len;
			Frame3 brace{FrameAt(f, 0.12, 0, zb + 0.12), f.x * c + f.z * s, f.y, f.x * -s + f.z * c};
			FrameBox(m.battens, brace, 0, len, y.first, y.second, -0.05, 0.05);
		}
		else if(theme == Stone && h > 0.5)
		{
			QPair<double, double> y = on(0.012);
			for(double k : {0.18, 0.5, 0.82})
				FrameBox(m.iron, f, 0.03, w - 0.03, y.first, y.second, z0 + h * k - 0.035, z0 + h * k + 0.035);
		}
		else if(theme == Metal)
		{
			QPair<double, double> y = on(0.006);
			int i = 0;
			for(double x = 0.02; x < w - 0.02; x += 0.12, i++)
				FrameBox(i % 2 ? m.iron : m.accent, f, x, qMin(x + 0.12, w - 0.02), y.first, y.second, z0 + 0.04, qMin(z1, z0 + 0.2));
			if(h > 1.2)
				FrameBox(m.iron, f, w * 0.3, w * 0.7, y.first, y.second, z0 + h * 0.62, z0 + h * 0.82);
		}
		if(handleX >= 0 && z0 < 1.0 && z1 > 1.15)
		{
			QPair<double, double> y = on(0.05);
			FrameBox(m.iron, f, handleX - 0.03, handleX + 0.03, y.first, y.second, 1.0, 1.12);
		}
	}
}

void RollerLeaf(DoorMesh& out, const Frame3& f, double w, double zb, double zt, double t, bool rollsUp)
{
	if(zt - zb < 0.01)
		return;
	FrameBox(out.leaf, f, 0.005, w - 0.005, -t / 2, t / 2, zb, zt);
	double edge = rollsUp ? zb : zt;
	FrameBox(out.iron, f, 0.0, w, -t / 2 - 0.015, t / 2 + 0.015, rollsUp ? zb : zt - 0.06, rollsUp ? zb + 0.06 : zt);
	for(double k = 0.15; k < zt - zb - 0.03; k += 0.15)
	{
		double z = rollsUp ? edge + k : edge - k;
		for(int side : {-1, 1})
			FrameBox(out.battens, f, 0.02, w - 0.02, side < 0 ? -t / 2 - 0.012 : t / 2, side < 0 ? -t / 2 : t / 2 + 0.012,
				z - 0.015, z + 0.015);
	}
}

double LeafThickness(Theme theme)
{
	return theme == Stone ? 0.07 : theme == Metal ? 0.06 : 0.05;
}

void DoorPose(DoorMesh& out, const DoorGeo& g, double f)
{
	const double W = g.width, t = LeafThickness(g.theme);
	const QString& s = g.style;
	if(s.startsWith("double-swing"))
	{
		bool in = s.endsWith("-in");
		V2 swing = in ? g.n * -1 : g.n;
		double across = in ? -g.inHere + t / 2 + 0.01 : g.inThere - t / 2 - 0.01;
		double a = f * M_PI / 2;
		for(V2 hinge : {g.L, g.L * -1})
		{
			V2 pivot = g.m + g.n * across + hinge * (W / 2 - 0.01);
			V2 d = hinge * -qCos(a) + swing * qSin(a);
			Leaf(out, LeafFrame(pivot, d), W / 2 - 0.015, 0.02, DOOR_HEIGHT - 0.01, t, g.theme, W / 2 - 0.13);
		}
	}
	else if(s == "slide-up" || s == "slide-down")
	{
		Frame3 frame = LeafFrame(g.m + g.n * g.mid - g.L * (W / 2), g.L);
		if(s == "slide-up")
			RollerLeaf(out, frame, W, 0.02 + f * (DOOR_HEIGHT - 0.02), DOOR_HEIGHT - 0.01, t, true);
		else
			RollerLeaf(out, frame, W, 0.0, (DOOR_HEIGHT - 0.01) * (1 - f), t, false);
	}
	else if(s.startsWith("swing"))
	{
		bool left = s.endsWith("left"), in = s.contains("-in-");
		V2 hinge = left ? g.L : g.L * -1, swing = in ? g.n * -1 : g.n;
		double across = in ? -g.inHere + t / 2 + 0.01 : g.inThere - t / 2 - 0.01;
		V2 pivot = g.m + g.n * across + hinge * (W / 2 - 0.01);
		double a = f * M_PI / 2;
		V2 d = hinge * -qCos(a) + swing * qSin(a);
		Leaf(out, LeafFrame(pivot, d), W - 0.02, 0.02, DOOR_HEIGHT - 0.01, t, g.theme, W - 0.14);
	}
	else if(s.startsWith("slide"))
	{
		V2 dir = s.endsWith("left") ? g.L : g.L * -1;
		Leaf(out, LeafFrame(g.m + g.n * g.mid - dir * (W / 2) + dir * (f * W), dir), W, 0.02, DOOR_HEIGHT - 0.01, t, g.theme, -1);
	}
	else if(s == "split-v")
	{
		for(V2 dir : {g.L, g.L * -1})
			Leaf(out, LeafFrame(g.m + g.n * g.mid + dir * (f * W / 2), dir), W / 2, 0.02, DOOR_HEIGHT - 0.01, t, g.theme, -1);
	}
	else if(s == "split-h")
	{
		double topH = qMin(DOOR_HEIGHT / 2, g.top - DOOR_HEIGHT - 0.05), split = DOOR_HEIGHT - topH;
		Frame3 frame = LeafFrame(g.m + g.n * g.mid - g.L * (W / 2), g.L);
		Frame3 up = frame, down = frame;
		up.o = up.o + AuPoint(0, 0, f * topH);
		down.o = down.o + AuPoint(0, 0, -f * split);
		Leaf(out, up, W, split, DOOR_HEIGHT - 0.01, t, g.theme, -1);
		Leaf(out, down, W, 0.0, split, t, g.theme, -1);
	}
	else if(s == "portcullis")
	{
		Frame3 frame = LeafFrame(g.m + g.n * g.mid - g.L * (W / 2), g.L, f * (DOOR_HEIGHT - 0.12));
		for(double x = 0.07; x < W - 0.05; x += 0.15)
		{
			FrameBox(out.iron, frame, x - 0.02, x + 0.02, -0.025, 0.025, 0.08, DOOR_HEIGHT + 0.3);
			FrameBox(out.iron, frame, x - 0.012, x + 0.012, -0.015, 0.015, 0.0, 0.08);
		}
		for(double z : {0.45, 1.05, 1.65})
			FrameBox(out.iron, frame, 0.02, W - 0.02, -0.035, 0.035, z, z + 0.06);
	}
	else if(f < 0.5)
		Leaf(out, LeafFrame(g.m + g.n * g.mid - g.L * (W / 2), g.L), W, 0.02, DOOR_HEIGHT - 0.01, t, g.theme, -1);
}

void DoorLock(DoorMesh& out, const DoorGeo& g, bool open)
{
	for(int side : {-1, 1})
	{
		double face = side < 0 ? -g.inHere : g.inThere;
		double inner = face + side * 0.012, outer = face + side * 0.07;
		if(open)
		{
			AddWallBox(out.lock, g.m, g.L, g.n, face, face + side * 0.065, 0.24, DOOR_HEIGHT + 0.03, DOOR_HEIGHT + 0.12);
			continue;
		}
		AuScFaceList& bar = g.theme == Metal ? out.lock : g.theme == Wood ? out.battens : out.iron;
		AddWallBox(bar, g.m, g.L, g.n, inner, outer, g.width + 0.3, 1.0, 1.1);
		for(double end : {-1.0, 1.0})
			AddWallBox(out.iron, g.m + g.L * (end * (g.width / 2 + 0.1)), g.L, g.n, face, outer + side * 0.01, 0.06, 0.96, 1.14);
		if(g.theme == Metal)
			AddWallBox(out.lock, g.m, g.L, g.n, face, face + side * 0.065, 0.24, DOOR_HEIGHT + 0.03, DOOR_HEIGHT + 0.12);
	}
}

void BuildDoorStates(AuScDoor& door, const DoorGeo& g)
{
	bool animated = g.style != "vanish";
	auto frame = [&](int state, double f, int lock) {
		DoorMesh mesh;
		DoorPose(mesh, g, f);
		if(lock)
			DoorLock(mesh, g, lock == 2);
		if(state != AuScDoor::Closed)
			door.AddState(state);
		AddDoorMesh(door, mesh, g.theme);
	};
	frame(AuScDoor::Closed, 0, 0);
	if(animated)
		for(int k = 1; k <= 9; k++)
			frame(AuScDoor::Closed + k, k / 10.0, 0);
	frame(AuScDoor::Open, 1, 0);
	frame(AuScDoor::StuckClosed, animated ? 0.05 : 0, 0);
	frame(AuScDoor::StuckOpen, animated ? 0.7 : 1, 0);
	frame(AuScDoor::LockedClosed, 0, 1);
	frame(AuScDoor::LockedOpen, 1, 2);
	door.SetState(AuScDoor::Closed);
}

void HatchAxes(int side, V2& fwd, V2& right)
{
	static const double dirs[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
	fwd = V2(dirs[side][0], dirs[side][1]);
	right = V2(fwd.y, -fwd.x);
}

void HatchPanel(DoorMesh& m, const Frame3& f, double w, double d, double t, Theme theme)
{
	FrameBox(m.leaf, f, 0.005, w - 0.005, 0.005, d - 0.005, -t, 0);
	if(theme == Wood)
		for(double k : {0.2, 0.8})
			FrameBox(m.battens, f, k * w - 0.05, k * w + 0.05, 0.03, d - 0.03, 0, 0.02);
	else if(theme == Stone)
		for(double k : {0.25, 0.75})
			FrameBox(m.iron, f, 0.02, w - 0.02, k * d - 0.035, k * d + 0.035, 0, 0.012);
	else if(theme == Metal)
	{
		int i = 0;
		for(double x = 0.02; x < w - 0.02; x += 0.1, i++)
			FrameBox(i % 2 ? m.iron : m.accent, f, x, qMin(x + 0.1, w - 0.02), d - 0.12, d - 0.03, 0, 0.006);
		FrameBox(m.iron, f, w / 2 - 0.12, w / 2 + 0.12, d / 2 - 0.02, d / 2 + 0.02, 0, 0.03);
		FrameBox(m.iron, f, w / 2 - 0.02, w / 2 + 0.02, d / 2 - 0.12, d / 2 + 0.12, 0, 0.03);
	}
	if(theme != Metal)
		FrameBox(m.iron, f, w / 2 - 0.07, w / 2 + 0.07, d - 0.14, d - 0.1, 0, 0.025);
}

void HatchPose(DoorMesh& out, const HatchGeo& g, double f)
{
	const double s = g.size, h = s / 2, t = 0.08, a = f * M_PI / 2;
	const Theme th = g.theme;
	const AuPoint R = g.R3(), F = g.F3(), Z(0, 0, 1);
	if(g.style == "flap-up")
	{
		Frame3 fr{g.At(-h, -h, g.top), R, F * qCos(a) + Z * qSin(a), F * -qSin(a) + Z * qCos(a)};
		HatchPanel(out, fr, s, s, t, th);
	}
	else if(g.style == "flap-down")
	{
		Frame3 fr{g.At(-h, -h, g.bottom + t), R, F * qCos(a) + Z * -qSin(a), F * qSin(a) + Z * qCos(a)};
		HatchPanel(out, fr, s, s, t, th);
	}
	else if(g.style == "double-flap")
	{
		Frame3 left{g.At(-h, -h, g.top), F, R * qCos(a) + Z * qSin(a), R * -qSin(a) + Z * qCos(a)};
		Frame3 right{g.At(h, -h, g.top), F, R * -qCos(a) + Z * qSin(a), R * qSin(a) + Z * qCos(a)};
		HatchPanel(out, left, s, h, t, th);
		HatchPanel(out, right, s, h, t, th);
	}
	else if(g.style == "slide")
	{
		double mid = (g.top + g.bottom) / 2 + t / 2;
		HatchPanel(out, Frame3{g.At(-h + f * s, -h, mid), R, F, Z}, s, s, t, th);
	}
	else if(f < 0.5)
		HatchPanel(out, Frame3{g.At(-h, -h, g.top), R, F, Z}, s, s, t, th);
}

void HatchLock(DoorMesh& out, const HatchGeo& g, bool open)
{
	const double h = g.size / 2;
	Frame3 fr{g.At(0, 0, g.top), g.R3(), g.F3(), AuPoint(0, 0, 1)};
	if(open)
	{
		FrameBox(out.lock, fr, h + 0.1, h + 0.25, -h + 0.05, -h + 0.2, 0, 0.06);
		return;
	}
	AuScFaceList& bar = g.theme == Metal ? out.lock : g.theme == Wood ? out.battens : out.iron;
	FrameBox(bar, fr, -h - 0.15, h + 0.15, -0.05, 0.05, 0, 0.07);
	for(double end : {-1.0, 1.0})
		FrameBox(out.iron, fr, end * (h + 0.1) - 0.04, end * (h + 0.1) + 0.04, -0.08, 0.08, 0, 0.09);
}

void BuildHatchStates(AuScDoor& hatch, const HatchGeo& g)
{
	bool animated = g.style != "vanish";
	auto frame = [&](int state, double f, int lock) {
		DoorMesh mesh;
		HatchPose(mesh, g, f);
		if(lock)
			HatchLock(mesh, g, lock == 2);
		if(state != AuScDoor::Closed)
			hatch.AddState(state);
		AddDoorMesh(hatch, mesh, g.theme);
	};
	frame(AuScDoor::Closed, 0, 0);
	if(animated)
		for(int k = 1; k <= 9; k++)
			frame(AuScDoor::Closed + k, k / 10.0, 0);
	frame(AuScDoor::Open, 1, 0);
	frame(AuScDoor::StuckClosed, animated ? 0.06 : 0, 0);
	frame(AuScDoor::StuckOpen, animated ? 0.6 : 1, 0);
	frame(AuScDoor::LockedClosed, 0, 1);
	frame(AuScDoor::LockedOpen, 1, 2);
	hatch.SetState(AuScDoor::Closed);
}

void BuildLadder(AuScFaceList& rails, AuScFaceList& rungs, AuScFaceList& extra, const HatchGeo& g, double z0, double top,
	double rail, const QString& style)
{
	const double yr = g.size / 2 - LADDER_IN;
	Frame3 f{g.At(0, 0, 0), g.R3(), g.F3(), AuPoint(0, 0, 1)};
	if(style == "pole")
	{
		FrameBox(rails, f, -0.03, 0.03, yr - 0.03, yr + 0.03, z0, top + rail);
		int i = 0;
		for(double z = z0 + 0.3; z < top - 0.1; z += 0.3, i++)
			FrameBox(rungs, f, i % 2 ? 0.03 : -0.22, i % 2 ? 0.22 : -0.03, yr - 0.015, yr + 0.015, z - 0.015, z + 0.015);
		return;
	}
	double rw = style == "rope" ? 0.02 : 0.04;
	double railTop = style == "rope" ? top : top + rail;
	double foot = style == "rope" ? z0 + 0.12 : z0;
	for(double side : {-1.0, 1.0})
		FrameBox(rails, f, side * 0.23 - rw / 2, side * 0.23 + rw / 2, yr - rw / 2, yr + rw / 2, foot, railTop);
	for(double z = foot + 0.25; z < top - 0.05; z += 0.3)
	{
		if(style == "rope")
			FrameBox(rungs, f, -0.23, 0.23, yr - 0.05, yr + 0.02, z - 0.015, z + 0.015);
		else
			FrameBox(rungs, f, -0.21, 0.21, yr - 0.015, yr + 0.015, z - 0.015, z + 0.015);
	}
	if(style == "caged")
	{
		const double c0 = yr - 0.7, w = 0.33;
		for(double z = z0 + 2.2; z < top + rail; z += 0.6)
		{
			FrameBox(extra, f, -w, w, c0, c0 + 0.03, z - 0.02, z + 0.02);
			FrameBox(extra, f, -w, -w + 0.03, c0, yr, z - 0.02, z + 0.02);
			FrameBox(extra, f, w - 0.03, w, c0, yr, z - 0.02, z + 0.02);
		}
		for(double sx : {-w + 0.015, 0.0, w - 0.015})
		{
			double y0 = sx == 0.0 ? c0 : c0 + 0.1;
			FrameBox(extra, f, sx - 0.015, sx + 0.015, y0, y0 + 0.03, z0 + 2.2, top + rail);
		}
	}
}

}
