#pragma once

#include <QtMath>

namespace SimpleMode::detail {

struct V2
{
	double x = 0, y = 0;
	constexpr V2() = default;
	constexpr V2(double x_, double y_) : x(x_), y(y_) {}
	constexpr V2 operator+(const V2& o) const { return V2(x + o.x, y + o.y); }
	constexpr V2 operator-(const V2& o) const { return V2(x - o.x, y - o.y); }
	constexpr V2 operator*(double f) const { return V2(x * f, y * f); }
	[[nodiscard]] constexpr double Dot(const V2& o) const { return x * o.x + y * o.y; }
	[[nodiscard]] constexpr double Cross(const V2& o) const { return x * o.y - y * o.x; }
	[[nodiscard]] double Length() const { return qSqrt(x * x + y * y); }
	[[nodiscard]] V2 Unit() const { double l = Length(); return l > 0 ? V2(x / l, y / l) : V2(); }
	[[nodiscard]] constexpr V2 Left() const { return V2(-y, x); }
};

}
