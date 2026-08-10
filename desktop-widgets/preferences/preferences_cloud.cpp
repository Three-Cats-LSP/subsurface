// SPDX-License-Identifier: GPL-2.0
#include "preferences_cloud.h"
#include "ui_preferences_cloud.h"
#include "subsurfacewebservices.h"
#include "core/cloudstorage.h"
#include "core/cloudsyncmanager.h"
#include "core/errorhelper.h"
#include "core/settings/qPrefCloudStorage.h"

#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PreferencesCloud::PreferencesCloud() : AbstractPreferencesWidget(tr("Cloud"),QIcon(":preferences-cloud-icon"), 9), ui(new Ui::PreferencesCloud())
{
	ui->setupUi(this);

	ui->label_help2->setWordWrap(true);
	ui->label_help3->setWordWrap(true);
	ui->label_help4->setWordWrap(true);
	setupNeoCloudProviders();
}

PreferencesCloud::~PreferencesCloud()
{
	delete ui;
}

void PreferencesCloud::setupNeoCloudProviders()
{
	neoCloudSync = new CloudSyncManager(manager(), this);

	auto *group = new QGroupBox(tr("Subsurface Neo cloud sync"), this);
	auto *layout = new QVBoxLayout(group);
	auto *description = new QLabel(tr("Connect Google Drive or Dropbox for Subsurface Neo synchronization. OAuth credentials are stored securely by the operating system."), group);
	description->setWordWrap(true);
	layout->addWidget(description);

	for (const QVariant &providerVariant : neoCloudSync->providers()) {
		const QVariantMap provider = providerVariant.toMap();
		const QString id = provider.value(QStringLiteral("id")).toString();
		const QString name = provider.value(QStringLiteral("name")).toString();

		auto *row = new QHBoxLayout();
		auto *nameLabel = new QLabel(name, group);
		auto *statusLabel = new QLabel(group);
		auto *backupButton = new QPushButton(tr("Backup now"), group);
		auto *actionButton = new QPushButton(group);
		nameLabel->setMinimumWidth(140);
		statusLabel->setMinimumWidth(100);
		row->addWidget(nameLabel);
		row->addWidget(statusLabel, 1);
		row->addWidget(backupButton);
		row->addWidget(actionButton);
		layout->addLayout(row);

		neoStatusLabels.insert(id, statusLabel);
		neoBackupButtons.insert(id, backupButton);
		neoActionButtons.insert(id, actionButton);
		connect(backupButton, &QPushButton::clicked, this, [this, id]() {
			neoCloudSync->backupDiveLog(id);
		});
		connect(actionButton, &QPushButton::clicked, this, [this, id]() {
			for (const QVariant &providerVariant : neoCloudSync->providers()) {
				const QVariantMap provider = providerVariant.toMap();
				if (provider.value(QStringLiteral("id")).toString() != id)
					continue;
				if (provider.value(QStringLiteral("connected")).toBool())
					neoCloudSync->disconnectProvider(id);
				else
					neoCloudSync->beginAuthorization(id);
				break;
			}
		});
	}

	ui->verticalLayout->insertWidget(1, group);
	connect(neoCloudSync, &CloudSyncManager::providersChanged, this, &PreferencesCloud::updateNeoCloudProviders);
	connect(neoCloudSync, &CloudSyncManager::authorizationInProgressChanged, this, &PreferencesCloud::updateNeoCloudProviders);
	connect(neoCloudSync, &CloudSyncManager::diveLogBackupFinished, this, [this](const QString &providerId) {
		for (const QVariant &providerVariant : neoCloudSync->providers()) {
			const QVariantMap provider = providerVariant.toMap();
			if (provider.value(QStringLiteral("id")).toString() == providerId) {
				QMessageBox::information(this, tr("Subsurface Neo cloud sync"),
					tr("Dive-log backup uploaded to %1.").arg(provider.value(QStringLiteral("name")).toString()));
				break;
			}
		}
	});
	connect(neoCloudSync, &CloudSyncManager::lastErrorChanged, this, [this]() {
		updateNeoCloudProviders();
		if (!neoCloudSync->lastError().isEmpty())
			QMessageBox::warning(this, tr("Subsurface Neo cloud sync"), neoCloudSync->lastError());
	});
	updateNeoCloudProviders();
}

void PreferencesCloud::updateNeoCloudProviders()
{
	if (!neoCloudSync)
		return;
	for (const QVariant &providerVariant : neoCloudSync->providers()) {
		const QVariantMap provider = providerVariant.toMap();
		const QString id = provider.value(QStringLiteral("id")).toString();
		QLabel *status = neoStatusLabels.value(id);
		QPushButton *button = neoActionButtons.value(id);
		QPushButton *backup = neoBackupButtons.value(id);
		if (!status || !button || !backup)
			continue;
		const bool configured = provider.value(QStringLiteral("configured")).toBool();
		const bool connected = provider.value(QStringLiteral("connected")).toBool();
		status->setText(connected ? tr("Connected") : configured ? tr("Not connected") : tr("Unavailable"));
		button->setText(connected ? tr("Disconnect") : tr("Connect"));
		button->setEnabled(configured && (!neoCloudSync->authorizationInProgress() || connected));
		backup->setVisible(connected);
		backup->setEnabled(connected);
	}
}

void PreferencesCloud::on_resetPassword_clicked()
{
	QDesktopServices::openUrl(QUrl("https://cloud.subsurface-divelog.org/passwordreset"));
}

void PreferencesCloud::refreshSettings()
{
	ui->cloud_storage_email->setText(QString::fromStdString(prefs.cloud_storage_email));
	ui->cloud_storage_password->setText(QString::fromStdString(prefs.cloud_storage_password));
	ui->save_password_local->setChecked(prefs.save_password_local);
	updateCloudAuthenticationState();
	updateNeoCloudProviders();
}

void PreferencesCloud::syncSettings()
{
	auto cloud = qPrefCloudStorage::instance();

	QString email = ui->cloud_storage_email->text().toLower();
	QString password = ui->cloud_storage_password->text();
	QString newpassword = ui->cloud_storage_new_passwd->text();
	QString emailpasswordformatwarning = tr("Change ignored. Cloud storage email and new password can only consist of letters, numbers, and '.', '-', '_', and '+'.");

	if (prefs.cloud_verification_status == qPrefCloudStorage::CS_VERIFIED && !newpassword.isEmpty()) {
		if (!email.isEmpty() && !password.isEmpty()) {
			if (!isValidEmail(email) || !isValidPassword(password)) {
				QMessageBox::warning(this, tr("Warning"), emailpasswordformatwarning);
				return;
			}
			if (!isValidEmail(email) || (!newpassword.isEmpty() && !isValidPassword(newpassword))) {
				QMessageBox::warning(this, tr("Warning"), emailpasswordformatwarning);
				ui->cloud_storage_new_passwd->setText(QString());
				return;
			}
			CloudStorageAuthenticate *cloudAuth = new CloudStorageAuthenticate(this);
			connect(cloudAuth, &CloudStorageAuthenticate::finishedAuthenticate, this, &PreferencesCloud::updateCloudAuthenticationState);
			connect(cloudAuth, &CloudStorageAuthenticate::passwordChangeSuccessful, this, &PreferencesCloud::passwordUpdateSuccessful);
			cloudAuth->backend(email, password, "", newpassword);
			ui->cloud_storage_new_passwd->setText(QString());
		}
	} else if (prefs.cloud_verification_status == qPrefCloudStorage::CS_UNKNOWN ||
		   prefs.cloud_verification_status == qPrefCloudStorage::CS_INCORRECT_USER_PASSWD ||
		   email.toStdString() != prefs.cloud_storage_email ||
		   password.toStdString() != prefs.cloud_storage_password) {

		int oldVerificationStatus = cloud->cloud_verification_status();
		cloud->set_cloud_verification_status(qPrefCloudStorage::CS_UNKNOWN);
		if (!email.isEmpty() && !password.isEmpty()) {
			if (!isValidEmail(email) || !isValidPassword(password)) {
				QMessageBox::warning(this, tr("Warning"), emailpasswordformatwarning);
				cloud->set_cloud_verification_status(oldVerificationStatus);
				return;
			}
			CloudStorageAuthenticate *cloudAuth = new CloudStorageAuthenticate(this);
			connect(cloudAuth, &CloudStorageAuthenticate::finishedAuthenticate, this, &PreferencesCloud::updateCloudAuthenticationState);
			cloudAuth->backend(email, password);
		}
	} else if (prefs.cloud_verification_status == qPrefCloudStorage::CS_NEED_TO_VERIFY) {
		QString pin = ui->cloud_storage_pin->text();
		if (!pin.isEmpty()) {
			if (!isValidEmail(email) || !isValidPassword(password)) {
				QMessageBox::warning(this, tr("Warning"), emailpasswordformatwarning);
				return;
			}
			CloudStorageAuthenticate *cloudAuth = new CloudStorageAuthenticate(this);
			connect(cloudAuth, &CloudStorageAuthenticate::finishedAuthenticate, this, &PreferencesCloud::updateCloudAuthenticationState);
			cloudAuth->backend(email, password, pin);
		}
	}
	cloud->set_cloud_storage_email(email);
	cloud->set_save_password_local(ui->save_password_local->isChecked());
	cloud->set_cloud_storage_password(password);
	cloud->set_cloud_verification_status(prefs.cloud_verification_status);
	cloud->set_cloud_base_url(QString::fromStdString(prefs.cloud_base_url));
}

void PreferencesCloud::updateCloudAuthenticationState()
{
	ui->cloud_storage_pin->setEnabled(prefs.cloud_verification_status == qPrefCloudStorage::CS_NEED_TO_VERIFY);
	ui->cloud_storage_pin->setVisible(prefs.cloud_verification_status == qPrefCloudStorage::CS_NEED_TO_VERIFY);
	ui->cloud_storage_pin_label->setEnabled(prefs.cloud_verification_status == qPrefCloudStorage::CS_NEED_TO_VERIFY);
	ui->cloud_storage_pin_label->setVisible(prefs.cloud_verification_status == qPrefCloudStorage::CS_NEED_TO_VERIFY);
	ui->cloud_storage_new_passwd->setEnabled(prefs.cloud_verification_status == qPrefCloudStorage::CS_VERIFIED);
	ui->cloud_storage_new_passwd->setVisible(prefs.cloud_verification_status == qPrefCloudStorage::CS_VERIFIED);
	ui->cloud_storage_new_passwd_label->setEnabled(prefs.cloud_verification_status == qPrefCloudStorage::CS_VERIFIED);
	ui->cloud_storage_new_passwd_label->setVisible(prefs.cloud_verification_status == qPrefCloudStorage::CS_VERIFIED);
	if (prefs.cloud_verification_status == qPrefCloudStorage::CS_VERIFIED) {
		ui->cloudStorageGroupBox->setTitle(tr("Subsurface cloud storage (credentials verified)"));
	} else if (prefs.cloud_verification_status == qPrefCloudStorage::CS_INCORRECT_USER_PASSWD) {
		ui->cloudStorageGroupBox->setTitle(tr("Subsurface cloud storage (incorrect password)"));
	} else if (prefs.cloud_verification_status == qPrefCloudStorage::CS_NEED_TO_VERIFY) {
		ui->cloudStorageGroupBox->setTitle(tr("Subsurface cloud storage (PIN required)"));
	} else {
		ui->cloudStorageGroupBox->setTitle(tr("Subsurface cloud storage"));
	}
	emit settingsChanged();
}

void PreferencesCloud::passwordUpdateSuccessful()
{
	ui->cloud_storage_password->setText(QString::fromStdString(prefs.cloud_storage_password));
}
