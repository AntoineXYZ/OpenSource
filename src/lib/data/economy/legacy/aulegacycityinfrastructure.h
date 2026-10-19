#ifndef AuLegacyCityInfrastructure_H
#define AuLegacyCityInfrastructure_H

#include "audata.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycitystate.h"

class AuItemHash;

namespace AuLegacyCity
{
struct StorageCapacities
{
	quint32 cargo = 0;
	quint32 electricity = 0;
};

struct PowerTransfer
{
	quint32 supplied = 0; // Satisfies demand immediately, without storing it.
	quint32 charged = 0;  // Remaining quota placed in the receiving capacitor.
	quint32 RemovedFromExporter() const { return supplied + charged; }
};

struct WorkforceCondition
{
	bool damaged = false, destroyed = false, burning = false;
};

struct EmploymentTotals
{
	quint32 jobs = 0, nonConstructionJobs = 0, repairJobs = 0, unemployed = 0;
};

// Original DpBuilding manufacturing slots, independent of process selection.
AUDATA_DLL quint16 ManufacturingSlots(quint8 industry, quint8 completedLevels);
AUDATA_DLL StorageCapacities DevelopmentStorage(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint8 buildingTechnology, quint8 orbitZone);
AUDATA_DLL StorageCapacities CityStorage(const AuLegacyCityCatalogue& catalogue,
	const QVector<Development>& developments, quint8 orbitZone);
AUDATA_DLL quint32 BuildingPowerDemand(const Development& development, bool isDark);
AUDATA_DLL quint32 RoadPowerDemand(const Development& development, bool isDark);

AUDATA_DLL quint32 IndustryJobs(const AuLegacyCityCatalogue& catalogue, quint8 industry);
AUDATA_DLL quint32 DevelopmentProductionJobs(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint32 enabledProcessCount);
AUDATA_DLL quint32 DevelopmentNonConstructionJobs(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint32 enabledProcessCount);
AUDATA_DLL quint32 DevelopmentJobs(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint32 enabledProcessCount);
// Job counts are keyed by city-local development ID. Allocation orders buildings
// and roads separately by their original LL map order; reverse changes each
// pass's direction and moves the nonessential road pass before buildings.
AUDATA_DLL EmploymentTotals EmployCitizens(const AuLegacyCityCatalogue& catalogue,
	QVector<Development>& developments, const QHash<quint32, quint32>& enabledProcessCounts,
	quint32 population, bool airProductionNeeded, bool reverse,
	const QHash<quint32, WorkforceCondition>& conditions = {});

// Tock checks a minute boundary and offsets each city's six-minute work by ID.
AUDATA_DLL bool CommerceCycleDue(qint64 previousMilliseconds, qint64 nowMilliseconds, quint32 cityId);
AUDATA_DLL quint32 IndustryImportPercentage(quint8 industry, quint32 completedLevels);
AUDATA_DLL quint32 ConnectionImportPercentage(quint32 percentage, quint32 connectionFlags);

AUDATA_DLL PowerTransfer PlanPowerTransfer(quint32 exporterElectricity, quint32 demand,
	quint32 receiverFreeCapacity);
// Caller owns locking, road connectivity, commerce permissions and ringworld
// exclusion. Electricity uses the explicit historical-to-runtime crosswalk.
AUDATA_DLL PowerTransfer TransferPower(AuItemHash& exporter, AuItemHash& receiver,
	quint16 runtimeElectricity, quint32 demand, quint32 receiverCapacitor);
}

#endif
