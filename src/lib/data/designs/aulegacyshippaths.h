#ifndef AuLegacyShipPaths_H
#define AuLegacyShipPaths_H

#include "aulegacyship.h"
#include <QBitArray>
#include <QHash>
#include <QSet>
#include <functional>

namespace AuLegacyShip
{
namespace Paths
{
// AuOpening after the original room/detail opening extraction. Endpoints and
// center are in feet; cellType and flags retain AuPath's original numbers.
struct Opening
{
	QPointF first;
	QPointF second;
	QPointF center;
	int deck = 0;
	int cellType = 3;
	quint16 flags = 0;
	quint16 room1 = 0;
	quint16 room2 = 0;
	bool large = false;
	bool firstEmbedded = false;
	bool secondEmbedded = false;
};

bool ExtractOpenings(const Design& design, QVector<Opening>& openings, QString& error);
bool AppendVerticalOpenings(const Design& design, QVector<Opening>& openings, QString& error);

class Graph
{
public:
	// Progress returns false to cancel. A failed build leaves no usable graph.
	bool Build(const Design& design, QString& error,
		const std::function<bool(int, int)>& progress = {});
	bool CanReach(const QPointF& from, int fromDeck, const QPointF& to,
		int toDeck, bool crew = true) const;
	bool CanReachOutside(const QPointF& from, int fromDeck, bool crew = true) const;
	bool IsEmpty() const { return m_Cells.isEmpty(); }
	void SetCancelCheck(const std::function<bool()>& cancel) { m_CancelCheck = cancel; m_Canceled = false; }
	bool WasCanceled() const { return m_Canceled; }

private:
	struct Cell
	{
		quint32 directions = 0x200;
		quint16 flags = 0;
		quint16 room1 = 0;
		quint16 room2 = 0;
		quint8 type = 0;
		quint8 stairs = 0;
		int index = -1;
	};
	struct ReachCache
	{
		QBitArray cells;
		quint64 used = 0;
	};
	QHash<quint32, Cell> m_Cells;
	QHash<quint32, quint8> m_Extant;
	QHash<quint32, QVector<quint16>> m_ExtantRooms;
	QHash<quint16, QPolygonF> m_RoomBoundaries;
	QSet<quint32> m_Entrances;
	QSet<quint32> m_PortalFootprint;
	QSet<quint32> m_Portals;
	QVector<quint32> m_CellKeys;
	mutable QHash<quint64, ReachCache> m_ReachCache;
	mutable quint64 m_CacheClock = 0;
	std::function<bool()> m_CancelCheck;
	mutable bool m_Canceled = false;
	int m_MinimumDeck = 0;
	int m_MaximumDeck = 0;
	void ConnectOpening(quint32 key);
	void ConnectBelow(quint32 key);
	void RemoveCell(quint32 key);
	void PaintStairs(const Detail& detail, QByteArray* floor, int pass);
	void TrimVerticalConnections();
	QVector<quint32> TerminalCells(const QPointF& point, int deck, bool crew,
		bool& outside) const;
	bool Reach(quint32 from, const QSet<quint32>& targets, bool crew) const;
};
}
}

#endif
