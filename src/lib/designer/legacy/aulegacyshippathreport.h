#ifndef AuLegacyShipPathReport_H
#define AuLegacyShipPathReport_H

#include "aulegacyship.h"
#include <functional>

// Returning false from progress stops checking and preserves the partial report
// with the original yellow Canceled message.
QString AuLegacyShipPathReport(const AuLegacyShip::Design& design,
	const std::function<bool(const QString&, int, int)>& progress = {});

#endif
