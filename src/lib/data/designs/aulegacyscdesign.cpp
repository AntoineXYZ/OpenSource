#include "aulegacyscdesign.h"

#include <QCryptographicHash>
#include <utility>

bool AuLegacyScDesign::Read(const QByteArray& source, AuLegacyScDesign& output, QString& error)
{
	AuLegacyScDesign accepted;
	if(!AuLegacyShip::Read(source, accepted.m_SourceDesign, error))
		return false;

	accepted.m_SourceData = source;
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
