#include "placement.h"
#include "../build/internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode {

DoorPlace PlaceOpening(const Plan& plan, int deck, const QPoint& at, Line line, double width, const QString& what)
{
	DoorPlace dp;
	if(deck < 0 || deck >= plan.Decks())
	{
		dp.problem = "There is no such deck.";
		return dp;
	}
	int x = at.x(), y = at.y();
	bool onLine = false;
	V2 u;
	switch(line)
	{
	case Horizontal: onLine = y % 4 == 0 && x % 2 == 0; u = V2(1, 0); break;
	case Vertical: onLine = x % 4 == 0 && y % 2 == 0; u = V2(0, 1); break;
	case Slash: onLine = (x + y) % 4 == 0; u = V2(1, 1).Unit(); break;
	case Backslash: onLine = ((y - x) % 4 + 4) % 4 == 0; u = V2(1, -1).Unit(); break;
	}
	if(!onLine)
	{
		dp.problem = QString("A %1 goes on a grid line.").arg(what);
		return dp;
	}
	V2 m(plan.QX(x), plan.QY(y));
	V2 n = u.Left();
	auto letterAt = [&](const V2& p) {
		QPoint cell;
		int q;
		return QuarterAtPoint(plan, p.x, p.y, cell, q) ? plan.Quarter(deck, cell, q) : QChar('.');
	};
	QChar left, right;
	const int samples = qMax(4, qRound(width * 55 / 6));
	for(int i = -samples; i <= samples; i++)
	{
		double s = i * (width / 2 - 0.07) / samples + 0.013;
		QChar l = letterAt(m + u * s + n * 0.02), r = letterAt(m + u * s - n * 0.02);
		if(i == -samples)
		{
			left = l;
			right = r;
		}
		else if(l != left || r != right)
		{
			dp.problem = QString("The whole %1 must be in one wall, between the same two spaces.").arg(what);
			return dp;
		}
	}
	if(left == right)
	{
		dp.problem = left == '.' ? QString("A %1 goes in the wall of a room.").arg(what)
			: QString("There is no wall here: both sides are the same room.");
		return dp;
	}
	if(IsOpenEdge(plan, left, right) || IsOpenEdge(plan, right, left))
	{
		dp.here = left != '.' ? left : right;
		dp.problem = QString("%1 is open to the air on this side: there is no wall for a %2 (it has its own railing).")
			.arg(plan.rooms.value(dp.here).name).arg(what);
		return dp;
	}
	dp.ok = true;
	dp.mx = m.x;
	dp.my = m.y;
	dp.ux = u.x;
	dp.uy = u.y;
	bool leftHere = left != '.' && !(right != '.' && IsOpenAirRoom(plan, left) && !IsOpenAirRoom(plan, right));
	if(leftHere)
	{
		dp.here = left;
		dp.there = right;
		dp.nx = -n.x;
		dp.ny = -n.y;
	}
	else
	{
		dp.here = right;
		dp.there = left;
		dp.nx = n.x;
		dp.ny = n.y;
	}
	return dp;
}

DoorPlace PlaceDoor(const Plan& plan, const Door& door)
{
	if(door.opening && door.style == "full")
	{
		int x = door.at.x(), y = door.at.y();
		bool middle = door.line == Horizontal ? x % 4 == 2 && y % 4 == 0
			: door.line == Vertical ? y % 4 == 2 && x % 4 == 0
			: x % 2 != 0 && y % 2 != 0;
		if(!middle)
		{
			DoorPlace dp;
			dp.problem = "A full opening takes the whole side of a cell, or half a diagonal: put it in the middle of one.";
			return dp;
		}
	}
	DoorPlace dp = PlaceOpening(plan, door.deck, door.at, door.line, DoorWidth(plan, door), "doorway");
	const QChar lift = dp.ok ? LiftOf(plan, dp) : QChar();
	if(lift.isNull())
		return dp;
	dp.ok = false;
	if(IsLiftRoom(plan, dp.here) && IsLiftRoom(plan, dp.there))
		dp.problem = "This wall is between two turbo lifts: a lift's door leads into a room, or outside.";
	else if(door.opening)
		dp.problem = "A doorway into a turbo lift needs a door: the car is not always there.";
	else
	{
		for(const Door& o : plan.doors)
		{
			if(o.deck != door.deck || o.opening)
				continue;
			if(o.at == door.at && o.line == door.line)
				break;
			DoorPlace op = PlaceOpening(plan, o.deck, o.at, o.line, DoorWidth(plan, o), "doorway");
			if(op.ok && LiftOf(plan, op) == lift)
			{
				dp.problem = QString("%1 has a door on deck %2 already: a lift has one door a deck, its stop there.")
					.arg(plan.rooms.value(lift).name).arg(door.deck);
				return dp;
			}
		}
		dp.ok = true;
	}
	return dp;
}

DoorPlace PlaceWindow(const Plan& plan, const Window& window)
{
	DoorPlace dp = PlaceOpening(plan, window.deck, window.at, window.line, WindowWidth(plan, window), "window");
	if(dp.ok && !LiftOf(plan, dp).isNull())
	{
		dp.ok = false;
		dp.problem = "A turbo lift's shaft has no windows.";
	}
	return dp;
}

DoorPlace PlaceRailing(const Plan& plan, const Railing& railing)
{
	int x = railing.at.x(), y = railing.at.y();
	bool middle = railing.line == Horizontal ? x % 4 == 2 && y % 4 == 0
		: railing.line == Vertical ? y % 4 == 2 && x % 4 == 0
		: x % 2 != 0 && y % 2 != 0;
	DoorPlace dp;
	if(!middle)
	{
		dp.problem = "A railing takes the place of the wall along one side of a cell, or half a diagonal.";
		return dp;
	}
	dp = PlaceOpening(plan, railing.deck, railing.at, railing.line, RailingLength(plan, railing), "railing");
	if(dp.ok && !LiftOf(plan, dp).isNull())
	{
		dp.ok = false;
		dp.problem = "A turbo lift's shaft is walled all round: no railing in its walls.";
		return dp;
	}
	if(dp.ok && IsOpenAirRoom(plan, dp.here) != IsOpenAirRoom(plan, dp.there))
	{
		dp.ok = false;
		dp.problem = "This is the building's outside wall, facing a balcony, platform or catwalk: a railing goes along its "
			"open sides (they have one already), not in a wall.";
		return dp;
	}
	if(dp.ok && dp.there == '.')
	{
		const double out = 0.2;
		dp.mx += dp.nx * out;
		dp.my += dp.ny * out;
		dp.here = dp.there = '.';
		return dp;
	}
	if(dp.ok || !dp.here.isNull() || railing.deck < 0 || railing.deck >= plan.Decks())
		return dp;
	V2 u = railing.line == Horizontal ? V2(1, 0) : railing.line == Vertical ? V2(0, 1) : railing.line == Slash ? V2(1, 1).Unit()
		: V2(1, -1).Unit();
	V2 m(plan.QX(x), plan.QY(y)), n = u.Left(), probe = m + u * 0.013 + n * 0.02;
	QPoint cell;
	int q;
	QChar side = QuarterAtPoint(plan, probe.x, probe.y, cell, q) ? plan.Quarter(railing.deck, cell, q) : QChar('.');
	dp = DoorPlace();
	dp.ok = true;
	dp.here = dp.there = side;
	dp.mx = m.x;
	dp.my = m.y;
	dp.ux = u.x;
	dp.uy = u.y;
	dp.nx = -n.x;
	dp.ny = -n.y;
	return dp;
}

DoorPlace PlaceGap(const Plan& plan, const RailingGap& gap)
{
	DoorPlace dp;
	if(gap.deck < 0 || gap.deck >= plan.Decks())
	{
		dp.problem = "There is no such deck.";
		return dp;
	}
	int x = gap.at.x(), y = gap.at.y();
	bool onLine = false;
	V2 u;
	switch(gap.line)
	{
	case Horizontal: onLine = y % 4 == 0 && x % 2 == 0; u = V2(1, 0); break;
	case Vertical: onLine = x % 4 == 0 && y % 2 == 0; u = V2(0, 1); break;
	case Slash: onLine = (x + y) % 4 == 0; u = V2(1, 1).Unit(); break;
	case Backslash: onLine = ((y - x) % 4 + 4) % 4 == 0; u = V2(1, -1).Unit(); break;
	}
	if(!onLine)
	{
		dp.problem = "A gap goes on a grid line.";
		return dp;
	}
	if(gap.full)
	{
		bool middle = gap.line == Horizontal ? x % 4 == 2 && y % 4 == 0 : gap.line == Vertical ? y % 4 == 2 && x % 4 == 0
			: x % 2 != 0 && y % 2 != 0;
		if(!middle)
		{
			dp.problem = "A full gap takes the railing along the whole side of a cell, or half a diagonal: put it in the middle of one.";
			return dp;
		}
	}
	V2 m(plan.QX(x), plan.QY(y));
	V2 n = u.Left();
	const double width = GapWidth(plan, gap);
	auto letterAt = [&](const V2& p) {
		QPoint cell;
		int q;
		return QuarterAtPoint(plan, p.x, p.y, cell, q) ? plan.Quarter(gap.deck, cell, q) : QChar('.');
	};
	QChar left, right;
	const int samples = qMax(4, qRound(width * 55 / 6));
	for(int i = -samples; i <= samples; i++)
	{
		double s = i * (width / 2 - 0.07) / samples + 0.013;
		QChar l = letterAt(m + u * s + n * 0.02), r = letterAt(m + u * s - n * 0.02);
		if(i == -samples)
		{
			left = l;
			right = r;
		}
		else if(l != left || r != right)
		{
			dp.problem = "The whole gap must be in one railing: along one open side of one balcony, platform or catwalk.";
			return dp;
		}
	}
	if(IsOpenEdge(plan, left, right))
	{
		dp.here = left;
		dp.there = right;
		n = n * -1;
	}
	else if(IsOpenEdge(plan, right, left))
	{
		dp.here = right;
		dp.there = left;
	}
	else
	{
		dp.problem = "A gap goes in the railing along a balcony's, platform's or catwalk's side open to the air.";
		return dp;
	}
	if(!HasOpenAirRailing(plan, gap.deck, dp.here))
	{
		dp.problem = "A platform on the ground deck has no railing to leave a gap in.";
		return dp;
	}
	dp.ok = true;
	dp.mx = m.x;
	dp.my = m.y;
	dp.ux = u.x;
	dp.uy = u.y;
	dp.nx = n.x;
	dp.ny = n.y;
	return dp;
}

double GapWidth(const Plan& plan, const RailingGap& gap)
{
	if(gap.full)
		return gap.line == Horizontal || gap.line == Vertical ? plan.cell : plan.cell * M_SQRT1_2;
	return DOOR_WIDTH;
}

bool WindowClear(const Plan& plan, const Window& window, QString& problem, int ignoreWindow)
{
	DoorPlace place = PlaceWindow(plan, window);
	if(!place.ok)
	{
		problem = place.problem;
		return false;
	}
	const double half = WindowWidth(plan, window) / 2;
	auto along = [&](const DoorPlace& other, double& distance) {
		double dx = other.mx - place.mx, dy = other.my - place.my;
		distance = qSqrt(dx * dx + dy * dy);
		return other.ok && qAbs(dx * place.uy - dy * place.ux) < 1e-6;
	};
	for(const Door& d : plan.doors)
	{
		double distance;
		if(d.deck == window.deck && d.line == window.line && along(PlaceDoor(plan, d), distance)
				&& distance < half + DoorWidth(plan, d) / 2 + SWITCH_ASIDE + 0.15)
		{
			problem = "Too close to a door: windows keep clear of doorways and their switches.";
			return false;
		}
	}
	for(int i = 0; i < plan.windows.size(); i++)
	{
		const Window& w = plan.windows[i];
		double distance;
		if(i != ignoreWindow && w.deck == window.deck && w.line == window.line && w.at != window.at
				&& along(PlaceWindow(plan, w), distance) && distance < half + WindowWidth(plan, w) / 2 + 0.2)
		{
			problem = "Too close to another window.";
			return false;
		}
	}
	return true;
}

bool RoomShapeOk(const Plan& plan, QChar room, QString& problem)
{
	if(IsLiftRoom(plan, room))
	{
		Plan one = plan;
		for(int d : LiftDecks(plan, room))
		{
			for(int e = 0; e < one.Decks(); e++)
				for(int r = 0; r < one.Rows(); r++)
					for(int c = 0; c < one.Cols(); c++)
						for(int q = 0; q < 4; q++)
							if(e != d && one.Quarter(e, QPoint(c, r), q) == room)
								one.SetQuarter(e, QPoint(c, r), q, '.');
			one.rooms[room].type = "hall";
			if(!RoomShapeOk(one, room, problem))
				return false;
			one = plan;
		}
		return true;
	}
	int deck = -1, total = 0;
	QPoint startCell;
	int startQ = -1;
	for(int d = 0; d < plan.Decks(); d++)
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
				for(int q = 0; q < 4; q++)
				{
					if(plan.Quarter(d, QPoint(c, r), q) != room)
						continue;
					if(deck < 0)
					{
						deck = d;
						startCell = QPoint(c, r);
						startQ = q;
					}
					else if(d != deck)
					{
						problem = QString("Room %1 is on more than one deck; a room is on one deck only.").arg(room);
						return false;
					}
					total++;
				}
	if(!total)
		return true;
	QSet<int> seen;
	QList<QPair<QPoint, int>> todo{qMakePair(startCell, startQ)};
	auto key = [&](const QPoint& p, int q) { return (p.y() * plan.Cols() + p.x()) * 4 + q; };
	seen.insert(key(startCell, startQ));
	while(!todo.isEmpty())
	{
		QPair<QPoint, int> cur = todo.takeLast();
		for(int k = 0; k < 3; k++)
		{
			QPoint oc;
			int oq;
			QuarterAcross(cur.first, cur.second, k, oc, oq);
			if(plan.Quarter(deck, oc, oq) != room || seen.contains(key(oc, oq)))
				continue;
			seen.insert(key(oc, oq));
			todo << qMakePair(oc, oq);
		}
	}
	if(seen.size() != total)
	{
		problem = QString("Room %1 is in more than one piece; a room is one connected piece.").arg(room);
		return false;
	}
	return true;
}

bool IsLiftRoom(const Plan& plan, QChar room)
{
	return plan.rooms.contains(room) && plan.rooms[room].type == "turbolift";
}

QList<int> LiftDecks(const Plan& plan, QChar room)
{
	QList<int> decks;
	for(int d = 0; d < plan.Decks(); d++)
	{
		bool here = false;
		for(int r = 0; r < plan.Rows() && !here; r++)
			for(int c = 0; c < plan.Cols() && !here; c++)
				for(int q = 0; q < 4 && !here; q++)
					here = plan.Quarter(d, QPoint(c, r), q) == room;
		if(here)
			decks << d;
	}
	return decks;
}

QString LiftProblem(const Plan& plan, QChar room)
{
	const QString name = plan.rooms.value(room).name;
	QList<int> decks = LiftDecks(plan, room);
	QRect first;
	for(int i = 0; i < decks.size(); i++)
	{
		const int d = decks[i];
		if(i && d != decks[i - 1] + 1)
			return QString("%1 skips deck %2: a lift's shaft runs through every deck between its top and bottom.").arg(name)
				.arg(decks[i - 1] + 1);
		QRect box;
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
				for(int q = 0; q < 4; q++)
					if(plan.Quarter(d, QPoint(c, r), q) == room)
					{
						if(plan.At(d, QPoint(c, r)) != room)
							return QString("%1 has part of a cell on deck %2: a lift's shaft is whole cells.").arg(name).arg(d);
						box |= QRect(c, r, 1, 1);
					}
		for(int r = box.top(); r <= box.bottom(); r++)
			for(int c = box.left(); c <= box.right(); c++)
				if(plan.At(d, QPoint(c, r)) != room)
					return QString("%1 on deck %2 is not a rectangle: a lift's car is one.").arg(name).arg(d);
		if(i && box != first)
			return QString("%1 on deck %2 is not over its cells on deck %3: a lift's shaft goes straight up.").arg(name).arg(d)
				.arg(decks[i - 1]);
		first = box;
		for(int r = box.top(); r <= box.bottom(); r++)
			for(int c = box.left(); c <= box.right(); c++)
			{
				QChar above = d + 1 < plan.Decks() ? plan.At(d + 1, QPoint(c, r)) : QChar('.');
				if(above != room && plan.rooms.contains(above) && plan.rooms[above].type == "opentobelow")
					return QString("%1 has Open to Deck Below over it: a lift's shaft ends in a ceiling.").arg(name);
			}
	}
	return QString();
}

QString LiftStopName(const Plan& plan, QChar room, int deck)
{
	int lifts = 0;
	for(const Room& r : plan.rooms)
		if(r.type == "turbolift" && !LiftDecks(plan, r.letter).isEmpty())
			lifts++;
	return lifts > 1 ? QString("%1 deck %2").arg(plan.rooms.value(room).name).arg(deck) : QString("Deck %1").arg(deck);
}

QChar LiftOf(const Plan& plan, const DoorPlace& place)
{
	if(IsLiftRoom(plan, place.here))
		return place.here;
	if(IsLiftRoom(plan, place.there))
		return place.there;
	return QChar();
}

double DoorWidth(const Plan& plan, const Door& door)
{
	if(door.opening && door.style == "full")
		return door.line == Horizontal || door.line == Vertical ? plan.cell : plan.cell * M_SQRT1_2;
	return door.wide ? 2 * DOOR_WIDTH : DOOR_WIDTH;
}

double WindowWidth(const Plan& plan, const Window& window)
{
	const WindowShape* shape = FindWindowShape(window.shape.isEmpty() ? "standard" : window.shape);
	double most = plan.cell - 0.4;
	return shape && shape->width > 0 ? qMin(shape->width, most) : most;
}

void OpeningHeights(const Plan& plan, const Door& opening, double& spring, double& top)
{
	const double most = plan.CeilingHt() - 0.15;
	spring = qMin(DOOR_HEIGHT, most);
	top = spring;
	if(opening.style == "tall")
		spring = top = qMax(spring, plan.CeilingHt() - 0.3);
	else if(opening.style == "full")
		spring = top = plan.CeilingHt();
	else if(opening.style == "arch")
		top = qMin(spring + DoorWidth(plan, opening) / 2, most);
}

double RailingLength(const Plan& plan, const Railing& railing)
{
	return railing.line == Horizontal || railing.line == Vertical ? plan.cell : plan.cell * M_SQRT1_2;
}

QString SkylightProblem(const Plan& plan, const Skylight& skylight, int ignore)
{
	if(skylight.deck < 0 || skylight.deck >= plan.Decks() || !plan.OnGrid(skylight.cell))
		return "A skylight must be in a cell of the grid.";
	QChar room = plan.At(skylight.deck, skylight.cell);
	if(room == '.' || room == '*' || !plan.rooms.contains(room))
		return "A skylight needs a whole cell of one room.";
	if(IsOpenAirRoom(plan, room))
		return "A balcony, platform or catwalk is open to the sky already.";
	for(int q = 0; q < 4; q++)
		if(plan.Quarter(skylight.deck + 1, skylight.cell, q) != '.')
			return "Something is built over this cell on the deck above: a skylight needs open sky.";
	QChar l = plan.RoofAt(skylight.deck, skylight.cell);
	QString roof = "flat";
	if(l != '.' && plan.roofs.contains(l))
		roof = FindRoofStyle(plan.roofs[l].style) ? plan.roofs[l].style : DefaultRoofStyle(plan.theme);
	if(roof == "dome" || roof == "spire")
		return "A skylight cannot go under a dome or a spire.";
	if(skylight.style == "lantern" && roof != "flat")
		return "A lantern stands on a flat top: under a sloping roof, use a pane.";
	for(int i = 0; i < plan.skylights.size(); i++)
		if(i != ignore && (ignore < 0 || i < ignore) && plan.skylights[i].deck == skylight.deck
				&& plan.skylights[i].cell == skylight.cell)
			return "There is a skylight in this cell already.";
	return QString();
}

QString RoofHatchProblem(const Plan& plan, const Hatch& hatch, int ignore)
{
	if(hatch.deck < 0 || hatch.deck >= plan.Decks() || !plan.OnGrid(hatch.cell))
		return "A hatch must be in a cell of the grid.";
	QChar room = plan.At(hatch.deck, hatch.cell);
	if(room == '.' || room == '*' || !plan.rooms.contains(room))
		return "A hull hatch needs a whole cell of one room.";
	if(IsOpenAirRoom(plan, room))
		return "A balcony, platform or catwalk is open to the sky already.";
	if(IsLiftRoom(plan, room))
		return "A turbo lift's shaft has its car in it: no hatch.";
	if(IsOpenCell(plan, hatch.deck, hatch.cell))
		return "This cell has no floor to stand the ladder on.";
	for(int q = 0; q < 4; q++)
		if(plan.Quarter(hatch.deck + 1, hatch.cell, q) != '.')
			return "Something is built over this cell on the deck above: a hull hatch opens onto the flat top of the hull.";
	if(plan.RoofAt(hatch.deck, hatch.cell) != '.')
		return "There is a roof over this cell: a hull hatch opens onto the flat top of the hull.";
	for(const Skylight& s : plan.skylights)
		if(s.deck == hatch.deck && s.cell == hatch.cell)
			return "There is a skylight in this cell.";
	for(int i = 0; i < plan.hatches.size(); i++)
		if(i != ignore && (ignore < 0 || i < ignore) && plan.hatches[i].roof && plan.hatches[i].deck == hatch.deck
				&& plan.hatches[i].cell == hatch.cell)
			return "There is a hull hatch in this cell already.";
	return QString();
}

QString DormerProblem(const Plan& plan, const Dormer& dormer, int ignore)
{
	if(dormer.deck < 0 || dormer.deck >= plan.Decks() || !plan.OnGrid(dormer.cell))
		return "A dormer must be in a cell of the grid.";
	QChar l = plan.RoofAt(dormer.deck, dormer.cell);
	if(l == '.' || !plan.roofs.contains(l) || !CanRoof(plan, dormer.deck, dormer.cell))
		return "A dormer stands on a roof: roof this cell first.";
	for(int i = 0; i < plan.dormers.size(); i++)
		if(i != ignore && (ignore < 0 || i < ignore) && plan.dormers[i].deck == dormer.deck && plan.dormers[i].cell == dormer.cell)
			return "There is a dormer in this cell already.";
	if(SkylightAt(plan, dormer.deck, dormer.cell))
		return "There is a skylight in this cell.";
	RoofRegion region(plan, dormer.deck, plan.roofs[l], ThemeOf(plan.theme), plan.height);
	RoofRegion::DormerFrame frame;
	QString problem;
	if(!region.FrameDormer(dormer, frame, problem))
		return problem;
	return QString();
}

static StairLayout StairShape(const Plan& plan, const Stair& stair)
{
	StairLayout L;
	const double H = plan.height;
	auto step = [](const QPoint& c, int side) { return Neighbour(c, Side(((side % 4) + 4) % 4)); };
	const int f = ((stair.dir % 4) + 4) % 4, back = (f + 2) % 4;
	const bool left = stair.kind == StairTurnLeft || stair.kind == StairUTurnLeft;
	const int side = left ? (f + 3) % 4 : (f + 1) % 4;
	const QPoint c0 = stair.cell, c1 = step(c0, f);
	L.entry = step(c0, back);
	if(stair.kind == StairTurnLeft || stair.kind == StairTurnRight)
	{
		QPoint c2 = step(c1, side);
		L.cells = {c0, c1, c2};
		L.flights << StairLayout::Flight{c0, c0, f, 0, H / 2} << StairLayout::Flight{c2, c2, side, H / 2, H};
		L.landings << c1;
		L.top = c2;
		L.arrivalSide = side;
	}
	else if(stair.kind == StairUTurnLeft || stair.kind == StairUTurnRight)
	{
		QPoint c2 = step(c0, side), c3 = step(c1, side);
		L.cells = {c0, c1, c3, c2};
		L.flights << StairLayout::Flight{c0, c0, f, 0, H / 2} << StairLayout::Flight{c2, c2, back, H / 2, H};
		L.landings << c1 << c3;
		L.top = c2;
		L.arrivalSide = back;
	}
	else
	{
		L.cells = {c0, c1};
		L.flights << StairLayout::Flight{c0, c1, f, 0, H};
		L.top = c1;
		L.arrivalSide = f;
	}
	L.arrival = step(L.top, L.arrivalSide);
	return L;
}

static bool IsOpenRoomQuarter(const Plan& plan, int deck, const QPoint& cell, int q)
{
	if(deck < 1 || deck >= plan.Decks() || !plan.OnGrid(cell))
		return false;
	QChar w = plan.Quarter(deck, cell, q);
	if(w == '.' || plan.Quarter(deck - 1, cell, q) == '.' || !plan.rooms.contains(w))
		return false;
	return plan.rooms[w].type == "opentobelow";
}

static bool IsWholeRoomCell(const Plan& plan, int deck, const QPoint& cell)
{
	QChar w = plan.At(deck, cell);
	return w != '.' && w != '*';
}

static bool IsOpenRoomCell(const Plan& plan, int deck, const QPoint& cell)
{
	for(int q = 0; q < 4; q++)
		if(!IsOpenRoomQuarter(plan, deck, cell, q))
			return false;
	return true;
}

StairLayout LayoutStair(const Plan& plan, const Stair& stair)
{
	StairLayout L = StairShape(plan, stair);
	const int d = stair.deck;
	auto fail = [&](const QString& problem) {
		L.ok = false;
		L.problem = problem;
		return L;
	};
	if(d < 0 || d >= plan.Decks())
		return fail("There is no such deck.");
	for(const QPoint& c : L.cells)
		if(!plan.OnGrid(c))
			return fail("The stair runs off the grid.");
		else if(IsLiftRoom(plan, plan.At(d, c)) || (d + 1 < plan.Decks() && IsLiftRoom(plan, plan.At(d + 1, c))))
			return fail("The stair runs into a turbo lift's shaft.");
	L.ok = true;
	return L;
}

bool IsOpenCell(const Plan& plan, int deck, const QPoint& cell)
{
	if(IsOpenRoomCell(plan, deck, cell))
		return true;
	if(!IsWholeRoomCell(plan, deck, cell))
		return false;
	for(const Stair& s : plan.stairs)
		if(s.deck == deck - 1 && StairShape(plan, s).cells.contains(cell) && LayoutStair(plan, s).ok)
			return true;
	return false;
}

QChar SpaceRoom(const Plan& plan, int deck, const QPoint& cell)
{
	while(IsOpenCell(plan, deck, cell))
		deck--;
	return plan.At(deck, cell);
}

bool IsOpenQuarter(const Plan& plan, int deck, const QPoint& cell, int quarter)
{
	if(IsOpenRoomQuarter(plan, deck, cell, quarter))
		return true;
	if(!IsWholeRoomCell(plan, deck, cell))
		return false;
	for(const Stair& s : plan.stairs)
		if(s.deck == deck - 1 && StairShape(plan, s).cells.contains(cell) && LayoutStair(plan, s).ok)
			return true;
	return false;
}

QChar SpaceRoom(const Plan& plan, int deck, const QPoint& cell, int quarter)
{
	while(IsOpenQuarter(plan, deck, cell, quarter))
		deck--;
	return plan.Quarter(deck, cell, quarter);
}

bool IsOpenAirRoom(const Plan& plan, QChar room)
{
	if(!plan.rooms.contains(room))
		return false;
	const QString& t = plan.rooms[room].type;
	return t == "balcony" || t == "platform" || t == "catwalk";
}

bool IsOpenAirQuarter(const Plan& plan, int deck, const QPoint& cell, int quarter)
{
	return IsOpenAirRoom(plan, plan.Quarter(deck, cell, quarter));
}

bool IsOpenEdge(const Plan& plan, QChar room, QChar other)
{
	return other == '.' && IsOpenAirRoom(plan, room);
}

double OpenEdgeInset(const Plan& plan, QChar room)
{
	if(plan.rooms.value(room).type == "catwalk")
		return qMax(plan.wall, (plan.cell - 1.2) / 2);
	return 0.1;
}

bool HasOpenAirRailing(const Plan& plan, int deck, QChar room)
{
	return IsOpenAirRoom(plan, room) && !(deck == 0 && plan.rooms[room].type == "platform");
}

void ParkingSize(const QString& kind, double& across, double& along)
{
	across = kind == "water" ? 12 : kind == "rocket" ? 10 : kind == "space" ? 12 : kind == "table" ? 4 : 6;
	along = kind == "water" ? 24 : kind == "rocket" ? 10 : kind == "space" ? 17 : kind == "table" ? 4 : 12;
}

bool ParkingCentre(const Plan& plan, const Fixture& fx, double& x, double& y)
{
	const FixtureType* type = FindFixtureType(fx.type);
	const QString kind = fx.kind.isEmpty() && type ? type->kinds.first() : fx.kind;
	double across, along;
	ParkingSize(kind, across, along);
	static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
	const V2 f = dirs[((fx.dir % 4) + 4) % 4], s(f.y, -f.x);
	const QChar w = plan.At(fx.deck, fx.cell);
	const V2 mid((plan.X0(fx.cell.x()) + plan.X1(fx.cell.x())) / 2, (plan.Y0(fx.cell.y()) + plan.Y1(fx.cell.y())) / 2);
	const double h = plan.cell / 2;
	for(const V2& off : {V2(0, 0), V2(h, 0), V2(-h, 0), V2(0, h), V2(0, -h), V2(h, h), V2(h, -h), V2(-h, h), V2(-h, -h)})
	{
		const V2 c = mid + off;
		bool fits = true;
		for(double a = -along / 2 + 0.1; fits && a <= along / 2 - 0.1 + 1e-6; a += 0.5)
			for(double b = -across / 2 + 0.1; fits && b <= across / 2 - 0.1 + 1e-6; b += 0.5)
			{
				V2 p = c + f * a + s * b;
				QPoint cell;
				int q;
				fits = QuarterAtPoint(plan, p.x, p.y, cell, q) && plan.Quarter(fx.deck, cell, q) == w && !IsOpenQuarter(plan, fx.deck, cell, q);
			}
		if(fits)
		{
			x = c.x;
			y = c.y;
			return true;
		}
	}
	return false;
}

QList<int> GunnerLinks(const Plan& plan)
{
	QList<int> turrets;
	for(int i = 0; i < plan.fixtures.size(); i++)
		if(plan.fixtures[i].type == "turret")
			turrets << (FixtureProblem(plan, plan.fixtures[i], i).isEmpty() ? i : -1);
	QList<int> links;
	QSet<int> taken{-1};
	for(const Station& st : plan.stations)
	{
		int link = -1;
		if(st.kind == "gunner" && st.turret > 0 && st.turret <= turrets.size() && !taken.contains(turrets[st.turret - 1]))
			link = turrets[st.turret - 1];
		if(link >= 0)
			taken.insert(link);
		links << link;
	}
	for(int i = 0; i < plan.stations.size(); i++)
	{
		if(plan.stations[i].kind != "gunner" || plan.stations[i].turret > 0)
			continue;
		for(int t : turrets)
			if(!taken.contains(t))
			{
				links[i] = t;
				taken.insert(t);
				break;
			}
	}
	return links;
}

int TurretNumber(const Plan& plan, int fixtureIndex)
{
	if(fixtureIndex < 0 || fixtureIndex >= plan.fixtures.size() || plan.fixtures[fixtureIndex].type != "turret")
		return 0;
	int n = 0;
	for(int i = 0; i <= fixtureIndex; i++)
		n += plan.fixtures[i].type == "turret";
	return n;
}

int TurretGunner(const Plan& plan, int fixtureIndex)
{
	return GunnerLinks(plan).indexOf(fixtureIndex);
}

QString RoomTexture(const Plan& plan, const Room& room, const QString& surface)
{
	const QStringList& palette = ThemePalette(plan.theme);
	const QString picked = surface == "floor" ? room.floor : surface == "walls" ? room.walls : room.ceiling;
	if(palette.contains(picked))
		return picked;
	for(const QString& k : RoomTypeTextures(room.type, surface))
		if(palette.contains(k))
			return k;
	return ThemeTexture(plan.theme, surface);
}

QString BuildingTexture(const Plan& plan, const QString& surface)
{
	const QString picked = surface == "outside" ? plan.outside : surface == "roofing" ? plan.roofing : QString();
	return ThemePalette(plan.theme).contains(picked) ? picked : ThemeTexture(plan.theme, surface);
}

double RoomTextureScale(const Room& room, const QString& surface)
{
	return qBound(10, room.scales.value(surface, 100), 1000) / 100.0;
}

double BuildingTextureScale(const Plan& plan, const QString& what)
{
	return qBound(10, plan.scales.value(what, 100), 1000) / 100.0;
}

QString RoomDesign(const Room& room)
{
	return room.design == "none" || RoomDesignKinds().contains(room.design) ? room.design : room.type;
}

QString OutsideDressing(const Plan& plan)
{
	const QString key = plan.outsideTrim.isEmpty() ? plan.theme : plan.outsideTrim;
	return key == "plain" || !FindThemeKind(key) ? QString("none") : key;
}

QString OutsideTrimTexture(const Plan& plan)
{
	return ThemePalette(plan.theme).contains(plan.outsideTrimTexture) ? plan.outsideTrimTexture : QString();
}

QColor OutsideTrimColor(const Plan& plan)
{
	return plan.outsideTrimColor.isEmpty() ? QColor() : QColor(plan.outsideTrimColor);
}

double OutsideTrimStrength(const Plan& plan)
{
	return qBound(0, plan.outsideTrimStrength, 100) / 100.0;
}

QString RoomTrimTexture(const Plan& plan, const Room& room)
{
	return ThemePalette(plan.theme).contains(room.trim) ? room.trim : QString();
}

QColor RoomColor(const Room& room, const QString& surface)
{
	return room.colors.contains(surface) ? QColor(room.colors[surface]) : QColor();
}

double RoomColorStrength(const Room& room, const QString& surface)
{
	return qBound(0, room.strengths.value(surface, 50), 100) / 100.0;
}

int TextureSlot(const Plan& plan, const QString& key)
{
	return ThemePalette(plan.theme).indexOf(key);
}

QList<QPoint> TurretCells(const Fixture& turret)
{
	const QPoint c = turret.cell;
	return {c, c + QPoint(1, 0), c + QPoint(0, 1), c + QPoint(1, 1)};
}

bool TurretOnGround(const Plan& plan, const Fixture& turret)
{
	if(turret.deck != 0)
		return false;
	for(const QPoint& c : TurretCells(turret))
		if(plan.At(0, c) != '.')
			return false;
	return true;
}

QString FixtureProblem(const Plan& plan, const Fixture& fx, int ignore)
{
	const FixtureType* type = FindFixtureType(fx.type);
	if(!type)
		return QString("There is no fixture of type %1.").arg(fx.type);
	if(!fx.kind.isEmpty() && !type->kinds.contains(fx.kind))
		return QString("A %1 is one of: %2.").arg(type->name.toLower()).arg(type->kinds.join(", "));
	if(!fx.color.isEmpty() && !LightColors().contains(fx.color))
		return QString("A light's colour is one of: %1.").arg(LightColors().join(", "));
	if(fx.deck < 0 || fx.deck >= plan.Decks() || !plan.OnGrid(fx.cell))
		return "It must be in a cell of the grid.";
	auto places = [&](const Fixture& f) {
		QList<QPair<QPoint, int>> at;
		if(f.type == "turret")
			for(const QPoint& c : TurretCells(f))
				at << qMakePair(c, TurretOnGround(plan, f) ? 0 : -1);
		else
			at << qMakePair(f.cell, f.type == "light" ? (f.dir == FixtureTop ? -1 : 1 + ((f.dir % 4) + 4) % 4) : 0);
		return at;
	};
	const QList<QPair<QPoint, int>> mine = places(fx);
	for(int i = 0; i < plan.fixtures.size(); i++)
	{
		const Fixture& o = plan.fixtures[i];
		bool shared = false;
		for(const QPair<QPoint, int>& a : places(o))
			shared = shared || mine.contains(a);
		if(i != ignore && o.deck == fx.deck && shared)
			return o.type == "turret" && fx.type != "turret" ? "A turret stands here." : "There is one here already.";
	}
	const QChar w = plan.At(fx.deck, fx.cell);
	const bool room = w != '.' && w != '*' && plan.rooms.contains(w) && !IsOpenCell(plan, fx.deck, fx.cell) && !IsLiftRoom(plan, w);
	const QString kind = fx.kind.isEmpty() ? type->kinds.first() : fx.kind;
	if(fx.type == "berth")
		return room ? QString() : QString("A berth needs a whole room cell, with a floor.");
	if(fx.type == "gametable")
	{
		double x, y;
		if(!room)
			return "A game table needs a whole room cell, with a floor.";
		return ParkingCentre(plan, fx, x, y) ? QString() : QString("A game table and its seats take a 4 m square: it does not fit in "
			"the room here.");
	}
	if(fx.type == "turret")
	{
		int before = 0;
		for(int i = 0; i < plan.fixtures.size() && (ignore < 0 || i < ignore); i++)
			before += plan.fixtures[i].type == "turret";
		if(before >= 6)
			return "A building has six turrets at most.";
		for(const QPoint& c : TurretCells(fx))
			if(!plan.OnGrid(c))
				return "A turret takes two cells by two, this one and those east and south of it: they run off the grid.";
		if(TurretOnGround(plan, fx))
			return QString();
		for(const QPoint& c : TurretCells(fx))
		{
			const QChar r = plan.At(fx.deck, c);
			if(r == '.' || r == '*' || !plan.rooms.contains(r) || IsOpenAirRoom(plan, r) || IsLiftRoom(plan, r))
				return "A turret stands on the roof of two cells by two of rooms (this one and those east and south of it; not "
					"balconies or lifts), or on the ground outside, all four empty.";
			for(int q = 0; q < 4; q++)
				if(plan.Quarter(fx.deck + 1, c, q) != '.')
					return "Something is built on top of its cells: no roof for a turret.";
			QChar roof = plan.RoofAt(fx.deck, c);
			if(roof != '.' && plan.roofs.contains(roof) && plan.roofs[roof].style != "flat")
				return "A turret needs a flat roof, or none.";
			for(const Skylight& s : plan.skylights)
				if(s.deck == fx.deck && s.cell == c)
					return "There is a skylight under it.";
			for(const Hatch& h : plan.hatches)
				if(h.roof && h.deck == fx.deck && h.cell == c)
					return "There is a hull hatch under it.";
		}
		return QString();
	}
	if(fx.type == "parking")
	{
		if(kind == "spacecraft")
			return fx.deck == 0 && w == '.' ? QString() : QString("A spacecraft's spot is on the ground outside the building.");
		if(!room)
			return "A parking spot needs a whole room cell, with a floor (a vehicle bay, with a door vehicles can use).";
		double x, y, across, along;
		ParkingSize(kind, across, along);
		if(!ParkingCentre(plan, fx, x, y))
			return QString("A %1 spot is %2 m by %3 m: it does not fit in the room here.").arg(kind).arg(across).arg(along);
		return QString();
	}
	if(fx.type == "light")
	{
		if(w == '.' || !plan.rooms.contains(w == '*' ? QChar() : w) || IsOpenAirRoom(plan, w))
			return "A light goes on the building: on a whole cell of a room (not a balcony).";
		if(fx.dir == FixtureTop)
		{
			for(int q = 0; q < 4; q++)
				if(plan.Quarter(fx.deck + 1, fx.cell, q) != '.')
					return "Something is built on top of this cell: no roof for a light.";
			QChar roof = plan.RoofAt(fx.deck, fx.cell);
			if(roof != '.' && plan.roofs.contains(roof) && plan.roofs[roof].style != "flat")
				return "A light on top needs a flat roof, or none.";
			return QString();
		}
		QChar other = plan.Quarter(fx.deck, Neighbour(fx.cell, Side(((fx.dir % 4) + 4) % 4)), Opposite(Side(((fx.dir % 4) + 4) % 4)));
		if(other != '.')
			return "A light goes on an outside wall: that side is not one.";
		return QString();
	}
	if(fx.deck != 0 || w != '.')
		return "A pole stands on the ground outside the building.";
	return QString();
}

QPoint StationSpareCell(const Plan& plan, const Station& st)
{
	QChar room = plan.At(st.deck, st.cell);
	const int d = ((st.dir % 4) + 4) % 4;
	QList<int> order = st.wall ? QList<int>{(d + 2) % 4, (d + 1) % 4, (d + 3) % 4} : QList<int>{d, (d + 2) % 4, (d + 1) % 4, (d + 3) % 4};
	for(int s : order)
	{
		QPoint c = Neighbour(st.cell, Side(s));
		if(plan.At(st.deck, c) == room && !IsOpenCell(plan, st.deck, c))
			return c;
	}
	return QPoint(-1, -1);
}

QString StationProblem(const Plan& plan, const Station& st, int ignore)
{
	const StationKind* kind = FindStationKind(st.kind);
	if(!kind)
		return QString("There is no station of kind %1.").arg(st.kind);
	if(!st.module.isEmpty() && !kind->modules.contains(st.module))
		return kind->modules.isEmpty() ? QString("A %1 station runs no module.").arg(kind->name)
			: QString("A %1 station runs one of: %2.").arg(kind->name).arg(kind->modules.join(", "));
	if(st.deck < 0 || st.deck >= plan.Decks() || !plan.OnGrid(st.cell))
		return "A station must be in a cell of the grid.";
	QChar room = plan.At(st.deck, st.cell);
	if(room == '.' || room == '*' || !plan.rooms.contains(room) || IsOpenCell(plan, st.deck, st.cell))
		return "A station needs a whole room cell, with a floor.";
	if(IsLiftRoom(plan, room))
		return "A turbo lift's shaft has no room for a station.";
	if(st.kind == "gunner")
	{
		Plan with = plan;
		int self = ignore;
		if(self < 0 || self >= with.stations.size())
		{
			with.stations << st;
			self = with.stations.size() - 1;
		}
		else
			with.stations[self] = st;
		if(GunnerLinks(with).value(self, -1) < 0)
			return st.turret > 0 ? QString("There is no turret %1, or it has a gunner already.").arg(st.turret)
				: QString("Every turret has a gunner already: place a turret first.");
	}
	for(const Stair& s : plan.stairs)
		if(s.deck == st.deck && LayoutStair(plan, s).cells.contains(st.cell))
			return "A stair takes up this cell.";
	int same = 0;
	for(int i = 0; i < plan.stations.size(); i++)
	{
		if(i == ignore)
			continue;
		const Station& o = plan.stations[i];
		if(o.deck == st.deck && o.cell == st.cell)
			return "There is a station in this cell already.";
		same += o.kind == st.kind;
	}
	if(same >= 10)
		return QString("The game allows 10 %1 stations.").arg(kind->name.toLower());
	const int d = ((st.dir % 4) + 4) % 4;
	if(st.wall)
	{
		QChar other = plan.Quarter(st.deck, Neighbour(st.cell, Side(d)), Opposite(Side(d)));
		if(other == room)
			return "There is no wall on that side: the room carries on.";
		if(IsOpenEdge(plan, room, other))
			return "That side is open to the air: no wall for a screen.";
		static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
		V2 m = V2((plan.X0(st.cell.x()) + plan.X1(st.cell.x())) / 2, (plan.Y0(st.cell.y()) + plan.Y1(st.cell.y())) / 2)
			+ dirs[d] * (plan.cell / 2);
		V2 u(dirs[d].y, -dirs[d].x);
		for(const Door& door : plan.doors)
		{
			DoorPlace dp = PlaceDoor(plan, door);
			V2 dm(dp.mx, dp.my);
			if(door.deck == st.deck && dp.ok && qAbs(u.Cross(dm - m)) < 1e-6 && qAbs(u.Dot(dm - m)) < DoorWidth(plan, door) / 2 + 0.55)
				return "There is a doorway in that wall.";
		}
		for(const Window& win : plan.windows)
		{
			DoorPlace wp = PlaceWindow(plan, win);
			V2 wm(wp.mx, wp.my);
			if(win.deck == st.deck && wp.ok && qAbs(u.Cross(wm - m)) < 1e-6 && qAbs(u.Dot(wm - m)) < WindowWidth(plan, win) / 2 + 0.55)
				return "There is a window in that wall.";
		}
	}
	if(st.kind == "transporter")
	{
		if(st.pads.isEmpty() && StationSpareCell(plan, st) == QPoint(-1, -1))
			return "A transporter needs a pad: another cell of its room (pads=).";
		for(const QPoint& p : st.pads)
			if(plan.At(st.deck, p) != room || IsOpenCell(plan, st.deck, p))
				return "A transporter's pads are whole cells of its room.";
	}
	if(st.kind == "medic" && StationSpareCell(plan, st) == QPoint(-1, -1))
		return "A medic station needs another cell of its room for the patient.";
	return QString();
}

bool IsOutsideWall(const Plan& plan, const DoorPlace& place)
{
	return place.there == '.' || IsOpenAirRoom(plan, place.here) != IsOpenAirRoom(plan, place.there);
}

bool CanRoof(const Plan& plan, int deck, const QPoint& cell)
{
	for(int q = 0; q < 4; q++)
		if(plan.Quarter(deck, cell, q) != '.' && plan.Quarter(deck + 1, cell, q) == '.')
			return true;
	return false;
}

}
