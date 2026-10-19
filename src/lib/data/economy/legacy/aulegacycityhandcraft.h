#ifndef AuLegacyCityHandcraft_H
#define AuLegacyCityHandcraft_H

#include "aulegacycityproduction.h"
#include "augear.h"

namespace AuLegacyCity
{
struct HandcraftContext
{
	quint8 industry = 0, technology = 0;
	bool atIndustry = false, hasPower = false;
};

AUDATA_DLL bool HandcraftAllowed(const ManufacturingProcessRecord& recipe, const HandcraftContext& context);
// Preview and consumption share the original gear/one-level-container ordering.
// No material-quality failures or labor timer apply to personal handcrafting.
AUDATA_DLL bool PrepareHandcraft(const AuLegacyCityCatalogue& catalogue, quint16 recipe,
	const AuGear& gear, const HandcraftContext& context, ProductionProcess& process);
AUDATA_DLL void ConsumeHandcraft(const AuLegacyCityCatalogue& catalogue,
	const ProductionProcess& process, AuGear& gear);
AUDATA_DLL bool FinishHandcraft(const AuLegacyCityCatalogue& catalogue,
	ProductionProcess& process, ProductionOutput& output);
}
#endif
