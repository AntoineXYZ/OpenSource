#ifndef AuLegacyScDesign_H
#define AuLegacyScDesign_H

#include "audata.h"
#include "aulegacyshipstats.h"
#include "aupoint.h"

// A server-accepted legacy payload. Original file metadata is provenance only;
// Id() is allocated by the legacy design database, in its own ID namespace.
// Publish as QSharedPointer<const AuLegacyScDesign> after assigning that ID.
class AUDATA_DLL AuLegacyScDesign
{
public:
	// Structural acceptance only. Geometry/statistics are recomputed locally;
	// diagnostics remain advisory until online semantic validation is added.
	// Failure leaves output unchanged.
	static bool Read(const QByteArray& source, AuLegacyScDesign& output, QString& error);
	// Online intake adds limits on file size, decks and analysis work.
	static bool ReadUpload(const QByteArray& source, AuLegacyScDesign& output, QString& error);

	quint32 Id() const { return m_Id; }
	void SetId(quint32 id) { m_Id = id; }
	const QByteArray& SourceData() const { return m_SourceData; }
	const QByteArray& ContentHash() const { return m_ContentHash; }
	// Display name is database metadata; SourceDesign retains the embedded name.
	const QString& Name() const { return m_Name; }
	void SetName(const QString& name) { m_Name = name; }
	const AuLegacyShip::Design& SourceDesign() const { return m_SourceDesign; }
	const AuLegacyShip::Stats& Stats() const { return m_Stats; }

	// Feet, z up. Original packed coordinates never change. This is the recovered
	// hull-bounds midpoint, rather than a mass-weighted center of gravity.
	const AuPoint& AuthorCenter() const { return m_AuthorCenter; }
	AuPoint AuthorToShip(const AuPoint& point) const { return point - m_AuthorCenter; }
	AuPoint ShipToAuthor(const AuPoint& point) const { return point + m_AuthorCenter; }

	// One-based identities in the immutable source record order. These survive
	// reloads even when original detail IDs have gaps, duplicates or large values.
	// They are separate from the later 16-bit station/subsystem identifiers.
	quint32 DetailRecordId(int sourceIndex) const;
	const AuLegacyShip::Detail* DetailRecord(quint32 recordId) const;

private:
	static bool ReadSource(const QByteArray& source, AuLegacyScDesign& output, QString& error, bool upload);
	quint32 m_Id = 0;
	QString m_Name;
	QByteArray m_SourceData;
	QByteArray m_ContentHash;
	AuLegacyShip::Design m_SourceDesign;
	AuLegacyShip::Stats m_Stats;
	AuPoint m_AuthorCenter = AuPoint(0, 0, 0);
};

#endif // AuLegacyScDesign_H
