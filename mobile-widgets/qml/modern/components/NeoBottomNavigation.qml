// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Modern

ToolBar {
	id: bar

	property string currentSection: "home"
	signal homeRequested()
	signal divesRequested()
	signal sitesRequested()
	signal statsRequested()
	signal moreRequested()

	Modern.DesignTokens { id: tokens }

	implicitHeight: 68
	padding: 0
	background: Rectangle {
		color: tokens.surface
		Rectangle {
			anchors.top: parent.top
			width: parent.width
			height: 1
			color: tokens.border
		}
	}

	RowLayout {
		anchors.fill: parent
		spacing: 0

		Repeater {
			model: [
				{ key: "home", label: qsTr("Home") },
				{ key: "dives", label: qsTr("Dives") },
				{ key: "sites", label: qsTr("Sites") },
				{ key: "stats", label: qsTr("Stats") },
				{ key: "more", label: qsTr("More") }
			]

			delegate: ItemDelegate {
				required property var modelData
				Layout.fillWidth: true
				Layout.fillHeight: true
				padding: 0
				hoverEnabled: true

				contentItem: Column {
					anchors.centerIn: parent
					spacing: 6

					Rectangle {
						anchors.horizontalCenter: parent.horizontalCenter
						width: modelData.key === bar.currentSection ? 24 : 6
						height: 3
						radius: 2
						color: modelData.key === bar.currentSection ? tokens.accent : "transparent"
						Behavior on width { NumberAnimation { duration: 120 } }
					}

					Text {
						anchors.horizontalCenter: parent.horizontalCenter
						text: modelData.label
						color: modelData.key === bar.currentSection ? tokens.textPrimary : tokens.textSecondary
						font.pixelSize: 12
						font.weight: modelData.key === bar.currentSection ? Font.DemiBold : Font.Medium
					}
				}

				background: Rectangle {
					color: parent.down ? tokens.surfaceRaised : "transparent"
				}

				onClicked: {
					switch (modelData.key) {
					case "home": bar.homeRequested(); break
					case "dives": bar.divesRequested(); break
					case "sites": bar.sitesRequested(); break
					case "stats": bar.statsRequested(); break
					case "more": bar.moreRequested(); break
					}
				}
			}
		}
	}
}
