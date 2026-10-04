// User-built CPU mesh checks; no OpenGL context or game session is required.
#include "aulegacyshipmesh.h"
#include "aulegacyshipgeometry.h"

#include <QCoreApplication>
#include <QTextStream>
#include <cmath>
#include <limits>

using namespace AuLegacyShip;
using namespace AuLegacyShip::Rendering;

namespace
{
int failures = 0;

void Require(bool condition, const char* message)
{
	if(!condition)
	{
		QTextStream(stderr) << "FAIL: " << message << '\n';
		++failures;
	}
}

Room MakeRoom(int type, int deck, const QPolygonF& polygon)
{
	Room room;
	room.roomId = quint16(deck + 100);
	room.roomType = quint8(type);
	room.deck = qint8(deck);
	room.hasWorkingBoundary = true;
	room.workingBoundary = polygon;
	return room;
}

QPolygonF Rectangle(double x0, double y0, double x1, double y1)
{
	QPolygonF result;
	result << QPointF(x0 * GridSpacing, y0 * GridSpacing)
		<< QPointF(x1 * GridSpacing, y0 * GridSpacing)
		<< QPointF(x1 * GridSpacing, y1 * GridSpacing)
		<< QPointF(x0 * GridSpacing, y1 * GridSpacing);
	return result;
}

double Area(const Mesh& mesh, Material material, int deck, bool interior)
{
	double result = 0;
	for(const Batch& batch : mesh.batches)
		if(batch.material == material && batch.deck == deck && batch.interior == interior)
			for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
				result += QVector3D::crossProduct(batch.vertices[i + 1].position - batch.vertices[i].position,
					batch.vertices[i + 2].position - batch.vertices[i].position).length() * .5;
	return result;
}

bool Near(double a, double b) { return std::abs(a - b) < .002; }

void CheckMesh(const Mesh& mesh)
{
	for(const Batch& batch : mesh.batches)
	{
		Require(batch.vertices.size() % 3 == 0, "complete triangles");
		for(const Vertex& vertex : batch.vertices)
		{
			Require(std::isfinite(vertex.position.x()) && std::isfinite(vertex.position.y())
				&& std::isfinite(vertex.position.z()), "finite positions");
			Require(std::isfinite(vertex.uv.x()) && std::isfinite(vertex.uv.y()), "finite texture coordinates");
			Require(Near(vertex.normal.length(), 1), "unit surface normals");
		}
	}
}

void CheckTurret(const Mesh& mesh, int deck, const QVector3D& center, const QVector3D& outward,
	const QVector3D& right, bool side)
{
	int domeTriangles[2] = {0, 0};
	double frameArea[2] = {0, 0};
	for(const Batch& batch : mesh.batches)
	{
		if(batch.material != Material::Turret || batch.deck != deck) continue;
		const int face = batch.interior ? 1 : 0;
		for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
		{
			const Vertex& a = batch.vertices[i];
			const Vertex& b = batch.vertices[i + 1];
			const Vertex& c = batch.vertices[i + 2];
			const QVector3D centroid = (a.position + b.position + c.position) / 3 - center;
			const bool frame = Near(QVector3D::dotProduct(a.position - center, outward), 0)
				&& Near(QVector3D::dotProduct(b.position - center, outward), 0)
				&& Near(QVector3D::dotProduct(c.position - center, outward), 0);
			const float sign = batch.interior ? -1.0f : 1.0f;
			if(frame)
			{
				frameArea[face] += QVector3D::crossProduct(b.position - a.position, c.position - a.position).length() * .5;
				Require(QVector3D::dotProduct(a.normal, outward) * sign > .999f, "turret frame normals face correctly");
			}
			else
			{
				++domeTriangles[face];
				Require(QVector3D::dotProduct(a.normal, centroid) * sign > 0, "turret dome normals face correctly");
			}
			for(int j = 0; j < 3; ++j)
			{
				const Vertex& vertex = batch.vertices[i + j];
				const double depth = QVector3D::dotProduct(vertex.position - center, outward);
				Require(depth > -.002 && depth < CoordinateSpacing + .002, "turret stays within original dome depth");
				if(side && frame && Near(std::abs(QVector3D::dotProduct(vertex.position - center, right)), 2)
					&& Near(std::abs(vertex.position.z() - center.z()), 2))
					Require(Near(vertex.uv.x(), .6767767) && Near(vertex.uv.y(), .6767767),
						"side turret frame retains original diagonal texture coordinates");
			}
		}
	}
	for(int face = 0; face < 2; ++face)
	{
		Require(domeTriangles[face] == 16, "turret has sixteen dome triangles per face");
		Require(Near(frameArea[face], side ? 4 * CoordinateSpacing * CoordinateSpacing - 8 * CoordinateSpacing : 0),
			"turret frame surrounds the exact octagonal aperture");
	}
}
}

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	const double cellArea = GridSpacing * GridSpacing;
	const QPolygonF rectangle = Rectangle(-1, -1, 1, 1);
	Design design;
	design.header.armor = 10;
	design.header.streamline = 1;
	design.rooms.append(MakeRoom(6, 0, rectangle));
	Mesh mesh = BuildMesh(design);
	CheckMesh(mesh);
	Require(Near(Area(mesh, Material::InteriorFloor, 0, true), 4 * cellArea), "single-deck floor area");
	Require(Near(Area(mesh, Material::HullBottom, 0, false), 4 * cellArea), "single-deck bottom area");
	Require(Near(Area(mesh, Material::HullTop, 0, false), 4 * cellArea), "single-deck top area");
	Require(Near(Area(mesh, Material::HullWallWindow, 0, false), 0), "armor ten hall has no wall glass");

	design.rooms.append(MakeRoom(6, 1, rectangle));
	mesh = BuildMesh(design);
	CheckMesh(mesh);
	Require(Near(Area(mesh, Material::InteriorFloor, 1, true), 4 * cellArea), "upper-deck floor area");
	Require(Near(Area(mesh, Material::InteriorCeiling, 0, true), 4 * cellArea), "lower-deck ceiling area");
	Require(Near(Area(mesh, Material::HullTop, 0, false), 0), "no hull top between adjacent decks");
	Require(Near(Area(mesh, Material::HullBottom, 1, false), 0), "no hull bottom between adjacent decks");

	for(int type : {11, 15})
	{
		design.rooms[1].roomType = quint8(type);
		mesh = BuildMesh(design);
		CheckMesh(mesh);
		Require(Near(Area(mesh, Material::InteriorFloor, 1, true), 0), "open-below has no upper floor over a hall");
		Require(Near(Area(mesh, Material::InteriorCeiling, 0, true), 0), "open-below removes lower ceiling");
		design.rooms[0].roomType = 9;
		mesh = BuildMesh(design);
		Require(Near(Area(mesh, Material::InteriorFloor, 1, true), 4 * cellArea), "open-below over fuel retains a floor");
		design.rooms.removeFirst();
		mesh = BuildMesh(design);
		Require(Near(Area(mesh, Material::InteriorFloor, 1, true), 4 * cellArea), "open-below with no deck beneath retains a floor");
		design.rooms.prepend(MakeRoom(6, 0, rectangle));
	}

	design.rooms.clear();
	QPolygonF concave;
	concave << QPointF(0, 0) << QPointF(2 * GridSpacing, 0) << QPointF(2 * GridSpacing, GridSpacing)
		<< QPointF(GridSpacing, GridSpacing) << QPointF(GridSpacing, 2 * GridSpacing)
		<< QPointF(0, 2 * GridSpacing);
	design.rooms.append(MakeRoom(6, -2, concave));
	mesh = BuildMesh(design);
	CheckMesh(mesh);
	Require(Near(Area(mesh, Material::InteriorFloor, -2, true), 3 * cellArea), "concave L does not fill its missing square");
	Require(Near(mesh.minimum.z(), -16) && Near(mesh.maximum.z(), -8), "signed deck elevations");

	// A square plus its neighbor must have the same exterior area whether it
	// was authored as one room or as two independently numbered rooms.
	design.rooms[0] = MakeRoom(6, 0, Rectangle(-1, 0, 1, 1));
	const Mesh joined = BuildMesh(design);
	design.rooms[0] = MakeRoom(6, 0, Rectangle(-1, 0, 0, 1));
	design.rooms.append(MakeRoom(6, 0, Rectangle(0, 0, 1, 1)));
	design.rooms[1].roomSubtype = 1;
	const Mesh divided = BuildMesh(design);
	Require(Near(Area(joined, Material::HullWall, 0, false), Area(divided, Material::HullWall, 0, false)),
		"neighboring rooms do not gain an internal hull wall");
	Require(Area(divided, Material::InteriorWallFinished, 0, true)
		+ Area(divided, Material::InteriorWallFill, 0, true) > 0, "different room numbers retain their partition");

	design.rooms = {MakeRoom(6, 0, Rectangle(-1, 0, 1, 1)),
		MakeRoom(6, 1, Rectangle(-1, 0, 0, 1)), MakeRoom(15, 1, Rectangle(0, 0, 1, 1))};
	mesh = BuildMesh(design);
	Require(Area(mesh, Material::InteriorWallFinished, 1, true)
		+ Area(mesh, Material::InteriorWallFill, 1, true) > 0,
		"open-below retains partition against current hall with matching inherited tag");

	Detail hatch;
	hatch.deck = 0;
	hatch.detailType = 8;
	for(int packed : {0x0101, 0})
	{
		hatch.packedXY = quint16(packed);
		design.details = {hatch};
		design.rooms = {MakeRoom(6, 0, rectangle)};
		mesh = BuildMesh(design);
		CheckMesh(mesh);
		Require(Near(Area(mesh, Material::InteriorFloor, 0, true), 4 * cellArea - 16),
			"square/diamond hatch removes exactly sixteen square feet of floor");
		Require(Near(Area(mesh, Material::InteriorHatch, 0, true), 16),
			"closed hatch covers its aperture without overlapping the floor");
	}
	design.details.clear();

	// The top and bottom turret share an octagonal opening, not the sixteen
	// square-foot hatch. The top record belongs to the plane above its room.
	Detail turret;
	turret.packedXY = 0;
	for(bool top : {false, true})
	{
		turret.deck = top ? 1 : 0;
		turret.detailType = top ? 22 : 33;
		design.details = {turret};
		design.rooms = {MakeRoom(6, 0, rectangle)};
		mesh = BuildMesh(design);
		CheckMesh(mesh);
		const double apertureArea = 8 * CoordinateSpacing;
		Require(Near(Area(mesh, top ? Material::HullTop : Material::HullBottom, 0, false), 4 * cellArea - apertureArea),
			"top/bottom turret cuts exact octagonal hull opening");
		Require(Near(Area(mesh, Material::InteriorFloor, 0, true), 4 * cellArea - (top ? 0 : apertureArea)),
			"bottom turret cuts matching interior floor opening");
		Require(Near(Area(mesh, Material::InteriorHatch, 0, true), 0), "turret aperture is not covered by a hatch");
		CheckTurret(mesh, 0, QVector3D(0, 0, top ? 8 : 0), QVector3D(0, 0, top ? 1 : -1),
			QVector3D(1, 0, 0), false);
		Require(Near(top ? mesh.maximum.z() : mesh.minimum.z(), top ? 8 + CoordinateSpacing : -CoordinateSpacing),
			"top/bottom turret extends exactly one half-cell beyond the hull plane");
	}
	// Type 31's side profile stands 1.5 feet off the wall; the dome starts
	// there at height four and cuts the whole 4 +/- CoordinateSpacing strip.
	turret.deck = 0;
	turret.detailType = 21;
	turret.packedXY = 0xfe00;
	design.details = {turret};
	mesh = BuildMesh(design);
	CheckMesh(mesh);
	CheckTurret(mesh, 0, QVector3D(float(-GridSpacing - 1.5), 0, 4), QVector3D(-1, 0, 0),
		QVector3D(0, 1, 0), true);
	Require(Near(mesh.minimum.x(), -GridSpacing - 1.5 - CoordinateSpacing), "side turret uses original profile outset and radius");
	Require(Near(Area(mesh, Material::InteriorFloor, 0, true), 4 * cellArea), "side turret leaves horizontal floor intact");
	design.details.clear();

	design.rooms = {MakeRoom(6, 0, Rectangle(-1, 0, 0, 1)), MakeRoom(6, 0, Rectangle(0, 0, 1, 1))};
	design.rooms[1].roomSubtype = 1;
	Detail door;
	door.deck = 0;
	door.detailType = 5;
	door.packedXY = 0x0001;
	design.details = {door};
	mesh = BuildMesh(design);
	CheckMesh(mesh);
	Require(Near(Area(mesh, Material::InteriorDoor, 0, true), 80), "small door is five by eight feet on both sides");
	design.details[0].detailType = 55;
	mesh = BuildMesh(design);
	Require(Near(Area(mesh, Material::InteriorWallFinished, 0, true)
		+ Area(mesh, Material::InteriorWallFill, 0, true), 0), "wall opening removes the entire interior span");
	design.details.clear();

	// Exercise every spacecraft palette type at each allowed armor value.
	for(int type = 1; type <= 21; ++type)
	{
		if(type == 12) continue;
		for(int armor = 0; armor <= 10; ++armor)
		{
			design.rooms = {MakeRoom(type, 0, rectangle)};
			design.header.armor = quint8(armor);
			mesh = BuildMesh(design);
			Require(!mesh.batches.isEmpty(), "all spacecraft room/armor combinations render");
			CheckMesh(mesh);
		}
	}
	design.header.armor = 10;
	for(int mask = 1; mask < 16; ++mask)
	{
		for(const QPointF& point : {QPointF(.875, .5), QPointF(.5, .875), QPointF(.125, .5), QPointF(.5, .125)})
		{
			const QPolygonF cell = Geometry::FillShape(quint8(mask), (point - QPointF(2, 3)) * GridSpacing);
			if(cell.isEmpty()) continue;
			double area = 0;
			for(int i = 0; i < cell.size(); ++i)
			{
				const QPointF a = cell[i], b = cell[(i + 1) % cell.size()];
				area += a.x() * b.y() - a.y() * b.x();
			}
			design.rooms = {MakeRoom(6, 0, cell)};
			mesh = BuildMesh(design);
			CheckMesh(mesh);
			Require(Near(Area(mesh, Material::InteriorFloor, 0, true), std::abs(area) * .5),
				"cell-mask mesh area matches authored triangles, squares, diamonds and parallelograms");
		}
	}
	design.rooms[0].workingBoundary[0].setX(std::numeric_limits<double>::quiet_NaN());
	mesh = BuildMesh(design);
	Require(mesh.batches.isEmpty() && !mesh.warnings.isEmpty(), "invalid geometry is reported and omitted");
	QPolygonF collinear;
	collinear << QPointF(0, 0) << QPointF(GridSpacing, 0) << QPointF(2 * GridSpacing, 0);
	design.rooms = {MakeRoom(6, 0, collinear)};
	mesh = BuildMesh(design);
	Require(mesh.batches.isEmpty() && !mesh.warnings.isEmpty(), "zero-area boundary does not produce phantom cells");

	QTextStream(stdout) << (failures ? "FAIL: " : "PASS: ") << failures << " failures\n";
	return failures ? 1 : 0;
}
