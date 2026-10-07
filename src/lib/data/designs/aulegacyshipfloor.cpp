#include "aulegacyshipgeometry.h"
#include "aulegacyship.h"

#include <QPoint>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <utility>

namespace AuLegacyShip
{
namespace Geometry
{
bool RasterFloor(const QPolygonF& boundary, QByteArray& output, QString* error)
{
	const auto fail = [error](const char* text) {
		if(error) *error = QString::fromLatin1(text);
		return false;
	};
	if(boundary.size() < 3)
		return fail("A path room needs at least three boundary vertices.");
	QVector<QPoint> vertices;
	vertices.reserve(boundary.size());
	for(const QPointF& point : boundary)
	{
		const double x = std::round(point.x() / CoordinateSpacing) + 128;
		const double y = std::round(point.y() / CoordinateSpacing) + 128;
		if(!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 255 || y < 0 || y > 255)
			return fail("A room exceeds the legacy path grid bounds.");
		vertices.append(QPoint(int(x), int(y)));
	}
	QPoint previous = vertices.last();
	int xmax = previous.x();
	QVector<QPoint> perimeter;
	QByteArray raster(256 * 256, char(0));
	for(const QPoint& vertex : vertices)
	{
		const int deltaX = vertex.x() - previous.x(), deltaY = vertex.y() - previous.y();
		if(deltaX && deltaY && std::abs(deltaX) != std::abs(deltaY))
			return fail("A room edge is outside the legacy path grid.");
		xmax = std::max(xmax, vertex.x());
		const QPoint step((deltaX > 0) - (deltaX < 0), (deltaY > 0) - (deltaY < 0));
		while(previous != vertex)
		{
			raster[previous.x() * 256 + previous.y()] = 1;
			perimeter.append(previous);
			previous += step;
		}
	}
	if(perimeter.size() < 3)
		return fail("A room has no usable path perimeter.");
	// Preserve the ordered perimeter crossing predicate. Polygon containment
	// tests differ at diagonal vertices and would change narrow crew routes.
	QPoint before = perimeter[perimeter.size() - 2];
	previous = perimeter.last();
	for(const QPoint& current : perimeter)
	{
		const QPoint incoming = previous - before, outgoing = current - previous;
		bool fill = false;
		if(outgoing.x() == 1)
			fill = outgoing.y() == 1 && !(incoming.x() == -1 && incoming.y() >= -1 && incoming.y() <= 0);
		else if(outgoing.x() == 0)
		{
			if(outgoing.y() == 1)
				fill = incoming.x() == -1 ? !(incoming.y() >= -1 && incoming.y() <= 0)
					: incoming.x() != 0 || incoming.y() != -1;
			else
				fill = incoming.x() == -1 && incoming.y() == 1;
		}
		else if(outgoing.y() == 1)
			fill = incoming.y() == 1 || (incoming.x() == 1 && incoming.y() == 0);
		else if(outgoing.y() == 0 || incoming.x() != 1)
			fill = incoming.y() == 1;
		if(fill)
			for(int x = previous.x() + 1; x < xmax; ++x)
			{
				const int index = x * 256 + previous.y();
				if(raster.at(index)) break;
				raster[index] = 2;
			}
		before = previous;
		previous = current;
	}
	output = std::move(raster);
	if(error) error->clear();
	return true;
}
}
}
