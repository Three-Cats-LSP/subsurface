// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Cloud & Sync")
	background: Rectangle { color: tokens.background }
	property string lastBackupProvider: ""
	property string lastSyncProvider: ""
	property string lastSyncResult: ""
	property string conflictProvider: ""
	property string initialChoiceProvider: ""

	Modern.DesignTokens { id: tokens }

	function resultText(result) {
		if (result === "up-to-date") return qsTr("Up to date")
		if (result === "uploaded") return qsTr("Local changes uploaded")
		if (result === "downloaded") return qsTr("Cloud changes downloaded")
		return result
	}

	Connections {
		target: CloudSync
		function onDiveLogBackupFinished(providerId) {
			page.lastBackupProvider = providerId
		}
		function onDiveLogSyncFinished(providerId, result) {
			page.lastSyncProvider = providerId
			page.lastSyncResult = result
			page.conflictProvider = ""
			page.initialChoiceProvider = ""
		}
		function onDiveLogSyncConflict(providerId) {
			page.conflictProvider = providerId
			page.lastSyncProvider = ""
		}
		function onDiveLogInitialChoiceRequired(providerId) {
			page.initialChoiceProvider = providerId
			page.lastSyncProvider = ""
		}
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16

		ColumnLayout {
			Layout.fillWidth: true
			spacing: tokens.space4
			Text {
				text: qsTr("Cloud & Sync")
				color: tokens.textPrimary
				font.pixelSize: 28
				font.weight: Font.DemiBold
			}
			Text {
				text: qsTr("Connect Google Drive or Dropbox to keep your Subsurface Neo data available across devices. Sync uses revision manifests and SHA-256 checksums so divergent logs are never silently overwritten.")
				color: tokens.textSecondary
				font.pixelSize: 14
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
		}

		Repeater {
			model: CloudSync.providers
			delegate: Components.ModernCard {
				required property var modelData
				Layout.fillWidth: true

				RowLayout {
					Layout.fillWidth: true
					spacing: tokens.space12

					ColumnLayout {
						Layout.fillWidth: true
						spacing: tokens.space4
						Text {
							text: modelData.name
							color: tokens.textPrimary
							font.pixelSize: 18
							font.weight: Font.DemiBold
						}
						Text {
							text: modelData.connected ? qsTr("Connected") :
								  modelData.configured ? qsTr("Ready to connect") : qsTr("Not available on this platform yet")
							color: modelData.connected ? tokens.accent : tokens.textSecondary
							font.pixelSize: 14
						}
						Text {
							visible: page.lastSyncProvider === modelData.id
							text: page.resultText(page.lastSyncResult)
							color: tokens.success
							font.pixelSize: 12
						}
						Text {
							visible: page.lastBackupProvider === modelData.id
							text: qsTr("Backup uploaded successfully")
							color: tokens.success
							font.pixelSize: 12
						}
						Text {
							visible: page.conflictProvider === modelData.id
							text: qsTr("Sync conflict: both this device and the cloud changed since the last sync. Nothing was overwritten.")
							color: "#FFB84D"
							font.pixelSize: 12
							wrapMode: Text.WordWrap
							Layout.fillWidth: true
						}
						Text {
							visible: page.initialChoiceProvider === modelData.id
							text: qsTr("This is the first sync and different data already exists in the cloud. Neo will not choose a winner automatically. Use Backup now only if you intentionally want this device to become the cloud baseline.")
							color: "#FFB84D"
							font.pixelSize: 12
							wrapMode: Text.WordWrap
							Layout.fillWidth: true
						}
						Text {
							visible: modelData.id === "google-drive" && !modelData.configured
							text: qsTr("Google requires a platform-specific Android OAuth client. Desktop and web clients are already configured.")
							color: tokens.textSecondary
							font.pixelSize: 12
							wrapMode: Text.WordWrap
							Layout.fillWidth: true
						}
					}

					RowLayout {
						visible: modelData.connected
						Button {
							text: qsTr("Sync now")
							enabled: !CloudSync.syncInProgress
							onClicked: {
								page.lastSyncProvider = ""
								page.conflictProvider = ""
								page.initialChoiceProvider = ""
								CloudSync.syncDiveLog(modelData.id)
							}
						}
						Button {
							text: qsTr("Backup now")
							enabled: !CloudSync.syncInProgress
							onClicked: {
								page.lastBackupProvider = ""
								CloudSync.backupDiveLog(modelData.id)
							}
						}
						Button {
							text: qsTr("Disconnect")
							enabled: !CloudSync.syncInProgress
							onClicked: CloudSync.disconnectProvider(modelData.id)
						}
					}

					Button {
						visible: !modelData.connected
						text: qsTr("Connect")
						enabled: modelData.configured && !CloudSync.authorizationInProgress && !CloudSync.syncInProgress
						onClicked: CloudSync.beginAuthorization(modelData.id)
					}
				}
			}
		}

		Components.ModernCard {
			visible: CloudSync.lastError.length > 0
			Layout.fillWidth: true
			Text {
				text: CloudSync.lastError
				color: tokens.textPrimary
				font.pixelSize: 13
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
		}

		Text {
			text: qsTr("Sync automatically uploads local-only changes and downloads cloud-only changes. If both sides changed, Neo stops and reports a conflict. Backup now is an explicit one-way snapshot operation. Subsurface Cloud remains available through the existing account workflow.")
			color: tokens.textSecondary
			font.pixelSize: 12
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}
	}
}
