#ifndef AULEGACYSHIPICONS_H
#define AULEGACYSHIPICONS_H

#include <QIcon>
#include <QImage>
#include <QString>

// Accepts an original base name (dp_Bridge), filename, or :data/ resource path.
// Original 2016 PNGs are registered by AuShipTex0.qrc through AuTexture.
QIcon AuLegacyShipIcon(const QString& originalBaseName);

QImage AuLegacyShipDesignAtlas();

#endif
