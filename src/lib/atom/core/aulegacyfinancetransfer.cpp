#include "aulegacyfinancetransfer.h"
#include "auglobal.h"
#include "daperson.h"
#include "davessel.h"
#include <QMutex>
#include <algorithm>
#include <functional>

AuLegacyFinanceTransfer::AuLegacyFinanceTransfer(const DaPerson* person, const DaVessel* vessel)
{
	if(g_Au.IsLegacyGeneration())
		Lock(person ? &person->m_LegacyFinanceMutex : nullptr, vessel ? &vessel->m_LegacyFinanceMutex : nullptr);
}

AuLegacyFinanceTransfer::AuLegacyFinanceTransfer(const DaVessel* vessel, const DaVessel* other)
{
	if(g_Au.IsLegacyGeneration())
		Lock(vessel ? &vessel->m_LegacyFinanceMutex : nullptr, other ? &other->m_LegacyFinanceMutex : nullptr);
}

AuLegacyFinanceTransfer::AuLegacyFinanceTransfer(const DaPerson* seller, const DaPerson* buyer,
	const DaVessel* sellerShip, const DaVessel* buyerShip)
{
	if(g_Au.IsLegacyGeneration())
		Lock(seller ? &seller->m_LegacyFinanceMutex : nullptr, buyer ? &buyer->m_LegacyFinanceMutex : nullptr,
			sellerShip ? &sellerShip->m_LegacyFinanceMutex : nullptr, buyerShip ? &buyerShip->m_LegacyFinanceMutex : nullptr);
}

void AuLegacyFinanceTransfer::Lock(QMutex* first, QMutex* second, QMutex* third, QMutex* fourth)
{
	std::array<QMutex*, 4> ordered = {{first, second, third, fourth}};
	std::sort(ordered.begin(), ordered.end(), std::less<QMutex*>());
	QMutex* previous = nullptr;
	for(QMutex* mutex : ordered)
		if(mutex && mutex != previous)
		{
			mutex->lock();
			m_Locks[m_Count++] = mutex;
			previous = mutex;
		}
}

AuLegacyFinanceTransfer::~AuLegacyFinanceTransfer()
{
	while(m_Count) m_Locks[--m_Count]->unlock();
}
