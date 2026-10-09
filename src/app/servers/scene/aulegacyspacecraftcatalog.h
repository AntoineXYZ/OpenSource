#ifndef AuLegacySpacecraftCatalog_H
#define AuLegacySpacecraftCatalog_H

class QSqlDatabase;
class QString;
class AuLegacyScDesign;
namespace AuLegacyScProtocol { struct Request; struct Reply; }

#include <QtGlobal>

// Persists an accepted payload independently of its acquisition method.
// The caller must authorize the operation and accept the bytes server-side.
bool StoreLegacySpacecraftDesign(QSqlDatabase& db, const QString& sourceName,
	const AuLegacyScDesign& design, quint32& id);

// Loads stored designs and publishes a validated, immutable server cache.
// Called once by the database thread during scene-server startup.
bool LoadLegacySpacecraftCatalog(QSqlDatabase& db);

// Called on the authenticated chat thread, using its own SQL connection.
void ProcessLegacySpacecraftRequest(const AuLegacyScProtocol::Request& request,
	AuLegacyScProtocol::Reply& reply);

#endif // AuLegacySpacecraftCatalog_H
