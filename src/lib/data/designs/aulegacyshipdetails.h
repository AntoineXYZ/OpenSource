#ifndef AuLegacyShipDetails_H
#define AuLegacyShipDetails_H

#include "aulegacyship.h"

namespace AuLegacyShip
{
// Replays the 2016 ordered equipment-validation pass after a room edit.
// Invalid equipment is removed; valid equipment can change type/room ID.
// An unavailable dependency returns false without changing the detail array.
bool ReconcileDetails(Design& candidate, QString& error);
}

#endif // AuLegacyShipDetails_H
