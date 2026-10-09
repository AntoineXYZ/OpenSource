#ifndef AuLegacyScGeometry_H
#define AuLegacyScGeometry_H

#include "audata.h"
#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>
#include <QVector2D>
#include <QVector4D>

class AuId;
class AuLegacyScDesign;
class AuScDesign;
class AuScReport;

namespace AuLegacyScGeometry
{
constexpr int MaximumTextureMapBytes = 64 * 1024;

struct TextureBinding
{
	int paletteIndex = 0;
	QRect packedTile;
	int material = -1;
	QString resource;
	QVector4D tint = QVector4D(1, 1, 1, 1);
	QVector2D textureInset;
	bool cutout = false;
};

// Compact material/UV bindings only. Texture pixels come from client resources.
AUDATA_DLL bool TextureMap(const AuLegacyScDesign& legacy, QByteArray& output, QString& error);
AUDATA_DLL bool ReadTextureMap(const QByteArray& bytes, QVector<TextureBinding>& output, QString& error);
AUDATA_DLL QImage OriginalTexture(const TextureBinding& binding);

// Builds a derived modern blueprint for the initial stationary debug ships.
// Legacy source bytes and dense detail identities remain in their own catalog.
// Generated parts have independent 16-bit IDs; door panels are initially open.
// Failure leaves output and report unchanged.
AUDATA_DLL bool Build(const AuLegacyScDesign& legacy, quint32 runtimeDesignId,
	const AuId& architectId, const QString& architectName,
	AuScDesign& output, AuScReport& report, QString& error);
}

#endif
