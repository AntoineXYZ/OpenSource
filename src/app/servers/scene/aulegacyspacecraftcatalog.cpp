#include "aulegacyspacecraftcatalog.h"

#include "auglobal.h"
#include "aulegacyscdesign.h"
#include "dgdata.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QHash>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
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

bool FindCatalogId(QSqlDatabase& db, const QByteArray& hash, quint32& id)
{
	id = 0;
	QSqlQuery query(db);
	if(!query.prepare("SELECT id FROM n_LegacyScDesign WHERE contentHash = ?"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot prepare hash lookup:" << query.lastError().text();
		return false;
	}
	query.addBindValue(QString::fromLatin1(hash));
	if(!query.exec())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot look up design:" << query.lastError().text();
		return false;
	}
	if(query.next())
	{
		if(!ReadCatalogId(query.value(0), id))
		{
			qWarning() << "Legacy spacecraft catalog:" << "stored design has an invalid server ID:" << query.value(0);
			return false;
		}
	}
	else if(query.lastError().isValid())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot read hash lookup:" << query.lastError().text();
		return false;
	}
	return true;
}

}

bool StoreLegacySpacecraftDesign(QSqlDatabase& db, const QString& sourceName, const AuLegacyScDesign& design, quint32& id)
{
	QSqlQuery query(db);
	if(!query.prepare("INSERT INTO n_LegacyScDesign (contentHash, sourceName, name, sourceData) "
			"VALUES (?, ?, ?, ?) ON CONFLICT (contentHash) DO NOTHING RETURNING id"))
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
	query.finish();
	// Another scene server may have imported the same bytes after our lookup.
	return FindCatalogId(db, design.ContentHash(), id) && id != 0;
}


bool LoadLegacySpacecraftCatalog(QSqlDatabase& db)
{
	QSqlQuery schema(db);
	if(!schema.exec("SELECT id, contentHash, sourceData, sourceName FROM n_LegacyScDesign WHERE false"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "storage unavailable; apply"
			<< "deploy/server/postgres/schema/legacyspacecraft.sql to auUniverse as aumaster, then restart scene servers."
			<< schema.lastError().text();
		return false;
	}
	schema.finish();

	QSqlQuery query(db);
	query.setForwardOnly(true);
	if(!query.exec("SELECT id, contentHash, sourceData, sourceName FROM n_LegacyScDesign ORDER BY id"))
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
