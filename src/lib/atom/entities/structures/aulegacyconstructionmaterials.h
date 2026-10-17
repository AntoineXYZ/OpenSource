#ifndef AuLegacyConstructionMaterials_H
#define AuLegacyConstructionMaterials_H

#include "auatom.h"
#include "auid.h"
#include "aulegacycitystate.h"
#include <QVector>

class DaItem;
class DaWorldLL;
class DeProcessComponent;

namespace AuLegacyConstructionMaterials
{
// The caller keeps the world's atom-deletion lock. Collect before taking a
// city's write lock: an invalid cached environment can query city geometry.
AUATOM_DLL QVector<DaItem*> ItemsAtSite(DaWorldLL& world, const AuId& cityId,
	const AuLegacyCity::Development& development);

// Applies exact world item stacks and direct container contents. Only item
// locks are taken here. Depleted atoms remain alive until Finalize, so the
// caller can safely hold its city write lock while updating the component.
AUATOM_DLL quint32 Fetch(const QVector<DaItem*>& items, DeProcessComponent& component,
	QVector<DaItem*>& changedItems, QVector<DaItem*>& emptiedItems);

// Publish partial container/stack updates and kill depleted loose item atoms.
// Call after releasing the city and player-gear locks.
AUATOM_DLL void Finalize(const QVector<DaItem*>& changedItems, const QVector<DaItem*>& emptiedItems);
}

#endif
