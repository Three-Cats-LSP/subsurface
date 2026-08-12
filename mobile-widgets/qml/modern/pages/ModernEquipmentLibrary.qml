// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Equipment library")
	background: Rectangle { color: tokens.background }
	Modern.DesignTokens { id: tokens }
	function editKit(data) {
		kitName.text = data.name || ""
		suit.text = data.suit || ""
		buddy.text = data.buddy || ""
		tags.text = data.tags || ""
		cylinder.text = data.cylinder || ""
		gas.text = data.gas || ""
		startPressure.text = data.startPressure || ""
		endPressure.text = data.endPressure || ""
		kitEditor.open()
	}
	function saveEditedKit() {
		NeoEquipmentKits.saveKit(kitName.text, { "suit": suit.text, "buddy": buddy.text,
			"tags": tags.text, "cylinder": cylinder.text, "gas": gas.text,
			"startPressure": startPressure.text, "endPressure": endPressure.text })
		kitEditor.close()
	}
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Reusable gear loadouts"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Kits are applied in the Neo Dive Editor. They store the normal equipment fields you reuse, while cylinder data remains in Subsurface’s canonical dive structures."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Button { Layout.fillWidth: true; text: qsTr("Create equipment kit"); onClicked: page.editKit({}) }
		Text { visible: NeoEquipmentKits.kits.length === 0; text: qsTr("No equipment kits yet. Save your current setup from a dive editor to create one."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Repeater {
			model: NeoEquipmentKits.kits
			delegate: Components.ModernCard {
				required property var modelData
				Layout.fillWidth: true
				Text { text: modelData.name; color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: [modelData.suit, modelData.cylinder, modelData.gas].filter(function(value) { return value && value.length > 0 }).join(" · "); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Text { visible: NeoEquipmentKits.defaultKit === modelData.name; text: qsTr("Default kit for new dives"); color: tokens.success; font.pixelSize: 12 }
				RowLayout { Layout.fillWidth: true; Button { text: qsTr("Edit"); onClicked: page.editKit(modelData) }; Button { text: NeoEquipmentKits.defaultKit === modelData.name ? qsTr("Clear default") : qsTr("Set as default"); onClicked: NeoEquipmentKits.defaultKit = NeoEquipmentKits.defaultKit === modelData.name ? "" : modelData.name }; Item { Layout.fillWidth: true }; Button { text: qsTr("Remove"); onClicked: NeoEquipmentKits.removeKit(modelData.name) } }
			}
		}
	}
	Dialog {
		id: kitEditor
		parent: page
		modal: true
		width: Math.min(page.width - tokens.space32, 560)
		x: (page.width - width) / 2
		y: tokens.space24
		title: kitName.text.length > 0 ? qsTr("Edit equipment kit") : qsTr("Create equipment kit")
		contentItem: Flickable {
			implicitWidth: 500
			implicitHeight: Math.min(contentHeight, 520)
			contentWidth: width
			contentHeight: kitFields.implicitHeight
			clip: true
			ColumnLayout {
				id: kitFields
				width: parent.width
				spacing: tokens.space12
				TextField { id: kitName; Layout.fillWidth: true; placeholderText: qsTr("Kit name") }
				TextField { id: suit; Layout.fillWidth: true; placeholderText: qsTr("Suit / primary gear") }
				TextField { id: cylinder; Layout.fillWidth: true; placeholderText: qsTr("Cylinder") }
				TextField { id: gas; Layout.fillWidth: true; placeholderText: qsTr("Gas mix") }
				RowLayout { Layout.fillWidth: true; TextField { id: startPressure; Layout.fillWidth: true; placeholderText: qsTr("Start pressure") }; TextField { id: endPressure; Layout.fillWidth: true; placeholderText: qsTr("End pressure") } }
				TextField { id: buddy; Layout.fillWidth: true; placeholderText: qsTr("Buddy") }
				TextField { id: tags; Layout.fillWidth: true; placeholderText: qsTr("Tags") }
			}
		}
		footer: DialogButtonBox { Button { text: qsTr("Save kit"); enabled: kitName.text.trim().length > 0; onClicked: page.saveEditedKit() }; Button { text: qsTr("Cancel"); onClicked: kitEditor.close() } }
	}
}
