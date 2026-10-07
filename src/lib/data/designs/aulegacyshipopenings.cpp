#include "aulegacyshippaths.h"
#include "aulegacyshipgeometry.h"
#include "aupoint.h"

#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace AuLegacyShip
{
namespace Paths
{
namespace
{
struct RoomGeometry
{
	const Room* room;
	AuPointArray boundary;
};

AuPoint Point(const QPointF& p) { return AuPoint(p.x(), p.y(), 0); }
QPointF Point(const AuPoint& p) { return QPointF(p.x, p.y); }
bool Near(const QPointF& a, const QPointF& b) { return Point(a).FuzzyEquals_2D(Point(b)); }
double Distance(const QPointF& a, const QPointF& b)
{
	return std::hypot(a.x() - b.x(), a.y() - b.y());
}
double Dot(const QPointF& a, const QPointF& b) { return a.x()*b.x() + a.y()*b.y(); }
QPointF Unit(const QPointF& p)
{
	const double length = std::hypot(p.x(), p.y());
	return length ? p / length : QPointF(1, 0);
}
bool OnSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
	return Point(p).FuzzyEquals_2D(Point(p).PointOnSegment_2D(Point(a), Point(b)));
}
void Segment(const RoomGeometry& room, const QPointF& p, QPointF& a, QPointF& b)
{
	AuPoint first, second;
	room.boundary.ClosestBoundarySegment_2D(Point(p), first, second);
	a = Point(first); b = Point(second);
}
bool Roof(const Room& room) { return room.roomType >= 22 && room.roomType <= 24; }
bool Hole(const Opening& opening) { return opening.cellType == 10 || opening.cellType == 11; }
bool Gravity(const RoomGeometry* room)
{
	return room && (room->room->roomType == 11 || room->room->roomType == 15);
}
const RoomGeometry* Find(const QVector<RoomGeometry>& rooms, quint16 id)
{
	for(const RoomGeometry& room : rooms) if(room.room->roomId == id) return &room;
	return nullptr;
}
const RoomGeometry* OtherWallRoom(const QVector<RoomGeometry>& rooms, const Detail& detail)
{
	const AuPoint p = Point(DecodePoint(detail.packedXY));
	for(const RoomGeometry& room : rooms)
		if(room.room->deck == detail.deck && room.room->roomId != detail.roomId
			&& room.boundary.PointOnBoundary_2D(p)) return &room;
	return nullptr;
}
const RoomGeometry* Below(const QVector<RoomGeometry>& rooms, const Detail& detail)
{
	const AuPoint p = Point(DecodePoint(detail.packedXY));
	for(const RoomGeometry& room : rooms)
		if(room.room->deck == detail.deck - 1 && room.boundary.PointInBoundary(p)) return &room;
	return nullptr;
}

void CommonSegment(const RoomGeometry& first, const RoomGeometry* second,
	const QPointF& p, QPointF& a, QPointF& b)
{
	Segment(first, p, a, b);
	if(!second) return;
	QPointF c, d;
	Segment(*second, p, c, d);
	const QPointF direction = Unit(b - a);
	const double length = Distance(a, b);
	const double t0 = Dot(c - a, direction), t1 = Dot(d - a, direction);
	// AnalyzeDesign projects the second wall onto the first without changing
	// its direction; stored endpoint order controls the subsequent raster walk.
	if(t0 <= t1)
	{
		if(t0 > 0) a = c;
		if(t1 < length) b = d;
	}
	else
	{
		if(t1 > 0) a = d;
		if(t0 < length) b = c;
	}
}

void HullSegment(const Design& design, const QVector<RoomGeometry>& rooms,
	const RoomGeometry& owner, const Detail& detail, QPointF& a, QPointF& b)
{
	const QPointF p = DecodePoint(detail.packedXY);
	Segment(owner, p, a, b);
	// DnRoomArray::TrimDoorSegment (180174040), including its unsigned deck
	// parameter. Adjacent vertices split the hull wall at the authored opening.
	for(const RoomGeometry& room : rooms)
		if(int(room.room->deck) == int(quint8(owner.room->deck)) && room.room->roomId != detail.roomId)
			for(const AuPoint& vertex : room.boundary)
			{
				const QPointF v = Point(vertex);
				if(Near(v, a) || Near(v, b) || !OnSegment(v, a, b)) continue;
				if(vertex.Between_2D(Point(a), Point(p))) a = v;
				else b = v;
			}
	const QPointF direction = Unit(b - a);
	// AnalyzeDesign trims 2 feet around wall turrets and ordinary hull doors.
	for(const Detail& other : design.details)
	{
		const int type = other.detailType;
		if(other.deck != detail.deck || !(type == 21 || (type >= 34 && type <= 40) || type == 50)) continue;
		const QPointF q = DecodePoint(other.packedXY);
		if(Near(a, q)) a += direction * 2.0;
		else if(Near(b, q)) b -= direction * 2.0;
		else if(OnSegment(q, a, b))
		{
			if(!Point(p).Between_2D(Point(a), Point(q))) a = q + direction * 2.0;
			else b = q - direction * 2.0;
		}
	}
	if(detail.detailType == 3)
	{
		a += direction * (11.0 / 12.0);
		b -= direction * (11.0 / 12.0);
	}
}

bool Embedded(const QPointF& endpoint, int deck, const QVector<Opening>& openings,
	const QVector<RoomGeometry>& rooms)
{
	// AuPath::IsWallEndEmbedded (1800ef6e0) only gathers wall holes, not doors.
	QVector<const Opening*> shared;
	QSet<quint16> ids;
	for(const Opening& opening : openings)
		if(Hole(opening) && opening.deck == deck
			&& (Near(endpoint, opening.first) || Near(endpoint, opening.second)))
		{
			shared.append(&opening);
			if(opening.room1) ids.insert(opening.room1);
			if(opening.room2) ids.insert(opening.room2);
		}
	if(shared.size() < 2) return true;
	for(const RoomGeometry& room : rooms)
		if(room.room->deck == deck && !Roof(*room.room) && !ids.contains(room.room->roomId)
			&& room.boundary.DistToBoundary_2D(Point(endpoint)) < 1.0) return true;
	constexpr double offset = 2.1213203433750003;
	const QPointF probes[] = {{offset, 0}, {offset, offset}, {0, offset}, {-offset, offset},
		{-offset, 0}, {-offset, -offset}, {0, -offset}, {offset, -offset}};
	for(const QPointF& delta : probes)
	{
		const QPointF probe = endpoint + delta;
		int kind = 0;
		quint16 id = 0;
		for(const RoomGeometry& room : rooms)
		{
			if(room.room->deck != deck || Roof(*room.room)) continue;
			if(room.boundary.PointInBoundary(Point(probe))) kind = 1;
			else if(room.boundary.PointOnBoundary_2D(Point(probe))) kind = 2;
			if(kind) { id = room.room->roomId; break; }
		}
		if(!kind || !ids.contains(id)) return true;
		if(kind == 2)
		{
			bool inOpening = false;
			for(const Opening* opening : shared)
				if(Point(probe).DistToSegment_2D(Point(opening->first), Point(opening->second)) < 0.1)
					{ inOpening = true; break; }
			if(!inOpening) return true;
		}
	}
	return false;
}
}

bool ExtractOpenings(const Design& design, QVector<Opening>& openings, QString& error)
{
	QVector<RoomGeometry> rooms;
	for(const Room& room : design.rooms)
	{
		QString problem;
		const QPolygonF polygon = RoomBoundary(room, &problem);
		if(!problem.isEmpty()) { error = problem; return false; }
		if(polygon.isEmpty()) continue;
		RoomGeometry geometry; geometry.room = &room;
		for(const QPointF& point : polygon) geometry.boundary.append(Point(point));
		rooms.append(geometry);
	}
	QVector<Opening> doors, holes, railings;
	for(const Detail& detail : design.details)
	{
		const int type = detail.detailType;
		const bool wall = type == 3 || type == 5 || type == 9 || (type >= 49 && type <= 50)
			|| (type >= 53 && type <= 56);
		const bool hatch = type == 8 || type == 51 || type == 52 || (type >= 77 && type <= 97);
		if(!wall && !hatch) continue;
		const RoomGeometry* owner = Find(rooms, detail.roomId);
		if(!owner) continue;
		Opening opening;
		opening.deck = detail.deck; opening.room1 = detail.roomId;
		opening.center = DecodePoint(detail.packedXY);
		opening.first = opening.second = opening.center;
		opening.large = type == 3 || type == 9 || (type >= 53 && type <= 56);
		opening.cellType = type == 3 || type == 50 ? 4 : type == 9 ? 11 : type == 55 ? 10 : type == 56 ? 18 : 3;
		const bool crew = type == 49 || type == 51 || type == 54 || (type >= 84 && type <= 90);
		const bool automatic = type == 5 || type == 8 || type == 49 || type == 51 || (type >= 77 && type <= 90);
		if(!Hole(opening) && type != 56) opening.flags = quint16(0x20 | (crew ? 2 : 0) | (automatic ? 1 : 0));
		if(hatch)
		{
			opening.flags |= 8;
			const RoomGeometry* below = Below(rooms, detail);
			if(type == 52 || type >= 91)
				opening.cellType = below && !Roof(*below->room) && below->room->roomType != 9 ? 7 : 6;
			else
			{
				opening.cellType = 5;
				opening.room2 = below ? below->room->roomId : 0;
			}
		}
		else if(type == 3 || type == 9)
			HullSegment(design, rooms, *owner, detail, opening.first, opening.second);
		else
		{
			const RoomGeometry* other = type == 50 ? nullptr : OtherWallRoom(rooms, detail);
			opening.room2 = other ? other->room->roomId : 0;
			if(opening.large)
			{
				CommonSegment(*owner, other, opening.center, opening.first, opening.second);
				if(type == 53 || type == 54)
				{
					const QPointF inset = Unit(opening.second - opening.first) * (11.0 / 12.0);
					opening.first += inset; opening.second -= inset;
				}
			}
			else
			{
				QPointF a, b; Segment(*owner, opening.center, a, b);
				const QPointF offset = Unit(b - a) * 2.0;
				opening.first = opening.center + offset; opening.second = opening.center - offset;
			}
		}
		if(opening.large)
		{
			const RoomGeometry* other = Find(rooms, opening.room2);
			if(Gravity(owner) && (!opening.room2 || Gravity(other)))
				opening.flags |= owner->room->roomType == 11 && (!other || other->room->roomType == 11) ? 4 : 16;
		}
		else if(wall && std::abs(opening.first.y() - opening.second.y()) > 1e-5)
		{
			const double dx = opening.second.x() - opening.first.x(), dy = opening.second.y() - opening.first.y();
			opening.flags |= std::abs(dx) <= 1e-5 ? 0x80 : dx * dy > 0 ? 0x40 : 0xc0;
		}
		QVector<Opening>& list = Hole(opening) ? holes : type == 56 ? railings : doors;
		bool duplicate = false;
		if(opening.large)
			for(const Opening& existing : list)
				if(existing.deck == opening.deck && ((Near(existing.first, opening.first) && Near(existing.second, opening.second))
					|| (Near(existing.first, opening.second) && Near(existing.second, opening.first))))
					{ duplicate = true; break; }
		if(!duplicate) list.append(opening);
	}
	QVector<Opening> result = doors;
	result += holes; result += railings;
	for(Opening& opening : result)
		if(opening.large)
		{
			opening.firstEmbedded = Embedded(opening.first, opening.deck, holes, rooms);
			opening.secondEmbedded = Embedded(opening.second, opening.deck, holes, rooms);
		}
	if(!AppendVerticalOpenings(design, result, error)) return false;
	openings = std::move(result);
	error.clear();
	return true;
}
}
}
