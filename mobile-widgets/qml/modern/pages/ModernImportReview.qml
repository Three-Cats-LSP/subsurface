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
	property string importError: ""
	signal finished()

	Modern.DesignTokens { id: tokens }

	DCImportModel {
		id: importModel
		onDownloadFinished: {
			page.downloading = false
			page.importsReady = rowCount() > 0
		}
	}

	function configureConnection() {
		manager.DC_vendor = vendor
		manager.DC_product = product
		var address = /((LE|BT):)?([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}/i.exec(connection)
		manager.DC_bluetoothMode = address !== null
		manager.DC_devName = address !== null ? address[0] : connection
		if (address !== null)
			manager.retrieveBluetoothName()
	}

	function startDownload() {
		configureConnection()
		importsReady = false
		importError = ""
		downloading = true
		manager.progressMessage = ""
		importModel.clearTable()
		importModel.startDownload()
	}

	function acceptSelected() {
		importModel.recordDives()
		manager.changesNeedSaving()
		importsReady = false
		finished()
	}

	Component.onCompleted: startDownload()

	Connections {
		target: manager
		function onErrorSignal() {
			page.downloading = false
			page.importError = manager.progressMessage.length > 0 ? manager.progressMessage : qsTr("The dive computer did not complete the import. Check the connection and try again.")
		}
		function onRestartDownloadSignal() {
			if (page.downloading)
				page.startDownload()
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
			Button { text: qsTr("Cancel download"); visible: downloading; onClicked: { manager.cancelDownloadDC(); downloading = false } }
		}

		Text { visible: !downloading && !importsReady; text: qsTr("No new dives were found. Check the connection and retry, or choose all dives in the Dive Computer Center."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Components.ModernCard {
			visible: importError.length > 0
			Layout.fillWidth: true
			Text { text: qsTr("Connection needs attention"); color: tokens.accent; font.weight: Font.DemiBold }
			Text { text: importError; color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}
		Text { visible: importsReady; text: qsTr("%1 downloaded dives — select the entries to add to your log.").arg(importModel.rowCount()); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

		ListView {
			Layout.fillWidth: true
			Layout.fillHeight: true
			visible: importsReady
			clip: true
			model: importModel
			spacing: tokens.space8
			delegate: Components.ModernCard {
				required property int index
				Layout.fillWidth: true
				width: ListView.view.width
				border.width: model.selected ? 1 : 0
				border.color: tokens.accent
				RowLayout {
					Layout.fillWidth: true
					CheckBox { checked: model.selected; onToggled: importModel.selectRow(index) }
					ColumnLayout { Layout.fillWidth: true; Text { text: model.datetime || ""; color: tokens.textPrimary; font.weight: Font.Medium }
 Text { text: (model.depth || "") + " · " + (model.duration || ""); color: tokens.textSecondary; font.pixelSize: 12 } }
				}
			}
		}

		RowLayout {
			Layout.fillWidth: true
			Button { text: qsTr("Retry"); enabled: !downloading; onClicked: page.startDownload() }
			Item { Layout.fillWidth: true }
			Button { text: qsTr("Select none"); visible: importsReady; onClicked: importModel.selectNone() }
			Button { text: qsTr("Select all"); visible: importsReady; onClicked: importModel.selectAll() }
			Button { text: qsTr("Add selected dives"); enabled: importsReady; onClicked: page.acceptSelected() }
		}
	}
}
