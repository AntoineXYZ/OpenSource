#include "aulegacybuildingpaths.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>
#include <algorithm>
#include <utility>

namespace AuLegacyBd
{
namespace Paths
{
namespace
{
class Reader
{
public:
	Reader(const QByteArray& bytes, QString& error) : m_Bytes(bytes), m_Error(error) {}
	int Remaining() const { return m_Bytes.size() - m_Offset; }
	bool Fail(const QString& text)
	{
		m_Error = QStringLiteral("building path offset 0x%1: %2").arg(m_Offset, 0, 16).arg(text);
		return false;
	}
	bool U8(quint8& value)
	{
		if(Remaining() < 1) return Fail(QStringLiteral("truncated byte"));
		value = quint8(m_Bytes.at(m_Offset++)); return true;
	}
	bool U16(quint16& value)
	{
		quint8 a = 0, b = 0; if(!U8(a) || !U8(b)) return false;
		value = quint16((quint16(a) << 8) | b); return true;
	}
	bool U32(quint32& value)
	{
		quint16 a = 0, b = 0; if(!U16(a) || !U16(b)) return false;
		value = (quint32(a) << 16) | b; return true;
	}
	bool I16(qint16& value)
	{
		quint16 raw = 0; if(!U16(raw)) return false;
		value = qint16(raw < 0x8000 ? int(raw) : int(raw) - 0x10000); return true;
	}
	bool I32(qint32& value)
	{
		quint32 raw = 0; if(!U32(raw)) return false;
		value = qint32(raw <= 0x7fffffff ? qint64(raw) : qint64(raw) - 0x100000000LL); return true;
	}
	bool Count(quint32& count, int minimumBytes)
	{
		if(!U32(count)) return false;
		if(count > quint32(Remaining() / minimumBytes) || count > quint32(m_Budget))
			return Fail(QStringLiteral("record count exceeds bounded input"));
		m_Budget -= int(count); return true;
	}
private:
	const QByteArray& m_Bytes;
	QString& m_Error;
	int m_Offset = 0, m_Budget = MaximumRecords;
};

struct Direction { int x, y, deck; quint32 mask, reverse; };
// Original AuData13.dll 18039f050. Preserve this order on equal-cost steps.
const Direction Directions[] = {
	{1,0,0,0x400,0x4000}, {1,1,0,0x800,0x8000}, {0,1,0,0x1000,0x10000}, {-1,1,0,0x2000,0x20000},
	{-1,0,0,0x4000,0x400}, {-1,-1,0,0x8000,0x800}, {0,-1,0,0x10000,0x1000}, {1,-1,0,0x20000,0x2000},
	{0,0,1,0x40000,1}, {1,0,1,0x80000,0x20}, {1,1,1,0x100000,0x40}, {0,1,1,0x200000,0x80},
	{-1,1,1,0x400000,0x100}, {-1,0,1,0x800000,2}, {-1,-1,1,0x1000000,4}, {0,-1,1,0x2000000,8},
	{1,-1,1,0x4000000,0x10}, {0,0,-1,1,0x40000}, {1,0,-1,2,0x800000}, {1,1,-1,4,0x1000000},
	{0,1,-1,8,0x2000000}, {-1,1,-1,0x10,0x4000000}, {-1,0,-1,0x20,0x80000}, {-1,-1,-1,0x40,0x100000},
	{0,-1,-1,0x80,0x200000}, {1,-1,-1,0x100,0x400000}
};
quint32 Key(int x, int y, int deck) { return (quint32(deck) << 16) | (quint32(x) << 8) | quint32(y); }
int X(quint32 key) { return int((key >> 8) & 255); }
int Y(quint32 key) { return int(key & 255); }
int Deck(quint32 key) { return int(qint16(key >> 16)); }
bool ValidKey(quint32 key) { return Deck(key) >= -127 && Deck(key) <= 127; }
bool Fields(Reader& reader, PathableCell& cell)
{
	return reader.I16(cell.doorIndex) && reader.I32(cell.holeIndex) && reader.U16(cell.flags)
		&& reader.I32(cell.railingIndex) && reader.U16(cell.room1) && reader.U16(cell.room2);
}
bool RoomMap(Reader& reader, QMap<quint16, bool>& output)
{
	quint32 count = 0; if(!reader.Count(count, 3)) return false;
	for(quint32 i = 0; i < count; ++i)
	{
		quint16 room = 0; quint8 value = 0;
		if(!reader.U16(room) || !reader.U8(value)) return false;
		if(value > 1 || output.contains(room)) return reader.Fail(QStringLiteral("invalid room map"));
		output.insert(room, bool(value));
	}
	return true;
}
bool Usable(const PathableCell& cell)
{
	if(cell.type == 0 || cell.type == 1 || cell.type == 15 || cell.type == 18) return false;
	if((cell.flags & 16) && !(cell.flags & 0xff00)) return false;
	// Original building openings have no runtime door state to gate passage.
	return true;
}
// AuPath::TestStepOk, AuData13.dll 18010cd30.
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
		if((!(from & 0xff00) || (from & 0x1000)) && ((to & 0x8000) || !(to & 0xff00))) return true;
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
int MoveCost(int type)
{
	switch(type)
	{
	case 3: case 4: return 30;
	case 8: case 9: case 10: case 13: return 20;
	case 11: return 50;
	case 12: return 40;
	case 14: case 75: case 76: return 150;
	case 16: return 10;
	case 17: return 100;
	default: return 0;
	}
}

bool HasOpening(const PathableCell& cell)
{ return cell.doorIndex >= 0 || cell.holeIndex >= 0 || cell.railingIndex >= 0; }
bool InRoom(const PathableCell& cell, quint16 room)
{ return cell.room1 == room || (HasOpening(cell) && cell.room2 == room); }
quint32 Adjacent(quint32 key, const Direction& d)
{ return Key(X(key) + d.x, Y(key) + d.y, Deck(key) + d.deck); }
bool InGrid(quint32 key, const Direction& d)
{ return X(key) + d.x >= 0 && X(key) + d.x <= 255 && Y(key) + d.y >= 0 && Y(key) + d.y <= 255; }
int DirectionCost(const Direction& d)
{
	return !d.deck ? (d.x && d.y ? 14 : 10)
		: !d.x && !d.y ? (d.deck > 0 ? 12 : 11) : d.x && d.y ? 34 : 28;
}
QMap<quint32, quint32> AccessibleBuildingDoors(const QMap<quint32, PathableCell>& cells,
	const QMap<quint32, PortalCell>& portals, quint32 start, quint16 room)
{
	// AuPath::AccessibleBuildingDoors/Neighbors, 1800e3ac0/1800e3f90.
	// AuCellToDoMap snapshots an entire cost-ordered frontier before processing
	// the next batch. AuAccessibleCell retains its first discovered cost. Both
	// original details matter: ordinary Dijkstra produces different BP costs.
	QMap<quint32, quint32> result;
	QHash<quint32, quint32> storedCosts, pendingCosts;
	QSet<quint32> visited;
	QMultiMap<quint32, quint32> pending;
	storedCosts.insert(start, 0); pending.insert(0, start); pendingCosts.insert(start, 0);
	while(!pending.isEmpty())
	{
		const auto batch = pending; pending.clear(); pendingCosts.clear();
		for(auto entry = batch.cbegin(); entry != batch.cend(); ++entry)
		{
			const quint32 key = entry.value(); const PathableCell cell = cells.value(key);
			for(const Direction& d : Directions)
			{
				if(!(cell.directions & d.mask) || !InGrid(key, d)) continue;
				const quint32 target = Adjacent(key, d);
				const auto neighbor = cells.constFind(target);
				if(visited.contains(target) || neighbor == cells.cend() || !InRoom(neighbor.value(), room)
					|| ((neighbor->flags & 16) && !(neighbor->flags & 0xff00))
					|| !StepOk(cell.flags, neighbor->flags, d.deck)) continue;
				if(neighbor->type == 0 || neighbor->type == 1 || neighbor->type == 15) continue;
				const quint32 cost = storedCosts.value(key) + DirectionCost(d) + MoveCost(neighbor->type);
				if(!storedCosts.contains(target)) storedCosts.insert(target, cost);
				if(portals.contains(target) && !result.contains(target))
				{
					switch(neighbor->type)
					{
					case 3: case 4: case 5: case 6: case 7: case 8: case 9:
					case 10: case 11: case 18: case 75: case 76: result.insert(target, cost); break;
					default: break;
					}
				}
				if(!pendingCosts.contains(target) || cost < pendingCosts.value(target))
				{
					if(pendingCosts.contains(target)) pending.remove(pendingCosts.value(target), target);
					pendingCosts.insert(target, cost); pending.insert(cost, target);
				}
			}
			visited.insert(key);
		}
	}
	return result;
}

bool Stairless(const QMap<quint32, PathableCell>& cells, quint16 room, quint32 from, quint32 to, int mode)
{
	if(from == to) return true;
	QVector<quint32> pending{to}; QSet<quint32> visited{to}; bool first = true;
	while(!pending.isEmpty())
	{
		const quint32 key = pending.takeLast(); const PathableCell cell = cells.value(key);
		const int before = visited.size();
		for(int i = 0; i < 8; ++i)
		{
			const Direction& d = Directions[i];
			if(!(cell.directions & d.mask) || !InGrid(key, d)) continue;
			const quint32 target = Adjacent(key, d); const auto neighbor = cells.constFind(target);
			if(visited.contains(target) || neighbor == cells.cend() || !InRoom(neighbor.value(), room)
				|| ((neighbor->flags & 16) && !(neighbor->flags & 0xff00))
				|| (neighbor->flags & (mode == 2 ? 0xc000 : 0x6000))
				|| neighbor->type == 0 || neighbor->type == 1 || neighbor->type == 15 || neighbor->type == 18) continue;
			if(target == from) return true;
			visited.insert(target); pending.append(target);
		}
		// Preserve DoesStairlessPathExist's no-neighbor result, 1800edd80.
		if(first && visited.size() == before) return true;
		first = false;
	}
	return false;
}
quint32 BottomStep(const QMap<quint32, PathableCell>& cells, quint32 key)
{
	QSet<quint32> visited; bool crossed = false;
	while(!(cells.value(key).flags & 0x1000) && !visited.contains(key))
	{
		visited.insert(key); const PathableCell cell = cells.value(key);
		const Direction& d = Directions[cell.stairs & 7];
		if(!(cell.directions & d.mask) || !InGrid(key, d)) break;
		const quint32 next = Adjacent(key, d); const auto neighbor = cells.constFind(next);
		if(neighbor == cells.cend()) break;
		if(neighbor->flags & 0x2000) { if(crossed) break; crossed = true; }
		key = next;
	}
	return key;
}
bool Connection(const QMap<quint32, PathableCell>& cells, const QMap<quint32, PortalCell>& portals,
	quint32 from, quint32 to)
{
	// TestConnection/DisconnectInaccessibleConnections, 180106120/1800ecd60.
	const PathableCell a = cells.value(from), b = cells.value(to);
	if(Deck(from) == Deck(to))
	{
		QVector<quint16> shared;
		if(InRoom(b, a.room1)) shared.append(a.room1);
		if(HasOpening(a) && a.room2 != a.room1 && InRoom(b, a.room2)) shared.append(a.room2);
		if(shared.isEmpty()) return false;
		const quint16 room = shared.first();
		const bool special = shared.size() == 1 && (a.flags & 0x1c) && (b.flags & 0x1c) && room == a.room2 && room == b.room2;
		if(!special && Stairless(cells, room, from, to, 3)) return true;
		if(!special) return shared.size() > 1 && Stairless(cells, shared.at(1), from, to, 2);
		const quint32 lowerA = Key(X(from), Y(from), Deck(from) - 1), lowerB = Key(X(to), Y(to), Deck(to) - 1);
		if(!cells.contains(lowerA) || !cells.contains(lowerB)) return true;
		return Stairless(cells, room, cells.value(lowerA).flags & 0x8000 ? BottomStep(cells, lowerA) : lowerA,
			cells.value(lowerB).flags & 0x8000 ? BottomStep(cells, lowerB) : lowerB, 2);
	}
	if(Deck(from) < Deck(to))
	{
		const quint32 lower = Key(X(to), Y(to), Deck(from));
		if(!cells.contains(lower)) return true;
		return Stairless(cells, portals.value(to).room2, from,
			cells.value(lower).flags & 0x8000 ? BottomStep(cells, lower) : lower, a.flags & 0x8000 ? 3 : 2);
	}
	const quint32 lower = Key(X(from), Y(from), Deck(to));
	if(!cells.contains(lower)) return true;
	return Stairless(cells, a.room2, cells.value(lower).flags & 0x8000 ? BottomStep(cells, lower) : lower,
		to, b.flags & 0x8000 ? 3 : 2);
}
QMap<quint16, bool> RoomReach(const QMap<quint32, PortalCell>& portals, quint32 start, quint16 room)
{
	// PathRoom1Cells/PathRoom2Cells seed the matching room side, then traverse
	// portal neighbors with the source removed and retain all other room IDs.
	QVector<quint32> pending; QSet<quint32> visited{start}; QMap<quint16, bool> result;
	const PortalCell source = portals.value(start);
	for(auto neighbor = source.neighbors.cbegin(); neighbor != source.neighbors.cend(); ++neighbor)
		if(portals.value(neighbor.key()).room1 == room || portals.value(neighbor.key()).room2 == room) pending.append(neighbor.key());
	while(!pending.isEmpty())
	{
		const quint32 key = pending.takeLast(); if(visited.contains(key)) continue;
		visited.insert(key); const PortalCell cell = portals.value(key);
		if(cell.room1 != room) result.insert(cell.room1, true);
		if(cell.room2 != room) result.insert(cell.room2, true);
		for(auto neighbor = cell.neighbors.cbegin(); neighbor != cell.neighbors.cend(); ++neighbor)
			if(!visited.contains(neighbor.key())) pending.append(neighbor.key());
	}
	return result;
}

quint32 PathCostBuilding(const QMap<quint32, PathableCell>& cells,
	const QMap<quint32, PortalCell>& portals, quint32 from, quint32 to)
{
	if(from == to) return 0;
	if(!portals.contains(from) || !portals.contains(to)) return std::numeric_limits<quint32>::max();
	// PathCellsBuilding/PathCostBuilding, 1800f5f20/1800f6510. Keep room-side
	// pruning, original complete frontier batches, and cost-map duplicate rule.
	QMultiMap<quint32, quint32> pending;
	QHash<quint32, quint32> queued;
	QSet<quint32> visited;
	const PortalCell destination = portals.value(to);
	auto expand = [&](quint32 key, quint32 cost)
	{
		const PortalCell current = portals.value(key);
		for(auto edge = current.neighbors.cbegin(); edge != current.neighbors.cend(); ++edge)
		{
			const quint32 next = edge.key(); if(visited.contains(next)) continue;
			const PortalCell candidate = portals.value(next); const PathableCell cell = cells.value(next);
			if(!Usable(cell)) continue;
			if(next != to)
			{
				const bool first = current.room1 == candidate.room1 || current.room2 == candidate.room1;
				const bool second = current.room1 == candidate.room2 || current.room2 == candidate.room2;
				if(first && second && !candidate.room1Cells.contains(destination.room1)
					&& !candidate.room2Cells.contains(destination.room1)) continue;
				if(first && !second && !candidate.room2Cells.contains(destination.room1)) continue;
				if(!first && second && !candidate.room1Cells.contains(destination.room1)) continue;
			}
			const quint32 total = cost + edge.value();
			if(!queued.contains(next) || total < queued.value(next))
			{
				if(queued.contains(next)) pending.remove(queued.value(next), next);
				queued.insert(next, total); pending.insert(total, next);
			}
		}
		visited.insert(key);
	};
	expand(from, 0);
	while(!pending.isEmpty())
	{
		const auto batch = pending; pending.clear(); queued.clear();
		for(auto next = batch.cbegin(); next != batch.cend(); ++next)
		{
			if(next.value() == to) return next.key();
			expand(next.value(), next.key());
		}
	}
	return std::numeric_limits<quint32>::max();
}

bool NextStep(const QMap<quint32, PathableCell>& cells, const QHash<quint32, quint32>& costs,
	quint32 current, quint32 previous, bool hasPrevious, bool square, bool avoidStairs, quint32& result)
{
	// AuPath::NextStep, 1800efc00. Flat/up-center candidates precede a straight
	// descent; diagonal changes of deck are considered only when those fail.
	const PathableCell cell = cells.value(current);
	const quint32 entryMasks[] = {0x31c00,0x23c00,0x7c00,0xf800,0x1f000,0x3e000,0x3c400,0x38c00};
	bool found = false; quint32 best = 0;
	auto consider = [&](int index, bool checkStairs)
	{
		const Direction& d = Directions[index];
		if(!(cell.directions & d.mask) || !InGrid(current, d)) return;
		const quint32 key = Adjacent(current, d);
		if((hasPrevious && key == previous) || !costs.contains(key)) return;
		const PathableCell candidate = cells.value(key);
		if(checkStairs && avoidStairs && !(cell.flags & 0xff00) && (candidate.flags & 0xff00)) return;
		if(checkStairs && !d.deck && ((cell.flags | candidate.flags) & 0xf000))
		{
			if(!StepOk(cell.flags, candidate.flags, 0)) return;
			if(!(cell.flags & 0xf000) && (candidate.flags & 0x1000)
				&& !(entryMasks[(candidate.stairs >> 4) & 7] & d.mask)) return;
		}
		const quint32 cost = costs.value(key);
		if(!found || cost < best || (cost == best && d.deck)) { found = true; best = cost; result = key; }
	};
	if(square)
	{
		for(int i = 0; i <= 8; i += 2) consider(i, true);
		if(!found || costs.value(current) <= best)
			for(int i = 1; i < 8; i += 2) consider(i, true);
	}
	else for(int i = 0; i <= 8; ++i) consider(i, true);
	consider(17, false);
	if(!found) for(int i = 9; i <= 16; ++i) consider(i, false);
	if(!found) for(int i = 18; i <= 25; ++i) consider(i, false);
	return found;
}

bool ClipAllowed(AuPoint& from, AuPoint& to)
{
	// DnBuilding::BoxClipAllowed, 180158ce0. The max of all three clipping
	// fractions is confirmed by original assembly; the manual C loses that phi.
	constexpr double limit = 229.1025970845;
	const auto coordinate = [](const AuPoint& point, int axis) { return axis == 0 ? point.x : axis == 1 ? point.y : point.z; };
	for(int axis = 0; axis < 3; ++axis)
		if((coordinate(from,axis) < -limit && coordinate(to,axis) < -limit)
			|| (coordinate(from,axis) > limit && coordinate(to,axis) > limit)) return false;
	const AuPoint direction = to - from;
	auto fraction = [&](const AuPoint& point, const AuPoint& delta)
	{
		double result = 0;
		for(int axis = 0; axis < 3; ++axis)
			if(std::abs(coordinate(point,axis)) > limit)
				result = std::max(result, (std::abs(coordinate(point,axis)) - limit) / std::abs(coordinate(delta,axis)));
		return result;
	};
	const double first = fraction(from, direction); if(first > 0) from += direction * first;
	const AuPoint reverse = from - to;
	const double second = fraction(to, reverse); if(second > 0) to += reverse * second;
	return !(to - from).IsZero() && (to - from).Dot(direction) > 0;
}
}

bool ReadFixturePayload(const QByteArray& payload, Fixture& output, QString& error)
{
	error.clear();
	if(payload.size() > MaximumPayloadBytes) { error = QStringLiteral("Building path payload exceeds limit."); return false; }
	Fixture result;
	Reader reader(payload, error);
	quint32 count = 0;
	if(!reader.Count(count, 6)) return false;
	for(quint32 i = 0; i < count; ++i)
	{
		quint32 key = 0; quint8 mask = 0; ExtantCell cell;
		if(!reader.U32(key) || !reader.U8(cell.type) || !reader.U8(mask)) return false;
		if(!ValidKey(key) || result.extant.contains(key)) return reader.Fail(QStringLiteral("invalid extant key"));
		for(int room = 0; room < 8; ++room)
			if((mask & (1 << room)) && !reader.U16(cell.rooms[room])) return false;
		result.extant.insert(key, cell);
	}
	quint8 spacecraft = 0;
	if(!reader.U8(spacecraft)) return false;
	if(spacecraft != 0) return reader.Fail(QStringLiteral("BP fixture is marked as spacecraft"));
	if(!reader.Count(count, 25)) return false;
	for(quint32 i = 0; i < count; ++i)
	{
		quint32 key = 0; PathableCell cell;
		if(!reader.U32(key) || !reader.U32(cell.directions) || !reader.U8(cell.type) || !Fields(reader, cell)) return false;
		if((cell.flags & 0xf000) && !reader.U8(cell.stairs)) return false;
		if(!ValidKey(key) || result.pathable.contains(key)) return reader.Fail(QStringLiteral("invalid pathable key"));
		result.pathable.insert(key, cell);
	}
	if(!reader.Count(count, 33)) return false;
	for(quint32 i = 0; i < count; ++i)
	{
		quint32 key = 0, neighbors = 0; PortalCell cell; PathableCell fields;
		if(!reader.U32(key) || !reader.U8(cell.type) || !reader.Count(neighbors, 8)) return false;
		for(quint32 n = 0; n < neighbors; ++n)
		{
			quint32 target = 0, cost = 0;
			if(!reader.U32(target) || !reader.U32(cost)) return false;
			if(cell.neighbors.contains(target)) return reader.Fail(QStringLiteral("duplicate portal target"));
			cell.neighbors.insert(target, cost);
		}
		if(!Fields(reader, fields) || !RoomMap(reader, cell.room1Cells) || !RoomMap(reader, cell.room2Cells)) return false;
		cell.doorIndex = fields.doorIndex; cell.holeIndex = fields.holeIndex; cell.flags = fields.flags;
		cell.railingIndex = fields.railingIndex; cell.room1 = fields.room1; cell.room2 = fields.room2;
		if(!ValidKey(key) || result.portals.contains(key) || !result.pathable.contains(key))
			return reader.Fail(QStringLiteral("invalid portal key"));
		result.portals.insert(key, cell);
	}
	for(const PortalCell& portal : result.portals)
		for(auto neighbor = portal.neighbors.cbegin(); neighbor != portal.neighbors.cend(); ++neighbor)
			if(!result.portals.contains(neighbor.key())) return reader.Fail(QStringLiteral("dangling portal target"));
	if(reader.Remaining()) return reader.Fail(QStringLiteral("trailing payload bytes"));
	result.payloadSha256 = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
	output = std::move(result); return true;
}

bool ReadFixture(const QByteArray& source, Fixture& output, QString& error)
{
	error.clear();
	if(source.size() < 19 || source.size() > MaximumCacheBytes || source.left(2) != QByteArray::fromHex("1c11"))
	{
		error = QStringLiteral("Invalid original BP cache header or size."); return false;
	}
	const QByteArray timestamp = source.mid(2, 9);
	QDataStream stream(timestamp); stream.setVersion(QDataStream::Qt_4_0);
	QDateTime cacheTime; stream >> cacheTime;
	if(stream.status() != QDataStream::Ok || !cacheTime.isValid())
	{
		error = QStringLiteral("Invalid original BP cache timestamp."); return false;
	}
	const QByteArray lengthBytes = source.mid(11, 4);
	Reader lengthReader(lengthBytes, error); quint32 length = 0;
	if(!lengthReader.U32(length) || length != quint32(source.size() - 15))
	{
		error = QStringLiteral("Original BP compressed length mismatch."); return false;
	}
	QByteArray payload;
	if(!Uncompress(source.mid(15), payload, error)) return false;
	Fixture result; if(!ReadFixturePayload(payload, result, error)) return false;
	result.cacheTime = cacheTime;
	result.sourceSha256 = QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex();
	output = std::move(result); return true;
}

bool Graph::Build(const Design& design, const Options& options, QString& error)
{
	m_Extant.clear(); m_Pathable.clear(); m_Portals.clear(); m_Boundaries.clear(); m_RoomTypes.clear();
	// CreatePath analyzes the full authored design before CreateBuilding applies
	// constructed limits. Keeping it intact also keeps opening indices stable
	// when a stacked building gains or loses a completed floor.
	const AuLegacyShip::Design authored = design.AuthoringDesign();
	AuLegacyShip::Paths::Graph shared;
	if(!shared.BuildBuilding(authored, options.minimumDeck, options.maximumDeck, options.siteRadius, error)) return false;
	for(auto cell = shared.ExtantCells().cbegin(); cell != shared.ExtantCells().cend(); ++cell)
	{
		ExtantCell value; value.type = cell.value();
		const QVector<quint16> rooms = shared.ExtantRooms().value(cell.key());
		for(int i = 0; i < std::min(8, int(rooms.size())); ++i) value.rooms[i] = rooms.at(i);
		m_Extant.insert(cell.key(), value);
	}
	const auto cells = shared.PathableCells();
	for(auto cell = cells.cbegin(); cell != cells.cend(); ++cell)
	{
		PathableCell value;
		value.directions = cell->directions; value.type = cell->type; value.flags = cell->flags;
		value.room1 = cell->room1; value.room2 = cell->room2; value.stairs = cell->stairs;
		value.doorIndex = cell->doorIndex; value.holeIndex = cell->holeIndex;
		value.railingIndex = cell->railingIndex; value.doorDetailId = cell->doorDetailId;
		m_Pathable.insert(cell.key(), value);
	}
	for(const AuLegacyShip::Room& room : authored.rooms)
	{
		m_Boundaries.insert(room.roomId, AuLegacyShip::RoomBoundary(room));
		m_RoomTypes.insert(room.roomId, room.roomType);
	}
	for(quint32 key : shared.PortalCells())
	{
		const PathableCell cell = m_Pathable.value(key); PortalCell portal;
		portal.type = cell.type == 75 ? 8 : cell.type == 76 ? 9 : cell.type;
		portal.doorIndex = cell.doorIndex; portal.holeIndex = cell.holeIndex;
		portal.railingIndex = cell.railingIndex; portal.flags = cell.flags & 0xff00;
		portal.room1 = cell.room1; portal.room2 = cell.room2;
		m_Portals.insert(key, portal);
	}
	for(auto portal = m_Portals.begin(); portal != m_Portals.end(); ++portal)
	{
		const PathableCell cell = m_Pathable.value(portal.key());
		portal->neighbors = AccessibleBuildingDoors(m_Pathable, m_Portals, portal.key(), cell.room1);
		if(HasOpening(cell))
		{
			const auto second = AccessibleBuildingDoors(m_Pathable, m_Portals, portal.key(), cell.room2);
			for(auto candidate = second.cbegin(); candidate != second.cend(); ++candidate)
				if(!portal->neighbors.contains(candidate.key())) portal->neighbors.insert(candidate.key(), candidate.value());
		}
	}
	// The original removes both directions when an existing edge fails its
	// stairless connection test. An absent reverse edge is never manufactured;
	// railings can retain incoming edges while their own neighbor map is empty.
	for(auto portal = m_Portals.begin(); portal != m_Portals.end(); ++portal)
	{
		const auto candidates = portal->neighbors;
		for(auto candidate = candidates.cbegin(); candidate != candidates.cend(); ++candidate)
			if(!Connection(m_Pathable, m_Portals, portal.key(), candidate.key()))
			{
				portal->neighbors.remove(candidate.key());
				m_Portals[candidate.key()].neighbors.remove(portal.key());
			}
	}
	for(auto portal = m_Portals.begin(); portal != m_Portals.end(); ++portal)
	{
		portal->room1Cells = RoomReach(m_Portals, portal.key(), portal->room1);
		portal->room2Cells = RoomReach(m_Portals, portal.key(), portal->room2);
	}
	m_Options = options; return true;
}

bool Graph::Terminal(const AuPoint& point, quint32& key,
	bool useGroundLevelDoors, bool* outside) const
{
	if(outside) *outside = false;
	if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)
		|| std::abs(point.x) > 32766 * AuLegacyShip::CoordinateSpacing
		|| std::abs(point.y) > 32766 * AuLegacyShip::CoordinateSpacing) return false;
	const double deckNumber = std::floor((point.z + .05) / AuLegacyBd::DeckHeight);
	if(deckNumber < -127 || deckNumber > 127) return false;
	const int deck = int(deckNumber), x = int(std::round(point.x / AuLegacyShip::CoordinateSpacing)) + 128;
	const int y = int(std::round(point.y / AuLegacyShip::CoordinateSpacing)) + 128;
	const bool inGrid = x >= 0 && x <= 255 && y >= 0 && y <= 255;
	key = inGrid ? Key(x, y, deck) : 0;
	const auto direct = inGrid ? m_Pathable.constFind(key) : m_Pathable.cend();
	if(direct != m_Pathable.cend()) return Usable(direct.value());
	const auto extant = inGrid ? m_Extant.constFind(key) : m_Extant.cend();
	if(extant == m_Extant.cend())
	{
		// BuildingPathTerminalInfo/ClosestBuildingEntrance, 1800e6350/1800e88b0.
		// Ground policy searches exterior ground cells by XY only; the other
		// policy searches hull openings by XY plus deck distance.
		if(outside) *outside = true;
		qint64 distance = std::numeric_limits<qint64>::max(); bool found = false;
		for(auto cell = m_Pathable.cbegin(); cell != m_Pathable.cend(); ++cell)
		{
			if(!Usable(cell.value())) continue;
			if(useGroundLevelDoors ? (cell->type != 16 || Deck(cell.key()) != 0)
				: (!m_Portals.contains(cell.key()) || (cell->type != 4 && cell->type != 6 && cell->type != 7 && cell->type != 11))) continue;
			const qint64 dx = x - X(cell.key()), dy = y - Y(cell.key());
			const qint64 dz = useGroundLevelDoors ? 0 : deck - Deck(cell.key());
			const qint64 candidate = dx * dx + dy * dy + dz * dz;
			if(candidate < distance) { distance = candidate; key = cell.key(); found = true; }
		}
		return found;
	}
	quint16 room = 0;
	for(quint16 candidate : extant->rooms)
	{
		if(!candidate) break;
		AuPointArray polygon;
		for(const QPointF& vertex : m_Boundaries.value(candidate)) polygon.append(AuPoint(vertex.x(), vertex.y(), 0));
		const AuPoint location(point.x, point.y, 0);
		if(polygon.PointInBoundary(location) || polygon.PointOnBoundary_2D(location)) { room = candidate; break; }
	}
	double distance = std::numeric_limits<double>::max(); bool found = false;
	for(int i = 0; i < 8; ++i)
	{
		const Direction& d = Directions[i];
		if(x + d.x < 0 || x + d.x > 255 || y + d.y < 0 || y + d.y > 255) continue;
		const quint32 adjacent = Key(x + d.x, y + d.y, deck);
		const auto cell = m_Pathable.constFind(adjacent);
		if(cell == m_Pathable.cend() || !Usable(cell.value())
			|| (cell->room1 != room && cell->room2 != room)) continue;
		const double dx = (x + d.x - 128) * AuLegacyShip::CoordinateSpacing - point.x;
		const double dy = (y + d.y - 128) * AuLegacyShip::CoordinateSpacing - point.y;
		if(dx * dx + dy * dy < distance) { distance = dx * dx + dy * dy; key = adjacent; found = true; }
	}
	return found;
}

bool Graph::MatchesFixtureCells(const Fixture& fixture, QString& error) const
{
	error.clear();
	if(m_Extant.size() != fixture.extant.size() || m_Pathable.size() != fixture.pathable.size())
	{
		error = QStringLiteral("Building cells differ: generated %1/%2 extant/pathable, original %3/%4.")
			.arg(m_Extant.size()).arg(m_Pathable.size()).arg(fixture.extant.size()).arg(fixture.pathable.size());
		return false;
	}
	for(auto cell = fixture.extant.cbegin(); cell != fixture.extant.cend(); ++cell)
	{
		const auto generated = m_Extant.constFind(cell.key());
		bool equal = generated != m_Extant.cend() && generated->type == cell->type;
		if(equal)
			for(int i = 0; i < 8; ++i) if(generated->rooms[i] != cell->rooms[i]) { equal = false; break; }
		if(!equal)
		{
			error = QStringLiteral("Original extant cell 0x%1 differs.").arg(cell.key(), 0, 16); return false;
		}
	}
	for(auto cell = fixture.pathable.cbegin(); cell != fixture.pathable.cend(); ++cell)
	{
		const auto generated = m_Pathable.constFind(cell.key());
		if(generated == m_Pathable.cend() || generated->directions != cell->directions || generated->type != cell->type
			|| generated->doorIndex != cell->doorIndex || generated->holeIndex != cell->holeIndex
			|| generated->flags != cell->flags || generated->railingIndex != cell->railingIndex
			|| generated->room1 != cell->room1 || generated->room2 != cell->room2 || generated->stairs != cell->stairs)
		{
			error = QStringLiteral("Original pathable cell 0x%1 differs.").arg(cell.key(), 0, 16); return false;
		}
	}
	if(m_Portals.size() != fixture.portals.size())
	{
		error = QStringLiteral("Generated portal count differs from the original."); return false;
	}
	for(auto portal = fixture.portals.cbegin(); portal != fixture.portals.cend(); ++portal)
	{
		const auto generated = m_Portals.constFind(portal.key());
		if(generated == m_Portals.cend() || generated->type != portal->type || generated->flags != portal->flags
			|| generated->doorIndex != portal->doorIndex || generated->holeIndex != portal->holeIndex
			|| generated->railingIndex != portal->railingIndex || generated->room1 != portal->room1 || generated->room2 != portal->room2
			|| generated->neighbors != portal->neighbors || generated->room1Cells != portal->room1Cells || generated->room2Cells != portal->room2Cells)
		{
			error = QStringLiteral("Original portal cell 0x%1 differs.").arg(portal.key(), 0, 16); return false;
		}
	}
	return true;
}

bool Graph::CanMoveDirectly(const AuPoint& inputFrom, const AuPoint& inputTo,
	bool& usesStairs) const
{
	usesStairs = false;
	if(!std::isfinite(inputFrom.x) || !std::isfinite(inputFrom.y) || !std::isfinite(inputFrom.z)
		|| !std::isfinite(inputTo.x) || !std::isfinite(inputTo.y) || !std::isfinite(inputTo.z)) return false;
	if(std::floor((inputFrom.z + .05) / AuLegacyBd::DeckHeight)
		!= std::floor((inputTo.z + .05) / AuLegacyBd::DeckHeight)) return false;
	AuPoint from = inputFrom, to = inputTo;
	if(!ClipAllowed(from, to)) return true;
	const int deck = int(std::floor((from.z + .05) / AuLegacyBd::DeckHeight));
	auto coordinate = [](double value) { return int(std::round(value / AuLegacyShip::CoordinateSpacing)) + 128; };
	int x = coordinate(from.x), y = coordinate(from.y);
	const int endX = coordinate(to.x), endY = coordinate(to.y);
	auto polygon = [&](quint16 room)
	{
		AuPointArray boundary;
		for(const QPointF& vertex : m_Boundaries.value(room)) boundary.append(AuPoint(vertex.x(), vertex.y(), 0));
		return boundary;
	};
	auto inside = [&](const AuPoint& location)
	{
		const int gx = coordinate(location.x), gy = coordinate(location.y);
		if(gx < 0 || gx > 255 || gy < 0 || gy > 255) return quint16(0);
		const auto extant = m_Extant.constFind(Key(gx, gy, deck));
		if(extant == m_Extant.cend()) return quint16(0);
		for(quint16 room : extant->rooms)
		{
			if(!room) break;
			const AuPointArray boundary = polygon(room);
			if(!boundary.isEmpty() && (boundary.PointInBoundary(location) || boundary.PointOnBoundary_2D(location))) return room;
		}
		return quint16(0);
	};
	auto roomHit = [&](const AuPoint& first, const AuPoint& last)
	{
		// DnRoomArray::DoesSegmentIntersectRoom, 180170ff0.
		const quint16 firstRoom = inside(first), lastRoom = inside(last);
		if(!firstRoom) return true;
		const AuPointArray boundary = polygon(firstRoom);
		if(boundary.isEmpty()) return false;
		AuPoint a, b;
		if(firstRoom != lastRoom) return boundary.ClosestBoundarySegment_2D(first, a, b) >= 0;
		if(!first.FuzzyEquals_2D(last) && boundary.DoesIntersectBoundary_2D(first, last, a, b)) return true;
		const double distance = boundary.ClosestBoundarySegment_2D(last, a, b);
		if(distance < 0 || distance > .083) return false;
		return first.Distance_2D(first.PointOnSegment_2D(a, b)) >= distance;
	};
	auto wallCell = [&](int gx, int gy, bool fromInside, bool toInside)
	{
		// TestWallCellPath clips the actual segment to the half-grid square,
		// retaining original boundary iteration order for its first/last hits.
		const double cx = (gx - 128) * AuLegacyShip::CoordinateSpacing;
		const double cy = (gy - 128) * AuLegacyShip::CoordinateSpacing;
		constexpr double half = 1.41421356225;
		AuPointArray box, hits;
		box.append(AuPoint(cx-half,cy-half,0)); box.append(AuPoint(cx-half,cy+half,0));
		box.append(AuPoint(cx+half,cy+half,0)); box.append(AuPoint(cx+half,cy-half,0));
		if(!box.IntersectBoundary_2D(from, to, hits)) return !roomHit(from, to);
		if(hits.size() > 1) return !roomHit(hits.first(), hits.last());
		if(fromInside) return !roomHit(from, hits.first());
		if(toInside) return !roomHit(hits.first(), to);
		return true;
	};
	auto clippedCell = [&](int gx, int gy)
	{
		if(gx < 0 || gx > 255 || gy < 0 || gy > 255) return false;
		const auto extant = m_Extant.constFind(Key(gx, gy, deck));
		return extant != m_Extant.cend() && extant->type == 1 && wallCell(gx, gy, false, false);
	};
	auto diagonal = [&](int gx, int gy, quint32 directions, quint32 move)
	{
		// TestDiagonalCornerClip, 180106620: which adjacent cardinal cell the
		// actual segment clips is selected by its signed side at the corner.
		if(!(move & 0x2a800)) return true;
		int dx = 1, dy = 1; quint32 mx = 0x400, my = 0x1000;
		bool chooseXWhenNonnegative = true;
		if(move == 0x2000) { dx=-1; mx=0x4000; chooseXWhenNonnegative=false; }
		else if(move == 0x8000) { dx=-1; dy=-1; mx=0x4000; my=0x10000; }
		else if(move == 0x20000) { dy=-1; my=0x10000; chooseXWhenNonnegative=false; }
		if((directions & (mx | my)) == (mx | my)) return true;
		const double cx = (gx - 128) * AuLegacyShip::CoordinateSpacing + dx * 1.41421356225;
		const double cy = (gy - 128) * AuLegacyShip::CoordinateSpacing + dy * 1.41421356225;
		const double side = (to.y-cy)*(from.x-cx) - (from.y-cy)*(to.x-cx);
		const bool chooseX = (side >= 0) == chooseXWhenNonnegative;
		return chooseX ? ((directions & mx) || clippedCell(gx+dx, gy))
			: ((directions & my) || clippedCell(gx, gy+dy));
	};
	auto threshold = [](quint8 type) { return type < 18 && ((0x30890U >> type) & 1U); };
	const int dx = endX - x, dy = endY - y, steps = std::max(std::abs(dx), std::abs(dy));
	const int biasX = std::abs(dy) > std::abs(dx) && dx ? (std::abs(dy) / std::abs(dx)) / 2 : 0;
	const int biasY = std::abs(dx) > std::abs(dy) && dy ? (std::abs(dx) / std::abs(dy)) / 2 : 0;
	const int startX = x, startY = y;
	auto railingSide = [&](const PathableCell& cell, int gx, int gy, bool preventApproach)
	{
		const double cx = (gx-128)*AuLegacyShip::CoordinateSpacing, cy = (gy-128)*AuLegacyShip::CoordinateSpacing;
		QPointF vector(1,0);
		if((cell.flags & 0xc0) == 0xc0) vector = QPointF(-0.7071,0.7071);
		else if(cell.flags & 0x80) vector = QPointF(0,1);
		else if(cell.flags & 0x40) vector = QPointF(0.7071,0.7071);
		const auto side = [&](const AuPoint& p) { const double value = (cy+vector.y()-p.y)*(cx-p.x) - (cx+vector.x()-p.x)*(cy-p.y);
			return value < 0 ? -1 : value > 0 ? 1 : 0; };
		const int a = side(from), b = side(to);
		if(!a || !b || a != b) return false;
		if(!preventApproach) return true;
		// The original same-cell branch also prevents approaching a railing.
		// Its sampled multi-cell branch only tests which side the endpoints use.
		const double firstDistance = from.DistToLine(AuPoint(cx,cy,from.z), AuPoint(cx+vector.x(),cy+vector.y(),from.z));
		const double lastDistance = to.DistToLine(AuPoint(cx,cy,to.z), AuPoint(cx+vector.x(),cy+vector.y(),to.z));
		return lastDistance >= firstDistance;
	};
	if(!steps)
	{
		const auto current = m_Pathable.constFind(Key(x, y, deck));
		if(current != m_Pathable.cend())
		{
			usesStairs = current->flags & 0xff00;
			return current->type != 18 || railingSide(current.value(),x,y,true);
		}
		const auto extant = m_Extant.constFind(Key(x, y, deck));
		return extant == m_Extant.cend() || (extant->type == 1 && !roomHit(from, to));
	}
	for(int i = 1; i <= steps; ++i)
	{
		const int nx = startX + (biasX+i)*dx/steps, ny = startY + (biasY+i)*dy/steps;
		if(nx < 0 || nx > 255 || ny < 0 || ny > 255 || std::abs(deck) > 43) return true;
		const quint32 key = Key(x,y,deck), nextKey = Key(nx,ny,deck);
		const auto current = m_Pathable.constFind(key), next = m_Pathable.constFind(nextKey);
		const auto currentExtant = m_Extant.constFind(key), nextExtant = m_Extant.constFind(nextKey);
		if(current == m_Pathable.cend())
		{
			if(currentExtant != m_Extant.cend())
			{
				if(currentExtant->type != 1 || !wallCell(x,y,true,false)) return false;
			}
			if(next == m_Pathable.cend())
			{
				if(nextExtant != m_Extant.cend()
					&& (nextExtant->type != 1 || currentExtant == m_Extant.cend() || !wallCell(nx,ny,false,true))) return false;
			}
			else if(currentExtant == m_Extant.cend() && !threshold(next->type)) return false;
		}
		else
		{
			usesStairs = usesStairs || (current->flags & 0xff00);
			if(i == 1 && current->type == 18 && !railingSide(current.value(),x,y,false)) return false;
			quint32 move = 0;
			for(int d = 0; d < 8; ++d) if(Directions[d].x == nx-x && Directions[d].y == ny-y) { move = Directions[d].mask; break; }
			if(!(current->directions & move))
			{
				if(threshold(current->type)) { if(nextExtant != m_Extant.cend()) return false; }
				else if(nextExtant == m_Extant.cend() || nextExtant->type != 1
					|| !diagonal(x,y,current->directions,move) || !wallCell(nx,ny,false,true)) return false;
			}
			else
			{
				if(!diagonal(x,y,current->directions,move)) return false;
				if(next != m_Pathable.cend() && next->type == 18) return false;
			}
		}
		x=nx; y=ny;
	}
	return true;
}

bool Graph::Route(const AuPoint& from, const AuPoint& to, QVector<RoutePoint>& output,
	bool useGroundLevelDoors) const
{
	output.clear(); quint32 source = 0, target = 0; bool fromOutside = false, toOutside = false;
	if(!Terminal(from, source, useGroundLevelDoors, &fromOutside)
		|| !Terminal(to, target, useGroundLevelDoors, &toOutside)) return false;
	auto point = [&](quint32 key)
	{
		const PathableCell cell = m_Pathable.value(key); RoutePoint result;
		result.location = AuPoint((X(key) - 128) * AuLegacyShip::CoordinateSpacing,
			(Y(key) - 128) * AuLegacyShip::CoordinateSpacing, Deck(key) * AuLegacyBd::DeckHeight + 4);
		result.cell = key; result.flags = cell.flags; result.doorIndex = cell.doorIndex;
		result.holeIndex = cell.holeIndex; result.railingIndex = cell.railingIndex; return result;
	};
	if(fromOutside && toOutside) { RoutePoint end; end.location = to; output.append(end); return true; }
	if(!fromOutside)
	{
		const int x = int(std::round(from.x / AuLegacyShip::CoordinateSpacing)) + 128;
		const int y = int(std::round(from.y / AuLegacyShip::CoordinateSpacing)) + 128;
		const int deck = int(std::floor((from.z + .05) / AuLegacyBd::DeckHeight));
		if(!m_Pathable.contains(Key(x, y, deck))) { output.append(point(source)); return true; }
	}
	if(m_Pathable.value(target).type == 6)
	{
		target = Key(X(target), Y(target), Deck(target) - 1);
		if(!m_Pathable.contains(target)) return false;
	}
	const PathableCell a = m_Pathable.value(source), b = m_Pathable.value(target);
	const bool sharedInside = (a.room1 && InRoom(b, a.room1)) || (HasOpening(a) && a.room2 && InRoom(b, a.room2));
	const AuPoint sourcePoint = fromOutside ? point(source).location : from;
	const AuPoint targetPoint = toOutside ? point(target).location : to;
	if(sharedInside || source == target)
	{
		if(AcrossCells(sourcePoint, targetPoint, output, useGroundLevelDoors))
		{
			if(fromOutside) output.prepend(point(source));
			if(toOutside && output.size() < 10 && !output.isEmpty() && output.last().cell == target)
			{ RoutePoint end; end.location = to; output.append(end); }
			return true;
		}
	}
	// BuildingRoute distills reachable source doors against the target portal
	// or its reachable doors, then returns one across-room leg. Its NPC caller
	// replans after consuming that short leg; do not skip intermediate portals.
	auto doors = [&](quint32 key)
	{
		QMap<quint32, quint32> result;
		const auto cached = m_Portals.constFind(key);
		if(cached != m_Portals.cend()) result = cached->neighbors;
		else
		{
			const PathableCell cell = m_Pathable.value(key);
			result = AccessibleBuildingDoors(m_Pathable, m_Portals, key, cell.room1);
			if(HasOpening(cell))
			{
				const auto second = AccessibleBuildingDoors(m_Pathable, m_Portals, key, cell.room2);
				for(auto entry = second.cbegin(); entry != second.cend(); ++entry)
					if(!result.contains(entry.key())) result.insert(entry.key(), entry.value());
			}
		}
		for(auto entry = result.begin(); entry != result.end(); )
			if(!Usable(m_Pathable.value(entry.key()))) entry = result.erase(entry); else ++entry;
		return result;
	};
	const auto fromDoors = doors(source);
	QMap<quint32, quint32> toDoors;
	if(HasOpening(b) && m_Portals.contains(target)) toDoors.insert(target, 0); else toDoors = doors(target);
	quint32 selected = 0; quint64 best = std::numeric_limits<quint64>::max();
	for(auto end = toDoors.cbegin(); end != toDoors.cend(); ++end)
		for(auto start = fromDoors.cbegin(); start != fromDoors.cend(); ++start)
		{
			const quint32 cost = PathCostBuilding(m_Pathable, m_Portals, start.key(), end.key());
			if(cost == std::numeric_limits<quint32>::max()) continue;
			const quint64 total = quint64(cost) + start.value() + end.value();
			if(total < best) { best = total; selected = start.key(); }
		}
	if(best == std::numeric_limits<quint64>::max()) return false;
	if(!AcrossCells(sourcePoint, point(selected).location, output, useGroundLevelDoors)) return false;
	if(fromOutside) output.prepend(point(source));
	return !output.isEmpty();
}

bool Graph::AcrossCells(const AuPoint& from, const AuPoint& to, QVector<RoutePoint>& output,
	bool useGroundLevelDoors) const
{
	output.clear(); quint32 source = 0, target = 0;
	bool fromOutside = false, toOutside = false;
	if(!Terminal(from, source, useGroundLevelDoors, &fromOutside)
		|| !Terminal(to, target, useGroundLevelDoors, &toOutside)) return false;
	if(fromOutside && toOutside)
	{
		RoutePoint point; point.location = to; output.append(point); return true;
	}
	if(m_Pathable.value(target).type == 6)
	{
		const quint32 below = Key(X(target), Y(target), Deck(target) - 1);
		if(!m_Pathable.contains(below)) return false;
		target = below;
	}
	const PathableCell initial = m_Pathable.value(source), finish = m_Pathable.value(target);
	bool directUsesStairs = false;
	if(Deck(source) == Deck(target) && CanMoveDirectly(from, to, directUsesStairs) && !directUsesStairs)
	{
		// RouteAcrossBuildingRoom's continuous shortcut retains the requested
		// location and destination opening reference, without half-grid turns.
		RoutePoint endpoint; endpoint.location = to; endpoint.cell = target; endpoint.flags = finish.flags;
		endpoint.doorIndex = finish.doorIndex; endpoint.holeIndex = finish.holeIndex; endpoint.railingIndex = finish.railingIndex;
		output.append(endpoint); return true;
	}
	if(source == target)
	{
		RoutePoint endpoint; endpoint.location = to; endpoint.cell = target; endpoint.flags = finish.flags;
		endpoint.doorIndex = finish.doorIndex; endpoint.holeIndex = finish.holeIndex; endpoint.railingIndex = finish.railingIndex;
		output.append(endpoint); return true;
	}
	QVector<quint16> rooms;
	if(InRoom(finish, initial.room1)) rooms.append(initial.room1);
	if(HasOpening(initial) && initial.room2 != initial.room1 && InRoom(finish, initial.room2)) rooms.append(initial.room2);
	if(rooms.isEmpty()) return false;
	for(quint16 room : rooms)
	{
		// RouteAcrossBuildingRoom seeds AccessibleCells at the destination,
		// searching back toward the caller with the same original frontier rules.
		QHash<quint32, quint32> costs, queued;
		QMultiMap<quint32, quint32> pending;
		QSet<quint32> visited;
		costs.insert(target, 0);
		int mode = Deck(source) < Deck(target) && (finish.flags & 0xc00) ? 1 : 0;
		if(Deck(source) == Deck(target) && HasOpening(initial) && HasOpening(finish)
			&& (!((initial.flags & 0xc00) && (finish.flags & 0xc00) && room == initial.room2))
			&& !(initial.flags & 0x8000)) mode = 2;
		auto expand = [&](quint32 key)
		{
			const PathableCell cell = m_Pathable.value(key); bool bottom = false;
			for(const Direction& d : Directions)
			{
				if(!(cell.directions & d.mask) || !InGrid(key, d)) continue;
				const quint32 next = Adjacent(key, d); const auto candidate = m_Pathable.constFind(next);
				if(visited.contains(next) || candidate == m_Pathable.cend() || !InRoom(candidate.value(), room)
					|| !Usable(candidate.value())) continue;
				if(mode == 1)
				{
					if(!(candidate->flags & 0xf000) || Deck(next) != Deck(source)) continue;
					if(candidate->flags & 0x1000) { if(d.deck) continue; bottom = true; }
				}
				else if(mode == 2 || mode == 3)
				{
					if(d.deck || (candidate->flags & (mode == 2 ? 0xc000 : 0x6000))) continue;
				}
				else
				{
					if(mode == 4 && Deck(next) != Deck(source) && !((cell.flags & 0xc000) && (candidate->flags & 0x800))) continue;
					if(!StepOk(cell.flags, candidate->flags, d.deck)) continue;
				}
				const quint32 cost = costs.value(key) + DirectionCost(d) + MoveCost(candidate->type);
				if(!costs.contains(next)) costs.insert(next, cost);
				if(!queued.contains(next) || cost < queued.value(next))
				{
					if(queued.contains(next)) pending.remove(queued.value(next), next);
					queued.insert(next, cost); pending.insert(cost, next);
				}
			}
			visited.insert(key); if(bottom) mode = 4;
		};
		expand(target);
		bool found = false;
		while(!pending.isEmpty() && !found)
		{
			const auto batch = pending; pending.clear(); queued.clear();
			for(auto next = batch.cbegin(); next != batch.cend(); ++next)
			{
				if(next.value() == source) { found = true; break; }
				expand(next.value());
			}
		}
		if(!found) continue;
		const quint8 roomType = m_RoomTypes.value(room);
		const bool square = roomType == 8 || roomType == 16 || roomType == 17 || roomType == 21;
		const bool avoidStairs = Deck(source) == Deck(target) && !(finish.flags & 0xff00);
		quint32 current = source, previous = 0; bool hasPrevious = false;
		int lastX = 0, lastY = 0; bool hasDirection = false;
		for(int steps = 0; current != target && steps < MaximumRecords; ++steps)
		{
			quint32 next = 0;
			if(!NextStep(m_Pathable, costs, current, previous, hasPrevious, square, avoidStairs, next)) break;
			const PathableCell cell = m_Pathable.value(next);
			const int dx = X(next) - X(current), dy = Y(next) - Y(current);
			RoutePoint point;
			point.location = AuPoint((X(next) - 128) * AuLegacyShip::CoordinateSpacing,
				(Y(next) - 128) * AuLegacyShip::CoordinateSpacing, Deck(next) * AuLegacyBd::DeckHeight + 4);
			point.cell = next; point.flags = cell.flags; point.doorIndex = cell.doorIndex;
			point.holeIndex = cell.holeIndex; point.railingIndex = cell.railingIndex;
			// RouteBacktrack replaces a collinear waypoint, retaining direction,
			// deck and flags. It returns after ten turns for the caller to replan.
			if(hasDirection && lastX == dx && lastY == dy && !output.isEmpty()
				&& Deck(current) == Deck(next) && output.last().flags == cell.flags)
				output.last() = point;
			else output.append(point);
			costs[next] = costs.value(next) + 1000;
			previous = current; current = next; hasPrevious = true;
			lastX = dx; lastY = dy; hasDirection = true;
			if(output.size() >= 10 && current != target) return true;
		}
		if(current == target) break;
		if(!output.isEmpty()) return true;
	}
	if(output.isEmpty()) return false;
	const int targetX = int(std::round(to.x / AuLegacyShip::CoordinateSpacing)) + 128;
	const int targetY = int(std::round(to.y / AuLegacyShip::CoordinateSpacing)) + 128;
	const int targetDeck = int(std::floor((to.z + .05) / AuLegacyBd::DeckHeight));
	// ClosestPathableBuilding snaps occupied destinations to an adjacent cell;
	// a genuinely pathable target retains the supplied location after turns.
	if(target == Key(targetX, targetY, targetDeck) && output.last().location != to)
	{
		RoutePoint endpoint = output.last(); endpoint.location = to; output.append(endpoint);
	}
	return !output.isEmpty();
}
}
}
