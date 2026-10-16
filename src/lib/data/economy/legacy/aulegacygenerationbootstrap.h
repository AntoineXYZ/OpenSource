#ifndef AuLegacyGenerationBootstrap_H
#define AuLegacyGenerationBootstrap_H

#include "aulegacybd.h"
#include "aulegacycitycatalogue.h"
#include "auglobal.h"
#include <QCryptographicHash>

inline bool AuPrepareLegacyGenerationIdentity(AuGameGenerationIdentity& identity, QString& error)
{
	if(!AuLegacyCityCatalogue::LoadBuiltIn(error) || !AuLegacyBd::Catalog::LoadBuiltIn(error)) return false;
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || !catalogue->IsValid())
	{
		error = QStringLiteral("Legacy city catalogue validation did not produce a valid catalogue.");
		return false;
	}
	if(!g_Au.InstallLegacyCommodityDescriptors(catalogue->RuntimeCommodityDescriptors(), error)) return false;
	identity.generation = AuGameGeneration::Legacy;
	identity.catalogueVersion = AuLegacyCityCatalogue::DataVersion() + QStringLiteral("-buildings-1");
	QCryptographicHash hash(QCryptographicHash::Sha256);
	hash.addData("Legacy game generation manifest 1\n");
	hash.addData(catalogue->SourceSha256());
	hash.addData("\n");
	hash.addData(AuLegacyBd::Catalog::SourceSha256());
	identity.manifestHash = hash.result();
	return true;
}

#endif
