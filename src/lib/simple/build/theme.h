#pragma once

#include "common.h"

namespace SimpleMode::detail {

enum Theme { Wood, Stone, Metal, Plain };

Theme ThemeOf(const QString& key);

HOLD_ACCESS HoldOf(const QString& type);

void FinishTrim(const Plan& plan, AuScFaceList& faces, const QColor& usual, const QString& texture, const QColor& colour,
	double strength, double scale = 1);
void FinishOutsideTrim(const Plan& plan, AuScFaceList& faces, const QColor& usual);
void FinishOutsideTrimWall(const Plan& plan, AuScFaceList& faces, const QColor& usual);

}
