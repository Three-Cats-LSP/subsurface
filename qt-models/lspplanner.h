// SPDX-License-Identifier: GPL-2.0
#ifndef LSPPLANNER_H
#define LSPPLANNER_H

#include <QJsonArray>
#include <QJsonObject>

// Shared native Windows/Android boundary. It exposes data only to the engine;
// no QObject, filesystem, networking or browser document is made available.
namespace LspPlanner {
QJsonObject identity();
QJsonArray presets();
QJsonObject calculate(const QJsonObject &request);
QJsonObject project(const QJsonObject &input);
}

#endif
