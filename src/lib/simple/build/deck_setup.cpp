#include "planbuilder.h"
#include "internal.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace SimpleMode::detail {

bool PlanBuilder::onShell(const DoorPlace& place)
{
	return IsOutsideWall(plan, place);
}

QList<Hole> PlanBuilder::windowHoles(const V2& a, const V2& b, const V2& lineA, const V2& lineB, QChar room, bool hull)
{
	QList<Hole> list;
	V2 u = (lineB - lineA).Unit();
	double len = (lineB - lineA).Length();
	for(const PlacedWindow& pw : windows)
	{
		V2 m(pw.place.mx, pw.place.my), wu(pw.place.ux, pw.place.uy);
		if(qAbs(wu.Cross(u)) > 1e-6 || qAbs((m - lineA).Cross(u)) > 1e-6)
			continue;
		double along = (m - lineA).Dot(u);
		if(along < -1e-6 || along > len + 1e-6)
			continue;
		if(hull ? !onShell(pw.place) : (pw.place.here != room && pw.place.there != room))
			continue;
		double s = (m - a).Dot((b - a).Unit());
		list << Hole{s - pw.width / 2, s + pw.width / 2, pw.sill, pw.head};
	}
	return list;
}

bool PlanBuilder::wallBeside(const PlacedDoor& pd, const V2& dir, double len)
{
	V2 m(pd.place.mx, pd.place.my), n(pd.place.nx, pd.place.ny);
	auto spaceAt = [&](const V2& p) {
		QPoint cell;
		int q;
		return QuarterAtPoint(plan, p.x, p.y, cell, q) ? plan.Quarter(D, cell, q) : QChar('.');
	};
	const double half = DoorWidth(plan, pd.door) / 2;
	for(double s = half + 0.05; s <= half + len + 0.05; s += 0.1)
	{
		V2 p = m + dir * s;
		if(spaceAt(p - n * 0.05) == spaceAt(p + n * 0.05))
			return false;
		for(const PlacedDoor& o : doors)
		{
			V2 om(o.place.mx, o.place.my);
			if(&o != &pd && qAbs((om - m).Cross(dir)) < 1e-3 && (p - om).Length() < DoorWidth(plan, o.door) / 2 + 0.05)
				return false;
		}
	}
	return true;
}

bool PlanBuilder::nothingAbove(const V2& m, const V2& n)
{
	if(D + 1 >= plan.Decks())
		return true;
	V2 L = n.Left();
	for(double a : {-0.4, 0.4})
		for(double b : {-0.6, 0.0, 0.6})
		{
			V2 p = m + n * a + L * b;
			QPoint cell;
			int q;
			if(QuarterAtPoint(plan, p.x, p.y, cell, q) && plan.Quarter(D + 1, cell, q) != '.')
				return false;
		}
	return true;
}

const PlanBuilder::ConsoleSpot* PlanBuilder::consoleOf(const PlacedDoor& pd)
{
	for(const ConsoleSpot& c : consoles)
		if(c.doorAt == pd.door.at && c.doorLine == pd.door.line)
			return &c;
	return nullptr;
}

QList<QPair<double, double>> PlanBuilder::openings(const V2& a, const V2& b, const V2& lineA, const V2& lineB, QChar room, bool hull)
{
	QList<QPair<double, double>> list;
	V2 u = (lineB - lineA).Unit();
	double len = (lineB - lineA).Length();
	for(const PlacedDoor& pd : doors)
	{
		V2 m(pd.place.mx, pd.place.my), du(pd.place.ux, pd.place.uy);
		if(pd.door.opening || qAbs(du.Cross(u)) > 1e-6 || qAbs((m - lineA).Cross(u)) > 1e-6)
			continue;
		double along = (m - lineA).Dot(u);
		if(along < -1e-6 || along > len + 1e-6)
			continue;
		if(hull ? !onShell(pd.place) : (pd.place.here != room && pd.place.there != room))
			continue;
		double s = (m - a).Dot((b - a).Unit());
		list << qMakePair(s - DoorWidth(plan, pd.door) / 2, s + DoorWidth(plan, pd.door) / 2);
	}
	return list;
}

void PlanBuilder::PlaceDeckItems()
{
	doors.clear();
	for(const Door& d : plan.doors)
	{
		if(d.deck != D)
			continue;
		DoorPlace dp = PlaceDoor(plan, d);
		if(!dp.ok)
		{
			log << "  Skipping door at " << d.at.x() << "," << d.at.y() << " on deck " << D << ": " << dp.problem << "\n";
			continue;
		}
		if(!plan.interiors && (d.opening || !onShell(dp)))
			continue;
		doors << PlacedDoor{d, dp};
	}

	windows.clear();
	windowSpots.clear();
	for(int wi = 0; wi < plan.windows.size(); wi++)
	{
		const Window& w = plan.windows[wi];
		if(w.deck != D)
			continue;
		DoorPlace wp = PlaceWindow(plan, w);
		QString problem = wp.problem;
		bool ok = wp.ok && WindowClear(plan, w, problem, wi);
		for(const PlacedWindow& o : windows)
			if(ok && o.window.at == w.at && o.window.line == w.line)
			{
				ok = false;
				problem = "There is already a window here.";
			}
		const WindowShape* shape = FindWindowShape(w.shape.isEmpty() ? "standard" : w.shape);
		if(!shape)
			shape = FindWindowShape("standard");
		double sill = qMax(0.1, shape->sill), head = qMin(shape->head, HC - 0.1);
		if(ok && head - sill < 0.3)
		{
			ok = false;
			problem = "The deck is too low for it.";
		}
		if(!ok)
		{
			log << "  Skipping window at " << w.at.x() << "," << w.at.y() << " on deck " << D << ": " << problem << "\n";
			continue;
		}
		if(!plan.interiors && !onShell(wp))
			continue;
		PlacedWindow pw{w, wp, WindowWidth(plan, w), sill, head, w.tech >= 1 && w.tech <= 5 ? w.tech : DefaultWindowTech(plan.theme)};
		windows << pw;
		bool shutters = pw.tech == 1 && pw.width >= 0.5 && pw.width <= 1.2;
		windowSpots << WindowSpot{V2(wp.mx, wp.my), V2(wp.ux, wp.uy), pw.width / 2, sill, head, shutters ? pw.width / 2 + 0.1 : 0.14};
	}
	for(const PlacedDoor& pd : doors)
	{
		if(!pd.door.opening)
			continue;
		double spring, top;
		OpeningHeights(plan, pd.door, spring, top);
		PlacedWindow pw{Window{}, pd.place, DoorWidth(plan, pd.door), 0, top, 0};
		pw.kind = KindOpening;
		pw.door = pd.door;
		windows << pw;
		windowSpots << WindowSpot{V2(pd.place.mx, pd.place.my), V2(pd.place.ux, pd.place.uy), pw.width / 2, 0, top, 0.14};
	}
	for(const Railing& r : plan.railings)
	{
		if(r.deck != D)
			continue;
		DoorPlace rp = PlaceRailing(plan, r);
		const double len = RailingLength(plan, r);
		QString problem = rp.problem;
		V2 rm(rp.mx, rp.my), ru(rp.ux, rp.uy);
		for(const PlacedDoor& pd : doors)
		{
			V2 dm(pd.place.mx, pd.place.my);
			if(rp.ok && qAbs(ru.Cross(V2(pd.place.ux, pd.place.uy))) < 1e-6 && qAbs(ru.Cross(dm - rm)) < 0.25
					&& qAbs(ru.Dot(dm - rm)) < len / 2 + DoorWidth(plan, pd.door) / 2)
			{
				rp.ok = false;
				problem = "There is a doorway in that wall.";
			}
		}
		for(const PlacedWindow& o : windows)
		{
			V2 om(o.place.mx, o.place.my);
			if(rp.ok && o.kind == KindWindow && qAbs(ru.Cross(V2(o.place.ux, o.place.uy))) < 1e-6
					&& qAbs(ru.Cross(om - rm)) < 1e-6 && qAbs(ru.Dot(om - rm)) < len / 2 + o.width / 2)
			{
				rp.ok = false;
				problem = "There is a window in that wall.";
			}
		}
		if(!rp.ok)
		{
			log << "  Skipping railing at " << r.at.x() << "," << r.at.y() << " on deck " << D << ": " << problem << "\n";
			continue;
		}
		if(!plan.interiors && rp.here != rp.there)
			continue;
		PlacedWindow pw{Window{}, rp, len, 0, HC, 0};
		pw.kind = KindRailing;
		pw.railing = r;
		windows << pw;
		if(rp.here != rp.there)
			windowSpots << WindowSpot{rm, ru, len / 2, 0, HC, 0.02};
	}

	consoles.clear();
	for(const PlacedDoor& pd : doors)
	{
		if(pd.door.opening || pd.door.operation != "console" || pd.door.console == QPoint(-1, -1))
			continue;
		V2 spot(plan.QX(pd.door.console.x()), plan.QY(pd.door.console.y()));
		QPoint cell;
		int q;
		if(!QuarterAtPoint(plan, spot.x, spot.y, cell, q) || plan.Quarter(D, cell, q) == '.')
			continue;
		QChar room = plan.Quarter(D, cell, q);
		double best = 1e9;
		ConsoleSpot found;
		for(const QList<Edge>& loop : RoomOutlines(plan, D, room))
			for(const Edge& e : loop)
			{
				V2 eu = (e.b - e.a).Unit(), in = eu.Left();
				double len = (e.b - e.a).Length();
				for(double s = 0.35; s <= len - 0.35; s += 0.05)
				{
					V2 line = e.a + eu * s;
					bool clear = true;
					for(const PlacedDoor& o : doors)
						clear = clear && (line - V2(o.place.mx, o.place.my)).Length() > DoorWidth(plan, o.door) / 2 + SWITCH_ASIDE + 0.35;
					for(const ConsoleSpot& c : consoles)
						clear = clear && (line + in * e.inset - c.at).Length() > 0.5;
					if(!clear)
						continue;
					V2 p = line + in * e.inset;
					if((p - spot).Length() < best)
					{
						best = (p - spot).Length();
						found = ConsoleSpot{pd.door.at, pd.door.line, p, eu, in, room};
					}
				}
			}
		if(best < 1e8)
			consoles << found;
	}

	doorStyles.clear();
	switchSides.clear();
	switchMarkers.clear();
	switchSpots.clear();
	for(const PlacedDoor& pd : doors)
	{
		if(pd.door.opening)
		{
			doorStyles << QString();
			switchSides << V2();
			switchMarkers << QList<int>();
			continue;
		}
		bool hullDoor = pd.place.there == '.';
		V2 m(pd.place.mx, pd.place.my), n(pd.place.nx, pd.place.ny), u(pd.place.ux, pd.place.uy), L = n.Left();
		const double width = DoorWidth(plan, pd.door);
		QString style = pd.door.style.isEmpty() ? DefaultDoorStyle(plan.theme, hullDoor, pd.door.wide) : pd.door.style;
		QString why;
		QStringList tried;
		while(!tried.contains(style))
		{
			tried << style;
			QString next;
			if(style == "portcullis" && !(hullDoor && nothingAbove(m, n)))
			{
				why = "a portcullis needs an outside wall with nothing built above it";
				next = "swing-in-left";
			}
			else if(style == "split-h" && D != 0)
			{
				why = "splitting up and down needs the ground deck, for the bottom half to sink into";
				next = "split-v";
			}
			else if(pd.door.wide && !FindDoorStyle(style)->wide)
			{
				why = "that style is for single doors";
				next = DefaultDoorStyle(plan.theme, hullDoor, true);
			}
			else if(style == "split-v" && !(wallBeside(pd, L, width / 2) && wallBeside(pd, L * -1, width / 2)))
			{
				why = "there is not enough plain wall on both sides for the halves to slide into";
				next = pd.door.wide ? "slide-up" : wallBeside(pd, L, width) ? "slide-left" : "slide-right";
			}
			else if((style == "slide-left" || style == "slide-right") && !wallBeside(pd, style.endsWith("left") ? L : L * -1, width))
			{
				why = "there is not enough plain wall beside it to slide into";
				next = style.endsWith("left") ? "slide-right" : "swing-in-left";
			}
			if(next.isEmpty())
				break;
			style = next;
		}
		if(!why.isEmpty())
			log << "  Door at " << pd.door.at.x() << "," << pd.door.at.y() << " on deck " << D << ": " << why << "; it is \""
				<< FindDoorStyle(style)->name << "\" instead.\n";
		V2 switchSide = style.startsWith("swing") || style == "slide-left" || style == "slide-right"
			? (style.endsWith("left") ? L * -1 : L) : u;
		QList<int> markers = {-1, 1};
		if(pd.door.operation == "door")
			markers.clear();
		else if(pd.door.operation == "console" && consoleOf(pd))
			markers = {consoleOf(pd)->room == pd.place.here ? 1 : -1};
		doorStyles << style;
		switchSides << switchSide;
		switchMarkers << markers;
		for(int side : markers)
			switchSpots << SwitchSpot{m + switchSide * (width / 2 + SWITCH_ASIDE), n, side};
	}
}

}
