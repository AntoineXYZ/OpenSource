#ifndef AuLegacyCityState_H
#define AuLegacyCityState_H

#include "audata.h"
#include "auglobell.h"
#include "aupoint.h"
#include <QByteArray>
#include <QVector>

namespace AuLegacyCity
{
constexpr quint8 StateVersion = 1;
constexpr int MaximumStateBytes = 8 * 1024 * 1024;
constexpr int MaximumDevelopments = 32768;

// These are owned records, not modern building atoms. IDs are local to a city;
// original blueprint and commodity IDs remain in their historical namespaces.
struct Development
{
	quint32 id = 0;
	quint32 designId = 0;
	quint8 industryId = 0;
	quint16 materialId = 51;
	AuGlobeLLF location;
	double altitudeMSL = 0;
	double facingRadians = 0;
	qint8 minimumDeck = 0;
	qint8 maximumDeck = 0;
	bool constructed = true;
	quint8 completedLevels = 1;
};

struct Flag
{
	quint32 empire = 0;
	AuGlobeLLF location;
	quint8 level = 30;
	qint64 capturedAt = 0;
};

struct State
{
	QString name;
	quint32 controllingEmpire = 0;
	quint32 allegianceEmpire = 0;
	quint32 loyalPopulation = 0;
	qint32 population = 0;
	qint8 satisfaction = 1;
	float capacitor = 0;
	qint64 lastVisit = 0;
	QVector<Development> developments;
	QVector<Flag> flags;
	// Versioned opaque groups preserve ownership of future city-wide systems
	// without substituting modern DaBd economics during infrastructure recovery.
	QByteArray inventory;
	QByteArray manufacturing;
	QByteArray accounting;
	QByteArray reportHistory;
};

// New runtime envelope, independent of the 2016 DB layout. Atomic bounded read;
// malformed/trailing data never publishes a partial city.
AUDATA_DLL bool EncodeState(const State& state, QByteArray& output, QString& error);
AUDATA_DLL bool DecodeState(const QByteArray& bytes, State& output, QString& error);
}
#endif
