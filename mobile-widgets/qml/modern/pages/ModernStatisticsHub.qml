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
	objectName: "ModernStatisticsHub"
	title: qsTr("Statistics")
	background: Rectangle { color: tokens.background }

	property bool wideLayout: width >= 760
	property bool controlsOpen: false
	property bool chartPickerOpen: false

	Modern.DesignTokens { id: tokens }
	StatsManager { id: statsManager }
	ChartListModel { id: chartListModel }

	Component.onCompleted: {
		statsManager.init(statsView, chartListModel)
		statsManager.setDarkThemeOverride(true)
	}
	onVisibleChanged: if (visible) statsManager.doit()
	onWidthChanged: if (visible) redrawTimer.restart()

	Timer {
		id: redrawTimer
		interval: 250
		repeat: false
		onTriggered: statsManager.doit()
	}

	function toggleChartPicker() {
		page.chartPickerOpen = !page.chartPickerOpen
		if (page.chartPickerOpen)
			page.controlsOpen = false
	}

	function toggleControls() {
		page.controlsOpen = !page.controlsOpen
		if (page.controlsOpen)
			page.chartPickerOpen = false
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16

		ColumnLayout {
			Layout.fillWidth: true
			spacing: 2
			Text {
				text: qsTr("Statistics")
				color: tokens.textPrimary
				font.pixelSize: page.wideLayout ? 30 : 25
				font.weight: Font.DemiBold
			}
			Text {
				text: qsTr("Explore patterns in your current dive log")
				color: tokens.textSecondary
				font.pixelSize: 13
			}
		}

		GridLayout {
			objectName: "NeoStatisticsMetrics"
			Layout.fillWidth: true
			columns: page.wideLayout ? 4 : 2
			columnSpacing: page.wideLayout ? tokens.space16 : tokens.space8
			rowSpacing: tokens.space8
			Components.MetricCard {
				label: qsTr("Dives")
				value: String(NeoDashboard.diveCount)
				iconName: "tank"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				label: qsTr("Dive time")
				value: NeoDashboard.totalTimeHours
				suffix: qsTr("h")
				iconName: "time"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				label: qsTr("Max depth")
				value: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepth : "—"
				suffix: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepthUnit : ""
				iconName: "depth"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				label: qsTr("Avg water")
				value: NeoDashboard.averageWaterTemp.length > 0 ? NeoDashboard.averageWaterTemp : "—"
				iconName: "temperature"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
		}

		Components.ModernCard {
			objectName: "NeoStatisticsChartCard"
			Layout.fillWidth: true
			contentPadding: tokens.space12

			RowLayout {
				Layout.fillWidth: true
				spacing: tokens.space8
				ColumnLayout {
					Layout.fillWidth: true
					spacing: 1
					Text {
						text: statsManager.chartTitle.length > 0 ? statsManager.chartTitle : qsTr("Dive activity")
						color: tokens.textPrimary
						font.pixelSize: 18
						font.weight: Font.DemiBold
					}
					Text {
						visible: page.wideLayout
						text: statsManager.chartSubtype.length > 0
							? qsTr("%1 · calculated from the current log and active filters").arg(statsManager.chartSubtype)
							: qsTr("Calculated from the current log and active filters")
						color: tokens.textMuted
						font.pixelSize: 10
					}
				}
				Components.NeoButton {
					text: page.wideLayout ? qsTr("Chart type") : qsTr("Chart")
					compact: true
					variant: page.chartPickerOpen ? "primary" : "secondary"
					onClicked: page.toggleChartPicker()
				}
				Components.NeoButton {
					text: page.wideLayout ? (page.controlsOpen ? qsTr("Hide controls") : qsTr("Configure")) : (page.controlsOpen ? qsTr("Hide") : qsTr("Options"))
					compact: true
					variant: page.controlsOpen ? "primary" : "secondary"
					onClicked: page.toggleControls()
				}
			}

			Rectangle {
				visible: page.chartPickerOpen
				Layout.fillWidth: true
				Layout.preferredHeight: Math.min(chartPicker.contentHeight + tokens.space8, page.wideLayout ? 280 : 220)
				color: tokens.background
				radius: tokens.radiusSmall
				border.width: 1
				border.color: tokens.border
				clip: true
				ListView {
					id: chartPicker
					anchors.fill: parent
					anchors.margins: tokens.space4
					clip: true
					model: chartListModel
					delegate: ItemDelegate {
						width: ListView.view.width
						height: isHeader ? 34 : 42
						enabled: !isHeader
						contentItem: Text {
							text: chartName
							color: isHeader ? tokens.accent : tokens.textPrimary
							font.pixelSize: isHeader ? 10 : 12
							font.weight: isHeader ? Font.DemiBold : Font.Normal
							verticalAlignment: Text.AlignVCenter
						}
						background: Rectangle { color: parent.down || parent.hovered ? tokens.surfaceRaised : "transparent"; radius: tokens.radiusSmall }
						onClicked: {
							statsManager.setChart(id)
							page.chartPickerOpen = false
						}
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true
				Layout.preferredHeight: page.wideLayout ? Math.max(420, page.width * 0.42) : Math.max(280, Math.min(360, page.width * 0.78))
				color: tokens.background
				radius: tokens.radiusSmall
				clip: true
				StatsView {
					id: statsView
					anchors.fill: parent
					anchors.margins: tokens.space8
				}
			}
		}

		Components.ModernCard {
			objectName: "NeoStatisticsControls"
			visible: page.controlsOpen
			Layout.fillWidth: true
			contentPadding: tokens.space12

			RowLayout {
				Layout.fillWidth: true
				Text { text: qsTr("Chart configuration"); color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true }
				Text { visible: page.wideLayout; text: qsTr("Native Subsurface statistics engine"); color: tokens.accent; font.pixelSize: 10 }
			}

			GridLayout {
				Layout.fillWidth: true
				columns: page.wideLayout ? 3 : 2
				columnSpacing: tokens.space12
				rowSpacing: tokens.space12

				ColumnLayout {
					Layout.fillWidth: true
					spacing: tokens.space4
					Text { text: qsTr("Base variable"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoComboBox { Layout.fillWidth: true; model: statsManager.var1List; currentIndex: statsManager.var1Index; onActivated: statsManager.var1Changed(currentIndex) }
				}
				ColumnLayout {
					Layout.fillWidth: true
					spacing: tokens.space4
					Text { text: qsTr("Base binning"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoComboBox { Layout.fillWidth: true; model: statsManager.binner1List; currentIndex: statsManager.binner1Index; onActivated: statsManager.var1BinnerChanged(currentIndex) }
				}
				ColumnLayout {
					Layout.fillWidth: true
					spacing: tokens.space4
					Text { text: qsTr("Sort"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoComboBox { Layout.fillWidth: true; model: statsManager.sortMode1List; currentIndex: statsManager.sortMode1Index; onActivated: statsManager.sortMode1Changed(currentIndex) }
				}
				ColumnLayout {
					Layout.fillWidth: true
					spacing: tokens.space4
					Text { text: qsTr("Data"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoComboBox { Layout.fillWidth: true; model: statsManager.var2List; currentIndex: statsManager.var2Index; onActivated: statsManager.var2Changed(currentIndex) }
				}
				ColumnLayout {
					Layout.fillWidth: true
					spacing: tokens.space4
					Text { text: qsTr("Data binning"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoComboBox { Layout.fillWidth: true; model: statsManager.binner2List; currentIndex: statsManager.binner2Index; onActivated: statsManager.var2BinnerChanged(currentIndex) }
				}
				ColumnLayout {
					Layout.fillWidth: true
					spacing: tokens.space4
					Text { text: qsTr("Operation"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoComboBox { Layout.fillWidth: true; model: statsManager.operation2List; currentIndex: statsManager.operation2Index; onActivated: statsManager.var2OperationChanged(currentIndex) }
				}
			}

			Components.NeoButton {
				visible: !page.wideLayout
				Layout.alignment: Qt.AlignRight
				text: qsTr("Done")
				variant: "primary"
				compact: true
				onClicked: page.controlsOpen = false
			}
		}

		Text {
			Layout.fillWidth: true
			text: qsTr("Charts remain interactive: select chart elements to restrict the current statistics view, using the established Subsurface behavior.")
			color: tokens.textSecondary
			font.pixelSize: 11
			wrapMode: Text.WordWrap
		}
	}
}
