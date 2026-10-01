#pragma once

#include "context.h"
#include "trims.h"

namespace SimpleMode::detail {

quint16 Add(AuBdDesign& design, AuScPart& part, const QString& what, QTextStream& log);

class PlanBuilder
{
public:
	PlanBuilder(BuildContext& context, QTextStream& logTo);

	void PlaceStairs();
	void BuildSite();
	void BuildDeck();
	void BuildPosts();
	void BuildStations();
	void BuildTurret(int fixtureIndex);
	void BuildFixtures();
	void SetVolumes(const QMap<QChar, int>& quarters);

	BuildContext& ctx;
	Plan& plan;
	int& D;
	const double S, H, HC, VF, VC;
	AuBdDesign design;
	QTextStream& log;
	bool checks = true;
	int voidCount = 0, doorCount = 0, hatchCount = 0, windowCount = 0, roofCount = 0, dormerCount = 0, skylightCount = 0;

private:
	struct ConsoleSpot
	{
		QPoint doorAt;
		Line doorLine;
		V2 at, u, inward;
		QChar room;
	};
	struct SwitchSpot
	{
		V2 p, n;
		int side;
	};
	struct WindowSpot
	{
		V2 m, u;
		double half, sill, head, margin;
	};

	struct PlacedDoor { Door door; DoorPlace place; };
	enum { KindWindow, KindOpening, KindRailing };
	struct PlacedWindow
	{
		Window window;
		DoorPlace place;
		double width, sill, head;
		int tech;
		int kind = KindWindow;
		Door door;
		Railing railing;
	};

	void PlaceDeckItems();
	void BuildHull();
	void BuildRooms();
	void BuildOutsideTrims();
	void BuildOpenAir();
	void BuildUnderVoids();
	void BuildRoofs();
	void BuildSkylights();
	void BuildDormers();
	void BuildRoomVoids();
	void BuildOpenVoids();
	void BuildWindows();
	void BuildDoors();
	void BuildLifts();
	void BuildHatches();
	void BuildStairs();
	void BuildStairWells();
	void BuildPaths();

	quint16 add(AuScPart& part, const QString& what);
	std::function<quint16(AuScPart&, const QString&)> adder() { return [this](AuScPart& p, const QString& w) { return add(p, w); }; }
	AuPoint up(const AuPoint& p, int deck);
	QChar whole(const QPoint& p);
	QChar facing(const QPoint& p, Side s);
	double thickness(QChar other);
	double voidInset(const QPoint& p, Side s);
	bool stairCellOn(int deck, const QPoint& p);
	double stairSurface(int deck, const V2& p);
	bool hatchAt(int deck, const QPoint& p);
	void textureBySide(AuScFaceList& faces, const QString& up, const QString& side, const QString& down, double upScale = 1,
		double sideScale = 1, double downScale = 1);
	bool roofHatchAt(int deck, const QPoint& p);
	double midX(const QPoint& p);
	double midY(const QPoint& p);
	AuPoint centre(const QPoint& c, double z);
	QList<Piece> piecesOf(int deck);
	int pieceIndex(const QList<Piece>& pieces, const QPoint& cell, int q);
	QVector<V2> quarterVoidUncut(const QPoint& cell, int q);
	const QList<QList<Edge>>& outlineOf(int deck, QChar room);
	QList<QVector<V2>> carveVoid(QChar room, QVector<V2> v, const QVector<V2>& base);
	static QVector<V2> rectPoly(double x0, double y0, double x1, double y1);
	WallRun wallRun(const V2& a, const V2& b, const V2& n, double top, const QList<QPair<double, double>>& doors);
	void addPath(const AuPoint& from, const AuPoint& to, quint16 doorId);
	QList<Hole> windowHoles(const V2& a, const V2& b, const V2& lineA, const V2& lineB, QChar room, bool hull);
	bool wallBeside(const PlacedDoor& pd, const V2& dir, double len);
	bool nothingAbove(const V2& m, const V2& n);
	const ConsoleSpot* consoleOf(const PlacedDoor& pd);
	bool onShell(const DoorPlace& place);
	QList<QPair<double, double>> openings(const V2& a, const V2& b, const V2& lineA, const V2& lineB, QChar room, bool hull);
	void addRoomVoid(const Room& room, AuScFaceList& faces, const QString& what);
	void addCarvedVoid(const Room& room, const QVector<V2>& poly, const QVector<V2>& base, const QString& what);
	AuPoint pieceMid(const V2& at, QChar room);
	double openFloorInset(QChar room, const QPoint& cell);
	bool arrivesOnOpenAir(const PlacedStair& ps);
	bool coveredAbove(const QPoint& cell, int q);
	bool coveredAbove(const QPoint& cell);
	QList<VoidBox> nearRects(QChar room);
	static QList<QPair<VoidBox, bool>> splitNear(const VoidBox& box, const QList<VoidBox>& nearList);
	const PlacedStair* stairArrivingAt(const QPoint& cell, Side s);

	AuMatrix4x4 lift;
	const Theme theme;
	int& roofPoints;
	int& roofBad;
	QTextStream& roofLog;
	QList<PlacedStair>& placedStairs;
	QList<OpenSpan>& openSpans;
	QList<OpenSpan>& railingLines;
	QList<GroundSpot>& groundSpots;
	QList<StairCheck>& stairChecks;
	QList<DoorCheck>& doorChecks;
	QList<HatchCheck>& hatchChecks;

	QList<ConsoleSpot> consoles;
	QList<SwitchSpot> switchSpots;
	QList<WindowSpot> windowSpots;
	QList<PlacedDoor> doors;
	QList<PlacedWindow> windows;
	QStringList doorStyles;
	QList<V2> switchSides;
	QList<QList<int>> switchMarkers;
	QList<Piece> pieces;
};

}
