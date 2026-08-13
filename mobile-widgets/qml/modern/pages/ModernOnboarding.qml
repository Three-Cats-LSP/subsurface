// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Welcome to Neo")
	background: Rectangle { color: tokens.background }

	signal useLocalLog()
	signal openLegacyCloudSetup()

	Modern.DesignTokens { id: tokens }

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space24

		Item { Layout.preferredHeight: tokens.space16 }

		ColumnLayout {
			Layout.fillWidth: true
			spacing: tokens.space8
			Text {
				text: qsTr("Your dive log,\nclearer at every depth.")
				color: tokens.textPrimary
				font.pixelSize: 32
				font.weight: Font.DemiBold
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
			Text {
				text: qsTr("Start with a private local log. You can connect Google Drive or Dropbox whenever you are ready.")
				color: tokens.textSecondary
				font.pixelSize: 16
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Start locally"); color: tokens.textPrimary; font.pixelSize: 20; font.weight: Font.DemiBold }
			Text {
				text: qsTr("Create or open a dive log on this device. Nothing is uploaded automatically.")
				color: tokens.textSecondary
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
			Button {
				Layout.fillWidth: true
				text: qsTr("Continue with local log")
				onClicked: page.useLocalLog()
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Bring an existing Subsurface Cloud log"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text {
				text: qsTr("Use this only if you already use the original Subsurface Cloud account. Google Drive and Dropbox are connected from Neo after setup.")
				color: tokens.textSecondary
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
			Button {
				Layout.fillWidth: true
				text: qsTr("Use Subsurface Cloud")
				onClicked: page.openLegacyCloudSetup()
			}
		}

		Text {
			text: qsTr("Neo keeps your canonical Subsurface log compatible while providing a modern workspace for reviewing dives, planning, and safe cloud backups.")
			color: tokens.textMuted
			font.pixelSize: 13
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}
	}
}
