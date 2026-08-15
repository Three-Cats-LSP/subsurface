// SPDX-License-Identifier: GPL-2.0
#ifndef SUBSURFACE_NEO_WEB_CAPABILITIES_H
#define SUBSURFACE_NEO_WEB_CAPABILITIES_H

#include <QObject>

class WebCapabilities : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool webAssemblyRuntime READ webAssemblyRuntime CONSTANT)
	Q_PROPERTY(bool secureContext READ secureContext CONSTANT)
	Q_PROPERTY(bool webBluetoothAvailable READ webBluetoothAvailable CONSTANT)
	Q_PROPERTY(bool webSerialAvailable READ webSerialAvailable CONSTANT)
	Q_PROPERTY(bool mobileBrowser READ mobileBrowser CONSTANT)
	Q_PROPERTY(QString browserSummary READ browserSummary CONSTANT)

public:
	explicit WebCapabilities(QObject *parent = nullptr);

	bool webAssemblyRuntime() const;
	bool secureContext() const;
	bool webBluetoothAvailable() const;
	bool webSerialAvailable() const;
	bool mobileBrowser() const;
	QString browserSummary() const;

private:
	bool m_secureContext = false;
	bool m_webBluetoothAvailable = false;
	bool m_webSerialAvailable = false;
	bool m_mobileBrowser = false;
	QString m_browserSummary;
};

#endif // SUBSURFACE_NEO_WEB_CAPABILITIES_H
