#ifndef AuLegacyShipEdit_H
#define AuLegacyShipEdit_H

#include "aulegacyship.h"

namespace AuLegacyShip
{
struct CellEditResult
{
	// Erasing unoccupied space can succeed without changing any records.
	bool accepted = false;
	bool changed = false;
	QString error;
};

// Apply one original mouse-event position to an offline working design.
// roomType -1 erases cells; nonnegative byte values add the selected room type.
// Rejection, including unavailable validation data, leaves design unchanged.
// The input file is never written. Room Number is the saved subtype byte.
CellEditResult EditCell(Design& design, int deck, const QPointF& cursor,
	quint8 gridMask, int roomType, quint8 roomNumber);
}

#endif // AuLegacyShipEdit_H
