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
// Older blueprints omitted hatch panels; retain their original palette order.
AUDATA_DLL bool TextureMap(const AuLegacyScDesign& legacy, QByteArray& output, QString& error,
	bool includeOpenHatches = false, bool includeDoors = false);
AUDATA_DLL bool ReadTextureMap(const QByteArray& bytes, QVector<TextureBinding>& output, QString& error);
// RAM image keys also identify the corresponding context-owned GPU texture.
// Only source/material, fixed tint and atlas variant are cached, not lighting.
AUDATA_DLL QString OriginalTextureKey(const TextureBinding& binding, bool illuminated = true);
AUDATA_DLL QImage OriginalTexture(const TextureBinding& binding, bool illuminated = true,
	bool* transparent = nullptr);
// Subsystem and FTL hull animation frames from the recovered 2016 effect atlas.
AUDATA_DLL QImage OriginalEffectTexture(int frame);

// Builds a derived runtime blueprint with recovered geometry, subsystem and
// console locations. Native old-style consoles own their chair/panel rendering.
// Legacy source bytes and dense detail identities remain in their own catalog.
// Generated parts have independent 16-bit IDs; doors and hatches start closed,
// and interior lighting starts off.
// Failure leaves output and report unchanged.
AUDATA_DLL bool Build(const AuLegacyScDesign& legacy, quint32 runtimeDesignId,
	const AuId& architectId, const QString& architectName,
	AuScDesign& output, AuScReport& report, QString& error);
}

#endif
