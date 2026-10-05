#ifndef AuLegacyShipPerformance_H
#define AuLegacyShipPerformance_H

#include "aulegacyshipstats.h"

namespace AuLegacyShip
{
struct WeaponBayPerformance
{
	quint16 module = 0;
	double weaponPower = 0;
	double rangeFeet = 0;
	int cannons = 0;
	int ejectorRacks = 0;
	int launchers = 0;
	int projectors = 0;
	int capacitorShots = 0;
};

struct Performance
{
	// Internal TL is zero based; the original report displays TL + 1.
	int armorTechLevel = 0;
	int massTechLevel = 0;
	int moduleTechLevel = 0;
	quint16 moduleTechSource = 0;
	int minimumTechLevel = 0;
	int techLevelUsed = 0;
	int structure = 0;
	bool mustBeBuiltInSpace = false;
	quint16 maneuverModule = 0;
	quint16 ftlModule = 0;
	quint16 shieldModule = 0;
	double hullMassLb = 0;
	double fuelMassLb = 0;
	double equipmentMassLb = 0;
	double massLb = 0;
	double densityLbPerCubicFoot = 0;
	quint64 fuelCapacity = 0;
	quint64 holdCapacity = 0;
	quint64 structureHitPoints = 0;
	quint64 equipmentHitPoints = 0;
	quint64 totalHitPoints = 0;
	int lifeSupportRequired = 1;
	double maneuverAccelerationFeetPerSecondSquared = 0;
	double verticalAccelerationFeetPerSecondSquared = 0;
	double turnDegreesPerSecond = 0;
	double sensorRangeFeet = 0;
	double capacitorPower = 0;
	double powerOutputPerSecond = 0;
	double fuelBurnPerSecond = 0;
	double maneuverFuelPerSecond = 0;
	double lifeSupportPowerPerSecond = 0;
	double maneuverPowerPerSecond = 0;
	double sensorPowerPerSecond = 0;
	double shieldPowerPerSecond = 0;
	double idlePowerPerSecond = 0;
	double holdingPowerPerSecond = 0;
	double idlePowerWithShieldsPerSecond = 0;
	double holdingPowerWithShieldsPerSecond = 0;
	double shieldDissipationPerDamage = 0;
	double ftlUnitsRequired = 0;
	double ftlEngagePower = 0;
	int maxWarpFactor = 0;
	bool canEngageFTL = false;
	QMap<quint8, WeaponBayPerformance> weaponBays;
	QStringList diagnostics;
};

// Unlike modern quality, the original technology level gives a linear 5% boost.
double LegacyTechBoost(int techLevel);

QString LegacyStructureName(int structure);

// 2016 SYSTEM numbering (0..14); 15 denotes an unrecognized detail.
// Hit counts use internal system hits, not the report's tenfold HP units.
struct TargetHealth
{
	int system = 4;
	quint16 which = 0;
	quint64 hits = 0;
	quint64 remaining = 0;
};

TargetHealth DesignTargetHealth(const Design& design, const Stats& stats,
	const Performance& performance, quint32 detailId);
// Pure calculation from current records; techLevel is an internal zero-based TL.
Performance CalculatePerformance(const Design& design, const Stats& stats, int techLevel);
}

#endif // AuLegacyShipPerformance_H
