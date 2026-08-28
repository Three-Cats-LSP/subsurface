// SPDX-License-Identifier: GPL-2.0
#ifndef SUBSURFACE_NEO_WEB_DEVICE_TRANSPORT_H
#define SUBSURFACE_NEO_WEB_DEVICE_TRANSPORT_H

#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class WebDeviceTransportBackend {
public:
	using Completion = std::function<void(bool, const QString &, const QString &)>;
	virtual ~WebDeviceTransportBackend() = default;
	virtual bool available(int transportKind) const = 0;
	virtual void requestDevice(int transportKind, Completion completion) = 0;
	virtual void disconnect() = 0;
};

class WebDeviceTransport : public QObject {
	Q_OBJECT
	Q_PROPERTY(State state READ state NOTIFY changed)
	Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY changed)
	Q_PROPERTY(QString deviceName READ deviceName NOTIFY changed)
	Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
	Q_PROPERTY(bool busy READ busy NOTIFY changed)
	Q_PROPERTY(bool connected READ connected NOTIFY changed)

public:
	enum TransportKind { Bluetooth = 0, Serial = 1 };
	Q_ENUM(TransportKind)
	enum State { Idle, Requesting, Connected, Error };
	Q_ENUM(State)

	explicit WebDeviceTransport(bool bluetoothAvailable, bool serialAvailable, QObject *parent = nullptr);
	explicit WebDeviceTransport(std::shared_ptr<WebDeviceTransportBackend> backend, QObject *parent = nullptr);

	State state() const;
	QString stateLabel() const;
	QString deviceName() const;
	QString errorMessage() const;
	bool busy() const;
	bool connected() const;

	Q_INVOKABLE bool transportAvailable(TransportKind kind) const;
	Q_INVOKABLE void requestBluetoothDevice();
	Q_INVOKABLE void requestSerialPort();
	Q_INVOKABLE void disconnectDevice();

signals:
	void changed();

private:
	std::shared_ptr<WebDeviceTransportBackend> m_backend;
	State m_state = Idle;
	QString m_deviceName;
	QString m_errorMessage;

	void requestDevice(TransportKind kind);
	void finishRequest(bool success, const QString &deviceName, const QString &errorMessage);
};

#endif // SUBSURFACE_NEO_WEB_DEVICE_TRANSPORT_H
