#include "aulegacyconstructionmaterials.h"
#include "daatomlist.h"
#include "daitem.h"
#include "daworldll.h"
#include "deprocesscomponent.h"
#include <QSet>

QVector<DaItem*> AuLegacyConstructionMaterials::ItemsAtSite(DaWorldLL& world,
	const AuId& cityId, const AuLegacyCity::Development& development)
{
	QVector<DaItem*> result; DaAtomList atoms; world.ListNormalSubatoms(atoms);
	for(auto* atom : atoms)
	{
		auto* item = dynamic_cast<DaItem*>(atom);
		if(!item || item->IsDead() || !item->DoesQueryEnvironment()) continue;
		// DaWorldLL::FetchConstructionMaterials, AuAtom13 0x516200, selects
		// items by their construction-site LL; there is no arbitrary fetch
		// radius. Added city/development IDs disambiguate the owned records.
		const auto* environment = item->LockEnvironmentQuery();
		const bool sameSite = environment->LegacyCityId() == cityId
			&& environment->LegacyDevelopmentId() == development.id;
		item->UnlockEnvironmentQuery();
		if(sameSite) result.append(item);
	}
	return result;
}

quint32 AuLegacyConstructionMaterials::Fetch(const QVector<DaItem*>& items, DeProcessComponent& component,
	QVector<DaItem*>& changedItems, QVector<DaItem*>& emptiedItems)
{
	quint32 labor = 0;
	for(auto* atom : items)
	{
		if(component.IsSatisfied()) break;
		if(!atom || atom->IsDead()) continue;
		AuItem* item = atom->LockItemForWrite();
		if(!item || !item->Count()) { atom->UnlockItem(); continue; }
		bool changed = false;
		auto apply = [&](AuItem& material) {
			if(component.IsSatisfied() || material.CommodityId() != component.CommodityId() || !material.Count()) return;
			const quint32 count = material.Count(); const quint8 condition = material.HitPoints();
			labor = qMin(quint32(65535), labor + quint32(qMax(0, component.ApplyMaterial(material))));
			changed = changed || count != material.Count() || condition != material.HitPoints();
		};
		// DaItem::FetchConstructionMaterials 0x30f110 and AuItemContent's
		// original 0xc67e0 loop offer direct contents, keeping the container.
		// An emptied container survives; a consumed loose commodity dies.
		if(auto* contents = item->Content())
		{
			for(auto it = contents->begin(); it != contents->end() && !component.IsSatisfied();)
			{
				apply(*it->second);
				if(!it->second->Count()) { it = contents->Erase(it); changed = true; }
				else ++it;
			}
		}
		else apply(*item);
		const bool empty = !item->Count(); atom->UnlockItem();
		if(changed) changedItems.append(atom);
		if(empty) emptiedItems.append(atom);
	}
	return labor;
}

void AuLegacyConstructionMaterials::Finalize(const QVector<DaItem*>& changedItems,
	const QVector<DaItem*>& emptiedItems)
{
	QSet<DaItem*> emptied, changed;
	for(auto* atom : emptiedItems) if(atom) emptied.insert(atom);
	for(auto* atom : changedItems) if(atom) changed.insert(atom);
	for(auto* atom : changed)
		if(!emptied.contains(atom) && !atom->IsDead()) atom->NotifyItemChanged();
	for(auto* atom : emptied)
		if(!atom->IsDead()) atom->Kill();
}
