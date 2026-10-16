// SPDX-License-Identifier: GPL-2.0
#ifndef TESTCLOUDSYNCMANAGER_H
#define TESTCLOUDSYNCMANAGER_H
#include "testbase.h"

class TestCloudSyncManager : public TestBase {
	Q_OBJECT
private slots:
	void init();
	void cleanup();
	void testFileLookup_data();
	void testFileLookup();
	void testDropboxMissingFile_data();
	void testDropboxMissingFile();
	void testAuthorizationFailureClearsBusy_data();
	void testAuthorizationFailureClearsBusy();
	void testDownloadKeepsConcurrentEdits();
	void testManifestKeepsConcurrentEdits();
	void testNetworkFailureAllowsRetry();
	void testMissingTextDoesNotStartUpload();
	void testCorruptDownload_data();
	void testCorruptDownload();
	void testCanonicalCloudLog();
	void testOrphanCloudLog();
	void testGoogleDesktopCredentials_data();
	void testGoogleDesktopCredentials();
	void testGoogleDesktopTokenRequests();
	void testLoopbackIgnoresBrowserRequests();
};
#endif
