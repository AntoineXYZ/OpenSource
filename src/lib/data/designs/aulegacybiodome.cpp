#include "aulegacybiodome.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycitystate.h"
#include "audef.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace AuLegacyBd
{
namespace Biodome
{
namespace
{
constexpr double Pi = 3.14159265358979323846;

struct Builder
{
	Geometry::Mesh mesh;
	bool hasBounds = false;
	void Triangle(int slot, const QVector3D& a, const QVector3D& b, const QVector3D& c,
		const QVector2D& ua = {0, 0}, const QVector2D& ub = {1, 0}, const QVector2D& uc = {1, 1})
	{
		QVector3D normal = QVector3D::crossProduct(b-a,c-a);
		if(normal.lengthSquared() < 1e-12f) return;
		normal.normalize();
		int index = -1;
		for(int i=0;i<mesh.batches.size();++i) if(mesh.batches[i].textureSlot==slot) { index=i; break; }
		if(index<0) { Geometry::Batch batch; batch.textureSlot=slot; index=mesh.batches.size(); mesh.batches.append(batch); }
		auto& vertices=mesh.batches[index].vertices;
		vertices.append({a,normal,ua}); vertices.append({b,normal,ub}); vertices.append({c,normal,uc});
		if(!hasBounds) { mesh.minimum=mesh.maximum=a; hasBounds=true; }
		for(const auto& point : {a,b,c}) for(int axis=0;axis<3;++axis)
		{ mesh.minimum[axis]=std::min(mesh.minimum[axis],point[axis]); mesh.maximum[axis]=std::max(mesh.maximum[axis],point[axis]); }
	}
	void Quad(int slot,const QVector3D& a,const QVector3D& b,const QVector3D& c,const QVector3D& d)
	{ Triangle(slot,a,b,c); Triangle(slot,a,c,d,{0,0},{1,1},{0,1}); }
};

void Beam(Builder& out, int slot, const QVector3D& a, const QVector3D& b, double halfWidth, double depth)
{
	QVector3D direction=b-a;
	if(direction.lengthSquared()<1e-8f) return;
	direction.normalize();
	QVector3D side=QVector3D::crossProduct(direction,{0,0,1});
	if(side.lengthSquared()<1e-8f) side={1,0,0};
	side.normalize();
	const QVector3D up(0,0,float(depth));
	side*=float(halfWidth);
	const QVector3D a0=a-side-up,a1=a+side-up,a2=a+side,a3=a-side;
	const QVector3D b0=b-side-up,b1=b+side-up,b2=b+side,b3=b-side;
	out.Quad(slot,a0,b0,b1,a1); out.Quad(slot,a3,a2,b2,b3);
	out.Quad(slot,a0,a3,b3,b0); out.Quad(slot,a1,b1,b2,a2);
}
}

double RoofHeight(double radiusFeet, double radialDistanceFeet)
{
	if(!std::isfinite(radiusFeet) || !std::isfinite(radialDistanceFeet) || radiusFeet<=0) return 0;
	// RenderExterior 0x58a1e0, assembly 0x58c198..0x58c34e:
	// six radial spans rise sinusoidally by one quarter of the site radius.
	const double fraction=std::max(0.0,std::min(1.0,1-radialDistanceFeet/radiusFeet));
	return CrownHeight + std::sin(fraction*Pi*.5)*radiusFeet*.25;
}

Geometry::Mesh Build(const AuLegacyCity::Development& development,
	const AltitudeProjection& projection,const TerrainHeight& terrain)
{
	Builder out;
	if(development.industryId!=129 || development.demolished) return out.mesh;
	const double radius=development.biodomeRadiusMeters*FeetPerMeter();
	const int sections=AuLegacyCityCatalogue::BiodomeSegmentCount(radius);
	if(!sections) return out.mesh;
	auto point=[&](double x,double y,double height) {
		const QVector3D local(float(x),float(y),0);
		return projection ? projection(local,height) : QVector3D(float(x),float(y),float(height));
	};
	// Original RenderExterior builds the ring and eight-sided support posts
	// while work is unfinished; radial roof members appear on completion.
	for(int section=0;section<sections;++section)
	{
		const double angle=double(float(section)*3.1415927f*2/float(sections));
		const double nextAngle=double(float(section+1)*3.1415927f*2/float(sections));
		const double x=radius*std::cos(angle),y=radius*std::sin(angle);
		const double nx=radius*std::cos(nextAngle),ny=radius*std::sin(nextAngle);
		const QVector3D inside=point((radius-RingHalfWidth)*std::cos(angle),(radius-RingHalfWidth)*std::sin(angle),BeamHeight);
		const QVector3D outside=point((radius+RingHalfWidth)*std::cos(angle),(radius+RingHalfWidth)*std::sin(angle),BeamHeight);
		const QVector3D nextInside=point((radius-RingHalfWidth)*std::cos(nextAngle),(radius-RingHalfWidth)*std::sin(nextAngle),BeamHeight);
		const QVector3D nextOutside=point((radius+RingHalfWidth)*std::cos(nextAngle),(radius+RingHalfWidth)*std::sin(nextAngle),BeamHeight);
		const QVector3D insideTop=point((radius-RingHalfWidth)*std::cos(angle),(radius-RingHalfWidth)*std::sin(angle),CrownHeight);
		const QVector3D outsideTop=point((radius+RingHalfWidth)*std::cos(angle),(radius+RingHalfWidth)*std::sin(angle),CrownHeight);
		const QVector3D nextInsideTop=point((radius-RingHalfWidth)*std::cos(nextAngle),(radius-RingHalfWidth)*std::sin(nextAngle),CrownHeight);
		const QVector3D nextOutsideTop=point((radius+RingHalfWidth)*std::cos(nextAngle),(radius+RingHalfWidth)*std::sin(nextAngle),CrownHeight);
		// Source BuildingTexture14 covers top, bottom and both ring sides.
		out.Quad(14,insideTop,outsideTop,nextOutsideTop,nextInsideTop);
		out.Quad(14,inside,nextInside,nextOutside,outside);
		out.Quad(14,outside,nextOutside,nextOutsideTop,outsideTop);
		out.Quad(14,insideTop,nextInsideTop,nextInside,inside);
		const double ground=terrain ? terrain(QVector3D(float(x),float(y),0)) : 0;
		if(std::isfinite(ground) && ground<BeamHeight)
			for(int side=0;side<8;++side)
			{
				const double a=side*Pi*.25,b=(side+1)*Pi*.25;
				// RenderExterior 0x58aab9 scales both support axes by 2 meters.
				const double ax=x+ContainmentMargin*std::cos(a),ay=y+ContainmentMargin*std::sin(a);
				const double bx=x+ContainmentMargin*std::cos(b),by=y+ContainmentMargin*std::sin(b);
				out.Quad(13,point(ax,ay,ground),point(bx,by,ground),point(bx,by,BeamHeight),point(ax,ay,BeamHeight));
			}
		if(!development.constructed) continue;
		// One roof member begins at each perimeter chord midpoint. Source
		// texture7 covers its 4-meter-wide, 2-meter-deep six-span profile.
		const double startX=(x+nx)*.5,startY=(y+ny)*.5;
		for(int span=0;span<6;++span)
		{
			const double first=double(span)/6,second=double(span+1)/6;
			const double firstHeight=CrownHeight+std::sin(first*Pi*.5)*radius*.25;
			const double secondHeight=CrownHeight+std::sin(second*Pi*.5)*radius*.25;
			Beam(out,7,point(startX*(1-first),startY*(1-first),firstHeight),
				point(startX*(1-second),startY*(1-second),secondHeight),ContainmentMargin,ContainmentMargin);
		}
	}
	return std::move(out.mesh);
}
}
}
