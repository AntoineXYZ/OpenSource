#include "planbuilder.h"
#include "internal.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace SimpleMode::detail {

void PlanBuilder::BuildStairs()
{
	for(const PlacedStair& ps : placedStairs)
	{
		if(ps.stair.deck != D)
			continue;
		const QString roomName = plan.rooms.value(whole(ps.stair.cell)).name;
		const QString railStyle = ps.style == "stone" ? "balustrade" : ps.style == "timber" ? "balusters"
			: ps.style == "floating" ? "glass" : "bars";
		AuScFaceList main, dark, glass;
		auto barrier = [&](AuScFaceList& faces, const QString& what) {
			AuScBarrier b;
			b.AddFaces(faces);
			b.SetFaceOpacity(VOID_OPACITY);
			add(b, what);
		};
		QList<AuPoint> path;
		QList<QPair<AuPoint, AuPoint>> flights;
		for(int i = 0; i < ps.frames.size(); i++)
		{
			const StairFrame& fr = ps.frames[i];
			const StairLayout::Flight& f = ps.layout.flights[i];
			StairFlightMesh(main, dark, glass, fr, f.z0, f.z1, ps.style);
			AuScFaceList wedge;
			const double h = fr.w / 2 + 0.05;
			SlopedBar(wedge, fr, -h, h, 0, fr.len, 0, f.z0 + VF, 0, f.z1 + VF);
			barrier(wedge, "stair slope");
			auto on = [&](double t) { return fr.At(t, 0, f.z0 + (f.z1 - f.z0) * t / fr.len + PATH_HEIGHT); };
			path << on(0.3) << on(fr.len - 0.3);
			flights << qMakePair(on(0.3), on(fr.len - 0.3));
			if(i == ps.frames.size() - 1)
			{
				AuScFaceList sheet;
				SlopedBar(sheet, fr, -h, h, fr.len, fr.len + 0.2, f.z1 + VF - 0.1, f.z1 + VF, f.z1 + VF - 0.1, f.z1 + VF);
				barrier(sheet, "stair top");
				path << fr.At(fr.len + 0.4, 0, f.z1 + PATH_HEIGHT);
			}
			else if(!ps.landings.isEmpty())
			{
				const PlacedStair::Landing& l = ps.landings[0];
				path << Ft((l.x0 + l.x1) / 2, (l.y0 + l.y1) / 2, l.z + PATH_HEIGHT);
			}
		}
		for(const PlacedStair::Landing& l : ps.landings)
		{
			AuScFaceList block;
			AddBox(block, l.x0, l.y0, 0, l.x1, l.y1, l.z + VF);
			barrier(block, "stair landing");
			if(ps.style == "stone")
				AddBox(main, l.x0, l.y0, 0, l.x1, l.y1, l.z, BoxBottom);
			else
			{
				AddBox(ps.style == "timber" ? dark : main, l.x0, l.y0, l.z - (ps.style == "steel" ? 0.03 : 0.1), l.x1, l.y1, l.z);
				for(double x : {l.x0 + 0.06, l.x1 - 0.06})
					for(double y : {l.y0 + 0.06, l.y1 - 0.06})
						AddBox(ps.style == "floating" ? dark : main, x - 0.05, y - 0.05, 0, x + 0.05, y + 0.05, l.z - 0.03, BoxBottom | BoxTop);
			}
			static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
			for(int s = 0; s < 4; s++)
			{
				if(!l.outer[s])
					continue;
				V2 c((l.x0 + l.x1) / 2, (l.y0 + l.y1) / 2), out = dirs[s], along(out.y, -out.x);
				double half = (s == North || s == South) ? (l.x1 - l.x0) / 2 : (l.y1 - l.y0) / 2;
				double reach = (s == North || s == South) ? (l.y1 - l.y0) / 2 : (l.x1 - l.x0) / 2;
				V2 m = c + out * (reach - 0.05);
				RailingMesh(main, glass, m - along * half, m + along * half, out, railStyle, l.z, 0.05);
			}
		}
		AuPoint entryMid = pieceMid(V2(midX(ps.layout.entry), midY(ps.layout.entry)), whole(ps.stair.cell));
		if(entryMid != AuPoint::Invalid())
			addPath(entryMid, path.first(), 0);
		for(int k = 0; k + 1 < path.size(); k++)
			addPath(path[k], path[k + 1], 0);
		addPath(path.last(), centre(ps.layout.arrival, H + PATH_HEIGHT), 0);
		Paint(main, ps.style == "stone" ? QColor(0xb4, 0xae, 0xa2) : ps.style == "steel" ? QColor(0x6c, 0x74, 0x7c)
			: ps.style == "floating" ? QColor(0xdc, 0xd8, 0xd0) : ps.style == "ramp" ? QColor(0xa8, 0xa8, 0xa0) : TRIM_PAINT);
		Paint(dark, ps.style == "timber" ? QColor(0x8a, 0x5a, 0x30) : ps.style == "stone" ? QColor(0x9a, 0x94, 0x88)
			: QColor(0x3c, 0x40, 0x46));
		Paint(glass, QColor(0xb0, 0xd0, 0xe0));
		glass.SetOpacity(0.3f);
		AuScMesh mesh;
		for(AuScFaceList* list : {&main, &dark, &glass})
			mesh.AddFaces(*list);
		mesh.SetGroupName(roomName);
		add(mesh, "stair");
		QList<AuPoint> whole{entryMid};
		whole << path << centre(ps.layout.arrival, H + PATH_HEIGHT);
		if(entryMid == AuPoint::Invalid())
			whole.removeFirst();
		if(!roomName.isEmpty())
			stairChecks << StairCheck{D, whole, flights};
		else if(arrivesOnOpenAir(ps))
			log << "  Stair at " << ps.stair.cell.x() << "," << ps.stair.cell.y() << " on deck " << D << " starts outside the building "
				<< "and comes up onto " << plan.rooms.value(plan.At(D + 1, ps.layout.arrival)).name << ": checked as a walk up onto it.\n";
		else
			log << "  Stair at " << ps.stair.cell.x() << "," << ps.stair.cell.y() << " on deck " << D << " starts outside the building: "
				<< "its climb is not checked (out there, walking is the planet's).\n";
	}
}

void PlanBuilder::BuildStairWells()
{
	for(const PlacedStair& ps : placedStairs)
	{
		if(ps.stair.deck != D - 1)
			continue;
		const QString railStyle = ps.style == "stone" ? "balustrade" : ps.style == "timber" ? "balusters"
			: ps.style == "floating" ? "glass" : "bars";
		AuScFaceList main, glass;
		QList<AuScFaceList*> blocks;
		for(const QPoint& p : ps.layout.cells)
			for(int s = 0; s < 4; s++)
			{
				QPoint q = Neighbour(p, Side(s));
				if(!IsOpenCell(plan, D, p) || ps.layout.cells.contains(q) || whole(q) != whole(p) || IsOpenCell(plan, D, q))
					continue;
				static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
				V2 m = V2(midX(p), midY(p)) + dirs[s] * (plan.cell / 2), along(dirs[s].y, -dirs[s].x);
				QList<QPair<double, double>> pieces{qMakePair(-plan.cell / 2, plan.cell / 2)};
				if(p == ps.layout.top && s == ps.layout.arrivalSide)
				{
					double gap = ps.frames.last().w / 2 + 0.05;
					pieces = {qMakePair(-plan.cell / 2, -gap), qMakePair(gap, plan.cell / 2)};
				}
				for(const QPair<double, double>& piece : pieces)
				{
					if(piece.second - piece.first < 0.15)
						continue;
					V2 a = m + along * piece.first, b = m + along * piece.second, pm = (a + b) * 0.5;
					double len = piece.second - piece.first;
					RailingMesh(main, glass, a, b, dirs[s], railStyle);
					blocks << new AuScFaceList;
					AddWallBox(*blocks.last(), pm, along, dirs[s], -0.05, 0.05, len, 0, HC);
					railingLines << OpenSpan{D, pm, along, len / 2};
				}
			}
		if(main.isEmpty())
			continue;
		Paint(main, railStyle == "bars" ? QColor(0x2c, 0x2d, 0x31) : railStyle == "glass" ? QColor(0x9a, 0xa2, 0xaa) : TRIM_PAINT);
		Paint(glass, QColor(0xb0, 0xd0, 0xe0));
		glass.SetOpacity(0.3f);
		AuScMesh mesh;
		mesh.AddFaces(main);
		mesh.AddFaces(glass);
		mesh.SetGroupName(plan.rooms.value(whole(ps.layout.top)).name);
		add(mesh, "stair well railing");
		for(AuScFaceList* block : blocks)
		{
			AuScBarrier b;
			b.AddFaces(*block);
			b.SetFaceOpacity(VOID_OPACITY);
			add(b, "stair well barrier");
			delete block;
		}
	}

}

void PlanBuilder::BuildPaths()
{
	QSet<QPair<int, int>> linked;
	for(int i = 0; i < pieces.size(); i++)
	{
		const Piece& pc = pieces[i];
		QList<int> ofPiece;
		if(pc.quarter < 0)
			ofPiece = {North, East, South, West};
		else
			ofPiece = {pc.quarter};
		for(int q : ofPiece)
			for(int k = 0; k < 3; k++)
			{
				QPoint oc;
				int oq;
				QuarterAcross(pc.cell, q, k, oc, oq);
				int j = pieceIndex(pieces, oc, oq);
				if(j < 0 || j == i || pieces[j].room != pc.room || linked.contains(qMakePair(qMin(i, j), qMax(i, j))))
					continue;
				linked.insert(qMakePair(qMin(i, j), qMax(i, j)));
				addPath(Ft(pc.mid, PATH_HEIGHT), Ft(pieces[j].mid, PATH_HEIGHT), 0);
			}
	}
}

}
