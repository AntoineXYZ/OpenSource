#pragma once

#include <QSet>

#include "constants.h"
#include "theme.h"
#include "roofhelpers.h"
#include "windowparts.h"
#include "../geometry/outline.h"

namespace SimpleMode::detail {

class RoofRegion
{
public:
	static void EdgesToRoof(AuScFaceList& from, AuScFaceList& to)
	{
		for(int i = from.size() - 1; i >= 0; i--)
			if(qAbs(from[i]->FaceNormal().z) < 0.1)
				to.append(from.takeAt(i));
	}

	static constexpr double ROOF_TINT = 0.35;
	void Roofing(AuScFaceList& faces) const
	{
		const QString key = BuildingTexture(m_Plan, "roofing");
		const TextureKind* k = FindTextureKind(key);
		TextureFaces(faces, TextureSlot(m_Plan, key), (k ? k->size : 2.0) * BuildingTextureScale(m_Plan, "roofing"));
	}

	void Outside(AuScFaceList& faces) const
	{
		const QString key = BuildingTexture(m_Plan, "outside");
		const TextureKind* k = FindTextureKind(key);
		TextureFaces(faces, TextureSlot(m_Plan, key), (k ? k->size : 2.0) * BuildingTextureScale(m_Plan, "outside"));
	}

	RoofRegion(const Plan& plan, int deck, const Roof& roof, Theme theme, double base)
		: m_Plan(plan), m_Deck(deck), m_Roof(roof), m_Theme(theme), m_Base(base), m_S(plan.cell)
	{
		m_Style = FindRoofStyle(roof.style);
		if(!m_Style)
			m_Style = FindRoofStyle(DefaultRoofStyle(plan.theme));
		m_Key = m_Style->key;
		m_T = qTan(qDegreesToRadians(roof.pitch > 0 ? roof.pitch : m_Style->pitch));
		m_TDiag = m_Style->pitch > 0 ? m_T : qTan(qDegreesToRadians(35.0));
		bool northSouth = roof.dir == North || roof.dir == South;
		m_AlongX = m_Style->sloped ? !northSouth : northSouth;
		m_Steps = m_Key == "round" ? 8 : (m_Key == "gambrel" || m_Key == "mansard") ? 4 : 2;

		QSet<QString> seen;
		auto addPiece = [&](QList<QVector<V2>>& list, const QPoint& c, int q, bool whole) {
			QString key = QString("%1,%2,%3").arg(c.x()).arg(c.y()).arg(whole ? -1 : q);
			if(seen.contains(key))
				return;
			seen.insert(key);
			if(whole)
			{
				double x0 = m_Plan.X0(c.x()), x1 = m_Plan.X1(c.x()), y0 = m_Plan.Y0(c.y()), y1 = m_Plan.Y1(c.y());
				list << QVector<V2>{V2(x0, y0), V2(x1, y0), V2(x1, y1), V2(x0, y1)};
			}
			else
				list << QuarterPoly(m_Plan, c, q);
		};
		for(int r = 0; r < m_Plan.Rows(); r++)
			for(int c = 0; c < m_Plan.Cols(); c++)
				for(int q = 0; q < 4; q++)
				{
					QPoint cell(c, r);
					if(!InQ(cell, q))
						continue;
					QVector<V2> t = QuarterPoly(m_Plan, cell, q);
					for(int k = 0; k < 3; k++)
					{
						QPoint oc;
						int oq;
						QuarterAcross(cell, q, k, oc, oq);
						if(InQ(oc, oq))
							continue;
						Edge e{t[k], t[(k + 1) % 3], cell, q, oc, oq,
							m_Plan.Quarter(m_Deck, oc, oq) == '.' || IsOpenAirQuarter(m_Plan, m_Deck, oc, oq)};
						m_Edges << e;
						addPiece(m_InPieces, cell, q, Whole(cell));
						addPiece(m_OutPieces, oc, oq, !In(oc));
						if(In(oc))
							m_DiagPieces << QuarterPoly(m_Plan, oc, oq);
					}
				}
		for(int r = 0; r < m_Plan.Rows(); r++)
			for(int c = 0; c < m_Plan.Cols(); c++)
				if(In(QPoint(c, r)) && !Whole(QPoint(c, r)))
					m_Steps = qMax(m_Steps, 4);
		if(m_Key != "flat" && m_Key != "dome" && m_Key != "spire")
			for(int r = 0; r < m_Plan.Rows(); r++)
				for(int c = 0; c < m_Plan.Cols(); c++)
					if(In(QPoint(c, r)) && SkylightAt(m_Plan, m_Deck, QPoint(c, r)))
					{
						m_Skylit << QPoint(c, r);
						m_Steps = qMax(m_Steps, 4);
					}
	}

	bool InQ(const QPoint& c, int q) const
	{
		return m_Plan.OnGrid(c) && m_Plan.RoofAt(m_Deck, c) == m_Roof.letter && m_Plan.Quarter(m_Deck, c, q) != '.'
			&& m_Plan.Quarter(m_Deck + 1, c, q) == '.';
	}
	bool In(const QPoint& c) const
	{
		for(int q = 0; q < 4; q++)
			if(InQ(c, q))
				return true;
		return false;
	}
	bool Whole(const QPoint& c) const
	{
		for(int q = 0; q < 4; q++)
			if(!InQ(c, q))
				return false;
		return true;
	}

	int Build(const std::function<quint16(AuScPart&, const QString&)>& add)
	{
		int count = 0;
		AuScFaceList eaves, trim, dress;
		const bool perCell = m_Key == "dome" || m_Key == "spire";
		for(int r = 0; r < m_Plan.Rows(); r++)
			for(int c = 0; c < m_Plan.Cols(); c++)
			{
				QPoint cell(c, r);
				if(!In(cell) || (perCell && !Whole(cell)))
					continue;
				count++;
				AuScFaceList hull, walls;
				if(m_Key == "dome")
					Dome(hull, dress, cell);
				else if(m_Key != "flat")
				{
					Surface(hull, cell);
					Walls(walls, cell);
					if(m_Key == "spire")
					{
						V2 m = Mid(cell);
						double top = m_Base + m_T * m_S / 2;
						AddBox(dress, m.x - 0.04, m.y - 0.04, top - 0.1, m.x + 0.04, m.y + 0.04, top + 0.6);
						AddBox(dress, m.x - 0.09, m.y - 0.09, top + 0.45, m.x + 0.09, m.y + 0.09, top + 0.63);
					}
				}
				if(hull.isEmpty() && walls.isEmpty())
					continue;
				Keep(hull, 0);
				Keep(walls, 0);
				if(m_Theme != Plain)
				{
					Paint(hull, Tint(RoofPaint(m_Theme), ROOF_TINT));
					FinishOutsideTrimWall(m_Plan, walls, OUTSIDE_TRIM_PAINT);
				}
				Roofing(hull);
				Outside(walls);
				AuScHull part;
				part.AddFaces(hull);
				part.AddFaces(walls);
				add(part, QString("roof %1,%2 deck %3").arg(c).arg(r).arg(m_Deck));
			}
		if(m_Key == "flat")
			Parapet(dress, trim);
		else if(!perCell)
		{
			Eaves(eaves, trim);
			EdgesToRoof(trim, eaves);
		}
		Keep(eaves, 1);
		Keep(trim, 2);
		if(m_Theme != Plain)
		{
			Paint(eaves, Tint(RoofPaint(m_Theme), ROOF_TINT));
			FinishOutsideTrim(m_Plan, trim, OUTSIDE_TRIM_PAINT);
			FinishOutsideTrim(m_Plan, dress, OUTSIDE_TRIM_PAINT);
		}
		Roofing(eaves);
		if(!eaves.isEmpty() || !trim.isEmpty() || !dress.isEmpty())
		{
			AuScMesh mesh;
			for(AuScFaceList* list : {&eaves, &trim, &dress})
				mesh.AddFaces(*list);
			add(mesh, QString("roof %1 eaves deck %2").arg(m_Roof.letter).arg(m_Deck));
		}
		return count;
	}

	void Check(QTextStream& log, int& points, int& bad) const
	{
		const double f = FeetPerMeter();
		auto report = [&](const QString& what, const V2& p) {
			if(bad++ < 5)
				log << "  Roof " << m_Roof.letter << " on deck " << m_Deck << ": " << what << " at " << p.x << ", " << p.y << "\n";
		};
		auto seen = [&](const V2& p, bool up, int kind, QList<double>& zs) {
			for(const Tri3& t : m_Tris)
			{
				if(t.kind != kind || (t.n.z > 1e-6) != up || qAbs(t.n.z) < 1e-6)
					continue;
				const AuPoint& a = t.v[0];
				const AuPoint& b = t.v[1];
				const AuPoint& c = t.v[2];
				double px = p.x * f, py = p.y * f;
				double det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
				if(qAbs(det) < 1e-12)
					continue;
				double l1 = ((b.y - c.y) * (px - c.x) + (c.x - b.x) * (py - c.y)) / det;
				double l2 = ((c.y - a.y) * (px - c.x) + (a.x - c.x) * (py - c.y)) / det, l3 = 1 - l1 - l2;
				if(l1 < -1e-9 || l2 < -1e-9 || l3 < -1e-9)
					continue;
				zs << (l1 * a.z + l2 * b.z + l3 * c.z) / f;
			}
		};
		auto doubled = [](QList<double> zs) {
			std::sort(zs.begin(), zs.end());
			for(int k = 0; k + 1 < zs.size(); k++)
				if(zs[k + 1] - zs[k] < 0.002)
					return true;
			return false;
		};
		const bool perCell = m_Key == "dome" || m_Key == "spire";
		if(m_Key == "flat")
			return;
		const int steps = 9;
		for(int r = 0; r < m_Plan.Rows(); r++)
			for(int c = 0; c < m_Plan.Cols(); c++)
			{
				QPoint cell(c, r);
				if(!In(cell) || (perCell && !Whole(cell)))
					continue;
				for(int i = 0; i < steps; i++)
					for(int j = 0; j < steps; j++)
					{
						V2 p(m_Plan.X0(c) + m_S * (i + 0.5137) / steps, m_Plan.Y0(r) + m_S * (j + 0.4713) / steps);
						QPoint pc;
						int q;
						if(!QuarterAtPoint(m_Plan, p.x, p.y, pc, q) || !InQ(pc, q) || InSkylight(p, cell))
							continue;
						if(m_Key == "dome" && (p - Mid(cell)).Length() > m_S / 2 - 0.2)
							continue;
						QList<double> zs;
						seen(p, true, 0, zs);
						points++;
						if(zs.isEmpty())
							report("a hole in the roof", p);
						else if(*std::min_element(zs.begin(), zs.end()) < m_Base - 0.02)
							report("the roof below the deck's top", p);
						else if(doubled(zs))
							report("two faces of the roof on top of each other", p);
					}
			}
		if(perCell)
			return;
		for(const Edge& e : m_Edges)
		{
			if(!e.outside)
				continue;
			V2 u = (e.b - e.a).Unit(), n = u.Left() * -1;
			double len = (e.b - e.a).Length();
			for(double s = 0.1; s < len - 0.1 + 1e-9; s += len / 7)
				for(double out : {0.08, 0.19})
				{
					V2 p = e.a + u * (s + 0.0137) + n * out;
					QList<double> above, below;
					seen(p, true, 1, above);
					seen(p, false, 2, below);
					points++;
					if(above.isEmpty())
						report("a gap in the eaves", p);
					else if(below.isEmpty())
						report("a gap in the soffit", p);
					else if(doubled(above))
						report("two eaves on top of each other", p);
				}
		}
	}

	bool InSkylight(const V2& p, const QPoint& cell) const
	{
		if(!m_Skylit.contains(cell))
			return false;
		double x0, y0, x1, y1;
		SkylightHole(m_Plan, cell, x0, y0, x1, y1);
		return p.x > x0 && p.x < x1 && p.y > y0 && p.y < y1;
	}

	void BuildSkylight(const Skylight& s, const std::function<quint16(AuScPart&, const QString&)>& add) const
	{
		double x0, y0, x1, y1;
		SkylightHole(m_Plan, s.cell, x0, y0, x1, y1);
		const V2 c[4] = {V2(x0, y0), V2(x1, y0), V2(x1, y1), V2(x0, y1)};
		const double cw = 0.1, ch = 0.12;
		AuScFaceList shaft, curb, glass;
		for(int k = 0; k < 4; k++)
		{
			const V2 a = c[k], b = c[(k + 1) % 4], u = (b - a).Unit(), in = u.Left();
			const QVector<V2> pts = Along(a, b, s.cell);
			auto outer = [&](int i) {
				double t = (pts[i] - a).Length() / (b - a).Length();
				return pts[i] - in * cw + u * (cw * (2 * t - 1));
			};
			for(int i = 0; i + 1 < pts.size(); i++)
			{
				const V2 &p = pts[i], &q = pts[i + 1], op = outer(i), oq = outer(i + 1);
				const double hp = Height(p, s.cell), hq = Height(q, s.cell), hop = Height(op, s.cell), hoq = Height(oq, s.cell);
				Upright(shaft, p, q, 0, hp, 0, hq, in);
				Upright(curb, p, q, hp, hp + ch, hq, hq + ch, in);
				Upright(curb, op, oq, hop - 0.03, hop + ch, hoq - 0.03, hoq + ch, in * -1);
				Tri(curb, p, hp + ch, op, hop + ch, oq, hoq + ch);
				Tri(curb, p, hp + ch, oq, hoq + ch, q, hq + ch);
			}
		}
		for(bool down : {false, true})
		{
			m_Lift = ch - 0.03;
			m_Flip = down;
			Surface(glass, s.cell, true);
		}
		m_Lift = 0;
		m_Flip = false;
		const QString room = m_Plan.rooms.value(m_Plan.At(m_Deck, s.cell)).name;
		if(m_Theme != Plain)
			FinishOutsideTrim(m_Plan, curb, OUTSIDE_TRIM_PAINT);
		AuScMesh mesh;
		mesh.AddFaces(shaft);
		mesh.AddFaces(curb);
		if(m_Plan.interiors)
			mesh.SetGroupName(room);
		add(mesh, QString("skylight %1,%2 deck %3").arg(s.cell.x()).arg(s.cell.y()).arg(m_Deck));
		Paint(glass, m_Plan.interiors ? QColor(0xb0, 0xd0, 0xe0) : QColor(0, 0, 0));
		AuScWindow pane;
		pane.AddFaces(glass);
		pane.SetOpacity(m_Plan.interiors ? 0.25f : 1.f);
		if(m_Plan.interiors)
			pane.SetGroupName(room);
		add(pane, "skylight glass");
	}

	struct DormerFrame
	{
		QString key;
		V2 f, u, v, n;
		double w = 0, zb[2] = {0, 0}, ze = 0, zr = 0, rise = 0, ye[2] = {0, 0}, ym = 0;
		V2 Corner(int i) const { return f + u * (i ? w / 2 : -w / 2); }
	};

	bool FrameDormer(const Dormer& d, DormerFrame& fr, QString& problem) const
	{
		fr.key = d.style.isEmpty() ? "gable" : d.style;
		if(m_Key != "gable" && m_Key != "hip" && m_Key != "shed" && m_Key != "gambrel" && m_Key != "mansard")
		{
			problem = "A dormer needs a sloping roof under it: a gable, hip, shed, gambrel or mansard roof.";
			return false;
		}
		if(!Whole(d.cell))
		{
			problem = "A dormer needs a whole cell of roof.";
			return false;
		}
		bool outside = true;
		for(int q = 0; q < 4; q++)
		{
			const QPoint nb = Neighbour(d.cell, Side(((d.side % 4) + 4) % 4));
			outside = outside && (m_Plan.Quarter(m_Deck, nb, q) == '.' || IsOpenAirQuarter(m_Plan, m_Deck, nb, q));
		}
		if(!outside)
		{
			problem = "A dormer looks out over an outside wall: the cell on that side must be outside the building.";
			return false;
		}
		static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
		fr.n = dirs[((d.side % 4) + 4) % 4];
		fr.v = fr.n * -1;
		fr.u = V2(-fr.n.y, fr.n.x);
		fr.w = qMin(1.4, m_S * 0.5);
		fr.f = Mid(d.cell) + fr.n * (m_S / 2 - DORMER_SETBACK);
		const QPoint home = d.cell;
		if(Height(fr.f + fr.v * 0.5, home) - Height(fr.f, home) < 0.12)
		{
			problem = "The roof does not rise from that side of the cell: a dormer looks out down the slope, over the eaves.";
			return false;
		}
		for(int i = 0; i < 2; i++)
			fr.zb[i] = Height(fr.Corner(i), home);
		fr.ze = qMax(fr.zb[0], fr.zb[1]) + 1.05;
		fr.zr = fr.key == "gable" ? fr.ze + fr.w / 2 * 0.8 : fr.ze;
		fr.rise = fr.key == "shed" ? qTan(qDegreesToRadians(12.0)) : 0;
		auto reach = [&](const V2& p, double z, double rise) {
			const double step = 0.05;
			for(double y = step; y < 2 * m_S; y += step)
			{
				V2 q = p + fr.v * y;
				QPoint c;
				int qq;
				if(!QuarterAtPoint(m_Plan, q.x, q.y, c, qq) || !InQ(c, qq))
					return -1.0;
				if(Height(q, home) < z + rise * y)
					continue;
				double lo = y - step, hi = y;
				for(int k = 0; k < 40; k++)
				{
					double mid = (lo + hi) / 2;
					(Height(p + fr.v * mid, home) >= z + rise * mid ? hi : lo) = mid;
				}
				return hi;
			}
			return -1.0;
		};
		for(int i = 0; i < 2; i++)
			fr.ye[i] = reach(fr.Corner(i), fr.ze, fr.rise);
		fr.ym = reach(fr.f, fr.zr, fr.rise);
		if(fr.ye[0] < 0 || fr.ye[1] < 0 || fr.ym < 0)
		{
			problem = "The roof does not rise high enough behind the dormer to take it in: it would stand clear of the roof.";
			return false;
		}
		return true;
	}

	void BuildDormer(const Dormer& d, const std::function<quint16(AuScPart&, const QString&)>& add) const
	{
		DormerFrame fr;
		QString problem;
		if(!FrameDormer(d, fr, problem))
			return;
		const QPoint home = d.cell;
		auto Z = [&](double h) { return m_Base + h; };
		AuScFaceList body, top, trim, dark;
		const double t = 0.08, ov = 0.15, ovs = 0.12;
		const double zlow = qMin(fr.zb[0], fr.zb[1]) - 0.1;
		const double ww = fr.w - 0.5, sill = qMax(fr.zb[0], fr.zb[1]) + 0.2, head = fr.ze - 0.12;
		const V2 A = fr.Corner(0), B = fr.Corner(1);
		AddWall(body, A, B, Z(zlow), Z(fr.ze), fr.n, {}, {Hole{(fr.w - ww) / 2, (fr.w + ww) / 2, Z(sill), Z(head)}});
		if(fr.key == "gable")
			AddTri(body, Ft(A, Z(fr.ze)), Ft(B, Z(fr.ze)), Ft(fr.f, Z(fr.zr)), AuPoint(fr.n.x, fr.n.y, 0));
		const V2 h0 = fr.f - fr.u * (ww / 2), h1 = fr.f + fr.u * (ww / 2), back = fr.v * 0.12;
		AddQuad(body, Ft(h0, Z(sill)), Ft(h0, Z(head)), Ft(h0 + back, Z(head)), Ft(h0 + back, Z(sill)), AuPoint(fr.u.x, fr.u.y, 0));
		AddQuad(body, Ft(h1, Z(sill)), Ft(h1, Z(head)), Ft(h1 + back, Z(head)), Ft(h1 + back, Z(sill)), AuPoint(-fr.u.x, -fr.u.y, 0));
		AddQuad(body, Ft(h0, Z(sill)), Ft(h0 + back, Z(sill)), Ft(h1 + back, Z(sill)), Ft(h1, Z(sill)), AuPoint(0, 0, 1));
		AddQuad(body, Ft(h0, Z(head)), Ft(h0 + back, Z(head)), Ft(h1 + back, Z(head)), Ft(h1, Z(head)), AuPoint(0, 0, -1));
		AddQuad(dark, Ft(h0 + back, Z(sill)), Ft(h0 + back, Z(head)), Ft(h1 + back, Z(head)), Ft(h1 + back, Z(sill)),
			AuPoint(fr.n.x, fr.n.y, 0));
		for(int i = 0; i < 2; i++)
		{
			const V2 side = i ? fr.u : fr.u * -1, c = fr.Corner(i);
			const QVector<V2> pts = Along(c, c + fr.v * fr.ye[i], home);
			for(int k = 0; k + 1 < pts.size(); k++)
			{
				double ya = (pts[k] - fr.f).Dot(fr.v), yb = (pts[k + 1] - fr.f).Dot(fr.v);
				Upright(body, pts[k], pts[k + 1], Height(pts[k], home) - 0.1, fr.ze + fr.rise * ya, Height(pts[k + 1], home) - 0.1,
					fr.ze + fr.rise * yb, side);
			}
		}
		auto slab = [&](const QVector<V2>& poly, const QVector<double>& h) {
			for(int k = 1; k + 1 < poly.size(); k++)
			{
				Tri(top, poly[0], h[0], poly[k], h[k], poly[k + 1], h[k + 1]);
				Tri(trim, poly[0], h[0] - t, poly[k], h[k] - t, poly[k + 1], h[k + 1] - t, false);
			}
		};
		auto fascia = [&](const V2& a, double ha, const V2& b, double hb, const V2& facing) {
			Upright(trim, a, b, ha - t, ha, hb - t, hb, facing);
		};
		if(fr.key == "gable")
		{
			for(int i = 0; i < 2; i++)
			{
				const V2 side = i ? fr.u : fr.u * -1, c = fr.Corner(i) + side * ovs;
				const V2 r0 = fr.f + fr.n * ov, r1 = fr.f + fr.v * fr.ym, e0 = c + fr.n * ov, e1 = c + fr.v * fr.ye[i];
				const double he = fr.ze - ovs * (fr.zr - fr.ze) / (fr.w / 2);
				slab({r0, e0, e1, r1}, {fr.zr, he, he, fr.zr});
				fascia(r0, fr.zr, e0, he, fr.n);
				fascia(e0, he, e1, he, side);
			}
		}
		else
		{
			auto at = [&](const V2& p) { return fr.ze + fr.rise * (p - fr.f).Dot(fr.v); };
			const V2 f0 = A - fr.u * ovs + fr.n * ov, f1 = B + fr.u * ovs + fr.n * ov;
			const V2 b1 = B + fr.u * ovs + fr.v * fr.ye[1], m = fr.f + fr.v * fr.ym, b0 = A - fr.u * ovs + fr.v * fr.ye[0];
			slab({f0, f1, b1, m, b0}, {at(f0), at(f1), at(b1), at(m), at(b0)});
			fascia(f0, at(f0), f1, at(f1), fr.n);
			fascia(f1, at(f1), b1, at(b1), fr.u);
			fascia(b0, at(b0), f0, at(f0), fr.u * -1);
		}
		const QString room = m_Plan.rooms.value(m_Plan.At(m_Deck, d.cell)).name;
		EdgesToRoof(trim, top);
		if(m_Theme != Plain)
		{
			Paint(top, Tint(RoofPaint(m_Theme), ROOF_TINT));
		}
		Roofing(top);
		Outside(body);
		Outside(trim);
		if(m_Theme != Plain)
		{
			FinishOutsideTrimWall(m_Plan, body, OUTSIDE_TRIM_PAINT);
			FinishOutsideTrim(m_Plan, trim, OUTSIDE_TRIM_PAINT);
		}
		Paint(dark, QColor(0x2c, 0x2d, 0x31));
		AuScHull hull;
		hull.AddFaces(body);
		hull.AddFaces(top);
		hull.AddFaces(trim);
		hull.AddFaces(dark);
		add(hull, QString("dormer %1,%2 deck %3").arg(d.cell.x()).arg(d.cell.y()).arg(m_Deck));
		const int tech = m_Theme == Metal || m_Theme == Plain ? 4 : 3;
		WindowGeo g{fr.f + fr.v * 0.06, fr.u, fr.n, ww, Z(sill), Z(head), 0.06, 0.06, true, tech, m_Theme};
		AuScWindow glass;
		AuScFaceList pane;
		WindowGlass(pane, g);
		if(!m_Plan.interiors)
			Paint(pane, QColor(0, 0, 0));
		glass.AddFaces(pane);
		glass.SetOpacity(m_Plan.interiors ? 0.25f : 1.f);
		if(m_Plan.interiors)
			glass.SetGroupName(room);
		add(glass, "dormer window");
		WindowMesh wm;
		WindowFrames(wm, g, 1);
		PaintWindowMesh(wm, g);
		if(!wm.frame.isEmpty() || !wm.dark.isEmpty() || !wm.shutter.isEmpty() || !wm.glow.isEmpty())
		{
			AuScMesh frames;
			for(AuScFaceList* list : {&wm.frame, &wm.dark, &wm.shutter, &wm.glow})
				frames.AddFaces(*list);
			add(frames, "dormer window frame");
		}
	}

	double Soffit(const V2& p) const
	{
		if(m_Key == "flat" || m_Key == "dome" || m_Key == "spire")
			return 1e9;
		double best = 1e9;
		for(const Edge& e : m_Edges)
		{
			if(!e.outside)
				continue;
			const V2 u = (e.b - e.a).Unit(), n = u.Left() * -1;
			const double len = (e.b - e.a).Length(), s = (p - e.a).Dot(u), out = (p - e.a).Dot(n);
			if(out < -0.01 || out > ROOF_OVERHANG + 0.3 || s < -ROOF_OVERHANG || s > len + ROOF_OVERHANG)
				continue;
			const double d = 0.02;
			const V2 f = e.a + u * qBound(0.05, s, len - 0.05);
			const double he = Height(f, e.cell), across = (he - Height(f - n * d, e.cell)) / d;
			best = qMin(best, m_Base + he + across * qMax(0.0, out) - ROOF_THICKNESS);
		}
		return best;
	}

private:
	struct Tri3
	{
		AuPoint v[3];
		AuPoint n;
		int kind;
	};
	QList<Tri3> m_Tris;

	void Keep(const AuScFaceList& faces, int kind)
	{
		for(const AuScFace* face : faces)
		{
			AuScTriList tl;
			face->Triangles(tl);
			for(AuScTriList::const_iterator ti = tl.cbegin(); ti != tl.cend(); ti++)
			{
				Tri3 t;
				for(int k = 0; k < 3; k++)
					t.v[k] = (*ti).Vertex(k);
				t.n = (*ti).Normal();
				t.kind = kind;
				m_Tris << t;
			}
		}
	}

	struct Edge
	{
		V2 a, b;
		QPoint cell;
		int q;
		QPoint ocell;
		int oq;
		bool outside;
	};

	V2 Mid(const QPoint& c) const { return V2((m_Plan.X0(c.x()) + m_Plan.X1(c.x())) / 2, (m_Plan.Y0(c.y()) + m_Plan.Y1(c.y())) / 2); }

	static double PieceDistance(const V2& p, const QVector<V2>& piece)
	{
		double d = -1e9;
		if(piece.size() == 4)
		{
			for(int k = 0; k < 4; k++)
			{
				V2 a = piece[k], u = (piece[(k + 1) % 4] - a).Unit();
				d = qMax(d, -u.Left().Dot(p - a));
			}
			return d;
		}
		for(int i = 0; i < 8; i++)
		{
			double angle = M_PI / 4 * i;
			V2 n(qCos(angle), qSin(angle));
			double support = -1e9;
			for(const V2& v : piece)
				support = qMax(support, n.Dot(v));
			d = qMax(d, n.Dot(p) - support);
		}
		return d;
	}

	void Run(const QPoint& home, double& a0, double& a1) const
	{
		QPoint lo = home, hi = home, step = m_AlongX ? QPoint(1, 0) : QPoint(0, 1);
		while(In(lo - step))
			lo -= step;
		while(In(hi + step))
			hi += step;
		if(m_AlongX)
		{
			a0 = m_Plan.X0(lo.x());
			a1 = m_Plan.X1(hi.x());
		}
		else
		{
			a0 = m_Plan.Y0(hi.y());
			a1 = m_Plan.Y1(lo.y());
		}
	}

	double EdgeDistance(const V2& p) const
	{
		bool inside = false;
		for(const V2& nudge : {V2(1e-5, 2e-5), V2(-2e-5, 1e-5), V2(-1e-5, -2e-5), V2(2e-5, -1e-5)})
		{
			QPoint cell;
			int q;
			inside = inside || (QuarterAtPoint(m_Plan, p.x + nudge.x, p.y + nudge.y, cell, q) && InQ(cell, q));
		}
		double best = 1e9;
		for(const QVector<V2>& piece : inside ? m_OutPieces : m_InPieces)
			best = qMin(best, qMax(0.0, PieceDistance(p, piece)));
		return inside ? best : -best;
	}

	double TwoPitch(double d) const
	{
		const double d1 = m_S / 4, shallow = qTan(qDegreesToRadians(m_Key == "mansard" ? 20.0 : 25.0));
		return d < d1 ? m_T * d : m_T * d1 + shallow * (d - d1);
	}

	double Height(const V2& p, const QPoint& home) const
	{
		if(m_Key == "hip")
			return m_T * EdgeDistance(p);
		if(m_Key == "mansard")
			return TwoPitch(EdgeDistance(p));
		if(m_Key == "spire")
		{
			V2 m = Mid(home);
			return m_T * (m_S / 2 - qMax(qAbs(p.x - m.x), qAbs(p.y - m.y)));
		}
		return qMin(RunHeight(p, home), DiagHeight(p));
	}

	double RunHeight(const V2& p, const QPoint& home) const
	{
		double a0, a1;
		Run(home, a0, a1);
		double pc = m_AlongX ? p.x : p.y, h;
		if(m_Key == "shed")
			h = m_Roof.dir == North || m_Roof.dir == East ? m_T * (a1 - pc) : m_T * (pc - a0);
		else
		{
			double d = qMin(pc - a0, a1 - pc);
			if(m_Key == "gambrel")
				h = TwoPitch(d);
			else if(m_Key == "round")
			{
				double w = a1 - a0, f = 0.3 * w, R = (w * w / 4 + f * f) / (2 * f), x = qAbs(pc - (a0 + a1) / 2);
				h = x <= w / 2 ? qSqrt(qMax(0.0, R * R - x * x)) - (R - f) : -(x - w / 2) * (w / 2) / qSqrt(R * R - w * w / 4);
			}
			else
				h = m_T * d;
		}
		return h;
	}

	double DiagHeight(const V2& p) const
	{
		double diag = 1e9;
		for(const QVector<V2>& piece : m_DiagPieces)
			diag = qMin(diag, PieceDistance(p, piece));
		return diag < 1e8 ? m_TDiag * diag : 1e9;
	}

	void Tri(AuScFaceList& f, const V2& a, double ha, const V2& b, double hb, const V2& c, double hc, bool up = true) const
	{
		AuPoint A(a.x, a.y, ha), B(b.x, b.y, hb), C(c.x, c.y, hc);
		AuPoint n = (B - A).Cross(C - A);
		double len = n.Length();
		if(len < 1e-9)
			return;
		n = n * (1 / len);
		if((n.z < 0) == (up != m_Flip))
			n = n * -1;
		AddTri(f, Ft(a, m_Base + m_Lift + ha), Ft(b, m_Base + m_Lift + hb), Ft(c, m_Base + m_Lift + hc), n);
	}

	void Upright(AuScFaceList& f, const V2& a, const V2& b, double loA, double hiA, double loB, double hiB, const V2& n) const
	{
		const double eps = 1e-4;
		bool tallA = hiA - loA > eps, tallB = hiB - loB > eps;
		AuPoint nn(n.x, n.y, 0);
		if(!tallA && !tallB)
			return;
		if(!tallA)
			AddTri(f, Ft(a, m_Base + loA), Ft(b, m_Base + loB), Ft(b, m_Base + hiB), nn);
		else if(!tallB)
			AddTri(f, Ft(a, m_Base + loA), Ft(a, m_Base + hiA), Ft(b, m_Base + loB), nn);
		else
			AddQuad(f, Ft(a, m_Base + loA), Ft(a, m_Base + hiA), Ft(b, m_Base + hiB), Ft(b, m_Base + loB), nn);
	}

	void Surface(AuScFaceList& f, const QPoint& cell, bool holes = false) const
	{
		const bool whole = Whole(cell);
		auto keep = [&](const V2& p) { return InSkylight(p, cell) == holes; };
		const int n = m_Steps;
		const double x0 = m_Plan.X0(cell.x()), y0 = m_Plan.Y0(cell.y()), d = m_S / n;
		for(int i = 0; i < n; i++)
			for(int j = 0; j < n; j++)
			{
				V2 p[4] = {V2(x0 + i * d, y0 + j * d), V2(x0 + (i + 1) * d, y0 + j * d), V2(x0 + (i + 1) * d, y0 + (j + 1) * d),
					V2(x0 + i * d, y0 + (j + 1) * d)};
				V2 m(x0 + (i + 0.5) * d, y0 + (j + 0.5) * d);
				double hm = Height(m, cell), h[4];
				for(int k = 0; k < 4; k++)
					h[k] = Height(p[k], cell);
				if(m_Key == "round" && whole && qAbs(h[0] + h[2] - h[1] - h[3]) < 1e-6)
				{
					if(!keep(m))
						continue;
					Facet(f, p[0], h[0], p[1], h[1], p[2], h[2], cell);
					Facet(f, p[0], h[0], p[2], h[2], p[3], h[3], cell);
					continue;
				}
				for(int k = 0; k < 4; k++)
				{
					if(!whole)
					{
						V2 centroid = (m + p[k] + p[(k + 1) % 4]) * (1.0 / 3);
						QPoint c;
						int q;
						if(!QuarterAtPoint(m_Plan, centroid.x, centroid.y, c, q) || !InQ(c, q))
							continue;
					}
					if(!keep((m + p[k] + p[(k + 1) % 4]) * (1.0 / 3)))
						continue;
					Facet(f, m, hm, p[k], h[k], p[(k + 1) % 4], h[(k + 1) % 4], cell);
				}
			}
	}

	bool Kink(V2 a, V2 b, const QPoint& home, V2& k, double& hk) const
	{
		if(a.x > b.x + 1e-9 || (qAbs(a.x - b.x) <= 1e-9 && a.y > b.y))
			std::swap(a, b);
		const double L = (b - a).Length();
		if(L < 1e-3)
			return false;
		const V2 u = (b - a) * (1 / L);
		const double ha = Height(a, home), hb = Height(b, home), hm = Height((a + b) * 0.5, home);
		if(qAbs(hm - (ha + hb) / 2) < 1e-4)
			return false;
		const double eps = qMin(1e-3, L / 100);
		auto creased = [&](double x) {
			V2 p = a + u * x;
			double h = Height(p, home);
			return qAbs((Height(p + u * eps, home) - h) - (h - Height(p - u * eps, home))) / eps > 0.01;
		};
		const double sa = (Height(a + u * eps, home) - ha) / eps, sb = (hb - Height(b - u * eps, home)) / eps;
		double x = -1;
		if(qAbs(sa - sb) > 1e-6)
		{
			double t = (hb - ha - sb * L) / (sa - sb);
			if(t > L * 0.01 && t < L * 0.99 && creased(t))
				x = t;
		}
		if(x < 0 && m_Key != "hip" && m_Key != "mansard" && m_Key != "spire" && !m_DiagPieces.isEmpty())
		{
			auto gap = [&](double t) { V2 p = a + u * t; return RunHeight(p, home) - DiagHeight(p); };
			double lo = 0, hi = L, glo = gap(lo), ghi = gap(hi);
			if((glo < 0) != (ghi < 0))
			{
				for(int i = 0; i < 50 && hi - lo > 1e-7; i++)
				{
					double mid = (lo + hi) / 2, g = gap(mid);
					((g < 0) == (glo < 0) ? lo : hi) = mid;
				}
				double t = (lo + hi) / 2;
				if(t > L * 0.01 && t < L * 0.99)
					x = t;
			}
		}
		if(x < 0)
			return false;
		k = a + u * x;
		hk = Height(k, home);
		return true;
	}

	QVector<V2> Along(const V2& a, const V2& b, const QPoint& home, const QPoint* other = nullptr) const
	{
		QVector<V2> points{a};
		V2 k;
		double hk;
		QList<V2> kinks;
		if(Kink(a, b, home, k, hk))
			kinks << k;
		if(other && Kink(a, b, *other, k, hk))
			kinks << k;
		std::sort(kinks.begin(), kinks.end(), [&](const V2& x, const V2& y) { return (x - a).Length() < (y - a).Length(); });
		for(const V2& p : kinks)
			points << p;
		points << b;
		return points;
	}

	void Facet(AuScFaceList& f, const V2& a, double ha, const V2& b, double hb, const V2& c, double hc, const QPoint& home) const
	{
		const V2 p[3] = {a, b, c};
		const double h[3] = {ha, hb, hc};
		V2 k[3];
		double hk[3];
		bool split[3];
		int count = 0;
		for(int e = 0; e < 3; e++)
			count += (split[e] = Kink(p[e], p[(e + 1) % 3], home, k[e], hk[e]));
		if(count == 0)
			Tri(f, a, ha, b, hb, c, hc);
		else if(count == 1)
		{
			int e = split[0] ? 0 : split[1] ? 1 : 2, o = (e + 2) % 3, e1 = (e + 1) % 3;
			Tri(f, p[e], h[e], k[e], hk[e], p[o], h[o]);
			Tri(f, k[e], hk[e], p[e1], h[e1], p[o], h[o]);
		}
		else if(count == 2)
		{
			int e = !split[0] ? 0 : !split[1] ? 1 : 2, e1 = (e + 1) % 3, e2 = (e + 2) % 3;
			Tri(f, k[e1], hk[e1], p[e2], h[e2], k[e2], hk[e2]);
			Tri(f, p[e], h[e], p[e1], h[e1], k[e1], hk[e1]);
			Tri(f, p[e], h[e], k[e1], hk[e1], k[e2], hk[e2]);
		}
		else
		{
			V2 m = (a + b + c) * (1.0 / 3);
			double hm = Height(m, home);
			for(int e = 0; e < 3; e++)
			{
				int e1 = (e + 1) % 3;
				Tri(f, p[e], h[e], k[e], hk[e], m, hm);
				Tri(f, k[e], hk[e], p[e1], h[e1], m, hm);
			}
		}
	}

	void Walls(AuScFaceList& f, const QPoint& cell) const
	{
		const int n = m_Steps;
		for(int q = 0; q < 4; q++)
		{
			if(!InQ(cell, q))
				continue;
			QVector<V2> t = QuarterPoly(m_Plan, cell, q);
			for(int k = 0; k < 3; k++)
			{
				QPoint oc;
				int oq;
				QuarterAcross(cell, q, k, oc, oq);
				const bool inner = InQ(oc, oq);
				if(inner && (oc == cell || (oc.x() < cell.x() || oc.y() < cell.y())))
					continue;
				V2 e0 = t[k], e1 = t[(k + 1) % 3], out = (e1 - e0).Unit().Left() * -1;
				for(int s = 0; s < n; s++)
				{
					V2 sa = e0 + (e1 - e0) * (double(s) / n), sb = e0 + (e1 - e0) * (double(s + 1) / n);
					const QVector<V2> points = Along(sa, sb, cell, inner ? &oc : nullptr);
					for(int i = 0; i + 1 < points.size(); i++)
					{
						const V2& a = points[i];
						const V2& b = points[i + 1];
						double ha = qMax(0.0, Height(a, cell)), hb = qMax(0.0, Height(b, cell));
						if(!inner)
						{
							Upright(f, a, b, 0, ha, 0, hb, out);
							continue;
						}
						double oa = qMax(0.0, Height(a, oc)), ob = qMax(0.0, Height(b, oc));
						if(qAbs(ha - oa) < 1e-4 && qAbs(hb - ob) < 1e-4)
							continue;
						V2 facing = ha + hb < oa + ob ? out * -1 : out;
						Upright(f, a, b, qMin(ha, oa), qMax(ha, oa), qMin(hb, ob), qMax(hb, ob), facing);
					}
				}
			}
		}
	}

	void Eaves(AuScFaceList& top, AuScFaceList& trim) const
	{
		auto same = [](const V2& a, const V2& b) { return (a - b).Length() < 1e-6; };
		const double OH = ROOF_OVERHANG, t = ROOF_THICKNESS;
		for(const Edge& e : m_Edges)
		{
			if(!e.outside)
				continue;
			const V2 u = (e.b - e.a).Unit(), n = u.Left() * -1;
			const Edge* before = nullptr;
			const Edge* after = nullptr;
			int beforeCount = 0, afterCount = 0;
			for(const Edge& f : m_Edges)
			{
				if(same(f.b, e.a))
					before = &f, beforeCount++;
				if(same(f.a, e.b))
					after = &f, afterCount++;
			}
			auto mitre = [&](const Edge* f, bool& capped) {
				capped = !f || !f->outside;
				if(capped)
					return n * OH;
				V2 nf = (f->b - f->a).Unit().Left() * -1;
				double denom = 1 + nf.Dot(n);
				if(denom < 0.3)
				{
					capped = true;
					return n * OH;
				}
				return (nf + n) * (OH / denom);
			};
			bool capA, capB;
			V2 ma = mitre(beforeCount == 1 ? before : nullptr, capA), mb = mitre(afterCount == 1 ? after : nullptr, capB);
			const double len = (e.b - e.a).Length();
			const bool straight = qAbs(u.x) < 1e-9 || qAbs(u.y) < 1e-9;
			const int count = qMax(1, qRound(m_Steps * len / (straight ? m_S : m_S * M_SQRT1_2)));
			QVector<V2> pts;
			for(int k = 0; k < count; k++)
			{
				QVector<V2> along = Along(e.a + (e.b - e.a) * (double(k) / count), e.a + (e.b - e.a) * (double(k + 1) / count), e.cell);
				pts << along.mid(k == 0 ? 0 : 1);
			}
			const int np = pts.size();
			QVector<double> he(np), across(np, 0), along(np, 0), ho(np);
			QVector<V2> off(np);
			QVector<int> pieces(np, 0);
			for(int i = 0; i < np; i++)
			{
				he[i] = Height(pts[i], e.cell);
				double s = (pts[i] - e.a).Dot(u) / len;
				off[i] = ma + (mb - ma) * s;
			}
			for(int i = 0; i + 1 < np; i++)
			{
				const double piece = (pts[i + 1] - pts[i]).Length(), d = qMin(0.02, piece / 4);
				const V2 mid = (pts[i] + pts[i + 1]) * 0.5;
				const double a = (Height(mid, e.cell) - Height(mid - n * d, e.cell)) / d, b = (he[i + 1] - he[i]) / piece;
				for(int j : {i, i + 1})
				{
					across[j] += a;
					along[j] += b;
					pieces[j]++;
				}
			}
			for(int i = 0; i < np; i++)
				ho[i] = he[i] + (across[i] * off[i].Dot(n) + along[i] * off[i].Dot(u)) / qMax(1, pieces[i]);
			for(int i = 0; i + 1 < np; i++)
			{
				const V2 &e0 = pts[i], &e1 = pts[i + 1], o0 = e0 + off[i], o1 = e1 + off[i + 1];
				Tri(top, e0, he[i], o0, ho[i], o1, ho[i + 1]);
				Tri(top, e0, he[i], o1, ho[i + 1], e1, he[i + 1]);
				Tri(trim, e0, he[i] - t, o0, ho[i] - t, o1, ho[i + 1] - t, false);
				Tri(trim, e0, he[i] - t, o1, ho[i + 1] - t, e1, he[i + 1] - t, false);
				Upright(trim, o0, o1, ho[i] - t, ho[i], ho[i + 1] - t, ho[i + 1], n);
			}
			if(capA)
				Upright(trim, e.a, e.a + ma, he[0] - t, he[0], ho[0] - t, ho[0], u * -1);
			if(capB)
				Upright(trim, e.b, e.b + mb, he[np - 1] - t, he[np - 1], ho[np - 1] - t, ho[np - 1], u);
		}
	}

	void Dome(AuScFaceList& f, AuScFaceList& dress, const QPoint& cell) const
	{
		const int around = 24, rings = 6;
		const double R = m_S / 2 - 0.15, drum = 0.35, rise = R * 0.95;
		V2 m = Mid(cell);
		auto at = [&](int i, double radius) {
			double a = 2 * M_PI * i / around;
			return m + V2(qCos(a), qSin(a)) * radius;
		};
		for(int i = 0; i < around; i++)
		{
			V2 p0 = at(i, R), p1 = at(i + 1, R), radial = (at(i, 1) + at(i + 1, 1) - m * 2).Unit();
			Upright(f, p0, p1, 0, drum, 0, drum, radial);
			for(int j = 0; j < rings; j++)
			{
				double a0 = M_PI / 2 * j / rings, a1 = M_PI / 2 * (j + 1) / rings;
				double r0 = R * qCos(a0), r1 = R * qCos(a1), h0 = drum + rise * qSin(a0), h1 = drum + rise * qSin(a1);
				if(j == rings - 1)
					Tri(f, at(i, r0), h0, at(i + 1, r0), h0, m, h1);
				else
				{
					Tri(f, at(i, r0), h0, at(i + 1, r0), h0, at(i + 1, r1), h1);
					Tri(f, at(i, r0), h0, at(i + 1, r1), h1, at(i, r1), h1);
				}
			}
		}
		double top = m_Base + drum + rise;
		AddBox(dress, m.x - 0.04, m.y - 0.04, top - 0.05, m.x + 0.04, m.y + 0.04, top + 0.45);
		AddBox(dress, m.x - 0.1, m.y - 0.1, top + 0.3, m.x + 0.1, m.y + 0.1, top + 0.5);
	}

	void Parapet(AuScFaceList& wall, AuScFaceList& coping) const
	{
		for(const Edge& e : m_Edges)
		{
			if(m_Plan.Quarter(m_Deck + 1, e.ocell, e.oq) != '.')
				continue;
			V2 u = (e.b - e.a).Unit(), out = u.Left() * -1, mid = (e.a + e.b) * 0.5;
			double len = (e.b - e.a).Length();
			AddWallBox(wall, mid, u, out, -0.2, 0, len, m_Base, m_Base + 0.7);
			AddWallBox(coping, mid, u, out, -0.24, 0.04, len + 0.04, m_Base + 0.7, m_Base + 0.78);
		}
	}

	const Plan& m_Plan;
	int m_Deck;
	Roof m_Roof;
	Theme m_Theme;
	double m_Base;
	double m_S;
	const RoofStyle* m_Style = nullptr;
	QString m_Key;
	double m_T = 0;
	double m_TDiag = 0;
	QList<QPoint> m_Skylit;
	mutable double m_Lift = 0;
	mutable bool m_Flip = false;
	bool m_AlongX = true;
	int m_Steps = 2;
	QList<Edge> m_Edges;
	QList<QVector<V2>> m_InPieces, m_OutPieces;
	QList<QVector<V2>> m_DiagPieces;
};

}
