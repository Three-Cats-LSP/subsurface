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
	title: qsTr("Settings")
	background: Rectangle { color: tokens.background }
	property bool wideLayout: width >= 760
	signal openCloudSync()
	signal openSubsurfaceCloud()
	signal openImport()
	signal openAdvancedSettings()
	signal openAbout()

	Modern.DesignTokens { id: tokens }

	function forgetComputers() {
		PrefDiveComputer.vendor1 = PrefDiveComputer.product1 = PrefDiveComputer.device1 = ""
		PrefDiveComputer.vendor2 = PrefDiveComputer.product2 = PrefDiveComputer.device2 = ""
		PrefDiveComputer.vendor3 = PrefDiveComputer.product3 = PrefDiveComputer.device3 = ""
		PrefDiveComputer.vendor4 = PrefDiveComputer.product4 = PrefDiveComputer.device4 = ""
		PrefDiveComputer.vendor = PrefDiveComputer.product = PrefDiveComputer.device = ""
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16

		ColumnLayout {
			Layout.fillWidth: true
			spacing: 2
			Text { text: qsTr("Settings"); color: tokens.textPrimary; font.pixelSize: page.wideLayout ? 30 : 25; font.weight: Font.DemiBold }
			Text { text: qsTr("Configure Neo while keeping advanced Subsurface options available"); color: tokens.textSecondary; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.wideLayout ? 2 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Cloud & Sync"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: qsTr("Google Drive, Dropbox, conflicts, and backups"); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Button { text: qsTr("Manage cloud providers"); Layout.fillWidth: true; onClicked: page.openCloudSync() }
				Button {
					text: Backend.cloud_verification_status === Enums.CS_VERIFIED ? qsTr("Subsurface Cloud: connected") : qsTr("Subsurface Cloud compatibility")
					Layout.fillWidth: true
					onClicked: page.openSubsurfaceCloud()
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Default equipment"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: qsTr("Cylinder used when creating a new dive"); color: tokens.textSecondary; font.pixelSize: 12 }
				Components.NeoComboBox {
					id: defaultCylinderBox
					Layout.fillWidth: true
					model: manager.defaultCylinderListInit
					Component.onCompleted: currentIndex = PrefEquipment.default_cylinder === "" ? 0 : Math.max(0, find(PrefEquipment.default_cylinder))
					onActivated: PrefEquipment.default_cylinder = currentIndex === 0 ? "" : currentText
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Dive computers"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text {
					text: PrefDiveComputer.vendor1 !== "" ? qsTr("Remembered: %1 %2").arg(PrefDiveComputer.vendor1).arg(PrefDiveComputer.product1) : qsTr("No remembered dive computer")
					color: tokens.textSecondary
					font.pixelSize: 12
					wrapMode: Text.WordWrap
					Layout.fillWidth: true
				}
				RowLayout {
					Layout.fillWidth: true
					Button { text: qsTr("Manage"); onClicked: page.openImport() }
					Item { Layout.fillWidth: true }
					Button { text: qsTr("Forget"); enabled: PrefDiveComputer.vendor1 !== ""; onClicked: page.forgetComputers() }
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Interface"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Switch { Layout.fillWidth: true; text: qsTr("Single-column portrait layout"); checked: PrefDisplay.singleColumnPortrait; onToggled: PrefDisplay.singleColumnPortrait = checked }
				Switch { Layout.fillWidth: true; text: qsTr("Three-metre profile grid"); checked: PrefDisplay.three_m_based_grid; onToggled: PrefDisplay.three_m_based_grid = checked }
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Profile & decompression display"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("These settings affect analysis presentation; Subsurface’s decompression calculations remain unchanged."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			GridLayout {
				Layout.fillWidth: true
				columns: page.wideLayout ? 2 : 1
				columnSpacing: tokens.space16
				Switch { Layout.fillWidth: true; text: qsTr("Dive-computer ceiling"); checked: PrefTechnicalDetails.dcceiling; onToggled: PrefTechnicalDetails.dcceiling = checked }
				Switch { Layout.fillWidth: true; text: qsTr("Calculated ceiling"); checked: PrefTechnicalDetails.calcceiling; onToggled: PrefTechnicalDetails.calcceiling = checked }
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("GF low"); color: tokens.textMuted; font.pixelSize: 10 }
					SpinBox { Layout.fillWidth: true; from: 0; to: 100; value: PrefTechnicalDetails.gflow; onValueModified: PrefTechnicalDetails.gflow = value }
				}
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("GF high"); color: tokens.textMuted; font.pixelSize: 10 }
					SpinBox { Layout.fillWidth: true; from: 0; to: 100; value: PrefTechnicalDetails.gfhigh; onValueModified: PrefTechnicalDetails.gfhigh = value }
				}
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout {
				Layout.fillWidth: true
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("Advanced Subsurface settings"); color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
					Text { text: qsTr("Compatibility options not yet represented in the Neo workspace"); color: tokens.textSecondary; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
				Button { text: qsTr("Open"); onClicked: page.openAdvancedSettings() }
			}
		}

		Button { Layout.fillWidth: true; text: qsTr("About Subsurface Neo"); onClicked: page.openAbout() }
	}
}
