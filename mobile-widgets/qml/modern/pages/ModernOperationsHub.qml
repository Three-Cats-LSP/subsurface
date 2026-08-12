// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Tools & library")
	background: Rectangle { color: tokens.background }
	signal openSites()
	signal openStatistics()
	signal openEquipment()
	signal openExport()
	signal openRecovery()
	Modern.DesignTokens { id: tokens }
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Your dive library"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Explore dive sites, analyze real log data, manage reusable equipment, and keep portable copies of your log."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Repeater {
			model: [
				{ title: qsTr("Dive sites & map"), detail: qsTr("Browse and edit the canonical Subsurface sites and their associated dives."), action: page.openSites },
				{ title: qsTr("Statistics"), detail: qsTr("Explore distributions and summary charts calculated from your real log."), action: page.openStatistics },
				{ title: qsTr("Equipment library"), detail: qsTr("Manage reusable Neo loadouts, defaults, and recent gear configurations."), action: page.openEquipment },
				{ title: qsTr("Export & portability"), detail: qsTr("Create Subsurface-compatible exports or recover a known cloud cache."), action: page.openExport }
			]
			delegate: Components.ModernCard {
				required property var modelData
				Layout.fillWidth: true
				Text { text: modelData.title; color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: modelData.detail; color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Button { Layout.fillWidth: true; text: qsTr("Open"); onClicked: modelData.action() }
			}
		}
	}
}
