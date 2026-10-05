#include "aulegacyshipperformance.h"

#include "aulegacyshipsave.h"
#include "auscdesign.h"
#include <QSet>
#include <algorithm>
#include <cmath>

namespace
{
using namespace AuLegacyShip;

quint16 ScalarModule(const Design& design, int slot)
{
	return slot < design.header.scalarSlots.size() ? design.header.scalarSlots.at(slot)
		: DefaultModuleSlots().at(slot);
}

quint16 MappedModule(const QVector<ModulePair>& modules, quint16 key, quint16 fallback)
{
	for(const ModulePair& pair : modules)
		if(pair.first == key) return pair.secondNormalized;
	return fallback;
}

void RequireModule(Performance& result, quint16 module, int techLevel)
{
	if(techLevel > result.moduleTechLevel)
	{
		result.moduleTechLevel = techLevel;
		result.moduleTechSource = module;
	}
}

void ModuleTechLevel(const Design& design, const Stats& stats, Performance& result)
{
	// AuSpaceShipModules::HighestTechLevel, AuData13.dll:0x18017a3e0.
	// Like the original map reader, the first value wins for duplicate keys.
	const quint16 hullModule = ScalarModule(design, 2);
	result.moduleTechSource = ScalarModule(design, 0);
	if(hullModule == 0x118) RequireModule(result, hullModule, 9);
	else if(hullModule == 0x119) RequireModule(result, hullModule, 18);
	QSet<quint16> transporterKeys;
	for(const ModulePair& pair : design.header.secondPairs)
	{
		if(transporterKeys.contains(pair.first)) continue;
		transporterKeys.insert(pair.first);
		const quint16 module = pair.secondNormalized;
		if(module == 0xf6) RequireModule(result, module, 6);
		else if(module == 0xf7) RequireModule(result, module, 13);
	}
	QSet<quint16> bayKeys;
	for(const ModulePair& pair : design.header.firstPairs)
	{
		if(bayKeys.contains(pair.first)) continue;
		bayKeys.insert(pair.first);
		const quint16 module = pair.secondNormalized;
		switch(module)
		{
		case 0xfc: case 0xfd: RequireModule(result, module, 3); break;
		case 0xfe: case 0xff: case 0x101: case 0x105: case 0x121: case 0x13f:
			RequireModule(result, module, 1); break;
		case 0x100: RequireModule(result, module, 5); break;
		case 0x102: RequireModule(result, module, 7); break;
		case 0x11b: RequireModule(result, module, 23); break;
		case 0x123: RequireModule(result, module, 2); break;
		}
	}
	for(const Detail& detail : design.details)
	{
		const int type = detail.detailType;
		if(type != 11 && type != 21 && type != 22 && (type < 33 || type > 40)) continue;
		const quint16 module = detail.commodityIdNormalized;
		if(module == 0xf3) RequireModule(result, module, 2);
		else if(module == 0xf4) RequireModule(result, module, 12);
		else if(module == 0xf5) RequireModule(result, module, 21);
	}
	if(stats.ftlUnits && result.ftlModule == 0x104) RequireModule(result, result.ftlModule, 23);
	if(stats.shieldUnits)
	{
		if(result.shieldModule == 0xf0) RequireModule(result, result.shieldModule, 3);
		else if(result.shieldModule == 0xf1) RequireModule(result, result.shieldModule, 11);
		else if(result.shieldModule == 0xf2) RequireModule(result, result.shieldModule, 27);
	}
}
}

double AuLegacyShip::LegacyTechBoost(int techLevel)
{
	return float(std::max(0, std::min(255, techLevel))) * .05f + 1.f;
}

AuLegacyShip::TargetHealth AuLegacyShip::DesignTargetHealth(const Design& design,
	const Stats& stats, const Performance& performance, quint32 detailId)
{
	TargetHealth target;
	if(detailId)
	{
		target.system = 15;
		quint16 console = 0, surgery = 0, turret = 0;
		// DetailSystem counts consoles, surgery units and turrets separately in
		// the design's stored order, not by numeric detail ID or console role.
		for(const Detail& detail : design.details)
		{
			int system = 15;
			quint16 which = 0;
			const int type = detail.detailType;
			switch(type)
			{
			case 1: case 4: case 6: case 7: case 12: case 13: case 15: case 17: case 19: case 73:
				system = 0; which = console++; break;
			case 10: system = 2; break;
			case 11: system = 10; which = surgery++; break;
			case 14: system = 7; break;
			case 16: system = 8; break;
			case 18: system = 9; break;
			case 20: case 23:
				for(const Room& room : design.rooms)
					if(room.roomId == detail.roomId)
					{
						system = type == 20 ? 11 : 13;
						which = type == 20 ? quint16(quint16(quint8(room.deck)) << 8) | room.roomSubtype
							: room.roomSubtype;
						break;
					}
				break;
			case 21: case 22: case 33: case 34: case 35: case 36: case 37: case 38: case 39: case 40:
				system = 12; which = turret++; break;
			case 24: system = 14; break;
			case 32: system = 6; break;
			default:
				if(type == 3 || type == 5 || type == 8 || type == 9 || (type >= 49 && type <= 64)
					|| (type >= 77 && type <= 97) || (type >= 101 && type <= 140)) system = 4;
				break;
			}
			if(detail.detailId == detailId)
			{
				target.system = system;
				target.which = which;
				break;
			}
		}
	}
	// AuAtom13:1801b85a0, SystemHits. HullHits subtracts separately rounded
	// fuel/hold/vehicle structure hits from the total (AuData13:1800733a0).
	float factor = float(design.header.armor + 1);
	if(design.header.streamline) factor *= 1.1f;
	factor *= float(LegacyTechBoost(performance.techLevelUsed));
	const quint16 hull = ScalarModule(design, 2);
	if(hull == 0x119) factor *= 1.5f;
	else if(hull == 0x118) factor *= 1.25f;
	const auto structureHits = [factor](quint64 volume) -> quint64 {
		return volume ? std::max<quint64>(1, quint64(factor * float(volume) * .00390625f)) : 0;
	};
	switch(target.system)
	{
	case 0: case 10: case 12: target.hits = 1; break;
	case 1: target.hits = structureHits(stats.roomVolumes[9]); break;
	case 2: target.hits = stats.maneuverUnits; break;
	case 3: target.hits = structureHits(stats.roomVolumes[5]); break;
	case 4:
	{
		const quint64 total = structureHits(stats.hullVolume);
		const quint64 storage = structureHits(stats.roomVolumes[9]) + structureHits(stats.roomVolumes[5])
			+ structureHits(stats.roomVolumes[13]);
		target.hits = total > storage ? total - storage : 0;
		break;
	}
	case 5: target.hits = structureHits(stats.roomVolumes[13]); break;
	case 6: target.hits = stats.lifeSupportUnits; break;
	case 7: target.hits = stats.powerPlantUnits; break;
	case 8: target.hits = stats.sensorUnits; break;
	case 9: target.hits = stats.shieldUnits; break;
	case 11: target.hits = stats.transporterRooms.value(target.which); break;
	case 13: target.hits = stats.weaponBays.value(quint8(target.which)).units; break;
	case 14: target.hits = stats.ftlUnits; break;
	default: break;
	}
	// Authoring designs have no damage state. Keep actual subsystem capacity
	// rather than inventing damage or displaying total ship HP for every target.
	target.remaining = target.hits;
	return target;
}

AuLegacyShip::Performance AuLegacyShip::CalculatePerformance(const Design& design, const Stats& stats, int techLevel)
{
	Performance result;
	result.maneuverModule = ScalarModule(design, 4);
	result.ftlModule = ScalarModule(design, 1);
	result.shieldModule = ScalarModule(design, 7);
	ModuleTechLevel(design, stats, result);

	// Reuse the modern physical constants and hull implementation. The 2016
	// density calculation applied the same atmospheric factor to all streamlining.
	float hullDensity = float(design.header.armor + 1);
	if(design.header.streamline) hullDensity *= 1.1f;
	hullDensity *= float(AuScDesign::HullDensity(0, SLINE_None));
	result.hullMassLb = hullDensity * float(stats.hullVolume);
	// Original single-precision constants differ slightly from modern doubles.
	result.fuelMassLb = 4.369957f * float(stats.roomVolumes[9]);
	result.equipmentMassLb = double(stats.equipmentMass);
	result.massLb = float(float(result.hullMassLb) + float(result.fuelMassLb)) + float(stats.equipmentMass);
	if(stats.hullVolume) result.densityLbPerCubicFoot = float(result.massLb) / float(stats.hullVolume);
	result.armorTechLevel = design.header.armor ? design.header.armor - 1 : 0;
	result.massTechLevel = int(std::min(255.f, float(result.massLb) / 771617.94f));
	result.minimumTechLevel = std::max({result.armorTechLevel, result.massTechLevel,
		result.moduleTechLevel, stats.equipmentTechLevel});
	result.techLevelUsed = std::max(result.minimumTechLevel, std::max(0, std::min(255, techLevel)));
	const float boost = float(LegacyTechBoost(result.techLevelUsed));

	// Preserve integer truncation in the original capacity and berth equations.
	const quint64 fuelVolume = stats.roomVolumes[9];
	const quint64 holdVolume = stats.roomVolumes[5];
	result.fuelCapacity = fuelVolume + fuelVolume * quint64(result.techLevelUsed) * 5 / 100;
	const quint64 holdBase = holdVolume / 3;
	result.holdCapacity = holdVolume < 3 ? (holdVolume ? 1 : 0)
		: holdBase + holdBase * quint64(result.techLevelUsed) * 5 / 100;
	const int berths = stats.crewBerths + stats.officerBerths + stats.passengerBerths + stats.troopBerths;
	result.lifeSupportRequired = std::max(1, berths * 100 / ((result.techLevelUsed + 20) * 5));

	const float unitForce = 8137140.f;
	float baseAcceleration = 0;
	if(stats.hullVolume && result.massLb > 0)
		baseAcceleration = float(stats.maneuverUnits) * unitForce / float(result.massLb);
	result.maneuverAccelerationFeetPerSecondSquared = baseAcceleration * boost;
	result.verticalAccelerationFeetPerSecondSquared = result.maneuverAccelerationFeetPerSecondSquared * .16;
	result.turnDegreesPerSecond = float(result.maneuverAccelerationFeetPerSecondSquared) / 25.f;
	// Modern orbital distances changed; keep the original 42,500-foot moon zone.
	result.sensorRangeFeet = boost * float(stats.sensorUnits) * 42500.f * .5f;
	result.capacitorPower = float(stats.powerPlantUnits) * boost;
	const float unitRate = float(UDaysPerRealDay()) / 86400.f;
	result.fuelBurnPerSecond = float(stats.powerPlantUnits) * .007777778f;
	result.powerOutputPerSecond = float(result.fuelBurnPerSecond) * float(AuScDesign::PowerPerFuel());
	result.lifeSupportPowerPerSecond = float(stats.lifeSupportUnits) * unitRate;
	if(result.maneuverModule == 0xec)
		result.maneuverFuelPerSecond = float(stats.maneuverUnits) * unitRate / float(AuScDesign::PowerPerFuel());
	else
		result.maneuverPowerPerSecond = float(stats.maneuverUnits) * unitRate;
	result.sensorPowerPerSecond = float(stats.sensorUnits) * unitRate;
	result.shieldPowerPerSecond = float(stats.shieldUnits) * unitRate;
	result.idlePowerPerSecond = float(result.lifeSupportPowerPerSecond)
		+ float(result.maneuverPowerPerSecond) + float(result.sensorPowerPerSecond);
	result.holdingPowerPerSecond = float(result.lifeSupportPowerPerSecond)
		+ float(result.maneuverPowerPerSecond) / 10.f + float(result.sensorPowerPerSecond);
	result.idlePowerWithShieldsPerSecond = float(result.idlePowerPerSecond) + float(result.shieldPowerPerSecond);
	result.holdingPowerWithShieldsPerSecond = float(result.holdingPowerPerSecond) + float(result.shieldPowerPerSecond);
	result.shieldDissipationPerDamage = 1.f / (boost * 10.f);
	// The client reports ten damage points for each internal hit. Hull material
	// changes durability without changing mass or the structure classification.
	float hitFactor = float(design.header.armor + 1);
	if(design.header.streamline) hitFactor *= 1.1f;
	hitFactor *= boost;
	const quint16 hullModule = ScalarModule(design, 2);
	if(hullModule == 0x119) hitFactor *= 1.5f;
	else if(hullModule == 0x118) hitFactor *= 1.25f;
	if(stats.hullVolume)
		result.structureHitPoints = std::max<quint64>(1,
			quint64(hitFactor * float(stats.hullVolume) * .00390625f)) * 10;
	result.equipmentHitPoints = quint64(stats.consoles + stats.surgeries + stats.turrets
		+ stats.transporterPads + stats.lifeSupportUnits + stats.maneuverUnits + stats.ftlUnits
		+ stats.powerPlantUnits + stats.sensorUnits + stats.shieldUnits + stats.weaponUnits) * 10;
	result.totalHitPoints = result.structureHitPoints + result.equipmentHitPoints;

	if(stats.hullVolume)
	{
		result.ftlUnitsRequired = std::round(std::pow(float(stats.hullVolume) * .75f * 3.1415927f, .33333334f));
		result.ftlEngagePower = float(AuScDesign::PowerNeededToEngageFTLDrive(result.ftlUnitsRequired * 256.)) / boost;
		const bool capacitorEnough = result.ftlEngagePower < result.capacitorPower;
		result.canEngageFTL = stats.ftlUnits >= result.ftlUnitsRequired && capacitorEnough;
		if(result.ftlModule == 0x104 && result.canEngageFTL && result.ftlEngagePower > 0)
			result.maxWarpFactor = std::max(0, 1 + std::min(result.techLevelUsed - 23,
				int((float(result.capacitorPower) / float(result.ftlEngagePower) - 1.f) * .5f)));
	}

	// Structure ignores the chosen TL when classifying acceleration, and one
	// drive unit always denotes a station. A transporter can replace a hull exit.
	if(fuelVolume && stats.hasHelm && stats.maneuverUnits && stats.powerPlantUnits
		&& stats.lifeSupportUnits >= result.lifeSupportRequired && (stats.transporterPads || stats.hullOpenings))
		result.structure = stats.maneuverUnits != 1 && baseAcceleration >= 100.f
			? (stats.ftlUnits ? 1 : 2) : (stats.ftlUnits ? 3 : 4);
	result.mustBeBuiltInSpace = result.structure == 3 || result.structure == 4
		|| (stats.hasHull && stats.maximumDeck - stats.minimumDeck + 1 >= 6)
		|| stats.width > 119. || stats.length > 119.;
	QMap<quint16, const Room*> roomsById;
	for(const Room& room : design.rooms)
		if(room.hasWorkingBoundary ? !room.workingBoundary.isEmpty() : !room.packedVertices.isEmpty())
			if(!roomsById.contains(room.roomId)) roomsById.insert(room.roomId, &room);
	std::array<bool, 256> hasConsole{};
	QSet<quint16> medicStations;
	QSet<quint16> surgeryRooms;
	QSet<quint16> transporterStations;
	for(const Detail& detail : design.details)
	{
		const Room* room = roomsById.value(detail.roomId, nullptr);
		if(!room) continue;
		hasConsole[detail.detailType] = true;
		const quint16 key = quint16(quint16(quint8(room->deck)) << 8) | room->roomSubtype;
		if(detail.detailType == 4) medicStations.insert(key);
		else if(detail.detailType == 11) surgeryRooms.insert(key);
		else if(detail.detailType == 19) transporterStations.insert(key);
	}

	for(auto it = stats.weaponBays.cbegin(); it != stats.weaponBays.cend(); ++it)
	{
		WeaponBayPerformance bay;
		bay.module = MappedModule(design.header.firstPairs, it.key(), 0x105);
		const int units = it.value().units;
		bay.weaponPower = boost * float(units * 10);
		const double volume = double(units) * 256.;
		bay.cannons = int(AuScDesign::BayWeaponCannons(volume));
		bay.ejectorRacks = int(AuScDesign::BayWeaponEjectorRacks(volume));
		bay.launchers = int(AuScDesign::BayWeaponLaunchers(quint32(volume)));
		bay.projectors = int(AuScDesign::BayWeaponProjectors(quint32(volume)));
		switch(bay.module)
		{
		case 0xfc:
			bay.rangeFeet = boost * 85000.f;
			if(bay.projectors) bay.capacitorShots = int(result.capacitorPower) / bay.projectors;
			if(bay.projectors && !bay.capacitorShots)
				result.diagnostics.append(QStringLiteral("Weapon bay %1 has inadequate capacitor power to fire its energy weapons at full power.")
					.arg(int(it.key()) + 1));
			break;
		case 0xfe: case 0x105: case 0x13f:
			bay.rangeFeet = boost * 85000.f;
			break;
		case 0x100: case 0x101: case 0x11b: case 0x121: case 0x123:
			bay.rangeFeet = float(std::sqrt(float(units * 10))) * 100.f * 3.28084f * boost;
			if(units) bay.capacitorShots = int(result.capacitorPower) / units;
			if(!units)
				result.diagnostics.append(QStringLiteral("Weapon bay %1 needs weapon units to fire its harvesting system.").arg(int(it.key()) + 1));
			else if(!bay.capacitorShots)
				result.diagnostics.append(QStringLiteral("Weapon bay %1 has inadequate capacitor power to fire its harvesting system at full power.")
					.arg(int(it.key()) + 1));
			break;
		}
		result.weaponBays.insert(it.key(), bay);
	}
	if(!stats.hasHelm) result.diagnostics.append(QStringLiteral("A helm is needed."));
	if(stats.lifeSupportUnits < result.lifeSupportRequired)
		result.diagnostics.append(QStringLiteral("%1 life support units are required for the installed berths; %2 are installed.")
			.arg(result.lifeSupportRequired).arg(stats.lifeSupportUnits));
	if(!stats.maneuverUnits) result.diagnostics.append(QStringLiteral("A maneuver drive is needed."));
	if(!stats.powerPlantUnits) result.diagnostics.append(QStringLiteral("A power plant is needed."));
	if(!fuelVolume) result.diagnostics.append(QStringLiteral("A fuel cell is needed."));
	if(!stats.transporterPads && !stats.hullOpenings)
		result.diagnostics.append(QStringLiteral("A hull opening or transporter pad is needed."));
	if(stats.hasHull && !stats.hullContiguous)
		result.diagnostics.append(QStringLiteral("Separate hull pieces must be connected."));
	if(result.techLevelUsed > 31)
		result.diagnostics.append(QStringLiteral("The design requires TL%1, above the original maximum TL32.").arg(result.techLevelUsed + 1));
	if(stats.powerPlantUnits && result.idlePowerWithShieldsPerSecond > result.powerOutputPerSecond)
		result.diagnostics.append(QStringLiteral("Power plant output is below idle demand with shields powered."));
	if(stats.ftlUnits)
	{
		if(stats.ftlUnits < result.ftlUnitsRequired)
			result.diagnostics.append(QStringLiteral("%1 FTL drive units are needed to engage; %2 are installed.")
				.arg(result.ftlUnitsRequired, 0, 'f', 0).arg(stats.ftlUnits));
		if(result.capacitorPower <= result.ftlEngagePower)
			result.diagnostics.append(QStringLiteral("The power plant capacitor is inadequate to engage the FTL drive."));
		if(!hasConsole[6])
			result.diagnostics.append(QStringLiteral("An engineer station is needed to operate the FTL drive."));
	}
	if(stats.sensorUnits && !hasConsole[15])
		result.diagnostics.append(QStringLiteral("A sensor station is needed to operate the sensors."));
	if(stats.shieldUnits && !hasConsole[17])
		result.diagnostics.append(QStringLiteral("A shield station is needed to operate the shields."));
	// The original report matches these stations by signed deck byte and room
	// number, permitting several systems while preserving independent controls.
	const auto roomDescription = [](quint16 key) {
		const int deck = key >> 8;
		return QStringLiteral("room %1 on deck %2").arg((key & 0xff) + 1).arg(deck < 128 ? deck : deck - 256);
	};
	for(quint16 key : surgeryRooms)
		if(!medicStations.contains(key))
			result.diagnostics.append(QStringLiteral("A medic station is needed to operate the surgery unit in %1.").arg(roomDescription(key)));
	for(quint16 key : medicStations)
		if(!surgeryRooms.contains(key))
			result.diagnostics.append(QStringLiteral("The medic station for %1 has no surgery unit to operate.").arg(roomDescription(key)));
	for(auto it = stats.transporterRooms.cbegin(); it != stats.transporterRooms.cend(); ++it)
		if(!transporterStations.contains(it.key()))
			result.diagnostics.append(QStringLiteral("A transporter station is needed to operate the pads in %1.").arg(roomDescription(it.key())));
	for(quint16 key : transporterStations)
		if(!stats.transporterRooms.contains(key))
			result.diagnostics.append(QStringLiteral("The transporter station for %1 has no pads to operate.").arg(roomDescription(key)));
	if(!stats.weaponBays.isEmpty() && !hasConsole[7])
		result.diagnostics.append(QStringLiteral("A fire control station is needed to operate the weapon bays."));
	if(hasConsole[7] && stats.weaponBays.isEmpty())
		result.diagnostics.append(QStringLiteral("The fire control station has no weapon bay to operate."));
	if(stats.transporterPads && !stats.sensorUnits)
		result.diagnostics.append(QStringLiteral("Sensors are needed to lock onto transporter targets."));
	if(!stats.weaponBays.isEmpty() && !stats.sensorUnits)
		result.diagnostics.append(QStringLiteral("Sensors are needed to lock onto weapon bay targets."));
	if(stats.officerBerths < stats.officerStations)
		result.diagnostics.append(QStringLiteral("%1 officer berths are recommended for the officer stations.").arg(stats.officerStations));
	if(stats.crewBerths < stats.crewStations)
		result.diagnostics.append(QStringLiteral("%1 crew berths are recommended for the crew stations.").arg(stats.crewStations));
	if(stats.troopBerths < stats.turrets)
		result.diagnostics.append(QStringLiteral("%1 troop berths are recommended for the turrets.").arg(stats.turrets));
	return result;
}
