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
	objectName: "ModernDashboard"
	title: qsTr("Home")
	background: Rectangle {
		color: tokens.background
		Rectangle {
			width: parent.width; height: 280
			gradient: Gradient {
				GradientStop { position: 0; color: tokens.surfaceRaised }
				GradientStop { position: 1; color: tokens.background }
			}
		}
	}

	property bool wideLayout: width >= 760
	readonly property int maxMonthlyCount: {
		var maximum = 1
		for (var i = 0; i < NeoDashboard.monthlyActivity.length; ++i)
			maximum = Math.max(maximum, NeoDashboard.monthlyActivity[i])
		return maximum
	}
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
		width: Math.min(page.availableWidth, 1450)
		anchors.horizontalCenter: parent.horizontalCenter
		spacing: tokens.space24

		RowLayout {
			Layout.fillWidth: true
			spacing: tokens.space12

			ColumnLayout {
				Layout.fillWidth: true
				spacing: tokens.space4
				Text { text: qsTr("YOUR DIVING"); color: tokens.accent; font.pixelSize: 11; font.letterSpacing: 1.5 }
				Text {
					text: page.greeting()
					color: tokens.textPrimary
					font.pixelSize: page.wideLayout ? 40 : 28
					font.weight: Font.Medium
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
			columns: page.width >= 600 ? 4 : 2
			columnSpacing: page.wideLayout ? tokens.space16 : tokens.space8
			rowSpacing: tokens.space8

			Components.MetricCard {
				showcase: true
				label: qsTr("Dives")
				value: String(NeoDashboard.diveCount)
				iconName: "dives"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				showcase: true
				label: qsTr("Dive time")
				value: NeoDashboard.totalTimeHours
				suffix: qsTr("h")
				iconName: "time"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				showcase: true
				label: qsTr("Max depth")
				value: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepth : "—"
				suffix: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepthUnit : ""
				iconName: "depth"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				showcase: true
				label: qsTr("Coldest / Warmest Temp")
				value: NeoDashboard.waterTemperatureRange.length > 0 ? NeoDashboard.waterTemperatureRange : "—"
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
					activeFocusOnTab: true
					Accessible.role: Accessible.Button
					Accessible.name: qsTr("Open dive %1: %2").arg(modelData.number).arg(modelData.location || qsTr("Unnamed dive site"))
					Accessible.onPressAction: page.openDive(modelData.id)
					Keys.onReturnPressed: page.openDive(modelData.id)
					Keys.onSpacePressed: page.openDive(modelData.id)

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space8

						Rectangle {
							Layout.preferredWidth: 54
							Layout.preferredHeight: 40
							color: tokens.surfaceRaised
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
								Layout.fillWidth: true
								elide: Text.ElideRight
								color: tokens.textSecondary
								font.pixelSize: 11
							}
						}
						Rectangle {
							Layout.preferredWidth: 52
							Layout.preferredHeight: 62
							color: tokens.background
							radius: 8
							border.color: tokens.border
							Column {
								anchors.centerIn: parent
								Text { anchors.horizontalCenter: parent.horizontalCenter; text: recentCard.modelData.month; color: tokens.accent; font.pixelSize: 9 }
								Text { anchors.horizontalCenter: parent.horizontalCenter; text: recentCard.modelData.day; color: tokens.accent; font.pixelSize: 23; font.weight: Font.DemiBold }
								Text { anchors.horizontalCenter: parent.horizontalCenter; text: recentCard.modelData.year; color: tokens.textMuted; font.pixelSize: 9 }
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

						Components.MiniDiveProfile {
							anchors.fill: parent
							samples: recentCard.modelData.profile || []
						}
						Text {
							visible: !recentCard.modelData.profile || recentCard.modelData.profile.length < 2
							anchors.centerIn: parent
							text: qsTr("No recorded depth samples")
							color: tokens.textMuted; font.pixelSize: 12
						}
					}

					TapHandler { onTapped: page.openDive(recentCard.modelData.id) }
				}
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Dive activity — %1").arg(NeoDashboard.activityYear); color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
			Text { text: qsTr("Number of dives per month"); color: tokens.textSecondary; font.pixelSize: 12 }
			RowLayout {
				Layout.fillWidth: true
				spacing: 4
				Repeater {
					model: NeoDashboard.monthlyActivity
					delegate: ColumnLayout {
						required property int index
						required property var modelData
						Layout.fillWidth: true
						Layout.minimumWidth: 0
						Accessible.name: Qt.locale().monthName(index, Locale.ShortFormat) + ": " + modelData
						Item {
							Layout.fillWidth: true; Layout.preferredHeight: 100
							Rectangle {
								anchors.bottom: parent.bottom; width: Math.max(2, parent.width - 4)
								height: modelData > 0 ? 8 + 92 * modelData / page.maxMonthlyCount : 2
								radius: 3; color: modelData > 0 ? tokens.accentStrong : tokens.border
							}
						}
						Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: modelData; color: tokens.textPrimary; font.pixelSize: 10 }
						Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: Qt.locale().monthName(index, Locale.ShortFormat); color: tokens.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
					}
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
					activeFocusOnTab: true
					Accessible.role: Accessible.Button
					Accessible.name: qsTr("Open saved plan")
					Keys.onReturnPressed: rootItem.openNeoDiveDetails(modelData.id, false, true)
					Accessible.onPressAction: rootItem.openNeoDiveDetails(modelData.id, false, true)
					TapHandler { onTapped: rootItem.openNeoDiveDetails(modelData.id, false, true) }
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
