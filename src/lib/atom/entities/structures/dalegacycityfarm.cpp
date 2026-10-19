#include "aulegacycityfarm.h"
#include "aulegacycityproduction.h"
#include "aulegacycitypopulationruntime.h"
#include "auanimalia.h"
#include "audna.h"
#include "auatompos.h"
#include "audice.h"
#include "auether.h"
#include "auglobal.h"
#include "auitem.h"
#include "auterraininfo.h"
#include "daanimal.h"
#include "daatomlist.h"
#include "daitem.h"
#include "dalegacycity.h"
#include "daworldll.h"
#include "defauna.h"
#include "dhdna.h"
#include "dpbody.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace AuLegacyCity
{
namespace
{
quint64 FarmKey(quint32 development, quint16 slot) { return (quint64(development) << 16) | slot; }

bool SupportedSpecies(const AuSpecies& species)
{
	std::unique_ptr<AuDNABase> dna;
	if(!ReadSpeciesDNA(species.Key(), dna)) return false;
	const auto* dna3 = dynamic_cast<const DhDNA*>(dna.get());
	return !dna3 || *dna3 != DhDNA();
}

float AnimalGroundHeight(DaAnimal& animal, AuDNABase::CHROMATID chromatid, HANDEDNESS hand)
{
	// Use the staged animal's actual DNA, including the retained DNA4 size
	// variation. The two existing body implementations supply posture and CG.
	std::unique_ptr<AuDNABase> dna(animal.DNAnew());
	if(const auto* dna3 = dynamic_cast<const DhDNA*>(dna.get()))
	{
		DpBody body(char(chromatid), bool(hand), *dna3);
		if(dna->EcoRole() == ECO_Herbivore) body.PoseEatOffGround();
		else body.Pose(AuPose::Stand, false, 1);
		return DaCreature::CGHeight(body, 0, 0, false);
	}
	if(const auto* dna4 = dynamic_cast<const AuDNA*>(dna.get()))
	{
		AuAnimalia body(*dna4, chromatid, hand, Mature);
		if(dna->EcoRole() == ECO_Herbivore) body.PoseEatOffGround(animal.Id());
		else body.Pose(animal.Id(), AuPose::Stand, false, 1);
		return DaCreature::CGHeight(body, 0, 0, false);
	}
	return 0;
}

struct Animal
{
	DaAnimal* atom = nullptr;
	AuGlobeLLF site, globe;
	AuPoint location;
	AuSpecies species;
	AuDNABase::CHROMATID chromatid = AuDNABase::Alpha;
	quint32 empire = 0;
};

struct Cage
{
	DaItem* atom = nullptr;
	AuGlobeLLF site;
	AuPoint location;
	std::shared_ptr<AuItem> item;
	bool alive = true;
};

struct Building
{
	const DaLegacyCity* city = nullptr;
	Development development;
	AuMatrix4x4 transform;
	double radius = 0;
};

double BuildRadius(const AuLegacyCityCatalogue& catalogue, const Development& development)
{
	if(development.industryId == 129) return development.biodomeRadiusMeters * FeetPerMeter();
	const auto* recipe = development.constructionProcessId
		? catalogue.ConstructionProcess(ConstructionProcessId(development.constructionProcessId))
		: catalogue.ConstructionProcessForIndustry(IndustryId(development.industryId));
	return recipe ? recipe->buildRadius * FeetPerMeter() : 0;
}

double LevelRadius(const Development& development, double buildRadius)
{
	// DpDevelopment::TerrainLevelRadius 0xfe610.
	switch(development.industryId)
	{
	case 20: return 26.24672f;
	case 30: case 32: return 42.650917f;
	case 31: case 50: case 51: return 32.8084f;
	case 66: case 67: return 49.2126f;
	case 113: return 65.6168f;
	case 114: return 9.84252f;
	default: return float(buildRadius);
	}
}

AuGlobeLLF RandomLocation(DaWorldLL& world, const Building& building, double minimum, double maximum)
{
	// GenRandomLocation 0x519130, verified assembly 180519398..18051946a:
	// each axis is minimum + uniform[-(maximum-minimum), maximum-minimum]
	// on a centifoot grid. It is neither a disk nor an annulus.
	const int extent = int((maximum - minimum) * 100.0);
	const double x = minimum + g_Au.Dice().Roll(1, extent * 2 + 1, -extent - 1) / 100.0;
	const double y = minimum + g_Au.Dice().Roll(1, extent * 2 + 1, -extent - 1) / 100.0;
	AuGlobeLLF ll;
	if(!world.LLOfLoc(ll, building.transform.Project(AuPoint(x, y, 0))))
		world.LLOfLoc(ll, building.transform.Translation());
	return ll;
}

const Building* ContainingBuilding(DaWorldLL& world, const QVector<Building>& buildings, const AuGlobeLLF& ll,
	bool finishedOnly = true)
{
	// Original FIND_FinishedLevel | FIND_BuildingsOnly (6), 2D Contains.
	// Biodomes are considered after ordinary building sites.
	for(int first = 0; first < buildings.size();)
	{
		int end = first + 1;
		while(end < buildings.size() && buildings[end].city == buildings[first].city) ++end;
		for(bool biodomes : {false, true})
		for(int index = first; index < end; ++index)
		{
			const auto& building = buildings[index];
			if(building.development.demolished) continue;
			if(finishedOnly && !building.development.completedLevels) continue;
			if((building.development.industryId == 129) != biodomes) continue;
			const auto local = building.transform.UnProject(world.LocOfLL(ll, building.development.altitudeMSL));
			if(biodomes ? std::hypot(local.x, local.y) <= building.radius
				: double(std::max(float(std::abs(local.x)), float(std::abs(local.y)))) <= building.radius)
				return &building;
		}
		first = end;
	}
	return nullptr;
}
}

struct FarmProductionBatch::Data
{
	struct Site
	{
		Development development;
		Building building;
		bool nativeFauna = false;
		TERRAIN terrain = TERRAIN(0);
		float sunlight = 0;
	};

	struct Attempt
	{
		quint64 key = 0;
		int site = -1, limit = 0;
		FarmProductionResult result;
		QVector<int> adopt;
		int cage = -1;
		quint8 cageHitPoints = 0;
		bool cageSurvived = true;
		std::unique_ptr<DaAnimal> animal;
		Animal birth;
		bool exactLocation = false;
		AuPoint location;
		DaAnimal* locationFrom = nullptr;
		AuGlobeLLF spawnLL;
		float height = 0;
		double rotation = 0;
		bool prepared = false, applied = false;
	};

	DaWorldLL* world = nullptr;
	const DaLegacyCity* city = nullptr;
	quint32 empire = 0;
	QVector<Site> sites;
	QVector<Animal> animals;
	QVector<Cage> cages;
	QVector<Building> buildings;
	std::shared_ptr<DePlanetFauna> fauna;
	std::vector<Attempt> attempts;
	QVector<int> appliedOrder;
	bool published = false;

	void PrepareAttempt(Attempt& attempt, const State* current)
	{
		if(attempt.prepared) return;
		attempt.prepared = true;
		if(current)
		{
			empire = current->controllingEmpire;
			// Only the calling city's level/technology fields can change during
			// this transaction. Placement frames were gathered before its lock.
			for(auto& building : buildings)
				if(building.city == city)
					for(const auto& development : current->developments)
						if(development.id == building.development.id)
						{ building.development = development; break; }
		}
		const auto& site = sites[attempt.site];
		const auto& development = site.development;
		int count = 0;
		const Animal* previous = nullptr;
		for(int index = 0; index < animals.size(); ++index)
		{
			const auto& animal = animals[index];
			if(!animal.empire || animal.site != development.location) continue;
			attempt.adopt.append(index);
			previous = &animal;
			++count;
		}
		int cageIndex = -1;
		for(int index = 0; index < cages.size(); ++index)
			if(cages[index].alive && cages[index].site == development.location)
			{ cageIndex = index; break; }
		const int target = attempt.limit + (cageIndex >= 0 ? 1 : 0);
		if(count >= target)
		{
			attempt.result.status = 1;
			if(previous) attempt.result.species = previous->species;
			return;
		}
		AuDNABase::CHROMATID chromatid;
		if(cageIndex >= 0)
		{
			const auto& cage = cages[cageIndex];
			attempt.result.species = cage.item->DNASpecies();
			attempt.cage = cageIndex;
			std::unique_ptr<AuItem> used(cage.item->Clone());
			attempt.cageSurvived = used->UseItem();
			attempt.cageHitPoints = used->HitPoints();
			chromatid = AuDNABase::CHROMATID(g_Au.Dice().Roll(1, 2, -1));
		}
		else if(previous)
		{
			attempt.result.species = previous->species;
			chromatid = previous->chromatid == AuDNABase::Alpha ? AuDNABase::Beta : AuDNABase::Alpha;
		}
		else
		{
			if(!site.nativeFauna || !fauna) return;
			const auto encounter = fauna->NewFarmAnimal(site.sunlight, site.terrain);
			attempt.result.species = DhDNA(encounter.DNA()).Species();
			chromatid = AuDNABase::CHROMATID(g_Au.Dice().Roll(1, 2, -1));
		}
		if(!SupportedSpecies(attempt.result.species)) return;
		const auto hand = HANDEDNESS(g_Au.Dice().Roll(1, 2, -1));
		attempt.animal.reset(new DaAnimal(attempt.result.species, chromatid, hand, Mature));
		attempt.animal->Create(); // Reserve identity; no Ether insertion or signals.
		attempt.height = AnimalGroundHeight(*attempt.animal, chromatid, hand);
		if(cageIndex >= 0)
		{
			attempt.exactLocation = true;
			attempt.location = cages[cageIndex].location;
			world->LLOfLoc(attempt.spawnLL, attempt.location);
		}
		else
		{
			const Building* building = previous ? ContainingBuilding(*world, buildings, previous->globe) : &site.building;
			if(building)
			{
				double minimum = 16.4041994750656, maximum = 32.8083989501312;
				if(!previous)
				{
					const double radius = (building->radius + LevelRadius(development, building->radius)) * .5;
					minimum = radius - 32.8083989501312;
					maximum = radius + 32.8083989501312;
				}
				attempt.spawnLL = RandomLocation(*world, *building, minimum, maximum);
			}
			else
			{
				// Original replication keeps the animal's exact location when
				// it is not inside a completed building.
				attempt.exactLocation = true;
				attempt.location = previous ? previous->location : AuPoint(1, 1, 1);
				if(previous && previous->atom && !previous->atom->Ether()) attempt.locationFrom = previous->atom;
				world->LLOfLoc(attempt.spawnLL, attempt.location);
			}
		}
		attempt.rotation = g_Au.Dice().Roll(1, 360, -1) * DEG2RAD;
		attempt.birth.site = AuGlobeLLF();
		attempt.birth.globe = attempt.spawnLL;
		if(const auto* occupied = ContainingBuilding(*world, buildings, attempt.spawnLL, false))
			attempt.birth.site = occupied->development.location;
		// Altitude does not affect the original two-dimensional site lookup.
		// A provisional MSL position supports a later replication fallback;
		// actual ground height is resolved only after committing city state.
		attempt.birth.location = attempt.exactLocation ? attempt.location
			: world->LocOfLL(attempt.spawnLL, development.altitudeMSL + attempt.height);
		attempt.birth.species = attempt.result.species;
		attempt.birth.chromatid = chromatid;
		attempt.birth.empire = empire;
		attempt.birth.atom = attempt.animal.get();
		attempt.result.status = 0;
	}
};

FarmProductionBatch::FarmProductionBatch() : m_Data(std::make_shared<Data>()) {}
FarmProductionBatch::~FarmProductionBatch() = default;

const FarmProductionResult* FarmProductionBatch::Find(quint32 developmentId, quint16 slot, const State* current) const
{
	for(auto& attempt : m_Data->attempts)
		if(attempt.key == FarmKey(developmentId, slot))
		{
			m_Data->PrepareAttempt(attempt, current);
			return &attempt.result;
		}
	return nullptr;
}

void FarmProductionBatch::MarkApplied(quint32 developmentId, quint16 slot)
{
	for(int attemptIndex = 0; attemptIndex < int(m_Data->attempts.size()); ++attemptIndex)
	{
		auto& attempt = m_Data->attempts[attemptIndex];
		if(attempt.key == FarmKey(developmentId, slot))
		{
			if(!attempt.prepared || attempt.applied) return;
			attempt.applied = true;
			m_Data->appliedOrder.append(attemptIndex);
			for(int index : attempt.adopt) m_Data->animals[index].empire = m_Data->empire;
			if(attempt.cage >= 0)
			{
				auto& cage = m_Data->cages[attempt.cage];
				cage.item->SetHitPoints(attempt.cageHitPoints);
				cage.alive = attempt.cageSurvived;
			}
			if(attempt.animal) m_Data->animals.append(attempt.birth);
			return;
		}
	}
}

void FarmProductionBatch::Publish(quint32 actualEmpire)
{
	if(m_Data->published || !m_Data->world || !m_Data->world->Ether()) return;
	m_Data->published = true;
	for(int attemptIndex : m_Data->appliedOrder)
	{
		auto& attempt = m_Data->attempts[attemptIndex];
		if(!attempt.applied) continue;
		for(int index : attempt.adopt)
		{
			auto* animal = m_Data->animals[index].atom;
			if(animal && !animal->IsDead()) animal->SetEmpireId(actualEmpire);
		}
		if(attempt.cage >= 0)
		{
			auto* cage = m_Data->cages[attempt.cage].atom;
			if(cage && !cage->IsDead())
			{
				// Publish the single quality/wear trial made on an owned copy.
				auto* item = cage->LockItemForWrite();
				if(item) item->SetHitPoints(attempt.cageHitPoints);
				cage->UnlockItem();
				if(attempt.cageSurvived) cage->NotifyItemChanged();
				else cage->Kill();
			}
		}
		if(!attempt.animal) continue;
		auto* animal = attempt.animal.release();
		const AuPoint location = attempt.locationFrom ? attempt.locationFrom->Location() : attempt.exactLocation ? attempt.location
			: m_Data->world->GetSurfaceLocation(attempt.spawnLL, attempt.height);
		AuMatrix4x4 placement;
		placement.Translate(location);
		placement *= AuPoint(0, 0, 1).GetAlignmentTransform(-m_Data->world->GravityDir(location));
		placement.RotateZ(attempt.rotation);
		animal->ConnectSignals();
		animal->SetPosAndOrient(AuAtomPos(m_Data->world->Id(), placement));
		m_Data->world->Ether()->AddAtom(animal);
		animal->SetEmpireId(actualEmpire);
		if(animal->EcoRole() == ECO_Herbivore) animal->EatOffGround();
		else animal->Stand();
		animal->ResetHitPoints();
		animal->ResetHunger();
		animal->ResetStamina();
		g_Au.AtomAdded(animal->Id());
		animal->SetNormal();
		animal->SetInMotion();
		animal->TickTock();
	}
}

bool PrepareFarmProduction(DaLegacyCity& city, DaWorldLL& world, const State& state,
	const ProductionState& production, qint64 now, FarmProductionBatch& output, QString& error)
{
	error.clear();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || !world.Ether() || city.Parent() != &world || now < 0)
	{ error = QStringLiteral("Invalid legacy livestock transaction context."); return false; }
	FarmProductionBatch batch;
	batch.m_Data->world = &world;
	batch.m_Data->city = &city;
	batch.m_Data->empire = state.controllingEmpire;
	for(const auto& development : state.developments)
	{
		if(development.demolished || !development.completedLevels || development.industryId == 2) continue;
		int siteIndex = -1;
		for(const auto& process : production.processes)
		{
			if(process.developmentId != development.id || !process.completeAt || now < process.completeAt) continue;
			const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(process.processId));
			const int limit = recipe ? ProductionFarmAnimalLimit(development.industryId, recipe->commodity.value) : 0;
			if(!limit) continue;
			if(siteIndex < 0)
			{
				FarmProductionBatch::Data::Site site;
				site.development = development;
				site.building.city = &city;
				site.building.development = development;
				site.building.radius = BuildRadius(*catalogue, development);
				const auto location = world.LocOfLL(development.location, development.altitudeMSL);
				world.BuildingConstructionTransform(location, development.facingRadians, nullptr, AuPoint(), site.building.transform);
				site.nativeFauna = world.CanHaveFaunaAtElev(world.TerrainElevationType(float(world.AltitudeMSL(location))));
				AuTerrainInfo terrain; world.QueryTerrain(terrain, development.location);
				site.terrain = terrain.TerrainType();
				site.sunlight = world.QuerySunlightAtSeaLevel(location);
				siteIndex = batch.m_Data->sites.size();
				batch.m_Data->sites.append(site);
			}
			FarmProductionBatch::Data::Attempt attempt;
			attempt.key = FarmKey(development.id, process.slot);
			attempt.site = siteIndex;
			attempt.limit = limit;
			batch.m_Data->attempts.push_back(std::move(attempt));
		}
	}
	if(batch.m_Data->attempts.empty()) { output = batch; return true; }
	auto* fauna = world.LockFaunaForWrite();
	if(fauna) batch.m_Data->fauna.reset(new DePlanetFauna(*fauna));
	world.UnlockFauna();
	DaAtomList atoms;
	world.ListNormalSubatoms(atoms);
	for(auto* atom : atoms)
	{
		if(auto* animal = dynamic_cast<DaAnimal*>(atom))
		{
			if(animal->IsWild() || animal->CurrentHitPoints() <= 0) continue;
			Animal found;
			found.atom = animal; found.site = animal->InIndustryLL(); found.globe = animal->GlobeLL();
			found.location = animal->Location(); found.species = animal->Species();
			found.chromatid = animal->Chromatid(); found.empire = animal->EmpireId();
			batch.m_Data->animals.append(found);
		}
		else if(auto* item = dynamic_cast<DaItem*>(atom))
		{
			const auto* value = item->LockItem();
			std::shared_ptr<AuItem> copied;
			if(value && value->CommodityId() == COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(285)))
				&& !value->DNAIsSentient()) copied.reset(value->Clone());
			item->UnlockItem();
			if(copied && SupportedSpecies(copied->DNASpecies()))
			{
				Cage cage;
				cage.atom = item; cage.site = item->InIndustryLL(); cage.location = item->Location(); cage.item = copied;
				batch.m_Data->cages.append(cage);
			}
		}
		if(auto* other = dynamic_cast<DaLegacyCity*>(atom))
		{
			const State otherState = other == &city ? state : other->Snapshot();
			QVector<Development> ordered = otherState.developments;
			std::stable_sort(ordered.begin(), ordered.end(), [](const Development& a, const Development& b) { return a.location < b.location; });
			for(const auto& development : ordered)
			{
				if(development.demolished || development.industryId == 2) continue;
				Building building; building.city = other; building.development = development;
				building.radius = BuildRadius(*catalogue, development);
				world.BuildingConstructionTransform(world.LocOfLL(development.location, development.altitudeMSL),
					development.facingRadians, nullptr, AuPoint(), building.transform);
				batch.m_Data->buildings.append(building);
			}
		}
	}
	output = batch;
	return true;
}
}
