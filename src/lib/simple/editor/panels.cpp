#include "editorwindow.h"
#include "gridview.h"
#include "palette.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QtMath>

using namespace SimpleMode;

void EditorWindow::RefreshDoor()
{
	int i = m_View->SelectedDoor();
	m_Refreshing = true;
	m_DoorBox->setEnabled(i >= 0);
	if(i < 0)
	{
		m_DoorLabel->setText(tr("Click a door with a door tool to pick it."));
		m_Refreshing = false;
		return;
	}
	const Door& d = m_Plan.doors[i];
	DoorPlace place = PlaceDoor(m_Plan, d);
	bool hull = place.ok && place.there == '.';
	QString here = place.ok && m_Plan.rooms.contains(place.here) ? m_Plan.rooms[place.here].name : QString("?");
	QString there = hull ? tr("outside") : place.ok && m_Plan.rooms.contains(place.there) ? m_Plan.rooms[place.there].name : QString("?");
	m_DoorLabel->setText(tr("<b>%1 %2</b> on deck %3, from %4 to %5").arg(hull ? tr("Hull") : tr("Interior"))
		.arg(d.opening ? tr("opening") : tr("door")).arg(d.deck).arg(here).arg(there));
	m_DoorWide->setChecked(d.wide);
	m_DoorStyle->clear();
	for(QWidget* w : QList<QWidget*>{m_DoorOp, m_PlaceConsole, m_DoorAuto, m_DoorLocked, m_DoorVehicles})
		w->setVisible(!d.opening);
	if(QFormLayout* form = qobject_cast<QFormLayout*>(m_DoorBox->layout()))
	{
		if(QWidget* label = form->labelForField(m_DoorOp))
			label->setVisible(!d.opening);
		if(QLabel* label = qobject_cast<QLabel*>(form->labelForField(m_DoorStyle)))
			label->setText(d.opening ? tr("Head") : tr("Opens"));
	}
	if(d.opening)
	{
		for(const OpeningStyle& s : OpeningStyles())
		{
			m_DoorStyle->addItem(s.name, s.key == "plain" ? QString() : s.key);
			m_DoorStyle->setItemData(m_DoorStyle->count() - 1, s.note, Qt::ToolTipRole);
		}
		m_DoorStyle->setCurrentIndex(qMax(0, m_DoorStyle->findData(d.style == "plain" ? QString() : d.style)));
		m_Refreshing = false;
		return;
	}
	m_DoorStyle->addItem(tr("The theme's usual (%1)").arg(FindDoorStyle(DefaultDoorStyle(m_Plan.theme, hull, d.wide))->name),
		QString());
	for(const DoorStyle& s : DoorStyles())
		if(d.wide ? s.wide : s.single)
		{
			m_DoorStyle->addItem(s.name, s.key);
			m_DoorStyle->setItemData(m_DoorStyle->count() - 1, s.note, Qt::ToolTipRole);
		}
	m_DoorStyle->setCurrentIndex(qMax(0, m_DoorStyle->findData(d.style)));
	m_DoorOp->setCurrentIndex(qMax(0, m_DoorOp->findData(d.operation)));
	m_PlaceConsole->setEnabled(d.operation == "console");
	m_PlaceConsole->setText(d.console == QPoint(-1, -1) ? tr("Place console...") : tr("Move console..."));
	m_DoorAuto->setChecked(d.automatic);
	m_DoorAuto->setEnabled(!hull);
	m_DoorLocked->setChecked(d.locked);
	m_DoorLocked->setEnabled(!hull);
	m_DoorVehicles->setChecked(d.vehicles);
	m_Refreshing = false;
}

void EditorWindow::DoorEdited()
{
	int i = m_View->SelectedDoor();
	if(m_Refreshing || i < 0)
		return;
	if(m_Plan.doors[i].opening)
	{
		Door changed = m_Plan.doors[i];
		changed.style = m_DoorStyle->currentData().toString();
		changed.wide = m_DoorWide->isChecked();
		DoorPlace place = PlaceDoor(m_Plan, changed);
		QString problem = place.problem;
		Plan with = m_Plan;
		with.doors[i] = changed;
		for(const Window& w : m_Plan.windows)
			if(place.ok && !WindowClear(with, w, problem))
				place.ok = false;
		if(!place.ok)
		{
			ShowStatus(tr("That does not fit here: %1").arg(problem), 8000);
			RefreshDoor();
			return;
		}
		PushUndo();
		m_Plan.doors[i] = changed;
		SetModified(true);
		RefreshDoor();
		m_View->update();
		return;
	}
	if(m_DoorWide->isChecked() != m_Plan.doors[i].wide)
	{
		Door wider = m_Plan.doors[i];
		wider.wide = m_DoorWide->isChecked();
		DoorPlace place = PlaceDoor(m_Plan, wider);
		for(int k = 0; place.ok && k < m_Plan.doors.size(); k++)
		{
			const Door& o = m_Plan.doors[k];
			if(k == i || o.deck != wider.deck || o.line != wider.line)
				continue;
			DoorPlace other = PlaceDoor(m_Plan, o);
			double dx = other.mx - place.mx, dy = other.my - place.my;
			if(other.ok && qAbs(dx * place.uy - dy * place.ux) < 1e-6
					&& qSqrt(dx * dx + dy * dy) < (DoorWidth(m_Plan, o) + DoorWidth(m_Plan, wider)) / 2 + 0.6)
			{
				place.ok = false;
				place.problem = tr("it would run into the next door along the wall.");
			}
		}
		Plan with = m_Plan;
		with.doors[i] = wider;
		for(const Window& w : m_Plan.windows)
		{
			QString problem;
			if(place.ok && !WindowClear(with, w, problem))
			{
				place.ok = false;
				place.problem = tr("it would run into a window.");
			}
		}
		if(!place.ok)
		{
			ShowStatus(tr("It does not fit as a %1 door here: %2").arg(wider.wide ? tr("double-width") : tr("single"))
				.arg(place.problem), 8000);
			RefreshDoor();
			return;
		}
	}
	PushUndo();
	Door& d = m_Plan.doors[i];
	bool widthChanged = d.wide != m_DoorWide->isChecked();
	d.wide = m_DoorWide->isChecked();
	d.style = m_DoorStyle->currentData().toString();
	if(widthChanged && FindDoorStyle(d.style) && !(d.wide ? FindDoorStyle(d.style)->wide : FindDoorStyle(d.style)->single))
		d.style.clear();
	d.operation = m_DoorOp->currentData().toString();
	d.automatic = m_DoorAuto->isChecked();
	d.locked = m_DoorLocked->isChecked();
	d.vehicles = m_DoorVehicles->isChecked();
	SetModified(true);
	RefreshDoor();
	m_View->update();
	if(d.operation == "console" && d.console == QPoint(-1, -1))
	{
		m_View->SetTool(GridView::PlaceConsole);
		ShowStatus(tr("Click in a room on the door's deck: its console goes on the nearest wall there."), 8000);
	}
}

void EditorWindow::RefreshHatch()
{
	int i = m_View->SelectedHatch();
	m_Refreshing = true;
	m_HatchBox->setEnabled(i >= 0);
	m_HatchStyle->setItemText(0, tr("The theme's usual (%1)").arg(FindHatchStyle(DefaultHatchStyle(m_Plan.theme))->name));
	m_LadderStyle->setItemText(0, tr("The theme's usual (%1)").arg(FindLadderStyle(DefaultLadderStyle(m_Plan.theme))->name));
	if(i < 0)
	{
		m_HatchLabel->setText(tr("Click a hatch with the hatch tool to pick it."));
		m_Refreshing = false;
		return;
	}
	const Hatch& h = m_Plan.hatches[i];
	QChar above = m_Plan.At(h.deck, h.cell), below = m_Plan.At(h.deck - 1, h.cell);
	static const char* sides[] = {"north", "east", "south", "west"};
	if(h.roof)
		m_HatchLabel->setText(tr("<b>Hull hatch</b> in the ceiling of %1 on deck %2, up onto the roof. The ladder is on its %3 side.")
			.arg(m_Plan.rooms.contains(above) ? m_Plan.rooms[above].name : QString("?")).arg(h.deck).arg(sides[h.side]));
	else
	m_HatchLabel->setText(tr("<b>Hatch</b> in the floor of deck %1, from %2 down to %3. The ladder is on its %4 side.").arg(h.deck)
		.arg(m_Plan.rooms.contains(above) ? m_Plan.rooms[above].name : QString("?"))
		.arg(m_Plan.rooms.contains(below) ? m_Plan.rooms[below].name : QString("?")).arg(sides[h.side]));
	m_HatchStyle->setCurrentIndex(qMax(0, m_HatchStyle->findData(h.style)));
	m_LadderStyle->setCurrentIndex(qMax(0, m_LadderStyle->findData(h.ladder)));
	m_Refreshing = false;
}

void EditorWindow::HatchEdited()
{
	int i = m_View->SelectedHatch();
	if(m_Refreshing || i < 0)
		return;
	PushUndo();
	m_Plan.hatches[i].style = m_HatchStyle->currentData().toString();
	m_Plan.hatches[i].ladder = m_LadderStyle->currentData().toString();
	SetModified(true);
	RefreshHatch();
}

void EditorWindow::RefreshWindow()
{
	int i = m_View->SelectedWindow();
	m_Refreshing = true;
	m_WindowBox->setEnabled(i >= 0);
	m_WindowTech->setItemText(0, tr("The theme's usual (%1)").arg(WindowTechNames()[DefaultWindowTech(m_Plan.theme) - 1]
		.section(':', 0, 0)));
	if(i < 0)
	{
		m_WindowLabel->setText(tr("Click a window with the window tool to pick it."));
		m_Refreshing = false;
		return;
	}
	const Window& w = m_Plan.windows[i];
	DoorPlace place = SimpleMode::PlaceWindow(m_Plan, w);
	auto name = [&](QChar letter) { return m_Plan.rooms.contains(letter) ? m_Plan.rooms[letter].name : QString("?"); };
	const WindowShape* shape = FindWindowShape(w.shape.isEmpty() ? "standard" : w.shape);
	QString size = shape ? tr("%1 m wide, from %2 to %3 m above the floor").arg(WindowWidth(m_Plan, w), 0, 'f', 1)
		.arg(shape->sill, 0, 'f', 2).arg(shape->head, 0, 'f', 2) : QString();
	m_WindowLabel->setText(!place.ok ? tr("<b>Window</b>: %1").arg(place.problem)
		: place.there == '.' ? tr("<b>Window</b> on deck %1, from %2 to the outside: %3.").arg(w.deck).arg(name(place.here)).arg(size)
		: tr("<b>Window</b> on deck %1, between %2 and %3: %4.").arg(w.deck).arg(name(place.here)).arg(name(place.there)).arg(size));
	m_WindowShape->setCurrentIndex(qMax(0, m_WindowShape->findData(w.shape.isEmpty() ? QString() : w.shape)));
	m_WindowTech->setCurrentIndex(qMax(0, m_WindowTech->findData(w.tech)));
	m_Refreshing = false;
}

void EditorWindow::WindowEdited()
{
	int i = m_View->SelectedWindow();
	if(m_Refreshing || i < 0)
		return;
	Window w = m_Plan.windows[i];
	w.shape = m_WindowShape->currentData().toString();
	w.tech = m_WindowTech->currentData().toInt();
	QString problem;
	if(!SimpleMode::PlaceWindow(m_Plan, w).ok || !WindowClear(m_Plan, w, problem, i))
	{
		ShowStatus(problem.isEmpty() ? SimpleMode::PlaceWindow(m_Plan, w).problem : problem, 8000);
		RefreshWindow();
		return;
	}
	PushUndo();
	m_Plan.windows[i] = w;
	SetModified(true);
	RefreshWindow();
	m_View->update();
}

void EditorWindow::UpdatePanels()
{
	if(!m_DoorBox || !m_RoofBox || !m_StairBox || !m_DormerBox || !m_SkylightBox)
		return;
	const GridView::Tool tool = m_View->CurrentTool();
	m_DoorBox->setVisible(m_View->DoorTool() || tool == GridView::PlaceConsole || m_View->SelectedDoor() >= 0);
	m_HatchBox->setVisible(tool == GridView::PlaceHatch || tool == GridView::PlaceHullHatch || m_View->SelectedHatch() >= 0);
	m_WindowBox->setVisible(tool == GridView::PlaceWindow || m_View->SelectedWindow() >= 0);
	m_RailingBox->setVisible(tool == GridView::PlaceRailing || m_View->SelectedRailing() >= 0);
	m_StationBox->setVisible(tool == GridView::PlaceStation || m_View->SelectedStation() >= 0);
	m_FixtureBox->setVisible(m_View->FixtureTool() || m_View->SelectedFixture() >= 0);
	if(m_View->FixtureTool())
		RefreshFixture();
	if(tool == GridView::PlaceStation)
		RefreshStation();
	m_StairBox->setVisible(m_View->StairTool() || m_View->SelectedStair() >= 0);
	m_DormerBox->setVisible(tool == GridView::PlaceDormer || m_View->SelectedDormer() >= 0);
	m_SkylightBox->setVisible(tool == GridView::PlaceSkylight || m_View->SelectedSkylight() >= 0);
	const bool roofs = m_View->RoofTool() || !m_View->SelectedRoof().isNull();
	m_RoofBox->setVisible(roofs);
	m_RoofAll->setVisible(roofs);
}

void EditorWindow::RefreshStair()
{
	int i = m_View->SelectedStair();
	m_Refreshing = true;
	m_StairBox->setEnabled(i >= 0);
	m_StairStyle->setItemText(0, tr("The theme's usual (%1)").arg(FindStairStyle(DefaultStairStyle(m_Plan.theme))->name));
	if(i < 0)
	{
		m_StairLabel->setText(tr("Click a stair with a stair tool to pick it."));
		m_Refreshing = false;
		return;
	}
	const Stair& st = m_Plan.stairs[i];
	StairLayout layout = LayoutStair(m_Plan, st);
	static const char* sides[] = {"north", "east", "south", "west"};
	m_StairLabel->setText(layout.ok ? tr("<b>Stair</b> from deck %1 up to deck %2, climbing %3 first.").arg(st.deck).arg(st.deck + 1)
		.arg(sides[((st.dir % 4) + 4) % 4]) : tr("<b>Stair</b>: %1").arg(layout.problem));
	m_StairKind->setCurrentIndex(qBound(0, st.kind, m_StairKind->count() - 1));
	m_StairStyle->setCurrentIndex(qMax(0, m_StairStyle->findData(st.style)));
	m_Refreshing = false;
}

void EditorWindow::StairEdited()
{
	int i = m_View->SelectedStair();
	if(m_Refreshing || i < 0)
		return;
	Stair changed = m_Plan.stairs[i];
	changed.kind = m_StairKind->currentData().toInt();
	changed.style = m_StairStyle->currentData().toString();
	if(changed.kind != m_Plan.stairs[i].kind)
	{
		bool fits = false;
		QString problem;
		const Plan saved = m_Plan;
		m_Plan.stairs.removeAt(i);
		for(int k = 0; k < 4 && !fits; k++)
		{
			changed.dir = (saved.stairs[i].dir + k) % 4;
			StairLayout layout = LayoutStair(m_Plan, changed);
			fits = layout.ok;
			for(const Stair& o : m_Plan.stairs)
				if(fits && o.deck == changed.deck)
					for(const QPoint& c : LayoutStair(m_Plan, o).cells)
						fits = fits && !layout.cells.contains(c);
			if(problem.isEmpty() && !layout.ok)
				problem = layout.problem;
		}
		m_Plan = saved;
		if(!fits)
		{
			ShowStatus(tr("That shape does not fit here: %1").arg(problem.isEmpty() ? tr("it would run into another stair.")
				: problem), 8000);
			RefreshStair();
			return;
		}
	}
	PushUndo();
	m_Plan.stairs[i] = changed;
	SetModified(true);
	RefreshStair();
	m_View->update();
}

void EditorWindow::RefreshDormer()
{
	int i = m_View->SelectedDormer();
	m_Refreshing = true;
	m_DormerBox->setEnabled(i >= 0);
	if(i < 0)
		m_DormerLabel->setText(tr("Click near the side of a roofed cell the dormer should look out of: the roof's lower edge, over "
			"an outside wall."));
	else
	{
		const Dormer& d = m_Plan.dormers[i];
		static const char* sides[] = {"north", "east", "south", "west"};
		QString problem = DormerProblem(m_Plan, d, i);
		m_DormerLabel->setText(problem.isEmpty() ? tr("<b>Dormer</b> on the roof of deck %1, looking out %2.").arg(d.deck)
			.arg(sides[((d.side % 4) + 4) % 4]) : tr("<b>Dormer</b>: %1 It is left out of the building.").arg(problem));
		m_DormerStyle->setCurrentIndex(qMax(0, m_DormerStyle->findData(d.style)));
	}
	m_Refreshing = false;
}

void EditorWindow::DormerEdited()
{
	int i = m_View->SelectedDormer();
	if(m_Refreshing || i < 0)
		return;
	Dormer changed = m_Plan.dormers[i];
	changed.style = m_DormerStyle->currentData().toString();
	QString problem = DormerProblem(m_Plan, changed, i);
	if(!problem.isEmpty())
	{
		ShowStatus(tr("That dormer does not fit here: %1").arg(problem), 8000);
		RefreshDormer();
		return;
	}
	PushUndo();
	m_Plan.dormers[i] = changed;
	SetModified(true);
	RefreshDormer();
	m_View->update();
}

void EditorWindow::RefreshSkylight()
{
	int i = m_View->SelectedSkylight();
	m_Refreshing = true;
	m_SkylightBox->setEnabled(i >= 0);
	if(i < 0)
		m_SkylightLabel->setText(tr("Click a whole room cell with nothing built above it for a skylight in its ceiling."));
	else
	{
		const Skylight& s = m_Plan.skylights[i];
		QString problem = SkylightProblem(m_Plan, s, i);
		m_SkylightLabel->setText(problem.isEmpty() ? tr("<b>Skylight</b> in the ceiling of deck %1.").arg(s.deck)
			: tr("<b>Skylight</b>: %1 It is left out of the building.").arg(problem));
		m_SkylightStyle->setCurrentIndex(qMax(0, m_SkylightStyle->findData(s.style)));
	}
	m_Refreshing = false;
}

void EditorWindow::SkylightEdited()
{
	int i = m_View->SelectedSkylight();
	if(m_Refreshing || i < 0)
		return;
	Skylight changed = m_Plan.skylights[i];
	changed.style = m_SkylightStyle->currentData().toString();
	QString problem = SkylightProblem(m_Plan, changed, i);
	if(!problem.isEmpty())
	{
		ShowStatus(problem, 8000);
		RefreshSkylight();
		return;
	}
	PushUndo();
	m_Plan.skylights[i] = changed;
	SetModified(true);
	RefreshSkylight();
	m_View->update();
}

void EditorWindow::RefreshFixture()
{
	int i = m_View->SelectedFixture();
	QString type = i >= 0 ? m_Plan.fixtures[i].type : m_View->FixtureToolType();
	if(type.isEmpty())
		return;
	const Fixture& f = i >= 0 ? m_Plan.fixtures[i] : m_View->FixtureTemplate(type);
	const FixtureType* t = FindFixtureType(type);
	m_Refreshing = true;
	if(i < 0)
		m_FixtureLabel->setText(tr("What the %1 tool puts down next. Click one to pick it.").arg(t ? t->name.toLower() : type));
	else
	{
		QString problem = FixtureProblem(m_Plan, f, i);
		m_FixtureLabel->setText(problem.isEmpty() ? tr("<b>%1</b> on deck %2.").arg(t ? t->name : type).arg(f.deck)
			: tr("<b>%1</b>: %2").arg(t ? t->name : type).arg(problem));
	}
	m_FixtureKind->clear();
	if(t)
		for(int k = 0; k < t->kinds.size(); k++)
			m_FixtureKind->addItem(t->kindNames.value(k, t->kinds[k]), k == 0 ? QString() : t->kinds[k]);
	m_FixtureKind->setCurrentIndex(qMax(0, m_FixtureKind->findData(t && f.kind == t->kinds.value(0) ? QString() : f.kind)));
	m_FixtureKind->setEnabled(t && t->kinds.size() > 1);
	m_FixtureColor->setCurrentIndex(qMax(0, m_FixtureColor->findData(f.color)));
	m_FixtureColor->setEnabled(type == "light" || type == "lightpole");
	m_Refreshing = false;
}

void EditorWindow::FixtureEdited()
{
	if(m_Refreshing)
		return;
	int i = m_View->SelectedFixture();
	QString type = i >= 0 ? m_Plan.fixtures[i].type : m_View->FixtureToolType();
	if(type.isEmpty())
		return;
	Fixture f = i >= 0 ? m_Plan.fixtures[i] : m_View->FixtureTemplate(type);
	f.kind = m_FixtureKind->currentData().toString();
	f.color = m_FixtureColor->currentData().toString();
	if(i < 0)
	{
		m_View->FixtureTemplate(type) = f;
		RefreshFixture();
		return;
	}
	QString problem = FixtureProblem(m_Plan, f, i);
	if(!problem.isEmpty())
	{
		ShowStatus(problem, 6000);
		RefreshFixture();
		return;
	}
	PushUndo();
	m_Plan.fixtures[i] = f;
	Fixture next = f;
	next.dir = North;
	m_View->FixtureTemplate(type) = next;
	SetModified(true);
	RefreshFixture();
	m_View->update();
}

void EditorWindow::RefreshStation()
{
	int i = m_View->SelectedStation();
	const Station& st = i >= 0 ? m_Plan.stations[i] : m_View->StationTemplate();
	m_Refreshing = true;
	const StationKind* kind = FindStationKind(st.kind);
	if(i < 0)
		m_StationLabel->setText(tr("What the station tool puts down next. Click a station to pick it."));
	else
	{
		QString problem = StationProblem(m_Plan, st, i);
		QString where = m_Plan.rooms.value(m_Plan.At(st.deck, st.cell)).name;
		m_StationLabel->setText(problem.isEmpty() ? tr("<b>%1 %2</b> in %3, deck %4.").arg(kind ? kind->name : st.kind)
			.arg(st.wall ? tr("screen") : tr("console")).arg(where).arg(st.deck) : tr("<b>Station</b>: %1").arg(problem));
	}
	m_StationKind->setCurrentIndex(qMax(0, m_StationKind->findData(st.kind)));
	m_StationMount->setCurrentIndex(st.wall ? 1 : 0);
	m_StationModule->clear();
	if(kind && !kind->modules.isEmpty())
		for(int k = 0; k < kind->modules.size(); k++)
			m_StationModule->addItem(kind->moduleNames.value(k, kind->modules[k]), k == 0 ? QString() : kind->modules[k]);
	else
		m_StationModule->addItem(tr("None"), QString());
	m_StationModule->setEnabled(kind && kind->modules.size() > 1);
	m_StationModule->setCurrentIndex(qMax(0, m_StationModule->findData(st.module == (kind && !kind->modules.isEmpty()
		? kind->modules.first() : QString()) ? QString() : st.module)));
	m_StationPose->setCurrentIndex(st.pose == "sit" ? 1 : 0);
	m_StationPose->setEnabled(st.wall || st.kind == "gunner");
	m_StationTurret->clear();
	m_StationTurret->addItem(tr("The next with no gunner"), 0);
	for(int f = 0; f < m_Plan.fixtures.size(); f++)
		if(const int n = TurretNumber(m_Plan, f))
		{
			const Fixture& t = m_Plan.fixtures[f];
			const FixtureType* type = FindFixtureType(t.type);
			const int k = type->kinds.indexOf(t.kind.isEmpty() ? type->kinds.first() : t.kind);
			m_StationTurret->addItem(tr("T%1: %2, deck %3").arg(n).arg(type->kindNames.value(k)).arg(t.deck), n);
		}
	m_StationTurret->setCurrentIndex(qMax(0, m_StationTurret->findData(st.turret)));
	m_StationTurret->setEnabled(st.kind == "gunner");
	m_StationPatient->setCurrentIndex(qMax(0, m_StationPatient->findData(st.patient)));
	m_StationPatient->setEnabled(st.kind == "medic");
	m_Refreshing = false;
}

void EditorWindow::StationEdited()
{
	if(m_Refreshing)
		return;
	int i = m_View->SelectedStation();
	Station st = i >= 0 ? m_Plan.stations[i] : m_View->StationTemplate();
	QString oldKind = st.kind;
	st.kind = m_StationKind->currentData().toString();
	st.wall = m_StationMount->currentData().toBool();
	st.module = st.kind == oldKind ? m_StationModule->currentData().toString() : QString();
	st.pose = m_StationPose->currentData().toString();
	st.patient = m_StationPatient->currentData().toString();
	st.turret = st.kind == "gunner" ? m_StationTurret->currentData().toInt() : 0;
	if(i < 0)
	{
		m_View->StationTemplate() = st;
		RefreshStation();
		return;
	}
	QString problem = StationProblem(m_Plan, st, i);
	if(!problem.isEmpty())
	{
		ShowStatus(problem, 6000);
		RefreshStation();
		return;
	}
	PushUndo();
	m_Plan.stations[i] = st;
	m_View->StationTemplate() = st;
	SetModified(true);
	RefreshStation();
	m_View->update();
}

void EditorWindow::RefreshRailing()
{
	int i = m_View->SelectedRailing();
	m_Refreshing = true;
	m_RailingBox->setEnabled(i >= 0);
	m_RailingStyle->setItemText(0, tr("The theme's usual (%1)").arg(FindRailingStyle(DefaultRailingStyle(m_Plan.theme))->name));
	if(i < 0)
	{
		m_RailingLabel->setText(tr("Click a railing with the railing tool to pick it."));
		m_Refreshing = false;
		return;
	}
	const Railing& r = m_Plan.railings[i];
	DoorPlace place = SimpleMode::PlaceRailing(m_Plan, r);
	auto name = [&](QChar letter) { return m_Plan.rooms.contains(letter) ? m_Plan.rooms[letter].name : QString("?"); };
	m_RailingLabel->setText(place.ok ? tr("<b>Railing</b> on deck %1, between %2 and %3.").arg(r.deck).arg(name(place.here))
		.arg(name(place.there)) : tr("<b>Railing</b>: %1").arg(place.problem));
	m_RailingStyle->setCurrentIndex(qMax(0, m_RailingStyle->findData(r.style)));
	m_Refreshing = false;
}

void EditorWindow::RailingEdited()
{
	int i = m_View->SelectedRailing();
	if(m_Refreshing || i < 0)
		return;
	PushUndo();
	m_Plan.railings[i].style = m_RailingStyle->currentData().toString();
	SetModified(true);
	RefreshRailing();
	m_View->update();
}

void EditorWindow::RefreshRoof()
{
	QChar letter = m_View->SelectedRoof();
	m_Refreshing = true;
	m_RoofBox->setEnabled(!letter.isNull());
	if(letter.isNull())
	{
		m_RoofLabel->setText(tr("Click a roof with a roof tool to pick it."));
		m_Refreshing = false;
		return;
	}
	const Roof& roof = m_Plan.roofs[letter];
	const RoofStyle* style = FindRoofStyle(roof.style);
	int cells = 0;
	QList<int> decks;
	for(int d = 0; d < m_Plan.roofGrids.size(); d++)
		for(const QString& row : m_Plan.roofGrids[d])
			for(QChar ch : row)
				if(ch == letter)
				{
					cells++;
					if(!decks.contains(d))
						decks << d;
				}
	static const char* sides[] = {"north", "east", "south", "west"};
	QString way = !style ? QString() : style->ridged ? (roof.dir == North || roof.dir == South
			? tr(", its ridge running north and south") : tr(", its ridge running east and west"))
		: style->sloped ? tr(", sloping down to the %1").arg(sides[roof.dir]) : QString();
	m_RoofLabel->setText(tr("<b>%1 roof</b> over %n cell(s) of deck %2%3.", nullptr, cells).arg(style ? style->name : roof.style)
		.arg(decks.isEmpty() ? 0 : decks.first()).arg(way));
	m_RoofStyle->setCurrentIndex(qMax(0, m_RoofStyle->findData(roof.style)));
	m_RoofPitch->setValue(qRound(roof.pitch));
	m_RoofPitch->setEnabled(style && style->pitch > 0);
	m_TurnRoof->setEnabled(style && (style->ridged || style->sloped));
	m_Refreshing = false;
}

void EditorWindow::RoofEdited()
{
	QChar letter = m_View->SelectedRoof();
	if(m_Refreshing || letter.isNull())
		return;
	QString style = m_RoofStyle->currentData().toString();
	double pitch = m_RoofPitch->value();
	const Roof& roof = m_Plan.roofs[letter];
	if(roof.style == style && qRound(roof.pitch) == qRound(pitch))
		return;
	PushUndo();
	Roof& edited = m_Plan.roofs[letter];
	edited.style = style;
	edited.pitch = pitch;
	SetModified(true);
	RefreshRoof();
	m_View->update();
}
