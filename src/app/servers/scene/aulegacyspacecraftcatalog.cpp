#include "aulegacyspacecraftcatalog.h"

#include "auglobal.h"
#include "aulegacyscdesign.h"
#include "aulegacyscprotocol.h"
#include "dgdata.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QHash>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>
#include <limits>

namespace
{
bool ReadCatalogId(const QVariant& value, quint32& id)
{
	bool ok = false;
	const qlonglong storedId = value.toLongLong(&ok);
	if(!ok || storedId <= 0 || quint64(storedId) > std::numeric_limits<quint32>::max())
		return false;
	id = quint32(storedId);
	return true;
}
}

bool StoreLegacySpacecraftDesign(QSqlDatabase& db, const QString& sourceName, const AuLegacyScDesign& design, quint32& id)
{
	id = 0;
	QSqlQuery query(db);
	if(!query.prepare("INSERT INTO n_LegacyScDesign (contentHash, sourceName, name, sourceData) "
			"VALUES (?, ?, ?, ?) ON CONFLICT (contentHash) DO UPDATE SET name = EXCLUDED.name RETURNING id"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot prepare design import:" << query.lastError().text();
		return false;
	}
	query.addBindValue(QString::fromLatin1(design.ContentHash()));
	query.addBindValue(sourceName);
	query.addBindValue(design.Name().isNull() ? QStringLiteral("") : design.Name());
	query.addBindValue(design.SourceData());
	if(!query.exec())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot store" << sourceName << ':' << query.lastError().text();
		return false;
	}
	if(query.next())
	{
		if(!ReadCatalogId(query.value(0), id))
		{
			qWarning() << "Legacy spacecraft catalog:" << "import returned an invalid server ID:" << query.value(0);
			return false;
		}
		return true;
	}
	if(query.lastError().isValid())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot read import result:" << query.lastError().text();
		return false;
	}
	qWarning() << "Legacy spacecraft catalog:" << "storage returned no design ID";
	return false;
}


bool LoadLegacySpacecraftCatalog(QSqlDatabase& db)
{
	QSqlQuery schema(db);
	if(!schema.exec("SELECT id, contentHash, sourceData, sourceName, name FROM n_LegacyScDesign WHERE false"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "storage unavailable; apply"
			<< "deploy/server/postgres/schema/legacyspacecraft.sql to auUniverse as aumaster, then restart scene servers."
			<< schema.lastError().text();
		return false;
	}
	schema.finish();

	QSqlQuery query(db);
	query.setForwardOnly(true);
	if(!query.exec("SELECT id, contentHash, sourceData, sourceName, name FROM n_LegacyScDesign ORDER BY id"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot load stored designs:" << query.lastError().text();
		return false;
	}
	QHash<quint32, QSharedPointer<const AuLegacyScDesign>> catalog;
	int rejected = 0;
	while(query.next())
	{
		quint32 id = 0;
		const QString sourceName = query.value(3).toString();
		if(!ReadCatalogId(query.value(0), id))
		{
			qWarning() << "Legacy spacecraft catalog:" << "skipping invalid server ID" << query.value(0) << "from" << sourceName;
			++rejected;
			continue;
		}
		const QByteArray source = query.value(2).toByteArray();
		if(source.isEmpty() || source.size() > AuLegacyShip::MaximumFileBytes ||
			query.value(1).toString().toLatin1() != QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex())
		{
			qWarning() << "Legacy spacecraft catalog:" << "skipping design" << id << "with invalid bytes or hash from" << sourceName;
			++rejected;
			continue;
		}
		QSharedPointer<AuLegacyScDesign> design(new AuLegacyScDesign);
		QString error;
		if(!AuLegacyScDesign::Read(source, *design, error))
		{
			qWarning() << "Legacy spacecraft catalog:" << "skipping stored design" << id << "from" << sourceName << ':' << error;
			++rejected;
			continue;
		}
		design->SetId(id);
		design->SetName(query.value(4).toString());
		catalog.insert(id, design);
		qInfo() << "Legacy spacecraft catalog:" << "loaded design" << id << design->Name()
			<< "with" << design->SourceDesign().rooms.size() << "rooms and"
			<< design->SourceDesign().details.size() << "details; author center (feet):"
			<< design->AuthorCenter().x << design->AuthorCenter().y << design->AuthorCenter().z;
		for(const QString& diagnostic : design->Stats().diagnostics)
			qWarning() << "Legacy spacecraft catalog:" << "design" << id << "diagnostic:" << diagnostic;
	}
	if(query.lastError().isValid())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot finish reading stored designs:" << query.lastError().text();
		return false;
	}
	DgData& data = g_Au.LockScDesignsForWrite();
	data.LegacyScDesignsReplace(catalog);
	g_Au.UnlockScDesigns();
	qInfo() << "Legacy spacecraft catalog:" << "published" << catalog.size() << "designs; skipped" << rejected << "stored records";
	return true;
}

namespace
{
class CatalogConnection
{
public:
	CatalogConnection() : name(QStringLiteral("LegacySc-%1").arg(QUuid::createUuid().toString()))
	{
		db = QSqlDatabase::addDatabase("QPSQL7", name);
		db.setDatabaseName("auUniverse");
		db.setHostName(qEnvironmentVariable("AU_UNIVERSE_DB_HOST", "UniverseDb.Hazeron.net"));
		db.setConnectOptions("connect_timeout=5");
	}
	~CatalogConnection()
	{
		db.close();
		db = QSqlDatabase();
		QSqlDatabase::removeDatabase(name);
	}
	bool Open() { return db.open("audog", qEnvironmentVariable("AU_DB_PASSWORD", "Gold")); }
	QSqlDatabase db;
private:
	QString name;
};

bool HasControlCharacters(const QString& text)
{
	for(QChar character : text)
		if(character.isNull() || character.category() == QChar::Other_Control)
			return true;
	return false;
}

QString CatalogText(const QString& text)
{
	QString result = text.left(AuLegacyScProtocol::MaximumNameCharacters);
	for(int index = 0; index < result.size(); ++index)
	{
		const QChar character = result.at(index);
		if(character.isNull() || character.category() == QChar::Other_Control)
			result[index] = QChar(' ');
	}
	return result;
}
}

void ProcessLegacySpacecraftRequest(const AuLegacyScProtocol::Request& request, AuLegacyScProtocol::Reply& reply)
{
	using namespace AuLegacyScProtocol;
	reply.token = request.token;
	reply.action = request.action == UploadRequest ? UploadReply : CatalogReply;
	if(request.action == UploadRequest)
	{
		const QString name = request.name.trimmed();
		if(name.isEmpty() || name.size() > MaximumNameCharacters || HasControlCharacters(name))
		{
			reply.message = QStringLiteral("Enter a name of 1 to 128 characters, without control characters.");
			return;
		}
		if(request.sourceName.isEmpty() || request.sourceName.contains('/') || request.sourceName.contains('\\')
			|| HasControlCharacters(request.sourceName) || !request.sourceName.endsWith(".ship", Qt::CaseInsensitive))
		{
			reply.message = QStringLiteral("The upload must have a .ship filename without a directory path.");
			return;
		}
		QSharedPointer<AuLegacyScDesign> design(new AuLegacyScDesign);
		if(!AuLegacyScDesign::ReadUpload(request.source, *design, reply.message))
			return;
		design->SetName(name);
		CatalogConnection connection;
		if(!connection.Open() || !StoreLegacySpacecraftDesign(connection.db, request.sourceName, *design, reply.designId))
		{
			reply.designId = 0;
			reply.message = QStringLiteral("Unable to store the design. Check the database schema and server logs.");
			return;
		}
		design->SetId(reply.designId);
		DgData& data = g_Au.LockScDesignsForWrite();
		data.LegacyScDesignAdd(design);
		g_Au.UnlockScDesigns();
		reply.name = name;
		reply.message = QStringLiteral("Stored %1 as design %2.").arg(name).arg(reply.designId);
		qInfo() << "Legacy spacecraft catalog:" << "uploaded design" << reply.designId << name;
		return;
	}

	CatalogConnection connection;
	if(!connection.Open())
	{
		reply.message = QStringLiteral("The legacy design database is unavailable.");
		return;
	}
	QSqlQuery query(connection.db);
	if(!query.prepare("SELECT id, left(name,128), left(sourceName,128) FROM n_LegacyScDesign "
			"WHERE id > ? ORDER BY id LIMIT 65"))
	{
		reply.message = QStringLiteral("Unable to read the legacy design catalog.");
		return;
	}
	query.addBindValue(qulonglong(request.afterId));
	if(!query.exec())
	{
		reply.message = QStringLiteral("Unable to read legacy designs. Check the database schema.");
		return;
	}
	while(query.next())
	{
		if(reply.entries.size() == CatalogPageSize)
		{
			reply.nextAfterId = reply.entries.last().id;
			break;
		}
		Entry entry;
		if(!ReadCatalogId(query.value(0), entry.id))
			continue;
		entry.name = CatalogText(query.value(1).toString());
		entry.sourceName = CatalogText(query.value(2).toString());
		reply.entries.append(entry);
	}
	if(query.lastError().isValid())
	{
		reply.entries.clear();
		reply.nextAfterId = 0;
		reply.message = QStringLiteral("Unable to finish reading the legacy design catalog.");
	}
}
