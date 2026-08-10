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

	Modern.DesignTokens { id: tokens }

	Connections {
		target: CloudSync
		function onDiveLogBackupFinished(providerId) { page.lastBackupProvider = providerId }
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
				text: qsTr("Connect Google Drive or Dropbox to keep your Subsurface Neo data available across devices. OAuth credentials are stored using the platform secure credential store.")
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
							visible: page.lastBackupProvider === modelData.id
							text: qsTr("Dive-log backup uploaded successfully")
							color: tokens.success
							font.pixelSize: 12
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
							text: qsTr("Backup now")
							onClicked: {
								page.lastBackupProvider = ""
								CloudSync.backupDiveLog(modelData.id)
							}
						}
						Button {
							text: qsTr("Disconnect")
							onClicked: CloudSync.disconnectProvider(modelData.id)
						}
					}

					Button {
						visible: !modelData.connected
						text: qsTr("Connect")
						enabled: modelData.configured && !CloudSync.authorizationInProgress
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
			text: qsTr("Backup now uploads a complete Subsurface XML snapshot. Bidirectional conflict-aware synchronization will build on the same provider transport. Subsurface Cloud remains available through the existing account workflow.")
			color: tokens.textSecondary
			font.pixelSize: 12
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}
	}
}
