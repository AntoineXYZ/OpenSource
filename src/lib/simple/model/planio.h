#pragma once

#include <QString>
#include <optional>
#include <QTextStream>
#include "plan.h"

namespace SimpleMode {

bool ParsePlanText(const QString& text, Plan& plan, QTextStream& log);
bool ParsePlan(const QString& path, Plan& plan, QTextStream& log);
std::optional<Plan> TryLoadPlan(const QString& path, QTextStream& log);

QString PlanToText(const Plan& plan);

bool CheckPlan(const Plan& plan, QTextStream& log);

}
