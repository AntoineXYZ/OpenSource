#include "aulegacyshipedit.h"
#include "aulegacyshipdetails.h"
#include "aulegacyshipgeometry.h"

#include <QSet>
#include <cmath>
#include <utility>

namespace
{
using namespace AuLegacyShip;

bool HasBoundary(const Room& room)
{
	return room.hasWorkingBoundary ? !room.workingBoundary.isEmpty() : !room.packedVertices.isEmpty();
}

bool IsRoof(const Room& room)
{
	return room.roomType >= 0x16 && room.roomType <= 0x18;
}

bool SameRoomKey(const Room& room, int deck, int type, quint8 number)
{
	return int(room.deck) == deck && int(room.roomType) == type && room.roomSubtype == number;
}

quint16 NextRoomId(const Design& design)
{
	QSet<quint16> used;
	for(const Room& room : design.rooms)
	{
		if(HasBoundary(room) && room.roomId != 0)
			used.insert(room.roomId);
	}
	for(unsigned int id = 1; id <= 0xffff; ++id)
	{
		if(!used.contains(quint16(id)))
			return quint16(id);
	}
	// SpaceShipDesignNextRoomId wraps to zero when every nonzero ID is used.
	return 0;
}

bool SetBoundary(Room& room, const QPolygonF& boundary, QString& error)
{
	QVector<quint16> packed;
	packed.reserve(boundary.size());
	for(const QPointF& point : boundary)
	{
		const double x = std::round(point.x() / CoordinateSpacing);
		const double y = std::round(point.y() / CoordinateSpacing);
		if(!std::isfinite(x) || !std::isfinite(y) || x < -128 || x > 127 || y < -128 || y > 127)
		{
			error = QStringLiteral("The resulting boundary exceeds the legacy coordinate range.");
			return false;
		}
		packed.append(quint16(((int(x) & 0xff) << 8) | (int(y) & 0xff)));
	}
	room.hasWorkingBoundary = true;
	room.workingBoundary = boundary;
	room.packedVertices = std::move(packed);
	return true;
}

bool SameDetails(const QVector<Detail>& left, const QVector<Detail>& right)
{
	if(left.size() != right.size())
		return false;
	for(int i = 0; i < left.size(); ++i)
	{
		const Detail& a = left[i];
		const Detail& b = right[i];
		if(a.deck != b.deck || a.detailType != b.detailType || a.hiddenRaw != b.hiddenRaw
			|| a.detailId != b.detailId || a.designId != b.designId || a.packedXY != b.packedXY
			|| a.roomId != b.roomId || a.commodityIdRaw != b.commodityIdRaw
			|| a.commodityIdNormalized != b.commodityIdNormalized)
			return false;
	}
	return true;
}
}

AuLegacyShip::CellEditResult AuLegacyShip::EditCell(Design& design, int deck,
	const QPointF& cursor, quint8 gridMask, int roomType, quint8 roomNumber)
{
	CellEditResult result;
	if(roomType < -1 || roomType > 255)
	{
		result.error = QStringLiteral("Select a room type or the Erase Cells tool.");
		return result;
	}
	const bool erase = roomType == -1;
	if(deck < -128 || deck > 127 || (!erase && (deck < -28 || deck > 28)))
	{
		result.error = QStringLiteral("The 2016 designer adds room cells only on decks -28 through 28.");
		return result;
	}
	if(!std::isfinite(cursor.x()) || !std::isfinite(cursor.y()))
	{
		result.error = QStringLiteral("The cell position is not finite.");
		return result;
	}
	const QPolygonF cell = Geometry::FillShape(gridMask, cursor);
	if(cell.size() < 3)
	{
		result.error = QStringLiteral("Enable at least two grid directions to form a cell.");
		return result;
	}
	for(const QPointF& point : cell)
	{
		const double x = std::round(point.x() / CoordinateSpacing);
		const double y = std::round(point.y() / CoordinateSpacing);
		if(!std::isfinite(x) || !std::isfinite(y) || x < -120 || x > 120 || y < -120 || y > 120)
		{
			result.error = QStringLiteral("This cell extends beyond the 2016 design grid.");
			return result;
		}
	}

	QPolygonF proposed = cell;
	bool existingRoom = false;
	if(!erase)
	{
		for(const Room& room : design.rooms)
		{
			if(!HasBoundary(room) || !SameRoomKey(room, deck, roomType, roomNumber))
				continue;
			QPolygonF boundary = RoomBoundary(room, &result.error);
			if(!result.error.isEmpty())
				return result;
			if(boundary.size() >= 3)
			{
				if(!Geometry::Merge(boundary, cell, &result.error))
					return result;
				proposed = std::move(boundary);
				existingRoom = true;
			}
			break;
		}
	}

	// Original add subtracts the whole proposed room, not just the cursor cell.
	// Empty file records remain inspectable but never enter the original room
	// array, so they cannot block edits or reserve working room IDs here.
	const auto affected = [=](const Room& room)
	{
		return HasBoundary(room) && int(room.deck) == deck
			&& (erase || (!IsRoof(room) && !SameRoomKey(room, deck, roomType, roomNumber)));
	};
	for(const Room& room : design.rooms)
	{
		if(!affected(room))
			continue;
		const QPolygonF boundary = RoomBoundary(room, &result.error);
		if(!result.error.isEmpty())
			return result;
		if(!Geometry::CanSubtract(boundary, proposed, &result.error))
		{
			if(result.error.isEmpty())
				result.error = QStringLiteral("This edit would leave an invalid boundary in room %1.").arg(room.roomId);
			return result;
		}
	}

	// Geometry and equipment changes are committed together. A failed preflight
	// or unavailable validation dependency cannot leave a partly repainted ship.
	Design candidate = design;
	bool changed = false;
	for(int i = 0; i < candidate.rooms.size();)
	{
		Room& room = candidate.rooms[i];
		if(!affected(room))
		{
			++i;
			continue;
		}
		QPolygonF boundary = RoomBoundary(room, &result.error);
		if(!result.error.isEmpty())
			return result;
		if(Geometry::Subtract(boundary, proposed, &result.error))
		{
			changed = true;
			if(boundary.size() < 3)
			{
				candidate.rooms.removeAt(i);
				continue;
			}
			if(!SetBoundary(room, boundary, result.error))
				return result;
		}
		else if(!result.error.isEmpty())
			return result;
		++i;
	}
	if(!erase)
	{
		if(existingRoom)
		{
			for(Room& room : candidate.rooms)
			{
				if(HasBoundary(room) && SameRoomKey(room, deck, roomType, roomNumber))
				{
					if(!SetBoundary(room, proposed, result.error))
						return result;
					break;
				}
			}
		}
		else
		{
			Room room;
			room.streamOffset = -1;
			room.designId = candidate.header.designId;
			room.roomId = NextRoomId(candidate);
			room.deck = qint8(deck);
			room.roomType = quint8(roomType);
			room.roomSubtype = roomNumber;
			if(!SetBoundary(room, proposed, result.error))
				return result;
			candidate.rooms.append(std::move(room));
		}
		changed = true;
	}

	// The legacy erase path verifies details even when no room area was removed.
	if(!ReconcileDetails(candidate, result.error))
		return result;
	result.accepted = true;
	result.changed = changed || !SameDetails(design.details, candidate.details);
	if(result.changed)
		design = std::move(candidate);
	return result;
}
