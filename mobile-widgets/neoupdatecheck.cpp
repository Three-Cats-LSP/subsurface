// SPDX-License-Identifier: GPL-2.0
#include "core/neoversion.h"
#include "core/qthelper.h"
#include "mobile-widgets/qmlmanager.h"

#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

namespace {
constexpr auto updateManifestUrl = "https://threecats-lsp.com/subsurface-neo/update.json";
constexpr auto lastCheckKey = "neoUpdate/lastCheck";

bool trustedDownloadUrl(const QUrl &url)
{
	if (url.scheme() != QStringLiteral("https"))
		return false;
	const QString host = url.host().toLower();
	return host == QStringLiteral("threecats-lsp.com") ||
	       host == QStringLiteral("github.com") ||
	       host.endsWith(QStringLiteral(".githubusercontent.com"));
}

QString releaseUrlForPlatform(const QJsonObject &root)
{
	const QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
#if defined(Q_OS_ANDROID)
	return platforms.value(QStringLiteral("android-arm64")).toObject().value(QStringLiteral("url")).toString();
#else
	return QString();
#endif
}

void showUpdate(const QString &version, const QString &downloadUrl)
{
#if defined(Q_OS_ANDROID)
	QJniObject::callStaticMethod<void>(
		"org/subsurfacedivelog/mobile/NeoUpdateNotifier",
		"show",
		"(Ljava/lang/String;Ljava/lang/String;)V",
		QJniObject::fromString(version).object<jstring>(),
		QJniObject::fromString(downloadUrl).object<jstring>());
#else
	if (QMLManager::instance())
		QMLManager::instance()->setNotificationText(
			QCoreApplication::translate("NeoUpdate", "Subsurface Neo %1 is available.").arg(version));
#endif
}

void performCheck()
{
	QSettings settings;
	const QDate lastCheck = settings.value(QString::fromLatin1(lastCheckKey)).toDate();
	if (lastCheck.isValid() && lastCheck >= QDate::currentDate())
		return;
	settings.setValue(QString::fromLatin1(lastCheckKey), QDate::currentDate());

	static QNetworkAccessManager network;
	QNetworkRequest request(QUrl(QString::fromLatin1(updateManifestUrl)));
	request.setRawHeader("Accept", "application/json");
	QNetworkReply *reply = network.get(request);
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply]() {
		const QByteArray payload = reply->readAll();
		const auto error = reply->error();
		reply->deleteLater();
		if (error != QNetworkReply::NoError)
			return;

		QJsonParseError parseError;
		const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
		if (parseError.error != QJsonParseError::NoError || !doc.isObject())
			return;

		const QJsonObject root = doc.object();
		if (root.value(QStringLiteral("schema")).toInt() != 1 ||
		    root.value(QStringLiteral("channel")).toString() != QStringLiteral("stable"))
			return;

		const QString latest = root.value(QStringLiteral("version")).toString();
		QString current = QString::fromLatin1(subsurface_neo_version());
		current.remove(QStringLiteral("-dev"));
		if (QVersionNumber::compare(QVersionNumber::fromString(latest), QVersionNumber::fromString(current)) <= 0)
			return;

		const QString download = releaseUrlForPlatform(root);
		const QUrl downloadUrl(download);
		if (download.isEmpty() || !trustedDownloadUrl(downloadUrl))
			return;

		showUpdate(latest, download);
	});
}

void startNeoUpdateCheck()
{
	QTimer::singleShot(2500, qApp, performCheck);
}
}

Q_COREAPP_STARTUP_FUNCTION(startNeoUpdateCheck)
