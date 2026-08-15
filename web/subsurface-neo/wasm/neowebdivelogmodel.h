// SPDX-License-Identifier: GPL-2.0
#ifndef NEO_WEB_DIVELOG_MODEL_H
#define NEO_WEB_DIVELOG_MODEL_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

class NeoWebDiveLogModel : public QObject {
	Q_OBJECT
	Q_PROPERTY(int diveCount READ diveCount NOTIFY changed)
	Q_PROPERTY(QString totalTime READ totalTime NOTIFY changed)
	Q_PROPERTY(QString maxDepth READ maxDepth NOTIFY changed)
	Q_PROPERTY(QVariantList recentDives READ recentDives NOTIFY changed)
	Q_PROPERTY(QString fileStatus READ fileStatus NOTIFY changed)
	Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
	Q_PROPERTY(bool error READ error NOTIFY changed)

public:
	explicit NeoWebDiveLogModel(QObject *parent = nullptr);

	int diveCount() const;
	QString totalTime() const;
	QString maxDepth() const;
	QVariantList recentDives() const;
	QString fileStatus() const;
	bool loaded() const;
	bool error() const;

	Q_INVOKABLE void openLocalFile(const QUrl &url);

signals:
	void changed();

private:
	int m_diveCount = 0;
	int m_totalSeconds = 0;
	double m_maxDepthMeters = 0.0;
	QVariantList m_recentDives;
	QString m_fileStatus;
	bool m_loaded = false;
	bool m_error = false;
};

#endif // NEO_WEB_DIVELOG_MODEL_H
