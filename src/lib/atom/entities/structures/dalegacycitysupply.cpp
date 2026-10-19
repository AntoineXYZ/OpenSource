#include "dalegacycity.h"
#include "aulegacycitysupply.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityproduction.h"
#include "daworldll.h"
#include <algorithm>

AuLegacyCity::DevelopmentSupplyFacts DaLegacyCity::GatherDevelopmentSupplyFacts(
	const AuLegacyCity::State& state, const AuLegacyCity::Development& development, qint64 now) const
{
	AuLegacyCity::DevelopmentSupplyFacts facts;
	facts.construction = GatherCitizenConstructionFacts(state, development, now, true);
	facts.ringworld = OnRingworld();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!world || development.demolished) return facts;
	AuGlobeLL cityLL;
	if(!world->LLOfLoc(cityLL, Location())) return facts;
	// DaWorldLL::FetchManufacturingMaterials 0x5164e0 always samples the
	// city's zone, with the receiving site's industry and darkness filters.
	world->QueryNaturalResources(facts.manufacturingNatural, world->ResourceZone(cityLL.Longitude()));
	facts.sunlightSubstitution = development.industryId == 66 ? 0 : facts.manufacturingNatural.m_SunlightQ;
	facts.waterSubstitution = development.industryId == 34 || development.industryId == 51
		|| development.industryId == 62 || development.industryId == 89 ? 0 : facts.manufacturingNatural.m_WaterInEnvironmentQ;
	if(world->Hydrographics() && world->AltitudeMSL(Location()) <= -32)
	{ facts.manufacturingNatural.m_Atmosphere = 0; facts.manufacturingNatural.m_Sunlight = 0; }
	else if(world->QuerySunlight(world->LocOfLL(development.location, development.altitudeMSL)) < .3f)
		facts.manufacturingNatural.m_Sunlight = 0;
	return facts;
}

bool DaLegacyCity::SupplyDevelopmentMaterials(AuLegacyCity::State& state,
	AuLegacyCity::Development& development, AuItemHash& inventory,
	AuLegacyCity::ProductionState& production, AuLegacyCity::AccountingState& accounting,
	const AuLegacyCity::DevelopmentSupplyFacts& facts,
	AuLegacyConstructionMaterials::Batch& materials) const
{
	using namespace AuLegacyCity;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || development.demolished || facts.construction.developmentId != development.id) return false;
	// AddCommodity 0x26e9b0 fetches for the deposit's development first,
	// without running processes, adding citizen labor, checking employment,
	// motivation, threat, or the construction's citizen-worker switch.
	bool physicalChanged = false;
	bool changed = ApplyCitizenConstruction(state, development, inventory, production,
		facts.construction, physicalChanged, materials, false);
	const quint16 slotCount = ManufacturingSlots(development.industryId, development.completedLevels);
	QVector<int> ordered;
	for(int index = 0; index < production.processes.size(); ++index)
	{
		const auto& process = production.processes[index];
		if(process.developmentId == development.id && process.slot < slotCount && process.enabled
			&& process.minimumTechnology <= development.technology) ordered.append(index);
	}
	std::stable_sort(ordered.begin(), ordered.end(), [&production](int first, int second) {
		return production.processes[first].slot < production.processes[second].slot;
	});
	for(const int index : ordered)
	{
		quint32 money = 0;
		changed |= FetchProductionMaterials(*catalogue, production.processes[index], inventory, production,
			facts.manufacturingNatural, accounting.accounts.government, money, facts.ringworld,
			facts.sunlightSubstitution, facts.waterSubstitution);
		if(money)
		{
			accounting.accounts.Debit(GovernmentExpense::Research, money);
			accounting.accounts.Credit(CitizenIncome::Salary, money, accounting.taxes);
			changed = true;
		}
	}
	return changed;
}
