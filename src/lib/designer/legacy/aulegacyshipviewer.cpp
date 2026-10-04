#include "aulegacyshipviewer.h"
#include "aulegacyship.h"
#include "aulegacyshipedit.h"
#include "aulegacyshipgeometry.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeySequence>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QSet>
#include <QSettings>
#include <QScrollBar>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QStyleOptionGraphicsItem>
#include <QToolBar>
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
	DetailKind
};

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
};

const RoomChoice RoomChoices[] = {
	{ 1, "Barracks" }, { 2, "Bridge" }, { 3, "Brig" }, { 4, "Officer Cabin" },
	{ 5, "Hold" }, { 6, "Hall" }, { 7, "Crew Cabin" }, { 8, "Engine Room" },
	{ 9, "Fuel Cell" }, { 10, "Galley" }, { 11, "Gravity Well" }, { 13, "Vehicle Bay" },
	{ 14, "Lounge" }, { 15, "Open to Deck Below" }, { 16, "Sensor Room" }, { 17, "Shield Room" },
	{ 18, "Sickbay" }, { 19, "Stateroom" }, { 20, "Transporter Room" }, { 21, "Weapon Bay" }
};

QString RoomName(int type)
{
	for (const RoomChoice& choice : RoomChoices)
		if (choice.type == type)
			return QString::fromLatin1(choice.name);
	return QStringLiteral("Type %1").arg(type);
}

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
		setMouseTracking(true);
		setBackgroundBrush(QColor(20, 27, 35));
		setAccessibleName(QStringLiteral("Legacy spacecraft deck plan"));
	}

	bool gridVisible = true;
	quint8 gridMask = 5;
	bool editing = false;
	bool erasing = false;
	QString emptyMessage;
	std::function<void(const QPointF&)> cursorMoved;
	std::function<void(const QPointF&)> cellEntered;

	void SetEditing(bool active, bool erase)
	{
		editing = active;
		erasing = erase;
		setDragMode(active ? QGraphicsView::NoDrag : QGraphicsView::ScrollHandDrag);
		viewport()->setCursor(active ? Qt::CrossCursor : Qt::ArrowCursor);
		viewport()->update();
	}

protected:
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
			if (cellEntered)
				cellEntered(m_Cursor);
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
			if ((event->buttons() & Qt::LeftButton) && cellEntered)
				cellEntered(m_Cursor);
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
		if (!gridVisible)
			return;

		const double pixelsPerFoot = std::abs(transform().m11());
		if (pixelsPerFoot <= 0.0)
			return;
		const double stride = std::max(1.0, std::ceil(std::max(
			3.0 / (AuLegacyShip::GridSpacing * pixelsPerFoot),
			(rect.width() + rect.height()) / (AuLegacyShip::GridSpacing * 1600.0))));
		const double step = AuLegacyShip::GridSpacing * stride;
		QPen pen(QColor(43, 54, 67));
		pen.setCosmetic(true);
		painter->setPen(pen);
		const double left = std::floor(rect.left() / step) * step;
		const double top = std::floor(rect.top() / step) * step;
		if (gridMask & 4)
			for (double x = left; x <= rect.right(); x += step)
				painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
		if (gridMask & 1)
			for (double y = top; y <= rect.bottom(); y += step)
				painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
		if (gridMask & 2)
		{
			const double first = std::floor((rect.left() + rect.top()) / step) * step;
			for (double c = first; c <= rect.right() + rect.bottom(); c += step)
				painter->drawLine(QPointF(rect.left(), c - rect.left()), QPointF(rect.right(), c - rect.right()));
		}
		if (gridMask & 8)
		{
			const double first = std::floor((rect.top() - rect.right()) / step) * step;
			for (double c = first; c <= rect.bottom() - rect.left(); c += step)
				painter->drawLine(QPointF(rect.left(), rect.left() + c), QPointF(rect.right(), rect.right() + c));
		}

		pen.setColor(QColor(91, 112, 132));
		painter->setPen(pen);
		painter->drawLine(QPointF(0.0, rect.top()), QPointF(0.0, rect.bottom()));
		painter->drawLine(QPointF(rect.left(), 0.0), QPointF(rect.right(), 0.0));
	}

	void drawForeground(QPainter* painter, const QRectF& rect) override
	{
		QGraphicsView::drawForeground(painter, rect);
		if (!editing || !m_CursorKnown)
			return;
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
	void UpdateCursor(const QPoint& pixel)
	{
		m_Cursor = ScenePoint(mapToScene(pixel));
		m_CursorKnown = viewport()->rect().contains(pixel);
		if (cursorMoved)
			cursorMoved(m_Cursor);
		viewport()->update();
	}

	QPointF m_Cursor;
	QPoint m_PanPosition;
	bool m_CursorKnown = false;
	bool m_Panning = false;
};

class DetailMarker : public QGraphicsItem
{
public:
	explicit DetailMarker(unsigned int detailType)
		: m_Color(RecordColor(detailType)), m_Shape(detailType % 3u)
	{
		setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIgnoresTransformations);
		setZValue(10.0);
	}

	QRectF boundingRect() const override
	{
		return QRectF(-5.0, -5.0, 10.0, 10.0);
	}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) override
	{
		const bool selected = (option->state & QStyle::State_Selected) != 0;
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
};
}

class AuLegacyShipViewer::Impl
{
public:
	enum ToolMode { InspectMode, PaintMode, EraseMode };

	explicit Impl(AuLegacyShipViewer* owner) : window(owner),
		settings(QSettings::NativeFormat, QSettings::UserScope,
			QStringLiteral("Hazeron Freehold"), QStringLiteral("Shores of Hazeron"))
	{
		scene = new QGraphicsScene(window);
		view = new DeckView(scene, window);
		view->emptyMessage = QStringLiteral("Open a 2016 .ship file to inspect or edit its decks.\n\n"
			"Mouse wheel: zoom   \u2022   Middle drag: pan\n"
			"Edits last for this session. Saving is not available yet.");
		scene->setSceneRect(-40.0, -40.0, 80.0, 80.0);

		QAction* openAction = new QAction(QStringLiteral("&Open .ship..."), window);
		openAction->setShortcut(QKeySequence::Open);
		QObject::connect(openAction, &QAction::triggered, window, &AuLegacyShipViewer::ChooseFile);
		QAction* fitAction = new QAction(QStringLiteral("&Fit deck"), window);
		fitAction->setShortcut(QKeySequence(QStringLiteral("F")));
		QObject::connect(fitAction, &QAction::triggered, window, [this]() { FitDeck(); });
		roomsAction = new QAction(QStringLiteral("Rooms"), window);
		detailsAction = new QAction(QStringLiteral("Details"), window);
		gridAction = new QAction(QStringLiteral("Grid"), window);
		gridAction->setToolTip(QStringLiteral("Show the selected grid line families. Lines thin at distant zoom levels; painting always uses the original grid."));
		for (QAction* action : { roomsAction, detailsAction, gridAction })
		{
			action->setCheckable(true);
			action->setChecked(true);
		}
		QObject::connect(roomsAction, &QAction::toggled, window, [this](bool visible) { SetLayerVisible(RoomKind, visible); });
		QObject::connect(detailsAction, &QAction::toggled, window, [this](bool visible) { SetLayerVisible(DetailKind, visible); });
		QObject::connect(gridAction, &QAction::toggled, window, [this](bool visible) {
			view->gridVisible = visible;
			view->viewport()->update();
		});

		window->menuBar()->setNativeMenuBar(false);
		const QString menuStyle = QStringLiteral(
			"QMenuBar, QMenu { background: #141b23; color: #dce5ef; }"
			"QMenuBar { border-bottom: 1px solid #344454; }"
			"QMenuBar::item { background: transparent; padding: 5px 10px; }"
			"QMenuBar::item:selected { background: #30465b; }"
			"QMenuBar::item:pressed { background: #3c5972; }"
			"QMenu { border: 1px solid #344454; padding: 4px; }"
			"QMenu::item { padding: 6px 28px; }"
			"QMenu::item:selected { background: #30465b; color: #ffffff; }"
			"QMenu::separator { height: 1px; background: #344454; margin: 4px 8px; }"
			"QMenuBar::item:disabled, QMenu::item:disabled { color: #7e8b99; }");
		window->menuBar()->setStyleSheet(menuStyle);
		QMenu* fileMenu = window->menuBar()->addMenu(QStringLiteral("&File"));
		fileMenu->setStyleSheet(menuStyle);
		fileMenu->addAction(openAction);
		closeAction = fileMenu->addAction(QStringLiteral("&Close"));
		closeAction->setShortcut(QKeySequence::Close);
		QObject::connect(closeAction, &QAction::triggered, window, [this]() {
			if (exitHandler)
			{
				const std::function<void()> handler = exitHandler;
				QTimer::singleShot(0, window, [handler]() { handler(); });
			}
			else
				window->close();
		});
		QMenu* viewMenu = window->menuBar()->addMenu(QStringLiteral("&View"));
		viewMenu->setStyleSheet(menuStyle);
		viewMenu->addAction(fitAction);
		viewMenu->addSeparator();
		viewMenu->addAction(roomsAction);
		viewMenu->addAction(detailsAction);
		viewMenu->addAction(gridAction);

		QToolBar* toolbar = window->addToolBar(QStringLiteral("Legacy spacecraft"));
		toolbar->setMovable(false);
		toolbar->setStyleSheet(QStringLiteral(
			"QToolBar { background: #141b23; border: 0; border-bottom: 1px solid #344454; spacing: 4px; padding: 4px; }"
			"QToolBar QLabel { color: #dce5ef; background: transparent; }"
			"QToolBar::separator { background: #344454; width: 1px; margin: 4px 6px; }"
			"QToolButton { color: #dce5ef; background: transparent; border-image: none;"
			" border: 1px solid transparent; border-radius: 3px; padding: 5px 8px; }"
			"QToolButton:checked { background: #24394b; border-color: #50728f; }"
			"QToolButton:hover { background: #30465b; border-color: #6084a3; }"
			"QToolButton:pressed { background: #3c5972; }"
			"QToolButton:disabled { color: #7e8b99; background: transparent; border-color: transparent; }"
			"QComboBox { color: #dce5ef; background: #1d2833; border-image: none;"
			" border: 1px solid #50728f; border-radius: 3px; padding: 4px 24px 4px 8px; }"
			"QComboBox:hover, QComboBox:focus { border-color: #89b9de; }"
			"QComboBox:on { background: #30465b; }"
			"QComboBox:disabled { color: #7e8b99; border-color: #344454; }"
			"QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right;"
			" width: 20px; border: 0; border-image: none; background: #24394b; }"
			"QComboBox QAbstractItemView { background: #1d2833; color: #dce5ef;"
			" border: 1px solid #50728f; selection-background-color: #30465b; selection-color: #ffffff; }"
			"QSpinBox { color: #dce5ef; background: #1d2833; border: 1px solid #50728f;"
			" border-radius: 3px; padding: 4px 6px; selection-background-color: #30465b; }"
			"QSpinBox:disabled { color: #7e8b99; border-color: #344454; }"
			"QSpinBox::up-button, QSpinBox::down-button { background: #24394b; border: 0; }"));
		QPalette controlsPalette = toolbar->palette();
		for (QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
		{
			controlsPalette.setColor(role, QColor(220, 229, 239));
			controlsPalette.setColor(QPalette::Disabled, role, QColor(126, 139, 153));
		}
		toolbar->setPalette(controlsPalette);
		fileMenu->setPalette(controlsPalette);
		viewMenu->setPalette(controlsPalette);
		toolbar->addAction(openAction);
		toolbar->addAction(fitAction);
		toolbar->addSeparator();
		toolbar->addWidget(new QLabel(QStringLiteral("  Deck: "), toolbar));
		deckCombo = new QComboBox(toolbar);
		deckCombo->setAccessibleName(QStringLiteral("Displayed deck"));
		deckCombo->setMinimumContentsLength(16);
		deckCombo->setEnabled(false);
		toolbar->addWidget(deckCombo);
		QObject::connect(deckCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
			window, [this](int index) { if (index >= 0) ShowDeck(deckCombo->itemData(index).toInt()); });
		toolbar->addSeparator();
		toolbar->addAction(roomsAction);
		toolbar->addAction(detailsAction);
		toolbar->addAction(gridAction);

		window->addToolBarBreak();
		editToolbar = window->addToolBar(QStringLiteral("Legacy room cells"));
		editToolbar->setMovable(false);
		editToolbar->setStyleSheet(toolbar->styleSheet());
		editToolbar->setPalette(controlsPalette);
		QActionGroup* tools = new QActionGroup(window);
		tools->setExclusive(true);
		const QString toolNames[] = { QStringLiteral("Inspect"), QStringLiteral("Paint"), QStringLiteral("Erase Cells") };
		for (int mode = InspectMode; mode <= EraseMode; ++mode)
		{
			QAction* action = editToolbar->addAction(toolNames[mode]);
			action->setCheckable(true);
			action->setChecked(mode == InspectMode);
			tools->addAction(action);
			QObject::connect(action, &QAction::triggered, window, [this, mode]() { SetToolMode(ToolMode(mode)); });
		}
		editToolbar->addSeparator();
		editToolbar->addWidget(new QLabel(QStringLiteral("Room: "), editToolbar));
		roomType = new QComboBox(editToolbar);
		roomType->setAccessibleName(QStringLiteral("Room type to paint"));
		for (const RoomChoice& choice : RoomChoices)
			roomType->addItem(QString::fromLatin1(choice.name), choice.type);
		roomType->setCurrentIndex(roomType->findData(2));
		editToolbar->addWidget(roomType);
		editToolbar->addWidget(new QLabel(QStringLiteral("Number: "), editToolbar));
		roomNumber = new QSpinBox(editToolbar);
		roomNumber->setRange(0, 255);
		roomNumber->setWrapping(true);
		roomNumber->setValue(0);
		roomNumber->setAccessibleName(QStringLiteral("Room number to paint"));
		roomNumber->setToolTip(QStringLiteral("Distinguishes rooms of the same type on this deck. This is the room subtype, not its record ID."));
		editToolbar->addWidget(roomNumber);
		editToolbar->addSeparator();
		editToolbar->addWidget(new QLabel(QStringLiteral("Grid: "), editToolbar));
		const QString lineNames[] = { QStringLiteral("0\u00b0"), QStringLiteral("45\u00b0"),
			QStringLiteral("90\u00b0"), QStringLiteral("135\u00b0") };
		const QString lineTips[] = { QStringLiteral("Lateral grid lines (on the beam)"),
			QStringLiteral("Diagonal grid lines (port quarter)"), QStringLiteral("Longitudinal grid lines (fore and aft)"),
			QStringLiteral("Diagonal grid lines (starboard quarter)") };
		for (int i = 0; i < 4; ++i)
		{
			lineActions[i] = editToolbar->addAction(lineNames[i]);
			lineActions[i]->setCheckable(true);
			lineActions[i]->setChecked((5 & (1 << i)) != 0);
			lineActions[i]->setToolTip(lineTips[i]);
			QObject::connect(lineActions[i], &QAction::toggled, window, [this](bool) { UpdateGrid(); });
		}
		editToolbar->setEnabled(false);
		roomType->setEnabled(false);
		roomNumber->setEnabled(false);

		QWidget* central = new QWidget(window);
		QVBoxLayout* layout = new QVBoxLayout(central);
		summary = new QLabel(QStringLiteral("Legacy spacecraft designer \u2014 session edits only; saving is not available yet"), central);
		summary->setTextFormat(Qt::PlainText);
		summary->setWordWrap(true);
		layout->addWidget(summary);
		QSplitter* splitter = new QSplitter(Qt::Horizontal, central);
		splitter->addWidget(view);
		inspector = new QPlainTextEdit(splitter);
		inspector->setReadOnly(true);
		inspector->setAccessibleName(QStringLiteral("Legacy design and selected record information"));
		inspector->setMinimumWidth(240);
		splitter->addWidget(inspector);
		splitter->setStretchFactor(0, 1);
		splitter->setStretchFactor(1, 0);
		splitter->setSizes(QList<int>() << 850 << 310);
		layout->addWidget(splitter, 1);
		QLabel* legend = new QLabel(QStringLiteral("X right, Y up; coordinates in feet. Full grid cells: 5.656854249 ft. "
			"Colors group numeric room types; detail colors and shapes group numeric detail types."), central);
		legend->setWordWrap(true);
		layout->addWidget(legend);
		window->setCentralWidget(central);
		coordinates = new QLabel(QStringLiteral("X \u2014   Y \u2014 ft"), window);
		window->statusBar()->addPermanentWidget(coordinates);
		view->cursorMoved = [this](const QPointF& point) {
			coordinates->setText(QStringLiteral("X %1   Y %2 ft").arg(point.x(), 0, 'f', 2).arg(point.y(), 0, 'f', 2));
		};
		view->cellEntered = [this](const QPointF& point) { EditAt(point); };
		QObject::connect(scene, &QGraphicsScene::selectionChanged, window, [this]() { ShowSelection(); });
		ShowOverview();
	}

	~Impl()
	{
		QObject::disconnect(scene, nullptr, window, nullptr);
		view->cursorMoved = std::function<void(const QPointF&)>();
		view->cellEntered = std::function<void(const QPointF&)>();
	}

	void FitDeck()
	{
		view->fitInView(deckBounds, Qt::KeepAspectRatio);
	}

	void SetToolMode(ToolMode mode)
	{
		toolMode = mode;
		view->SetEditing(loaded && mode != InspectMode, mode == EraseMode);
		roomType->setEnabled(loaded && mode == PaintMode);
		roomNumber->setEnabled(loaded && mode == PaintMode);
		window->statusBar()->showMessage(mode == InspectMode
			? QStringLiteral("Click a room or detail to inspect it. Middle drag pans; the wheel zooms.")
			: QStringLiteral("Left click or drag to %1 cells. Middle drag pans. Edits are not saved.")
				.arg(mode == EraseMode ? QStringLiteral("erase") : QStringLiteral("paint")));
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
		if (!loaded || toolMode == InspectMode || deckCombo->currentIndex() < 0)
			return;
		const int deck = deckCombo->currentData().toInt();
		const int type = toolMode == EraseMode ? -1 : roomType->currentData().toInt();
		const AuLegacyShip::CellEditResult result = AuLegacyShip::EditCell(design, deck, cursor,
			view->gridMask, type, quint8(roomNumber->value()));
		if (!result.accepted)
		{
			window->statusBar()->showMessage(result.error.isEmpty()
				? QStringLiteral("This cell edit could not be applied.") : result.error);
			return;
		}
		if (result.changed)
		{
			modified = true;
			IndexDecks(deck);
			UpdateSummary();
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
			.arg(DisplayName(), fileName).arg(header.designId)
			.arg(header.architectName.isEmpty() ? QStringLiteral("(not stored)") : header.architectName)
			.arg(header.architectId).arg(header.version).arg(header.state).arg(header.streamline).arg(header.armor);
		const bool hasExtendedHeader = header.version > 1;
		const QString absent = QStringLiteral("(not stored in this format version)");
		text += QStringLiteral("Minimum tech level: %1\nHull volume: %2\nStructure: %3\nTech level: %4\n\n"
			"Current rooms: %5\nCurrent details: %6\nDecks with geometry: %7\nSource bytes read: %8\n")
			.arg(header.minimumTechLevel).arg(hasExtendedHeader ? QString::number(header.hullVolume) : absent)
			.arg(hasExtendedHeader ? QString::number(header.structure) : absent)
			.arg(hasExtendedHeader ? QString::number(header.techLevel) : absent)
			.arg(design.rooms.size()).arg(design.details.size()).arg(occupiedDeckCount).arg(design.bytesConsumed);
		text += modified ? QStringLiteral("\nThis design has session edits. Saving is not available yet.\n")
			: QStringLiteral("\nNo session edits. Saving is not available yet.\n");
		text += QStringLiteral("\nUse Inspect to select a room or detail for its record. "
			"Marker shapes and colors distinguish records; they do not represent recovered equipment shapes.\n");
		if (!geometryWarnings.isEmpty())
			text += QStringLiteral("\nGeometry warnings for this deck:\n") + geometryWarnings.join(QLatin1Char('\n'));
		return text;
	}

	QString DisplayName() const
	{
		return design.header.name.isEmpty() ? QFileInfo(fileName).completeBaseName() : design.header.name;
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
		text += QStringLiteral("Room type: %1\nRoom number (subtype): %2 (%3)\nBoundary: %4\n\nPoints (X, Y feet):\n")
			.arg(RoomName(room.roomType)).arg(room.roomSubtype).arg(Hex(room.roomSubtype))
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
			.arg(Hex(detail.streamOffset, 8)).arg(Hex(detail.packedXY, 4))
			+ QStringLiteral("\nNumeric detail type: %1\nHidden byte (raw): %2\n"
				"Room ID: %3\nCommodity ID (raw): %4\n"
				"Commodity ID after legacy normalization: %5\n\n"
				"This view uses numeric equipment types and marker symbols. Hidden details are included in this inspection view.")
				.arg(Hex(detail.detailType)).arg(Hex(detail.hiddenRaw)).arg(detail.roomId)
				.arg(Hex(detail.commodityIdRaw, 4)).arg(Hex(detail.commodityIdNormalized, 4));
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
			inspector->setPlainText(RoomInformation(index));
		else if (kind == DetailKind && index >= 0 && index < design.details.size())
			inspector->setPlainText(DetailInformation(index));
	}

	void ShowDeck(int deck, bool fit = true)
	{
		const QTransform previousTransform = view->transform();
		const int previousHorizontal = view->horizontalScrollBar()->value();
		const int previousVertical = view->verticalScrollBar()->value();
		scene->clear();
		geometryWarnings.clear();
		QRectF bounds;
		bool hasBounds = false;
		int visibleRooms = 0;
		int warningCount = 0;
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
				.arg(RoomName(room.roomType)).arg(room.roomSubtype).arg(room.roomId)
				.arg(room.roomType).arg(Hex(room.roomType)).arg(deck));
			item->setVisible(roomsAction->isChecked());
			bounds = hasBounds ? bounds.united(polygon.boundingRect()) : polygon.boundingRect();
			hasBounds = true;
			++visibleRooms;
		}
		for (int index : detailDecks.value(deck))
		{
			const AuLegacyShip::Detail& detail = design.details[index];
			const QPointF point = ScenePoint(AuLegacyShip::DecodePoint(detail.packedXY));
			DetailMarker* item = new DetailMarker(detail.detailType);
			item->setPos(point);
			item->setData(RecordKind, DetailKind);
			item->setData(RecordIndex, index);
			item->setToolTip(QStringLiteral("Detail %1 | numeric type %2 | deck %3")
				.arg(detail.detailId).arg(Hex(detail.detailType)).arg(deck));
			item->setVisible(detailsAction->isChecked());
			scene->addItem(item);
			const QRectF pointBounds(point - QPointF(0.1, 0.1), QSizeF(0.2, 0.2));
			bounds = hasBounds ? bounds.united(pointBounds) : pointBounds;
			hasBounds = true;
		}
		if (warningCount > geometryWarnings.size())
			geometryWarnings.append(QStringLiteral("... and %1 more invalid boundaries").arg(warningCount - geometryWarnings.size()));
		const double margin = AuLegacyShip::GridSpacing * 1.5;
		deckBounds = hasBounds ? bounds.adjusted(-margin, -margin, margin, margin) : QRectF(-40.0, -40.0, 80.0, 80.0);
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
		if (fit)
			FitDeck();
		else
		{
			view->setTransform(previousTransform);
			view->horizontalScrollBar()->setValue(previousHorizontal);
			view->verticalScrollBar()->setValue(previousVertical);
		}
		view->viewport()->update();
	}

	void IndexDecks(int selectedDeck)
	{
		roomDecks.clear();
		detailDecks.clear();
		QSet<int> decks;
		QSet<int> occupied;
		for (int deck = -28; deck <= 28; ++deck)
			decks.insert(deck);
		for (int i = 0; i < design.rooms.size(); ++i)
		{
			const AuLegacyShip::Room& room = design.rooms[i];
			decks.insert(int(room.deck));
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
			decks.insert(deck);
			occupied.insert(deck);
		}
		occupiedDeckCount = occupied.size();
		decks.insert(selectedDeck);
		QList<int> sortedDecks = decks.values();
		std::sort(sortedDecks.begin(), sortedDecks.end());
		deckCombo->blockSignals(true);
		deckCombo->clear();
		for (int deck : sortedDecks)
			deckCombo->addItem(QStringLiteral("%1  (Z %2 ft)%3").arg(deck).arg(deck * AuLegacyShip::DeckHeight)
				.arg(occupied.contains(deck) ? QString() : QStringLiteral(" \u2014 empty")), deck);
		deckCombo->setCurrentIndex(deckCombo->findData(selectedDeck));
		deckCombo->setEnabled(true);
		deckCombo->blockSignals(false);
	}

	void UpdateSummary()
	{
		window->setWindowTitle(QStringLiteral("%1%2 \u2014 Legacy Spacecraft Designer")
			.arg(modified ? QStringLiteral("*") : QString(), DisplayName()));
		summary->setText(QStringLiteral("%1 \u2014 %2 rooms, %3 details, %4 occupied decks \u2014 %5; saving is not available yet")
			.arg(DisplayName()).arg(design.rooms.size()).arg(design.details.size()).arg(occupiedDeckCount)
			.arg(modified ? QStringLiteral("session edits") : QStringLiteral("no session edits")));
	}

	void SetDesign(const QString& path, AuLegacyShip::Design&& incoming)
	{
		scene->clear();
		design = std::move(incoming);
		fileName = QFileInfo(path).absoluteFilePath();
		loaded = true;
		modified = false;
		IndexDecks(0);
		UpdateSummary();
		editToolbar->setEnabled(true);
		SetToolMode(toolMode);
		ShowDeck(0);
	}

	AuLegacyShipViewer* window;
	// Use the same preference store for embedded and standalone launches.
	QSettings settings;
	QGraphicsScene* scene;
	DeckView* view;
	QComboBox* deckCombo;
	QComboBox* roomType;
	QSpinBox* roomNumber;
	QToolBar* editToolbar;
	QLabel* summary;
	QLabel* coordinates;
	QPlainTextEdit* inspector;
	QAction* roomsAction;
	QAction* detailsAction;
	QAction* gridAction;
	QAction* closeAction;
	QAction* lineActions[4];
	std::function<void()> exitHandler;
	AuLegacyShip::Design design;
	QString fileName;
	QStringList geometryWarnings;
	QMap<int, QVector<int> > roomDecks;
	QMap<int, QVector<int> > detailDecks;
	QRectF deckBounds = QRectF(-40.0, -40.0, 80.0, 80.0);
	ToolMode toolMode = InspectMode;
	int occupiedDeckCount = 0;
	bool loaded = false;
	bool modified = false;
	bool fittedOnShow = false;
};

AuLegacyShipViewer::AuLegacyShipViewer(QWidget* parent)
	: QMainWindow(parent), m_Impl(new Impl(this))
{
	setWindowTitle(QStringLiteral("Legacy Spacecraft Designer"));
	resize(1200, 800);
}

AuLegacyShipViewer::~AuLegacyShipViewer()
{
	delete m_Impl;
}

void AuLegacyShipViewer::SetExitHandler(const std::function<void()>& handler)
{
	m_Impl->exitHandler = handler;
	m_Impl->closeAction->setText(handler ? QStringLiteral("E&xit Designer") : QStringLiteral("&Close"));
	if (handler)
		menuBar()->setNativeMenuBar(false);
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
		QStringLiteral("This design has unsaved room edits. Saving .ship files is not available yet.\n\n"
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
