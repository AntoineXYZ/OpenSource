#ifndef AuLegacyCityFarm_H
#define AuLegacyCityFarm_H

#include "auatom.h"
#include "auspecies.h"
#include <memory>

class DaLegacyCity;
class DaWorldLL;
namespace AuLegacyCity
{
struct State;
struct ProductionState;

struct FarmProductionResult
{
	quint8 status = 2; // 0 spawned, 1 existing livestock limit, 2 no suitable fauna.
	AuSpecies species;
};

// Owns unpublished births and the associated deferred ownership/cage wear.
// Prepare outside city locks while holding normal atom-deletion protection.
// Find resolves an actual attempt from owned facts and pure world coordinate
// math; MarkApplied advances only selected attempts. Both are safe in the city
// transaction. Terrain surface queries and publication happen only afterward.
class AUATOM_DLL FarmProductionBatch
{
public:
	FarmProductionBatch();
	~FarmProductionBatch();
	const FarmProductionResult* Find(quint32 developmentId, quint16 slot, const State* current = nullptr) const;
	void MarkApplied(quint32 developmentId, quint16 slot);
	void Publish(quint32 actualEmpire); // Call only after committing city state.
private:
	struct Data;
	std::shared_ptr<Data> m_Data;
	friend bool PrepareFarmProduction(DaLegacyCity&, DaWorldLL&, const State&,
		const ProductionState&, qint64, FarmProductionBatch&, QString&);
};

AUATOM_DLL bool PrepareFarmProduction(DaLegacyCity& city, DaWorldLL& world, const State& state,
	const ProductionState& production, qint64 now, FarmProductionBatch& output, QString& error);
}
#endif
