#ifndef AuLegacyCityInventory_H
#define AuLegacyCityInventory_H

#include "audata.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinfrastructure.h"

class AuItem;
class AuItemHash;

namespace AuLegacyCity
{
// Shares the existing AuItem stream format with stage-4 city inventories.
// Reads publish atomically, after bounds, commodity and stream validation.
AUDATA_DLL bool ReadCityInventory(const QByteArray& bytes, const AuLegacyCityCatalogue& catalogue,
	AuItemHash& output, QString& error);
AUDATA_DLL bool WriteCityInventory(const AuItemHash& inventory, QByteArray& output, QString& error);
AUDATA_DLL bool IsUpgradeModule(CommodityId historicalId);
AUDATA_DLL bool IsObsoleteCityItem(const AuLegacyCityCatalogue& catalogue, const AuItemHash& inventory,
	const AuItem& item, bool hasBreathableAtmosphere, quint8 ecologicalRole, quint32 storageCapacity);
// CityCycle discards whole obsolete stacks before its inventory snapshot.
AUDATA_DLL void DiscardObsoleteCityItems(const AuLegacyCityCatalogue& catalogue, AuItemHash& inventory,
	bool hasBreathableAtmosphere, quint8 ecologicalRole, quint32 storageCapacity);
AUDATA_DLL quint32 TestFitCityCommodity(const AuLegacyCityCatalogue& catalogue, const AuItemHash& inventory,
	const AuItem& item, const StorageCapacities& capacity, bool hasBreathableAtmosphere, quint8 ecologicalRole);
// Original AddCommodity accepts the entire offer, then discards the worst
// excess stacks of that commodity. Capacity is per commodity, not total cargo.
AUDATA_DLL bool AddCityCommodity(const AuLegacyCityCatalogue& catalogue, AuItemHash& inventory,
	const AuItem& item, const StorageCapacities& capacity, bool hasBreathableAtmosphere, quint8 ecologicalRole);
}

#endif
