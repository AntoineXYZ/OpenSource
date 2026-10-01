#include <cmath>
#include "gridview.h"
#include "palette.h"

#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QtMath>

using namespace SimpleMode;

QList<QPair<QPoint, int>> ShapeAt(const Plan& plan, const QPoint& start, int startQ, const bool lines[4]);

void GridView::paintEvent(QPaintEvent* e)
{
	Q_UNUSED(e);
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.fillRect(rect(), QColor(0x05, 0x07, 0x09));
	auto triangle = [&](const QPoint& cell, int q) {
		QPoint corners[3];
		QuarterCornerPoints(cell, q, corners);
		QPolygonF poly;
		for(const QPoint& c : corners)
			poly << LatticePx(c);
		return poly;
	};
	const double pxPerMetre = CellPx / m_Plan.cell;

	QFont small = font();
	small.setPointSizeF(small.pointSizeF() * 0.85);
	p.setFont(small);
	p.setPen(QColor(0x7a, 0x84, 0x94));
	for(int c = 0; c < m_Plan.Cols(); c++)
		p.drawText(QRect(Margin + c * CellPx, 0, CellPx, Margin), Qt::AlignCenter, QString::number(c));
	for(int r = 0; r < m_Plan.Rows(); r++)
		p.drawText(QRect(0, Margin + r * CellPx, Margin, CellPx), Qt::AlignCenter, QString::number(r));

	p.setPen(Qt::NoPen);
	for(int r = 0; r < m_Plan.Rows(); r++)
		for(int c = 0; c < m_Plan.Cols(); c++)
			for(int q = 0; q < 4; q++)
			{
				QPoint cell(c, r);
				QPolygonF tri = triangle(cell, q);
				QChar ch = m_Plan.Quarter(m_Deck, cell, q);
				if(ch == '.')
				{
					p.setBrush(QColor(0x0e, 0x12, 0x17));
					p.drawPolygon(tri);
					QChar below = m_Plan.Quarter(m_Deck - 1, cell, q);
					if(below != '.')
					{
						QColor faint = RoomColor(below);
						faint.setAlpha(80);
						p.setBrush(faint);
						p.drawPolygon(tri);
					}
					continue;
				}
				bool open = IsOpenQuarter(m_Plan, m_Deck, cell, q);
				QColor color = RoomColor(open ? SpaceRoom(m_Plan, m_Deck, cell, q) : ch);
				if(open)
					color = color.darker(140);
				p.setBrush(color);
				p.drawPolygon(tri);
				if(open)
				{
					p.setBrush(QBrush(QColor(0xc8, 0xd0, 0xdc, 90), Qt::BDiagPattern));
					p.drawPolygon(tri);
				}
			}
	p.setBrush(Qt::NoBrush);

	for(int r = 0; r < m_Plan.Rows(); r++)
		for(int c = 0; c < m_Plan.Cols(); c++)
		{
			QRectF rc(Margin + c * CellPx, Margin + r * CellPx, CellPx, CellPx);
			auto pen = [&](Line line) { return QPen(QColor(0x6d, 0x96, 0xd8, m_Lines[line] ? 85 : 22), 1); };
			p.setPen(pen(Horizontal));
			p.drawLine(rc.topLeft(), rc.topRight());
			p.drawLine(rc.bottomLeft(), rc.bottomRight());
			p.setPen(pen(Vertical));
			p.drawLine(rc.topLeft(), rc.bottomLeft());
			p.drawLine(rc.topRight(), rc.bottomRight());
			p.setPen(pen(Slash));
			p.drawLine(rc.bottomLeft(), rc.topRight());
			p.setPen(pen(Backslash));
			p.drawLine(rc.topLeft(), rc.bottomRight());
		}

	if(m_Deck > 0)
	{
		p.setPen(QPen(QColor(0xc8, 0xd0, 0xdc, 90), 2, Qt::DashLine));
		for(int r = 0; r < m_Plan.Rows(); r++)
			for(int c = 0; c < m_Plan.Cols(); c++)
				for(int q = 0; q < 4; q++)
				{
					QPoint cell(c, r);
					if(m_Plan.Quarter(m_Deck - 1, cell, q) == '.')
						continue;
					QPoint corners[3];
					QuarterCornerPoints(cell, q, corners);
					for(int k = 0; k < 3; k++)
					{
						QPoint oc;
						int oq;
						Line line;
						QuarterEdge(cell, q, k, oc, oq, line);
						if(m_Plan.Quarter(m_Deck - 1, oc, oq) == '.')
							p.drawLine(LatticePx(corners[k]), LatticePx(corners[(k + 1) % 3]));
					}
				}
	}

	for(int r = 0; r < m_Plan.Rows(); r++)
		for(int c = 0; c < m_Plan.Cols(); c++)
			for(int q = 0; q < 4; q++)
			{
				QPoint cell(c, r);
				QChar ch = m_Plan.Quarter(m_Deck, cell, q);
				if(ch == '.')
					continue;
				QPoint corners[3];
				QuarterCornerPoints(cell, q, corners);
				for(int k = 0; k < 3; k++)
				{
					QPoint oc;
					int oq;
					Line line;
					QuarterEdge(cell, q, k, oc, oq, line);
					QChar other = m_Plan.Quarter(m_Deck, oc, oq);
					if(other == ch || (other != '.' && other < ch))
						continue;
					QPointF a = LatticePx(corners[k]), b = LatticePx(corners[(k + 1) % 3]);
					if(SimpleMode::IsOpenEdge(m_Plan, ch, other))
					{
						if(!SimpleMode::HasOpenAirRailing(m_Plan, m_Deck, ch))
						{
							p.setPen(QPen(QColor(0xb8, 0xa6, 0x8c), 1.5, Qt::DashLine, Qt::FlatCap));
							p.drawLine(a, b);
							continue;
						}
						QColor color = m_Plan.rooms[ch].type == "catwalk" ? QColor(0x9c, 0xa4, 0xb0) : QColor(0xd8, 0xb0, 0x78);
						QPointF u = (b - a) / qMax(1e-6, std::hypot((b - a).x(), (b - a).y())), n(-u.y(), u.x());
						p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::FlatCap));
						p.drawLine(a, b);
						p.setPen(QPen(color, 1));
						double len = std::hypot((b - a).x(), (b - a).y());
						int ticks = qMax(2, int(len / 5));
						for(int t = 0; t <= ticks; t++)
						{
							QPointF m = a + (b - a) * (double(t) / ticks);
							p.drawLine(m - n * 3, m + n * 3);
						}
						continue;
					}
					p.setPen(QPen(QColor(0xd6, 0xda, 0xe0), other == '.' ? 5 : 3, Qt::SolidLine, Qt::RoundCap));
					p.drawLine(a, b);
				}
			}

	for(int r = 0; r < m_Plan.Rows(); r++)
		for(int c = 0; c < m_Plan.Cols(); c++)
		{
			QPoint cell(c, r);
			QChar letter = m_Plan.RoofAt(m_Deck, cell);
			if(letter == '.' || !m_Plan.roofs.contains(letter))
				continue;
			const Roof& roof = m_Plan.roofs[letter];
			QRectF rc(Margin + c * CellPx, Margin + r * CellPx, CellPx, CellPx);
			QPointF mid = rc.center();
			const double h = CellPx / 2.0 - 5;
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(0xd0, 0x80, 0x50, 55));
			p.drawRect(rc);
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(QColor(0xf0, 0xb0, 0x80, 200), 1.3));
			bool northSouth = roof.dir == North || roof.dir == South;
			if(roof.style == "gable" || roof.style == "gambrel" || roof.style == "round")
			{
				QPointF along = northSouth ? QPointF(0, CellPx / 2.0) : QPointF(CellPx / 2.0, 0);
				p.setPen(QPen(QColor(0xf0, 0xb0, 0x80, 200), roof.style == "round" ? 3 : 1.6));
				p.drawLine(mid - along, mid + along);
				if(roof.style == "gambrel")
				{
					QPointF across = northSouth ? QPointF(h / 2, 0) : QPointF(0, h / 2);
					p.setPen(QPen(QColor(0xf0, 0xb0, 0x80, 120), 1));
					p.drawLine(mid - along + across, mid + along + across);
					p.drawLine(mid - along - across, mid + along - across);
				}
			}
			else if(roof.style == "hip" || roof.style == "mansard" || roof.style == "spire")
			{
				p.drawLine(rc.topLeft() + QPointF(5, 5), rc.bottomRight() - QPointF(5, 5));
				p.drawLine(rc.topRight() + QPointF(-5, 5), rc.bottomLeft() + QPointF(5, -5));
				if(roof.style == "mansard")
					p.drawRect(rc.adjusted(CellPx / 4.0, CellPx / 4.0, -CellPx / 4.0, -CellPx / 4.0));
				if(roof.style == "spire")
				{
					p.setBrush(QColor(0xf0, 0xb0, 0x80));
					p.drawEllipse(mid, 2.5, 2.5);
					p.setBrush(Qt::NoBrush);
				}
			}
			else if(roof.style == "shed")
			{
				static const QPointF downs[4] = {QPointF(0, -1), QPointF(1, 0), QPointF(0, 1), QPointF(-1, 0)};
				QPointF down = downs[roof.dir] * h * 0.8, side(-down.y() * 0.3, down.x() * 0.3);
				p.drawLine(mid - down, mid + down);
				p.drawLine(mid + down, mid + down * 0.6 + side);
				p.drawLine(mid + down, mid + down * 0.6 - side);
			}
			else if(roof.style == "dome")
				p.drawEllipse(mid, h * 0.8, h * 0.8);
			else if(roof.style == "flat")
				p.drawRect(rc.adjusted(5, 5, -5, -5));
			bool picked = letter == m_RoofSelection;
			p.setPen(QPen(picked ? QColor(0x5a, 0xd8, 0xff, 220) : QColor(0xf0, 0xb0, 0x80, 150), picked ? 2.5 : 1.2, Qt::DashLine));
			static const QPointF corner[4][2] = {{{0, 0}, {1, 0}}, {{1, 0}, {1, 1}}, {{0, 1}, {1, 1}}, {{0, 0}, {0, 1}}};
			for(int s = 0; s < 4; s++)
				if(m_Plan.RoofAt(m_Deck, Neighbour(cell, Side(s))) != letter)
					p.drawLine(rc.topLeft() + corner[s][0] * CellPx, rc.topLeft() + corner[s][1] * CellPx);
		}

	for(int i = 0; i < m_Plan.dormers.size(); i++)
	{
		const Dormer& d = m_Plan.dormers[i];
		if(d.deck != m_Deck)
			continue;
		bool ok = DormerProblem(m_Plan, d, i).isEmpty(), picked = m_HasDormerSelection && SelectedDormer() == i;
		QColor color = !ok ? QColor(0xff, 0x55, 0x55) : picked ? QColor(0x5a, 0xd8, 0xff) : QColor(0xf0, 0xe0, 0xc0);
		QRectF rc(Margin + d.cell.x() * CellPx, Margin + d.cell.y() * CellPx, CellPx, CellPx);
		static const QPointF outs[4] = {QPointF(0, -1), QPointF(1, 0), QPointF(0, 1), QPointF(-1, 0)};
		QPointF out = outs[((d.side % 4) + 4) % 4], along(-out.y(), out.x()), f = rc.center() + out * (CellPx / 2.0 - 7);
		QPolygonF shape;
		if(d.style == "shed" || d.style == "box")
			shape << f - along * 9 << f + along * 9 << f + along * 9 - out * 15 << f - along * 9 - out * 15;
		else
			shape << f - along * 8 << f + along * 8 << f + along * 8 - out * 9 << f - out * 17 << f - along * 8 - out * 9;
		p.setPen(QPen(color, picked ? 2.2 : 1.4));
		p.setBrush(QColor(color.red(), color.green(), color.blue(), 90));
		p.drawPolygon(shape);
		p.setPen(QPen(QColor(0x9a, 0xd8, 0xff), 2.5, Qt::SolidLine, Qt::FlatCap));
		p.drawLine(f - along * 4, f + along * 4);
		p.setBrush(Qt::NoBrush);
	}
	for(int i = 0; i < m_Plan.skylights.size(); i++)
	{
		const Skylight& s = m_Plan.skylights[i];
		if(s.deck != m_Deck)
			continue;
		bool ok = SkylightProblem(m_Plan, s, i).isEmpty(), picked = m_HasSkylightSelection && SelectedSkylight() == i;
		QColor color = !ok ? QColor(0xff, 0x55, 0x55) : picked ? QColor(0x5a, 0xd8, 0xff) : QColor(0xb8, 0xe0, 0xf0);
		QRectF rc(Margin + s.cell.x() * CellPx, Margin + s.cell.y() * CellPx, CellPx, CellPx);
		QRectF pane = rc.adjusted(CellPx / 4.0, CellPx / 4.0, -CellPx / 4.0, -CellPx / 4.0);
		p.setPen(QPen(color, picked ? 2.2 : 1.4));
		p.setBrush(QColor(0x9a, 0xd8, 0xff, 70));
		p.drawRect(pane);
		p.setBrush(Qt::NoBrush);
		if(s.style == "lantern")
		{
			p.drawLine(pane.topLeft(), pane.bottomRight());
			p.drawLine(pane.topRight(), pane.bottomLeft());
		}
	}

	for(int i = 0; i < m_Plan.stairs.size(); i++)
	{
		const Stair& st = m_Plan.stairs[i];
		if(st.deck != m_Deck)
			continue;
		StairLayout layout = LayoutStair(m_Plan, st);
		bool picked = m_HasStairSelection && SelectedStair() == i;
		QColor color = !layout.ok ? QColor(0xff, 0x55, 0x55) : picked ? QColor(0x5a, 0xd8, 0xff) : QColor(0xe8, 0xd0, 0xa0);
		auto cellRect = [&](const QPoint& c) { return QRectF(Margin + c.x() * CellPx, Margin + c.y() * CellPx, CellPx, CellPx); };
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(color.red(), color.green(), color.blue(), 45));
		for(const QPoint& c : layout.cells)
			p.drawRect(cellRect(c));
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(color, picked ? 2.5 : 1.5));
		for(const QPoint& c : layout.landings)
			p.drawRect(cellRect(c).adjusted(8, 8, -8, -8));
		static const QPointF steps[4] = {QPointF(0, -1), QPointF(1, 0), QPointF(0, 1), QPointF(-1, 0)};
		for(const StairLayout::Flight& f : layout.flights)
		{
			QPointF dir = steps[f.dir], side(-dir.y(), dir.x());
			QPointF a = cellRect(f.from).center() - dir * (CellPx / 2.0 - 4), b = cellRect(f.to).center() + dir * (CellPx / 2.0 - 4);
			int count = qRound(QLineF(a, b).length() / 6);
			p.setPen(QPen(color, 1));
			for(int k = 1; k < count; k++)
			{
				QPointF t = a + (b - a) * (double(k) / count);
				p.drawLine(t - side * 9, t + side * 9);
			}
			p.setPen(QPen(color, picked ? 2.5 : 1.8));
			p.drawLine(a, b);
			p.drawLine(b, b - dir * 7 + side * 5);
			p.drawLine(b, b - dir * 7 - side * 5);
		}
	}

	auto drawDoor = [&](const Door& d, const QColor& leaf, bool gap) {
		DoorPlace dp = SimpleMode::PlaceDoor(m_Plan, d);
		QPointF mid(Margin + (dp.ok ? (dp.mx - m_Plan.X0(0)) * pxPerMetre : d.at.x() * CellPx / 4.0),
			Margin + (dp.ok ? (m_Plan.Y1(0) - dp.my) * pxPerMetre : d.at.y() * CellPx / 4.0));
		static const double dirs[4][2] = {{1, 0}, {0, -1}, {M_SQRT1_2, -M_SQRT1_2}, {M_SQRT1_2, M_SQRT1_2}};
		QPointF u(dirs[d.line][0], dirs[d.line][1]);
		const double half = DoorWidth(m_Plan, d) / 2;
		QPointF a = mid - u * half * pxPerMetre, b = mid + u * half * pxPerMetre;
		if(gap)
		{
			p.setPen(QPen(QColor(0x0e, 0x12, 0x17), 7, Qt::SolidLine, Qt::FlatCap));
			p.drawLine(a, b);
		}
		if(gap && m_HasSelection && FindDoor(d) == SelectedDoor())
		{
			p.setPen(QPen(QColor(0x5a, 0xd8, 0xff, 150), 11, Qt::SolidLine, Qt::RoundCap));
			p.drawLine(a, b);
		}
		if(d.opening)
		{
			QPointF n(dp.ok ? dp.nx : 0, dp.ok ? -dp.ny : 1);
			p.setPen(QPen(leaf, 2.5, Qt::SolidLine, Qt::FlatCap));
			p.drawLine(a - n * 5, a + n * 5);
			p.drawLine(b - n * 5, b + n * 5);
			p.setPen(QPen(leaf, 1.2, Qt::DashLine));
			if(d.style == "arch")
			{
				QPainterPath arc;
				arc.moveTo(a);
				for(int k = 1; k <= 12; k++)
				{
					double t = M_PI * k / 12;
					arc.lineTo(mid - u * (half * pxPerMetre * qCos(t)) + n * (half * pxPerMetre * 0.5 * qSin(t)));
				}
				p.drawPath(arc);
			}
			else
				p.drawLine(a, b);
			return;
		}
		p.setPen(QPen(leaf, 3, Qt::SolidLine, Qt::FlatCap));
		p.drawLine(a, b);
		if(!gap || !dp.ok)
			return;
		QString style = d.style.isEmpty() ? DefaultDoorStyle(m_Plan.theme, dp.there == '.', d.wide) : d.style;
		QPointF n(dp.nx, -dp.ny), L(-dp.ny, -dp.nx);
		double w = half * 2 * pxPerMetre;
		QPen thin(QColor(0xf0, 0x9a, 0x3c, 190), 1.4);
		p.setPen(thin);
		p.setBrush(Qt::NoBrush);
		auto arrow = [&](const QPointF& from, const QPointF& dir, double len) {
			QPointF tip = from + dir * len, side(-dir.y(), dir.x());
			p.drawLine(from, tip);
			p.drawLine(tip, tip - dir * 5 + side * 3);
			p.drawLine(tip, tip - dir * 5 - side * 3);
		};
		if(style.startsWith("double-swing"))
		{
			QPointF swing = style.endsWith("-in") ? -n : n;
			for(QPointF hinge : {L, -L})
			{
				QPointF pivot = mid + hinge * (w / 2);
				p.drawLine(pivot, pivot + swing * (w / 2));
				QPainterPath arc;
				arc.moveTo(mid);
				for(int k = 1; k <= 10; k++)
				{
					double t = k / 10.0 * M_PI / 2;
					arc.lineTo(pivot - hinge * (w / 2 * qCos(t)) + swing * (w / 2 * qSin(t)));
				}
				p.drawPath(arc);
			}
		}
		else if(style == "slide-up" || style == "slide-down")
		{
			p.drawLine(a + n * 3, b + n * 3);
			p.drawLine(a - n * 3, b - n * 3);
			p.setFont(small);
			p.drawText(QRectF(mid.x() - 8 - n.x() * 11, mid.y() - 8 - n.y() * 11, 16, 16), Qt::AlignCenter,
				style == "slide-up" ? QString(QChar(0x2191)) : QString(QChar(0x2193)));
		}
		else if(style.startsWith("swing"))
		{
			QPointF hinge = style.endsWith("left") ? L : -L, swing = style.contains("-in-") ? -n : n;
			QPointF pivot = mid + hinge * (w / 2), latch = mid - hinge * (w / 2);
			p.drawLine(pivot, pivot + swing * w);
			QPainterPath arc;
			arc.moveTo(latch);
			for(int k = 1; k <= 12; k++)
			{
				double t = k / 12.0 * M_PI / 2;
				arc.lineTo(pivot - hinge * (w * qCos(t)) + swing * (w * qSin(t)));
			}
			p.drawPath(arc);
		}
		else if(style.startsWith("slide"))
			arrow(mid, style.endsWith("left") ? L : -L, w * 0.9);
		else if(style == "split-v")
		{
			arrow(mid + L * 3, L, w * 0.45);
			arrow(mid - L * 3, -L, w * 0.45);
		}
		else if(style == "split-h")
		{
			arrow(mid + n * 2, n, 6);
			arrow(mid - n * 2, -n, 6);
		}
		else if(style == "portcullis")
			for(int k = -3; k <= 3; k++)
				p.drawLine(mid + L * (k * w / 7) - n * 4, mid + L * (k * w / 7) + n * 4);
		if(d.operation == "console" && d.console != QPoint(-1, -1))
		{
			QPointF c(Margin + d.console.x() * CellPx / 4.0, Margin + d.console.y() * CellPx / 4.0);
			p.setPen(QPen(QColor(0x7f, 0xb0, 0xff, 170), 1.2, Qt::DashLine));
			p.drawLine(mid, c);
			p.setPen(QPen(QColor(0x7f, 0xb0, 0xff), 1.5));
			p.setBrush(QColor(0x2a, 0x5a, 0x9a));
			p.drawRect(QRectF(c.x() - 5, c.y() - 4, 10, 8));
			p.setBrush(Qt::NoBrush);
		}
		QString flags = QString(d.automatic ? "A" : "") + (d.operation == "door" ? "C" : "") + (d.locked ? "L" : "")
			+ (d.vehicles ? "V" : "");
		if(!flags.isEmpty())
		{
			p.setPen(leaf);
			p.setFont(small);
			p.drawText(QRectF(mid.x() - 16 + n.x() * 10, mid.y() - 8 + n.y() * 10, 32, 16), Qt::AlignCenter, flags);
		}
	};
	for(const Door& d : m_Plan.doors)
		if(d.deck == m_Deck)
			drawDoor(d, QColor(0xf0, 0x9a, 0x3c), true);

	const QColor glassColor(0x8f, 0xd4, 0xff);
	auto drawWindow = [&](const Window& w, const QColor& color, bool placed) {
		DoorPlace dp = SimpleMode::PlaceWindow(m_Plan, w);
		QPointF mid(Margin + (dp.ok ? (dp.mx - m_Plan.X0(0)) * pxPerMetre : w.at.x() * CellPx / 4.0),
			Margin + (dp.ok ? (m_Plan.Y1(0) - dp.my) * pxPerMetre : w.at.y() * CellPx / 4.0));
		static const double dirs[4][2] = {{1, 0}, {0, -1}, {M_SQRT1_2, -M_SQRT1_2}, {M_SQRT1_2, M_SQRT1_2}};
		QPointF u(dirs[w.line][0], dirs[w.line][1]), n(-u.y(), u.x());
		const double half = WindowWidth(m_Plan, w) / 2 * pxPerMetre;
		QPointF a = mid - u * half, b = mid + u * half;
		if(placed)
		{
			p.setPen(QPen(QColor(0x0e, 0x12, 0x17), 7, Qt::SolidLine, Qt::FlatCap));
			p.drawLine(a, b);
			if(m_HasWindowSelection && FindWindow(w) == SelectedWindow())
			{
				p.setPen(QPen(QColor(0x5a, 0xd8, 0xff, 150), 11, Qt::SolidLine, Qt::RoundCap));
				p.drawLine(a, b);
			}
		}
		p.setPen(QPen(color, 1.2, Qt::SolidLine, Qt::FlatCap));
		p.drawLine(a + n * 3, b + n * 3);
		p.drawLine(a - n * 3, b - n * 3);
		p.drawLine(a + n * 3, a - n * 3);
		p.drawLine(b + n * 3, b - n * 3);
		p.setPen(QPen(color, 2, Qt::SolidLine, Qt::FlatCap));
		p.drawLine(a, b);
		if(placed && w.tech && w.tech != DefaultWindowTech(m_Plan.theme) && dp.ok)
		{
			QPointF out = QPointF(dp.nx, -dp.ny) * -10;
			p.setFont(small);
			p.drawText(QRectF(mid.x() - 8 + out.x(), mid.y() - 8 + out.y(), 16, 16), Qt::AlignCenter, QString::number(w.tech));
		}
	};
	for(const Window& w : m_Plan.windows)
		if(w.deck == m_Deck)
			drawWindow(w, glassColor, true);

	auto drawRailing = [&](const Railing& r, const QColor& color, bool placed) {
		static const double dirs[4][2] = {{1, 0}, {0, -1}, {M_SQRT1_2, -M_SQRT1_2}, {M_SQRT1_2, M_SQRT1_2}};
		QPointF mid(Margin + r.at.x() * CellPx / 4.0, Margin + r.at.y() * CellPx / 4.0);
		QPointF u(dirs[r.line][0], dirs[r.line][1]), n(-u.y(), u.x());
		const double half = RailingLength(m_Plan, r) / 2 * pxPerMetre;
		QPointF a = mid - u * half, b = mid + u * half;
		if(placed)
		{
			p.setPen(QPen(QColor(0x0e, 0x12, 0x17), 7, Qt::SolidLine, Qt::FlatCap));
			p.drawLine(a, b);
			if(m_HasRailingSelection && FindRailing(r) == SelectedRailing())
			{
				p.setPen(QPen(QColor(0x5a, 0xd8, 0xff, 150), 11, Qt::SolidLine, Qt::RoundCap));
				p.drawLine(a, b);
			}
		}
		p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::FlatCap));
		p.drawLine(a, b);
		p.setPen(QPen(color, 1));
		int ticks = qMax(2, int(half * 2 / 5));
		for(int k = 0; k <= ticks; k++)
		{
			QPointF t = a + (b - a) * (double(k) / ticks);
			p.drawLine(t - n * 3, t + n * 3);
		}
	};
	for(const Railing& r : m_Plan.railings)
		if(r.deck == m_Deck)
			drawRailing(r, QColor(0xd8, 0xb0, 0x78), true);
	if(m_HoverRailingValid)
	{
		bool exists = FindRailing(m_HoverRailing) >= 0;
		bool fits = SimpleMode::PlaceRailing(m_Plan, m_HoverRailing).ok;
		drawRailing(m_HoverRailing, exists ? QColor(0xff, 0xc0, 0x40) : fits ? QColor(0x4c, 0xd0, 0x6a) : QColor(0xff, 0x55, 0x55), false);
	}

	auto drawGap = [&](const RailingGap& g, const QColor& color, bool placed) {
		DoorPlace gp = SimpleMode::PlaceGap(m_Plan, g);
		QPointF mid(Margin + g.at.x() * CellPx / 4.0, Margin + g.at.y() * CellPx / 4.0);
		QPointF u(gp.ux, -gp.uy), n(-u.y(), u.x());
		if(!gp.ok)
		{
			static const double dirs[4][2] = {{1, 0}, {0, -1}, {M_SQRT1_2, -M_SQRT1_2}, {M_SQRT1_2, M_SQRT1_2}};
			u = QPointF(dirs[g.line][0], dirs[g.line][1]);
			n = QPointF(-u.y(), u.x());
		}
		const double half = GapWidth(m_Plan, g) / 2 * pxPerMetre;
		QPointF a = mid - u * half, b = mid + u * half;
		if(placed)
		{
			p.setPen(QPen(QColor(0x0e, 0x12, 0x17), 7, Qt::SolidLine, Qt::FlatCap));
			p.drawLine(a, b);
		}
		else
		{
			p.setPen(QPen(color, 2, Qt::DashLine, Qt::FlatCap));
			p.drawLine(a, b);
		}
		p.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::FlatCap));
		p.drawLine(a - n * 4, a + n * 4);
		p.drawLine(b - n * 4, b + n * 4);
	};
	for(const RailingGap& g : m_Plan.gaps)
		if(g.deck == m_Deck)
			drawGap(g, SimpleMode::PlaceGap(m_Plan, g).ok ? QColor(0xd8, 0xb0, 0x78) : QColor(0xff, 0x55, 0x55), true);
	if(m_HoverGapValid)
		drawGap(m_HoverGap, SimpleMode::PlaceGap(m_Plan, m_HoverGap).ok ? QColor(0x4c, 0xd0, 0x6a) : QColor(0xff, 0x55, 0x55), false);

	for(int r = 0; r < m_Plan.Rows(); r++)
		for(int c = 0; c < m_Plan.Cols(); c++)
		{
			const QPoint cell(c, r);
			const QChar ch = m_Plan.At(m_Deck, cell);
			if(!IsLiftRoom(m_Plan, ch))
				continue;
			const QPointF mid(Margin + (c + 0.5) * CellPx, Margin + (r + 0.5) * CellPx);
			const double a = CellPx * 0.12;
			p.setPen(Qt::NoPen);
			for(int dir : {-1, 1})
			{
				const bool goes = m_Plan.At(m_Deck - dir, cell) == ch;
				p.setBrush(goes ? QColor(0x9a, 0xd8, 0xff) : QColor(0x9a, 0xd8, 0xff, 70));
				const QPointF tip = mid + QPointF(0, dir * CellPx * 0.3), base = mid + QPointF(0, dir * CellPx * 0.12);
				p.drawPolygon(QPolygonF() << tip << base + QPointF(-a, 0) << base + QPointF(a, 0));
			}
		}

	for(int hi = 0; hi < m_Plan.hatches.size(); hi++)
	{
		const Hatch& h = m_Plan.hatches[hi];
		if(h.roof ? h.deck != m_Deck : h.deck != m_Deck && h.deck != m_Deck + 1)
			continue;
		QRectF hr = QRectF(Margin + h.cell.x() * CellPx, Margin + h.cell.y() * CellPx, CellPx, CellPx)
			.adjusted(CellPx * 0.3, CellPx * 0.3, -CellPx * 0.3, -CellPx * 0.3);
		const QColor hc = h.roof ? (RoofHatchProblem(m_Plan, h, hi).isEmpty() ? QColor(0x6a, 0xb8, 0xff) : QColor(0xff, 0x55, 0x55))
			: QColor(0xf0, 0x9a, 0x3c);
		if(h.deck == m_Deck && m_HasHatchSelection && h.deck == m_HatchDeck && h.cell == m_HatchCell && h.roof == m_HatchRoof)
		{
			p.setPen(QPen(QColor(0x5a, 0xd8, 0xff, 150), 6));
			p.drawRect(hr.adjusted(-3, -3, 3, 3));
		}
		if(h.deck == m_Deck)
		{
			p.setPen(QPen(hc, 2));
			p.setBrush(QColor(hc.red(), hc.green(), hc.blue(), 50));
			p.drawRect(hr);
			p.setBrush(Qt::NoBrush);
			QPointF c = hr.center(), fwd, side;
			switch(h.side)
			{
			case North: fwd = QPointF(0, -1); break;
			case East: fwd = QPointF(1, 0); break;
			case South: fwd = QPointF(0, 1); break;
			default: fwd = QPointF(-1, 0); break;
			}
			side = QPointF(-fwd.y(), fwd.x());
			double half = hr.width() / 2, across = hr.width() * 0.25;
			QPointF outer = c + fwd * half, inner = c + fwd * (half - across * 0.5);
			p.setPen(QPen(hc, 2.5));
			p.drawLine(outer - side * across, outer + side * across);
			p.setPen(QPen(hc, 1.5));
			p.drawLine(inner - side * across, inner + side * across);
			for(int k = -2; k <= 2; k++)
				p.drawLine(outer + side * (k * across / 2.5), inner + side * (k * across / 2.5));
		}
		else
		{
			p.setPen(QPen(hc, 2, Qt::DashLine));
			p.drawRect(hr);
		}
	}

	for(int fi = 0; fi < m_Plan.fixtures.size(); fi++)
	{
		const Fixture& f = m_Plan.fixtures[fi];
		if(f.deck != m_Deck)
			continue;
		static const QPointF faces[4] = {QPointF(0, -1), QPointF(1, 0), QPointF(0, 1), QPointF(-1, 0)};
		const QPointF mid(Margin + (f.cell.x() + 0.5) * CellPx, Margin + (f.cell.y() + 0.5) * CellPx);
		const QPointF fw = faces[((f.dir % 4) + 4) % 4], side(-fw.y(), fw.x());
		const bool ok = FixtureProblem(m_Plan, f, fi).isEmpty();
		const bool picked = m_HasFixtureSelection && fi == SelectedFixture();
		const QColor ink = !ok ? QColor(0xff, 0x55, 0x55) : picked ? QColor(0x5a, 0xd8, 0xff) : QColor(0xe0, 0xe4, 0xea);
		p.setPen(QPen(ink, picked ? 2 : 1.2));
		p.setBrush(Qt::NoBrush);
		if(f.type == "berth")
		{
			QPolygonF bed;
			const double a = pxPerMetre, w = 0.5 * a, l = 1.0 * a;
			bed << mid - side * w - fw * l << mid + side * w - fw * l << mid + side * w + fw * l << mid - side * w + fw * l;
			p.setBrush(QColor(0x80, 0x90, 0xb0, 160));
			p.drawPolygon(bed);
			p.setBrush(QColor(0xe8, 0xe8, 0xe0));
			p.drawRect(QRectF(mid - fw * (l * 0.8) - side * (w * 0.7) - QPointF(2, 2), QSizeF(4, 4)) .united(
				QRectF(mid - fw * (l * 0.8) + side * (w * 0.7) - QPointF(2, 2), QSizeF(4, 4))));
		}
		else if(f.type == "parking")
		{
			const FixtureType* t = FindFixtureType(f.type);
			const QString kind = f.kind.isEmpty() ? t->kinds.first() : f.kind;
			QPointF c = mid;
			double x, y;
			if(kind != "spacecraft" && ParkingCentre(m_Plan, f, x, y))
				c = QPointF(Margin + (x - m_Plan.X0(0)) * pxPerMetre, Margin + (m_Plan.Y1(0) - y) * pxPerMetre);
			double across = 20, along = 20;
			if(kind != "spacecraft")
			{
				ParkingSize(kind, across, along);
				across *= pxPerMetre;
				along *= pxPerMetre;
			}
			QPolygonF spot;
			spot << c - side * (across / 2) - fw * (along / 2) << c + side * (across / 2) - fw * (along / 2)
				<< c + side * (across / 2) + fw * (along / 2) << c - side * (across / 2) + fw * (along / 2);
			p.setPen(QPen(ink, 1.5, Qt::DashLine));
			p.drawPolygon(spot);
			p.drawLine(c, c + fw * (along / 2 - 4));
			p.drawText(QRectF(c.x() - 8, c.y() - 8, 16, 16), Qt::AlignCenter, "P");
		}
		else if(f.type == "gametable")
		{
			QPointF c = mid;
			double x, y;
			if(ParkingCentre(m_Plan, f, x, y))
				c = QPointF(Margin + (x - m_Plan.X0(0)) * pxPerMetre, Margin + (m_Plan.Y1(0) - y) * pxPerMetre);
			const double t = 0.6 * pxPerMetre, s = 1.22 * pxPerMetre;
			p.setBrush(QColor(0x2e, 0x7d, 0x4a, 200));
			p.drawRect(QRectF(c.x() - t, c.y() - t, 2 * t, 2 * t));
			p.setBrush(QColor(0xe0, 0xe4, 0xea, 160));
			for(double sx : {-1.0, 1.0})
				for(double sy : {-1.0, 1.0})
					p.drawEllipse(c + QPointF(sx * s, sy * s), 3.5, 3.5);
		}
		else if(f.type == "turret")
		{
			const QPointF at = mid + QPointF(CellPx / 2.0, CellPx / 2.0);
			const double r = 0.8 * CellPx;
			p.setBrush(QColor(0x5a, 0x60, 0x68, 200));
			p.drawEllipse(at, r, r);
			p.setPen(QPen(ink, 4));
			p.drawLine(at, at + fw * (1.3 * CellPx));
			p.setPen(QPen(ink, 1));
			p.drawText(QRectF(at.x() - r, at.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString("T%1").arg(TurretNumber(m_Plan, fi)));
		}
		else if(f.type == "light")
		{
			QPointF at = f.dir == FixtureTop ? mid : mid + fw * (CellPx / 2.0 - 5);
			static const QMap<QString, QColor> colors = {{"red", QColor(0xff, 0x30, 0x20)}, {"green", QColor(0x30, 0xff, 0x50)},
				{"white", QColor(0xff, 0xff, 0xf0)}, {"amber", QColor(0xff, 0xa0, 0x20)}, {"blue", QColor(0x40, 0x80, 0xff)}};
			QString c = !f.color.isEmpty() ? f.color : f.kind == "beacon" ? "amber" : f.kind == "strobe" ? "white" : "red";
			p.setBrush(colors.value(c));
			p.drawEllipse(at, f.kind == "beacon" ? 5.0 : 3.5, f.kind == "beacon" ? 5.0 : 3.5);
		}
		else
		{
			p.drawEllipse(mid, 5, 5);
			p.drawEllipse(mid, 1.5, 1.5);
			if(f.type == "flagpole")
			{
				p.setBrush(QColor(0xc0, 0x30, 0x30));
				QPolygonF flag;
				flag << mid + fw * 5 + side * 3 << mid + fw * 16 << mid + fw * 5 - side * 3;
				p.drawPolygon(flag);
			}
			else
			{
				p.setBrush(QColor(0xff, 0xa0, 0x20));
				p.drawEllipse(mid, 2.5, 2.5);
			}
		}
		p.setBrush(Qt::NoBrush);
	}

	const QList<int> gunnerLinks = GunnerLinks(m_Plan);
	for(int si = 0; si < m_Plan.stations.size(); si++)
	{
		const Station& st = m_Plan.stations[si];
		if(st.deck != m_Deck)
			continue;
		static const QPointF faces[4] = {QPointF(0, -1), QPointF(1, 0), QPointF(0, 1), QPointF(-1, 0)};
		QPointF mid(Margin + (st.cell.x() + 0.5) * CellPx, Margin + (st.cell.y() + 0.5) * CellPx);
		QPointF f = faces[((st.dir % 4) + 4) % 4], side(-f.y(), f.x());
		const bool ok = StationProblem(m_Plan, st, si).isEmpty();
		const QColor body = ok ? QColor(0x5a, 0xd8, 0xff) : QColor(0xff, 0x55, 0x55), seat = ok ? QColor(0x3a, 0x8a, 0xb0) : QColor(0xa0, 0x30, 0x30);
		if(m_HasStationSelection && si == SelectedStation())
		{
			p.setPen(QPen(QColor(0x5a, 0xd8, 0xff, 150), 2.5));
			p.setBrush(Qt::NoBrush);
			p.drawRect(QRectF(mid.x() - CellPx / 2.0 + 3, mid.y() - CellPx / 2.0 + 3, CellPx - 6, CellPx - 6));
		}
		p.setPen(QPen(QColor(0x20, 0x24, 0x2a), 1));
		QPointF chair;
		if(!st.wall)
		{
			QPolygonF desk;
			desk << mid + f * 7 + side * 10 << mid + f * 13 + side * 10 << mid + f * 13 - side * 10 << mid + f * 7 - side * 10;
			p.setBrush(body);
			p.drawPolygon(desk);
			chair = mid - f * 2;
		}
		else
		{
			QPolygonF screen;
			const double w = CellPx / 2.0 - 3;
			screen << mid + f * (w - 3) + side * 10 << mid + f * w + side * 10 << mid + f * w - side * 10 << mid + f * (w - 3) - side * 10;
			p.setBrush(body);
			p.drawPolygon(screen);
			chair = mid + f * (w - 12);
		}
		p.setBrush(seat);
		p.drawEllipse(chair, 4.5, 4.5);
		static const QMap<QString, QString> letters = {{"command", "C"}, {"firecontrol", "F"}, {"medic", "M"}, {"navigator", "N"},
			{"sensor", "S"}, {"shield", "H"}, {"transporter", "T"}};
		p.setPen(body);
		const QString letter = st.kind == "gunner" ? QString("G%1").arg(TurretNumber(m_Plan, gunnerLinks.value(si, -1)))
			: letters.value(st.kind, "?");
		p.drawText(QRectF(chair.x() + side.x() * 13 - 9, chair.y() + side.y() * 13 - 7, 18, 14), Qt::AlignCenter, letter);
		p.setBrush(Qt::NoBrush);
	}

	for(const Post& post : m_Plan.posts)
	{
		if(post.deck != m_Deck)
			continue;
		QPointF mid = QPointF(Margin + (post.cell.x() + 0.75) * CellPx, Margin + (post.cell.y() + 0.75) * CellPx);
		p.drawPixmap(QRectF(mid.x() - 10, mid.y() - 10, 20, 20).toRect(),
			PaletteIcon(post.kind == "troop" ? "ScTroop" : "ScCitizen").pixmap(20, 20));
	}

	QMap<QChar, QPointF> anchors;
	for(int r = 0; r < m_Plan.Rows(); r++)
		for(int c = 0; c < m_Plan.Cols(); c++)
		{
			QPoint cell(c, r);
			QChar w = m_Plan.At(m_Deck, cell);
			if(w != '.' && w != '*' && !anchors.contains(w))
				anchors[w] = QPointF(Margin + (c + 0.5) * CellPx, Margin + (r + 0.5) * CellPx);
		}
	for(int r = 0; r < m_Plan.Rows(); r++)
		for(int c = 0; c < m_Plan.Cols(); c++)
			for(int q = 0; q < 4; q++)
			{
				QChar ch = m_Plan.Quarter(m_Deck, QPoint(c, r), q);
				if(ch != '.' && !anchors.contains(ch))
				{
					QPolygonF tri = triangle(QPoint(c, r), q);
					anchors[ch] = (tri[0] + tri[1] + tri[2]) / 3;
				}
			}
	for(auto it = anchors.begin(); it != anchors.end(); ++it)
	{
		if(!m_Plan.rooms.contains(it.key()))
			continue;
		const Room& room = m_Plan.rooms[it.key()];
		QPointF at = it.value();
		QRectF box(at.x() - 11, at.y() - 11, 22, 22);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 120));
		p.drawRoundedRect(box, 4, 4);
		p.drawPixmap(box.adjusted(3, 3, -3, -3).toRect(), PaletteIcon(RoomKindIcon(room.type)).pixmap(16, 16));
		bool several = false;
		for(const Room& o : m_Plan.rooms)
			several = several || (o.letter != room.letter && o.type == room.type);
		if(several)
		{
			p.setPen(QColor(0x9c, 0xc4, 0xff));
			p.setFont(small);
			p.drawText(QRectF(box.right() - 2, box.bottom() - 12, 16, 14), Qt::AlignLeft | Qt::AlignVCenter, QString::number(room.tag));
		}
		p.setBrush(Qt::NoBrush);
	}

	if(m_HoverDoorValid)
	{
		bool exists = FindDoor(m_HoverDoor) >= 0;
		QString problem;
		bool fits = DoorKindFits(SimpleMode::PlaceDoor(m_Plan, m_HoverDoor), problem);
		drawDoor(m_HoverDoor, exists ? QColor(0xff, 0xc0, 0x40) : fits ? QColor(0x4c, 0xd0, 0x6a) : QColor(0xff, 0x55, 0x55), false);
	}
	else if(m_HoverWindowValid)
	{
		bool exists = FindWindow(m_HoverWindow) >= 0;
		QString problem;
		bool fits = SimpleMode::PlaceWindow(m_Plan, m_HoverWindow).ok && WindowClear(m_Plan, m_HoverWindow, problem);
		drawWindow(m_HoverWindow, exists ? QColor(0xff, 0xc0, 0x40) : fits ? QColor(0x4c, 0xd0, 0x6a) : QColor(0xff, 0x55, 0x55), false);
	}
	else if(m_HoverValid && (m_Tool == PaintRoom || m_Tool == Erase))
	{
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0x5a, 0x9b, 0xff, 70));
		for(const QPair<QPoint, int>& s : ShapeAt(m_Plan, m_HoverCell, m_HoverQuarter, m_Lines))
			p.drawPolygon(triangle(s.first, s.second));
		p.setBrush(Qt::NoBrush);
	}
	else if(m_HoverValid && m_Tool == DeleteRoom)
	{
		QChar ch = m_Plan.Quarter(m_Deck, m_HoverCell, m_HoverQuarter);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0xff, 0x55, 0x55, 70));
		for(int r = 0; ch != '.' && r < m_Plan.Rows(); r++)
			for(int c = 0; c < m_Plan.Cols(); c++)
				for(int q = 0; q < 4; q++)
					if(m_Plan.Quarter(m_Deck, QPoint(c, r), q) == ch)
						p.drawPolygon(triangle(QPoint(c, r), q));
		p.setBrush(Qt::NoBrush);
	}
	else if(m_HoverValid && !DoorTool() && m_Tool != PlaceWindow)
	{
		p.setPen(QPen(QColor(0x5a, 0x9b, 0xff, 200), 2));
		p.drawRect(QRect(Margin + m_HoverCell.x() * CellPx, Margin + m_HoverCell.y() * CellPx, CellPx, CellPx).adjusted(1, 1, -2, -2));
	}
}
