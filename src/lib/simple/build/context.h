#pragma once

#include <QHash>
#include <QTextStream>

#include "common.h"
#include "../geometry/outline.h"
#include "stairs.h"

namespace SimpleMode::detail {

struct VoidBox
{
	double x0, y0, x1, y1;
};

struct PlacedStair
{
	Stair stair;
	StairLayout layout;
	QString style;
	QList<StairFrame> frames;
	struct Landing { double x0, y0, x1, y1, z; bool outer[4]; };
	QList<Landing> landings;
};

struct Piece { QPoint cell; int quarter; QChar room; V2 mid; };

struct DoorCheck { quint16 id; int deck; AuPoint inGap; AuPoint outside; bool hull; AuPoint walkFrom, walkTo, door, outward;
	bool always = false; };
struct OpenSpan { int deck; V2 m, u; double half; };

struct GroundSpot { int deck; V2 m, u, n; double from, half; };

struct StairCheck { int deck; QList<AuPoint> path; QList<QPair<AuPoint, AuPoint>> flights; };

struct HatchCheck { quint16 id; int deck; double x, y; bool roof = false; double bx = 0, by = 0, sx = 0, sy = 0; };

struct StationCheck { quint16 id; int deck; QPoint cell; QString room; QList<AuPoint> spots; };

struct ArrivalCheck { int deck; V2 edge, F; double half; bool ground = false; };

class BuildContext
{
public:
	Plan plan;
	double S = 0, H = 0, HC = 0;
	double VF = 0, VC = 0;
	int D = 0;

	QList<PlacedStair> placedStairs;
	QList<DoorCheck> doorChecks;
	QList<HatchCheck> hatchChecks;
	QList<OpenSpan> openSpans;
	QList<OpenSpan> railingLines;
	QList<GroundSpot> groundSpots;
	QList<StairCheck> stairChecks;
	QList<ArrivalCheck> arrivalChecks;
	QList<StationCheck> stationChecks;
	struct FixtureCheck { quint16 id; QString type, kind, room; int deck; QPoint cell; };
	QList<FixtureCheck> fixtureChecks;
	QList<QPair<int, VoidBox>> nearAreas;

	int roofPoints = 0, roofBad = 0;
	QString roofText;
	QTextStream roofLog{&roofText};

	AuPoint up(const AuPoint& p, int deck) const;
	QChar whole(const QPoint& p) const;
	QChar facing(const QPoint& p, Side s) const;
	double thickness(QChar other) const;
	double voidInset(const QPoint& p, Side s) const;
	bool stairCellOn(int deck, const QPoint& p) const;
	double stairSurface(int deck, const V2& p) const;
	double midX(const QPoint& p) const;
	double midY(const QPoint& p) const;
	QList<Piece> piecesOf(int deck) const;
	const QList<QList<Edge>>& outlineOf(int deck, QChar room);

private:
	QHash<QPair<int, QChar>, QList<QList<Edge>>> outlines;
};

INTERIOR_LIGHT LightOf(const QString& lights);

int RunSelfChecks(BuildContext& ctx, const QString& outPath, QTextStream& log);

}
