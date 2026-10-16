#ifndef AuLegacyCityProtocol_H
#define AuLegacyCityProtocol_H

#include "audata.h"
#include <QByteArray>
#include <QString>
#include <QVector>

// Architect test tools for the recovered infrastructure, separate from gameplay construction.
namespace AuLegacyCityProtocol
{
constexpr int MaximumRequestBytes = 96;
constexpr int MaximumReplyBytes = 128 * 1024;
constexpr int MaximumEntries = 256;
// Keep the retired action's number reserved so stale clients cannot reinterpret
// a door request as removal or another mutation.
// The version-1 door boolean/count byte positions also remain reserved in the codec.
enum Action : quint8 { List = 0, Spawn = 1, Update = 2, RetiredDoors = 3, Remove = 4, Route = 5 };
struct Request
{
	Action action = List;
	quint32 token = 0, cityId = 0, developmentId = 0, designId = 0;
	quint16 material = 76;
	qint8 minimumDeck = 0, maximumDeck = 0;
	bool constructed = true;
	float distanceMeters = 40, headingDegrees = 0, elevationMeters = 0;
	float targetX = 0, targetY = 0, targetZ = 4; // Building-local feet.
};
struct Entry
{
	quint32 cityId = 0, developmentId = 0, designId = 0;
	quint16 material = 76;
	qint8 minimumDeck = 0, maximumDeck = 0;
	bool constructed = true;
	float elevationMeters = 0, headingDegrees = 0;
	QString name;
};
struct Reply
{
	Action action = List;
	quint32 token = 0, selectedCity = 0, selectedDevelopment = 0;
	QString message;
	QVector<Entry> entries;
	bool hasList = false;
	bool truncated = false;
};
AUDATA_DLL QByteArray EncodeRequest(const Request& request);
AUDATA_DLL bool DecodeRequest(const QByteArray& bytes, Request& request);
AUDATA_DLL QByteArray EncodeReply(const Reply& reply);
AUDATA_DLL bool DecodeReply(const QByteArray& bytes, Reply& reply);
}
#endif
