#pragma once

#include <QColor>

namespace SimpleMode::detail {

inline constexpr double SHELL_INSET = 0.1;
inline constexpr double VOID_INSET = 0.1;

inline constexpr double TRIM_TEXTURE_SCALE = 0.25;
inline constexpr double OPEN_AIR_NEAR = 0.9;

inline constexpr double VOID_OVERLAP = 0.1;

inline constexpr double DOOR_WIDTH = 1.2;

inline constexpr double DOOR_HEIGHT = 2.1;

inline constexpr double DOOR_PANEL = 0.1;

inline constexpr double HULL_MARGIN = 2 / 3.28084;

inline constexpr double SWITCH_ASIDE = 0.3;

inline constexpr double SWITCH_HEIGHT = 1.1;

inline constexpr double SWITCH_OUT = 0.05;

inline constexpr double PATH_HEIGHT = 1.0;

inline constexpr double HATCH_SIZE = 1.0;

inline constexpr double HATCH_PANEL = 0.1;

inline constexpr float VOID_OPACITY = .25f;

inline constexpr double TRIM_MAX = 0.08;

const QColor TRIM_PAINT(0xa8, 0x72, 0x44, 255);

const QColor OUTSIDE_TRIM_PAINT(0x8c, 0x6e, 0x50, 255);

inline constexpr double LOW_POLY_TINT = 0.5;

const QColor DOOR_PAINT(0x2f, 0x6f, 0xc0, 255);

const QColor DOOR_BATTEN_PAINT(0x22, 0x4c, 0x88, 255);

inline constexpr double LADDER_IN = 0.04;

inline constexpr double CLIMBER_BACK = 0.4;

inline constexpr double ROOF_OVERHANG = 0.3;

inline constexpr double ROOF_THICKNESS = 0.1;

inline constexpr double DORMER_SETBACK = 0.4;

}
