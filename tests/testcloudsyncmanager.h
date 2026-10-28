// SPDX-License-Identifier: GPL-2.0
#ifndef TESTCLOUDSYNCMANAGER_H
#define TESTCLOUDSYNCMANAGER_H
#include "testbase.h"
#include <string>

class TestCloudSyncManager : public TestBase {
	Q_OBJECT
private:
	QString previousApplicationName;
	QString previousOrganizationName;
	std::string previousSettingsSuffix;
private slots:
	void init();
	void testStorageReview_data();
	void testStorageReview();
	void testHistoryGuard_data();
	void testHistoryGuard();
	void testWorkspaceGuard_data();
	void testWorkspaceGuard();
	void testWorkspaceSelection_data();
	void testWorkspaceSelection();
	void testWorkspaceCopy_data();
	void testWorkspaceCopy();
	void testWorkspaceModel_data();
	void testWorkspaceModel();
	void testWorkspaceVersions_data();
	void testWorkspaceVersions();
	void testAccountIdentity_data();
	void testAccountIdentity();
	void testAccountLookupFailure_data();
	void testAccountLookupFailure();
	void testAccountLookupCancellation();
	void testAccountChangeDuringRefresh();
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
	void testRefreshFailure_data();
	void testRefreshFailure();
	void testAccessTokenRejected_data();
	void testAccessTokenRejected();
	void testDisconnectCancelsRefresh();
	void testDisconnectCancelsAuthorization();
	void testDisconnectCancelsFileRequests_data();
	void testDisconnectCancelsFileRequests();
	void testMissingTextDoesNotStartUpload();
	void testCorruptDownload_data();
	void testCorruptDownload();
	void testCanonicalCloudLog();
	void testOrphanCloudLog();
	void testGoogleDesktopCredentials_data();
	void testGoogleDesktopCredentials();
	void testGoogleDesktopTokenRequests();
	void testLoopbackIgnoresBrowserRequests();
	void testTransactionalUpload_data();
	void testTransactionalUpload();
	void testSimultaneousWriters_data();
	void testSimultaneousWriters();
	void testGoogleInitialRecovery_data();
	void testGoogleInitialRecovery();
	void testGoogleDifferentInitialLog();
	void testVersionedRead_data();
	void testVersionedRead();
};
#endif
