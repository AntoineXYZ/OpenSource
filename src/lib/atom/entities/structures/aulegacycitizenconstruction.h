#ifndef AuLegacyCitizenConstruction_H
#define AuLegacyCitizenConstruction_H

#include "auzoneresource.h"
#include <QVector>

class DaItem;

namespace AuLegacyCity
{
// World/environment queries are gathered before entering a city transaction.
// The caller retains the world's atom-deletion protection until finalization.
struct CitizenConstructionFacts
{
	quint32 developmentId = 0;
	quint8 completedLevels = 0, processId = 0, environment = 1;
	quint32 nextPreparation = 1;
	qint64 now = 0;
	bool blocked = false, ringworld = false;
	AuZoneResource natural;
	QVector<DaItem*> groundItems;
};
}
#endif
