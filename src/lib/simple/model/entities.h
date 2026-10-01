#pragma once

#include <QList>
#include <QMap>
#include <QPoint>
#include <QString>

#include "types.h"

namespace SimpleMode {

struct Room
{
	QChar letter;
	QString name;
	QString type;
	int tag = 1;
	QString lights = "bright";
	bool sunlit = true;
	QString floor, walls, ceiling;
	QString trim;
	QString design;
	QMap<QString, QString> colors;
	QMap<QString, int> strengths;
	QMap<QString, int> scales;
};

struct Door
{
	int deck;
	QPoint at;
	Line line;
	bool automatic;
	QString style;
	QString operation = "switches";
	QPoint console = QPoint(-1, -1);
	bool locked = false;
	bool vehicles = false;
	bool wide = false;
	bool opening = false;
};

struct RailingGap
{
	int deck;
	QPoint at;
	Line line;
	bool full = false;
};

struct Railing
{
	int deck;
	QPoint at;
	Line line;
	QString style;
};

struct Window
{
	int deck;
	QPoint at;
	Line line;
	QString shape;
	int tech = 0;
};

struct Hatch
{
	int deck;
	QPoint cell;
	QString style;
	QString ladder;
	int side = North;
	bool roof = false;
};

enum StairKind { StairStraight, StairTurnLeft, StairTurnRight, StairUTurnLeft, StairUTurnRight };
struct Stair
{
	int deck;
	QPoint cell;
	int dir = North;
	int kind = StairStraight;
	QString style;
};

struct StairLayout
{
	bool ok = false;
	QString problem;
	QList<QPoint> cells;
	struct Flight { QPoint from, to; int dir; double z0, z1; };
	QList<Flight> flights;
	QList<QPoint> landings;
	QPoint entry;
	QPoint top;
	int arrivalSide = North;
	QPoint arrival;
};

struct Station
{
	int deck;
	QPoint cell;
	int dir = North;
	QString kind = "command";
	bool wall = false;
	QString module;
	QString pose;
	QString patient;
	QList<QPoint> pads;
	int effect = 0;
	int turret = 0;
};

struct Fixture
{
	int deck;
	QPoint cell;
	int dir = North;
	QString type;
	QString kind;
	QString color;
	double height = 0;
};
inline constexpr int FixtureTop = 4;

struct Post
{
	int deck;
	QPoint cell;
	QString kind;
};

struct Roof
{
	QChar letter;
	QString style;
	int dir = East;
	double pitch = 0;
};

struct Dormer
{
	int deck;
	QPoint cell;
	int side = South;
	QString style;
};

struct Skylight
{
	int deck;
	QPoint cell;
	QString style;
};

}
