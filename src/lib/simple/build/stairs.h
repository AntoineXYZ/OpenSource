#pragma once

#include "../geometry/mesh.h"

namespace SimpleMode::detail {

struct StairFrame
{
	V2 s0, F, R;
	double len, w;
	AuPoint At(double t, double a, double z) const { return Ft(s0 + F * t + R * a, z); }
};

void SlopedBar(AuScFaceList& f, const StairFrame& fr, double a0, double a1, double t0, double t1, double lo0, double hi0,
	double lo1, double hi1, int skip = 0);

void StairFlightMesh(AuScFaceList& main, AuScFaceList& dark, AuScFaceList& glass, const StairFrame& fr, double z0, double z1,
	const QString& style);

}
