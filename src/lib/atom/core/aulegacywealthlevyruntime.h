#ifndef AuLegacyWealthLevyRuntime_H
#define AuLegacyWealthLevyRuntime_H
#include "auatom.h"
#include <QByteArray>

// Applies once on each owning worker, including offline resident avatars.
AUATOM_DLL void ApplyLegacyWealthLevy(const QByteArray& order);
#endif
