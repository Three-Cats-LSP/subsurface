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
	title: qsTr("Home")
	background: Rectangle { color: tokens.background }

	property bool wideLayout: width >= 760
	signal openDive(int diveId)
	signal openDiveList()
	signal openImport()
	signal openCloudSync()

	Modern.DesignTokens { id: tokens }

	function greeting() {
		var hour = new Date().getHours()
		if (hour < 12)
			return qsTr("Good morning")
		if (hour < 18)
			return qsTr("Good afternoon")
		return qsTr("Good evening")
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space24

		RowLayout {
			Layout.fillWidth: true
			spacing: tokens.space12

			ColumnLayout {
				Layout.fillWidth: true
				spacing: tokens.space4
				Text {
					text: page.greeting()
					color: tokens.textPrimary
					font.pixelSize: page.wideLayout ? 30 : 25
					font.weight: Font.DemiBold
				}
				Text {
					text: qsTr("Here’s your diving summary")
					color: tokens.textSecondary
					font.pixelSize: 13
				}
			}

			Components.NeoButton {
				visible: page.wideLayout
				text: qsTr("Import")
				variant: "primary"
				onClicked: page.openImport()
			}
			ToolButton {
				Accessible.name: qsTr("Cloud & Sync")
				contentItem: Components.NeoDiveIcon { name: "cloud"; iconColor: tokens.accent; width: 24; height: 24 }
				ToolTip.visible: hovered
				ToolTip.text: qsTr("Cloud & Sync")
				onClicked: page.openCloudSync()
			}
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.wideLayout ? 4 : 3
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
				visible: page.wideLayout
				label: qsTr("Avg water")
				value: NeoDashboard.averageWaterTemp.length > 0 ? NeoDashboard.averageWaterTemp : "—"
				iconName: "temperature"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
		}

		RowLayout {
			Layout.fillWidth: true
			Text {
				text: qsTr("Recent dives")
				color: tokens.textPrimary
				font.pixelSize: 19
				font.weight: Font.DemiBold
				Layout.fillWidth: true
			}
			Components.NeoButton {
				text: qsTr("See all")
				variant: "ghost"
				compact: true
				onClicked: page.openDiveList()
			}
		}

		Text {
			visible: NeoDashboard.recentDives.length === 0
			text: qsTr("No dives yet. Import from a dive computer or add a dive to get started.")
			color: tokens.textSecondary
			font.pixelSize: 14
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}
		Components.NeoButton {
			visible: NeoDashboard.recentDives.length === 0
			text: qsTr("Import dives")
			variant: "primary"
			Layout.alignment: Qt.AlignLeft
			onClicked: page.openImport()
		}

		GridLayout {
			visible: NeoDashboard.recentDives.length > 0
			Layout.fillWidth: true
			columns: page.wideLayout ? 2 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Repeater {
				model: NeoDashboard.recentDives

				delegate: Components.ModernCard {
					id: recentCard
					required property var modelData
					Layout.fillWidth: true
					contentPadding: tokens.space12

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space8

						Rectangle {
							Layout.preferredWidth: 54
							Layout.preferredHeight: 40
							color: "transparent"
							radius: tokens.radiusSmall
							border.width: 1
							border.color: tokens.accentStrong
							Text {
								anchors.centerIn: parent
								text: recentCard.modelData.number > 0 ? "#" + recentCard.modelData.number : qsTr("Dive")
								color: tokens.accent
								font.pixelSize: 15
								font.weight: Font.DemiBold
							}
						}

						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2
							Text {
								Layout.fillWidth: true
								text: recentCard.modelData.location && recentCard.modelData.location.length > 0 ? recentCard.modelData.location : qsTr("Unnamed dive site")
								color: tokens.textPrimary
								font.pixelSize: 16
								font.weight: Font.DemiBold
								elide: Text.ElideRight
							}
							Text {
								text: recentCard.modelData.date
								color: tokens.textSecondary
								font.pixelSize: 11
							}
						}
					}

					GridLayout {
						Layout.fillWidth: true
						columns: 3
						columnSpacing: tokens.space8
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 1
							Text { text: qsTr("MAX DEPTH"); color: tokens.textMuted; font.pixelSize: 8 }
							Text { text: recentCard.modelData.depth || "—"; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
						}
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 1
							Text { text: qsTr("DIVE TIME"); color: tokens.textMuted; font.pixelSize: 8 }
							Text { text: recentCard.modelData.duration || "—"; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
						}
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 1
							Text { text: qsTr("WATER TEMP"); color: tokens.textMuted; font.pixelSize: 8 }
							Text { text: recentCard.modelData.waterTemp || "—"; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
						}
					}

					Rectangle {
						Layout.fillWidth: true
						Layout.preferredHeight: page.wideLayout ? 112 : 124
						color: tokens.background
						radius: tokens.radiusSmall
						clip: true

						QMLProfile {
							id: miniProfile
							anchors.fill: parent
							diveId: recentCard.modelData.id
							Component.onCompleted: setMargin(3)
						}
					}

					TapHandler { onTapped: page.openDive(recentCard.modelData.id) }
				}
			}
		}

		ColumnLayout {
			visible: NeoDashboard.recentPlans.length > 0
			Layout.fillWidth: true
			spacing: tokens.space8
			Text {
				text: qsTr("Saved plans (%1)").arg(NeoDashboard.planCount)
				color: tokens.textPrimary
				font.pixelSize: 17
				font.weight: Font.DemiBold
			}
			Repeater {
				model: NeoDashboard.recentPlans
				delegate: Components.ModernCard {
					required property var modelData
					Layout.fillWidth: true
					contentPadding: tokens.space12
					RowLayout {
						Layout.fillWidth: true
						Text { text: qsTr("Planned dive"); color: tokens.textPrimary; font.weight: Font.DemiBold; Layout.fillWidth: true }
						Text { text: modelData.depth; color: tokens.accent; font.weight: Font.DemiBold }
						Text { text: modelData.duration; color: tokens.textSecondary }
					}
				}
			}
		}

	}
}
