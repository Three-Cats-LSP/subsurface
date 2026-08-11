// SPDX-License-Identifier: GPL-2.0
#include "neoupdatemanager.h"

#include "neoversion.h"
#include "qthelper.h"
#include "settings/qPrefUpdateManager.h"

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
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

	return currentVersion.prerelease && !remoteVersion.prerelease;
}

bool isTrustedUrl(const QUrl &url)
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

NeoUpdateManager::NeoUpdateManager(QObject *parent) : QObject(parent)
{
}

void NeoUpdateManager::checkForUpdates(bool force)
{
	if (m_checking)
		return;

	if (!force) {
		if (qPrefUpdateManager::dont_check_for_updates())
			return;

		const QString currentVersion = QString::fromLatin1(subsurface_neo_version());
		if (qPrefUpdateManager::last_version_used() == currentVersion &&
		    qPrefUpdateManager::next_check() > QDate::currentDate())
			return;
		qPrefUpdateManager::set_last_version_used(currentVersion);
	}

	m_checking = true;
	m_lastError.clear();
	emit stateChanged();

	QNetworkRequest request(QUrl(QString::fromLatin1(neoUpdateManifestUrl)));
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader("User-Agent", getUserAgent().toUtf8());
	QNetworkReply *reply = manager()->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleReply(reply); });
}

void NeoUpdateManager::setError(const QString &message)
{
	m_checking = false;
	m_lastError = message;
	qPrefUpdateManager::set_next_check(QDate::currentDate().addDays(1));
	emit stateChanged();
}

void NeoUpdateManager::handleReply(QNetworkReply *reply)
{
	if (!reply)
		return;

	if (reply->error() != QNetworkReply::NoError) {
		const QString error = reply->errorString();
		reply->deleteLater();
		setError(error);
		return;
	}

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
	reply->deleteLater();
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		setError(tr("The Subsurface Neo update manifest is invalid."));
		return;
	}

	const QJsonObject root = document.object();
	if (root.value(QStringLiteral("schema")).toInt() != 1 ||
	    root.value(QStringLiteral("channel")).toString() != QStringLiteral("stable")) {
		setError(tr("The Subsurface Neo update manifest is not supported by this build."));
		return;
	}

	const QString latestVersion = root.value(QStringLiteral("version")).toString().trimmed();
	const QString currentVersion = QString::fromLatin1(subsurface_neo_version());
	const bool available = isNewerVersion(latestVersion, currentVersion);

	QString platformKey;
#if defined(Q_OS_ANDROID)
	platformKey = QStringLiteral("android-arm64");
#elif defined(Q_OS_WIN)
	platformKey = QStringLiteral("windows-x64");
#endif

	QString download;
	if (!platformKey.isEmpty()) {
		const QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
		download = platforms.value(platformKey).toObject().value(QStringLiteral("url")).toString().trimmed();
	}

	const QString releaseNotes = root.value(QStringLiteral("releaseNotesUrl")).toString().trimmed();
	if (available && (!isTrustedUrl(QUrl(download)) || !isTrustedUrl(QUrl(releaseNotes)))) {
		setError(tr("The update manifest contains an untrusted address."));
		return;
	}

	m_checking = false;
	m_updateAvailable = available;
	m_latestVersion = latestVersion;
	m_summary = root.value(QStringLiteral("summary")).toString().trimmed();
	m_downloadUrl = download;
	m_releaseNotesUrl = releaseNotes;
	m_lastError.clear();
	qPrefUpdateManager::set_next_check(QDate::currentDate().addDays(1));
	emit stateChanged();

	if (m_updateAvailable)
		emit updateAvailableFound();
}
