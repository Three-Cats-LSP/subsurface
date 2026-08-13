// SPDX-License-Identifier: GPL-2.0
#ifndef TESTCLOUDSYNCMANIFEST_H
#define TESTCLOUDSYNCMANIFEST_H

#include "testbase.h"

class TestCloudSyncManifest : public TestBase {
	Q_OBJECT

private slots:
	void testRoundTrip();
	void testRejectsInvalidManifest();
	void testStateRelation_data();
	void testStateRelation();
};

#endif // TESTCLOUDSYNCMANIFEST_H
