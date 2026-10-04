// Standalone regression/dump harness. Intentionally outside the game targets.
#include "aulegacyship.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <cmath>

using namespace AuLegacyShip;

namespace
{
QJsonValue JsonString(const QString& value)
{
	return value.isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue(value);
}

QJsonObject Point(quint16 packed, qint8 deck)
{
	const QPointF point = DecodePoint(packed);
	const int x = packed >> 8;
	const int y = packed & 0xff;
	return QJsonObject{
		{QStringLiteral("packed_xy"), int(packed)},
		{QStringLiteral("grid_xy"), QJsonArray{x >= 128 ? x - 256 : x, y >= 128 ? y - 256 : y}},
		{QStringLiteral("decoded_xyz"), QJsonArray{point.x(), point.y(), int(deck) * DeckHeight}}
	};
}

QJsonArray Pairs(const QVector<ModulePair>& pairs, bool normalized)
{
	QJsonArray array;
	for(const ModulePair& pair : pairs)
	{
		QJsonObject item{{QStringLiteral("first"), int(pair.first)},
			{QStringLiteral("second_raw"), int(pair.secondRaw)}};
		if(normalized) item.insert(QStringLiteral("second_normalized"), int(pair.secondNormalized));
		array.append(item);
	}
	return array;
}

// The dump deliberately retains the original inspector's offset-based keys,
// making its complete record schema an independent cross-language oracle.
QJsonObject Dump(const Design& design)
{
	const Header& h = design.header;
	QJsonObject header{
		{QStringLiteral("design_id"), double(h.designId)},
		{QStringLiteral("name"), JsonString(h.name)},
		{QStringLiteral("architect_id"), double(h.architectId)},
		{QStringLiteral("streamline"), int(h.streamline)},
		{QStringLiteral("armor"), int(h.armor)},
		{QStringLiteral("version_state_raw"), int(h.versionStateRaw)},
		{QStringLiteral("version"), int(h.version)},
		{QStringLiteral("state"), int(h.state)},
		{QStringLiteral("architect_name"), JsonString(h.architectName)},
		{QStringLiteral("minimum_tech_level"), int(h.minimumTechLevel)}
	};
	if(h.version < 2) header.insert(QStringLiteral("legacy_discarded_qstring"), JsonString(h.legacyDiscardedString));
	if(h.version > 1)
	{
		header.insert(QStringLiteral("hull_volume"), double(h.hullVolume));
		header.insert(QStringLiteral("structure"), int(h.structure));
		header.insert(QStringLiteral("tech_level"), int(h.techLevel));
	}
	QJsonValue modules(QJsonValue::Null);
	if(h.version != 0)
	{
		QJsonArray slots;
		for(quint16 slot : h.scalarSlots) slots.append(int(slot));
		modules = QJsonObject{
			{QStringLiteral("first_pairs"), Pairs(h.firstPairs, true)},
			{QStringLiteral("scalar_u16_slots"), slots},
			{QStringLiteral("second_pairs"), Pairs(h.secondPairs, false)}
		};
	}
	QJsonArray rooms;
	int roomBytes = 0;
	for(const Room& room : design.rooms)
	{
		QJsonArray points;
		for(quint16 vertex : room.packedVertices) points.append(Point(vertex, room.deck));
		const int size = 11 + 2 * room.packedVertices.size();
		roomBytes += size;
		rooms.append(QJsonObject{
			{QStringLiteral("stream_offset"), room.streamOffset},
			{QStringLiteral("design_id"), double(room.designId)},
			{QStringLiteral("room_id"), int(room.roomId)},
			{QStringLiteral("deck"), int(room.deck)},
			{QStringLiteral("room_type"), int(room.roomType)},
			{QStringLiteral("offset_0x08_u8"), int(room.roomSubtype)},
			{QStringLiteral("stored_vertex_count"), room.packedVertices.size()},
			{QStringLiteral("retained_by_legacy_reader"), !room.packedVertices.isEmpty()},
			{QStringLiteral("ellipse_controls"), room.IsEllipse()},
			{QStringLiteral("vertices"), points},
			{QStringLiteral("stream_size"), size}
		});
	}
	QJsonArray details;
	for(const Detail& detail : design.details)
	{
		QJsonObject item = Point(detail.packedXY, detail.deck);
		item.insert(QStringLiteral("stream_offset"), detail.streamOffset);
		item.insert(QStringLiteral("deck"), int(detail.deck));
		item.insert(QStringLiteral("offset_0x01_u8"), int(detail.detailType));
		item.insert(QStringLiteral("bool_raw_u8"), int(detail.hiddenRaw));
		item.insert(QStringLiteral("bool_normalized"), bool(detail.hiddenRaw));
		item.insert(QStringLiteral("detail_id"), double(detail.detailId));
		item.insert(QStringLiteral("design_id"), double(detail.designId));
		item.insert(QStringLiteral("offset_0x28_u16"), int(detail.roomId));
		item.insert(QStringLiteral("offset_0x2a_u16_raw"), int(detail.commodityIdRaw));
		item.insert(QStringLiteral("offset_0x2a_u16_normalized"), int(detail.commodityIdNormalized));
		details.append(item);
	}
	const int detailsOffset = design.bytesConsumed - 4 - 17 * design.details.size();
	return QJsonObject{
		{QStringLiteral("header"), header}, {QStringLiteral("modules"), modules},
		{QStringLiteral("rooms"), rooms}, {QStringLiteral("details"), details},
		{QStringLiteral("stream"), QJsonObject{
			{QStringLiteral("byte_order"), QStringLiteral("big-endian")},
			{QStringLiteral("qdatastream_version"), QStringLiteral("Qt_4_0 (integer 7)")},
			{QStringLiteral("rooms_count_offset"), detailsOffset - roomBytes - 2},
			{QStringLiteral("details_count_offset"), detailsOffset},
			{QStringLiteral("bytes_consumed"), design.bytesConsumed},
			{QStringLiteral("exact_eof"), true}
		}}
	};
}

struct Fixture
{
	QByteArray bytes;
	int versionOffset = 0;
	int roomsOffset = 0;
	int detailsOffset = 0;
};

// QDataStream supplies an independent encoder for the recovered wire layout.
Fixture MakeFixture(quint8 version)
{
	Fixture fixture;
	QDataStream out(&fixture.bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_0);
	out.setByteOrder(QDataStream::BigEndian);
	QString name;
	if(version == 1) name = QStringLiteral("");
	if(version == 2) name = QString(1, QChar(0xd800));
	out << quint32(0xffffffffU) << name << quint32(0xabcdef01U) << quint8(3) << quint8(10);
	fixture.versionOffset = fixture.bytes.size();
	out << quint8((version << 4) | 7) << QStringLiteral("Architect") ;
	if(version < 2) out << QStringLiteral("Discarded");
	out << quint8(19);
	if(version > 1) out << quint32(123456) << quint8(5) << quint8(20);
	if(version != 0)
	{
		out << quint32(3) << quint16(1) << quint16(0xfd) << quint16(2) << quint16(0xff)
			<< quint16(3) << quint16(0x1fd);
		for(int i = 0; i < 8; ++i) out << quint16(i + 100);
		out << quint32(1) << quint16(11) << quint16(0xfd);
	}
	fixture.roomsOffset = fixture.bytes.size();
	out << quint16(3);
	out << quint32(55) << quint16(0xffff) << qint8(-128) << quint8(0x18) << quint8(2)
		<< quint16(3) << quint16(0) << quint16(0x0400) << quint16(0x0002);
	out << quint32(56) << quint16(2) << qint8(127) << quint8(3) << quint8(0)
		<< quint16(3) << quint16(0x807f) << quint16(0x0001) << quint16(0xff00);
	out << quint32(57) << quint16(3) << qint8(0) << quint8(4) << quint8(0) << quint16(0);
	fixture.detailsOffset = fixture.bytes.size();
	out << quint32(3);
	for(int i = 0; i < 3; ++i)
		out << qint8(-1) << quint8(4) << quint8(255) << quint32(400 + i) << quint32(55)
			<< quint16(0x80ff) << quint16(0xffff) << quint16(i == 0 ? 0xf9 : (i == 1 ? 0xfb : 0x1f9));
	return fixture;
}

int failures = 0;
void Check(bool passed, const QString& message)
{
	if(!passed)
	{
		++failures;
		QTextStream(stderr) << "FAIL: " << message << '\n';
	}
}

void RejectAtomic(const QByteArray& bytes, const QString& label, const QString& errorFragment = QString())
{
	Design output;
	output.header.designId = 0x12345678;
	Room sentinel;
	sentinel.roomId = 54321;
	output.rooms.append(sentinel);
	QString error;
	Check(!Read(bytes, output, error), label + QStringLiteral(" rejected"));
	Check(error.startsWith(QStringLiteral("offset 0x")), label + QStringLiteral(" error offset"));
	if(!errorFragment.isEmpty())
		Check(error.contains(errorFragment), label + QStringLiteral(" expected diagnostic: ") + error);
	Check(output.header.designId == 0x12345678 && output.rooms.size() == 1
		&& output.rooms.first().roomId == 54321, label + QStringLiteral(" output unchanged"));
}

int SelfTest()
{
	for(quint8 version = 0; version <= 2; ++version)
	{
		const Fixture fixture = MakeFixture(version);
		Design parsed;
		QString error = QStringLiteral("stale");
		const bool valid = Read(fixture.bytes, parsed, error);
		Check(valid, QStringLiteral("version %1 valid fixture: %2").arg(version).arg(error));
		if(!valid) continue;
		Check(error.isEmpty() && parsed.bytesConsumed == fixture.bytes.size(), QStringLiteral("exact EOF and error clear"));
		Check(parsed.header.designId == 0xffffffffU && parsed.header.architectId == 0xabcdef01U
			&& parsed.header.version == version && parsed.header.state == 7, QStringLiteral("header unsigned values"));
		Check(version == 0 ? parsed.header.name.isNull()
			: (version == 1 ? (!parsed.header.name.isNull() && parsed.header.name.isEmpty())
			: parsed.header.name.size() == 1 && parsed.header.name.at(0).unicode() == 0xd800),
			QStringLiteral("null, empty, unpaired UTF-16 strings"));
		Check(parsed.rooms.size() == 3 && parsed.details.size() == 3, QStringLiteral("record counts"));
		if(parsed.rooms.size() != 3 || parsed.details.size() != 3) continue;
		Check(parsed.rooms.at(0).deck == -128 && parsed.rooms.at(1).deck == 127
			&& parsed.rooms.at(0).roomSubtype == 2, QStringLiteral("room fields and signed decks"));
		Check(parsed.rooms.at(0).streamOffset == fixture.roomsOffset + 2
			&& parsed.details.at(0).streamOffset == fixture.detailsOffset + 4, QStringLiteral("record offsets"));
		Check(parsed.details.at(0).hiddenRaw == 255 && parsed.details.at(0).roomId == 0xffff
			&& parsed.details.at(0).commodityIdRaw == 0xf9 && parsed.details.at(0).commodityIdNormalized == 0xf8
			&& parsed.details.at(1).commodityIdNormalized == 0xfa && parsed.details.at(2).commodityIdNormalized == 0x1f9,
			QStringLiteral("detail raw values and exact normalizations"));
		Check(DecodePoint(0x80ff) == QPointF(-128 * CoordinateSpacing, -CoordinateSpacing), QStringLiteral("signed XY decode"));
		Check(DecodePoint(0x7f80) == QPointF(127 * CoordinateSpacing, -128 * CoordinateSpacing), QStringLiteral("opposite coordinate extremes"));
		if(version != 0)
		{
			Check(parsed.header.firstPairs.size() == 3 && parsed.header.secondPairs.size() == 1
				&& parsed.header.scalarSlots.size() == 8, QStringLiteral("module array counts"));
			if(parsed.header.firstPairs.size() == 3 && parsed.header.secondPairs.size() == 1)
				Check(parsed.header.firstPairs.at(0).secondRaw == 0xfd && parsed.header.firstPairs.at(0).secondNormalized == 0xfc
					&& parsed.header.firstPairs.at(1).secondNormalized == 0xfe && parsed.header.firstPairs.at(2).secondNormalized == 0x1fd
					&& parsed.header.secondPairs.at(0).secondNormalized == 0xfd, QStringLiteral("first-pair-only remaps"));
		}
		const QPolygonF ellipse = RoomBoundary(parsed.rooms.at(0), &error);
		Check(ellipse.size() == 12 && error.isEmpty(), QStringLiteral("ellipse facet formula"));
		Check(parsed.rooms.at(0).packedVertices.size() == 3, QStringLiteral("ellipse stored controls preserved"));
		if(ellipse.size() == 12)
		{
			Check(std::abs(ellipse.first().x() - 4 * CoordinateSpacing) < 0.00001
				&& std::abs(ellipse.first().y()) < 0.00001 && ellipse.at(1).y() < 0,
				QStringLiteral("ellipse axis placement and clockwise winding"));
		}
		Check(RoomBoundary(parsed.rooms.at(1)).size() == 3 && RoomBoundary(parsed.rooms.at(2)).isEmpty(),
			QStringLiteral("ordinary and empty boundaries"));
		Room degenerate = parsed.rooms.at(0);
		degenerate.packedVertices[1] = 0;
		Check(RoomBoundary(degenerate, &error).isEmpty() && !error.isEmpty(), QStringLiteral("zero-axis ellipse guard"));
		degenerate.packedVertices[1] = 0x0400;
		degenerate.packedVertices[2] = 0x0800;
		Check(RoomBoundary(degenerate, &error).isEmpty() && !error.isEmpty(), QStringLiteral("collinear ellipse guard"));
		for(int size = 0; size < fixture.bytes.size(); ++size)
			RejectAtomic(fixture.bytes.left(size), QStringLiteral("truncation v%1 at %2").arg(version).arg(size));
		RejectAtomic(fixture.bytes + QByteArray(1, '\0'), QStringLiteral("trailing byte"));
		QByteArray future = fixture.bytes;
		future[fixture.versionOffset] = char(0x30);
		RejectAtomic(future, QStringLiteral("unsupported version"), QStringLiteral("unsupported .ship version"));
		QByteArray odd = fixture.bytes;
		odd.replace(4, 4, QByteArray::fromHex("00000003"));
		RejectAtomic(odd, QStringLiteral("odd UTF-16 length"), QStringLiteral("odd UTF-16"));
		QByteArray oversizedString = fixture.bytes;
		oversizedString.replace(4, 4, QByteArray::fromHex("fffffffe"));
		RejectAtomic(oversizedString, QStringLiteral("oversized UTF-16 length"));
		QByteArray oversizedDetails = fixture.bytes;
		oversizedDetails.replace(fixture.detailsOffset, 4, QByteArray::fromHex("ffffffff"));
		RejectAtomic(oversizedDetails, QStringLiteral("oversized detail count"));
	}
	Fixture budget = MakeFixture(0);
	budget.bytes.resize(budget.detailsOffset);
	QDataStream records(&budget.bytes, QIODevice::WriteOnly | QIODevice::Append);
	records.setVersion(QDataStream::Qt_4_0);
	records << quint32(MaximumRecords);
	budget.bytes.append(QByteArray(MaximumRecords * 17, '\0'));
	RejectAtomic(budget.bytes, QStringLiteral("combined record budget across arrays"), QStringLiteral("combined"));
	RejectAtomic(QByteArray(MaximumFileBytes + 1, '\0'), QStringLiteral("file size budget"), QStringLiteral("byte safety limit"));
	QTextStream(stdout) << (failures ? "FAILED" : "PASS") << ": native reader self-tests (" << failures << " failures)\n";
	return failures ? 1 : 0;
}
}

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	const QStringList args = app.arguments();
	if(args.size() == 2 && args.at(1) == QStringLiteral("--self-test")) return SelfTest();
	if(args.size() < 2)
	{
		QTextStream(stderr) << "Usage: native_ship_reader --self-test | FILE.ship [FILE.ship ...]\n";
		return 2;
	}
	QJsonArray files;
	bool failed = false;
	for(int index = 1; index < args.size(); ++index)
	{
		Design parsed;
		QString error;
		QJsonObject file{{QStringLiteral("path"), args.at(index)}};
		if(Load(args.at(index), parsed, error)) file.insert(QStringLiteral("records"), Dump(parsed));
		else
		{
			file.insert(QStringLiteral("error"), error);
			failed = true;
		}
		files.append(file);
	}
	QTextStream(stdout) << QJsonDocument(QJsonObject{{QStringLiteral("files"), files}}).toJson(QJsonDocument::Indented);
	return failed ? 1 : 0;
}
