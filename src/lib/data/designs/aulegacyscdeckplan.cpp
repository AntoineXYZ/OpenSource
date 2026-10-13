#include "aulegacyscdeckplan.h"
#include "aulegacyshippaths.h"

#include <QIODevice>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <cmath>
#include <utility>

namespace
{
constexpr quint32 MaximumRooms = 512;
constexpr quint32 MaximumRoomVertices = 1024;
constexpr quint32 MaximumDetails = 32768;

bool HasBytes(QDataStream& in, quint64 count)
{
	return in.status() == QDataStream::Ok && in.device()
		&& count <= quint64(in.device()->bytesAvailable());
}
}

struct AuLegacyScDeckPlan::PathCache
{
	QMutex mutex;
	bool attempted = false;
	QSharedPointer<const AuLegacyShip::Paths::Graph> graph;
};

AuLegacyScDeckPlan::AuLegacyScDeckPlan() : m_PathCache(new PathCache)
{
}

AuLegacyScDeckPlan::AuLegacyScDeckPlan(const AuLegacyShip::Design& design, const AuPoint& authorCenter)
	: AuLegacyScDeckPlan()
{
	if(design.rooms.size() > int(MaximumRooms) || design.details.size() > int(MaximumDetails)) return;
	for(const AuLegacyShip::Room& room : design.rooms)
		if(room.hasWorkingBoundary || room.packedVertices.size() > int(MaximumRoomVertices)) return;
	m_SourceDesign.rooms = design.rooms;
	m_SourceDesign.details = design.details;
	for(AuLegacyShip::Room& room : m_SourceDesign.rooms)
	{
		room.streamOffset = 0;
		room.designId = 0;
	}
	for(AuLegacyShip::Detail& detail : m_SourceDesign.details)
	{
		detail.streamOffset = 0;
		detail.designId = 0;
	}
	m_AuthorCenter = authorCenter;
	m_Valid = Validate();
	if(!m_Valid) m_SourceDesign = AuLegacyShip::Design();
}

QSharedPointer<const AuLegacyShip::Paths::Graph> AuLegacyScDeckPlan::PathGraph() const
{
	if(!m_Valid || !m_PathCache) return {};
	QMutexLocker guard(&m_PathCache->mutex);
	if(!m_PathCache->attempted)
	{
		m_PathCache->attempted = true;
		QSharedPointer<AuLegacyShip::Paths::Graph> graph(new AuLegacyShip::Paths::Graph);
		// Runtime door metadata uses one-based source-record identities, as
		// AuLegacyScGeometry::Build does. File detail IDs may have gaps or
		// duplicates and cannot identify the generated door parts. Keep the
		// replicated deck view's original records intact; normalize this copy
		// for both newly generated and reloaded runtime blueprints.
		AuLegacyShip::Design runtimeSource = m_SourceDesign;
		for(int index = 0; index < runtimeSource.details.size(); ++index)
			runtimeSource.details[index].detailId = quint32(index) + 1;
		QString error;
		if(graph->Build(runtimeSource, error)) m_PathCache->graph = graph;
		else qWarning() << "Legacy spacecraft path graph:" << error;
	}
	return m_PathCache->graph;
}

bool AuLegacyScDeckPlan::Validate() const
{
	if(m_SourceDesign.rooms.isEmpty() || m_SourceDesign.rooms.size() > int(MaximumRooms)
		|| m_SourceDesign.details.size() > int(MaximumDetails)
		|| !std::isfinite(m_AuthorCenter.x) || !std::isfinite(m_AuthorCenter.y) || !std::isfinite(m_AuthorCenter.z)
		|| std::abs(m_AuthorCenter.x) > 1024 || std::abs(m_AuthorCenter.y) > 1024
		|| std::abs(m_AuthorCenter.z) > AuLegacyShip::MaximumDeck * AuLegacyShip::DeckHeight) return false;
	for(const AuLegacyShip::Room& room : m_SourceDesign.rooms)
		if(room.hasWorkingBoundary || room.packedVertices.size() > int(MaximumRoomVertices)
			|| room.deck < AuLegacyShip::MinimumDeck || room.deck > AuLegacyShip::MaximumDeck) return false;
	return true;
}

void AuLegacyScDeckPlan::operator>>(QDataStream& out) const
{
	if(!m_Valid)
	{
		out.setStatus(QDataStream::WriteFailed);
		return;
	}
	out << quint8(1) << m_AuthorCenter.x << m_AuthorCenter.y << m_AuthorCenter.z;
	out << quint32(m_SourceDesign.rooms.size());
	for(const AuLegacyShip::Room& room : m_SourceDesign.rooms)
	{
		out << room.roomId << room.deck << room.roomType << room.roomSubtype << quint16(room.packedVertices.size());
		for(quint16 point : room.packedVertices) out << point;
	}
	out << quint32(m_SourceDesign.details.size());
	for(const AuLegacyShip::Detail& detail : m_SourceDesign.details)
		out << detail.deck << detail.detailType << detail.hiddenRaw << detail.detailId
			<< detail.packedXY << detail.roomId << detail.commodityIdRaw << detail.commodityIdNormalized;
}

void AuLegacyScDeckPlan::operator<<(QDataStream& in)
{
	AuLegacyScDeckPlan accepted;
	quint8 version = 0;
	in >> version >> accepted.m_AuthorCenter.x >> accepted.m_AuthorCenter.y >> accepted.m_AuthorCenter.z;
	quint32 roomCount = 0;
	in >> roomCount;
	if(version != 1 || !roomCount || roomCount > MaximumRooms || !HasBytes(in, quint64(roomCount) * 7 + 4))
	{
		in.setStatus(QDataStream::ReadCorruptData);
		return;
	}
	accepted.m_SourceDesign.rooms.reserve(int(roomCount));
	for(quint32 index = 0; index < roomCount; ++index)
	{
		AuLegacyShip::Room room;
		quint16 vertexCount = 0;
		in >> room.roomId >> room.deck >> room.roomType >> room.roomSubtype >> vertexCount;
		if(vertexCount > MaximumRoomVertices || !HasBytes(in, quint64(vertexCount) * 2))
		{
			in.setStatus(QDataStream::ReadCorruptData);
			return;
		}
		room.packedVertices.reserve(vertexCount);
		for(quint16 vertex = 0; vertex < vertexCount; ++vertex)
		{
			quint16 point = 0;
			in >> point;
			room.packedVertices.append(point);
		}
		accepted.m_SourceDesign.rooms.append(room);
	}
	quint32 detailCount = 0;
	in >> detailCount;
	if(detailCount > MaximumDetails || !HasBytes(in, quint64(detailCount) * 15))
	{
		in.setStatus(QDataStream::ReadCorruptData);
		return;
	}
	accepted.m_SourceDesign.details.reserve(int(detailCount));
	for(quint32 index = 0; index < detailCount; ++index)
	{
		AuLegacyShip::Detail detail;
		in >> detail.deck >> detail.detailType >> detail.hiddenRaw >> detail.detailId
			>> detail.packedXY >> detail.roomId >> detail.commodityIdRaw >> detail.commodityIdNormalized;
		accepted.m_SourceDesign.details.append(detail);
	}
	if(in.status() != QDataStream::Ok || !accepted.Validate())
	{
		in.setStatus(QDataStream::ReadCorruptData);
		return;
	}
	accepted.m_Valid = true;
	*this = std::move(accepted);
}

quint64 AuLegacyScDeckPlan::MemoryUsage() const
{
	quint64 bytes = sizeof(*this) + quint64(m_SourceDesign.rooms.size()) * sizeof(AuLegacyShip::Room)
		+ quint64(m_SourceDesign.details.size()) * sizeof(AuLegacyShip::Detail);
	for(const AuLegacyShip::Room& room : m_SourceDesign.rooms)
		bytes += quint64(room.packedVertices.size()) * sizeof(quint16);
	return bytes;
}
