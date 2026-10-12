#ifndef AuLegacyDestructionTrace_H
#define AuLegacyDestructionTrace_H

#include "auglobal.h"
#include "auid.h"
#include <QMutex>
#include <QMutexLocker>
#include <QString>

namespace AuLegacyDestructionTrace
{
// Temporary diagnostics for destruction while aboard a legacy ship.
constexpr bool Enabled = true;

inline void Log(const QString& stage, const AuId& shipId, const QString& detail = QString())
{
	if(!Enabled) return;
	const QString message = QStringLiteral("Legacy destruction trace: stage=%1 ship=%2 timeMSec=%3 %4")
		.arg(stage).arg(shipId.GetID()).arg(g_Au.CoordinatedTime().MSecsSinceEpoch()).arg(detail);
	if(g_Au.IsSceneClient())
	{
		// Model, network and view threads share this diagnostic file.
		static QMutex fileMutex;
		QMutexLocker guard(&fileMutex);
		g_Au.Log(g_Au.CacheApp() + QStringLiteral("/legacy-destruction.log"), message);
	}
	else
		g_Au.Log(message);
}
}

#endif // AuLegacyDestructionTrace_H
