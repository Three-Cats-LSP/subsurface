// SPDX-License-Identifier: GPL-2.0
#ifndef NEO_WEB_SYNC_MODEL_H
#define NEO_WEB_SYNC_MODEL_H

#include <QObject>

class NeoWebSyncModel : public QObject {
	Q_OBJECT
	Q_PROPERTY(QString state READ state NOTIFY changed)
	Q_PROPERTY(QString status READ status NOTIFY changed)
	Q_PROPERTY(bool conflict READ conflict NOTIFY changed)
	Q_PROPERTY(bool actionReady READ actionReady NOTIFY changed)

public:
	explicit NeoWebSyncModel(QObject *parent = nullptr);
	QString state() const;
	QString status() const;
	bool conflict() const;
	bool actionReady() const;
	Q_INVOKABLE void evaluate(int localRevision, const QString &localChecksum,
		int remoteRevision, const QString &remoteChecksum);
	Q_INVOKABLE void keepLocal();
	Q_INVOKABLE void keepRemote();
	Q_INVOKABLE void reset();

signals:
	void changed();

private:
	QString m_state = QStringLiteral("idle");
	QString m_status;
};

#endif
