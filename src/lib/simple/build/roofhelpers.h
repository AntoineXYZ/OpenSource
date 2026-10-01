#pragma once

#include "../geometry/mesh.h"
#include "theme.h"

namespace SimpleMode::detail {

QColor RoofPaint(Theme theme);

const Skylight* SkylightAt(const Plan& plan, int deck, const QPoint& cell);

void SkylightHole(const Plan& plan, const QPoint& cell, double& x0, double& y0, double& x1, double& y1);

}
