#ifndef AuLegacyShipGeometry_H
#define AuLegacyShipGeometry_H

#include <QPointF>
#include <QPolygonF>
#include <QString>

namespace AuLegacyShip
{
namespace Geometry
{
// Exact 2016 grid selection, including the negative-coordinate seam and ties.
// An unbounded mask or a nonfinite/out-of-range cursor returns no polygon.
QPolygonF FillShape(quint8 mask, const QPointF& cursor);

// The original raster edge walker supports horizontal, vertical and 45-degree
// edges. Reject unsupported input before its sign-step loops can fail to end.
bool IsSupportedBoundary(const QPolygonF& boundary, QString* error = nullptr);

// Original DnRoom operations. Failed/unchanged operations leave boundary alone.
// Subtract returns true when a room is fully erased (the new boundary is empty).
// CanSubtract returns true for a no-op. A nonempty error distinguishes rejected
// geometry from ordinary unchanged occupancy. All coordinates are world feet.
bool Merge(QPolygonF& boundary, const QPolygonF& cell, QString* error = nullptr);
bool CanSubtract(const QPolygonF& boundary, const QPolygonF& cell,
	QString* error = nullptr);
bool Subtract(QPolygonF& boundary, const QPolygonF& cell, QString* error = nullptr);
}
}

#endif // AuLegacyShipGeometry_H
