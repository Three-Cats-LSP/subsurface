// SPDX-License-Identifier: GPL-2.0
#ifndef TEST_CLOUD_SYNC_REVIEW_H
#define TEST_CLOUD_SYNC_REVIEW_H
#include "testbase.h"
class TestCloudSyncReview : public TestBase {
	Q_OBJECT
private slots:
	void classification_data();
	void classification();
	void transport_data();
	void transport();
	void cancellation_data();
	void cancellation();
};
#endif
