#include "aulegacywealthlevyruntime.h"
#include "aulegacyfinancetransfer.h"
#include "auether.h"
#include "auglobal.h"
#include "aulegacywealthlevy.h"
#include "auqueryenvironment.h"
#include "daatomlist.h"
#include "dalegacycity.h"
#include "daperson.h"
#include "dasc.h"
#include "dgdata.h"
#include <QSet>
#include <functional>

namespace
{
bool ExcludedShip(const DaSc& ship, const AuLegacyCity::WealthLevyOrder& order)
{
	return ship.EmpireId() == order.empire && (order.exemptions & (ship.IsPrivatelyOwned() ? 4 : 8));
}

DaLegacyCity* AccountCity(DaAtom& atom, AuEther& ether)
{
	// A person aboard a vessel shares its enclosing city. Preserve recursive
	// original traversal's exclusion of a ship together with its occupants.
	for(DaAtom* place = &atom; place; place = place->Parent())
	{
		if(!place->DoesQueryEnvironment()) continue;
		const auto* environment = place->LockEnvironmentQuery();
		const AuId city = environment->LegacyCityId(); place->UnlockEnvironmentQuery();
		if(!city.IsNull()) return dynamic_cast<DaLegacyCity*>(ether.FindNormalAtom(city));
	}
	return nullptr;
}

void Notify(const AuLegacyCity::WealthLevyOrder& order, DaAtom& asset, const AuId& recipient,
	double taken, const QString& explanation, const DaLegacyCity* city)
{
	if(recipient.IsNull()) return;
	QString empireName;
	const DgData& gd = g_Au.LockDataForRead();
	gd.EmpireName(order.empire, empireName);
	const quint32 player = gd.PlayerId(), confirmation = gd.ConfirmationNumber();
	g_Au.UnlockData();
	const QString result = taken >= 1
		? QObject::tr("%L1%2 was taken from the account of %3 and transferred to the bank in %4.")
			.arg(taken).arg(g_Au.CurrencySymbol()).arg(asset.Name().toHtmlEscaped(), city ? city->Name().toHtmlEscaped() : QString())
		: QObject::tr("No money was taken from the account of %1. %2").arg(asset.Name().toHtmlEscaped(), explanation);
	const QString body = QObject::tr("%1<br><br>Re: Executive Order to Levy Wealth Tax<br><br>"
		"%2 (%3), using the power vested in the leader of %4, issued an executive order to levy a tax of %5% on all private wealth."
		"<br><br>%6<br><br>The following public statement accompanied this decree.<br><br><b>%7</b>"
		"<br><br>Service Plus Enforcement Equals Compliance.<br><i>Treasury Department</i>")
		.arg(g_Au.EncodeUTCString()).arg(order.issuerName.toHtmlEscaped(), order.issuer.StringId(), empireName.toHtmlEscaped())
		.arg(order.percent).arg(result, order.statement.toHtmlEscaped());
	g_Au.SendMail(player, confirmation, AuId(1), QObject::tr("Treasury Department"), recipient,
		AuMailMsg::MSG_Government, QObject::tr("Tax Levy"), body,
		AuId(), QString(), AuId(), QString(), asset.Galaxy(), AuPoint());
}
}

void ApplyLegacyWealthLevy(const QByteArray& bytes)
{
	AuLegacyCity::WealthLevyOrder order;
	if(!g_Au.IsLegacyGeneration() || !AuLegacyCity::DecodeWealthLevy(bytes, order)
		|| !order.empire || order.issuer.IsNull()) return;
	AuEther& ether = g_Au.LockEtheriumDelete();
	DaAtomList atoms; ether.ListNormalAtoms(atoms);
	QSet<DaAtom*> visited;
	std::function<void(DaAtom*)> visit = [&](DaAtom* atom) {
		if(!atom || !atom->IsNormal() || visited.contains(atom)) return;
		visited.insert(atom);
		if(auto* city = dynamic_cast<DaLegacyCity*>(atom)) { city->ApplyWealthLevy(order); return; }
		auto* person = dynamic_cast<DaPerson*>(atom);
		auto* ship = dynamic_cast<DaSc*>(atom);
		if(atom->EmpireId() == order.empire && ((person && (order.exemptions & 1)) || (ship && ExcludedShip(*ship, order)))) return;
		// Original 0x3287f0/0x3d9970 do not test exemption bit 2. Preserve
		// that observed behavior instead of inventing a new government filter.
		if(atom->EmpireId() == order.empire && (person || ship))
		{
			// Serialize the balance test, city credit and final account debit
			// against bank transfers and normal account mutations of this asset.
			AuLegacyFinanceTransfer transfer(person, ship);
			const AuId recipient = person ? person->Id() : ship->CaptainId();
			auto* city = AccountCity(*atom, ether);
			QString reason; double taken = 0;
			if(!city) reason = QObject::tr("You are not in a city.");
			else if(city->EmpireId() != order.empire) reason = QObject::tr("The city belongs to another empire.");
			else
			{
				const double balance = person ? person->AccountBalance() : ship->AccountBalance();
				const double levy = balance * double(float(order.percent) / 100.0f);
				if(levy < 1) reason = QObject::tr("The account is empty.");
				else if(city->ReceiveConfiscatedWealth(order.empire, levy))
				{
					if(person) person->DebitAccount(levy); else ship->DebitAccount(levy);
					atom->SetModified(); taken = levy;
				}
				else reason = QObject::tr("The city has no bank or has not recognized an empire capital.");
			}
			Notify(order, *atom, recipient, taken, reason, city);
		}
		DaAtomList children; atom->ListNormalSubatoms(children);
		for(DaAtom* child : children) visit(child);
	};
	// Preserve DaAtom/DaAtomList's pre-order traversal and subtree exclusions.
	// Visiting a flat hash would make a city revolution and its residents'
	// account levies occur in an unrelated order.
	for(DaAtom* atom : atoms)
		if(!atom->Parent() || !atom->Parent()->IsNormal()) visit(atom);
	g_Au.UnlockEtheriumDelete();
}
