#ifndef AuLegacyConstructionMaterials_H
#define AuLegacyConstructionMaterials_H

#include "auatom.h"
#include "auid.h"
#include "aulegacycitystate.h"
#include <QVector>
#include <memory>

class DaItem;
class DaWorldLL;
class AuLegacyCityCatalogue;
class QString;
namespace AuLegacyCity { struct ProductionProcess; }

namespace AuLegacyConstructionMaterials
{
// The caller keeps the world's atom-deletion lock. Collect before taking a
// city's write lock: an invalid cached environment can query city geometry.
AUATOM_DLL QVector<DaItem*> ItemsAtSite(DaWorldLL& world, const AuId& cityId,
	const AuLegacyCity::Development& development);

// One batch spans a complete city transaction. Fetch edits owned clones only,
// reusing each clone across citizen construction and deposit supply callbacks.
// Commit validates all observed items before replacing any live item. Call it
// after city serialization and before publishing city/power/farm changes.
class AUATOM_DLL Batch
{
public:
	Batch();
	~Batch();
	Batch(const Batch&) = delete;
	Batch& operator=(const Batch&) = delete;
	bool Fetch(const QVector<DaItem*>& items, const AuLegacyCityCatalogue& catalogue,
		AuLegacyCity::ProductionProcess& components);
	bool Commit(QString& error);
	// Notify updates/kill consumed loose atoms after publishing the city and
	// releasing city/gear locks. Atom-deletion protection remains caller-owned.
	void Finalize();
private:
	struct Data;
	std::unique_ptr<Data> m_Data;
};
}

#endif
