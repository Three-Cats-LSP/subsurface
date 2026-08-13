// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Statistics")
	background: Rectangle { color: tokens.background }
	Modern.DesignTokens { id: tokens }
	StatsManager { id: statsManager }
	ChartListModel { id: chartListModel }
	property bool controlsOpen: true
	property bool chartPickerOpen: false
	Component.onCompleted: statsManager.init(statsView, chartListModel)
	onVisibleChanged: if (visible) statsManager.doit()

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Analyze your real diving"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 3 : 1
			Components.MetricCard { label: qsTr("Dives"); value: String(NeoDashboard.diveCount); Layout.fillWidth: true }
			Components.MetricCard { label: qsTr("Dive time"); value: NeoDashboard.totalTimeHours; suffix: qsTr("hours"); Layout.fillWidth: true }
			Components.MetricCard { label: qsTr("Max depth"); value: NeoDashboard.maxDepth; suffix: NeoDashboard.maxDepthUnit; Layout.fillWidth: true }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout { Layout.fillWidth: true; Text { text: qsTr("Chart configuration"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
 Button { text: page.controlsOpen ? qsTr("Hide") : qsTr("Show"); onClicked: page.controlsOpen = !page.controlsOpen } }
			GridLayout {
				visible: page.controlsOpen; Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				Label { text: qsTr("Base variable"); color: tokens.textMuted }
				ComboBox { Layout.fillWidth: true; model: statsManager.var1List; currentIndex: statsManager.var1Index; onActivated: statsManager.var1Changed(currentIndex) }
				Label { text: qsTr("Base binning"); color: tokens.textMuted }
				ComboBox { Layout.fillWidth: true; model: statsManager.binner1List; currentIndex: statsManager.binner1Index; onActivated: statsManager.var1BinnerChanged(currentIndex) }
				Label { text: qsTr("Data"); color: tokens.textMuted }
				ComboBox { Layout.fillWidth: true; model: statsManager.var2List; currentIndex: statsManager.var2Index; onActivated: statsManager.var2Changed(currentIndex) }
				Label { text: qsTr("Data binning"); color: tokens.textMuted }
				ComboBox { Layout.fillWidth: true; model: statsManager.binner2List; currentIndex: statsManager.binner2Index; onActivated: statsManager.var2BinnerChanged(currentIndex) }
				Label { text: qsTr("Operation"); color: tokens.textMuted }
				ComboBox { Layout.fillWidth: true; model: statsManager.operation2List; currentIndex: statsManager.operation2Index; onActivated: statsManager.var2OperationChanged(currentIndex) }
				Label { text: qsTr("Sort"); color: tokens.textMuted }
				ComboBox { Layout.fillWidth: true; model: statsManager.sortMode1List; currentIndex: statsManager.sortMode1Index; onActivated: statsManager.sortMode1Changed(currentIndex) }
			}
			Button { text: qsTr("Choose chart type"); onClicked: page.chartPickerOpen = !page.chartPickerOpen }
			ListView { visible: page.chartPickerOpen; Layout.fillWidth: true; Layout.preferredHeight: Math.min(contentHeight, 260); clip: true; model: chartListModel; delegate: ItemDelegate { width: ListView.view.width; text: chartName; enabled: !isHeader; font.bold: isHeader; onClicked: { statsManager.setChart(id); page.chartPickerOpen = false } } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Live chart"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			StatsView { id: statsView; Layout.fillWidth: true; Layout.preferredHeight: Math.max(320, page.width * 0.55) }
		}
		Text { text: qsTr("All values and chart bins are calculated by Subsurface's existing statistics engine from the current log and filters."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
