// Manual regression harness, outside game targets. The user builds this.
#include "aulegacyshipsave.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <limits>

using namespace AuLegacyShip;

namespace
{
int failures = 0;

void Check(bool passed, const char* label)
{
	if(!passed)
	{
		QTextStream(stderr) << "FAIL: " << label << '\n';
		++failures;
	}
}

ModulePair Pair(quint16 first, quint16 raw, quint16 normalized)
{
	ModulePair pair;
	pair.first = first;
	pair.secondRaw = raw;
	pair.secondNormalized = normalized;
	return pair;
}

Design Fixture()
{
	Design design;
	Header& h = design.header;
	h.version = 2;
	h.designId = 0x12345678;
	h.name = QStringLiteral("A");
	h.architectId = 0x89abcdef;
	h.streamline = 1;
	h.armor = 10;
	h.state = 5;
	h.minimumTechLevel = 31;
	h.hullVolume = 0x10203040;
	h.structure = 2;
	h.techLevel = 32;
	h.firstPairs << Pair(2, 0xfd, 0xfc) << Pair(1, 0xff, 0xfe);
	for(int i = 0; i < 8; ++i) h.scalarSlots << quint16(i);
	h.secondPairs << Pair(9, 0xfd, 0xfd);
	Room room;
	room.designId = h.designId;
	room.roomId = 0x2468;
	room.deck = -2;
	room.roomType = 2;
	room.roomSubtype = 255;
	room.packedVertices = { 0x80ff, 0x7f00, 0x0001 };
	design.rooms << room << Room();
	Detail detail;
	detail.deck = -128;
	detail.detailType = 0x3f;
	detail.hiddenRaw = 0xfe;
	detail.detailId = 0x10203040;
	detail.designId = h.designId;
	detail.packedXY = 0xff80;
	detail.roomId = room.roomId;
	detail.commodityIdRaw = 0xf9;
	detail.commodityIdNormalized = 0xf8;
	design.details << detail;
	return design;
}

QByteArray ReadFile(const QString& path)
{
	QFile file(path);
	if(!file.open(QIODevice::ReadOnly)) return QByteArray();
	return file.readAll();
}

void CheckRejected(const Design& design, const char* label)
{
	QByteArray bytes("untouched");
	QString error;
	Check(!Write(design, bytes, error) && bytes == "untouched" && !error.isEmpty(), label);
}

Room TestRoom(quint16 id, int deck, int type, int number)
{
	Room room;
	room.roomId = id;
	room.deck = qint8(deck);
	room.roomType = quint8(type);
	room.roomSubtype = quint8(number);
	room.packedVertices = {0, 0x0200, 0x0202, 0x0002};
	return room;
}

Detail TestDetail(quint32 id, int deck, quint16 roomId, int type)
{
	Detail detail;
	detail.detailId = id;
	detail.deck = qint8(deck);
	detail.roomId = roomId;
	detail.detailType = quint8(type);
	return detail;
}

bool HasPair(const QVector<ModulePair>& pairs, quint16 key, quint16 value)
{
	for(const ModulePair& pair : pairs)
		if(pair.first == key && pair.secondNormalized == value) return true;
	return false;
}

void PreparationTests()
{
	QString error;
	Design design, prepared;
	design.header.designId = 123;
	design.header.architectId = 456;
	design.header.architectName = QStringLiteral("Original author");
	design.details << TestDetail(0xffffffffU, -1, 2, 1) << TestDetail(8, -1, 1, 1)
		<< TestDetail(2, -1, 1, 1) << TestDetail(1, 1, 1, 1) << TestDetail(9, -2, 2, 1);
	Check(PrepareForSave(design, prepared, error), "prepare equipment ordering");
	QVector<quint32> ids;
	for(const Detail& detail : prepared.details) ids << detail.detailId;
	Check(ids == QVector<quint32>({9, 2, 8, 0xffffffffU, 1}), "signed deck, unsigned room and equipment ID ordering");
	Check(design.details.first().detailId == 0xffffffffU && prepared.header.designId == 123
		&& prepared.header.architectId == 456 && prepared.header.architectName == design.header.architectName,
		"preparation retains source identity and does not mutate input");

	design.details.clear();
	design.rooms << TestRoom(1, -1, 20, 7) << TestRoom(2, 0, 20, 7) << TestRoom(3, 1, 20, 7);
	design.details << TestDetail(1, -1, 1, 19);
	Check(PrepareForSave(design, prepared, error) && prepared.header.secondPairs.isEmpty(),
		"rooms and transporter consoles alone do not create module entries");
	design.details << TestDetail(2, -1, 1, 20) << TestDetail(3, -1, 1, 20)
		<< TestDetail(4, 0, 2, 20) << TestDetail(5, 1, 3, 20) << TestDetail(6, 0, 99, 20);
	design.header.secondPairs << Pair(0xff07, 0x321, 0x321) << Pair(0xff07, 0x322, 0x322)
		<< Pair(0x1234, 0x999, 0x999);
	Check(PrepareForSave(design, prepared, error) && prepared.header.secondPairs.size() == 3
		&& HasPair(prepared.header.secondPairs, 0xff07, 0x321)
		&& HasPair(prepared.header.secondPairs, 0x0007, 0xf6)
		&& HasPair(prepared.header.secondPairs, 0x0107, 0xf6),
		"pad groups use deck/subtype keys, retain first existing module, trim stale keys and ignore orphan pads");
	Check(design.header.secondPairs.size() == 3 && design.header.secondPairs.last().first == 0x1234,
		"module reconciliation leaves original header untouched");
	design.rooms[0].hasWorkingBoundary = true;
	Check(PrepareForSave(design, prepared, error) && prepared.header.secondPairs.size() == 2
		&& !HasPair(prepared.header.secondPairs, 0xff07, 0x321), "empty working room overrides stored boundary");
	design.details.clear();
	Check(PrepareForSave(design, prepared, error) && prepared.header.secondPairs.isEmpty(),
		"removing the last pad trims its module even when the room remains");

	design.rooms = {TestRoom(1, 0, 21, 5), TestRoom(2, 1, 21, 5),
		TestRoom(3, 0, 2, 6), TestRoom(4, 0, 2, 7)};
	design.details << TestDetail(1, 0, 3, 0x17) << TestDetail(2, 0, 4, 0x63);
	design.header.firstPairs << Pair(5, 0xfc, 0xfc) << Pair(9, 0x105, 0x105);
	Check(PrepareForSave(design, prepared, error) && prepared.header.firstPairs.size() == 3
		&& HasPair(prepared.header.firstPairs, 5, 0xfc) && HasPair(prepared.header.firstPairs, 6, 0x105)
		&& HasPair(prepared.header.firstPairs, 7, 0x105),
		"bay number spans decks; weapon units and emitters also establish bay keys");

	// Fixed wire digests from the independent bucket-group reference, covering
	// collisions and each recovered growth transition (not the host hash table).
	const int counts[] = {8, 9, 64, 65, 512, 513};
	const char* hashes[] = {
		"04c7234636cce613bfe0aa955d1c204ff67c5fcb381f32c1ff500f895b0b290e",
		"58592e6e5f93bb14c02e1d311c11d6361a440ab746c63d6a025125b09394aafd",
		"efb2e0c3111a7aa0e20ce401936c9c277f9d47793c6d39200e35d0b01c1c5e48",
		"63e3b3b837c32b07f06ef1ac2b4dd096efa5ae08353da74dab9332508fa480de",
		"952f642cda0bd8de2cd4a3f96de37b3206bab92760064ec69ae63426d653011c",
		"1e13154853a5718ffc33b1c6309dfdeae11e8af5976b6cd409de5227db6f982b"
	};
	for(int test = 0; test < 6; ++test)
	{
		design = Design();
		for(int key = 0; key < counts[test]; ++key)
		{
			design.rooms << TestRoom(quint16(key + 1), key >> 8, 20, key & 255);
			design.details << TestDetail(quint32(key + 1), key >> 8, quint16(key + 1), 20);
		}
		Check(PrepareForSave(design, prepared, error), "module map growth preparation");
		QByteArray wire;
		QDataStream stream(&wire, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_4_0);
		for(const ModulePair& pair : prepared.header.secondPairs) stream << pair.first << pair.secondNormalized;
		Check(QCryptographicHash::hash(wire, QCryptographicHash::Sha256).toHex() == hashes[test],
			"original module hash iteration order across bucket growth");
	}
	design.details.resize(MaximumRecords + 1);
	prepared.header.name = QStringLiteral("unchanged");
	Check(!PrepareForSave(design, prepared, error) && !error.isEmpty()
		&& prepared.header.name == QStringLiteral("unchanged"), "preparation rejects excess records atomically");
}

void SelfTest()
{
	PreparationTests();
	// Fixed wire oracle assembled independently with big-endian integer fields,
	// UTF-16 code units and the recovered widths. No Qt writer builds this oracle.
	const QByteArray golden = QByteArray::fromHex(
		"1234567800000002004189abcdef010a25ffffffff1f102030400220"
		"00000002000200fc000100fe00000001000200030004000500060007"
		"00000001000900fd0001123456782468fe02ff000380ff7f000001"
		"00000001803f011020304012345678ff80246800f8");
	Design design = Fixture();
	QByteArray bytes;
	QString error;
	Check(Write(design, bytes, error) && bytes == golden && error.isEmpty(),
		"exact 104-byte golden: widths, order, normalization, signed values, null string, empty room exclusion");
	Check(design.rooms.size() == 2 && design.header.firstPairs[0].secondRaw == 0xfd
		&& design.details[0].hiddenRaw == 0xfe, "write leaves working records untouched");
	Design reloaded;
	Check(Read(bytes, reloaded, error), "saved fixture reads to exact EOF");
	QByteArray again;
	Check(Write(reloaded, again, error) && again == golden, "normalized save is byte-stable");
	QByteArray obsolete = golden;
	const int pairOffset = obsolete.indexOf(QByteArray::fromHex("000200fc000100fe"));
	if(pairOffset >= 0 && !reloaded.details.isEmpty())
	{
		obsolete[pairOffset + 3] = char(0xfd);
		obsolete[pairOffset + 7] = char(0xff);
		obsolete[reloaded.details[0].streamOffset + 2] = char(0xfe);
		for(int commodity : {0xf9, 0xfb})
		{
			obsolete[reloaded.details[0].streamOffset + 16] = char(commodity);
			QByteArray expected = golden;
			expected[expected.size() - 1] = char(commodity - 1);
			Design normalized;
			Check(Read(obsolete, normalized, error) && Write(normalized, again, error) && again == expected,
				"read/save normalizes fd/ff module IDs, f9/fb vehicle IDs and noncanonical booleans only");
		}
	}
	else Check(false, "normalization fixture offsets");

	const QVector<quint16> defaults = { 234, 259, 237, 238, 235, 239, 263, 240 };
	Check(DefaultModuleSlots() == defaults, "original constructor module defaults");
	for(int version = 0; version <= 1; ++version)
	{
		QByteArray old;
		QDataStream stream(&old, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_4_0);
		stream << quint32(0) << QString() << quint32(0) << quint8(0) << quint8(0)
			<< quint8(version << 4) << QStringLiteral("") << QStringLiteral("discarded") << quint8(0);
		if(version == 1)
		{
			stream << quint32(0);
			for(quint16 slot : defaults) stream << slot;
			stream << quint32(0);
		}
		stream << quint16(0) << quint32(0);
		Design loaded;
		Check(Read(old, loaded, error) && Write(loaded, bytes, error), "version 0/1 upgrade saves");
		const QByteArray upgraded = QByteArray::fromHex(
			"00000000ffffffff000000000000200000000000000000000000"
			"0000000000ea010300ed00ee00eb00ef010700f000000000000000000000");
		Check(bytes == upgraded, "version 0/1 upgrades to exact v2 golden, omits obsolete QString");
	}

	design = Fixture();
	design.header.name = QString();
	design.header.architectName = QStringLiteral("");
	Check(Write(design, bytes, error) && Read(bytes, reloaded, error)
		&& reloaded.header.name.isNull() && !reloaded.header.architectName.isNull()
		&& reloaded.header.architectName.isEmpty(), "null and empty QString remain distinct");
	design.header.name = QString(QChar(0xd800));
	design.header.architectName = QString(QChar(0)) + QChar(0xdc00);
	Check(Write(design, bytes, error) && Read(bytes, reloaded, error)
		&& reloaded.header.name == design.header.name && reloaded.header.architectName == design.header.architectName,
		"raw UTF-16 preserves lone surrogates and embedded NUL");
	design.header.name = QString();
	Check(Write(design, bytes, error, QStringLiteral("Named")) && Read(bytes, reloaded, error)
		&& reloaded.header.name == QStringLiteral("Named") && design.header.name.isNull(), "fallback name without mutation");
	design.header.name = QStringLiteral("Existing");
	Check(Write(design, bytes, error, QStringLiteral("Other")) && Read(bytes, reloaded, error)
		&& reloaded.header.name == QStringLiteral("Existing"), "fallback never renames existing design");

	design = Fixture();
	Room& room = design.rooms[0];
	room.hasWorkingBoundary = true;
	room.workingBoundary << QPointF(-0.5 * CoordinateSpacing, 0.5 * CoordinateSpacing)
		<< QPointF(-128 * CoordinateSpacing, 127 * CoordinateSpacing)
		<< QPointF(1.49 * CoordinateSpacing, -1.49 * CoordinateSpacing);
	Check(Write(design, bytes, error) && Read(bytes, reloaded, error)
		&& reloaded.rooms[0].packedVertices == QVector<quint16>({0xff01, 0x807f, 0x01ff}),
		"working doubles override cached vertices; ties round away from zero");
	room.roomType = 0x18;
	room.workingBoundary.clear();
	room.workingBoundary << QPointF(4 * CoordinateSpacing, 0) << QPointF(0, -2 * CoordinateSpacing)
		<< QPointF(-4 * CoordinateSpacing, 0) << QPointF(0, 2 * CoordinateSpacing);
	Check(Write(design, bytes, error) && Read(bytes, reloaded, error)
		&& reloaded.rooms[0].packedVertices == QVector<quint16>({0, 0x0400, 0x0002}),
		"dome writes center, vertex zero, and vertex at three quarters");
	Check(Write(reloaded, again, error) && again == bytes, "expanded ellipse controls resave identically");

	design = Fixture();
	design.header.scalarSlots.removeLast();
	CheckRejected(design, "reject invalid scalar count atomically");
	design = Fixture();
	design.header.state = 16;
	CheckRejected(design, "reject invalid state atomically");
	design = Fixture();
	design.rooms[0].hasWorkingBoundary = true;
	design.rooms[0].workingBoundary << QPointF(std::numeric_limits<double>::quiet_NaN(), 0);
	CheckRejected(design, "reject nonfinite geometry atomically");
	design.rooms[0].workingBoundary[0] = QPointF(127.5 * CoordinateSpacing, 0);
	CheckRejected(design, "reject rounded coordinate overflow atomically");
	design = Fixture();
	design.rooms[0].packedVertices.fill(0, 65536);
	CheckRejected(design, "reject vertex count narrowing");
	design = Fixture();
	Room repeated = design.rooms.first();
	repeated.packedVertices = { 0 };
	design.rooms.fill(repeated, 65536);
	CheckRejected(design, "reject room count narrowing");
	design = Fixture();
	design.details.resize(MaximumRecords + 1);
	CheckRejected(design, "reject combined record overflow");
	design = Fixture();
	design.rooms[0].roomType = 0x18;
	design.rooms[0].packedVertices = {0, 0, 0};
	CheckRejected(design, "reject invalid ellipse instead of losing geometry");

	QTemporaryDir directory;
	Check(directory.isValid(), "temporary save directory");
	if(directory.isValid())
	{
		const QByteArray preparedGolden = QByteArray::fromHex(
			"1234567800000002004189abcdef010a25ffffffff1f102030400220"
			"000000000000000100020003000400050006000700000000"
			"0001123456782468fe02ff000380ff7f000001"
			"00000001803f011020304012345678ff80246800f8");
		const QString path = directory.filePath(QStringLiteral("saved.ship"));
		QFile file(path);
		Check(file.open(QIODevice::WriteOnly) && file.write("original") == 8, "prepare existing destination");
		file.close();
		Check(!Save(path, design, error) && ReadFile(path) == "original", "invalid design cannot damage existing file");
		Check(Save(path, Fixture(), error) && ReadFile(path) == preparedGolden,
			"atomic save prepares working records and replaces exact file bytes");
		Check(!Save(directory.filePath(QStringLiteral("missing/ship.ship")), Fixture(), error), "missing directory fails");
		Check(!Save(directory.path(), Fixture(), error), "directory cannot be overwritten as a file");
		Check(ReadFile(path) == preparedGolden, "failed saves leave previous destination intact");
	}
}
}

int main(int argc, char** argv)
{
	QCoreApplication application(argc, argv);
	const QStringList args = application.arguments();
	if(args.size() == 4 && args[1] == QStringLiteral("--rewrite"))
	{
		Design design;
		QString error;
		if(!Load(args[2], design, error) || !Save(args[3], design, error))
		{
			QTextStream(stderr) << error << '\n';
			return 1;
		}
		return 0;
	}
	if(args.size() != 1 && !(args.size() == 2 && args[1] == QStringLiteral("--self-test")))
	{
		QTextStream(stderr) << "Usage: native_ship_writer [--self-test | --rewrite input.ship output.ship]\n";
		return 2;
	}
	SelfTest();
	QTextStream(stdout) << (failures ? "FAILED" : "PASS") << ": native legacy writer checks\n";
	return failures ? 1 : 0;
}
