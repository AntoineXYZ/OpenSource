#include "aulegacyscdesign.h"
#include "aulegacyscprotocol.h"

#include <QCryptographicHash>
#include <QRectF>
#include <QVector>
#include <utility>

bool AuLegacyScDesign::Read(const QByteArray& source, AuLegacyScDesign& output, QString& error)
{
	return ReadSource(source, output, error, false);
}

bool AuLegacyScDesign::ReadUpload(const QByteArray& source, AuLegacyScDesign& output, QString& error)
{
	return ReadSource(source, output, error, true);
}

bool AuLegacyScDesign::ReadSource(const QByteArray& source, AuLegacyScDesign& output, QString& error, bool upload)
{
	if(upload && (source.isEmpty() || source.size() > AuLegacyScProtocol::MaximumUploadBytes))
	{
		error = QStringLiteral("Legacy spacecraft uploads must contain between 1 byte and 8 MiB.");
		return false;
	}
	AuLegacyScDesign accepted;
	if(!AuLegacyShip::Read(source, accepted.m_SourceDesign, error))
		return false;
	if(upload)
	{
		// Bound the worst-case boundary comparisons before CalculateStats performs
		// its contiguity analysis. File allocation limits alone cannot bound this.
		if(accepted.m_SourceDesign.rooms.size() > 512)
		{
			error = QStringLiteral("The design exceeds the upload limit of 512 rooms.");
			return false;
		}
		struct BoundaryWork
		{
			int deck;
			QRectF bounds;
			quint64 vertices;
		};
		QVector<BoundaryWork> boundaries;
		for(const AuLegacyShip::Room& room : accepted.m_SourceDesign.rooms)
		{
			if(room.packedVertices.size() > 1024 || room.deck < AuLegacyShip::MinimumDeck
				|| room.deck > AuLegacyShip::MaximumDeck)
			{
				error = QStringLiteral("A room exceeds the upload limits for vertices or deck number.");
				return false;
			}
			const QPolygonF boundary = AuLegacyShip::RoomBoundary(room);
			if(!boundary.isEmpty())
				boundaries.append(BoundaryWork{room.deck, boundary.boundingRect(), quint64(boundary.size())});
		}
		quint64 work = 0;
		// BoundariesTouch rejects disjoint bounds before testing points against
		// edges. Charge both directions of each possible room pair, including
		// both containment and boundary tests, without charging disjoint rooms.
		constexpr double fuzz = .00001;
		for(int first = 0; first < boundaries.size(); ++first)
		{
			const BoundaryWork& a = boundaries.at(first);
			for(int second = first + 1; second < boundaries.size(); ++second)
			{
				const BoundaryWork& b = boundaries.at(second);
				if(a.deck < b.deck - 1 || a.deck > b.deck + 1
					|| a.bounds.left() > b.bounds.right() + fuzz || b.bounds.left() > a.bounds.right() + fuzz
					|| a.bounds.top() > b.bounds.bottom() + fuzz || b.bounds.top() > a.bounds.bottom() + fuzz)
					continue;
				work += 4 * a.vertices * b.vertices;
				if(work > 4 * 1024 * 1024)
				{
					error = QStringLiteral("Room boundaries are too complex for online upload analysis.");
					return false;
				}
			}
		}
	}

	accepted.m_SourceData = source;
	accepted.m_Name = accepted.m_SourceDesign.header.name;
	accepted.m_ContentHash = QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex();
	accepted.m_Stats = AuLegacyShip::CalculateStats(accepted.m_SourceDesign);
	if(accepted.m_Stats.hasHull)
	{
		// AnalyzeDesign @ AuAtom13.dll 0x1803c63c0 centers the hull extents;
		// its symmetric one-deck vertical expansion does not change the midpoint.
		accepted.m_AuthorCenter = AuPoint(accepted.m_Stats.hullCenter.x(),
			accepted.m_Stats.hullCenter.y(),
			(accepted.m_Stats.minimumDeck + accepted.m_Stats.maximumDeck) * AuLegacyShip::DeckHeight / 2.0);
	}
	output = std::move(accepted);
	return true;
}

quint32 AuLegacyScDesign::DetailRecordId(int sourceIndex) const
{
	if(sourceIndex < 0 || sourceIndex >= m_SourceDesign.details.size())
		return 0;
	return quint32(sourceIndex) + 1;
}

const AuLegacyShip::Detail* AuLegacyScDesign::DetailRecord(quint32 recordId) const
{
	if(!recordId || recordId > quint32(m_SourceDesign.details.size()))
		return nullptr;
	return &m_SourceDesign.details.at(int(recordId - 1));
}
