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
			columns: page.width >= 700 ? 3 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Components.MetricCard {
				label: qsTr("Dives")
				value: "—"
				Layout.fillWidth: true
			}
			Components.MetricCard {
				label: qsTr("Dive time")
				value: "—"
				suffix: qsTr("hours")
				Layout.fillWidth: true
			}
			Components.MetricCard {
				label: qsTr("Max depth")
				value: "—"
				suffix: "m"
				Layout.fillWidth: true
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true

			Text {
				text: qsTr("Recent dives")
				color: tokens.textPrimary
				font.pixelSize: 18
				font.weight: Font.DemiBold
				Layout.fillWidth: true
			}
			Text {
				text: qsTr("Dive metrics and recent-dive cards are being connected directly to the existing Subsurface models. No placeholder dive data is generated.")
				color: tokens.textSecondary
				font.pixelSize: 14
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
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
