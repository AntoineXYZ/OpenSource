#include "aulegacyshipviewer.h"
#include "aulegacyship.h"

#include <QAction>
#include <QComboBox>
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
#include <QShowEvent>
#include <QSplitter>
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
	QString emptyMessage;
	std::function<void(const QPointF&)> cursorMoved;

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
		event->accept();
	}

	void mouseMoveEvent(QMouseEvent* event) override
	{
		QGraphicsView::mouseMoveEvent(event);
		if (cursorMoved)
		{
			const QPointF point = mapToScene(event->pos());
			cursorMoved(QPointF(point.x(), -point.y()));
		}
	}

	void drawBackground(QPainter* painter, const QRectF& rect) override
	{
		painter->fillRect(rect, backgroundBrush());
		if (!gridVisible)
			return;

		double step = AuLegacyShip::GridSpacing;
		const double pixelsPerFoot = std::abs(transform().m11());
		if (pixelsPerFoot <= 0.0)
			return;
		while (step * pixelsPerFoot < 15.0 || rect.width() / step > 300.0 || rect.height() / step > 300.0)
			step *= 2.0;
		QPen pen(QColor(43, 54, 67));
		pen.setCosmetic(true);
		painter->setPen(pen);
		const double left = std::floor(rect.left() / step) * step;
		const double top = std::floor(rect.top() / step) * step;
		for (double x = left; x <= rect.right(); x += step)
			painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
		for (double y = top; y <= rect.bottom(); y += step)
			painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));

		pen.setColor(QColor(91, 112, 132));
		painter->setPen(pen);
		painter->drawLine(QPointF(0.0, rect.top()), QPointF(0.0, rect.bottom()));
		painter->drawLine(QPointF(rect.left(), 0.0), QPointF(rect.right(), 0.0));
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
	explicit Impl(AuLegacyShipViewer* owner) : window(owner)
	{
		scene = new QGraphicsScene(window);
		view = new DeckView(scene, window);
		view->emptyMessage = QStringLiteral("Open a 2016 .ship file to inspect its decks.\n\n"
			"Mouse wheel: zoom   \u2022   Drag empty space: pan\n"
			"Click a room or detail to inspect its stored values.");
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
		gridAction->setToolTip(QStringLiteral("Full cells are 5.656854249 feet wide. The grid coarsens when zoomed out."));
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
			" border: 1px solid #50728f; selection-background-color: #30465b; selection-color: #ffffff; }"));
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

		QWidget* central = new QWidget(window);
		QVBoxLayout* layout = new QVBoxLayout(central);
		summary = new QLabel(QStringLiteral("Offline legacy spacecraft viewer \u2014 read-only"), central);
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
		QObject::connect(scene, &QGraphicsScene::selectionChanged, window, [this]() { ShowSelection(); });
		ShowOverview();
	}

	~Impl()
	{
		QObject::disconnect(scene, nullptr, window, nullptr);
		view->cursorMoved = std::function<void(const QPointF&)>();
	}

	void FitDeck()
	{
		view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
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
			return QStringLiteral("This viewer displays recovered room boundaries and detail positions from 2016 .ship files.\n\n"
				"It works offline. It does not edit, save, or convert designs.\n\n"
				"Room and detail names have not yet been recovered. Numeric values are shown without assigning names or behavior.");
		const AuLegacyShip::Header& header = design.header;
		QString text = QStringLiteral("%1\n\nFile: %2\nDesign ID: %3\nArchitect: %4\nArchitect ID: %5\n"
			"Format version: %6\nState: %7\nStreamline: %8\nArmor: %9\n")
			.arg(DisplayName(), fileName).arg(header.designId)
			.arg(header.architectName.isEmpty() ? QStringLiteral("(not stored)") : header.architectName)
			.arg(header.architectId).arg(header.version).arg(header.state).arg(header.streamline).arg(header.armor);
		const bool hasExtendedHeader = header.version > 1;
		const QString absent = QStringLiteral("(not stored in this format version)");
		text += QStringLiteral("Minimum tech level: %1\nHull volume: %2\nStructure: %3\nTech level: %4\n\n"
			"Stored rooms: %5\nStored details: %6\nDecks with geometry: %7\nBytes read: %8\n")
			.arg(header.minimumTechLevel).arg(hasExtendedHeader ? QString::number(header.hullVolume) : absent)
			.arg(hasExtendedHeader ? QString::number(header.structure) : absent)
			.arg(hasExtendedHeader ? QString::number(header.techLevel) : absent)
			.arg(design.rooms.size()).arg(design.details.size()).arg(deckCombo->count()).arg(design.bytesConsumed);
		text += QStringLiteral("\nSelect a room or detail for its record. Room and detail types use numeric labels. "
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
		QString text = QStringLiteral("Room %1\n\nNumeric room type: %2 (%3)\nDeck: %4\nZ: %5 ft\n"
			"Design ID: %6\nRecord index: %7\nStream offset: %8\nStored vertices: %9\n")
			.arg(room.roomId).arg(room.roomType).arg(Hex(room.roomType)).arg(int(room.deck))
			.arg(int(room.deck) * AuLegacyShip::DeckHeight).arg(room.designId).arg(index)
			.arg(Hex(room.streamOffset, 8)).arg(room.packedVertices.size());
		text += QStringLiteral("Room subtype: %1\nBoundary: %2\n\nStored points (X, Y feet):\n")
			.arg(Hex(room.roomSubtype)).arg(room.IsEllipse() ? QStringLiteral("ellipse from three control points") : QStringLiteral("polygon"));
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
				"Type names and equipment shapes have not yet been recovered. Hidden details are included in this inspection view.")
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

	void ShowDeck(int deck)
	{
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
			item->setToolTip(QStringLiteral("Room %1 | numeric type %2 (%3) | deck %4")
				.arg(room.roomId).arg(room.roomType).arg(Hex(room.roomType)).arg(deck));
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
		scene->setSceneRect(hasBounds ? bounds.adjusted(-margin, -margin, margin, margin) : QRectF(-40.0, -40.0, 80.0, 80.0));
		view->emptyMessage = hasBounds ? QString() : QStringLiteral("No displayable geometry on this deck.");
		QString message = QStringLiteral("Deck %1 (Z %2 ft): %3 room boundaries, %4 details")
			.arg(deck).arg(deck * AuLegacyShip::DeckHeight).arg(visibleRooms).arg(detailDecks.value(deck).size());
		if (warningCount)
			message += QStringLiteral("; %1 geometry warnings (see inspector)").arg(warningCount);
		window->statusBar()->showMessage(message);
		ShowOverview();
		FitDeck();
	}

	void SetDesign(const QString& path, AuLegacyShip::Design&& incoming)
	{
		scene->clear();
		design = std::move(incoming);
		fileName = QFileInfo(path).absoluteFilePath();
		loaded = true;
		roomDecks.clear();
		detailDecks.clear();
		geometryWarnings.clear();
		QSet<int> decks;
		for (int i = 0; i < design.rooms.size(); ++i)
		{
			const AuLegacyShip::Room& room = design.rooms[i];
			if (!room.packedVertices.isEmpty())
			{
				roomDecks[int(room.deck)].append(i);
				decks.insert(int(room.deck));
			}
		}
		for (int i = 0; i < design.details.size(); ++i)
		{
			const int deck = design.details[i].deck;
			detailDecks[deck].append(i);
			decks.insert(deck);
		}
		QList<int> sortedDecks = decks.values();
		std::sort(sortedDecks.begin(), sortedDecks.end());
		deckCombo->blockSignals(true);
		deckCombo->clear();
		for (int deck : sortedDecks)
			deckCombo->addItem(QStringLiteral("%1  (Z %2 ft)").arg(deck).arg(deck * AuLegacyShip::DeckHeight), deck);
		const int initial = std::max(0, deckCombo->findData(0));
		deckCombo->setCurrentIndex(sortedDecks.isEmpty() ? -1 : initial);
		deckCombo->setEnabled(!sortedDecks.isEmpty());
		deckCombo->blockSignals(false);
		window->setWindowTitle(QStringLiteral("%1 \u2014 Legacy Spacecraft Viewer (read-only)").arg(DisplayName()));
		summary->setText(QStringLiteral("%1 \u2014 %2 stored rooms, %3 details, %4 decks \u2014 read-only")
			.arg(DisplayName()).arg(design.rooms.size()).arg(design.details.size()).arg(decks.size()));
		if (!sortedDecks.isEmpty())
			ShowDeck(sortedDecks[initial]);
		else
		{
			scene->setSceneRect(-40.0, -40.0, 80.0, 80.0);
			view->emptyMessage = QStringLiteral("This design contains no room boundaries or details.");
			window->statusBar()->showMessage(QStringLiteral("No decks with geometry; empty room records are omitted as in the legacy loader."));
			ShowOverview();
			FitDeck();
		}
	}

	AuLegacyShipViewer* window;
	QGraphicsScene* scene;
	DeckView* view;
	QComboBox* deckCombo;
	QLabel* summary;
	QLabel* coordinates;
	QPlainTextEdit* inspector;
	QAction* roomsAction;
	QAction* detailsAction;
	QAction* gridAction;
	QAction* closeAction;
	std::function<void()> exitHandler;
	AuLegacyShip::Design design;
	QString fileName;
	QStringList geometryWarnings;
	QMap<int, QVector<int> > roomDecks;
	QMap<int, QVector<int> > detailDecks;
	bool loaded = false;
	bool fittedOnShow = false;
};

AuLegacyShipViewer::AuLegacyShipViewer(QWidget* parent)
	: QMainWindow(parent), m_Impl(new Impl(this))
{
	setWindowTitle(QStringLiteral("Legacy Spacecraft Viewer (read-only)"));
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
	m_Impl->SetDesign(fileName, std::move(incoming));
	return true;
}

void AuLegacyShipViewer::ChooseFile()
{
	const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Open 2016 spacecraft design"),
		m_Impl->fileName, QStringLiteral("2016 spacecraft designs (*.ship);;All files (*)"));
	if (!selected.isEmpty())
		OpenFile(selected);
}
