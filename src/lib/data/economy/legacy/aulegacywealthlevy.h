#ifndef AuLegacyWealthLevy_H
#define AuLegacyWealthLevy_H

#include "audata.h"
#include "auid.h"
#include <QByteArray>
#include <QString>
class QDataStream;

namespace AuLegacyCity
{
// AuConfiscate 0x14016c8a0: four exclusion bits and an integer percentage.
struct WealthLevyOrder
{
	quint32 empire = 0;
	AuId issuer;
	QString issuerName, statement;
	quint8 percent = 1, exemptions = 0;
};
constexpr int MaximumWealthLevyBytes = 2048;
AUDATA_DLL QByteArray EncodeWealthLevy(const WealthLevyOrder& order);
AUDATA_DLL bool DecodeWealthLevy(const QByteArray& bytes, WealthLevyOrder& order);
AUDATA_DLL bool ReadWealthLevyEnvelope(QDataStream& in, QByteArray& bytes);
}
#endif
