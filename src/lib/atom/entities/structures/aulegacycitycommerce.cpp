#include "aulegacycitycommerce.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "auglobal.h"
#include "auitem.h"
#include "daatomlist.h"
#include "dalegacycity.h"
#include "dasolarsystem.h"
#include "daworldll.h"
#include "daworldringsection.h"
#include "dgdata.h"
#include <vector>

namespace AuLegacyCity
{
struct PowerImportBatch::Data
{
	struct Exporter
	{
		DaLegacyCity* city = nullptr;
		QReadWriteLock* lock = nullptr;
		State state;
		bool changed = false;
		~Exporter() { if(lock) lock->unlock(); }
	};
	DaLegacyCity* receiver = nullptr;
	std::vector<std::unique_ptr<Exporter>> exporters;
	bool committed = false;
};

PowerImportBatch::PowerImportBatch(DaLegacyCity& receiver) : m_Data(new Data)
{ m_Data->receiver = &receiver; }
PowerImportBatch::~PowerImportBatch() = default;

quint32 PowerImportBatch::Import(const State& state, AuItemHash& inventory,
	const QVector<CityConnection>& connections, quint32 demand, quint32 capacitor)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || !demand || m_Data->committed) return 0;
	const auto electricity = catalogue->RuntimeCommodityId(CommodityId(3));
	quint32 remaining = demand;
	for(const auto& connection : connections)
	{
		auto* city = connection.city;
		if(!(connection.flags & 1) || !city || city == m_Data->receiver || city->IsDead()) continue;
		Data::Exporter* exporter = nullptr;
		for(const auto& entry : m_Data->exporters)
			if(entry->city == city) { exporter = entry.get(); break; }
		if(!exporter)
		{
			if(!city->m_CityLock.tryLockForWrite()) continue;
			std::unique_ptr<Data::Exporter> entry(new Data::Exporter);
			entry->city = city; entry->lock = &city->m_CityLock; entry->state = city->m_City;
			exporter = entry.get();
			m_Data->exporters.push_back(std::move(entry));
		}
		State candidate = exporter->state;
		const auto stance = city->PoliticalStanceMakeContact(candidate.controllingEmpire, m_Data->receiver,
			state.controllingEmpire, &candidate.name, &state.name);
		const auto& data = g_Au.LockDataForRead();
		const bool restricted = data.EmpireCommerceRestricted(candidate.controllingEmpire, COMMODITYID(electricity), stance);
		g_Au.UnlockData();
		if(restricted) continue;
		AuItemHash exporterInventory; AccountingState accounting; QString error;
		if(!ReadCityInventory(candidate.inventory, *catalogue, exporterInventory, error)
			|| !DecodeAccounting(candidate.accounting, accounting, error)) continue;
		AuItemHash receiving(inventory);
		const auto transfer = TransferPower(exporterInventory, receiving, electricity, remaining, capacitor);
		if(!transfer.RemovedFromExporter()) continue;
		accounting.powerRequested += transfer.RemovedFromExporter();
		candidate.capacitor = float(exporterInventory.RawCount(COMMODITYID(electricity)));
		QByteArray receivingBytes, checked;
		if(!WriteCityInventory(receiving, receivingBytes, error)
			|| !WriteCityInventory(exporterInventory, candidate.inventory, error)
			|| !EncodeAccounting(accounting, candidate.accounting, error)
			|| !EncodeState(candidate, checked, error)) continue;
		inventory = receiving;
		remaining -= transfer.supplied;
		exporter->state = candidate;
		exporter->changed = true;
		if(!remaining && inventory.RawCount(COMMODITYID(electricity)) >= capacitor) break;
	}
	return demand - remaining;
}

void PowerImportBatch::Commit()
{
	if(m_Data->committed) return;
	for(const auto& entry : m_Data->exporters)
		if(entry->changed)
		{
			entry->state.lastVisit = qMax(entry->state.lastVisit, entry->city->m_City.lastVisit);
			entry->city->m_City = entry->state;
			++entry->city->m_CityRevision;
		}
	m_Data->committed = true;
}

void PowerImportBatch::Publish()
{
	for(const auto& entry : m_Data->exporters)
		if(entry->lock) { entry->lock->unlock(); entry->lock = nullptr; }
	if(m_Data->committed)
		for(const auto& entry : m_Data->exporters)
			if(entry->changed) entry->city->SetModified();
	m_Data->exporters.clear();
}

namespace
{
bool HasIndustry(const State& state, quint8 industry)
{
	for(const auto& dev : state.developments)
		if(dev.industryId == industry && dev.completedLevels && !dev.demolished) return true;
	return false;
}
}

QVector<CityConnection> QueryCityConnections(DaLegacyCity& city, DaWorldLL& world, const State& state)
{
	// DaCity::AnalyzeConnections, AuAtom13 0x26fb80. Airport111 and
	// Wharf34 enable the original air and sea routes independently of roads.
	QVector<CityConnection> result;
	DaAtomList atoms;
	if(auto* system = city.FindParentSolarSystem()) system->ListNormalSubatomsRecursive(atoms);
	else world.ListNormalSubatoms(atoms);
	const bool airport = HasIndustry(state, 111), seaport = HasIndustry(state, 34);
	const bool ringworld = dynamic_cast<DaWorldRingSection*>(&world) != nullptr;
	const auto* star = world.FindParentStarInEnv();
	for(auto* atom : atoms)
	{
		auto* other = dynamic_cast<DaLegacyCity*>(atom);
		if(!other || other == &city || other->IsDead()) continue;
		auto* otherWorld = dynamic_cast<DaWorldLL*>(other->Parent());
		if(!otherWorld) continue;
		const auto otherState = other->Snapshot();
		const bool bothAirports = airport && HasIndustry(otherState, 111);
		quint32 flags = bothAirports ? 8 : 0;
		if(otherWorld == &world)
		{
			if(bothAirports) flags |= 4;
			if(seaport && HasIndustry(otherState, 34)) flags |= 2;
			if(city.TestRoadConnection(*other)) flags |= 1;
		}
		else if(ringworld && star && otherWorld->FindParentStarInEnv() == star)
		{
			if(bothAirports) flags |= 0x20;
			if(seaport && HasIndustry(otherState, 34)) flags |= 0x10;
		}
		if(flags) { CityConnection connection; connection.city = other; connection.flags = flags; result.append(connection); }
	}
	return result;
}
}
