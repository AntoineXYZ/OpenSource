#pragma once

#include "../geometry/mesh.h"
#include "theme.h"

namespace SimpleMode::detail {

struct WindowGeo
{
	V2 m, u, n;
	double width, sill, head;
	double inHere, inThere;
	bool hull;
	int tech;
	Theme theme;
};

struct WindowMesh
{
	AuScFaceList frame, dark, shutter, glow;
};

double GlassAcross(const WindowGeo& g);

void PaintWindowMesh(WindowMesh& m, const WindowGeo& g);

void WindowFrames(WindowMesh& out, const WindowGeo& g, int side);

void WindowGlass(AuScFaceList& glass, const WindowGeo& g);

void ArchSpandrels(AuScFaceList& f, const V2& m, const V2& u, const V2& n, double width, double spring, double top, double from,
	double to);

void RailingMesh(AuScFaceList& main, AuScFaceList& glass, const V2& a, const V2& b, const V2& n, QString style,
	double base = 0, double floorPast = 0);

}
