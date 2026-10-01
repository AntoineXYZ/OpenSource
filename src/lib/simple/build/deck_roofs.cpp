#include "planbuilder.h"
#include "internal.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace SimpleMode::detail {

void PlanBuilder::BuildRoofs()
{
	{
		QList<QChar> letters;
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint cell(c, r);
				QChar l = plan.RoofAt(D, cell);
				if(l == '.' || !plan.roofs.contains(l))
					continue;
				if(!CanRoof(plan, D, cell))
					log << "  Roof " << l << " over cell " << c << "," << r << " on deck " << D
						<< ": not a whole room cell with nothing above it; left flat.\n";
				else if(!letters.contains(l))
					letters << l;
			}
		for(QChar l : letters)
		{
			RoofRegion region(plan, D, plan.roofs[l], theme, H);
			roofCount += region.Build(adder());
			if(checks)
				region.Check(roofLog, roofPoints, roofBad);
		}
	}

}

void PlanBuilder::BuildSkylights()
{
	for(int i = 0; i < plan.skylights.size(); i++)
	{
		const Skylight& s = plan.skylights[i];
		if(s.deck != D)
			continue;
		QString problem = SkylightProblem(plan, s, i);
		if(!problem.isEmpty())
		{
			log << "  Skylight in cell " << s.cell.x() << "," << s.cell.y() << " on deck " << D << ": " << problem << " Left out.\n";
			continue;
		}
		skylightCount++;
		QChar l = plan.RoofAt(D, s.cell);
		if(l != '.' && plan.roofs.contains(l) && (FindRoofStyle(plan.roofs[l].style) ? plan.roofs[l].style
				: DefaultRoofStyle(plan.theme)) != "flat")
		{
			RoofRegion(plan, D, plan.roofs[l], theme, H).BuildSkylight(s, adder());
			continue;
		}
		double hx0, hy0, hx1, hy1;
		SkylightHole(plan, s.cell, hx0, hy0, hx1, hy1);
		const double cw = 0.1, ch = 0.15, mx = (hx0 + hx1) / 2, my = (hy0 + hy1) / 2;
		AuScFaceList frame, glass;
		AddBox(frame, hx0 - cw, hy0 - cw, H - 0.02, hx1 + cw, hy0, H + ch);
		AddBox(frame, hx0 - cw, hy1, H - 0.02, hx1 + cw, hy1 + cw, H + ch);
		AddBox(frame, hx0 - cw, hy0, H - 0.02, hx0, hy1, H + ch);
		AddBox(frame, hx1, hy0, H - 0.02, hx1 + cw, hy1, H + ch);
		auto both = [&](const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d, const AuPoint& n) {
			AddQuad(glass, a, b, c, d, n);
			AddQuad(glass, a, b, c, d, AuPoint(-n.x, -n.y, -n.z));
		};
		const V2 corner[4] = {V2(hx0, hy0), V2(hx1, hy0), V2(hx1, hy1), V2(hx0, hy1)};
		if(s.style != "lantern")
			both(Ft(hx0, hy0, H + ch - 0.03), Ft(hx0, hy1, H + ch - 0.03), Ft(hx1, hy1, H + ch - 0.03), Ft(hx1, hy0, H + ch - 0.03),
				AuPoint(0, 0, 1));
		else
		{
			const double z0 = H + ch, z1 = z0 + 0.4, apex = z1 + 0.5, pw = 0.05;
			for(int k = 0; k < 4; k++)
			{
				const V2 a = corner[k], b = corner[(k + 1) % 4], out = (b - a).Unit().Left() * -1;
				both(Ft(a, z0), Ft(a, z1), Ft(b, z1), Ft(b, z0), AuPoint(out.x, out.y, 0));
				AuPoint n = (Ft(b, z1) - Ft(a, z1)).Cross(Ft(mx, my, apex) - Ft(a, z1));
				n = n * (1 / n.Length());
				if(n.z < 0)
					n = n * -1;
				AddTri(glass, Ft(a, z1), Ft(b, z1), Ft(mx, my, apex), n);
				AddTri(glass, Ft(a, z1), Ft(b, z1), Ft(mx, my, apex), AuPoint(-n.x, -n.y, -n.z));
				AddBox(frame, a.x - pw / 2, a.y - pw / 2, z0, a.x + pw / 2, a.y + pw / 2, z1 + 0.02);
			}
			AddBox(frame, hx0 - pw / 2, hy0 - pw / 2, z1 - 0.03, hx1 + pw / 2, hy0 + pw / 2, z1 + 0.02);
			AddBox(frame, hx0 - pw / 2, hy1 - pw / 2, z1 - 0.03, hx1 + pw / 2, hy1 + pw / 2, z1 + 0.02);
			AddBox(frame, hx0 - pw / 2, hy0 + pw / 2, z1 - 0.03, hx0 + pw / 2, hy1 - pw / 2, z1 + 0.02);
			AddBox(frame, hx1 - pw / 2, hy0 + pw / 2, z1 - 0.03, hx1 + pw / 2, hy1 - pw / 2, z1 + 0.02);
			AddBox(frame, mx - 0.03, my - 0.03, apex - 0.05, mx + 0.03, my + 0.03, apex + 0.2);
		}
		const QString room = plan.rooms.value(plan.At(D, s.cell)).name;
		FinishOutsideTrim(plan, frame, theme == Plain ? QColor(0x3a, 0x3e, 0x44) : OUTSIDE_TRIM_PAINT);
		AuScMesh mesh;
		mesh.AddFaces(frame);
		if(plan.interiors)
			mesh.SetGroupName(room);
		add(mesh, QString("skylight %1,%2 deck %3").arg(s.cell.x()).arg(s.cell.y()).arg(D));
		Paint(glass, plan.interiors ? QColor(0xb0, 0xd0, 0xe0) : QColor(0, 0, 0));
		AuScWindow pane;
		pane.AddFaces(glass);
		pane.SetOpacity(plan.interiors ? 0.25f : 1.f);
		if(plan.interiors)
			pane.SetGroupName(room);
		add(pane, "skylight glass");
	}

}

void PlanBuilder::BuildDormers()
{
	for(int i = 0; i < plan.dormers.size(); i++)
	{
		const Dormer& d = plan.dormers[i];
		if(d.deck != D)
			continue;
		QString problem = DormerProblem(plan, d, i);
		if(!problem.isEmpty())
		{
			log << "  Dormer in cell " << d.cell.x() << "," << d.cell.y() << " on deck " << D << ": " << problem << " Left out.\n";
			continue;
		}
		RoofRegion(plan, D, plan.roofs[plan.RoofAt(D, d.cell)], theme, H).BuildDormer(d, adder());
		dormerCount++;
	}

}

}
