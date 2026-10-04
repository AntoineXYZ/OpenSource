// Manual end-to-end working-design tests. Never built automatically.
#include "aulegacyshipedit.h"
#include "aulegacyshipgeometry.h"

#include <QCoreApplication>
#include <QTextStream>
#include <cmath>
#include <limits>

using namespace AuLegacyShip;

namespace
{
int failures = 0;

void Check(bool condition, const QString& label)
{
	if(!condition)
	{
		++failures;
		QTextStream(stderr) << "FAIL: " << label << '\n';
	}
}

Design Empty()
{
	Design design;
	design.header.designId = 77;
	design.header.name = QStringLiteral("In-memory fixture");
	design.header.version = 2;
	design.header.versionStateRaw = 0x20;
	return design;
}

QPointF Cell(double x, double y)
{
	return QPointF(x * GridSpacing, y * GridSpacing);
}

CellEditResult Paint(Design& design, double x, double y, int type = 5,
	quint8 number = 0, int deck = 0, quint8 mask = 5)
{
	return EditCell(design, deck, Cell(x, y), mask, type, number);
}

bool SameBoundary(const QPolygonF& a, const QPolygonF& b)
{
	if(a.size() != b.size()) return false;
	for(int i = 0; i < a.size(); ++i)
		if(a[i].x() != b[i].x() || a[i].y() != b[i].y()) return false;
	return true;
}

double Area(const Room& room)
{
	const QPolygonF points = RoomBoundary(room);
	double area = 0;
	for(int i = 0; i < points.size(); ++i)
	{
		const QPointF a = points[i], b = points[(i + 1) % points.size()];
		area += a.x() * b.y() - a.y() * b.x();
	}
	return std::abs(area / 2);
}

bool Close(double a, double b) { return std::abs(a - b) < 1e-8; }

void BasicEdits()
{
	Design design = Empty();
	CellEditResult result = Paint(design, .25, .25);
	Check(result.accepted && result.changed && design.rooms.size() == 1, "first cell: " + result.error);
	if(design.rooms.size() != 1) return;
	Check(design.rooms[0].roomId == 1 && design.rooms[0].designId == 77
		&& design.rooms[0].hasWorkingBoundary, "new room ID and working geometry");
	const QPolygonF first = RoomBoundary(design.rooms[0]);
	result = Paint(design, .25, .25);
	Check(!result.accepted && !result.changed && SameBoundary(first, RoomBoundary(design.rooms[0])),
		"contained paint is rejected without changes");
	result = Paint(design, 4.25, .25);
	Check(!result.accepted && design.rooms.size() == 1
		&& SameBoundary(first, RoomBoundary(design.rooms[0])), "disconnected same-key paint is atomic");
	result = Paint(design, 1.25, .25);
	Check(result.accepted && result.changed && design.rooms.size() == 1
		&& design.rooms[0].roomId == 1 && Close(Area(design.rooms[0]), 2 * GridSpacing * GridSpacing),
		"adjacent same-key paint merges, retains ID: " + result.error);
	result = Paint(design, 1.25, .25, 5, 1);
	Check(result.accepted && result.changed && design.rooms.size() == 2, "repaint with another room number: " + result.error);
	if(design.rooms.size() != 2) return;
	Check(design.rooms[0].roomId == 1 && design.rooms[0].roomSubtype == 0
		&& design.rooms[1].roomId == 2 && design.rooms[1].roomSubtype == 1
		&& Close(Area(design.rooms[0]), GridSpacing * GridSpacing)
		&& Close(Area(design.rooms[1]), GridSpacing * GridSpacing), "repaint subtracts from competing room");
	result = Paint(design, .25, .25, 5, 1);
	Check(result.accepted && design.rooms.size() == 1 && design.rooms[0].roomId == 2,
		"growing target consumes competitor and preserves target ID");
	result = Paint(design, .25, .25, 5, 0, 1);
	Check(result.accepted && design.rooms.size() == 2 && design.rooms[1].roomId == 1
		&& design.rooms[1].deck == 1, "room key includes deck; freed lowest ID reused");
	result = Paint(design, .25, .25, -1, 0, 1);
	Check(result.accepted && result.changed && design.rooms.size() == 1, "complete cell erasure deletes room");
	result = Paint(design, .25, .25, -1, 0, 1);
	Check(result.accepted && !result.changed, "empty erasure reports success without dirty geometry");
}

void TopologyAndLimits()
{
	Design design = Empty();
	for(int x = 0; x < 3; ++x) Check(Paint(design, x + .25, .25).accepted, "bridge setup");
	if(design.rooms.size() != 1) return;
	const QPolygonF before = RoomBoundary(design.rooms[0]);
	CellEditResult result = Paint(design, 1.25, .25, -1);
	Check(!result.accepted && !result.changed && design.rooms.size() == 1
		&& SameBoundary(before, RoomBoundary(design.rooms[0])), "bridge erasure rejected atomically");
	result = Paint(design, 1.25, .25, 6);
	Check(!result.accepted && !result.changed && design.rooms.size() == 1
		&& SameBoundary(before, RoomBoundary(design.rooms[0])), "competing repaint preflight is atomic");
	result = Paint(design, 2.25, .25, -1);
	Check(result.accepted && result.changed, "edge erasure accepted");
	Check(Paint(design, .25, .25, 5, 0, 28).accepted, "positive deck limit inclusive");
	Check(Paint(design, .25, .25, 5, 0, -28).accepted, "negative deck limit inclusive");
	Check(!Paint(design, .25, .25, 5, 0, 29).accepted, "above deck limit rejected");
	Check(!Paint(design, .25, .25, 5, 0, -29).accepted, "below deck limit rejected");
	Check(Paint(design, .25, .25, -1, 0, 29).accepted, "erase bypasses paint-deck limit");
	Check(!Paint(design, 60.25, .25, 6, 1).accepted, "cell vertices beyond grid rejected");
	Check(!Paint(design, .25, .25, 6, 1, 0, 1).accepted, "unbounded grid mask rejected");
	result = EditCell(design, 0, QPointF(std::numeric_limits<double>::infinity(), 0), 5, 5, 0);
	Check(!result.accepted && !result.changed, "nonfinite cursor rejected");
	Design halves = Empty();
	Check(Paint(halves, .875, .5, 5, 0, 0, 7).accepted, "diagonal half cell added");
	Check(halves.rooms.size() == 1 && Close(Area(halves.rooms[0]), .5 * GridSpacing * GridSpacing),
		"diagonal grid uses half-square triangle");
	Check(Paint(halves, .5, .875, 5, 0, 0, 7).accepted, "opposite half merges");
	Check(halves.rooms.size() == 1 && Close(Area(halves.rooms[0]), GridSpacing * GridSpacing),
		"opposite halves form full square");
}

void RoofsAndDetails()
{
	Design design = Empty();
	Check(Paint(design, .25, .25).accepted, "roof setup ordinary room");
	if(design.rooms.size() != 1) return;
	Room roof = design.rooms[0];
	roof.roomId = 20;
	roof.roomType = 0x16;
	design.rooms.append(roof);
	const QPolygonF roofBefore = RoomBoundary(roof);
	Check(Paint(design, .25, .25, 6).accepted, "repaint under roof");
	bool foundRoof = false;
	for(const Room& room : design.rooms)
		if(room.roomId == 20) foundRoof = SameBoundary(RoomBoundary(room), roofBefore);
	Check(foundRoof, "add/repaint excludes existing roof overlap");
	Check(Paint(design, .25, .25, -1).accepted && design.rooms.isEmpty(), "erase includes roof records");
	Check(Paint(design, .25, .25, 2).accepted, "bridge equipment setup");
	if(design.rooms.size() != 1) return;
	Detail detail;
	detail.designId = 77;
	detail.detailId = 12;
	detail.detailType = 1;
	detail.packedXY = 0x0101;
	detail.roomId = 500;
	design.details.append(detail);
	CellEditResult result = Paint(design, 20.25, 20.25, -1);
	Check(result.accepted && result.changed && design.details.size() == 1
		&& design.details[0].roomId == design.rooms[0].roomId,
		"empty erase still reconciles equipment room association: " + result.error);
	result = Paint(design, .25, .25, -1);
	Check(result.accepted && design.rooms.isEmpty() && design.details.isEmpty(),
		"erasing room removes invalid equipment");
}
}

int main(int argc, char** argv)
{
	QCoreApplication application(argc, argv);
	BasicEdits();
	TopologyAndLimits();
	RoofsAndDetails();
	// Optional original files are loaded read-only and edited only in memory.
	for(int i = 1; i < argc; ++i)
	{
		Design design;
		QString error;
		const QString file = QString::fromLocal8Bit(argv[i]);
		if(!Load(file, design, error)) { Check(false, file + ": " + error); continue; }
		const Header original = design.header;
		const CellEditResult result = Paint(design, .25, .25, 5, 0, 28);
		Check(result.accepted && result.changed, file + " new-deck edit/reconciliation: " + result.error);
		Check(design.header.name == original.name && design.header.designId == original.designId,
			file + " source metadata retained");
		QTextStream(stdout) << file << ": " << design.rooms.size() << " rooms, "
			<< design.details.size() << " details after in-memory edit\n";
	}
	QTextStream(stdout) << "Working-design edit tests: " << failures << " failures\n";
	return failures ? 1 : 0;
}
