#include "aulegacyscgeometry.h"
#include "aulegacyscdesign.h"
#include "aulegacyshipgeometry.h"
#include "aulegacyshipdetails.h"
#include "aulegacyshipmesh.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipstairs.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auscreport.h"

#include <QHash>
#include <QDataStream>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace AuLegacyScGeometry
{
namespace
{
using namespace AuLegacyShip;
using namespace AuLegacyShip::Rendering;
constexpr int MaximumTriangles = 750000;
constexpr int MaximumGeneratedTriangles = 1000000;
constexpr int MaximumParts = 8192;
constexpr int MaximumTileVisits = 4000000;
constexpr int MaximumRoomRasterVisits = 500000;
constexpr quint64 MaximumGeometryMemory = 256 * 1024 * 1024;
constexpr int MaximumGeometryBytes = 128 * 1024 * 1024;
QMutex modelCacheMutex;

struct TextureRegion { int x, y, width, height; };
// Recovered SHIP_TEXTURE atlas rectangles; the source image has top-origin rows.
const TextureRegion ShipTextures[] = {
	{0,480,128,32}, {0,480,24,12},
	{0,2176,128,128}, {0,2048,128,128}, {0,1920,128,128},
	{0,2560,128,128}, {0,2432,128,128}, {0,2304,128,128},
	{0,2944,128,128}, {0,2816,128,128}, {0,2688,128,128},
	{0,1536,64,128}, {64,1536,64,128}, {0,1408,64,128},
	{0,896,64,64}, {64,960,64,64}, {0,960,64,64},
	{32,576,32,32}, {0,3456,128,128},
	{0,1792,64,128}, {64,1792,64,128}, {0,1664,64,128},
	{0,1024,64,64}, {64,1088,64,64}, {0,1088,64,64},
	{0,3584,128,128}, {0,3200,128,128},
	{0,3712,128,128}, {0,3328,128,128}, {0,0,128,128},
	{0,1280,64,128}, {64,1280,64,128}, {0,1152,64,128},
	{0,3840,128,128},
	{0,768,64,64}, {64,832,64,64}, {0,832,64,64},
	{96,1152,32,128}, {0,3968,128,128}, {64,1152,32,128},
	{0,256,128,128}, {96,1408,32,128},
	{0,128,128,128}, {64,1408,32,128},
	{0,640,64,128}, {0,640,11,128},
	{64,544,32,32}, {32,608,32,32},
	{96,624,32,16}, {96,608,16,16}, {64,576,32,32},
	{0,512,32,128}, {0,512,2,2}, {32,512,32,32},
	{64,640,64,128}, {64,608,32,32}, {0,3072,128,128},
	{64,512,32,32}, {32,544,32,32},
	{0,416,128,32}, {0,412,128,36}, {0,384,128,32}
};

AuPoint Point(const QVector3D& value) { return AuPoint(value.x(), value.y(), value.z()); }

bool IsHatchPanel(const Batch& batch)
{
	// The recovered hatch remains a face when open: its final atlas frame
	// contains the visible rim and a transparent passage through the center.
	return batch.doorId && (batch.material == Material::CrewHatch
		|| batch.material == Material::HullHatch || batch.material == Material::InteriorHatch);
}

Material TextureMaterial(const Batch& batch)
{
	return IsHatchPanel(batch) ? DoorMaterialFrame(batch.material, DoorFrame::Open) : batch.material;
}

QString TextureKey(const Batch& batch)
{
	const QString source = batch.textureFile.isEmpty() ? QStringLiteral("material:%1").arg(int(TextureMaterial(batch))) : batch.textureFile;
	return QStringLiteral("%1/%2/%3/%4").arg(source).arg(batch.tint.x()).arg(batch.tint.y()).arg(batch.tint.z());
}

struct Tile
{
	int texture = 0;
	QRect rect;
	bool translucent = false;
};

// The modern palette permits only 256px images and 14 user textures. Pack
// cropped materials into pages; UV subdivision keeps their repeat semantics.
class Palette
{
public:
	AuDesignTex textures[SCTEXI_Count];
	QHash<QString, Tile> tiles;
	QByteArray textureMap;
	bool Build(const Mesh& mesh, QString& error, bool includeOpenHatches)
	{
		QVector<const Batch*> sources;
		QSet<QString> keys;
		for(const Batch& batch : mesh.batches)
		{
			if((batch.doorId && (!includeOpenHatches || !IsHatchPanel(batch))) || batch.vertices.isEmpty()) continue;
			const QString key = TextureKey(batch);
			if(keys.contains(key)) continue;
			keys.insert(key);
			sources.append(&batch);
		}
		const int side = sources.size() <= 56 ? 128 : 64;
		const int columns = 256 / side;
		const int perPage = columns * columns;
		if(sources.size() > 14 * perPage)
		{
			error = QStringLiteral("The legacy design uses too many textures for the debug geometry adapter.");
			return false;
		}
		QVector<QImage> pages;
		QJsonArray bindings;
		QImage atlas(QStringLiteral(":/data/legacy_txSpacecraft0Lit.png"));
		if(atlas.isNull())
		{
			error = QStringLiteral("The server is missing the recovered spacecraft texture resources.");
			return false;
		}
		for(int index = 0; index < sources.size(); ++index)
		{
			const Batch& batch = *sources[index];
			const int material = int(TextureMaterial(batch));
			QImage image;
			if(!batch.textureFile.isEmpty())
			{
				const QString path = batch.textureFile == QStringLiteral("legacy:pc_Console.bmp")
					? QStringLiteral(":/data/legacy_pc_Console.bmp") : batch.textureFile;
				image.load(path);
			}
			else if(material >= 0 && material < int(sizeof(ShipTextures) / sizeof(ShipTextures[0])))
			{
				const TextureRegion& region = ShipTextures[material];
				image = atlas.copy(region.x * atlas.width() / 128,
					(4096 - region.y - region.height) * atlas.height() / 4096,
					region.width * atlas.width() / 128, region.height * atlas.height() / 4096);
			}
			else
			{
				image = QImage(1, 1, QImage::Format_ARGB32);
				image.fill(Qt::white);
			}
			if(image.isNull())
			{
				error = QStringLiteral("A recovered spacecraft texture is missing: %1.").arg(TextureKey(batch));
				return false;
			}
			image = image.convertToFormat(QImage::Format_ARGB32);
			bool translucent = false;
			for(int y = 0; y < image.height(); ++y)
			{
				QRgb* pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
				for(int x = 0; x < image.width(); ++x)
				{
					translucent = translucent || (qAlpha(pixels[x]) > 0 && qAlpha(pixels[x]) < 255);
					pixels[x] = qRgba(int(qRed(pixels[x]) * batch.tint.x()), int(qGreen(pixels[x]) * batch.tint.y()),
						int(qBlue(pixels[x]) * batch.tint.z()), qAlpha(pixels[x]));
				}
			}
			const int page = index / perPage;
			if(page == pages.size())
			{
				QImage blank(256, 256, QImage::Format_ARGB32);
				blank.fill(Qt::transparent);
				pages.append(blank);
			}
			const int cell = index % perPage;
			const QRect rect((cell % columns) * side, (cell / columns) * side, side, side);
			QPainter painter(&pages[page]);
			painter.drawImage(rect, image);
			painter.end();
			Tile tile;
			tile.texture = page == 0 ? SCTEXI_User0 : SCTEXI_User1 + page - 1;
			tile.rect = rect;
			tile.translucent = translucent;
			tiles.insert(TextureKey(batch), tile);
			QJsonObject binding;
			binding.insert(QStringLiteral("palette"), tile.texture);
			binding.insert(QStringLiteral("x"), rect.x());
			binding.insert(QStringLiteral("y"), rect.y());
			binding.insert(QStringLiteral("side"), side);
			binding.insert(QStringLiteral("material"), material);
			binding.insert(QStringLiteral("resource"), batch.textureFile == QStringLiteral("legacy:pc_Console.bmp")
				? QStringLiteral(":/data/legacy_pc_Console.bmp") : batch.textureFile);
			QJsonArray tint;
			for(int component = 0; component < 4; ++component) tint.append(double(batch.tint[component]));
			binding.insert(QStringLiteral("tint"), tint);
			bindings.append(binding);
		}
		for(int page = 0; page < pages.size(); ++page)
		{
			const int slot = page == 0 ? SCTEXI_User0 : SCTEXI_User1 + page - 1;
			textures[slot].SetImage(QStringLiteral("Legacy debug atlas %1").arg(page), pages[page]);
		}
		QJsonObject map;
		map.insert(QStringLiteral("version"), 1);
		map.insert(QStringLiteral("bindings"), bindings);
		textureMap = QJsonDocument(map).toJson(QJsonDocument::Compact);
		if(textureMap.size() > MaximumTextureMapBytes)
		{
			error = QStringLiteral("The legacy material map exceeds its size limit.");
			return false;
		}
		return true;
	}
};

Vertex Interpolate(const Vertex& a, const Vertex& b, float fraction)
{
	Vertex result;
	result.position = a.position + (b.position - a.position) * fraction;
	result.normal = a.normal + (b.normal - a.normal) * fraction;
	result.uv = a.uv + (b.uv - a.uv) * fraction;
	return result;
}

QVector<Vertex> Clip(const QVector<Vertex>& input, int axis, float edge, bool keepHigher)
{
	QVector<Vertex> output;
	if(input.isEmpty()) return output;
	Vertex previous = input.last();
	bool previousInside = keepHigher ? previous.uv[axis] >= edge : previous.uv[axis] <= edge;
	for(const Vertex& current : input)
	{
		const bool inside = keepHigher ? current.uv[axis] >= edge : current.uv[axis] <= edge;
		if(inside != previousInside)
			output.append(Interpolate(previous, current, (edge - previous.uv[axis]) / (current.uv[axis] - previous.uv[axis])));
		if(inside) output.append(current);
		previous = current;
		previousInside = inside;
	}
	return output;
}

AuScPoint VertexPoint(const Vertex& vertex, const AuLegacyScDesign& legacy, const Tile& tile, int u, int v)
{
	// Palette upload flips image rows. Half-pixel margins prevent tile bleed.
	const float tx = (tile.rect.left() + .5f + (vertex.uv.x() - u) * (tile.rect.width() - 1)) / 256.f;
	const float ty = 1.f - (tile.rect.top() + .5f + (1.f - vertex.uv.y() + v) * (tile.rect.height() - 1)) / 256.f;
	return AuScPoint(legacy.AuthorToShip(Point(vertex.position)), tx, ty, Point(vertex.normal.normalized()));
}

bool AddTriangleFaces(AuScFaceList& faces, const Vertex* triangle, const Batch& batch,
	const AuLegacyScDesign& legacy, const Tile& tile, int& count, int& visits)
{
	float minU = triangle[0].uv.x(), maxU = minU, minV = triangle[0].uv.y(), maxV = minV;
	for(int index = 0; index < 3; ++index)
	{
		if(!std::isfinite(triangle[index].uv.x()) || !std::isfinite(triangle[index].uv.y())) return false;
		minU = std::min(minU, triangle[index].uv.x()); maxU = std::max(maxU, triangle[index].uv.x());
		minV = std::min(minV, triangle[index].uv.y()); maxV = std::max(maxV, triangle[index].uv.y());
	}
	if(minU < -65536 || maxU > 65536 || minV < -65536 || maxV > 65536) return false;
	const int firstU = int(std::floor(minU)), lastU = std::max(firstU, int(std::ceil(maxU)) - 1);
	const int firstV = int(std::floor(minV)), lastV = std::max(firstV, int(std::ceil(maxV)) - 1);
	const qint64 needed = qint64(lastU - firstU + 1) * (lastV - firstV + 1);
	if(needed > MaximumTileVisits - visits) return false;
	visits += int(needed);
	for(int u = firstU; u <= lastU; ++u)
		for(int v = firstV; v <= lastV; ++v)
		{
			QVector<Vertex> polygon{triangle[0], triangle[1], triangle[2]};
			polygon = Clip(polygon, 0, float(u), true);
			polygon = Clip(polygon, 0, float(u + 1), false);
			polygon = Clip(polygon, 1, float(v), true);
			polygon = Clip(polygon, 1, float(v + 1), false);
			for(int index = 1; index + 1 < polygon.size(); ++index)
			{
				if(QVector3D::crossProduct(polygon[index].position - polygon[0].position,
					polygon[index + 1].position - polygon[0].position).lengthSquared() < 1e-10f) continue;
				if(++count > MaximumTriangles) return false;
				AuScFaceTri face(VertexPoint(polygon[0], legacy, tile, u, v),
					VertexPoint(polygon[index], legacy, tile, u, v), VertexPoint(polygon[index + 1], legacy, tile, u, v));
				face.SetTextureIndex(quint8(tile.texture));
				face.SetPaint(QColor(255, 255, 255, 0));
				face.SetOpacity(tile.translucent ? std::min(.999f, batch.tint.w()) : batch.tint.w());
				face.SetSelfIllumination(batch.interior ? std::max(.25f, batch.selfIllumination) : batch.selfIllumination);
				faces.AddFace(face);
				if(batch.twoSided)
				{
					if(++count > MaximumTriangles) return false;
					AuScFaceTri reverse(VertexPoint(polygon[0], legacy, tile, u, v),
						VertexPoint(polygon[index + 1], legacy, tile, u, v), VertexPoint(polygon[index], legacy, tile, u, v));
					reverse.SetTextureIndex(quint8(tile.texture));
					reverse.SetPaint(face.Paint());
					reverse.SetOpacity(tile.translucent ? std::min(.999f, batch.tint.w()) : batch.tint.w());
					reverse.SetSelfIllumination(face.SelfIllumination());
					faces.AddFace(reverse);
				}
			}
		}
	return true;
}

bool AddPart(AuScDesign& design, const AuScPart& part)
{
	return design.Parts().size() < MaximumParts && design.Parts().AddPart(part) != 0;
}

bool BlocksPathCell(int type)
{
	switch(type)
	{
	case 10: case 14: case 16: case 18: case 23: case 24: case 32:
	case 74: case 75: case 100: case 142: return true;
	default: return false;
	}
}

AuPointArray Rectangle(const AuLegacyScDesign& legacy, const QPointF& a, const QPointF& b,
	const QPointF& offset, double bottom)
{
	AuPointArray points;
	for(const QPointF& point : {a - offset, b - offset, b + offset, a + offset})
		points.append(legacy.AuthorToShip(AuPoint(point.x(), point.y(), bottom)));
	return points;
}

}

bool Build(const AuLegacyScDesign& legacy, quint32 runtimeDesignId, const AuId& architectId,
	const QString& architectName, AuScDesign& output, AuScReport& report, QString& error)
{
	// The recovered resource-model importer has a shared cache. Catalog requests
	// arrive on independent socket threads, so serialize its use on the server.
	QMutexLocker guard(&modelCacheMutex);
	error.clear();
	if(!runtimeDesignId || !legacy.Stats().hasHull || !legacy.Stats().hullVolume)
	{
		error = QStringLiteral("The selected legacy design has no spawnable hull.");
		return false;
	}
	Design source = legacy.SourceDesign();
	if(source.details.size() > 32768)
	{
		error = QStringLiteral("The selected legacy design has too many details for debug spawning.");
		return false;
	}
	qint64 rasterVisits = 0;
	for(const Room& room : source.rooms)
	{
		const QRectF bounds = RoomBoundary(room).boundingRect();
		rasterVisits += qint64(std::ceil(bounds.width() / 1.41421356225) + 2)
			* qint64(std::ceil(bounds.height() / 1.41421356225) + 2);
		if(rasterVisits > MaximumRoomRasterVisits)
		{
			error = QStringLiteral("The selected legacy design exceeds the debug geometry raster-work limit.");
			return false;
		}
	}
	for(int index = 0; index < source.details.size(); ++index)
		source.details[index].detailId = legacy.DetailRecordId(index);
	const Mesh mesh = BuildMesh(source);
	qint64 meshVertices = 0;
	for(const Batch& batch : mesh.batches) meshVertices += batch.vertices.size();
	if(meshVertices >= 6000000)
	{
		error = QStringLiteral("The selected legacy design exceeds the recovered mesh-generation limit.");
		return false;
	}
	for(const QString& warning : mesh.warnings)
		if(warning.contains(QStringLiteral("truncated")))
		{
			error = QStringLiteral("The selected legacy design exceeds the debug geometry limit.");
			return false;
		}
	Palette palette;
	if(!palette.Build(mesh, error, true)) return false;
	AuScDesign generated(runtimeDesignId, architectId, architectName);
	generated.SetName(legacy.Name());
	generated.SetState(AuDesign::FinalAssembly);
	generated.SetStructure(SC_Hull);
	generated.SetMinQ(QualityBaseline());
	generated.SetTexturePalette(palette.textures);
	generated.SetLegacyTextureMap(palette.textureMap);
	int triangleCount = 0, tileVisits = 0, generatedTriangles = 0;
	const auto chargeTriangles = [&](int count) {
		if(count > MaximumGeneratedTriangles - generatedTriangles)
		{
			error = QStringLiteral("The selected legacy design exceeds the generated geometry limit.");
			return false;
		}
		generatedTriangles += count;
		return true;
	};
	bool foundHull = false;
	AuScFaceList exteriorFaces;
	for(int index = 0; index < mesh.batches.size(); ++index)
	{
		const Batch& batch = mesh.batches[index];
		const bool hatchPanel = IsHatchPanel(batch);
		if((batch.doorId && !hatchPanel) || batch.vertices.isEmpty()) continue;
		AuScFaceList faces;
		const int previousTriangles = triangleCount;
		const Tile tile = palette.tiles.value(TextureKey(batch));
		for(int vertex = 0; vertex + 2 < batch.vertices.size(); vertex += 3)
			if(!AddTriangleFaces(faces, &batch.vertices[vertex], batch, legacy, tile, triangleCount, tileVisits))
			{
				error = QStringLiteral("The selected legacy design exceeds the debug geometry processing limit.");
				return false;
			}
		if(!chargeTriangles(triangleCount - previousTriangles)) return false;
		if(faces.isEmpty()) continue;
		AuScHull hull;
		AuScMesh equipment;
		AuScRoom interior;
		// Open hatch faces are visual meshes. They never enter the hull or the
		// barrier geometry consumed by character movement and floor queries.
		AuScMeshBase* part = batch.equipment || hatchPanel ? static_cast<AuScMeshBase*>(&equipment)
			: batch.interior ? static_cast<AuScMeshBase*>(&interior) : static_cast<AuScMeshBase*>(&hull);
		part->SetFaces(faces);
		part->SetGroupName(hatchPanel ? QStringLiteral("legacy/open-hatch/detail/%1").arg(batch.doorId)
			: QStringLiteral("legacy/geometry/%1/detail/%2").arg(index).arg(batch.detailId));
		if(part == &hull)
		{
			exteriorFaces.AddFaces(faces);
			foundHull = true;
		}
		interior.SetInteriorLighting(false, INTLIGHT_Bright);
		if(part != &hull && !AddPart(generated, *part))
		{
			error = QStringLiteral("The selected legacy design has too many generated parts.");
			return false;
		}
		if(!batch.doorId && !batch.equipment && batch.material != Material::Ladder && batch.material != Material::LadderPole)
		{
			AuScFaceList collision;
			for(int vertex = 0; vertex + 2 < batch.vertices.size(); vertex += 3)
			{
				if(!chargeTriangles(1)) return false;
				const auto point = [&](int offset) {
					const Vertex& value = batch.vertices[vertex + offset];
					return AuScPoint(legacy.AuthorToShip(Point(value.position)), 0, 0, Point(value.normal));
				};
				collision.AddFace(AuScFaceTri(point(0), point(1), point(2)));
			}
			AuScBarrier barrier;
			barrier.SetFaces(collision);
			barrier.SetGroupName(QStringLiteral("legacy/collision/%1").arg(index));
			if(!AddPart(generated, barrier))
			{
				error = QStringLiteral("The selected legacy design has too many generated collision parts.");
				return false;
			}
		}
	}
	if(foundHull)
	{
		AuScHull hull;
		hull.SetFaces(exteriorFaces);
		hull.SetGroupName(QStringLiteral("legacy/debug/static/v4"));
		if(!AddPart(generated, hull))
		{
			error = QStringLiteral("The selected legacy design has too many generated hull parts.");
			return false;
		}
	}
	AuScFaceList windowBoundaries;
	for(const HullWindowBoundary& boundary : mesh.hullWindowBoundaries)
	{
		const QPointF direction = boundary.b - boundary.a;
		const double length = std::hypot(direction.x(), direction.y());
		if(length < .01) continue;
		if(!chargeTriangles(2)) return false;
		const AuPoint normal(direction.y() / length, -direction.x() / length, 0);
		const auto point = [&](const QPointF& position, double height) {
			return AuScPoint(legacy.AuthorToShip(AuPoint(position.x(), position.y(),
				boundary.deck * AuLegacyShip::DeckHeight + height)), 0, 0, normal);
		};
		const AuScPoint a0 = point(boundary.a, 0), b0 = point(boundary.b, 0);
		const AuScPoint a1 = point(boundary.a, AuLegacyShip::DeckHeight), b1 = point(boundary.b, AuLegacyShip::DeckHeight);
		windowBoundaries.AddFace(AuScFaceTri(a0, b0, b1));
		windowBoundaries.AddFace(AuScFaceTri(a0, b1, a1));
	}
	if(!windowBoundaries.isEmpty())
	{
		// Individual vertical spans retain corners and gaps. Do not bound a
		// whole glass draw batch: it can contain every window on this deck.
		AuScBarrier barrier;
		barrier.SetFaces(windowBoundaries);
		barrier.SetGroupName(QStringLiteral("legacy/window-boundary"));
		if(!AddPart(generated, barrier))
		{
			error = QStringLiteral("The selected legacy design has too many generated window collision parts.");
			return false;
		}
	}
	int roomCount = 0;
	for(int index = 0; index < source.rooms.size(); ++index)
	{
		const Room& room = source.rooms[index];
		if(room.roomType == 0 || room.roomType == 12 || room.roomType > 21) continue;
		const QPolygonF boundary = RoomBoundary(room);
		if(boundary.size() < 3 || !Geometry::IsSupportedBoundary(boundary)) continue;
		AuPointArray points;
		for(const QPointF& point : boundary)
			points.append(legacy.AuthorToShip(AuPoint(point.x(), point.y(), room.deck * AuLegacyShip::DeckHeight)));
		AuScRoomVoid volume;
		if(!chargeTriangles(points.size() * 4)) return false;
		volume.MakeExtrusion(points, AuPoint(0, 0, AuLegacyShip::DeckHeight), AuMatrix4x4(), 1, true);
		volume.SetGravity(room.roomType == 11 ? G_Zero : G_Artificial, AuPoint(0, 0, 1));
		volume.SetHoldAccess(HOLD_Inaccessible);
		volume.SetGroupName(QStringLiteral("Legacy room %1").arg(index + 1));
		if(!AddPart(generated, volume))
		{
			error = QStringLiteral("The selected legacy design has too many generated room parts.");
			return false;
		}
		++roomCount;
	}
	// Open panel apertures need voids through the hull's conservative silhouette;
	// otherwise its collision margin can stop a player before entering a room.
	for(const HullOpening& aperture : mesh.hullOpenings)
	{
		const QPointF direction = aperture.b - aperture.a;
		const double length = std::hypot(direction.x(), direction.y());
		if(length < .01) continue;
		// Extend past the two-foot silhouette margin on both sides. Keep the
		// actual mesh jambs and small-door lintel instead of path clearances.
		const QPointF normal(-direction.y() / length * 4, direction.x() / length * 4);
		AuScHullVoid opening;
		opening.MakeExtrusion(Rectangle(legacy, aperture.a, aperture.b, normal,
			aperture.deck * AuLegacyShip::DeckHeight - .1),
			AuPoint(0, 0, (aperture.detailType == 50 ? 7 : AuLegacyShip::DeckHeight) + .2), AuMatrix4x4(), 1, true);
		opening.SetGroupName(QStringLiteral("legacy/opening/detail/%1").arg(aperture.detailId));
		if(!chargeTriangles(12)) return false;
		if(!AddPart(generated, opening))
		{
			error = QStringLiteral("The selected legacy design has too many generated opening parts.");
			return false;
		}
	}
	QHash<quint32, QPolygonF> hatchBoundaries;
	for(const HatchOpening& hatch : mesh.hatchOpenings)
		hatchBoundaries.insert(hatch.detailId, hatch.boundary);
	for(const DoorDefinition& door : DoorDefinitions(source, mesh))
	{
		if(door.type == 3 || door.type == 50) continue;
		AuScHullVoid opening;
		if(IsHatchDetail(door.type))
		{
			const auto boundary = hatchBoundaries.constFind(door.id);
			if(boundary == hatchBoundaries.constEnd()) continue;
			AuPointArray points;
			for(const QPointF& point : boundary.value())
				points.append(legacy.AuthorToShip(AuPoint(point.x(), point.y(), door.deck * AuLegacyShip::DeckHeight - 4)));
			opening.MakeExtrusion(points, AuPoint(0, 0, 8), AuMatrix4x4(), 1, true);
		}
		else
		{
			const QPointF direction = door.b - door.a;
			const double length = std::hypot(direction.x(), direction.y());
			if(length < .01) continue;
			const QPointF normal(-direction.y() / length * 2, direction.x() / length * 2);
			opening.MakeExtrusion(Rectangle(legacy, door.a, door.b, normal, door.deck * AuLegacyShip::DeckHeight - .1),
				AuPoint(0, 0, AuLegacyShip::DeckHeight + .2), AuMatrix4x4(), 1, true);
		}
		opening.SetGroupName(QStringLiteral("legacy/opening/detail/%1").arg(door.id));
		if(!chargeTriangles(12)) return false;
		if(!AddPart(generated, opening))
		{
			error = QStringLiteral("The selected legacy design has too many generated opening parts.");
			return false;
		}
	}
	Mesh stairs;
	AppendStairWalkingSurfaces(source, stairs);
	for(const Batch& batch : stairs.batches)
	{
		AuScFaceList faces;
		for(int vertex = 0; vertex + 2 < batch.vertices.size(); vertex += 3)
		{
			if(!chargeTriangles(1)) return false;
			const auto point = [&](int offset) {
				const Vertex& value = batch.vertices[vertex + offset];
				return AuScPoint(legacy.AuthorToShip(Point(value.position)), 0, 0, Point(value.normal));
			};
			faces.AddFace(AuScFaceTri(point(0), point(1), point(2)));
		}
		if(faces.isEmpty()) continue;
		AuScBarrier barrier;
		barrier.SetFaces(faces);
		barrier.SetGroupName(QStringLiteral("legacy/stair-support"));
		if(!AddPart(generated, barrier))
		{
			error = QStringLiteral("The selected legacy design has too many stair-support parts.");
			return false;
		}
	}
	for(const Detail& detail : source.details)
	{
		if(detail.hiddenRaw || !IsHatchDetail(detail.detailType)
			|| detail.deck < MinimumDeck || detail.deck > MaximumDeck + 1) continue;
		const QPointF center = DecodePoint(detail.packedXY);
		const AuPoint top = legacy.AuthorToShip(AuPoint(center.x(), center.y(), detail.deck * AuLegacyShip::DeckHeight));
		AuScPathLadder ladder;
		ladder.MakePath(top - AuPoint(0, 0, AuLegacyShip::DeckHeight), top);
		const double angle = DetailRotationStep(detail.detailType) * 3.14159265358979323846 / 4;
		ladder.SetFacing(AuPoint(std::cos(angle), std::sin(angle), 0));
		ladder.SetGroupName(QStringLiteral("legacy/hatch-ladder/detail/%1/cell/%2/deck/%3")
			.arg(detail.detailId).arg(detail.packedXY).arg(detail.deck));
		if(!AddPart(generated, ladder))
		{
			error = QStringLiteral("The selected legacy design has too many hatch ladder parts.");
			return false;
		}
	}
	QSet<quint32> occupied;
	QHash<quint64, AuScFaceList> obstacleChunks;
	for(const Detail& detail : source.details)
	{
		if(detail.deck < MinimumDeck || detail.deck > MaximumDeck || !BlocksPathCell(detail.detailType)) continue;
		const quint32 key = (quint32(quint8(detail.deck)) << 16) | detail.packedXY;
		if(occupied.contains(key)) continue;
		occupied.insert(key);
		const QPointF center = DecodePoint(detail.packedXY);
		const QPointF a = center - QPointF(CoordinateSpacing / 2, 0), b = center + QPointF(CoordinateSpacing / 2, 0);
		AuScBarrier barrier;
		barrier.MakeExtrusion(Rectangle(legacy, a, b, QPointF(0, CoordinateSpacing / 2), detail.deck * AuLegacyShip::DeckHeight - .05),
			AuPoint(0, 0, AuLegacyShip::DeckHeight), AuMatrix4x4(), 1, true);
		if(!chargeTriangles(12)) return false;
		const quint64 chunk = quint64(quint8(detail.deck))
			| (quint64(quint16(int(std::floor(center.x() / 32)))) << 8)
			| (quint64(quint16(int(std::floor(center.y() / 32)))) << 24);
		AuScFaceList faces;
		barrier.Faces(faces);
		obstacleChunks[chunk].AddFaces(faces);
	}
	// Preserve each authored obstacle volume, while avoiding one scene part per
	// cell (Station.ship has thousands). Barrier sight tests retain exact faces.
	for(auto chunk = obstacleChunks.constBegin(); chunk != obstacleChunks.constEnd(); ++chunk)
	{
		AuScBarrier barrier;
		barrier.SetFaces(chunk.value());
		barrier.SetGroupName(QStringLiteral("legacy/obstacle-chunk/%1").arg(chunk.key()));
		if(!AddPart(generated, barrier))
		{
			error = QStringLiteral("The selected legacy design has too many obstacle geometry parts.");
			return false;
		}
	}
	if(!foundHull || !roomCount)
	{
		error = QStringLiteral("The selected legacy design has no recovered hull with a boardable room.");
		return false;
	}
	// Build silhouettes without the modern voxel volume pass. Volume is already
	// computed from the accepted legacy source, and carries no working systems.
	generated.Parts().RefreshObstructionsCache();
	generated.SetLegacyHullVolume(quint32(std::max(1.0, std::ceil(double(legacy.Stats().hullVolume) / CubicFeetPerCubicMeter()))));
	if(generated.MemoryUsage() > MaximumGeometryMemory)
	{
		error = QStringLiteral("The generated legacy blueprint exceeds the debug memory limit.");
		return false;
	}
	QByteArray serialized;
	QDataStream stream(&serialized, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_0);
	stream << generated;
	if(stream.status() != QDataStream::Ok || serialized.size() > MaximumGeometryBytes
		|| qCompress(serialized).size() > int(generated.CompressedSizeLimit()))
	{
		error = QStringLiteral("The generated legacy blueprint exceeds the network blueprint-size limit.");
		return false;
	}
	AuScReport generatedReport;
	generated.ReportDesign(generatedReport);
	output = generated;
	report = generatedReport;
	return true;
}

bool TextureMap(const AuLegacyScDesign& legacy, QByteArray& output, QString& error, bool includeOpenHatches)
{
	QMutexLocker guard(&modelCacheMutex);
	output.clear();
	error.clear();
	Design source = legacy.SourceDesign();
	if(source.details.size() > 32768)
	{
		error = QStringLiteral("The design exceeds the legacy material-analysis limit.");
		return false;
	}
	qint64 visits = 0;
	for(const Room& room : source.rooms)
	{
		const QRectF bounds = RoomBoundary(room).boundingRect();
		visits += qint64(std::ceil(bounds.width() / 1.41421356225) + 2)
			* qint64(std::ceil(bounds.height() / 1.41421356225) + 2);
		if(visits > MaximumRoomRasterVisits)
		{
			error = QStringLiteral("The design exceeds the legacy material-analysis limit.");
			return false;
		}
	}
	for(int index = 0; index < source.details.size(); ++index)
		source.details[index].detailId = legacy.DetailRecordId(index);
	const Mesh mesh = BuildMesh(source);
	qint64 vertices = 0;
	for(const Batch& batch : mesh.batches) vertices += batch.vertices.size();
	if(vertices >= 6000000)
	{
		error = QStringLiteral("The design exceeds the legacy material-analysis mesh limit.");
		return false;
	}
	for(const QString& warning : mesh.warnings)
		if(warning.contains(QStringLiteral("truncated")))
		{
			error = QStringLiteral("The design exceeds the legacy material-analysis mesh limit.");
			return false;
		}
	Palette palette;
	if(!palette.Build(mesh, error, includeOpenHatches)) return false;
	output = palette.textureMap;
	return true;
}

bool ReadTextureMap(const QByteArray& bytes, QVector<TextureBinding>& output, QString& error)
{
	output.clear();
	error = QStringLiteral("Invalid legacy material map.");
	if(bytes.isEmpty() || bytes.size() > MaximumTextureMapBytes) return false;
	const QJsonObject map = QJsonDocument::fromJson(bytes).object();
	const QJsonArray bindings = map.value(QStringLiteral("bindings")).toArray();
	if(map.value(QStringLiteral("version")).toInt() != 1 || bindings.isEmpty() || bindings.size() > 224) return false;
	QSet<quint32> used;
	int expectedSide = 0;
	for(const QJsonValue& value : bindings)
	{
		const QJsonObject item = value.toObject();
		TextureBinding binding;
		binding.paletteIndex = item.value(QStringLiteral("palette")).toInt(-1);
		const int x = item.value(QStringLiteral("x")).toInt(-1), y = item.value(QStringLiteral("y")).toInt(-1);
		const int side = item.value(QStringLiteral("side")).toInt();
		if(expectedSide && side != expectedSide) return false;
		expectedSide = side;
		binding.material = item.value(QStringLiteral("material")).toInt(-2);
		binding.resource = item.value(QStringLiteral("resource")).toString();
		const QJsonArray tint = item.value(QStringLiteral("tint")).toArray();
		if(binding.paletteIndex < 0 || binding.paletteIndex >= SCTEXI_Count
			|| (side != 64 && side != 128) || x < 0 || y < 0 || x > 256 - side || y > 256 - side
			|| x % side || y % side || tint.size() != 4 || binding.material < -1
			|| binding.material >= int(sizeof(ShipTextures) / sizeof(ShipTextures[0]))
			|| (!binding.resource.isEmpty() && (!binding.resource.startsWith(QStringLiteral(":/data/"))
				|| binding.resource.size() > 256 || binding.resource.contains(QStringLiteral(".."))))) return false;
		for(int component = 0; component < 4; ++component)
		{
			const double color = tint[component].toDouble(-1);
			if(!std::isfinite(color) || color < 0 || color > 1) return false;
			binding.tint[component] = float(color);
		}
		const quint32 key = (quint32(binding.paletteIndex) << 16) | (quint32(x) << 8) | quint32(y);
		if(used.contains(key)) return false;
		used.insert(key);
		binding.packedTile = QRect(x, y, side, side);
		binding.cutout = binding.resource.isEmpty() && binding.material >= 0;
		if(binding.cutout)
		{
			// Match the offline renderer's inset in recovered atlas units;
			// the bundled image itself has twice that resolution.
			const TextureRegion& region = ShipTextures[binding.material];
			binding.textureInset = QVector2D(.5f / region.width, .5f / region.height);
		}
		output.append(binding);
	}
	error.clear();
	return true;
}

QImage OriginalTexture(const TextureBinding& binding)
{
	QImage image;
	if(!binding.resource.isEmpty()) image.load(binding.resource);
	else if(binding.material >= 0 && binding.material < int(sizeof(ShipTextures) / sizeof(ShipTextures[0])))
	{
		const QImage atlas(QStringLiteral(":/data/legacy_txSpacecraft0Lit.png"));
		if(atlas.isNull()) return QImage();
		const TextureRegion& region = ShipTextures[binding.material];
		image = atlas.copy(region.x * atlas.width() / 128,
			(4096 - region.y - region.height) * atlas.height() / 4096,
			region.width * atlas.width() / 128, region.height * atlas.height() / 4096);
	}
	else
	{
		image = QImage(1, 1, QImage::Format_ARGB32);
		image.fill(Qt::white);
	}
	image = image.convertToFormat(QImage::Format_ARGB32);
	for(int y = 0; y < image.height(); ++y)
	{
		QRgb* pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
		for(int x = 0; x < image.width(); ++x)
			pixels[x] = qRgba(int(qRed(pixels[x]) * binding.tint.x()), int(qGreen(pixels[x]) * binding.tint.y()),
				int(qBlue(pixels[x]) * binding.tint.z()), qAlpha(pixels[x]));
	}
	return image;
}
}
