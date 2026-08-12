// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Statistics")
	background: Rectangle { color: tokens.background }
	signal openStatistics()
	Modern.DesignTokens { id: tokens }
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Analyze your real diving"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 3 : 1; Components.MetricCard { label: qsTr("Dives"); value: String(NeoDashboard.diveCount); Layout.fillWidth: true }; Components.MetricCard { label: qsTr("Dive time"); value: NeoDashboard.totalTimeHours; suffix: qsTr("hours"); Layout.fillWidth: true }; Components.MetricCard { label: qsTr("Max depth"); value: NeoDashboard.maxDepth; suffix: NeoDashboard.maxDepthUnit; Layout.fillWidth: true } }
		Components.ModernCard { Layout.fillWidth: true; Text { text: qsTr("Detailed distributions"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }; Text { text: qsTr("Choose real Subsurface variables, bins, and chart types for depth, time, locations, gases, equipment, and dive modes."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }; Button { Layout.fillWidth: true; text: qsTr("Open detailed statistics"); onClicked: page.openStatistics() } }
	}
}
