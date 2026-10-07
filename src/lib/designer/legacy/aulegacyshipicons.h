#ifndef AULEGACYSHIPICONS_H
#define AULEGACYSHIPICONS_H

#include <QIcon>
#include <QImage>
#include <QString>

// Accepts an original base name (dp_Bridge), filename, or :data/ resource path.
// Self-contained original resources also work in the standalone QApplication.
QIcon AuLegacyShipIcon(const QString& originalBaseName);

QImage AuLegacyShipDesignAtlas();

#endif
