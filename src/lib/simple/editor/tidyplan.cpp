#include "gridview.h"
#include "../util.h"

#include <QSet>

using namespace SimpleMode;

namespace {

bool SameDoor(const Door& a, const Door& b)
{
	return a.deck == b.deck && a.at == b.at && a.line == b.line;
}

bool SameWindow(const Window& a, const Window& b)
{
	return a.deck == b.deck && a.at == b.at && a.line == b.line;
}

bool WholeRoom(const Plan& plan, int deck, const QPoint& cell)
{
	QChar w = plan.At(deck, cell);
	return util::isWhole(w);
}

}

void TidyPlan(Plan& plan)
{
	for(auto it = plan.rooms.begin(); it != plan.rooms.end(); )
	{
		bool painted = false;
		for(const QStringList& grid : plan.decks)
			for(const QString& row : grid)
				painted = painted || row.contains(it.key());
		it = painted ? std::next(it) : plan.rooms.erase(it);
	}

	QList<Door> doors;
	for(const Door& d : plan.doors)
	{
		if(!SimpleMode::PlaceDoor(plan, d).ok)
			continue;
		bool duplicate = false;
		for(const Door& e : doors)
			duplicate = duplicate || SameDoor(e, d);
		if(!duplicate)
			doors << d;
	}
	plan.doors = doors;

	QList<Window> all = plan.windows, windows;
	for(const Window& w : all)
	{
		bool duplicate = false;
		for(const Window& e : windows)
			duplicate = duplicate || SameWindow(e, w);
		plan.windows = windows;
		QString problem;
		if(!duplicate && SimpleMode::PlaceWindow(plan, w).ok && WindowClear(plan, w, problem))
			windows << w;
	}
	plan.windows = windows;

	QList<Railing> railings;
	for(const Railing& r : plan.railings)
	{
		bool duplicate = false;
		for(const Railing& e : railings)
			duplicate = duplicate || (e.deck == r.deck && e.at == r.at && e.line == r.line);
		if(!duplicate && SimpleMode::PlaceRailing(plan, r).ok)
			railings << r;
	}
	plan.railings = railings;

	QList<RailingGap> gaps;
	for(const RailingGap& g : plan.gaps)
	{
		bool duplicate = false;
		for(const RailingGap& e : gaps)
			duplicate = duplicate || (e.deck == g.deck && e.at == g.at && e.line == g.line);
		if(!duplicate && SimpleMode::PlaceGap(plan, g).ok)
			gaps << g;
	}
	plan.gaps = gaps;

	QList<Stair> stairs;
	for(const Stair& st : plan.stairs)
	{
		bool duplicate = false;
		for(const Stair& e : stairs)
			duplicate = duplicate || (e.deck == st.deck && e.cell == st.cell);
		if(!duplicate && st.deck >= 0 && st.deck < plan.Decks())
			stairs << st;
	}
	plan.stairs = stairs;

	QList<Hatch> hatches;
	for(int i = 0; i < plan.hatches.size(); i++)
	{
		const Hatch& h = plan.hatches[i];
		if(h.roof)
		{
			if(RoofHatchProblem(plan, h, i).isEmpty())
				hatches << h;
			continue;
		}
		if(h.deck < 1 || !WholeRoom(plan, h.deck, h.cell) || !WholeRoom(plan, h.deck - 1, h.cell) || IsOpenCell(plan, h.deck, h.cell)
				|| IsLiftRoom(plan, plan.At(h.deck, h.cell)) || IsLiftRoom(plan, plan.At(h.deck - 1, h.cell)))
			continue;
		bool duplicate = false;
		for(const Hatch& e : hatches)
			duplicate = duplicate || (!e.roof && e.deck == h.deck && e.cell == h.cell);
		if(!duplicate)
			hatches << h;
	}
	plan.hatches = hatches;

	QList<Post> posts;
	for(const Post& p : plan.posts)
	{
		if(!WholeRoom(plan, p.deck, p.cell) || IsOpenCell(plan, p.deck, p.cell))
			continue;
		bool duplicate = false;
		for(const Post& e : posts)
			duplicate = duplicate || (e.deck == p.deck && e.cell == p.cell);
		if(!duplicate)
			posts << p;
	}
	plan.posts = posts;

	QList<Station> stations;
	for(const Station& st : plan.stations)
	{
		if(!WholeRoom(plan, st.deck, st.cell) || IsOpenCell(plan, st.deck, st.cell))
			continue;
		bool duplicate = false;
		for(const Station& e : stations)
			duplicate = duplicate || (e.deck == st.deck && e.cell == st.cell);
		if(!duplicate)
			stations << st;
	}
	plan.stations = stations;

	QList<Fixture> fixtures;
	for(const Fixture& f : plan.fixtures)
	{
		Plan tried = plan;
		tried.fixtures = fixtures;
		if(FixtureProblem(tried, f).isEmpty())
			fixtures << f;
	}
	plan.fixtures = fixtures;

	while(plan.roofGrids.size() > plan.Decks())
		plan.roofGrids.removeLast();
	QSet<QChar> used;
	for(int d = 0; d < plan.roofGrids.size(); d++)
	{
		QStringList grid;
		for(int r = 0; r < plan.Rows(); r++)
		{
			QString row;
			for(int c = 0; c < plan.Cols(); c++)
			{
				QChar ch = plan.RoofAt(d, QPoint(c, r));
				if(ch != '.' && (!plan.roofs.contains(ch) || !CanRoof(plan, d, QPoint(c, r))))
					ch = '.';
				if(ch != '.')
					used.insert(ch);
				row += ch;
			}
			grid << row;
		}
		plan.roofGrids[d] = grid;
	}
	for(auto it = plan.roofs.begin(); it != plan.roofs.end(); )
		it = used.contains(it.key()) ? std::next(it) : plan.roofs.erase(it);

	QList<Dormer> dormers;
	for(const Dormer& d : plan.dormers)
	{
		if(d.deck < 0 || d.deck >= plan.Decks() || !plan.OnGrid(d.cell) || plan.RoofAt(d.deck, d.cell) == '.')
			continue;
		bool duplicate = false;
		for(const Dormer& e : dormers)
			duplicate = duplicate || (e.deck == d.deck && e.cell == d.cell);
		if(!duplicate)
			dormers << d;
	}
	plan.dormers = dormers;
	QList<Skylight> skylights;
	for(const Skylight& s : plan.skylights)
	{
		if(!WholeRoom(plan, s.deck, s.cell))
			continue;
		bool duplicate = false;
		for(const Skylight& e : skylights)
			duplicate = duplicate || (e.deck == s.deck && e.cell == s.cell);
		if(!duplicate)
			skylights << s;
	}
	plan.skylights = skylights;
}

QChar NewRoof(Plan& plan, const QString& style, int dir)
{
	static const QString letters = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for(QChar ch : letters)
		if(!plan.roofs.contains(ch))
		{
			Roof roof;
			roof.letter = ch;
			roof.style = style;
			roof.dir = dir;
			plan.roofs.insert(ch, roof);
			return ch;
		}
	return QChar();
}

int RoofTheBuilding(Plan& plan)
{
	int count = 0;
	for(int d = 0; d < plan.Decks(); d++)
	{
		QSet<int> seen;
		auto key = [&](const QPoint& p) { return p.y() * plan.Cols() + p.x(); };
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint start(c, r);
				if(seen.contains(key(start)) || !CanRoof(plan, d, start) || plan.RoofAt(d, start) != '.')
					continue;
				QList<QPoint> piece{start};
				seen.insert(key(start));
				for(int i = 0; i < piece.size(); i++)
					for(int s = 0; s < 4; s++)
					{
						QPoint n = Neighbour(piece[i], Side(s));
						if(plan.OnGrid(n) && !seen.contains(key(n)) && CanRoof(plan, d, n) && plan.RoofAt(d, n) == '.')
						{
							seen.insert(key(n));
							piece << n;
						}
					}
				int c0 = start.x(), c1 = c0, r0 = start.y(), r1 = r0;
				for(const QPoint& p : piece)
				{
					c0 = qMin(c0, p.x());
					c1 = qMax(c1, p.x());
					r0 = qMin(r0, p.y());
					r1 = qMax(r1, p.y());
				}
				QChar letter = NewRoof(plan, DefaultRoofStyle(plan.theme), c1 - c0 >= r1 - r0 ? East : North);
				if(letter.isNull())
					return count;
				for(const QPoint& p : piece)
					plan.SetRoof(d, p, letter);
				count += piece.size();
			}
	}
	return count;
}
