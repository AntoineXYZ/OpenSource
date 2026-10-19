#include "aulegacyconstructionmaterials.h"
#include "daatomlist.h"
#include "daitem.h"
#include "daworldll.h"
#include "aulegacycityproduction.h"
#include "auitem.h"
#include <QSet>
#include <map>
#include <vector>
#include <utility>

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

struct AuLegacyConstructionMaterials::Batch::Data
{
	struct Item
	{
		QByteArray original;
		std::unique_ptr<AuItem> planned;
		bool changed = false;
	};
	// std::less supplies a total order for unrelated object pointers, allowing
	// overlapping batches to acquire all source-item locks in the same order.
	std::map<DaItem*, Item, std::less<DaItem*>> items;
	QVector<DaItem*> changedItems, emptiedItems;
	QString error;
	bool committed = false, finalized = false;
};

AuLegacyConstructionMaterials::Batch::Batch() : m_Data(new Data) {}
AuLegacyConstructionMaterials::Batch::~Batch() = default;

bool AuLegacyConstructionMaterials::Batch::Fetch(const QVector<DaItem*>& items,
	const AuLegacyCityCatalogue& catalogue, AuLegacyCity::ProductionProcess& components)
{
	if(m_Data->committed)
	{ m_Data->error = QStringLiteral("Construction materials were already committed."); return false; }
	const auto needsMaterial = [&components] {
		for(const auto& component : components.components) if(!component.IsSatisfied()) return true;
		return false;
	};
	bool anyChanged = false;
	for(auto* atom : items)
	{
		if(!needsMaterial()) break;
		if(!atom || atom->IsDead()) continue;
		auto entry = m_Data->items.find(atom);
		if(entry == m_Data->items.end())
		{
			Data::Item snapshot;
			const AuItem* source = atom->LockItem();
			if(source)
			{
				snapshot.original = source->Pack();
				snapshot.planned.reset(source->Clone());
			}
			atom->UnlockItem();
			if(!source) continue;
			if(!snapshot.planned || snapshot.planned->Pack() != snapshot.original)
			{ m_Data->error = QStringLiteral("A construction material could not be copied without changing its data."); return false; }
			entry = m_Data->items.emplace(atom, std::move(snapshot)).first;
		}
		AuItem* item = entry->second.planned.get();
		if(!item->Count()) continue;
		bool changed = false;
		auto apply = [&](AuItem& material) {
			if(!needsMaterial() || !material.Count()) return;
			const quint32 count = material.Count(); const quint8 condition = material.HitPoints();
			AuLegacyCity::ApplyProductionMaterial(catalogue, components, material);
			changed = changed || count != material.Count() || condition != material.HitPoints();
		};
		// DaItem::FetchConstructionMaterials 0x30f110 and AuItemContent's
		// original 0xc67e0 loop offer direct contents, keeping the container.
		// An emptied container survives; a consumed loose commodity dies.
		if(auto* contents = item->Content())
		{
			for(auto it = contents->begin(); it != contents->end() && needsMaterial();)
			{
				apply(*it->second);
				if(!it->second->Count()) { it = contents->Erase(it); changed = true; }
				else ++it;
			}
		}
		else apply(*item);
		anyChanged |= changed;
	}
	return anyChanged;
}

bool AuLegacyConstructionMaterials::Batch::Commit(QString& error)
{
	error.clear();
	if(!m_Data->error.isEmpty()) { error = m_Data->error; return false; }
	if(m_Data->committed)
	{ error = QStringLiteral("Construction materials were already committed."); return false; }
	m_Data->changedItems.clear();
	m_Data->emptiedItems.clear();
	for(auto& pair : m_Data->items)
	{
		auto& item = pair.second;
		item.changed = item.planned->Pack() != item.original;
		if(item.changed)
		{
			m_Data->changedItems.append(pair.first);
			if(!item.planned->Count()) m_Data->emptiedItems.append(pair.first);
		}
	}
	struct ItemLocks
	{
		std::vector<DaItem*> items;
		~ItemLocks() { for(auto it = items.rbegin(); it != items.rend(); ++it) (*it)->UnlockItem(); }
	} locked;
	locked.items.reserve(m_Data->items.size());
	for(const auto& pair : m_Data->items)
	{
		pair.first->m_ItemLock.lockForWrite();
		locked.items.push_back(pair.first);
	}
	// Validate the complete observed set, including a tool whose successful
	// use left its condition unchanged. A conflict aborts before any swap.
	for(const auto& pair : m_Data->items)
		if(pair.first->IsDead() || !pair.first->m_Item || pair.first->m_Item->Pack() != pair.second.original)
		{ error = QStringLiteral("Construction materials changed while work was being prepared; try again."); return false; }
	for(auto& pair : m_Data->items)
		if(pair.second.changed)
		{
			// Keep the replaced item alive until batch destruction, outside the
			// city transaction. No notification or world callback runs here.
			std::unique_ptr<AuItem> previous(pair.first->m_Item);
			pair.first->m_Item = pair.second.planned.release();
			pair.second.planned = std::move(previous);
			pair.first->m_ItemChangeTime.Set();
		}
	m_Data->committed = true;
	return true;
}

void AuLegacyConstructionMaterials::Batch::Finalize()
{
	if(!m_Data->committed || m_Data->finalized) return;
	m_Data->finalized = true;
	QSet<DaItem*> emptied;
	for(auto* atom : m_Data->emptiedItems) emptied.insert(atom);
	for(auto* atom : m_Data->changedItems)
		if(!emptied.contains(atom) && !atom->IsDead()) atom->NotifyItemChanged();
	for(auto* atom : m_Data->emptiedItems)
		if(!atom->IsDead()) atom->Kill();
}
