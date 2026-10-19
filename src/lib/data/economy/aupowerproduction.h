#ifndef AuPowerProduction_H
#define AuPowerProduction_H

#include "audata.h"
#include "audef.h"
#include "dbindustry.h"

namespace AuPowerProduction
{
// Shared modern/2016 rule: wind strengthens and sunlight weakens with the
// original five orbit zones. Integer division retains truncation of output.
AUDATA_DLL qint32 ScaleForOrbit(qint32 quantity, INDUSTRYID industry, ORBITZONE zone);
}
#endif
