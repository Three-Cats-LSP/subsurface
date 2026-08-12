// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Dive sites")
	background: Rectangle { color: tokens.background }
	signal openMap()
	Modern.DesignTokens { id: tokens }
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Sites, locations, and maps"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Your log currently contains %1 saved locations.").arg(manager.locationList.length); color: tokens.textSecondary; Layout.fillWidth: true }
		Components.ModernCard { Layout.fillWidth: true; Text { text: qsTr("Explore dive sites"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }; Text { text: qsTr("Use the established Subsurface map and site model to view markers, associated dives, coordinates, and location details."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }; Button { Layout.fillWidth: true; text: qsTr("Open map"); onClicked: page.openMap() } }
		Text { text: qsTr("Site edits remain stored in canonical Subsurface site data so they appear consistently in every compatible client."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
