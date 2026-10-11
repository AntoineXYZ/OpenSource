#ifndef AuLegacyScStats_H
#define AuLegacyScStats_H

#include "audata.h"
#include "aulegacyshipperformance.h"

#include <QDataStream>

// Authoritative numeric snapshot of the recovered 2016 calculations. Quantities
// retain their original feet, cubic feet, pounds, and zero-based technology units.
// Performance HP are display points; native system hits are those values / 10.
// Source geometry, manufacturing recipes, diagnostics, and report HTML stay out
// of this persisted and replicated payload.
class AUDATA_DLL AuLegacyScStats
{
public:
	AuLegacyScStats() = default;
	explicit AuLegacyScStats(const AuLegacyShip::Design& design);
	AuLegacyScStats(const AuLegacyShip::Design& design, const AuLegacyShip::Stats& stats);

	bool IsValid() const { return m_Valid; }
	const AuLegacyShip::Header& Header() const { return m_Header; }
	const AuLegacyShip::Stats& Stats() const { return m_Stats; }
	const AuLegacyShip::Performance& Performance() const { return m_Performance; }

	void operator>>(QDataStream& out) const;
	void operator<<(QDataStream& in);
	bool operator==(const AuLegacyScStats& other) const;
	bool operator!=(const AuLegacyScStats& other) const { return !(*this == other); }
	quint64 MemoryUsage() const;

private:
	bool Validate() const;
	bool m_Valid = false;
	AuLegacyShip::Header m_Header;
	AuLegacyShip::Stats m_Stats;
	AuLegacyShip::Performance m_Performance;
};

inline QDataStream& operator<<(QDataStream& out, const AuLegacyScStats& stats)
{
	stats.operator>>(out);
	return out;
}

inline QDataStream& operator>>(QDataStream& in, AuLegacyScStats& stats)
{
	stats.operator<<(in);
	return in;
}

#endif // AuLegacyScStats_H
