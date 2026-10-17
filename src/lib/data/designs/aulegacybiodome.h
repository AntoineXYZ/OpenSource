#ifndef AuLegacyBiodome_H
#define AuLegacyBiodome_H

#include "aulegacybuildinggeometry.h"
#include <functional>

namespace AuLegacyBd
{
namespace Biodome
{
// Heights are relative to the stored site MSL. Runtime projections preserve
// the original world's curved altitude surfaces without an AuData->AuAtom link.
using AltitudeProjection = std::function<QVector3D(const QVector3D&, double)>;
using TerrainHeight = std::function<double(const QVector3D&)>;
constexpr double BeamHeight = 48.0;
constexpr double RingHalfWidth = 9.84251968503936;
constexpr double CrownHeight = BeamHeight + RingHalfWidth;
constexpr double ContainmentMargin = 6.56167979002624;
AUDATA_DLL double RoofHeight(double radiusFeet, double radialDistanceFeet);
AUDATA_DLL Geometry::Mesh Build(const AuLegacyCity::Development& development,
	const AltitudeProjection& project = {}, const TerrainHeight& terrain = {});
}
}

#endif
