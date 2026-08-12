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
	title: qsTr("Dive computers")
	background: Rectangle { color: tokens.background }

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
		width: page.availableWidth
		spacing: tokens.space16

		Text { text: qsTr("Download from your dive computer"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Text { text: qsTr("Neo uses Subsurface’s proven libdivecomputer import engine for Bluetooth, USB, and serial connections."); color: tokens.textSecondary; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Connection status"); color: tokens.textMuted; font.pixelSize: 10 }
			Text { text: manager.btEnabled ? qsTr("Bluetooth ready") : qsTr("Bluetooth is unavailable or disabled"); color: manager.btEnabled ? tokens.success : tokens.accent; font.pixelSize: 14; font.weight: Font.Medium }
			Button { text: qsTr("Rescan devices"); onClicked: manager.rescanConnections() }
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Choose a computer"); color: tokens.textMuted; font.pixelSize: 10 }
			ComboBox {
				id: vendorBox
				Layout.fillWidth: true
				model: vendorList
				onActivated: { productBox.model = manager.getProductListFromVendor(currentText); productBox.currentIndex = manager.getDetectedProductIndex(currentText) }
			}
			ComboBox {
				id: productBox
				Layout.fillWidth: true
				model: vendorBox.currentIndex >= 0 ? manager.getProductListFromVendor(vendorBox.currentText) : []
				onActivated: connectionBox.currentIndex = manager.getMatchingAddress(vendorBox.currentText, currentText)
			}
			ComboBox { id: connectionBox; Layout.fillWidth: true; model: connectionListModel }
			CheckBox { text: qsTr("Download all dives, including previously imported ones"); checked: manager.DC_forceDownload; onToggled: manager.DC_forceDownload = checked }
			CheckBox { text: qsTr("Sync dive computer time"); checked: Backend.sync_dc_time; onToggled: Backend.sync_dc_time = checked }
			Button {
				Layout.fillWidth: true
				text: qsTr("Start secure import")
				enabled: vendorBox.currentIndex >= 0 && productBox.currentIndex >= 0 && connectionBox.currentIndex >= 0
				onClicked: page.openNativeImport(vendorBox.currentText, productBox.currentText, connectionBox.currentText)
			}
		}

		Components.ModernCard {
			visible: PrefDiveComputer.vendor1 !== ""
			Layout.fillWidth: true
			Text { text: qsTr("Previously used computers"); color: tokens.textMuted; font.pixelSize: 10 }
			Flow {
				Layout.fillWidth: true
				spacing: tokens.space8
				Button { visible: PrefDiveComputer.vendor1 !== ""; text: PrefDiveComputer.vendor1 + " · " + PrefDiveComputer.product1; onClicked: page.selectDevice(PrefDiveComputer.vendor1, PrefDiveComputer.product1, PrefDiveComputer.device1) }
				Button { visible: PrefDiveComputer.vendor2 !== ""; text: PrefDiveComputer.vendor2 + " · " + PrefDiveComputer.product2; onClicked: page.selectDevice(PrefDiveComputer.vendor2, PrefDiveComputer.product2, PrefDiveComputer.device2) }
				Button { visible: PrefDiveComputer.vendor3 !== ""; text: PrefDiveComputer.vendor3 + " · " + PrefDiveComputer.product3; onClicked: page.selectDevice(PrefDiveComputer.vendor3, PrefDiveComputer.product3, PrefDiveComputer.device3) }
				Button { visible: PrefDiveComputer.vendor4 !== ""; text: PrefDiveComputer.vendor4 + " · " + PrefDiveComputer.product4; onClicked: page.selectDevice(PrefDiveComputer.vendor4, PrefDiveComputer.product4, PrefDiveComputer.device4) }
			}
		}

		Text { text: qsTr("The importer will show downloaded dives for review before you record the selected entries. You can cancel and retry safely at any time."); color: tokens.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
