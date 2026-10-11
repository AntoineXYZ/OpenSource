#include "aulegacyscstats.h"
#include "aulegacyshipcommoditydata.h"
#include "aulegacyshipsave.h"

#include <QIODevice>
#include <QSet>
#include <cstddef>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
constexpr quint32 SnapshotMagic = 0x4c535431; // LST1
constexpr quint8 SnapshotVersion = 1;
constexpr quint32 MaximumWeaponBays = 256;
constexpr quint32 MaximumTransporterRooms = 512;
constexpr quint32 MaximumModuleKeys = 65536;
constexpr quint32 MaximumVehicleTypes = 65536;

template<typename Value>
bool Finite(const Value&)
{
	return true;
}

bool Finite(double value) { return std::isfinite(value); }
bool Finite(float value) { return std::isfinite(value); }
bool Finite(const QPointF& value) { return Finite(value.x()) && Finite(value.y()); }

class Writer
{
public:
	explicit Writer(QDataStream& stream) : m_Stream(stream) {}
	bool Fields() { return m_Stream.status() == QDataStream::Ok; }
	template<typename Value, typename... Rest>
	bool Fields(const Value& value, const Rest&... rest)
	{
		m_Stream << value;
		return Fields(rest...);
	}
	template<typename Value, std::size_t Size>
	bool Array(const std::array<Value, Size>& values)
	{
		for(const Value& value : values)
			if(!Fields(value)) return false;
		return true;
	}
	template<typename Key, typename Value, typename Transfer>
	bool Map(const QMap<Key, Value>& values, quint32 limit, const Transfer& transfer)
	{
		if(quint32(values.size()) > limit)
		{
			m_Stream.setStatus(QDataStream::WriteFailed);
			return false;
		}
		if(!Fields(quint32(values.size()))) return false;
		for(auto it = values.constBegin(); it != values.constEnd(); ++it)
			if(!Fields(it.key()) || !transfer(*this, it.value())) return false;
		return true;
	}
	template<typename Value, typename Transfer>
	bool Vector(const QVector<Value>& values, quint32 limit, const Transfer& transfer)
	{
		if(quint32(values.size()) > limit)
		{
			m_Stream.setStatus(QDataStream::WriteFailed);
			return false;
		}
		if(!Fields(quint32(values.size()))) return false;
		for(const Value& value : values)
			if(!transfer(*this, value)) return false;
		return true;
	}
private:
	QDataStream& m_Stream;
};

class Reader
{
public:
	explicit Reader(QDataStream& stream) : m_Stream(stream) {}
	bool Fields() { return m_Stream.status() == QDataStream::Ok; }
	template<typename Value, typename... Rest>
	bool Fields(Value& value, Rest&... rest)
	{
		m_Stream >> value;
		if(!Finite(value))
			return Corrupt();
		return Fields(rest...);
	}
	template<typename Value, std::size_t Size>
	bool Array(std::array<Value, Size>& values)
	{
		for(Value& value : values)
			if(!Fields(value)) return false;
		return true;
	}
	template<typename Key, typename Value, typename Transfer>
	bool Map(QMap<Key, Value>& values, quint32 limit, const Transfer& transfer)
	{
		quint32 count = 0;
		if(!Fields(count)) return false;
		if(count > limit) return Corrupt();
		for(quint32 index = 0; index < count; ++index)
		{
			Key key{};
			Value value{};
			if(!Fields(key) || !transfer(*this, value)) return false;
			if(values.contains(key)) return Corrupt();
			values.insert(key, std::move(value));
		}
		return true;
	}
	template<typename Value, typename Transfer>
	bool Vector(QVector<Value>& values, quint32 limit, const Transfer& transfer)
	{
		quint32 count = 0;
		if(!Fields(count)) return false;
		if(count > limit) return Corrupt();
		for(quint32 index = 0; index < count; ++index)
		{
			Value value{};
			if(!transfer(*this, value)) return false;
			values.append(std::move(value));
		}
		return true;
	}
private:
	bool Corrupt()
	{
		m_Stream.setStatus(QDataStream::ReadCorruptData);
		return false;
	}
	QDataStream& m_Stream;
};

struct ScalarFields
{
	template<typename Archive, typename Value>
	bool operator()(Archive& archive, Value& value) const { return archive.Fields(value); }
};

struct ModuleFields
{
	template<typename Archive, typename Value>
	bool operator()(Archive& archive, Value& value) const
	{
		return archive.Fields(value.first, value.secondRaw, value.secondNormalized);
	}
};

struct BayQuantityFields
{
	template<typename Archive, typename Value>
	bool operator()(Archive& archive, Value& value) const
	{
		return archive.Fields(value.volume, value.units, value.emitters);
	}
};

struct BayPerformanceFields
{
	template<typename Archive, typename Value>
	bool operator()(Archive& archive, Value& value) const
	{
		return archive.Fields(value.module, value.weaponPower, value.rangeFeet,
			value.cannons, value.ejectorRacks, value.launchers, value.projectors,
			value.capacitorShots, value.harvestQuantity, value.example.ammunition,
			value.example.techLevel, value.example.quality, value.example.damage,
			value.example.damageModifier, value.example.radiusFeet);
	}
};

template<typename Archive, typename Value>
bool HeaderFields(Archive& archive, Value& header)
{
	return archive.Fields(header.streamline, header.armor, header.minimumTechLevel,
		header.hullVolume, header.structure, header.techLevel)
		&& archive.Vector(header.scalarSlots, 8, ScalarFields())
		&& archive.Vector(header.firstPairs, MaximumModuleKeys, ModuleFields())
		&& archive.Vector(header.secondPairs, MaximumModuleKeys, ModuleFields());
}

template<typename Archive, typename Value>
bool StatsFields(Archive& archive, Value& stats)
{
	return archive.Array(stats.roomVolumes) && archive.Array(stats.roomCounts)
		&& archive.Array(stats.detailCounts)
		&& archive.Fields(stats.hullVolume, stats.equipmentMass, stats.unknownVehicleMasses,
			stats.unknownDetailTypes, stats.crewBerths, stats.officerBerths,
			stats.passengerBerths, stats.troopBerths, stats.passengerClass,
			stats.officerStations, stats.crewStations, stats.drivers, stats.pilots,
			stats.crewRequired, stats.consoles, stats.turrets, stats.surgeries,
			stats.gameTables, stats.parkingSpots, stats.troopStations, stats.maneuverUnits,
			stats.powerPlantUnits, stats.sensorUnits, stats.shieldUnits, stats.weaponUnits,
			stats.ftlUnits, stats.lifeSupportUnits, stats.transporterPads, stats.operableDoors,
			stats.hullOpenings, stats.hasHelm, stats.hasGalley, stats.hasLounge,
			stats.hasHull, stats.hullContiguous, stats.minimumDeck, stats.maximumDeck,
			stats.width, stats.length, stats.height, stats.hullCenter,
			stats.equipmentTechLevel, stats.equipmentTechSource)
		&& archive.Map(stats.weaponBays, MaximumWeaponBays, BayQuantityFields())
		&& archive.Map(stats.transporterRooms, MaximumTransporterRooms, ScalarFields())
		&& archive.Map(stats.vehicleComplement, MaximumVehicleTypes, ScalarFields());
}

template<typename Archive, typename Value>
bool PerformanceFields(Archive& archive, Value& performance)
{
	return archive.Fields(performance.armorTechLevel, performance.massTechLevel,
		performance.moduleTechLevel, performance.moduleTechSource,
		performance.minimumTechLevel, performance.techLevelUsed, performance.structure,
		performance.mustBeBuiltInSpace, performance.maneuverModule,
		performance.ftlModule, performance.shieldModule, performance.hullMassLb,
		performance.fuelMassLb, performance.equipmentMassLb, performance.massLb,
		performance.densityLbPerCubicFoot, performance.fuelCapacity,
		performance.holdCapacity, performance.structureHitPoints,
		performance.equipmentHitPoints, performance.totalHitPoints)
		&& archive.Array(performance.systemHitPoints)
		&& archive.Fields(performance.hullKinetic.absorbedFraction,
			performance.hullKinetic.blockedDamage, performance.hullEnergy.absorbedFraction,
			performance.hullEnergy.blockedDamage, performance.shieldKinetic.absorbedFraction,
			performance.shieldKinetic.blockedDamage, performance.shieldEnergy.absorbedFraction,
			performance.shieldEnergy.blockedDamage, performance.lifeSupportRequired,
			performance.maneuverAccelerationFeetPerSecondSquared,
			performance.verticalAccelerationFeetPerSecondSquared,
			performance.turnDegreesPerSecond, performance.sensorRangeFeet,
			performance.capacitorPower, performance.powerOutputPerSecond,
			performance.fuelBurnPerSecond, performance.maneuverFuelPerSecond,
			performance.lifeSupportPowerPerSecond, performance.maneuverPowerPerSecond,
			performance.sensorPowerPerSecond, performance.shieldPowerPerSecond,
			performance.idlePowerPerSecond, performance.holdingPowerPerSecond,
			performance.idlePowerWithShieldsPerSecond,
			performance.holdingPowerWithShieldsPerSecond,
			performance.shieldDissipationPerDamage, performance.ftlUnitsRequired,
			performance.ftlEngagePower, performance.maxWarpFactor, performance.canEngageFTL)
		&& archive.Map(performance.weaponBays, MaximumWeaponBays, BayPerformanceFields());
}

QVector<AuLegacyShip::ModulePair> UniqueModules(const QVector<AuLegacyShip::ModulePair>& source)
{
	QVector<AuLegacyShip::ModulePair> result;
	QSet<quint16> keys;
	for(const AuLegacyShip::ModulePair& module : source)
		if(!keys.contains(module.first))
		{
			keys.insert(module.first);
			result.append(module);
		}
	return result;
}

template<typename Key, typename Value>
quint64 MapMemory(const QMap<Key, Value>& values)
{
	return quint64(values.size()) * (sizeof(Key) + sizeof(Value) + 3 * sizeof(void*));
}
}

AuLegacyScStats::AuLegacyScStats(const AuLegacyShip::Design& design)
	: AuLegacyScStats(design, AuLegacyShip::CalculateStats(design))
{
}

AuLegacyScStats::AuLegacyScStats(const AuLegacyShip::Design& design, const AuLegacyShip::Stats& stats)
	: m_Stats(stats), m_Performance(AuLegacyShip::CalculatePerformance(design, stats,
		design.header.minimumTechLevel))
{
	// Only numeric source header fields affect this snapshot. Duplicate module
	// keys preserve the original first-value-wins semantics, with bounded storage.
	m_Header.streamline = design.header.streamline;
	m_Header.armor = design.header.armor;
	m_Header.minimumTechLevel = design.header.minimumTechLevel;
	m_Header.hullVolume = design.header.hullVolume;
	m_Header.structure = design.header.structure;
	m_Header.techLevel = design.header.techLevel;
	m_Header.scalarSlots = design.header.scalarSlots.isEmpty()
		? AuLegacyShip::DefaultModuleSlots() : design.header.scalarSlots;
	m_Header.firstPairs = UniqueModules(design.header.firstPairs);
	m_Header.secondPairs = UniqueModules(design.header.secondPairs);
	m_Stats.diagnostics.clear();
	m_Performance.diagnostics.clear();
	m_Performance.manufacturing = AuLegacyShip::Manufacturing();
	m_Valid = Validate();
}

bool AuLegacyScStats::Validate() const
{
	constexpr quint64 MaximumCapacity = std::numeric_limits<quint32>::max();
	constexpr int MaximumBerths = std::numeric_limits<quint16>::max();
	if(!m_Stats.hasHull || !m_Stats.hullVolume || m_Stats.unknownVehicleMasses
		|| m_Stats.unknownDetailTypes || !Finite(m_Performance.massLb)
		|| m_Performance.massLb <= 0 || m_Performance.techLevelUsed < 0
		|| m_Performance.techLevelUsed > 255 || m_Performance.structure < 0
		|| m_Performance.structure > 4 || m_Header.scalarSlots.size() != 8
		|| m_Performance.fuelCapacity > MaximumCapacity
		|| m_Performance.holdCapacity > MaximumCapacity
		|| m_Performance.totalHitPoints / 10 > MaximumCapacity
		|| m_Stats.crewBerths > MaximumBerths || m_Stats.officerBerths > MaximumBerths
		|| m_Stats.passengerBerths > MaximumBerths || m_Stats.troopBerths > MaximumBerths
		|| m_Stats.parkingSpots > MaximumBerths
		|| m_Stats.officerStations > MaximumBerths || m_Stats.crewStations > MaximumBerths
		|| m_Stats.consoles > MaximumBerths || m_Stats.surgeries > MaximumBerths
		|| m_Stats.transporterRooms.size() > int(MaximumTransporterRooms))
		return false;
	for(quint16 module : m_Header.scalarSlots)
		if(!AuLegacyShip::LegacyCommodity::Get(module)) return false;
	for(const AuLegacyShip::ModulePair& module : m_Header.firstPairs)
		if(!AuLegacyShip::LegacyCommodity::Get(module.secondNormalized)) return false;
	for(const AuLegacyShip::ModulePair& module : m_Header.secondPairs)
		if(!AuLegacyShip::LegacyCommodity::Get(module.secondNormalized)) return false;
	quint64 volume = 0;
	for(quint64 roomVolume : m_Stats.roomVolumes)
	{
		if(roomVolume > std::numeric_limits<quint64>::max() - volume) return false;
		volume += roomVolume;
	}
	if(volume != m_Stats.hullVolume) return false;
	for(int count : m_Stats.roomCounts)
		if(count < 0 || count > AuLegacyShip::MaximumRecords) return false;
	for(int count : m_Stats.detailCounts)
		if(count < 0 || count > AuLegacyShip::MaximumRecords) return false;
	const int counts[] = {m_Stats.unknownVehicleMasses, m_Stats.unknownDetailTypes,
		m_Stats.crewBerths, m_Stats.officerBerths, m_Stats.passengerBerths,
		m_Stats.troopBerths, m_Stats.officerStations, m_Stats.crewStations,
		m_Stats.drivers, m_Stats.pilots, m_Stats.crewRequired, m_Stats.consoles,
		m_Stats.turrets, m_Stats.surgeries, m_Stats.gameTables, m_Stats.parkingSpots,
		m_Stats.troopStations, m_Stats.maneuverUnits, m_Stats.powerPlantUnits,
		m_Stats.sensorUnits, m_Stats.shieldUnits, m_Stats.weaponUnits, m_Stats.ftlUnits,
		m_Stats.lifeSupportUnits, m_Stats.transporterPads, m_Stats.operableDoors,
		m_Stats.hullOpenings};
	for(int count : counts)
		if(count < 0 || count > 2 * AuLegacyShip::MaximumRecords) return false;
	for(auto it = m_Stats.weaponBays.constBegin(); it != m_Stats.weaponBays.constEnd(); ++it)
		if(it.value().units < 0 || it.value().units > AuLegacyShip::MaximumRecords
			|| it.value().emitters < 0 || it.value().emitters > AuLegacyShip::MaximumRecords)
			return false;
	for(int count : m_Stats.transporterRooms)
		if(count < 0 || count > AuLegacyShip::MaximumRecords) return false;
	for(int count : m_Stats.vehicleComplement)
		if(count < 0 || count > AuLegacyShip::MaximumRecords) return false;
	if(m_Performance.structureHitPoints > std::numeric_limits<quint64>::max()
		- m_Performance.equipmentHitPoints
		|| m_Performance.totalHitPoints != m_Performance.structureHitPoints
			+ m_Performance.equipmentHitPoints)
		return false;
	for(quint64 hp : m_Performance.systemHitPoints)
		if(hp % 10 || hp / 10 > MaximumCapacity) return false;
	if((m_Performance.systemHitPoints[4] + m_Performance.systemHitPoints[5]) / 10
		> MaximumCapacity) return false;
	return m_Performance.structureHitPoints % 10 == 0
		&& m_Performance.equipmentHitPoints % 10 == 0;
}

void AuLegacyScStats::operator>>(QDataStream& out) const
{
	out << SnapshotMagic << SnapshotVersion << m_Valid;
	if(!m_Valid) return;
	Writer archive(out);
	HeaderFields(archive, m_Header);
	StatsFields(archive, m_Stats);
	PerformanceFields(archive, m_Performance);
}

void AuLegacyScStats::operator<<(QDataStream& in)
{
	AuLegacyScStats snapshot;
	quint32 magic = 0;
	quint8 version = 0;
	bool valid = false;
	in >> magic >> version >> valid;
	if(in.status() != QDataStream::Ok || magic != SnapshotMagic || version != SnapshotVersion)
	{
		in.setStatus(QDataStream::ReadCorruptData);
		*this = AuLegacyScStats();
		return;
	}
	if(valid)
	{
		Reader archive(in);
		if(!HeaderFields(archive, snapshot.m_Header)
			|| !StatsFields(archive, snapshot.m_Stats)
			|| !PerformanceFields(archive, snapshot.m_Performance)
			|| !snapshot.Validate())
		{
			in.setStatus(QDataStream::ReadCorruptData);
			*this = AuLegacyScStats();
			return;
		}
		snapshot.m_Valid = true;
	}
	*this = std::move(snapshot);
}

bool AuLegacyScStats::operator==(const AuLegacyScStats& other) const
{
	if(!m_Valid || !other.m_Valid) return m_Valid == other.m_Valid;
	QByteArray first, second;
	QDataStream firstStream(&first, QIODevice::WriteOnly);
	QDataStream secondStream(&second, QIODevice::WriteOnly);
	firstStream.setVersion(QDataStream::Qt_5_0);
	secondStream.setVersion(QDataStream::Qt_5_0);
	firstStream << *this;
	secondStream << other;
	return first == second;
}

quint64 AuLegacyScStats::MemoryUsage() const
{
	return sizeof(*this) + quint64(m_Header.scalarSlots.capacity()) * sizeof(quint16)
		+ quint64(m_Header.firstPairs.capacity() + m_Header.secondPairs.capacity())
			* sizeof(AuLegacyShip::ModulePair)
		+ MapMemory(m_Stats.weaponBays) + MapMemory(m_Stats.transporterRooms)
		+ MapMemory(m_Stats.vehicleComplement) + MapMemory(m_Performance.weaponBays);
}
