#include "dalegacycity.h"
#include "auatomrenderinfo.h"
#include "aucityreport.h"
#include "aubox.h"
#include "auglobal.h"
#include "aulegacybd.h"
#include "aulegacybuildinggeometry.h"
#include "aulegacycitycatalogue.h"
#include "aulegacyshipequipment.h"
#include "aulegacysccollision.h"
#include "aunoise.h"
#include "autriangle.h"
#include "dacreature.h"
#include "daperson.h"
#include "daworldll.h"
#include <QDataStream>
#include <QCryptographicHash>
#include <QFile>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <cmath>

namespace
{
AuPoint Point(const QVector3D& p) { return AuPoint(p.x(), p.y(), p.z()); }

double TerrainLevelRadius(quint8 industry, const AuLegacyCityCatalogue& catalogue)
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
		if(p.industryId != q.industryId || p.materialId != q.materialId || p.location != q.location
			|| p.altitudeMSL != q.altitudeMSL || p.facingRadians != q.facingRadians
			|| p.constructed != q.constructed) return false;
	}
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
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(dev.designId);
		if(!design || design->header.industryId != dev.industryId
			|| AuLegacyBd::Geometry::MaterialAtlas(dev.materialId).isEmpty())
		{ error = QStringLiteral("Legacy development %1 has an incompatible design, industry or material.").arg(dev.id); return false; }
	}
	return true;
}
}

struct DaLegacyCity::Runtime
{
	struct Site
	{
		AuLegacyCity::Development state;
		AuMatrix4x4 placement, inverse;
		float radius = 0;
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
	QVector<Building> buildings;
	QVector<DrawBatch> drawBatches;
	quint64 siteRevision = 0, terrainRevision = 0, terrainDrawRevision = 0;
	const DaWorldLL* siteWorld = nullptr;
	QVector<AuLegacyCity::Development> siteState;
	QVector<Site> sites, renderedSites;

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
				const double radius = TerrainLevelRadius(dev.industryId, *catalogue);
				if(radius < 0) continue;
				Site site; site.state = dev;
				site.radius = float(radius) + 2.828427f; // InBuildingSite 0x5a0c90.
				world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL),
					dev.facingRadians, nullptr, AuPoint(), site.placement);
				site.inverse = site.placement.Inverse();
				sites.append(site);
			}
		++terrainRevision;
	}

	void Ensure(const DaLegacyCity& city)
	{
		QReadLocker stateLock(&city.m_CityLock);
		if(revision == city.m_CityRevision) return;
		buildings.clear();
		for(const auto& dev : city.m_City.developments)
		{
			const auto design = AuLegacyBd::Catalog::BuiltIn().Find(dev.designId);
			if(!design) continue; // SetState/loader rejects unknown designs before publication.
			Building building;
			building.state = dev;
			AuLegacyBd::Geometry::Options options;
			options.minimumDeck = dev.minimumDeck; options.maximumDeck = dev.maximumDeck;
			options.materialCommodity = dev.materialId; options.constructed = dev.constructed;
			building.mesh = AuLegacyBd::Geometry::Build(*design, options);
			if(dev.constructed)
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
				if(batch.doorId) continue;
				AuLegacyShip::Rendering::Batch imported;
				imported.vertices = batch.vertices;
				imported.material = batch.stairs ? AuLegacyShip::Rendering::Material::StairTread
					: AuLegacyShip::Rendering::Material::HullWall;
				collisionMesh.batches.append(imported);
			}
			building.collision.reset(new AuLegacyScGeometry::CollisionWorld(collisionMesh));
			buildings.append(building);
		}
		revision = city.m_CityRevision;
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
	for(const auto& site : m_Runtime->sites)
	{
		// The original projects at the site's altitude, not the incoming terrain
		// altitude. This keeps its square boundary stable on slopes and globes.
		const auto& dev = site.state;
		const AuPoint worldLocation = world.LocOfLL(ll, dev.altitudeMSL);
		// The old world's development lookup excluded distant sites before
		// InBuildingSite. A tangent-plane projection alone also hits antipodes.
		const double range = 2 * (site.radius + 32.8084);
		if(worldLocation.Distance2(site.placement.Translation()) > range * range) continue;
		const AuPoint p = site.inverse.Project(worldLocation);
		const float distance = std::max(float(std::abs(p.x)), float(std::abs(p.y)));
		const float margin = distance - site.radius;
		if(margin >= 32.8084f) continue;
		if(margin > 0)
		{
			// DpBuilding::AdjustAltMSL 0x597380: a level site takes priority
			// over all neighboring blend margins, including other cities.
			if(!levelSites)
				altitudeMSL = AuNoiseF::Cosine_Interpolate(float(altitudeMSL), float(dev.altitudeMSL),
					1.f - margin / 32.8084f);
			continue;
		}
		altitudeMSL = dev.altitudeMSL;
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
		const int slot = AuLegacyBd::Geometry::SiteTextureSlot(dev.materialId, dev.constructed);
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
		if(query.value(11).toUInt() != AuLegacyCity::StateVersion || !city->ApplyPayload(query.value(12).toByteArray(), failure))
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
			if(batch.collisionOnly || batch.doorId) continue;
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
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	AuMatrix4x4 actorToWorld; if(!actor.GetTransformRelativeToParent(actorToWorld, world)) return;
	for(const auto& building : m_Runtime->buildings)
	{
		if(!building.collision->IsValid()) continue;
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		const auto local = transform.UnProject(location);
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
		// Pressure and powered life support belong to city simulation, not an
		// unconditional breathable flag on a recovered visual building.
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
			const QImage atlas(AuLegacyBd::Geometry::MaterialAtlas(building.state.materialId));
			for(const auto& batch : building.mesh.batches)
			{
				if(batch.collisionOnly) continue;
				QList<AuTriTexNS> triangles;
				const bool buildingAtlas = batch.textureFile.isEmpty() && batch.textureSlot >= 0;
				const QVector2D inset = buildingAtlas
					? AuLegacyBd::Geometry::TextureInset(batch.textureSlot) : QVector2D();
				const auto vertex = [inset](const AuLegacyShip::Rendering::Vertex& v) {
					const QPointF uv(inset.x() + v.uv.x() * (1 - 2 * inset.x()),
						inset.y() + v.uv.y() * (1 - 2 * inset.y()));
					return AuPointTexNS(Point(v.position), uv, Point(v.normal), 0); };
				for(int i=0; i+2<batch.vertices.size(); i+=3)
					// The shared CPU mesh uses CCW surfaces for collision. Submit
					// native 2016 CW faces without changing normals or UV bindings.
					triangles.append(AuTriTexNS(vertex(batch.vertices[i]), vertex(batch.vertices[i+2]), vertex(batch.vertices[i+1])));
				Runtime::DrawBatch draw; draw.development = building.state.id; draw.twoSided = batch.twoSided;
				draw.vao.CreateForModelTexFog(*ari.re, triangles, GL_STATIC_DRAW);
				QImage texture;
				if(!batch.textureFile.isEmpty()) texture.load(batch.textureFile);
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
						.arg(batch.textureFile),
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
