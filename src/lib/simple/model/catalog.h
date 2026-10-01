#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace SimpleMode {

struct StyleInfo
{
	QString key;
	QString name;
	QString note;
};

using ThemeKind = StyleInfo;
using OpeningStyle = StyleInfo;
using RailingStyle = StyleInfo;
using HatchStyle = StyleInfo;
using StairStyle = StyleInfo;
using DetailStyle = StyleInfo;

struct RoomKind
{
	QString key;
	QString name;
	bool buildable;
	QString note;
};

struct StationKind
{
	QString key;
	QString name;
	QString note;
	QStringList modules;
	QStringList moduleNames;
};

struct WindowShape
{
	QString key;
	QString name;
	QString note;
	double width;
	double sill, head;
};

struct DoorStyle
{
	QString key;
	QString name;
	QString note;
	bool single;
	bool wide;
};

struct RoofStyle
{
	QString key;
	QString name;
	QString note;
	bool ridged;
	bool sloped;
	bool perCell;
	double pitch;
};

const QList<RoomKind>& RoomKinds();
const RoomKind* FindRoomKind(const QString& key);
QString DefaultRoomName(const QString& type, int tag);
const QList<ThemeKind>& ThemeKinds();

const QList<DoorStyle>& DoorStyles();
const DoorStyle* FindDoorStyle(const QString& key);
QString DefaultDoorStyle(const QString& theme, bool hullDoor, bool wide = false);
const QList<OpeningStyle>& OpeningStyles();
const OpeningStyle* FindOpeningStyle(const QString& key);

struct FixtureType
{
	QString key;
	QString name;
	QStringList kinds;
	QStringList kindNames;
	QString note;
};
const QList<FixtureType>& FixtureTypes();
const FixtureType* FindFixtureType(const QString& key);
const QStringList& LightColors();

const QList<StationKind>& StationKinds();
const StationKind* FindStationKind(const QString& key);

const QList<RailingStyle>& RailingStyles();
const RailingStyle* FindRailingStyle(const QString& key);
QString DefaultRailingStyle(const QString& theme);

const QList<WindowShape>& WindowShapes();
const WindowShape* FindWindowShape(const QString& key);
QStringList WindowTechNames();
int DefaultWindowTech(const QString& theme);

const QList<HatchStyle>& HatchStyles();
const QList<HatchStyle>& LadderStyles();
const HatchStyle* FindHatchStyle(const QString& key);
const HatchStyle* FindLadderStyle(const QString& key);
QString DefaultHatchStyle(const QString& theme);
QString DefaultLadderStyle(const QString& theme);

const QList<StairStyle>& StairStyles();
const StairStyle* FindStairStyle(const QString& key);
QString DefaultStairStyle(const QString& theme);
QStringList StairKindKeys();
QStringList StairKindNames();

const QList<RoofStyle>& RoofStyles();
const RoofStyle* FindRoofStyle(const QString& key);
QString DefaultRoofStyle(const QString& theme);
const QList<DetailStyle>& DormerStyles();
const DetailStyle* FindDormerStyle(const QString& key);
const QList<DetailStyle>& SkylightStyles();
const DetailStyle* FindSkylightStyle(const QString& key);

struct TextureKind
{
	QString key;
	QString name;
	int texlib;
	double size;
};
const QList<TextureKind>& TextureKinds();
const TextureKind* FindTextureKind(const QString& key);
const QStringList& ThemePalette(const QString& theme);
QString ThemeTexture(const QString& theme, const QString& surface);
QStringList RoomTypeTextures(const QString& type, const QString& surface);
int MaterialTexlib(const QString& theme);
QStringList RoomDesignKinds();
const ThemeKind* FindThemeKind(const QString& key);

}
