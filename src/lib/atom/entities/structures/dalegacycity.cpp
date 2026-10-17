#include "dalegacycity.h"
#include "auatomrenderinfo.h"
#include "aucityreport.h"
#include "aubox.h"
#include "auglobal.h"
#include "aulegacybd.h"
#include "aulegacybiodome.h"
#include "aulegacybuildinggeometry.h"
#include "aulegacycitycatalogue.h"
#include "aulegacyconstructionmaterials.h"
#include "augear.h"
#include "auitem.h"
#include "auterraininfo.h"
#include "auzoneresource.h"
#include "deprocesscomponent.h"
#include "aulegacyshipequipment.h"
#include "aulegacysccollision.h"
#include "aunoise.h"
#include "autriangle.h"
#include "daanimal.h"
#include "dacreature.h"
#include "daatomlist.h"
#include "daperson.h"
#include "dgdata.h"
#include "daworldll.h"
#include "daworldsphere.h"
#include <QDataStream>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
AuPoint Point(const QVector3D& p) { return AuPoint(p.x(), p.y(), p.z()); }

// DpGlobeRoad::Contains, AuAtom13 0x652cf0. The visible pavement's
// eight-foot border is deliberately absent from the placement footprint.
bool RoadContains(const DaWorldLL& world, const AuLegacyCity::Development& road,
	const AuGlobeLLF& start, const AuGlobeLLF& end, const AuGlobeLLF& point, double margin)
{
	float fraction;
	const auto closest = point.CoordOnSegment(start, end, &fraction, world.LLShape());
	const float startAltitude = float(road.roadStartAltitudeMSL), endAltitude = float(road.roadEndAltitudeMSL);
	const float altitude = fraction <= 0 ? startAltitude : fraction >= 1 ? endAltitude
		: (endAltitude - startAltitude) * fraction + startAltitude;
	const AuPoint endpoint = world.LocOfLL(fraction <= 0 ? start : fraction >= 1 ? end : closest, altitude);
	const AuPoint query = world.LocOfLL(point, altitude);
	const float halfWidth = float(road.roadWidthMeters * FeetPerMeter() / 2);
	if(fraction >= 0 && fraction <= 1)
	{
		const float width = float(halfWidth + margin);
		return float(query.Distance2(endpoint)) <= width * width;
	}
	if(margin <= 0) return false;
	const AuPoint first = world.LocOfLL(start, startAltitude), last = world.LocOfLL(end, endAltitude);
	const double beyond = (fraction < 0 ? -double(fraction) : double(fraction) - 1) * first.Distance(last);
	if(beyond > margin) return false;
	AuPoint direction = last - first;
	if(direction.IsZero()) direction.x = 1; else direction.Unit();
	AuPoint side = direction.Cross(-world.GravityDir(endpoint)); side.Unit();
	const AuPoint delta = query - endpoint;
	const double lateral = delta.x * side.x + delta.y * side.y + delta.z * side.z;
	if(std::abs(lateral) <= halfWidth + margin) return true;
	const AuPoint edge = endpoint + side * (lateral > 0 ? halfWidth : -halfWidth);
	return query.Distance2(edge) <= double(float(margin * margin));
}

bool BuildingTouchesRoad(const DaWorldLL& world, const AuLegacyCity::Development& building,
	const AuMatrix4x4& placement, double radius, const AuLegacyCity::Development& road,
	const AuMatrix4x4& roadPlacement)
{
	AuGlobeLLF start, end;
	const double halfLength = road.roadLengthMeters * FeetPerMeter() / 2;
	if(!world.LLOfLoc(start, roadPlacement.Project(AuPoint(0, -halfLength, 0)))
		|| !world.LLOfLoc(end, roadPlacement.Project(AuPoint(0, halfLength, 0)))) return false;
	// DaCity::DevelopmentContaining 0x28c050: an expanded center query,
	// then 32 square-boundary samples with no expansion (0x28c354-0x28c3ad).
	if(RoadContains(world, road, start, end, building.location, radius)) return true;
	for(int i = 0; i < 32; ++i)
	{
		const float angle = float(i) * 6.2831855f * .03125f;
		const float x = std::cos(angle), y = std::sin(angle), scale = qMax(std::abs(x), std::abs(y));
		AuGlobeLLF ll;
		if(world.LLOfLoc(ll, placement.Project(AuPoint(radius * (x / scale), radius * (y / scale), 0)))
			&& RoadContains(world, road, start, end, ll, 0)) return true;
	}
	return false;
}

bool RoadTouchesBuilding(const DaWorldLL& world, const AuLegacyCity::Development& road,
	const AuMatrix4x4& placement, const AuLegacyCity::Development& building,
	const AuMatrix4x4& buildingPlacement, double radius)
{
	// LockTestBuildLocation 0x5264d0 checks center and both edges
	// every fifteen feet, including the final endpoint, against DpBuilding::Contains.
	const double halfLength = road.roadLengthMeters * FeetPerMeter() / 2;
	AuGlobeLLF startLL, endLL;
	world.LLOfLoc(startLL, placement.Project(AuPoint(0, -halfLength, 0)));
	world.LLOfLoc(endLL, placement.Project(AuPoint(0, halfLength, 0)));
	const AuPoint start = world.LocOfLL(startLL, road.roadStartAltitudeMSL);
	const AuPoint end = world.LocOfLL(endLL, road.roadEndAltitudeMSL), direction = end - start;
	const double length = direction.Length(); if(length <= 0) return false;
	const auto inverse = buildingPlacement.Inverse();
	for(double along = 0;; along = qMin(length, along + 15))
	{
		AuPoint center = start + direction * (along / length);
		world.ElevateLocMSL(center, road.roadStartAltitudeMSL
			+ (road.roadEndAltitudeMSL - road.roadStartAltitudeMSL) * along / length);
		AuPoint side = direction.Cross(-world.GravityDir(center)); side.Unit(road.roadWidthMeters * FeetPerMeter() / 2);
		for(int edge : {0, -1, 1})
		{
			AuGlobeLLF ll; if(!world.LLOfLoc(ll, center + side * edge)) continue;
			const AuPoint local = inverse.Project(world.LocOfLL(ll, building.altitudeMSL));
			if(qMax(float(std::abs(local.x)), float(std::abs(local.y))) <= radius) return true;
		}
		if(along >= length) return false;
	}
}

double TerrainLevelRadius(quint8 industry, const AuLegacyCityCatalogue& catalogue, quint8 constructionProcess = 0)
{
	// DpDevelopment::TerrainLevelRadius, AuAtom13 0xfe610. Default BuildRadius
	// uses the construction recipe's meter byte (AuData13 0x2c4e0).
	switch(industry)
	{
	case 20: return 26.24672f;
	case 30: case 32: return 42.650917f;
	case 31: case 50: case 51: return 32.8084f;
	case 66: case 67: return 49.2126f;
	case 113: return 65.6168f;
	case 114: return 9.84252f;
	default: break;
	}
	if(constructionProcess)
	{
		const auto* process = catalogue.ConstructionProcess(AuLegacyCity::ConstructionProcessId(constructionProcess));
		return process && process->industry.value == industry ? double(float(process->buildRadius * FeetPerMeter())) : -1;
	}
	int meters = -1;
	for(const auto& process : catalogue.ConstructionProcesses())
		if(process.industry.value == industry)
		{
			if(meters >= 0 && meters != process.buildRadius) return -1;
			meters = process.buildRadius;
		}
	return meters < 0 ? -1 : double(float(meters * FeetPerMeter()));
}

bool SameTerrainSites(const QVector<AuLegacyCity::Development>& a, const QVector<AuLegacyCity::Development>& b)
{
	if(a.size() != b.size()) return false;
	for(int i = 0; i < a.size(); ++i)
	{
		const auto& p = a[i]; const auto& q = b[i];
		if(p.industryId != q.industryId || p.constructionProcessId != q.constructionProcessId || p.materialId != q.materialId || p.location != q.location
			|| p.altitudeMSL != q.altitudeMSL || p.facingRadians != q.facingRadians
			|| p.constructed != q.constructed || p.demolished != q.demolished || p.roadSlab != q.roadSlab
			|| p.components.isEmpty()!=q.components.isEmpty()
			|| p.roadLengthMeters != q.roadLengthMeters || p.roadWidthMeters != q.roadWidthMeters
			|| p.roadStartAltitudeMSL != q.roadStartAltitudeMSL || p.roadEndAltitudeMSL != q.roadEndAltitudeMSL
			|| p.biodomeRadiusMeters != q.biodomeRadiusMeters) return false;
	}
	return true;
}

bool SameGeometry(const QVector<AuLegacyCity::Development>& a, const QVector<AuLegacyCity::Development>& b)
{
	if(!SameTerrainSites(a,b)) return false;
	for(int i=0;i<a.size();++i)
		if(a[i].id!=b[i].id || a[i].designId!=b[i].designId || a[i].constructionProcessId!=b[i].constructionProcessId
			|| a[i].minimumDeck!=b[i].minimumDeck || a[i].maximumDeck!=b[i].maximumDeck
			|| a[i].completedLevels!=b[i].completedLevels) return false;
	return true;
}

const AuLegacyBd::Paths::PathableCell* PathCell(const AuLegacyBd::Paths::Graph& graph, const AuPoint& location)
{
	const double spacing = AuLegacyShip::CoordinateSpacing;
	if(!std::isfinite(location.x) || !std::isfinite(location.y) || !std::isfinite(location.z)
		|| std::abs(location.x) > 127*spacing || std::abs(location.y) > 127*spacing) return nullptr;
	const double level = std::floor((location.z + .05) / AuLegacyBd::DeckHeight);
	if(level < -127 || level > 127) return nullptr;
	const int deck = int(level);
	const quint32 key = (quint32(quint16(qint16(deck))) << 16)
		| (quint32(int(std::round(location.x / spacing)) + 128) << 8)
		| quint32(int(std::round(location.y / spacing)) + 128);
	const auto found = graph.PathableCells().constFind(key);
	return found == graph.PathableCells().cend() ? nullptr : &found.value();
}

bool ReadEnvelope(QDataStream& stream, QByteArray& bytes)
{
	quint32 size = 0;
	stream >> size;
	if(stream.status() != QDataStream::Ok || size > AuLegacyCity::MaximumStateBytes
		|| !stream.device() || size > quint64(stream.device()->bytesAvailable()))
	{ stream.setStatus(QDataStream::ReadCorruptData); return false; }
	bytes.resize(int(size));
	if(stream.readRawData(bytes.data(), int(size)) != int(size))
	{ stream.setStatus(QDataStream::ReadPastEnd); return false; }
	return true;
}

void WriteEnvelope(QDataStream& stream, const QByteArray& bytes)
{
	stream << quint32(bytes.size());
	if(!bytes.isEmpty()) stream.writeRawData(bytes.constData(), bytes.size());
}

bool ValidateDesigns(const AuLegacyCity::State& state, QString& error)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) { error = QStringLiteral("Legacy catalogue has not been initialized."); return false; }
	for(const auto& dev : state.developments)
	{
		if(dev.constructionProcessId)
		{
			const auto* process = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId));
			if(!process || process->industry.value != dev.industryId)
			{ error = QStringLiteral("A legacy construction process disagrees with its industry."); return false; }
			if(dev.completedLevels>32 || dev.targetLevels<dev.completedLevels)
			{ error=QStringLiteral("Legacy construction level counts disagree."); return false; }
		}
		for(const auto& component : dev.components)
			if(!catalogue->Commodity(AuLegacyCity::CommodityId(component.commodityId)))
			{ error = QStringLiteral("A legacy construction component is absent from the original catalogue."); return false; }
		if(!dev.designId && (dev.industryId == 1 || dev.industryId == 2 || dev.industryId == 10
			|| dev.industryId == 114 || dev.industryId == 129))
		{
			if((AuLegacyBd::Geometry::MaterialAtlas(dev.materialId).isEmpty() && !(dev.industryId==2 && dev.materialId==23))
				|| (dev.industryId==129 && !AuLegacyCityCatalogue::BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter())))
			{ error=QStringLiteral("A procedural legacy development has an invalid material or size."); return false; }
			continue;
		}
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(dev.designId);
		if(!design || design->header.industryId != dev.industryId
			|| AuLegacyBd::Geometry::MaterialAtlas(dev.materialId).isEmpty())
		{ error = QStringLiteral("Legacy development %1 has an incompatible design, industry or material.").arg(dev.id); return false; }
	}
	return true;
}

const AuLegacyCity::ConstructionProcessRecord* ConstructionRecipe(quint8 industry, quint8 process = 0)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) return nullptr;
	if(process)
	{
		const auto* recipe = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(process));
		return recipe && recipe->industry.value == industry ? recipe : nullptr;
	}
	return catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(industry));
}

int MaximumDesignDeck(quint8 industry, quint16 material)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	return catalogue ? catalogue->BuildingDesignMaximumDeck(AuLegacyCity::IndustryId(industry),AuLegacyCity::CommodityId(material)) : 0;
}

void ConstructedDecks(AuLegacyCity::Development& dev)
{
	// DpBuilding::ConstructedDecks, 0x59cb20. Level counts have different
	// geometry meanings for mines, wells and fixed-shape industrial buildings.
	dev.minimumDeck = dev.industryId == 11 || dev.industryId == 12 ? -5
		: dev.industryId == 50 ? -8 : dev.industryId == 51 ? -1 : 0;
	dev.maximumDeck = qint8(MaximumDesignDeck(dev.industryId, dev.materialId));
	const auto* recipe = ConstructionRecipe(dev.industryId, dev.constructionProcessId);
	if(recipe && recipe->stackLimit == 32) dev.minimumDeck = qint8(-2 * int(dev.completedLevels));
	else if(dev.industryId == 51 && dev.completedLevels <= 1) dev.minimumDeck = 0;
	else if(recipe && recipe->stackLimit > 0 && recipe->stackLimit < 32)
		dev.maximumDeck = qint8(qMin(int(dev.maximumDeck), qMax(0, int(dev.completedLevels) - 1)));
	dev.constructed = dev.completedLevels > 0 && !dev.demolished;
}

void InitializeConstruction(AuLegacyCity::Development& dev, quint32 labor, quint8 environment)
{
	dev.components.clear(); dev.laborRemaining = qMin(labor, quint32(65535));
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) return;
	const quint32 pavement = dev.industryId == 2 ? catalogue->RoadPavementQuantity(dev.roadLengthMeters*FeetPerMeter(),dev.roadWidthMeters*FeetPerMeter()) : 0;
	const quint32 multiplier=dev.industryId==129 ? quint32(catalogue->BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter())) : 1;
	for(const auto& source : catalogue->ConstructionRequirements(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),
		AuLegacyCity::CommodityId(dev.materialId),environment,pavement,multiplier))
	{
		AuLegacyCity::ConstructionComponent component;
		component.commodityId = source.commodity.value; component.effect = source.effect; component.quantity = source.quantity;
		dev.components.append(component);
	}
}

quint8 ConstructionEnvironment(const DaWorldLL& world, const AuGlobeLLF& location)
{
	AuTerrainInfo terrain; world.QueryTerrain(terrain, location);
	if(terrain.IsWater() && world.SurfaceAltitudeMSL(location)<=-32) return 4;
	return world.HasBreathableAtmosphere() ? 1 : 2;
}

quint32 SitePreparationLabor(const DaWorldLL& world, const AuLegacyCity::Development& dev)
{
	const auto* recipe = ConstructionRecipe(dev.industryId, dev.constructionProcessId);
	if(!recipe) return 1;
	if(dev.industryId==129) return quint32(AuLegacyCityCatalogue::BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter()))*10;
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians, nullptr, AuPoint(), placement);
	if(dev.industryId == 2)
	{
		// LockAddRoad 0x5253d0: sample at fifteen-foot intervals. Landscapers
		// use area; roads use the cut/fill volume against the endpoint grade.
		const double length = dev.roadLengthMeters*FeetPerMeter(), width = dev.roadWidthMeters*FeetPerMeter();
		double deviation = 0; int samples = 0;
		for(double along=0;;along=qMin(length,along+15))
		{
			AuGlobeLL sample;
			if(world.LLOfLoc(sample,placement.Project(AuPoint(0,along-length/2,0))))
			{
				const double grade = dev.roadStartAltitudeMSL+(dev.roadEndAltitudeMSL-dev.roadStartAltitudeMSL)*along/length;
				deviation += std::abs(grade-world.TerrainAltitudeMSL(sample)); ++samples;
			}
			if(along>=length) break;
		}
		return AuLegacyCityCatalogue::RoadPreparationLabor(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),
			length,width,deviation/qMax(1,samples));
	}
	unsigned int rocks = 0, shrubs = 0, trees = 0; float water = 0;
	const double radius = recipe->buildRadius * FeetPerMeter();
	world.ResourceDistributionAt(placement, placement.Translation(), AuPoint(radius, radius, 0), rocks, shrubs, trees, water, 10);
	return AuLegacyCityCatalogue::BuiltIn()->SitePreparationLabor(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),
		dev.completedLevels,rocks,shrubs,trees);
}

int ResourceLevelLimit(const DaWorldLL& world, const AuLegacyCity::Development& dev)
{
	const auto* recipe = ConstructionRecipe(dev.industryId, dev.constructionProcessId);
	if(!recipe) return 0;
	if(recipe->stackLimit < 32) return qMin(32, int(recipe->stackLimit) + 1);
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians, nullptr, AuPoint(), placement);
	unsigned int rocks = 0, shrubs = 0, trees = 0; float water = 0;
	const double radius = recipe->buildRadius * FeetPerMeter();
	world.ResourceDistributionAt(placement, placement.Translation(), AuPoint(radius, radius, 0), rocks, shrubs, trees, water, 4);
	return AuLegacyCityCatalogue::BuiltIn()->ConstructionResourceLevelLimit(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),rocks,shrubs,trees);
}

bool ConstructionAuthority(const DaLegacyCity& city, const DaPerson& person, bool owner, QString& error)
{
	if(!g_Au.IsLegacyGeneration() || !city.Parent() || person.FindParentPlanetInEnv() != city.Parent())
	{ error = QStringLiteral("Move onto this Legacy city's world first."); return false; }
	if(owner && city.EmpireId() != person.EmpireId() && !person.HasArchitectPower() && !person.HasProgrammerPower())
	{ error = QStringLiteral("Only the city's empire may change its construction projects."); return false; }
	return true;
}

int ConstructionThreat(DaWorldLL& world, const AuId& cityId, const AuLegacyCity::Development& dev, quint32 empire)
{
	// CheckWildAnimalOrEnemyCreature, AuAtom13 0x50edc0. These industries
	// limit the occupied part of the site; the others use the whole site.
	double distanceLimit=0;
	switch(dev.industryId)
	{
	case 20: distanceLimit=26.24672; break;
	case 30: case 32: distanceLimit=42.650917; break;
	case 31: case 50: case 51: distanceLimit=32.8084; break;
	case 66: case 67: distanceLimit=49.2126; break;
	case 113: distanceLimit=65.6168; break;
	case 114: distanceLimit=9.84252; break;
	default: break;
	}
	const AuPoint center=world.LocOfLL(dev.location,dev.altitudeMSL);
	DaAtomList atoms; world.ListSubatoms(atoms);
	for(auto* atom : atoms)
	{
		auto* creature=dynamic_cast<DaCreature*>(atom);
		if(!creature || (!creature->IsNormal() && !creature->IsTraveling()) || creature->CurrentHitPoints()<=0
			|| creature->IsUnderGround() || !creature->IsOnGround()) continue;
		const auto* environment=creature->LockEnvironmentQuery();
		const bool sameSite=environment->LegacyCityId()==cityId && environment->LegacyDevelopmentId()==dev.id;
		creature->UnlockEnvironmentQuery();
		if(!sameSite || (distanceLimit && creature->Location().Distance2(center)>distanceLimit*distanceLimit)) continue;
		if(dynamic_cast<DaAnimal*>(creature) && !creature->EmpireId()) return 1;
		const auto* person=dynamic_cast<DaPerson*>(creature);
		if(person && person->IsInvisibleOffLine()) continue;
		if(PoliticalStance(creature->EmpireId(),empire)==POL_Enemy) return 2;
	}
	return 0;
}

bool ReadConstructionInventory(const QByteArray& bytes, AuItemHash& inventory)
{
	if(bytes.isEmpty()) return true;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	quint32 count = 0; in >> count;
	if(in.status() != QDataStream::Ok || count > 65535 || quint64(count) * 8 > quint64(bytes.size())) return false;
	in.device()->seek(0); in >> inventory;
	return in.status() == QDataStream::Ok && in.atEnd();
}

bool ComponentsComplete(const AuLegacyCity::Development& dev)
{
	for(const auto& component : dev.components)
		if((component.effect == CE_Required || component.effect == CE_RequiredQ)
			&& component.applied < qMax(quint32(1), component.quantity)) return false;
	return true;
}
}

struct DaLegacyCity::Runtime
{
	struct Site
	{
		AuLegacyCity::Development state;
		AuMatrix4x4 placement, inverse;
		AuGlobeLLF roadStart, roadEnd;
		float radius = 0;
		float halfWidth = 0, halfLength = 0;
	};
	struct Building
	{
		AuLegacyCity::Development state;
		AuLegacyBd::Geometry::Mesh mesh;
		QSharedPointer<AuLegacyScGeometry::CollisionWorld> collision;
		AuLegacyBd::Paths::Graph paths;
	};
	struct DrawBatch
	{
		quint32 development = 0;
		AuGLVAO vao;
		GLuint texture = 0;
		bool twoSided = false;
		QVector4D tint = QVector4D(1,1,1,1);
		float selfIllumination = 0;
	};
	QMutex lock;
	quint64 revision = 0, drawRevision = 0;
	quint64 sourceRevision = 0;
	quint32 geometryEmpire = 0;
	QVector<AuLegacyCity::Development> geometryState;
	QVector<Building> buildings;
	QVector<DrawBatch> drawBatches;
	quint64 siteRevision = 0, terrainRevision = 0, terrainDrawRevision = 0;
	const DaWorldLL* siteWorld = nullptr;
	QVector<AuLegacyCity::Development> siteState;
	QVector<Site> sites, renderedSites;
	double AdjustTerrain(const DaWorldLL& world, double altitudeMSL, const AuGlobeLL& ll,
		int& levelSites, bool& hideRocks, int& hideGrass, bool& hideShrubs,
		bool& hideTrees, WORLDTEX_TYPE& pavementType) const;

	void EnsureSites(const DaLegacyCity& city, const DaWorldLL& world)
	{
		QReadLocker stateLock(&city.m_CityLock);
		if(siteWorld == &world && siteRevision == city.m_CityRevision) return;
		siteRevision = city.m_CityRevision;
		if(siteWorld == &world && SameTerrainSites(siteState, city.m_City.developments)) return;
		siteWorld = &world;
		siteState = city.m_City.developments;
		sites.clear();
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			for(const auto& dev : siteState)
			{
				if(dev.industryId==129) continue; // DpBldBiodome::AdjustAltMSL is a no-op.
				const double radius = TerrainLevelRadius(dev.industryId, *catalogue, dev.constructionProcessId);
				if(radius < 0) continue;
				Site site; site.state = dev;
				site.radius = float(radius) + 2.828427f; // InBuildingSite 0x5a0c90.
				site.halfWidth = dev.industryId == 2 ? dev.roadWidthMeters * FeetPerMeter()/2+8 : site.radius;
				site.halfLength = dev.industryId == 2 ? dev.roadLengthMeters * FeetPerMeter()/2 : site.radius;
				site.radius = qMax(site.halfWidth,site.halfLength);
				world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL),
					dev.facingRadians, nullptr, AuPoint(), site.placement);
				site.inverse = site.placement.Inverse();
				if(dev.industryId == 2)
				{
					world.LLOfLoc(site.roadStart, site.placement.Project(AuPoint(0, -site.halfLength, 0)));
					world.LLOfLoc(site.roadEnd, site.placement.Project(AuPoint(0, site.halfLength, 0)));
				}
				sites.append(site);
			}
		++terrainRevision;
	}

	void Ensure(const DaLegacyCity& city)
	{
		const auto* world=dynamic_cast<const DaWorldLL*>(city.Parent());
		if(world) EnsureSites(city,*world);
		QReadLocker stateLock(&city.m_CityLock);
		if(sourceRevision == city.m_CityRevision) return;
		const bool unchanged=sourceRevision && geometryEmpire==city.m_City.controllingEmpire
			&& SameGeometry(geometryState,city.m_City.developments);
		sourceRevision=city.m_CityRevision;
		if(unchanged)
		{
			for(int i=0;i<buildings.size();++i) buildings[i].state=city.m_City.developments[i];
			return;
		}
		geometryState=city.m_City.developments; geometryEmpire=city.m_City.controllingEmpire;
		buildings.clear();
		for(const auto& dev : city.m_City.developments)
		{
			const auto design = AuLegacyBd::Catalog::BuiltIn().Find(dev.designId);
			if(!design && dev.designId) continue; // Loader rejects unknown authored designs.
			Building building;
			building.state = dev;
			AuLegacyBd::Geometry::Options options;
			options.minimumDeck = dev.minimumDeck; options.maximumDeck = dev.maximumDeck;
			options.materialCommodity = dev.materialId; options.constructed = dev.constructed;
			building.mesh = design ? AuLegacyBd::Geometry::Build(*design, options) : AuLegacyBd::Geometry::BuildProcedural(dev);
			if(world && dev.industryId==129)
			{
				AuMatrix4x4 placement;
				world->BuildingConstructionTransform(world->LocOfLL(dev.location,dev.altitudeMSL),dev.facingRadians,nullptr,AuPoint(),placement);
				const auto project=[world,placement,dev](const QVector3D& p,double height) {
					AuGlobeLL ll;
					if(!world->LLOfLoc(ll,placement.Project(Point(p)))) return QVector3D(p.x(),p.y(),float(height));
					const AuPoint result=placement.UnProject(world->LocOfLL(ll,dev.altitudeMSL+height));
					return QVector3D(float(result.x),float(result.y),float(result.z));
				};
				const auto terrain=[this,world,placement,dev](const QVector3D& p) {
					AuGlobeLL ll; if(!world->LLOfLoc(ll,placement.Project(Point(p)))) return 0.0;
					WORLDTEX_TYPE pavement=WORLDTEX_Count; AuTime newest;
					double altitude=world->VirginTerrainAltitudeMSL(ll,pavement,newest);
					int leveled=0,grass=0; bool rocks=false,shrubs=false,trees=false;
					if(pavement==WORLDTEX_Count) altitude=AdjustTerrain(*world,altitude,ll,leveled,rocks,grass,shrubs,trees,pavement);
					return altitude-dev.altitudeMSL;
				};
				building.mesh=AuLegacyBd::Biodome::Build(dev,project,terrain);
			}
			if(world && dev.industryId==2)
			{
				AuMatrix4x4 placement;
				world->BuildingConstructionTransform(world->LocOfLL(dev.location,dev.altitudeMSL),dev.facingRadians,nullptr,AuPoint(),placement);
				bool first=true;
				for(auto& batch : building.mesh.batches) for(auto& vertex : batch.vertices)
				{
					AuGlobeLL ll;
					if(world->LLOfLoc(ll,placement.Project(Point(vertex.position))))
					{
						const AuPoint corrected=placement.UnProject(world->LocOfLL(ll,dev.altitudeMSL+vertex.position.z()));
						vertex.position=QVector3D(float(corrected.x),float(corrected.y),float(corrected.z));
					}
					if(first) { building.mesh.minimum=building.mesh.maximum=vertex.position; first=false; }
					else for(int axis=0;axis<3;++axis)
					{ building.mesh.minimum[axis]=qMin(building.mesh.minimum[axis],vertex.position[axis]); building.mesh.maximum[axis]=qMax(building.mesh.maximum[axis],vertex.position[axis]); }
				}
			}
			if(dev.demolished) building.mesh = AuLegacyBd::Geometry::Mesh();
			if(dev.industryId == 2)
			{
				building.mesh.minimum.setX(qMin(building.mesh.minimum.x(),float(-dev.roadWidthMeters*FeetPerMeter()/2)));
				building.mesh.minimum.setY(qMin(building.mesh.minimum.y(),float(-dev.roadLengthMeters*FeetPerMeter()/2)));
				building.mesh.maximum.setX(qMax(building.mesh.maximum.x(),float(dev.roadWidthMeters*FeetPerMeter()/2)));
				building.mesh.maximum.setY(qMax(building.mesh.maximum.y(),float(dev.roadLengthMeters*FeetPerMeter()/2)));
			}
			if(design && dev.constructed)
				if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
				{
					AuLegacyBd::Paths::Options pathOptions;
					pathOptions.minimumDeck = dev.minimumDeck; pathOptions.maximumDeck = dev.maximumDeck;
					pathOptions.siteRadius = TerrainLevelRadius(dev.industryId, *catalogue) + AuLegacyShip::CoordinateSpacing;
					QString error;
					if(!building.paths.Build(*design, pathOptions, error)) building.mesh.warnings.append(error);
				}
			AuLegacyShip::Rendering::Mesh collisionMesh;
			for(const auto& batch : building.mesh.batches)
			{
				// Original building doorways and hatches are always open. Their
				// textured frame quads must never become solid collision panels.
				if(batch.doorId || batch.empireFlag) continue;
				AuLegacyShip::Rendering::Batch imported;
				imported.vertices = batch.vertices;
				imported.material = batch.stairs ? AuLegacyShip::Rendering::Material::StairTread
					: AuLegacyShip::Rendering::Material::HullWall;
				collisionMesh.batches.append(imported);
			}
			building.collision.reset(new AuLegacyScGeometry::CollisionWorld(collisionMesh));
			buildings.append(building);
		}
		++revision;
	}

	void DeleteDraw(AuAtomRenderInfo& ari, const AuId& id)
	{
		for(auto& batch : drawBatches) if(batch.vao.VAO()) batch.vao.DeleteGL();
		drawBatches.clear(); drawRevision = 0;
		if(ari.re) ari.re->DeleteTextures(id.GetID());
	}
};

DaLegacyCity::DaLegacyCity() : m_Runtime(new Runtime) {}
DaLegacyCity::~DaLegacyCity() = default;
DaLegacyCity::DaLegacyCity(const DaLegacyCity& city) : DaAtom(city), m_Runtime(new Runtime)
{ QReadLocker lock(&city.m_CityLock); m_City = city.m_City; }
DaAtom* DaLegacyCity::Clone() const { return new DaLegacyCity(*this); }
AuLegacyCity::State DaLegacyCity::Snapshot() const { QReadLocker lock(&m_CityLock); return m_City; }
bool DaLegacyCity::ReadReportHistory(const QByteArray& bytes, AuCityReportHistory& result)
{
	// The shared history assignment appends, so clear it using its existing
	// zero-count reader before publishing or returning an empty result.
	const QByteArray emptyHistory(2, char(0));
	QDataStream reset(emptyHistory); reset.setVersion(QDataStream::Qt_4_1); reset >> result;
	if(bytes.size() < 2 || bytes.size() > AuLegacyCity::MaximumStateBytes / 2) return false;
	QDataStream countReader(bytes); countReader.setVersion(QDataStream::Qt_4_1);
	quint16 count = 0; countReader >> count;
	if(countReader.status() != QDataStream::Ok || !count || count > 240) return false;
	QDataStream reader(bytes); reader.setVersion(QDataStream::Qt_4_1);
	AuCityReportHistory candidate; reader >> candidate;
	// The shared display history applies its normal retention and inventory
	// minimization. The original raw bytes remain intact in State and the DB.
	if(reader.status() != QDataStream::Ok || !reader.atEnd() || candidate.IsEmpty() || candidate.Count() > count) return false;
	result = candidate;
	return true;
}
bool DaLegacyCity::ReportHistory(AuCityReportHistory& result) const
{ return ReadReportHistory(Snapshot().reportHistory, result); }
QString DaLegacyCity::Name() const { QReadLocker lock(&m_CityLock); return m_City.name; }
quint32 DaLegacyCity::EmpireId() const { QReadLocker lock(&m_CityLock); return m_City.controllingEmpire; }

bool DaLegacyCity::IsInScene(const DaPerson* sceneAtom, const AuPoint& cameraEnvironmentLocation) const
{
	Q_UNUSED(cameraEnvironmentLocation);
	return sceneAtom && sceneAtom->IsNearPlanet()
		&& FindParentWorldNotSatelliteInEnv() == sceneAtom->FindParentWorldNotSatelliteInEnv();
}

double DaLegacyCity::AdjustSiteTerrain(const DaWorldLL& world, double altitudeMSL, const AuGlobeLL& ll,
	int& levelSites, bool& hideRocks, int& hideGrass, bool& hideShrubs,
	bool& hideTrees, WORLDTEX_TYPE& pavementType) const
{
	if(!g_Au.IsLegacyGeneration() || IsDead()) return altitudeMSL;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->EnsureSites(*this, world);
	return m_Runtime->AdjustTerrain(world,altitudeMSL,ll,levelSites,hideRocks,hideGrass,hideShrubs,hideTrees,pavementType);
}

double DaLegacyCity::Runtime::AdjustTerrain(const DaWorldLL& world, double altitudeMSL, const AuGlobeLL& ll,
	int& levelSites, bool& hideRocks, int& hideGrass, bool& hideShrubs,
	bool& hideTrees, WORLDTEX_TYPE& pavementType) const
{
	for(const auto& site : sites)
	{
		// The original projects at the site's altitude, not the incoming terrain
		// altitude. This keeps its square boundary stable on slopes and globes.
		const auto& dev = site.state;
		const AuPoint worldLocation = world.LocOfLL(ll, dev.altitudeMSL);
		// The old world's development lookup excluded distant sites before
		// InBuildingSite. A tangent-plane projection alone also hits antipodes.
		const double range = 2 * (site.radius + 32.8084);
		if(worldLocation.Distance2(site.placement.Translation()) > range * range) continue;
		if(dev.industryId == 2)
		{
			// DpGlobeRoad::AdjustAltMSL, 0x652140. Use the original LL segment
			// query, including endpoint blend caps and the eight-foot side margin.
			float fraction;
			const AuGlobeLLF closest = ll.ToLLF().CoordOnSegment(site.roadStart, site.roadEnd, &fraction, world.LLShape());
			const float grade = float(dev.roadStartAltitudeMSL + (dev.roadEndAltitudeMSL - dev.roadStartAltitudeMSL)
				* qBound(0.f, fraction, 1.f));
			const AuGlobeLLF segmentLL = fraction <= 0 ? site.roadStart : fraction >= 1 ? site.roadEnd : closest;
			const float distance2 = float(world.LocOfLL(ll, grade).Distance2(world.LocOfLL(segmentLL, grade)));
			const float width = site.halfWidth;
			const bool landscaping = dev.constructionProcessId == 74 || dev.constructionProcessId == 80 || dev.constructionProcessId == 81;
			const bool endpoint = fraction <= 0 || fraction >= 1;
			if(endpoint || distance2 > width * width)
			{
				if(levelSites || landscaping) continue;
				const float blendRadius = width + 32.8084f;
				if(distance2 >= blendRadius * blendRadius) continue;
				const float blend = endpoint ? std::sqrt(distance2) / blendRadius
					: (std::sqrt(distance2) - width) / 32.8084f;
				altitudeMSL = AuNoiseF::Cosine_Interpolate(grade, float(altitudeMSL), blend);
				continue;
			}
			if(landscaping)
			{
				if(dev.constructionProcessId == 80) hideGrass = 1;
				else if(!hideGrass) hideGrass = 2;
				if(dev.constructionProcessId != 74) hideShrubs = hideTrees = true;
			}
			else
			{
				altitudeMSL = levelSites ? (altitudeMSL * levelSites + grade) / (levelSites + 1) : grade;
				++levelSites;
				if(dev.constructionProcessId != 5)
				{
					hideGrass = 1;
					hideRocks = hideShrubs = hideTrees = true;
				}
			}
			if(!dev.constructed) pavementType = WORLDTEX_TYPE(0);
			else if(dev.constructionProcessId == 2) pavementType = WORLDTEX_TYPE(6);
			else if(dev.constructionProcessId == 3) pavementType = WORLDTEX_TYPE(4);
			else if(dev.constructionProcessId == 4) pavementType = WORLDTEX_TYPE(3);
			else if(dev.constructionProcessId == 7) pavementType = WORLDTEX_TYPE(2);
			continue;
		}
		const AuPoint p = site.inverse.Project(worldLocation);
		const double siteAltitude = dev.altitudeMSL;
		const float margin = std::max(float(std::abs(p.x))-site.halfWidth, float(std::abs(p.y))-site.halfLength);
		if(margin >= 32.8084f) continue;
		if(margin > 0)
		{
			// DpBuilding::AdjustAltMSL 0x597380: a level site takes priority
			// over all neighboring blend margins, including other cities.
			if(!levelSites)
				altitudeMSL = AuNoiseF::Cosine_Interpolate(float(altitudeMSL), float(siteAltitude),
					1.f - margin / 32.8084f);
			continue;
		}
		altitudeMSL = siteAltitude;
		++levelSites;
		hideGrass = 1;
		// DoesHideRocks/Shrubs/Trees 0x5a00d0/0x5a0180/0x5a0260.
		// Military bases and guard towers override all three with false.
		const int industry = dev.industryId;
		if(industry != 11 && industry != 12 && industry != 128)
		{
			if(industry != 50 && industry != 51 && industry != 113 && industry != 114) hideRocks = true;
			if(industry != 20 && industry != 31 && industry != 51 && industry != 66
				&& industry != 67 && industry != 113 && industry != 114) hideShrubs = true;
			if(industry != 20 && industry != 30 && industry != 31 && industry != 32
				&& industry != 113 && industry != 114) hideTrees = true;
		}
		const int slot = AuLegacyBd::Geometry::SiteTextureSlot(dev.materialId, dev.constructed && dev.components.isEmpty());
		if(slot >= 0) pavementType = WORLDTEX_TYPE(slot);
	}
	return altitudeMSL;
}

void DaLegacyCity::UpdateSiteTerrain(AuAtomRenderInfo& ari, bool removed)
{
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!world) return;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->EnsureSites(*this, *world);
	removed = removed || IsDead();
	if(!removed && m_Runtime->terrainDrawRevision == m_Runtime->terrainRevision) return;
	// Invalidate old sites as well, so moved/removed developments restore their
	// natural terrain and flora. Release the city lock before terrain work.
	QVector<Runtime::Site> dirty = m_Runtime->renderedSites;
	dirty += m_Runtime->sites;
	m_Runtime->renderedSites = removed ? QVector<Runtime::Site>() : m_Runtime->sites;
	m_Runtime->terrainDrawRevision = removed ? 0 : m_Runtime->terrainRevision;
	lock.unlock();
	for(const auto& site : dirty)
		world->BuildingChanged(site.placement, (site.radius + 32.8084) * std::sqrt(2.0), ari.mm);
}

bool DaLegacyCity::SetState(const AuLegacyCity::State& state, QString& error)
{
	if(!g_Au.IsLegacyGeneration()) { error = QStringLiteral("Legacy cities require a Legacy universe."); return false; }
	QByteArray checked;
	if(!AuLegacyCity::EncodeState(state, checked, error) || !ValidateDesigns(state, error)) return false;
	{ QWriteLocker lock(&m_CityLock); m_City = state; ++m_CityRevision; }
	SetModified();
	return true;
}

int DaLegacyCity::ConstructionLevelLimit(quint8 industry, quint16 material)
{
	const auto* recipe = ConstructionRecipe(industry);
	if(!recipe) return 0;
	return AuLegacyCityCatalogue::BuiltIn()->ConstructionLevelLimit(recipe->id,AuLegacyCity::CommodityId(material));
}

bool DaLegacyCity::BeginConstruction(DaPerson& person, quint8 industry, quint8 process, quint32 designId,
	quint16 material, const AuGlobeLLF& location, double heading, quint8 levels, QString& error,
	double roadLength, double roadWidth, bool roadSlab, double biodomeRadius)
{
	if(!ConstructionAuthority(*this, person, true, error)) return false;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const auto* recipe = ConstructionRecipe(industry, process);
	// Road pavement comes from its process, independently of the building
	// material retained in the construction window.
	if(industry==2 && recipe) material=recipe->id.value==3 ? 23 : 76;
	if(!world || !recipe || !levels || levels > AuLegacyCityCatalogue::BuiltIn()->ConstructionLevelLimit(recipe->id,AuLegacyCity::CommodityId(material),world->OrbitZone()!=OZONE_Inferno)
		|| !std::isfinite(heading) || !std::isfinite(location.Latitude()) || !std::isfinite(location.Longitude())
		|| std::abs(location.Latitude()) > PIf/2 || std::abs(location.Longitude()) > PIf
		|| !std::isfinite(roadLength) || !std::isfinite(roadWidth)
		|| roadLength < 1 || roadLength > 1000 || roadWidth < 1 || roadWidth > 100
		|| !std::isfinite(biodomeRadius) || biodomeRadius<1 || biodomeRadius>1000)
	{ error = QStringLiteral("Invalid original construction recipe, material, location or level count."); return false; }
	if(industry != 1 && industry != 2 && industry != 10 && industry != 114 && industry != 129)
	{
		if(!designId) designId = AuLegacyBd::Catalog::BuiltIn().DefaultForIndustry(industry);
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(designId);
		if(!design || design->header.industryId != industry)
		{ error = industry == 35 ? QStringLiteral("The original Dairy blueprint has not been recovered.")
			: QStringLiteral("Select an original public blueprint for this industry."); return false; }
	}
	else designId = 0;
	AuLegacyCity::Development dev;
	dev.designId = designId; dev.industryId = industry; dev.materialId = material;
	dev.location = location; dev.facingRadians = heading; dev.constructionProcessId = recipe->id.value;
	const double naturalAltitude=world->SurfaceAltitudeMSL(location);
	dev.altitudeMSL = naturalAltitude; dev.completedLevels = 0; dev.targetLevels = levels;
	if(world->Hydrographics() && dev.altitudeMSL>-32 && dev.altitudeMSL<1) dev.altitudeMSL=1;
	dev.roadLengthMeters = float(roadLength); dev.roadWidthMeters = float(roadWidth);
	dev.roadSlab = roadSlab;
	dev.biodomeRadiusMeters=float(biodomeRadius);
	if(industry==129 && !AuLegacyCityCatalogue::BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter()))
	{ error=QStringLiteral("The biodome circumference must contain 6 to 100 original fifty-meter segments."); return false; }
	AuTerrainInfo terrain; world->QueryTerrain(terrain, location);
	const double radius = industry==129 ? biodomeRadius*FeetPerMeter() : recipe->buildRadius * FeetPerMeter();
	if(world->IsForbiddenZone(location,dev.altitudeMSL,radius) || dev.altitudeMSL>world->AtmosphereHeightMSL())
	{ error=QStringLiteral("This site is outside the world's permitted construction terrain."); return false; }
	if(industry==62 && naturalAltitude>=0 && (std::abs(location.Latitude())>world->PolarLatitude() || world->OrbitZone()==OZONE_Frigid))
	{ error=QStringLiteral("Hydroelectric power plants require unfrozen water outside the polar region."); return false; }
	AuMatrix4x4 placement;
	world->BuildingConstructionTransform(world->LocOfLL(location, dev.altitudeMSL), heading, nullptr, AuPoint(), placement);
	if(industry == 2)
	{
		AuGlobeLLF start, end;
		if(!world->LLOfLoc(start,placement.Project(AuPoint(0,-roadLength*FeetPerMeter()/2,0)))
			|| !world->LLOfLoc(end,placement.Project(AuPoint(0,roadLength*FeetPerMeter()/2,0))))
		{ error = QStringLiteral("The road endpoints must remain on this world's surface."); return false; }
		dev.roadStartAltitudeMSL = world->SurfaceAltitudeMSL(start);
		dev.roadEndAltitudeMSL = roadSlab ? dev.roadStartAltitudeMSL : world->SurfaceAltitudeMSL(end);
		if(world->Hydrographics())
		{
			if(dev.roadStartAltitudeMSL>-32 && dev.roadStartAltitudeMSL<1) dev.roadStartAltitudeMSL=1;
			if(dev.roadEndAltitudeMSL>-32 && dev.roadEndAltitudeMSL<1) dev.roadEndAltitudeMSL=1;
		}
	}
	unsigned int rocks = 0, shrubs = 0, trees = 0; float water = 0;
	world->ResourceDistributionAt(placement, placement.Translation(), AuPoint(radius,radius,0), rocks,shrubs,trees,water,4);
	// Original terrain categories from LockTestBuildLocation 0x5264d0.
	const int category = recipe->terrain;
	bool terrainOk = true;
	const bool seaBottom = terrain.IsWater() && dev.altitudeMSL <= -32;
	if(seaBottom && -dev.altitudeMSL<(MaximumDesignDeck(industry,material)+4)*AuLegacyBd::DeckHeight)
	{ error=QStringLiteral("The water is too shallow for this original building's required underwater clearance."); return false; }
	if(category == 1 || category == 2 || category == 3) terrainOk = seaBottom || (!terrain.IsWater() && water <= .1f);
	else if(category == 4) terrainOk = terrain.m_byWater && water >= .3f;
	else if(category == 5) terrainOk = terrain.m_byWater && water >= .4f;
	else if(category == 7) terrainOk = (seaBottom || (!terrain.IsWater() && water <= .25f)) && trees > 0;
	else if(category == 8) terrainOk = (seaBottom || (!terrain.IsWater() && water <= .25f)) && rocks > 0;
	else if(category == 9) terrainOk = terrain.m_byWater && water >= .9f;
	else if(category == 10) terrainOk = terrain.m_byWater && water >= 1.f;
	else if(category == 11 || category == 13 || category == 14) terrainOk = seaBottom || !terrain.IsWater();
	if(!terrainOk || levels > ResourceLevelLimit(*world, dev))
	{ error = QStringLiteral("This terrain or its natural resources cannot support the selected construction and levels."); return false; }
	const AuPoint center = placement.Translation();
	const double extentX = industry == 2 ? roadWidth*FeetPerMeter()/2 : radius;
	const double extentY = industry == 2 ? roadLength*FeetPerMeter()/2 : radius;
	const double proposedHeight=industry==129 ? AuLegacyBd::Biodome::RoofHeight(radius,0)
		: (MaximumDesignDeck(industry,material)+1)*AuLegacyBd::DeckHeight;
	AuBox proposed(AuPoint(-extentX,-extentY,-8),AuPoint(extentX,extentY,proposedHeight));
	proposed.Transform(placement);
	DaAtomList peers; world->ListNormalSubatoms(peers);
	bool adjacent = industry == 1 || category == 15 || category == 16;
	int baseFlagsInZone=0;
	const int resourceZone=world->ResourceZone(location.Longitude());
	for(auto* peer : peers)
	{
		if(auto* city = dynamic_cast<DaLegacyCity*>(peer))
		{
			const auto otherCity=city->Snapshot();
			if(industry==10 && world->ResourceZone(city->Transform().Translation())==resourceZone)
				for(const auto& existing : otherCity.developments) if(existing.industryId==10)
					baseFlagsInZone+=qMax(existing.completedLevels,existing.targetLevels);
			for(const auto& existing : otherCity.developments)
			{
				if(existing.demolished) continue;
				const bool landscapingRoad = existing.industryId == 2 && (existing.constructionProcessId == 5
					|| existing.constructionProcessId == 74 || existing.constructionProcessId == 80 || existing.constructionProcessId == 81);
				if(landscapingRoad && industry != 129) continue; // Original FIND_SkipLandscapers.
				const auto* otherRecipe = ConstructionRecipe(existing.industryId, existing.constructionProcessId);
				if(!otherRecipe) continue;
				AuMatrix4x4 otherPlacement;
				world->BuildingConstructionTransform(world->LocOfLL(existing.location,existing.altitudeMSL),existing.facingRadians,nullptr,AuPoint(),otherPlacement);
				const AuPoint p = otherPlacement.UnProject(center);
				const double ownX = industry == 2 ? roadWidth * FeetPerMeter()/2 : radius;
				const double ownY = industry == 2 ? roadLength * FeetPerMeter()/2 : radius;
				const double otherX = existing.industryId == 2 ? existing.roadWidthMeters*FeetPerMeter()/2
					: existing.industryId==129 ? existing.biodomeRadiusMeters*FeetPerMeter() : otherRecipe->buildRadius*FeetPerMeter();
				const double otherY = existing.industryId == 2 ? existing.roadLengthMeters*FeetPerMeter()/2 : otherX;
				const double angle = heading - existing.facingRadians;
				const double cosine=std::cos(angle),sine=std::sin(angle);
				const double rx = ownX*std::abs(cosine) + ownY*std::abs(sine);
				const double ry = ownX*std::abs(sine) + ownY*std::abs(cosine);
				// Test all four axes of the two oriented footprints.
				bool overlapping = std::abs(p.x) < otherX+rx-.05 && std::abs(p.y) < otherY+ry-.05
					&& std::abs(p.x*cosine+p.y*sine) < ownX+otherX*std::abs(cosine)+otherY*std::abs(sine)-.05
					&& std::abs(-p.x*sine+p.y*cosine) < ownY+otherX*std::abs(sine)+otherY*std::abs(cosine)-.05;
				if(industry != 2 && industry != 129 && existing.industryId == 2)
					overlapping = BuildingTouchesRoad(*world, dev, placement, radius, existing, otherPlacement);
				else if(industry == 2 && existing.industryId != 2 && existing.industryId != 129)
					overlapping = RoadTouchesBuilding(*world, dev, placement, existing, otherPlacement, otherX);
				const bool inMargin = std::abs(p.x) <= otherX+rx+32.8084 && std::abs(p.y) <= otherY+ry+32.8084;
				bool domeCompatible=false;
				if(industry==129 || existing.industryId==129)
				{
					const bool constructingDome=industry==129;
					const double domeRadius=constructingDome ? radius : otherX;
					const double rectangleX=constructingDome ? otherX : ownX;
					const double rectangleY=constructingDome ? otherY : ownY;
					const AuMatrix4x4 toDome=constructingDome ? placement.Inverse()*otherPlacement : otherPlacement.Inverse()*placement;
					double outerRadius=0;
					for(int x : {-1,1}) for(int y : {-1,1})
					{
						const AuPoint corner=toDome.Project(AuPoint(x*rectangleX,y*rectangleY,0));
						outerRadius=qMax(outerRadius,std::hypot(corner.x,corner.y));
					}
					const AuPoint domeInRectangle=toDome.UnProject(AuPoint());
					double innerRadius=std::hypot(qMax(0.0,std::abs(domeInRectangle.x)-rectangleX),qMax(0.0,std::abs(domeInRectangle.y)-rectangleY));
					if(industry==129 && existing.industryId==129)
					{
						const double centerDistance=std::hypot(domeInRectangle.x,domeInRectangle.y);
						outerRadius=centerDistance+otherX; innerRadius=qMax(0.0,centerDistance-otherX);
					}
					const bool completelyInside=outerRadius<=domeRadius-AuLegacyBd::Biodome::ContainmentMargin;
					const bool completelyOutside=innerRadius>=domeRadius+AuLegacyBd::Biodome::ContainmentMargin;
					if(!completelyInside && !completelyOutside)
					{ error=QStringLiteral("A biodome's outer wall would intersect an existing development."); return false; }
					if(!constructingDome && industry==111 && completelyInside)
					{ error=QStringLiteral("Airport terminals cannot be enclosed by an existing biodome."); return false; }
					domeCompatible=true;
				}
				if((overlapping && city->EmpireId()!=EmpireId()) || (industry == 1 && inMargin))
				{ error = QStringLiteral("The founding site or road intrudes on another city development."); return false; }
				if(overlapping && !domeCompatible && !(industry == 2 && (existing.industryId == 2 || existing.industryId == 1)))
				{ error = QStringLiteral("The construction site overlaps an existing development."); return false; }
				if(city==this && inMargin)
				{
					const bool town=existing.industryId==1;
					const bool road=existing.industryId==2 && existing.constructed && !landscapingRoad;
					// LockTestForAdjacentFinishedRoad 0x527fe0 explicitly tries
					// the town square before a finished non-landscaping road.
					if(category==3) adjacent=adjacent || (road && existing.roadSlab);
					else if(category==13) adjacent=adjacent || town || road || existing.industryId==industry;
					else if(category==14 || category==9 || category==10) adjacent=adjacent || !landscapingRoad;
					else adjacent=adjacent || town || road;
				}
			}
		}
		else if(peer != &person && !peer->IsEthereal())
		{
			AuBox bounds;
			if(peer->RoughExtents(bounds))
			{
				bounds.Transform(peer->Transform());
				if(bounds.DoesIntersect(proposed)) { error = QStringLiteral("An actor, vehicle or spacecraft occupies this site."); return false; }
			}
		}
	}
	if(industry==10)
	{
		// MaxBaseFlagsInResourceZone 0x4b74f0 uses the original population
		// ceiling, not the later modern-world population formula.
		const auto* sphere=dynamic_cast<const DaWorldSphere*>(world);
		const int populationMaximum=sphere ? sphere->Radius100km()*100 : 1000;
		const auto* flagIndustry=AuLegacyCityCatalogue::BuiltIn()->Industry(AuLegacyCity::IndustryId(10));
		const int troops=flagIndustry ? std::abs(int(flagIndustry->employees)) : 0;
		if(!troops || baseFlagsInZone>=(troops-1+populationMaximum/30)/troops+2)
		{ error=QStringLiteral("This resource zone has reached its original military flag limit."); return false; }
	}
	if(!adjacent) { error = QStringLiteral("Build beside a completed city development or road."); return false; }
	ConstructedDecks(dev);
	InitializeConstruction(dev, SitePreparationLabor(*world,dev), ConstructionEnvironment(*world,location));
	QWriteLocker lock(&m_CityLock);
	if(m_City.developments.size() >= AuLegacyCity::MaximumDevelopments)
	{ error = QStringLiteral("The city has reached its development limit."); return false; }
	int flagSites=industry==1 || industry==10 ? 1 : 0;
	for(const auto& existing : m_City.developments)
	{
		if(existing.industryId==1 || existing.industryId==10) ++flagSites;
		dev.id = qMax(dev.id,existing.id);
		if(industry == 1 && existing.industryId == 1)
		{ error = QStringLiteral("This city already has a town square."); return false; }
	}
	if(flagSites>4096) { error=QStringLiteral("The city has reached its flag limit."); return false; }
	if(dev.id == std::numeric_limits<quint32>::max()) { error = QStringLiteral("The city has exhausted development identities."); return false; }
	++dev.id;
	AuLegacyCity::State candidate=m_City;
	if(industry == 1)
	{
		if(!person.EmpireId()) { error = QStringLiteral("Join an empire before founding a city."); return false; }
		// Validate the entire publishable city before consuming the flag.
		dev.completedLevels = dev.targetLevels = 1; dev.laborRemaining=0; dev.components.clear(); ConstructedDecks(dev);
		AuLegacyCity::Flag cityFlag; cityFlag.empire=person.EmpireId(); cityFlag.location=dev.location;
		cityFlag.level=30; cityFlag.capturedAt=QDateTime::currentMSecsSinceEpoch(); candidate.flags.append(cityFlag);
	}
	candidate.developments.append(dev);
	QByteArray checked;
	if(!AuLegacyCity::EncodeState(candidate,checked,error) || !ValidateDesigns(candidate,error)) return false;
	if(industry == 1)
	{
		AuGear* gear = person.LockGearForWrite(); AuItemHash items; gear->CombineItemsForSale(items);
		const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
		const auto flagId = COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(1)));
		QScopedPointer<AuItem> flag;
		for(const auto& entry : items) if(entry.second->CommodityId()==flagId && entry.second->Count())
		{ flag.reset(entry.second->Clone()); flag->SetCount(1); break; }
		if(!flag) { person.UnlockGear(); error=QStringLiteral("An empire flag in your inventory is required to found a city."); return false; }
		gear->RemoveItems(*flag); person.UnlockGear();
	}
	m_City=std::move(candidate); ++m_CityRevision;
	lock.unlock(); SetModified();
	if(industry==1) { person.LockGearForWrite(); person.UpdateGearWeight(); person.UnlockGear(); }
	return true;
}

bool DaLegacyCity::ExtendConstruction(DaPerson& person, quint32 id, quint8 levels, QString& error)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	auto state = Snapshot();
	for(auto& dev : state.developments) if(dev.id == id)
	{
		const auto* recipe=ConstructionRecipe(dev.industryId,dev.constructionProcessId);
		if(!world || !levels || dev.demolished || !dev.components.isEmpty() || !dev.constructed
			|| !recipe || int(dev.completedLevels)+levels > AuLegacyCityCatalogue::BuiltIn()->ConstructionLevelLimit(recipe->id,
				AuLegacyCity::CommodityId(dev.materialId),world->OrbitZone()!=OZONE_Inferno)
			|| int(dev.completedLevels)+levels > ResourceLevelLimit(*world,dev))
		{ error = QStringLiteral("Finish current work first; extensions must fit the original material and resource limits."); return false; }
		const quint8 expectedProcess=dev.constructionProcessId;
		dev.constructionProcessId=recipe->id.value;
		dev.targetLevels = quint8(dev.completedLevels+levels);
		InitializeConstruction(dev,SitePreparationLabor(*world,dev),ConstructionEnvironment(*world,dev.location));
		return ReplaceFinishedDevelopment(dev,expectedProcess,dev.completedLevels,error);
	}
	error = QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::ReplaceFinishedDevelopment(const AuLegacyCity::Development& replacement,
	quint8 expectedProcess, quint8 expectedLevels, QString& error)
{
	QWriteLocker lock(&m_CityLock);
	auto candidate=m_City;
	for(auto& current : candidate.developments) if(current.id==replacement.id)
	{
		if(!current.components.isEmpty() || !current.constructed || current.constructionProcessId!=expectedProcess
			|| current.completedLevels!=expectedLevels || current.designId!=replacement.designId
			|| current.industryId!=replacement.industryId || (current.industryId!=2 && current.materialId!=replacement.materialId))
		{ error=QStringLiteral("This development changed while the request was being prepared. Refresh its construction status."); return false; }
		const bool citizenWorker=current.citizenWorker;
		current=replacement; current.citizenWorker=citizenWorker;
		QByteArray checked;
		if(!AuLegacyCity::EncodeState(candidate,checked,error) || !ValidateDesigns(candidate,error)) return false;
		m_City=std::move(candidate); ++m_CityRevision;
		lock.unlock(); SetModified(); return true;
	}
	error=QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::SetConstructionHalted(DaPerson& person, quint32 id, bool halted, QString& error)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	QWriteLocker lock(&m_CityLock);
	for(auto& dev : m_City.developments) if(dev.id == id && !dev.components.isEmpty())
	{ dev.halted = halted; ++m_CityRevision; lock.unlock(); SetModified(); return true; }
	error = QStringLiteral("There is no construction to halt or resume."); return false;
}

bool DaLegacyCity::EmployConstructionWorker(DaPerson& person, quint32 id, bool enabled, QString& error)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	QWriteLocker lock(&m_CityLock);
	for(auto& dev : m_City.developments) if(dev.id == id)
	{ dev.citizenWorker = enabled; ++m_CityRevision; lock.unlock(); SetModified(); return true; }
	error = QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::DemolishDevelopment(DaPerson& person, quint32 id, bool clear, QString& error)
{
	Q_UNUSED(clear);
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	QWriteLocker lock(&m_CityLock);
	auto state = m_City;
	for(int i=0;i<state.developments.size();++i) if(state.developments[i].id == id)
	{
		auto& dev = state.developments[i];
		if(dev.industryId == 1 && state.developments.size() != 1)
		{ error = QStringLiteral("Remove all other buildings and roads before the town square."); return false; }
		if(dev.industryId == 1 || dev.industryId == 10)
			for(int j=state.flags.size()-1;j>=0;--j) if(state.flags[j].location==dev.location) state.flags.removeAt(j);
		// DeleteConstruction 0x514bf0 deletes the development, with no synthetic
		// rubble phase and no refund of materials already applied.
		state.developments.removeAt(i);
		QByteArray checked;
		if(!AuLegacyCity::EncodeState(state,checked,error)) return false;
		m_City=std::move(state); ++m_CityRevision;
		lock.unlock(); SetModified(); return true;
	}
	error = QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::FetchConstructionMaterials(DaPerson& person, quint32 id, QString& error)
{ return OperateConstruction(&person,id,true,false,error); }
bool DaLegacyCity::ContributeConstructionLabor(DaPerson& person, quint32 id, QString& error, bool* accepted)
{ return OperateConstruction(&person,id,true,true,error,accepted); }

bool DaLegacyCity::ChangeRoadConstruction(DaPerson& person, quint32 id, quint8 process, QString& error)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	if(process!=2 && process!=3 && process!=4 && process!=5 && process!=7)
	{ error=QStringLiteral("Existing roads may be changed to dirt, asphalt, concrete, pressurized pavement or grading."); return false; }
	auto* world=dynamic_cast<DaWorldLL*>(Parent()); if(!world) return false;
	if(process==3 && world->OrbitZone()==OZONE_Inferno)
	{ error=QStringLiteral("Asphalt roads cannot be constructed in an Inferno orbit zone."); return false; }
	auto state=Snapshot();
	for(auto& dev : state.developments) if(dev.id==id)
	{
		// DpGlobeRoad::ChangeConstruction 0x652b20 leaves unfinished projects
		// intact and does not allow the grade-only development to be repaved.
		if(dev.industryId!=2 || !dev.components.isEmpty() || !dev.constructed)
		{ error=QStringLiteral("Finish this road's current construction before changing its pavement."); return false; }
		if(dev.constructionProcessId==5)
		{ error=QStringLiteral("A graded area cannot be repaved; build a road on its prepared terrain."); return false; }
		if(dev.constructionProcessId==process)
		{ error=QStringLiteral("This road already uses that construction process."); return false; }
		const quint8 expectedProcess=dev.constructionProcessId, expectedLevels=dev.completedLevels;
		dev.constructionProcessId=process; dev.materialId=process==3 ? 23 : 76;
		dev.completedLevels=0; dev.targetLevels=1; dev.constructed=false; dev.halted=false;
		const quint32 pavement=AuLegacyCityCatalogue::RoadPavementQuantity(dev.roadLengthMeters*FeetPerMeter(),dev.roadWidthMeters*FeetPerMeter());
		InitializeConstruction(dev,qMin(quint32(65535),(pavement+1)/2+4),ConstructionEnvironment(*world,dev.location));
		return ReplaceFinishedDevelopment(dev,expectedProcess,expectedLevels,error);
	}
	error=QStringLiteral("The road no longer exists."); return false;
}

bool DaLegacyCity::OperateConstruction(DaPerson* person, quint32 id, bool fetch, bool labor, QString& error, bool* accepted)
{
	if(accepted) *accepted=false;
	if(person && !ConstructionAuthority(*this,*person,false,error)) return false;
	auto* world = dynamic_cast<DaWorldLL*>(Parent()); if(!world) return false;
	const auto before = Snapshot();
	auto original = std::find_if(before.developments.cbegin(),before.developments.cend(),[id](const AuLegacyCity::Development& dev){return dev.id==id;});
	if(original == before.developments.cend()) { error = QStringLiteral("The development no longer exists."); return false; }
	if(person)
	{
		AuMatrix4x4 actor;
		if(!person->GetTransformRelativeToParent(actor,world)) return false;
		AuMatrix4x4 site;
		world->BuildingConstructionTransform(world->LocOfLL(original->location,original->altitudeMSL),original->facingRadians,nullptr,AuPoint(),site);
		AuGlobeLL actorLL;
		if(!world->LLOfLoc(actorLL,actor.Translation())) return false;
		const AuPoint local=site.UnProject(world->LocOfLL(actorLL,original->altitudeMSL));
		const double actorAltitude=world->AltitudeMSL(actor.Translation());
		const auto* recipe=ConstructionRecipe(original->industryId,original->constructionProcessId);
		const double halfWidth=original->industryId==2 ? original->roadWidthMeters*FeetPerMeter()/2
			: original->industryId==129 ? original->biodomeRadiusMeters*FeetPerMeter()+AuLegacyBd::Biodome::ContainmentMargin
			: recipe ? recipe->buildRadius*FeetPerMeter()+2.828427 : 0;
		const double halfLength=original->industryId==2 ? original->roadLengthMeters*FeetPerMeter()/2 : halfWidth;
		const double baseAltitude=original->industryId==2 ? original->roadStartAltitudeMSL+(original->roadEndAltitudeMSL-original->roadStartAltitudeMSL)
			*qBound(0.0,(local.y+halfLength)/(2*halfLength),1.0) : original->altitudeMSL;
		const double height=actorAltitude-baseAltitude;
		if(std::abs(local.x)>halfWidth || std::abs(local.y)>halfLength || (original->industryId==129 && std::hypot(local.x,local.y)>halfWidth)
			|| height<(original->minimumDeck-1)*AuLegacyBd::DeckHeight
			|| height>(original->industryId==129 ? AuLegacyBd::Biodome::RoofHeight(original->biodomeRadiusMeters*FeetPerMeter(),std::hypot(local.x,local.y))
				: (original->maximumDeck+2)*AuLegacyBd::DeckHeight))
		{ error = QStringLiteral("Stand inside the construction site to supply materials or contribute labor."); return false; }
	}
	const int threat=ConstructionThreat(*world,Id(),*original,person ? person->EmpireId() : EmpireId());
	if(threat)
	{ error=threat==1 ? QStringLiteral("STOP! Wild Animal Present") : QStringLiteral("STOP! Enemy Unit Present"); return false; }
	quint32 nextPreparation=1, possibleLabor=4;
	for(const auto& component : original->components)
		if(component.effect==CE_DecreasesTime) possibleLabor+=component.commodityId==3 ? 1 : AuItem::GetLaborBoost();
	if(original->completedLevels+1<original->targetLevels && original->laborRemaining<=possibleLabor)
	{
		// CompleteConstruction 0x510830 repeats the original preparation
		// calculation before ContinueConstruction starts each queued level.
		// Only sample when this effort could finish the current level.
		auto next=*original; ++next.completedLevels; nextPreparation=SitePreparationLabor(*world,next);
	}
	AuZoneResource natural; world->QueryNaturalResources(natural,world->ResourceZone(original->location.Longitude()));
	const quint8 environment = ConstructionEnvironment(*world,original->location);
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn(); if(!catalogue) return false;
	const auto groundItems=fetch ? AuLegacyConstructionMaterials::ItemsAtSite(*world,Id(),*original) : QVector<DaItem*>();
	QVector<DaItem*> changedItems,emptiedItems;
	QWriteLocker lock(&m_CityLock);
	auto found = std::find_if(m_City.developments.begin(),m_City.developments.end(),[id](const AuLegacyCity::Development& dev){return dev.id==id;});
	if(found == m_City.developments.end() || found->components.isEmpty() || found->demolished)
	{ error = QStringLiteral("There is no unfinished construction here."); return false; }
	if(found->halted) { error = QStringLiteral("Resume construction before contributing materials or labor."); return false; }
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	AuItemHash inventory;
	if(!ReadConstructionInventory(m_City.inventory,inventory))
	{ error = QStringLiteral("The city's material inventory is malformed."); return false; }
	if(accepted) *accepted=true;
	if(found->completedLevels!=original->completedLevels || found->constructionProcessId!=original->constructionProcessId
		|| (found->completedLevels+1<found->targetLevels && found->laborRemaining<=possibleLabor && original->laborRemaining>possibleLabor))
	{ error=QStringLiteral("Construction advanced while the site was being checked. Its current status is ready for the next effort."); return false; }
	if(person && labor && now-m_ConstructionLaborTimes.value(person->Id().GetID(),0) < 1000)
	{ error.clear(); return false; }
	AuGear* gear = person && fetch ? person->LockGearForWrite() : nullptr;
	bool playerGearChanged=false;
	AuItemHash available;
	if(gear) gear->CombineItemsForSale(available);
	auto& dev = *found; bool changed = false, physicalChanged = false;
	const quint32 laborBefore = dev.laborRemaining;
	if(fetch)
		for(auto& component : dev.components)
		{
			if(!component.quantity || component.applied >= component.quantity) continue;
			const auto runtime = COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(component.commodityId)));
			DeProcessComponent working; working.SetCommodity(runtime,1); working.SetQuality(component.quality); working.SetEffect(COMPEFFECT(component.effect));
			working.SetQtyConsumed(component.quantity); working.SetQtyApplied(component.applied);
			auto apply = [&](AuItem& item) {
				if(item.CommodityId()!=runtime || working.IsSatisfied() || !item.Count()) return;
				const int needed = working.ApplyMaterial(item);
				dev.laborRemaining = qMin(quint32(65535),dev.laborRemaining+quint32(qMax(0,needed)));
			};
			for(auto& item : inventory) apply(*item.second);
			for(auto& item : available)
			{
				const quint32 count = item.second->Count(); apply(*item.second);
				if(gear && item.second->Count() < count)
				{
					QScopedPointer<AuItem> removed(item.second->Clone()); removed->SetCount(count-item.second->Count());
					gear->RemoveItems(*removed);
					changed = playerGearChanged = true;
				}
			}
			if(!working.IsSatisfied())
			{
				const int amount = natural.Fetch(runtime,1);
				if(amount>0) { AuItem item(runtime,natural.Quality(runtime),0,amount); item.SetHitPoints(1); apply(item); }
			}
			changed = changed || component.applied != working.QtyApplied() || component.quality != working.Quality();
			component.applied = working.QtyApplied(); component.quality = working.Quality();
		}
	if(gear) person->UnlockGear();
	if(fetch)
		for(auto& component : dev.components)
		{
			if(component.applied>=qMax(quint32(1),component.quantity)) continue;
			const auto runtime=COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(component.commodityId)));
			DeProcessComponent working; working.SetCommodity(runtime,1); working.SetQuality(component.quality); working.SetEffect(COMPEFFECT(component.effect));
			working.SetQtyConsumed(component.quantity); working.SetQtyApplied(component.applied);
			const quint32 added=AuLegacyConstructionMaterials::Fetch(groundItems,working,changedItems,emptiedItems);
			dev.laborRemaining=qMin(quint32(65535),dev.laborRemaining+added);
			changed=changed || component.applied!=working.QtyApplied() || component.quality!=working.Quality() || added;
			component.applied=working.QtyApplied(); component.quality=working.Quality();
		}
	if(labor && dev.laborRemaining)
	{
		bool missingTool = false; int boost = 0;
		// Verify every unsatisfied required modifier before wearing any tool.
		// DpDevelopment::ApplyConstructionLabor, AuAtom13 0x64b390.
		for(const auto& component : dev.components)
		{
			if(component.effect != CE_Required || component.quantity || component.applied) continue;
			const auto runtime = COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(component.commodityId)));
			const bool available = component.commodityId == 3 ? m_City.capacitor > 0
				: (person && person->FindReadyItem(runtime).IsValid()) || inventory.RawCount(runtime)>0;
			if(!available) missingTool = true;
		}
		if(!missingTool)
		for(auto& component : dev.components)
		{
			if((component.effect != CE_Required || component.quantity) && component.effect != CE_DecreasesTime) continue;
			if(component.effect == CE_Required && component.applied) continue;
			const auto runtime = COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(component.commodityId)));
			const bool power = component.commodityId == 3 && m_City.capacitor > 0;
			const AuGearPos ready = person ? person->FindReadyItem(runtime) : AuGearPos();
			const bool tool = person && ready.IsValid();
			const bool cityTool = inventory.RawCount(runtime)>0;
			component.applied = power || tool || cityTool ? 1 : 0;
			auto usePlayerTool=[&]() {
				AuGear* equipped=person->LockGearForWrite(); AuItem* item=equipped->ItemAt(ready);
				const int itemBoost=item ? item->GetLaborBoost() : 0;
				const bool hasItem=item!=nullptr;
				person->UnlockGear();
				if(hasItem)
				{
					// Use the public gear API for wear, removal and notifications.
					// Weight is refreshed below, after releasing the city lock.
					person->UseGearItem(ready,IPS_Item,false);
					playerGearChanged=true;
				}
				return itemBoost;
			};
			if(component.effect == CE_DecreasesTime && component.applied)
			{
				if(power) ++boost;
				else if(tool) boost += usePlayerTool();
				else for(auto& entry : inventory) if(entry.second->CommodityId()==runtime && entry.second->Count())
				{
					boost += entry.second->GetLaborBoost();
					if(!entry.second->UseItem()) entry.second->Consume(1);
					break;
				}
			}
			else if(component.effect == CE_Required && tool) usePlayerTool();
			else if(component.effect == CE_Required && cityTool)
				for(auto& entry : inventory) if(entry.second->CommodityId()==runtime && entry.second->Count())
				{ if(!entry.second->UseItem()) entry.second->Consume(1); break; }
		}
		if(!missingTool)
		{
			dev.laborRemaining -= qMin(dev.laborRemaining,quint32(4+boost)); changed = true;
			if(person)
			{
				dev.lastPlayerLabor = now; m_ConstructionLaborTimes.insert(person->Id().GetID(),now);
				if(m_ConstructionLaborTimes.size()>4096)
					for(auto it=m_ConstructionLaborTimes.begin();it!=m_ConstructionLaborTimes.end();)
						if(now-it.value()>60000) it=m_ConstructionLaborTimes.erase(it); else ++it;
			}
		}
	}
	// Retain original technology/quality stacks in city storage; discard only
	// emptied stacks after materials and worn tools have consumed exact items.
	for(auto it=inventory.begin();it!=inventory.end();) if(!it->second->Count()) { auto old=it++; inventory.Erase(old); } else ++it;
	QByteArray stored; QDataStream inventoryOut(&stored,QIODevice::WriteOnly); inventoryOut.setVersion(QDataStream::Qt_4_1); inventoryOut << inventory;
	m_City.inventory = stored;
	if(!dev.laborRemaining && ComponentsComplete(dev))
	{
		++dev.completedLevels; ConstructedDecks(dev); changed = physicalChanged = true;
		if(dev.completedLevels < dev.targetLevels) InitializeConstruction(dev,nextPreparation,environment);
		else dev.components.clear();
		if(dev.industryId == 10)
		{
			AuLegacyCity::Flag flag; flag.empire=m_City.controllingEmpire; flag.location=dev.location;
			flag.level=30; flag.capturedAt=now; m_City.flags.append(flag);
		}
	}
	changed = changed || dev.laborRemaining != laborBefore || stored != before.inventory;
	if(changed) ++m_CityRevision;
	lock.unlock();
	AuLegacyConstructionMaterials::Finalize(changedItems,emptiedItems);
	if(person && playerGearChanged) { person->LockGearForWrite(); person->UpdateGearWeight(); person->UnlockGear(); }
	if(changed) SetModified();
	if(physicalChanged) world->InvalidateEnvironmentRecursive();
	return changed;
}

void DaLegacyCity::Tock(const AuTime& last, const AuTime& now, const AuScripts& scripts, DtTockTransactions& transactions)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker()) return;
	DaAtom::Tock(last,now,scripts,transactions);
	if(IsDead() || !last.IsNewPeriod(now,1000,Id().GetID())) return;
	const auto state = Snapshot();
	// One citizen per enabled construction job; population is city-wide. Full
	// employment/economic allocation remains in stage 5.
	int available = state.population;
	for(const auto& dev : state.developments)
		if(available>0 && dev.citizenWorker && !dev.halted && !dev.components.isEmpty())
		{ --available; QString error; OperateConstruction(nullptr,dev.id,true,true,error); }
}

void DaLegacyCity::SetName(const QString& name)
{ auto state = Snapshot(); state.name = name; QString error; if(!SetState(state, error)) qWarning() << error; }

bool DaLegacyCity::ApplyPayload(const QByteArray& bytes, QString& error)
{
	AuLegacyCity::State state;
	if(!g_Au.IsLegacyGeneration() || !AuLegacyCity::DecodeState(bytes, state, error) || !ValidateDesigns(state, error)) return false;
	QWriteLocker lock(&m_CityLock); m_City = state; ++m_CityRevision;
	return true;
}

bool DaLegacyCity::ValidateStateData(const QByteArray& bytes, QString& error)
{
	AuLegacyCity::State state;
	return AuLegacyCity::DecodeState(bytes, state, error) && ValidateDesigns(state, error);
}

void DaLegacyCity::operator>>(QDataStream& out) const
{
	DaAtom::operator>>(out);
	QByteArray bytes; QString error;
	if(!AuLegacyCity::EncodeState(Snapshot(), bytes, error)) out.setStatus(QDataStream::WriteFailed);
	WriteEnvelope(out, bytes);
}
void DaLegacyCity::operator<<(QDataStream& in)
{
	DaAtom::operator<<(in);
	QByteArray bytes; QString error;
	if(!ReadEnvelope(in, bytes) || !ApplyPayload(bytes, error)) in.setStatus(QDataStream::ReadCorruptData);
}
void DaLegacyCity::TxUpdate(QDataStream& out, const AuTime& lastUpdate,
	const AuElapsedTimer& lastTx, const DaPerson& sceneAtom)
{
	DaAtom::TxUpdate(out, lastUpdate, lastTx, sceneAtom);
	auto state = Snapshot();
	// Public physical state is replicated. Accounts, inventory and report history
	// remain on the authority; later report requests must authorize their reader.
	state.inventory.clear(); state.manufacturing.clear(); state.accounting.clear(); state.reportHistory.clear();
	QByteArray bytes; QString error;
	if(!AuLegacyCity::EncodeState(state, bytes, error)) out.setStatus(QDataStream::WriteFailed);
	WriteEnvelope(out, bytes);
}
void DaLegacyCity::RxUpdate(QDataStream& in, bool firstUpdate)
{
	Q_UNUSED(firstUpdate);
	QByteArray bytes; QString error;
	if(!ReadEnvelope(in, bytes) || !ApplyPayload(bytes, error)) in.setStatus(QDataStream::ReadCorruptData);
}
bool DaLegacyCity::Refresh(DaAtom* counterpart, AuAtomRenderInfo* ari)
{
	auto* city = dynamic_cast<DaLegacyCity*>(counterpart);
	if(!city) return false;
	DaAtom::Refresh(counterpart, ari);
	const auto state = city->Snapshot();
	{ QWriteLocker lock(&m_CityLock); m_City = state; ++m_CityRevision; }
	if(ari) UpdateSiteTerrain(*ari);
	return false;
}

void DaLegacyCity::Insert(QSqlDatabase& db) const { Store(db, true); }
void DaLegacyCity::Update(QSqlDatabase& db) const { Store(db, false); }
void DaLegacyCity::Store(QSqlDatabase& db, bool insert) const
{
	if(IsDead() || !g_Au.IsLegacyGeneration()) return;
	const auto state = Snapshot(); QByteArray bytes; QString error;
	if(!AuLegacyCity::EncodeState(state, bytes, error)) { qWarning() << error; return; }
	const AuAtomPos pos(Pos()); const auto loc = pos.Translation(), rot = pos.Rotation();
	QSqlQuery query(db);
	query.prepare(insert
		? QStringLiteral("INSERT INTO a_LegacyCity(id,idp,locX,locY,locZ,rotX,rotY,rotZ,timeCreate,timeModified,timeTock,timeDeath,name,controllingEmpire,allegianceEmpire,population,satisfaction,stateVersion,stateData,reportHistory) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)")
		: QStringLiteral("UPDATE a_LegacyCity SET idp=?,locX=?,locY=?,locZ=?,rotX=?,rotY=?,rotZ=?,timeModified=?,timeTock=?,timeDeath=?,name=?,controllingEmpire=?,allegianceEmpire=?,population=?,satisfaction=?,stateVersion=?,stateData=?,reportHistory=? WHERE id=?"));
	if(insert) query.addBindValue(Id().GetID());
	query.addBindValue(pos.ParentId().GetID());
	for(double v : {loc.x, loc.y, loc.z, rot.x, rot.y, rot.z}) query.addBindValue(FuzzyClamp(v));
	if(insert) query.addBindValue(CreatedTime().UTC());
	query.addBindValue(ModifyTime().UTC()); query.addBindValue(TockTime().UTC()); query.addBindValue(DeathTime().UTC());
	query.addBindValue(state.name); query.addBindValue(state.controllingEmpire); query.addBindValue(state.allegianceEmpire);
	query.addBindValue(state.population); query.addBindValue(state.satisfaction); query.addBindValue(AuLegacyCity::StateVersion);
	query.addBindValue(bytes); query.addBindValue(state.reportHistory);
	if(!insert) query.addBindValue(Id().GetID());
	if(!query.exec()) qWarning() << "Legacy city persistence failed:" << Id() << query.lastError().text();
	else if(!insert && !query.numRowsAffected()) Insert(db);
}

bool DaLegacyCity::LoadSubatoms(AuEther& ether, QSqlDatabase& db, const AuId& world, QString* error)
{
	if(!g_Au.IsLegacyGeneration()) { if(error) *error = QStringLiteral("Legacy city loader called for a Modern universe."); return false; }
	QSqlQuery query(db);
	query.prepare(QStringLiteral("SELECT id,locX,locY,locZ,rotX,rotY,rotZ,timeCreate,timeModified,timeTock,timeDeath,stateVersion,stateData,name,controllingEmpire,allegianceEmpire,population,satisfaction,reportHistory FROM a_LegacyCity WHERE idp=?"));
	query.addBindValue(world.GetID());
	if(!query.exec()) { if(error) *error = query.lastError().text(); return false; }
	QVector<QSharedPointer<DaLegacyCity>> pending;
	while(query.next())
	{
		QSharedPointer<DaLegacyCity> city(new DaLegacyCity);
		QString failure;
		if((query.value(11).toUInt() != 1 && query.value(11).toUInt() != AuLegacyCity::StateVersion)
			|| !city->ApplyPayload(query.value(12).toByteArray(), failure))
		{ if(error) *error = QStringLiteral("Invalid saved legacy city %1: %2").arg(query.value(0).toString(), failure); return false; }
		const auto state = city->Snapshot();
		if(state.name != query.value(13).toString() || state.controllingEmpire != query.value(14).toUInt()
			|| state.allegianceEmpire != query.value(15).toUInt() || state.population != query.value(16).toInt()
			|| state.satisfaction != query.value(17).toInt() || state.reportHistory != query.value(18).toByteArray())
		{ if(error) *error = QStringLiteral("Legacy city indexed summary disagrees with its state envelope."); return false; }
		city->SetId(AuId(query.value(0).toUInt()));
		city->SetPosAndOrient(AuAtomPos(world, query.value(1).toDouble(), query.value(2).toDouble(), query.value(3).toDouble(),
			query.value(4).toDouble(), query.value(5).toDouble(), query.value(6).toDouble()));
		city->SetCreatedTime(query.value(7).toDateTime().toUTC()); city->SetModifyTime(query.value(8).toDateTime().toUTC());
		city->SetTockTime(query.value(9).toDateTime().toUTC()); city->SetDeathTime(query.value(10).toDateTime().toUTC());
		pending.append(city);
	}
	// Ownership passes to the ether only after every row has validated.
	for(auto& item : pending)
	{
		auto* city = new DaLegacyCity(*item);
		city->ConnectSignals(); ether.AddAtom(city); city->SetNormal();
	}
	return true;
}

bool DaLegacyCity::RoughExtents(AuBox& extents) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world) return false;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	AuPoint minimum, maximum; bool any = false;
	const auto worldToCity = Transform().Inverse();
	for(const auto& building : m_Runtime->buildings)
	{
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		AuBox box(Point(building.mesh.minimum), Point(building.mesh.maximum)); box.Transform(worldToCity * transform);
		AuPoint a, b; box.GetMinMax(a, b);
		minimum = any ? minimum.Min(a) : a; maximum = any ? maximum.Max(b) : b; any = true;
	}
	if(any) extents.SetFromMinMax(minimum, maximum);
	return any;
}

bool DaLegacyCity::IsPathBlocked(const AuPoint& from, const AuPoint& to) const
{
	AuPoint hit, normal;
	return const_cast<DaLegacyCity*>(this)->IsPathBlocked(from, to, hit, normal, nullptr);
}

bool DaLegacyCity::IsSightTarget(const DaAtom* observer) const
{ return g_Au.IsLegacyGeneration() && observer != this; }

DaAtom* DaLegacyCity::SightTarget(const DaAtom* observer, const AuPoint& eye,
	const AuMatrix4x4& model, DaAtom* best, AuPoint& sighted, unsigned int& part,
	bool& shield, AuPoint* surfaceNormal) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !IsSightTarget(observer)) return best;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	const auto worldToObserver = model * Transform().Inverse();
	for(const auto& building : m_Runtime->buildings)
	{
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), placement);
		const auto transform = worldToObserver * placement;
		const auto source = transform.UnProject(eye);
		const auto target = transform.UnProject(sighted);
		const AuBox box(Point(building.mesh.minimum), Point(building.mesh.maximum));
		AuPoint first, second;
		if(!box.IsPointInside(source) && !box.IsPointInside(target)
			&& !box.SegmentIntersection(source, target, first, second)) continue;
		for(const auto& batch : building.mesh.batches)
		{
			// Sight uses visible triangles; walking ramps are movement proxies.
			if(batch.collisionOnly || batch.doorId || batch.empireFlag) continue;
			for(int i=0; i+2<batch.vertices.size(); i+=3)
			{
				const AuTriangle triangle(Point(batch.vertices[i].position), Point(batch.vertices[i+1].position), Point(batch.vertices[i+2].position));
				AuPoint hit;
				if(triangle.SegmentIntersection(hit, source, target) != 3) continue;
				const auto location = transform.Project(hit);
				if(location.Distance2(eye) >= sighted.Distance2(eye)) continue;
				sighted = location; part = building.state.id; shield = false; best = const_cast<DaLegacyCity*>(this);
				if(surfaceNormal) *surfaceNormal = transform.ProjectVector(Point(batch.vertices[i].normal));
			}
		}
	}
	return best;
}

bool DaLegacyCity::IsPathBlocked(const AuPoint& from, const AuPoint& to, AuPoint& hit,
	AuPoint& normal, const DaAtom* actor)
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return false;
	const auto* creature = dynamic_cast<const DaCreature*>(actor);
	// A gravity query can rebuild actor environment and re-enter the city.
	const AuPoint gravityUp = creature ? creature->GravityNormal() : AuPoint();
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	bool blocked = false; double closest = from.Distance2(to);
	for(const auto& building : m_Runtime->buildings)
	{
		if(!building.collision->IsValid()) continue;
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		AuBox body(AuPoint(-.01,-.01,-.01), AuPoint(.01,.01,.01));
		if(actor)
		{
			if(creature) creature->LegacyCollisionExtents(body); else actor->RoughExtents(body);
			AuMatrix4x4 actorToWorld; actor->GetTransformRelativeToParent(actorToWorld, world);
			const auto actorToBuilding = transform.Inverse() * actorToWorld;
			if(creature) body = AuLegacyScGeometry::MovementBodyOffsets(body, actorToBuilding.ProjectVector(gravityUp));
			else for(auto& corner : body.m_fCorners) corner = actorToBuilding.ProjectVector(corner);
		}
		AuPoint localHit, localNormal;
		if(building.collision->IsBodyPathBlocked(transform.UnProject(from), transform.UnProject(to), body, localHit, localNormal))
		{
			const auto candidate = transform.Project(localHit);
			if(candidate.Distance2(from) <= closest)
			{ hit = candidate; normal = transform.ProjectVector(localNormal); closest = candidate.Distance2(from); blocked = true; }
		}
	}
	return blocked;
}

bool DaLegacyCity::FilterDestination(const DaAtom& actor, const AuPoint& from, AuPoint& to, const AuBox& anatomy) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return false;
	AuMatrix4x4 actorToWorld; if(!actor.GetTransformRelativeToParent(actorToWorld, world)) return false;
	const auto* creature = dynamic_cast<const DaCreature*>(&actor);
	// Match the legacy spacecraft's walking eligibility. A creature in flight,
	// a jump or a ladder transition must retain its requested vertical movement.
	const bool followGround = creature && !creature->IsJumping()
		&& !creature->IsFlying() && !creature->IsLadderPose() && !creature->InLadderWell()
		&& !creature->IsInGravityWell() && creature->IsTouchingGround();
	// DaCreature::CGHeight already subtracts CGOffset.z, so this is the
	// insertion-point-to-ground height expected by the collision resolver.
	const double standingHeight = actor.GroundHeightAboveCG();
	// These environment reads can rebuild the query and re-enter this city's
	// QueryLegacyEnvironment, so resolve them before taking the runtime lock.
	const AuPoint gravityUp = actor.GravityNormal();
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	bool changed = false;
	for(const auto& building : m_Runtime->buildings)
	{
		if(!building.collision->IsValid()) continue;
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		const auto bodyTransform = transform.Inverse() * actorToWorld;
		const auto up = bodyTransform.ProjectVector(gravityUp);
		AuBox body(anatomy);
		if(creature) body = AuLegacyScGeometry::MovementBodyOffsets(body, up);
		else for(auto& corner : body.m_fCorners) corner = bodyTransform.ProjectVector(corner);
		AuPoint normal;
		const auto result = building.collision->ResolveDestination(transform.UnProject(from), transform.UnProject(to),
			body, up, standingHeight, followGround, normal);
		const auto destination = transform.Project(result);
		if(destination.Distance2(to) > 1e-16) { changed = true; to = destination; }
	}
	return changed;
}

void DaLegacyCity::QueryLegacyEnvironment(const DaAtom& actor, const AuPoint& location, AuQueryEnvironment& query) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return;
	// EnclosureNeeded 0x28f190 depends on the city's origin and world, without
	// a power gate. Resolve these world properties before taking the mesh lock.
	const bool undersea=world->Hydrographics() && world->AltitudeMSL(Transform().Translation())<=-32;
	const int enclosure=undersea ? 2 : world->HasBreathableAtmosphere() ? 0 : 1;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	AuMatrix4x4 actorToWorld; if(!actor.GetTransformRelativeToParent(actorToWorld, world)) return;
	for(const auto& building : m_Runtime->buildings)
	{
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		const auto local = transform.UnProject(location);
		const auto& dev=building.state;
		const auto* recipe=ConstructionRecipe(dev.industryId,dev.constructionProcessId);
		const double radius=dev.industryId==129 ? dev.biodomeRadiusMeters*FeetPerMeter() : recipe ? recipe->buildRadius*FeetPerMeter() : 0;
		const double halfWidth=dev.industryId==2 ? dev.roadWidthMeters*FeetPerMeter()/2 : radius;
		const double halfLength=dev.industryId==2 ? dev.roadLengthMeters*FeetPerMeter()/2 : radius;
		const AuPoint surfaceLocal=transform.UnProject(world->LocOfLL(query.m_GlobeLL,dev.altitudeMSL));
		const double gradeAltitude=dev.industryId==2 ? dev.roadStartAltitudeMSL+(dev.roadEndAltitudeMSL-dev.roadStartAltitudeMSL)
			*qBound(0.0,(surfaceLocal.y+halfLength)/(2*halfLength),1.0) : dev.altitudeMSL;
		const bool curvedSite=dev.industryId==2 || dev.industryId==129;
		const AuPoint& siteLocal=curvedSite ? surfaceLocal : local;
		const double siteHeight=curvedSite ? query.m_fAltitudeMSL-gradeAltitude : local.z;
		const bool close=location.Distance2(transform.Translation())<std::pow(2*(qMax(halfWidth,halfLength)+32.8084),2);
		if(close && !dev.demolished && (dev.industryId!=129 || query.m_LegacyDevelopmentId==0)
			&& std::abs(siteLocal.x)<=halfWidth+2.828427 && std::abs(siteLocal.y)<=halfLength+2.828427
			&& (dev.industryId!=129 || std::hypot(siteLocal.x,siteLocal.y)<=radius+AuLegacyBd::Biodome::ContainmentMargin)
			&& siteHeight>=(dev.minimumDeck-1)*AuLegacyBd::DeckHeight
			&& siteHeight<(dev.industryId==129 ? AuLegacyBd::Biodome::RoofHeight(radius,std::hypot(siteLocal.x,siteLocal.y))
				: (dev.maximumDeck+2)*AuLegacyBd::DeckHeight))
		{
			query.m_Environment|=EB_LegacyCity;
			if(dev.industryId==2) query.m_Environment|=EB_Road;
			query.m_LegacyCityId=Id(); query.m_LegacyDevelopmentId=dev.id; query.m_CityEmpire=EmpireId();
			query.m_InIndustryLL=dev.location;
			if(const auto* catalogue=AuLegacyCityCatalogue::BuiltIn())
				query.m_InIndustry=INDUSTRYID(catalogue->RuntimeIndustryId(AuLegacyCity::IndustryId(dev.industryId)));
		}
		// InsideEnclosure 0x29b6a0: completed pressurized roads use a five-
		// meter margin; completed ordinary building sites use half a meter.
		// Biodomes own their separate curved roof volume and are excluded.
		const double pressureMargin=dev.industryId==2 ? 16.4041994750656 : 1.64041994750656;
		const bool pressureSite=dev.constructed && !dev.demolished && dev.industryId!=129
			&& (dev.industryId!=2 || dev.constructionProcessId==7);
		const bool withinPressureHeight=query.m_fAltitudeMSL-gradeAltitude<=48;
		const bool cityPressure=enclosure && pressureSite && withinPressureHeight && std::abs(surfaceLocal.x)<=halfWidth+pressureMargin
			&& std::abs(surfaceLocal.y)<=halfLength+pressureMargin;
		const bool domePressure=dev.industryId==129 && dev.constructed && !dev.demolished
			&& std::hypot(surfaceLocal.x,surfaceLocal.y)<radius-AuLegacyBd::Biodome::ContainmentMargin
			&& query.m_fAltitudeMSL-dev.altitudeMSL<AuLegacyBd::Biodome::RoofHeight(radius,std::hypot(surfaceLocal.x,surfaceLocal.y));
		if(close && (cityPressure || domePressure))
		{
			query.m_EnclosureId=Id(); query.m_AtmType=ATMTYPE_Standard; query.m_fAirDensity=.5f;
			query.m_Environment|=EB_Breathable;
			if(enclosure==2 || (domePressure && undersea)) query.m_Environment&=~(EB_OnWater|EB_InWater|EB_MouthUnderwater);
		}
		if(!building.collision->IsValid()) continue;
		const auto up = transform.UnProjectVector(-world->GravityDir(location));
		AuPoint floorNormal;
		const double height = building.collision->HeightAboveFloor(local, up, floorNormal);
		// HeightAboveFloor rays from the insertion point, and the creature's
		// ground height already includes its CG offset. No second shift applies.
		// The native floor query subtracts one inch, matching the collision
		// resolver's support clearance and precise IsTouchingGround test.
		const double fall = height - actor.GroundHeightAboveCG() - 1./12.;
		if(!std::isfinite(height) || height >= 100000) continue;
		const AuLegacyBd::Geometry::FloorState* room = nullptr;
		for(const auto& floor : building.mesh.floors)
			if(floor.boundary.containsPoint(QPointF(local.x, local.y), Qt::OddEvenFill)
				&& local.z >= floor.deck * AuLegacyBd::DeckHeight - .25
				&& local.z < (floor.deck + 1) * AuLegacyBd::DeckHeight) { room = &floor; break; }
		if(!room && fall > query.m_FallDist + d_GroundTolerance) continue;
		query.m_FallDist = fall;
		query.m_Environment |= EB_LegacyCityFloor;
		query.m_fCeilingDist = std::min(double(query.m_fCeilingDist), building.collision->HeightOfCeiling(local, up));
		query.m_terNormal = actorToWorld.UnProjectVector(transform.ProjectVector(floorNormal));
		if(fall < d_GroundTolerance) query.m_OnGround = room ? OG_HardFloor : OG_Pavement;
		const auto* cell = PathCell(building.paths, local);
		if(cell && (cell->stairs || (cell->flags & 0xff00))) query.m_Environment |= EB_Stairs;
		if(room && room->roomType != 9 && (room->roomType < 22 || room->roomType > 24))
		{
			query.m_Environment |= EB_LegacyCity;
			if(cell && cell->type != 0 && cell->type != 1 && cell->type != 15 && cell->type != 18)
				query.m_Environment |= EB_Pathable;
			query.m_LegacyCityId = Id(); query.m_LegacyDevelopmentId = building.state.id;
			query.m_LegacyRoomType = room->roomType; query.m_CityEmpire = EmpireId();
			query.m_InIndustryLL = building.state.location;
			if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
				query.m_InIndustry = static_cast<INDUSTRYID>(catalogue->RuntimeIndustryId(AuLegacyCity::IndustryId{building.state.industryId}));
		}
	}
}

bool DaLegacyCity::Route(quint32 development, const AuPoint& from, const AuPoint& to,
	QVector<AuLegacyBd::Paths::RoutePoint>& points, bool useGroundLevelDoors) const
{
	points.clear();
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return false;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	for(const auto& building : m_Runtime->buildings)
		if(building.state.id == development)
		{
			AuMatrix4x4 transform;
			world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
				building.state.facingRadians, nullptr, AuPoint(), transform);
			if(!building.paths.Route(transform.UnProject(from), transform.UnProject(to), points, useGroundLevelDoors)) return false;
			for(auto& point : points) point.location = transform.Project(point.location);
			return true;
		}
	return false;
}

void DaLegacyCity::DeleteDeviceObjects(AuAtomRenderInfo& ari)
{
	UpdateSiteTerrain(ari, true);
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->DeleteDraw(ari, Id());
	DaAtom::DeleteDeviceObjects(ari);
}

void DaLegacyCity::RenderEnvironmentObject(const AuMatrix4x4& projection, AuAtomRenderInfo& ari,
	AuModelMatrixHash& matrices, const DaPerson* sceneAtom, AuRenderLastMap& last,
	RENDERPASS pass, DaAtomList& sc, DaAtomList& scFog, DaAtomList& scGlobe,
	DaAtomList& scGlobeFog, DaAtomList& scRing, DaAtomList& scRingFog)
{
	Q_UNUSED(last); Q_UNUSED(sc); Q_UNUSED(scFog); Q_UNUSED(scGlobe);
	Q_UNUSED(scGlobeFog); Q_UNUSED(scRing); Q_UNUSED(scRingFog);
	if(pass != PASS_Near || !ari.re || !g_Au.IsLegacyGeneration()) return;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto model = matrices.constFind(Id());
	if(!world || model == matrices.cend()) return;
	UpdateSiteTerrain(ari);
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	if(m_Runtime->drawRevision != m_Runtime->revision)
	{
		m_Runtime->DeleteDraw(ari, Id());
		for(const auto& building : m_Runtime->buildings)
		{
			// Asphalt is a pavement commodity, with no authored building atlas.
			// Road fixtures still use the shared original building texture slots.
			const quint16 atlasMaterial = building.state.industryId == 2 && building.state.materialId == 23
				? 76 : building.state.materialId;
			const QImage atlas(AuLegacyBd::Geometry::MaterialAtlas(atlasMaterial));
			for(const auto& batch : building.mesh.batches)
			{
				if(batch.collisionOnly) continue;
				QImage flagImage;
				if(batch.empireFlag)
				{
					const quint32 flagEmpire = EmpireId();
					const DgData& gd = g_Au.LockDataForRead(); gd.EmpireFlag(flagEmpire, flagImage); g_Au.UnlockData();
					if(flagImage.isNull()) continue;
				}
				const float flagRatio = flagImage.width() ? float(flagImage.height())/flagImage.width() : 1.f;
				QList<AuTriTexNS> triangles;
				const bool buildingAtlas = batch.textureFile.isEmpty() && batch.textureSlot >= 0;
				const QVector2D inset = buildingAtlas
					? AuLegacyBd::Geometry::TextureInset(batch.textureSlot) : QVector2D();
				const auto vertex = [inset,flagRatio,&batch](const AuLegacyShip::Rendering::Vertex& v) {
					const QPointF uv(inset.x() + v.uv.x() * (1 - 2 * inset.x()),
						inset.y() + v.uv.y() * (1 - 2 * inset.y()));
					QVector3D position=v.position;
					if(batch.empireFlag) position.setZ(31-(31-position.z())*flagRatio);
					return AuPointTexNS(Point(position), uv, Point(v.normal), 0); };
				for(int i=0; i+2<batch.vertices.size(); i+=3)
					// The shared CPU mesh uses CCW surfaces for collision. Submit
					// native 2016 CW faces without changing normals or UV bindings.
					triangles.append(AuTriTexNS(vertex(batch.vertices[i]), vertex(batch.vertices[i+2]), vertex(batch.vertices[i+1])));
				Runtime::DrawBatch draw; draw.development = building.state.id; draw.twoSided = batch.twoSided;
				draw.vao.CreateForModelTexFog(*ari.re, triangles, GL_STATIC_DRAW);
				QImage texture;
				if(batch.empireFlag) texture = flagImage;
				else if(!batch.textureFile.isEmpty()) texture.load(batch.textureFile);
				else if(batch.textureSlot < 0)
				{ texture = QImage(1,1,QImage::Format_ARGB32); texture.fill(Qt::white); }
				else
				{
					texture = atlas.copy(AuLegacyBd::Geometry::TextureTile(batch.textureSlot));
					if(batch.textureSlot == 16) texture = texture.mirrored(true, false);
				}
				draw.tint = batch.tint; draw.selfIllumination = batch.selfIllumination;
				// DpBuilding::RenderTexture (2016 AuAtom13 0x5eae80) uses
				// GL_LINEAR for both filters and no mipmaps on building atlases.
				draw.texture = ari.re->CreateTextureFromBitmap(Id().GetID(),
					QStringLiteral("legacy-city/%1/%2/%3").arg(building.state.materialId).arg(batch.textureSlot)
						.arg(batch.empireFlag ? QStringLiteral("flag-%1").arg(EmpireId()) : batch.textureFile),
					texture, GL_LINEAR, buildingAtlas ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR, !buildingAtlas);
				m_Runtime->drawBatches.append(draw);
			}
		}
		m_Runtime->drawRevision = m_Runtime->revision;
	}
	auto& shader = ari.re->GLSL().ModelTexColorShader();
	shader.Select(projection, *ari.re);
	float ambientR, ambientG, ambientB, fogDensity, fogR, fogG, fogB;
	bool sunsBlocked;
	world->SceneRenderParameters(sceneAtom, ambientR, ambientG, ambientB, fogDensity,
		fogR, fogG, fogB, ari.IsViewTopDown(), sunsBlocked);
	shader.SetAmbientLight(ambientR, ambientG, ambientB);
	const AuMatrix4x4 worldModel = model->Matrix() * Transform().Inverse();
	shader.SetDirectTextureGlobe(worldModel.Translation(),
		world->AtomType() == ATOM_WorldGlobe || world->AtomType() == ATOM_WorldGasGiant);
	if(sunsBlocked)
		for(int sun = 0; sun < MAX_SUNS; ++sun)
		{
			const AuGLSun& light = ari.re->Sun(sun);
			shader.SetSun(sun, false, light.m_Attenuation, light.m_ColorR, light.m_ColorG,
				light.m_ColorB, light.m_LocX, light.m_LocY, light.m_LocZ, light.m_DiskRadius);
		}
	const bool cullingEnabled = ari.re->glIsEnabled(GL_CULL_FACE);
	GLint previousFrontFace = GL_CW;
	ari.re->glGetIntegerv(GL_FRONT_FACE, &previousFrontFace);
	// All uploaded building batches use the original clockwise convention.
	ari.re->glFrontFace(GL_CW);
	for(const auto& draw : m_Runtime->drawBatches)
	{
		const auto found = std::find_if(m_Runtime->buildings.cbegin(), m_Runtime->buildings.cend(),
			[&draw](const Runtime::Building& b) { return b.state.id == draw.development; });
		if(found == m_Runtime->buildings.cend() || !draw.vao.VAO()) continue;
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(found->state.location, found->state.altitudeMSL),
			found->state.facingRadians, nullptr, AuPoint(), transform);
		const AuMatrix4x4 buildingModel = worldModel * transform;
		shader.SetModelView(buildingModel); shader.SetTexture(draw.texture);
		AuPoint ambientUp = buildingModel.ProjectVector(0, 0, 1);
		ambientUp.Unit();
		shader.SetAmbientLightUp(ambientUp.x, ambientUp.y, ambientUp.z);
		shader.SetColor(draw.tint.x(), draw.tint.y(), draw.tint.z(), draw.tint.w());
		shader.SetGlow(draw.selfIllumination, draw.selfIllumination, draw.selfIllumination);
		if(draw.twoSided && cullingEnabled) ari.re->glDisable(GL_CULL_FACE);
		draw.vao.SelectGL(); draw.vao.DrawGL(); draw.vao.DeselectGL();
		if(draw.twoSided && cullingEnabled) ari.re->glEnable(GL_CULL_FACE);
	}
	ari.re->glFrontFace(GLenum(previousFrontFace));
	if(sunsBlocked) shader.InvalidateSuns();
	shader.Deselect();
}

bool DaLegacyCity::RegisterPublicDesigns(QSqlDatabase& db, QString& error)
{
	if(!g_Au.IsLegacyGeneration()) return true;
	for(quint32 id=1; id<=74; ++id)
	{
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(id);
		QFile asset(QStringLiteral(":/legacy/cities/designs/B%1").arg(id));
		if(!design || !asset.open(QIODevice::ReadOnly))
		{ error = QStringLiteral("Missing packaged legacy building design %1.").arg(id); return false; }
		const auto bytes = asset.readAll();
		QSqlQuery write(db);
		write.prepare(QStringLiteral("INSERT INTO n_LegacyBdDesign(id,sourceId,contentHash,sourceData,name,industry,publicDefault) VALUES(?,?,?,?,?,?,true) ON CONFLICT(id) DO NOTHING"));
		write.addBindValue(id); write.addBindValue(design->header.designId); write.addBindValue(QString::fromLatin1(design->sourceHash.toHex()));
		write.addBindValue(bytes); write.addBindValue(design->header.name); write.addBindValue(design->header.industryId);
		if(!write.exec()) { error = write.lastError().text(); return false; }
		QSqlQuery read(db);
		read.prepare(QStringLiteral("SELECT sourceId,contentHash,sourceData,name,industry,publicDefault FROM n_LegacyBdDesign WHERE id=?"));
		read.addBindValue(id);
		if(!read.exec() || !read.next() || read.value(0).toUInt() != id
			|| read.value(1).toString().toLatin1() != design->sourceHash.toHex()
			|| QCryptographicHash::hash(read.value(2).toByteArray(), QCryptographicHash::Sha256) != design->sourceHash
			|| read.value(3).toString() != design->header.name || read.value(4).toUInt() != design->header.industryId
			|| !read.value(5).toBool())
		{ error = QStringLiteral("Stored legacy building design %1 conflicts with the packaged original.").arg(id); return false; }
	}
	return true;
}
