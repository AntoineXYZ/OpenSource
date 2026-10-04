#ifndef AuLegacyShipHud_H
#define AuLegacyShipHud_H

class QPainter;
class QSize;
class QString;

namespace AuLegacyShip
{
namespace Rendering
{
// Original target HUD: undamaged authoring preview, system icon, and ship name.
// Visibility and acquisition/distance gating belong to the caller. SYS_Hull=4.
void PaintShipTarget(QPainter& painter, const QSize& size, const QString& name, int systemIcon = 4);
// The native acquisition level is 0..15; its inverse controls the arm spacing.
void PaintCrosshair(QPainter& painter, const QSize& size, int targetLevel, float opacity = 1);
}
}

#endif
