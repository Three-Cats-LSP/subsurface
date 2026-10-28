// SPDX-License-Identifier: GPL-2.0
#ifndef TESTLSPPLANNER_H
#define TESTLSPPLANNER_H
#include <QObject>
class TestLspPlanner : public QObject {
	Q_OBJECT
private slots:
	void identityAndPresets();
	void upstreamReference_data();
	void upstreamReference();
	void invalidRequest_data();
	void invalidRequest();
	void freshEngineIsolation();
	void physicalProjection_data();
	void physicalProjection();
	void ndlReference();
	void readableDiveSettings_data();
	void readableDiveSettings();
};
#endif
