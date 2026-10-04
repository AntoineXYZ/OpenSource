#include "aulegacyshipdetails.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace AuLegacyShip
{
namespace
{
constexpr double Fuzz = 1e-5;
constexpr double Pi = 3.14159265358979323846;
struct Placement { int room; bool bridge; };
// Original GD 0x18/0x3a; see detail-types.json and DnDetailType::Rx.
constexpr Placement Placements[] = {
	{0, false}, // 0: None
	{2, true}, // 1: Navigator Station
	{-1, false}, // 2: Double Berth
	{-2, true}, // 3: Large Hull Door
	{18, false}, // 4: Medic Station
	{-3, true}, // 5: Door
	{8, true}, // 6: Engineer Station
	{21, true}, // 7: Fire Control Station
	{-4, true}, // 8: Hatch
	{-2, false}, // 9: Hull Opening
	{8, false}, // 10: Maneuver Drive
	{18, false}, // 11: Surgery Unit
	{2, true}, // 12: Helm
	{8, false}, // 13: Power Relay Station
	{8, false}, // 14: Power Plant
	{16, true}, // 15: Sensor Station
	{16, false}, // 16: Sensors
	{17, true}, // 17: Shield Station
	{17, false}, // 18: Shields
	{20, false}, // 19: Transporter Station
	{20, false}, // 20: Transporter Pad
	{-2, true}, // 21: Turret (Side)
	{-5, true}, // 22: Turret (Top/Bottom)
	{21, false}, // 23: Weapon System
	{8, false}, // 24: FTL Drive
	{-1, false}, // 25: Double Berth
	{-1, false}, // 26: Double Berth
	{-1, false}, // 27: Double Berth
	{-1, false}, // 28: Double Berth
	{-1, false}, // 29: Double Berth
	{-1, false}, // 30: Double Berth
	{-1, false}, // 31: Double Berth
	{8, false}, // 32: Life Support
	{-5, true}, // 33: Turret (Top/Bottom)
	{-2, true}, // 34: Turret (Side)
	{-2, true}, // 35: Turret (Side)
	{-2, true}, // 36: Turret (Side)
	{-2, true}, // 37: Turret (Side)
	{-2, true}, // 38: Turret (Side)
	{-2, true}, // 39: Turret (Side)
	{-2, true}, // 40: Turret (Side)
	{13, false}, // 41: Space Fighter Location
	{13, false}, // 42: Space Fighter Location
	{13, false}, // 43: Space Fighter Location
	{13, false}, // 44: Space Fighter Location
	{13, false}, // 45: Space Fighter Location
	{13, false}, // 46: Space Fighter Location
	{13, false}, // 47: Space Fighter Location
	{13, false}, // 48: Space Fighter Location
	{-3, true}, // 49: Crew Door
	{-2, true}, // 50: Hull Door
	{-4, true}, // 51: Crew Hatch
	{-5, true}, // 52: Hull Hatch
	{-3, true}, // 53: Large Door
	{-3, true}, // 54: Large Crew Door
	{-3, true}, // 55: Wall Opening
	{-3, true}, // 56: Railing
	{13, false}, // 57: Vehicle Location
	{13, false}, // 58: Vehicle Location
	{13, false}, // 59: Vehicle Location
	{13, false}, // 60: Vehicle Location
	{13, false}, // 61: Vehicle Location
	{13, false}, // 62: Vehicle Location
	{13, false}, // 63: Vehicle Location
	{13, false}, // 64: Vehicle Location
	{-1, false}, // 65: Single Berth
	{-1, false}, // 66: Single Berth
	{-1, false}, // 67: Single Berth
	{-1, false}, // 68: Single Berth
	{-1, false}, // 69: Single Berth
	{-1, false}, // 70: Single Berth
	{-1, false}, // 71: Single Berth
	{-1, false}, // 72: Single Berth
	{2, true}, // 73: Captain's Chair
	{14, false}, // 74: Game Table
	{14, false}, // 75: Game Table
	{-6, true}, // 76: Pivot Point
	{-4, true}, // 77: Hatch
	{-4, true}, // 78: Hatch
	{-4, true}, // 79: Hatch
	{-4, true}, // 80: Hatch
	{-4, true}, // 81: Hatch
	{-4, true}, // 82: Hatch
	{-4, true}, // 83: Hatch
	{-4, true}, // 84: Crew Hatch
	{-4, true}, // 85: Crew Hatch
	{-4, true}, // 86: Crew Hatch
	{-4, true}, // 87: Crew Hatch
	{-4, true}, // 88: Crew Hatch
	{-4, true}, // 89: Crew Hatch
	{-4, true}, // 90: Crew Hatch
	{-5, true}, // 91: Hull Hatch
	{-5, true}, // 92: Hull Hatch
	{-5, true}, // 93: Hull Hatch
	{-5, true}, // 94: Hull Hatch
	{-5, true}, // 95: Hull Hatch
	{-5, true}, // 96: Hull Hatch
	{-5, true}, // 97: Hull Hatch
	{-7, true}, // 98: Contrail Emitter
	{21, false}, // 99: Weapon Emitter
	{-8, false}, // 100: Flag Pole
	{-9, true}, // 101: Stairs, Straight
	{-9, true}, // 102: Stairs, Straight
	{-9, true}, // 103: Stairs, Straight
	{-9, true}, // 104: Stairs, Straight
	{-9, true}, // 105: Stairs, Straight
	{-9, true}, // 106: Stairs, Straight
	{-9, true}, // 107: Stairs, Straight
	{-9, true}, // 108: Stairs, Straight
	{-9, true}, // 109: Stairs, Turn Left
	{-9, true}, // 110: Stairs, Turn Left
	{-9, true}, // 111: Stairs, Turn Left
	{-9, true}, // 112: Stairs, Turn Left
	{-9, true}, // 113: Stairs, Turn Left
	{-9, true}, // 114: Stairs, Turn Left
	{-9, true}, // 115: Stairs, Turn Left
	{-9, true}, // 116: Stairs, Turn Left
	{-9, true}, // 117: Stairs, Turn Right
	{-9, true}, // 118: Stairs, Turn Right
	{-9, true}, // 119: Stairs, Turn Right
	{-9, true}, // 120: Stairs, Turn Right
	{-9, true}, // 121: Stairs, Turn Right
	{-9, true}, // 122: Stairs, Turn Right
	{-9, true}, // 123: Stairs, Turn Right
	{-9, true}, // 124: Stairs, Turn Right
	{-9, true}, // 125: Stairs, U-turn Left
	{-9, true}, // 126: Stairs, U-turn Left
	{-9, true}, // 127: Stairs, U-turn Left
	{-9, true}, // 128: Stairs, U-turn Left
	{-9, true}, // 129: Stairs, U-turn Left
	{-9, true}, // 130: Stairs, U-turn Left
	{-9, true}, // 131: Stairs, U-turn Left
	{-9, true}, // 132: Stairs, U-turn Left
	{-9, true}, // 133: Stairs, U-turn Right
	{-9, true}, // 134: Stairs, U-turn Right
	{-9, true}, // 135: Stairs, U-turn Right
	{-9, true}, // 136: Stairs, U-turn Right
	{-9, true}, // 137: Stairs, U-turn Right
	{-9, true}, // 138: Stairs, U-turn Right
	{-9, true}, // 139: Stairs, U-turn Right
	{-9, true}, // 140: Stairs, U-turn Right
	{-10, true}, // 141: Troop Station
	{-8, false}, // 142: Light Pole
};

double Dot(const QPointF& a, const QPointF& b) { return a.x()*b.x() + a.y()*b.y(); }
bool Near(const QPointF& a, const QPointF& b)
{
	return std::abs(a.x()-b.x()) <= Fuzz && std::abs(a.y()-b.y()) <= Fuzz;
}
QPointF Rotate(const QPointF& p, double angle)
{
	const double c = std::cos(angle), s = std::sin(angle);
	return QPointF(p.x()*c - p.y()*s, p.y()*c + p.x()*s);
}
QPointF Closest(const QPointF& p, const QPointF& a, const QPointF& b)
{
	const QPointF v = b-a;
	const double len = std::sqrt(Dot(v,v));
	if(len == 0) return a;
	const double along = Dot(p-a,v)/len;
	if(along <= 0) return a;
	if(along >= len) return b;
	return QPointF(v.x()*along/len+a.x(), v.y()*along/len+a.y());
}
double Distance(const QPointF& a, const QPointF& b)
{
	const QPointF v=a-b;
	return std::sqrt(Dot(v,v));
}
bool Between(const QPointF& p, const QPointF& a, const QPointF& b)
{
	if(Near(p,a) || Near(p,b)) return true;
	const QPointF v=b-a;
	const double len=std::sqrt(Dot(v,v));
	if(len==0) return false;
	const double along=Dot(p-a,v/len);
	return along>=0 && along<=len;
}
bool On(const QPolygonF& polygon, const QPointF& p)
{
	for(int i=1;i<polygon.size();++i)
		if(Distance(p,Closest(p,polygon[i-1],polygon[i]))<=Fuzz) return true;
	return polygon.size()>2 &&
		Distance(p,Closest(p,polygon.last(),polygon.first()))<=Fuzz;
}
// AuMath13.dll 0x18000ce40: reproduce the original rotated-ray test and
// its fuzzy vertex retries instead of Qt's differently defined edge handling.
bool Inside(const QPolygonF& polygon, const QPointF& p)
{
	if(polygon.size()<3) return false;
	const QRectF bounds=polygon.boundingRect();
	if(p.x()<=bounds.left() || p.x()>=bounds.right() ||
		p.y()<=bounds.top() || p.y()>=bounds.bottom()) return false;
	QPointF ray=Rotate(QPointF(bounds.width(),bounds.height()),Pi/180);
	int tries=0;
	for(;;)
	{
		const QPointF end=p+ray;
		QPointF previous=polygon.last();
		int crossings=0;
		bool retry=false;
		for(const QPointF& current:polygon)
		{
			if(current.x()==previous.x()&&current.y()==previous.y()) continue;
			const QPointF d0=end-p, d1=previous-current, d2=current-p;
			const double d00=Dot(d0,d0), d01=Dot(d0,d1), d11=Dot(d1,d1);
			const double fa=d01*d01, fb=d00*d11, det=fa-fb;
			if(std::abs(det)<=Fuzz*(std::abs(fa)+std::abs(fb)))
				retry=Distance(current,Closest(current,p,end))<=Fuzz;
			else
			{
				const double r0=Dot(d2,d0), r1=Dot(d2,d1);
				const QPointF a=p+d0*((r1*d01-r0*d11)/det);
				const QPointF b=current+d1*((d00*r1-d01*r0)/det);
				if(Near(a,b) && Between(a,p,end) && Between(a,current,previous))
				{
					if(Near(a,p)) return false;
					if(Near(a,current)||Near(a,previous))
					{
						if(++tries==360) return false;
						retry=true;
					}
					++crossings;
				}
			}
			if(retry) break;
			previous=current;
		}
		if(!retry) return (crossings&1)!=0;
		ray=Rotate(ray,Pi/180);
	}
}
int RotationStep(int t)
{
	if(t>=0x19 && t<=0x1f) return t-0x18;
	if(t>=0x22 && t<=0x28) return t-0x21;
	if(t>=0x39 && t<=0x40) return t-0x39;
	if(t>=0x41 && t<=0x48) return t-0x41;
	if(t==0x4b) return 1;
	if(t>=0x4d && t<=0x53) return t-0x4c;
	if(t>=0x54 && t<=0x5a) return t-0x53;
	if(t>=0x5b && t<=0x61) return t-0x5a;
	if(t>=0x65 && t<=0x8c) return (t-0x65)%8;
	return 0;
}
double Rotation(int t) { return double(float(RotationStep(t)*Pi/4)); }
bool IsBerth(int t) { return t==2 || (t>=0x19&&t<=0x1f) || (t>=0x41&&t<=0x48); }
bool IsTurret(int t) { return t==0x15 || t==0x16 || (t>=0x21&&t<=0x28); }
bool IsStairs(int t) { return t>=0x65&&t<=0x8c; }
bool LimitOne(int t)
{
	switch(t)
	{
	case 1: case 4: case 6: case 7: case 0xb: case 0xc: case 0xd:
	case 0xf: case 0x11: case 0x13: return true;
	default: return false;
	}
}
int VehicleCommodity(const Detail& d)
{
	if(d.detailType>=0x39 && d.detailType<=0x40) return d.commodityIdNormalized;
	return 0;
}
// Filled from original model-resource bounds, never modern replacement assets.
bool VehicleHalfSize(int commodity, QPointF& result, QString& error)
{
	QPointF half;
	switch(commodity)
	{
	case 0: case 54: case 70: case 71: case 103:
		// Empty/missing original resources cause CreateGeometry to return null.
		half=QPointF(0,0); break;
	case 6: half=QPointF(8.319742202758789,11.813350200653076); break;
	case 7: half=QPointF(10.208333015441895,19.750000715255737); break;
	case 8: half=QPointF(14.044610023498535,14.789060592651367); break;
	case 9: half=QPointF(18.64858913421631,13.657800674438477); break;
	case 28: half=QPointF(4.406362771987915,7.853582143783569); break;
	case 77: half=QPointF(7.353567838668823,10.512041091918945); break;
	case 82: half=QPointF(8.201797008514404,9.152505874633789); break;
	case 104: half=QPointF(6.217533111572266,8.501940727233887); break;
	case 132: half=QPointF(5,16.090107917785645); break;
	case 133: half=QPointF(1.49844628572464,4.320924758911133); break;
	case 231: half=QPointF(12.763139724731445,13.934858798980713); break;
	default:
		error=QStringLiteral("The original vehicle model bounds for commodity %1 are not available; the edit was not applied.").arg(commodity);
		return false;
	}
	// Both original fit and spacing routines narrow model half-extents to float.
	result=QPointF(double(float(half.x())),double(float(half.y())));
	return true;
}
bool Spacing(const Detail& d, QPointF& result, QString& error)
{
	const int t=d.detailType;
	if(t>=0x39&&t<=0x40)
	{
		QPointF half;
		if(!VehicleHalfSize(d.commodityIdNormalized,half,error)) return false;
		result=QPointF();
		const double angle=(t-0x39)*Pi/4;
		for(int x:{-1,1}) for(int y:{-1,1})
		{
			const QPointF p=Rotate(QPointF(x*half.y(),y*half.x()),angle);
			result.setX(std::max(result.x(),p.x()));
			result.setY(std::max(result.y(),p.y()));
		}
		return true;
	}
	double value=0;
	switch(t)
	{
	case 1: case 2: case 4: case 6: case 7: case 0xb: case 0xc: case 0xd:
	case 0xf: case 0x11: case 0x13: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x41: case 0x42:
	case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48:
	case 0x49: value=2.7450937911666666; break;
	case 10: case 0xe: case 0x10: case 0x12: case 0x14: case 0x15:
	case 0x16: case 0x17: case 0x18: case 0x20: case 0x21: case 0x22:
	case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28:
	case 100: case 0x8d: case 0x8e: value=1; break;
	case 0x4a: value=5; break;
	case 0x4b: value=7.071; break;
	default: if(IsStairs(t)) value=1; break;
	}
	result=QPointF(value,value);
	return true;
}
struct Geometry
{
	const Room* room;
	QPolygonF polygon;
};
// DnDetail::IsValid constructs seven double-precision test points. This is
// deliberately distinct from StairBoundary's float-sized collision polygon.
QVector<QPointF> StairChecks(const Detail& d)
{
	const int family=(d.detailType-0x65)/8;
	const bool diagonal=((d.detailType-0x65)&1)!=0;
	QVector<QPointF> p(7);
	double a,b;
	switch(family)
	{
	case 0:
		a=diagonal?12.0:GridSpacing*2; b=diagonal?4.0:GridSpacing;
		p={QPointF(0,b),QPointF(0,0),QPointF(a,b),QPointF(a,0),
			QPointF(a*.5,b),QPointF(a*.5,0),QPointF(a*.5,b*.5)};
		break;
	case 1:
		a=diagonal?8.0:GridSpacing; b=diagonal?4.0:GridSpacing;
		p={QPointF(0,b),QPointF(0,0),QPointF(a,a+b),QPointF(a+b,a+b),
			QPointF(a,b),QPointF(a+b,0),QPointF((2*a+b)*.5,b*.5)};
		break;
	case 2:
		a=diagonal?8.0:GridSpacing; b=diagonal?4.0:GridSpacing;
		p={QPointF(0,0),QPointF(0,-b),QPointF(a+b,-a-b),QPointF(a,-a-b),
			QPointF(a+b,0),QPointF(a,-b),QPointF((2*a+b)*.5,-b*.5)};
		break;
	case 3:
		a=diagonal?12.0:GridSpacing*2; b=diagonal?6.0:GridSpacing;
		p={QPointF(0,a),QPointF(0,0),QPointF(a,a),QPointF(a,0),
			QPointF(b,a),QPointF(b,0),QPointF(a*.5,a*.5)};
		break;
	default:
		a=diagonal?12.0:GridSpacing*2; b=diagonal?6.0:GridSpacing;
		p={QPointF(0,0),QPointF(0,-a),QPointF(a,0),QPointF(a,-a),
			QPointF(b,0),QPointF(b,-a),QPointF(a*.5,-a*.5)};
		break;
	}
	const QPointF origin=DecodePoint(d.packedXY);
	for(QPointF& v:p) v=origin+Rotate(v,Rotation(d.detailType));
	return p;
}
QPolygonF StairBoundary(const Detail& d)
{
	const int family=(d.detailType-0x65)/8;
	const bool diagonal=((d.detailType-0x65)&1)!=0;
	const float a=diagonal?(family==1||family==2?8.0f:12.0f):
		(family==1||family==2?float(GridSpacing):float(GridSpacing*2));
	const float b=diagonal?4.0f:float(GridSpacing);
	QVector<QPointF> p;
	if(family==0) p={QPointF(0,0),QPointF(0,b),QPointF(a,b),QPointF(a,0)};
	else if(family==1) p={QPointF(0,0),QPointF(0,b),QPointF(a,b),
		QPointF(a,float(a+b)),QPointF(float(a+b),float(a+b)),QPointF(float(a+b),0)};
	else if(family==2) p={QPointF(0,0),QPointF(float(a+b),0),
		QPointF(float(a+b),-float(a+b)),QPointF(a,-float(a+b)),
		QPointF(a,-b),QPointF(0,-b)};
	else if(family==3) p={QPointF(0,0),QPointF(0,a),QPointF(a,a),QPointF(a,0)};
	else p={QPointF(0,0),QPointF(a,0),QPointF(a,-a),QPointF(0,-a)};
	const QPointF origin=DecodePoint(d.packedXY);
	for(QPointF& v:p) v=origin+Rotate(v,Rotation(d.detailType));
	return p;
}
bool ValidStairs(const Detail& d, const QVector<Geometry>& rooms)
{
	const QVector<QPointF> points=StairChecks(d);
	QVector<const Geometry*> possible;
	for(const Geometry& r:rooms)
	{
		if(r.room->deck!=d.deck) continue;
		const bool forbidden=r.room->roomType==9 ||
			(r.room->roomType>=0x16&&r.room->roomType<=0x18);
		if(On(r.polygon,points[0]))
		{
			if(!forbidden) possible.append(&r);
		}
		else if(Inside(r.polygon,points[0]))
		{
			if(forbidden) return false;
			possible.append(&r);
		}
	}
	if(possible.isEmpty()) return false;
	for(int i=1;i<points.size();++i)
	{
		bool found=false;
		for(const Geometry* r:possible)
			if(Inside(r->polygon,points[i])||On(r->polygon,points[i])) {found=true;break;}
		if(!found) return false;
	}
	return true;
}
bool OnVertex(const QPolygonF& polygon, const QPointF& p)
{
	// AuPointArray::DoesContain uses component-wise fuzzy equality.
	for(const QPointF& v:polygon) if(Near(v,p)) return true;
	return false;
}
int SideTurretType(const QPolygonF& polygon, const QPointF& p)
{
	QPointF a,b;
	double closest=std::numeric_limits<double>::max();
	for(int i=0;i<polygon.size();++i)
	{
		const QPointF current=polygon[i], next=polygon[(i+1)%polygon.size()];
		const double distance=Distance(p,Closest(p,current,next));
		if(distance<closest) {closest=distance;a=current;b=next;}
	}
	QPointF tangent=b-a;
	const double length=std::sqrt(Dot(tangent,tangent));
	if(length==0) tangent.setX(1);
	else tangent/=length;
	QPointF normal(-tangent.y(),tangent.x());
	if(Inside(polygon,p+normal)) normal=-normal;
	double angle;
	if(std::abs(normal.y())<=Fuzz) angle=normal.x()<-Fuzz?Pi:0;
	else if(std::abs(normal.x())<=Fuzz) angle=normal.y()>=-Fuzz?Pi/2:-Pi/2;
	else angle=std::atan2(normal.y(),normal.x());
	if(angle<0) angle+=2*Pi;
	if(angle<0.39269908169872414) return 0x15;
	if(angle<1.1780972450961724) return 0x22;
	if(angle<1.9634954084936207) return 0x23;
	if(angle<2.748893571891069) return 0x24;
	if(angle<3.5342917352885173) return 0x25;
	if(angle<4.319689898685966) return 0x26;
	if(angle<5.105088062083414) return 0x27;
	if(angle<5.8904862254808625) return 0x28;
	return 0x15;
}
enum class DetailResult { Valid, Invalid, Unavailable };
DetailResult TestDetail(Detail& d, const QVector<Geometry>& rooms,
	const QVector<Detail>& details, QString& error)
{
	const int t=d.detailType;
	const QPointF point=DecodePoint(d.packedXY);
	const int gx=qint8(d.packedXY>>8), gy=qint8(d.packedXY&255);
	if(std::abs(int(d.deck))>42 || std::abs(gx)>120 || std::abs(gy)>120 ||
		t>=int(sizeof(Placements)/sizeof(Placements[0]))) return DetailResult::Invalid;
	const Placement placement=Placements[t];
	if(placement.room==-8 && d.deck!=0) return DetailResult::Invalid;
	if(placement.room==-9) return ValidStairs(d,rooms)?DetailResult::Valid:DetailResult::Invalid;
	if(placement.room==-6)
	{
		bool any=false;
		double minX=0,minY=0,maxX=0,maxY=0;
		int low=0,high=0;
		for(const Geometry& r:rooms) if(!r.polygon.isEmpty())
		{
			const QRectF b=r.polygon.boundingRect();
			if(!any) {minX=b.left();minY=b.top();maxX=b.right();maxY=b.bottom();low=high=r.room->deck;any=true;}
			else {minX=std::min(minX,b.left());minY=std::min(minY,b.top());maxX=std::max(maxX,b.right());maxY=std::max(maxY,b.bottom());low=std::min(low,int(r.room->deck));high=std::max(high,int(r.room->deck));}
		}
		return any && point.x()>=minX&&point.x()<=maxX&&point.y()>=minY&&point.y()<=maxY&&
			d.deck>=low&&d.deck<=high+1?DetailResult::Valid:DetailResult::Invalid;
	}
	int count=0;
	for(const Geometry& r:rooms)
	{
		if(r.room->deck!=d.deck) continue;
		const bool inside=Inside(r.polygon,point), on=On(r.polygon,point);
		if(!inside&&!on) continue;
		const int roomType=r.room->roomType;
		if(roomType==2&&!placement.bridge) return DetailResult::Invalid;
		switch(placement.room)
		{
		case -1:
			if(count!=0 || !(roomType==1||roomType==4||roomType==7||roomType==0x13)||on)
				return DetailResult::Invalid;
			if(IsBerth(t)&&!Inside(r.polygon,point+Rotate(QPointF(CoordinateSpacing,0),Rotation(t))))
				return DetailResult::Invalid;
			break;
		case -2:
			if((roomType==9&&t!=0x62)||count!=0||!on||OnVertex(r.polygon,point)) return DetailResult::Invalid;
			if(IsTurret(t)) d.detailType=quint8(SideTurretType(r.polygon,point));
			break;
		case -3:
			if(roomType==9||count>1||!on||OnVertex(r.polygon,point)) return DetailResult::Invalid;
			break;
		case -4: case -10:
			if(roomType==9||count!=0||on) return DetailResult::Invalid;
			break;
		case -5:
			if(roomType==9||count!=0||on) return DetailResult::Invalid;
			if(IsTurret(t)) d.detailType=0x21;
			break;
		case -7:
			for(const Detail& other:details)
				if(other.detailId!=d.detailId && other.detailType==t&&other.deck==d.deck&&
					Near(DecodePoint(other.packedXY),point)) return DetailResult::Invalid;
			break;
		case -8: return DetailResult::Invalid;
		default:
			if(count!=0) return DetailResult::Invalid;
			if(t==0x4a||t==0x4b)
			{
				if(roomType!=placement.room&&roomType!=10) return DetailResult::Invalid;
			}
			else if(roomType!=placement.room && !(roomType==2&&placement.bridge)) return DetailResult::Invalid;
			if(on && !(t==10||t==14||t==16||t==18||t==23||t==0x63)) return DetailResult::Invalid;
			if(LimitOne(t))
			{
				for(const Detail& other:details)
					if(other.detailId!=d.detailId&&other.detailType==t&&other.deck==d.deck&&
						other.roomId==r.room->roomId) return DetailResult::Invalid;
			}
			else if(t==0x63)
			{
				for(const Detail& other:details) if(other.detailId!=d.detailId&&other.detailType==t)
				{
					int subtype=0;
					for(const Geometry& rr:rooms) if(rr.room->roomId==other.roomId)
						{subtype=rr.room->roomSubtype;break;}
					if(subtype==r.room->roomSubtype) return DetailResult::Invalid;
				}
			}
			if(VehicleCommodity(d)!=0)
			{
				QPointF half;
				if(!VehicleHalfSize(VehicleCommodity(d),half,error)) return DetailResult::Unavailable;
				for(int x:{-1,1}) for(int y:{-1,1})
					if(!Inside(r.polygon,point+Rotate(QPointF(x*half.y(),y*half.x()),Rotation(t))))
						return DetailResult::Invalid;
			}
			else if(t==0x4a||t==0x4b)
				for(int x:{-1,1}) for(int y:{-1,1})
					if(!Inside(r.polygon,point+Rotate(QPointF(x*5,y*5),t==0x4b?Pi/4:0)))
						return DetailResult::Invalid;
			break;
		}
		QPointF spacing;
		Detail original=d;
		original.detailType=quint8(t);
		if(!Spacing(original,spacing,error)) return DetailResult::Unavailable;
		if(spacing.x()!=0&&spacing.y()!=0)
			for(const Detail& other:details)
			{
				if(other.detailId==d.detailId||other.deck!=d.deck||other.roomId!=r.room->roomId) continue;
				if((t==100||t==0x8e)&&other.detailType==t) continue;
				if(IsStairs(other.detailType))
				{
					const QPolygonF stairs=StairBoundary(other);
					// Confirmed original bug: tests the stair origin, not point.
					const QPointF origin=DecodePoint(other.packedXY);
					if(Inside(stairs,origin)||On(stairs,origin)) return DetailResult::Invalid;
				}
				else
				{
					QPointF theirs;
					if(!Spacing(other,theirs,error)) return DetailResult::Unavailable;
					if(theirs.x()!=0||theirs.y()!=0)
					{
						const QPointF delta=DecodePoint(other.packedXY)-point;
						if(std::abs(delta.x())<spacing.x()+theirs.x()&&
							std::abs(delta.y())<spacing.y()+theirs.y()) return DetailResult::Invalid;
					}
				}
			}
		d.roomId=r.room->roomId;
		++count;
	}
	if(placement.room==-5||placement.room==-4)
	{
		int below=0;
		for(const Geometry& r:rooms) if(r.room->deck==d.deck-1)
		{
			if(On(r.polygon,point)) return DetailResult::Invalid;
			if(!Inside(r.polygon,point)) continue;
			if(r.room->roomType==9||below!=0) return DetailResult::Invalid;
			if(count==0)
			{
				if(IsTurret(t)) d.detailType=0x16;
				d.roomId=r.room->roomId;
			}
			below=1;
			++count;
		}
		if(count==0) return DetailResult::Invalid;
	}
	else if(count==0) return DetailResult::Invalid;
	if((placement.room==-4||placement.room==-3)&&count!=2) return DetailResult::Invalid;
	if((placement.room==-5||placement.room==-2)&&count!=1) return DetailResult::Invalid;
	return DetailResult::Valid;
}
}

bool ReconcileDetails(Design& candidate, QString& error)
{
	error.clear();
	QVector<Geometry> rooms;
	rooms.reserve(candidate.rooms.size());
	for(const Room& r:candidate.rooms)
	{
		QString geometryError;
		const QPolygonF polygon=RoomBoundary(r,&geometryError);
		if(!geometryError.isEmpty()) {error=geometryError;return false;}
		// The 2016 room-array reader discards empty room records. Retained
		// source records must not participate even in room-ID/subtype lookups.
		if(polygon.isEmpty()) continue;
		rooms.append({&r,polygon});
	}
	QVector<Detail> details=candidate.details;
	for(int i=0;i<details.size();)
	{
		Detail corrected=details[i];
		switch(TestDetail(corrected,rooms,details,error))
		{
		case DetailResult::Unavailable: return false;
		case DetailResult::Invalid: details.removeAt(i); break;
		case DetailResult::Valid: details[i]=corrected; ++i; break;
		}
	}
	candidate.details=std::move(details);
	return true;
}
}
