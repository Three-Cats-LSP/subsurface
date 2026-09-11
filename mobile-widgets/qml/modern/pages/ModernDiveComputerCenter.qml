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
	title: qsTr("Import dives")
	background: Rectangle { color: tokens.background }
	property bool wideLayout: width >= 760
	property bool scanning: false
	signal openNativeImport(string vendor, string product, string connection)
	Modern.DesignTokens { id: tokens }

	function selectDevice(vendor, product, connection) {
		vendorBox.currentIndex = vendorBox.find(vendor)
		productBox.currentIndex = productBox.find(product)
		connectionBox.currentIndex = manager.getConnectionIndex(connection)
	}
	function rescanDevices() {
		scanning = true
		connectionBox.currentIndex = -1
		manager.stopBluetoothDiscovery()
		rescanStartTimer.restart()
		rescanFinishTimer.restart()
	}
	function deleteRecent(slot) {
		var removedDevice = slot === 1 ? PrefDiveComputer.device1 : slot === 2 ? PrefDiveComputer.device2 : slot === 3 ? PrefDiveComputer.device3 : PrefDiveComputer.device4
		if (slot <= 1) { PrefDiveComputer.vendor1 = PrefDiveComputer.vendor2; PrefDiveComputer.product1 = PrefDiveComputer.product2; PrefDiveComputer.device1 = PrefDiveComputer.device2; PrefDiveComputer.device_name1 = PrefDiveComputer.device_name2 }
		if (slot <= 2) { PrefDiveComputer.vendor2 = PrefDiveComputer.vendor3; PrefDiveComputer.product2 = PrefDiveComputer.product3; PrefDiveComputer.device2 = PrefDiveComputer.device3; PrefDiveComputer.device_name2 = PrefDiveComputer.device_name3 }
		if (slot <= 3) { PrefDiveComputer.vendor3 = PrefDiveComputer.vendor4; PrefDiveComputer.product3 = PrefDiveComputer.product4; PrefDiveComputer.device3 = PrefDiveComputer.device4; PrefDiveComputer.device_name3 = PrefDiveComputer.device_name4 }
		PrefDiveComputer.vendor4 = ""; PrefDiveComputer.product4 = ""; PrefDiveComputer.device4 = ""; PrefDiveComputer.device_name4 = ""
		if (PrefDiveComputer.device === removedDevice) { PrefDiveComputer.vendor = ""; PrefDiveComputer.product = ""; PrefDiveComputer.device = ""; PrefDiveComputer.device_name = "" }
		page.rescanDevices()
	}
	Timer { id: rescanStartTimer; interval: 250; repeat: false; onTriggered: manager.rescanConnections() }
	Timer { id: rescanFinishTimer; interval: 2500; repeat: false; onTriggered: page.scanning = false }
	Component.onCompleted: {
		page.rescanDevices()
		vendorBox.currentIndex = manager.getDetectedVendorIndex()
		if (vendorBox.currentIndex >= 0)
			productBox.currentIndex = manager.getDetectedProductIndex(vendorBox.currentText)
		if (productBox.currentIndex >= 0)
			connectionBox.currentIndex = manager.getMatchingAddress(vendorBox.currentText, productBox.currentText)
	}

	ColumnLayout {
		width: page.availableWidth; spacing: tokens.space16
		ColumnLayout {
			Layout.fillWidth: true; spacing: 2
			Text { text: qsTr("Import dives"); color: tokens.textPrimary; font.pixelSize: page.wideLayout ? 30 : 25; font.weight: Font.DemiBold }
			Text { text: qsTr("Download safely with Subsurface's proven device engine"); color: tokens.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}
		GridLayout {
			Layout.fillWidth: true; columns: page.wideLayout && PrefDiveComputer.vendor1 !== "" ? 2 : 1; columnSpacing: tokens.space12; rowSpacing: tokens.space12
			Components.ModernCard {
				Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
				RowLayout {
					Layout.fillWidth: true
					Rectangle {
						Layout.preferredWidth: 40; Layout.preferredHeight: 40; radius: 20
						color: manager.btEnabled ? "#12352D" : "#382D20"
						Components.NeoDiveIcon { anchors.centerIn: parent; width: 23; height: 23; name: "device"; iconColor: manager.btEnabled ? tokens.success : tokens.warning }
					}
					ColumnLayout {
						Layout.fillWidth: true; spacing: 1
						Text { text: qsTr("CONNECTION"); color: tokens.textMuted; font.pixelSize: 9; font.weight: Font.DemiBold }
						Text { text: manager.btEnabled ? qsTr("Bluetooth ready") : qsTr("Bluetooth unavailable"); color: manager.btEnabled ? tokens.success : tokens.warning; font.pixelSize: 17; font.weight: Font.DemiBold }
					}
				}
				Text { text: manager.btEnabled ? qsTr("Nearby Bluetooth devices can be detected.") : qsTr("Enable Bluetooth, or choose an available USB or serial connection below."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Components.NeoButton { text: page.scanning ? qsTr("Scanning…") : qsTr("Rescan devices"); enabled: !page.scanning; Layout.alignment: Qt.AlignLeft; onClicked: page.rescanDevices() }
			}
			Components.ModernCard {
				visible: PrefDiveComputer.vendor1 !== ""; Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
				Text { text: qsTr("RECENT COMPUTERS"); color: tokens.textMuted; font.pixelSize: 10; font.weight: Font.DemiBold }
				Flow {
					Layout.fillWidth: true; spacing: tokens.space8
					RowLayout {
						visible: PrefDiveComputer.vendor1 !== ""
						spacing: 2
						Components.NeoButton { text: PrefDiveComputer.vendor1 + "  •  " + PrefDiveComputer.product1; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor1, PrefDiveComputer.product1, PrefDiveComputer.device1) }
						ToolButton { Layout.preferredWidth: 22; Layout.preferredHeight: 22; padding: 0; text: "×"; font.pixelSize: 13; Accessible.name: qsTr("Delete recent computer"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; background: Rectangle { radius: 4; color: parent.hovered || parent.down ? "#38212B" : "transparent" }; contentItem: Text { text: parent.text; color: "#D94B5B"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }; onClicked: page.deleteRecent(1) }
					}
					RowLayout {
						visible: PrefDiveComputer.vendor2 !== ""
						spacing: 2
						Components.NeoButton { text: PrefDiveComputer.vendor2 + "  •  " + PrefDiveComputer.product2; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor2, PrefDiveComputer.product2, PrefDiveComputer.device2) }
						ToolButton { Layout.preferredWidth: 22; Layout.preferredHeight: 22; padding: 0; text: "×"; font.pixelSize: 13; Accessible.name: qsTr("Delete recent computer"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; background: Rectangle { radius: 4; color: parent.hovered || parent.down ? "#38212B" : "transparent" }; contentItem: Text { text: parent.text; color: "#D94B5B"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }; onClicked: page.deleteRecent(2) }
					}
					RowLayout {
						visible: PrefDiveComputer.vendor3 !== ""
						spacing: 2
						Components.NeoButton { text: PrefDiveComputer.vendor3 + "  •  " + PrefDiveComputer.product3; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor3, PrefDiveComputer.product3, PrefDiveComputer.device3) }
						ToolButton { Layout.preferredWidth: 22; Layout.preferredHeight: 22; padding: 0; text: "×"; font.pixelSize: 13; Accessible.name: qsTr("Delete recent computer"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; background: Rectangle { radius: 4; color: parent.hovered || parent.down ? "#38212B" : "transparent" }; contentItem: Text { text: parent.text; color: "#D94B5B"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }; onClicked: page.deleteRecent(3) }
					}
					RowLayout {
						visible: PrefDiveComputer.vendor4 !== ""
						spacing: 2
						Components.NeoButton { text: PrefDiveComputer.vendor4 + "  •  " + PrefDiveComputer.product4; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor4, PrefDiveComputer.product4, PrefDiveComputer.device4) }
						ToolButton { Layout.preferredWidth: 22; Layout.preferredHeight: 22; padding: 0; text: "×"; font.pixelSize: 13; Accessible.name: qsTr("Delete recent computer"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; background: Rectangle { radius: 4; color: parent.hovered || parent.down ? "#38212B" : "transparent" }; contentItem: Text { text: parent.text; color: "#D94B5B"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }; onClicked: page.deleteRecent(4) }
					}
				}
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Choose a dive computer"); color: tokens.textPrimary; font.pixelSize: 19; font.weight: Font.DemiBold }
			GridLayout {
				Layout.fillWidth: true; columns: page.wideLayout ? 3 : 1; columnSpacing: tokens.space12; rowSpacing: tokens.space8
				ColumnLayout {
					Layout.fillWidth: true; spacing: 4
					Text { text: qsTr("MANUFACTURER"); color: tokens.textMuted; font.pixelSize: 9 }
					Components.NeoComboBox { id: vendorBox; Layout.fillWidth: true; model: vendorList; onActivated: { productBox.model = manager.getProductListFromVendor(currentText); productBox.currentIndex = manager.getDetectedProductIndex(currentText) } }
				}
				ColumnLayout {
					Layout.fillWidth: true; spacing: 4
					Text { text: qsTr("MODEL"); color: tokens.textMuted; font.pixelSize: 9 }
					Components.NeoComboBox { id: productBox; Layout.fillWidth: true; model: vendorBox.currentIndex >= 0 ? manager.getProductListFromVendor(vendorBox.currentText) : []; onActivated: connectionBox.currentIndex = manager.getMatchingAddress(vendorBox.currentText, currentText) }
				}
				ColumnLayout {
					Layout.fillWidth: true; spacing: 4
					Text { text: qsTr("CONNECTION"); color: tokens.textMuted; font.pixelSize: 9 }
					Components.NeoComboBox {
						id: connectionBox
						Layout.fillWidth: true
						model: connectionListModel
						onCountChanged: {
							if (count === 0) {
								currentIndex = -1
								return
							}
							var detected = manager.getMatchingAddress(vendorBox.currentText, productBox.currentText)
							currentIndex = detected >= 0 ? detected : (currentIndex >= 0 ? currentIndex : 0)
						}
					}
				}
			}
			GridLayout {
				Layout.fillWidth: true; columns: page.wideLayout ? 2 : 1
				CheckBox { Layout.fillWidth: true; text: qsTr("Include previously imported dives"); checked: manager.DC_forceDownload; onToggled: manager.DC_forceDownload = checked }
				CheckBox { Layout.fillWidth: true; text: qsTr("Synchronize dive-computer time"); checked: Backend.sync_dc_time; onToggled: Backend.sync_dc_time = checked }
			}
			Components.NeoButton {
				Layout.fillWidth: true; text: qsTr("Download dives")
				variant: "primary"
				enabled: vendorBox.currentIndex >= 0 && productBox.currentIndex >= 0 && connectionBox.currentIndex >= 0
				onClicked: page.openNativeImport(vendorBox.currentText, productBox.currentText, connectionBox.currentText)
			}
			Text {
				visible: vendorBox.currentIndex < 0 || productBox.currentIndex < 0 || connectionBox.currentIndex < 0
				text: qsTr("Choose a manufacturer, model, and connection to continue.")
				color: tokens.textMuted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Review before saving"); color: tokens.accent; font.pixelSize: 14; font.weight: Font.DemiBold }
			Text { text: qsTr("Downloaded dives open in a review screen first. Nothing is added to your log until you confirm the selected entries."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}
	}
}
