// SPDX-License-Identifier: GPL-2.0
#include "web/subsurface-neo/wasm/webdevicetransport.h"

#include <QtTest>

class MockTransportBackend final : public WebDeviceTransportBackend {
public:
	bool bluetoothAvailable = true;
	bool serialAvailable = true;
	bool disconnectCalled = false;
	bool nextSuccess = true;
	QString nextDevice = QStringLiteral("Mock Perdix");
	QString nextError = QStringLiteral("Selection cancelled");

	bool available(int kind) const override { return kind == WebDeviceTransport::Bluetooth ? bluetoothAvailable : serialAvailable; }
	void requestDevice(int, Completion completion) override { completion(nextSuccess, nextDevice, nextSuccess ? QString() : nextError); }
	void disconnect() override { disconnectCalled = true; }
};

class TestWebDeviceTransport : public QObject {
	Q_OBJECT
private slots:
	void connectsAndDisconnects();
	void exposesSelectionFailure();
	void reportsCapabilities();
};

void TestWebDeviceTransport::connectsAndDisconnects()
{
	auto backend = std::make_shared<MockTransportBackend>();
	WebDeviceTransport transport(backend);
	QSignalSpy changed(&transport, &WebDeviceTransport::changed);
	transport.requestBluetoothDevice();
	QCOMPARE(transport.state(), WebDeviceTransport::Connected);
	QCOMPARE(transport.deviceName(), QStringLiteral("Mock Perdix"));
	QVERIFY(transport.connected());
	QCOMPARE(changed.count(), 2);
	transport.disconnectDevice();
	QCOMPARE(transport.state(), WebDeviceTransport::Idle);
	QVERIFY(backend->disconnectCalled);
}

void TestWebDeviceTransport::exposesSelectionFailure()
{
	auto backend = std::make_shared<MockTransportBackend>();
	backend->nextSuccess = false;
	WebDeviceTransport transport(backend);
	transport.requestSerialPort();
	QCOMPARE(transport.state(), WebDeviceTransport::Error);
	QCOMPARE(transport.errorMessage(), backend->nextError);
	QVERIFY(!transport.connected());
}

void TestWebDeviceTransport::reportsCapabilities()
{
	auto backend = std::make_shared<MockTransportBackend>();
	backend->serialAvailable = false;
	WebDeviceTransport transport(backend);
	QVERIFY(transport.transportAvailable(WebDeviceTransport::Bluetooth));
	QVERIFY(!transport.transportAvailable(WebDeviceTransport::Serial));
}

QTEST_GUILESS_MAIN(TestWebDeviceTransport)
#include "test_webdevicetransport.moc"
