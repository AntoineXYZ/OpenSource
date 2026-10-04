#include "aulegacyshipmesh.h"
#include "aulegacyshipgeometry.h"

#include <QHash>
#include <QPoint>
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
constexpr int RasterSize = 516;
constexpr int RasterOrigin = 256;
constexpr double RasterSpacing = 1.41421356225;
constexpr double Epsilon = 1e-5;
constexpr int MaximumVertices = 6000000;

struct ProfilePoint
{
	double offset;
	double height;
	int strip;
};

// AuData13!DnRoom::WallProfile, 180174d00. Each strip value describes
// the segment beginning at that point: 4 opaque hull, 5 hull window.
QVector<ProfilePoint> WallProfile(int type, int armor)
{
	QVector<ProfilePoint> result;
	const auto point = [&](double offset, double height, int strip)
		{ result.append({offset, height, strip}); };
	switch(type)
	{
	case 1:
		point(0, 0, armor < 2 ? 5 : 4);
		point(1, 1, armor < 6 ? 5 : 4); point(1, 6, 5); break;
	case 2:
		point(0, 0, 5); point(2, 1, 5);
		if(armor > 0) point(2, 1.5 + .5 * armor, 5);
		break;
	case 3:
		point(0, 0, 4); point(1, armor < 3 ? 4 : armor < 6 ? 5 : armor < 9 ? 6 : 7, 5);
		if(armor < 9) point(1, 7, 4);
		break;
	case 4:
		point(0, 0, armor < 6 ? 5 : 4); point(2, 2, 5); point(2, 6, armor < 9 ? 5 : 4); break;
	case 5:
		point(0, 0, armor < 3 ? 5 : 4); point(2, 1, 4); point(2, 7, armor < 7 ? 5 : 4); break;
	case 6:
	case 15:
		point(0, 0, armor < 2 ? 5 : 4); point(1, 1, armor < 4 ? 5 : 4);
		if(armor >= 4 && armor < 9) point(1, 6, 5);
		point(1, 7, armor < 7 ? 5 : 4); break;
	case 7:
		point(0, 0, armor < 8 ? 5 : 4); point(1, 4, armor < 3 ? 5 : 4); point(1, 5, 5); break;
	case 8:
		point(0, 0, 4);
		point(armor < 6 ? 1 : 1.5, armor < 6 || armor == 10 ? 1 : 1.5, 4);
		if(armor > 0) point(armor < 6 ? 1 : 1.5, armor < 6 ? armor + 1 : armor - 3.5, 4);
		break;
	case 9:
		point(0, 0, 4); point(2, armor < 6 ? 2 : 1, 4); point(2, armor < 6 ? 6 : 7, 4); break;
	case 10:
		point(0, 0, armor < 2 ? 5 : 4); point(1, 3, armor < 8 ? 5 : 4);
		point(1, 7, armor < 6 || (armor >= 8 && armor < 10) ? 5 : 4); break;
	case 11:
		point(0, 0, armor >= 4 && armor < 7 ? 5 : 4); point(1, 1, armor < 4 ? 5 : 4);
		point(1, 7, armor >= 4 && armor < 7 ? 5 : 4); break;
	case 13:
		point(0, 0, armor < 3 ? 5 : 4); point(2, 1, 4); point(2, 7, armor < 6 ? 5 : 4); break;
	case 14:
		point(0, 0, armor < 5 ? 5 : 4); point(1, 1, 5); point(2, 7, armor < 8 ? 5 : 4); break;
	case 16:
		point(0, 0, armor < 6 ? 5 : 4); point(.75, 7, armor < 3 || (armor >= 6 && armor < 8) ? 5 : 4); break;
	case 17:
		point(0, 0, 4); point(.75, 2, armor < 5 ? 5 : 4);
		point(1.75, 6, armor < 2 || (armor >= 5 && armor < 8) ? 5 : 4); break;
	case 18:
		point(0, 0, 4); point(1, 3, armor < 5 ? 5 : 4);
		point(1, 7, armor < 2 || (armor >= 5 && armor < 9) ? 5 : 4); break;
	case 19:
		point(0, 0, armor < 5 ? 5 : 4); point(1, armor < 9 ? 1 : 2, 5); break;
	case 20:
		point(0, 0, 4); point(1, 2, armor < 3 ? 5 : 4); point(1, 6, armor < 9 ? 5 : 4); break;
	case 21:
		point(0, 0, armor < 6 ? 5 : 4); point(1.75, 2, 5);
		point(1, 7, armor < 3 || (armor >= 6 && armor < 8) ? 5 : 4); break;
	case 28:
		point(0, 0, 1); break;
	case 29:
		point(0, 0, 0); break;
	case 30:
		point(0, 0, 2); point(0, 7, 4); point(1, 7, armor < 9 ? 5 : 4); break;
	default:
		point(0, 0, 4); break;
	}
	point(0, DeckHeight, 0);
	return result;
}

int Type(quint16 sample) { return (sample >> 8) & 31; }
bool Current(quint16 sample) { return sample && !(sample & 0x8000); }
bool WindowFloor(int type, int armor)
{
	return type == 2 || type == 14 || (type == 21 && armor < 8) || (type == 4 && armor < 7);
}
bool HasInterior(int type) { return type != 9 && !(type >= 22 && type <= 24); }
bool IsHatch(int type) { return type == 8 || type == 51 || type == 52 || (type >= 77 && type <= 97); }

struct Hatch
{
	QPointF center;
	QPolygonF boundary;
	Material material;
	bool diamond;
};

QVector<Hatch> Hatches(const QVector<Detail>& details, int deck)
{
	QVector<Hatch> result;
	QSet<quint16> positions;
	for(const Detail& detail : details)
	{
		if(detail.deck != deck || !IsHatch(detail.detailType)) continue;
		if(positions.contains(detail.packedXY)) continue;
		positions.insert(detail.packedXY);
		Hatch hatch;
		hatch.center = DecodePoint(detail.packedXY);
		const int x = int(std::round(hatch.center.x() / RasterSpacing)) + RasterOrigin;
		const int y = int(std::round(hatch.center.y() / RasterSpacing)) + RasterOrigin;
		hatch.diamond = x % 4 != 2 || y % 4 != 2;
		hatch.material = detail.detailType == 8 || (detail.detailType >= 77 && detail.detailType <= 83)
			? Material::InteriorHatch : detail.detailType == 51 || (detail.detailType >= 84 && detail.detailType <= 90)
				? Material::CrewHatch : Material::HullHatch;
		if(hatch.diamond)
			hatch.boundary << hatch.center + QPointF(-CoordinateSpacing, 0)
				<< hatch.center + QPointF(0, -CoordinateSpacing) << hatch.center + QPointF(CoordinateSpacing, 0)
				<< hatch.center + QPointF(0, CoordinateSpacing);
		else
			hatch.boundary << hatch.center + QPointF(-2, -2) << hatch.center + QPointF(2, -2)
				<< hatch.center + QPointF(2, 2) << hatch.center + QPointF(-2, 2);
		result.append(hatch);
	}
	return result;
}

QPointF PlanPoint(const QPoint& point)
{
	return QPointF((point.x() - RasterOrigin) * RasterSpacing,
		(point.y() - RasterOrigin) * RasterSpacing);
}
QVector3D Position(const QPointF& point, double z)
{
	return QVector3D(float(point.x()), float(point.y()), float(z));
}
double Cross(const QPointF& a, const QPointF& b) { return a.x() * b.y() - a.y() * b.x(); }
double Length(const QPointF& a) { return std::hypot(a.x(), a.y()); }
QPointF Unit(const QPointF& a)
{
	const double length = Length(a);
	return length > Epsilon ? a / length : QPointF();
}
quint32 PointKey(const QPoint& p) { return quint32(p.x() * RasterSize + p.y()); }

struct Raster
{
	QVector<quint16> samples = QVector<quint16>(RasterSize * RasterSize, 0);
	QVector<int> owners = QVector<int>(RasterSize * RasterSize, -1);
	quint16 At(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < RasterSize && y < RasterSize
			? samples.at(x * RasterSize + y) : 0;
	}
	int Owner(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < RasterSize && y < RasterSize
			? owners.at(x * RasterSize + y) : -1;
	}

	// The 16-bit floor buffer uses a perimeter walk followed by directed
	// rightward fills. Open-below keeps the previous room's tag, except fuel.
	// This is distinct from the designer's 8-bit paint occupancy buffer.
	void Fill(const QPolygonF& boundary, int type, int subtype, int owner)
	{
		QVector<QPoint> polygon;
		for(const QPointF& point : boundary)
		{
			const QPoint p(int(std::round(point.x() / RasterSpacing)) + RasterOrigin,
				int(std::round(point.y() / RasterSpacing)) + RasterOrigin);
			if(polygon.isEmpty() || p != polygon.last()) polygon.append(p);
		}
		if(polygon.size() > 1 && polygon.first() == polygon.last()) polygon.removeLast();
		if(polygon.size() < 3) return;
		double area = 0;
		for(int i = 0; i < polygon.size(); ++i)
			area += Cross(polygon.at(i), polygon.at((i + 1) % polygon.size()));
		if(area > 0) std::reverse(polygon.begin(), polygon.end());
		const quint16 tag = quint16(((type & 31) << 8) | subtype);
		const bool openBelow = type == 11 || type == 15;
		int xmin = polygon.first().x(), xmax = xmin, ymin = polygon.first().y(), ymax = ymin;
		for(const QPoint& p : polygon)
		{
			xmin = std::min(xmin, p.x()); xmax = std::max(xmax, p.x());
			ymin = std::min(ymin, p.y()); ymax = std::max(ymax, p.y());
		}
		const int height = ymax - ymin + 1;
		QVector<quint16> temporary;
		if(openBelow) temporary.fill(0, (xmax - xmin + 1) * height);
		const auto write = [&](int x, int y)
		{
			const int index = x * RasterSize + y;
			const quint16 old = samples.at(index);
			quint16 value = tag;
			if(old & 0xc000)
				value = openBelow && Type(old) != 9 ? quint16((old & 0x1fff) | 0x6000)
					: quint16(tag | 0x4000);
			if(openBelow) temporary[(x - xmin) * height + y - ymin] = value;
			else { samples[index] = value; owners[index] = owner; }
		};
		QVector<QPoint> perimeter;
		QPoint previous = polygon.last();
		for(const QPoint& p : polygon)
		{
			const QPoint step((p.x() > previous.x()) - (p.x() < previous.x()),
				(p.y() > previous.y()) - (p.y() < previous.y()));
			while(previous != p)
			{
				perimeter.append(previous);
				write(previous.x(), previous.y());
				previous += step;
			}
		}
		if(perimeter.size() < 3) return;
		for(int i = 0; i < perimeter.size(); ++i)
		{
			const QPoint p = perimeter.at(i);
			const QPoint incoming = p - perimeter.at((i + perimeter.size() - 1) % perimeter.size());
			const QPoint outgoing = perimeter.at((i + 1) % perimeter.size()) - p;
			bool fill = false;
			if(outgoing.x() == 1)
				fill = outgoing.y() == 1 && !(incoming.x() == -1 && incoming.y() >= -1 && incoming.y() <= 0);
			else if(outgoing.x() == 0)
				fill = outgoing.y() == 1
					? !(incoming.x() == -1 && incoming.y() >= -1 && incoming.y() <= 0)
						&& !(incoming.x() == 0 && incoming.y() == -1)
					: incoming.x() == -1 && incoming.y() == 1;
			else if(outgoing.y() == 1)
				fill = incoming.y() == 1 || (incoming.x() == 1 && incoming.y() == 0);
			else if(outgoing.y() == 0 || incoming.x() != 1)
				fill = incoming.x() == -1 && incoming.y() == 1;
			if(!fill) continue;
			for(int x = p.x() + 1; x <= xmax; ++x)
			{
				const int index = x * RasterSize + p.y();
				if(openBelow ? temporary.at((x - xmin) * height + p.y() - ymin) != 0 : (samples.at(index) & 0x9fff) == tag) break;
				write(x, p.y());
			}
		}
		if(openBelow)
			for(int x = xmin; x <= xmax; ++x)
				for(int y = ymin; y <= ymax; ++y)
				{
					const quint16 value = temporary.at((x - xmin) * height + y - ymin);
					if(value) { const int i = x * RasterSize + y; samples[i] = value; owners[i] = owner; }
				}
	}

	void NextDeck()
	{
		for(quint16& sample : samples)
		{
			if(sample & 0x8000) sample = 0;
			else if(sample) sample = quint16((sample & ~0x2000) | 0x8000);
		}
	}
};

struct Builder
{
	Mesh mesh;
	QHash<int, int> batchIndices;
	int vertices = 0;
	bool truncated = false;

	void Warning(const QString& message)
	{
		if(!mesh.warnings.contains(message)) mesh.warnings.append(message);
	}

	void Triangle(Material material, int deck, bool interior,
		const QVector3D& a, const QVector3D& b, const QVector3D& c,
		const QVector2D& ua, const QVector2D& ub, const QVector2D& uc)
	{
		if(vertices >= MaximumVertices)
		{
			truncated = true;
			return;
		}
		QVector3D normal = QVector3D::crossProduct(b - a, c - a);
		if(normal.lengthSquared() < 1e-10f) return;
		normal.normalize();
		const int key = ((deck - MinimumDeck + 1) * 2 + int(interior)) * 128 + int(material);
		int index = batchIndices.value(key, -1);
		if(index < 0)
		{
			index = mesh.batches.size();
			batchIndices.insert(key, index);
			Batch batch;
			batch.material = material; batch.deck = deck; batch.interior = interior;
			mesh.batches.append(batch);
		}
		QVector<Vertex>& out = mesh.batches[index].vertices;
		out.append({a, normal, ua}); out.append({b, normal, ub}); out.append({c, normal, uc});
		if(vertices == 0) mesh.minimum = mesh.maximum = a;
		for(const QVector3D& p : {a, b, c})
			for(int axis = 0; axis != 3; ++axis)
			{
				mesh.minimum[axis] = std::min(mesh.minimum[axis], p[axis]);
				mesh.maximum[axis] = std::max(mesh.maximum[axis], p[axis]);
			}
		vertices += 3;
	}

	void Quad(Material material, int deck, bool interior,
		const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d,
		float repeat = 1)
	{
		Triangle(material, deck, interior, a, b, c, {0, 0}, {repeat, 0}, {repeat, 1});
		Triangle(material, deck, interior, a, c, d, {0, 0}, {repeat, 1}, {0, 1});
	}
};

void SplitPolygon(const QPolygonF& polygon, const QPointF& a, const QPointF& b,
	QPolygonF& inside, QPolygonF& outside)
{
	if(polygon.isEmpty()) return;
	QPointF previous = polygon.last();
	double oldDistance = Cross(b - a, previous - a);
	for(const QPointF& point : polygon)
	{
		const double distance = Cross(b - a, point - a);
		if((distance > Epsilon && oldDistance < -Epsilon) || (distance < -Epsilon && oldDistance > Epsilon))
		{
			const QPointF crossing = previous + (point - previous) * (oldDistance / (oldDistance - distance));
			inside.append(crossing); outside.append(crossing);
		}
		if(distance >= -Epsilon) inside.append(point);
		if(distance <= Epsilon) outside.append(point);
		previous = point; oldDistance = distance;
	}
}

void Plane(Builder& out, const QPoint (&points)[3], int x, int y, double z,
	Material material, int deck, bool interior, bool up, const QVector<Hatch>& hatches)
{
	const auto emitPolygon = [&](const QPolygonF& polygon, Material texture, const Hatch* hatch)
	{
		const auto uv = [&](const QPointF& point)
		{
			QVector2D result;
			if(hatch)
			{
				const QPointF p = point - hatch->center;
				if(hatch->diamond)
					result = QVector2D(float((p.x() + p.y()) / (2 * CoordinateSpacing) + .5),
						float((p.y() - p.x()) / (2 * CoordinateSpacing) + .5));
				else result = QVector2D(float(p.x() / 4 + .5), float(p.y() / 4 + .5));
			}
			else
			{
				result = QVector2D(float((point.x() / RasterSpacing + RasterOrigin - x) / 4),
					float((point.y() / RasterSpacing + RasterOrigin - y) / 4));
				if(texture == Material::InteriorCeiling) result.setX(1 - result.x());
			}
			return result;
		};
		for(int i = 1; i + 1 < polygon.size(); ++i)
		{
			const QPointF a = polygon.first(), b = polygon.at(up ? i : i + 1), c = polygon.at(up ? i + 1 : i);
			out.Triangle(texture, deck, interior, Position(a, z), Position(b, z), Position(c, z), uv(a), uv(b), uv(c));
		}
	};
	QPolygonF triangle;
	for(const QPoint& p : points) triangle.append(PlanPoint(p));
	if(hatches.isEmpty()) { emitPolygon(triangle, material, nullptr); return; }
	QVector<QPolygonF> remainder = {triangle};
	for(const Hatch& hatch : hatches)
	{
		if(!triangle.boundingRect().intersects(hatch.boundary.boundingRect())) continue;
		QVector<QPolygonF> next;
		for(const QPolygonF& polygon : remainder)
		{
			QPolygonF intersection = polygon;
			for(int i = 0; i < hatch.boundary.size() && !intersection.isEmpty(); ++i)
			{
				QPolygonF inside, outside;
				SplitPolygon(intersection, hatch.boundary.at(i), hatch.boundary.at((i + 1) % hatch.boundary.size()), inside, outside);
				if(outside.size() >= 3) next.append(outside);
				intersection = inside;
			}
			// The designer preview keeps hatches closed. The original runtime
			// selected the half-open region and animated its atlas coordinates.
			emitPolygon(intersection, hatch.material, &hatch);
		}
		remainder = next;
	}
	for(const QPolygonF& polygon : remainder) emitPolygon(polygon, material, nullptr);
}

struct Edge
{
	QPoint a;
	QPoint b;
	quint16 tag;
	quint16 adjacent;
	int owner;
	int adjacentOwner;
	Edge(const QPoint& from, const QPoint& to, quint16 room, quint16 neighbor = 0,
		int roomOwner = -1, int neighborOwner = -1)
		: a(from), b(to), tag(room), adjacent(neighbor), owner(roomOwner), adjacentOwner(neighborOwner) {}
};

QVector<Edge> MergeEdges(const QVector<Edge>& edges)
{
	QHash<quint32, QVector<int>> starts, ends;
	for(int i = 0; i < edges.size(); ++i)
	{
		starts[PointKey(edges.at(i).a)].append(i);
		ends[PointKey(edges.at(i).b)].append(i);
	}
	QVector<bool> used(edges.size(), false);
	QVector<Edge> merged;
	for(int i = 0; i < edges.size(); ++i)
	{
		if(used.at(i)) continue;
		Edge edge = edges.at(i);
		used[i] = true;
		for(int direction : {1, -1})
		{
			bool extended;
			do
			{
				extended = false;
				const QVector<int> candidates = direction > 0
					? starts.value(PointKey(edge.b)) : ends.value(PointKey(edge.a));
				for(int candidate : candidates)
				{
					if(used.at(candidate)) continue;
					const Edge& other = edges.at(candidate);
					if(other.tag != edge.tag || other.adjacent != edge.adjacent
						|| other.owner != edge.owner || other.adjacentOwner != edge.adjacentOwner) continue;
					const QPointF a = edge.b - edge.a, b = other.b - other.a;
					if(Cross(a, b) != 0 || QPointF::dotProduct(a, b) <= 0) continue;
					if(direction > 0) edge.b = other.b; else edge.a = other.a;
					used[candidate] = true;
					extended = true;
					break;
				}
			} while(extended);
		}
		merged.append(edge);
	}
	return merged;
}

QVector<QVector<Edge>> Loops(const QVector<Edge>& edges)
{
	QHash<quint32, QVector<int>> starts;
	for(int i = 0; i < edges.size(); ++i) starts[PointKey(edges.at(i).a)].append(i);
	QVector<bool> used(edges.size(), false);
	QVector<QVector<Edge>> loops;
	for(int i = 0; i < edges.size(); ++i)
	{
		if(used.at(i)) continue;
		QVector<Edge> loop;
		int next = i;
		while(next >= 0 && !used.at(next))
		{
			used[next] = true;
			const Edge edge = edges.at(next);
			loop.append(edge);
			if(edge.b == loop.first().a) break;
			next = -1;
			double bestTurn = -4;
			const QPointF incoming = edge.b - edge.a;
			for(int candidate : starts.value(PointKey(edge.b)))
			{
				if(used.at(candidate)) continue;
				const QPointF outgoing = edges.at(candidate).b - edge.b;
				const double turn = std::atan2(Cross(incoming, outgoing),
					QPointF::dotProduct(incoming, outgoing));
				if(turn > bestTurn) { next = candidate; bestTurn = turn; }
			}
		}
		if(loop.size() >= 3 && loop.last().b == loop.first().a) loops.append(loop);
	}
	return loops;
}

QPointF JoinNormal(const QPointF& previous, const QPointF& next)
{
	const QPointF a = Unit(previous), b = Unit(next);
	const QPointF na(a.y(), -a.x()), nb(b.y(), -b.x());
	const double denominator = 1 + QPointF::dotProduct(na, nb);
	return denominator > Epsilon ? (na + nb) / denominator : nb;
}

Material InteriorWall(int type, double length)
{
	const bool fill = length <= 5.0 + Epsilon;
	if(type == 3 || type == 5 || type == 13)
		return fill ? Material::InteriorWallCargoFill : Material::InteriorWallCargo;
	if(type == 8 || type == 16 || type == 17 || type == 21)
		return fill ? Material::InteriorWallEquipmentFill : Material::InteriorWallEquipment;
	return fill ? Material::InteriorWallFill : Material::InteriorWallFinished;
}

struct WallSpan
{
	QPointF a;
	QPointF b;
	quint16 tag;
	int detail;
};

using DetailGrid = QHash<quint32, QVector<Detail>>;

QVector<WallSpan> WallSpans(const Edge& edge, bool exterior, const DetailGrid& details)
{
	const QPointF a = PlanPoint(edge.a), b = PlanPoint(edge.b), direction = Unit(b - a);
	const double length = Length(b - a);
	struct Opening { double start; double end; int type; };
	QVector<Opening> openings;
	QVector<double> cuts = {0, length};
	QVector<Detail> candidates;
	QPoint cursor = edge.a;
	const QPoint step((edge.b.x() > cursor.x()) - (edge.b.x() < cursor.x()),
		(edge.b.y() > cursor.y()) - (edge.b.y() < cursor.y()));
	while(cursor != edge.b)
	{
		const auto at = details.constFind(PointKey(cursor));
		if(at != details.constEnd()) candidates += at.value();
		cursor += step;
	}
	for(const Detail& detail : candidates)
	{
		const int type = detail.detailType;
		if(exterior ? type != 3 && type != 9 && type != 50
			: type != 5 && type != 49 && type != 50 && type != 53 && type != 54 && type != 55) continue;
		const QPointF position = DecodePoint(detail.packedXY) - a;
		const double at = QPointF::dotProduct(position, direction);
		if(std::abs(Cross(direction, position)) > Epsilon || at <= Epsilon || at >= length - Epsilon) continue;
		double start = at - 2.5, end = at + 2.5;
		if(exterior && (type == 3 || type == 9)) { start = 1; end = length - 1; }
		else if(!exterior && type >= 53) { start = 0; end = length; }
		start = std::max(0.0, start); end = std::min(length, end);
		if(end - start <= Epsilon) continue;
		openings.append({start, end, type}); cuts.append(start); cuts.append(end);
	}
	std::sort(cuts.begin(), cuts.end());
	cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
	QVector<WallSpan> spans;
	for(int i = 1; i < cuts.size(); ++i)
	{
		const double middle = (cuts.at(i - 1) + cuts.at(i)) / 2;
		WallSpan span = {a + direction * cuts.at(i - 1), a + direction * cuts.at(i), edge.tag, 0};
		for(const Opening& opening : openings)
			if(middle > opening.start && middle < opening.end)
			{
				span.detail = opening.type;
				if(exterior) span.tag = quint16((opening.type == 3 ? 28 : opening.type == 9 ? 29 : 30) << 8);
				break;
			}
		spans.append(span);
	}
	return spans;
}

Material HullMaterial(int strip)
{
	if(strip == 1) return Material::LargeHullDoor;
	if(strip == 2) return Material::HullDoor;
	return strip == 5 ? Material::HullWallWindow : Material::HullWall;
}

void HullWalls(Builder& out, const QVector<Edge>& edges, int deck, int armor, const DetailGrid& details)
{
	for(const QVector<Edge>& loop : Loops(MergeEdges(edges)))
	{
		QVector<WallSpan> spans;
		for(const Edge& edge : loop) spans += WallSpans(edge, true, details);
		for(int i = 0; i < spans.size(); ++i)
		{
			const WallSpan& edge = spans.at(i);
			const WallSpan& previous = spans.at((i + spans.size() - 1) % spans.size());
			const WallSpan& next = spans.at((i + 1) % spans.size());
			const QPointF a = edge.a, b = edge.b;
			const QPointF direction = b - a;
			const QPointF na = JoinNormal(a - previous.a, direction);
			const QPointF nb = JoinNormal(direction, next.b - b);
			const QVector<ProfilePoint> profile = WallProfile(Type(edge.tag), armor);
			for(int j = 1; j < profile.size(); ++j)
			{
				const ProfilePoint p = profile.at(j - 1), q = profile.at(j);
				if(!p.strip) continue;
				const Material material = HullMaterial(p.strip);
				const QVector3D a0 = Position(a + na * p.offset, deck * DeckHeight + p.height);
				const QVector3D b0 = Position(b + nb * p.offset, deck * DeckHeight + p.height);
				const QVector3D a1 = Position(a + na * q.offset, deck * DeckHeight + q.height);
				const QVector3D b1 = Position(b + nb * q.offset, deck * DeckHeight + q.height);
				out.Quad(material, deck, false, a0, b0, b1, a1);
				out.Triangle(material, deck, true, a0, b1, b0, {0, 0}, {1, 1}, {1, 0});
				out.Triangle(material, deck, true, a0, a1, b1, {0, 0}, {0, 1}, {1, 1});
			}
			// RenderWallEndCap closes both room profiles to the baseline. Its
			// interior-facing strip uses the end-cap tile and height-scaled V.
			if(Type(edge.tag) != Type(next.tag))
			{
				const QVector<ProfilePoint> adjacent = WallProfile(Type(next.tag), armor);
				const auto cap = [&](const QVector<ProfilePoint>& points, bool reverse)
				{
					for(int j = 1; j < points.size(); ++j)
					{
						const ProfilePoint p = points.at(j - 1), q = points.at(j);
						if(p.height == q.height || (p.offset == 0 && q.offset == 0)) continue;
						const QVector3D quad[4] = {Position(b, deck * DeckHeight + p.height),
							Position(b + nb * p.offset, deck * DeckHeight + p.height),
							Position(b + nb * q.offset, deck * DeckHeight + q.height),
							Position(b, deck * DeckHeight + q.height)};
						const QVector2D uv[4] = {{0, float(p.height / 8)}, {1, float(p.height / 8)},
							{1, float(q.height / 8)}, {0, float(q.height / 8)}};
						for(bool interior : {false, true})
						{
							const int order[4] = {0, (reverse != interior) ? 3 : 1, 2, (reverse != interior) ? 1 : 3};
							const Material material = interior ? Material::InteriorWallEndCap : Material::HullWall;
							out.Triangle(material, deck, interior, quad[order[0]], quad[order[1]], quad[order[2]],
								uv[order[0]], uv[order[1]], uv[order[2]]);
							out.Triangle(material, deck, interior, quad[order[0]], quad[order[2]], quad[order[3]],
								uv[order[0]], uv[order[2]], uv[order[3]]);
						}
					}
				};
				cap(profile, false); cap(adjacent, true);
			}
		}
	}
}

// The four triangle samples in each original full square. Keep the diagonals
// at the original center so triangles, diamonds and concave rooms all agree
// with the floor-plan cell masks without triangulating a polygon as a fan.
const QPoint SampleOffsets[4] = {{3, 2}, {2, 3}, {1, 2}, {2, 1}};
const QPoint Corners[4] = {{4, 0}, {4, 4}, {0, 4}, {0, 0}};

void DeckSurfaces(Builder& out, const Raster& raster, int deck, int armor, const QVector<Detail>& details)
{
	const QVector<Hatch> hatches = Hatches(details, deck);
	DetailGrid wallDetails;
	QSet<quint32> wallPositions;
	for(const Detail& detail : details)
	{
		const int type = detail.detailType;
		if(detail.deck != deck || (type != 3 && type != 5 && type != 9 && type != 49 && type != 50
			&& type != 53 && type != 54 && type != 55)) continue;
		const quint32 id = (quint32(type) << 16) | detail.packedXY;
		if(wallPositions.contains(id)) continue;
		wallPositions.insert(id);
		const QPointF p = DecodePoint(detail.packedXY);
		const QPoint grid(int(std::round(p.x() / RasterSpacing)) + RasterOrigin,
			int(std::round(p.y() / RasterSpacing)) + RasterOrigin);
		wallDetails[PointKey(grid)].append(detail);
	}
	QHash<quint32, QVector<Hatch>> hatchCells;
	const QVector<Hatch> noHatches;
	for(const Hatch& hatch : hatches)
	{
		const QRectF bounds = hatch.boundary.boundingRect();
		const int xmin = int(std::floor((bounds.left() / RasterSpacing + RasterOrigin) / 4)) * 4;
		const int xmax = int(std::floor((bounds.right() / RasterSpacing + RasterOrigin) / 4)) * 4;
		const int ymin = int(std::floor((bounds.top() / RasterSpacing + RasterOrigin) / 4)) * 4;
		const int ymax = int(std::floor((bounds.bottom() / RasterSpacing + RasterOrigin) / 4)) * 4;
		for(int x = xmin; x <= xmax; x += 4)
			for(int y = ymin; y <= ymax; y += 4) hatchCells[PointKey(QPoint(x, y))].append(hatch);
	}
	QVector<Edge> hull;
	QVector<Edge> partitions;
	for(int x = 0; x + 4 < RasterSize; x += 4)
		for(int y = 0; y + 4 < RasterSize; y += 4)
			for(int t = 0; t < 4; ++t)
			{
				const QPoint origin(x, y), sample = origin + SampleOffsets[t];
				const quint16 tag = raster.At(sample.x(), sample.y());
				const int owner = raster.Owner(sample.x(), sample.y());
				if(!tag) continue;
				const QPoint points[3] = {origin + QPoint(2, 2), origin + Corners[t], origin + Corners[(t + 1) % 4]};
				const bool window = WindowFloor(Type(tag), armor);
				const auto plane = [&](Material material, int planeDeck, bool inside, bool up)
				{
					const auto cell = hatchCells.constFind(PointKey(origin));
					Plane(out, points, x, y, deck * DeckHeight, material, planeDeck, inside, up,
						cell == hatchCells.constEnd() ? noHatches : cell.value());
				};
				if(tag & 0x8000)
				{
					const Material material = window ? Material::HullTopBottomWindow : Material::HullTop;
					plane(material, deck - 1, false, true);
					plane(material, deck - 1, true, false);
					continue;
				}
				if(!(tag & 0x4000))
				{
					plane(window ? Material::HullTopBottomWindow : Material::HullBottom, deck, false, false);
					if(window) plane(Material::HullTopBottomWindow, deck, true, true);
				}
				if(!(tag & 0x2000))
				{
					if(!window || (tag & 0x4000)) plane(Material::InteriorFloor, deck, true, true);
					if(tag & 0x4000) plane(Material::InteriorCeiling, deck - 1, true, false);
				}
				for(int side = 0; side != 3; ++side)
				{
					const QPoint a = points[side], b = points[(side + 1) % 3];
					QPoint neighbor;
					if(side == 0) neighbor = origin + SampleOffsets[(t + 3) % 4];
					else if(side == 2) neighbor = origin + SampleOffsets[(t + 1) % 4];
					else neighbor = sample + QPoint((t == 0 ? 2 : t == 2 ? -2 : 0), (t == 1 ? 2 : t == 3 ? -2 : 0));
					const quint16 other = raster.At(neighbor.x(), neighbor.y());
					const int adjacentOwner = raster.Owner(neighbor.x(), neighbor.y());
					if(!Current(other)) hull.append({a, b, tag});
					else if(owner != adjacentOwner && HasInterior(Type(tag)))
						partitions.append({a, b, tag, other, owner, adjacentOwner});
				}
			}
	HullWalls(out, hull, deck, armor, wallDetails);
	for(const Edge& edge : MergeEdges(partitions))
		for(const WallSpan& span : WallSpans(edge, false, wallDetails))
		{
			if(span.detail == 55) continue;
			const double length = Length(span.b - span.a);
			Material material = InteriorWall(Type(edge.tag), length);
			switch(span.detail)
			{
			case 5: material = Material::InteriorDoor; break;
			case 49:
			case 50: material = Material::CrewDoor; break;
			case 53: material = Material::LargeInteriorDoor; break;
			case 54: material = Material::LargeCrewDoor; break;
			}
			out.Quad(material, deck, true,
				Position(span.b, deck * DeckHeight), Position(span.a, deck * DeckHeight),
				Position(span.a, (deck + 1) * DeckHeight), Position(span.b, (deck + 1) * DeckHeight),
				span.detail || length <= 5 + Epsilon ? 1 : float(std::ceil(length / 10)));
		}
}
}

Mesh BuildMesh(const Design& design)
{
	Builder builder;
	QHash<int, QVector<int>> byDeck;
	QVector<QPolygonF> boundaries(design.rooms.size());
	int lowest = MaximumDeck + 1, highest = MinimumDeck - 1;
	for(int i = 0; i < design.rooms.size(); ++i)
	{
		const Room& room = design.rooms.at(i);
		if(room.deck < MinimumDeck || room.deck > MaximumDeck) continue;
		if(room.roomType == 0 || room.roomType == 12 || room.roomType > 24)
		{
			builder.Warning(QStringLiteral("Room %1 has an unsupported spacecraft room type (%2).")
				.arg(room.roomId).arg(room.roomType));
			continue;
		}
		if(room.roomType >= 22 && room.roomType <= 24)
		{
			builder.Warning(QStringLiteral("Building roof geometry (room types 22–24) is not yet recovered."));
			continue;
		}
		QString error;
		const QPolygonF boundary = RoomBoundary(room, &error);
		if(!Geometry::IsSupportedBoundary(boundary, &error))
		{
			builder.Warning(QStringLiteral("Room %1: %2").arg(room.roomId).arg(error));
			continue;
		}
		double area = 0, perimeterSteps = 0;
		QPointF previous = boundary.last();
		for(const QPointF& p : boundary)
		{
			area += Cross(previous, p);
			perimeterSteps += std::max(std::abs(p.x() - previous.x()), std::abs(p.y() - previous.y())) / RasterSpacing;
			previous = p;
		}
		if(std::abs(area) < Epsilon || perimeterSteps > 65536)
		{
			builder.Warning(QStringLiteral("Room %1 has a degenerate or excessively complex boundary.").arg(room.roomId));
			continue;
		}
		boundaries[i] = boundary;
		byDeck[room.deck].append(i);
		lowest = std::min(lowest, int(room.deck)); highest = std::max(highest, int(room.deck));
	}
	Raster raster;
	for(int deck = lowest; deck <= highest + 1 && !builder.truncated; ++deck)
	{
		for(int index : byDeck.value(deck))
		{
			const Room& room = design.rooms.at(index);
			raster.Fill(boundaries.at(index), room.roomType, room.roomSubtype, index);
		}
		DeckSurfaces(builder, raster, deck, std::min(int(design.header.armor), 10), design.details);
		raster.NextDeck();
	}
	if(builder.truncated) builder.Warning(QStringLiteral("The render preview exceeded its geometry limit and was truncated."));
	for(const Detail& detail : design.details)
		if(!IsHatch(detail.detailType) && detail.detailType != 0 && detail.detailType != 76
			&& detail.detailType != 3 && detail.detailType != 5 && detail.detailType != 9
			&& detail.detailType != 49 && detail.detailType != 50 && detail.detailType != 53
			&& detail.detailType != 54 && detail.detailType != 55)
		{
			builder.Warning(QStringLiteral("Equipment models, turrets, railings, ladders and stairs are not yet recovered in the 3D preview."));
			break;
		}
	return builder.mesh;
}
}
}
