// SPDX-License-Identifier: GPL-2.0
#ifndef PREFERENCES_CLOUD_H
#define PREFERENCES_CLOUD_H

#include "abstractpreferenceswidget.h"

#include <QHash>

namespace Ui {
	class PreferencesCloud;
}

class CloudSyncManager;
class QLabel;
class QPushButton;

class PreferencesCloud : public AbstractPreferencesWidget {
	Q_OBJECT

public:
	PreferencesCloud();
	~PreferencesCloud();
	void refreshSettings() override;
	void syncSettings() override;

public slots:
	void updateCloudAuthenticationState();
	void passwordUpdateSuccessful();
	void on_resetPassword_clicked();

private:
	void setupNeoCloudProviders();
	void updateNeoCloudProviders();

	Ui::PreferencesCloud *ui;
	CloudSyncManager *neoCloudSync = nullptr;
	QHash<QString, QLabel *> neoStatusLabels;
	QHash<QString, QPushButton *> neoActionButtons;
	QHash<QString, QPushButton *> neoSyncButtons;
	QHash<QString, QPushButton *> neoBackupButtons;
};

#endif
