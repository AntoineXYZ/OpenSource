#include "aulegacyshipstatsreport.h"
#include "aulegacyshipstats.h"
#include "aulegacyshipperformance.h"
#include "aulegacyshipsave.h"
#include "aulegacyshipdetails.h"
#include "audef.h"
#include "auglobal.h"

#include <QLocale>
#include <cmath>

namespace
{
QString Number(double value, int precision = 0)
{
	return std::isfinite(value) ? QLocale().toString(value, 'f', precision) : QStringLiteral("Unavailable");
}

QString Volume(quint64 cubicFeet)
{
	return QStringLiteral("%1m\u00b3").arg(Number(std::ceil(double(cubicFeet) / CubicFeetPerCubicMeter())));
}

QString Mass(double pounds)
{
	return QStringLiteral("%1kg").arg(Number(std::ceil(pounds / PoundsPerKilogram())));
}

QString Power(double power)
{
	return QStringLiteral("%1 / sec").arg(Number(power, 6));
}

QString Endurance(double fuel, double fuelPerSecond, const QString& condition)
{
	if (fuelPerSecond <= 0.0)
		return QStringLiteral("No fuel demand");
	return QStringLiteral("%1 days at %2m\u00b3 H\u2082 per Real Day while %3")
		.arg(Number(fuel / fuelPerSecond / 86400.0, 2),
			Number(fuelPerSecond * 86400.0 / CubicFeetPerCubicMeter(), 2), condition);
}

QString StructureName(int structure)
{
	switch (structure)
	{
	case 1: return QStringLiteral("Starship");
	case 2: return QStringLiteral("Spaceship");
	case 3: return QStringLiteral("Starbase");
	case 4: return QStringLiteral("Spaceport");
	default: return QStringLiteral("Invalid Design");
	}
}

QString ModuleName(quint16 module)
{
	return g_Au.CommodityName(COMMODITYID(module));
}

void FormattedLine(QString& html, const QString& markup, int depth = 1, const QString& color = QString())
{
	html += QStringLiteral("<p style=\"margin: 0; margin-left: %1px;%2\">%3</p>")
		.arg(depth * 10).arg(color.isEmpty() ? QString() : QStringLiteral(" color: %1;").arg(color), markup);
}

void Line(QString& html, const QString& text, int depth = 1, const QString& color = QString())
{
	FormattedLine(html, text.toHtmlEscaped(), depth, color);
}

void Section(QString& html, const QString& title)
{
	html += QStringLiteral("<hr><p style=\"margin: 0;\"><b>%1</b></p>").arg(title.toUpper().toHtmlEscaped());
}

void Row(QString& html, const QString& label, const QString& value, int depth = 1)
{
	Line(html, QStringLiteral("%1 %2").arg(value, label), depth);
}

void Count(QString& html, const QString& label, int value, int depth = 1)
{
	Row(html, label, QLocale().toString(value), depth);
}

quint16 ScalarModule(const AuLegacyShip::Design& design, int slot)
{
	return slot < design.header.scalarSlots.size() ? design.header.scalarSlots.at(slot)
		: AuLegacyShip::DefaultModuleSlots().at(slot);
}

QString ModuleWithTech(quint16 module, int techLevel)
{
	return QStringLiteral("%1 TL%2").arg(ModuleName(module)).arg(techLevel + 1);
}

QString VehicleName(quint16 commodity)
{
	switch (commodity)
	{
	case 9: return QStringLiteral("Space transport");
	case 8: return QStringLiteral("Space fighter F1");
	case 231: return QStringLiteral("Space fighter F2");
	case 104: return QStringLiteral("APC");
	case 133: return QStringLiteral("Motorbike");
	case 82: return QStringLiteral("SUV");
	default: return QStringLiteral("Vehicle %1").arg(commodity);
	}
}
}

QString AuLegacyShipStatsReport(const AuLegacyShip::Design& design)
{
	const AuLegacyShip::Stats stats = AuLegacyShip::CalculateStats(design);
	const AuLegacyShip::Performance performance = AuLegacyShip::CalculatePerformance(design, stats, design.header.minimumTechLevel);
	if (!stats.hasHull)
		return QString();
	const int tl = performance.techLevelUsed;
	QString classification = StructureName(performance.structure).toUpper();
	if (design.header.streamline)
		classification.prepend(QStringLiteral("STREAMLINED "));
	if (!stats.hullContiguous)
		classification.prepend(QStringLiteral("DISJOINT "));
	QString html = QStringLiteral("<p style=\"margin: 0;\"><b>%1</b></p>").arg(classification.toHtmlEscaped());
	Line(html, QStringLiteral("Tech Level %1").arg(tl + 1));
	Line(html, QStringLiteral("Armor Level %1 = TL%2").arg(design.header.armor).arg(performance.armorTechLevel + 1), 2);
	Line(html, QStringLiteral("%1 Mass = TL%2").arg(Mass(performance.massLb)).arg(performance.massTechLevel + 1), 2);
	const QString equipment = stats.equipmentTechSource == 2
		? QStringLiteral("%1 Crew+Passenger+Troop Berths").arg(stats.crewBerths + stats.passengerBerths + stats.troopBerths)
		: AuLegacyShip::DetailName(stats.equipmentTechSource);
	Line(html, QStringLiteral("%1 = TL%2").arg(equipment).arg(stats.equipmentTechLevel + 1), 2);
	Line(html, QStringLiteral("%1 = TL%2").arg(ModuleName(performance.moduleTechSource)).arg(performance.moduleTechLevel + 1), 2);
	Line(html, QStringLiteral("Manufacture = TL Unavailable"), 2);
	Line(html, QStringLiteral("Estimated Cost Unavailable"));

	Section(html, QStringLiteral("HULL"));
	if (!stats.hasHelm) Line(html, QStringLiteral("Helm Needed"), 1, QStringLiteral("#ff2020"));
	if (stats.lifeSupportUnits < performance.lifeSupportRequired)
		Line(html, QStringLiteral("%1 Life Support Units Needed").arg(performance.lifeSupportRequired), 1, QStringLiteral("#ff2020"));
	if (!stats.maneuverUnits) Line(html, QStringLiteral("Gravity Drive Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.powerPlantUnits) Line(html, QStringLiteral("Power Plant Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.roomVolumes[9]) Line(html, QStringLiteral("Fuel Cell Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.hullOpenings && !stats.transporterPads)
		Line(html, QStringLiteral("Hull Opening or Transporter Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.hullContiguous)
		Line(html, QStringLiteral("Must Connect Separate Hull Pieces Together"), 1, QStringLiteral("#ff2020"));
	Line(html, ModuleWithTech(ScalarModule(design, 2), tl));
	Line(html, QStringLiteral("Armor Level %1 (Damage Absorption Unavailable)").arg(design.header.armor));
	Row(html, QStringLiteral("Volume"), Volume(stats.hullVolume));
	const bool station = performance.structure == 3 || performance.structure == 4;
	const auto dimension = [&](const QString& text, bool needsStation) {
		FormattedLine(html, text.toHtmlEscaped() + (needsStation
			? QStringLiteral(", <b>Must Spawn at Station</b>") : QString()), 2);
	};
	dimension(QStringLiteral("%1m Length").arg(Number(stats.length / FeetPerMeter(), 2)), station || stats.length > 119.);
	dimension(QStringLiteral("%1m Width").arg(Number(stats.width / FeetPerMeter(), 2)), station || stats.width > 119.);
	const int decks = stats.maximumDeck - stats.minimumDeck + 1;
	dimension(QStringLiteral("%1m Height, %2 Decks").arg(Number(stats.height / FeetPerMeter(), 2)).arg(decks), station || decks >= 6);
	Row(html, QStringLiteral("Mass"), Mass(performance.massLb));
	Row(html, QStringLiteral("Equipment"), Mass(performance.equipmentMassLb), 2);
	Row(html, QStringLiteral("Fuel"), Mass(performance.fuelMassLb), 2);
	Row(html, QStringLiteral("Hull"), Mass(performance.hullMassLb), 2);

	Section(html, QStringLiteral("Engineering"));
	Count(html, QStringLiteral("Maneuver Drive Units"), stats.maneuverUnits);
	Line(html, ModuleWithTech(performance.maneuverModule, tl), 2);
	Row(html, QStringLiteral("Acceleration"), QStringLiteral("%1 m/sec/sec")
		.arg(Number(std::ceil(performance.maneuverAccelerationFeetPerSecondSquared / FeetPerMeter()))), 2);
	Row(html, QStringLiteral("Vertical Acceleration"), QStringLiteral("%1 m/sec/sec")
		.arg(Number(std::ceil(performance.verticalAccelerationFeetPerSecondSquared / FeetPerMeter()))), 2);
	Row(html, QStringLiteral("Turn Rate"), QStringLiteral("%1\u00b0/sec").arg(Number(performance.turnDegreesPerSecond, 2)), 2);
	Count(html, QStringLiteral("Power Plant Units"), stats.powerPlantUnits);
	Line(html, ModuleWithTech(ScalarModule(design, 5), tl), 2);
	Line(html, QStringLiteral("Capacitor %1").arg(Number(performance.capacitorPower)), 2);
	Row(html, QStringLiteral("Power Output"), Power(performance.powerOutputPerSecond), 2);
	Row(html, QStringLiteral("Idle Demand"), Power(performance.idlePowerPerSecond), 2);
	Row(html, QStringLiteral("Idle with Shields"), Power(performance.idlePowerWithShieldsPerSecond), 2);
	if (performance.maneuverModule != 0xec)
	{
		Row(html, QStringLiteral("Holding Demand"), Power(performance.holdingPowerPerSecond), 2);
		Row(html, QStringLiteral("Holding with Shields"), Power(performance.holdingPowerWithShieldsPerSecond), 2);
	}
	if (stats.ftlUnits)
	{
		Count(html, QStringLiteral("FTL Drive Units"), stats.ftlUnits);
		Line(html, ModuleWithTech(performance.ftlModule, tl), 2);
		Row(html, QStringLiteral("FTL units needed"), Number(performance.ftlUnitsRequired, 2), 2);
		Row(html, QStringLiteral("FTL engagement power"), Number(performance.ftlEngagePower, 1), 2);
		Line(html, performance.canEngageFTL ? QStringLiteral("FTL Units and Capacitor Sufficient to Engage")
			: QStringLiteral("FTL Units or Capacitor Insufficient to Engage"), 2,
			performance.canEngageFTL ? QString() : QStringLiteral("#ff2020"));
		if (performance.maxWarpFactor)
			Line(html, QStringLiteral("Maximum Warp Factor %1").arg(performance.maxWarpFactor), 2);
	}

	Section(html, QStringLiteral("FUEL"));
	Line(html, QStringLiteral("Fuel Cell TL%1").arg(tl + 1));
	Row(html, QStringLiteral("Hydrogen (H\u2082)"), Volume(performance.fuelCapacity));
	Line(html, Endurance(performance.fuelCapacity,
		performance.idlePowerPerSecond / 10.0 + performance.maneuverFuelPerSecond, QStringLiteral("Idling")), 2);
	Line(html, QStringLiteral("Life Support, Maneuver Drive and Sensor Powered On"), 3);
	Line(html, Endurance(performance.fuelCapacity,
		performance.idlePowerWithShieldsPerSecond / 10.0 + performance.maneuverFuelPerSecond, QStringLiteral("Idling with Shields Up")), 2);
	Line(html, QStringLiteral("Life Support, Maneuver Drive, Sensor and Shields Powered On"), 3);
	if (performance.maneuverModule != 0xec)
	{
		Line(html, Endurance(performance.fuelCapacity, performance.holdingPowerPerSecond / 10.0, QStringLiteral("Holding")), 2);
		Line(html, QStringLiteral("Life Support and Sensor Powered On; Maneuver Drive on Hold"), 3);
		Line(html, Endurance(performance.fuelCapacity, performance.holdingPowerWithShieldsPerSecond / 10.0, QStringLiteral("Holding with Shields Up")), 2);
		Line(html, QStringLiteral("Life Support, Sensor and Shields Powered On; Maneuver Drive on Hold"), 3);
	}

	Section(html, QStringLiteral("AVIONICS"));
	Line(html, ModuleWithTech(ScalarModule(design, 0), tl));
	Count(html, QStringLiteral("Consoles"), stats.consoles);
	Count(html, QStringLiteral("Game Tables"), stats.gameTables);
	Count(html, QStringLiteral("Life Support Units"), stats.lifeSupportUnits);
	Line(html, ModuleWithTech(ScalarModule(design, 3), tl), 2);
	Count(html, QStringLiteral("Life Support Units Needed"), performance.lifeSupportRequired, 2);
	Row(html, QStringLiteral("Life Support Power"), Power(performance.lifeSupportPowerPerSecond), 2);
	Count(html, QStringLiteral("Sensor Units"), stats.sensorUnits);
	Line(html, ModuleWithTech(ScalarModule(design, 6), tl), 2);
	Row(html, QStringLiteral("Sensor Range"), QStringLiteral("%1m").arg(Number(performance.sensorRangeFeet / FeetPerMeter(), 1)), 2);
	Row(html, QStringLiteral("Sensor Power"), Power(performance.sensorPowerPerSecond), 2);
	Count(html, QStringLiteral("Shield Units"), stats.shieldUnits);
	Line(html, ModuleWithTech(performance.shieldModule, tl), 2);
	Row(html, QStringLiteral("Shield Power"), Power(performance.shieldPowerPerSecond), 2);

	Count(html, QStringLiteral("Surgery Units"), stats.surgeries);
	Count(html, QStringLiteral("Transporter Pads"), stats.transporterPads);
	if (!stats.transporterRooms.isEmpty())
	{
		for (auto room = stats.transporterRooms.cbegin(); room != stats.transporterRooms.cend(); ++room)
		{
			const int deckByte = room.key() >> 8;
			const int deck = deckByte < 128 ? deckByte : deckByte - 256;
			Row(html, QStringLiteral("Deck %1, room %2").arg(deck).arg((room.key() & 0xff) + 1),
				QStringLiteral("%1 pads").arg(room.value()), 2);
		}
	}
	Section(html, QStringLiteral("ARMAMENT"));
	Count(html, QStringLiteral("Turrets"), stats.turrets);
	if (!stats.weaponBays.isEmpty())
	{
		for (auto bay = stats.weaponBays.cbegin(); bay != stats.weaponBays.cend(); ++bay)
		{
			Row(html, QStringLiteral("Bay %1").arg(int(bay.key()) + 1), Volume(bay.value().volume));
			Row(html, QStringLiteral("Units / emitters"), QStringLiteral("%1 / %2").arg(bay.value().units).arg(bay.value().emitters), 2);
			const auto firing = performance.weaponBays.constFind(bay.key());
			if (firing != performance.weaponBays.constEnd())
			{
				Line(html, ModuleWithTech(firing.value().module, tl), 2);
				if (firing.value().rangeFeet <= 0.0)
				{
					Row(html, QStringLiteral("Weapon performance"), QStringLiteral("Unavailable for this module"), 2);
					continue;
				}
				Row(html, QStringLiteral("Targeting range"), QStringLiteral("%1 m").arg(Number(firing.value().rangeFeet / FeetPerMeter(), 1)), 2);
				Row(html, QStringLiteral("Weapon output power"), Number(firing.value().weaponPower, 1), 2);
				if (firing.value().module == 0xfc || firing.value().module == 0x100
					|| firing.value().module == 0x101 || firing.value().module == 0x11b
					|| firing.value().module == 0x121 || firing.value().module == 0x123)
					Count(html, QStringLiteral("Full-power capacitor shots"), firing.value().capacitorShots, 2);
				switch (firing.value().module)
				{
				case 0x105: Count(html, QStringLiteral("Cannon barrels"), firing.value().cannons, 2); break;
				case 0xfc: Count(html, QStringLiteral("Beam projectors"), firing.value().projectors, 2); break;
				case 0xfe: Count(html, QStringLiteral("Launchers"), firing.value().launchers, 2); break;
				case 0x13f: Count(html, QStringLiteral("Ejector racks"), firing.value().ejectorRacks, 2); break;
				default: break;
				}
			}
		}
	}
	Section(html, QStringLiteral("CREW"));
	Count(html, QStringLiteral("Officer Stations"), stats.officerStations);
	Count(html, QStringLiteral("Crew Stations"), stats.crewStations);
	Count(html, QStringLiteral("Drivers"), stats.drivers);
	Count(html, QStringLiteral("Pilots"), stats.pilots);
	Count(html, QStringLiteral("Turret Gunners"), stats.turrets);
	Count(html, QStringLiteral("Guards"), stats.troopStations);
	Count(html, QStringLiteral("Crew Required"), stats.crewRequired);
	Section(html, QStringLiteral("QUARTERS"));
	Count(html, QStringLiteral("Officer Berths"), stats.officerBerths);
	Count(html, QStringLiteral("Crew Berths"), stats.crewBerths);
	const QString passengerClass = stats.passengerClass == 1 ? QStringLiteral("1st Class")
		: stats.passengerClass == 2 ? QStringLiteral("2nd Class") : QStringLiteral("3rd Class");
	Count(html, QStringLiteral("%1 Passenger Berths").arg(passengerClass), stats.passengerBerths);
	Count(html, QStringLiteral("Troop Berths"), stats.troopBerths);
	Count(html, QStringLiteral("Troop Stations"), stats.troopStations);
	Row(html, QStringLiteral("Galley / lounge"), QStringLiteral("%1 / %2")
		.arg(stats.hasGalley ? QStringLiteral("Yes") : QStringLiteral("No"), stats.hasLounge ? QStringLiteral("Yes") : QStringLiteral("No")));

	Section(html, QStringLiteral("HOLD"));
	Row(html, QStringLiteral("Storage Space"), Volume(stats.roomVolumes[5]));
	Line(html, QStringLiteral("Room for %1 Commodities").arg(QLocale().toString(qulonglong(performance.holdCapacity))));

	Section(html, QStringLiteral("VEHICLE BAYS"));
	Row(html, QStringLiteral("Vehicle Bays"), Volume(stats.roomVolumes[13]));
	if (!stats.vehicleComplement.isEmpty())
	{
		for (auto vehicle = stats.vehicleComplement.cbegin(); vehicle != stats.vehicleComplement.cend(); ++vehicle)
			Count(html, VehicleName(vehicle.key()), vehicle.value());
	}
	Section(html, QStringLiteral("Damage control"));
	Row(html, QStringLiteral("Total Hit Points"), QLocale().toString(qulonglong(performance.totalHitPoints)));
	Row(html, QStringLiteral("Structure Hit Points"), QLocale().toString(qulonglong(performance.structureHitPoints)), 2);
	Row(html, QStringLiteral("Equipment Hit Points"), QLocale().toString(qulonglong(performance.equipmentHitPoints)), 2);
	Count(html, QStringLiteral("Operable Doors and Hatches"), stats.operableDoors);
	Count(html, QStringLiteral("Hull Openings"), stats.hullOpenings);
	Section(html, QStringLiteral("MANUFACTURE"));
	Line(html, QStringLiteral("Manufacturing material requirements, costs and TL are unavailable. "
		"Manufacturing may require a higher TL than the calculated systems TL."));
	QStringList diagnostics = performance.diagnostics;
	diagnostics.append(stats.diagnostics);
	diagnostics.removeDuplicates();
	if (!diagnostics.isEmpty())
	{
		html += QStringLiteral("<hr>");
		for (const QString& message : diagnostics)
			Line(html, message, 1, QStringLiteral("#ffff20"));
	}
	return html;
}
