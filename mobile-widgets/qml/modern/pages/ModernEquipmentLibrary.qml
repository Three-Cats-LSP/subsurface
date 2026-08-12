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
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Reusable gear loadouts"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Kits are applied in the Neo Dive Editor. They store the normal equipment fields you reuse, while cylinder data remains in Subsurface’s canonical dive structures."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Text { visible: NeoEquipmentKits.kits.length === 0; text: qsTr("No equipment kits yet. Save your current setup from a dive editor to create one."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Repeater {
			model: NeoEquipmentKits.kits
			delegate: Components.ModernCard {
				required property var modelData
				Layout.fillWidth: true
				Text { text: modelData.name; color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: [modelData.suit, modelData.cylinder, modelData.gas].filter(function(value) { return value && value.length > 0 }).join(" · "); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Text { visible: NeoEquipmentKits.defaultKit === modelData.name; text: qsTr("Default kit for new dives"); color: tokens.success; font.pixelSize: 12 }
				RowLayout { Layout.fillWidth: true; Button { text: NeoEquipmentKits.defaultKit === modelData.name ? qsTr("Clear default") : qsTr("Set as default"); onClicked: NeoEquipmentKits.defaultKit = NeoEquipmentKits.defaultKit === modelData.name ? "" : modelData.name }; Item { Layout.fillWidth: true }; Button { text: qsTr("Remove"); onClicked: NeoEquipmentKits.removeKit(modelData.name) } }
			}
		}
	}
}
