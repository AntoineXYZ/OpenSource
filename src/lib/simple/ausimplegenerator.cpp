#include "ausimple.h"
#include "simplemode.h"

#include "aubddesign.h"
#include "auscparts.h"

#include <QTextStream>

namespace {

bool Generate(const QString& recipe, AuBdDesign& design, QString& log)
{
	QString text;
	QTextStream out(&text);
	SimpleMode::Plan plan;
	if(!SimpleMode::ParsePlanText(recipe, plan, out))
	{
		out.flush();
		log = text;
		return false;
	}
	AuBdDesign built;
	if(SimpleMode::BuildPlanInto(plan, built, out) != 0)
	{
		out.flush();
		log = text;
		return false;
	}
	design.Parts() = built.Parts();
	AuDesignTex palette[SCTEXI_Count];
	built.TexturePalette(palette);
	design.SetTexturePalette(palette);
	out.flush();
	log = text;
	return true;
}

}

void AuSimple::RegisterGenerator()
{
	AuBdDesign::SetSimpleModeGenerator(&Generate);
}
