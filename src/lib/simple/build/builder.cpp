#include "builder.h"
#include "internal.h"
#include "planbuilder.h"

#include <cstdio>
#include <QDir>
#include <QFileInfo>
#include <memory>

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

static bool g_debugParts = false;

INTERIOR_LIGHT LightOf(const QString& lights)
{
	return lights == "dark" ? INTLIGHT_Dark : lights == "dim" ? INTLIGHT_Dim : INTLIGHT_Bright;
}

static thread_local QMap<QString, int> g_triangles;

static QString TriangleCategory(const QString& what)
{
	static const struct { const char* start; const char* category; } kinds[] = {
		{"hull hatch", "Doors and hatches"}, {"hull ", "Outside walls"},
		{"door", "Doors and hatches"}, {"hatch", "Doors and hatches"}, {"opening arch", "Doors and hatches"},
		{"dormer window frame", "Window frames"}, {"dormer window", "Windows and skylights"}, {"dormer ", "Roofs"},
		{"window frame", "Window frames"}, {"window", "Windows and skylights"},
		{"skylight glass", "Windows and skylights"}, {"skylight ", "Window frames"},
		{"outside trim", "Outside trims"}, {"trim ", "Room trims"}, {"room ", "Rooms (floors, walls, ceilings)"},
		{"stair", "Stairs and ladders"}, {"ladder", "Stairs and ladders"}, {"railing", "Railings"},
		{"underside ", "Balconies and platforms"}, {"legs ", "Balconies and platforms"},
		{"turbo lift", "Lifts"}, {"turret", "Turrets"}, {"gunnery console", "Turrets"},
		{"light", "Lights, poles and flags"}, {"flag", "Lights, poles and flags"}, {"empire flag", "Lights, poles and flags"},
		{"rotating beacon", "Lights, poles and flags"}, {"strobe", "Lights, poles and flags"}, {"navigation light", "Lights, poles and flags"},
	};
	if(what.startsWith("roof "))
		return what.contains(" eaves ") ? "Eaves" : "Roofs";
	for(const auto& k : kinds)
		if(what.startsWith(k.start))
			return k.category;
	return "Fittings";
}

static int DrawnTriangles(const AuScPart& part, bool& invisible)
{
	invisible = false;
	if(part.IsVoid() || part.IsPath() || part.IsBarrier() || part.IsSite())
		return 0;
	QList<const AuScPart*> looks;
	std::unique_ptr<AuScPart> turret[3];
	if(part.IsTurret())
	{
		const quint8 states[3] = {quint8(AuScTurret::Base), quint8(AuScTurret::Gun), quint8(AuScTurret::GunnerScreen)};
		for(int s = 0; s < 3; s++)
		{
			turret[s].reset(part.Clone());
			turret[s]->SetState(states[s]);
			looks << turret[s].get();
		}
	}
	else if(part.IsVisibleInFinalModel())
		looks << &part;
	int count = 0;
	bool seen = false;
	for(const AuScPart* p : looks)
	{
		AuScFaceList faces;
		p->Faces(faces);
		for(const AuScFace* face : faces)
		{
			AuScTriList tl;
			face->Triangles(tl);
			count += tl.size();
			seen = seen || face->Opacity() >= 0.02f;
		}
	}
	invisible = count && part.IsWindow() && !seen;
	return count;
}

quint16 Add(AuBdDesign& design, AuScPart& part, const QString& what, QTextStream& log)
{
	part.SetArchitect(AuId(), "SimpleModePOC");
	AuScParts added;
	quint16 id = design.PartAdd(part, added);
	if(!id)
		log << "  FAILED to add " << what << " (the part says it is not valid)\n";
	if(g_debugParts == true)
		fprintf(stderr, "added %s -> %d\n", qPrintable(what), (int)id);
	bool invisible;
	const int drawn = id ? DrawnTriangles(part, invisible) : 0;
	if(drawn)
		g_triangles[invisible ? QString("Invisible (open-air cells, unglazed panes)") : TriangleCategory(what)] += drawn;
	return id;
}

}

int BuildFromGrid(const QString& gridPath, const QString& outPath, QTextStream& log)
{
	auto plan = TryLoadPlan(gridPath, log);
	if(!plan)
		return 1;
	return BuildPlan(*plan, outPath, log);
}

QMap<QString, int> SimpleMode::LastBuildTriangles()
{
	return g_triangles;
}

static std::unique_ptr<PlanBuilder> Assemble(BuildContext& ctx, const Plan& source, QTextStream& log, bool checks)
{
	g_triangles.clear();
	ctx.plan = source;
	Plan& plan = ctx.plan;
	if(!CheckPlan(plan, log))
		return nullptr;
	QMap<QChar, int> quarters;
	for(const QStringList& grid : plan.decks)
		for(const QString& row : grid)
			for(QChar ch : row)
				if(ch != '.')
					quarters[ch]++;
	for(QMap<QChar, Room>::iterator ri = plan.rooms.begin(); ri != plan.rooms.end(); )
		if(!quarters.contains(ri.key()))
			ri = plan.rooms.erase(ri);
		else
			ri++;
	if(plan.rooms.isEmpty())
	{
		log << "No room has any cells yet.\n";
		return nullptr;
	}

	const double S = plan.cell, H = plan.height, HC = plan.CeilingHt();
	const double VF = plan.voidGap, VC = HC - plan.voidGap;
	ctx.S = S;
	ctx.H = H;
	ctx.HC = HC;
	ctx.VF = VF;
	ctx.VC = VC;

	std::unique_ptr<PlanBuilder> b(new PlanBuilder(ctx, log));
	b->checks = checks;
	b->PlaceStairs();
	b->BuildSite();
	for(ctx.D = 0; ctx.D < plan.Decks(); ctx.D++)
		b->BuildDeck();
	ctx.D = 0;

	if(plan.interiors)
	{
		b->BuildPosts();
		b->BuildStations();
	}
	b->BuildFixtures();
	b->SetVolumes(quarters);
	return b;
}

int SimpleMode::BuildPlanInto(const Plan& source, AuBdDesign& out, QTextStream& log)
{
	BuildContext ctx;
	std::unique_ptr<PlanBuilder> b = Assemble(ctx, source, log, false);
	if(!b)
		return 1;
	out = b->design;
	return 0;
}

int SimpleMode::BuildPlan(const Plan& source, const QString& outPath, QTextStream& log, bool checks)
{
	BuildContext ctx;
	std::unique_ptr<PlanBuilder> built = Assemble(ctx, source, log, checks);
	if(!built)
		return 1;
	PlanBuilder& b = *built;
	const Plan& plan = ctx.plan;

	AuBdDesign& design = b.design;
	QDir().mkpath(QFileInfo(outPath).absolutePath());
	if(!design.SaveToFile(outPath, SOH_FILE_TYPE))
	{
		log << "Could not write " << outPath << "\n";
		return 1;
	}
	int wholeCells = 0, splitCells = 0;
	for(int d = 0; d < plan.Decks(); d++)
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QChar w = plan.At(d, QPoint(c, r));
				if(w == '*')
					splitCells++;
				else if(w != '.')
					wholeCells++;
			}
	log << "Wrote " << outPath << ": " << plan.Decks() << (plan.Decks() == 1 ? " deck, " : " decks, ") << plan.rooms.size()
		<< " rooms, " << wholeCells << " whole cells, " << splitCells << " split cells, " << b.voidCount << " room voids, "
		<< b.doorCount << " doors, " << b.hatchCount << " hatches, " << b.windowCount << " windows, " << b.roofCount << " roofed cells, " << b.dormerCount << " dormers, " << b.skylightCount
		<< " skylights, " << design.Parts().size() << " parts\n";
	if(!checks)
		return 0;

	return RunSelfChecks(ctx, outPath, log);
}
