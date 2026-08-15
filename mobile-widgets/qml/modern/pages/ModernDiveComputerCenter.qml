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
	signal openNativeImport(string vendor, string product, string connection)
	Modern.DesignTokens { id: tokens }

	function selectDevice(vendor, product, connection) {
		vendorBox.currentIndex = vendorBox.find(vendor)
		productBox.currentIndex = productBox.find(product)
		connectionBox.currentIndex = manager.getConnectionIndex(connection)
	}
	Component.onCompleted: {
		manager.rescanConnections()
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
				Components.NeoButton { text: qsTr("Rescan devices"); Layout.alignment: Qt.AlignLeft; onClicked: manager.rescanConnections() }
			}
			Components.ModernCard {
				visible: PrefDiveComputer.vendor1 !== ""; Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
				Text { text: qsTr("RECENT COMPUTERS"); color: tokens.textMuted; font.pixelSize: 10; font.weight: Font.DemiBold }
				Flow {
					Layout.fillWidth: true; spacing: tokens.space8
					Components.NeoButton { visible: PrefDiveComputer.vendor1 !== ""; text: PrefDiveComputer.vendor1 + "  •  " + PrefDiveComputer.product1; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor1, PrefDiveComputer.product1, PrefDiveComputer.device1) }
					Components.NeoButton { visible: PrefDiveComputer.vendor2 !== ""; text: PrefDiveComputer.vendor2 + "  •  " + PrefDiveComputer.product2; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor2, PrefDiveComputer.product2, PrefDiveComputer.device2) }
					Components.NeoButton { visible: PrefDiveComputer.vendor3 !== ""; text: PrefDiveComputer.vendor3 + "  •  " + PrefDiveComputer.product3; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor3, PrefDiveComputer.product3, PrefDiveComputer.device3) }
					Components.NeoButton { visible: PrefDiveComputer.vendor4 !== ""; text: PrefDiveComputer.vendor4 + "  •  " + PrefDiveComputer.product4; compact: true; onClicked: page.selectDevice(PrefDiveComputer.vendor4, PrefDiveComputer.product4, PrefDiveComputer.device4) }
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
					Components.NeoComboBox { id: connectionBox; Layout.fillWidth: true; model: connectionListModel }
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
