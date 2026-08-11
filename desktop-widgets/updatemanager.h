// SPDX-License-Identifier: GPL-2.0
#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateManager : public QObject {
	Q_OBJECT
public:
	explicit UpdateManager(QObject *parent = 0);
	void checkForUpdates(bool automatic = false);

public
slots:
	void requestReceived();

private:
	bool isAutomaticCheck;
	QNetworkAccessManager *networkManager;
};

#endif // UPDATEMANAGER_H
