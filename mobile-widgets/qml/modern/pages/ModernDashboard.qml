// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Home")
	background: Rectangle { color: tokens.background }

	signal openDiveList()
	signal openImport()
	signal openCloudSync()

	Modern.DesignTokens { id: tokens }

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space24

		ColumnLayout {
			spacing: tokens.space4
			Layout.fillWidth: true

			Text {
				text: qsTr("Your diving, at a glance")
				color: tokens.textPrimary
				font.pixelSize: 28
				font.weight: Font.DemiBold
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
			Text {
				text: qsTr("Subsurface Neo keeps the proven Subsurface dive engine underneath a faster, clearer mobile interface.")
				color: tokens.textSecondary
				font.pixelSize: 14
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.width >= 700 ? 4 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Components.MetricCard {
				label: qsTr("Dives")
				value: String(NeoDashboard.diveCount)
				Layout.fillWidth: true
			}
			Components.MetricCard {
				label: qsTr("Saved plans")
				value: String(NeoDashboard.planCount)
				Layout.fillWidth: true
			}
			Components.MetricCard {
				label: qsTr("Dive time")
				value: NeoDashboard.totalTimeHours
				suffix: qsTr("hours")
				Layout.fillWidth: true
			}
			Components.MetricCard {
				label: qsTr("Max depth")
				value: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepth : "—"
				suffix: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepthUnit : ""
				Layout.fillWidth: true
			}
		}

		ColumnLayout {
			visible: NeoDashboard.recentPlans.length > 0
			Layout.fillWidth: true
			spacing: tokens.space12
			Text { text: qsTr("Saved plans"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
			Repeater {
				model: NeoDashboard.recentPlans
				delegate: Components.ModernCard {
					required property var modelData
					Layout.fillWidth: true
					RowLayout {
						Layout.fillWidth: true
						Text {
							text: qsTr("Planned dive")
							color: tokens.textPrimary
							font.weight: Font.DemiBold
							Layout.fillWidth: true
						}
						Text {
							text: modelData.depth
							color: tokens.accent
							font.weight: Font.DemiBold
						}
					}
					RowLayout {
						Layout.fillWidth: true
						Text {
							text: modelData.date
							color: tokens.textSecondary
							Layout.fillWidth: true
						}
						Text {
							text: modelData.duration
							color: tokens.textSecondary
						}
					}
				}
			}
		}

		ColumnLayout {
			Layout.fillWidth: true
			spacing: tokens.space12

			Text {
				text: qsTr("Recent dives")
				color: tokens.textPrimary
				font.pixelSize: 18
				font.weight: Font.DemiBold
				Layout.fillWidth: true
			}

			Text {
				visible: NeoDashboard.recentDives.length === 0
				text: qsTr("No dives yet. Import from a dive computer or add a dive to get started.")
				color: tokens.textSecondary
				font.pixelSize: 14
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}

			Repeater {
				model: NeoDashboard.recentDives

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
								text: modelData.location && modelData.location.length > 0 ? modelData.location : qsTr("Dive %1").arg(modelData.number > 0 ? modelData.number : "")
								color: tokens.textPrimary
								font.pixelSize: 16
								font.weight: Font.DemiBold
								elide: Text.ElideRight
								Layout.fillWidth: true
							}
							Text {
								text: modelData.date
								color: tokens.textSecondary
								font.pixelSize: 12
								Layout.fillWidth: true
							}
						}

						Text {
							text: modelData.depth
							color: tokens.accent
							font.pixelSize: 16
							font.weight: Font.DemiBold
						}
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space16

						Text {
							text: modelData.duration
							color: tokens.textSecondary
							font.pixelSize: 13
						}
						Text {
							visible: modelData.waterTemp !== undefined && modelData.waterTemp.length > 0
							text: modelData.waterTemp || ""
							color: tokens.textSecondary
							font.pixelSize: 13
						}
						Item { Layout.fillWidth: true }
					}
				}
			}
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.width >= 700 ? 3 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Button {
				text: qsTr("Open dive list")
				Layout.fillWidth: true
				onClicked: page.openDiveList()
			}
			Button {
				text: qsTr("Import dives")
				Layout.fillWidth: true
				onClicked: page.openImport()
			}
			Button {
				text: qsTr("Cloud & Sync")
				Layout.fillWidth: true
				onClicked: page.openCloudSync()
			}
		}
	}
}
