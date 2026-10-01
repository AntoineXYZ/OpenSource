#include "editorwindow.h"
#include "gridview.h"
#include "palette.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QToolButton>
#include <QPainter>
#include <QMenu>
#include <QColorDialog>
#include <QInputDialog>
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

namespace {

QValidator* NameValidator(QObject* parent)
{
	return new QRegularExpressionValidator(QRegularExpression("[^/\\\\:!|%#]{0,30}"), parent);
}

QDoubleSpinBox* Metres(double min, double max, double step, QWidget* parent)
{
	QDoubleSpinBox* box = new QDoubleSpinBox(parent);
	box->setRange(min, max);
	box->setSingleStep(step);
	box->setDecimals(2);
	box->setSuffix(" m");
	return box;
}

}

Plan StarterPlan();
int Quarters(const Plan& plan, QChar letter, int* deck = nullptr);

Plan StarterPlan()
{
	Plan plan;
	plan.name = "New Building";
	QStringList grid;
	for(int r = 0; r < 5; r++)
		grid << QString(6 * 4, '.');
	plan.decks << grid;
	return plan;
}

int Quarters(const Plan& plan, QChar letter, int* deck)
{
	int n = 0;
	for(int d = 0; d < plan.Decks(); d++)
		for(const QString& row : plan.decks[d])
		{
			int k = row.count(letter);
			if(k && deck)
				*deck = d;
			n += k;
		}
	return n;
}

EditorWindow::EditorWindow()
{
	m_Plan = StarterPlan();
	BuildUi();
	RefreshAll();
	SetModified(false);
}

void EditorWindow::BuildUi()
{
	resize(1240, 780);
	setWindowIcon(PaletteIcon("SoHDesignerIcon"));

	m_View = new GridView(m_Plan, this);
	QScrollArea* scroll = new QScrollArea(this);
	scroll->setWidget(m_View);
	scroll->setAlignment(Qt::AlignCenter);
	scroll->setFrameShape(QFrame::NoFrame);
	setCentralWidget(scroll);
	connect(m_View, &GridView::EditStarting, this, &EditorWindow::PushUndo);
	connect(m_View, &GridView::PlanChanged, this, &EditorWindow::PlanEdited);
	connect(m_View, &GridView::EditRefused, this, [this]() {
		if(!m_Undo.isEmpty())
			m_Undo.removeLast();
		UpdateActions();
	});
	connect(m_View, &GridView::Status, this, [this](const QString& text) { ShowStatus(text); });

	m_Panel = new DesignPanel(this);
	m_Panel->SetLines(m_View->Lines());
	connect(m_Panel, &DesignPanel::ActionTriggered, this, &EditorWindow::PanelAction);
	connect(m_Panel, &DesignPanel::LineToggled, m_View, &GridView::SetLine);
	connect(m_Panel, &DesignPanel::ToolChosen, this, [this](int tool) { m_View->SetTool((GridView::Tool)tool); });
	connect(m_Panel, &DesignPanel::RoomChosen, this, &EditorWindow::RoomPicked);
	connect(m_Panel, &DesignPanel::ThemeChosen, this, &EditorWindow::ThemeChosen);
	QDockWidget* leftDock = new QDockWidget(tr("Tools"), this);
	leftDock->setObjectName("design");
	leftDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	leftDock->setWidget(m_Panel);
	addDockWidget(Qt::LeftDockWidgetArea, leftDock);

	QWidget* right = new QWidget(this);
	QVBoxLayout* rightLayout = new QVBoxLayout(right);

	QGroupBox* roomBox = new QGroupBox(tr("Room"), right);
	QFormLayout* roomForm = new QFormLayout(roomBox);
	m_RoomLabel = new QLabel(roomBox);
	m_RoomName = new QLineEdit(roomBox);
	m_RoomName->setValidator(NameValidator(m_RoomName));
	m_RoomName->setToolTip(tr("The room's name, shown in the game. Every room needs its own."));
	roomForm->addRow(m_RoomLabel);
	roomForm->addRow(tr("Name"), m_RoomName);
	connect(m_RoomName, &QLineEdit::textEdited, this, &EditorWindow::RoomNameEdited);
	m_RoomLights = new QComboBox(roomBox);
	m_RoomLights->addItem(tr("Bright"), "bright");
	m_RoomLights->addItem(tr("Dim"), "dim");
	m_RoomLights->addItem(tr("Dark"), "dark");
	m_RoomLights->setToolTip(tr("How the room's lights start. Whoever is in it can switch them in the game."));
	m_RoomSunlit = new QCheckBox(tr("Sunlit"), roomBox);
	m_RoomSunlit->setToolTip(tr("Daylight reaches in. Every stock room is sunlit."));
	m_RoomFloor = new QComboBox(roomBox);
	m_RoomWalls = new QComboBox(roomBox);
	m_RoomCeiling = new QComboBox(roomBox);
	m_RoomTrim = new QComboBox(roomBox);
	for(QComboBox* box : {m_RoomFloor, m_RoomWalls, m_RoomCeiling})
		box->setToolTip(tr("The room's texture here: one of the building theme's, from the game's own library."));
	m_RoomTrim->setToolTip(tr("The room's wall design (skirting, rails, cornices, posts, beams): painted in the theme's colour, "
		"or in one of the theme's textures."));
	roomForm->addRow(tr("Lights"), m_RoomLights);
	roomForm->addRow(QString(), m_RoomSunlit);
	auto surfaceRow = [&](QComboBox* box, const QString& surface) {
		QWidget* row = new QWidget(roomBox);
		QHBoxLayout* h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		h->setSpacing(4);
		h->addWidget(box, 1);
		QToolButton* colour = new QToolButton(row);
		colour->setPopupMode(QToolButton::InstantPopup);
		colour->setFixedWidth(34);
		QMenu* menu = new QMenu(colour);
		menu->addAction(tr("Choose a colour..."), this, [this, surface]() {
			QChar letter = CurrentRoom();
			if(letter.isNull() || !m_Plan.rooms.contains(letter))
				return;
			const Room& r = m_Plan.rooms[letter];
			QColor c = QColorDialog::getColor(RoomColor(r, surface).isValid() ? RoomColor(r, surface) : QColor(0x80, 0x80, 0x80),
				this, tr("Colour"));
			if(c.isValid())
				SetRoomColor(surface, c, r.strengths.value(surface, 50));
		});
		menu->addSeparator();
		for(int strength : {25, 50, 75, 100})
			menu->addAction(strength == 100 ? tr("Solid colour") : tr("%1% over the texture").arg(strength), this, [this, surface, strength]() {
				QChar letter = CurrentRoom();
				if(!letter.isNull() && m_Plan.rooms.contains(letter))
					SetRoomColor(surface, RoomColor(m_Plan.rooms[letter], surface).isValid() ? RoomColor(m_Plan.rooms[letter], surface)
						: QColor(0x80, 0x80, 0x80), strength);
			});
		menu->addSeparator();
		menu->addAction(tr("No colour"), this, [this, surface]() { SetRoomColor(surface, QColor(), 50); });
		menu->addSeparator();
		AddSizeMenu(menu, [this, surface](int percent) { SetRoomScale(surface, percent); });
		colour->setMenu(menu);
		h->addWidget(colour);
		m_RoomColors[surface] = colour;
		return row;
	};
	roomForm->addRow(tr("Floor"), surfaceRow(m_RoomFloor, "floor"));
	roomForm->addRow(tr("Walls"), surfaceRow(m_RoomWalls, "walls"));
	roomForm->addRow(tr("Ceiling"), surfaceRow(m_RoomCeiling, "ceiling"));
	m_RoomDesign = new QComboBox(roomBox);
	m_RoomDesign->setToolTip(tr("The room's wall design (skirting, rails, cornices, posts, beams): its own type's, another "
		"room type's, or none. Its shapes follow the building's theme."));
	roomForm->addRow(tr("Wall design"), m_RoomDesign);
	connect(m_RoomDesign, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::TexturesEdited);
	roomForm->addRow(tr("Trims"), surfaceRow(m_RoomTrim, "trim"));
	for(QComboBox* box : {m_RoomFloor, m_RoomWalls, m_RoomCeiling, m_RoomTrim})
		connect(box, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::TexturesEdited);
	connect(m_RoomLights, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::RoomLightsEdited);
	connect(m_RoomSunlit, &QCheckBox::toggled, this, &EditorWindow::RoomLightsEdited);
	rightLayout->addWidget(roomBox);

	m_DoorBox = new QGroupBox(tr("Door"), right);
	QFormLayout* doorForm = new QFormLayout(m_DoorBox);
	m_DoorLabel = new QLabel(m_DoorBox);
	m_DoorLabel->setWordWrap(true);
	m_DoorStyle = new QComboBox(m_DoorBox);
	m_DoorWide = new QCheckBox(tr("Double width (2.4 m)"), m_DoorBox);
	m_DoorWide->setToolTip(tr("A double door: pairs of leaves, rollers that slide up or down, or halves that part."));
	m_DoorStyle->setToolTip(tr("How the door opens. The game animates it through the frames between shut and open; if a style "
		"does not fit where the door is, the blueprint uses the nearest that does and the Report says so."));
	m_DoorOp = new QComboBox(m_DoorBox);
	m_DoorOp->addItem(tr("Wall switches"), "switches");
	m_DoorOp->addItem(tr("Click the door"), "door");
	m_DoorOp->addItem(tr("Console"), "console");
	m_DoorOp->setToolTip(tr("How it is worked: switches on the wall beside it, one each side; clicking the door itself; or a "
		"console on a wall of a room, with a switch on the other side of the door."));
	m_PlaceConsole = new QPushButton(tr("Place console..."), m_DoorBox);
	m_PlaceConsole->setToolTip(tr("Then click in a room on the door's deck: the console goes on the nearest wall there."));
	m_DoorAuto = new QCheckBox(tr("Automatic: opens when someone comes near"), m_DoorBox);
	m_DoorLocked = new QCheckBox(tr("Starts locked"), m_DoorBox);
	m_DoorVehicles = new QCheckBox(tr("Opens for vehicles and spacecraft"), m_DoorBox);
	m_RemoveDoor = new QPushButton(tr("Remove door"), m_DoorBox);
	doorForm->addRow(m_DoorLabel);
	doorForm->addRow(m_DoorWide);
	doorForm->addRow(tr("Opens"), m_DoorStyle);
	doorForm->addRow(tr("Worked by"), m_DoorOp);
	doorForm->addRow(QString(), m_PlaceConsole);
	doorForm->addRow(m_DoorAuto);
	doorForm->addRow(m_DoorLocked);
	doorForm->addRow(m_DoorVehicles);
	doorForm->addRow(m_RemoveDoor);
	rightLayout->addWidget(m_DoorBox);
	connect(m_DoorStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::DoorEdited);
	connect(m_DoorOp, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::DoorEdited);
	for(QCheckBox* box : {m_DoorAuto, m_DoorLocked, m_DoorVehicles, m_DoorWide})
		connect(box, &QCheckBox::toggled, this, &EditorWindow::DoorEdited);
	connect(m_PlaceConsole, &QPushButton::clicked, this, [this]() {
		m_View->SetTool(GridView::PlaceConsole);
		ShowStatus(tr("Click in a room on the door's deck: its console goes on the nearest wall there."), 8000);
	});
	connect(m_RemoveDoor, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedDoor();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.doors.removeAt(i);
		m_View->SelectDoor(-1);
		SetModified(true);
	});
	connect(m_View, &GridView::DoorSelected, this, &EditorWindow::RefreshDoor);

	m_HatchBox = new QGroupBox(tr("Hatch"), right);
	QFormLayout* hatchForm = new QFormLayout(m_HatchBox);
	m_HatchLabel = new QLabel(m_HatchBox);
	m_HatchLabel->setWordWrap(true);
	m_HatchStyle = new QComboBox(m_HatchBox);
	m_HatchStyle->addItem(QString(), QString());
	for(const HatchStyle& s : HatchStyles())
	{
		m_HatchStyle->addItem(s.name, s.key);
		m_HatchStyle->setItemData(m_HatchStyle->count() - 1, s.note, Qt::ToolTipRole);
	}
	m_LadderStyle = new QComboBox(m_HatchBox);
	m_LadderStyle->addItem(QString(), QString());
	for(const HatchStyle& s : LadderStyles())
	{
		m_LadderStyle->addItem(s.name, s.key);
		m_LadderStyle->setItemData(m_LadderStyle->count() - 1, s.note, Qt::ToolTipRole);
	}
	QPushButton* rotateHatch = new QPushButton(tr("Rotate"), m_HatchBox);
	rotateHatch->setToolTip(tr("Turns the hatch a quarter turn clockwise: the ladder moves to the next edge of the opening, and the "
		"hatch opens away from it. Right-clicking a hatch with the hatch tool does the same."));
	QPushButton* removeHatch = new QPushButton(tr("Remove hatch"), m_HatchBox);
	hatchForm->addRow(m_HatchLabel);
	hatchForm->addRow(tr("Opens"), m_HatchStyle);
	hatchForm->addRow(tr("Ladder"), m_LadderStyle);
	hatchForm->addRow(rotateHatch);
	hatchForm->addRow(removeHatch);
	connect(rotateHatch, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedHatch();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.hatches[i].side = (m_Plan.hatches[i].side + 1) % 4;
		SetModified(true);
		RefreshHatch();
		m_View->update();
	});
	rightLayout->addWidget(m_HatchBox);
	connect(m_HatchStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::HatchEdited);
	connect(m_LadderStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::HatchEdited);
	connect(removeHatch, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedHatch();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.hatches.removeAt(i);
		m_View->SelectHatch(-1);
		SetModified(true);
	});
	connect(m_View, &GridView::HatchSelected, this, &EditorWindow::RefreshHatch);

	m_WindowBox = new QGroupBox(tr("Window"), right);
	QFormLayout* windowForm = new QFormLayout(m_WindowBox);
	m_WindowLabel = new QLabel(m_WindowBox);
	m_WindowLabel->setWordWrap(true);
	m_WindowShape = new QComboBox(m_WindowBox);
	for(const WindowShape& s : WindowShapes())
	{
		m_WindowShape->addItem(s.name, s.key == "standard" ? QString() : s.key);
		m_WindowShape->setItemData(m_WindowShape->count() - 1, s.note, Qt::ToolTipRole);
	}
	m_WindowTech = new QComboBox(m_WindowBox);
	m_WindowTech->addItem(QString(), 0);
	for(int t = 0; t < WindowTechNames().size(); t++)
		m_WindowTech->addItem(WindowTechNames()[t], t + 1);
	m_WindowTech->setToolTip(tr("How the window is built: an open hole with shutters, leaded panes, a frame with glazing bars, a slim "
		"modern frame, or frameless glass with a glowing edge. Right-clicking a window with the window tool steps through them."));
	QPushButton* removeWindow = new QPushButton(tr("Remove window"), m_WindowBox);
	windowForm->addRow(m_WindowLabel);
	windowForm->addRow(tr("Shape"), m_WindowShape);
	windowForm->addRow(tr("Built"), m_WindowTech);
	windowForm->addRow(removeWindow);
	rightLayout->addWidget(m_WindowBox);
	connect(m_WindowShape, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::WindowEdited);
	connect(m_WindowTech, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::WindowEdited);
	connect(removeWindow, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedWindow();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.windows.removeAt(i);
		m_View->SelectWindow(-1);
		SetModified(true);
	});
	connect(m_View, &GridView::WindowSelected, this, &EditorWindow::RefreshWindow);

	m_StairBox = new QGroupBox(tr("Stair"), right);
	QFormLayout* stairForm = new QFormLayout(m_StairBox);
	m_StairLabel = new QLabel(m_StairBox);
	m_StairLabel->setWordWrap(true);
	m_StairKind = new QComboBox(m_StairBox);
	for(int k = 0; k < StairKindKeys().size(); k++)
		m_StairKind->addItem(StairKindNames()[k], k);
	m_StairStyle = new QComboBox(m_StairBox);
	m_StairStyle->addItem(QString(), QString());
	for(const StairStyle& s : StairStyles())
	{
		m_StairStyle->addItem(s.name, s.key);
		m_StairStyle->setItemData(m_StairStyle->count() - 1, s.note, Qt::ToolTipRole);
	}
	QPushButton* turnStair = new QPushButton(tr("Turn"), m_StairBox);
	turnStair->setToolTip(tr("Turns the stair to climb the next way it fits. Right-clicking a stair with a stair tool does the same."));
	QPushButton* removeStair = new QPushButton(tr("Remove stair"), m_StairBox);
	stairForm->addRow(m_StairLabel);
	stairForm->addRow(tr("Shape"), m_StairKind);
	stairForm->addRow(tr("Style"), m_StairStyle);
	stairForm->addRow(turnStair);
	stairForm->addRow(removeStair);
	rightLayout->addWidget(m_StairBox);
	connect(m_StairKind, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::StairEdited);
	connect(m_StairStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::StairEdited);
	connect(turnStair, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedStair();
		if(i < 0)
			return;
		m_View->TurnStairIndex(i);
	});
	connect(removeStair, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedStair();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.stairs.removeAt(i);
		m_View->SelectStair(-1);
		SetModified(true);
		m_View->update();
	});
	connect(m_View, &GridView::StairSelected, this, &EditorWindow::RefreshStair);

	m_DormerBox = new QGroupBox(tr("Dormer"), right);
	QFormLayout* dormerForm = new QFormLayout(m_DormerBox);
	m_DormerLabel = new QLabel(m_DormerBox);
	m_DormerLabel->setWordWrap(true);
	m_DormerStyle = new QComboBox(m_DormerBox);
	for(const DetailStyle& s : DormerStyles())
	{
		m_DormerStyle->addItem(s.name, s.key == "gable" ? QString() : s.key);
		m_DormerStyle->setItemData(m_DormerStyle->count() - 1, s.note, Qt::ToolTipRole);
	}
	QPushButton* turnDormer = new QPushButton(tr("Turn"), m_DormerBox);
	turnDormer->setToolTip(tr("Turns the dormer to look out of the next side of its cell it fits. Right-clicking it with the dormer "
		"tool does the same."));
	QPushButton* removeDormer = new QPushButton(tr("Remove dormer"), m_DormerBox);
	dormerForm->addRow(m_DormerLabel);
	dormerForm->addRow(tr("Style"), m_DormerStyle);
	dormerForm->addRow(turnDormer);
	dormerForm->addRow(removeDormer);
	rightLayout->addWidget(m_DormerBox);
	connect(m_DormerStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::DormerEdited);
	connect(turnDormer, &QPushButton::clicked, this, [this]() { m_View->TurnDormerIndex(m_View->SelectedDormer()); });
	connect(removeDormer, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedDormer();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.dormers.removeAt(i);
		m_View->SelectDormer(-1);
		SetModified(true);
		m_View->update();
	});
	connect(m_View, &GridView::DormerSelected, this, &EditorWindow::RefreshDormer);

	m_SkylightBox = new QGroupBox(tr("Skylight"), right);
	QFormLayout* skylightForm = new QFormLayout(m_SkylightBox);
	m_SkylightLabel = new QLabel(m_SkylightBox);
	m_SkylightLabel->setWordWrap(true);
	m_SkylightStyle = new QComboBox(m_SkylightBox);
	for(const DetailStyle& s : SkylightStyles())
	{
		m_SkylightStyle->addItem(s.name, s.key == "pane" ? QString() : s.key);
		m_SkylightStyle->setItemData(m_SkylightStyle->count() - 1, s.note, Qt::ToolTipRole);
	}
	QPushButton* removeSkylight = new QPushButton(tr("Remove skylight"), m_SkylightBox);
	skylightForm->addRow(m_SkylightLabel);
	skylightForm->addRow(tr("Style"), m_SkylightStyle);
	skylightForm->addRow(removeSkylight);
	rightLayout->addWidget(m_SkylightBox);
	connect(m_SkylightStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::SkylightEdited);
	connect(removeSkylight, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedSkylight();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.skylights.removeAt(i);
		m_View->SelectSkylight(-1);
		SetModified(true);
		m_View->update();
	});
	connect(m_View, &GridView::SkylightSelected, this, &EditorWindow::RefreshSkylight);

	m_FixtureBox = new QGroupBox(tr("Fixture"), right);
	QFormLayout* fixtureForm = new QFormLayout(m_FixtureBox);
	m_FixtureLabel = new QLabel(m_FixtureBox);
	m_FixtureLabel->setWordWrap(true);
	m_FixtureKind = new QComboBox(m_FixtureBox);
	m_FixtureColor = new QComboBox(m_FixtureBox);
	m_FixtureColor->addItem(tr("Its kind's usual"), QString());
	for(const QString& c : LightColors())
		m_FixtureColor->addItem(c.left(1).toUpper() + c.mid(1), c);
	QPushButton* removeFixture = new QPushButton(tr("Remove"), m_FixtureBox);
	fixtureForm->addRow(m_FixtureLabel);
	fixtureForm->addRow(tr("Kind"), m_FixtureKind);
	fixtureForm->addRow(tr("Colour"), m_FixtureColor);
	fixtureForm->addRow(removeFixture);
	rightLayout->addWidget(m_FixtureBox);
	connect(m_FixtureKind, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::FixtureEdited);
	connect(m_FixtureColor, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::FixtureEdited);
	connect(removeFixture, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedFixture();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.fixtures.removeAt(i);
		m_View->SelectFixture(-1);
		SetModified(true);
	});
	connect(m_View, &GridView::FixtureSelected, this, &EditorWindow::RefreshFixture);
	connect(m_View, &GridView::FixtureSelected, this, &EditorWindow::UpdatePanels);

	m_StationBox = new QGroupBox(tr("Station"), right);
	QFormLayout* stationForm = new QFormLayout(m_StationBox);
	m_StationLabel = new QLabel(m_StationBox);
	m_StationLabel->setWordWrap(true);
	m_StationKind = new QComboBox(m_StationBox);
	for(const StationKind& k : StationKinds())
	{
		m_StationKind->addItem(k.name, k.key);
		m_StationKind->setItemData(m_StationKind->count() - 1, k.note, Qt::ToolTipRole);
	}
	m_StationMount = new QComboBox(m_StationBox);
	m_StationMount->addItem(tr("Console, with its chair"), false);
	m_StationMount->addItem(tr("Screen on the wall"), true);
	m_StationModule = new QComboBox(m_StationBox);
	m_StationModule->setToolTip(tr("Which of the building's modules it runs."));
	m_StationPose = new QComboBox(m_StationBox);
	m_StationPose->addItem(tr("Stands"), QString());
	m_StationPose->addItem(tr("Sits"), "sit");
	m_StationPose->setToolTip(tr("How the operator works a screen on the wall (the screen is lower for a seated one)."));
	m_StationPatient = new QComboBox(m_StationBox);
	m_StationPatient->addItem(tr("Stands"), QString());
	m_StationPatient->addItem(tr("Sits"), "sit");
	m_StationPatient->addItem(tr("Lies prone"), "prone");
	m_StationPatient->setToolTip(tr("How a medic station's patient is treated, in the cell next to it."));
	m_StationTurret = new QComboBox(m_StationBox);
	m_StationTurret->setToolTip(tr("Which turret a turret gunner station works: the next one with no gunner, or one by its number "
		"(T1, T2... on the grid)."));
	QPushButton* removeStation = new QPushButton(tr("Remove station"), m_StationBox);
	stationForm->addRow(m_StationLabel);
	stationForm->addRow(tr("Kind"), m_StationKind);
	stationForm->addRow(tr("Made as"), m_StationMount);
	stationForm->addRow(tr("Module"), m_StationModule);
	stationForm->addRow(tr("Operator"), m_StationPose);
	stationForm->addRow(tr("Patient"), m_StationPatient);
	stationForm->addRow(tr("Turret"), m_StationTurret);
	stationForm->addRow(removeStation);
	rightLayout->addWidget(m_StationBox);
	for(QComboBox* box : {m_StationKind, m_StationMount, m_StationModule, m_StationPose, m_StationPatient, m_StationTurret})
		connect(box, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::StationEdited);
	connect(removeStation, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedStation();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.stations.removeAt(i);
		m_View->SelectStation(-1);
		SetModified(true);
	});
	connect(m_View, &GridView::StationSelected, this, &EditorWindow::RefreshStation);
	connect(m_View, &GridView::StationSelected, this, &EditorWindow::UpdatePanels);

	m_RailingBox = new QGroupBox(tr("Railing"), right);
	QFormLayout* railingForm = new QFormLayout(m_RailingBox);
	m_RailingLabel = new QLabel(m_RailingBox);
	m_RailingLabel->setWordWrap(true);
	m_RailingStyle = new QComboBox(m_RailingBox);
	m_RailingStyle->addItem(QString(), QString());
	for(const RailingStyle& s : RailingStyles())
	{
		m_RailingStyle->addItem(s.name, s.key);
		m_RailingStyle->setItemData(m_RailingStyle->count() - 1, s.note, Qt::ToolTipRole);
	}
	QPushButton* removeRailing = new QPushButton(tr("Remove railing"), m_RailingBox);
	railingForm->addRow(m_RailingLabel);
	railingForm->addRow(tr("Style"), m_RailingStyle);
	railingForm->addRow(removeRailing);
	rightLayout->addWidget(m_RailingBox);
	connect(m_RailingStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::RailingEdited);
	connect(removeRailing, &QPushButton::clicked, this, [this]() {
		int i = m_View->SelectedRailing();
		if(i < 0)
			return;
		PushUndo();
		m_Plan.railings.removeAt(i);
		m_View->SelectRailing(-1);
		SetModified(true);
	});
	connect(m_View, &GridView::RailingSelected, this, &EditorWindow::RefreshRailing);

	m_RoofBox = new QGroupBox(tr("Roof"), right);
	QFormLayout* roofForm = new QFormLayout(m_RoofBox);
	m_RoofLabel = new QLabel(m_RoofBox);
	m_RoofLabel->setWordWrap(true);
	m_RoofStyle = new QComboBox(m_RoofBox);
	for(const RoofStyle& s : RoofStyles())
	{
		m_RoofStyle->addItem(s.name, s.key);
		m_RoofStyle->setItemData(m_RoofStyle->count() - 1, s.note, Qt::ToolTipRole);
	}
	m_TurnRoof = new QPushButton(tr("Turn"), m_RoofBox);
	m_TurnRoof->setToolTip(tr("Turns the ridge (or a barrel's axis) the other way, or a shed roof to slope down to the next side. "
		"Right-clicking a roof with a roof tool does the same."));
	m_RoofPitch = new QSpinBox(m_RoofBox);
	m_RoofPitch->setRange(0, 80);
	m_RoofPitch->setSuffix(QString(QChar(0x00b0)));
	m_RoofPitch->setSpecialValueText(tr("The style's usual"));
	m_RemoveRoof = new QPushButton(tr("Remove roof"), m_RoofBox);
	QPushButton* roofAll = m_RoofAll = new QPushButton(tr("Roof the building"), right);
	roofAll->setToolTip(tr("Roofs every part of the building's top that has no roof yet, in the theme's usual style: a roof for "
		"each piece, its ridge along the piece's longer side."));
	roofForm->addRow(m_RoofLabel);
	roofForm->addRow(tr("Style"), m_RoofStyle);
	roofForm->addRow(tr("Pitch"), m_RoofPitch);
	roofForm->addRow(m_TurnRoof);
	roofForm->addRow(m_RemoveRoof);
	rightLayout->addWidget(m_RoofBox);
	rightLayout->addWidget(roofAll);
	connect(m_RoofStyle, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::RoofEdited);
	connect(m_RoofPitch, &QSpinBox::editingFinished, this, &EditorWindow::RoofEdited);
	connect(m_TurnRoof, &QPushButton::clicked, this, [this]() {
		QChar letter = m_View->SelectedRoof();
		if(letter.isNull())
			return;
		Roof& roof = m_Plan.roofs[letter];
		const RoofStyle* style = FindRoofStyle(roof.style);
		if(!style || (!style->ridged && !style->sloped))
			return;
		PushUndo();
		roof.dir = style->ridged ? (roof.dir == East || roof.dir == West ? North : East) : (roof.dir + 1) % 4;
		SetModified(true);
		RefreshRoof();
		m_View->update();
	});
	connect(m_RemoveRoof, &QPushButton::clicked, this, [this]() {
		QChar letter = m_View->SelectedRoof();
		if(letter.isNull())
			return;
		PushUndo();
		for(int d = 0; d < m_Plan.roofGrids.size(); d++)
			for(QString& row : m_Plan.roofGrids[d])
				row.replace(letter, '.');
		TidyPlan(m_Plan);
		m_View->SelectRoof(QChar());
		SetModified(true);
	});
	connect(roofAll, &QPushButton::clicked, this, [this]() {
		Plan before = m_Plan;
		int cells = RoofTheBuilding(m_Plan);
		if(!cells)
		{
			ShowStatus(tr("Every part of the building's top that can have a roof has one."), 8000);
			return;
		}
		m_Undo << before;
		m_Redo.clear();
		SetModified(true);
		UpdateActions();
		ShowStatus(tr("Roofed %n cell(s).", nullptr, cells), 8000);
		m_View->update();
	});
	connect(m_View, &GridView::RoofSelected, this, &EditorWindow::RefreshRoof);
	connect(m_View, &GridView::ToolChanged, this, [this](int tool) { m_Panel->SetTool((GridView::Tool)tool); });
	connect(m_View, &GridView::ToolSet, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::DoorSelected, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::HatchSelected, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::WindowSelected, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::RailingSelected, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::StairSelected, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::DormerSelected, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::SkylightSelected, this, &EditorWindow::UpdatePanels);
	connect(m_View, &GridView::RoofSelected, this, &EditorWindow::UpdatePanels);

	QGroupBox* listBox = new QGroupBox(tr("Rooms"), right);
	QVBoxLayout* listLayout = new QVBoxLayout(listBox);
	m_Rooms = new QListWidget(listBox);
	m_Rooms->setIconSize(QSize(16, 16));
	m_Rooms->setToolTip(tr("Click a room to pick it for painting and go to its deck."));
	listLayout->addWidget(m_Rooms);
	connect(m_Rooms, &QListWidget::itemClicked, this, &EditorWindow::RoomListClicked);
	rightLayout->addWidget(listBox, 1);

	QGroupBox* buildingBox = new QGroupBox(tr("Blueprint"), right);
	QFormLayout* form = new QFormLayout(buildingBox);
	m_Name = new QLineEdit(buildingBox);
	m_Name->setValidator(NameValidator(m_Name));
	m_Cell = Metres(1, 20, 0.5, buildingBox);
	m_Cell->setToolTip(tr("Width of one grid cell."));
	m_Height = Metres(2, 20, 0.5, buildingBox);
	m_Height->setToolTip(tr("Height of one deck, floor to floor."));
	m_Wall = Metres(0.05, 1, 0.05, buildingBox);
	m_Wall->setToolTip(tr("Thickness of the outside walls: hull to room surface."));
	m_Partition = Metres(0.05, 2, 0.05, buildingBox);
	m_Partition->setToolTip(tr("Thickness of the walls between rooms."));
	m_VoidGap = Metres(0, 1, 0.05, buildingBox);
	m_VoidGap->setToolTip(tr("How far the rooms' walkable space stays clear of the floor and ceiling."));
	m_Cols = new QSpinBox(buildingBox);
	m_Cols->setRange(1, 40);
	m_Rows = new QSpinBox(buildingBox);
	m_Rows->setRange(1, 40);
	form->addRow(tr("Name"), m_Name);
	m_Interiors = new QCheckBox(tr("Interiors?"), buildingBox);
	m_Interiors->setToolTip(tr("Built to go into: rooms, doors that open, stairs, hatches, lifts, stations. Without, only its "
		"outside, like most stock buildings: black windows (still windows, which light up at night), doors that stay shut, and "
		"nothing inside, balconies included."));
	form->addRow(QString(), m_Interiors);
	connect(m_Interiors, &QCheckBox::toggled, this, [this](bool on) {
		if(m_Refreshing || on == m_Plan.interiors)
			return;
		PushUndo();
		m_Plan.interiors = on;
		SetModified(true);
		ShowStatus(on ? tr("With interiors: rooms, doors that open, stairs and the rest.")
			: tr("No interiors: only the outside, black windows and shut doors."), 6000);
	});
	m_LowPoly = new QCheckBox(tr("Low poly"), buildingBox);
	m_LowPoly->setToolTip(tr("Stone blocks and tiles, inside and out, as flat panels in a texture of them instead of a piece for "
		"every block: far fewer triangles, for a building seen mostly from afar or one of many."));
	form->addRow(QString(), m_LowPoly);
	connect(m_LowPoly, &QCheckBox::toggled, this, [this](bool on) {
		if(m_Refreshing || on == m_Plan.lowPoly)
			return;
		PushUndo();
		m_Plan.lowPoly = on;
		SetModified(true);
		ShowStatus(on ? tr("Low poly: blocks and tiles as textured panels.") : tr("Blocks and tiles built block by block."),
			6000);
	});
	form->addRow(tr("Cell size"), m_Cell);
	form->addRow(tr("Height"), m_Height);
	form->addRow(tr("Outside wall"), m_Wall);
	form->addRow(tr("Inside wall"), m_Partition);
	form->addRow(tr("Void gap"), m_VoidGap);
	form->addRow(tr("Columns"), m_Cols);
	form->addRow(tr("Rows"), m_Rows);
	m_Outside = new QComboBox(buildingBox);
	m_Outside->setToolTip(tr("The outside walls' texture. The building's material is what the game shows for what it is built "
		"of: wood siding for lumber, stone wall for stone, metal plate for metal."));
	m_Roofing = new QComboBox(buildingBox);
	m_Roofing->setToolTip(tr("The roofs' texture, tinted with the theme's roof colour."));
	auto sized = [&](QComboBox* box, QToolButton*& size, const QString& what) {
		QWidget* row = new QWidget(buildingBox);
		QHBoxLayout* h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		h->setSpacing(4);
		h->addWidget(box, 1);
		size = new QToolButton(row);
		size->setPopupMode(QToolButton::InstantPopup);
		size->setFixedWidth(46);
		QMenu* menu = new QMenu(size);
		AddSizeMenu(menu, [this, what](int percent) { SetBuildingScale(what, percent); });
		size->setMenu(menu);
		h->addWidget(size);
		return row;
	};
	form->addRow(tr("Outside walls"), sized(m_Outside, m_OutsideSize, "outside"));
	form->addRow(tr("Roofs"), sized(m_Roofing, m_RoofingSize, "roofing"));
	m_OutsideTrim = new QComboBox(buildingBox);
	m_OutsideTrim->setToolTip(tr("The dressing along the outside walls: the theme's (log courses, ashlar, ribs), another theme's, "
		"or none. The roofs' fascias and parapets, and dormers' and skylights' frames, take its colour and texture too."));
	m_OutsideTrimTexture = new QComboBox(buildingBox);
	m_OutsideTrimTexture->setToolTip(tr("The outside trims' texture: painted, or one of the theme's."));
	{
		QWidget* row = new QWidget(buildingBox);
		QHBoxLayout* h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		h->setSpacing(4);
		h->addWidget(m_OutsideTrimTexture, 1);
		m_OutsideTrimColor = new QToolButton(row);
		m_OutsideTrimColor->setPopupMode(QToolButton::InstantPopup);
		m_OutsideTrimColor->setFixedWidth(34);
		QMenu* menu = new QMenu(m_OutsideTrimColor);
		menu->addAction(tr("Choose a colour..."), this, [this]() {
			QColor c = QColorDialog::getColor(OutsideTrimColor(m_Plan).isValid() ? OutsideTrimColor(m_Plan) : QColor(0x8c, 0x6e, 0x50),
				this, tr("Colour"));
			if(c.isValid())
				SetOutsideTrimColor(c, m_Plan.outsideTrimStrength);
		});
		menu->addSeparator();
		for(int strength : {25, 50, 75, 100})
			menu->addAction(strength == 100 ? tr("Solid colour") : tr("%1% over the texture").arg(strength), this, [this, strength]() {
				SetOutsideTrimColor(OutsideTrimColor(m_Plan).isValid() ? OutsideTrimColor(m_Plan) : QColor(0x8c, 0x6e, 0x50), strength);
			});
		menu->addSeparator();
		menu->addAction(tr("The usual colour"), this, [this]() { SetOutsideTrimColor(QColor(), 50); });
		menu->addSeparator();
		AddSizeMenu(menu, [this](int percent) { SetBuildingScale("outsidetrim", percent); });
		m_OutsideTrimColor->setMenu(menu);
		h->addWidget(m_OutsideTrimColor);
		form->addRow(tr("Outside trims"), m_OutsideTrim);
		form->addRow(tr("Their texture"), row);
	}
	for(QComboBox* box : {m_OutsideTrim, m_OutsideTrimTexture})
		connect(box, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::TexturesEdited);
	for(QComboBox* box : {m_Outside, m_Roofing})
		connect(box, QOverload<int>::of(&QComboBox::activated), this, &EditorWindow::TexturesEdited);
	connect(m_Name, &QLineEdit::textEdited, this, &EditorWindow::SettingsEdited);
	for(QDoubleSpinBox* box : {m_Cell, m_Height, m_Wall, m_Partition, m_VoidGap})
		connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EditorWindow::SettingsEdited);
	for(QSpinBox* box : {m_Cols, m_Rows})
		connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, &EditorWindow::GridSizeEdited);
	rightLayout->addWidget(buildingBox);

	QDockWidget* rightDock = new QDockWidget(tr("Properties"), this);
	rightDock->setObjectName("building");
	rightDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	QScrollArea* rightScroll = new QScrollArea(rightDock);
	rightScroll->setWidget(right);
	rightScroll->setWidgetResizable(true);
	rightScroll->setFrameShape(QFrame::NoFrame);
	rightScroll->setMinimumWidth(right->sizeHint().width() + 20);
	rightDock->setWidget(rightScroll);
	addDockWidget(Qt::RightDockWidgetArea, rightDock);

	auto key = [this](const QKeySequence& keys, void (EditorWindow::*slot)()) {
		QAction* a = new QAction(this);
		a->setShortcut(keys);
		a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(a, &QAction::triggered, this, slot);
		addAction(a);
		return a;
	};
	m_UndoAction = key(QKeySequence::Undo, &EditorWindow::Undo);
	m_RedoAction = key(QKeySequence::Redo, &EditorWindow::Redo);
	key(QKeySequence(Qt::Key_PageUp), &EditorWindow::DeckAbove);
	key(QKeySequence(Qt::Key_PageDown), &EditorWindow::DeckBelow);
	UpdateActions();
}

void EditorWindow::PanelAction(int action)
{
	switch(action)
	{
	case DesignPanel::DeckAbove: DeckAbove(); break;
	case DesignPanel::DeckBelow: DeckBelow(); break;
	case DesignPanel::DeleteDeck: DeleteDeck(); break;
	}
}

void EditorWindow::StartNew()
{
	m_Plan = StarterPlan();
	m_Undo.clear();
	m_Redo.clear();
	m_View->SetDeck(0);
	RefreshAll();
	SetModified(false);
}

bool EditorWindow::LoadPlanText(const QString& text)
{
	QString problems;
	QTextStream log(&problems);
	Plan plan;
	if(!ParsePlanText(text, plan, log))
		return false;
	TidyPlan(plan);
	m_Plan = plan;
	m_Undo.clear();
	m_Redo.clear();
	m_View->SetDeck(0);
	RefreshAll();
	SetModified(false);
	return true;
}

void EditorWindow::PushUndo()
{
	m_Undo << m_Plan;
	if(m_Undo.size() > 200)
		m_Undo.removeFirst();
	m_Redo.clear();
	UpdateActions();
}

void EditorWindow::Undo()
{
	if(m_Undo.isEmpty())
		return;
	m_Redo << m_Plan;
	m_Plan = m_Undo.takeLast();
	RefreshAll();
	SetModified(true);
}

void EditorWindow::Redo()
{
	if(m_Redo.isEmpty())
		return;
	m_Undo << m_Plan;
	m_Plan = m_Redo.takeLast();
	RefreshAll();
	SetModified(true);
}

void EditorWindow::UpdateActions()
{
	if(m_UndoAction)
		m_UndoAction->setEnabled(!m_Undo.isEmpty());
	if(m_RedoAction)
		m_RedoAction->setEnabled(!m_Redo.isEmpty());
}

void EditorWindow::PlanEdited()
{
	SetModified(true);
	RefreshRooms();
	RefreshDoor();
	RefreshHatch();
	RefreshWindow();
	RefreshRoof();
}

void EditorWindow::SetModified(bool modified)
{
	if(modified)
		emit PlanChanged();
	setWindowModified(modified);
	UpdateActions();
}

void EditorWindow::ShowStatus(const QString& text, int)
{
	emit Status(text);
}

QChar EditorWindow::NextLetter() const
{
	for(QChar ch : GridView::RoomLetters())
		if(!m_Plan.rooms.contains(ch))
			return ch;
	return QChar();
}

QChar EditorWindow::CurrentRoom() const
{
	for(const Room& r : m_Plan.rooms)
		if(r.type == m_Panel->RoomType() && r.tag == m_Panel->Tag())
			return r.letter;
	return QChar();
}

void EditorWindow::RoomPicked(const QString& type, int tag)
{
	Q_UNUSED(type);
	Q_UNUSED(tag);
	RefreshRooms();
}

void EditorWindow::RoomListClicked()
{
	QListWidgetItem* item = m_Rooms->currentItem();
	if(!item)
		return;
	QChar letter = item->data(Qt::UserRole).toString().at(0);
	if(!m_Plan.rooms.contains(letter))
		return;
	const Room& room = m_Plan.rooms[letter];
	m_Panel->SetRoom(room.type, room.tag);
	m_Panel->SetTool(GridView::PaintRoom);
	m_View->SetTool(GridView::PaintRoom);
	int deck = m_View->Deck();
	Quarters(m_Plan, letter, &deck);
	ShowDeck(deck);
	RefreshRooms();
}

void EditorWindow::RoomNameEdited(const QString& name)
{
	QChar letter = CurrentRoom();
	if(m_Refreshing || letter.isNull() || name.trimmed().isEmpty())
		return;
	for(const Room& r : m_Plan.rooms)
		if(r.letter != letter && r.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
		{
			ShowStatus(tr("Another room is called %1; every room needs its own name.").arg(name.trimmed()), 6000);
			return;
		}
	PushUndo();
	m_Plan.rooms[letter].name = name.trimmed();
	RefreshRooms();
	SetModified(true);
}

void EditorWindow::RoomLightsEdited()
{
	QChar letter = CurrentRoom();
	if(m_Refreshing || letter.isNull() || !m_Plan.rooms.contains(letter))
		return;
	PushUndo();
	m_Plan.rooms[letter].lights = m_RoomLights->currentData().toString();
	m_Plan.rooms[letter].sunlit = m_RoomSunlit->isChecked();
	SetModified(true);
}

void EditorWindow::RefreshRooms()
{
	QString type = m_Panel->RoomType();
	int tag = m_Panel->Tag();
	const RoomKind* kind = FindRoomKind(type);
	QChar letter = CurrentRoom();
	Room pending;
	if(letter.isNull())
	{
		letter = NextLetter();
		QString name = DefaultRoomName(type, tag);
		for(int n = 2; ; n++)
		{
			bool taken = false;
			for(const Room& r : m_Plan.rooms)
				taken = taken || r.name.compare(name, Qt::CaseInsensitive) == 0;
			if(!taken)
				break;
			name = QString("%1 (%2)").arg(DefaultRoomName(type, tag)).arg(n);
		}
		pending = Room{letter, name, type, tag};
	}
	m_View->SetCurrentRoom(letter, pending);

	m_Refreshing = true;
	bool exists = m_Plan.rooms.contains(letter);
	int deck = -1;
	int quarters = exists ? Quarters(m_Plan, letter, &deck) : 0;
	QString what = kind ? kind->name : type;
	auto where = [&](QChar room, int quarters, int deck) {
		QList<int> decks = IsLiftRoom(m_Plan, room) ? LiftDecks(m_Plan, room) : QList<int>();
		if(decks.size() > 1)
			return tr("%1 %2 a deck, decks %3 to %4").arg(quarters / 4.0 / decks.size())
				.arg(quarters == 4 * decks.size() ? tr("cell") : tr("cells")).arg(decks.first()).arg(decks.last());
		return tr("%1 %2 on deck %3").arg(quarters / 4.0).arg(quarters == 4 ? tr("cell") : tr("cells")).arg(deck);
	};
	if(exists)
		m_RoomLabel->setText(tr("<b>%1 %2</b>: %3").arg(what).arg(tag).arg(where(letter, quarters, deck)));
	else
		m_RoomLabel->setText(tr("<b>%1 %2</b>: not painted yet").arg(what).arg(tag));
	m_RoomName->setEnabled(exists);
	if(!m_RoomName->hasFocus())
		m_RoomName->setText(exists ? m_Plan.rooms[letter].name : pending.name);
	m_RoomLights->setEnabled(exists);
	m_RoomSunlit->setEnabled(exists);
	m_RoomLights->setCurrentIndex(qMax(0, m_RoomLights->findData(exists ? m_Plan.rooms[letter].lights : QString("bright"))));
	m_RoomSunlit->setChecked(exists ? m_Plan.rooms[letter].sunlit : true);
	{
		Room usual = exists ? m_Plan.rooms[letter] : pending;
		const Room picked = usual;
		usual.floor.clear();
		usual.walls.clear();
		usual.ceiling.clear();
		FillTextures(m_RoomFloor, RoomTexture(m_Plan, usual, "floor"), picked.floor, false);
		FillTextures(m_RoomWalls, RoomTexture(m_Plan, usual, "walls"), picked.walls, false);
		FillTextures(m_RoomCeiling, RoomTexture(m_Plan, usual, "ceiling"), picked.ceiling, false);
		FillTextures(m_RoomTrim, QString(), picked.trim, false);
		m_RoomTrim->setItemText(0, tr("Painted, in the theme's colour"));
		m_RoomDesign->clear();
		const RoomKind* own = FindRoomKind(picked.type);
		m_RoomDesign->addItem(tr("Its own: %1").arg(own ? own->name : picked.type), QString());
		m_RoomDesign->addItem(tr("None"), "none");
		for(const QString& key : RoomDesignKinds())
			if(const RoomKind* k = FindRoomKind(key))
			{
				m_RoomDesign->addItem(tr("A %1's").arg(k->name.toLower()), key);
				m_RoomDesign->setItemData(m_RoomDesign->count() - 1, k->note, Qt::ToolTipRole);
			}
		m_RoomDesign->setCurrentIndex(qMax(0, m_RoomDesign->findData(picked.design)));
		for(QComboBox* box : {m_RoomFloor, m_RoomWalls, m_RoomCeiling, m_RoomTrim, m_RoomDesign})
			box->setEnabled(exists);
		for(auto ci = m_RoomColors.begin(); ci != m_RoomColors.end(); ci++)
		{
			ShowColor(ci.value(), RoomColor(picked, ci.key()), picked.strengths.value(ci.key(), 50), false,
				picked.scales.value(ci.key(), 100));
			ci.value()->setEnabled(exists);
		}
	}

	m_Rooms->clear();
	for(const Room& room : m_Plan.rooms)
	{
		int d = 0;
		int q = Quarters(m_Plan, room.letter, &d);
		const RoomKind* k = FindRoomKind(room.type);
		QListWidgetItem* item = new QListWidgetItem(PaletteIcon(RoomKindIcon(room.type)),
			QString("%1   (%2 %3, %4)").arg(room.name).arg(k ? k->name : room.type).arg(room.tag).arg(where(room.letter, q, d)), m_Rooms);
		item->setData(Qt::UserRole, QString(room.letter));
		if(room.letter == letter)
			m_Rooms->setCurrentItem(item);
	}
	m_Refreshing = false;
}

void EditorWindow::RefreshSettings()
{
	m_Refreshing = true;
	m_Name->setText(m_Plan.name);
	m_Interiors->setChecked(m_Plan.interiors);
	m_LowPoly->setChecked(m_Plan.lowPoly);
	m_Panel->SetTheme(m_Plan.theme);
	m_Cell->setValue(m_Plan.cell);
	m_Height->setValue(m_Plan.height);
	m_Wall->setValue(m_Plan.wall);
	m_Partition->setValue(m_Plan.partition);
	m_VoidGap->setValue(m_Plan.voidGap);
	m_Cols->setValue(m_Plan.Cols());
	m_Rows->setValue(m_Plan.Rows());
	FillTextures(m_Outside, ThemeTexture(m_Plan.theme, "outside"), m_Plan.outside, true);
	FillTextures(m_Roofing, ThemeTexture(m_Plan.theme, "roofing"), m_Plan.roofing, false);
	m_OutsideTrim->clear();
	const ThemeKind* own = FindThemeKind(m_Plan.theme);
	m_OutsideTrim->addItem(m_Plan.theme == "plain" ? tr("The theme's: none") : tr("The theme's: %1").arg(own ? own->name : m_Plan.theme),
		QString());
	m_OutsideTrim->addItem(tr("None"), "none");
	for(const ThemeKind& k : ThemeKinds())
		if(k.key != "plain")
		{
			m_OutsideTrim->addItem(k.name, k.key);
			m_OutsideTrim->setItemData(m_OutsideTrim->count() - 1, k.note, Qt::ToolTipRole);
		}
	m_OutsideTrim->setCurrentIndex(qMax(0, m_OutsideTrim->findData(m_Plan.outsideTrim)));
	FillTextures(m_OutsideTrimTexture, QString(), m_Plan.outsideTrimTexture, false);
	m_OutsideTrimTexture->setItemText(0, tr("Painted"));
	ShowColor(m_OutsideTrimColor, OutsideTrimColor(m_Plan), m_Plan.outsideTrimStrength, true, m_Plan.scales.value("outsidetrim", 100));
	for(QToolButton* b : {m_OutsideSize, m_RoofingSize})
	{
		const int percent = m_Plan.scales.value(b == m_OutsideSize ? "outside" : "roofing", 100);
		b->setText(QString("%1%").arg(percent));
		b->setToolTip(tr("The texture %1% of its usual size. Click to change it.").arg(percent));
	}
	m_Refreshing = false;
}

void EditorWindow::AddSizeMenu(QMenu* menu, const std::function<void(int)>& set)
{
	menu->addSection(tr("Texture size"));
	for(int percent : {50, 75, 100, 150, 200, 300})
		menu->addAction(percent == 100 ? tr("Its usual size") : tr("%1%").arg(percent), this, [set, percent]() { set(percent); });
	menu->addAction(tr("Another size..."), this, [this, set]() {
		bool ok = false;
		const int percent = QInputDialog::getInt(this, tr("Texture size"), tr("Percent of its usual size:"), 100, 10, 1000, 10, &ok);
		if(ok)
			set(percent);
	});
}

void EditorWindow::SetRoomScale(const QString& surface, int percent)
{
	QChar letter = CurrentRoom();
	if(letter.isNull() || !m_Plan.rooms.contains(letter))
		return;
	PushUndo();
	if(percent == 100)
		m_Plan.rooms[letter].scales.remove(surface);
	else
		m_Plan.rooms[letter].scales[surface] = qBound(10, percent, 1000);
	SetModified(true);
	RefreshRooms();
}

void EditorWindow::SetBuildingScale(const QString& what, int percent)
{
	PushUndo();
	if(percent == 100)
		m_Plan.scales.remove(what);
	else
		m_Plan.scales[what] = qBound(10, percent, 1000);
	SetModified(true);
	RefreshSettings();
}

void EditorWindow::ShowColor(QToolButton* button, const QColor& c, int strength, bool usualIsPaint, int scale)
{
	QPixmap swatch(18, 14);
	swatch.fill(Qt::transparent);
	QPainter p(&swatch);
	p.setPen(QColor(0x9a, 0xa4, 0xb0));
	if(c.isValid())
	{
		p.fillRect(QRectF(0, 0, 18, 14), QColor(0x60, 0x66, 0x70));
		p.fillRect(QRectF(0, 0, 18.0 * strength / 100, 14), c);
	}
	else
		p.drawLine(0, 13, 17, 0);
	p.drawRect(0, 0, 17, 13);
	p.end();
	button->setIcon(QIcon(swatch));
	button->setToolTip((c.isValid() ? tr("Colour %1, %2% over the texture (100%: solid).").arg(c.name()).arg(strength)
		: usualIsPaint ? tr("The usual colour (the theme's paint).") : tr("No colour: the texture as it is."))
		+ " " + tr("The texture %1% of its usual size. Click to change them.").arg(scale));
}

void EditorWindow::SetOutsideTrimColor(const QColor& color, int strength)
{
	PushUndo();
	m_Plan.outsideTrimColor = color.isValid() ? color.name() : QString();
	m_Plan.outsideTrimStrength = color.isValid() ? qBound(0, strength, 100) : 50;
	SetModified(true);
	RefreshSettings();
}

void EditorWindow::FillTextures(QComboBox* box, const QString& usual, const QString& picked, bool material)
{
	const bool was = m_Refreshing;
	m_Refreshing = true;
	box->clear();
	const TextureKind* u = FindTextureKind(usual);
	box->addItem(tr("The usual: %1").arg(u ? u->name : usual), QString());
	for(const QString& key : ThemePalette(m_Plan.theme))
	{
		if(key == "material" && !material)
			continue;
		const TextureKind* k = FindTextureKind(key);
		box->addItem(k ? k->name : key, key);
	}
	box->setCurrentIndex(qMax(0, box->findData(picked)));
	m_Refreshing = was;
}

void EditorWindow::SetRoomColor(const QString& surface, const QColor& color, int strength)
{
	QChar letter = CurrentRoom();
	if(letter.isNull() || !m_Plan.rooms.contains(letter))
		return;
	PushUndo();
	Room& r = m_Plan.rooms[letter];
	if(color.isValid())
	{
		r.colors[surface] = color.name();
		if(strength == 50)
			r.strengths.remove(surface);
		else
			r.strengths[surface] = qBound(0, strength, 100);
	}
	else
	{
		r.colors.remove(surface);
		r.strengths.remove(surface);
	}
	SetModified(true);
	RefreshRooms();
}

void EditorWindow::TexturesEdited()
{
	if(m_Refreshing)
		return;
	PushUndo();
	QChar letter = CurrentRoom();
	if(!letter.isNull() && m_Plan.rooms.contains(letter))
	{
		m_Plan.rooms[letter].floor = m_RoomFloor->currentData().toString();
		m_Plan.rooms[letter].walls = m_RoomWalls->currentData().toString();
		m_Plan.rooms[letter].ceiling = m_RoomCeiling->currentData().toString();
		m_Plan.rooms[letter].trim = m_RoomTrim->currentData().toString();
		m_Plan.rooms[letter].design = m_RoomDesign->currentData().toString();
	}
	m_Plan.outsideTrim = m_OutsideTrim->currentData().toString();
	m_Plan.outsideTrimTexture = m_OutsideTrimTexture->currentData().toString();
	m_Plan.outside = m_Outside->currentData().toString();
	m_Plan.roofing = m_Roofing->currentData().toString();
	SetModified(true);
}

void EditorWindow::SettingsEdited()
{
	if(m_Refreshing)
		return;
	PushUndo();
	if(!m_Name->text().trimmed().isEmpty())
		m_Plan.name = m_Name->text().trimmed();
	m_Plan.cell = m_Cell->value();
	m_Plan.height = m_Height->value();
	m_Plan.wall = m_Wall->value();
	m_Plan.partition = m_Partition->value();
	m_Plan.voidGap = m_VoidGap->value();
	SetModified(true);
}

void EditorWindow::ThemeChosen(const QString& key)
{
	if(m_Refreshing || key == m_Plan.theme)
		return;
	PushUndo();
	m_Plan.theme = key;
	SetModified(true);
	RefreshDoor();
	RefreshHatch();
	RefreshWindow();
	RefreshRoof();
	RefreshRooms();
	RefreshSettings();
	m_View->update();
	for(const ThemeKind& k : ThemeKinds())
		if(k.key == key)
			ShowStatus(k.note, 8000);
}

void EditorWindow::GridSizeEdited()
{
	if(m_Refreshing)
		return;
	PushUndo();
	int cols = m_Cols->value(), rows = m_Rows->value();
	for(QStringList& deck : m_Plan.decks)
	{
		QStringList grid;
		for(int r = 0; r < rows; r++)
		{
			QString row = r < deck.size() ? deck[r] : QString();
			row = row.left(cols * 4);
			row += QString(cols * 4 - row.size(), '.');
			grid << row;
		}
		deck = grid;
	}
	TidyPlan(m_Plan);
	m_View->PlanResized();
	RefreshRooms();
	SetModified(true);
}

void EditorWindow::ShowDeck(int deck)
{
	m_View->SetDeck(deck);
	RefreshDeck();
}

void EditorWindow::DeckAbove()
{
	int deck = m_View->Deck();
	if(deck + 1 < m_Plan.Decks())
	{
		ShowDeck(deck + 1);
		return;
	}
	if(m_Plan.Decks() >= 64)
		return;
	PushUndo();
	QStringList grid;
	for(int r = 0; r < m_Plan.Rows(); r++)
		grid << QString(m_Plan.Cols() * 4, '.');
	m_Plan.decks << grid;
	ShowDeck(m_Plan.Decks() - 1);
	SetModified(true);
	ShowStatus(tr("Deck %1 added. The deck below shows faintly under its empty cells.").arg(m_Plan.Decks() - 1), 6000);
}

void EditorWindow::DeckBelow()
{
	int deck = m_View->Deck();
	if(deck == 0)
	{
		ShowStatus(tr("This is the ground deck."), 6000);
		return;
	}
	bool empty = true;
	for(const QString& row : m_Plan.decks[deck])
		empty = empty && row.count('.') == row.size();
	if(empty && deck == m_Plan.Decks() - 1)
	{
		m_Plan.decks.removeLast();
		TidyPlan(m_Plan);
		m_View->PlanResized();
		SetModified(true);
	}
	ShowDeck(deck - 1);
}

void EditorWindow::DeleteDeck()
{
	int deck = m_View->Deck();
	if(m_Plan.Decks() < 2)
	{
		ShowStatus(tr("A building needs at least one deck."), 6000);
		return;
	}
	PushUndo();
	m_Plan.decks.removeAt(deck);
	auto shift = [&](auto& list) {
		for(int i = list.size() - 1; i >= 0; i--)
		{
			if(list[i].deck == deck)
				list.removeAt(i);
			else if(list[i].deck > deck)
				list[i].deck--;
		}
	};
	shift(m_Plan.doors);
	shift(m_Plan.hatches);
	shift(m_Plan.windows);
	shift(m_Plan.posts);
	shift(m_Plan.stations);
	shift(m_Plan.fixtures);
	shift(m_Plan.gaps);
	if(deck < m_Plan.roofGrids.size())
		m_Plan.roofGrids.removeAt(deck);
	TidyPlan(m_Plan);
	m_View->PlanResized();
	ShowDeck(qMin(deck, m_Plan.Decks() - 1));
	RefreshRooms();
	SetModified(true);
	ShowStatus(tr("Deck %1 deleted.").arg(deck), 6000);
}

void EditorWindow::RefreshDeck()
{
	int deck = m_View->Deck();
	QString where = deck == 0 ? tr("on the ground") : deck == m_Plan.Decks() - 1 ? tr("the top") : tr("between");
	emit DeckShown(tr("Deck %1, %2 (%3 %4)").arg(deck).arg(where).arg(m_Plan.Decks())
		.arg(m_Plan.Decks() == 1 ? tr("deck") : tr("decks")));
	m_View->update();
}

void EditorWindow::RefreshAll()
{
	RefreshSettings();
	m_View->PlanResized();
	RefreshRooms();
	RefreshDeck();
	RefreshDoor();
	RefreshHatch();
	RefreshWindow();
	RefreshRoof();
	RefreshRailing();
	RefreshStair();
	RefreshDormer();
	RefreshSkylight();
	UpdatePanels();
	UpdateActions();
}
