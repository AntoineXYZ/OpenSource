#ifndef AuLegacyCityCommerce_H
#define AuLegacyCityCommerce_H

#include "auatom.h"
#include "aulegacycitystate.h"
#include <memory>

class AuItemHash;
class DaLegacyCity;
class DaWorldLL;

namespace AuLegacyCity
{
struct CityConnection
{
	DaLegacyCity* city = nullptr;
	quint32 flags = 0;
};

// Exporter locks and debits are staged until the receiving city's complete
// transaction has passed serialization. Abandoning the batch changes no city.
class AUATOM_DLL PowerImportBatch
{
public:
	explicit PowerImportBatch(DaLegacyCity& receiver);
	~PowerImportBatch();
	quint32 Import(const State& receiverState, AuItemHash& inventory,
		const QVector<CityConnection>& connections, quint32 demand, quint32 capacitor);
	// Receiver is committed while its lock and all exporter locks are held.
	void Commit();
	// Call after releasing the receiver lock; releases exporter locks and
	// publishes database/scene notifications only for committed transfers.
	void Publish();
private:
	struct Data;
	std::unique_ptr<Data> m_Data;
};

// Caller holds the scene's atom-deletion guard. Gather outside city locks.
AUATOM_DLL QVector<CityConnection> QueryCityConnections(DaLegacyCity& city, DaWorldLL& world, const State& state);
}

#endif
