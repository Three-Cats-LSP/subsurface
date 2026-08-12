// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.Page {
	id: page
	title: qsTr("Edit dive")
	background: Rectangle { color: tokens.background }

	property var dive: null
	signal saved()
	signal advancedEditorRequested(int diveId)

	Modern.DesignTokens { id: tokens }

	function listValue(value) {
		return value === undefined || value === null ? [] : value
	}

	function save() {
		if (!dive)
			return
		manager.commitChanges(dive.id, numberField.text, dateField.text, locationField.text,
			gpsField.text, durationField.text, depthField.text, airTempField.text,
			waterTempField.text, suitField.text, buddyField.text, dive.diveGuide || "", tagsField.text,
			dive.sumWeight || "", notesField.text, listValue(dive.startPressure),
			listValue(dive.endPressure), listValue(dive.firstGas), listValue(dive.getCylinder),
			dive.rating || 0, dive.viz || 0, "view")
		saved()
	}

	function currentKitData() {
		return { "suit": suitField.text, "buddy": buddyField.text, "tags": tagsField.text }
	}

	function applyKit(name) {
		var kit = NeoEquipmentKits.kit(name)
		if (!kit || !kit.name)
			return
		suitField.text = kit.suit || ""
		buddyField.text = kit.buddy || ""
		tagsField.text = kit.tags || ""
	}

	Flickable {
		anchors.fill: parent
		contentWidth: width
		contentHeight: editorColumn.implicitHeight + tokens.space24 * 2
		flickableDirection: Flickable.VerticalFlick
		boundsBehavior: Flickable.StopAtBounds
		clip: true

		ColumnLayout {
			id: editorColumn
			width: parent.width
			spacing: tokens.space16

			Text {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Layout.topMargin: tokens.space12
				text: dive && dive.location ? dive.location : qsTr("Dive details")
				color: tokens.textPrimary
				font.pixelSize: 24
				font.weight: Font.DemiBold
				wrapMode: Text.WordWrap
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("Equipment kit"); color: tokens.textMuted; font.pixelSize: 10 }
				ComboBox {
					id: kitSelector
					Layout.fillWidth: true
					model: [qsTr("Choose a kit")].concat(NeoEquipmentKits.kits.map(function(kit) { return kit.name }))
					onActivated: if (currentIndex > 0) page.applyKit(currentText)
				}
				RowLayout {
					Layout.fillWidth: true
					TextField { id: kitNameField; Layout.fillWidth: true; placeholderText: qsTr("Save current fields as kit") }
					Button { text: qsTr("Save kit"); enabled: kitNameField.text.length > 0; onClicked: { NeoEquipmentKits.saveKit(kitNameField.text, page.currentKitData()); kitNameField.clear() } }
				}
				CheckBox { text: qsTr("Use selected kit by default"); checked: kitSelector.currentIndex > 0 && NeoEquipmentKits.defaultKit === kitSelector.currentText; onToggled: NeoEquipmentKits.defaultKit = checked ? kitSelector.currentText : "" }
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("Core details"); color: tokens.textMuted; font.pixelSize: 10 }
				GridLayout {
					Layout.fillWidth: true
					columns: page.width >= 700 ? 2 : 1
					columnSpacing: tokens.space12
					rowSpacing: tokens.space8
					TextField { id: dateField; Layout.fillWidth: true; placeholderText: qsTr("Date and time"); text: dive ? dive.dateTime || "" : "" }
					TextField { id: numberField; Layout.fillWidth: true; placeholderText: qsTr("Dive number"); inputMethodHints: Qt.ImhDigitsOnly; text: dive ? dive.number || "" : "" }
					TextField { id: depthField; Layout.fillWidth: true; placeholderText: qsTr("Maximum depth"); text: dive ? dive.depth || "" : "" }
					TextField { id: durationField; Layout.fillWidth: true; placeholderText: qsTr("Duration"); text: dive ? dive.duration || "" : "" }
					TextField { id: waterTempField; Layout.fillWidth: true; placeholderText: qsTr("Water temperature"); text: dive ? dive.waterTemp || "" : "" }
					TextField { id: airTempField; Layout.fillWidth: true; placeholderText: qsTr("Air temperature"); text: dive ? dive.airTemp || "" : "" }
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("People and place"); color: tokens.textMuted; font.pixelSize: 10 }
				TextField { id: locationField; Layout.fillWidth: true; placeholderText: qsTr("Dive site"); text: dive ? dive.location || "" : "" }
				TextField { id: gpsField; Layout.fillWidth: true; placeholderText: qsTr("GPS coordinates"); text: dive ? dive.gps || "" : "" }
				TextField { id: buddyField; Layout.fillWidth: true; placeholderText: qsTr("Buddy"); text: dive ? dive.buddy || "" : "" }
				TextField { id: suitField; Layout.fillWidth: true; placeholderText: qsTr("Suit / gear"); text: dive ? dive.suit || "" : "" }
				TextField { id: tagsField; Layout.fillWidth: true; placeholderText: qsTr("Tags"); text: dive ? dive.tags || "" : "" }
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("Notes"); color: tokens.textMuted; font.pixelSize: 10 }
				TextArea { id: notesField; Layout.fillWidth: true; Layout.preferredHeight: 150; wrapMode: TextEdit.Wrap; placeholderText: qsTr("Dive notes"); text: dive ? dive.notes || "" : "" }
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Layout.bottomMargin: tokens.space24
				Text { text: qsTr("Cylinders and weights"); color: tokens.textMuted; font.pixelSize: 10 }
				Text { Layout.fillWidth: true; text: qsTr("Use the full editor for multi-cylinder gas, pressure, and weight-system changes."); color: tokens.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap }
				Button { text: qsTr("Open full equipment editor"); onClicked: if (dive) advancedEditorRequested(dive.id) }
			}
		}
	}

	footer: ToolBar {
		background: Rectangle { color: tokens.surface }
		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: tokens.space12
			anchors.rightMargin: tokens.space12
			Button { text: qsTr("Cancel"); onClicked: page.saved() }
			Item { Layout.fillWidth: true }
			Button { text: qsTr("Save dive"); enabled: !!dive; onClicked: page.save() }
		}
	}
}
