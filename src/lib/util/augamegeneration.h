#ifndef AuGameGeneration_H
#define AuGameGeneration_H

#include <QString>
#include <QtGlobal>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

// Persisted values and protocol values. Append new generations; never renumber.
enum class AuGameGeneration : quint8
{
	Modern = 0,
	Legacy = 1,
};

inline QString AuGameGenerationName(AuGameGeneration generation)
{
	return generation == AuGameGeneration::Legacy
		? QStringLiteral("legacy") : QStringLiteral("modern");
}

inline QString AuGameGenerationLabel(AuGameGeneration generation)
{
	return generation == AuGameGeneration::Legacy
		? QStringLiteral("Legacy") : QStringLiteral("Modern");
}

inline bool AuParseGameGeneration(const QString& text, AuGameGeneration& generation)
{
	const QString name = text.trimmed().toLower();
	if(name == QStringLiteral("modern")) generation = AuGameGeneration::Modern;
	else if(name == QStringLiteral("legacy")) generation = AuGameGeneration::Legacy;
	else return false;
	return true;
}

// Advertised by scene clients and subordinate servers before gameplay data.
static constexpr quint8 AuGameGenerationProtocolVersion = 1;
static constexpr quint32 AuGameGenerationProtocolMagic = 0x4747454e; // GGEN

struct AuGameGenerationIdentity
{
	AuGameGeneration generation = AuGameGeneration::Modern;
	QString catalogueVersion;
	QByteArray manifestHash;

	bool operator==(const AuGameGenerationIdentity& other) const
	{
		return generation == other.generation && catalogueVersion == other.catalogueVersion
			&& manifestHash == other.manifestHash;
	}
	bool operator!=(const AuGameGenerationIdentity& other) const { return !(*this == other); }
};

inline void AuWriteGameGenerationIdentity(QDataStream& out, const AuGameGenerationIdentity& identity)
{
	out << AuGameGenerationProtocolMagic << AuGameGenerationProtocolVersion
		<< quint8(identity.generation) << identity.catalogueVersion << identity.manifestHash;
}

inline bool AuReadGameGenerationIdentity(QDataStream& in, AuGameGenerationIdentity& identity)
{
	if(in.status() != QDataStream::Ok || !in.device()) return false;
	const auto fail = [&in](QDataStream::Status status) {
		in.setStatus(status);
		return false;
	};
	quint32 magic = 0;
	quint8 version = 0, generation = 0;
	if(in.device()->bytesAvailable() < 6) return fail(QDataStream::ReadPastEnd);
	in >> magic >> version >> generation;
	if(in.status() != QDataStream::Ok || magic != AuGameGenerationProtocolMagic
		|| version != AuGameGenerationProtocolVersion || generation > quint8(AuGameGeneration::Legacy))
		return fail(QDataStream::ReadCorruptData);
	AuGameGenerationIdentity candidate;
	candidate.generation = AuGameGeneration(generation);
	// Qt_4_1/5 encode QString as a byte count followed by UTF-16 units, and
	// QByteArray as a byte count followed by raw bytes. Check before allocation;
	// 0xffffffff retains the original null-string/null-array semantics.
	quint32 stringBytes = 0;
	if(in.device()->bytesAvailable() < 4) return fail(QDataStream::ReadPastEnd);
	in >> stringBytes;
	if(stringBytes != 0xffffffff)
	{
		if(stringBytes > 256 || (stringBytes & 1)) return fail(QDataStream::ReadCorruptData);
		if(in.device()->bytesAvailable() < qint64(stringBytes)) return fail(QDataStream::ReadPastEnd);
		candidate.catalogueVersion.resize(int(stringBytes / 2));
		for(int index = 0; index < candidate.catalogueVersion.size(); ++index)
		{
			quint16 unit = 0;
			in >> unit;
			candidate.catalogueVersion[index] = QChar(unit);
		}
	}
	quint32 hashBytes = 0;
	if(in.status() != QDataStream::Ok || in.device()->bytesAvailable() < 4) return fail(QDataStream::ReadPastEnd);
	in >> hashBytes;
	if(hashBytes != 0xffffffff)
	{
		if(hashBytes > 32) return fail(QDataStream::ReadCorruptData);
		if(in.device()->bytesAvailable() < qint64(hashBytes)) return fail(QDataStream::ReadPastEnd);
		candidate.manifestHash.resize(int(hashBytes));
		if(hashBytes && in.readRawData(candidate.manifestHash.data(), int(hashBytes)) != int(hashBytes))
			return fail(QDataStream::ReadPastEnd);
	}
	const bool valid = candidate.generation == AuGameGeneration::Modern
		? candidate.catalogueVersion.isEmpty() && candidate.manifestHash.isEmpty()
		: !candidate.catalogueVersion.isEmpty() && candidate.manifestHash.size() == 32;
	if(in.status() != QDataStream::Ok || !valid) return fail(QDataStream::ReadCorruptData);
	identity = candidate;
	return true;
}

#endif
