#include "dgdata.h"
#include "auglobal.h"

quint32 DgData::SelectLegacyPirateEmpire() const
{
	// DgData::SelectPirateEmpire, AuData13 0x1402c0.
	QList<quint32> pirates, all;
	quint32 agent = 0;
	for(const auto& empire : m_gEmpires)
	{
		all.append(empire.first);
		if(empire.second.Role() == EMPIRE_Pirate) pirates.append(empire.first);
		else if(empire.second.Role() == EMPIRE_Agent) agent = empire.first;
	}
	if(pirates.isEmpty())
	{
		if(agent) return agent;
		pirates = all;
	}
	if(pirates.isEmpty()) return 0;
	return pirates.at(pirates.size() == 1 ? 0 : g_Au.Dice().Roll(1, pirates.size(), -1));
}
