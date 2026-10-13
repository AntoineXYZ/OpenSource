#include "aulegacyscnpcocclusion.h"
#include <QMap>
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	bool Finite(const AuPoint& point)
	{
		return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
	}
	using Edge = std::array<double, 6>;
	Edge EdgeKey(const AuPoint& a, const AuPoint& b)
	{
		const std::array<double, 3> av = {a.x, a.y, a.z}, bv = {b.x, b.y, b.z};
		const auto& start = av < bv ? av : bv;
		const auto& end = av < bv ? bv : av;
		return {start[0], start[1], start[2], end[0], end[1], end[2]};
	}
	double Axis(const AuPoint& point, int axis)
	{
		return axis == 0 ? point.x : axis == 1 ? point.y : point.z;
	}
	bool SegmentBox(const AuPoint& eye, const AuPoint& target, const AuPoint& minimum, const AuPoint& maximum)
	{
		double enter = 0, exit = 1;
		for(int axis = 0; axis < 3; ++axis)
		{
			const double origin = Axis(eye, axis), delta = Axis(target, axis) - origin;
			const double low = Axis(minimum, axis) - .001, high = Axis(maximum, axis) + .001;
			if(std::abs(delta) < 1e-12)
			{
				if(origin < low || origin > high) return false;
				continue;
			}
			double a = (low - origin) / delta, b = (high - origin) / delta;
			if(a > b) std::swap(a, b);
			enter = std::max(enter, a); exit = std::min(exit, b);
			if(enter > exit) return false;
		}
		return true;
	}
}

void AuLegacyScNpcOcclusion::Add(const AuScTriRenderList& triangles, quint16 doorId)
{
	QMap<Edge, int> edges;
	for(const AuScTriRender& triangle : triangles)
	{
		if(m_Faces.size() >= 250000) break; // Missing occluders leave actors visible.
		Face face;
		// Legacy draws use GL_CCW; AuScTri::Normal describes GL_CW. Keep
		// a front-facing normal and clockwise edge tests for the shadow cone.
		face.points[0] = triangle.V0(); face.points[1] = triangle.V2(); face.points[2] = triangle.V1();
		face.normal = -triangle.Normal(); face.door = doorId;
		if(!Finite(face.points[0]) || !Finite(face.points[1]) || !Finite(face.points[2])
			|| !Finite(face.normal) || face.normal.Length2() < .99) continue;
		bool merged = false;
		for(int edge = 0; edge < 3 && !merged; ++edge)
		{
			const AuPoint& a = face.points[edge];
			const AuPoint& b = face.points[(edge + 1) % 3];
			const auto found = edges.constFind(EdgeKey(a, b));
			if(found == edges.cend()) continue;
			Face& previous = m_Faces[found.value()];
			if(previous.count != 3 || previous.normal.Dot(face.normal) < 1 - 1e-10) continue;
			int third = -1;
			for(int vertex = 0; vertex < 3; ++vertex)
				if(previous.points[vertex] != a && previous.points[vertex] != b) third = vertex;
			if(third < 0) continue;
			const AuPoint& c = face.points[(edge + 2) % 3];
			const AuPoint& d = previous.points[third];
			if(c == d || std::abs((c - d).Dot(face.normal)) > 1e-8) continue;
			AuPoint quad[4] = {c, a, d, b};
			double sign = 0;
			bool convex = true;
			for(int vertex = 0; vertex < 4; ++vertex)
			{
				const double turn = (quad[(vertex + 1) % 4] - quad[vertex])
					.Cross(quad[(vertex + 2) % 4] - quad[(vertex + 1) % 4]).Dot(face.normal);
				if(std::abs(turn) < 1e-8 || (sign && turn * sign <= 0)) { convex = false; break; }
				sign = turn;
			}
			if(!convex) continue;
			if(sign > 0) std::swap(quad[1], quad[3]); // Clockwise edge tests, same polygon.
			for(int vertex = 0; vertex < 4; ++vertex) previous.points[vertex] = quad[vertex];
			previous.count = 4;
			merged = true;
		}
		if(merged) continue;
		const int index = m_Faces.size();
		m_Faces.append(face);
		for(int edge = 0; edge < 3; ++edge)
			edges.insert(EdgeKey(face.points[edge], face.points[(edge + 1) % 3]), index);
	}
}

int AuLegacyScNpcOcclusion::BuildNode(int first, int count)
{
	Node node;
	node.first = first; node.count = count;
	node.minimum = m_Faces[first].minimum; node.maximum = m_Faces[first].maximum;
	for(int face = first + 1; face < first + count; ++face)
	{
		node.minimum = node.minimum.Min(m_Faces[face].minimum);
		node.maximum = node.maximum.Max(m_Faces[face].maximum);
	}
	const int index = m_Nodes.size();
	m_Nodes.append(node);
	if(count <= 8) return index;
	const AuPoint size = node.maximum - node.minimum;
	const int axis = size.x >= size.y && size.x >= size.z ? 0 : size.y >= size.z ? 1 : 2;
	const int middle = first + count / 2;
	std::nth_element(m_Faces.begin() + first, m_Faces.begin() + middle, m_Faces.begin() + first + count,
		[axis](const Face& a, const Face& b)
		{
			return Axis(a.minimum + a.maximum, axis) < Axis(b.minimum + b.maximum, axis);
		});
	const int left = BuildNode(first, middle - first);
	const int right = BuildNode(middle, first + count - middle);
	m_Nodes[index].left = left; m_Nodes[index].right = right;
	m_Nodes[index].count = 0;
	return index;
}

void AuLegacyScNpcOcclusion::Build()
{
	m_Nodes.clear();
	for(Face& face : m_Faces)
	{
		face.minimum = face.maximum = face.points[0];
		for(int vertex = 1; vertex < face.count; ++vertex)
		{
			face.minimum = face.minimum.Min(face.points[vertex]);
			face.maximum = face.maximum.Max(face.points[vertex]);
		}
	}
	if(!m_Faces.isEmpty()) BuildNode(0, m_Faces.size());
}

bool AuLegacyScNpcOcclusion::Covers(const Face& face, const AuBox& bounds, const AuPoint& eye)
{
	const double cameraDistance = (eye - face.points[0]).Dot(face.normal);
	if(cameraDistance <= .05) return false; // Backfaces and camera-on-wall are never occluders.
	for(const AuPoint& corner : bounds.m_fCorners)
	{
		const double distance = (corner - face.points[0]).Dot(face.normal);
		if(distance >= -.05) return false; // The whole actor must lie beyond the face.
		const double t = cameraDistance / (cameraDistance - distance);
		const AuPoint hit = eye + (corner - eye) * t;
		for(int vertex = 0; vertex < face.count; ++vertex)
		{
			const AuPoint edge = face.points[(vertex + 1) % face.count] - face.points[vertex];
			// Shrink the blocking polygon slightly to preserve doorway edges.
			if(edge.Cross(hit - face.points[vertex]).Dot(face.normal) >= -.02 * edge.Length()) return false;
		}
	}
	// A convex box lies entirely inside this opaque face's shadow cone. Unlike
	// a handful of ray samples, this cannot bridge a crack or an open doorway.
	return true;
}

bool AuLegacyScNpcOcclusion::Occluded(const AuBox& bounds, const AuPoint& eye,
	const QSet<quint16>& closedDoors) const
{
	if(m_Nodes.isEmpty() || !Finite(eye)) return false;
	AuPoint minimum, maximum;
	bounds.GetMinMax(minimum, maximum);
	if(!Finite(minimum) || !Finite(maximum)) return false;
	const AuPoint target = (minimum + maximum) / 2;
	int stack[64], count = 1, visits = 0;
	stack[0] = 0;
	while(count)
	{
		if(++visits > 512) return false; // Bounded work, conservative fallback.
		const Node& node = m_Nodes[stack[--count]];
		if(!SegmentBox(eye, target, node.minimum, node.maximum)) continue;
		if(node.count)
		{
			for(int index = node.first; index < node.first + node.count; ++index)
			{
				const Face& face = m_Faces[index];
				if(face.door && !closedDoors.contains(face.door)) continue;
				if(Covers(face, bounds, eye)) return true;
			}
		}
		else
		{
			if(count + 2 > 64) return false;
			stack[count++] = node.left; stack[count++] = node.right;
		}
	}
	return false;
}
