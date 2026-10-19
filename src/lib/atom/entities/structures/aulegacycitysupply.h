#ifndef AuLegacyCitySupply_H
#define AuLegacyCitySupply_H

#include "aulegacycitizenconstruction.h"

namespace AuLegacyCity
{
// Gather outside city/gear locks while retaining atom deletion protection.
// Construction collection is unconditional even with pending labor, disabled
// citizen work or a nearby threat. Manufacturing uses its own dark/undersea
// natural-resource filters and substitutions.
struct DevelopmentSupplyFacts
{
	CitizenConstructionFacts construction;
	AuZoneResource manufacturingNatural;
	quint8 sunlightSubstitution = 0, waterSubstitution = 0;
	bool ringworld = false;
};
}
#endif
