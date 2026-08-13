// SPDX-License-Identifier: GPL-2.0
#ifndef SUBSURFACE_NEO_WEB_CAPABILITIES_H
#define SUBSURFACE_NEO_WEB_CAPABILITIES_H

#include <QObject>
#include <QUrl>

class WebCapabilities : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool webAssemblyRuntime READ webAssemblyRuntime CONSTANT)
	Q_PROPERTY(bool secureContext READ secureContext CONSTANT)
	Q_PROPERTY(bool webBluetoothAvailable READ webBluetoothAvailable CONSTANT)
	Q_PROPERTY(bool webSerialAvailable READ webSerialAvailable CONSTANT)
	Q_PROPERTY(bool mobileBrowser READ mobileBrowser CONSTANT)
	Q_PROPERTY(QString browserSummary READ browserSummary CONSTANT)
	Q_PROPERTY(QString selectedFileStatus READ selectedFileStatus NOTIFY selectedFileStatusChanged)

public:
	explicit WebCapabilities(QObject *parent = nullptr);

	bool webAssemblyRuntime() const;
	bool secureContext() const;
	bool webBluetoothAvailable() const;
	bool webSerialAvailable() const;
	bool mobileBrowser() const;
	QString browserSummary() const;
	QString selectedFileStatus() const;

	Q_INVOKABLE void inspectLocalFile(const QUrl &url);

signals:
	void selectedFileStatusChanged();

private:
	bool m_secureContext = false;
	bool m_webBluetoothAvailable = false;
	bool m_webSerialAvailable = false;
	bool m_mobileBrowser = false;
	QString m_browserSummary;
	QString m_selectedFileStatus;
};

#endif // SUBSURFACE_NEO_WEB_CAPABILITIES_H
