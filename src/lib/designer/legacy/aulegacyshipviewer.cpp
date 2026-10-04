#include "aulegacyshipviewer.h"
#include "aulegacyship.h"
#include "aulegacyshipedit.h"
#include "aulegacyshipdetails.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipgeometry.h"
#include "aulegacyshipicons.h"
#include "aulegacyshiprender.h"
#include "aulegacyshipvehicles.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QCursor>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QSet>
#include <QSettings>
#include <QScrollBar>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStackedWidget>
#include <QStringList>
#include <QStyleOptionGraphicsItem>
#include <QTabWidget>
#include <QTextBrowser>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace
{
enum ItemData
{
	RecordKind = Qt::UserRole,
	RecordIndex
};

enum Kind
{
	RoomKind = 1,
	DetailKind,
	GridLabelKind
};

// AuAtom13.dll RenderSpacecraftGrid: 40 full grid cells either side of origin.
constexpr int GridHalfCells = 40;
constexpr double GridRadius = GridHalfCells * AuLegacyShip::GridSpacing;

QRectF GridBounds()
{
	return QRectF(-GridRadius, -GridRadius, GridRadius * 2.0, GridRadius * 2.0);
}

QString Hex(unsigned int value, int width = 2)
{
	return QStringLiteral("0x%1").arg(value, width, 16, QLatin1Char('0'));
}

QColor RecordColor(unsigned int value, int alpha = 255)
{
	QColor color = QColor::fromHsv(int((value * 137u + 31u) % 360u), 145, 230);
	color.setAlpha(alpha);
	return color;
}

QPointF ScenePoint(const QPointF& point)
{
	return QPointF(point.x(), -point.y());
}

struct RoomChoice
{
	int type;
	const char* name;
	const char* icon;
};

const RoomChoice RoomChoices[] = {
	{ 1, "Barracks", "dp_Barracks" }, { 2, "Bridge", "dp_Bridge" },
	{ 3, "Brig", "dp_Brig" }, { 4, "Officer Cabin", "dp_CaptCabin" },
	{ 5, "Hold", "dp_Cargo" }, { 6, "Hall", "dp_Corridor" },
	{ 7, "Crew Cabin", "dp_CrewCabin" }, { 8, "Engine Room", "dp_EngineRoom" },
	{ 9, "Fuel Cell", "dp_Fuel" }, { 10, "Galley", "dp_Galley" },
	{ 11, "Gravity Well", "dp_GravityWell" }, { 13, "Vehicle Bay", "dp_LandingBay" },
	{ 14, "Lounge", "dp_Lounge" }, { 15, "Open to Deck Below", "dp_OpenToBelow" },
	{ 16, "Sensor Room", "dp_SensorRoom" }, { 17, "Shield Room", "dp_ShieldRoom" },
	{ 18, "Sickbay", "dp_SickBay" }, { 19, "Stateroom", "dp_Stateroom" },
	{ 20, "Transporter Room", "dp_TransporterRoom" }, { 21, "Weapon Bay", "dp_WeaponBay" }
};

struct EquipmentChoice
{
	int type;
	quint16 commodity;
	const char* name;
	const char* icon;
	int row;
	int column;
};

// Original AuView equipment slots and AuTabDesign vehicle commodity IDs.
const EquipmentChoice EquipmentChoices[] = {
	{73, 0, "Captain's chair", "dp_CmdConsole", 0, 1},
	{12, 0, "Helm", "dp_PilotConsole", 0, 2},
	{6, 0, "Engineer station", "dp_EngineerConsole", 0, 3},
	{17, 0, "Shield station", "dp_ShieldConsole", 0, 4},
	{15, 0, "Sensor station", "dp_SensorConsole", 0, 5},
	{1, 0, "Navigator station", "dp_NavConsole", 0, 6},
	{7, 0, "Fire control station", "dp_FireControlConsole", 0, 7},
	{13, 0, "Power relay station", "dp_PowerConsole", 0, 8},
	{4, 0, "Medic station", "dp_SurgeryConsole", 0, 9},
	{19, 0, "Transporter station", "dp_TransporterConsole", 0, 10},
	{22, 0, "Top/bottom turret", "dp_TurretTopBottom", 0, 11},
	{21, 0, "Side turret", "dp_TurretSide", 0, 12},
	{10, 0, "Maneuver drive", "dp_ManeuverDrive", 1, 2},
	{14, 0, "Power plant", "dp_PowerPlant", 1, 3},
	{18, 0, "Shields", "dp_Shield", 1, 4},
	{16, 0, "Sensors", "dp_Sensor", 1, 5},
	{24, 0, "FTL drive", "dp_WormholeDrive", 1, 6},
	{23, 0, "Weapon system", "dp_Weapon", 1, 7},
	{32, 0, "Life support", "dp_LifeSupport", 1, 8},
	{11, 0, "Surgery unit", "dp_OperatingTable", 1, 9},
	{20, 0, "Transporter pad", "dp_TransporterPad", 1, 10},
	{65, 0, "Single berth", "dp_Berth0", 1, 11},
	{2, 0, "Double berth", "dp_Berth2", 1, 12},
	{50, 0, "Hull door", "dp_HullDoor", 2, 1},
	{49, 0, "Crew door", "dp_CrewDoor", 2, 2},
	{5, 0, "Door", "dp_Door", 2, 3},
	{3, 0, "Large hull door", "dp_CargoDoor", 2, 4},
	{54, 0, "Large crew door", "dp_LargeCrewDoor", 2, 5},
	{53, 0, "Large door", "dp_LargeDoor", 2, 6},
	{52, 0, "Hull hatch", "dp_HullHatch", 2, 7},
	{51, 0, "Crew hatch", "dp_CrewHatch", 2, 8},
	{8, 0, "Hatch", "dp_Hatch", 2, 9},
	{55, 0, "Wall opening", "dp_WallOpening", 2, 10},
	{9, 0, "Hull opening", "dp_HullOpening", 2, 11},
	{56, 0, "Railing", "dp_Railing", 2, 12},
	{141, 0, "Troop station", "Troop", 3, 6},
	{101, 0, "Straight stairs", "dp_StairsStraight", 3, 8},
	{109, 0, "Stairs turning left", "dp_StairsTurnLeft", 3, 9},
	{117, 0, "Stairs turning right", "dp_StairsTurnRight", 3, 10},
	{125, 0, "Stairs U-turn left", "dp_StairsUTurnLeft", 3, 11},
	{133, 0, "Stairs U-turn right", "dp_StairsUTurnRight", 3, 12},
	{76, 0, "Pivot point", "dp_PivotPoint", 4, 0},
	{98, 0, "Contrail emitter", "dp_ContrailEmitter", 4, 1},
	{99, 0, "Weapon emitter", "dp_WeaponEmitter", 4, 2},
	{74, 0, "Game table", "dp_GameTable", 4, 4},
	{57, 9, "Space transport", "c_CommercialAircraft", 4, 6},
	{57, 8, "Space fighter F1", "c_SpaceFighter", 4, 7},
	{57, 231, "Space fighter F2", "c_SpaceFighter2", 4, 8},
	{57, 104, "APC", "c_APC", 4, 10},
	{57, 133, "Motorbike", "c_MotorBike", 4, 11},
	{57, 82, "SUV", "c_Suv", 4, 12}
};

const EquipmentChoice* EquipmentFor(int type, quint16 commodity)
{
	const int base = AuLegacyShip::CanonicalDetailType(type);
	for (const EquipmentChoice& choice : EquipmentChoices)
		if (choice.type == base && (base != 57 || choice.commodity == commodity))
			return &choice;
	return nullptr;
}

bool EquipmentUsesBrush(int type)
{
	return type == 10 || type == 14 || type == 16 || type == 18
		|| type == 23 || type == 24 || type == 32;
}

bool EquipmentCanRotate(int type)
{
	const int base = AuLegacyShip::CanonicalDetailType(type);
	return base == 2 || base == 8 || base == 51 || base == 52 || base == 57
		|| base == 65 || base == 74 || (base >= 101 && base <= 133);
}

// Borders follow grid cells so resizing/docking preserves the original layout.
class PaletteGroups : public QWidget
{
public:
	explicit PaletteGroups(QWidget* parent) : QWidget(parent) {}
	QVector<QRect> groups;

protected:
	void paintEvent(QPaintEvent* event) override
	{
		QWidget::paintEvent(event);
		const QGridLayout* grid = static_cast<QGridLayout*>(layout());
		if (!grid) return;
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(QPen(QColor(160, 57, 64, 210), 1.0));
		painter.setBrush(Qt::NoBrush);
		for (const QRect& group : groups)
		{
			const QRectF bounds = grid->cellRect(group.top(), group.left())
				.united(grid->cellRect(group.bottom(), group.right()));
			painter.drawRoundedRect(bounds.adjusted(-1.0, -1.0, 1.0, 1.0), 4.0, 4.0);
		}
	}
};

QString RoomName(int type)
{
	for (const RoomChoice& choice : RoomChoices)
		if (choice.type == type)
			return QString::fromLatin1(choice.name);
	// Original GD names for records outside the spacecraft paint palette.
	switch (type)
	{
	case 0: return QStringLiteral("None");
	case 12: return QString();
	case 22: return QStringLiteral("Roof, Gable");
	case 23: return QStringLiteral("Roof, Hip");
	case 24: return QStringLiteral("Roof, Round");
	}
	return QStringLiteral("Type %1").arg(type);
}

QColor RoomLabelColor(unsigned int type)
{
	// Original AuAtom13.dll RGB float table at 0x180a0e620, recovered as RGB8.
	static const QRgb colors[] = {
		0xff0000, 0x89a349, 0xffdd00, 0x707070, 0x2d0e93,
		0xddc358, 0xa3e293, 0xb7ac80, 0xb7b7b7, 0xedfaff,
		0xedeca8, 0xc9ffbc, 0xfffc5b, 0xff9a3d, 0xe50000,
		0x807080, 0xff0087, 0x5de3f4, 0x69f4ca, 0xa50050,
		0xba4cd3, 0x3258c9, 0xe74222, 0xe74222, 0xe74222
	};
	return type < sizeof(colors) / sizeof(colors[0])
		? QColor::fromRgb(colors[type]) : RecordColor(type);
}

class RoomLabel : public QGraphicsItem
{
public:
	RoomLabel(const QString& text, const QFont& font, const QColor& color)
		: m_Text(text), m_Font(font), m_Color(color)
	{
		setFlag(QGraphicsItem::ItemIgnoresTransformations);
		setAcceptedMouseButtons(Qt::NoButton);
		setZValue(20.0);
		const QRectF measured = QFontMetricsF(m_Font).boundingRect(QRectF(),
			Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine, m_Text);
		// The original colored pass is one screen pixel above/left of its shadow.
		m_TextRect = QRectF(-1.0, -measured.height() - 1.0, measured.width(), measured.height());
	}

	QRectF boundingRect() const override
	{
		return m_TextRect.united(m_TextRect.translated(1.0, 1.0)).adjusted(-1.0, -1.0, 1.0, 1.0);
	}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
	{
		painter->setRenderHint(QPainter::TextAntialiasing);
		painter->setFont(m_Font);
		const int flags = Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine;
		painter->setPen(Qt::black);
		painter->drawText(m_TextRect.translated(1.0, 1.0), flags, m_Text);
		painter->setPen(m_Color);
		painter->drawText(m_TextRect, flags, m_Text);
	}

private:
	QString m_Text;
	QFont m_Font;
	QColor m_Color;
	QRectF m_TextRect;
};

class DeckView : public QGraphicsView
{
public:
	explicit DeckView(QGraphicsScene* scene, QWidget* parent)
		: QGraphicsView(scene, parent)
	{
		setRenderHint(QPainter::Antialiasing);
		setDragMode(QGraphicsView::ScrollHandDrag);
		setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		setResizeAnchor(QGraphicsView::AnchorViewCenter);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setMouseTracking(true);
		setBackgroundBrush(QColor(20, 27, 35));
		setAccessibleName(QStringLiteral("Legacy spacecraft deck plan"));
	}

	bool gridVisible = true;
	quint8 gridMask = 5;
	bool editing = false;
	bool erasing = false;
	bool equipmentTool = false;
	int equipmentBrushRadius = 0;
	int equipmentRotation = 0;
	QIcon equipmentIcon;
	QRectF equipmentFootprint;
	QString emptyMessage;
	std::function<void(const QPointF&)> cursorMoved;
	std::function<void(const QPointF&)> cellEntered;
	std::function<void(int)> rotateEquipment;

	void SetEditing(bool active, bool erase)
	{
		ResetEquipmentStroke();
		editing = active;
		erasing = erase;
		setDragMode(active ? QGraphicsView::NoDrag : QGraphicsView::ScrollHandDrag);
		viewport()->setCursor(active ? Qt::CrossCursor : Qt::ArrowCursor);
		viewport()->update();
	}

	void ResetEquipmentStroke()
	{
		m_LastEquipmentPointKnown = false;
	}

protected:
	void keyPressEvent(QKeyEvent* event) override
	{
		if (editing && equipmentTool && rotateEquipment
			&& (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right))
		{
			rotateEquipment(event->key() == Qt::Key_Left ? 1 : -1);
			event->accept();
			return;
		}
		QGraphicsView::keyPressEvent(event);
	}

	void wheelEvent(QWheelEvent* event) override
	{
		const int delta = event->angleDelta().y();
		if (delta == 0)
		{
			event->ignore();
			return;
		}
		const double currentScale = transform().m11();
		const double requested = currentScale * std::pow(1.2, delta / 120.0);
		const double bounded = std::max(0.05, std::min(200.0, requested));
		if (currentScale > 0.0)
			scale(bounded / currentScale, bounded / currentScale);
		UpdateCursor(viewport()->mapFromGlobal(QCursor::pos()));
		event->accept();
	}

	void mousePressEvent(QMouseEvent* event) override
	{
		UpdateCursor(event->pos());
		if (event->button() == Qt::MiddleButton)
		{
			m_Panning = true;
			m_PanPosition = event->pos();
			viewport()->setCursor(Qt::ClosedHandCursor);
			event->accept();
			return;
		}
		if (editing && event->button() == Qt::LeftButton)
		{
			ResetEquipmentStroke();
			EditCursor();
			event->accept();
			return;
		}
		QGraphicsView::mousePressEvent(event);
	}

	void mouseMoveEvent(QMouseEvent* event) override
	{
		if (m_Panning)
		{
			const QPoint delta = event->pos() - m_PanPosition;
			m_PanPosition = event->pos();
			horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
			verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
			UpdateCursor(event->pos());
			event->accept();
			return;
		}
		if (editing)
		{
			UpdateCursor(event->pos());
			if (event->buttons() & Qt::LeftButton)
				EditCursor();
			event->accept();
			return;
		}
		QGraphicsView::mouseMoveEvent(event);
		UpdateCursor(event->pos());
	}

	void mouseReleaseEvent(QMouseEvent* event) override
	{
		if (event->button() == Qt::MiddleButton && m_Panning)
		{
			m_Panning = false;
			viewport()->setCursor(editing ? Qt::CrossCursor : Qt::ArrowCursor);
			UpdateCursor(event->pos());
			event->accept();
			return;
		}
		if (editing && event->button() == Qt::LeftButton)
		{
			ResetEquipmentStroke();
			event->accept();
			return;
		}
		QGraphicsView::mouseReleaseEvent(event);
	}

	void mouseDoubleClickEvent(QMouseEvent* event) override
	{
		if (editing)
			event->accept();
		else
			QGraphicsView::mouseDoubleClickEvent(event);
	}

	void leaveEvent(QEvent* event) override
	{
		m_CursorKnown = false;
		viewport()->update();
		QGraphicsView::leaveEvent(event);
	}

	void drawBackground(QPainter* painter, const QRectF& rect) override
	{
		painter->fillRect(rect, backgroundBrush());
		if (!gridVisible || !rect.intersects(GridBounds()))
			return;

		QPen pen(QColor::fromRgbF(0.1, 0.2, 0.3));
		pen.setCosmetic(true);
		painter->setPen(pen);
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(GridBounds());
		const auto line = [painter](double x1, double y1, double x2, double y2) {
			painter->drawLine(ScenePoint(QPointF(x1, y1)), ScenePoint(QPointF(x2, y2)));
		};
		// Draw the original finite segments, with every tenth line in a brighter pass.
		for (bool major : { false, true })
		{
			pen.setColor(major ? QColor::fromRgbF(0.2, 0.3, 0.4) : QColor::fromRgbF(0.1, 0.2, 0.3));
			painter->setPen(pen);
			for (int i = -GridHalfCells; i <= GridHalfCells; ++i)
			{
				if ((i % 10 == 0) != major)
					continue;
				const double position = i * AuLegacyShip::GridSpacing;
				if (gridMask & 4)
					line(position, GridRadius, position, -GridRadius);
				if (gridMask & 1)
					line(GridRadius, position, -GridRadius, position);
				if (i == GridHalfCells)
					continue;
				if (gridMask & 2)
				{
					line(position, -GridRadius, GridRadius, -position);
					if (i != -GridHalfCells)
						line(-GridRadius, position, -position, GridRadius);
				}
				if (gridMask & 8)
				{
					line(position, GridRadius, GridRadius, position);
					if (i != -GridHalfCells)
						line(-GridRadius, -position, -position, -GridRadius);
				}
			}
		}
	}

	void drawForeground(QPainter* painter, const QRectF& rect) override
	{
		QGraphicsView::drawForeground(painter, rect);
		if (!editing || !m_CursorKnown)
			return;
		if (equipmentTool)
		{
			const double spacing = AuLegacyShip::CoordinateSpacing;
			const QPointF point = ScenePoint(QPointF(std::round(m_Cursor.x() / spacing) * spacing,
				std::round(m_Cursor.y() / spacing) * spacing));
			painter->save();
			painter->translate(point);
			QPen pen(erasing ? QColor(255, 116, 110) : QColor(129, 218, 255), 1.5);
			pen.setCosmetic(true);
			painter->setPen(pen);
			painter->setBrush(Qt::NoBrush);
			if (!equipmentFootprint.isEmpty())
			{
				painter->save();
				painter->rotate(-equipmentRotation * 45.0);
				painter->drawRect(equipmentFootprint);
				painter->restore();
			}
			else
			{
				const double radius = (equipmentBrushRadius + 0.5) * spacing;
				painter->drawRect(QRectF(-radius, -radius, radius * 2, radius * 2));
			}
			painter->drawLine(QPointF(-spacing / 3, 0), QPointF(spacing / 3, 0));
			painter->drawLine(QPointF(0, -spacing / 3), QPointF(0, spacing / 3));
			const double pixelScale = std::max(0.05, transform().m11());
			painter->scale(1.0 / pixelScale, 1.0 / pixelScale);
			painter->rotate(-equipmentRotation * 45.0);
			if (!equipmentIcon.isNull())
				painter->drawPixmap(QRect(-12, -12, 24, 24), equipmentIcon.pixmap(24, 24));
			painter->restore();
			return;
		}
		QPolygonF cell = AuLegacyShip::Geometry::FillShape(gridMask, m_Cursor);
		if (cell.isEmpty())
			return;
		for (QPointF& point : cell)
			point = ScenePoint(point);
		const QColor color = erasing ? QColor(255, 116, 110) : QColor(129, 218, 255);
		QPen pen(color, 1.5);
		pen.setCosmetic(true);
		painter->setPen(pen);
		QColor fill = color;
		fill.setAlpha(55);
		painter->setBrush(fill);
		painter->drawPolygon(cell);
	}

	void paintEvent(QPaintEvent* event) override
	{
		QGraphicsView::paintEvent(event);
		if (!emptyMessage.isEmpty())
		{
			QPainter painter(viewport());
			painter.setPen(QColor(194, 205, 219));
			painter.drawText(viewport()->rect().adjusted(30, 30, -30, -30),
				Qt::AlignCenter | Qt::TextWordWrap, emptyMessage);
		}
	}

private:
	void EditCursor()
	{
		if (!cellEntered)
			return;
		if (equipmentTool)
		{
			const QPointF snapped(std::round(m_Cursor.x() / AuLegacyShip::CoordinateSpacing),
				std::round(m_Cursor.y() / AuLegacyShip::CoordinateSpacing));
			if (m_LastEquipmentPointKnown && snapped == m_LastEquipmentPoint)
				return;
			m_LastEquipmentPoint = snapped;
			m_LastEquipmentPointKnown = true;
		}
		cellEntered(m_Cursor);
	}

	void UpdateCursor(const QPoint& pixel)
	{
		m_Cursor = ScenePoint(mapToScene(pixel));
		m_CursorKnown = viewport()->rect().contains(pixel);
		if (cursorMoved)
			cursorMoved(m_Cursor);
		viewport()->update();
	}

	QPointF m_Cursor;
	QPointF m_LastEquipmentPoint;
	bool m_LastEquipmentPointKnown = false;
	QPoint m_PanPosition;
	bool m_CursorKnown = false;
	bool m_Panning = false;
};

class DetailMarker : public QGraphicsItem
{
public:
	explicit DetailMarker(const AuLegacyShip::Detail& detail)
		: m_Color(RecordColor(detail.detailType)), m_Shape(detail.detailType % 3u)
	{
		setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIgnoresTransformations);
		setZValue(10.0);
		const EquipmentChoice* choice = EquipmentFor(detail.detailType, detail.commodityIdNormalized);
		if (choice)
			m_Icon = AuLegacyShipIcon(QString::fromLatin1(choice->icon)).pixmap(16, 16);
		m_Rotation = AuLegacyShip::DetailRotationStep(detail.detailType);
	}

	QRectF boundingRect() const override
	{
		return QRectF(-13.0, -13.0, 26.0, 26.0);
	}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) override
	{
		const bool selected = (option->state & QStyle::State_Selected) != 0;
		if (!m_Icon.isNull())
		{
			if (selected)
			{
				painter->setPen(QPen(Qt::white, 1.2));
				painter->setBrush(QColor(32, 49, 72, 210));
				painter->drawRoundedRect(QRectF(-11, -11, 22, 22), 3, 3);
			}
			painter->rotate(-45.0 * m_Rotation);
			painter->drawPixmap(-8, -8, m_Icon);
			return;
		}
		QPen pen(selected ? QColor(Qt::white) : m_Color, selected ? 2.0 : 1.2);
		pen.setCosmetic(true);
		painter->setPen(pen);
		QColor fill = m_Color;
		fill.setAlpha(selected ? 240 : 130);
		painter->setBrush(fill);
		if (m_Shape == 0)
			painter->drawEllipse(QRectF(-3.0, -3.0, 6.0, 6.0));
		else if (m_Shape == 1)
			painter->drawRect(QRectF(-3.0, -3.0, 6.0, 6.0));
		else
		{
			QPolygonF triangle;
			triangle << QPointF(0.0, -3.5) << QPointF(3.5, 3.0) << QPointF(-3.5, 3.0);
			painter->drawPolygon(triangle);
		}
	}

private:
	QColor m_Color;
	unsigned int m_Shape;
	QPixmap m_Icon;
	int m_Rotation = 0;
};
}

class AuLegacyShipViewer::Impl
{
public:
	enum ToolMode { InspectMode, PaintMode, EraseMode, EquipmentMode, DeleteEquipmentMode, DeleteEquipmentAreaMode };

	explicit Impl(AuLegacyShipViewer* owner) : window(owner),
		settings(QSettings::NativeFormat, QSettings::UserScope,
			QStringLiteral("Hazeron Freehold"), QStringLiteral("Shores of Hazeron"))
	{
		scene = new QGraphicsScene(window);
		view = new DeckView(scene, window);
		renderView = new AuLegacyShipRender(window);
		views = new QStackedWidget(window);
		views->addWidget(view);
		views->addWidget(renderView);
		scene->setSceneRect(GridBounds());

		QAction* newAction = MakeAction("New spacecraft", "dp_New", "Start a new spacecraft (Ctrl+N).");
		newAction->setShortcut(QKeySequence::New);
		QObject::connect(newAction, &QAction::triggered, window, &AuLegacyShipViewer::NewDesign);
		QAction* openAction = MakeAction("Open .ship...", "dp_Load", "Open a 2016 spacecraft design (Ctrl+O).");
		openAction->setShortcut(QKeySequence::Open);
		QObject::connect(openAction, &QAction::triggered, window, &AuLegacyShipViewer::ChooseFile);
		QAction* saveAction = UnavailableAction("Save spacecraft", "dp_Save", "Saving .ship files is not available yet.");
		QAction* fitAction = MakeAction("Fit view", "dp_ShowDesignPlane", "Fit the current floor plan or spacecraft in the view (F).");
		fitAction->setShortcut(QKeySequence(QStringLiteral("F")));
		QObject::connect(fitAction, &QAction::triggered, window, [this]() { FitDeck(); });
		renderAction = MakeAction("Render Mode", "dp_RenderMode",
			"<b>Render Mode</b><br>Switch view between line drawing or solid rendering.");
		renderAction->setCheckable(true);
		QObject::connect(renderAction, &QAction::toggled, window, [this](bool solid) { SetRenderMode(solid); });
		roomsAction = new QAction(QStringLiteral("Rooms"), window);
		detailsAction = new QAction(QStringLiteral("Equipment markers"), window);
		gridAction = MakeAction("Grid", "dp_ShowDesignPlane", "Show the selected grid line families.");
		for (QAction* action : { roomsAction, detailsAction, gridAction })
		{
			action->setCheckable(true);
			action->setChecked(true);
		}
		QObject::connect(roomsAction, &QAction::toggled, window, [this](bool visible) { SetLayerVisible(RoomKind, visible); });
		QObject::connect(detailsAction, &QAction::toggled, window, [this](bool visible) { SetLayerVisible(DetailKind, visible); });
		QObject::connect(gridAction, &QAction::toggled, window, [this](bool visible) {
			view->gridVisible = visible;
			SetLayerVisible(GridLabelKind, visible);
			view->viewport()->update();
		});

		tools = new QActionGroup(window);
		tools->setExclusive(true);
		inspectAction = MakeAction("Inspect", "dp_Select", "Select a room or equipment marker to inspect it.");
		eraseAction = MakeAction("Erase room cells", "dp_DelRoomCells", "Erase cells from the current deck.");
		for (QAction* action : { inspectAction, eraseAction })
		{
			action->setCheckable(true);
			tools->addAction(action);
		}
		inspectAction->setChecked(true);
		QObject::connect(inspectAction, &QAction::triggered, window, [this]() { SetToolMode(InspectMode); });
		QObject::connect(eraseAction, &QAction::triggered, window, [this]() { SetToolMode(EraseMode); });
		for (const RoomChoice& choice : RoomChoices)
		{
			QAction* action = MakeAction(choice.name, choice.icon,
				qPrintable(QStringLiteral("Paint %1 cells on the current deck. Room Number distinguishes rooms of the same type.").arg(QString::fromLatin1(choice.name))));
			action->setCheckable(true);
			tools->addAction(action);
			roomActions.insert(choice.type, action);
			const int type = choice.type;
			QObject::connect(action, &QAction::triggered, window, [this, type]() {
				selectedRoomType = type;
				SetToolMode(PaintMode);
			});
		}

		roomNumber = new QSpinBox(window);
		roomNumber->setRange(1, 256);
		roomNumber->setWrapping(true);
		roomNumber->setButtonSymbols(QAbstractSpinBox::NoButtons);
		roomNumber->setAlignment(Qt::AlignCenter);
		roomNumber->setAccessibleName(QStringLiteral("Room number"));
		roomNumber->setToolTip(QStringLiteral("Room Number: distinguishes rooms of the same type on the same deck. Values wrap from 256 to 1."));
		deckUpAction = MakeAction("Raise design grid", "dp_DeckAbove", "Move the design grid up one deck.");
		deckDownAction = MakeAction("Lower design grid", "dp_DeckBelow", "Move the design grid down one deck.");
		QObject::connect(deckUpAction, &QAction::triggered, window, [this]() { ChangeDeck(1); });
		QObject::connect(deckDownAction, &QAction::triggered, window, [this]() { ChangeDeck(-1); });
		const char* lineIcons[] = { "dp_Grid0", "dp_Grid45", "dp_Grid90", "dp_Grid135" };
		const char* lineTips[] = { "Lateral grid lines (on the beam).", "Diagonal grid lines (to the port quarter).",
			"Longitudinal grid lines (fore and aft).", "Diagonal grid lines (to the starboard quarter)." };
		for (int i = 0; i < 4; ++i)
		{
			lineActions[i] = MakeAction(lineTips[i], lineIcons[i], lineTips[i]);
			lineActions[i]->setCheckable(true);
			lineActions[i]->setChecked((5 & (1 << i)) != 0);
			QObject::connect(lineActions[i], &QAction::toggled, window, [this](bool) { UpdateGrid(); });
		}

		closeAction = new QAction(QStringLiteral("Exit Designer"), window);
		closeAction->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_Escape) << QKeySequence::Close);
		closeAction->setAutoRepeat(false);
		for (QAction* action : { newAction, openAction, fitAction, closeAction })
			window->addAction(action);
		QObject::connect(closeAction, &QAction::triggered, window, [this]() {
			if (exitHandler)
			{
				const std::function<void()> handler = exitHandler;
				QTimer::singleShot(0, window, [handler]() { handler(); });
			}
			else
				window->close();
		});

		QWidget* central = new QWidget(window);
		QVBoxLayout* layout = new QVBoxLayout(central);
		layout->setContentsMargins(6, 4, 6, 4);
		layout->addWidget(views, 1);
		window->setCentralWidget(central);

		toolDock = new QDockWidget(QStringLiteral("Spacecraft Design (F6)"), window);
		toolDock->setObjectName(QStringLiteral("LegacySpacecraftTools"));
		toolDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		toolDock->setStyleSheet(QStringLiteral(
			"QDockWidget { color: #d5e3fa; background: #000000; font-size: 12px; }"
			"QDockWidget::title { background: #07152d; border: 1px solid #315986; padding: 4px; }"
			"QWidget#LegacyPalette { background: #000000; color: #d5e3fa; }"
			"QToolButton { background: transparent; color: #d5e3fa; border: 1px solid transparent; border-radius: 2px; padding: 0; }"
			"QToolButton:hover { border-color: #668fc8; background: #10233e; }"
			"QToolButton:checked { border-color: #8da7d0; background: #1e3459; }"
			"QToolButton:pressed { background: #294c7b; }"
			"QToolButton:disabled { color: #606979; }"
			"QSpinBox, QComboBox { background: #102442; color: #d5e3fa; border: 1px solid #40618d; padding: 2px; }"
			"QComboBox QAbstractItemView { background: #0a1c3b; color: #d5e3fa; selection-background-color: #294c7b; }"
			"QLabel, QCheckBox { color: #d5e3fa; background: transparent; }"
			"QTabWidget::pane { border: 1px solid #55709a; }"
			"QTabBar::tab { color: #b7cff3; background: #081834; padding: 3px 9px; }"
			"QTabBar::tab:selected { background: #16305c; }"
			"QTextBrowser, QPlainTextEdit { background: #081a40; color: #b8cef5; border: 0; padding: 3px; }"));
		QWidget* panel = new QWidget(toolDock);
		panel->setObjectName(QStringLiteral("LegacyPalette"));
		QVBoxLayout* panelLayout = new QVBoxLayout(panel);
		panelLayout->setContentsMargins(4, 4, 4, 4);
		panelLayout->setSpacing(4);
		editPanel = new QWidget(panel);
		editPanel->setObjectName(QStringLiteral("LegacyPalette"));
		BuildPalette(editPanel, newAction, openAction, saveAction);
		panelLayout->addWidget(editPanel);
		QHBoxLayout* viewTools = new QHBoxLayout;
		viewTools->addStretch();
		QToolButton* fit = new QToolButton(panel);
		fit->setDefaultAction(fitAction);
		fit->setToolButtonStyle(Qt::ToolButtonTextOnly);
		viewTools->addWidget(fit);
		QToolButton* inspect = new QToolButton(panel);
		inspect->setDefaultAction(inspectAction);
		inspect->setIconSize(QSize(16, 16));
		inspect->setAccessibleName(QStringLiteral("Inspect rooms and equipment"));
		viewTools->addWidget(inspect);
		panelLayout->addLayout(viewTools);
		cutaway = new QCheckBox(QStringLiteral("Cut away above current deck"), panel);
		cutaway->setToolTip(QStringLiteral("Expose the current deck's interior by cutting the ship at half the wall height. Use the deck arrows to move the cut."));
		cutaway->setVisible(false);
		QObject::connect(cutaway, &QCheckBox::toggled, window, [this](bool enabled) {
			renderView->SetCutaway(enabled);
			ShowRenderHelp();
		});
		panelLayout->addWidget(cutaway);
		toolLabel = new QLabel(panel);
		toolLabel->setWordWrap(true);
		panelLayout->addWidget(toolLabel);
		informationTabs = new QTabWidget(panel);
		QTextBrowser* tips = new QTextBrowser(informationTabs);
		tips->setOpenExternalLinks(false);
		tips->setHtml(QStringLiteral(
			"<b>NEW SPACECRAFT DESIGN TIPS</b><br>"
			"- All the buttons on this window have tooltips.<br>"
			"- Start by painting the floor of a room, such as the Bridge.<br>"
			"- Move the design grid up and down using the deck buttons.<br>"
			"- Combine the four grid directions to change the cell shape.<br>"
			"- Room Number distinguishes rooms of the same type on the same deck. Change it before painting a separate room.<br>"
			"- Left click or drag to paint. Select Erase Room Cells to erase.<br>"
			"- Select equipment, then left click or drag to place it. Its tooltip lists the room restrictions.<br>"
			"- Left/right arrow keys rotate berths, vehicles, hatches and stairs before placement.<br>"
			"- The brush arrows resize machinery placement and Erase Equipment Area.<br>"
			"- Middle drag pans; the mouse wheel zooms.<br>"
			"- Use the selection arrow to inspect rooms and equipment.<br><br>"
			"- Render Mode switches between the floor plan and the textured spacecraft.<br>"
			"- In 3D, left drag orbits, middle/right drag pans, and the wheel zooms. F fits the ship.<br>"
			"- Cut away above current deck exposes the interior; the deck arrows move the cut.<br><br>"
			"<b>CURRENT EDITOR</b><br>"
			"Rooms and equipment can be edited in the floor plan. Unavailable tools remain disabled. Saving is not available yet.<br><br>"
			"<b>WHEN FINISHED</b><br>"
			"Press Escape to exit the designer.<br>"
			"Edits last for this session. New, Open and Exit ask before discarding changes.<br>"
			"F6 hides or shows this panel. Drag its title to dock it elsewhere or float it beside the deck view."));
		informationTabs->addTab(tips, QStringLiteral("Tips"));
		inspector = new QPlainTextEdit(informationTabs);
		inspector->setReadOnly(true);
		inspector->setAccessibleName(QStringLiteral("Selected room or equipment record"));
		informationTabs->addTab(inspector, QStringLiteral("Selection"));
		informationTabs->setMinimumHeight(130);
		panelLayout->addWidget(informationTabs, 1);
		QScrollArea* panelScroll = new QScrollArea(toolDock);
		panelScroll->setFrameShape(QFrame::NoFrame);
		panelScroll->setWidgetResizable(true);
		panelScroll->setWidget(panel);
		toolDock->setWidget(panelScroll);
		toolDock->setMinimumWidth(380);
		window->addDockWidget(Qt::RightDockWidgetArea, toolDock);
		window->resizeDocks(QList<QDockWidget*>() << toolDock, QList<int>() << 380, Qt::Horizontal);
		QAction* showTools = toolDock->toggleViewAction();
		showTools->setText(QStringLiteral("Spacecraft Design tools"));
		showTools->setShortcut(QKeySequence(Qt::Key_F6));
		toolDock->addAction(showTools);
		toolDock->addAction(closeAction);
		window->addAction(showTools);

		coordinates = new QLabel(QStringLiteral("X \u2014   Y \u2014 ft"), window);
		window->statusBar()->addPermanentWidget(coordinates);
		view->cursorMoved = [this](const QPointF& point) {
			coordinates->setText(QStringLiteral("X %1   Y %2 ft").arg(point.x(), 0, 'f', 2).arg(point.y(), 0, 'f', 2));
		};
		view->cellEntered = [this](const QPointF& point) { EditAt(point); };
		view->rotateEquipment = [this](int steps) {
			if (toolMode != EquipmentMode || !EquipmentCanRotate(selectedEquipmentType)) return;
			selectedEquipmentType = AuLegacyShip::RotateDetailType(selectedEquipmentType, steps);
			SetToolMode(EquipmentMode);
		};
		QObject::connect(scene, &QGraphicsScene::selectionChanged, window, [this]() { ShowSelection(); });
		ShowOverview();
	}

	~Impl()
	{
		QObject::disconnect(scene, nullptr, window, nullptr);
		view->cursorMoved = std::function<void(const QPointF&)>();
		view->cellEntered = std::function<void(const QPointF&)>();
		view->rotateEquipment = std::function<void(int)>();
	}

	QAction* MakeAction(const char* text, const char* icon, const char* tip)
	{
		QAction* action = new QAction(AuLegacyShipIcon(QString::fromLatin1(icon)), QString::fromLatin1(text), window);
		action->setToolTip(QString::fromLatin1(tip));
		return action;
	}

	QAction* UnavailableAction(const char* text, const char* icon, const char* reason)
	{
		QAction* action = MakeAction(text, icon, reason);
		action->setToolTip(QStringLiteral("%1\n%2").arg(QString::fromLatin1(text), QString::fromLatin1(reason)));
		action->setEnabled(false);
		return action;
	}

	void AddButton(QGridLayout* layout, QAction* action, int row, int column)
	{
		QToolButton* button = new QToolButton(layout->parentWidget());
		button->setDefaultAction(action);
		button->setAccessibleName(action->text());
		button->setIconSize(QSize(16, 16));
		button->setFixedSize(22, 22);
		button->setToolButtonStyle(Qt::ToolButtonIconOnly);
		layout->addWidget(button, row, column, Qt::AlignCenter);
	}

	void BuildPalette(QWidget* parent, QAction* newAction, QAction* openAction, QAction* saveAction)
	{
		QGridLayout* outer = new QGridLayout(parent);
		outer->setContentsMargins(0, 0, 0, 0);
		outer->setSpacing(2);
		AddButton(outer, newAction, 0, 1);
		AddButton(outer, openAction, 0, 2);
		AddButton(outer, saveAction, 0, 3);
		AddButton(outer, renderAction, 1, 0);
		AddButton(outer, UnavailableAction("Gravity", "dp_Gravity", "Avatar movement is not available in the deck view."), 1, 1);
		AddButton(outer, UnavailableAction("Check paths", "dp_Check", "Path checking is not available yet."), 1, 2);
		AddButton(outer, UnavailableAction("Burn design CD", "dp_BurnCD", "In-world design publishing is not available here."), 1, 3);
		AddButton(outer, deckUpAction, 2, 0);
		AddButton(outer, lineActions[2], 2, 1);
		AddButton(outer, lineActions[0], 2, 2);
		AddButton(outer, UnavailableAction("Erase current deck", "dp_DelDeck", "Whole-deck erasure is not available yet. Use Erase Room Cells."), 2, 4);
		AddButton(outer, deckDownAction, 3, 0);
		AddButton(outer, lineActions[1], 3, 1);
		AddButton(outer, lineActions[3], 3, 2);
		AddButton(outer, UnavailableAction("Move or copy", "dp_Move", "Moving and copying rooms is not available yet."), 3, 4);

		PaletteGroups* roomPalette = new PaletteGroups(parent);
		roomPalette->groups << QRect(0, 0, 8, 3) << QRect(9, 0, 1, 3);
		QGridLayout* rooms = new QGridLayout(roomPalette);
		rooms->setContentsMargins(2, 2, 2, 2);
		rooms->setSpacing(2);
		rooms->setColumnMinimumWidth(8, 6);
		rooms->setColumnStretch(8, 1);
		QAction* drawMode = MakeAction("Room mode: draw", "dp_RoomMode0", "Draw room cells using the selected room type. Changing a whole room's type is not available yet.");
		QObject::connect(drawMode, &QAction::triggered, window, [this]() { SetToolMode(PaintMode); });
		AddButton(rooms, drawMode, 0, 0);
		AddButton(rooms, eraseAction, 1, 0);
		AddButton(rooms, UnavailableAction("Erase room", "dp_DelRoom", "Whole-room erasure is not available yet. Use Erase Room Cells."), 2, 0);
		const int roomTypes[3][7] = { {1, 2, 3, 7, 8, 13, 9}, {10, 11, 6, 5, 4, 15, 14}, {16, 17, 18, 19, 20, 21, 0} };
		for (int row = 0; row < 3; ++row)
			for (int column = 0; column < 7; ++column)
				if (roomTypes[row][column])
					AddButton(rooms, roomActions.value(roomTypes[row][column]), row, column + 1);
		QAction* roomUp = MakeAction("Increase room number", "dp_RoomIdUp", "Increase Room Number; wraps after 256.");
		QAction* roomDown = MakeAction("Decrease room number", "dp_RoomIdDown", "Decrease Room Number; wraps before 1.");
		QObject::connect(roomUp, &QAction::triggered, roomNumber, &QSpinBox::stepUp);
		QObject::connect(roomDown, &QAction::triggered, roomNumber, &QSpinBox::stepDown);
		AddButton(rooms, roomUp, 0, 9);
		rooms->addWidget(roomNumber, 1, 9);
		AddButton(rooms, roomDown, 2, 9);
		outer->addWidget(roomPalette, 0, 6, 4, 8);

		PaletteGroups* equipmentPalette = new PaletteGroups(parent);
		equipmentPalette->groups << QRect(0, 0, 1, 2) << QRect(1, 0, 12, 2)
			<< QRect(1, 2, 12, 1) << QRect(13, 0, 2, 3) << QRect(8, 3, 5, 1)
			<< QRect(0, 4, 3, 1) << QRect(6, 4, 7, 1);
		QGridLayout* equipment = new QGridLayout(equipmentPalette);
		equipment->setContentsMargins(2, 6, 2, 3);
		equipment->setSpacing(3);
		deleteEquipmentAction = MakeAction("Erase equipment", "dp_DelDetail", "Remove equipment at the cursor. Left click or drag.");
		deleteEquipmentAreaAction = MakeAction("Erase equipment area", "dp_DelDetails", "Remove equipment at the cursor, then equipment of that same type within the brush.");
		for (QAction* action : { deleteEquipmentAction, deleteEquipmentAreaAction })
		{
			action->setCheckable(true);
			tools->addAction(action);
		}
		QObject::connect(deleteEquipmentAction, &QAction::triggered, window, [this]() { SetToolMode(DeleteEquipmentMode); });
		QObject::connect(deleteEquipmentAreaAction, &QAction::triggered, window, [this]() { SetToolMode(DeleteEquipmentAreaMode); });
		AddButton(equipment, deleteEquipmentAction, 0, 0);
		AddButton(equipment, deleteEquipmentAreaAction, 1, 0);
		for (const EquipmentChoice& choice : EquipmentChoices)
		{
			QAction* action = MakeAction(choice.name, choice.icon, choice.name);
			const bool supported = choice.commodity ? AuLegacyShip::Rendering::VehicleSupported(choice.commodity)
				: AuLegacyShip::Rendering::EquipmentSupported(choice.type)
					|| AuLegacyShip::Rendering::ResourceEquipmentSupported(choice.type);
			action->setEnabled(supported);
			action->setCheckable(true);
			tools->addAction(action);
			QString tip = QString::fromLatin1(choice.name) + QLatin1Char('\n')
				+ AuLegacyShip::DetailPlacementDescription(choice.type);
			if (EquipmentCanRotate(choice.type))
				tip += QStringLiteral("\nLeft/right arrow keys rotate the placement tool.");
			if (EquipmentUsesBrush(choice.type))
				tip += QStringLiteral("\nUses the equipment brush size.");
			if (choice.type == 76 || choice.type == 98 || choice.type == 99 || choice.type == 141)
				tip += QStringLiteral("\nPosition marker; has no physical model in the stationary preview.");
			if (!supported)
				tip += QStringLiteral("\nThis object's 2016 model is not available yet.");
			action->setToolTip(tip);
			const EquipmentChoice* selected = &choice;
			QObject::connect(action, &QAction::triggered, window, [this, selected, action]() {
				selectedEquipment = selected;
				selectedEquipmentType = selected->type;
				selectedEquipmentAction = action;
				SetToolMode(EquipmentMode);
			});
			AddButton(equipment, action, choice.row, choice.column);
		}
		brushUpAction = MakeAction("Increase equipment brush size", "dp_BrushSzUp", "Increase the equipment brush. The original radius range is 0 to 25.");
		brushDownAction = MakeAction("Decrease equipment brush size", "dp_BrushSzDown", "Decrease the equipment brush; the smallest brush places one object.");
		QObject::connect(brushUpAction, &QAction::triggered, window, [this]() { ChangeEquipmentBrush(1); });
		QObject::connect(brushDownAction, &QAction::triggered, window, [this]() { ChangeEquipmentBrush(-1); });
		AddButton(equipment, brushUpAction, 0, 14);
		AddButton(equipment, brushDownAction, 2, 14);
		brushSize = new QLabel(QStringLiteral("1"), equipmentPalette);
		brushSize->setAlignment(Qt::AlignCenter);
		brushSize->setToolTip(QStringLiteral("Equipment brush size, 1 to 26. Applies to machinery and Erase Equipment Area."));
		brushDownAction->setEnabled(false);
		equipment->addWidget(brushSize, 1, 13, 1, 2);
		outer->addWidget(equipmentPalette, 4, 0, 1, 14);

		AddButton(outer, UnavailableAction("System modules", "UpgradeModule", "Module allocation is not available yet."), 5, 0);
		armorUpAction = MakeAction("Increase armor", "dp_ArmorIncrease", "Increase armor, up to 10.");
		armorDownAction = MakeAction("Decrease armor", "dp_ArmorDecrease", "Decrease armor, down to 0.");
		QObject::connect(armorUpAction, &QAction::triggered, window, [this]() { ChangeArmor(1); });
		QObject::connect(armorDownAction, &QAction::triggered, window, [this]() { ChangeArmor(-1); });
		AddButton(outer, armorUpAction, 5, 2);
		AddButton(outer, armorDownAction, 5, 3);
		streamline = new QCheckBox(QStringLiteral("Streamline Hull"), parent);
		streamline->setToolTip(QStringLiteral("Streamline the hull for atmospheric flight: improves speed and reduces hull stress."));
		QObject::connect(streamline, &QCheckBox::toggled, window, [this](bool enabled) {
			if (loaded && bool(design.header.streamline) != enabled)
			{
				design.header.streamline = enabled ? 1 : 0;
				HeaderChanged();
			}
		});
		outer->addWidget(streamline, 5, 5, 1, 7);
		minimumTech = new QComboBox(parent);
		minimumTech->setAccessibleName(QStringLiteral("Minimum tech level"));
		minimumTech->setToolTip(QStringLiteral("Minimum technology level for manufacturing. The displayed levels are 1 through 32."));
		for (int level = 31; level >= 0; --level)
			minimumTech->addItem(QString::number(level + 1), level);
		QObject::connect(minimumTech, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), window, [this](int index) {
			if (loaded && index >= 0 && design.header.minimumTechLevel != minimumTech->itemData(index).toInt())
			{
				design.header.minimumTechLevel = quint8(minimumTech->itemData(index).toInt());
				HeaderChanged();
			}
		});
		outer->addWidget(minimumTech, 5, 12, 1, 2);
	}

	void ChangeDeck(int delta)
	{
		if (!loaded) return;
		const int next = currentDeck + delta;
		if (next >= AuLegacyShip::MinimumDeck && next <= AuLegacyShip::MaximumDeck)
			ShowDeck(next);
	}

	void ChangeArmor(int delta)
	{
		if (!loaded || (delta > 0 && design.header.armor >= 10) || (delta < 0 && design.header.armor == 0))
			return;
		design.header.armor = quint8(int(design.header.armor) + delta);
		HeaderChanged(true);
	}

	void RefreshHeaderControls()
	{
		streamline->blockSignals(true);
		streamline->setChecked(design.header.streamline != 0);
		streamline->blockSignals(false);
		minimumTech->blockSignals(true);
		minimumTech->setCurrentIndex(minimumTech->findData(design.header.minimumTechLevel));
		minimumTech->blockSignals(false);
		armorUpAction->setEnabled(loaded && design.header.armor < 10);
		armorDownAction->setEnabled(loaded && design.header.armor > 0);
		const QString armor = QStringLiteral("Current armor: %1 (0-10).").arg(design.header.armor);
		armorUpAction->setToolTip(QStringLiteral("Increase armor\n") + armor);
		armorDownAction->setToolTip(QStringLiteral("Decrease armor\n") + armor);
	}

	void HeaderChanged(bool geometryChanged = false)
	{
		modified = true;
		renderDirty = renderDirty || geometryChanged;
		if (geometryChanged && renderAction->isChecked())
			RefreshRendering();
		RefreshHeaderControls();
		UpdateTitle();
		ShowOverview();
		window->statusBar()->showMessage(QStringLiteral("Design settings updated. Session edits are not saved."));
	}

	void FitDeck()
	{
		if (renderAction->isChecked())
		{
			renderView->FitDesign();
			return;
		}
		view->fitInView(deckBounds, Qt::KeepAspectRatio);
		// Leave screen-space room for labels centered on the grid's outer corners.
		QFont labelFont = window->font();
		labelFont.setPixelSize(16);
		const QFontMetricsF metrics(labelFont);
		const double horizontalPadding = metrics.boundingRect(QStringLiteral("Starboard Quarter")).width() * 0.5 + 12.0;
		const double verticalPadding = metrics.height() * 0.5 + 12.0;
		const double width = std::max(1, view->viewport()->width());
		const double height = std::max(1, view->viewport()->height());
		const double scale = std::max(0.1, std::min((width - horizontalPadding * 2.0) / width,
			(height - verticalPadding * 2.0) / height));
		view->scale(scale, scale);
		view->centerOn(deckBounds.center());
	}

	void RefreshRendering()
	{
		if (renderDirty)
		{
			renderView->SetDesign(design);
			renderDirty = false;
		}
		renderView->SetDeck(currentDeck);
		if (renderNeedsFit)
		{
			renderView->FitDesign();
			renderNeedsFit = false;
		}
	}

	void ShowRenderHelp()
	{
		if (!renderAction->isChecked())
			return;
		toolLabel->setText(QStringLiteral("3D view: left drag orbits; middle/right drag pans; wheel zooms. F fits the ship."));
		window->statusBar()->showMessage(cutaway->isChecked()
			? QStringLiteral("Interior cutaway at deck %1. Use the deck arrows to move the cut.").arg(currentDeck)
			: QStringLiteral("Solid spacecraft view. Use Render Mode to return to the floor plan."));
	}

	void SetRenderMode(bool solid)
	{
		if (solid)
			RefreshRendering();
		views->setCurrentWidget(solid ? static_cast<QWidget*>(renderView) : static_cast<QWidget*>(view));
		cutaway->setVisible(solid);
		coordinates->setVisible(!solid);
		if (solid)
		{
			renderView->setFocus();
			ShowRenderHelp();
			ShowOverview();
		}
		else
		{
			view->setFocus();
			SetToolMode(toolMode);
		}
	}

	void SetToolMode(ToolMode mode)
	{
		if (renderAction->isChecked())
			renderAction->setChecked(false);
		toolMode = mode;
		view->equipmentTool = mode == EquipmentMode || mode == DeleteEquipmentMode || mode == DeleteEquipmentAreaMode;
		view->equipmentBrushRadius = mode == DeleteEquipmentAreaMode
			|| (mode == EquipmentMode && EquipmentUsesBrush(selectedEquipmentType)) ? equipmentBrushRadius : 0;
		view->equipmentIcon = mode == EquipmentMode && selectedEquipment
			? AuLegacyShipIcon(QString::fromLatin1(selectedEquipment->icon)) : QIcon();
		view->equipmentRotation = AuLegacyShip::DetailRotationStep(selectedEquipmentType);
		view->equipmentFootprint = mode == EquipmentMode && selectedEquipment && selectedEquipment->commodity
			? AuLegacyShip::Rendering::VehicleFootprint(selectedEquipment->commodity) : QRectF();
		view->SetEditing(loaded && mode != InspectMode,
			mode == EraseMode || mode == DeleteEquipmentMode || mode == DeleteEquipmentAreaMode);
		inspectAction->setChecked(mode == InspectMode);
		eraseAction->setChecked(mode == EraseMode);
		deleteEquipmentAction->setChecked(mode == DeleteEquipmentMode);
		deleteEquipmentAreaAction->setChecked(mode == DeleteEquipmentAreaMode);
		if (mode == PaintMode)
			roomActions.value(selectedRoomType)->setChecked(true);
		if (mode == EquipmentMode && selectedEquipmentAction)
			selectedEquipmentAction->setChecked(true);
		roomNumber->setEnabled(loaded);
		if (view->equipmentTool)
		{
			QString text;
			if (mode == EquipmentMode && selectedEquipment)
			{
				text = QString::fromLatin1(selectedEquipment->name) + QStringLiteral(": ")
					+ AuLegacyShip::DetailPlacementDescription(selectedEquipmentType);
				if (EquipmentCanRotate(selectedEquipmentType))
					text += QStringLiteral(" Left/right arrows rotate (%1 degrees).")
						.arg(AuLegacyShip::DetailRotationStep(selectedEquipmentType) * 45);
			}
			else
				text = mode == DeleteEquipmentAreaMode
					? QStringLiteral("Erase Equipment Area: click equipment to remove that type within the brush.")
					: QStringLiteral("Erase Equipment: left click or drag over equipment.");
			toolLabel->setText(text);
			window->statusBar()->showMessage(text);
			view->setFocus();
			return;
		}
		toolLabel->setText(mode == InspectMode ? QStringLiteral("Selection tool: click a room or equipment marker.")
			: (mode == EraseMode ? QStringLiteral("Erase Room Cells: left click or drag.")
				: QStringLiteral("Paint %1: left click or drag.").arg(RoomName(selectedRoomType))));
		window->statusBar()->showMessage(mode == InspectMode
			? QStringLiteral("Click a room or detail to inspect it. Middle drag pans; the wheel zooms.")
			: QStringLiteral("Left click or drag to %1 cells. Middle drag pans. Edits are not saved.")
				.arg(mode == EraseMode ? QStringLiteral("erase") : QStringLiteral("paint")));
	}

	void ChangeEquipmentBrush(int delta)
	{
		equipmentBrushRadius = std::max(0, std::min(25, equipmentBrushRadius + delta));
		brushSize->setText(QString::number(equipmentBrushRadius + 1));
		brushUpAction->setEnabled(equipmentBrushRadius < 25);
		brushDownAction->setEnabled(equipmentBrushRadius > 0);
		if (view->equipmentTool)
			SetToolMode(toolMode);
	}

	void UpdateGrid()
	{
		quint8 mask = 0;
		for (int i = 0; i < 4; ++i)
			if (lineActions[i]->isChecked())
				mask |= quint8(1 << i);
		view->gridMask = mask;
		view->viewport()->update();
		if (mask == 0 || mask == 1 || mask == 2 || mask == 4 || mask == 8)
			window->statusBar()->showMessage(QStringLiteral("These grid lines do not form closed cells. Enable another direction to paint or erase."));
		else
			window->statusBar()->showMessage(QStringLiteral("Grid directions updated. The highlighted cell follows the original 2016 shape rules."));
	}

	void EditAt(const QPointF& cursor)
	{
		if (!loaded || renderAction->isChecked() || toolMode == InspectMode)
			return;
		const int deck = currentDeck;
		if (view->equipmentTool)
		{
			if (toolMode == EquipmentMode && !selectedEquipment) return;
			const AuLegacyShip::DetailEditResult result = toolMode == EquipmentMode
				? AuLegacyShip::PlaceDetail(design, deck, cursor, selectedEquipmentType,
					selectedEquipment->commodity, view->equipmentBrushRadius)
				: AuLegacyShip::DeleteDetails(design, deck, cursor, view->equipmentBrushRadius);
			if (!result.accepted)
			{
				window->statusBar()->showMessage(result.error.isEmpty()
					? QStringLiteral("Equipment cannot be placed here.") : result.error);
				return;
			}
			if (result.changed)
			{
				modified = true;
				renderDirty = true;
				IndexDecks();
				UpdateTitle();
				ShowDeck(deck, false);
			}
			window->statusBar()->showMessage(result.error.isEmpty()
				? (result.changed ? QStringLiteral("Equipment updated (%1 added, %2 removed). Session edits are not saved.")
					.arg(result.added).arg(result.removed) : QStringLiteral("No equipment change at this point.")) : result.error);
			return;
		}
		const int type = toolMode == EraseMode ? -1 : selectedRoomType;
		const AuLegacyShip::CellEditResult result = AuLegacyShip::EditCell(design, deck, cursor,
			view->gridMask, type, quint8(roomNumber->value() - 1));
		if (!result.accepted)
		{
			window->statusBar()->showMessage(result.error.isEmpty()
				? QStringLiteral("This cell edit could not be applied.") : result.error);
			return;
		}
		if (result.changed)
		{
			modified = true;
			renderDirty = true;
			IndexDecks();
			UpdateTitle();
			ShowDeck(deck, false);
		}
		window->statusBar()->showMessage(result.error.isEmpty()
			? (result.changed ? QStringLiteral("Cell updated. Session edits are not saved.")
				: QStringLiteral("No change at this cell.")) : result.error);
	}

	void SetLayerVisible(int kind, bool visible)
	{
		for (QGraphicsItem* item : scene->items())
		{
			if (item->data(RecordKind).toInt() == kind)
				item->setVisible(visible);
		}
	}

	QString DesignInformation() const
	{
		if (!loaded)
			return QStringLiteral("Open a 2016 .ship design to inspect its room boundaries and equipment positions.\n\n"
				"Paint and Erase Cells edit the loaded design in memory. Saving is not available yet.\n\n"
				"The four grid directions combine to form the original squares, triangles, diamonds and parallelograms.\n\n"
				"Left click or drag applies the selected tool. Middle drag pans; the wheel zooms.");
		const AuLegacyShip::Header& header = design.header;
		QString text = QStringLiteral("%1\n\nFile: %2\nDesign ID: %3\nArchitect: %4\nArchitect ID: %5\n"
			"Format version: %6\nState: %7\nStreamline: %8\nArmor: %9\n")
			.arg(DisplayName(), fileName.isEmpty() ? QStringLiteral("(new spacecraft; no file)") : fileName).arg(header.designId)
			.arg(header.architectName.isEmpty() ? QStringLiteral("(not stored)") : header.architectName)
			.arg(header.architectId).arg(header.version).arg(header.state).arg(header.streamline).arg(header.armor);
		const bool hasExtendedHeader = header.version > 1;
		const QString absent = QStringLiteral("(not stored in this format version)");
		text += QStringLiteral("Minimum tech level: %1\nHull volume: %2\nStructure: %3\nTech level: %4\n\n"
			"Current rooms: %5\nCurrent details: %6\nDecks with geometry: %7\nSource bytes read: %8\n")
			.arg(int(header.minimumTechLevel) + 1).arg(hasExtendedHeader ? QString::number(header.hullVolume) : absent)
			.arg(hasExtendedHeader ? QString::number(header.structure) : absent)
			.arg(hasExtendedHeader ? QString::number(header.techLevel) : absent)
			.arg(design.rooms.size()).arg(design.details.size()).arg(occupiedDeckCount).arg(design.bytesConsumed);
		text += modified ? QStringLiteral("\nThis design has session edits. Saving is not available yet.\n")
			: QStringLiteral("\nNo session edits. Saving is not available yet.\n");
		text += QStringLiteral("\nUse Inspect to select a room or detail for its record. "
			"Equipment uses the original palette icons; Render Mode shows its 3D geometry.\n");
		if (!geometryWarnings.isEmpty())
			text += QStringLiteral("\nGeometry warnings for this deck:\n") + geometryWarnings.join(QLatin1Char('\n'));
		if (!renderDirty && !renderView->Warnings().isEmpty())
			text += QStringLiteral("\n3D rendering notes:\n") + renderView->Warnings().join(QLatin1Char('\n'));
		return text;
	}

	QString DisplayName() const
	{
		if (!design.header.name.isEmpty())
			return design.header.name;
		return fileName.isEmpty() ? QStringLiteral("Untitled spacecraft") : QFileInfo(fileName).completeBaseName();
	}

	void ShowOverview()
	{
		inspector->setPlainText(DesignInformation());
	}

	QString RoomInformation(int index) const
	{
		const AuLegacyShip::Room& room = design.rooms[index];
		QString text = QStringLiteral("Room record %1\n\nNumeric room type: %2 (%3)\nDeck: %4\nZ: %5 ft\n"
			"Design ID: %6\nRecord index: %7\nSource stream offset: %8\nCurrent vertices: %9\n")
			.arg(room.roomId).arg(room.roomType).arg(Hex(room.roomType)).arg(int(room.deck))
			.arg(int(room.deck) * AuLegacyShip::DeckHeight).arg(room.designId).arg(index)
			.arg(room.streamOffset < 0 ? QStringLiteral("new room") : Hex(room.streamOffset, 8))
			.arg(room.packedVertices.size());
		text += QStringLiteral("Room type: %1\nRoom Number: %2\nStored subtype: %3\nBoundary: %4\n\nPoints (X, Y feet):\n")
			.arg(RoomName(room.roomType)).arg(int(room.roomSubtype) + 1).arg(Hex(room.roomSubtype))
			.arg(room.IsEllipse() ? QStringLiteral("ellipse from three control points") : QStringLiteral("polygon"));
		const int limit = std::min(room.packedVertices.size(), 64);
		for (int i = 0; i < limit; ++i)
		{
			const QPointF point = AuLegacyShip::DecodePoint(room.packedVertices[i]);
			text += QStringLiteral("%1: %2, %3  [%4]\n").arg(i).arg(point.x(), 0, 'f', 4)
				.arg(point.y(), 0, 'f', 4).arg(Hex(room.packedVertices[i], 4));
		}
		if (room.packedVertices.size() > limit)
			text += QStringLiteral("... %1 more points\n").arg(room.packedVertices.size() - limit);
		return text;
	}

	QString DetailInformation(int index) const
	{
		const AuLegacyShip::Detail& detail = design.details[index];
		const QPointF point = AuLegacyShip::DecodePoint(detail.packedXY);
		return QStringLiteral("Detail %1\n\nDeck: %2\nX: %3 ft\nY: %4 ft\nZ: %5 ft\n"
			"Design ID: %6\nRecord index: %7\nStream offset: %8\nPacked coordinate: %9\n")
			.arg(detail.detailId).arg(int(detail.deck)).arg(point.x(), 0, 'f', 4).arg(point.y(), 0, 'f', 4)
			.arg(int(detail.deck) * AuLegacyShip::DeckHeight).arg(detail.designId).arg(index)
			.arg(detail.streamOffset < 0 ? QStringLiteral("new equipment") : Hex(detail.streamOffset, 8))
			.arg(Hex(detail.packedXY, 4))
			+ QStringLiteral("\nNumeric detail type: %1\nHidden byte (raw): %2\n"
				"Room ID: %3\nCommodity ID (raw): %4\n"
				"Commodity ID after legacy normalization: %5\n\n"
				"Hidden details are included in this inspection view.")
				.arg(Hex(detail.detailType)).arg(Hex(detail.hiddenRaw)).arg(detail.roomId)
				.arg(Hex(detail.commodityIdRaw, 4)).arg(Hex(detail.commodityIdNormalized, 4))
			+ QStringLiteral("\n\n%1\n%2").arg(AuLegacyShip::DetailName(detail.detailType),
				AuLegacyShip::DetailPlacementDescription(detail.detailType));
	}

	void ShowSelection()
	{
		const QList<QGraphicsItem*> selected = scene->selectedItems();
		if (selected.isEmpty())
		{
			ShowOverview();
			return;
		}
		QGraphicsItem* item = selected.first();
		const int kind = item->data(RecordKind).toInt();
		const int index = item->data(RecordIndex).toInt();
		if (kind == RoomKind && index >= 0 && index < design.rooms.size())
		{
			inspector->setPlainText(RoomInformation(index));
			informationTabs->setCurrentWidget(inspector);
		}
		else if (kind == DetailKind && index >= 0 && index < design.details.size())
		{
			inspector->setPlainText(DetailInformation(index));
			informationTabs->setCurrentWidget(inspector);
		}
	}

	void AddGridLabels(const QFont& font)
	{
		struct GridLabel { const char* text; int x; int y; };
		static const GridLabel labels[] = {
			{ "Fore", 0, 1 }, { "Aft", 0, -1 },
			{ "Port", -1, 0 }, { "Starboard", 1, 0 },
			{ "Port Quarter", -1, -1 }, { "Starboard Quarter", 1, -1 },
			{ "Port Fore", -1, 1 }, { "Starboard Fore", 1, 1 }
		};
		for (const GridLabel& entry : labels)
		{
			QGraphicsSimpleTextItem* label = scene->addSimpleText(QString::fromLatin1(entry.text), font);
			label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
			label->setAcceptedMouseButtons(Qt::NoButton);
			label->setBrush(QColor::fromRgbF(0.3, 0.4, 0.5));
			label->setZValue(30.0);
			const QPointF center = label->boundingRect().center();
			label->setTransform(QTransform::fromTranslate(-center.x(), -center.y()));
			label->setPos(ScenePoint(QPointF(entry.x * GridRadius, entry.y * GridRadius)));
			label->setData(RecordKind, GridLabelKind);
			label->setVisible(view->gridVisible);
		}
	}

	void ShowDeck(int deck, bool fit = true)
	{
		deck = std::max(AuLegacyShip::MinimumDeck, std::min(AuLegacyShip::MaximumDeck, deck));
		if (deck != currentDeck)
			view->ResetEquipmentStroke();
		currentDeck = deck;
		if (renderAction->isChecked())
			RefreshRendering();
		deckUpAction->setEnabled(loaded && deck < AuLegacyShip::MaximumDeck);
		deckDownAction->setEnabled(loaded && deck > AuLegacyShip::MinimumDeck);
		const QTransform previousTransform = view->transform();
		const int previousHorizontal = view->horizontalScrollBar()->value();
		const int previousVertical = view->verticalScrollBar()->value();
		scene->clear();
		geometryWarnings.clear();
		QRectF bounds;
		bool hasBounds = false;
		int visibleRooms = 0;
		int warningCount = 0;
		QFont labelFont = window->font();
		labelFont.setPixelSize(16);
		AddGridLabels(labelFont);
		for (int index : roomDecks.value(deck))
		{
			const AuLegacyShip::Room& room = design.rooms[index];
			if (room.packedVertices.isEmpty())
				continue;
			QString error;
			QPolygonF polygon = AuLegacyShip::RoomBoundary(room, &error);
			if (polygon.size() < 3 || !error.isEmpty())
			{
				++warningCount;
				if (geometryWarnings.size() < 10)
					geometryWarnings.append(QStringLiteral("Room %1: %2").arg(room.roomId)
						.arg(error.isEmpty() ? QStringLiteral("fewer than three boundary points") : error));
				continue;
			}
			for (QPointF& point : polygon)
				point = ScenePoint(point);
			QPen pen(RecordColor(room.roomType));
			pen.setCosmetic(true);
			QGraphicsPolygonItem* item = scene->addPolygon(polygon, pen, QBrush(RecordColor(room.roomType, 70)));
			item->setFlag(QGraphicsItem::ItemIsSelectable);
			item->setData(RecordKind, RoomKind);
			item->setData(RecordIndex, index);
			item->setToolTip(QStringLiteral("%1 %2 | record %3 | type %4 (%5) | deck %6")
				.arg(RoomName(room.roomType)).arg(int(room.roomSubtype) + 1).arg(room.roomId)
				.arg(room.roomType).arg(Hex(room.roomType)).arg(deck));
			item->setVisible(roomsAction->isChecked());
			RoomLabel* label = new RoomLabel(QStringLiteral("%1 %2")
				.arg(RoomName(room.roomType)).arg(int(room.roomSubtype) + 1),
				labelFont, RoomLabelColor(room.roomType));
			// RenderDesign labels boundary[0], not the room's center or record ID.
			label->setPos(polygon.first());
			label->setData(RecordKind, RoomKind);
			label->setVisible(roomsAction->isChecked());
			scene->addItem(label);
			bounds = hasBounds ? bounds.united(polygon.boundingRect()) : polygon.boundingRect();
			hasBounds = true;
			++visibleRooms;
		}
		for (int index : detailDecks.value(deck))
		{
			const AuLegacyShip::Detail& detail = design.details[index];
			const QPointF point = ScenePoint(AuLegacyShip::DecodePoint(detail.packedXY));
			DetailMarker* item = new DetailMarker(detail);
			item->setPos(point);
			item->setData(RecordKind, DetailKind);
			item->setData(RecordIndex, index);
			const EquipmentChoice* choice = EquipmentFor(detail.detailType, detail.commodityIdNormalized);
			item->setToolTip(QStringLiteral("%1 | detail %2 | deck %3")
				.arg(choice ? QString::fromLatin1(choice->name) : AuLegacyShip::DetailName(detail.detailType))
				.arg(detail.detailId).arg(deck));
			item->setVisible(detailsAction->isChecked());
			scene->addItem(item);
			const QRectF pointBounds(point - QPointF(0.1, 0.1), QSizeF(0.2, 0.2));
			bounds = hasBounds ? bounds.united(pointBounds) : pointBounds;
			hasBounds = true;
		}
		if (warningCount > geometryWarnings.size())
			geometryWarnings.append(QStringLiteral("... and %1 more invalid boundaries").arg(warningCount - geometryWarnings.size()));
		const double margin = AuLegacyShip::GridSpacing * 1.5;
		deckBounds = (hasBounds ? GridBounds().united(bounds) : GridBounds()).adjusted(-margin, -margin, margin, margin);
		const double extent = AuLegacyShip::GridSpacing * 66.0;
		scene->setSceneRect(QRectF(-extent, -extent, extent * 2.0, extent * 2.0).united(deckBounds));
		view->emptyMessage = hasBounds ? QString() : (warningCount
			? QStringLiteral("No displayable geometry on this deck. See the inspector for boundary warnings.")
			: QStringLiteral("This deck is empty. Select Paint to add room cells."));
		QString message = QStringLiteral("Deck %1 (Z %2 ft): %3 room boundaries, %4 details")
			.arg(deck).arg(deck * AuLegacyShip::DeckHeight).arg(visibleRooms).arg(detailDecks.value(deck).size());
		if (warningCount)
			message += QStringLiteral("; %1 geometry warnings (see inspector)").arg(warningCount);
		window->statusBar()->showMessage(message);
		ShowOverview();
		if (fit && !renderAction->isChecked())
			FitDeck();
		else
		{
			view->setTransform(previousTransform);
			view->horizontalScrollBar()->setValue(previousHorizontal);
			view->verticalScrollBar()->setValue(previousVertical);
		}
		view->viewport()->update();
		ShowRenderHelp();
	}

	void IndexDecks()
	{
		roomDecks.clear();
		detailDecks.clear();
		QSet<int> occupied;
		for (int i = 0; i < design.rooms.size(); ++i)
		{
			const AuLegacyShip::Room& room = design.rooms[i];
			if (!room.packedVertices.isEmpty())
			{
				roomDecks[int(room.deck)].append(i);
				occupied.insert(int(room.deck));
			}
		}
		for (int i = 0; i < design.details.size(); ++i)
		{
			const int deck = design.details[i].deck;
			detailDecks[deck].append(i);
			occupied.insert(deck);
		}
		occupiedDeckCount = occupied.size();
	}

	void UpdateTitle()
	{
		window->setWindowTitle(QStringLiteral("%1%2 \u2014 Legacy Spacecraft Designer")
			.arg(modified ? QStringLiteral("*") : QString(), DisplayName()));
	}

	void SetDesign(const QString& path, AuLegacyShip::Design&& incoming)
	{
		scene->clear();
		design = std::move(incoming);
		fileName = path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath();
		loaded = true;
		modified = false;
		renderDirty = true;
		renderNeedsFit = true;
		IndexDecks();
		UpdateTitle();
		editPanel->setEnabled(true);
		RefreshHeaderControls();
		SetToolMode(toolMode);
		ShowDeck(0);
	}

	AuLegacyShipViewer* window;
	// Use the same preference store for embedded and standalone launches.
	QSettings settings;
	QGraphicsScene* scene;
	DeckView* view;
	AuLegacyShipRender* renderView;
	QStackedWidget* views;
	QSpinBox* roomNumber;
	QWidget* editPanel;
	QDockWidget* toolDock;
	QTabWidget* informationTabs;
	QCheckBox* streamline;
	QCheckBox* cutaway;
	QComboBox* minimumTech;
	QLabel* toolLabel;
	QLabel* coordinates;
	QLabel* brushSize;
	QPlainTextEdit* inspector;
	QAction* roomsAction;
	QAction* detailsAction;
	QAction* gridAction;
	QAction* renderAction;
	QAction* closeAction;
	QAction* inspectAction;
	QAction* eraseAction;
	QAction* deleteEquipmentAction;
	QAction* deleteEquipmentAreaAction;
	QAction* brushUpAction;
	QAction* brushDownAction;
	QAction* selectedEquipmentAction = nullptr;
	QActionGroup* tools;
	QAction* deckUpAction;
	QAction* deckDownAction;
	QAction* armorUpAction;
	QAction* armorDownAction;
	QMap<int, QAction*> roomActions;
	QAction* lineActions[4];
	std::function<void()> exitHandler;
	AuLegacyShip::Design design;
	QString fileName;
	QStringList geometryWarnings;
	QMap<int, QVector<int> > roomDecks;
	QMap<int, QVector<int> > detailDecks;
	QRectF deckBounds = GridBounds();
	ToolMode toolMode = InspectMode;
	int selectedRoomType = 2;
	const EquipmentChoice* selectedEquipment = nullptr;
	int selectedEquipmentType = 0;
	int equipmentBrushRadius = 0;
	int currentDeck = 0;
	int occupiedDeckCount = 0;
	bool loaded = false;
	bool modified = false;
	bool fittedOnShow = false;
	bool renderDirty = true;
	bool renderNeedsFit = true;
};

AuLegacyShipViewer::AuLegacyShipViewer(QWidget* parent)
	: QMainWindow(parent), m_Impl(new Impl(this))
{
	setWindowTitle(QStringLiteral("Legacy Spacecraft Designer"));
	resize(1200, 800);
	NewDesign();
}

AuLegacyShipViewer::~AuLegacyShipViewer()
{
	delete m_Impl;
}

void AuLegacyShipViewer::SetExitHandler(const std::function<void()>& handler)
{
	m_Impl->exitHandler = handler;
	m_Impl->closeAction->setText(handler ? QStringLiteral("E&xit Designer") : QStringLiteral("&Close"));
}

void AuLegacyShipViewer::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);
	if (!m_Impl->fittedOnShow)
	{
		m_Impl->fittedOnShow = true;
		QTimer::singleShot(0, this, [this]() { m_Impl->FitDeck(); });
	}
}

bool AuLegacyShipViewer::ConfirmDiscardChanges()
{
	if (!m_Impl->modified)
		return true;
	return QMessageBox::question(this, QStringLiteral("Discard session edits?"),
		QStringLiteral("This design has unsaved edits. Saving .ship files is not available yet.\n\n"
			"Discard these edits and continue?"), QMessageBox::Discard | QMessageBox::Cancel,
		QMessageBox::Cancel) == QMessageBox::Discard;
}

void AuLegacyShipViewer::closeEvent(QCloseEvent* event)
{
	if (!ConfirmDiscardChanges())
	{
		event->ignore();
		return;
	}
	QMainWindow::closeEvent(event);
}

void AuLegacyShipViewer::NewDesign()
{
	if (!ConfirmDiscardChanges())
		return;
	AuLegacyShip::Design incoming;
	incoming.header.version = 2;
	incoming.header.versionStateRaw = 0x20;
	incoming.header.streamline = 1;
	incoming.header.scalarSlots.fill(0, 8);
	m_Impl->SetDesign(QString(), std::move(incoming));
}

bool AuLegacyShipViewer::OpenFile(const QString& fileName)
{
	AuLegacyShip::Design incoming;
	QString error;
	if (!AuLegacyShip::Load(fileName, incoming, error))
	{
		QMessageBox::warning(this, QStringLiteral("Could not open legacy spacecraft"),
			QStringLiteral("%1\n\n%2").arg(fileName, error));
		return false;
	}
	if (!ConfirmDiscardChanges())
		return false;
	m_Impl->SetDesign(fileName, std::move(incoming));
	m_Impl->settings.setValue(QStringLiteral("LegacySpacecraft/LastOpenDirectory"),
		QFileInfo(fileName).absolutePath());
	m_Impl->settings.sync();
	return true;
}

void AuLegacyShipViewer::ChooseFile()
{
	QString directory = m_Impl->fileName.isEmpty()
		? m_Impl->settings.value(QStringLiteral("LegacySpacecraft/LastOpenDirectory")).toString()
		: QFileInfo(m_Impl->fileName).absolutePath();
	if(directory.isEmpty() || !QFileInfo(directory).isDir())
		directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	if(directory.isEmpty() || !QFileInfo(directory).isDir())
		directory = QDir::homePath();
	const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Open 2016 spacecraft design"),
		directory, QStringLiteral("2016 spacecraft designs (*.ship);;All files (*)"));
	if (!selected.isEmpty())
		OpenFile(selected);
}
