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
// roomType -1 erases cells; 1..21 except 12 are spacecraft room-paint tools.
// Building roofs and unknown room types remain readable but cannot be painted.
// Rejection, including unavailable validation data, leaves design unchanged.
// The input file is never written. Room Number is the saved subtype byte.
CellEditResult EditCell(Design& design, int deck, const QPointF& cursor,
	quint8 gridMask, int roomType, quint8 roomNumber);

// DnRoomArray::InsideRoom: first room containing the point, excluding walls.
// Returns -1 when no room is selected or the geometry cannot be read.
int RoomAtPoint(const Design& design, int deck, const QPointF& cursor,
	QString* error = nullptr);

// Whole-room/deck erasure revalidates equipment before committing the edit.
CellEditResult DeleteRoomAt(Design& design, int deck, const QPointF& cursor);
CellEditResult DeleteDeck(Design& design, int deck);
}

#endif // AuLegacyShipEdit_H
