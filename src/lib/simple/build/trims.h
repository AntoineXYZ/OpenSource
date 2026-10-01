#pragma once

#include <functional>

#include "../geometry/mesh.h"
#include "theme.h"

namespace SimpleMode::detail {

struct WallRun
{
	V2 a, u, n;
	double len = 0;
	double top = 0;
	QList<QPair<double, double>> doors;
	struct Keep { double s0, s1, z0, z1; };
	QList<Keep> keep;
	QList<Keep> ceilings;
	bool onFloor = false;
	bool lowPoly = false;
	std::function<int(const QString&)> slot;
	AuScFaceList* panels = nullptr;
};

enum Trim
{
	Skirting = 1, TallSkirting = 2, HeavySkirting = 4, ChairRail = 8, TwoRails = 16, BumperRail = 32, Cornice = 64,
	PlainCornice = 128, Wainscot = 256, Tiles = 512, Finish = 1024, Posts = 2048, HeavyPosts = 4096, Bars = 8192,
	Pipes = 16384, Beams = 32768,
};

double Ceiling(const WallRun& w, double s0, double s1);

QList<QPair<double, double>> Free(const WallRun& w, double s0, double s1, double z0, double z1);

bool IsFree(const WallRun& w, double s0, double s1, double z0, double z1);

void RunBox(AuScFaceList& f, const WallRun& w, double s0, double s1, double z0, double z1, double d0, double d1, int skip = 0);

void Band(AuScFaceList& f, const WallRun& w, double z0, double z1, double d, double ext = 0);

void LogCourse(AuScFaceList& f, const WallRun& w, double s0, double s1, double zc, double h, double d);

void LogCourses(AuScFaceList& f, const WallRun& w, double z0, double z1, double pitch, double d);

void Blocks(AuScFaceList& f, const WallRun& w, double z0, double z1, double rowH, double blockL, double gap, double d,
	const QString& texture, double size);

void FlatPanel(AuScFaceList& f, const WallRun& w, double z0, double z1, double d);

void Panels(AuScFaceList& f, const WallRun& w, double z0, double z1, double panelW, double margin, double d);

QList<double> PostSpots(const WallRun& w, double spacing, double width, double z0, double z1);

void Casings(AuScFaceList& f, const WallRun& w, Theme theme, double scale = 1);

int TrimsOf(const QString& type);

void TrimRoomWall(AuScFaceList& f, const WallRun& w, Theme theme, int trims);

void TrimOutsideWall(AuScFaceList& f, const WallRun& w, Theme theme, bool ground, const QList<QPair<double, double>>& eaves);

void TrimOutsideCorner(AuScFaceList& f, const V2& p, const V2& u1, const V2& u2, double top, bool ground, Theme theme);

}
