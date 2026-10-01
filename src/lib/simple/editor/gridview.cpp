#include "gridview.h"
#include "palette.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QtMath>

using namespace SimpleMode;

namespace {

const double SWITCH_ROOM = 0.6;

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
	return w != '.' && w != '*';
}

}

QList<QPair<QPoint, int>> ShapeAt(const Plan& plan, const QPoint& start, int startQ, const bool lines[4])
{
	QList<QPair<QPoint, int>> shape{qMakePair(start, startQ)};
	QSet<int> seen;
	auto key = [&](const QPoint& p, int q) { return (p.y() * plan.Cols() + p.x()) * 4 + q; };
	seen.insert(key(start, startQ));
	for(int i = 0; i < shape.size(); i++)
		for(int k = 0; k < 3; k++)
		{
			QPoint oc;
			int oq;
			Line line;
			QuarterEdge(shape[i].first, shape[i].second, k, oc, oq, line);
			if(lines[line] || !plan.OnGrid(oc) || seen.contains(key(oc, oq)))
				continue;
			seen.insert(key(oc, oq));
			shape << qMakePair(oc, oq);
		}
	return shape;
}

GridView::GridView(Plan& plan, QWidget* parent) : QWidget(parent), m_Plan(plan)
{
	setMouseTracking(true);
	PlanResized();
}

QString GridView::RoomLetters()
{
	return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
}

QColor GridView::RoomColor(QChar letter)
{
	static const QColor colors[] = {
		QColor(0x2f, 0x5d, 0x86), QColor(0x86, 0x5c, 0x22), QColor(0x3e, 0x74, 0x3a), QColor(0x82, 0x3c, 0x55),
		QColor(0x5b, 0x48, 0x86), QColor(0x7c, 0x70, 0x22), QColor(0x2a, 0x72, 0x6a), QColor(0x86, 0x4b, 0x30),
		QColor(0x58, 0x6c, 0x2c), QColor(0x3c, 0x4f, 0x8a), QColor(0x74, 0x5c, 0x40), QColor(0x74, 0x3e, 0x78),
	};
	int index = RoomLetters().indexOf(letter);
	return index < 0 ? QColor(0x55, 0x55, 0x55) : colors[index % 12];
}

void GridView::SetTool(Tool tool)
{
	if(tool == PlaceConsole && m_Tool != PlaceConsole)
		m_ToolBeforeConsole = DoorTool() ? m_Tool : InteriorDoor;
	m_Tool = tool;
	if(m_HasSelection && !DoorTool() && m_Tool != PlaceConsole)
		SelectDoor(-1);
	if(m_HasHatchSelection && !HatchTool())
		SelectHatch(-1);
	if(m_HasWindowSelection && m_Tool != PlaceWindow)
		SelectWindow(-1);
	if(m_HasRailingSelection && m_Tool != PlaceRailing)
		SelectRailing(-1);
	if(m_HasStationSelection && m_Tool != PlaceStation)
		SelectStation(-1);
	if(m_HasFixtureSelection && !FixtureTool())
		SelectFixture(-1);
	if(!m_RoofSelection.isNull() && !RoofTool())
		SelectRoof(QChar());
	if(m_HasStairSelection && !StairTool())
		SelectStair(-1);
	if(m_HasDormerSelection && m_Tool != PlaceDormer)
		SelectDormer(-1);
	if(m_HasSkylightSelection && m_Tool != PlaceSkylight)
		SelectSkylight(-1);
	emit ToolSet(m_Tool);
	update();
}

int GridView::SelectedDoor() const
{
	return m_HasSelection ? FindDoor(m_Selection) : -1;
}

int GridView::SelectedHatch() const
{
	if(!m_HasHatchSelection)
		return -1;
	for(int i = 0; i < m_Plan.hatches.size(); i++)
		if(m_Plan.hatches[i].deck == m_HatchDeck && m_Plan.hatches[i].cell == m_HatchCell && m_Plan.hatches[i].roof == m_HatchRoof)
			return i;
	return -1;
}

void GridView::SelectHatch(int index)
{
	m_HasHatchSelection = index >= 0 && index < m_Plan.hatches.size();
	if(m_HasHatchSelection)
	{
		m_HatchDeck = m_Plan.hatches[index].deck;
		m_HatchCell = m_Plan.hatches[index].cell;
		m_HatchRoof = m_Plan.hatches[index].roof;
	}
	emit HatchSelected(SelectedHatch());
	update();
}

int GridView::SelectedDormer() const
{
	if(!m_HasDormerSelection)
		return -1;
	for(int i = 0; i < m_Plan.dormers.size(); i++)
		if(m_Plan.dormers[i].deck == m_DormerDeck && m_Plan.dormers[i].cell == m_DormerCell)
			return i;
	return -1;
}

void GridView::SelectDormer(int index)
{
	m_HasDormerSelection = index >= 0 && index < m_Plan.dormers.size();
	if(m_HasDormerSelection)
	{
		m_DormerDeck = m_Plan.dormers[index].deck;
		m_DormerCell = m_Plan.dormers[index].cell;
	}
	emit DormerSelected(SelectedDormer());
	update();
}

int GridView::SelectedSkylight() const
{
	if(!m_HasSkylightSelection)
		return -1;
	for(int i = 0; i < m_Plan.skylights.size(); i++)
		if(m_Plan.skylights[i].deck == m_SkylightDeck && m_Plan.skylights[i].cell == m_SkylightCell)
			return i;
	return -1;
}

void GridView::SelectSkylight(int index)
{
	m_HasSkylightSelection = index >= 0 && index < m_Plan.skylights.size();
	if(m_HasSkylightSelection)
	{
		m_SkylightDeck = m_Plan.skylights[index].deck;
		m_SkylightCell = m_Plan.skylights[index].cell;
	}
	emit SkylightSelected(SelectedSkylight());
	update();
}

int GridView::DormerAt(const QPoint& cell) const
{
	for(int i = 0; i < m_Plan.dormers.size(); i++)
		if(m_Plan.dormers[i].deck == m_Deck && m_Plan.dormers[i].cell == cell)
			return i;
	return -1;
}

int GridView::SkylightIn(const QPoint& cell) const
{
	for(int i = 0; i < m_Plan.skylights.size(); i++)
		if(m_Plan.skylights[i].deck == m_Deck && m_Plan.skylights[i].cell == cell)
			return i;
	return -1;
}

int GridView::SideAtPx(const QPoint& px, const QPoint& cell) const
{
	double fx = double(px.x() - Margin - cell.x() * CellPx) / CellPx, fy = double(px.y() - Margin - cell.y() * CellPx) / CellPx;
	const double d[4] = {fy, 1 - fx, 1 - fy, fx};
	int best = North;
	for(int s = 1; s < 4; s++)
		if(d[s] < d[best])
			best = s;
	return best;
}

static QString SideName(int side)
{
	static const char* names[] = {"north", "east", "south", "west"};
	return names[((side % 4) + 4) % 4];
}

void GridView::PlaceDormerAt(const QPoint& cell, int side)
{
	int i = DormerAt(cell);
	if(i >= 0)
	{
		SelectDormer(i);
		return;
	}
	Dormer d{m_Deck, cell, side, QString()};
	QString problem = DormerProblem(m_Plan, d);
	if(!problem.isEmpty())
	{
		emit Status(tr("No dormer looking %1 here: %2").arg(SideName(side)).arg(problem));
		return;
	}
	emit EditStarting();
	m_Plan.dormers << d;
	emit Status(tr("Dormer looking out %1. Its style is in the Dormer panel; right-click it to turn it.").arg(SideName(side)));
	emit PlanChanged();
	SelectDormer(m_Plan.dormers.size() - 1);
}

void GridView::TurnDormerIndex(int index)
{
	if(index < 0 || index >= m_Plan.dormers.size())
		return;
	Dormer changed = m_Plan.dormers[index];
	for(int k = 1; k < 4; k++)
	{
		changed.side = (m_Plan.dormers[index].side + k) % 4;
		if(DormerProblem(m_Plan, changed, index).isEmpty())
		{
			emit EditStarting();
			m_Plan.dormers[index] = changed;
			emit Status(tr("The dormer looks out %1.").arg(SideName(changed.side)));
			emit PlanChanged();
			SelectDormer(index);
			return;
		}
	}
	emit Status(tr("The dormer fits no other way round here."));
	SelectDormer(index);
}

void GridView::PlaceSkylightAt(const QPoint& cell)
{
	int i = SkylightIn(cell);
	if(i >= 0)
	{
		SelectSkylight(i);
		return;
	}
	Skylight s{m_Deck, cell, QString()};
	QString problem = DormerAt(cell) >= 0 ? tr("A dormer stands on the roof over this cell.") : SkylightProblem(m_Plan, s);
	if(!problem.isEmpty())
	{
		emit Status(tr("No skylight here: %1").arg(problem));
		return;
	}
	emit EditStarting();
	m_Plan.skylights << s;
	emit Status(tr("Skylight in the ceiling, up through the roof. Right-click it (on a flat top) for a lantern."));
	emit PlanChanged();
	SelectSkylight(m_Plan.skylights.size() - 1);
}

void GridView::CycleSkylight(const QPoint& cell)
{
	int i = SkylightIn(cell);
	if(i < 0)
		return;
	Skylight changed = m_Plan.skylights[i];
	changed.style = changed.style == "lantern" ? QString() : QString("lantern");
	QString problem = SkylightProblem(m_Plan, changed, i);
	if(!problem.isEmpty())
	{
		emit Status(problem);
		SelectSkylight(i);
		return;
	}
	emit EditStarting();
	m_Plan.skylights[i] = changed;
	emit Status(changed.style == "lantern" ? tr("A glass lantern over the skylight.") : tr("A pane over the skylight."));
	emit PlanChanged();
	SelectSkylight(i);
}

int GridView::SelectedWindow() const
{
	return m_HasWindowSelection ? FindWindow(m_WindowSelection) : -1;
}

void GridView::SelectWindow(int index)
{
	m_HasWindowSelection = index >= 0 && index < m_Plan.windows.size();
	if(m_HasWindowSelection)
		m_WindowSelection = m_Plan.windows[index];
	emit WindowSelected(SelectedWindow());
	update();
}

QChar GridView::SelectedRoof() const
{
	return m_Plan.roofs.contains(m_RoofSelection) ? m_RoofSelection : QChar();
}

void GridView::SelectRoof(QChar letter)
{
	m_RoofSelection = m_Plan.roofs.contains(letter) ? letter : QChar();
	emit RoofSelected(m_RoofSelection);
	update();
}

void GridView::SelectDoor(int index)
{
	m_HasSelection = index >= 0 && index < m_Plan.doors.size();
	if(m_HasSelection)
		m_Selection = m_Plan.doors[index];
	emit DoorSelected(SelectedDoor());
	update();
}

void GridView::SetCurrentRoom(QChar letter, const Room& pending)
{
	m_Room = letter;
	m_PendingRoom = pending;
}

void GridView::SetDeck(int deck)
{
	m_Deck = qBound(0, deck, qMax(0, m_Plan.Decks() - 1));
	update();
}

void GridView::SetLine(Line line, bool on)
{
	m_Lines[line] = on;
	update();
}

void GridView::PlanResized()
{
	m_Deck = qBound(0, m_Deck, qMax(0, m_Plan.Decks() - 1));
	resize(sizeHint());
	updateGeometry();
	update();
}

QSize GridView::sizeHint() const
{
	return QSize(Margin * 2 + qMax(1, m_Plan.Cols()) * CellPx, Margin * 2 + qMax(1, m_Plan.Rows()) * CellPx);
}

QPointF GridView::LatticePx(const QPoint& p) const
{
	return QPointF(Margin + p.x() * CellPx / 2.0, Margin + p.y() * CellPx / 2.0);
}

bool GridView::QuarterAtPx(const QPoint& px, QPoint& cell, int& quarter) const
{
	double x = m_Plan.X0(0) + double(px.x() - Margin) / CellPx * m_Plan.cell;
	double y = m_Plan.Y1(0) - double(px.y() - Margin) / CellPx * m_Plan.cell;
	return QuarterAtPoint(m_Plan, x, y, cell, quarter);
}

bool GridView::CellAt(const QPoint& px, QPoint& cell) const
{
	int q;
	return QuarterAtPx(px, cell, q);
}

bool GridView::DoorAtPx(const QPoint& px, Door& door) const
{
	double fx = double(px.x() - Margin) / (CellPx / 4.0), fy = double(px.y() - Margin) / (CellPx / 4.0);
	struct Candidate { QPoint at; Line line; double dist; };
	QList<Candidate> list;
	{
		int y = qRound(fy / 4) * 4, x = qRound(fx / 2) * 2;
		list << Candidate{QPoint(x, y), Horizontal, qAbs(fy - y)};
	}
	{
		int x = qRound(fx / 4) * 4, y = qRound(fy / 2) * 2;
		list << Candidate{QPoint(x, y), Vertical, qAbs(fx - x)};
	}
	{
		int k = qRound((fx + fy) / 4) * 4;
		int x = qRound((fx - fy + k) / 2);
		list << Candidate{QPoint(x, k - x), Slash, qAbs(fx + fy - k) / M_SQRT2};
	}
	{
		int k = qRound((fy - fx) / 4) * 4;
		int x = qRound((fx + fy - k) / 2);
		list << Candidate{QPoint(x, x + k), Backslash, qAbs(fy - fx - k) / M_SQRT2};
	}
	const Candidate* best = nullptr;
	for(const Candidate& c : list)
		if(!best || c.dist < best->dist)
			best = &c;
	if(!best || best->dist * CellPx / 4.0 > 9)
		return false;
	door = Door{m_Deck, best->at, best->line, false};
	door.wide = WideTool();
	door.opening = OpeningTool();
	return true;
}

bool GridView::RailingAtPx(const QPoint& px, Railing& railing) const
{
	double fx = double(px.x() - Margin) / (CellPx / 4.0), fy = double(px.y() - Margin) / (CellPx / 4.0);
	struct Candidate { QPoint at; Line line; double dist; };
	QList<Candidate> list;
	{
		int y = qRound(fy / 4) * 4, x = qRound((fx - 2) / 4) * 4 + 2;
		list << Candidate{QPoint(x, y), Horizontal, qAbs(fy - y) + qAbs(fx - x) * 0.1};
	}
	{
		int x = qRound(fx / 4) * 4, y = qRound((fy - 2) / 4) * 4 + 2;
		list << Candidate{QPoint(x, y), Vertical, qAbs(fx - x) + qAbs(fy - y) * 0.1};
	}
	{
		int k = qRound((fx + fy) / 4) * 4;
		int x = qRound(((fx - fy + k) / 2 - 1) / 2) * 2 + 1;
		list << Candidate{QPoint(x, k - x), Slash, qAbs(fx + fy - k) / M_SQRT2};
	}
	{
		int k = qRound((fy - fx) / 4) * 4;
		int x = qRound(((fx + fy - k) / 2 - 1) / 2) * 2 + 1;
		list << Candidate{QPoint(x, x + k), Backslash, qAbs(fy - fx - k) / M_SQRT2};
	}
	const Candidate* best = nullptr;
	for(const Candidate& c : list)
		if(!best || c.dist < best->dist)
			best = &c;
	if(!best || best->dist * CellPx / 4.0 > 9)
		return false;
	railing = Railing{m_Deck, best->at, best->line, QString()};
	return true;
}

bool GridView::GapAtPx(const QPoint& px, RailingGap& gap) const
{
	Door door;
	if(!DoorAtPx(px, door))
		return false;
	gap = RailingGap{m_Deck, door.at, door.line};
	return true;
}

int GridView::FindGapAt(const QPoint& px) const
{
	RailingGap probe;
	if(!GapAtPx(px, probe))
		return -1;
	const double x = m_Plan.QX(probe.at.x()), y = m_Plan.QY(probe.at.y());
	for(int i = 0; i < m_Plan.gaps.size(); i++)
	{
		const RailingGap& g = m_Plan.gaps[i];
		if(g.deck != m_Deck || g.line != probe.line)
			continue;
		if(g.at == probe.at)
			return i;
		DoorPlace gp = SimpleMode::PlaceGap(m_Plan, g);
		double dx = x - gp.mx, dy = y - gp.my;
		if(gp.ok && qAbs(dx * gp.uy - dy * gp.ux) < 1e-6 && qAbs(dx * gp.ux + dy * gp.uy) <= GapWidth(m_Plan, g) / 2 + 1e-6)
			return i;
	}
	return -1;
}

void GridView::ClickGap(const QPoint& px)
{
	int i = FindGapAt(px);
	if(i >= 0)
	{
		emit EditStarting();
		m_Plan.gaps.removeAt(i);
		emit Status(tr("Gap closed: the railing runs on through."));
		emit PlanChanged();
		update();
		return;
	}
	RailingGap gap;
	if(!GapAtPx(px, gap))
		return;
	DoorPlace place = SimpleMode::PlaceGap(m_Plan, gap);
	if(!place.ok)
	{
		emit Status(place.problem);
		return;
	}
	emit EditStarting();
	m_Plan.gaps << gap;
	emit Status(tr("Gap in the railing, a doorway wide. Right-click it to take the railing away along the whole side; click it again "
		"to close it."));
	emit PlanChanged();
	update();
}

void GridView::FlipGapWidth(const QPoint& px)
{
	int i = FindGapAt(px);
	if(i < 0)
	{
		emit Status(tr("Right-click a gap to switch it between a doorway's width and the whole side."));
		return;
	}
	RailingGap g = m_Plan.gaps[i];
	g.full = !g.full;
	if(g.full)
	{
		int x = g.at.x(), y = g.at.y();
		auto down = [](int v, int step) { return v >= 0 ? v / step * step : -((-v + step - 1) / step) * step; };
		if(g.line == Horizontal)
			x = down(x, 4) + 2;
		else if(g.line == Vertical)
			y = down(y, 4) + 2;
		else if(x % 2 == 0)
		{
			x += 1;
			y += g.line == Slash ? -1 : 1;
		}
		g.at = QPoint(x, y);
	}
	DoorPlace place = SimpleMode::PlaceGap(m_Plan, g);
	if(!place.ok)
	{
		emit Status(place.problem);
		return;
	}
	emit EditStarting();
	m_Plan.gaps[i] = g;
	emit Status(g.full ? tr("Gap along the whole side.") : tr("Gap a doorway wide."));
	emit PlanChanged();
	update();
}

int GridView::FindRailing(const Railing& railing) const
{
	for(int i = 0; i < m_Plan.railings.size(); i++)
	{
		const Railing& r = m_Plan.railings[i];
		if(r.deck == railing.deck && r.at == railing.at && r.line == railing.line)
			return i;
	}
	return -1;
}

int GridView::StairAt(const QPoint& cell) const
{
	for(int i = 0; i < m_Plan.stairs.size(); i++)
		if(m_Plan.stairs[i].deck == m_Deck && LayoutStair(m_Plan, m_Plan.stairs[i]).cells.contains(cell))
			return i;
	return -1;
}

int GridView::SelectedStair() const
{
	if(!m_HasStairSelection)
		return -1;
	for(int i = 0; i < m_Plan.stairs.size(); i++)
		if(m_Plan.stairs[i].deck == m_StairSelection.deck && m_Plan.stairs[i].cell == m_StairSelection.cell)
			return i;
	return -1;
}

void GridView::SelectStair(int index)
{
	m_HasStairSelection = index >= 0 && index < m_Plan.stairs.size();
	if(m_HasStairSelection)
		m_StairSelection = m_Plan.stairs[index];
	emit StairSelected(SelectedStair());
	update();
}

bool GridView::StairFits(const Stair& stair, int ignore, QString& problem) const
{
	StairLayout layout = LayoutStair(m_Plan, stair);
	if(!layout.ok)
	{
		problem = layout.problem;
		return false;
	}
	for(int i = 0; i < m_Plan.stairs.size(); i++)
	{
		if(i == ignore || m_Plan.stairs[i].deck != stair.deck)
			continue;
		for(const QPoint& c : LayoutStair(m_Plan, m_Plan.stairs[i]).cells)
			if(layout.cells.contains(c))
			{
				problem = tr("It would run into another stair.");
				return false;
			}
	}
	return true;
}

void GridView::PlaceStairAt(const QPoint& cell)
{
	int i = StairAt(cell);
	if(i >= 0)
	{
		SelectStair(i);
		return;
	}
	Stair stair;
	stair.deck = m_Deck;
	stair.cell = cell;
	stair.kind = m_Tool - PlaceStairStraight;
	QString first;
	for(int k = 0; k < 4; k++)
	{
		stair.dir = k;
		QString problem;
		if(StairFits(stair, -1, problem))
		{
			emit EditStarting();
			m_Plan.stairs << stair;
			emit Status(tr("%1 stair up to deck %2. Right-click it to turn it; its style is in the Stair panel.")
				.arg(StairKindNames().value(stair.kind)).arg(m_Deck + 1));
			emit PlanChanged();
			SelectStair(m_Plan.stairs.size() - 1);
			return;
		}
		if(first.isEmpty())
			first = problem;
	}
	emit Status(first);
}

void GridView::TurnStair(const QPoint& cell)
{
	int i = StairAt(cell);
	if(i < 0)
	{
		emit Status(tr("Right-click a stair to turn it."));
		return;
	}
	TurnStairIndex(i);
}

void GridView::TurnStairIndex(int i)
{
	if(i < 0 || i >= m_Plan.stairs.size())
		return;
	Stair turned = m_Plan.stairs[i];
	for(int k = 1; k < 4; k++)
	{
		turned.dir = (m_Plan.stairs[i].dir + k) % 4;
		QString problem;
		if(StairFits(turned, i, problem))
		{
			emit EditStarting();
			m_Plan.stairs[i] = turned;
			static const char* sides[] = {"north", "east", "south", "west"};
			emit Status(tr("The stair climbs %1 first.").arg(sides[turned.dir]));
			emit PlanChanged();
			SelectStair(i);
			return;
		}
	}
	emit Status(tr("It fits no other way here."));
}

int GridView::SelectedRailing() const
{
	return m_HasRailingSelection ? FindRailing(m_RailingSelection) : -1;
}

void GridView::SelectRailing(int index)
{
	m_HasRailingSelection = index >= 0 && index < m_Plan.railings.size();
	if(m_HasRailingSelection)
		m_RailingSelection = m_Plan.railings[index];
	emit RailingSelected(SelectedRailing());
	update();
}

void GridView::ToggleRailing(const Railing& railing)
{
	int i = FindRailing(railing);
	if(i >= 0)
	{
		SelectRailing(i);
		return;
	}
	DoorPlace place = SimpleMode::PlaceRailing(m_Plan, railing);
	if(!place.ok)
	{
		emit Status(place.problem);
		return;
	}
	const double half = RailingLength(m_Plan, railing) / 2;
	for(const Door& d : m_Plan.doors)
	{
		DoorPlace o = SimpleMode::PlaceDoor(m_Plan, d);
		double dx = o.mx - place.mx, dy = o.my - place.my;
		if(d.deck == railing.deck && o.ok && qAbs(dx * place.uy - dy * place.ux) < 1e-6 && qAbs(dx * place.ux + dy * place.uy) < half + DoorWidth(m_Plan, d) / 2)
		{
			emit Status(tr("There is a doorway in that wall."));
			return;
		}
	}
	for(const Window& w : m_Plan.windows)
	{
		DoorPlace o = SimpleMode::PlaceWindow(m_Plan, w);
		double dx = o.mx - place.mx, dy = o.my - place.my;
		if(w.deck == railing.deck && o.ok && qAbs(dx * place.uy - dy * place.ux) < 1e-6
				&& qAbs(dx * place.ux + dy * place.uy) < half + WindowWidth(m_Plan, w) / 2)
		{
			emit Status(tr("There is a window in that wall."));
			return;
		}
	}
	emit EditStarting();
	m_Plan.railings << railing;
	emit Status(place.here == place.there
		? tr("Railing, standing on its own. Its style is in the Railing panel; right-click it for the next style.")
		: tr("Railing in place of the wall. Its style is in the Railing panel; right-click it for the next style."));
	emit PlanChanged();
	SelectRailing(m_Plan.railings.size() - 1);
}

void GridView::CycleRailingStyle(const Railing& railing)
{
	int i = FindRailing(railing);
	if(i < 0)
	{
		emit Status(tr("Right-click a railing to change its style."));
		return;
	}
	emit EditStarting();
	Railing& r = m_Plan.railings[i];
	QString current = r.style.isEmpty() ? DefaultRailingStyle(m_Plan.theme) : r.style;
	const QList<RailingStyle>& styles = RailingStyles();
	int k = 0;
	while(k < styles.size() && styles[k].key != current)
		k++;
	r.style = styles[(k + 1) % styles.size()].key;
	emit Status(tr("Railing: %1.").arg(styles[(k + 1) % styles.size()].name));
	emit PlanChanged();
	SelectRailing(i);
}

int GridView::FindDoor(const Door& door) const
{
	for(int i = 0; i < m_Plan.doors.size(); i++)
		if(SameDoor(m_Plan.doors[i], door))
			return i;
	return -1;
}

bool GridView::WindowAtPx(const QPoint& px, Window& window) const
{
	Door door;
	if(!DoorAtPx(px, door))
		return false;
	window = Window{m_Deck, door.at, door.line, QString(), 0};
	return true;
}

int GridView::FindWindow(const Window& window) const
{
	for(int i = 0; i < m_Plan.windows.size(); i++)
		if(SameWindow(m_Plan.windows[i], window))
			return i;
	return -1;
}

QString GridView::Describe(const QPoint& cell, int quarter) const
{
	static const char* quarterNames[] = {"north", "east", "south", "west"};
	QChar ch = m_Plan.Quarter(m_Deck, cell, quarter);
	QString where = tr("Deck %1, column %2, row %3").arg(m_Deck).arg(cell.x()).arg(cell.y());
	if(m_Plan.At(m_Deck, cell) == '*')
		where += tr(", %1 quarter").arg(quarterNames[quarter]);
	where += ": ";
	if(ch == '.')
		return where + tr("empty");
	if(!m_Plan.rooms.contains(ch))
		return where + tr("room %1 (not in the room list)").arg(ch);
	const Room& room = m_Plan.rooms[ch];
	const RoomKind* kind = FindRoomKind(room.type);
	QString text = where + (kind && kind->name != room.name ? QString("%1 (%2)").arg(room.name).arg(kind->name) : room.name);
	for(const Hatch& h : m_Plan.hatches)
	{
		if(h.cell != cell)
			continue;
		if(h.roof)
		{
			if(h.deck == m_Deck)
				text += tr(", hull hatch up onto the roof");
		}
		else if(h.deck == m_Deck)
			text += tr(", hatch down to deck %1").arg(m_Deck - 1);
		else if(h.deck == m_Deck + 1)
			text += tr(", hatch up to deck %1").arg(m_Deck + 1);
	}
	return text;
}

void GridView::Paint(const QPoint& cell, int quarter)
{
	QChar ch = m_Tool == Erase ? QChar('.') : m_Room;
	if(ch.isNull())
	{
		emit Status(tr("That is as many rooms as a building can have."));
		return;
	}
	QList<QPair<QPoint, int>> shape = ShapeAt(m_Plan, cell, quarter, m_Lines);
	const Room& painting = m_Plan.rooms.contains(ch) ? m_Plan.rooms[ch] : m_PendingRoom;
	if(ch != '.' && painting.type == "turbolift")
	{
		QList<QPair<QPoint, int>> whole;
		for(const QPair<QPoint, int>& s : shape)
			for(int q = 0; q < 4; q++)
				if(!whole.contains(qMakePair(s.first, q)))
					whole << qMakePair(s.first, q);
		shape = whole;
	}
	bool any = false;
	for(const QPair<QPoint, int>& s : shape)
		any = any || m_Plan.Quarter(m_Deck, s.first, s.second) != ch;
	if(!any)
		return;
	Plan before = m_Plan;
	if(ch != '.' && !m_Plan.rooms.contains(ch))
		m_Plan.rooms.insert(ch, m_PendingRoom);
	auto roomName = [&](QChar letter) { return m_Plan.rooms.contains(letter) ? m_Plan.rooms[letter].name : QString(letter); };
	QSet<QChar> touched;
	for(const QPair<QPoint, int>& s : shape)
	{
		touched.insert(m_Plan.Quarter(m_Deck, s.first, s.second));
		m_Plan.SetQuarter(m_Deck, s.first, s.second, ch);
	}
	touched.insert(ch);
	for(QChar room : touched)
	{
		QString problem;
		if(room != '.' && !RoomShapeOk(m_Plan, room, problem))
		{
			QString name = roomName(room);
			m_Plan = before;
			emit Status(room == ch && m_Tool == PaintRoom
				? tr(IsLiftRoom(m_Plan, room) ? "%1 is one connected piece on each deck. Paint next to its cells, or change the room tag for another %2."
					: "%1 is one connected piece on one deck. Paint next to its cells, or change the room tag for another %2.")
					.arg(name).arg(FindRoomKind(m_PendingRoom.type) ? FindRoomKind(m_PendingRoom.type)->name : QString())
				: tr("That would cut %1 in two. A room is one connected piece.").arg(name));
			update();
			return;
		}
	}
	if(ch != '.' && m_Plan.rooms.contains(ch) && m_Plan.rooms[ch].type == "opentobelow")
	{
		QChar below;
		bool ok = true;
		for(int r = 0; r < m_Plan.Rows() && ok; r++)
			for(int c = 0; c < m_Plan.Cols() && ok; c++)
				for(int q = 0; q < 4 && ok; q++)
				{
					QPoint cell(c, r);
					if(m_Plan.Quarter(m_Deck, cell, q) != ch)
						continue;
					ok = IsOpenQuarter(m_Plan, m_Deck, cell, q) && (below.isNull() || SpaceRoom(m_Plan, m_Deck, cell, q) == below);
					if(ok)
						below = SpaceRoom(m_Plan, m_Deck, cell, q);
				}
		if(!ok)
		{
			m_Plan = before;
			emit Status(tr("Open to Deck Below goes over one room on the deck below, in whole cells, halves or quarters."));
			update();
			return;
		}
	}
	TidyPlan(m_Plan);
	emit PlanChanged();
	update();
}

void GridView::RemoveRoom(QChar letter)
{
	if(letter == '.' || !m_Plan.rooms.contains(letter))
		return;
	emit EditStarting();
	QString name = m_Plan.rooms[letter].name;
	for(QStringList& grid : m_Plan.decks)
		for(QString& row : grid)
			row.replace(letter, '.');
	TidyPlan(m_Plan);
	emit Status(tr("%1 removed.").arg(name));
	emit PlanChanged();
	update();
}

void GridView::EraseDetailAt(const QPoint& px)
{
	Door door;
	int i = DoorAtPx(px, door) ? FindDoor(door) : -1;
	if(i >= 0)
	{
		emit EditStarting();
		m_Plan.doors.removeAt(i);
		emit PlanChanged();
		update();
		return;
	}
	i = FindGapAt(px);
	if(i >= 0)
	{
		emit EditStarting();
		m_Plan.gaps.removeAt(i);
		emit PlanChanged();
		update();
		return;
	}
	Railing railing;
	i = RailingAtPx(px, railing) ? FindRailing(railing) : -1;
	if(i >= 0)
	{
		emit EditStarting();
		m_Plan.railings.removeAt(i);
		emit PlanChanged();
		update();
		return;
	}
	Window window;
	i = WindowAtPx(px, window) ? FindWindow(window) : -1;
	if(i >= 0)
	{
		emit EditStarting();
		m_Plan.windows.removeAt(i);
		emit PlanChanged();
		update();
		return;
	}
	QPoint cell;
	if(!CellAt(px, cell))
		return;
	if(FixtureAt(cell, QString(), North) >= 0)
	{
		emit EditStarting();
		m_Plan.fixtures.removeAt(FixtureAt(cell, QString(), North));
		emit PlanChanged();
		update();
		return;
	}
	if(StationAt(cell) >= 0)
	{
		emit EditStarting();
		m_Plan.stations.removeAt(StationAt(cell));
		emit PlanChanged();
		update();
		return;
	}
	for(int k = 0; k < m_Plan.posts.size(); k++)
		if(m_Plan.posts[k].deck == m_Deck && m_Plan.posts[k].cell == cell)
		{
			emit EditStarting();
			m_Plan.posts.removeAt(k);
			emit PlanChanged();
			update();
			return;
		}
	for(int k = 0; k < m_Plan.hatches.size(); k++)
		if(m_Plan.hatches[k].deck == m_Deck && m_Plan.hatches[k].cell == cell)
		{
			emit EditStarting();
			m_Plan.hatches.removeAt(k);
			emit PlanChanged();
			update();
			return;
		}
	int stairIndex = StairAt(cell);
	if(stairIndex >= 0)
	{
		emit EditStarting();
		m_Plan.stairs.removeAt(stairIndex);
		emit PlanChanged();
		update();
		return;
	}
	int dormerIndex = DormerAt(cell);
	if(dormerIndex >= 0)
	{
		emit EditStarting();
		m_Plan.dormers.removeAt(dormerIndex);
		emit PlanChanged();
		update();
		return;
	}
	int skylightIndex = SkylightIn(cell);
	if(skylightIndex >= 0)
	{
		emit EditStarting();
		m_Plan.skylights.removeAt(skylightIndex);
		emit PlanChanged();
		update();
		return;
	}
	if(m_Plan.RoofAt(m_Deck, cell) != '.')
	{
		emit EditStarting();
		m_Plan.SetRoof(m_Deck, cell, '.');
		TidyPlan(m_Plan);
		emit PlanChanged();
		update();
		return;
	}
	emit Status(tr("Nothing to erase here. Click a door, window, railing, hatch, post, stair, dormer, skylight or roof."));
}

bool GridView::DoorKindFits(const DoorPlace& place, QString& problem) const
{
	if(!place.ok)
	{
		problem = place.problem;
		return false;
	}
	if(HullTool() && !SimpleMode::IsOutsideWall(m_Plan, place))
	{
		problem = tr("A hull door goes in an outside wall. Use Door between two rooms.");
		return false;
	}
	if(!HullTool() && place.there == '.')
	{
		problem = tr("This is an outside wall. Use Hull Door for a way in.");
		return false;
	}
	return true;
}

void GridView::ToggleDoor(const Door& door)
{
	int i = FindDoor(door);
	if(i >= 0)
	{
		SelectDoor(i);
		return;
	}
	DoorPlace place = SimpleMode::PlaceDoor(m_Plan, door);
	QString problem;
	if(!DoorKindFits(place, problem))
	{
		emit Status(problem);
		return;
	}
	for(const Door& d : m_Plan.doors)
	{
		if(d.deck != door.deck || d.line != door.line)
			continue;
		DoorPlace other = SimpleMode::PlaceDoor(m_Plan, d);
		double dx = other.mx - place.mx, dy = other.my - place.my;
		bool sameLine = qAbs(dx * place.uy - dy * place.ux) < 1e-6;
		double spacing = (DoorWidth(m_Plan, d) + DoorWidth(m_Plan, door)) / 2 + SWITCH_ROOM;
		if(sameLine && qSqrt(dx * dx + dy * dy) < spacing)
		{
			emit Status(tr("Too close to another door: doors on one wall need %1 m of wall between them, for their switches.")
				.arg(SWITCH_ROOM));
			return;
		}
	}
	Plan with = m_Plan;
	with.doors << door;
	for(const Window& w : m_Plan.windows)
	{
		QString problem;
		if(w.deck == door.deck && !WindowClear(with, w, problem))
		{
			emit Status(tr("Too close to a window: doors keep clear of windows, for their switches. Move or remove the window first."));
			return;
		}
	}
	for(const Stair& st : m_Plan.stairs)
		if(LayoutStair(m_Plan, st).ok && !LayoutStair(with, st).ok)
		{
			emit Status(LayoutStair(with, st).problem);
			return;
		}
	emit EditStarting();
	m_Plan.doors << door;
	emit PlanChanged();
	SelectDoor(m_Plan.doors.size() - 1);
}

void GridView::ToggleAutomatic(const Door& door)
{
	int i = FindDoor(door);
	if(i < 0)
	{
		emit Status(tr("Right-click a door to switch it between manual and automatic."));
		return;
	}
	Door& d = m_Plan.doors[i];
	DoorPlace place = SimpleMode::PlaceDoor(m_Plan, d);
	if(place.there == '.')
	{
		emit Status(tr("Doors to the outside are always manual."));
		return;
	}
	emit EditStarting();
	d.automatic = !d.automatic;
	emit Status(d.automatic ? tr("Automatic door: it opens whenever someone comes near.") : tr("Manual door: open and close it with its switches."));
	emit PlanChanged();
	update();
}

void GridView::ToggleHatch(const QPoint& cell)
{
	if(m_Deck < 1)
	{
		emit Status(tr("A hatch goes in the floor of an upper deck, down to the deck below. Go up a deck first."));
		return;
	}
	if(!WholeRoom(m_Plan, m_Deck, cell) || !WholeRoom(m_Plan, m_Deck - 1, cell))
	{
		emit Status(tr("A hatch needs a whole room cell here and on the same cell of the deck below."));
		return;
	}
	if(IsLiftRoom(m_Plan, m_Plan.At(m_Deck, cell)) || IsLiftRoom(m_Plan, m_Plan.At(m_Deck - 1, cell)))
	{
		emit Status(tr("A turbo lift's shaft has its car in it: no hatch."));
		return;
	}
	for(int i = 0; i < m_Plan.hatches.size(); i++)
		if(m_Plan.hatches[i].deck == m_Deck && m_Plan.hatches[i].cell == cell && !m_Plan.hatches[i].roof)
		{
			SelectHatch(i);
			return;
		}
	emit EditStarting();
	m_Plan.hatches << Hatch{m_Deck, cell, QString(), QString()};
	emit Status(tr("Hatch with a ladder down to deck %1. Its style and ladder are in the Hatch panel.").arg(m_Deck - 1));
	emit PlanChanged();
	SelectHatch(m_Plan.hatches.size() - 1);
}

void GridView::ToggleRoofHatch(const QPoint& cell)
{
	for(int i = 0; i < m_Plan.hatches.size(); i++)
		if(m_Plan.hatches[i].deck == m_Deck && m_Plan.hatches[i].cell == cell && m_Plan.hatches[i].roof)
		{
			SelectHatch(i);
			return;
		}
	Hatch h{m_Deck, cell, QString(), QString()};
	h.roof = true;
	QString problem = RoofHatchProblem(m_Plan, h);
	if(!problem.isEmpty())
	{
		emit Status(problem);
		return;
	}
	emit EditStarting();
	m_Plan.hatches << h;
	emit Status(tr("Hull hatch with a ladder up onto the roof. Its style and ladder are in the Hatch panel."));
	emit PlanChanged();
	SelectHatch(m_Plan.hatches.size() - 1);
}

void GridView::ToggleWindow(const Window& window)
{
	int i = FindWindow(window);
	if(i >= 0)
	{
		SelectWindow(i);
		return;
	}
	DoorPlace place = SimpleMode::PlaceWindow(m_Plan, window);
	QString problem;
	if(!place.ok || !WindowClear(m_Plan, window, problem))
	{
		emit Status(place.ok ? problem : place.problem);
		return;
	}
	emit EditStarting();
	m_Plan.windows << window;
	emit Status(place.there == '.' ? tr("Window to the outside. Its shape and how it is built are in the Window panel.")
		: tr("Window between two rooms. Its shape and how it is built are in the Window panel."));
	emit PlanChanged();
	SelectWindow(m_Plan.windows.size() - 1);
}

void GridView::CycleWindowTech(const Window& window)
{
	int i = FindWindow(window);
	if(i < 0)
	{
		emit Status(tr("Right-click a window to change how it is built."));
		return;
	}
	emit EditStarting();
	Window& w = m_Plan.windows[i];
	int tech = w.tech ? w.tech : DefaultWindowTech(m_Plan.theme);
	w.tech = tech % WindowTechNames().size() + 1;
	emit Status(tr("Window: %1.").arg(WindowTechNames()[w.tech - 1]));
	emit PlanChanged();
	SelectWindow(i);
}

void GridView::PaintRoof(const QPoint& cell)
{
	const QString style = m_Tool == RoofHip ? "hip" : m_Tool == RoofRound ? "round" : "gable";
	if(!CanRoof(m_Plan, m_Deck, cell))
	{
		if(!m_RoofStroke.isNull())
			return;
		emit Status(tr("A roof goes over a whole room cell with nothing built on it above."));
		return;
	}
	QChar there = m_Plan.RoofAt(m_Deck, cell);
	if(m_RoofStroke.isNull())
	{
		if(there != '.')
		{
			SelectRoof(there);
			return;
		}
		for(int s = 0; s < 4 && m_RoofStroke.isNull(); s++)
		{
			QChar next = m_Plan.RoofAt(m_Deck, Neighbour(cell, Side(s)));
			if(next != '.' && m_Plan.roofs.contains(next) && m_Plan.roofs[next].style == style)
				m_RoofStroke = next;
		}
		emit EditStarting();
		if(m_RoofStroke.isNull())
			m_RoofStroke = NewRoof(m_Plan, style);
		if(m_RoofStroke.isNull())
		{
			emit Status(tr("That is as many roofs as a building can have."));
			emit EditRefused();
			return;
		}
	}
	if(there == m_RoofStroke)
		return;
	m_Plan.SetRoof(m_Deck, cell, m_RoofStroke);
	TidyPlan(m_Plan);
	m_RoofSelection = m_RoofStroke;
	emit PlanChanged();
	emit RoofSelected(m_RoofSelection);
	update();
}

void GridView::TurnRoof(const QPoint& cell)
{
	QChar letter = m_Plan.RoofAt(m_Deck, cell);
	if(letter == '.' || !m_Plan.roofs.contains(letter))
	{
		emit Status(tr("Right-click a roof to turn it."));
		return;
	}
	Roof& roof = m_Plan.roofs[letter];
	const RoofStyle* style = FindRoofStyle(roof.style);
	if(!style || (!style->ridged && !style->sloped))
	{
		emit Status(tr("A %1 roof looks the same whichever way it is turned.").arg(style ? style->name.toLower() : roof.style));
		return;
	}
	emit EditStarting();
	static const char* sides[] = {"north", "east", "south", "west"};
	if(style->ridged)
	{
		roof.dir = roof.dir == East || roof.dir == West ? North : East;
		emit Status(roof.dir == North ? tr("The ridge runs north and south.") : tr("The ridge runs east and west."));
	}
	else
	{
		roof.dir = (roof.dir + 1) % 4;
		emit Status(tr("The roof slopes down to the %1.").arg(sides[roof.dir]));
	}
	emit PlanChanged();
	SelectRoof(letter);
}

QString GridView::FixtureToolType() const
{
	switch(m_Tool)
	{
	case PlaceBerth: return "berth";
	case PlaceParking: return "parking";
	case PlaceLight: return "light";
	case PlaceLightPole: return "lightpole";
	case PlaceFlagPole: return "flagpole";
	case PlaceGameTable: return "gametable";
	case PlaceTurret: return "turret";
	default: return QString();
	}
}

Fixture& GridView::FixtureTemplate(const QString& type)
{
	if(!m_FixtureTemplates.contains(type))
	{
		Fixture f{0, QPoint()};
		f.type = type;
		m_FixtureTemplates.insert(type, f);
	}
	return m_FixtureTemplates[type];
}

int GridView::FixtureAt(const QPoint& cell, const QString& type, int dir) const
{
	for(int i = 0; i < m_Plan.fixtures.size(); i++)
	{
		const Fixture& f = m_Plan.fixtures[i];
		if(f.deck != m_Deck || (f.type == "turret" ? !TurretCells(f).contains(cell) : f.cell != cell))
			continue;
		if(type == "light" ? (f.type == "light" && f.dir == dir) : (type.isEmpty() || f.type == type))
			return i;
	}
	return -1;
}

void GridView::ClickFixture(const QPoint& px)
{
	QPoint cell;
	if(!CellAt(px, cell))
		return;
	const QString type = FixtureToolType();
	int dir = North;
	if(type == "light")
	{
		double fx = double(px.x() - Margin - cell.x() * CellPx) / CellPx, fy = double(px.y() - Margin - cell.y() * CellPx) / CellPx;
		dir = qAbs(fx - 0.5) < 0.2 && qAbs(fy - 0.5) < 0.2 ? FixtureTop : SideAtPx(px, cell);
	}
	int i = FixtureAt(cell, type, dir);
	if(i >= 0)
	{
		SelectFixture(i);
		const FixtureType* t = FindFixtureType(m_Plan.fixtures[i].type);
		emit Status(tr("%1. Its settings are in the Fixture panel; right-click it to turn it, Erase equipment removes it.")
			.arg(t ? t->name : QString()));
		return;
	}
	Fixture f = FixtureTemplate(type);
	f.deck = m_Deck;
	f.cell = cell;
	if(type == "light")
		f.dir = dir;
	QString problem = FixtureProblem(m_Plan, f);
	if(!problem.isEmpty())
	{
		emit Status(problem);
		return;
	}
	emit EditStarting();
	m_Plan.fixtures << f;
	const FixtureType* t = FindFixtureType(type);
	emit Status(tr("%1 placed. Right-click it to turn it; the Fixture panel changes it.").arg(t ? t->name : type));
	emit PlanChanged();
	SelectFixture(m_Plan.fixtures.size() - 1);
	update();
}

void GridView::TurnFixture(const QPoint& px)
{
	QPoint cell;
	if(!CellAt(px, cell))
		return;
	const QString type = FixtureToolType();
	int i = -1;
	for(int k = 0; k < m_Plan.fixtures.size() && i < 0; k++)
		if(m_Plan.fixtures[k].deck == m_Deck && m_Plan.fixtures[k].type == type
				&& (type == "turret" ? TurretCells(m_Plan.fixtures[k]).contains(cell) : m_Plan.fixtures[k].cell == cell))
			i = k;
	if(i < 0)
	{
		emit Status(tr("Right-click one to turn it."));
		return;
	}
	Fixture f = m_Plan.fixtures[i];
	const FixtureType* t = FindFixtureType(f.type);
	if(f.type == "light" || f.type == "lightpole")
	{
		int k = t->kinds.indexOf(f.kind.isEmpty() ? t->kinds.first() : f.kind);
		f.kind = t->kinds[(k + 1) % t->kinds.size()];
		if(f.kind == t->kinds.first())
			f.kind.clear();
	}
	else
		f.dir = (f.dir + 1) % 4;
	QString problem = FixtureProblem(m_Plan, f, i);
	if(!problem.isEmpty())
	{
		emit Status(problem);
		return;
	}
	emit EditStarting();
	m_Plan.fixtures[i] = f;
	emit PlanChanged();
	SelectFixture(i);
	update();
}

int GridView::SelectedFixture() const
{
	if(!m_HasFixtureSelection)
		return -1;
	for(int i = 0; i < m_Plan.fixtures.size(); i++)
	{
		const Fixture& f = m_Plan.fixtures[i];
		if(f.deck == m_FixtureSelection.deck && f.cell == m_FixtureSelection.cell && f.type == m_FixtureSelection.type
				&& (f.type != "light" || f.dir == m_FixtureSelection.dir))
			return i;
	}
	return -1;
}

void GridView::SelectFixture(int index)
{
	m_HasFixtureSelection = index >= 0 && index < m_Plan.fixtures.size();
	if(m_HasFixtureSelection)
		m_FixtureSelection = m_Plan.fixtures[index];
	emit FixtureSelected(SelectedFixture());
	update();
}

int GridView::StationAt(const QPoint& cell) const
{
	for(int i = 0; i < m_Plan.stations.size(); i++)
		if(m_Plan.stations[i].deck == m_Deck && m_Plan.stations[i].cell == cell)
			return i;
	return -1;
}

void GridView::ClickStation(const QPoint& px)
{
	QPoint cell;
	if(!CellAt(px, cell))
		return;
	int i = StationAt(cell);
	if(i >= 0)
	{
		SelectStation(i);
		const StationKind* k = FindStationKind(m_Plan.stations[i].kind);
		emit Status(tr("%1 %2. Its settings are in the Station panel; right-click it to turn it, Erase equipment removes it.")
			.arg(k ? k->name : QString()).arg(m_Plan.stations[i].wall ? tr("screen on the wall") : tr("console")));
		return;
	}
	Station st = m_StationTemplate;
	st.deck = m_Deck;
	st.cell = cell;
	st.pads.clear();
	if(st.wall)
		st.dir = SideAtPx(px, cell);
	QString problem = StationProblem(m_Plan, st);
	if(!problem.isEmpty())
	{
		emit Status(problem);
		return;
	}
	emit EditStarting();
	m_Plan.stations << st;
	const StationKind* k = FindStationKind(st.kind);
	emit Status(tr("%1 %2: whoever is at it, in this room, works it. Right-click it to turn it; the Station panel changes it.")
		.arg(k ? k->name : QString()).arg(st.wall ? tr("screen on the wall") : tr("console")));
	emit PlanChanged();
	SelectStation(m_Plan.stations.size() - 1);
	update();
}

void GridView::TurnStation(const QPoint& cell)
{
	int i = StationAt(cell);
	if(i < 0)
	{
		emit Status(tr("Right-click a station to turn it."));
		return;
	}
	Station st = m_Plan.stations[i];
	static const char* names[4] = {"north", "east", "south", "west"};
	for(int k = 1; k <= 3; k++)
	{
		st.dir = (m_Plan.stations[i].dir + k) % 4;
		if(StationProblem(m_Plan, st, i).isEmpty())
		{
			emit EditStarting();
			m_Plan.stations[i] = st;
			emit Status(st.wall ? tr("The screen is on the %1 wall.").arg(names[st.dir]) : tr("The operator faces %1.").arg(names[st.dir]));
			emit PlanChanged();
			update();
			return;
		}
	}
	emit Status(tr("There is no other way round for it here."));
}

int GridView::SelectedStation() const
{
	return m_HasStationSelection ? [&]() {
		for(int i = 0; i < m_Plan.stations.size(); i++)
			if(m_Plan.stations[i].deck == m_StationSelectionDeck && m_Plan.stations[i].cell == m_StationSelectionCell)
				return i;
		return -1;
	}() : -1;
}

void GridView::SelectStation(int index)
{
	m_HasStationSelection = index >= 0 && index < m_Plan.stations.size();
	if(m_HasStationSelection)
	{
		m_StationSelectionDeck = m_Plan.stations[index].deck;
		m_StationSelectionCell = m_Plan.stations[index].cell;
	}
	emit StationSelected(SelectedStation());
	update();
}

void GridView::TogglePost(const QPoint& cell, const QString& kind)
{
	if(!WholeRoom(m_Plan, m_Deck, cell))
	{
		emit Status(tr("Posts go in the middle of a whole room cell."));
		return;
	}
	emit EditStarting();
	for(int i = 0; i < m_Plan.posts.size(); i++)
		if(m_Plan.posts[i].deck == m_Deck && m_Plan.posts[i].cell == cell)
		{
			if(m_Plan.posts[i].kind == kind)
				m_Plan.posts.removeAt(i);
			else
				m_Plan.posts[i].kind = kind;
			emit PlanChanged();
			update();
			return;
		}
	m_Plan.posts << Post{m_Deck, cell, kind};
	emit PlanChanged();
	update();
}

void GridView::mousePressEvent(QMouseEvent* e)
{
	QPoint cell;
	int q;
	Door door;
	if(e->button() == Qt::RightButton)
	{
		if(HatchTool() && CellAt(e->pos(), cell))
		{
			for(int i = 0; i < m_Plan.hatches.size(); i++)
				if(m_Plan.hatches[i].deck == m_Deck && m_Plan.hatches[i].cell == cell
						&& m_Plan.hatches[i].roof == (m_Tool == PlaceHullHatch))
				{
					emit EditStarting();
					m_Plan.hatches[i].side = (m_Plan.hatches[i].side + 1) % 4;
					static const char* names[] = {"north", "east", "south", "west"};
					emit Status(tr("The ladder is on the %1 side of the hatch.").arg(names[m_Plan.hatches[i].side]));
					emit PlanChanged();
					SelectHatch(i);
					return;
				}
			return;
		}
		if(RoofTool())
		{
			if(CellAt(e->pos(), cell))
				TurnRoof(cell);
			return;
		}
		if(StairTool())
		{
			if(CellAt(e->pos(), cell))
				TurnStair(cell);
			return;
		}
		if(m_Tool == PlaceDormer)
		{
			if(CellAt(e->pos(), cell) && DormerAt(cell) >= 0)
				TurnDormerIndex(DormerAt(cell));
			return;
		}
		if(m_Tool == PlaceSkylight)
		{
			if(CellAt(e->pos(), cell))
				CycleSkylight(cell);
			return;
		}
		if(m_Tool == PlaceRailing)
		{
			Railing railing;
			if(RailingAtPx(e->pos(), railing))
				CycleRailingStyle(railing);
			return;
		}
		if(m_Tool == PlaceGap)
		{
			FlipGapWidth(e->pos());
			return;
		}
		if(FixtureTool())
		{
			TurnFixture(e->pos());
			return;
		}
		if(m_Tool == PlaceStation)
		{
			if(CellAt(e->pos(), cell))
				TurnStation(cell);
			return;
		}
		Window window;
		if(m_Tool == PlaceWindow)
		{
			if(WindowAtPx(e->pos(), window))
				CycleWindowTech(window);
			return;
		}
		if(DoorAtPx(e->pos(), door))
			ToggleAutomatic(door);
		return;
	}
	if(e->button() != Qt::LeftButton)
		return;
	switch(m_Tool)
	{
	case PaintRoom:
	case Erase:
		if(QuarterAtPx(e->pos(), cell, q))
		{
			emit EditStarting();
			Plan before = m_Plan;
			m_Painting = true;
			Paint(cell, q);
			if(m_Plan.decks == before.decks)
				emit EditRefused();
		}
		break;
	case DeleteRoom:
		if(QuarterAtPx(e->pos(), cell, q))
			RemoveRoom(m_Plan.Quarter(m_Deck, cell, q));
		break;
	case HullDoor:
	case InteriorDoor:
	case WideHullDoor:
	case WideInteriorDoor:
	case WallOpening:
	case HullOpening:
		if(DoorAtPx(e->pos(), door))
			ToggleDoor(door);
		break;
	case PlaceRailing:
		{
			Railing railing;
			if(RailingAtPx(e->pos(), railing))
				ToggleRailing(railing);
		}
		break;
	case PlaceGap:
		ClickGap(e->pos());
		break;
	case EraseDetail:
		EraseDetailAt(e->pos());
		if(m_HasSelection && SelectedDoor() < 0)
			SelectDoor(-1);
		if(m_HasHatchSelection && SelectedHatch() < 0)
			SelectHatch(-1);
		if(m_HasWindowSelection && SelectedWindow() < 0)
			SelectWindow(-1);
		if(m_HasRailingSelection && SelectedRailing() < 0)
			SelectRailing(-1);
		if(m_HasStairSelection && SelectedStair() < 0)
			SelectStair(-1);
		if(m_HasDormerSelection && SelectedDormer() < 0)
			SelectDormer(-1);
		if(m_HasSkylightSelection && SelectedSkylight() < 0)
			SelectSkylight(-1);
		break;
	case PlaceDormer:
		if(CellAt(e->pos(), cell))
			PlaceDormerAt(cell, SideAtPx(e->pos(), cell));
		break;
	case PlaceSkylight:
		if(CellAt(e->pos(), cell))
			PlaceSkylightAt(cell);
		break;
	case PlaceStairStraight:
	case PlaceStairTurnLeft:
	case PlaceStairTurnRight:
	case PlaceStairULeft:
	case PlaceStairURight:
		if(CellAt(e->pos(), cell))
			PlaceStairAt(cell);
		break;
	case PlaceWindow:
		{
			Window window;
			if(WindowAtPx(e->pos(), window))
				ToggleWindow(window);
		}
		break;
	case RoofGable:
	case RoofHip:
	case RoofRound:
		if(CellAt(e->pos(), cell))
		{
			m_RoofStroke = QChar();
			m_Painting = true;
			PaintRoof(cell);
		}
		break;
	case PlaceConsole:
		{
			int i = SelectedDoor();
			if(i < 0 || !QuarterAtPx(e->pos(), cell, q))
				break;
			if(m_Plan.doors[i].deck != m_Deck || m_Plan.Quarter(m_Deck, cell, q) == '.')
			{
				emit Status(tr("Click inside a room on the door's deck (deck %1) for its console.").arg(m_Plan.doors[i].deck));
				break;
			}
			emit EditStarting();
			Door& d = m_Plan.doors[i];
			d.operation = "console";
			d.console = QPoint(qRound(double(e->pos().x() - Margin) / (CellPx / 4.0)), qRound(double(e->pos().y() - Margin) / (CellPx / 4.0)));
			m_Selection = d;
			emit Status(tr("Console placed. It goes on the nearest wall of %1.").arg(m_Plan.rooms[m_Plan.Quarter(m_Deck, cell, q)].name));
			emit PlanChanged();
			m_Tool = m_ToolBeforeConsole;
			emit ToolChanged(m_Tool);
			emit DoorSelected(i);
			update();
		}
		break;
	case PlaceHatch:
		if(CellAt(e->pos(), cell))
			ToggleHatch(cell);
		break;
	case PlaceHullHatch:
		if(CellAt(e->pos(), cell))
			ToggleRoofHatch(cell);
		break;
	case CitizenPost:
	case TroopPost:
		if(CellAt(e->pos(), cell))
			TogglePost(cell, m_Tool == CitizenPost ? "citizen" : "troop");
		break;
	case PlaceStation:
		ClickStation(e->pos());
		break;
	case PlaceBerth:
	case PlaceParking:
	case PlaceLight:
	case PlaceLightPole:
	case PlaceFlagPole:
	case PlaceGameTable:
	case PlaceTurret:
		ClickFixture(e->pos());
		break;
	}
}

void GridView::mouseMoveEvent(QMouseEvent* e)
{
	QPoint cell;
	int q;
	m_HoverValid = QuarterAtPx(e->pos(), cell, q);
	if(m_HoverValid)
	{
		m_HoverCell = cell;
		m_HoverQuarter = q;
		if(m_Painting && RoofTool())
			PaintRoof(cell);
		else if(m_Painting)
			Paint(cell, q);
	}
	m_HoverDoorValid = (DoorTool() || m_Tool == EraseDetail) && DoorAtPx(e->pos(), m_HoverDoor)
		&& (DoorTool() || FindDoor(m_HoverDoor) >= 0);
	if(m_HoverDoorValid)
	{
		int i = FindDoor(m_HoverDoor);
		if(i >= 0)
			m_HoverDoor.wide = m_Plan.doors[i].wide;
		DoorPlace place = SimpleMode::PlaceDoor(m_Plan, m_HoverDoor);
		QString problem;
		if(i >= 0 && m_Tool == EraseDetail)
			emit Status(tr("Click to remove this door."));
		else if(i >= 0)
			emit Status(place.there == '.' ? tr("Hull door. Click to pick it for the Door panel.")
				: m_Plan.doors[i].automatic ? tr("Automatic door. Click to pick it for the Door panel, right-click to make it manual.")
				: tr("Manual door. Click to pick it for the Door panel, right-click to make it automatic."));
		else if(DoorKindFits(place, problem))
			emit Status(SimpleMode::IsOutsideWall(m_Plan, place) ? tr("Click for a hull door here.") : tr("Click for a door here."));
		else
			emit Status(problem);
	}
	m_HoverRailingValid = m_Tool == PlaceRailing && RailingAtPx(e->pos(), m_HoverRailing);
	if(m_HoverRailingValid)
	{
		int i = FindRailing(m_HoverRailing);
		DoorPlace place = SimpleMode::PlaceRailing(m_Plan, m_HoverRailing);
		emit Status(i >= 0 ? tr("Railing. Click to pick it for the Railing panel, right-click for the next style.")
			: place.ok ? tr("Click for a railing in place of this wall.") : place.problem);
	}
	m_HoverGapValid = m_Tool == PlaceGap && GapAtPx(e->pos(), m_HoverGap);
	if(m_HoverGapValid)
	{
		DoorPlace place = SimpleMode::PlaceGap(m_Plan, m_HoverGap);
		emit Status(FindGapAt(e->pos()) >= 0 ? tr("Gap. Click to close it, right-click to switch it between a doorway's width and the "
			"whole side.") : place.ok ? tr("Click for a gap in the railing here.") : place.problem);
	}
	m_HoverWindowValid = !m_HoverDoorValid && (m_Tool == PlaceWindow || m_Tool == EraseDetail) && WindowAtPx(e->pos(), m_HoverWindow)
		&& (m_Tool == PlaceWindow || FindWindow(m_HoverWindow) >= 0);
	if(m_HoverWindowValid)
	{
		int i = FindWindow(m_HoverWindow);
		if(i >= 0)
			m_HoverWindow = m_Plan.windows[i];
		DoorPlace place = SimpleMode::PlaceWindow(m_Plan, m_HoverWindow);
		QString problem;
		if(i >= 0 && m_Tool == EraseDetail)
			emit Status(tr("Click to remove this window."));
		else if(i >= 0)
		{
			int tech = m_HoverWindow.tech ? m_HoverWindow.tech : DefaultWindowTech(m_Plan.theme);
			emit Status(tr("%1 window. Click to pick it for the Window panel, right-click to build it another way.")
				.arg(WindowTechNames()[tech - 1].section(':', 0, 0)));
		}
		else if(place.ok && WindowClear(m_Plan, m_HoverWindow, problem))
			emit Status(SimpleMode::IsOutsideWall(m_Plan, place) ? tr("Click for a window in the outside wall here.")
				: tr("Click for a window between the rooms here."));
		else
			emit Status(place.ok ? problem : place.problem);
	}
	else if(m_HoverValid && !m_HoverDoorValid && !m_HoverRailingValid && !m_HoverGapValid)
		emit Status(Describe(m_HoverCell, m_HoverQuarter));
	update();
}

void GridView::mouseReleaseEvent(QMouseEvent* e)
{
	if(e->button() == Qt::LeftButton)
	{
		m_Painting = false;
		m_RoofStroke = QChar();
	}
}

void GridView::leaveEvent(QEvent* e)
{
	Q_UNUSED(e);
	m_HoverValid = false;
	m_HoverDoorValid = false;
	m_HoverWindowValid = false;
	m_HoverRailingValid = false;
	m_HoverGapValid = false;
	update();
}
