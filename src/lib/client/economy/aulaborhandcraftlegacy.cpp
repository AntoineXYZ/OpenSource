#include "aulaborhandcraft.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityhandcraft.h"
#include "aulegacyhandcraft.h"
#include "daperson.h"
#include "deprocesscomponent.h"
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolButton>

void AuHandCraftPanel::UpdateLegacyContents(DaPerson* person)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || !person) return;
	const auto context = AuLegacyCity::QueryHandcraftContext(*person);
	bool industryRequired = false;
	{
		const QSignalBlocker blocked(m_MakeWhat); m_MakeWhat->clear();
		QMultiMap<QString, quint16> recipes;
		for(const auto& recipe : catalogue->ManufacturingProcesses())
			if(AuLegacyCity::HandcraftAllowed(recipe, context))
				recipes.insert(recipe.name + (recipe.craft == 2 ? QStringLiteral(" *") : QString()), recipe.id.value);
		for(auto entry = recipes.cbegin(); entry != recipes.cend(); ++entry)
		{
			const auto* recipe = catalogue->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(entry.value()));
			const auto* commodity = catalogue->Commodity(recipe->commodity);
			m_MakeWhat->addItem(commodity ? QIcon(commodity->descriptor.BitmapName()) : QIcon(), entry.key(), entry.value());
			industryRequired = industryRequired || recipe->craft == 2;
		}
		int selected = m_MakeWhat->findData(m_MPID);
		if(selected < 0) selected = m_MakeWhat->count() ? 0 : -1;
		m_MakeWhat->setCurrentIndex(selected); m_MPID = selected >= 0 ? m_MakeWhat->itemData(selected).toInt() : 0;
	}
	const auto* industry = catalogue->Industry(AuLegacyCity::IndustryId(context.industry));
	m_Note->setText(industryRequired && industry ? tr("<small>* %1</small>").arg(industry->name.toHtmlEscaped()) : QString());
	AuLegacyCity::ProductionProcess process;
	const auto* gear = person->LockGearForRead();
	const bool valid = AuLegacyCity::PrepareHandcraft(*catalogue, quint16(m_MPID), *gear, context, process);
	person->UnlockGear();
	m_Components->clear(); m_Modifiers->setRowCount(0);
	if(valid)
		for(const auto& value : process.components)
		{
			if(value.effect != 5) continue;
			const auto* descriptor = catalogue->Commodity(AuLegacyCity::CommodityId(value.commodityId));
			const QString name = descriptor ? descriptor->descriptor.m_name : QString::number(value.commodityId);
			DeProcessComponent component;
			component.SetCommodity(COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(value.commodityId))), value.quality);
			component.SetEffect(COMPEFFECT(value.effect)); component.SetQtyConsumed(value.required); component.SetQtyApplied(value.applied);
			const QIcon icon(component.CommodityBitmapName());
			if(value.required)
			{
				QString text = !value.applied ? tr("%1 needed").arg(value.required)
					: value.applied == value.required ? QString::number(value.applied) : tr("%1 of %2").arg(value.applied).arg(value.required);
				if(value.applied && value.quality) text += tr(" Q%1 TL%2").arg(value.quality).arg(value.technology + 1);
				auto* item = new QListWidgetItem(icon, text, m_Components); item->setToolTip(name);
			}
			else
			{
				const int row = m_Modifiers->rowCount(); m_Modifiers->insertRow(row);
				auto* item = new QTableWidgetItem(icon, QString()); item->setToolTip(name); m_Modifiers->setItem(row, 0, item);
				const char* effect = component.EffectBitmapName();
				item = new QTableWidgetItem(effect ? QIcon(QString::fromLatin1(effect)) : QIcon(), QString());
				item->setToolTip(tr("Required")); m_Modifiers->setItem(row, 1, item);
				item = new QTableWidgetItem(QIcon(value.applied ? ":data/mpReady.png" : ":data/mpNotReady.png"), QString());
				item->setToolTip(value.applied ? tr("Ready") : tr("Required")); m_Modifiers->setItem(row, 2, item);
			}
		}
	m_Modifiers->resizeColumnsToContents();
	const bool ready = valid && AuLegacyCity::CanRunProduction(process);
	m_Make->setEnabled(ready);
	m_StatusIcon->setPixmap(QPixmap(ready ? ":data/mpReady.png" : ":data/mpNotReady.png"));
	const auto* recipe = catalogue->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(quint16(m_MPID)));
	m_Status->setText(recipe ? recipe->name + QLatin1Char('\n') + (ready ? tr("Ready") : tr("Not Ready"))
		: tr("Select a Handcrafting Process\nNot Ready"));
}
