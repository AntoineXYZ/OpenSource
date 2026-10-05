// Optional regression harness. The user builds this manually; it is not a game target.
#include "aulegacyshipstats.h"
#include "aulegacyshipperformance.h"
#include "aulegacyshipsave.h"

#include <QCoreApplication>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace AuLegacyShip;

namespace
{
Room Rectangle(int id, int type, int deck = 0, int number = 0, double x = 0.0)
{
	Room room;
	room.roomId = quint16(id);
	room.roomType = quint8(type);
	room.roomSubtype = quint8(number);
	room.deck = qint8(deck);
	room.hasWorkingBoundary = true;
	room.workingBoundary << QPointF(x, 0) << QPointF(x + 8, 0) << QPointF(x + 8, 4) << QPointF(x, 4);
	return room;
}

Detail Equipment(int type, int room, int commodity = 0)
{
	Detail detail;
	detail.detailType = quint8(type);
	detail.roomId = quint16(room);
	detail.commodityIdRaw = detail.commodityIdNormalized = quint16(commodity);
	return detail;
}
}

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	Design design;
	design.header.scalarSlots = DefaultModuleSlots();
	Stats stats = CalculateStats(design);
	assert(!stats.hasHull && stats.hullVolume == 0 && stats.crewRequired == 0);
	const Performance empty = CalculatePerformance(design, stats, 0);
	assert(empty.massLb == 0 && empty.totalHitPoints == 0 && !empty.canEngageFTL);
	assert(std::isfinite(empty.maneuverAccelerationFeetPerSecondSquared));

	// A 32-square-foot floor contributes exactly one 256-cubic-foot hull unit.
	design.rooms << Rectangle(1, 7);
	stats = CalculateStats(design);
	assert(stats.hullVolume == 256 && stats.hullContiguous);
	assert(stats.width == 8 && stats.length == 4 && stats.height == 16);
	const Performance bare = CalculatePerformance(design, stats, 0);
	assert(bare.hullMassLb == 200 && bare.structureHitPoints == 10);
	design.details << Equipment(2, 1) << Equipment(72, 1);
	stats = CalculateStats(design);
	assert(stats.crewBerths == 3 && stats.equipmentMass == 418);

	// Empty inspection records must not mask a valid room with the same ID.
	Room emptyRoom;
	emptyRoom.roomId = 1;
	emptyRoom.roomType = 19;
	design.rooms.prepend(emptyRoom);
	stats = CalculateStats(design);
	assert(stats.crewBerths == 3 && stats.passengerBerths == 0 && stats.hullVolume == 256);

	// Berth roles follow the referenced room ID, including rotated double berths.
	design.rooms << Rectangle(2, 4, 0, 0, 8);
	design.details << Equipment(31, 2) << Equipment(2, 999);
	stats = CalculateStats(design);
	assert(stats.officerBerths == 2 && stats.crewBerths == 3 && stats.passengerBerths == 0);
	assert(stats.equipmentMass == 946 && !stats.diagnostics.isEmpty());

	// Bays share room number across decks; transporters retain signed deck bytes.
	design.rooms << Rectangle(3, 21, 0, 4, 16) << Rectangle(4, 21, 1, 4, 16)
		<< Rectangle(5, 20, -1, 6) << Rectangle(6, 20, 0, 6);
	design.details << Equipment(23, 3) << Equipment(23, 4) << Equipment(99, 4)
		<< Equipment(20, 5) << Equipment(20, 6) << Equipment(20, 6);
	stats = CalculateStats(design);
	assert(stats.weaponBays.size() == 1 && stats.weaponBays[4].volume == 512);
	assert(stats.weaponBays[4].units == 2 && stats.weaponBays[4].emitters == 1);
	assert(stats.transporterRooms.size() == 2 && stats.transporterRooms[0xff06] == 1
		&& stats.transporterRooms[0x0006] == 2);

	// Sensor/shield stations also count as system units. Pilots and drivers are
	// separate from crew stations, and vehicle markers add the parked mass.
	design.details << Equipment(15, 1) << Equipment(17, 1)
		<< Equipment(57, 1, 133) << Equipment(64, 1, 8);
	stats = CalculateStats(design);
	assert(stats.sensorUnits == 1 && stats.shieldUnits == 1 && stats.crewStations == 2);
	assert(stats.drivers == 1 && stats.pilots == 1 && stats.parkingSpots == 2);

	Design cargo;
	cargo.header.scalarSlots = DefaultModuleSlots();
	cargo.rooms << Rectangle(1, 5) << Rectangle(2, 9, 0, 0, 8);
	const Stats cargoStats = CalculateStats(cargo);
	const Performance tl1 = CalculatePerformance(cargo, cargoStats, 0);
	const Performance tl21 = CalculatePerformance(cargo, cargoStats, 20);
	assert(tl1.holdCapacity == 85 && tl21.holdCapacity == 170);
	assert(tl1.fuelCapacity == 256 && tl21.fuelCapacity == 512);
	assert(tl1.massLb == tl21.massLb);
	assert(tl21.structureHitPoints == 2 * tl1.structureHitPoints);

	// The original capacitor comparison is strict: exactly enough power fails.
	Stats ftlStats;
	ftlStats.hullVolume = 64;
	ftlStats.ftlUnits = 5;
	ftlStats.powerPlantUnits = 2;
	const Performance ftlAtLimit = CalculatePerformance(cargo, ftlStats, 10);
	assert(ftlAtLimit.ftlUnitsRequired == 5 && ftlAtLimit.ftlEngagePower == 3);
	assert(ftlAtLimit.capacitorPower == 3 && !ftlAtLimit.canEngageFTL);
	ftlStats.powerPlantUnits = 3;
	assert(CalculatePerformance(cargo, ftlStats, 10).canEngageFTL);
	Design warp = cargo;
	warp.header.scalarSlots[1] = 0x104;
	const Performance warpAtMinimum = CalculatePerformance(warp, ftlStats, 0);
	assert(warpAtMinimum.techLevelUsed == 23 && warpAtMinimum.maxWarpFactor == 1);

	Design targets;
	targets.header.scalarSlots = DefaultModuleSlots();
	targets.rooms << Rectangle(1, 2) << Rectangle(2, 20, -1, 6)
		<< Rectangle(3, 20, 0, 6) << Rectangle(4, 21, 0, 4) << Rectangle(5, 21, 1, 4);
	targets.details << Equipment(15, 1) << Equipment(17, 1) << Equipment(16, 1)
		<< Equipment(20, 2) << Equipment(20, 3) << Equipment(20, 3)
		<< Equipment(23, 4) << Equipment(23, 5) << Equipment(11, 1)
		<< Equipment(11, 1) << Equipment(21, 1) << Equipment(40, 1) << Equipment(3, 1);
	for(int i = 0; i < targets.details.size(); ++i) targets.details[i].detailId = 100 - i;
	const Stats targetStats = CalculateStats(targets);
	const Performance targetPerformance = CalculatePerformance(targets, targetStats, 0);
	const auto target = [&](quint32 id) { return DesignTargetHealth(targets, targetStats, targetPerformance, id); };
	assert(target(100).system == 0 && target(100).which == 0 && target(100).hits == 1);
	assert(target(99).system == 0 && target(99).which == 1);
	assert(target(98).system == 8 && target(98).hits == 2);
	assert(target(97).system == 11 && target(97).which == 0xff06 && target(97).hits == 1);
	assert(target(96).which == 6 && target(96).hits == 2 && target(95).remaining == 2);
	assert(target(94).system == 13 && target(94).which == 4 && target(94).hits == 2);
	assert(target(93).system == 13 && target(93).which == 4 && target(93).remaining == 2);
	assert(target(92).system == 10 && target(92).which == 0 && target(92).hits == 1);
	assert(target(91).which == 1 && target(91).hits == 1);
	assert(target(90).system == 12 && target(90).which == 0);
	assert(target(89).system == 12 && target(89).which == 1);
	assert(target(88).system == 4 && target(0).system == 4);
	assert(target(999).system == 15 && target(999).hits == 0);
	assert(DesignTargetHealth(cargo, cargoStats, tl1, 0).hits == 0);
	assert(targets.details.first().detailId == 100 && targets.header.hullVolume == 0);

	// The selected TL changes performance but not the underlying design records.
	assert(cargo.header.minimumTechLevel == 0 && cargo.header.hullVolume == 0);
	assert(cargo.rooms.size() == 2 && cargo.details.isEmpty());
	std::cout << "Legacy ship stats regression fixtures passed.\n";
}
