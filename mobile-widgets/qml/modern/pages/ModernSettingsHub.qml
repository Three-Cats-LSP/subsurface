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
	signal openAccountSecurity()
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

	function saveUnitChange() {
		manager.changesNeedSaving()
		manager.refreshDiveList()
	}

	function fontScaleIndex() {
		var scales = [0.75, 0.85, 1.0, 1.15, 1.3]
		var closest = 0
		for (var i = 1; i < scales.length; ++i)
			if (Math.abs(scales[i] - subsurfaceTheme.currentScale) < Math.abs(scales[closest] - subsurfaceTheme.currentScale))
				closest = i
		return closest
	}

	function setFontScale(index) {
		var scales = [0.75, 0.85, 1.0, 1.15, 1.3]
		subsurfaceTheme.currentScale = scales[index]
		rootItem.setupUnits()
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
				Components.NeoButton { text: qsTr("Manage cloud providers"); Layout.fillWidth: true; variant: "primary"; onClicked: page.openCloudSync() }
				Components.NeoButton {
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
					Components.NeoButton { text: qsTr("Manage"); compact: true; onClicked: page.openImport() }
					Item { Layout.fillWidth: true }
					Components.NeoButton { text: qsTr("Forget"); variant: "danger"; compact: true; enabled: PrefDiveComputer.vendor1 !== ""; onClicked: page.forgetComputers() }
				}
				Components.NeoSwitch {
					Layout.fillWidth: true
					text: qsTr("Show unrecognized Bluetooth devices")
					checked: manager.showNonDiveComputers
					onToggled: manager.showNonDiveComputers = checked
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Interface"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Components.NeoSwitch { Layout.fillWidth: true; text: qsTr("Single-column portrait layout"); checked: PrefDisplay.singleColumnPortrait; onToggled: PrefDisplay.singleColumnPortrait = checked }
				Components.NeoSwitch { Layout.fillWidth: true; text: qsTr("Three-metre profile grid"); checked: PrefDisplay.three_m_based_grid; onToggled: PrefDisplay.three_m_based_grid = checked }
				GridLayout {
					Layout.fillWidth: true
					columns: 2
					Text { text: qsTr("Text size"); color: tokens.textSecondary; font.pixelSize: 12 }
					Components.NeoComboBox {
						Layout.fillWidth: true
						model: [qsTr("Very small"), qsTr("Small"), qsTr("Regular"), qsTr("Large"), qsTr("Very large")]
						currentIndex: page.fontScaleIndex()
						onActivated: page.setFontScale(currentIndex)
					}
				}
			}
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.wideLayout ? 2 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Units"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: qsTr("Choose the measurement system used throughout Neo"); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				ButtonGroup { id: unitSystemGroup }
				GridLayout {
					Layout.fillWidth: true
					columns: 3
					Components.NeoRadioButton { Layout.fillWidth: true; text: qsTr("Metric"); checked: Backend.unit_system === Enums.METRIC; ButtonGroup.group: unitSystemGroup; onClicked: { Backend.unit_system = Enums.METRIC; page.saveUnitChange() } }
					Components.NeoRadioButton { Layout.fillWidth: true; text: qsTr("Imperial"); checked: Backend.unit_system === Enums.IMPERIAL; ButtonGroup.group: unitSystemGroup; onClicked: { Backend.unit_system = Enums.IMPERIAL; page.saveUnitChange() } }
					Components.NeoRadioButton { Layout.fillWidth: true; text: qsTr("Personalize"); checked: Backend.unit_system === Enums.PERSONALIZE; ButtonGroup.group: unitSystemGroup; onClicked: { Backend.unit_system = Enums.PERSONALIZE; page.saveUnitChange() } }
				}
				GridLayout {
					visible: Backend.unit_system === Enums.PERSONALIZE
					Layout.fillWidth: true
					columns: 2
					columnSpacing: tokens.space12
					rowSpacing: tokens.space8

					Text { text: qsTr("Depth"); color: tokens.textSecondary; font.pixelSize: 12 }
					Components.NeoComboBox { Layout.fillWidth: true; model: [qsTr("Meters"), qsTr("Feet")]; currentIndex: Backend.length === Enums.METERS ? 0 : 1; onActivated: { Backend.length = currentIndex === 0 ? Enums.METERS : Enums.FEET; page.saveUnitChange() } }
					Text { text: qsTr("Pressure"); color: tokens.textSecondary; font.pixelSize: 12 }
					Components.NeoComboBox { Layout.fillWidth: true; model: [qsTr("Bar"), qsTr("PSI")]; currentIndex: Backend.pressure === Enums.BAR ? 0 : 1; onActivated: { Backend.pressure = currentIndex === 0 ? Enums.BAR : Enums.PSI; page.saveUnitChange() } }
					Text { text: qsTr("Volume"); color: tokens.textSecondary; font.pixelSize: 12 }
					Components.NeoComboBox { Layout.fillWidth: true; model: [qsTr("Litres"), qsTr("Cubic feet")]; currentIndex: Backend.volume === Enums.LITER ? 0 : 1; onActivated: { Backend.volume = currentIndex === 0 ? Enums.LITER : Enums.CUFT; page.saveUnitChange() } }
					Text { text: qsTr("Temperature"); color: tokens.textSecondary; font.pixelSize: 12 }
					Components.NeoComboBox { Layout.fillWidth: true; model: [qsTr("Celsius"), qsTr("Fahrenheit")]; currentIndex: Backend.temperature === Enums.CELSIUS ? 0 : 1; onActivated: { Backend.temperature = currentIndex === 0 ? Enums.CELSIUS : Enums.FAHRENHEIT; page.saveUnitChange() } }
					Text { text: qsTr("Weight"); color: tokens.textSecondary; font.pixelSize: 12 }
					Components.NeoComboBox { Layout.fillWidth: true; model: [qsTr("Kilograms"), qsTr("Pounds")]; currentIndex: Backend.weight === Enums.KG ? 0 : 1; onActivated: { Backend.weight = currentIndex === 0 ? Enums.KG : Enums.LBS; page.saveUnitChange() } }
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Updates"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: qsTr("Installed: %1").arg(manager.getVersion()); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
				Components.NeoSwitch { Layout.fillWidth: true; text: qsTr("Automatically check for updates"); checked: !PrefUpdateManager.dont_check_for_updates; onToggled: PrefUpdateManager.dont_check_for_updates = !checked }
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Update details"); onClicked: page.openAbout() }
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
				Components.NeoSwitch { Layout.fillWidth: true; text: qsTr("Dive-computer ceiling"); checked: PrefTechnicalDetails.dcceiling; onToggled: PrefTechnicalDetails.dcceiling = checked }
				Components.NeoSwitch { Layout.fillWidth: true; text: qsTr("Calculated ceiling"); checked: PrefTechnicalDetails.calcceiling; onToggled: PrefTechnicalDetails.calcceiling = checked }
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("GF low"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoSpinBox { Layout.fillWidth: true; from: 0; to: 100; value: PrefTechnicalDetails.gflow; onValueModified: PrefTechnicalDetails.gflow = value }
				}
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("GF high"); color: tokens.textMuted; font.pixelSize: 10 }
					Components.NeoSpinBox { Layout.fillWidth: true; from: 0; to: 100; value: PrefTechnicalDetails.gfhigh; onValueModified: PrefTechnicalDetails.gfhigh = value }
				}
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout {
				Layout.fillWidth: true
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("Accounts, privacy & security"); color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
					Text { text: qsTr("Review provider connections, credential protection, data flow, and account deletion"); color: tokens.textSecondary; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
				Components.NeoButton { text: qsTr("Open"); variant: "ghost"; compact: true; onClicked: page.openAccountSecurity() }
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout {
				Layout.fillWidth: true
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("Specialist compatibility settings"); color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
					Text { text: qsTr("Legacy profile colors, diagnostics, and uncommon Subsurface controls"); color: tokens.textSecondary; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
				Components.NeoButton { text: qsTr("Compatibility panel"); variant: "ghost"; compact: true; onClicked: page.openAdvancedSettings() }
			}
		}

		Components.NeoButton { Layout.fillWidth: true; text: qsTr("About Subsurface Neo"); onClicked: page.openAbout() }
	}
}
