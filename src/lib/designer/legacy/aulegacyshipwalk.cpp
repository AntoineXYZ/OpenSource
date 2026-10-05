#include "aulegacyshipwalk.h"
#include "aulegacyshipgeometry.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipdetails.h"

#include <QHash>
#include <QMatrix4x4>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace AuLegacyShip
{
namespace Rendering
{
namespace
{
constexpr float CellSize = 8;
constexpr float Skin = .002f;
constexpr float ContactTolerance = .015f;
constexpr float WalkableNormal = .55f;

struct Ladder
{
	QVector3D anchor;
	int rotation = 0;
	int deck = 0;
};

bool Finite(const QVector3D& p)
{
	return std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z());
}

struct Box
{
	QVector3D minimum, maximum;
	Box() {}
	Box(const QVector3D& a, const QVector3D& b) : minimum(a), maximum(b) {}
	void Include(const QVector3D& point)
	{
		for(int axis = 0; axis < 3; ++axis)
		{
			minimum[axis] = std::min(minimum[axis], point[axis]);
			maximum[axis] = std::max(maximum[axis], point[axis]);
		}
	}
	bool Intersects(const Box& other) const
	{
		for(int axis = 0; axis < 3; ++axis)
			if(maximum[axis] < other.minimum[axis] || minimum[axis] > other.maximum[axis]) return false;
		return true;
	}
};

struct Triangle
{
	QVector3D a, b, c, normal;
	Box bounds;
	quint32 doorId;
	quint32 consoleId = 0;
	int obstacle;
	bool blocksMovement;
	Triangle() : doorId(0), obstacle(-1), blocksMovement(true) {}
	Triangle(const QVector3D& p, const QVector3D& q, const QVector3D& r, quint32 door, int cell = -1)
		: a(p), b(q), c(r), normal(QVector3D::crossProduct(q - p, r - p).normalized()),
		bounds(p, p), doorId(door), obstacle(cell), blocksMovement(true)
	{ bounds.Include(q); bounds.Include(r); }
};

struct RoomVolume
{
	QPolygonF boundary;
	QRectF bounds;
	float bottom, top;
	int type;
};

struct Contact
{
	bool ground = false;
	bool ceiling = false;
	bool wall = false;
	QVector<quint32> doors;
	QVector<quint32> supportDoors;
	QVector<QVector3D> normals;
	void Add(const QVector3D& normal)
	{
		normals.append(normal);
		ground = ground || normal.z() >= WalkableNormal;
		ceiling = ceiling || normal.z() <= -WalkableNormal;
		wall = wall || std::abs(normal.z()) < WalkableNormal;
	}
};

int Cell(float value) { return int(std::floor(double(value) / CellSize)); }

quint64 CellKey(int x, int y, int z)
{
	return quint64(quint32(x) & 0x1fffffU) | (quint64(quint32(y) & 0x1fffffU) << 21)
		| (quint64(quint32(z) & 0x1fffffU) << 42);
}

bool BlocksPathCell(int detailType)
{
	// AuData13!AuPath::PaintObstaclesAndTurrets (1800f25c0): only
	// cell type 0x0f removes pathability. Type 0x0e equipment stays passable.
	switch(detailType)
	{
	case 10: case 14: case 16: case 18: case 23: case 24: case 32:
	case 74: case 75: case 100: case 142:
		return true;
	default:
		return false;
	}
}

bool StructuralBatch(const Batch& batch)
{
	// Imported models, berths and other display equipment are not a collision
	// source. Actual obstacles are authored path cells added separately below.
	return !batch.equipment || batch.doorId != 0 || batch.material == Material::StairCasing
		|| batch.material == Material::StairRiser || batch.material == Material::StairTread;
}

bool InsideTriangle(const QVector3D& p, const Triangle& t)
{
	const float e = -1e-5f;
	return QVector3D::dotProduct(QVector3D::crossProduct(t.b - t.a, p - t.a), t.normal) >= e
		&& QVector3D::dotProduct(QVector3D::crossProduct(t.c - t.b, p - t.b), t.normal) >= e
		&& QVector3D::dotProduct(QVector3D::crossProduct(t.a - t.c, p - t.c), t.normal) >= e;
}

QVector3D ClosestPoint(const QVector3D& p, const Triangle& t)
{
	const QVector3D ab = t.b - t.a, ac = t.c - t.a, ap = p - t.a;
	const float d1 = QVector3D::dotProduct(ab, ap), d2 = QVector3D::dotProduct(ac, ap);
	if(d1 <= 0 && d2 <= 0) return t.a;
	const QVector3D bp = p - t.b;
	const float d3 = QVector3D::dotProduct(ab, bp), d4 = QVector3D::dotProduct(ac, bp);
	if(d3 >= 0 && d4 <= d3) return t.b;
	const float vc = d1 * d4 - d3 * d2;
	if(vc <= 0 && d1 >= 0 && d3 <= 0) return t.a + ab * (d1 / (d1 - d3));
	const QVector3D cp = p - t.c;
	const float d5 = QVector3D::dotProduct(ab, cp), d6 = QVector3D::dotProduct(ac, cp);
	if(d6 >= 0 && d5 <= d6) return t.c;
	const float vb = d5 * d2 - d1 * d6;
	if(vb <= 0 && d2 >= 0 && d6 <= 0) return t.a + ac * (d2 / (d2 - d6));
	const float va = d3 * d6 - d5 * d4;
	if(va <= 0 && d4 >= d3 && d5 >= d6)
		return t.b + (t.c - t.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
	const float inverse = 1 / (va + vb + vc);
	return t.a + ab * (vb * inverse) + ac * (vc * inverse);
}

void ClosestSegments(const QVector3D& a, const QVector3D& b, const QVector3D& c,
	const QVector3D& d, QVector3D& first, QVector3D& second)
{
	const QVector3D u = b - a, v = d - c, w = a - c;
	const float aa = QVector3D::dotProduct(u, u), bb = QVector3D::dotProduct(u, v);
	const float cc = QVector3D::dotProduct(v, v), dd = QVector3D::dotProduct(u, w);
	const float ee = QVector3D::dotProduct(v, w), denominator = aa * cc - bb * bb;
	float s = denominator > 1e-10f ? qBound(0.f, (bb * ee - cc * dd) / denominator, 1.f) : 0;
	float t = cc > 1e-10f ? (bb * s + ee) / cc : 0;
	if(t < 0) { t = 0; s = aa > 1e-10f ? qBound(0.f, -dd / aa, 1.f) : 0; }
	else if(t > 1) { t = 1; s = aa > 1e-10f ? qBound(0.f, (bb - dd) / aa, 1.f) : 0; }
	first = a + u * s; second = c + v * t;
}

float CapsuleDistance(const QVector3D& feet, const WalkSettings& settings,
	const Triangle& triangle, QVector3D& spinePoint, QVector3D& surfacePoint)
{
	const QVector3D bottom = feet + QVector3D(0, 0, settings.radius);
	const QVector3D top = feet + QVector3D(0, 0, settings.height - settings.radius);
	const QVector3D segment = top - bottom;
	const float divisor = QVector3D::dotProduct(triangle.normal, segment);
	if(std::abs(divisor) > 1e-8f)
	{
		const float t = QVector3D::dotProduct(triangle.normal, triangle.a - bottom) / divisor;
		if(t >= 0 && t <= 1)
		{
			const QVector3D p = bottom + segment * t;
			if(InsideTriangle(p, triangle)) { spinePoint = surfacePoint = p; return 0; }
		}
	}
	spinePoint = bottom; surfacePoint = ClosestPoint(bottom, triangle);
	float distance = (spinePoint - surfacePoint).lengthSquared();
	const auto candidate = [&](const QVector3D& a, const QVector3D& b)
	{
		const float next = (a - b).lengthSquared();
		if(next < distance) { distance = next; spinePoint = a; surfacePoint = b; }
	};
	candidate(top, ClosestPoint(top, triangle));
	const QVector3D vertices[] = {triangle.a, triangle.b, triangle.c};
	for(int i = 0; i < 3; ++i)
	{
		QVector3D a, b;
		ClosestSegments(bottom, top, vertices[i], vertices[(i + 1) % 3], a, b);
		candidate(a, b);
	}
	return std::sqrt(distance);
}

}

class WalkWorld::Impl
{
public:
	WalkSettings settings;
	QVector<Triangle> triangles;
	QVector<Box> obstacles;
	QVector<Ladder> ladders;
	QVector<RoomVolume> rooms;
	QHash<quint64, QVector<int>> cells;
	QHash<quint64, QVector<int>> roomCells;
	QSet<quint32> openDoors;
	quint32 consoleId = 0;
	QMatrix4x4 consoleTransform;
	Box sceneBounds;
	float plane = 0;

	void AddTriangle(const Triangle& triangle)
	{
		const int index = triangles.size();
		if(!index) sceneBounds = triangle.bounds;
		else
		{
			sceneBounds.Include(triangle.bounds.minimum);
			sceneBounds.Include(triangle.bounds.maximum);
		}
		triangles.append(triangle);
		const Box& box = triangle.bounds;
		for(int x = Cell(box.minimum.x()); x <= Cell(box.maximum.x()); ++x)
			for(int y = Cell(box.minimum.y()); y <= Cell(box.maximum.y()); ++y)
				for(int z = Cell(box.minimum.z()); z <= Cell(box.maximum.z()); ++z)
					cells[CellKey(x, y, z)].append(index);
	}

	void AddObstacle(const QPointF& center, int deck)
	{
		const float half = float(CoordinateSpacing * .5);
		const float bottom = float(deck * DeckHeight) - .05f;
		const Box box(QVector3D(float(center.x()) - half, float(center.y()) - half, bottom),
			QVector3D(float(center.x()) + half, float(center.y()) + half, bottom + float(DeckHeight)));
		const int index = obstacles.size();
		obstacles.append(box);
		const QVector3D& lo = box.minimum;
		const QVector3D& hi = box.maximum;
		const QVector3D points[] = {lo, {hi.x(), lo.y(), lo.z()}, {hi.x(), hi.y(), lo.z()},
			{lo.x(), hi.y(), lo.z()}, {lo.x(), lo.y(), hi.z()}, {hi.x(), lo.y(), hi.z()},
			hi, {lo.x(), hi.y(), hi.z()}};
		const int faces[][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
			{0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
			{2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
		for(const auto& face : faces)
		{
			Triangle triangle(points[face[0]], points[face[1]], points[face[2]], 0, index);
			// Include the volume interior in broad-phase queries as well as its
			// faces, so an initial position inside the cell can be corrected.
			triangle.bounds = box;
			AddTriangle(triangle);
		}
	}

	bool InsideObstacle(const QVector3D& feet, int index) const
	{
		if(index < 0) return false;
		const Box& box = obstacles.at(index);
		return feet.x() > box.minimum.x() && feet.x() < box.maximum.x()
			&& feet.y() > box.minimum.y() && feet.y() < box.maximum.y()
			&& feet.z() + settings.radius < box.maximum.z()
			&& feet.z() + settings.height - settings.radius > box.minimum.z();
	}

	Box BodyBounds(const QVector3D& feet, float extra = 0) const
	{
		const float radius = settings.radius + extra;
		return Box(feet - QVector3D(radius, radius, extra),
			feet + QVector3D(radius, radius, settings.height + extra));
	}

	QVector<int> Candidates(const Box& box) const
	{
		QVector<int> result;
		if(!Finite(box.minimum) || !Finite(box.maximum)) return result;
		for(int axis = 0; axis < 3; ++axis)
			if(std::abs(box.minimum[axis]) > 1000000 || std::abs(box.maximum[axis]) > 1000000) return result;
		QSet<int> seen;
		for(int x = Cell(box.minimum.x()); x <= Cell(box.maximum.x()); ++x)
			for(int y = Cell(box.minimum.y()); y <= Cell(box.maximum.y()); ++y)
				for(int z = Cell(box.minimum.z()); z <= Cell(box.maximum.z()); ++z)
				{
					const auto bucket = cells.constFind(CellKey(x, y, z));
					if(bucket == cells.constEnd()) continue;
					for(int index : bucket.value())
						if(!seen.contains(index) && triangles.at(index).bounds.Intersects(box))
						{ seen.insert(index); result.append(index); }
				}
		return result;
	}

	int RoomType(const QVector3D& feet) const
	{
		const QPointF point(feet.x(), feet.y());
		const float z = feet.z() + settings.radius;
		const auto bucket = roomCells.constFind(CellKey(Cell(feet.x()), Cell(feet.y()), Cell(feet.z())));
		if(bucket == roomCells.constEnd()) return 0;
		for(int index : bucket.value())
		{
			const RoomVolume& room = rooms.at(index);
			if(z >= room.bottom - .25f && z <= room.top + .25f && room.bounds.contains(point)
				&& room.boundary.containsPoint(point, Qt::OddEvenFill)) return room.type;
		}
		return 0;
	}

	bool InShip(const QVector3D& feet) const
	{
		if(RoomType(feet)) return true;
		// Keep a standing/jumping body above its supporting hull attached to
		// ship gravity. This substitutes for the original parent-ship query.
		const QPointF point(feet.x(), feet.y());
		const auto bucket = roomCells.constFind(CellKey(Cell(feet.x()), Cell(feet.y()), Cell(feet.z())));
		if(bucket == roomCells.constEnd()) return false;
		for(int index : bucket.value())
		{
			const RoomVolume& room = rooms.at(index);
			if(feet.z() >= room.bottom - .25f && feet.z() <= room.top + settings.height
				&& room.bounds.contains(point) && room.boundary.containsPoint(point, Qt::OddEvenFill)) return true;
		}
		return false;
	}

	Contact Resolve(QVector3D& feet, const QVector3D& previous, bool groundPlane) const
	{
		Contact contact;
		for(int iteration = 0; iteration < 8; ++iteration)
		{
			bool moved = false;
			for(int index : Candidates(BodyBounds(feet, ContactTolerance)))
			{
				const Triangle& triangle = triangles.at(index);
				if(!triangle.blocksMovement) continue;
				if(triangle.doorId && openDoors.contains(triangle.doorId)) continue;
				if(InsideObstacle(feet, triangle.obstacle))
				{
					// A closed volume must also reject a character initially inside it,
					// where testing only distance to its surface would miss the overlap.
					const Box& box = obstacles.at(triangle.obstacle);
					float distance = std::numeric_limits<float>::max();
					QVector3D normal;
					for(int axis = 0; axis < 2; ++axis)
					{
						const float low = feet[axis] - box.minimum[axis];
						const float high = box.maximum[axis] - feet[axis];
						if(low < distance) { distance = low; normal = QVector3D(); normal[axis] = -1; }
						if(high < distance) { distance = high; normal = QVector3D(); normal[axis] = 1; }
					}
					feet += normal * (distance + settings.radius + Skin);
					contact.wall = true;
					moved = true;
					continue;
				}
				QVector3D spine, surface;
				const float distance = CapsuleDistance(feet, settings, triangle, spine, surface);
				if(distance > settings.radius + ContactTolerance) continue;
				QVector3D normal;
				if(distance > 1e-6f) normal = (spine - surface) / distance;
				else
				{
					normal = triangle.normal;
					const QVector3D oldCenter = previous + QVector3D(0, 0, settings.height * .5f);
					if(QVector3D::dotProduct(oldCenter - triangle.a, normal) < 0) normal = -normal;
				}
				contact.Add(normal);
				if(triangle.doorId)
				{
					QVector<quint32>& ids = normal.z() >= WalkableNormal ? contact.supportDoors : contact.doors;
					if(!ids.contains(triangle.doorId)) ids.append(triangle.doorId);
				}
				if(distance < settings.radius)
				{ feet += normal * (settings.radius - distance + Skin); moved = true; }
			}
			if(!moved) break;
		}
		if(groundPlane && !InShip(feet) && feet.z() <= plane + ContactTolerance)
		{
			feet.setZ(std::max(feet.z(), plane + Skin));
			contact.ground = true;
		}
		return contact;
	}

	bool Clear(const QVector3D& feet) const
	{
		for(int index : Candidates(BodyBounds(feet)))
		{
			const Triangle& triangle = triangles.at(index);
			if(!triangle.blocksMovement) continue;
			if(triangle.doorId && openDoors.contains(triangle.doorId)) continue;
			if(InsideObstacle(feet, triangle.obstacle)) return false;
			QVector3D spine, surface;
			if(CapsuleDistance(feet, settings, triangle, spine, surface) < settings.radius - Skin) return false;
		}
		return true;
	}

	bool Drop(QVector3D& feet, float distance, bool groundPlane) const
	{
		const int steps = std::max(1, int(std::ceil(distance / (settings.radius * .3f))));
		for(int i = 0; i < steps; ++i)
		{
			const QVector3D previous = feet;
			feet.setZ(feet.z() - distance / steps);
			if(Resolve(feet, previous, groundPlane).ground) return true;
		}
		return false;
	}
};

WalkWorld::WalkWorld() : m_Impl(new Impl) {}
WalkWorld::~WalkWorld() {}

void WalkWorld::SetSettings(const WalkSettings& settings)
{
	WalkSettings value = settings;
	if(!std::isfinite(value.radius) || !std::isfinite(value.height) || !std::isfinite(value.eyeHeight)
		|| !std::isfinite(value.stepHeight)) return;
	value.radius = qBound(.2f, value.radius, 3.f);
	value.height = qBound(value.radius * 2 + .1f, value.height, 20.f);
	value.eyeHeight = qBound(value.radius, value.eyeHeight, value.height);
	value.stepHeight = qBound(0.f, value.stepHeight, 2.f);
	m_Impl->settings = value;
}

const WalkSettings& WalkWorld::Settings() const { return m_Impl->settings; }

bool WalkWorld::TrySetBodySize(const QVector3D& feet, float height, float eyeHeight, bool requireClear)
{
	const WalkSettings previous = m_Impl->settings;
	WalkSettings next = previous;
	next.height = height;
	next.eyeHeight = eyeHeight;
	SetSettings(next);
	// Explicit stance moves must fit before committing. Animated size changes
	// during physics movement are resolved by the collision solver instead.
	if(requireClear && !m_Impl->Clear(feet))
	{
		m_Impl->settings = previous;
		return false;
	}
	return true;
}

void WalkWorld::SetDesignPlane(float height) { if(std::isfinite(height)) m_Impl->plane = height; }

void WalkWorld::SetMesh(const Mesh& mesh, const Design& design)
{
	Impl& world = *m_Impl;
	world.triangles.clear(); world.obstacles.clear(); world.cells.clear(); world.openDoors.clear();
	world.rooms.clear(); world.ladders.clear(); world.roomCells.clear();
	world.consoleId = 0;
	int lowestDeck = MaximumDeck + 1;
	int highestDeck = MinimumDeck - 1;
	for(const Room& room : design.rooms)
	{
		if(room.deck < MinimumDeck || room.deck > MaximumDeck || !room.roomType
			|| room.roomType == 12 || room.roomType > 21) continue;
		RoomVolume volume;
		volume.boundary = RoomBoundary(room);
		if(!Geometry::IsSupportedBoundary(volume.boundary)) continue;
		volume.bounds = volume.boundary.boundingRect();
		volume.bottom = room.deck * float(DeckHeight); volume.top = volume.bottom + float(DeckHeight);
		volume.type = room.roomType;
		lowestDeck = std::min(lowestDeck, int(room.deck));
		highestDeck = std::max(highestDeck, int(room.deck));
		const int index = world.rooms.size();
		world.rooms.append(volume);
		for(int x = Cell(float(volume.bounds.left())); x <= Cell(float(volume.bounds.right())); ++x)
			for(int y = Cell(float(volume.bounds.top())); y <= Cell(float(volume.bounds.bottom())); ++y)
				for(int z = Cell(volume.bottom - 3.25f); z <= Cell(volume.top + 20); ++z)
					world.roomCells[CellKey(x, y, z)].append(index);
	}
	for(const Batch& batch : mesh.batches)
	{
		const bool ladder = batch.material == Material::Ladder;
		if(batch.degreesPerSecond != 0) continue;
		const bool solid = StructuralBatch(batch) && !ladder && batch.material != Material::LadderPole;
		for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
		{
			const QVector3D& a = batch.vertices.at(i).position;
			const QVector3D& b = batch.vertices.at(i + 1).position;
			const QVector3D& c = batch.vertices.at(i + 2).position;
			if(!Finite(a) || !Finite(b) || !Finite(c)) continue;
			Triangle triangle(a, b, c, batch.doorId);
			if(!Finite(triangle.normal) || triangle.normal.isNull()) continue;
			triangle.blocksMovement = solid;
			triangle.consoleId = batch.consoleId;
			world.AddTriangle(triangle);
		}
	}
	QSet<quint32> occupied;
	for(const Detail& detail : design.details)
	{
		if(IsHatchDetail(detail.detailType) && !detail.hiddenRaw
			&& detail.deck >= MinimumDeck && detail.deck <= MaximumDeck + 1)
		{
			const QPointF xy = DecodePoint(detail.packedXY);
			Ladder ladder;
			ladder.anchor = QVector3D(float(xy.x()), float(xy.y()), detail.deck * float(DeckHeight));
			ladder.rotation = DetailRotationStep(detail.detailType);
			ladder.deck = detail.deck;
			world.ladders.append(ladder);
		}
		if(detail.deck < lowestDeck || detail.deck > highestDeck || !BlocksPathCell(detail.detailType)) continue;
		// The original painter does not test hiddenRaw. Detail coordinates are
		// already half-grid centers; multiple records can share a single cell.
		const quint32 key = (quint32(quint8(detail.deck)) << 16) | detail.packedXY;
		if(occupied.contains(key)) continue;
		occupied.insert(key);
		world.AddObstacle(DecodePoint(detail.packedXY), detail.deck);
	}
}

void WalkWorld::SetDoorOpen(quint32 doorId, bool open)
{
	if(!doorId) return;
	if(open) m_Impl->openDoors.insert(doorId);
	else m_Impl->openDoors.remove(doorId);
}

bool WalkWorld::ToggleLadder(WalkState& state) const
{
	if(state.consoleType) return false;
	if(state.ladderEngaged)
	{
		state.ladderEngaged = false;
		state.onLadder = false;
		state.velocity = QVector3D();
		return true;
	}
	if(!Finite(state.feet)) return false;
	const Impl& world = *m_Impl;
	const int deck = int(std::floor((state.feet.z() + .05f) / DeckHeight));
	const Ladder* nearest = nullptr;
	float distance = 36;
	for(const Ladder& ladder : world.ladders)
	{
		if(ladder.deck != deck && ladder.deck != deck + 1) continue;
		QVector3D target = ladder.anchor;
		target.setZ(state.feet.z());
		const QVector3D delta = target - state.feet;
		if(delta.lengthSquared() >= distance) continue;
		// Sweep the local capsule to the attachment point, so a nearby ladder
		// cannot pull the player through a wall or a closed door.
		const int steps = std::max(1, int(std::ceil(delta.length() / (world.settings.radius * .25f))));
		bool clear = true;
		for(int i = 1; i <= steps; ++i)
			if(!world.Clear(state.feet + delta * (float(i) / steps))) { clear = false; break; }
		if(!clear) continue;
		nearest = &ladder;
		distance = delta.lengthSquared();
	}
	if(!nearest) return false;
	state.ladderAnchor = nearest->anchor;
	state.ladderFacing = nearest->rotation * 45.f;
	state.ladderBottom = nearest->anchor.z() - float(DeckHeight);
	state.ladderTop = nearest->anchor.z();
	// Aligned hatches form one continuous vertical path across decks.
	bool extended;
	do
	{
		extended = false;
		for(const Ladder& ladder : world.ladders)
		{
			if(ladder.anchor.x() != state.ladderAnchor.x() || ladder.anchor.y() != state.ladderAnchor.y()) continue;
			const float bottom = ladder.anchor.z() - float(DeckHeight), top = ladder.anchor.z();
			if(bottom > state.ladderTop || top < state.ladderBottom) continue;
			if(bottom < state.ladderBottom) { state.ladderBottom = bottom; extended = true; }
			if(top > state.ladderTop) { state.ladderTop = top; extended = true; }
		}
	} while(extended);
	state.feet.setX(state.ladderAnchor.x());
	state.feet.setY(state.ladderAnchor.y());
	state.ladderEngaged = true;
	state.onLadder = true;
	state.gravityActive = false;
	state.gravitySuspended = true;
	state.velocity = QVector3D();
	return true;
}

bool WalkWorld::FindSpawn(const Design& design, int deck, WalkState& state) const
{
	const Impl& world = *m_Impl;
	const float floor = qBound(MinimumDeck, deck, MaximumDeck) * float(DeckHeight);
	const auto tryPoint = [&](const QPointF& point, float z)
	{
		QVector3D feet(float(point.x()), float(point.y()), z + .04f);
		if(!world.Clear(feet)) return false;
		if(!world.Drop(feet, world.settings.stepHeight + .1f, true)) return false;
		if(!world.Clear(feet)) return false;
		state = WalkState(); state.feet = feet; state.grounded = true;
		return true;
	};
	if(tryPoint(QPointF(), floor)) return true;
	int probes = 0;
	for(const Room& room : design.rooms)
	{
		if(room.deck != deck || !room.roomType || room.roomType == 9
			|| room.roomType == 12 || room.roomType > 21) continue;
		const QPolygonF polygon = RoomBoundary(room);
		if(!Geometry::IsSupportedBoundary(polygon)) continue;
		const QRectF bounds = polygon.boundingRect();
		const QPointF center = bounds.center();
		if(polygon.containsPoint(center, Qt::OddEvenFill) && tryPoint(center, floor)) return true;
		for(double x = bounds.left() + CoordinateSpacing; x < bounds.right(); x += CoordinateSpacing)
			for(double y = bounds.top() + CoordinateSpacing; y < bounds.bottom(); y += CoordinateSpacing)
			{
				if(++probes > 8192) return false;
				const QPointF p(x, y);
				if(polygon.containsPoint(p, Qt::OddEvenFill) && tryPoint(p, floor)) return true;
			}
	}
	if(world.rooms.isEmpty() && world.Clear(QVector3D(0, 0, world.plane + Skin)))
	{
		state = WalkState(); state.feet = QVector3D(0, 0, world.plane + Skin); state.grounded = true;
		return true;
	}
	return false;
}

bool WalkWorld::EnterConsole(const Design& design, WalkState& state, float cgHeight) const
{
	if(state.consoleType || state.ladderEngaged || !Finite(state.feet) || !std::isfinite(cgHeight)
		|| !m_Impl->InShip(state.feet)) return false;
	const int deck = int(std::floor((state.feet.z() + cgHeight + .05f) / DeckHeight));
	// AuAtom13!DaSpaceShip::CanEnterConsole: first eligible detail, same
	// deck at the avatar's CG, strictly less than three feet in XY.
	// Hidden is not tested.
	for(const Detail& detail : design.details)
	{
		switch(detail.detailType)
		{
		case 1: case 4: case 6: case 7: case 12: case 13:
		case 15: case 17: case 19: case 73:
			break;
		default: continue;
		}
		if(detail.deck != deck) continue;
		const QPointF point = DecodePoint(detail.packedXY);
		const QVector3D seat(float(point.x()), float(point.y()), float(deck * DeckHeight));
		const QVector3D delta = seat - state.feet;
		if(delta.x() * delta.x() + delta.y() * delta.y() >= 9.f) continue;
		state.feet = seat;
		state.velocity = QVector3D();
		state.consoleType = detail.detailType;
		state.consoleId = detail.detailId;
		state.grounded = false;
		state.gravityActive = false;
		state.gravitySuspended = true;
		state.inShip = true;
		state.blockedDoors.clear();
		return true;
	}
	return false;
}

void WalkWorld::SetConsoleRotation(quint32 consoleId, const QVector3D& floorPoint, float facing)
{
	Impl& world = *m_Impl;
	world.consoleId = consoleId;
	world.consoleTransform.setToIdentity();
	world.consoleTransform.translate(floorPoint);
	world.consoleTransform.rotate(facing - 90.f, 0, 0, 1);
	world.consoleTransform.translate(-floorPoint);
}

WalkEnvironment WalkWorld::Environment(const QVector3D& feet, bool studioGravity, float bodyOriginHeight) const
{
	WalkEnvironment environment;
	environment.roomType = m_Impl->RoomType(feet);
	environment.inShip = m_Impl->InShip(feet);
	// Probe from the atom origin, as the native floor query does. Returning
	// from Jump to Stand can temporarily put the new feet below the floor;
	// keep finding that floor so native physics can correct the penetration.
	const float probeHeight = std::max(.25f, bodyOriginHeight);
	const QVector3D origin = feet + QVector3D(0, 0, probeHeight);
	if(!environment.inShip)
	{
		// A pose expansion below the deck must not lose ship gravity while
		// the atom origin remains inside the room. RoomType adds the radius.
		environment.roomType = m_Impl->RoomType(origin - QVector3D(0, 0, m_Impl->settings.radius));
		environment.inShip = environment.roomType != 0;
	}
	environment.gravityWell = environment.roomType == 11;
	environment.gravity = studioGravity || environment.inShip;
	Box bounds(origin - QVector3D(0, 0, 256), origin);
	for(int index : m_Impl->Candidates(bounds))
	{
		const Triangle& triangle = m_Impl->triangles.at(index);
		if(!triangle.blocksMovement || std::abs(triangle.normal.z()) < .5f
			|| (triangle.doorId && m_Impl->openDoors.contains(triangle.doorId))) continue;
		const float distance = QVector3D::dotProduct(triangle.a - origin, triangle.normal) / -triangle.normal.z();
		if(distance < 0 || distance > 256) continue;
		if(InsideTriangle(origin - QVector3D(0, 0, distance), triangle))
			environment.fallDistance = std::min(environment.fallDistance, distance - probeHeight);
	}
	if(studioGravity && !environment.inShip)
		environment.fallDistance = std::min(environment.fallDistance, feet.z() - m_Impl->plane);
	return environment;
}

void WalkWorld::MoveDisplacement(WalkState& state, const QVector3D& displacement, bool studioGravity) const
{
	if(!Finite(displacement) || !Finite(state.feet)) return;
	const Impl& world = *m_Impl;
	const bool groundPlane = studioGravity && !world.InShip(state.feet);
	const auto contacts = [&](const Contact& contact, bool descending) {
		for(quint32 id : contact.doors)
			if(!state.blockedDoors.contains(id)) state.blockedDoors.append(id);
		if(descending)
			for(quint32 id : contact.supportDoors)
				if(!state.blockedDoors.contains(id)) state.blockedDoors.append(id);
		for(const QVector3D& normal : contact.normals)
		{
			const float inward = QVector3D::dotProduct(state.velocity, normal);
			if(inward < 0) state.velocity -= normal * inward;
		}
	};
	Box sweep = world.BodyBounds(state.feet);
	const Box destinationBounds = world.BodyBounds(state.feet + displacement);
	sweep.Include(destinationBounds.minimum);
	sweep.Include(destinationBounds.maximum);
	// Empty-space movement needs no collision subdivisions.
	const int steps = groundPlane || sweep.Intersects(world.sceneBounds)
		? std::max(1, int(std::ceil(displacement.length() / (world.settings.radius * .25f)))) : 1;
	const QVector3D step = displacement / steps;
	for(int i = 0; i < steps; ++i)
	{
		const QVector3D previous = state.feet;
		state.feet += step;
		const Contact contact = world.Resolve(state.feet, previous, groundPlane);
		contacts(contact, step.z() < 0);
		if(state.ladderEngaged)
		{
			if(std::abs(state.feet.x() - state.ladderAnchor.x()) > Skin
				|| std::abs(state.feet.y() - state.ladderAnchor.y()) > Skin)
			{
				state.feet = previous;
				state.velocity = QVector3D();
			}
			state.feet.setX(state.ladderAnchor.x());
			state.feet.setY(state.ladderAnchor.y());
		}
		else if(state.grounded && contact.wall && world.settings.stepHeight > 0)
		{
			const QVector3D raised = previous + QVector3D(0, 0, world.settings.stepHeight);
			if(world.Clear(raised))
			{
				QVector3D stepped = raised + step;
				world.Resolve(stepped, raised, groundPlane);
				if(QVector3D::dotProduct(stepped - previous, step) > QVector3D::dotProduct(state.feet - previous, step)
					&& world.Drop(stepped, world.settings.stepHeight + .06f, groundPlane))
					state.feet = stepped;
			}
		}
		state.grounded = contact.ground;
	}
	const WalkEnvironment environment = Environment(state.feet, studioGravity, world.settings.eyeHeight);
	state.inShip = environment.inShip;
	state.gravityActive = environment.gravity && !environment.gravityWell;
	state.gravitySuspended = !state.gravityActive;
	state.grounded = state.gravityActive && !state.ladderEngaged && environment.fallDistance < .25f;
	state.onLadder = state.ladderEngaged;
	if(state.ladderEngaged && (state.feet.z() < state.ladderBottom - world.settings.stepHeight
		|| state.feet.z() > state.ladderTop + world.settings.stepHeight))
		ToggleLadder(state);
}

bool WalkWorld::Raycast(const QVector3D& origin, const QVector3D& direction,
	float maximumDistance, WalkHit& hit, bool includeOpenDoors) const
{
	hit = WalkHit();
	if(!Finite(origin) || !Finite(direction) || direction.lengthSquared() < 1e-10f
		|| !std::isfinite(maximumDistance) || maximumDistance <= 0) return false;
	maximumDistance = std::min(maximumDistance, 256.f);
	const QVector3D ray = direction.normalized(), end = origin + ray * maximumDistance;
	Box bounds(origin, origin); bounds.Include(end);
	bool found = false;
	float nearest = maximumDistance;
	const auto intersect = [&](const Triangle& triangle, const QVector3D& from, const QVector3D& rayDirection, bool swivel)
	{
		if(triangle.obstacle >= 0) return;
		if(!includeOpenDoors && triangle.doorId && m_Impl->openDoors.contains(triangle.doorId)) return;
		const float denominator = QVector3D::dotProduct(triangle.normal, rayDirection);
		if(std::abs(denominator) < 1e-8f) return;
		const float distance = QVector3D::dotProduct(triangle.a - from, triangle.normal) / denominator;
		if(distance < 0 || distance > nearest) return;
		const QVector3D point = from + rayDirection * distance;
		if(!InsideTriangle(point, triangle)) return;
		found = true; nearest = distance;
		hit.point = point; hit.normal = denominator < 0 ? triangle.normal : -triangle.normal;
		if(swivel)
		{
			hit.point = m_Impl->consoleTransform.map(hit.point);
			hit.normal = m_Impl->consoleTransform.mapVector(hit.normal);
		}
		hit.distance = distance; hit.doorId = triangle.doorId;
	};
	for(int index : m_Impl->Candidates(bounds))
	{
		const Triangle& triangle = m_Impl->triangles.at(index);
		if(!m_Impl->consoleId || triangle.consoleId != m_Impl->consoleId)
			intersect(triangle, origin, ray, false);
	}
	if(m_Impl->consoleId)
	{
		// Query the occupied console in its authored coordinates, so hand
		// and sight targeting see the same swivelling geometry as rendering.
		const QMatrix4x4 inverse = m_Impl->consoleTransform.inverted();
		const QVector3D from = inverse.map(origin), localRay = inverse.mapVector(ray);
		Box localBounds(from, from); localBounds.Include(from + localRay * maximumDistance);
		for(int index : m_Impl->Candidates(localBounds))
		{
			const Triangle& triangle = m_Impl->triangles.at(index);
			if(triangle.consoleId == m_Impl->consoleId)
				intersect(triangle, from, localRay, true);
		}
	}
	return found;
}
}
}
