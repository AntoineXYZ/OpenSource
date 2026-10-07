#include "aulegacyshippathreport.h"
#include "aulegacyshipdetails.h"
#include "aulegacyshippaths.h"

#include <QHash>
#include <algorithm>
#include <limits>

namespace
{
using namespace AuLegacyShip;

struct Endpoint
{
	const Detail* detail;
	const Room* room;
};

enum class TargetRule { None, Crew, Troop, Officer, Public };

// AuDesignCheck::Report, Shores of Hazeron.exe:0x14017d380. These are
// deliberately the raw detail types from its switch, including vehicle seats.
TargetRule Rule(int type)
{
	switch(type)
	{
	case 1: case 6: case 7: case 12: case 13: case 15: case 17:
	case 57: case 58: case 59: case 60: case 61: case 62: case 63: case 64:
		return TargetRule::Crew;
	case 21: case 22: case 33: case 34: case 35: case 36:
	case 37: case 38: case 39: case 40:
		return TargetRule::Troop;
	case 73:
		return TargetRule::Officer;
	case 4: case 11: case 19: case 20: case 74: case 75:
		return TargetRule::Public;
	default:
		return TargetRule::None;
	}
}

bool IsBerth(int type)
{
	return type == 2 || (type >= 25 && type <= 31) || (type >= 65 && type <= 72);
}

bool IsOutsideStation(int type)
{
	// DnDetailType::IsConsole, AuData13.dll:0x180031d50, plus game tables.
	return (Rule(type) != TargetRule::None && !(type >= 57 && type <= 64) && type != 20);
}

bool IsPublicOutsideStation(int type)
{
	return type == 4 || type == 11 || type == 19 || type == 20 || type == 74 || type == 75;
}

bool ChecksBerth(TargetRule rule, int roomType)
{
	if(roomType == 19)
		return rule != TargetRule::None;
	switch(rule)
	{
	case TargetRule::Crew: return roomType == 4 || roomType == 7;
	case TargetRule::Troop: return roomType == 1;
	case TargetRule::Officer: return roomType == 4;
	case TargetRule::Public: return true;
	default: return false;
	}
}

QString RoomTypeName(int type)
{
	// DnRoomType::Rx names decoded from the supplied GD_2016 (0x18/0x3a).
	static const char* const names[] = {
		"None", "Barracks", "Bridge", "Brig", "Officer Cabin", "Hold", "Hall",
		"Crew Cabin", "Engine Room", "Fuel Cell", "Galley", "Gravity Well", "",
		"Vehicle Bay", "Lounge", "Open to Deck Below", "Sensor Room", "Shield Room",
		"Sickbay", "Stateroom", "Transporter Room", "Weapon Bay", "Roof, Gable",
		"Roof, Hip", "Roof, Round"
	};
	return type >= 0 && type < int(sizeof(names) / sizeof(names[0]))
		? QString::fromLatin1(names[type]) : QStringLiteral("Unknown Room");
}

QString Place(const Room* room, int deck, int minimumDeck)
{
	return QStringLiteral("%1 %2 deck %3")
		.arg(room ? RoomTypeName(room->roomType).toHtmlEscaped() : QStringLiteral("Unknown Room"))
		.arg(room ? int(room->roomSubtype) + 1 : 1).arg(deck - minimumDeck + 1);
}

QString Station(const Detail& detail, const Room* room, int minimumDeck, bool roomDeck)
{
	return QStringLiteral("%1 in %2").arg(DetailName(detail.detailType).toHtmlEscaped(),
		Place(room, roomDeck && room ? int(room->deck) : int(detail.deck), minimumDeck));
}

void Warning(QString& html, const QString& message, bool security = false)
{
	html += QStringLiteral("&middot; <span style=\"color: rgb(%1);\">%2 Warning:</span> %3<br>")
		.arg(security ? QStringLiteral("255,255,0") : QStringLiteral("255,0,0"),
			security ? QStringLiteral("Security") : QStringLiteral("Path"), message);
}

void FinishSection(QString& html, bool warned)
{
	if(!warned)
		html += QStringLiteral("&middot; <span style=\"color: rgb(0,255,0);\">Ok</span><br>");
	html += QStringLiteral("<br>");
}

QString Canceled(QString html)
{
	return html + QStringLiteral("<span style=\"color: rgb(255,255,0);\">Canceled</span><br>");
}

QPointF SeatPoint(const Detail& detail)
{
	// DaCreature::ConsoleSeatOffset, AuAtom13.dll:0x1802e5dc0. Report always
	// supplies angle=0 and seat=0, including both game-table orientations.
	QPointF offset;
	switch(detail.detailType)
	{
	case 21: offset = QPointF(-0.5, 0.0); break;
	case 34: offset = QPointF(-0.35, -0.35); break;
	case 35: offset = QPointF(0.0, -0.5); break;
	case 36: offset = QPointF(0.35, -0.35); break;
	case 37: offset = QPointF(0.5, 0.0); break;
	case 38: offset = QPointF(0.35, 0.35); break;
	case 39: offset = QPointF(0.0, 0.5); break;
	case 40: offset = QPointF(-0.35, 0.35); break;
	case 74: case 75: offset = QPointF(4.0, 4.0); break;
	default: break;
	}
	return DecodePoint(detail.packedXY) + offset;
}

int SeatDeck(const Detail& detail)
{
	// The bottom turret's seat is 2.5 feet below its detail. All other report
	// offsets are 0..1.6667 feet above. DnSpaceShip::OnDeck at 0x18015b510
	// takes floor((z + 0.05) / 8), so only type 22 changes its terminal deck.
	return int(detail.deck) - (detail.detailType == 22 ? 1 : 0);
}
}

QString AuLegacyShipPathReport(const AuLegacyShip::Design& design,
	const std::function<bool(const QString&, int, int)>& progress)
{
	using namespace AuLegacyShip;
	QHash<quint16, const Room*> rooms;
	int minimumDeck = MaximumDeck;
	bool hasDeck = false;
	for(const Room& room : design.rooms)
	{
		if(!rooms.contains(room.roomId))
			rooms.insert(room.roomId, &room);
		if(!(room.roomType >= 22 && room.roomType <= 24) && !RoomBoundary(room).isEmpty())
		{
			minimumDeck = std::min(minimumDeck, int(room.deck));
			hasDeck = true;
		}
	}
	if(!hasDeck)
		minimumDeck = design.rooms.isEmpty() ? 0 : int(design.rooms.first().deck);

	QVector<Endpoint> berths;
	QVector<Endpoint> outsideStations;
	for(const Detail& detail : design.details)
	{
		const Room* room = rooms.value(detail.roomId, nullptr);
		if(!room)
			continue;
		if(IsBerth(detail.detailType))
			berths.append(Endpoint{&detail, room});
		else if(IsOutsideStation(detail.detailType))
			outsideStations.append(Endpoint{&detail, room});
	}

	QString html;
	bool canceled = false;
	Paths::Graph graph;
	QString error;
	const QString buildLabel = QStringLiteral("Creating paths...");
	if(!graph.Build(design, error, [&](int value, int maximum) {
		if(progress && !progress(buildLabel, value, maximum))
			canceled = true;
		return !canceled;
	}))
	{
		if(canceled)
			return Canceled(html);
		Warning(html, QStringLiteral("Unable to check paths: %1").arg(error.toHtmlEscaped()));
		return html;
	}

	qint64 matchingBerths[5] = {};
	for(int rule = int(TargetRule::Crew); rule <= int(TargetRule::Public); ++rule)
		for(const Endpoint& berth : berths)
			if(ChecksBerth(TargetRule(rule), berth.room->roomType))
				++matchingBerths[rule];
	qint64 work = qint64(berths.size()) + outsideStations.size();
	for(const Detail& detail : design.details)
		work += matchingBerths[int(Rule(detail.detailType))];
	const int maximum = int(std::min<qint64>(std::numeric_limits<int>::max(), std::max<qint64>(1, work)));
	qint64 completed = 0;
	QString currentLabel;
	const auto tick = [&](const QString& label) {
		currentLabel = label;
		if(progress && !progress(label, int(std::min<qint64>(completed, maximum)), maximum))
			return false;
		++completed;
		return true;
	};
	graph.SetCancelCheck([&]() {
		if(progress && !progress(currentLabel, int(std::min<qint64>(std::max<qint64>(0, completed - 1), maximum)), maximum))
			canceled = true;
		return canceled;
	});

	html += QStringLiteral("<b>Paths to Outside from Berths</b><br>");
	bool warned = false;
	for(const Endpoint& berth : berths)
	{
		if(!tick(QStringLiteral("Finding paths to the outside from all berths...")))
			return Canceled(html);
		const QPointF point = DecodePoint(berth.detail->packedXY);
		const bool crew = berth.room->roomType != 19;
		const bool accessible = graph.CanReachOutside(point, berth.detail->deck, crew);
		if(canceled)
			return Canceled(html);
		if(accessible)
			continue;
		const QString place = Place(berth.room, berth.room->deck, minimumDeck);
		const bool crewAccess = !crew && graph.CanReachOutside(point, berth.detail->deck, true);
		if(canceled)
			return Canceled(html);
		if(crewAccess)
			Warning(html, QStringLiteral("Path from berth in %1 to the outside is passable by crew but not by passengers.").arg(place));
		else
			Warning(html, QStringLiteral("No path from berth in %1 to the outside.").arg(place));
		warned = true;
	}
	FinishSection(html, warned);

	html += QStringLiteral("<b>Paths to Outside from Stations and Turrets</b><br>");
	warned = false;
	for(const Endpoint& endpoint : outsideStations)
	{
		if(!tick(QStringLiteral("Finding paths to the outside from all stations and turrets...")))
			return Canceled(html);
		const Detail& detail = *endpoint.detail;
		const QPointF point = IsPublicOutsideStation(detail.detailType) && detail.detailType < 74
			? DecodePoint(detail.packedXY) : SeatPoint(detail);
		const int deck = SeatDeck(detail);
		const bool publicAccess = graph.CanReachOutside(point, deck, false);
		const bool crewAccess = publicAccess || graph.CanReachOutside(point, deck, true);
		if(canceled)
			return Canceled(html);
		const QString station = Station(detail, endpoint.room, minimumDeck, true);
		if(IsPublicOutsideStation(detail.detailType))
		{
			if(publicAccess)
				continue;
			Warning(html, crewAccess
				? QStringLiteral("Path from %1 to the outside is passable by crew but not by passengers or guests.").arg(station)
				: QStringLiteral("No path from %1 to the outside.").arg(station));
		}
		else if(publicAccess)
			Warning(html, QStringLiteral("%1 is accessible to passengers or guests from a hull entrance.").arg(station), true);
		else if(!crewAccess)
			Warning(html, QStringLiteral("No path from %1 to the outside.").arg(station));
		else
			continue;
		warned = true;
	}
	FinishSection(html, warned);

	html += QStringLiteral("<b>Paths to Stations and Turrets from Berths</b><br>");
	warned = false;
	for(const Detail& detail : design.details)
	{
		const TargetRule rule = Rule(detail.detailType);
		if(rule == TargetRule::None)
			continue;
		const Room* room = rooms.value(detail.roomId, nullptr);
		const QPointF target = rule == TargetRule::Troop || detail.detailType == 74 || detail.detailType == 75
			? SeatPoint(detail) : DecodePoint(detail.packedXY);
		const int targetDeck = SeatDeck(detail);
		for(const Endpoint& berth : berths)
		{
			if(!ChecksBerth(rule, berth.room->roomType))
				continue;
			if(!tick(QStringLiteral("Finding paths to stations and turrets from berths...")))
				return Canceled(html);
			const bool passenger = berth.room->roomType == 19;
			const bool crew = !passenger;
			const QPointF start = DecodePoint(berth.detail->packedXY);
			const bool reachable = graph.CanReach(start, berth.detail->deck, target, targetDeck, crew);
			if(canceled)
				return Canceled(html);
			const QString from = Place(berth.room, berth.room->deck, minimumDeck);
			const QString station = Station(detail, room, minimumDeck, false);
			if(rule != TargetRule::Public && passenger)
			{
				if(!reachable)
					continue;
				Warning(html, QStringLiteral("%1 is accessible to passenger berth in %2.").arg(station, from), true);
			}
			else
			{
				if(reachable)
					continue;
				const bool crewAccess = passenger && graph.CanReach(start, berth.detail->deck, target, targetDeck, true);
				if(canceled)
					return Canceled(html);
				Warning(html, crewAccess
					? QStringLiteral("Path from berth in %1 to %2 is passable by crew but not by passengers or guests.").arg(from, station)
					: QStringLiteral("No path from berth in %1 to %2.").arg(from, station));
			}
			warned = true;
		}
	}
	FinishSection(html, warned);
	if(progress && !progress(QStringLiteral("Checking paths..."), maximum, maximum))
		return Canceled(html);
	return html;
}
