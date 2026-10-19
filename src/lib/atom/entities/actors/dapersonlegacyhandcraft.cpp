#include "daperson.h"
#include "aulegacyhandcraft.h"
#include "auether.h"
#include "audnabase.h"
#include "auglobal.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinventory.h"
#include "auqueryenvironment.h"
#include "auskill.h"
#include "dalegacycity.h"
#include "davehicle.h"
#include "daworldll.h"
#include <memory>

namespace AuLegacyCity
{
HandcraftContext QueryHandcraftContext(DaPerson& person)
{
	HandcraftContext context;
	const auto* environment = person.LockEnvironmentQuery();
	const auto cityId = environment->LegacyCityId();
	const auto developmentId = environment->LegacyDevelopmentId();
	person.UnlockEnvironmentQuery();
	if(!person.Ether()) return context;
	const auto* city = dynamic_cast<const DaLegacyCity*>(person.Ether()->FindNormalAtom(cityId));
	if(!city || city->IsDead()) return context;
	const auto state = city->Snapshot();
	for(const auto& development : state.developments)
		if(development.id == developmentId && development.completedLevels && !development.demolished)
		{
			context.atIndustry = true; context.industry = development.industryId; context.technology = development.technology;
			context.hasPower = city->OnRingworld();
			if(!context.hasPower)
			{
				if(g_Au.IsSceneWorker())
				{
					// Original HasPower reads current stock. The replicated
					// capacitor summary can lag a manual material fetch.
					const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
					AuItemHash inventory; QString error;
					context.hasPower = catalogue && ReadCityInventory(state.inventory, *catalogue, inventory, error)
						&& inventory.RawCount(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(3)))) != 0;
				}
				else context.hasPower = state.capacitor > 0;
			}
			break;
		}
	return context;
}
}

void DaPerson::HandcraftLegacyItem(int recipeId)
{
	using namespace AuLegacyCity;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !catalogue || !Ether() || IsDead()
		|| recipeId <= 0 || recipeId > 65535) return;
	const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(quint16(recipeId)));
	const auto context = QueryHandcraftContext(*this);
	if(!recipe || !HandcraftAllowed(*recipe, context)) return;
	if((recipe->commodity.value == 342 || recipe->commodity.value == 349) && !IsInCapsule())
	{
		// Original 0x331090 permits a capsule's air, but open fires require
		// breathable air and cannot burn while visibly submerged.
		if(!IsAirBreathable()) { Thought(tr("The fire won't light without breathable air.")); return; }
		if(IsInVisibleWater()) { Thought(tr("The fire won't burn in water.")); return; }
	}
	ProductionProcess process;
	AuGear* gear = LockGearForWrite();
	if(!PrepareHandcraft(*catalogue, quint16(recipeId), *gear, context, process) || !CanRunProduction(process))
	{ UnlockGear(); return; }
	ProductionOutput output;
	if(!FinishHandcraft(*catalogue, process, output) || !output.quantity)
	{ UnlockGear(); return; }
	ConsumeHandcraft(*catalogue, process, *gear);
	const auto runtime = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(output.commodityId)));
	Stats().ItemHandCrafted(runtime, output.quality, context.atIndustry && context.industry == recipe->industry.value);
	if(output.commodityId == 157)
	{
		UpdateGearWeight(); UnlockGear(); CreditAccount(output.quantity);
		Thought(tr("%L1%2 handcrafted bullion added to my personal account.").arg(output.quantity).arg(g_Au.CurrencySymbol()));
		return;
	}
	if(output.unit && DaVehicle::Mass(runtime) > 0)
	{
		UpdateGearWeight(); UnlockGear();
		// Existing vehicle publication/mount mechanics are shared with modern
		// handcrafting. Recipes and item TL remain independently historical.
		auto* vehicle = new DaVehicle(runtime, output.quality);
		vehicle->ConnectSignals(); vehicle->Create(); vehicle->SetPosAndOrient(Pos()); Ether()->AddAtom(vehicle);
		vehicle->ResetHitPoints(); vehicle->SetEmpireId(EmpireId()); vehicle->SetInMotion();
		g_Au.AtomAdded(vehicle->Id()); vehicle->SetNormal(); ForceMountVehicle(vehicle);
		Thought(tr("Handcrafted %1 Q%2.").arg(vehicle->Name()).arg(output.quality)); vehicle->TickTockRecursive();
		return;
	}
	std::unique_ptr<AuItem> item(AuItem::Create(runtime, output.quality, EmpireId(), output.quantity));
	if(!item || !item->SetTechLevel(output.technology)) { UpdateGearWeight(); UnlockGear(); return; }
	if(auto* biological = dynamic_cast<AuItemDNA*>(item.get()))
	{
		std::unique_ptr<AuDNABase> dna(output.species.CreateDNA());
		if(dna) biological->SetDNA(dna.get(), ProductionDNAIsViable(output.commodityId, output.species), output.sentient, Mature);
	}
	QString response;
	if(output.commodityId == 342 || output.commodityId == 349)
	{
		item->SetState(1); DropItem(*item); UpdateGearWeight();
		response = tr("Handcrafted %1 on the ground.").arg(item->ListDesc());
	}
	else if(AddGearItemNoLock(m_Gear, *item)) response = tr("Handcrafted %1.").arg(item->ListDesc());
	else
	{ DropItem(*item); UpdateGearWeight(); response = tr("Handcrafted %1 but dropped it because there is no place to carry it.").arg(item->ListDesc()); }
	UnlockGear(); Thought(response);
}
