#ifndef AuLegacyScDeckPlan_H
#define AuLegacyScDeckPlan_H

#include "audata.h"
#include "aulegacyship.h"
#include "aupoint.h"

#include <QDataStream>

// Immutable plan geometry for the recovered in-world legacy deck view. The
// packed authoring records retain the designer's colours, names and icons;
// source-file metadata and modules are not part of this replicated snapshot.
class AUDATA_DLL AuLegacyScDeckPlan
{
public:
	static constexpr quint32 MaximumSerializedBytes = 2 * 1024 * 1024;
	AuLegacyScDeckPlan() = default;
	AuLegacyScDeckPlan(const AuLegacyShip::Design& design, const AuPoint& authorCenter);

	bool IsValid() const { return m_Valid; }
	const AuLegacyShip::Design& SourceDesign() const { return m_SourceDesign; }
	const AuPoint& AuthorCenter() const { return m_AuthorCenter; }
	quint64 MemoryUsage() const;

	void operator>>(QDataStream& out) const;
	// Failure leaves the previous snapshot unchanged and marks the stream corrupt.
	void operator<<(QDataStream& in);

private:
	bool Validate() const;
	bool m_Valid = false;
	AuLegacyShip::Design m_SourceDesign;
	AuPoint m_AuthorCenter = AuPoint(0, 0, 0);
};

inline QDataStream& operator<<(QDataStream& out, const AuLegacyScDeckPlan& plan)
{
	plan.operator>>(out);
	return out;
}

inline QDataStream& operator>>(QDataStream& in, AuLegacyScDeckPlan& plan)
{
	plan.operator<<(in);
	return in;
}

#endif // AuLegacyScDeckPlan_H
