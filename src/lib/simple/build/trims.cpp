#include "trims.h"
#include "internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

double Ceiling(const WallRun& w, double s0, double s1)
{
	double c = 1e9;
	for(const WallRun::Keep& k : w.ceilings)
		if(k.s0 < s1 - 1e-6 && k.s1 > s0 + 1e-6)
			c = qMin(c, k.z0);
	return c;
}

QList<QPair<double, double>> Free(const WallRun& w, double s0, double s1, double z0, double z1)
{
	QList<QPair<double, double>> cuts;
	if(z0 < DOOR_HEIGHT)
		cuts = w.doors;
	for(const WallRun::Keep& k : w.keep)
		if(z0 < k.z1 && z1 > k.z0)
			cuts << qMakePair(k.s0, k.s1);
	std::sort(cuts.begin(), cuts.end());
	QList<QPair<double, double>> free;
	double s = s0;
	for(const QPair<double, double>& c : cuts)
	{
		if(c.second <= s)
			continue;
		if(c.first >= s1)
			break;
		if(c.first > s)
			free << qMakePair(s, c.first);
		s = qMax(s, c.second);
	}
	if(s < s1)
		free << qMakePair(s, s1);
	return free;
}

bool IsFree(const WallRun& w, double s0, double s1, double z0, double z1)
{
	QList<QPair<double, double>> f = Free(w, s0, s1, z0, z1);
	return f.size() == 1 && f[0].first <= s0 + 1e-9 && f[0].second >= s1 - 1e-9;
}

void RunBox(AuScFaceList& f, const WallRun& w, double s0, double s1, double z0, double z1, double d0, double d1, int skip)
{
	if(s1 - s0 < 0.01 || z1 - z0 < 0.005 || d1 - d0 < 0.002)
		return;
	if(d0 < 1e-9)
		skip |= BoxBack;
	if(w.onFloor && z0 < 1e-9)
		skip |= BoxBottom;
	AddWallBox(f, w.a + w.u * ((s0 + s1) / 2), w.u, w.n, d0, d1, s1 - s0, z0, z1, skip);
}

void Band(AuScFaceList& f, const WallRun& w, double z0, double z1, double d, double ext)
{
	for(const QPair<double, double>& p : Free(w, -ext, w.len + ext, z0, z1))
		RunBox(f, w, p.first, p.second, z0, z1, 0, d);
}

void LogCourse(AuScFaceList& f, const WallRun& w, double s0, double s1, double zc, double h, double d)
{
	if(s1 - s0 < 0.02)
		return;
	V2 p0 = w.a + w.u * s0, p1 = w.a + w.u * s1;
	auto at = [&](const V2& p, double depth, double z) { return Ft(p + w.n * depth, z); };
	double zb = zc - h / 2, zt = zc + h / 2, fb = zc - h / 4, ft = zc + h / 4;
	AuPoint nn(w.n.x, w.n.y, 0), nu(w.u.x, w.u.y, 0);
	AuPoint upOut = Unit3(w.n.x * h / 4, w.n.y * h / 4, d), downOut = Unit3(w.n.x * h / 4, w.n.y * h / 4, -d);
	AddQuad(f, at(p0, d, fb), at(p0, d, ft), at(p1, d, ft), at(p1, d, fb), nn);
	AddQuad(f, at(p0, d, ft), at(p0, 0, zt), at(p1, 0, zt), at(p1, d, ft), upOut);
	AddQuad(f, at(p0, 0, zb), at(p0, d, fb), at(p1, d, fb), at(p1, 0, zb), downOut);
	AddQuad(f, at(p0, 0, zb), at(p0, d, fb), at(p0, d, ft), at(p0, 0, zt), nu * -1);
	AddQuad(f, at(p1, 0, zb), at(p1, d, fb), at(p1, d, ft), at(p1, 0, zt), nu);
}

void LogCourses(AuScFaceList& f, const WallRun& w, double z0, double z1, double pitch, double d)
{
	for(double z = z0; z + pitch <= z1 + 1e-6; z += pitch)
		for(const QPair<double, double>& p : Free(w, 0, w.len, z, z + pitch))
		{
			QList<double> cuts = {p.first, p.second};
			for(const WallRun::Keep& k : w.ceilings)
				if(k.z0 < z + pitch)
					for(double s : {k.s0, k.s1})
						if(s > p.first && s < p.second)
							cuts << s;
			std::sort(cuts.begin(), cuts.end());
			for(int i = 0; i + 1 < cuts.size(); i++)
			{
				double top = qMin(z + pitch, Ceiling(w, cuts[i], cuts[i + 1]));
				if(top - z > 0.08)
					LogCourse(f, w, cuts[i], cuts[i + 1], (z + top) / 2, (top - z) * 0.94, d);
			}
		}
}

void Blocks(AuScFaceList& f, const WallRun& w, double z0, double z1, double rowH, double blockL, double gap, double d,
	const QString& texture, double size)
{
	const int slot = w.lowPoly && w.slot ? w.slot(texture) : -1;
	if(slot >= 0)
	{
		AuScFaceList panel;
		FlatPanel(panel, w, z0, z1, d);
		TextureFaces(panel, slot, size);
		AuScFaceList& to = w.panels ? *w.panels : f;
		while(!panel.isEmpty())
			to.append(panel.takeFirst());
		return;
	}
	auto at = [&](double s, double depth, double z) { return Ft(w.a + w.u * s + w.n * depth, z); };
	const AuPoint nn(w.n.x, w.n.y, 0), nu(w.u.x, w.u.y, 0);
	struct Block { double s0, s1, top; };
	int row = 0;
	for(double z = z0; z + rowH * 0.5 <= z1 + 1e-6; z += rowH, row++)
	{
		const double zt = qMin(z + rowH, z1), zb = z + gap / 2;
		QList<Block> blocks;
		for(double s = (row % 2) ? -blockL / 2 : 0; s < w.len; s += blockL)
			for(const QPair<double, double>& p : Free(w, qMax(0.0, s + gap / 2), qMin(w.len, s + blockL - gap / 2), zb, zt - gap / 2))
				if(p.second - p.first > 0.04)
				{
					double top = qMin(zt - gap / 2, Ceiling(w, p.first, p.second));
					if(top - zb > 0.05)
						blocks << Block{p.first, p.second, top};
				}
		for(int i = 0; i < blocks.size(); )
		{
			int j = i;
			while(j + 1 < blocks.size() && qAbs(blocks[j + 1].s0 - blocks[j].s1 - gap) < 1e-6 && qAbs(blocks[j + 1].top - blocks[i].top) < 1e-9)
				j++;
			const double s0 = blocks[i].s0, s1 = blocks[j].s1, top = blocks[i].top;
			if(i == j)
				RunBox(f, w, s0, s1, zb, top, 0, d);
			else
			{
				for(int k = i; k <= j; k++)
					AddQuad(f, at(blocks[k].s0, d, zb), at(blocks[k].s0, d, top), at(blocks[k].s1, d, top), at(blocks[k].s1, d, zb), nn);
				AddQuad(f, at(s0, 0, top), at(s0, d, top), at(s1, d, top), at(s1, 0, top), AuPoint(0, 0, 1));
				if(!(w.onFloor && zb < 1e-9))
					AddQuad(f, at(s0, 0, zb), at(s0, d, zb), at(s1, d, zb), at(s1, 0, zb), AuPoint(0, 0, -1));
				AddQuad(f, at(s0, 0, zb), at(s0, 0, top), at(s0, d, top), at(s0, d, zb), nu * -1);
				AddQuad(f, at(s1, 0, zb), at(s1, 0, top), at(s1, d, top), at(s1, d, zb), nu);
			}
			i = j + 1;
		}
	}
}

void FlatPanel(AuScFaceList& f, const WallRun& w, double z0, double z1, double d)
{
	QList<double> zs{z0, z1};
	for(const WallRun::Keep& k : w.keep)
		for(double z : {k.z0, k.z1})
			if(z > z0 + 1e-6 && z < z1 - 1e-6)
				zs << z;
	if(!w.doors.isEmpty() && DOOR_HEIGHT > z0 + 1e-6 && DOOR_HEIGHT < z1 - 1e-6)
		zs << DOOR_HEIGHT;
	std::sort(zs.begin(), zs.end());
	struct Piece { double s0, s1, z0, z1; bool capped; };
	QList<Piece> pieces;
	for(int b = 0; b + 1 < zs.size(); b++)
	{
		const double za = zs[b], zb = zs[b + 1];
		if(zb - za < 1e-6)
			continue;
		for(const QPair<double, double>& p : Free(w, 0, w.len, za, zb))
		{
			QList<double> cuts = {p.first, p.second};
			for(const WallRun::Keep& k : w.ceilings)
				for(double s : {k.s0, k.s1})
					if(s > p.first && s < p.second)
						cuts << s;
			std::sort(cuts.begin(), cuts.end());
			for(int i = 0; i + 1 < cuts.size(); i++)
			{
				const double top = qMin(zb, Ceiling(w, cuts[i], cuts[i + 1]));
				if(top - za < 0.01 || cuts[i + 1] - cuts[i] < 0.01)
					continue;
				bool merged = false;
				for(Piece& below : pieces)
					if(!below.capped && qAbs(below.z1 - za) < 1e-9 && qAbs(below.s0 - cuts[i]) < 1e-9 && qAbs(below.s1 - cuts[i + 1]) < 1e-9)
					{
						below.z1 = top;
						below.capped = top < zb - 1e-9;
						merged = true;
						break;
					}
				if(!merged)
					pieces << Piece{cuts[i], cuts[i + 1], za, top, top < zb - 1e-9};
			}
		}
	}
	auto covered = [&](const Piece& p, bool above) {
		for(const Piece& o : pieces)
			if(qAbs(above ? o.z0 - p.z1 : o.z1 - p.z0) < 1e-9 && o.s0 <= p.s0 + 1e-9 && o.s1 >= p.s1 - 1e-9)
				return true;
		return false;
	};
	for(const Piece& p : pieces)
		RunBox(f, w, p.s0, p.s1, p.z0, p.z1, 0, d, (covered(p, true) ? BoxTop : 0) | (covered(p, false) ? BoxBottom : 0));
}

void Panels(AuScFaceList& f, const WallRun& w, double z0, double z1, double panelW, double margin, double d)
{
	for(const QPair<double, double>& span : Free(w, 0, w.len, z0, z1))
	{
		double len = span.second - span.first;
		int count = qMax(1, qRound(len / panelW));
		double pw = len / count;
		if(pw < margin * 3)
			continue;
		for(int i = 0; i < count; i++)
			RunBox(f, w, span.first + i * pw + margin, span.first + (i + 1) * pw - margin, z0 + margin, z1 - margin, 0, d);
	}
}

QList<double> PostSpots(const WallRun& w, double spacing, double width, double z0, double z1)
{
	QList<double> spots;
	if(w.len < width)
		return spots;
	int count = qMax(1, qRound(w.len / spacing));
	for(int i = 0; i <= count; i++)
	{
		double s = qBound(width / 2, w.len * i / count, w.len - width / 2);
		if(IsFree(w, s - width / 2 - 0.03, s + width / 2 + 0.03, z0, z1) && (spots.isEmpty() || s - spots.last() > width))
			spots << s;
	}
	return spots;
}

void Casings(AuScFaceList& f, const WallRun& w, Theme theme, double scale)
{
	double cw = (theme == Wood ? 0.1 : 0.12) * scale, cd = (theme == Wood ? 0.035 : theme == Stone ? 0.045 : 0.05) * scale;
	double head = (theme == Stone ? 0.16 : 0.12) * scale;
	for(const QPair<double, double>& g : w.doors)
	{
		RunBox(f, w, g.first - cw, g.first, 0, DOOR_HEIGHT + head, 0, cd);
		RunBox(f, w, g.second, g.second + cw, 0, DOOR_HEIGHT + head, 0, cd);
		RunBox(f, w, g.first - cw, g.second + cw, DOOR_HEIGHT, DOOR_HEIGHT + head, 0, cd);
		double mid = (g.first + g.second) / 2;
		if(theme == Stone)
			RunBox(f, w, mid - 0.08 * scale, mid + 0.08 * scale, DOOR_HEIGHT - 0.04, DOOR_HEIGHT + head + 0.1 * scale, 0, cd + 0.02);
		else if(theme == Metal)
		{
			RunBox(f, w, g.first, g.first + 0.1, DOOR_HEIGHT - 0.1, DOOR_HEIGHT, 0, cd * 0.6);
			RunBox(f, w, g.second - 0.1, g.second, DOOR_HEIGHT - 0.1, DOOR_HEIGHT, 0, cd * 0.6);
		}
	}
}

int TrimsOf(const QString& type)
{
	static const QMap<QString, int> trims = {
		{"hall", Skirting | Posts | Cornice},
		{"stateroom", Skirting | ChairRail | Cornice},
		{"crewcabin", Skirting | Finish},
		{"officercabin", Skirting | Wainscot | ChairRail | Cornice},
		{"lounge", Skirting | Wainscot | ChairRail | Cornice | Beams},
		{"galley", Skirting | Tiles | PlainCornice},
		{"hold", TallSkirting | Posts | Beams},
		{"barracks", Skirting | Finish | TwoRails},
		{"brig", Skirting | Bars},
		{"sickbay", Skirting | BumperRail | PlainCornice},
		{"engineroom", TallSkirting | Finish | Pipes | Posts | Beams},
		{"vehiclebay", HeavySkirting | HeavyPosts | Beams},
	};
	return trims.value(type, Skirting);
}

void TrimRoomWall(AuScFaceList& f, const WallRun& w, Theme theme, int trims)
{
	if(theme == Plain || w.len < 0.05)
		return;
	const double top = w.top;
	double skirtH = theme == Wood ? 0.15 : theme == Stone ? 0.22 : 0.25;
	if(trims & TallSkirting)
		skirtH *= 2;
	if(trims & HeavySkirting)
		skirtH = 0.5;
	double skirtD = theme == Wood ? 0.025 : theme == Stone ? 0.04 : 0.02;
	if(trims & (Skirting | TallSkirting | HeavySkirting))
		Band(f, w, 0, skirtH, skirtD, skirtD);

	if(trims & Finish)
	{
		if(theme == Wood)
			LogCourses(f, w, skirtH, top - 0.02, 0.26, 0.045);
		else if(theme == Stone)
			Blocks(f, w, skirtH, top, 0.34, 0.62, 0.018, 0.022, "stone-wall", 2.5);
		else
		{
			for(double s = 0.3; s < w.len - 0.1; s += 0.6)
				if(IsFree(w, s - 0.05, s + 0.05, skirtH, top))
					RunBox(f, w, s - 0.025, s + 0.025, skirtH, top, 0, 0.03);
			Band(f, w, 1.45, 1.47, 0.012);
			Band(f, w, top - 0.5, top - 0.48, 0.012);
		}
	}

	double railZ = theme == Metal ? 0.85 : 0.9;
	if(trims & Wainscot)
	{
		if(theme == Wood)
			Panels(f, w, skirtH, railZ, 0.6, 0.07, 0.012);
		else if(theme == Stone)
			Blocks(f, w, skirtH, railZ, 0.25, 0.5, 0.015, 0.02, "stone-wall", 2.5);
		else
			Panels(f, w, skirtH, railZ, 0.9, 0.025, 0.01);
	}
	if(trims & Tiles)
	{
		double t = theme == Wood ? 0.2 : theme == Stone ? 0.15 : 0.3;
		Blocks(f, w, skirtH, 1.35, t, t, 0.01, 0.01, theme == Wood ? "wood-rustic" : theme == Stone ? "stone-tiles" : "plate-squares",
			theme == Stone ? t * 3 : t * 2);
		Band(f, w, 1.35, 1.39, 0.02);
	}

	double railH = theme == Wood ? 0.06 : theme == Stone ? 0.08 : 0.1;
	double railD = theme == Metal ? 0.05 : theme == Stone ? 0.04 : 0.03;
	if(trims & (ChairRail | TwoRails))
		Band(f, w, railZ, railZ + railH, railD, railD);
	if(trims & TwoRails)
		Band(f, w, 1.6, 1.6 + railH, railD, railD);
	if(trims & BumperRail)
		Band(f, w, 0.8, 0.9, theme == Wood ? 0.04 : 0.05, 0.05);

	if(trims & (Cornice | PlainCornice))
	{
		double h1 = theme == Stone ? 0.16 : theme == Metal ? 0.1 : 0.12, d1 = theme == Stone ? 0.04 : theme == Metal ? 0.05 : 0.03;
		Band(f, w, top - h1, top, d1, d1);
		if(trims & Cornice)
			Band(f, w, top - h1 / 2, top, qMin(TRIM_MAX, d1 * 2), d1 * 2);
	}

	if(trims & (Posts | HeavyPosts))
	{
		bool heavy = trims & HeavyPosts;
		double pw = (theme == Wood ? 0.18 : theme == Stone ? 0.3 : 0.14) * (heavy ? 1.6 : 1);
		double pd = theme == Stone ? 0.06 : theme == Metal ? 0.07 : 0.07;
		for(double s : PostSpots(w, 3.0, pw + (theme == Stone ? 0.08 : 0), 0, top))
		{
			if(theme == Metal)
			{
				RunBox(f, w, s - pw * 0.2, s + pw * 0.2, 0, top, 0, pd);
				RunBox(f, w, s - pw / 2, s + pw / 2, 0, top, pd - 0.015, pd);
			}
			else
				RunBox(f, w, s - pw / 2, s + pw / 2, 0, top, 0, pd);
			if(theme == Stone)
			{
				RunBox(f, w, s - pw / 2 - 0.04, s + pw / 2 + 0.04, 0, 0.16, 0, TRIM_MAX);
				RunBox(f, w, s - pw / 2 - 0.04, s + pw / 2 + 0.04, top - 0.16, top, 0, TRIM_MAX);
			}
			else if(theme == Wood)
			{
				RunBox(f, w, s - pw / 2 - 0.18, s - pw / 2, top - 0.1, top, 0, pd * 0.8);
				RunBox(f, w, s + pw / 2, s + pw / 2 + 0.18, top - 0.1, top, 0, pd * 0.8);
			}
		}
	}

	if(trims & Bars)
	{
		double pitch = theme == Stone ? 0.2 : 0.15, bw = theme == Wood ? 0.05 : 0.03;
		for(double s = pitch / 2; s < w.len; s += pitch)
			if(IsFree(w, s - bw, s + bw, 0, top))
				RunBox(f, w, s - bw / 2, s + bw / 2, skirtH, top - 0.05, 0.02, 0.06);
		Band(f, w, top - 0.12, top - 0.05, 0.07);
		Band(f, w, skirtH, skirtH + 0.07, 0.07);
	}

	if(trims & Pipes)
	{
		for(double z : {top - 0.55, top - 0.4})
		{
			Band(f, w, z, z + 0.07, 0.075);
			for(double s = 0.4; s < w.len; s += 1.2)
				if(IsFree(w, s - 0.03, s + 0.03, z - 0.04, z + 0.1))
					RunBox(f, w, s - 0.02, s + 0.02, z - 0.03, z + 0.1, 0, 0.078);
		}
	}

	if(!w.doors.isEmpty())
		Casings(f, w, theme);
}

void TrimOutsideWall(AuScFaceList& f, const WallRun& w, Theme theme, bool ground, const QList<QPair<double, double>>& eaves)
{
	if(theme == Plain || w.len < 0.05)
		return;
	const double top = w.top;
	if(theme == Wood)
	{
		double base = ground ? 0.35 : 0;
		if(ground == true)
			Band(f, w, 0, base, 0.06, 0.06);
		LogCourses(f, w, base, top - 0.02, 0.3, 0.06);
		const double plate = base + qFloor((top - 0.02 - base) / 0.3 + 1e-6) * 0.3;
		double from = 0, to = 0, zt = -1;
		auto flush = [&]() {
			if(zt - plate > 0.03)
				for(const QPair<double, double>& p : Free(w, from, to, plate, zt))
					RunBox(f, w, p.first, p.second, plate, zt, 0, 0.05);
		};
		for(const WallRun::Keep& k : w.ceilings)
		{
			double z = qMin(top, k.z0);
			if(qAbs(k.s0 - to) < 1e-6 && qAbs(z - zt) < 0.01)
			{
				to = k.s1;
				continue;
			}
			flush();
			from = k.s0;
			to = k.s1;
			zt = z;
		}
		flush();
	}
	else if(theme == Stone)
	{
		if(ground)
			Band(f, w, 0, 0.5, 0.1, 0.1);
		Blocks(f, w, ground ? 0.5 : 0, top, 0.4, 0.75, 0.02, 0.03, "stone-wall", 2.5);
	}
	else
	{
		if(ground)
			Band(f, w, 0, 0.3, 0.04, 0.04);
		for(double s = 0.5; s < w.len - 0.1; s += 1.0)
			if(IsFree(w, s - 0.06, s + 0.06, 0, top))
				RunBox(f, w, s - 0.04, s + 0.04, ground ? 0.3 : 0, qMin(top, Ceiling(w, s - 0.04, s + 0.04)), 0, 0.06);
	}
	for(const QPair<double, double>& e : eaves)
	{
		double h = theme == Stone ? 0.22 : theme == Wood ? 0.2 : 0.15, d = theme == Stone ? 0.14 : theme == Wood ? 0.1 : 0.06;
		RunBox(f, w, e.first, e.second, top - h, top, 0, d);
		if(theme == Stone)
			RunBox(f, w, e.first, e.second, top - h / 3, top, 0, d + 0.06);
	}
	if(!w.doors.isEmpty())
		Casings(f, w, theme, 1.2);
}

void TrimOutsideCorner(AuScFaceList& f, const V2& p, const V2& u1, const V2& u2, double top, bool ground, Theme theme)
{
	if(theme == Plain)
		return;
	V2 n1 = u1.Left() * -1, n2 = u2.Left() * -1;
	WallRun a, b;
	a.a = p - u1 * 1.0; a.u = u1; a.n = n1; a.len = 1.0; a.top = top;
	b.a = p; b.u = u2; b.n = n2; b.len = 1.0; b.top = top;
	double z0 = ground ? (theme == Stone ? 0.5 : theme == Wood ? 0.35 : 0.3) : 0;
	if(theme == Wood)
	{
		RunBox(f, a, 0.76, 1.0, z0, top, 0, 0.12);
		RunBox(f, b, 0, 0.24, z0, top, 0, 0.12);
	}
	else if(theme == Stone)
	{
		int row = 0;
		for(double z = z0; z + 0.15 <= top; z += 0.3, row++)
		{
			double zt = qMin(z + 0.28, top);
			double la = row % 2 ? 0.25 : 0.45, lb = row % 2 ? 0.45 : 0.25;
			RunBox(f, a, 1.0 - la, 1.0, z, zt, 0, 0.06);
			RunBox(f, b, 0, lb, z, zt, 0, 0.06);
		}
	}
	else
	{
		RunBox(f, a, 0.85, 1.0, z0, top, 0, 0.07);
		RunBox(f, b, 0, 0.15, z0, top, 0, 0.07);
	}
}

}
