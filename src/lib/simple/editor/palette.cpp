#include "palette.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QStyleFactory>
#include <QToolButton>
#include <QVBoxLayout>

using namespace SimpleMode;

namespace {

const int ButtonPx = 24;

QString NotYet(const QString& what)
{
	return QObject::tr("<b>%1</b><br>Not available yet.").arg(what);
}

}

static QIcon DrawnIcon(const QString& name)
{
	QPixmap pm(32, 32);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	const QColor roof(0x8a, 0x94, 0xa4), wall(0xc8, 0x9a, 0x68), glass(0x9a, 0xd8, 0xff), edge(0x20, 0x24, 0x2a);
	p.setPen(QPen(edge, 1.5));
	if(name == "dormer")
	{
		p.setBrush(roof);
		p.drawPolygon(QPolygonF() << QPointF(2, 30) << QPointF(16, 6) << QPointF(30, 30));
		p.setBrush(wall);
		p.drawRect(QRectF(10, 17, 12, 11));
		p.drawPolygon(QPolygonF() << QPointF(8, 18) << QPointF(16, 9) << QPointF(24, 18));
		p.setBrush(glass);
		p.drawRect(QRectF(13, 19, 6, 6));
	}
	else if(name == "skylight")
	{
		p.setBrush(roof);
		p.drawPolygon(QPolygonF() << QPointF(2, 28) << QPointF(10, 4) << QPointF(30, 4) << QPointF(22, 28));
		p.setBrush(glass);
		p.drawPolygon(QPolygonF() << QPointF(10, 22) << QPointF(14, 10) << QPointF(24, 10) << QPointF(20, 22));
	}
	else if(name == "berth")
	{
		p.setBrush(QColor(0x80, 0x90, 0xb0));
		p.drawRect(QRectF(10, 3, 12, 26));
		p.setBrush(QColor(0xe8, 0xe8, 0xe0));
		p.drawRect(QRectF(11.5, 4.5, 9, 6));
	}
	else if(name == "parking")
	{
		p.setPen(QPen(QColor(0xe0, 0xe4, 0xea), 1.8, Qt::DashLine));
		p.drawRect(QRectF(5, 3, 22, 26));
		p.setPen(QPen(QColor(0xe0, 0xe4, 0xea), 1));
		QFont font = p.font();
		font.setBold(true);
		font.setPixelSize(14);
		p.setFont(font);
		p.drawText(QRectF(5, 3, 22, 26), Qt::AlignCenter, "P");
	}
	else if(name == "light")
	{
		p.setBrush(QColor(0x60, 0x66, 0x70));
		p.drawRect(QRectF(4, 14, 8, 4));
		p.setBrush(QColor(0xff, 0x50, 0x30));
		p.drawEllipse(QPointF(18, 16), 6, 6);
		p.setPen(QPen(QColor(0xff, 0x90, 0x60), 1.2));
		for(int k = -1; k <= 1; k++)
			p.drawLine(QPointF(26, 16 + k * 6), QPointF(30, 16 + k * 9));
	}
	else if(name == "console")
	{
		p.setBrush(QColor(0x5a, 0xd8, 0xff));
		p.drawRoundedRect(QRectF(5, 4, 22, 8), 2, 2);
		p.setBrush(QColor(0x3a, 0x8a, 0xb0));
		p.drawEllipse(QPointF(16, 21), 6.5, 6.5);
	}
	else if(name == "gap")
	{
		const QColor rail(0xd8, 0xb0, 0x78);
		p.setPen(QPen(rail, 1.8));
		for(double x0 : {2.0, 21.0})
		{
			p.drawLine(QPointF(x0, 10), QPointF(x0 + 9, 10));
			for(double x = x0; x <= x0 + 9; x += 3)
				p.drawLine(QPointF(x, 10), QPointF(x, 26));
		}
		p.setPen(QPen(QColor(0x4c, 0xd0, 0x6a), 1.5, Qt::DashLine));
		p.drawLine(QPointF(13, 26), QPointF(19, 26));
	}
	else if(name == "balcony" || name == "platform" || name == "catwalk")
	{
		const QColor floor(0xb8, 0xa6, 0x8c), rail(0xd8, 0xb0, 0x78), bars(0x9c, 0xa4, 0xb0);
		const bool cat = name == "catwalk";
		double x0 = name == "balcony" ? 9 : 3, x1 = 29, top = cat ? 17 : 18, bottom = cat ? 19 : 21;
		if(name == "balcony")
		{
			p.setBrush(wall);
			p.drawRect(QRectF(2, 3, 7, 27));
		}
		if(name == "platform")
		{
			p.setBrush(floor);
			p.drawRect(QRectF(5, 21, 2.5, 9));
			p.drawRect(QRectF(24.5, 21, 2.5, 9));
		}
		p.setBrush(floor);
		p.drawRect(QRectF(x0, top, x1 - x0, bottom - top));
		p.setPen(QPen(cat ? bars : rail, cat ? 1.2 : 1.8));
		p.drawLine(QPointF(x0 + 1, 8), QPointF(x1 - 1, 8));
		for(double x = x0 + 1; x <= x1 - 0.5; x += cat ? 2.5 : 4)
			p.drawLine(QPointF(x, 8), QPointF(x, top));
	}
	else if(name == "turret")
	{
		const QColor steel(0x9c, 0xa4, 0xb0), dark(0x5a, 0x60, 0x68);
		p.setBrush(dark);
		p.drawRect(QRectF(5, 23, 18, 5));
		p.setBrush(steel);
		p.drawRect(QRectF(7, 13, 14, 10));
		p.drawRect(QRectF(20, 16, 10, 3));
	}
	else if(name == "turbolift")
	{
		const QColor shaft(0x9c, 0xa4, 0xb0), car(0xd8, 0xb0, 0x78), arrow(0x9a, 0xd8, 0xff);
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(shaft, 2));
		p.drawRect(QRectF(6, 2, 14, 28));
		p.setPen(QPen(edge, 1.2));
		p.setBrush(car);
		p.drawRect(QRectF(8, 14, 10, 10));
		p.setPen(Qt::NoPen);
		p.setBrush(arrow);
		p.drawPolygon(QPolygonF() << QPointF(22, 12) << QPointF(26, 5) << QPointF(30, 12));
		p.drawPolygon(QPolygonF() << QPointF(22, 20) << QPointF(26, 27) << QPointF(30, 20));
	}
	return QIcon(pm);
}

QIcon PaletteIcon(const QString& name)
{
	if(name == "dormer" || name == "skylight" || name == "balcony" || name == "platform" || name == "catwalk" || name == "gap" || name == "console" || name == "berth"
			|| name == "parking" || name == "light" || name == "turbolift" || name == "turret")
		return DrawnIcon(name);
	return QIcon(QString(":/icons/%1.png").arg(name));
}

QString RoomKindIcon(const QString& key)
{
	static const QMap<QString, QString> icons = {
		{"hall", "dp_Corridor"}, {"stateroom", "dp_Stateroom"}, {"crewcabin", "dp_CrewCabin"}, {"officercabin", "dp_CaptCabin"},
		{"lounge", "dp_Lounge"}, {"galley", "dp_Galley"}, {"hold", "dp_Cargo"}, {"barracks", "dp_Barracks"}, {"brig", "dp_Brig"},
		{"sickbay", "dp_SickBay"}, {"engineroom", "dp_EngineRoom"}, {"vehiclebay", "dp_LandingBay"},
		{"gravitywell", "dp_GravityWell"}, {"opentobelow", "dp_OpenToBelow"}, {"roofgable", "dp_RoofGable"},
		{"roofhip", "dp_RoofHip"}, {"roofround", "dp_RoofRound"},
		{"balcony", "balcony"}, {"platform", "platform"}, {"catwalk", "catwalk"},
		{"turbolift", "turbolift"},
	};
	return icons.value(key);
}

void ApplyDesignerTheme(QApplication& app)
{
	app.setStyle(QStyleFactory::create("Fusion"));
	QPalette pal;
	pal.setColor(QPalette::Window, QColor(0x10, 0x12, 0x16));
	pal.setColor(QPalette::WindowText, QColor(0xdc, 0xe2, 0xec));
	pal.setColor(QPalette::Base, QColor(0x08, 0x0a, 0x0e));
	pal.setColor(QPalette::AlternateBase, QColor(0x16, 0x1a, 0x21));
	pal.setColor(QPalette::Text, QColor(0xdc, 0xe2, 0xec));
	pal.setColor(QPalette::Button, QColor(0x1a, 0x1f, 0x28));
	pal.setColor(QPalette::ButtonText, QColor(0xdc, 0xe2, 0xec));
	pal.setColor(QPalette::Highlight, QColor(0x2a, 0x4d, 0x8c));
	pal.setColor(QPalette::HighlightedText, Qt::white);
	pal.setColor(QPalette::ToolTipBase, QColor(0x0c, 0x14, 0x24));
	pal.setColor(QPalette::ToolTipText, QColor(0xe6, 0xec, 0xf5));
	pal.setColor(QPalette::PlaceholderText, QColor(0x7a, 0x84, 0x94));
	pal.setColor(QPalette::Disabled, QPalette::Text, QColor(0x5c, 0x63, 0x6e));
	pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x5c, 0x63, 0x6e));
	pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5c, 0x63, 0x6e));
	app.setPalette(pal);
	app.setStyleSheet(
		"QToolTip { border: 1px solid #3d5d8f; padding: 4px; }"
		"QDockWidget::title { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b5b9a, stop:0.5 #1c2c52, stop:1 #0a1020);"
		"  padding: 3px 6px; color: #e6ecf5; }");
}

DesignPanel::DesignPanel(QWidget* parent) : QWidget(parent)
{
	setObjectName("designPanel");

	m_Tools = new QButtonGroup(this);
	m_Types = new QButtonGroup(this);
	connect(m_Tools, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &DesignPanel::ToolChosen);

	QVBoxLayout* outer = new QVBoxLayout(this);
	outer->setContentsMargins(6, 6, 6, 6);
	outer->setSpacing(6);

	QWidget* left = new QWidget(this);
	QGridLayout* tools = new QGridLayout(left);
	tools->setContentsMargins(0, 0, 0, 0);
	tools->setSpacing(1);
	auto action = [&](QToolButton* b, Action a) { connect(b, &QToolButton::clicked, this, [this, a]() { emit ActionTriggered(a); }); };
	QToolButton* b;
	tools->addWidget(Button(left, "dp_RenderMode", NotYet(tr("Render mode")), false), 1, 0);
	tools->addWidget(Button(left, "dp_Gravity", NotYet(tr("Gravity")), false), 1, 1);
	b = Button(left, "dp_DeckAbove", tr("<b>Deck above</b> (Page Up)<br>Above the top deck, adds a new deck."));
	action(b, DeckAbove);
	tools->addWidget(b, 2, 0);
	b = Button(left, "dp_DeckBelow", tr("<b>Deck below</b> (Page Down)<br>Leaving an empty top deck removes it."));
	action(b, DeckBelow);
	tools->addWidget(b, 3, 0);
	b = Button(left, "dp_DelDeck", tr("<b>Delete deck</b><br>Removes this deck and everything on it. The decks above move down."));
	action(b, DeleteDeck);
	tools->addWidget(b, 2, 4);
	tools->addWidget(Button(left, "dp_Move", NotYet(tr("Move design")), false), 3, 4);
	struct { Line line; const char* icon; QString tip; int row, col; } lines[] = {
		{Vertical, "dp_Grid90", tr("<b>Vertical grid lines</b><br>A click paints up to the nearest lines that are on."), 2, 1},
		{Horizontal, "dp_Grid0", tr("<b>Horizontal grid lines</b><br>A click paints up to the nearest lines that are on."), 2, 2},
		{Slash, "dp_Grid45", tr("<b>Diagonal lines toward the port quarter</b> (/)<br>Switch on to paint triangles."), 3, 1},
		{Backslash, "dp_Grid135", tr("<b>Diagonal lines toward the starboard quarter</b> (\\)<br>Switch on to paint triangles."), 3, 2},
	};
	for(const auto& l : lines)
	{
		b = Button(left, l.icon, l.tip);
		b->setCheckable(true);
		m_LineButtons[l.line] = b;
		tools->addWidget(b, l.row, l.col);
		Line line = l.line;
		connect(b, &QToolButton::toggled, this, [this, line](bool on) { emit LineToggled(line, on); });
	}
	for(int c = 0; c < 5; c++)
		tools->setColumnMinimumWidth(c, ButtonPx);
	m_Theme = new QComboBox(left);
	m_Theme->setObjectName("material");
	m_Theme->setIconSize(QSize(16, 16));
	static const QMap<QString, QString> themeIcons = {{"wood", "c_Wood"}, {"stone", "c_Stone"}, {"metal", "c_Metal"}, {"plain", "c_Plastic"}};
	for(int i = 0; i < ThemeKinds().size(); i++)
	{
		const ThemeKind& k = ThemeKinds()[i];
		m_Theme->addItem(PaletteIcon(themeIcons.value(k.key)), k.name, k.key);
		m_Theme->setItemData(i, k.note, Qt::ToolTipRole);
	}
	m_Theme->setToolTip(tr("<b>Theme</b><br>What the building is built of. It sets the shapes of every room's wall design, and of "
		"the outside."));
	connect(m_Theme, QOverload<int>::of(&QComboBox::activated), this, [this](int i) { emit ThemeChosen(m_Theme->itemData(i).toString()); });
	tools->addWidget(m_Theme, 4, 0, 1, 5);
	tools->setRowMinimumHeight(4, 30);

	QGroupBox* rooms = new QGroupBox(tr("Rooms"), this);
	QGridLayout* rg = new QGridLayout(rooms);
	rg->setContentsMargins(6, 8, 6, 6);
	rg->setSpacing(0);
	rg->addWidget(ToolButton(rooms, GridView::PaintRoom, "dp_RoomMode0", tr("<b>Paint room</b><br>Click or drag to give the shape "
		"under the cursor to the room picked here: its type and its tag.")), 0, 0);
	rg->addWidget(ToolButton(rooms, GridView::Erase, "dp_DelRoomCells", tr("<b>Erase room cells</b><br>Click or drag to empty the "
		"shape under the cursor.")), 1, 0);
	rg->addWidget(ToolButton(rooms, GridView::DeleteRoom, "dp_DelRoom", tr("<b>Delete room</b><br>Click a room to remove all of it.")),
		2, 0);
	rg->setColumnMinimumWidth(1, 8);
	const char* layout[4][5] = {
		{"barracks", "brig", "engineroom", "vehiclebay", NULL},
		{"gravitywell", "hall", "hold", "opentobelow", "lounge"},
		{"galley", "sickbay", "stateroom", "crewcabin", "officercabin"},
		{"balcony", "platform", "catwalk", "turbolift", NULL},
	};
	for(int r = 0; r < 4; r++)
		for(int c = 0; c < 5; c++)
		{
			if(layout[r][c] == NULL)
				continue;
			const RoomKind* kind = FindRoomKind(layout[r][c]);
			b = Button(rooms, RoomKindIcon(kind->key), QString("<b>%1</b><br>%2").arg(kind->name).arg(kind->note), kind->buildable);
			b->setCheckable(true);
			m_Types->addButton(b);
			m_TypeButtons[kind->key] = b;
			rg->addWidget(b, r, c + 2);
			QString key = kind->key;
			connect(b, &QToolButton::clicked, this, [this, key]() {
				m_Type = key;
				SetTool(GridView::PaintRoom);
				emit ToolChosen(GridView::PaintRoom);
				emit RoomChosen(m_Type, m_Tag);
			});
			if(kind->key == m_Type)
				b->setChecked(true);
		}
	rg->setColumnMinimumWidth(7, 12);
	rg->setColumnStretch(7, 1);
	QString tagTip = tr("<b>Room tag</b><br>Tells rooms of the same type apart: Lounge 1 and Lounge 2 are two rooms. Pick a type and "
		"a tag, then paint.");
	b = Button(rooms, "dp_RoomIdUp", tagTip);
	connect(b, &QToolButton::clicked, this, [this]() { ChangeTag(1); });
	rg->addWidget(b, 0, 8);
	m_TagLabel = new QLabel("1", rooms);
	m_TagLabel->setObjectName("tag");
	m_TagLabel->setAlignment(Qt::AlignCenter);
	m_TagLabel->setFixedSize(ButtonPx, ButtonPx);
	m_TagLabel->setToolTip(tagTip);
	rg->addWidget(m_TagLabel, 1, 8);
	b = Button(rooms, "dp_RoomIdDown", tagTip);
	connect(b, &QToolButton::clicked, this, [this]() { ChangeTag(-1); });
	rg->addWidget(b, 2, 8);

	QHBoxLayout* top = new QHBoxLayout;
	top->setSpacing(8);
	top->addWidget(left, 0, Qt::AlignTop);
	top->addWidget(rooms, 1);
	outer->addLayout(top);

	QGroupBox* equipment = new QGroupBox(tr("Equipment"), this);
	QGridLayout* eg = new QGridLayout(equipment);
	eg->setContentsMargins(6, 8, 6, 6);
	eg->setSpacing(0);
	eg->addWidget(ToolButton(equipment, GridView::EraseDetail, "dp_DelDetail", tr("<b>Erase equipment</b><br>Click a door, opening, "
		"window, railing, hatch, post or roof to remove it.")), 0, 0);
	eg->addWidget(ToolButton(equipment, GridView::PlaceLightPole, "dp_LightPole", tr("<b>Light pole</b><br>Click a cell of the "
		"ground outside for a pole with a light on top (a rotating beacon; right-click it for a navigation light or a strobe).")), 1, 2);
	eg->addWidget(ToolButton(equipment, GridView::PlaceFlagPole, "dp_FlagPole", tr("<b>Flag pole</b><br>Click a cell of the "
		"ground outside for a pole flying the empire's flag; right-click it to turn the flag.")), 1, 3);
	eg->addWidget(ToolButton(equipment, GridView::PlaceBerth, "berth", tr("<b>Berth</b><br>Click a room cell for a bed (crew, "
		"captain, officer, troop or passenger: the Fixture panel); right-click it to turn it. The game counts each as a bed.")), 0, 2);
	eg->addWidget(ToolButton(equipment, GridView::PlaceParking, "parking", tr("<b>Parking spot</b><br>Click a room cell for where "
		"a vehicle driven into the room parks: ground (6 x 12 m), water, rocket or space vehicle (the Fixture panel). The whole "
		"spot must fit in the room, so paint a vehicle bay big enough, with a door vehicles can use. A spacecraft's spot goes on "
		"the ground outside. Right-click it to turn it.")), 0, 3);
	eg->addWidget(ToolButton(equipment, GridView::PlaceLight, "light", tr("<b>Light</b><br>Click near an outside wall of a room "
		"cell for a light on it, or near the middle of the cell for one on the (flat) roof: a navigation light, a rotating beacon "
		"or a strobe, in a colour (the Fixture panel); right-click it for the next kind.")), 0, 4);
	eg->addWidget(ToolButton(equipment, GridView::PlaceGameTable, "dp_GameTable", tr("<b>Game table</b><br>Click a room cell for a "
		"card table the game draws, with four seats round it: it takes a 4 m square of the room's floor.")), 1, 4);
	eg->addWidget(ToolButton(equipment, GridView::PlaceTurret, "turret", tr("<b>Turret</b><br>A battery against spacecraft, two cells "
		"by two, its gun raised to the sky: click the north-west cell of four room cells with open sky over them for one on "
		"their roof, or of four cells of the ground outside for one standing there. A heavy gun, a twin gun, a beam projector "
		"or a missile launcher (the Fixture panel); right-click it to turn it. A turret gunner station (the Station tool) works it; with "
		"none, the building's crew fire it at enemies. Six at most.")), 1, 5);
	eg->addWidget(ToolButton(equipment, GridView::CitizenPost, "ScCitizen", tr("<b>Citizen post</b><br>Click a whole room cell to add "
		"or remove a post where a citizen works.")), 1, 6);
	eg->addWidget(ToolButton(equipment, GridView::TroopPost, "ScTroop", tr("<b>Troop post</b><br>Click a whole room cell to add or "
		"remove a post where a troop stands guard.")), 1, 7);
	eg->addWidget(ToolButton(equipment, GridView::HullDoor, "dp_HullDoor", tr("<b>Hull door</b><br>A door in an outside wall. Click the "
		"wall to add or remove one; it snaps to the middle of a cell side or to the joint between two cells.")), 2, 1);
	eg->addWidget(ToolButton(equipment, GridView::PlaceWindow, "ScWindow", tr("<b>Window</b><br>A window in an outside wall or "
		"between two rooms, up to a cell wide. Click the wall to add one, or click one to pick it for the Window panel (its shape and "
		"how it is built); right-click one to build it another way.")), 2, 2);
	eg->addWidget(ToolButton(equipment, GridView::InteriorDoor, "dp_Door", tr("<b>Door</b><br>A door between two rooms. Click the wall "
		"to add or remove one; right-click it to make it automatic (it opens when someone comes near).")), 2, 3);
	eg->addWidget(ToolButton(equipment, GridView::WideHullDoor, "dp_CargoDoor", tr("<b>Cargo door</b><br>A double-width (2.4 m) "
		"door in an outside wall. Click the wall to add one; click one that is there to pick it for the Door panel.")), 2, 4);
	eg->addWidget(ToolButton(equipment, GridView::WideInteriorDoor, "dp_LargeDoor", tr("<b>Large door</b><br>A double-width "
		"(2.4 m) door between two rooms. Click the wall to add one; click one that is there to pick it for the Door panel.")), 2, 6);
	eg->addWidget(ToolButton(equipment, GridView::PlaceHullHatch, "dp_HullHatch", tr("<b>Hull hatch</b><br>Click a whole room "
		"cell with open sky over it (nothing built above, no roof) for a hatch in its ceiling, with a ladder up onto the top of the "
		"hull. Right-click one to turn it.")), 2, 7);
	eg->addWidget(ToolButton(equipment, GridView::PlaceHatch, "dp_Hatch", tr("<b>Hatch</b><br>On an upper deck, click a whole room "
		"cell for an opening in its floor, with a hatch and a ladder down to the same cell on the deck below.")), 2, 9);
	eg->addWidget(ToolButton(equipment, GridView::WallOpening, "dp_WallOpening", tr("<b>Wall opening</b><br>A doorway with no door "
		"between two rooms: square, arched or tall. Click the wall to add one, or click one to pick it for the Door panel.")), 2, 10);
	eg->addWidget(ToolButton(equipment, GridView::HullOpening, "dp_HullOpening", tr("<b>Hull opening</b><br>A doorway with no door "
		"in an outside wall, always open.")), 2, 11);
	eg->addWidget(ToolButton(equipment, GridView::PlaceRailing, "dp_Railing", tr("<b>Railing</b><br>Click any side of a cell, or half a diagonal, "
		"inside or outside: in a wall it takes the wall's place, anywhere else it stands on its own. Right-click a railing for the "
		"next style.")), 2, 12);
	eg->addWidget(ToolButton(equipment, GridView::PlaceStation, "console", tr("<b>Station</b><br>A control station: command, fire "
		"control, medic, navigator, sensor, shield, transporter or turret gunner (it works a turret), as a console with its chair or a screen on a wall (set in the "
		"Station panel). Click a whole room cell to put one there (a screen goes on the wall nearest the click), or click one to "
		"pick it; right-click to turn it. Up to 10 of each kind.")), 1, 9);
	eg->addWidget(ToolButton(equipment, GridView::PlaceGap, "gap", tr("<b>Railing gap</b><br>Click the railing along a balcony's, "
		"platform's or catwalk's open side to leave a gap in it, a doorway wide; right-click the gap to take the railing away along "
		"the whole side of the cell. Click a gap again to close it. On the ground deck people walk on and off through it; higher "
		"up it leads out into thin air (put an outside stair there instead: it makes its own gap).")), 1, 12);
	struct { const char* icon; QString name; QString shape; } stairs[] = {
		{"dp_StairsStraight", tr("Stairs, straight"), tr("One flight up two cells.")},
		{"dp_StairsTurnLeft", tr("Stairs, turning left"), tr("A flight up one cell, a landing, and a flight up the cell to its left.")},
		{"dp_StairsTurnRight", tr("Stairs, turning right"), tr("A flight up one cell, a landing, and a flight up the cell to its right.")},
		{"dp_StairsUTurnLeft", tr("Stairs, U-turn left"), tr("A flight up one cell, a landing across two, and a flight back beside the "
			"first, on its left.")},
		{"dp_StairsUTurnRight", tr("Stairs, U-turn right"), tr("A flight up one cell, a landing across two, and a flight back beside "
			"the first, on its right.")},
	};
	for(int i = 0; i < 5; i++)
		eg->addWidget(ToolButton(equipment, GridView::Tool(GridView::PlaceStairStraight + i), stairs[i].icon,
			tr("<b>%1</b><br>%2 Click the cell the stair starts in, on the lower deck: it climbs the first way that fits, and the "
			"deck above is opened over it. Right-click it to turn it.").arg(stairs[i].name).arg(stairs[i].shape)), 3, 4 + i);
	struct { GridView::Tool tool; const char* icon; QString tip; } roofs[] = {
		{GridView::RoofGable, "dp_RoofGable", tr("<b>Gable roof</b><br>Click or drag over whole room cells with nothing above them "
			"to roof them. Cells next to a gable roof join it; its slopes run across all of them. Click a roof to pick it for the "
			"Roof panel, right-click it to turn its ridge.")},
		{GridView::RoofHip, "dp_RoofHip", tr("<b>Hip roof</b><br>Slopes on every side, with valleys where the roof turns a "
			"corner. Click or drag to roof cells; cells next to a hip roof join it.")},
		{GridView::RoofRound, "dp_RoofRound", tr("<b>Barrel roof</b><br>A curved vault over each run of cells. Click or drag to "
			"roof cells; right-click the roof to turn its axis.")},
	};
	for(int i = 0; i < 3; i++)
		eg->addWidget(ToolButton(equipment, roofs[i].tool, roofs[i].icon, roofs[i].tip), 3, 10 + i);
	eg->addWidget(ToolButton(equipment, GridView::PlaceDormer, "dormer", tr("<b>Dormer</b><br>A little windowed gable (or shed, or "
		"box) standing out of a sloping roof. Click near the side of a roofed cell it should look out of: the roof's lower edge, "
		"over an outside wall. Right-click it to turn it; its style is in the Dormer panel.")), 1, 10);
	eg->addWidget(ToolButton(equipment, GridView::PlaceSkylight, "skylight", tr("<b>Skylight</b><br>A glazed hole in the ceiling of "
		"a whole room cell with open sky above, up through the roof: a pane set in the roof, or on a flat top a glass lantern "
		"(right-click to switch).")), 1, 11);
	for(int c = 0; c < 13; c++)
		eg->setColumnMinimumWidth(c, ButtonPx);
	eg->setColumnStretch(13, 1);
	outer->addWidget(equipment);
	outer->addStretch(1);

	SetTool(GridView::PaintRoom);
}

QToolButton* DesignPanel::Button(QWidget* parent, const QString& icon, const QString& tip, bool enabled)
{
	QToolButton* b = new QToolButton(parent);
	b->setIcon(PaletteIcon(icon));
	b->setIconSize(QSize(16, 16));
	b->setFixedSize(ButtonPx, ButtonPx);
	b->setToolTip(tip);
	b->setEnabled(enabled);
	return b;
}

QToolButton* DesignPanel::ToolButton(QWidget* parent, GridView::Tool tool, const QString& icon, const QString& tip)
{
	QToolButton* b = Button(parent, icon, tip);
	b->setCheckable(true);
	m_Tools->addButton(b, tool);
	return b;
}

void DesignPanel::SetLines(const bool lines[4])
{
	for(auto it = m_LineButtons.begin(); it != m_LineButtons.end(); ++it)
	{
		it.value()->blockSignals(true);
		it.value()->setChecked(lines[it.key()]);
		it.value()->blockSignals(false);
	}
}

void DesignPanel::SetTool(GridView::Tool tool)
{
	if(QAbstractButton* b = m_Tools->button(tool))
		b->setChecked(true);
}

void DesignPanel::SetRoom(const QString& type, int tag)
{
	m_Type = type;
	m_Tag = qBound(1, tag, 99);
	m_TagLabel->setText(QString::number(m_Tag));
	if(m_TypeButtons.contains(type))
		m_TypeButtons[type]->setChecked(true);
}

void DesignPanel::SetTheme(const QString& key)
{
	int i = m_Theme->findData(key);
	if(i >= 0)
		m_Theme->setCurrentIndex(i);
}

void DesignPanel::ChangeTag(int step)
{
	SetRoom(m_Type, m_Tag + step);
	emit RoomChosen(m_Type, m_Tag);
}
