#ifndef AuLegacyBuildingPaths_H
#define AuLegacyBuildingPaths_H

#include "aulegacybd.h"
#include "aulegacyshippaths.h"
#include <QMap>

namespace AuLegacyBd
{
namespace Paths
{
struct ExtantCell
{
	quint8 type = 0;
	// Keep the eight original slots, including sparse masks in cache files.
	quint16 rooms[8] = {};
};

struct PathableCell
{
	quint32 directions = 0x200;
	quint8 type = 0;
	qint16 doorIndex = -1;
	qint32 holeIndex = -1;
	quint16 flags = 0;
	qint32 railingIndex = -1;
	quint16 room1 = 0, room2 = 0;
	quint8 stairs = 0;
	// Generated only. The original cache serializes opening indices instead.
	quint32 doorDetailId = 0;
};

struct PortalCell
{
	quint8 type = 0;
	QMap<quint32, quint32> neighbors;
	qint16 doorIndex = -1;
	qint32 holeIndex = -1;
	quint16 flags = 0;
	qint32 railingIndex = -1;
	quint16 room1 = 0, room2 = 0;
	QMap<quint16, bool> room1Cells, room2Cells;
};

struct Fixture
{
	QMap<quint32, ExtantCell> extant;
	QMap<quint32, PathableCell> pathable;
	QMap<quint32, PortalCell> portals;
	bool spacecraft = false;
	QDateTime cacheTime;
	QByteArray sourceSha256, payloadSha256;
};

// Original BP stream: 1c11, Qt_4_0 QDateTime, qCompress QByteArray,
// extant hash, spacecraft flag, pathable hash, portal hash. Fixture loading is
// for parity inspection only; generated runtime paths never depend on BP files.
AUDATA_DLL bool ReadFixture(const QByteArray& source, Fixture& output, QString& error);
AUDATA_DLL bool ReadFixturePayload(const QByteArray& payload, Fixture& output, QString& error);

struct Options
{
	int minimumDeck = 0, maximumDeck = 0;
	// Original DpBuilding::UpdatePathCache passes TerrainLevelRadius + one half-grid.
	// The caller supplies this environmental radius in feet, not an industry ID.
	double siteRadius = 0;
};

struct RoutePoint
{
	AuPoint location; // Original building-local feet, z up, deck middle (+4).
	quint32 cell = 0;
	qint16 doorIndex = -1;
	qint32 holeIndex = -1, railingIndex = -1;
	quint16 flags = 0;
};

class AUDATA_DLL Graph
{
public:
	// Openings retain their original indices and flags for graph topology;
	// building routes do not consult spacecraft-style open/closed door state.
	bool Build(const Design& design, const Options& options, QString& error);
	bool IsEmpty() const { return m_Pathable.isEmpty(); }
	const QMap<quint32, ExtantCell>& ExtantCells() const { return m_Extant; }
	const QMap<quint32, PathableCell>& PathableCells() const { return m_Pathable; }
	const QMap<quint32, PortalCell>& PortalCells() const { return m_Portals; }
	// Exact cells, portal edges/costs and room-map comparison for a manual C++
	// fixture harness. Reports the first mismatch instead of accepting a count.
	bool MatchesFixtureCells(const Fixture& fixture, QString& error) const;
	// Same-deck continuous TestBuildingPath used by the original room shortcut.
	// A clear segment through a stair cell still requires normalized waypoints.
	bool CanMoveDirectly(const AuPoint& from, const AuPoint& to, bool& usesStairs) const;
	// Original compressed portal search selects the next room leg; reverse
	// accessible-cell search and NextStep retain ground-door policy, costs,
	// stair-height rules and opening references. At most ten turns precede a
	// replan, independently of a modern spacecraft route follower.
	bool Route(const AuPoint& from, const AuPoint& to, QVector<RoutePoint>& output,
		bool useGroundLevelDoors = true) const;
private:
	QMap<quint32, ExtantCell> m_Extant;
	QMap<quint32, PathableCell> m_Pathable;
	QMap<quint32, PortalCell> m_Portals;
	QMap<quint16, QPolygonF> m_Boundaries;
	QMap<quint16, quint8> m_RoomTypes;
	Options m_Options;
	bool Terminal(const AuPoint& point, quint32& key,
		bool useGroundLevelDoors = true, bool* outside = nullptr) const;
	bool AcrossCells(const AuPoint& from, const AuPoint& to, QVector<RoutePoint>& output,
		bool useGroundLevelDoors) const;
};
}
}

#endif
