#pragma once

#include <QMap>
#include <QString>
#include <QTextStream>

#include "../model/plan.h"

class AuBdDesign;

namespace SimpleMode {

int BuildPlan(const Plan& plan, const QString& outPath, QTextStream& log, bool checks = true);

int BuildPlanInto(const Plan& plan, AuBdDesign& out, QTextStream& log);

QMap<QString, int> LastBuildTriangles();

}
int BuildFromGrid(const QString& gridPath, const QString& outPath, QTextStream& log);
