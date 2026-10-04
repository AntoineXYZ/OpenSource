#include "aulegacyshipequipment.h"
#include "aulegacyshipdetails.h"

#include <QHash>
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace AuLegacyShip
{
namespace Rendering
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
constexpr float HalfDiagonal = 1.41421356225f;
constexpr int MaximumEquipmentVertices = 6000000;

struct Ring
{
	float major, minor, height;
};

// Each pair below is copied from the original RenderDriveUnit/RenderShield/
// RenderWeapon/RenderLifeSupport point tables, rather than a regular cylinder.
QVector3D RingPoint(const Ring& ring, int index)
{
	const float a = ring.major, b = ring.minor, z = ring.height;
	const QVector3D points[] = {{a, -b, z}, {a, b, z}, {b, a, z}, {-b, a, z},
		{-a, b, z}, {-a, -b, z}, {-b, -a, z}, {b, -a, z}};
	return points[index & 7];
}

QVector2D CapUV(int index)
{
	const float low = .5f - .382683432f * .5f, high = 1 - low;
	const QVector2D uv[] = {{1, low}, {1, high}, {high, 1}, {low, 1},
		{0, high}, {0, low}, {low, 0}, {high, 0}};
	return uv[index & 7];
}

class EquipmentBuilder
{
public:
	explicit EquipmentBuilder(Mesh& result) : mesh(result)
	{
		for(const Batch& batch : mesh.batches) vertexCount += batch.vertices.size();
		bounded = vertexCount != 0;
	}

	void DetailTransform(const Detail& detail)
	{
		const QPointF xy = DecodePoint(detail.packedXY);
		origin = QVector3D(float(xy.x()), float(xy.y()), detail.deck * float(DeckHeight));
		const float angle = float(DetailRotationStep(detail.detailType) * Pi / 4);
		cosine = std::cos(angle); sine = std::sin(angle);
		deck = detail.deck;
	}

	int BatchIndex(Material material, int effect = -1, int frameCount = 0, bool twoSided = false)
	{
		const quint64 key = quint64(quint16(int(material) + 1))
			| (quint64(quint8(effect + 1)) << 16) | (quint64(quint8(deck + 64)) << 24)
			| (quint64(twoSided) << 32);
		const auto found = batches.constFind(key);
		if(found != batches.constEnd()) return found.value();
		Batch batch;
		batch.material = material;
		batch.deck = deck;
		batch.interior = true;
		batch.equipment = true;
		batch.twoSided = twoSided;
		batch.effectFirstFrame = effect;
		batch.effectFrameCount = frameCount;
		const int index = mesh.batches.size();
		mesh.batches.append(batch);
		batches.insert(key, index);
		return index;
	}

	QVector3D Rotate(const QVector3D& p) const
	{
		return QVector3D(cosine * p.x() - sine * p.y(),
			sine * p.x() + cosine * p.y(), p.z());
	}

	QVector3D Transform(const QVector3D& p) const { return origin + Rotate(p); }

	void Triangle(int batch, QVector3D a, QVector3D b, QVector3D c,
		QVector2D ua, QVector2D ub, QVector2D uc, const QVector3D& outward)
	{
		if(vertexCount + 3 > MaximumEquipmentVertices) { truncated = true; return; }
		if(QVector3D::dotProduct(QVector3D::crossProduct(b - a, c - a), outward) < 0)
		{ std::swap(b, c); std::swap(ub, uc); }
		a = Transform(a); b = Transform(b); c = Transform(c);
		// The 2016 machines deliberately use horizontal side normals even
		// along their tapered sections; retain those explicit ModelAdd normals.
		const QVector3D normal = Rotate(outward).normalized();
		if(normal.isNull()) return;
		const Vertex vertices[] = {{a, normal, ua}, {b, normal, ub}, {c, normal, uc}};
		for(const Vertex& v : vertices)
		{
			mesh.batches[batch].vertices.append(v);
			if(!bounded) { mesh.minimum = mesh.maximum = v.position; bounded = true; }
			else for(int i = 0; i < 3; ++i)
			{
				mesh.minimum[i] = std::min(mesh.minimum[i], v.position[i]);
				mesh.maximum[i] = std::max(mesh.maximum[i], v.position[i]);
			}
		}
		vertexCount += 3;
	}

	void Quad(int batch, const QVector3D& a, const QVector3D& b,
		const QVector3D& c, const QVector3D& d, const QVector3D& outward,
		const QVector2D& ua = {0, 0}, const QVector2D& ub = {0, 1},
		const QVector2D& uc = {1, 1}, const QVector2D& ud = {1, 0})
	{
		Triangle(batch, a, b, c, ua, ub, uc, outward);
		Triangle(batch, a, c, d, ua, uc, ud, outward);
	}

	void Octagon(Material material, int effect, int frameCount,
		std::initializer_list<Ring> profile, bool top = true, bool bottom = true,
		bool twoSided = false)
	{
		const int batch = BatchIndex(material, effect, frameCount, twoSided);
		const Ring* previous = profile.begin();
		for(const Ring* ring = previous + 1; ring != profile.end(); previous = ring++)
			for(int i = 0; i < 8; ++i)
			{
				const QVector3D a = RingPoint(*previous, i), b = RingPoint(*ring, i);
				const QVector3D c = RingPoint(*ring, i + 1), d = RingPoint(*previous, i + 1);
				Quad(batch, a, b, c, d, QVector3D(a.x() + d.x(), a.y() + d.y(), 0));
			}
		// Original caps are three quads. Retain that split: the deliberately
		// nongeometric minor UV (.382683432) is not linear across an eight-fan cap.
		const int indices[][4] = {{4, 3, 2, 1}, {1, 0, 5, 4}, {0, 7, 6, 5}};
		for(int side = 0; side < 2; ++side)
		{
			if(side ? !top : !bottom) continue;
			const Ring& ring = side ? *(profile.end() - 1) : *profile.begin();
			for(const auto& q : indices)
				Quad(batch, RingPoint(ring, q[0]), RingPoint(ring, q[1]),
					RingPoint(ring, q[2]), RingPoint(ring, q[3]), {0, 0, side ? 1.f : -1.f},
					CapUV(q[0]), CapUV(q[1]), CapUV(q[2]), CapUV(q[3]));
		}
	}

	void Sensor()
	{
		const float a = HalfDiagonal;
		const int side = BatchIndex(Material::Sensor, 24, 3);
		const QVector3D corners[] = {{a, -a, .1f}, {a, a, .1f}, {-a, a, .1f}, {-a, -a, .1f}};
		for(int i = 0; i < 4; ++i)
		{
			const QVector3D p = corners[i], q = corners[(i + 1) & 3], up(0, 0, 7.8f);
			Quad(side, p, p + up, q + up, q, {p.x() + q.x(), p.y() + q.y(), 0});
		}
		const int cap = BatchIndex(Material::SensorTop);
		Quad(cap, {-a, -a, 7.9f}, {-a, a, 7.9f}, {a, a, 7.9f}, {a, -a, 7.9f}, {0, 0, 1});
		Quad(cap, {-a, -a, .1f}, {-a, a, .1f}, {a, a, .1f}, {a, -a, .1f}, {0, 0, -1});
	}

	void Berth(float height)
	{
		// RenderBerthSingle 1804117d0 and RenderBerthDouble 180410490.
		const float x = -HalfDiagonal, X = x + float(GridSpacing);
		const float y = -HalfDiagonal, Y = HalfDiagonal, z = height, Z = z + 1;
		const int sides = BatchIndex(Material::Berth, 0, 4);
		Quad(sides, {X, y, z}, {X, y, Z}, {X, Y, Z}, {X, Y, z}, {1, 0, 0},
			{0, .5f}, {0, .96875f}, {.2421875f, .96875f}, {.2421875f, .5f});
		Quad(sides, {x, Y, z}, {x, Y, Z}, {x, y, Z}, {x, y, z}, {-1, 0, 0},
			{0, 0}, {0, .46875f}, {.2421875f, .46875f}, {.2421875f, 0});
		Quad(sides, {X, Y, z}, {X, Y, Z}, {x, Y, Z}, {x, Y, z}, {0, 1, 0},
			{.9921875f, 0}, {.9921875f, .46875f}, {.2578125f, .46875f}, {.2578125f, 0});
		Quad(sides, {x, y, z}, {x, y, Z}, {X, y, Z}, {X, y, z}, {0, -1, 0},
			{.2578125f, 0}, {.2578125f, .46875f}, {.9921875f, .46875f}, {.9921875f, 0});
		const int top = BatchIndex(Material::Berth);
		Quad(top, {x, y, Z}, {x, Y, Z}, {X, Y, Z}, {X, y, Z}, {0, 0, 1},
			{.2578125f, .5f}, {.2578125f, .96875f}, {.9921875f, .96875f}, {.9921875f, .5f});
		Quad(BatchIndex(Material::BerthBottom), {x, Y, z}, {x, y, z}, {X, y, z}, {X, Y, z}, {0, 0, -1});
	}

	void Ladder()
	{
		// RenderLadder 180440a90: the texture supplies the rungs; the two
		// narrow perpendicular planes supply the rail thickness.
		--deck;
		const float x = 1.834f, y = .9428090415f, pole = .7807637374921875f;
		Quad(BatchIndex(Material::Ladder, -1, 0, true), {x, -y, -8}, {x, -y, 0},
			{x, y, 0}, {x, y, -8}, {1, 0, 0});
		const int rails = BatchIndex(Material::LadderPole, -1, 0, true);
		for(int sign : {-1, 1})
			Quad(rails, {x - .15f, sign * pole, -8}, {x - .15f, sign * pole, 0},
				{x + .15f, sign * pole, 0}, {x + .15f, sign * pole, -8}, {0, float(sign), 0});
		++deck;
	}

	Mesh& mesh;
	QHash<quint64, int> batches;
	QVector3D origin;
	float cosine = 1, sine = 0;
	int deck = 0, vertexCount = 0;
	bool bounded = false, truncated = false;
};

bool IsHatchType(int type)
{
	return type == 8 || type == 51 || type == 52 || (type >= 77 && type <= 97);
}
}

bool EquipmentSupported(int type)
{
	if(IsHatchType(type) || (type >= 25 && type <= 31) || (type >= 65 && type <= 72)
		|| (type >= 33 && type <= 40) || (type >= 101 && type <= 140)) return true;
	switch(type)
	{
	case 2: case 3: case 5: case 9: case 10: case 11: case 14: case 16:
	case 18: case 20: case 21: case 22: case 23: case 24: case 32: case 49: case 50:
	case 53: case 54: case 55: case 56: return true;
	// The original 3D dispatch explicitly skips these logical floor-plan
	// locations (RenderInteriorNoModel 18043acba and 18043acd3).
	case 76: case 98: case 99: case 141: return true;
	default: return false;
	}
}

void AppendEquipment(const Design& design, Mesh& mesh)
{
	EquipmentBuilder out(mesh);
	for(const Detail& detail : design.details)
	{
		if(out.truncated) break;
		if(detail.hiddenRaw || detail.deck < MinimumDeck || detail.deck > MaximumDeck) continue;
		out.DetailTransform(detail);
		const int type = detail.detailType;
		if(IsHatchType(type)) out.Ladder();
		else if(type == 2 || (type >= 25 && type <= 31)) { out.Berth(.5f); out.Berth(4); }
		else if(type >= 65 && type <= 72) out.Berth(.5f);
		else switch(type)
		{
		case 10: case 14: case 24:
			out.Octagon(type == 10 ? Material::GravityDrive : type == 14 ? Material::PowerPlant : Material::FTLDrive,
				type == 10 ? 16 : type == 14 ? 20 : 55, 4,
				{{HalfDiagonal, .5857864367071931f, .1f}, {HalfDiagonal, .5857864367071931f, 7.9f}});
			break;
		case 11:
			out.Octagon(Material::Surgery, 40, 3,
				{{2.2627416996f, .9372582987315091f, 0}, {2.2627416996f, .9372582987315091f, 8}}, false, false, true);
			break;
		case 16: out.Sensor(); break;
		case 18:
			out.Octagon(Material::Shield, 27, 4,
				{{1.488645855f, .6166173017970454f, .1f}, {.9229604301f, .3823027271141682f, 3.95f},
				{1.488645855f, .6166173017970454f, 7.8f}});
			break;
		case 20:
			out.Octagon(Material::TransporterPad, 47, 4,
				{{1.3468700592857143f, .5578918444830411f, 1.f / 24},
				{1.3468700592857143f, .5578918444830411f, 1.f / 6}}, true, false);
			break;
		case 23:
			out.Octagon(Material::WeaponBay, 51, 4,
				{{.9229604301f, .3823027271141682f, .1f}, {1.488645855f, .6166173017970454f, 3.95f},
				{.9229604301f, .3823027271141682f, 7.8f}});
			break;
		case 32:
			out.Octagon(Material::LifeSupport, 8, 4,
				{{.9229604601860046f, .38230273127555847f, .10000000149011612f},
				{1.4886459112167358f, .6166173219680786f, 2.0250000953674316f},
				{.9229604601860046f, .38230273127555847f, 7.800000190734863f}});
			break;
		}
	}
	if(out.truncated && !mesh.warnings.contains(QStringLiteral("The equipment preview exceeded its geometry limit and was truncated.")))
		mesh.warnings.append(QStringLiteral("The equipment preview exceeded its geometry limit and was truncated."));
}
}
}
