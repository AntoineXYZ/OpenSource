#include "aulegacycitypopulation.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycitystate.h"
#include <QDataStream>
#include <QIODevice>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace AuLegacyCity
{
namespace
{
template<class T> double Sum(const T& values) { return std::accumulate(values.begin(), values.end(), 0.0); }
template<class T> bool ValidMoney(const T& values)
{
	for(double amount : values) if(!std::isfinite(amount)) return false;
	return true;
}
template<class T> void WriteValues(QDataStream& out, const T& values) { for(double amount : values) out << amount; }
template<class T> void ReadValues(QDataStream& in, T& values) { for(double& amount : values) in >> amount; }
bool Valid(const AccountingState& state)
{
	const Accounts& a = state.accounts;
	return std::isfinite(a.citizens) && std::isfinite(a.government)
		&& ValidMoney(a.citizenExpenses) && ValidMoney(a.citizenIncome)
		&& ValidMoney(a.governmentExpenses) && ValidMoney(a.governmentIncome)
		&& std::isfinite(state.lastTribute) && state.lastTribute >= 0
		&& std::isfinite(state.taxes.income) && state.taxes.income >= 0 && state.taxes.income <= 100
		&& std::isfinite(state.taxes.sales) && state.taxes.sales >= 0 && state.taxes.sales <= 100
		&& std::isfinite(state.taxes.subsidy) && state.taxes.subsidy >= 0 && state.taxes.subsidy <= 100;
}
}

void Accounts::Credit(CitizenIncome category, double amount, const TaxRates& taxes)
{
	if(!(amount > 0) || !std::isfinite(amount) || category >= CitizenIncome::Count) return;
	float rate = 0;
	GovernmentIncome taxCategory = GovernmentIncome::IncomeTax;
	if(category == CitizenIncome::Salary) rate = taxes.income;
	else if(category == CitizenIncome::Sales) { rate = taxes.sales; taxCategory = GovernmentIncome::SalesTax; }
	if(rate > 0)
	{
		const double tax = (double(rate) * amount) / 100.0;
		Credit(taxCategory, tax, taxes.hasCapital);
		amount -= tax;
	}
	citizens += amount;
	citizenIncome[std::size_t(category)] += amount;
}

void Accounts::Credit(GovernmentIncome category, double amount, bool hasCapital)
{
	if(!hasCapital || !(amount > 0) || !std::isfinite(amount) || category >= GovernmentIncome::Count) return;
	government += amount;
	governmentIncome[std::size_t(category)] += amount;
}

double Accounts::Debit(CitizenExpense category, double amount)
{
	if(!(amount > 0) || !std::isfinite(amount) || category >= CitizenExpense::Count) return 0;
	// The original Commission routine compares against the treasury, then
	// subtracts from citizens. Verified in assembly at 180289912..180289953.
	const double available = category == CitizenExpense::Commission ? government : citizens;
	const double taken = available <= amount ? available : amount;
	if(available <= amount) citizens = 0;
	else citizens -= amount;
	citizenExpenses[std::size_t(category)] += taken;
	return taken;
}

double Accounts::Debit(GovernmentExpense category, double amount)
{
	if(!(amount > 0) || !std::isfinite(amount) || category >= GovernmentExpense::Count) return 0;
	const double taken = government <= amount ? government : amount;
	if(government <= amount) government = 0;
	else government -= amount;
	governmentExpenses[std::size_t(category)] += taken;
	return taken;
}

double Accounts::BeginningCitizenBalance() const { return citizens - Sum(citizenIncome) + Sum(citizenExpenses); }
double Accounts::BeginningGovernmentBalance() const { return government - Sum(governmentIncome) + Sum(governmentExpenses); }

double Accounts::LoanFundsAvailable(int population) const
{
	const double available = citizens - (double(population) + 1.0) * 1000.0;
	if(available < 0) return 0;
	const double thousands = std::trunc(((available > 0 ? 500.0 : -500.0) + available) / 1000.0);
	return thousands == 0 ? 0 : thousands * 1000.0;
}

void Accounts::ResetPeriod()
{
	citizenExpenses.fill(0); citizenIncome.fill(0);
	governmentExpenses.fill(0); governmentIncome.fill(0);
}

void Accounts::CirculateSalaries(int population, int jobs, const TaxRates& taxes)
{
	if(population <= 0 || citizens <= 0) return;
	float salary = float(std::min(population, jobs)) * 25.0f;
	if(citizens < double(salary)) salary = float(citizens);
	Debit(CitizenExpense::Salary, double(salary));
	Credit(CitizenIncome::Salary, double(salary), taxes);
	const double sales = double(salary * 0.5f) * double(1.0f - taxes.sales / 100.0f);
	Debit(CitizenExpense::Purchases, sales);
	Credit(CitizenIncome::Sales, sales, taxes);
}

bool EncodeAccounting(const AccountingState& state, QByteArray& output, QString& error)
{
	error.clear();
	if(!Valid(state)) { error = QStringLiteral("Invalid legacy city accounts."); return false; }
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	const Accounts& a = state.accounts;
	out << quint16(0x4c41) << quint8(1) << a.citizens << a.government;
	WriteValues(out, a.citizenExpenses); WriteValues(out, a.citizenIncome);
	WriteValues(out, a.governmentExpenses); WriteValues(out, a.governmentIncome);
	out << state.unemployed << state.foodPerMeal << state.lastPopulationCycle << state.lastEconomyCycle
		<< state.lastTribute << state.tributeSentToday << state.taxes.income << state.taxes.sales
		<< state.taxes.subsidy << state.taxes.hasCapital << state.powerRequested;
	if(out.status() != QDataStream::Ok) { error = QStringLiteral("Could not encode legacy city accounts."); return false; }
	output = bytes;
	return true;
}

bool DecodeAccounting(const QByteArray& bytes, AccountingState& output, QString& error)
{
	error.clear();
	if(bytes.isEmpty()) { output = AccountingState(); return true; }
	if(bytes.size() > 1024) { error = QStringLiteral("Legacy city accounts exceed their size limit."); return false; }
	QDataStream in(bytes);
	in.setVersion(QDataStream::Qt_4_1);
	quint16 magic = 0;
	quint8 version = 0;
	AccountingState state;
	Accounts& a = state.accounts;
	in >> magic >> version >> a.citizens >> a.government;
	ReadValues(in, a.citizenExpenses); ReadValues(in, a.citizenIncome);
	ReadValues(in, a.governmentExpenses); ReadValues(in, a.governmentIncome);
	in >> state.unemployed >> state.foodPerMeal >> state.lastPopulationCycle >> state.lastEconomyCycle
		>> state.lastTribute >> state.tributeSentToday >> state.taxes.income >> state.taxes.sales
		>> state.taxes.subsidy >> state.taxes.hasCapital >> state.powerRequested;
	if(magic != 0x4c41 || version != 1 || in.status() != QDataStream::Ok || !in.atEnd() || !Valid(state))
	{ error = QStringLiteral("Invalid legacy city accounting envelope."); return false; }
	output = state;
	return true;
}

int Homes(const AuLegacyCityCatalogue& catalogue, const std::array<quint32, 256>& levels, int* apartmentHomes)
{
	int homes = 0;
	if(apartmentHomes) *apartmentHomes = 0;
	for(quint8 industry : {20, 21, 22, 30, 32})
	{
		const IndustryRecord* record = catalogue.Industry(IndustryId(industry));
		if(!record) continue;
		const int count = int(levels[industry]) * int(record->populationPerLevel);
		homes += count;
		if(apartmentHomes && industry == 22) *apartmentHomes = count;
	}
	return homes;
}

PopulationCycleResult PopulationCycle(const PopulationContext& c)
{
	PopulationCycleResult r;
	r.population = c.population;
	r.unemployed = c.unemployed;
	r.satisfaction = c.satisfaction;
	int change = 0;
	bool airShortage = false, foodShortage = false;
	if(c.population > 0)
	{
		if(c.needsEnclosure)
		{
			change -= c.hasPower ? 1 : 2;
			if(c.hasAir)
			{
				if(c.airConsumed < quint32(c.population))
				{
					change -= c.airConsumed < (quint32(c.population) >> 1) ? 2 : 1;
					airShortage = true;
				}
			}
			else
			{
				change -= 5;
				r.suffocated = std::min(c.population, std::max(4, c.population / 10));
				airShortage = true;
			}
		}
		if(c.foodVariety == 0)
		{
			change -= 5;
			r.starved = std::min(c.population, std::max(4, c.population / 10));
			foodShortage = true;
		}
		else if(!c.ateFullMeal) { change -= 2; foodShortage = true; }
		else if(c.ecoRole == 0 && c.foodVariety >= 7) change += 2;
		else if(c.ecoRole <= 3) change += std::min(2, int(c.foodVariety >> 2));
		if(c.occupied) change -= int(c.loyalty * 10.0f * 0.5f);
		else if(c.loyalty > 0.9f) change += 2;
		else if(c.loyalty > 0.5f) change += 1;
		if(c.differentNativeRace) --change;
		if(c.environmentCapacity < c.environmentPopulation)
		{
			const int step = c.environmentCapacity * 5 / 100;
			// A capacity below twenty is outside the original world's valid range.
			if(step > 0) change -= (step - 1 + c.environmentPopulation - c.environmentCapacity) / step;
		}
		if(c.population > c.homes && !c.destroyedBuildings) change -= std::min(10, (c.population - c.homes) / 4);
		change -= c.decayedLevels;
		if(!c.exemptFromVisitPenalty && c.daysSinceSystemVisit > 6 && c.daysSinceSystemVisit < 1001)
			change -= (c.daysSinceSystemVisit - 7) / 7;
		if(c.destroyedBuildings) --change;
		else
		{
			const int otherHomes = c.homes - c.apartmentHomes;
			if(c.homes && otherHomes < c.apartmentHomes)
				change -= otherHomes * 3 < c.apartmentHomes ? 3 : (otherHomes * 2 < c.apartmentHomes ? 2 : 1);
			const float unemployment = float(c.unemployed) / float(c.population);
			const int penalty = int(double(unemployment) * 5.0);
			if(penalty) change -= penalty;
			else if(unemployment == 0 && c.population >= c.homes) --change;
			const int arena = int(c.levels[112]) - c.population / 175;
			if(arena < 0) change += std::max(-2, arena);
			const int requiredTroops = c.townSquareTroops + c.militaryFlagTroops * int(c.levels[10]);
			if(c.townSquareTroops < requiredTroops && c.militaryFlagTroops)
			{
				const int military = (c.population / 30 - requiredTroops + c.townSquareTroops) / c.militaryFlagTroops;
				if(military < 0) change += military;
			}
			const int cantina = std::max(-2, int(c.levels[105]) - c.population / 50);
			if(cantina < 0 || (!foodShortage && !airShortage)) change += std::min(1, cantina);
			const int casino = int(c.levels[102]) - c.population / 200;
			if(casino < 0) change += std::max(-2, casino);
			const int church = std::max(-2, int(c.levels[120]) - c.population / 45);
			change += std::min(foodShortage || airShortage ? 1 : 2, church);
			const int grocery = int(c.levels[103]) - c.population / 100;
			if(grocery < 0) change += std::max(-2, grocery);
			r.hospitalShortage = std::max(0, std::min(2, c.population / 80 - int(c.levels[121])));
			change -= r.hospitalShortage;
			struct Service { quint8 industry; int citizensPerLevel; };
			for(const Service service : {Service{114, 90}, Service{124, 60}, Service{104, 55}, Service{125, 70}, Service{113, 150}})
			{
				const int deficit = int(c.levels[service.industry]) - c.population / service.citizensPerLevel;
				if(deficit < 0) change += std::max(-2, deficit);
			}
		}
	}
	r.moraleChange = change;
	r.satisfaction = c.population == 0 ? 1 : std::max(-20, std::min(20, c.satisfaction + change));
	auto decrease = [&r](int amount)
	{
		amount = std::min(r.population, amount);
		r.population -= amount;
		r.unemployed = std::max(0, r.unemployed - amount);
	};
	auto increase = [&r](int amount) { r.population += amount; r.unemployed += amount; };
	decrease(r.suffocated);
	r.starved = std::min(r.population, r.starved);
	decrease(r.starved);
	const bool noImmigrationRoute = c.isolated && !c.levels[111];
	if(r.population > c.homes)
	{
		if(c.destroyedBuildings || r.satisfaction >= 0) return r;
		int leaving = -r.satisfaction;
		if(!c.indigenousWorld)
		{
			if(c.needsEnclosure)
			{
				if(noImmigrationRoute) leaving = (leaving + 3) / 4;
				else leaving = std::max(1, leaving / 2);
			}
			else leaving = std::max(1, leaving * 2 / 3);
		}
		if(c.occupied && !(c.needsEnclosure && !c.indigenousWorld && noImmigrationRoute)) return r;
		r.emigrants = std::min(r.population, leaving);
		decrease(r.emigrants);
		return r;
	}
	if(r.population < c.homes && r.population > 1)
	{
		int births = std::min(2, c.homes - r.population);
		if(r.hospitalShortage > 0 && births == 2) births = 2 - r.hospitalShortage;
		else if(births && c.population / 80 == 0) births = 1;
		r.births = births;
		increase(births);
	}
	int migration = r.satisfaction;
	if((migration < 0 && c.occupied) || (migration > 0 && c.underAttack)
		|| (c.destroyedBuildings && migration < 0 && r.population != 0)) migration = 0;
	if(!c.indigenousWorld)
	{
		if(c.needsEnclosure)
		{
			if(noImmigrationRoute)
			{
				if(migration > 0) migration = 0;
				else if(migration < 0) migration = -((-migration + 3) / 4);
			}
			else
			{
				if(migration > 1 && c.occupied) migration /= 2;
				if(migration > 1 || migration < -1) migration /= 2;
			}
		}
		else
		{
			if(migration > 0 && noImmigrationRoute) migration = 0;
			else if(migration > 1)
			{
				if(c.occupied) migration /= 2;
				if(migration > 1) migration = std::max(1, migration * 2 / 3);
			}
			else if(migration < -1) migration = std::min(-1, migration * 2 / 3);
		}
	}
	else if(c.differentNativeRace && migration > 0)
	{
		if(noImmigrationRoute) migration = 0;
		else if(migration > 1)
		{
			if(c.occupied) migration /= 2;
			if(migration > 1) migration = std::max(1, migration * 3 / 4);
		}
	}
	if(migration < 0) { r.emigrants = std::min(r.population, -migration); decrease(r.emigrants); }
	else if(migration > 0 && r.population < c.homes)
	{
		r.immigrants = std::min(migration, c.homes - r.population);
		increase(r.immigrants);
	}
	return r;
}

bool ApplyCitySurrender(State& state, Accounts& accounts, quint32 oldEmpire, quint32 newEmpire)
{
	if(oldEmpire == newEmpire || state.flags.isEmpty()) return false;
	// Original surrender requires all flags to belong to the surrendering
	// empire, or an existing flag of the receiving empire.
	bool allOld = true, hasNew = false;
	for(const auto& flag : state.flags)
	{ allOld = allOld && flag.empire == oldEmpire; hasNew = hasNew || flag.empire == newEmpire; }
	if(!allOld && !hasNew) return false;
	for(auto& flag : state.flags) if(flag.empire == oldEmpire) flag.empire = newEmpire;
	if(state.controllingEmpire != oldEmpire) return false;
	state.controllingEmpire = newEmpire;
	if(state.allegianceEmpire == oldEmpire) state.allegianceEmpire = newEmpire;
	const float loyalty = state.population <= int(state.loyalPopulation) ? 1.0f
		: state.population ? float(state.loyalPopulation) / float(state.population) : 0.0f;
	const double bankRun = accounts.citizens * (state.allegianceEmpire == state.controllingEmpire ? double(1.0f - loyalty) : 1.0);
	if(bankRun >= 1) accounts.Debit(CitizenExpense::Withdrawn, bankRun);
	return true;
}

WealthLevyResult LevyCitizenWealth(Accounts& accounts, const WealthLevyContext& c)
{
	WealthLevyResult r;
	r.loyalPopulation = c.loyalPopulation;
	if(!c.hasCapital || c.underAttack || c.inArena || !c.empireMatches
		|| !std::isfinite(c.fraction) || c.fraction < 0 || c.fraction > 1 || c.population < 0) return r;
	r.applied = true;
	quint32 disloyal = quint32(float(c.population) * c.fraction * 2.0f);
	if(c.occupied)
	{
		if(c.loyalPopulation < quint32(c.population) && quint64(c.loyalPopulation) + disloyal <= quint32(c.population))
			r.loyalPopulation += disloyal;
		else
		{
			r.loyalPopulation = std::max(c.loyalPopulation, quint32(c.population));
			disloyal = std::min(disloyal, quint32(c.population));
			r.revolution = disloyal != 0;
		}
	}
	else if(c.loyalPopulation < disloyal)
	{
		disloyal -= c.loyalPopulation;
		r.loyalPopulation = 0;
		r.revolution = true;
	}
	else r.loyalPopulation -= disloyal;
	if(r.revolution)
	{
		const float fraction = c.population > 0 ? std::min(1.0f, float(disloyal) / float(c.population)) : 0.0f;
		const double bounty = accounts.citizens * double(fraction);
		if(c.hasGovernors && bounty >= 1) r.bounty = accounts.Debit(CitizenExpense::Purchases, bounty);
		r.loyalPopulation = std::min(r.loyalPopulation, quint32(c.population));
	}
	else
	{
		const double confiscated = accounts.citizens * double(c.fraction);
		if(confiscated >= 1 && c.hasBank)
		{
			r.confiscated = accounts.Debit(CitizenExpense::Confiscated, confiscated);
			accounts.Credit(GovernmentIncome::Confiscated, confiscated, c.hasCapital);
		}
	}
	const double withdrawn = accounts.citizens * double(c.fraction);
	if(!c.deferFinalWithdrawal && withdrawn >= 1) r.withdrawn = accounts.Debit(CitizenExpense::Withdrawn, withdrawn);
	return r;
}
}
