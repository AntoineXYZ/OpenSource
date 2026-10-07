#include "aulegacyshippaths.h"
#include "aulegacyshipgeometry.h"

#include <QPoint>
#include <algorithm>
#include <cmath>

namespace AuLegacyShip
{
namespace Paths
{
namespace
{
struct Floor
{
	QByteArray cells;
	int minX = 256, minY = 256, maxX = -1, maxY = -1;
};

bool MakeFloor(const Room& room, Floor& floor, QString& error)
{
	if(!Geometry::RasterFloor(RoomBoundary(room), floor.cells, &error)) return false;
	for(int x = 0; x < 256; ++x)
		for(int y = 0; y < 256; ++y)
			if(floor.cells.at(x * 256 + y))
			{
				floor.minX = std::min(floor.minX, x); floor.maxX = std::max(floor.maxX, x);
				floor.minY = std::min(floor.minY, y); floor.maxY = std::max(floor.maxY, y);
			}
	return true;
}

bool UpperBlocker(int type)
{
	switch(type)
	{
	case 8: case 33: case 51: case 52:
	case 77: case 78: case 79: case 80: case 81: case 82: case 83: case 84:
	case 85: case 86: case 87: case 88: case 89: case 90:
	case 91: case 92: case 93: case 94: case 95: case 96: case 97: return true;
	default: return false;
	}
}

bool LowerBlocker(int type)
{
	switch(type)
	{
	case 10: case 11: case 14: case 16: case 18:
	case 23: case 24: case 32: case 74: case 75: return true;
	default: return false;
	}
}

void ExcludeDetails(const Design& design, const Room& room, Floor& floor, bool upper)
{
	for(const Detail& detail : design.details)
	{
		if(detail.roomId != room.roomId
			|| !(upper ? UpperBlocker(detail.detailType) : LowerBlocker(detail.detailType))) continue;
		const QPointF point = DecodePoint(detail.packedXY);
		const int x = int(std::round(point.x() / CoordinateSpacing)) + 128;
		const int y = int(std::round(point.y() / CoordinateSpacing)) + 128;
		if(x >= 0 && x < 256 && y >= 0 && y < 256) floor.cells[x * 256 + y] = 3;
	}
}
}

// DaSpaceShip::AnalyzeDesign, AuAtom13.dll:1803c63c0, final room-overlap
// pass. Upper gravity wells (11) and open-to-below rooms (15) contribute one
// opening per shared interior sample. Wall/boundary samples never qualify.
bool AppendVerticalOpenings(const Design& design, QVector<Opening>& openings, QString& error)
{
	if(design.rooms.isEmpty()) return true;
	int minimumDeck = 0;
	bool hasHull = false;
	for(const Room& room : design.rooms)
	{
		if((room.roomType >= 22 && room.roomType <= 24) || RoomBoundary(room).isEmpty()) continue;
		minimumDeck = hasHull ? std::min(minimumDeck, int(room.deck)) : int(room.deck);
		hasHull = true;
	}
	if(!hasHull) return true;
	QHash<int, Floor> lowerFloors;
	for(const Room& upper : design.rooms)
	{
		if((upper.roomType != 11 && upper.roomType != 15) || upper.deck == minimumDeck) continue;
		Floor upperFloor;
		if(!MakeFloor(upper, upperFloor, error)) return false;
		// Floor hatches in open-to-below rooms already have explicit portals.
		// Gravity wells deliberately do not apply this upper exclusion pass.
		if(upper.roomType == 15) ExcludeDetails(design, upper, upperFloor, true);
		for(int index = 0; index < design.rooms.size(); ++index)
		{
			const Room& lower = design.rooms.at(index);
			if(int(lower.deck) != int(upper.deck) - 1) continue;
			if(!lowerFloors.contains(index))
			{
				Floor floor;
				if(!MakeFloor(lower, floor, error)) return false;
				ExcludeDetails(design, lower, floor, false);
				lowerFloors.insert(index, floor);
			}
			const Floor& lowerFloor = lowerFloors[index];
			const int minX = std::max(upperFloor.minX, lowerFloor.minX);
			const int minY = std::max(upperFloor.minY, lowerFloor.minY);
			const int maxX = std::min(upperFloor.maxX, lowerFloor.maxX);
			const int maxY = std::min(upperFloor.maxY, lowerFloor.maxY);
			if(minX >= maxX || minY >= maxY) continue;
			for(int x = minX; x <= maxX; ++x)
				for(int y = minY; y <= maxY; ++y)
				{
					if(upperFloor.cells.at(x * 256 + y) != 2
						|| lowerFloor.cells.at(x * 256 + y) != 2) continue;
					Opening opening;
					opening.first = opening.second = opening.center = QPointF(
						(x - 128) * CoordinateSpacing, (y - 128) * CoordinateSpacing);
					opening.deck = upper.deck;
					opening.cellType = upper.roomType == 11 ? 8 : 9;
					opening.flags = upper.roomType == 11 ? 4 : 16;
					opening.room1 = upper.roomId;
					opening.room2 = lower.roomId;
					openings.append(opening);
				}
		}
	}
	return true;
}
}
}
