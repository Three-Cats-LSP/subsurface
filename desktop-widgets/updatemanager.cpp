// SPDX-License-Identifier: GPL-2.0
#include "desktop-widgets/updatemanager.h"

#include "core/neoversion.h"
#include "core/qthelper.h"
#include "core/settings/qPrefUpdateManager.h"
#include "desktop-widgets/mainwindow.h"

#include <QDate>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QUrl>
#include <QVersionNumber>

namespace {
constexpr auto neoUpdateManifestUrl = "https://threecats-lsp.com/subsurface-neo/update.json";

struct ParsedVersion {
	QVersionNumber number;
	bool prerelease = false;
};

ParsedVersion parseVersion(const QString &value)
{
	const QString trimmed = value.trimmed();
	const int suffix = trimmed.indexOf('-');
	const QString numeric = suffix >= 0 ? trimmed.left(suffix) : trimmed;
	return { QVersionNumber::fromString(numeric), suffix >= 0 };
}

bool isNewerVersion(const QString &remote, const QString &current)
{
	const ParsedVersion remoteVersion = parseVersion(remote);
	const ParsedVersion currentVersion = parseVersion(current);
	if (remoteVersion.number.isNull() || currentVersion.number.isNull())
		return false;

	const int comparison = QVersionNumber::compare(remoteVersion.number, currentVersion.number);
	if (comparison != 0)
		return comparison > 0;

	// A stable release of the same numeric version supersedes a prerelease/dev build.
	return currentVersion.prerelease && !remoteVersion.prerelease;
}

bool isTrustedDownloadUrl(const QUrl &url)
{
	if (!url.isValid() || url.scheme() != QStringLiteral("https"))
		return false;
	const QString host = url.host().toLower();
	return host == QStringLiteral("threecats-lsp.com") ||
	       host == QStringLiteral("www.threecats-lsp.com") ||
	       host == QStringLiteral("github.com") ||
	       host.endsWith(QStringLiteral(".githubusercontent.com"));
}
}

UpdateManager::UpdateManager(QObject *parent) :
	QObject(parent),
	isAutomaticCheck(false)
{
	if (qPrefUpdateManager::dont_check_for_updates())
		return;

	const QString currentVersion = QString::fromLatin1(subsurface_neo_version());
	if (qPrefUpdateManager::last_version_used() == currentVersion &&
	    qPrefUpdateManager::next_check() > QDate::currentDate())
		return;

	qPrefUpdateManager::set_last_version_used(currentVersion);
	checkForUpdates(true);
}

void UpdateManager::checkForUpdates(bool automatic)
{
	isAutomaticCheck = automatic;

	QNetworkRequest request(QUrl(QString::fromLatin1(neoUpdateManifestUrl)));
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader("User-Agent", getUserAgent().toUtf8());
	connect(manager()->get(request), &QNetworkReply::finished, this, &UpdateManager::requestReceived, Qt::UniqueConnection);
}

void UpdateManager::requestReceived()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply)
		return;

	const auto setNextCheck = []() {
		// Neo releases can happen independently of upstream Subsurface, so a daily
		// cached check is cheap while still being reasonably responsive.
		qPrefUpdateManager::set_next_check(QDate::currentDate().addDays(1));
	};

	if (reply->error() != QNetworkReply::NoError) {
		if (!isAutomaticCheck) {
			QMessageBox::warning(MainWindow::instance(), tr("Check for updates"),
				tr("Subsurface Neo was unable to check for updates.\n\n%1")
					.arg(reply->errorString()));
		}
		setNextCheck();
		reply->deleteLater();
		return;
	}

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
	reply->deleteLater();
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		if (!isAutomaticCheck)
			QMessageBox::warning(MainWindow::instance(), tr("Check for updates"), tr("The Subsurface Neo update manifest is invalid."));
		setNextCheck();
		return;
	}

	const QJsonObject root = document.object();
	if (root.value(QStringLiteral("schema")).toInt() != 1 ||
	    root.value(QStringLiteral("channel")).toString() != QStringLiteral("stable")) {
		if (!isAutomaticCheck)
			QMessageBox::warning(MainWindow::instance(), tr("Check for updates"), tr("The Subsurface Neo update manifest is not supported by this build."));
		setNextCheck();
		return;
	}

	const QString latestVersion = root.value(QStringLiteral("version")).toString().trimmed();
	const QString currentVersion = QString::fromLatin1(subsurface_neo_version());
	const bool updateAvailable = isNewerVersion(latestVersion, currentVersion);

	if (!updateAvailable) {
		if (!isAutomaticCheck) {
			QMessageBox::information(MainWindow::instance(), tr("Check for updates"),
				tr("You are using the latest version of Subsurface Neo (%1).")
					.arg(currentVersion));
		}
		setNextCheck();
		return;
	}

	QString download = root.value(QStringLiteral("releaseUrl")).toString();
	const QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
#if defined(Q_OS_WIN)
	const QJsonObject platform = platforms.value(QStringLiteral("windows-x64")).toObject();
#elif defined(Q_OS_ANDROID)
	const QJsonObject platform = platforms.value(QStringLiteral("android-arm64")).toObject();
#else
	const QJsonObject platform;
#endif
	if (!platform.value(QStringLiteral("url")).toString().isEmpty())
		download = platform.value(QStringLiteral("url")).toString();

	const QUrl downloadUrl(download);
	if (!isTrustedDownloadUrl(downloadUrl)) {
		if (!isAutomaticCheck)
			QMessageBox::warning(MainWindow::instance(), tr("Check for updates"), tr("The update manifest contains an untrusted download address."));
		setNextCheck();
		return;
	}

	QString summary = root.value(QStringLiteral("summary")).toString().trimmed();
	if (summary.isEmpty())
		summary = tr("A newer Subsurface Neo build is available.");

	QMessageBox message(MainWindow::instance());
	message.setWindowTitle(tr("Subsurface Neo update available"));
	message.setWindowIcon(QIcon(":subsurface-icon"));
	message.setIcon(QMessageBox::Information);
	message.setText(tr("<b>Subsurface Neo %1 is available.</b><br/><br/>You are running %2.<br/><br/>%3")
		.arg(latestVersion.toHtmlEscaped(), currentVersion.toHtmlEscaped(), summary.toHtmlEscaped()));
	message.setTextFormat(Qt::RichText);
	QPushButton *downloadButton = message.addButton(tr("Download Update"), QMessageBox::AcceptRole);
	message.addButton(tr("Remind Me Later"), QMessageBox::RejectRole);
	message.exec();

	if (message.clickedButton() == downloadButton)
		QDesktopServices::openUrl(downloadUrl);

	setNextCheck();
}
