// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Equipment")
	background: Rectangle { color: tokens.background }
	property bool wideLayout: width >= 760
	Modern.DesignTokens { id: tokens }

	function editKit(data) {
		kitName.text = data.name || ""; suit.text = data.suit || ""; buddy.text = data.buddy || ""
		tags.text = data.tags || ""; cylinder.text = data.cylinder || ""; gas.text = data.gas || ""
		startPressure.text = data.startPressure || ""; endPressure.text = data.endPressure || ""; kitEditor.open()
	}
	function saveEditedKit() {
		NeoEquipmentKits.saveKit(kitName.text.trim(), { "suit": suit.text, "buddy": buddy.text,
			"tags": tags.text, "cylinder": cylinder.text, "gas": gas.text,
			"startPressure": startPressure.text, "endPressure": endPressure.text })
		kitEditor.close()
	}
	function editItem(data) {
		itemName.text = data.name || ""; itemCategory.text = data.category || ""
		itemManufacturer.text = data.manufacturer || ""; itemModel.text = data.model || ""
		itemSerial.text = data.serial || ""; itemPurchaseDate.text = data.purchaseDate || ""
		itemServiceDate.text = data.serviceDate || ""; itemServiceInterval.text = data.serviceInterval || ""
		itemRetired.checked = data.retired === true; itemNotes.text = data.notes || ""; itemEditor.open()
	}
	function saveEditedItem() {
		NeoEquipmentKits.saveEquipmentItem(itemName.text.trim(), { "category": itemCategory.text,
			"manufacturer": itemManufacturer.text, "model": itemModel.text, "serial": itemSerial.text,
			"purchaseDate": itemPurchaseDate.text, "serviceDate": itemServiceDate.text,
			"serviceInterval": itemServiceInterval.text, "retired": itemRetired.checked, "notes": itemNotes.text })
		itemEditor.close()
	}
	function activeItemCount() {
		var count = 0
		for (var i = 0; i < NeoEquipmentKits.equipmentItems.length; ++i)
			if (NeoEquipmentKits.equipmentItems[i].retired !== true) ++count
		return count
	}

	ColumnLayout {
		width: page.availableWidth; spacing: tokens.space16
		ColumnLayout {
			Layout.fillWidth: true; spacing: 2
			Text { text: qsTr("Equipment"); color: tokens.textPrimary; font.pixelSize: page.wideLayout ? 30 : 25; font.weight: Font.DemiBold }
			Text { text: qsTr("Reusable loadouts and service records"); color: tokens.textSecondary; font.pixelSize: 13 }
		}
		GridLayout {
			Layout.fillWidth: true; columns: 3; columnSpacing: tokens.space8
			Components.MetricCard { Layout.fillWidth: true; Layout.minimumWidth: 0; label: qsTr("Kits"); value: String(NeoEquipmentKits.kits.length); iconName: "gear" }
			Components.MetricCard { Layout.fillWidth: true; Layout.minimumWidth: 0; label: qsTr("Items"); value: String(NeoEquipmentKits.equipmentItems.length); iconName: "gas" }
			Components.MetricCard { Layout.fillWidth: true; Layout.minimumWidth: 0; label: qsTr("Active"); value: String(page.activeItemCount()); iconName: "gear" }
		}

		RowLayout { Layout.fillWidth: true; Text { text: qsTr("Gear loadouts"); color: tokens.textPrimary; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.fillWidth: true }; Components.NeoButton { text: qsTr("Add kit"); variant: "primary"; compact: true; onClicked: page.editKit({}) } }
		Text { visible: NeoEquipmentKits.kits.length === 0; text: qsTr("No loadouts yet. Create one here or save the current setup from the Neo Dive Editor."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		GridLayout {
			Layout.fillWidth: true; columns: page.wideLayout ? 2 : 1; columnSpacing: tokens.space12; rowSpacing: tokens.space12
			Repeater {
				model: NeoEquipmentKits.kits
				delegate: Components.ModernCard {
					required property var modelData
					Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
					RowLayout {
						Layout.fillWidth: true
						Rectangle { Layout.preferredWidth: 40; Layout.preferredHeight: 40; radius: 20; color: "#083449"; Components.NeoDiveIcon { anchors.centerIn: parent; width: 23; height: 23; name: "gear"; iconColor: tokens.accent } }
						ColumnLayout { Layout.fillWidth: true; spacing: 1; Text { text: modelData.name; color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }; Text { visible: NeoEquipmentKits.defaultKit === modelData.name; text: qsTr("DEFAULT LOADOUT"); color: tokens.success; font.pixelSize: 10 } }
					}
					Text { text: [modelData.suit, modelData.cylinder, modelData.gas].filter(function(value) { return value && value.length > 0 }).join("  •  ") || qsTr("No equipment details yet"); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					GridLayout { Layout.fillWidth: true; columns: 3; columnSpacing: tokens.space8; Components.NeoButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Edit"); compact: true; onClicked: page.editKit(modelData) }; Components.NeoButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: NeoEquipmentKits.defaultKit === modelData.name ? qsTr("Clear default") : qsTr("Make default"); compact: true; onClicked: NeoEquipmentKits.defaultKit = NeoEquipmentKits.defaultKit === modelData.name ? "" : modelData.name }; Components.NeoButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Remove"); variant: "danger"; compact: true; onClicked: NeoEquipmentKits.removeKit(modelData.name) } }
				}
			}
		}

		RowLayout {
			Layout.fillWidth: true; Layout.topMargin: tokens.space8
			ColumnLayout { Layout.fillWidth: true; spacing: 1; Text { text: qsTr("Individual equipment"); color: tokens.textPrimary; font.pixelSize: 20; font.weight: Font.DemiBold }; Text { text: qsTr("Track identity and service information"); color: tokens.textSecondary; font.pixelSize: 12 } }
			Components.NeoButton { text: qsTr("Add item"); variant: "primary"; compact: true; onClicked: page.editItem({}) }
		}
		Text { visible: NeoEquipmentKits.equipmentItems.length === 0; text: qsTr("No individual equipment records yet."); color: tokens.textSecondary }
		GridLayout {
			Layout.fillWidth: true; columns: page.wideLayout ? 2 : 1; columnSpacing: tokens.space12; rowSpacing: tokens.space12
			Repeater {
				model: NeoEquipmentKits.equipmentItems
				delegate: Components.ModernCard {
					required property var modelData
					Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
					RowLayout { Layout.fillWidth: true; Rectangle { Layout.preferredWidth: 40; Layout.preferredHeight: 40; radius: 20; color: tokens.surfaceRaised; Components.NeoDiveIcon { anchors.centerIn: parent; width: 22; height: 22; name: "gear"; iconColor: tokens.accent } }; ColumnLayout { Layout.fillWidth: true; spacing: 2; Text { text: modelData.name; color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }; Text { text: [modelData.category, modelData.manufacturer, modelData.model].filter(function(value) { return value && value.length > 0 }).join("  •  ") || qsTr("Uncategorized"); color: tokens.textSecondary; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true } }; Text { visible: modelData.retired === true; text: qsTr("RETIRED"); color: tokens.warning; font.pixelSize: 10; font.weight: Font.DemiBold } }
					Text { visible: modelData.serviceDate && modelData.serviceDate.length > 0; text: qsTr("Last service: %1%2").arg(modelData.serviceDate).arg(modelData.serviceInterval ? qsTr("  •  every %1").arg(modelData.serviceInterval) : ""); color: tokens.textMuted; font.pixelSize: 12 }
					Text { text: qsTr("Used on %1 dives").arg(NeoEquipmentKits.usageCount(modelData.name)); color: tokens.textMuted; font.pixelSize: 12 }
					RowLayout { Layout.fillWidth: true; Components.NeoButton { text: qsTr("Edit"); compact: true; onClicked: page.editItem(modelData) }; Item { Layout.fillWidth: true }; Components.NeoButton { text: qsTr("Remove"); variant: "danger"; compact: true; onClicked: NeoEquipmentKits.removeEquipmentItem(modelData.name) } }
				}
			}
		}
	}

	Dialog {
		id: kitEditor; parent: Overlay.overlay; anchors.centerIn: parent; modal: true
		width: Math.min(page.width - tokens.space32, 560); height: Math.min(page.height - tokens.space32, 620)
		title: kitName.text.length > 0 ? qsTr("Edit equipment kit") : qsTr("Create equipment kit")
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Flickable {
			contentWidth: width; contentHeight: kitFields.implicitHeight; clip: true; ScrollBar.vertical: ScrollBar {}
			ColumnLayout { id: kitFields; width: parent.width; spacing: tokens.space12
				Components.NeoTextField { id: kitName; Layout.fillWidth: true; placeholderText: qsTr("Kit name") }; Components.NeoTextField { id: suit; Layout.fillWidth: true; placeholderText: qsTr("Suit / primary gear") }; Components.NeoTextField { id: cylinder; Layout.fillWidth: true; placeholderText: qsTr("Cylinder") }; Components.NeoTextField { id: gas; Layout.fillWidth: true; placeholderText: qsTr("Gas mix") }
				RowLayout { Layout.fillWidth: true; Components.NeoTextField { id: startPressure; Layout.fillWidth: true; placeholderText: qsTr("Start pressure") }; Components.NeoTextField { id: endPressure; Layout.fillWidth: true; placeholderText: qsTr("End pressure") } }
				Components.NeoTextField { id: buddy; Layout.fillWidth: true; placeholderText: qsTr("Buddy") }; Components.NeoTextField { id: tags; Layout.fillWidth: true; placeholderText: qsTr("Tags") }
			}
		}
		footer: RowLayout { spacing: tokens.space8; Item { Layout.fillWidth: true }; Components.NeoButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: kitEditor.close() }; Components.NeoButton { text: qsTr("Save kit"); variant: "primary"; enabled: kitName.text.trim().length > 0; onClicked: page.saveEditedKit() } }
	}
	Dialog {
		id: itemEditor; parent: Overlay.overlay; anchors.centerIn: parent; modal: true
		width: Math.min(page.width - tokens.space32, 560); height: Math.min(page.height - tokens.space32, 680)
		title: itemName.text.length > 0 ? qsTr("Edit equipment item") : qsTr("Add equipment item")
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Flickable {
			contentWidth: width; contentHeight: itemFields.implicitHeight; clip: true; ScrollBar.vertical: ScrollBar {}
			ColumnLayout { id: itemFields; width: parent.width; spacing: tokens.space12
				Components.NeoTextField { id: itemName; Layout.fillWidth: true; placeholderText: qsTr("Name") }; Components.NeoTextField { id: itemCategory; Layout.fillWidth: true; placeholderText: qsTr("Category") }; Components.NeoTextField { id: itemManufacturer; Layout.fillWidth: true; placeholderText: qsTr("Manufacturer") }; Components.NeoTextField { id: itemModel; Layout.fillWidth: true; placeholderText: qsTr("Model") }; Components.NeoTextField { id: itemSerial; Layout.fillWidth: true; placeholderText: qsTr("Serial number") }; Components.NeoTextField { id: itemPurchaseDate; Layout.fillWidth: true; placeholderText: qsTr("Purchase date") }; Components.NeoTextField { id: itemServiceDate; Layout.fillWidth: true; placeholderText: qsTr("Last service date") }; Components.NeoTextField { id: itemServiceInterval; Layout.fillWidth: true; placeholderText: qsTr("Service interval, e.g. 12 months") }; CheckBox { id: itemRetired; text: qsTr("Retired / inactive") }; Components.NeoTextArea { id: itemNotes; Layout.fillWidth: true; placeholderText: qsTr("Notes") }
			}
		}
		footer: RowLayout { spacing: tokens.space8; Item { Layout.fillWidth: true }; Components.NeoButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: itemEditor.close() }; Components.NeoButton { text: qsTr("Save item"); variant: "primary"; enabled: itemName.text.trim().length > 0; onClicked: page.saveEditedItem() } }
	}
}
