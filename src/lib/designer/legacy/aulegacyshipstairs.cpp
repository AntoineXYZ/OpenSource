#include "aulegacyshipstairs.h"
#include "aulegacyshipdetails.h"

#include <QHash>
#include <algorithm>
#include <cmath>

namespace AuLegacyShip
{
namespace Rendering
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
constexpr double EdgeInset = 1.0 / 96.0;
constexpr double Rise = 8.0 - EdgeInset;
constexpr int MaximumVertices = 6000000;

struct Point
{
	double x, y, z;
	Point(double px = 0, double py = 0, double pz = 0) : x(px), y(py), z(pz) {}
	Point operator+(const Point& p) const { return Point(x+p.x,y+p.y,z+p.z); }
	Point operator-(const Point& p) const { return Point(x-p.x,y-p.y,z-p.z); }
	Point operator*(double scale) const { return Point(x*scale,y*scale,z*scale); }
};

struct Stairs
{
	Mesh& mesh;
	QHash<int,int> batches;
	int vertices = 0;
	int deck = 0;
	bool hasBounds = false;
	double cosine = 1, sine = 0;
	QPointF origin;

	explicit Stairs(Mesh& target) : mesh(target)
	{
		for(const Batch& batch : mesh.batches) vertices += batch.vertices.size();
		hasBounds = vertices != 0;
	}

	QVector3D Position(const Point& p) const
	{
		return QVector3D(float(origin.x()+p.x*cosine-p.y*sine),
			float(origin.y()+p.x*sine+p.y*cosine),float(deck*DeckHeight+p.z));
	}

	void Triangle(Material material, const Point& normal, const Point& a, const Point& b, const Point& c,
		const QVector2D& ua, const QVector2D& ub, const QVector2D& uc)
	{
		if(vertices + 3 > MaximumVertices) return;
		const QVector3D pa = Position(a), pb = Position(b), pc = Position(c);
		QVector3D n(float(normal.x*cosine-normal.y*sine),float(normal.x*sine+normal.y*cosine),float(normal.z));
		n.normalize();
		const QVector3D cross = QVector3D::crossProduct(pb-pa,pc-pa);
		if(cross.lengthSquared() < 1e-12f) return;
		const int key = (deck+43)*128+int(material);
		int index = batches.value(key,-1);
		if(index < 0)
		{
			index = mesh.batches.size(); batches.insert(key,index);
			Batch batch; batch.material = material; batch.deck = deck; batch.interior = true; batch.equipment = true;
			mesh.batches.append(batch);
		}
		QVector<Vertex>& out = mesh.batches[index].vertices;
		out.append({pa,n,ua});
		if(QVector3D::dotProduct(cross,n) >= 0) { out.append({pb,n,ub}); out.append({pc,n,uc}); }
		else { out.append({pc,n,uc}); out.append({pb,n,ub}); }
		if(!hasBounds) { mesh.minimum = mesh.maximum = pa; hasBounds = true; }
		for(const QVector3D& point : {pa,pb,pc})
			for(int axis = 0; axis < 3; ++axis)
			{
				mesh.minimum[axis] = std::min(mesh.minimum[axis],point[axis]);
				mesh.maximum[axis] = std::max(mesh.maximum[axis],point[axis]);
			}
		vertices += 3;
	}

	void Quad(Material material, const Point& normal, const Point& a, const Point& b,
		const Point& c, const Point& d, const QVector2D& ua = QVector2D(0,0),
		const QVector2D& ub = QVector2D(1,0), const QVector2D& uc = QVector2D(1,1),
		const QVector2D& ud = QVector2D(0,1))
	{
		Triangle(material,normal,a,b,c,ua,ub,uc);
		Triangle(material,normal,a,c,d,ua,uc,ud);
	}

	// Each original step has a riser, tread and two triangular side faces.
	// Its lower edge lies on the sloping casing, not on a solid rectangular box.
	void Flight(const Point& start, const Point& across, const Point& run, double height, int count)
	{
		const Point rise(0,0,height/count), step = run*(1.0/count);
		const Point back(-run.x,-run.y,0), near(-across.x,-across.y,0), far(across.x,across.y,0);
		for(int i = 0; i < count; ++i)
		{
			const Point a = start+(step+rise)*i, b = a+rise, c = b+step;
			const Point d = a+across, e = b+across, f = c+across;
			Quad(Material::StairRiser,back,a,b,e,d,{0,1},{0,0},{1,0},{1,1});
			Quad(Material::StairTread,{0,0,1},b,c,f,e,{0,0},{0,1},{1,1},{1,0});
			Triangle(Material::StairRiser,near,a,b,c,{1,1},{1,0},{0,0});
			Triangle(Material::StairRiser,far,d,e,f,{0,1},{0,0},{1,0});
		}
	}

	void Landing(double xmin, double ymin, double xmax, double ymax)
	{
		const double top = Rise/2, bottom = top-top/7;
		const Point a(xmin,ymin,top), b(xmin,ymax,top), c(xmax,ymax,top), d(xmax,ymin,top);
		const Point e(xmin,ymin,bottom), f(xmin,ymax,bottom), g(xmax,ymax,bottom), h(xmax,ymin,bottom);
		Quad(Material::StairTread,{0,0,1},a,b,c,d,{0,0},{0,1},{1,1},{1,0});
		Quad(Material::StairCasing,{0,0,-1},f,e,h,g,{0,0},{0,1},{1,1},{1,0});
		Quad(Material::StairRiser,{0,-1,0},e,a,d,h,{0,1},{0,0},{1,0},{1,1});
		Quad(Material::StairRiser,{-1,0,0},f,b,a,e,{0,1},{0,0},{1,0},{1,1});
		Quad(Material::StairRiser,{0,1,0},g,c,b,f,{0,1},{0,0},{1,0},{1,1});
		Quad(Material::StairRiser,{1,0,0},h,d,c,g,{0,1},{0,0},{1,0},{1,1});
	}

	void DetailGeometry(const Detail& detail)
	{
		deck = detail.deck; origin = DecodePoint(detail.packedXY);
		const int orientation = DetailRotationStep(detail.detailType);
		const double angle = double(float(orientation*Pi/4));
		cosine = std::cos(angle); sine = std::sin(angle);
		const bool diagonal = (orientation&1) != 0;
		const int family = (detail.detailType-101)/8;
		const double e = EdgeInset, half = Rise/2;
		if(family == 0)
		{
			const double length = diagonal ? 12 : GridSpacing*2, width = diagonal ? 4 : GridSpacing;
			Quad(Material::StairCasing,{1,0,-1},{0,e,0},{length,e,Rise},
				{length,width-e,Rise},{0,width-e,0});
			Flight({0,width-e,0},{0,-(width-2*e),0},{length,0,0},Rise,13);
			return;
		}
		const double length = diagonal ? 8 : GridSpacing;
		if(family == 1 || family == 2)
		{
			const double width = diagonal ? 4 : GridSpacing;
			if(family == 1)
			{
				Landing(length,e,length+width-e,width);
				Quad(Material::StairCasing,{1,0,-1},{0,width-e,0},{0,e,0},{length,e,half},{length,width,half});
				Quad(Material::StairCasing,{0,1,-1},{length,width,half},{length+width-e,width,half},
					{length+width-e,length+width,Rise},{length+e,length+width,Rise});
				Flight({0,width-e,0},{0,-(width-2*e),0},{length,0,0},half,7);
				Flight({length+e,width,half},{width-2*e,0,0},{0,length,0},half,7);
			}
			else
			{
				Landing(length,-width,length+width-e,-e);
				Quad(Material::StairCasing,{1,0,-1},{0,-e,0},{0,-width+e,0},{length,-width,half},{length,-e,half});
				Quad(Material::StairCasing,{0,-1,-1},{length+width-e,-width,half},{length,-width,half},
					{length+e,-length-width,Rise},{length+width-e,-length-width,Rise});
				Flight({0,-e,0},{0,-(width-2*e),0},{length,0,0},half,7);
				Flight({length+width-e,-width,half},{-(width-2*e),0,0},{0,-length,0},half,7);
			}
			return;
		}
		const double width = diagonal ? 6 : GridSpacing, landing = diagonal ? 4 : GridSpacing;
		if(family == 3)
		{
			Landing(length,e,length+landing-e,2*width-e);
			Quad(Material::StairCasing,{1,0,-1},{0,width,0},{0,e,0},{length,e,half},{length,width,half});
			Quad(Material::StairCasing,{-1,0,-1},{length,width,half},{length,2*width-e,half},
				{0,2*width-e,Rise},{0,width,Rise});
			Flight({0,width,0},{0,-(width-e),0},{length,0,0},half,7);
			Flight({length,width,half},{0,width-e,0},{-length,0,0},half,7);
		}
		else
		{
			Landing(length,-2*width+e,length+landing-e,-e);
			Quad(Material::StairCasing,{1,0,-1},{0,-e,0},{0,-width,0},{length,-width,half},{length,-e,half});
			Quad(Material::StairCasing,{-1,0,-1},{length,-2*width+e,half},{length,-width,half},
				{0,-width,Rise},{0,-2*width+e,Rise});
			Flight({0,-e,0},{0,-(width-e),0},{length,0,0},half,7);
			Flight({length,-2*width+e,half},{0,width-e,0},{-length,0,0},half,7);
		}
	}
};
}

void AppendStairGeometry(const Design& design, Mesh& mesh)
{
	Stairs stairs(mesh);
	for(const Detail& detail : design.details)
	{
		if(detail.hiddenRaw || detail.detailType < 101 || detail.detailType > 140
			|| detail.deck < MinimumDeck || detail.deck > MaximumDeck) continue;
		if(stairs.vertices+300 > MaximumVertices)
		{
			const QString message = QStringLiteral("The render preview exceeded its geometry limit; remaining stairs were omitted.");
			if(!mesh.warnings.contains(message)) mesh.warnings.append(message);
			break;
		}
		stairs.DetailGeometry(detail);
	}
}

void AppendStairWalkingSurfaces(const Design& design, Mesh& mesh)
{
	// The original TestStairs ramps follow the casing planes. Keep this
	// derivation out of the render generator used by wireframe/placement.
	Mesh rendered;
	AppendStairGeometry(design, rendered);
	for(const Batch& batch : rendered.batches)
	{
		if(batch.material != Material::StairCasing) continue;
		Batch support;
		support.material = Material::StairTread;
		support.deck = batch.deck;
		support.interior = true;
		support.equipment = true;
		for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
		{
			Vertex a = batch.vertices.at(i), b = batch.vertices.at(i + 1), c = batch.vertices.at(i + 2);
			const QVector3D normal = -QVector3D::crossProduct(b.position - a.position, c.position - a.position).normalized();
			if(normal.z() <= 0) continue;
			if(std::abs(normal.x()) < 1e-6f && std::abs(normal.y()) < 1e-6f)
			{
				// The flat casing is the landing underside, one riser below its top.
				const QVector3D rise(0, 0, float(Rise / 14));
				a.position += rise; b.position += rise; c.position += rise;
			}
			a.normal = b.normal = c.normal = normal;
			support.vertices.append(a); support.vertices.append(c); support.vertices.append(b);
		}
		if(!support.vertices.isEmpty()) mesh.batches.append(support);
	}
}
}
}
