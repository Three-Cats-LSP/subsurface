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

	Modern.DesignTokens { id: tokens }

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
							visible: modelData.id === "google-drive" && !modelData.configured
							text: qsTr("Google requires a platform-specific Android OAuth client. Desktop and web clients are already configured.")
							color: tokens.textSecondary
							font.pixelSize: 12
							wrapMode: Text.WordWrap
							Layout.fillWidth: true
						}
					}

					Button {
						text: modelData.connected ? qsTr("Disconnect") : qsTr("Connect")
						enabled: modelData.connected || (modelData.configured && !CloudSync.authorizationInProgress)
						onClicked: {
							if (modelData.connected)
								CloudSync.disconnectProvider(modelData.id)
							else
								CloudSync.beginAuthorization(modelData.id)
						}
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
			text: qsTr("Subsurface Cloud remains available through the existing Subsurface account workflow. Neo cloud providers are additive and do not replace the legacy backend.")
			color: tokens.textSecondary
			font.pixelSize: 12
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}
	}
}
