#ifndef AuLegacyFinanceTransfer_H
#define AuLegacyFinanceTransfer_H

#include "auatom.h"
#include <array>

class DaPerson;
class DaVessel;
class QMutex;

// Hold before reading a balance that will fund a transfer, through the final
// debit/credit. Actor-local recursive locks allow the normal account setters
// to participate without serializing unrelated city economy work. Modern
// generations acquire no lock. Callers must enter before actor/city locks.
class AUATOM_DLL AuLegacyFinanceTransfer
{
public:
	explicit AuLegacyFinanceTransfer(const DaPerson* person, const DaVessel* vessel = nullptr);
	explicit AuLegacyFinanceTransfer(const DaVessel* vessel, const DaVessel* other = nullptr);
	AuLegacyFinanceTransfer(const DaPerson* seller, const DaPerson* buyer,
		const DaVessel* sellerShip, const DaVessel* buyerShip);
	~AuLegacyFinanceTransfer();
	AuLegacyFinanceTransfer(const AuLegacyFinanceTransfer&) = delete;
	AuLegacyFinanceTransfer& operator=(const AuLegacyFinanceTransfer&) = delete;

private:
	void Lock(QMutex* first, QMutex* second, QMutex* third = nullptr, QMutex* fourth = nullptr);
	std::array<QMutex*, 4> m_Locks = {};
	int m_Count = 0;
};

#endif
