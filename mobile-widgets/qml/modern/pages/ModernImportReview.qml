// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern
import "../components" as Components

Kirigami.Page {
	id: page
	title: qsTr("Import review")
	background: Rectangle { color: tokens.background }

	property string vendor: ""
	property string product: ""
	property string connection: ""
	property bool downloading: false
	property bool importsReady: false
	property bool downloadFailed: false
	property string importError: ""
	property bool forceBluetoothAddress: false
	property string pairedSerialPort: ""
	property var pairedSerialPorts: []
	property int pairedSerialPortIndex: 0
	property bool automaticBluetoothFallbackUsed: false
	property int selectedImportCount: 0
	signal finished()

	Modern.DesignTokens { id: tokens }

	DCImportModel {
		id: importModel
		onDownloadFinished: {
			page.downloading = false
			page.importsReady = rowCount() > 0
			page.selectedImportCount = page.importsReady ? rowCount() : 0
			if (page.importsReady && page.pairedSerialPort.length > 0)
				manager.rememberBluetoothSerialPort(page.connection, page.pairedSerialPort)
			page.downloadFailed = !page.importsReady && /error|failed|timeout/i.test(manager.progressMessage)
			if (page.downloadFailed && !page.forceBluetoothAddress && page.pairedSerialPortIndex + 1 < page.pairedSerialPorts.length) {
				page.pairedSerialPortIndex += 1
				manager.appendTextToLog("Paired serial connection failed; retrying the Perdix on " + page.pairedSerialPorts[page.pairedSerialPortIndex])
				page.startDownload(false)
			} else if (page.downloadFailed && page.pairedSerialPort.length > 0 && !page.forceBluetoothAddress && !page.automaticBluetoothFallbackUsed) {
				page.automaticBluetoothFallbackUsed = true
				manager.appendTextToLog("All paired serial connections failed; retrying the Perdix through Bluetooth services")
				page.forceBluetoothAddress = true
				page.startDownload(false)
			} else if (page.downloadFailed) {
				page.importError = manager.progressMessage
			}
		}
	}

	function configureConnection() {
		manager.DC_vendor = vendor
		manager.DC_product = product
		var address = /((LE|BT):)?([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}/i.exec(connection)
		if (address !== null && Qt.platform.os === "windows" && !forceBluetoothAddress && pairedSerialPorts.length === 0)
			pairedSerialPorts = manager.pairedBluetoothSerialPorts(address[0])
		var serialPort = !forceBluetoothAddress && pairedSerialPortIndex < pairedSerialPorts.length ? pairedSerialPorts[pairedSerialPortIndex] : ""
		pairedSerialPort = serialPort
		manager.DC_bluetoothMode = address !== null && serialPort.length === 0
		manager.DC_devName = serialPort.length > 0 ? serialPort : (address !== null ? address[0] : connection)
		if (address !== null && serialPort.length === 0)
			manager.retrieveBluetoothName()
		manager.stopBluetoothDiscovery()
		manager.appendTextToLog("Neo import configured " + vendor + " " + product + " on " + manager.DC_devName + (manager.DC_bluetoothMode ? " (Bluetooth)" : (serialPort.length > 0 ? " (paired Bluetooth serial port)" : "")))
	}

	function startDownload(resetTransport) {
		if (resetTransport === undefined || resetTransport) {
			forceBluetoothAddress = false
			automaticBluetoothFallbackUsed = false
			pairedSerialPorts = []
			pairedSerialPortIndex = 0
		}
		configureConnection()
		importsReady = false
		downloadFailed = false
		importError = ""
		downloading = true
		manager.progressMessage = qsTr("Preparing Bluetooth connection…")
		importModel.clearTable()
		connectionDelay.restart()
	}
	Timer {
		id: connectionDelay
		interval: 750
		repeat: false
		onTriggered: {
			manager.progressMessage = ""
			importModel.startDownload()
		}
	}

	function acceptSelected() {
		if (selectedImportCount <= 0)
			return
		importModel.recordDives()
		manager.changesNeedSaving()
		importsReady = false
		finished()
	}

	Component.onCompleted: startDownload(true)

	Connections {
		target: manager
		function onErrorSignal() {
			page.downloading = false
			page.downloadFailed = true
			page.importError = manager.progressMessage.length > 0 ? manager.progressMessage : qsTr("The dive computer did not complete the import. Check the connection and try again.")
		}
		function onRestartDownloadSignal() {
			if (page.downloading)
				page.startDownload(true)
		}
	}

	ColumnLayout {
		anchors.fill: parent
		anchors.margins: tokens.space16
		spacing: tokens.space12

		Text { text: vendor + " · " + product; color: tokens.textPrimary; font.pixelSize: 22; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: connection; color: tokens.textSecondary; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }

		Components.ModernCard {
			Layout.fillWidth: true
			visible: downloading || manager.progressMessage.length > 0
			Text { text: downloading ? qsTr("Downloading dives…") : qsTr("Import status"); color: tokens.textPrimary; font.weight: Font.DemiBold }
			ProgressBar { Layout.fillWidth: true; indeterminate: downloading && manager.progress <= 0; value: manager.progress }
			Text { Layout.fillWidth: true; visible: manager.progressMessage.length > 0; text: manager.progressMessage; color: tokens.textSecondary; wrapMode: Text.WordWrap }
			Components.NeoButton { text: qsTr("Cancel download"); variant: "danger"; compact: true; visible: downloading; onClicked: { manager.cancelDownloadDC(); downloading = false } }
		}

		Text { visible: !downloading && !importsReady && importError.length === 0; text: qsTr("No new dives were found. Check the connection and retry, or choose all dives in the Dive Computer Center."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Components.ModernCard {
			visible: importError.length > 0
			Layout.fillWidth: true
			Text { text: qsTr("Connection needs attention"); color: tokens.accent; font.weight: Font.DemiBold }
			Text { text: importError; color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { visible: vendor === "Shearwater"; text: qsTr("For a classic Shearwater Perdix on Windows, pair it in Windows Bluetooth settings first and put the computer in Dive Log → Upload before retrying."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}
		Components.NeoButton { visible: !downloading && !importsReady; text: qsTr("Copy diagnostic log"); compact: true; Layout.alignment: Qt.AlignLeft; onClicked: manager.copyAppLogToClipboard() }
		Text { visible: importsReady; text: qsTr("%1 downloaded dives — %2 selected.").arg(importModel.rowCount()).arg(selectedImportCount); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

		ListView {
			id: importList
			Layout.fillWidth: true
			Layout.fillHeight: true
			visible: importsReady
			clip: true
			model: importModel
			spacing: tokens.space8
			ScrollBar.vertical: ScrollBar {
				id: importScrollBar
				policy: ScrollBar.AlwaysOn
			}
			delegate: Components.ModernCard {
				required property int index
				required property string datetime
				required property string duration
				required property string depth
				required property bool selected
				width: Math.max(0, importList.width - importScrollBar.width - tokens.space4)
				border.width: selected ? 1 : 0
				border.color: tokens.accent
				RowLayout {
					Layout.fillWidth: true
					Layout.minimumWidth: 0
					CheckBox {
						checked: selected
						onClicked: {
							importModel.selectRow(index)
							page.selectedImportCount += checked ? 1 : -1
						}
					}
					ColumnLayout { Layout.fillWidth: true; Layout.minimumWidth: 0; Text { Layout.fillWidth: true; text: datetime; color: tokens.textPrimary; font.weight: Font.Medium; elide: Text.ElideRight }
 Text { Layout.fillWidth: true; text: depth + " · " + duration; color: tokens.textSecondary; font.pixelSize: 12; elide: Text.ElideRight } }
				}
			}
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.width >= 760 ? 4 : 2
			columnSpacing: tokens.space8
			rowSpacing: tokens.space8
			Components.NeoButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Retry"); enabled: !downloading; compact: true; onClicked: page.startDownload(true) }
			Components.NeoButton {
				Layout.fillWidth: true; Layout.minimumWidth: 0
				visible: !downloading && !importsReady && Qt.platform.os === "windows" && pairedSerialPort.length > 0
				text: qsTr("Try Bluetooth directly")
				compact: true
				onClicked: { page.forceBluetoothAddress = true; page.startDownload(false) }
			}
			Components.NeoButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Select none"); visible: importsReady; compact: true; onClicked: { importModel.selectNone(); page.selectedImportCount = 0 } }
			Components.NeoButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Select all"); visible: importsReady; compact: true; onClicked: { importModel.selectAll(); page.selectedImportCount = importModel.rowCount() } }
			Components.NeoButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Add selected dives (%1)").arg(selectedImportCount); variant: "primary"; enabled: importsReady && selectedImportCount > 0; compact: true; onClicked: page.acceptSelected() }
		}
	}
}
