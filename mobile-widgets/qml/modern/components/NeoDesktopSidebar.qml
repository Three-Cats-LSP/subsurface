// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Modern

Control {
	id: sidebar
	objectName: "neoDesktopSidebar"

	property string currentSection: "dives"
	property string accountText: ""
	property string statusText: ""

	signal divesRequested()
	signal sitesRequested()
	signal mapRequested()
	signal statisticsRequested()
	signal equipmentRequested()
	signal plannerRequested()
	signal importRequested()
	signal portabilityRequested()
	signal settingsRequested()
	signal cloudRequested()

	Modern.DesignTokens { id: tokens }

	implicitWidth: 232
	padding: 0

	background: Rectangle {
		color: tokens.background

		Rectangle {
			anchors.right: parent.right
			width: 1
			height: parent.height
			color: tokens.border
		}
	}

	contentItem: ColumnLayout {
		spacing: 0

		ItemDelegate {
			id: brandButton
			objectName: "neoSidebarDashboardBrand"
			Layout.fillWidth: true
			Layout.preferredHeight: 84
			leftPadding: 22
			rightPadding: 18
			hoverEnabled: true
			activeFocusOnTab: true
			Accessible.name: qsTr("Open dives")

			contentItem: RowLayout {
				spacing: 12

				Image {
					Layout.preferredWidth: 38
					Layout.preferredHeight: 38
					source: "qrc:/qml/subsurface-neo-icon.svg"
					fillMode: Image.PreserveAspectFit
				}

				ColumnLayout {
					spacing: 1

					Text {
						text: "SUBSURFACE"
						color: tokens.textPrimary
						font.pixelSize: 13
						font.weight: Font.DemiBold
						font.letterSpacing: 2.8
					}

					Text {
						text: "NEO"
						color: tokens.accent
						font.pixelSize: 10
						font.weight: Font.DemiBold
						font.letterSpacing: 2.2
					}
				}
			}

			background: Rectangle {
				color: brandButton.down ? tokens.surfaceRaised :
					(brandButton.hovered ? tokens.surface : "transparent")
			}

			onClicked: sidebar.divesRequested()
		}

		ColumnLayout {
			Layout.fillWidth: true
			Layout.leftMargin: 12
			Layout.rightMargin: 12
			spacing: 4

			Repeater {
				model: [
					{ key: "dives", label: qsTr("Dives"), icon: "dives" },
					{ key: "sites", label: qsTr("Dive Sites"), icon: "site" },
					{ key: "map", label: qsTr("Map"), icon: "map" },
					{ key: "statistics", label: qsTr("Statistics"), icon: "stats" },
					{ key: "equipment", label: qsTr("Equipment"), icon: "gear" },
					{ key: "planner", label: qsTr("Dive Planner"), icon: "planner" },
					{ key: "import", label: qsTr("Import"), icon: "diveComputer" },
					{ key: "portability", label: qsTr("Data & Backup"), icon: "export" },
					{ key: "settings", label: qsTr("Settings"), icon: "settings" }
				]

				delegate: ItemDelegate {
					id: navigationButton
					required property var modelData
					readonly property bool selected: modelData.key === sidebar.currentSection
					objectName: "neoSidebar-" + modelData.key
					Layout.fillWidth: true
					Layout.preferredHeight: 48
					leftPadding: 14
					rightPadding: 12
					hoverEnabled: true
					activeFocusOnTab: true
					Accessible.name: modelData.label
					Accessible.description: selected ? qsTr("Current section") : ""

					contentItem: RowLayout {
						spacing: 13

						NeoDiveIcon {
							Layout.preferredWidth: 22
							Layout.preferredHeight: 22
							name: navigationButton.modelData.icon
							iconColor: navigationButton.selected ? tokens.accent : tokens.textSecondary
						}

						Text {
							Layout.fillWidth: true
							text: navigationButton.modelData.label
							color: navigationButton.selected ? tokens.textPrimary : tokens.textSecondary
							font.pixelSize: 14
							font.weight: navigationButton.selected ? Font.DemiBold : Font.Medium
						}
					}

					background: Rectangle {
						radius: tokens.radiusSmall
						color: navigationButton.selected ? tokens.surfaceRaised :
							(navigationButton.down ? tokens.surfaceRaised :
							 (navigationButton.hovered ? tokens.surface : "transparent"))

						Rectangle {
							visible: navigationButton.selected
							anchors.left: parent.left
							anchors.verticalCenter: parent.verticalCenter
							width: 3
							height: 26
							radius: 2
							color: tokens.accent
						}
					}

					onClicked: {
						switch (modelData.key) {
						case "dives": sidebar.divesRequested(); break
						case "sites": sidebar.sitesRequested(); break
						case "map": sidebar.mapRequested(); break
						case "statistics": sidebar.statisticsRequested(); break
						case "equipment": sidebar.equipmentRequested(); break
						case "planner": sidebar.plannerRequested(); break
						case "import": sidebar.importRequested(); break
						case "portability": sidebar.portabilityRequested(); break
						case "settings": sidebar.settingsRequested(); break
						}
					}
				}
			}
		}

		Item { Layout.fillHeight: true }

		Rectangle {
			id: cloudCard
			Layout.fillWidth: true
			Layout.leftMargin: 12
			Layout.rightMargin: 12
			Layout.bottomMargin: 14
			Layout.preferredHeight: accountText.length > 0 ? 72 : 60
			radius: tokens.radiusSmall
			color: tokens.surface
			border.width: 1
			border.color: tokens.border
			Accessible.role: Accessible.Button
			Accessible.name: qsTr("Open Cloud & Sync")
			activeFocusOnTab: true
			Keys.onReturnPressed: sidebar.cloudRequested()
			Keys.onEnterPressed: sidebar.cloudRequested()
			Keys.onSpacePressed: sidebar.cloudRequested()
			TapHandler { onTapped: sidebar.cloudRequested() }

			RowLayout {
				anchors.fill: parent
				anchors.margins: 12
				spacing: 10

				Rectangle {
					Layout.preferredWidth: 34
					Layout.preferredHeight: 34
					radius: 17
					color: tokens.surfaceRaised

					NeoDiveIcon {
						anchors.centerIn: parent
						width: 20
						height: 20
						name: "cloud"
						iconColor: tokens.accent
					}
				}

				ColumnLayout {
					Layout.fillWidth: true
					spacing: 2

					Text {
						Layout.fillWidth: true
						text: sidebar.accountText.length > 0 ? sidebar.accountText : qsTr("Local dive log")
						color: tokens.textPrimary
						font.pixelSize: 12
						font.weight: Font.Medium
						elide: Text.ElideRight
					}

					Text {
						Layout.fillWidth: true
						text: sidebar.statusText.length > 0 ? sidebar.statusText : qsTr("Stored on this device")
						color: tokens.textMuted
						font.pixelSize: 10
						elide: Text.ElideRight
					}
				}
			}
		}
	}
}
