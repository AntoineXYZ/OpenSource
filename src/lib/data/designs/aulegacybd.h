#ifndef AuLegacyBd_H
#define AuLegacyBd_H

#include "audata.h"
#include "aulegacyship.h"
#include <QDateTime>
#include <QMap>
#include <QSharedPointer>

// Original 2016 building designs have their own header and ID namespace. Room
// and detail records are shared with the recovered authoring format, not with
// modern AuBdDesign. All author coordinates are feet, z up.
namespace AuLegacyBd
{
constexpr int MaximumCacheBytes = 8 * 1024 * 1024;
constexpr int MaximumPayloadBytes = 16 * 1024 * 1024;
constexpr int MaximumRecords = 250000;
constexpr double DeckHeight = 8.0;

struct Header
{
	quint32 designId = 0;
	QString name;
	quint32 architectId = 0;
	QString architectName;
	quint8 industryId = 0;
	quint16 moduleRaw = 0;
	quint16 module = 0;
	quint8 state = 0;
	QVector<quint32> publishedEmpires;
};

struct Design
{
	Header header;
	QVector<AuLegacyShip::Room> rooms;
	QVector<AuLegacyShip::Detail> details;
	QByteArray sourceHash;
	QByteArray payloadHash;
	QDateTime cacheTime;
	int bytesConsumed = 0;

	bool IsPublicDefault() const { return header.state == 2 && !header.architectId; }
	int MinimumDeck() const;
	int MaximumDeck() const;
	// The adapter shares original room/detail geometry only. A building header
	// must never be interpreted as spacecraft statistics or manufacturing data.
	AuLegacyShip::Design AuthoringDesign() const;
};

// Atomic reads, with length checks before every allocation. ReadCache expects
// 1b11, the original Qt_4_0 QDateTime and a qCompress QByteArray. ReadPayload
// expects DnBuilding::Tx followed by DnRoomArray and DnDetailArray streams.
AUDATA_DLL bool ReadCache(const QByteArray& source, Design& output, QString& error);
AUDATA_DLL bool ReadPayload(const QByteArray& payload, Design& output, QString& error);
AUDATA_DLL bool LoadCache(const QString& fileName, Design& output, QString& error);

// Bounded qCompress decoder shared by original geometry/path fixture readers.
// Requires the advertised output size and a single fully consumed zlib stream.
AUDATA_DLL bool Uncompress(const QByteArray& compressed, QByteArray& output, QString& error);

class AUDATA_DLL Catalog
{
public:
	static bool LoadBuiltIn(QString& error);
	static const Catalog& BuiltIn();
	// Hex SHA256 over the versioned source domain, ascending original IDs and
	// their binary source SHA256 values. Independent of JSON formatting.
	static QByteArray SourceSha256();
	// root is :/legacy/cities or a deployed directory containing manifest.json
	// and designs/B1..B74. Private historical fixture B130703 is never loaded.
	bool LoadDefaults(const QString& root, QString& error);
	QSharedPointer<const Design> Find(quint32 designId) const;
	QVector<quint32> DesignsForIndustry(quint8 industryId) const;
	// Explicit stable tie rule for public variants: smallest original ID.
	// Existing developments retain their persisted design ID.
	quint32 DefaultForIndustry(quint8 industryId) const;
	int Count() const { return m_Designs.size(); }
private:
	QMap<quint32, QSharedPointer<const Design>> m_Designs;
};
}

#endif
