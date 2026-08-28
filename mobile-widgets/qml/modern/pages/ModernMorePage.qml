// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("More")
	background: Rectangle { color: tokens.background }

	signal openPlanner()
	signal openCloudSync()
	signal openImport()
	signal openEquipment()
	signal openPortability()
	signal openSettings()
	signal openAbout()
	property bool wideLayout: width >= 760

	Modern.DesignTokens { id: tokens }

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16

		Text { text: qsTr("Tools & settings"); color: tokens.textPrimary; font.pixelSize: 28; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Plan dives, manage devices and equipment, protect your log, and access advanced Subsurface tools when needed."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

		GridLayout {
			Layout.fillWidth: true
			columns: page.wideLayout ? 2 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12
			Repeater {
				model: [
				{ title: qsTr("Dive planner"), detail: qsTr("Build a plan with Subsurface’s established decompression engine."), icon: "depth", action: page.openPlanner },
				{ title: qsTr("Cloud & Sync"), detail: qsTr("Connect Google Drive or Dropbox, choose primary and backup providers, and sync safely."), icon: "cloud", action: page.openCloudSync },
				{ title: qsTr("Download from dive computer"), detail: qsTr("Use the mature Subsurface import engine for Bluetooth, USB, and serial devices."), icon: "device", action: page.openImport },
				{ title: qsTr("Equipment library"), detail: qsTr("Maintain reusable kits, cylinders, and recent gear configurations."), icon: "gear", action: page.openEquipment },
				{ title: qsTr("Export & backup"), detail: qsTr("Create compatible exports and local backup packages."), icon: "export", action: page.openPortability },
				{ title: qsTr("Settings"), detail: qsTr("Configure equipment defaults, devices, interface, and profile display."), icon: "settings", action: page.openSettings }
				]
				delegate: Components.ModernCard {
					required property var modelData
					Layout.fillWidth: true
					Layout.alignment: Qt.AlignTop
					RowLayout {
						Layout.fillWidth: true; spacing: tokens.space12
						Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 42; radius: 21; color: tokens.surfaceRaised; Components.NeoDiveIcon { anchors.centerIn: parent; width: 24; height: 24; name: modelData.icon; iconColor: tokens.accent } }
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2
							Text {
								text: modelData.title
								color: tokens.textPrimary
								font.pixelSize: 17
								font.weight: Font.DemiBold
								Layout.fillWidth: true
								elide: Text.ElideRight
							}
							Text {
								text: modelData.detail
								color: tokens.textSecondary
								font.pixelSize: 12
								wrapMode: Text.WordWrap
								Layout.fillWidth: true
							}
						}
					}
					Components.NeoButton { Layout.alignment: Qt.AlignRight; text: qsTr("Open"); variant: "ghost"; compact: true; onClicked: modelData.action() }
				}
			}
		}
		Components.NeoButton { Layout.fillWidth: true; text: qsTr("About Subsurface Neo"); onClicked: page.openAbout() }
	}
}
