#pragma once

#include <QColor>
#include <QString>
#include "plan.h"

namespace SimpleMode {

struct DoorPlace
{
	bool ok = false;
	QString problem;
	QChar here, there;
	double mx = 0, my = 0;
	double ux = 0, uy = 0;
	double nx = 0, ny = 0;
};
DoorPlace PlaceDoor(const Plan& plan, const Door& door);
DoorPlace PlaceWindow(const Plan& plan, const Window& window);
DoorPlace PlaceRailing(const Plan& plan, const Railing& railing);
bool WindowClear(const Plan& plan, const Window& window, QString& problem, int ignoreWindow = -1);

double DoorWidth(const Plan& plan, const Door& door);
double WindowWidth(const Plan& plan, const Window& window);
double RailingLength(const Plan& plan, const Railing& railing);
double RailingHeight(const Plan& plan, const Railing& railing);
DoorPlace PlaceGap(const Plan& plan, const RailingGap& gap);
double GapWidth(const Plan& plan, const RailingGap& gap);
void OpeningHeights(const Plan& plan, const Door& opening, double& spring, double& top);

StairLayout LayoutStair(const Plan& plan, const Stair& stair);

bool CanRoof(const Plan& plan, int deck, const QPoint& cell);

QString DormerProblem(const Plan& plan, const Dormer& dormer, int ignore = -1);
QString SkylightProblem(const Plan& plan, const Skylight& skylight, int ignore = -1);
QString RoofHatchProblem(const Plan& plan, const Hatch& hatch, int ignore = -1);

bool IsOpenCell(const Plan& plan, int deck, const QPoint& cell);
QChar SpaceRoom(const Plan& plan, int deck, const QPoint& cell);
bool IsOpenQuarter(const Plan& plan, int deck, const QPoint& cell, int quarter);
QChar SpaceRoom(const Plan& plan, int deck, const QPoint& cell, int quarter);

bool IsOpenAirRoom(const Plan& plan, QChar room);
bool IsOpenAirQuarter(const Plan& plan, int deck, const QPoint& cell, int quarter);
bool IsOpenEdge(const Plan& plan, QChar room, QChar other);
double OpenEdgeInset(const Plan& plan, QChar room);
bool HasOpenAirRailing(const Plan& plan, int deck, QChar room);
bool IsOutsideWall(const Plan& plan, const DoorPlace& place);

QString StationProblem(const Plan& plan, const Station& station, int ignore = -1);
QPoint StationSpareCell(const Plan& plan, const Station& station);

QString FixtureProblem(const Plan& plan, const Fixture& fixture, int ignore = -1);
void ParkingSize(const QString& kind, double& across, double& along);
bool ParkingCentre(const Plan& plan, const Fixture& fixture, double& x, double& y);

QList<int> GunnerLinks(const Plan& plan);
int TurretNumber(const Plan& plan, int fixtureIndex);
int TurretGunner(const Plan& plan, int fixtureIndex);
QString RoomTexture(const Plan& plan, const Room& room, const QString& surface);
QString BuildingTexture(const Plan& plan, const QString& surface);
QString RoomDesign(const Room& room);
double RoomTextureScale(const Room& room, const QString& surface);
double BuildingTextureScale(const Plan& plan, const QString& what);
QString OutsideDressing(const Plan& plan);
QString OutsideTrimTexture(const Plan& plan);
QColor OutsideTrimColor(const Plan& plan);
double OutsideTrimStrength(const Plan& plan);
QString RoomTrimTexture(const Plan& plan, const Room& room);
QColor RoomColor(const Room& room, const QString& surface);
double RoomColorStrength(const Room& room, const QString& surface);
int TextureSlot(const Plan& plan, const QString& key);

QList<QPoint> TurretCells(const Fixture& turret);
bool TurretOnGround(const Plan& plan, const Fixture& turret);

bool RoomShapeOk(const Plan& plan, QChar room, QString& problem);

bool IsLiftRoom(const Plan& plan, QChar room);
QList<int> LiftDecks(const Plan& plan, QChar room);
QString LiftProblem(const Plan& plan, QChar room);
QString LiftStopName(const Plan& plan, QChar room, int deck);
QChar LiftOf(const Plan& plan, const DoorPlace& place);

}
