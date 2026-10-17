#ifndef AuLegacyCityProtocol_H
#define AuLegacyCityProtocol_H

#include "audata.h"
#include "aulegacycitystate.h"
#include <QByteArray>
#include <QString>
#include <QVector>

// Legacy gameplay has its own interface and commands. Architect fixture actions
// retain their original numbers and their separate server authorization.
namespace AuLegacyCityProtocol
{
constexpr int MaximumRequestBytes = 1024;
constexpr int MaximumReplyBytes = 512 * 1024;
constexpr int MaximumEntries = 256;
// Keep the retired action's number reserved so stale clients cannot reinterpret
// a door request as removal or another mutation.
// The version-1 door boolean/count byte positions also remain reserved in the codec.
enum Action : quint8
{
	List = 0, Spawn = 1, Update = 2, RetiredDoors = 3, Remove = 4, Route = 5,
	ConstructionList = 6, Build = 7, Extend = 8, Halt = 9, Resume = 10,
	Contribute = 11, Demolish = 12, ClearLand = 13, FetchMaterials = 14, EmployCitizen = 15, ChangeRoad = 16
};
inline bool IsGameplay(Action action) { return action >= ConstructionList && action <= ChangeRoad; }
struct Request
{
	Action action = List;
	quint32 token = 0, cityId = 0, developmentId = 0, designId = 0;
	quint16 material = 76;
	qint8 minimumDeck = 0, maximumDeck = 0;
	bool constructed = true;
	float distanceMeters = 40, headingDegrees = 0, elevationMeters = 0;
	float targetX = 0, targetY = 0, targetZ = 4; // Building-local feet.
	quint8 industryId = 0, processId = 0, levels = 1;
	float roadLengthMeters = 10, roadWidthMeters = 10;
	bool roadSlab = false;
	float biodomeRadiusMeters = 100;
	bool citizenEnabled = true;
	QString cityName;
	bool explicitLocation = false;
	quint32 worldId = 0;
	float latitudeRadians = 0, longitudeRadians = 0;
};
struct Entry
{
	quint32 cityId = 0, developmentId = 0, designId = 0;
	quint16 material = 76;
	qint8 minimumDeck = 0, maximumDeck = 0;
	bool constructed = true;
	float elevationMeters = 0, headingDegrees = 0;
	QString name;
	quint8 industryId = 0, processId = 0, completedLevels = 0, targetLevels = 0;
	bool halted = false, citizenEnabled = false;
	quint32 siteLaborRemaining = 0, levelLaborRemaining = 0;
	QString materials;
	QVector<AuLegacyCity::ConstructionComponent> components;
};
struct Reply
{
	Action action = List;
	quint32 token = 0, selectedCity = 0, selectedDevelopment = 0;
	QString message;
	QVector<Entry> entries;
	bool hasList = false;
	bool truncated = false;
	// A valid labor command can be accepted while waiting for materials.
	bool accepted = false;
};
AUDATA_DLL QByteArray EncodeRequest(const Request& request);
AUDATA_DLL bool DecodeRequest(const QByteArray& bytes, Request& request);
AUDATA_DLL QByteArray EncodeReply(const Reply& reply);
AUDATA_DLL bool DecodeReply(const QByteArray& bytes, Reply& reply);
}
#endif
