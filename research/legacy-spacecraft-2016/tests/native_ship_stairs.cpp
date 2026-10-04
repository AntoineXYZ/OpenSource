// Manual native regression harness. The assistant must never compile this.
#include "aulegacyshipstairs.h"
#include <QCoreApplication>
#include <QTextStream>
#include <cmath>

using namespace AuLegacyShip;
using namespace AuLegacyShip::Rendering;
namespace
{
int failures = 0;
void Check(bool value, const char* message)
{
	if(!value) { ++failures; QTextStream(stderr) << "FAIL: " << message << '\n'; }
}
bool Close(double a, double b) { return std::abs(a-b) < 2e-5; }
Mesh Stair(int type, bool hidden = false, int deck = 0)
{
	Design design; Detail detail;
	detail.detailType = quint8(type); detail.hiddenRaw = hidden ? 1 : 0;
	detail.deck = qint8(deck);
	design.details.append(detail);
	Mesh mesh; AppendStairGeometry(design,mesh); return mesh;
}
int Count(const Mesh& mesh, Material material)
{
	int count = 0;
	for(const Batch& batch : mesh.batches) if(batch.material == material) count += batch.vertices.size();
	return count;
}
}
int main(int argc, char** argv)
{
	QCoreApplication app(argc,argv);
	const double e = 1.0/96.0, rise = 8-e, grid = GridSpacing;
	for(int type = 101; type <= 140; ++type)
	{
		const Mesh mesh = Stair(type);
		const bool straight = type <= 108;
		Check(Count(mesh,Material::StairCasing) == (straight ? 6 : 18), "original casing vertex count");
		Check(Count(mesh,Material::StairRiser) == (straight ? 156 : 192), "original riser/side vertex count");
		Check(Count(mesh,Material::StairTread) == (straight ? 78 : 90), "original tread vertex count");
		Check(Close(mesh.minimum.z(),0) && Close(mesh.maximum.z(),rise), "original rise preserved for every orientation");
		for(const Batch& batch : mesh.batches)
		{
			Check(batch.interior && batch.equipment && batch.deck == 0 && batch.degreesPerSecond == 0, "stairs are static interior equipment");
			for(int i = 0; i < batch.vertices.size(); i += 3)
			{
				const Vertex& a = batch.vertices.at(i);
				const Vertex& b = batch.vertices.at(i+1);
				const Vertex& c = batch.vertices.at(i+2);
				const QVector3D cross = QVector3D::crossProduct(b.position-a.position,c.position-a.position);
				Check(cross.lengthSquared() > 1e-10f && QVector3D::dotProduct(cross,a.normal) > 0,
					"nondegenerate triangles face their supplied normal");
				Check(Close(a.normal.length(),1), "unit transformed normals");
			}
		}
	}
	Mesh mesh = Stair(101);
	Check(Close(mesh.minimum.x(),0) && Close(mesh.maximum.x(),2*grid)
		&& Close(mesh.minimum.y(),e) && Close(mesh.maximum.y(),grid-e), "axis straight dimensions");
	mesh = Stair(109);
	Check(Close(mesh.minimum.x(),0) && Close(mesh.maximum.x(),2*grid-e)
		&& Close(mesh.minimum.y(),e) && Close(mesh.maximum.y(),2*grid), "left quarter-turn bounds");
	mesh = Stair(117);
	Check(Close(mesh.minimum.x(),0) && Close(mesh.maximum.x(),2*grid-e)
		&& Close(mesh.minimum.y(),-2*grid) && Close(mesh.maximum.y(),-e), "right quarter-turn bounds");
	mesh = Stair(125);
	Check(Close(mesh.maximum.x(),2*grid-e) && Close(mesh.minimum.y(),e)
		&& Close(mesh.maximum.y(),2*grid-e), "left U-turn bounds");
	mesh = Stair(133);
	Check(Close(mesh.maximum.x(),2*grid-e) && Close(mesh.minimum.y(),-2*grid+e)
		&& Close(mesh.maximum.y(),-e), "right U-turn bounds");
	Check(Stair(101,true).batches.isEmpty(), "hidden stairs are omitted");
	Check(Stair(100).batches.isEmpty() && Stair(141).batches.isEmpty(), "helper handles only stair families");
	Check(Stair(101,false,MinimumDeck-1).batches.isEmpty()
		&& Stair(101,false,MaximumDeck+1).batches.isEmpty(), "out-of-range stair decks are omitted");
	QTextStream(stdout) << "Stair geometry failures: " << failures << '\n';
	return failures ? 1 : 0;
}
