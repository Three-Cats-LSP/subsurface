// SPDX-License-Identifier: GPL-2.0
#ifndef TEST_CLOUD_SYNC_HISTORY_H
#define TEST_CLOUD_SYNC_HISTORY_H
#include <QtTest>
class TestCloudSyncHistory : public QObject {
	Q_OBJECT
private slots:
	void stateTransitions();
	void invalidRecords_data();
	void invalidRecords();
	void workflow_data();
	void workflow();
	void interruption_data();
	void interruption();
	void concurrentInitialization_data();
	void concurrentInitialization();
	void initialRootRecovery_data();
	void initialRootRecovery();
	void restoreRejection_data();
	void restoreRejection();
};
#endif
