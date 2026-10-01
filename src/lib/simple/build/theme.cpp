#include "theme.h"
#include "internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

Theme ThemeOf(const QString& key)
{
	return key == "stone" ? Stone : key == "metal" ? Metal : key == "plain" ? Plain : Wood;
}

HOLD_ACCESS HoldOf(const QString& type)
{
	static const QMap<QString, HOLD_ACCESS> holds = {
		{"barracks", HOLD_Armory}, {"brig", HOLD_Feeder}, {"engineroom", HOLD_FuelCoupling}, {"galley", HOLD_Pantry},
		{"hold", HOLD_Hold}, {"lounge", HOLD_Bar}, {"sickbay", HOLD_Pharmacy}, {"stateroom", HOLD_Wardrobe},
		{"crewcabin", HOLD_Wardrobe}, {"officercabin", HOLD_Wardrobe}, {"vehiclebay", HOLD_VehicleBay},
	};
	return holds.value(type, HOLD_Inaccessible);
}

void FinishTrim(const Plan& plan, AuScFaceList& faces, const QColor& usual, const QString& texture, const QColor& colour,
	double strength, double scale)
{
	if(texture.isEmpty())
	{
		Paint(faces, colour.isValid() ? colour : usual);
		return;
	}
	Paint(faces, colour.isValid() ? Tint(colour, strength) : QColor(0, 0, 0, 0));
	const TextureKind* k = FindTextureKind(texture);
	TextureFaces(faces, TextureSlot(plan, texture), (k ? k->size : 2.0) * TRIM_TEXTURE_SCALE * scale);
}

void FinishOutsideTrim(const Plan& plan, AuScFaceList& faces, const QColor& usual)
{
	FinishTrim(plan, faces, usual, OutsideTrimTexture(plan), OutsideTrimColor(plan), OutsideTrimStrength(plan),
		BuildingTextureScale(plan, "outsidetrim"));
}

void FinishOutsideTrimWall(const Plan& plan, AuScFaceList& faces, const QColor& usual)
{
	if(OutsideDressing(plan) == "none" && !plan.outsideTrim.isEmpty())
		return;
	Paint(faces, Tint(OutsideTrimColor(plan).isValid() ? OutsideTrimColor(plan) : usual, 0.25));
}

}
