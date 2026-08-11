// SPDX-License-Identifier: GPL-2.0
#ifndef SUBSURFACE_NEO_UPDATE_MANAGER_H
#define SUBSURFACE_NEO_UPDATE_MANAGER_H

#include <QObject>
#include <QString>

class QNetworkReply;

class NeoUpdateManager : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
	Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
	Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
	Q_PROPERTY(QString summary READ summary NOTIFY stateChanged)
	Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY stateChanged)
	Q_PROPERTY(QString releaseNotesUrl READ releaseNotesUrl NOTIFY stateChanged)
	Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)

public:
	explicit NeoUpdateManager(QObject *parent = nullptr);

	bool checking() const { return m_checking; }
	bool updateAvailable() const { return m_updateAvailable; }
	QString latestVersion() const { return m_latestVersion; }
	QString summary() const { return m_summary; }
	QString downloadUrl() const { return m_downloadUrl; }
	QString releaseNotesUrl() const { return m_releaseNotesUrl; }
	QString lastError() const { return m_lastError; }

	Q_INVOKABLE void checkForUpdates(bool force = false);

signals:
	void stateChanged();
	void updateAvailableFound();

private:
	void handleReply(QNetworkReply *reply);
	void setError(const QString &message);

	bool m_checking = false;
	bool m_updateAvailable = false;
	QString m_latestVersion;
	QString m_summary;
	QString m_downloadUrl;
	QString m_releaseNotesUrl;
	QString m_lastError;
};

#endif // SUBSURFACE_NEO_UPDATE_MANAGER_H
