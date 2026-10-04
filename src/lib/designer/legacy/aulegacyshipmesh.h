#ifndef AuLegacyShipMesh_H
#define AuLegacyShipMesh_H

#include "aulegacyship.h"
#include <QStringList>
#include <QVector2D>
#include <QVector3D>

namespace AuLegacyShip
{
namespace Rendering
{
// Original DaSpaceShip::SHIP_TEXTURE numbers. UVs address an individual atlas
// region, rather than the whole image, so a renderer can repeat cropped tiles.
enum class Material
{
	LargeInteriorDoor = 2,
	LargeCrewDoor = 5,
	LargeHullDoor = 8,
	CrewDoor = 11,
	CrewHatch = 14,
	HullBottom = 18,
	HullDoor = 19,
	HullHatch = 22,
	HullTop = 25,
	HullTopBottomWindow = 26,
	HullWall = 27,
	HullWallWindow = 28,
	InteriorCeiling = 29,
	InteriorDoor = 30,
	InteriorFloor = 33,
	InteriorHatch = 34,
	InteriorWallEndCap = 37,
	InteriorWallFinished = 38,
	InteriorWallFill = 39,
	InteriorWallCargo = 40,
	InteriorWallCargoFill = 41,
	InteriorWallEquipment = 42,
	InteriorWallEquipmentFill = 43,
	Railing = 48,
	RailingPost = 49
};

struct Vertex
{
	QVector3D position;
	QVector3D normal;
	QVector2D uv;
};

struct Batch
{
	Material material = Material::HullWall;
	QVector<Vertex> vertices;
	int deck = 0;
	bool interior = false;
};

struct Mesh
{
	QVector<Batch> batches;
	QVector3D minimum;
	QVector3D maximum;
	QStringList warnings;
};

// Positions retain the author's feet coordinates: x/y plan, z up. Triangle
// winding and normals face the visible side of each exterior/interior surface.
Mesh BuildMesh(const Design& design);
}
}

#endif
