// Manual CPU regression harness. Compile/run only when requested by the user.
#include "aulegacyshipequipment.h"
#include "aulegacyshipdetails.h"

#include <QCoreApplication>
#include <QTextStream>
#include <cmath>

using namespace AuLegacyShip;
using namespace AuLegacyShip::Rendering;

namespace
{
int failures = 0;
void Require(bool condition, const char* message)
{
	if(!condition) { QTextStream(stderr) << "FAIL: " << message << '\n'; ++failures; }
}
bool Near(double a, double b) { return std::abs(a - b) < .0001; }
Mesh Equipment(int type, int deck = 0, bool hidden = false)
{
	Design design;
	Detail detail;
	detail.detailType = quint8(type); detail.deck = qint8(deck);
	detail.packedXY = 0; detail.hiddenRaw = hidden ? 1 : 0;
	design.details.append(detail);
	Mesh mesh;
	AppendEquipment(design, mesh);
	return mesh;
}
int Vertices(const Mesh& mesh)
{
	int count = 0;
	for(const Batch& batch : mesh.batches) count += batch.vertices.size();
	return count;
}
void Validate(const Mesh& mesh)
{
	Require(!mesh.batches.isEmpty(), "supported procedural object has a model");
	for(const Batch& batch : mesh.batches)
	{
		Require(batch.equipment && batch.interior, "equipment is independently clipped from walls");
		Require(batch.vertices.size() % 3 == 0, "complete equipment triangles");
		for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
		{
			const Vertex& a = batch.vertices[i];
			const QVector3D cross = QVector3D::crossProduct(batch.vertices[i + 1].position - a.position,
				batch.vertices[i + 2].position - a.position);
			Require(cross.lengthSquared() > 1e-10f, "nondegenerate geometry");
			Require(QVector3D::dotProduct(cross, a.normal) > 0, "winding follows the recovered outward normal");
		}
		for(const Vertex& vertex : batch.vertices)
		{
			Require(Near(vertex.normal.length(), 1), "normalized normals");
			Require(std::isfinite(vertex.position.x()) && std::isfinite(vertex.position.y())
				&& std::isfinite(vertex.position.z()), "finite positions");
			Require(vertex.uv.x() >= 0 && vertex.uv.x() <= 1 && vertex.uv.y() >= 0
				&& vertex.uv.y() <= 1, "local atlas coordinates");
		}
	}
}
}

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	const int types[] = {2, 10, 11, 14, 16, 18, 20, 23, 24, 32, 65, 8};
	for(int type : types)
	{
		Require(EquipmentSupported(type), "supported equipment declaration");
		Validate(Equipment(type));
		Require(Vertices(Equipment(type, 0, true)) == 0, "hidden equipment does not render");
	}
	for(int type : {10, 14, 24})
	{
		const Mesh mesh = Equipment(type, 2);
		Require(Vertices(mesh) == 84, "drive has eight side quads and six cap quads");
		Require(Near(mesh.minimum.x(), -1.41421356225) && Near(mesh.maximum.x(), 1.41421356225), "drive footprint");
		Require(Near(mesh.minimum.z(), 16.1) && Near(mesh.maximum.z(), 23.9), "drive deck height and clearance");
		Require(mesh.batches.first().effectFrameCount == 4, "drive animation frame count");
		Require(mesh.batches.first().effectFirstFrame == (type == 10 ? 16 : type == 14 ? 20 : 55), "drive effect series");
	}
	const Mesh surgery = Equipment(11);
	Require(Vertices(surgery) == 48 && surgery.batches.first().twoSided, "surgery tube is open and double sided");
	Require(surgery.batches.first().effectFirstFrame == 40 && surgery.batches.first().effectFrameCount == 3, "surgery effect series");
	const Mesh sensor = Equipment(16);
	Require(Vertices(sensor) == 36 && sensor.batches.size() == 2, "sensor has separate unanimated cap material");
	const Mesh berth = Equipment(65), doubleBerth = Equipment(2);
	Require(Vertices(berth) == 36 && Vertices(doubleBerth) == 72, "single and double berth slab count");
	Require(Near(berth.minimum.z(), .5) && Near(berth.maximum.z(), 1.5)
		&& Near(doubleBerth.maximum.z(), 5), "recovered berth heights");
	Require(Near(berth.minimum.x(), -1.41421356225) && Near(berth.maximum.x(), 4.24264068675), "asymmetric berth anchor");
	const Mesh rotated = Equipment(RotateDetailType(65, 2));
	Require(Near(rotated.minimum.y(), berth.minimum.x()) && Near(rotated.maximum.y(), berth.maximum.x()), "berth rotates around detail anchor");
	const Mesh ladder = Equipment(8, 1);
	Require(Vertices(ladder) == 18 && Near(ladder.minimum.z(), 0) && Near(ladder.maximum.z(), 8), "hatch ladder extends to previous deck");
	for(const Batch& batch : ladder.batches) Require(batch.deck == 0 && batch.twoSided, "ladder belongs to lower deck");
	const Mesh pad = Equipment(20);
	Require(Vertices(pad) == 66 && Near(pad.minimum.z(), 1.0 / 24)
		&& Near(pad.maximum.z(), 1.0 / 6), "transporter pad has original low profile and no bottom cap");
	QTextStream(stdout) << (failures ? "Equipment checks failed: " : "Equipment checks passed: ") << failures << '\n';
	return failures ? 1 : 0;
}
