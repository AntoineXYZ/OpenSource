#include "aulegacyshipicons.h"

QImage AuLegacyShipDesignAtlas()
{
	return QImage(QStringLiteral(":/data/legacy_txSpacecraftDesign.png"));
}

QIcon AuLegacyShipIcon(const QString& originalBaseName)
{
	QString name = originalBaseName.section(QLatin1Char('/'), -1);
	if(name.endsWith(QStringLiteral(".png"))) name.chop(4);
	return QIcon(QStringLiteral(":/data/legacy_%1.png").arg(name));
}
