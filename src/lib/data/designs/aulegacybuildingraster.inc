// Shared DnFloorBuffer16 raster and boundary tracing, 2016 AuData13.dll
// 0x1713a0, already recovered for legacy rooms. Building wall profiles, materials,
// floor selection and roof construction remain separate in the caller.
QPointF PlanPoint(const QPoint& point)
{
	return QPointF((point.x() - RasterOrigin) * RasterSpacing,
		(point.y() - RasterOrigin) * RasterSpacing);
}
QVector3D Position(const QPointF& point, double z)
{
	return QVector3D(float(point.x()), float(point.y()), float(z));
}
double Cross(const QPointF& a, const QPointF& b) { return a.x() * b.y() - a.y() * b.x(); }
double Length(const QPointF& a) { return std::hypot(a.x(), a.y()); }
QPointF Unit(const QPointF& a)
{
	const double length = Length(a);
	return length > Epsilon ? a / length : QPointF();
}
quint32 PointKey(const QPoint& p) { return quint32(p.x() * RasterSize + p.y()); }

struct Raster
{
	QVector<quint16> samples = QVector<quint16>(RasterSize * RasterSize, 0);
	QVector<int> owners = QVector<int>(RasterSize * RasterSize, -1);
	quint16 At(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < RasterSize && y < RasterSize
			? samples.at(x * RasterSize + y) : 0;
	}
	int Owner(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < RasterSize && y < RasterSize
			? owners.at(x * RasterSize + y) : -1;
	}

	// The 16-bit floor buffer uses a perimeter walk followed by directed
	// rightward fills. Open-below keeps the previous room's tag, except fuel.
	// This is distinct from the designer's 8-bit paint occupancy buffer.
	void Fill(const QPolygonF& boundary, int type, int subtype, int owner)
	{
		QVector<QPoint> polygon;
		for(const QPointF& point : boundary)
		{
			const QPoint p(int(std::round(point.x() / RasterSpacing)) + RasterOrigin,
				int(std::round(point.y() / RasterSpacing)) + RasterOrigin);
			if(polygon.isEmpty() || p != polygon.last()) polygon.append(p);
		}
		if(polygon.size() > 1 && polygon.first() == polygon.last()) polygon.removeLast();
		if(polygon.size() < 3) return;
		for(int i = 0; i < polygon.size(); ++i)
		{
			const QPoint p = polygon.at(i), d = polygon.at((i + 1) % polygon.size()) - p;
			if(p.x() < 1 || p.y() < 1 || p.x() + 1 >= RasterSize || p.y() + 1 >= RasterSize
				|| (d.x() && d.y() && std::abs(d.x()) != std::abs(d.y()))) return;
		}
		double area = 0;
		for(int i = 0; i < polygon.size(); ++i)
			area += Cross(polygon.at(i), polygon.at((i + 1) % polygon.size()));
		if(area > 0) std::reverse(polygon.begin(), polygon.end());
		const quint16 tag = quint16(((type & 31) << 8) | subtype);
		const bool openBelow = type == 11 || type == 15;
		int xmin = polygon.first().x(), xmax = xmin, ymin = polygon.first().y(), ymax = ymin;
		for(const QPoint& p : polygon)
		{
			xmin = std::min(xmin, p.x()); xmax = std::max(xmax, p.x());
			ymin = std::min(ymin, p.y()); ymax = std::max(ymax, p.y());
		}
		const int height = ymax - ymin + 1;
		QVector<quint16> temporary;
		if(openBelow) temporary.fill(0, (xmax - xmin + 1) * height);
		const auto write = [&](int x, int y)
		{
			const int index = x * RasterSize + y;
			const quint16 old = samples.at(index);
			quint16 value = tag;
			if(old & 0xc000)
				value = openBelow && Type(old) != 9 ? quint16((old & 0x1fff) | 0x6000)
					: quint16(tag | 0x4000);
			if(openBelow) temporary[(x - xmin) * height + y - ymin] = value;
			else { samples[index] = value; owners[index] = owner; }
		};
		QVector<QPoint> perimeter;
		QPoint previous = polygon.last();
		for(const QPoint& p : polygon)
		{
			const QPoint step((p.x() > previous.x()) - (p.x() < previous.x()),
				(p.y() > previous.y()) - (p.y() < previous.y()));
			while(previous != p)
			{
				perimeter.append(previous);
				write(previous.x(), previous.y());
				previous += step;
			}
		}
		if(perimeter.size() < 3) return;
		for(int i = 0; i < perimeter.size(); ++i)
		{
			const QPoint p = perimeter.at(i);
			const QPoint incoming = p - perimeter.at((i + perimeter.size() - 1) % perimeter.size());
			const QPoint outgoing = perimeter.at((i + 1) % perimeter.size()) - p;
			bool fill = false;
			if(outgoing.x() == 1)
				fill = outgoing.y() == 1 && !(incoming.x() == -1 && incoming.y() >= -1 && incoming.y() <= 0);
			else if(outgoing.x() == 0)
				fill = outgoing.y() == 1
					? !(incoming.x() == -1 && incoming.y() >= -1 && incoming.y() <= 0)
						&& !(incoming.x() == 0 && incoming.y() == -1)
					: incoming.x() == -1 && incoming.y() == 1;
			else if(outgoing.y() == 1)
				fill = incoming.y() == 1 || (incoming.x() == 1 && incoming.y() == 0);
			else if(outgoing.y() == 0 || incoming.x() != 1)
				fill = incoming.x() == -1 && incoming.y() == 1;
			if(!fill) continue;
			for(int x = p.x() + 1; x <= xmax; ++x)
			{
				const int index = x * RasterSize + p.y();
				if(openBelow ? temporary.at((x - xmin) * height + p.y() - ymin) != 0 : (samples.at(index) & 0x9fff) == tag) break;
				write(x, p.y());
			}
		}
		if(openBelow)
			for(int x = xmin; x <= xmax; ++x)
				for(int y = ymin; y <= ymax; ++y)
				{
					const quint16 value = temporary.at((x - xmin) * height + y - ymin);
					if(value) { const int i = x * RasterSize + y; samples[i] = value; owners[i] = owner; }
				}
	}

	void NextDeck()
	{
		for(quint16& sample : samples)
		{
			if(sample & 0x8000) sample = 0;
			else if(sample) sample = quint16((sample & ~0x2000) | 0x8000);
		}
	}
};


struct Edge
{
	QPoint a;
	QPoint b;
	quint16 tag;
	quint16 adjacent;
	int owner;
	int adjacentOwner;
	Edge(const QPoint& from, const QPoint& to, quint16 room, quint16 neighbor = 0,
		int roomOwner = -1, int neighborOwner = -1)
		: a(from), b(to), tag(room), adjacent(neighbor), owner(roomOwner), adjacentOwner(neighborOwner) {}
};

QVector<Edge> MergeEdges(const QVector<Edge>& edges)
{
	QHash<quint32, QVector<int>> starts, ends;
	for(int i = 0; i < edges.size(); ++i)
	{
		starts[PointKey(edges.at(i).a)].append(i);
		ends[PointKey(edges.at(i).b)].append(i);
	}
	QVector<bool> used(edges.size(), false);
	QVector<Edge> merged;
	for(int i = 0; i < edges.size(); ++i)
	{
		if(used.at(i)) continue;
		Edge edge = edges.at(i);
		used[i] = true;
		for(int direction : {1, -1})
		{
			bool extended;
			do
			{
				extended = false;
				const QVector<int> candidates = direction > 0
					? starts.value(PointKey(edge.b)) : ends.value(PointKey(edge.a));
				for(int candidate : candidates)
				{
					if(used.at(candidate)) continue;
					const Edge& other = edges.at(candidate);
					if(other.tag != edge.tag || other.adjacent != edge.adjacent
						|| other.owner != edge.owner || other.adjacentOwner != edge.adjacentOwner) continue;
					const QPointF a = edge.b - edge.a, b = other.b - other.a;
					if(Cross(a, b) != 0 || QPointF::dotProduct(a, b) <= 0) continue;
					if(direction > 0) edge.b = other.b; else edge.a = other.a;
					used[candidate] = true;
					extended = true;
					break;
				}
			} while(extended);
		}
		merged.append(edge);
	}
	return merged;
}

QVector<QVector<Edge>> Loops(const QVector<Edge>& edges)
{
	QHash<quint32, QVector<int>> starts;
	for(int i = 0; i < edges.size(); ++i) starts[PointKey(edges.at(i).a)].append(i);
	QVector<bool> used(edges.size(), false);
	QVector<QVector<Edge>> loops;
	for(int i = 0; i < edges.size(); ++i)
	{
		if(used.at(i)) continue;
		QVector<Edge> loop;
		int next = i;
		while(next >= 0 && !used.at(next))
		{
			used[next] = true;
			const Edge edge = edges.at(next);
			loop.append(edge);
			if(edge.b == loop.first().a) break;
			next = -1;
			double bestTurn = -4;
			const QPointF incoming = edge.b - edge.a;
			for(int candidate : starts.value(PointKey(edge.b)))
			{
				if(used.at(candidate)) continue;
				const QPointF outgoing = edges.at(candidate).b - edge.b;
				const double turn = std::atan2(Cross(incoming, outgoing),
					QPointF::dotProduct(incoming, outgoing));
				if(turn > bestTurn) { next = candidate; bestTurn = turn; }
			}
		}
		if(loop.size() >= 3 && loop.last().b == loop.first().a) loops.append(loop);
	}
	return loops;
}

QPointF JoinNormal(const QPointF& previous, const QPointF& next)
{
	const QPointF a = Unit(previous), b = Unit(next);
	const QPointF na(a.y(), -a.x()), nb(b.y(), -b.x());
	const double denominator = 1 + QPointF::dotProduct(na, nb);
	return denominator > Epsilon ? (na + nb) / denominator : nb;
}



