// Manual standalone validation; never compiled automatically by the agent.
#include "aulegacyshipgeometry.h"
#include "aulegacyship.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <cmath>
#include <limits>

namespace
{
int failures = 0;

void Require(bool condition, const QString& message)
{
	if(!condition)
	{
		QTextStream(stderr) << "FAIL: " << message << '\n';
		++failures;
	}
}

QJsonObject ReadJson(const QString& path)
{
	QFile file(path);
	Require(file.open(QIODevice::ReadOnly), "Cannot open " + path);
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
	Require(error.error == QJsonParseError::NoError && document.isObject(), "Invalid JSON: " + path);
	return document.object();
}

QPolygonF Polygon(const QJsonArray& points, double spacing)
{
	QPolygonF result;
	for(const QJsonValue& point : points)
	{
		const QJsonArray pair = point.toArray();
		result.append(QPointF(pair.at(0).toDouble() * spacing, pair.at(1).toDouble() * spacing));
	}
	return result;
}

bool Same(const QPolygonF& a, const QPolygonF& b)
{
	if(a.size() != b.size()) return false;
	for(int i = 0; i < a.size(); ++i)
		if(a.at(i).x() != b.at(i).x() || a.at(i).y() != b.at(i).y()) return false;
	return true;
}
}

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	if(argc != 3)
	{
		QTextStream(stderr) << "Usage: native_ship_geometry room-raster-fixtures.json cell-shapes.json\n";
		return 2;
	}
	const QJsonObject fixtures = ReadJson(QString::fromLocal8Bit(argv[1]));
	const double spacing = fixtures.value("spacing").toDouble();
	const QJsonArray cases = fixtures.value("cases").toArray();
	Require(spacing == 1.41421356225 && !cases.isEmpty(), "Missing raster fixtures");
	for(const QJsonValue& entry : cases)
	{
		const QJsonObject test = entry.toObject();
		const QString name = test.value("name").toString();
		const QPolygonF old = Polygon(test.value("old").toArray(), spacing);
		const QPolygonF cell = Polygon(test.value("cell").toArray(), spacing);
		QPolygonF boundary = old;
		QString error;
		bool accepted;
		if(test.value("mode").toString() == "merge")
			accepted = AuLegacyShip::Geometry::Merge(boundary, cell, &error);
		else if(test.value("mode").toString() == "can_subtract")
			accepted = AuLegacyShip::Geometry::CanSubtract(boundary, cell, &error);
		else
			accepted = AuLegacyShip::Geometry::Subtract(boundary, cell, &error);
		Require(accepted == test.value("accepted").toBool(), name + " return value: " + error);
		Require(Same(boundary, Polygon(test.value("boundary").toArray(), spacing)), name + " exact vertex order/doubles");
		if(!accepted)
			Require(Same(boundary, old), name + " failure must leave input untouched");
	}
	const QJsonObject shapeData = ReadJson(QString::fromLocal8Bit(argv[2]));
	const QJsonObject masks = shapeData.value("masks").toObject();
	const double sectors[4][2] = {{0.875, 0.5}, {0.5, 0.875}, {0.125, 0.5}, {0.5, 0.125}};
	const int origins[3][2] = {{0, 0}, {-3, -2}, {2, -3}};
	int shapeChecks = 0;
	for(int mask = 0; mask < 256; ++mask)
		for(const auto& origin : origins)
			for(int sector = 0; sector < 4; ++sector)
			{
				const QPointF cursor((origin[0] + sectors[sector][0]) * AuLegacyShip::GridSpacing,
					(origin[1] + sectors[sector][1]) * AuLegacyShip::GridSpacing);
				QPolygonF expected;
				if(mask < 16)
				{
					expected = Polygon(masks.value(QString::number(mask)).toArray().at(sector).toArray(),
						AuLegacyShip::CoordinateSpacing);
					for(QPointF& point : expected)
						point += QPointF(origin[0] * AuLegacyShip::GridSpacing, origin[1] * AuLegacyShip::GridSpacing);
				}
				Require(Same(AuLegacyShip::Geometry::FillShape(quint8(mask), cursor), expected),
					QString("FillShape mask %1 sector %2").arg(mask).arg(sector));
				++shapeChecks;
			}
	const double grid = AuLegacyShip::GridSpacing;
	const QPolygonF negativeSeam = AuLegacyShip::Geometry::FillShape(5, QPointF(-grid, -grid));
	Require(negativeSeam.size() == 4 && negativeSeam.first().x() == -2 * grid
		&& negativeSeam.first().y() == -2 * grid, "negative exact-grid seam");
	Require(AuLegacyShip::Geometry::FillShape(5,
		QPointF(std::numeric_limits<double>::infinity(), 0)).isEmpty(), "nonfinite cursor guard");
	QPolygonF square;
	square << QPointF(0, 0) << QPointF(0, grid) << QPointF(grid, grid) << QPointF(grid, 0);
	QPolygonF unsupported;
	unsupported << QPointF(0, 0) << QPointF(grid, 0) << QPointF(spacing, spacing * 2);
	QString error;
	QPolygonF unchanged = square;
	Require(!AuLegacyShip::Geometry::Merge(unchanged, unsupported, &error)
		&& !error.isEmpty() && Same(unchanged, square), "unsupported edge rejects atomically");
	unsupported[2] = QPointF(100000, 100000);
	Require(!AuLegacyShip::Geometry::CanSubtract(square, unsupported, &error)
		&& !error.isEmpty(), "out-of-range coordinates reject before raster conversion");
	if(failures) return 1;
	QTextStream(stdout) << "PASS: " << cases.size() << " raster fixtures, " << shapeChecks
		<< " cell-table checks, seam and invalid-input guards\n";
	return 0;
}
