// Manual native regression harness. The assistant must never compile this.
#include "aulegacyshipdetails.h"
#include <QCoreApplication>
#include <QTextStream>
#include <cmath>
#include <limits>

using namespace AuLegacyShip;
namespace
{
int failures = 0;
void Check(bool value, const char* message)
{
	if(!value) { ++failures; QTextStream(stderr) << "FAIL: " << message << '\n'; }
}
quint16 Packed(int x, int y) { return quint16((quint16(quint8(x)) << 8) | quint8(y)); }
QPointF Point(double x, double y) { return QPointF(x * CoordinateSpacing, y * CoordinateSpacing); }
Room Square(int id, int type, int deck = 0, int xmin = 0, int ymin = 0, int xmax = 12, int ymax = 12)
{
	Room room;
	room.roomId = quint16(id); room.roomType = quint8(type); room.deck = qint8(deck);
	room.packedVertices = {Packed(xmin,ymin), Packed(xmin,ymax), Packed(xmax,ymax), Packed(xmax,ymin)};
	return room;
}
Detail Item(int id, int type, int x, int y, int deck = 0, int room = 1)
{
	Detail detail;
	detail.detailId = quint32(id); detail.detailType = quint8(type); detail.packedXY = Packed(x,y);
	detail.deck = qint8(deck); detail.roomId = quint16(room);
	return detail;
}
bool Same(const QVector<Detail>& a, const QVector<Detail>& b)
{
	if(a.size() != b.size()) return false;
	for(int i = 0; i < a.size(); ++i)
	{
		const Detail& x = a.at(i); const Detail& y = b.at(i);
		if(x.streamOffset != y.streamOffset || x.deck != y.deck || x.detailType != y.detailType
			|| x.hiddenRaw != y.hiddenRaw || x.detailId != y.detailId || x.designId != y.designId
			|| x.packedXY != y.packedXY || x.roomId != y.roomId || x.commodityIdRaw != y.commodityIdRaw
			|| x.commodityIdNormalized != y.commodityIdNormalized) return false;
	}
	return true;
}

void BasicPlacement()
{
	Design design; design.header.designId = 77; design.rooms = {Square(42,8)};
	DetailEditResult result = PlaceDetail(design,0,Point(4.4,4.2),10);
	Check(result.accepted && result.changed && result.added == 1, "engine placement accepted");
	Check(design.details.size() == 1 && design.details[0].packedXY == Packed(4,4)
		&& design.details[0].detailId == 1 && design.details[0].roomId == 42
		&& design.details[0].designId == 77 && design.details[0].hiddenRaw == 0, "snap and new record metadata");
	const QVector<Detail> before = design.details;
	result = PlaceDetail(design,0,Point(4,4),10);
	Check(result.accepted && !result.changed && Same(before,design.details), "repeat exact type is unchanged");
	result = PlaceDetail(design,0,Point(4,4),1);
	Check(!result.accepted && !result.error.isEmpty() && Same(before,design.details), "invalid replacement is atomic");
	result = PlaceDetail(design,0,Point(4,4),14);
	Check(result.changed && result.added == 1 && result.removed == 1 && design.details.size() == 1
		&& design.details[0].detailType == 14 && design.details[0].detailId == 1, "replacement reuses lowest free ID");
	const QVector<Detail> replaced = design.details;
	result = PlaceDetail(design,0,QPointF(std::numeric_limits<double>::quiet_NaN(),0),10);
	Check(!result.accepted && Same(replaced,design.details), "nonfinite cursor is atomic");
	result = PlaceDetail(design,0,Point(121,0),10);
	Check(!result.accepted && Same(replaced,design.details), "grid range validation");
	result = PlaceDetail(design,0,Point(6,6),10,0,26);
	Check(!result.accepted && Same(replaced,design.details), "original brush range");
	Design negative; negative.rooms = {Square(1,8,0,-12,-12,0,0)};
	result = PlaceDetail(negative,0,Point(-2.5,-2.5),10);
	Check(result.changed && negative.details[0].packedXY == Packed(-3,-3), "negative halfway snap is away from zero");
}

void RestrictionsAndOrdering()
{
	Design design; design.rooms = {Square(42,2)};
	Check(PlaceDetail(design,0,Point(3,3),6).changed, "engineer station allowed on bridge");
	Check(!PlaceDetail(design,0,Point(9,9),10).accepted, "maneuver drive rejected on bridge");
	Check(!PlaceDetail(design,0,Point(9,9),6).accepted, "station limit is per room");
	design.details = {Item(2,76,1,1,-1,0), Item(4,98,1,1,1,42)};
	design.rooms = {Square(42,8)};
	DetailEditResult result = PlaceDetail(design,0,Point(6,6),10);
	Check(result.changed && result.detailId == 1 && design.details.size() == 3
		&& design.details[0].detailId == 2 && design.details[1].detailId == 1
		&& design.details[2].detailId == 4, "ID allocation and signed deck insertion order");
	design.rooms = {Square(1,3,0,0,0,4,4), Square(2,3,0,4,0,8,4)}; design.details.clear();
	Check(PlaceDetail(design,0,Point(4,2),5).changed && design.details[0].roomId == 2,
		"door requires shared wall and takes final matching room ID");
	Check(!PlaceDetail(design,0,Point(0,2),5).accepted, "interior door rejected on hull");
	design.rooms = {Square(1,3,0), Square(2,3,1)}; design.details.clear();
	Check(PlaceDetail(design,1,Point(4,4),8).changed, "interior hatch spans adjacent decks");
	design.rooms.removeLast(); design.details.clear();
	Check(!PlaceDetail(design,1,Point(4,4),8).accepted, "interior hatch requires both rooms");
	Check(PlaceDetail(design,1,Point(4,4),52).changed, "hull hatch may attach to room below");
}

void SpacecraftModeRestrictions()
{
	Design design;
	for(int type : {100, 142})
	{
		design.rooms = {Square(1,5)};
		design.details.clear();
		for(const QPointF& point : {Point(6,6), Point(0,6), Point(14,14)})
			Check(!PlaceDetail(design,0,point,type).accepted && design.details.isEmpty(),
				"building poles rejected inside, on and outside spacecraft rooms");
		design.details = {Item(1,type,14,14)};
		const QVector<Detail> before = design.details;
		Check(!PlaceDetail(design,0,Point(14,14),type).accepted && Same(before,design.details),
			"imported building pole cannot bypass authoring restriction as a repeated placement");
		design.rooms.clear(); design.details.clear();
		Check(!PlaceDetail(design,0,Point(6,6),type).accepted,
			"building pole rejected on an empty spacecraft");
	}
	design.rooms = {Square(1,5,-2)}; design.details.clear();
	Check(PlaceDetail(design,-2,Point(6,6),141).changed, "troop station inside spacecraft room");
	const QVector<Detail> troop = design.details;
	Check(!PlaceDetail(design,-2,Point(14,14),141).accepted && Same(troop,design.details),
		"building outdoor troop-station permission does not apply to spacecraft");
	Check(!PlaceDetail(design,-2,Point(0,6),141).accepted,
		"troop station cannot be on a room wall");
	design.details.clear();
	Check(PlaceDetail(design,-2,Point(0,6),50).changed,
		"building underground hull-door restriction does not apply to spacecraft");
	design.details.clear();
	Check(PlaceDetail(design,-2,Point(6,6),52).changed,
		"building underground hull-hatch restriction does not apply to spacecraft");
	QString error;
	Check(ReconcileDetails(design,error) && design.details.size() == 1,
		"negative-deck spacecraft hull hatch survives reconciliation");
	design.rooms = {Square(1,9)}; design.details.clear();
	Check(!PlaceDetail(design,0,Point(6,6),141).accepted, "troop station rejected in fuel cell");
	design.rooms = {Square(1,5,28)};
	Check(PlaceDetail(design,29,Point(6,6),52).changed && design.details[0].roomId == 1,
		"hull hatch attaches above highest paintable spacecraft deck");
	design.details.clear();
	Check(PlaceDetail(design,29,Point(6,6),22).changed && design.details[0].detailType == 22,
		"top turret attaches above highest paintable spacecraft deck");
	design.rooms = {Square(1,5,41)}; design.details.clear();
	Check(PlaceDetail(design,42,Point(6,6),52).changed,
		"data-level equipment limit is wider than grid movement limit");
	const QVector<Detail> top = design.details;
	Check(!PlaceDetail(design,43,Point(6,6),52).accepted && Same(top,design.details),
		"equipment above data-level limit is rejected atomically");
}

void Brushes()
{
	Design design; design.rooms = {Square(1,8,0,0,0,4,4), Square(2,8,0,4,0,8,4)};
	DetailEditResult result = PlaceDetail(design,0,Point(2,2),10,0,3);
	Check(result.changed && result.added > 1, "equipment brush fills multiple points");
	for(const Detail& detail : design.details)
		Check(qint8(detail.packedXY >> 8) < 4 && detail.roomId == 1, "brush does not cross into adjoining room");
	const QVector<Detail> before = design.details;
	result = PlaceDetail(design,0,Point(2,2),10,0,3);
	Check(result.accepted && !result.changed && Same(before,design.details), "unchanged center does not brush surroundings");
	design.details = {Item(1,10,2,2), Item(2,10,3,2), Item(3,14,2,3), Item(4,10,8,8)};
	result = DeleteDetails(design,0,Point(2,2),1);
	Check(result.changed && result.removed == 2 && design.details.size() == 2
		&& design.details[0].detailType == 14 && design.details[1].packedXY == Packed(8,8),
		"erase brush only removes center's exact type in range");
	const QVector<Detail> after = design.details;
	result = DeleteDetails(design,0,Point(2,2),2);
	Check(result.accepted && !result.changed && Same(after,design.details), "empty center does not brush erase");
	design.details = {Item(1,10,2,2), Item(2,14,3,2), Item(3,10,3,2)};
	result = DeleteDetails(design,0,Point(2,2),1);
	Check(result.removed == 1 && design.details.size() == 2, "type filter stops at first coincident record");
	design.details = {Item(1,0,2,2), Item(2,10,3,2)};
	result = DeleteDetails(design,0,Point(2,2),1);
	Check(result.changed && result.removed == 1 && design.details.size() == 1
		&& design.details[0].detailType == 10, "erase removes a type-zero source record without expanding its brush");
}

void VehiclesAndMarkers()
{
	Design design; design.rooms = {Square(1,13)};
	DetailEditResult result = PlaceDetail(design,0,Point(6,6),57,82);
	Check(result.changed && design.details[0].commodityIdNormalized == 82, "SUV with original footprint fits bay");
	const QVector<Detail> before = design.details;
	result = PlaceDetail(design,0,Point(6,6),57,7);
	Check(result.accepted && !result.changed && Same(before,design.details), "same vehicle dtype retains original commodity");
	result = PlaceDetail(design,0,Point(6,6),58,0xfffe);
	Check(!result.accepted && !result.error.isEmpty() && Same(before,design.details), "missing bounds rolls back replacement");
	design.details.clear(); design.rooms = {Square(1,13,0,4,4,8,8)};
	Check(!PlaceDetail(design,0,Point(6,6),57,82).accepted, "vehicle footprint rejects undersized bay");
	design.rooms = {Square(1,8)};
	Check(PlaceDetail(design,0,Point(3,3),76).changed, "pivot inside design bounds");
	Check(PlaceDetail(design,1,Point(6,6),76).changed && design.details.size() == 1
		&& design.details[0].deck == 1, "pivot relocates rather than duplicates");
	design.details.clear();
	for(int i = 0; i < 10; ++i) Check(PlaceDetail(design,0,Point(i+1,1),98).changed, "contrail within global limit");
	const QVector<Detail> ten = design.details;
	Check(!PlaceDetail(design,0,Point(11,1),98).accepted && Same(ten,design.details), "contrail limit is atomic");
	Check(!PlaceDetail(design,0,Point(-2,-2),100).accepted, "flag pole is building-only");
}

void Metadata()
{
	Check(DetailName(1) == QStringLiteral("Navigator Station"), "original equipment names");
	Check(!DetailPlacementDescription(6).isEmpty(), "placement explanation available");
	Check(RotateDetailType(2) == 25 && RotateDetailType(31) == 2
		&& RotateDetailType(2,-1) == 31, "double berth orientation wraps");
	Check(RotateDetailType(8) == 77 && RotateDetailType(83) == 8
		&& RotateDetailType(51) == 84 && RotateDetailType(90) == 51, "hatch families preserve reserved gaps");
	Check(RotateDetailType(57,7) == 64 && RotateDetailType(64) == 57, "vehicle orientation wraps");
	Check(RotateDetailType(74) == 75 && RotateDetailType(75) == 74, "game table has two orientations");
	Check(RotateDetailType(109,-1) == 116 && CanonicalDetailType(116) == 109
		&& DetailRotationStep(116) == 7 && RotateDetailType(10) == 10, "stair and fixed-equipment orientation");
}

void StairReplacement()
{
	Design design; design.rooms = {Square(1,8)};
	design.details = {Item(5,10,1,1), Item(6,98,1,1)};
	DetailEditResult result = PlaceDetail(design,0,Point(0,0),101);
	Check(result.changed && result.removed == 1 && design.details.size() == 2,
		"stair replacement removes ordinary equipment but preserves emitter");
	const QVector<Detail> before = design.details;
	result = PlaceDetail(design,0,Point(1,1),10);
	Check(!result.accepted && Same(before,design.details), "ordinary equipment cannot overlap stairs");
	design.rooms = {Square(1,8,0,0,0,2,2)};
	design.details = {Item(5,10,1,1)};
	const QVector<Detail> small = design.details;
	result = PlaceDetail(design,0,Point(0,0),101);
	Check(!result.accepted && Same(small,design.details), "oversized stair rejection preserves equipment it would remove");
}
}

int main(int argc, char** argv)
{
	QCoreApplication app(argc,argv);
	BasicPlacement(); RestrictionsAndOrdering(); Brushes(); VehiclesAndMarkers(); Metadata(); StairReplacement();
	SpacecraftModeRestrictions();
	QTextStream(stdout) << (failures ? "Equipment editing checks failed: " : "Equipment editing checks passed: ") << failures << '\n';
	return failures ? 1 : 0;
}
