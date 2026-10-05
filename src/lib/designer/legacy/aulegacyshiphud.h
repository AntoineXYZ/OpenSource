#ifndef AuLegacyShipHud_H
#define AuLegacyShipHud_H

class QPainter;
class QSize;
class QString;

namespace AuLegacyShip
{
struct TargetHealth;
namespace Rendering
{
// Original subsystem health fraction, system icon and ship name (no numeric HP).
// Visibility and acquisition/distance gating belong to the caller.
void PaintShipTarget(QPainter& painter, const QSize& size, const QString& name, const TargetHealth& health);
// The native acquisition level is 0..15; its inverse controls the arm spacing.
void PaintCrosshair(QPainter& painter, const QSize& size, int targetLevel, float opacity = 1);
}
}

#endif
