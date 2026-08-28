// SPDX-License-Identifier: GPL-2.0
#include "webdevicetransport.h"

#include <QPointer>

namespace {

class CapabilityBackend final : public WebDeviceTransportBackend {
public:
	CapabilityBackend(bool bluetoothAvailable, bool serialAvailable) : bluetooth(bluetoothAvailable), serial(serialAvailable) {}

	bool available(int transportKind) const override
	{
		return transportKind == WebDeviceTransport::Bluetooth ? bluetooth : serial;
	}

	void requestDevice(int transportKind, Completion completion) override
	{
		if (!available(transportKind)) {
			completion(false, {}, transportKind == WebDeviceTransport::Bluetooth ?
				QObject::tr("Web Bluetooth is unavailable in this browser.") :
				QObject::tr("Web Serial is unavailable in this browser."));
			return;
		}
		completion(false, {}, QObject::tr("The browser selection bridge is ready for integration but is not enabled in this build."));
	}

	void disconnect() override {}

private:
	bool bluetooth;
	bool serial;
};

} // namespace

WebDeviceTransport::WebDeviceTransport(bool bluetoothAvailable, bool serialAvailable, QObject *parent) :
	WebDeviceTransport(std::make_shared<CapabilityBackend>(bluetoothAvailable, serialAvailable), parent)
{
}

WebDeviceTransport::WebDeviceTransport(std::shared_ptr<WebDeviceTransportBackend> backend, QObject *parent) :
	QObject(parent), m_backend(std::move(backend))
{
}

WebDeviceTransport::State WebDeviceTransport::state() const { return m_state; }
QString WebDeviceTransport::deviceName() const { return m_deviceName; }
QString WebDeviceTransport::errorMessage() const { return m_errorMessage; }
bool WebDeviceTransport::busy() const { return m_state == Requesting; }
bool WebDeviceTransport::connected() const { return m_state == Connected; }

QString WebDeviceTransport::stateLabel() const
{
	switch (m_state) {
	case Idle: return tr("No browser device connected");
	case Requesting: return tr("Waiting for browser device selection…");
	case Connected: return tr("Connected to %1").arg(m_deviceName);
	case Error: return m_errorMessage;
	}
	return {};
}

bool WebDeviceTransport::transportAvailable(TransportKind kind) const
{
	return m_backend && m_backend->available(kind);
}

void WebDeviceTransport::requestBluetoothDevice() { requestDevice(Bluetooth); }
void WebDeviceTransport::requestSerialPort() { requestDevice(Serial); }

void WebDeviceTransport::requestDevice(TransportKind kind)
{
	if (!m_backend || m_state == Requesting)
		return;
	m_state = Requesting;
	m_deviceName.clear();
	m_errorMessage.clear();
	emit changed();
	QPointer<WebDeviceTransport> guard(this);
	m_backend->requestDevice(kind, [guard](bool success, const QString &deviceName, const QString &errorMessage) {
		if (guard)
			guard->finishRequest(success, deviceName, errorMessage);
	});
}

void WebDeviceTransport::finishRequest(bool success, const QString &deviceName, const QString &errorMessage)
{
	m_deviceName = success ? deviceName : QString();
	m_errorMessage = success ? QString() : errorMessage;
	m_state = success ? Connected : Error;
	emit changed();
}

void WebDeviceTransport::disconnectDevice()
{
	if (m_backend)
		m_backend->disconnect();
	m_state = Idle;
	m_deviceName.clear();
	m_errorMessage.clear();
	emit changed();
}
