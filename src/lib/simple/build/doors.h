#pragma once

#include "../geometry/mesh.h"
#include "theme.h"

namespace SimpleMode::detail {

struct Frame3
{
	AuPoint o, x, y, z;
};

struct DoorMesh
{
	AuScFaceList leaf, battens, iron, accent, lock;
};

struct DoorGeo
{
	V2 m, n, L;
	double inHere, inThere, mid;
	double top;
	Theme theme;
	QString style;
	double width;
};

struct HatchGeo
{
	double x, y, size, top, bottom;
	Theme theme;
	QString style;
	V2 fwd, right;

	AuPoint At(double r, double f, double z) const
	{
		return AuPoint(x + right.x * r + fwd.x * f, y + right.y * r + fwd.y * f, z);
	}
	AuPoint R3() const { return AuPoint(right.x, right.y, 0); }
	AuPoint F3() const { return AuPoint(fwd.x, fwd.y, 0); }
};

AuPoint FrameAt(const Frame3& f, double a, double b, double c);

Frame3 LeafFrame(const V2& o, const V2& x, double z = 0);

void FrameBox(AuScFaceList& faces, const Frame3& f, double x0, double x1, double y0, double y1, double z0, double z1);

void AddDoorMesh(AuScStatePart& door, DoorMesh& m, Theme theme);

void Leaf(DoorMesh& m, const Frame3& f, double w, double z0, double z1, double t, Theme theme, double handleX);

void RollerLeaf(DoorMesh& out, const Frame3& f, double w, double zb, double zt, double t, bool rollsUp);

double LeafThickness(Theme theme);

void DoorPose(DoorMesh& out, const DoorGeo& g, double f);

void DoorLock(DoorMesh& out, const DoorGeo& g, bool open);

void BuildDoorStates(AuScDoor& door, const DoorGeo& g);

void HatchAxes(int side, V2& fwd, V2& right);

void HatchPanel(DoorMesh& m, const Frame3& f, double w, double d, double t, Theme theme);

void HatchPose(DoorMesh& out, const HatchGeo& g, double f);

void HatchLock(DoorMesh& out, const HatchGeo& g, bool open);

void BuildHatchStates(AuScDoor& hatch, const HatchGeo& g);

void BuildLadder(AuScFaceList& rails, AuScFaceList& rungs, AuScFaceList& extra, const HatchGeo& g, double z0, double top,
	double rail, const QString& style);

}
