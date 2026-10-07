#include "aulegacyshippaths.h"
#include "aulegacyshipgeometry.h"
#include "aupoint.h"

#include <QPoint>
#include <algorithm>
#include <cmath>
#include <limits>

namespace AuLegacyShip
{
namespace Paths
{
namespace
{
struct Direction { int x, y, deck; quint32 mask, reverse; };
// AuData13.dll:18039f050, all 26 neighbors in the original order.
const Direction Directions[] =
{
	{ 1, 0, 0, 0x400, 0x4000}, { 1, 1, 0, 0x800, 0x8000},
	{ 0, 1, 0, 0x1000, 0x10000}, {-1, 1, 0, 0x2000, 0x20000},
	{-1, 0, 0, 0x4000, 0x400}, {-1,-1, 0, 0x8000, 0x800},
	{ 0,-1, 0, 0x10000, 0x1000}, { 1,-1, 0, 0x20000, 0x2000},
	{ 0, 0, 1, 0x40000, 1}, { 1, 0, 1, 0x80000, 0x20},
	{ 1, 1, 1, 0x100000, 0x40}, { 0, 1, 1, 0x200000, 0x80},
	{-1, 1, 1, 0x400000, 0x100}, {-1, 0, 1, 0x800000, 2},
	{-1,-1, 1, 0x1000000, 4}, { 0,-1, 1, 0x2000000, 8},
	{ 1,-1, 1, 0x4000000, 0x10}, { 0, 0,-1, 1, 0x40000},
	{ 1, 0,-1, 2, 0x800000}, { 1, 1,-1, 4, 0x1000000},
	{ 0, 1,-1, 8, 0x2000000}, {-1, 1,-1, 0x10, 0x4000000},
	{-1, 0,-1, 0x20, 0x80000}, {-1,-1,-1, 0x40, 0x100000},
	{ 0,-1,-1, 0x80, 0x200000}, { 1,-1,-1, 0x100, 0x400000}
};
bool InGrid(int x, int y) { return x >= 0 && x < 256 && y >= 0 && y < 256; }
quint32 Key(int x, int y, int deck)
{
	return (quint32(deck) << 16) | (quint32(x) << 8) | quint32(y);
}
int X(quint32 key) { return int((key >> 8) & 255); }
int Y(quint32 key) { return int(key & 255); }
int Deck(quint32 key) { return int(qint16(key >> 16)); }
quint32 Neighbor(quint32 key, const Direction& d)
{
	return Key(X(key) + d.x, Y(key) + d.y, Deck(key) + d.deck);
}
QPoint Grid(const QPointF& point)
{
	return QPoint(int(std::round(point.x() / CoordinateSpacing)) + 128,
		int(std::round(point.y() / CoordinateSpacing)) + 128);
}
quint8 At(const QByteArray& floor, int x, int y)
{
	return InGrid(x, y) ? quint8(floor.at(x * 256 + y)) : 0;
}
// AuPath::TestStepOk, AuData13.dll:18010cd30. Stair heights are quarters
// of the deck height. The asymmetry preserves the original permitted drops.
bool StepOk(quint16 from, quint16 to, int deck)
{
	if(deck == 0)
	{
		if(!(from & 0xff00) && ((to & 0x1800) || !(to & 0xff00))) return true;
		if(from & 0x8000) { if(to & 0xc000) return true; }
		else if(from & 0x4000) { if(to & 0xe000) return true; }
		else if(from & 0x2000) { if(to & 0x7000) return true; }
		else if(from & 0x1000) { if((to & 0x3800) || !(to & 0xff00)) return true; }
		if(from & 0x800) return (to & 0x1c00) || !(to & 0xff00);
		if(from & 0x400) return to & 0xe00;
		if(from & 0x200) return to & 0x700;
		return (from & 0x100) && (to & 0x300);
	}
	if(deck < 0)
	{
		if((!(from & 0xff00) || (from & 0x1000))
			&& ((to & 0x8000) || !(to & 0xff00))) return true;
		if(from & 0x800) return to & 0xc000;
		if(from & 0x400) return to & 0xe000;
		if(from & 0x200) return to & 0x7000;
		return (from & 0x100) && ((to & 0x3800) || !(to & 0xff00));
	}
	if(!(from & 0xff00) && ((to & 0x1000) || !(to & 0xff00))) return true;
	if(from & 0x8000) return (to & 0x1c00) || !(to & 0xff00);
	if(from & 0x4000) return to & 0xe00;
	if(from & 0x2000) return to & 0x700;
	return (from & 0x1000) && (to & 0x300);
}
struct StairStencil { int width, height, x, y; quint8 cells[49]; };
// Literal stencil arguments of AuPath::PaintStairs, 1800f4390, detail 101..140.
const StairStencil StairStencils[] =
{
	{5, 3, 0, 0, {41, 142, 143, 144, 145, 41, 42, 43, 44, 145, 41, 142, 143, 144, 145}},
	{6, 6, 1, 0, {0, 0, 0, 0, 155, 0, 0, 0, 0, 154, 154, 155, 0, 0, 153, 54, 154, 0, 0, 152, 53, 153, 0, 0, 51, 52, 152, 0, 0, 0, 0, 51, 0, 0, 0, 0}},
	{3, 5, 2, 0, {165, 165, 165, 164, 64, 164, 163, 63, 163, 162, 62, 162, 61, 61, 61}},
	{6, 6, 5, 1, {0, 175, 0, 0, 0, 0, 175, 174, 174, 0, 0, 0, 0, 174, 74, 173, 0, 0, 0, 0, 173, 73, 172, 0, 0, 0, 0, 172, 72, 71, 0, 0, 0, 0, 71, 0}},
	{5, 3, 4, 2, {105, 104, 103, 102, 1, 105, 4, 3, 2, 1, 105, 104, 103, 102, 1}},
	{6, 6, 4, 5, {0, 0, 0, 0, 11, 0, 0, 0, 0, 112, 12, 11, 0, 0, 113, 13, 112, 0, 0, 114, 14, 113, 0, 0, 115, 114, 114, 0, 0, 0, 0, 115, 0, 0, 0, 0}},
	{3, 5, 0, 4, {21, 21, 21, 122, 22, 122, 123, 23, 123, 124, 24, 124, 125, 125, 125}},
	{6, 6, 0, 4, {0, 31, 0, 0, 0, 0, 31, 32, 132, 0, 0, 0, 0, 132, 33, 133, 0, 0, 0, 0, 133, 34, 134, 0, 0, 0, 0, 134, 134, 135, 0, 0, 0, 0, 135, 0}},
	{5, 5, 0, 0, {0, 0, 165, 165, 165, 0, 0, 164, 64, 164, 41, 142, 153, 63, 163, 41, 42, 43, 48, 163, 41, 142, 143, 143, 148}},
	{5, 7, 1, 0, {0, 175, 0, 0, 0, 175, 174, 174, 0, 0, 0, 174, 74, 173, 0, 0, 0, 163, 58, 158, 0, 152, 52, 153, 0, 51, 52, 152, 0, 0, 0, 51, 0, 0, 0}},
	{5, 5, 4, 0, {105, 104, 103, 103, 168, 105, 4, 3, 68, 163, 105, 104, 173, 63, 163, 0, 0, 162, 62, 162, 0, 0, 61, 61, 61}},
	{7, 5, 6, 1, {0, 0, 0, 178, 0, 0, 0, 0, 0, 113, 78, 173, 0, 0, 0, 114, 14, 103, 72, 172, 0, 115, 114, 114, 0, 172, 72, 71, 0, 115, 0, 0, 0, 71, 0}},
	{5, 5, 4, 4, {108, 103, 103, 102, 1, 123, 8, 3, 2, 1, 123, 23, 113, 102, 1, 124, 24, 124, 0, 0, 125, 125, 125, 0, 0}},
	{5, 7, 3, 6, {0, 0, 0, 11, 0, 0, 0, 112, 12, 11, 0, 113, 12, 112, 0, 118, 18, 123, 0, 0, 0, 133, 34, 134, 0, 0, 0, 134, 134, 135, 0, 0, 0, 135, 0}},
	{5, 5, 0, 4, {21, 21, 21, 0, 0, 122, 22, 122, 0, 0, 123, 23, 133, 144, 145, 123, 28, 43, 44, 145, 128, 143, 143, 144, 145}},
	{7, 5, 0, 3, {0, 31, 0, 0, 0, 155, 0, 31, 32, 132, 0, 154, 154, 155, 0, 132, 32, 143, 54, 154, 0, 0, 0, 133, 38, 153, 0, 0, 0, 0, 0, 138, 0, 0, 0}},
	{5, 5, 0, 4, {41, 142, 143, 143, 147, 41, 42, 43, 47, 123, 41, 142, 133, 23, 123, 0, 0, 124, 24, 124, 0, 0, 125, 125, 125}},
	{7, 5, 0, 1, {0, 0, 0, 157, 0, 0, 0, 0, 0, 153, 57, 133, 0, 0, 0, 152, 52, 143, 34, 134, 0, 51, 52, 152, 0, 134, 134, 135, 0, 51, 0, 0, 0, 135, 0}},
	{5, 5, 0, 0, {167, 143, 143, 144, 145, 163, 67, 43, 44, 145, 163, 63, 153, 144, 145, 162, 62, 162, 0, 0, 61, 61, 61, 0, 0}},
	{5, 7, 3, 0, {0, 0, 0, 155, 0, 0, 0, 154, 154, 155, 0, 153, 54, 154, 0, 177, 77, 163, 0, 0, 0, 173, 72, 172, 0, 0, 0, 172, 72, 71, 0, 0, 0, 71, 0}},
	{5, 5, 4, 0, {165, 165, 165, 0, 0, 164, 64, 164, 0, 0, 163, 63, 173, 102, 1, 163, 7, 3, 2, 1, 107, 103, 103, 102, 1}},
	{7, 5, 6, 3, {0, 175, 0, 0, 0, 11, 0, 175, 174, 174, 0, 112, 12, 11, 0, 174, 74, 103, 12, 112, 0, 0, 0, 173, 17, 113, 0, 0, 0, 0, 0, 117, 0, 0, 0}},
	{5, 5, 4, 4, {0, 0, 21, 21, 21, 0, 0, 122, 22, 122, 105, 104, 113, 23, 123, 105, 4, 3, 27, 123, 105, 104, 103, 103, 127}},
	{5, 7, 1, 6, {0, 31, 0, 0, 0, 31, 32, 132, 0, 0, 0, 132, 32, 133, 0, 0, 0, 123, 37, 137, 0, 114, 14, 113, 0, 115, 114, 114, 0, 0, 0, 115, 0, 0, 0}},
	{5, 5, 0, 0, {105, 104, 103, 103, 168, 105, 4, 3, 68, 163, 6, 6, 163, 63, 163, 41, 42, 43, 48, 163, 41, 142, 143, 143, 148}},
	{7, 7, 3, 0, {0, 0, 0, 178, 0, 0, 0, 0, 0, 113, 78, 173, 0, 0, 0, 114, 14, 178, 73, 173, 0, 115, 114, 114, 6, 158, 58, 158, 0, 115, 6, 152, 52, 153, 0, 0, 0, 51, 52, 152, 0, 0, 0, 0, 0, 51, 0, 0, 0}},
	{5, 5, 4, 0, {108, 103, 103, 103, 168, 123, 8, 3, 68, 163, 123, 23, 103, 63, 163, 124, 24, 6, 62, 162, 125, 125, 6, 61, 61}},
	{7, 7, 6, 3, {0, 0, 0, 178, 0, 0, 0, 0, 0, 113, 78, 173, 0, 0, 0, 113, 13, 178, 72, 172, 0, 118, 18, 118, 6, 172, 72, 71, 0, 133, 34, 134, 6, 71, 0, 0, 0, 134, 134, 135, 0, 0, 0, 0, 0, 135, 0, 0, 0}},
	{5, 5, 4, 4, {108, 103, 103, 102, 1, 123, 8, 3, 2, 1, 123, 23, 123, 6, 6, 123, 28, 43, 44, 145, 128, 143, 143, 144, 145}},
	{7, 7, 3, 6, {0, 0, 0, 11, 0, 0, 0, 0, 0, 112, 12, 11, 0, 0, 0, 113, 12, 112, 6, 155, 0, 118, 18, 118, 6, 154, 154, 155, 0, 133, 33, 138, 54, 154, 0, 0, 0, 133, 38, 153, 0, 0, 0, 0, 0, 138, 0, 0, 0}},
	{5, 5, 0, 4, {21, 21, 6, 165, 165, 122, 22, 6, 64, 164, 123, 23, 143, 63, 163, 123, 28, 43, 48, 163, 128, 143, 143, 143, 148}},
	{7, 7, 0, 3, {0, 0, 0, 175, 0, 0, 0, 0, 0, 175, 174, 174, 0, 0, 0, 31, 6, 174, 74, 173, 0, 31, 32, 132, 6, 158, 58, 158, 0, 132, 32, 138, 53, 153, 0, 0, 0, 133, 38, 153, 0, 0, 0, 0, 0, 133, 0, 0, 0}},
	{5, 5, 0, 4, {41, 142, 143, 143, 147, 41, 42, 43, 47, 123, 6, 6, 123, 23, 123, 105, 4, 3, 27, 123, 105, 104, 103, 103, 127}},
	{7, 7, 0, 3, {0, 0, 0, 157, 0, 0, 0, 0, 0, 153, 57, 133, 0, 0, 0, 152, 52, 157, 33, 133, 0, 51, 52, 152, 6, 137, 37, 137, 0, 51, 6, 114, 14, 113, 0, 0, 0, 115, 114, 114, 0, 0, 0, 0, 0, 115, 0, 0, 0}},
	{5, 5, 0, 0, {167, 143, 143, 143, 147, 163, 67, 43, 47, 123, 163, 63, 143, 23, 123, 162, 62, 6, 24, 124, 61, 61, 6, 125, 125}},
	{7, 7, 3, 0, {0, 0, 0, 157, 0, 0, 0, 0, 0, 153, 57, 133, 0, 0, 0, 153, 53, 157, 34, 134, 0, 177, 77, 177, 6, 134, 134, 135, 0, 173, 72, 172, 6, 135, 0, 0, 0, 172, 72, 71, 0, 0, 0, 0, 0, 71, 0, 0, 0}},
	{5, 5, 4, 0, {167, 143, 143, 144, 145, 163, 67, 43, 44, 145, 163, 63, 163, 6, 6, 163, 7, 3, 2, 1, 107, 103, 103, 2, 1}},
	{7, 7, 6, 3, {0, 0, 0, 155, 0, 0, 0, 0, 0, 154, 154, 155, 0, 0, 0, 153, 54, 154, 6, 11, 0, 177, 77, 177, 6, 112, 12, 11, 0, 173, 73, 117, 12, 112, 0, 0, 0, 173, 17, 113, 0, 0, 0, 0, 0, 117, 0, 0, 0}},
	{5, 5, 4, 4, {165, 165, 6, 21, 21, 164, 64, 6, 22, 122, 163, 63, 103, 23, 123, 163, 7, 3, 27, 123, 107, 103, 103, 103, 127}},
	{7, 7, 3, 6, {0, 0, 0, 31, 0, 0, 0, 0, 0, 31, 32, 132, 0, 0, 0, 175, 6, 132, 32, 133, 0, 175, 174, 174, 6, 137, 37, 137, 0, 174, 74, 117, 13, 113, 0, 0, 0, 173, 17, 113, 0, 0, 0, 0, 0, 117, 0, 0, 0}}
};
int DetailCellType(int type)
{
	switch(type)
	{
	case 1: case 2: case 4: case 6: case 7: case 11: case 12: case 13:
	case 15: case 17: case 19: case 20: case 25: case 26: case 27: case 28:
	case 29: case 30: case 31: case 33: case 65: case 66: case 67: case 68:
	case 69: case 70: case 71: case 72: case 73: return 14;
	case 10: case 14: case 16: case 18: case 23: case 24: case 32:
	case 74: case 75: case 100: case 142: return 15;
	case 21: case 34: case 35: case 36: case 37: case 38: case 39: case 40: return 12;
	case 22: return 13;
	default: return 0;
	}
}
}

bool Graph::Build(const Design& design, QString& error,
	const std::function<bool(int, int)>& progress)
{
	m_Cells.clear(); m_Extant.clear(); m_ExtantRooms.clear(); m_Entrances.clear();
	m_RoomBoundaries.clear();
	m_PortalFootprint.clear(); m_Portals.clear();
	m_CellKeys.clear(); m_ReachCache.clear(); m_CacheClock = 0; m_Canceled = false;
	error.clear();
	if(design.rooms.isEmpty()) return true;
	bool hasHull = false;
	m_MinimumDeck = m_MaximumDeck = 0;
	for(const Room& room : design.rooms)
	{
		const QPolygonF boundary = RoomBoundary(room);
		if(!m_RoomBoundaries.contains(room.roomId))
			m_RoomBoundaries.insert(room.roomId, boundary);
		// AnalyzeDesign only includes nonempty non-roof rooms in ship extents.
		if(boundary.isEmpty() || (room.roomType >= 22 && room.roomType <= 24)) continue;
		if(!hasHull) m_MinimumDeck = m_MaximumDeck = room.deck;
		else
		{
			m_MinimumDeck = std::min(m_MinimumDeck, int(room.deck));
			m_MaximumDeck = std::max(m_MaximumDeck, int(room.deck));
		}
		hasHull = true;
	}
	if(!hasHull) return true;
	QVector<Opening> openings;
	if(!ExtractOpenings(design, openings, error)) return false;
	QByteArray floor(256 * 256, 0), raster;
	for(int deck = m_MinimumDeck; deck <= m_MaximumDeck + 1; ++deck)
	{
		if(progress && !progress(deck - m_MinimumDeck, m_MaximumDeck - m_MinimumDeck + 2))
		{
			error = QStringLiteral("Path checking canceled.");
			m_Canceled = true; m_Cells.clear(); return false;
		}
		for(const Room& room : design.rooms)
		{
			if(room.deck != deck || (room.roomType >= 22 && room.roomType <= 24)) continue;
			if(!Geometry::RasterFloor(RoomBoundary(room), raster, &error))
			{
				error = QStringLiteral("Room %1 on deck %2: %3").arg(room.roomId).arg(deck).arg(error);
				m_Cells.clear(); return false;
			}
			for(int x = 0; x < 256; ++x)
				for(int y = 0; y < 256; ++y)
				{
					const quint8 type = At(raster, x, y);
					if(!type) continue;
					const quint32 key = Key(x, y, deck);
					m_Extant[key] = 1;
					QVector<quint16>& ids = m_ExtantRooms[key];
					if(!ids.contains(room.roomId)) ids.append(room.roomId);
					floor[x * 256 + y] = char(type);
					if(room.roomType == 9 || type != 2) continue;
					Cell cell; cell.type = 2; cell.room1 = room.roomId;
					for(int i = 0; i < 8; ++i)
						if(At(raster, x + Directions[i].x, y + Directions[i].y) == 2)
							cell.directions |= Directions[i].mask;
					m_Cells.insert(key, cell);
				}
		}
		// PaintFloorPlan retains the deck-below footprint as exterior floor (16).
		for(int x = 0; x < 256; ++x)
			for(int y = 0; y < 256; ++y)
				if(At(floor, x, y) == 16)
				{
					Cell cell; cell.type = 16;
					for(int i = 0; i < 8; ++i)
						if(At(floor, x + Directions[i].x, y + Directions[i].y) == 16)
							cell.directions |= Directions[i].mask;
					m_Cells.insert(Key(x, y, deck), cell);
				}
		for(const Opening& extracted : openings)
		{
			Opening opening = extracted;
			if(opening.deck != deck) continue;
			const QPoint first = Grid(opening.first), second = Grid(opening.second);
			const QPoint center = Grid(opening.center);
			if(opening.cellType == 6 || opening.cellType == 7)
			{
				const auto lower = m_Cells.constFind(Key(center.x(), center.y(), deck - 1));
				opening.cellType = lower == m_Cells.constEnd() || lower->type == 16 ? 6 : 7;
			}
			auto paint = [&](int x, int y)
			{
				if(!InGrid(x, y)) return;
				const quint32 key = Key(x, y, deck);
				Cell& cell = m_Cells[key];
				cell.type = quint8(opening.cellType); cell.flags = opening.flags;
				cell.room1 = opening.room1; cell.room2 = opening.room2;
				m_PortalFootprint.insert(key);
				floor[x * 256 + y] = char(cell.type);
				switch(cell.type)
				{
				case 3: case 4: case 10: case 11: case 18:
					ConnectOpening(key);
					if(cell.flags & 0x14) ConnectBelow(key);
					break;
				case 5: case 7:
					if(m_Cells.contains(Key(x, y, deck - 1)))
					{
						cell.directions |= 1;
						m_Cells[Key(x, y, deck - 1)].directions |= 0x40000;
					}
					break;
				case 6:
					if(!m_Cells.contains(Key(x, y, deck - 1))
						&& !m_Extant.contains(Key(x, y, deck - 1)))
					{
						Cell lower; lower.type = 17; lower.directions |= 0x40000;
						m_Cells.insert(Key(x, y, deck - 1), lower);
						m_Extant.insert(Key(x, y, deck - 1), 17);
					}
					if(m_Cells.contains(Key(x, y, deck - 1)))
					{
						m_Cells[key].directions |= 1;
						m_Cells[Key(x, y, deck - 1)].directions |= 0x40000;
					}
					break;
				case 8: case 9: case 75: case 76: ConnectBelow(key); break;
				}
			};
			if(!opening.large)
			{
				paint(center.x(), center.y());
				if(opening.cellType == 4 && first.x() != second.x() && first.y() != second.y())
					for(int i = 0; i < 8; i += 2)
						if(!At(floor, center.x() + Directions[i].x, center.y() + Directions[i].y))
							paint(center.x() + Directions[i].x, center.y() + Directions[i].y);
			}
			else
			{
				const int dx = second.x() - first.x(), dy = second.y() - first.y();
				const int length = std::max(std::abs(dx), std::abs(dy));
				if(length == 0) continue;
				const int last = length - int(opening.secondEmbedded);
				for(int i = int(opening.firstEmbedded); i <= last; ++i)
				{
					const int x = first.x() + i * dx / length, y = first.y() + i * dy / length;
					paint(x, y);
					if(!opening.room2 && dx && dy)
					{
						int ex = x, ey = y;
						if(dx < 1) { if(dy < 0) ++ex; else --ey; }
						else if(dy < 1) ++ey; else --ex;
						if(!At(floor, ex, ey)) paint(ex, ey);
						if(i == last)
						{
							ex = x; ey = y;
							if(dx < 1) { if(dy < 0) --ey; else --ex; }
							else if(dy < 1) ++ex; else ++ey;
							if(!At(floor, ex, ey)) paint(ex, ey);
						}
					}
				}
			}
			if(InGrid(center.x(), center.y()) && m_Cells.contains(Key(center.x(), center.y(), deck)))
				m_Portals.insert(Key(center.x(), center.y(), deck));
			if(InGrid(center.x(), center.y())
				&& (opening.cellType == 4 || opening.cellType == 6
					|| opening.cellType == 7 || opening.cellType == 11))
				m_Entrances.insert(Key(center.x(), center.y(), deck));
		}
		for(const Detail& detail : design.details)
		{
			if(detail.deck != deck) continue;
			if(detail.detailType >= 101 && detail.detailType <= 140)
			{
				PaintStairs(detail, &floor, 1); continue;
			}
			const int type = DetailCellType(detail.detailType);
			if(!type) continue;
			const QPoint point = Grid(DecodePoint(detail.packedXY));
			if(!InGrid(point.x(), point.y())) continue;
			const quint32 key = Key(point.x(), point.y(), deck);
			floor[point.x() * 256 + point.y()] = char(type);
			m_Extant[key] = quint8(type);
			if(type == 15)
			{
				RemoveCell(key);
				if(detail.detailType == 74 || detail.detailType == 75)
					for(int dx = -1; dx <= 1; ++dx)
						for(int dy = -1; dy <= 1; ++dy)
						{
							if((!dx && !dy) || At(floor, point.x() + dx, point.y() + dy) != 2) continue;
							floor[(point.x() + dx) * 256 + point.y() + dy] = 14;
							auto cell = m_Cells.find(Key(point.x() + dx, point.y() + dy, deck));
							if(cell != m_Cells.end() && cell->type == 2) cell->type = 14;
						}
			}
			else if(type == 14)
			{
				auto cell = m_Cells.find(key);
				if(cell != m_Cells.end() && cell->type == 2) cell->type = 14;
			}
			else if(type == 13)
			{
				auto cell = m_Cells.find(key);
				if(cell != m_Cells.end()) cell->type = 13;
			}
			else
			{
				Cell cell; cell.type = 12; cell.room1 = detail.roomId;
				m_Cells.insert(key, cell);
				// CanTurretConnect, 1800721f0, excludes exterior roof cells
				// and hatch cells even when they touch a turret's location.
				for(int i = 0; i < 8; ++i)
				{
					const Direction& d = Directions[i];
					if(!InGrid(point.x() + d.x, point.y() + d.y)) continue;
					auto neighbor = m_Cells.find(Neighbor(key, d));
					if(neighbor == m_Cells.end()) continue;
					switch(neighbor->type)
					{
					case 2: case 3: case 8: case 9: case 10: case 12: case 14: case 75: case 76:
						m_Cells[key].directions |= d.mask; neighbor->directions |= d.reverse;
						break;
					default: break;
					}
				}
			}
		}
		// PaintDiagonalHullWalls, 1800f1150.
		for(int x = 0; x < 256; ++x)
			for(int y = 0; y < 256; ++y)
				if((At(floor, x, y) == 0 || At(floor, x, y) == 16)
					&& (At(floor, x - 1, y) == 1 || At(floor, x + 1, y) == 1)
					&& (At(floor, x, y - 1) == 1 || At(floor, x, y + 1) == 1))
				{
					if(At(floor, x, y) == 16) RemoveCell(Key(x, y, deck));
					floor[x * 256 + y] = 15;
				}
		for(int x = 0; x < 256; ++x)
			for(int y = 0; y < 256; ++y)
			{
				const int type = At(floor, x, y);
				if(type) m_Extant[Key(x, y, deck)] = quint8(type);
				floor[x * 256 + y] = type == 16 || type == 7 || type == 13 ? 0 : type ? 16 : 0;
			}
	}
	// ConnectLedges, 1800e9640.
	const QList<quint32> keys = m_Cells.keys();
	for(quint32 key : keys)
		if(m_Cells[key].directions & 0x40000)
			for(int i = 9; i <= 16; ++i)
			{
				const Direction& d = Directions[i];
				if(!InGrid(X(key) + d.x, Y(key) + d.y)) continue;
				auto neighbor = m_Cells.find(Neighbor(key, d));
				if(neighbor != m_Cells.end())
				{
					m_Cells[key].directions |= d.mask; neighbor->directions |= d.reverse;
				}
			}
	for(const Detail& detail : design.details) PaintStairs(detail, nullptr, 2);
	TrimVerticalConnections();
	for(auto cell = m_Cells.begin(); cell != m_Cells.end(); ++cell)
		for(const Direction& d : Directions)
		{
			if(!(cell->directions & d.mask)) continue;
			const auto neighbor = m_Cells.constFind(Neighbor(cell.key(), d));
			if(!InGrid(X(cell.key()) + d.x, Y(cell.key()) + d.y)
				|| neighbor == m_Cells.constEnd() || !(neighbor->directions & d.reverse))
				cell->directions &= ~d.mask;
		}
	m_CellKeys.reserve(m_Cells.size());
	for(auto cell = m_Cells.begin(); cell != m_Cells.end(); ++cell)
	{
		cell->index = m_CellKeys.size(); m_CellKeys.append(cell.key());
	}
	return true;
}

void Graph::TrimVerticalConnections()
{
	// AuData13.dll:18010d6c0. Gravity/open-to-below footprints contribute
	// many raw cells but only their centroid, or stair-top waypoints, switches
	// between the two rooms. Keep the raw cells for walking within each room.
	QHash<quint64, QVector<quint32>> groups;
	for(quint32 key : m_Portals)
	{
		const Cell cell = m_Cells.value(key);
		const int family = cell.type == 8 || cell.type == 75 ? 8
			: cell.type == 9 || cell.type == 76 ? 9 : 0;
		if(!family || !cell.room1 || !cell.room2) continue;
		groups[(quint64(family) << 32) | (quint64(cell.room1) << 16) | cell.room2].append(key);
	}
	for(auto group = groups.constBegin(); group != groups.constEnd(); ++group)
	{
		const QVector<quint32>& keys = group.value();
		QSet<quint32> tops;
		for(quint32 key : keys) if(m_Cells.value(key).flags & 0x800) tops.insert(key);
		if(!tops.isEmpty())
		{
			for(quint32 key : keys)
			{
				if(!tops.contains(key)) { m_Portals.remove(key); continue; }
				const Cell cell = m_Cells.value(key);
				if(cell.type != 75 && cell.type != 76) continue;
				for(int i = 0; i < 8; ++i)
					if((cell.directions & Directions[i].mask)
						&& m_Portals.contains(Neighbor(key, Directions[i]))
						&& (m_Cells.value(Neighbor(key, Directions[i])).flags & 0x800))
					{
						m_Portals.remove(key); break;
					}
			}
			continue;
		}
		if(keys.size() < 2) continue;
		double x = 0, y = 0, z = 0;
		for(quint32 key : keys) { x += X(key); y += Y(key); z += Deck(key); }
		x /= keys.size(); y /= keys.size(); z /= keys.size();
		const int gx = int(std::round(x - 128)) + 128;
		const int gy = int(std::round(y - 128)) + 128;
		const int deck = int(std::floor(z + .05 / DeckHeight));
		quint32 selected = Key(gx, gy, deck);
		if(!keys.contains(selected))
		{
			double distance = std::numeric_limits<double>::max();
			for(quint32 key : keys)
			{
				const double dx = X(key) - x, dy = Y(key) - y;
				const double candidate = dx * dx + dy * dy;
				if(candidate < distance || (candidate == distance && key < selected))
				{
					distance = candidate; selected = key;
				}
			}
		}
		for(quint32 key : keys) if(key != selected) m_Portals.remove(key);
	}
}

QVector<quint32> Graph::TerminalCells(const QPointF& point, int deck, bool crew, bool& outside) const
{
	QVector<quint32> result;
	outside = false;
	if(!std::isfinite(point.x()) || !std::isfinite(point.y())) return result;
	if(std::abs(point.x()) > 128 * CoordinateSpacing
		|| std::abs(point.y()) > 128 * CoordinateSpacing) return result;
	const QPoint grid = Grid(point);
	if(!InGrid(grid.x(), grid.y())) return result;
	const quint32 key = Key(grid.x(), grid.y(), deck);
	if(m_Cells.contains(key)) { result.append(key); return result; }
	if(!m_Extant.contains(key))
	{
		outside = true;
		quint32 nearest = 0;
		int distance = std::numeric_limits<int>::max();
		// ClosestHullEntrance, 1800e8bc0, uses squared half-grid distance,
		// counting each deck difference as one grid step.
		for(quint32 entrance : m_Entrances)
		{
			if(!m_Cells.contains(entrance)) continue;
			const int dx = X(entrance) - grid.x(), dy = Y(entrance) - grid.y();
			const int dz = Deck(entrance) - deck;
			const int candidate = dx * dx + dy * dy + dz * dz;
			if(candidate < distance || (candidate == distance && entrance < nearest))
			{
				nearest = entrance; distance = candidate;
			}
		}
		if(distance != std::numeric_limits<int>::max()) result.append(nearest);
		return result;
	}
	// AuExtantCell::InsideRoom, 180073550: inspect the cell's first eight room
	// IDs in order and use the first polygon containing the actual location,
	// including its boundary. Raster membership alone is insufficient at walls.
	quint16 roomId = 0;
	const QVector<quint16> rooms = m_ExtantRooms.value(key);
	const AuPoint location(point.x(), point.y(), deck * DeckHeight);
	for(int i = 0; i < std::min(8, int(rooms.size())); ++i)
	{
		if(!rooms.at(i)) break;
		const auto boundary = m_RoomBoundaries.constFind(rooms.at(i));
		if(boundary == m_RoomBoundaries.constEnd()) continue;
		AuPointArray polygon;
		for(const QPointF& vertex : *boundary)
			polygon.append(AuPoint(vertex.x(), vertex.y(), deck * DeckHeight));
		if(polygon.PointInBoundary(location) || polygon.PointOnBoundary_2D(location))
		{
			roomId = rooms.at(i); break;
		}
	}
	// ClosestPathableSpacecraft, 1800e9090, retains the first equally close
	// candidate in the original eight-neighbor order.
	quint32 nearest = 0;
	double distance = std::numeric_limits<double>::max();
	for(int i = 0; i < 8; ++i)
	{
		const Direction& d = Directions[i];
		if(!InGrid(grid.x() + d.x, grid.y() + d.y)) continue;
		const quint32 adjacent = Neighbor(key, d);
		const auto cell = m_Cells.constFind(adjacent);
		if(cell == m_Cells.constEnd()) continue;
		if(cell->room1 != roomId && (!m_PortalFootprint.contains(adjacent) || cell->room2 != roomId)) continue;
		// A hatch, a cell without a door, or an open door qualifies regardless
		// of crew permission. Here design-check doors are closed; the remaining
		// branch permits regular doors according to crew access.
		if(!(cell->flags & 8) && (cell->flags & 32)
			&& (cell->type == 4 || (!crew && (cell->flags & 2)))) continue;
		const double dx = (X(adjacent) - 128) * CoordinateSpacing - point.x();
		const double dy = (Y(adjacent) - 128) * CoordinateSpacing - point.y();
		const double candidate = dx * dx + dy * dy;
		if(candidate < distance) { distance = candidate; nearest = adjacent; }
	}
	if(distance != std::numeric_limits<double>::max()) result.append(nearest);
	return result;
}

bool Graph::Reach(quint32 from, const QSet<quint32>& targets, bool crew) const
{
	const auto start = m_Cells.constFind(from);
	if(start == m_Cells.constEnd() || m_Canceled) return false;
	const quint64 cacheKey = (quint64(crew) << 32) | from;
	auto cached = m_ReachCache.find(cacheKey);
	if(cached == m_ReachCache.end())
	{
		ReachCache result;
		result.cells = QBitArray(m_CellKeys.size(), false);
		QBitArray states(m_CellKeys.size() * 4, false);
		QVector<int> todo;
		auto add = [&](int index, int slot, bool cachedConnections)
		{
			const int state = index * 4 + slot * 2 + int(cachedConnections);
			if(states.testBit(state)) return;
			states.setBit(state); todo.append(state); result.cells.setBit(index);
		};
		const bool startsWithConnections = crew || m_Portals.contains(from);
		add(start->index, 0, startsWithConnections);
		if(m_PortalFootprint.contains(from) && start->room1 != start->room2)
			add(start->index, 1, startsWithConnections);
		for(int index = 0; index < todo.size(); ++index)
		{
			if((index & 2047) == 0 && m_CancelCheck && m_CancelCheck())
			{
				m_Canceled = true; return false;
			}
			const int state = todo.at(index), cellIndex = state / 4;
			const int roomSlot = (state >> 1) & 1;
			const bool cachedConnections = (state & 1) != 0;
			const quint32 key = m_CellKeys.at(cellIndex);
			const Cell cell = m_Cells.value(key);
			const quint16 room = roomSlot ? cell.room2 : cell.room1;
			// SpacecraftRoute (1800fcaf0) first discovers openings using raw
			// crew-door flags, then routes through CreateSpacecraft's cached
			// connections. PaintOpening/ PaintLargeOpening construct AuPathCell
			// with flags=0; the original never copies its raw crew-lock bit.
			// DiscardInaccessibleDoors and PathCellsSpacecraft therefore permit
			// those cached connections even for public routes. A source already
			// on an opening starts in that phase, as with turret seats on hatches.
			if(m_Portals.contains(key))
			{
				if(!cachedConnections)
				{
					add(cellIndex, roomSlot, true);
					continue;
				}
				if(cell.room1 != cell.room2) add(cellIndex, 1 - roomSlot, true);
			}
			for(const Direction& d : Directions)
			{
				if(!(cell.directions & d.mask)) continue;
				const quint32 next = Neighbor(key, d);
				const auto neighbor = m_Cells.constFind(next);
				if(neighbor == m_Cells.constEnd()) continue;
				const int slot = neighbor->room1 == room ? 0 : 1;
				if(slot && (!m_PortalFootprint.contains(next) || neighbor->room2 != room)) continue;
				if(states.testBit(neighbor->index * 4 + slot * 2 + int(cachedConnections))) continue;
				if(neighbor->type == 18 || ((neighbor->flags & 16) && !(neighbor->flags & 0xff00))) continue;
				if(!crew && !cachedConnections && (neighbor->type == 3 || neighbor->type == 5)
					&& (neighbor->flags & 2)) continue;
				if(!StepOk(cell.flags, neighbor->flags, d.deck)) continue;
				add(neighbor->index, slot, cachedConnections);
			}
		}
		// Cache complete reachability per berth/source and permission mode.
		// Budget includes a conservative node/allocation allowance per entry.
		constexpr qint64 cacheBudget = 64 * 1024 * 1024;
		const qint64 entryBytes = (qint64(m_CellKeys.size()) + 7) / 8 + 128;
		const qint64 limit = std::max(qint64(1), cacheBudget / entryBytes);
		while(m_ReachCache.size() >= limit)
		{
			auto oldest = m_ReachCache.begin();
			for(auto entry = m_ReachCache.begin(); entry != m_ReachCache.end(); ++entry)
				if(entry->used < oldest->used) oldest = entry;
			m_ReachCache.erase(oldest);
		}
		result.used = ++m_CacheClock;
		cached = m_ReachCache.insert(cacheKey, result);
	}
	else cached->used = ++m_CacheClock;
	for(quint32 target : targets)
	{
		const auto cell = m_Cells.constFind(target);
		if(cell != m_Cells.constEnd() && cached->cells.testBit(cell->index)) return true;
	}
	return false;
}

bool Graph::CanReach(const QPointF& from, int fromDeck, const QPointF& to, int toDeck, bool crew) const
{
	if(m_Canceled) return false;
	bool fromOutside, toOutside;
	const QVector<quint32> starts = TerminalCells(from, fromDeck, crew, fromOutside);
	// The original clears the route before resolving its source. If that
	// source is extant but blocked, StepToPathableSpacecraft returns after
	// adding one adjacent step; it does not inspect the requested destination.
	if(!fromOutside && !starts.isEmpty())
	{
		const QPoint source = Grid(from);
		if(!m_Cells.contains(Key(source.x(), source.y(), fromDeck))) return true;
	}
	QVector<quint32> finishes = TerminalCells(to, toDeck, crew, toOutside);
	// With hull doors usable, SpacecraftRoute resolves an outward floor hatch
	// destination to its actual lower cell, rather than stopping on its panel.
	for(int i = finishes.size() - 1; i >= 0; --i)
		if(m_Cells.value(finishes.at(i)).type == 6)
		{
			const quint32 hatch = finishes.at(i);
			const quint32 lower = Key(X(hatch), Y(hatch), Deck(hatch) - 1);
			if(m_Cells.contains(lower)) finishes[i] = lower;
			else finishes.removeAt(i);
		}
	// SpacecraftRoute (1800fcaf0) permits travel between two exterior
	// locations after both have successfully resolved a hull entrance.
	if(fromOutside && toOutside && !starts.isEmpty() && !finishes.isEmpty()) return true;
	QSet<quint32> targets;
	for(quint32 finish : finishes) targets.insert(finish);
	for(quint32 start : starts) if(Reach(start, targets, crew)) return true;
	return false;
}

bool Graph::CanReachOutside(const QPointF& from, int fromDeck, bool crew) const
{
	// Report's exact bit patterns decode to these world-foot values. Every
	// outside probe has z=0, regardless of the ship's lowest deck.
	const double outerDistance = 341.41125494, halfDistance = 170.70562747;
	const QPointF outside[] =
	{
		{outerDistance, 0}, {outerDistance, halfDistance}, {outerDistance, outerDistance}, {halfDistance, outerDistance},
		{0, outerDistance}, {-halfDistance, outerDistance}, {-outerDistance, outerDistance}, {-outerDistance, halfDistance},
		{-outerDistance, 0}, {-outerDistance, -halfDistance}, {-outerDistance, -outerDistance}, {-halfDistance, -outerDistance},
		{0, -outerDistance}, {halfDistance, -outerDistance}, {outerDistance, -outerDistance}, {outerDistance, -halfDistance}
	};
	for(const QPointF& point : outside) if(CanReach(from, fromDeck, point, 0, crew)) return true;
	return false;
}
void Graph::ConnectOpening(quint32 key)
{
	for(int i = 0; i < 8; ++i)
	{
		const Direction& d = Directions[i];
		if(!InGrid(X(key) + d.x, Y(key) + d.y)) continue;
		auto next = m_Cells.find(Neighbor(key, d));
		if(next == m_Cells.end()) continue;
		m_Cells[key].directions |= d.mask;
		next->directions |= d.reverse;
	}
}

void Graph::ConnectBelow(quint32 key)
{
	const quint32 below = Key(X(key), Y(key), Deck(key) - 1);
	auto lower = m_Cells.find(below);
	if(lower == m_Cells.end())
	{
		m_Cells[key].flags &= ~quint16(16);
		if(m_Cells[key].type == 9) m_Cells[key].type = 76;
		return;
	}
	Cell& upper = m_Cells[key];
	upper.directions |= 1;
	lower->directions |= 0x40000;
	if(upper.flags & 4) lower->flags |= 4;
	for(int band = 0; band < 4; ++band)
		if(lower->flags & (0x1000 << band)) { upper.flags |= 0x100 << band; break; }
	for(int i = 0; i < 8; ++i)
	{
		const Direction& flat = Directions[i];
		if(!(lower->directions & flat.mask)) continue;
		if(!InGrid(X(key) + flat.x, Y(key) + flat.y)) continue;
		auto next = m_Cells.find(Key(X(key) + flat.x, Y(key) + flat.y, Deck(key) - 1));
		if(next == m_Cells.end()) continue;
		const Direction& down = Directions[18 + i];
		upper.directions |= down.mask;
		next->directions |= down.reverse;
	}
}

void Graph::RemoveCell(quint32 key)
{
	m_Cells.remove(key);
	// Seal the diagonal passage between two touching obstacles, as in
	// PaintObstaclesAndTurrets (1800f25c0) and PaintStairCells (1800f39d0).
	const int x = X(key), y = Y(key), deck = Deck(key);
	const int pairs[4][6] =
	{
		{1,1, 1,0, 0,1}, {-1,1, -1,0, 0,1},
		{-1,-1, -1,0, 0,-1}, {1,-1, 1,0, 0,-1}
	};
	for(const auto& pair : pairs)
	{
		if(!InGrid(x + pair[0], y + pair[1])
			|| m_Extant.value(Key(x + pair[0], y + pair[1], deck)) != 15) continue;
		const quint32 first = Key(x + pair[2], y + pair[3], deck);
		const quint32 second = Key(x + pair[4], y + pair[5], deck);
		for(int i = 0; i < 8; ++i)
			if(InGrid(X(first) + Directions[i].x, Y(first) + Directions[i].y)
				&& Neighbor(first, Directions[i]) == second)
			{
				auto a = m_Cells.find(first), b = m_Cells.find(second);
				if(a != m_Cells.end()) a->directions &= ~Directions[i].mask;
				if(b != m_Cells.end()) b->directions &= ~Directions[i].reverse;
			}
	}
}

void Graph::PaintStairs(const Detail& detail, QByteArray* floor, int pass)
{
	if(detail.detailType < 101 || detail.detailType > 140) return;
	const StairStencil& stencil = StairStencils[detail.detailType - 101];
	const QPoint origin = Grid(DecodePoint(detail.packedXY));
	for(int row = 0; row < stencil.height; ++row)
		for(int column = 0; column < stencil.width; ++column)
		{
			const int x = origin.x() - stencil.x + column;
			const int y = origin.y() - stencil.y + row;
			if(!InGrid(x, y)) continue;
			const int raw = stencil.cells[(stencil.height - row - 1) * stencil.width + column];
			const int code = raw >= 100 ? raw - 100 : raw;
			const int direction = code / 10, stage = code % 10;
			if(!stage || stage == 9) continue;
			const quint32 key = Key(x, y, detail.deck);
			if(pass == 1)
			{
				if(stage == 6)
				{
					(*floor)[x * 256 + y] = 15;
					m_Extant[key] = 15; RemoveCell(key);
				}
				else if(raw >= 100)
				{
					auto cell = m_Cells.find(key);
					if(cell != m_Cells.end())
					{
						if(cell->type == 2) cell->type = 14;
						else if(cell->type == 8) cell->type = 75;
						else if(cell->type == 9) cell->type = 76;
						(*floor)[x * 256 + y] = char(cell->type);
					}
				}
				continue;
			}
			if(stage == 6) continue;
			auto cell = m_Cells.find(key);
			if(cell == m_Cells.end()) continue;
			int band = stage;
			if(stage == 7 || stage == 8) band = 3;
			if(band > 4) continue;
			cell->flags |= quint16(0x1000 << (band - 1));
			const int up = (direction + (stage == 7 ? 2 : stage == 8 ? 6 : 4)) % 8;
			cell->stairs = quint8((up << 4) | direction);
			auto upper = m_Cells.find(Key(x, y, int(detail.deck) + 1));
			if(upper != m_Cells.end() && (upper->flags & 0x14))
			{
				upper->flags |= quint16(0x100 << (band - 1));
				if(cell->type == 14 || cell->type == 75 || cell->type == 76)
				{
					if(upper->type == 8) upper->type = 75;
					else if(upper->type == 9) upper->type = 76;
				}
			}
		}
}

}
}
