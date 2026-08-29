// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.Page {
	id: page
	objectName: "ModernDiveEditor"
	title: qsTr("Edit dive")
	background: Rectangle { color: tokens.background }

	property var dive: null
	property bool newDive: false
	property bool defaultKitApplied: false
	signal saved()

	Modern.DesignTokens { id: tokens }

	function listValue(value) {
		return value === undefined || value === null ? [] : value
	}

	function save() {
		if (!dive)
			return
		manager.commitChanges(dive.id, numberField.text, dateField.text, locationField.text,
			gpsField.text, durationField.text, depthField.text, airTempField.text,
			waterTempField.text, suitField.text, buddyField.text, diveGuideField.text, composedTags(), modeBox.currentText,
			weightField.text, notesField.text, equipmentValues(dive.startPressure, startPressureField.text),
			equipmentValues(dive.endPressure, endPressureField.text), equipmentValues(dive.firstGas, gasField.text), equipmentValues(dive.getCylinder, cylinderBox.currentText),
			ratingBox.value, visibilityBox.value, "view")
		saved()
	}

	function primaryDiveType() {
		if (!dive || !dive.tags)
			return ""
		return String(dive.tags).split(",")[0].trim()
	}

	function additionalTags() {
		if (!dive || !dive.tags)
			return ""
		var values = String(dive.tags).split(",")
		values.shift()
		return values.join(",").trim()
	}

	function composedTags() {
		var values = []
		var requestedType = typeBox.currentText.trim()
		var remaining = tagsField.text.split(",")
		if (requestedType.length > 0)
			values.push(requestedType)
		for (var i = 0; i < remaining.length; ++i) {
			var tag = remaining[i].trim()
			if (tag.length > 0 && values.indexOf(tag) < 0)
				values.push(tag)
		}
		return values.join(", ")
	}

	function equipmentValues(original, replacement) {
		var values = listValue(original).slice()
		if (replacement.length > 0)
			values[0] = replacement
		return values
	}

	function currentKitData() {
		return { "suit": suitField.text, "buddy": buddyField.text, "tags": tagsField.text,
			"cylinder": cylinderBox.currentText, "gas": gasField.text,
			"startPressure": startPressureField.text, "endPressure": endPressureField.text }
	}

	function applyKit(name) {
		var kit = NeoEquipmentKits.kit(name)
		if (!kit || !kit.name)
			return
		suitField.text = kit.suit || ""
		buddyField.text = kit.buddy || ""
		tagsField.text = kit.tags || ""
		var cylinderIndex = cylinderBox.find(kit.cylinder || "")
		if (cylinderIndex >= 0)
			cylinderBox.currentIndex = cylinderIndex
		else
			cylinderBox.editText = kit.cylinder || ""
		gasField.text = kit.gas || ""
		startPressureField.text = kit.startPressure || ""
		endPressureField.text = kit.endPressure || ""
		NeoEquipmentKits.useKit(name)
	}

	Component.onCompleted: {
		if (newDive && NeoEquipmentKits.defaultKit.length > 0) {
			applyKit(NeoEquipmentKits.defaultKit)
			defaultKitApplied = true
		}
	}

	Flickable {
		id: editorFlickable
		anchors.fill: parent
		contentWidth: width
		contentHeight: editorColumn.implicitHeight + tokens.space24 * 2
		flickableDirection: Flickable.VerticalFlick
		boundsBehavior: Flickable.StopAtBounds
		clip: true
		ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

		ColumnLayout {
			id: editorColumn
			width: parent.width
			spacing: tokens.space16

			Text {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Layout.topMargin: tokens.space12
				text: defaultKitApplied ? qsTr("New dive — default kit applied") : (dive && dive.location ? dive.location : qsTr("Dive details"))
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
				Components.NeoComboBox {
					id: kitSelector
					Layout.fillWidth: true
					model: [qsTr("Choose a kit")].concat(NeoEquipmentKits.kits.map(function(kit) { return kit.name }))
				onActivated: if (currentIndex > 0) page.applyKit(currentText)
				}
				RowLayout {
					Layout.fillWidth: true
					Components.NeoTextField { id: kitNameField; Layout.fillWidth: true; placeholderText: qsTr("Save current fields as kit") }
					Components.NeoButton { text: qsTr("Save kit"); enabled: kitNameField.text.length > 0; onClicked: { NeoEquipmentKits.saveKit(kitNameField.text, page.currentKitData()); kitNameField.clear() } }
					Components.NeoButton { text: qsTr("Remove"); variant: "danger"; enabled: kitSelector.currentIndex > 0; onClicked: { NeoEquipmentKits.removeKit(kitSelector.currentText); kitSelector.currentIndex = 0 } }
				}
				Components.NeoCheckBox { text: qsTr("Use selected kit by default"); checked: kitSelector.currentIndex > 0 && NeoEquipmentKits.defaultKit === kitSelector.currentText; onToggled: NeoEquipmentKits.defaultKit = checked ? kitSelector.currentText : "" }
				Flow {
					Layout.fillWidth: true
					visible: NeoEquipmentKits.recentKitNames.length > 0
					spacing: tokens.space8
					Text { text: qsTr("Recent:"); color: tokens.textSecondary; font.pixelSize: 13 }
					Repeater { model: NeoEquipmentKits.recentKitNames; delegate: Components.NeoButton { required property string modelData; compact: true; text: modelData; onClicked: page.applyKit(modelData) } }
				}
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
					Components.NeoTextField { id: dateField; Layout.fillWidth: true; placeholderText: qsTr("Date and time"); text: dive ? dive.dateTime || "" : "" }
					Components.NeoTextField { id: numberField; Layout.fillWidth: true; placeholderText: qsTr("Dive number"); inputMethodHints: Qt.ImhDigitsOnly; text: dive ? dive.number || "" : "" }
					Components.NeoTextField { id: depthField; Layout.fillWidth: true; placeholderText: qsTr("Maximum depth"); text: dive ? dive.depth || "" : "" }
					Components.NeoTextField { id: durationField; Layout.fillWidth: true; placeholderText: qsTr("Duration"); text: dive ? dive.duration || "" : "" }
					Components.NeoTextField { id: waterTempField; Layout.fillWidth: true; placeholderText: qsTr("Water temperature"); text: dive ? dive.waterTemp || "" : "" }
					Components.NeoTextField { id: airTempField; Layout.fillWidth: true; placeholderText: qsTr("Air temperature"); text: dive ? dive.airTemp || "" : "" }
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("Dive classification"); color: tokens.textMuted; font.pixelSize: 10 }
				GridLayout {
					Layout.fillWidth: true
					columns: page.width >= 700 ? 2 : 1
					columnSpacing: tokens.space12
					rowSpacing: tokens.space8
					RowLayout {
						Layout.fillWidth: true
						Components.NeoDiveIcon { name: "regulator"; iconColor: tokens.accent; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 3
							Text { text: qsTr("MODE"); color: tokens.textMuted; font.pixelSize: 9; font.weight: Font.DemiBold }
							Components.NeoComboBox {
								id: modeBox
								Layout.fillWidth: true
								model: ["OC", "CCR", "PSCR", qsTr("Freedive")]
								currentIndex: Math.max(0, find(dive && dive.diveMode ? dive.diveMode : "OC"))
							}
						}
					}
					RowLayout {
						Layout.fillWidth: true
						Components.NeoDiveIcon { name: "type"; iconColor: tokens.accent; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 3
							Text { text: qsTr("TYPE"); color: tokens.textMuted; font.pixelSize: 9; font.weight: Font.DemiBold }
							Components.NeoComboBox {
								id: typeBox
								Layout.fillWidth: true
								editable: true
								model: [qsTr("Boat"), qsTr("Shore"), qsTr("Pool"), qsTr("Cave"), qsTr("Wreck"), qsTr("Training"), qsTr("Other")]
								Component.onCompleted: {
									var initialType = page.primaryDiveType()
									var typeIndex = find(initialType)
									if (typeIndex >= 0)
										currentIndex = typeIndex
									else
										editText = initialType
								}
							}
						}
					}
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("People and place"); color: tokens.textMuted; font.pixelSize: 10 }
				Components.NeoTextField { id: locationField; Layout.fillWidth: true; placeholderText: qsTr("Dive site"); text: dive ? dive.location || "" : "" }
				Components.NeoTextField { id: gpsField; Layout.fillWidth: true; placeholderText: qsTr("GPS coordinates"); text: dive ? dive.gps || "" : "" }
				Components.NeoTextField { id: buddyField; Layout.fillWidth: true; placeholderText: qsTr("Buddy"); text: dive ? dive.buddy || "" : "" }
				Components.NeoTextField { id: diveGuideField; Layout.fillWidth: true; placeholderText: qsTr("Dive guide"); text: dive ? dive.diveGuide || "" : "" }
				Components.NeoTextField { id: suitField; Layout.fillWidth: true; placeholderText: qsTr("Suit / gear"); text: dive ? dive.suit || "" : "" }
				Components.NeoTextField { id: tagsField; Layout.fillWidth: true; placeholderText: qsTr("Additional tags"); text: page.additionalTags() }
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("Dive conditions"); color: tokens.textMuted; font.pixelSize: 10 }
				GridLayout {
					Layout.fillWidth: true
					columns: page.width >= 700 ? 2 : 1
					columnSpacing: tokens.space12
					rowSpacing: tokens.space8
					ColumnLayout {
						Layout.fillWidth: true
						Text { text: qsTr("Rating"); color: tokens.textSecondary; font.pixelSize: 12 }
						Components.NeoSpinBox { id: ratingBox; Layout.fillWidth: true; from: 0; to: 5; value: dive ? dive.rating || 0 : 0; accessibleName: qsTr("Dive rating") }
					}
					ColumnLayout {
						Layout.fillWidth: true
						Text { text: qsTr("Visibility"); color: tokens.textSecondary; font.pixelSize: 12 }
						Components.NeoSpinBox { id: visibilityBox; Layout.fillWidth: true; from: 0; to: 5; value: dive ? dive.viz || 0 : 0; accessibleName: qsTr("Underwater visibility rating") }
					}
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Text { text: qsTr("Notes"); color: tokens.textMuted; font.pixelSize: 10 }
				Components.NeoTextArea { id: notesField; Layout.fillWidth: true; Layout.preferredHeight: 150; text: dive ? dive.notes || "" : "" }
			}

			Components.ModernCard {
				Layout.fillWidth: true
				Layout.leftMargin: tokens.space16
				Layout.rightMargin: tokens.space16
				Layout.bottomMargin: tokens.space24
				Text { text: qsTr("Cylinders and weights"); color: tokens.textMuted; font.pixelSize: 10 }
				Text { Layout.fillWidth: true; text: qsTr("Primary cylinder, gas and weighting used for this dive."); color: tokens.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap }
				Components.NeoComboBox { id: cylinderBox; Layout.fillWidth: true; editable: true; model: dive ? dive.cylinderList : []; currentIndex: find(dive && dive.getCylinder && dive.getCylinder.length ? dive.getCylinder[0] : "") }
				GridLayout {
					Layout.fillWidth: true
					columns: page.width >= 700 ? 2 : 1
					Components.NeoTextField { id: gasField; Layout.fillWidth: true; placeholderText: qsTr("Gas mix"); text: dive && dive.firstGas && dive.firstGas.length ? dive.firstGas[0] : "" }
					Components.NeoTextField { id: weightField; Layout.fillWidth: true; placeholderText: qsTr("Weight"); text: dive ? dive.sumWeight || "" : "" }
					Components.NeoTextField { id: startPressureField; Layout.fillWidth: true; placeholderText: qsTr("Start pressure"); text: dive && dive.startPressure && dive.startPressure.length ? dive.startPressure[0] : "" }
					Components.NeoTextField { id: endPressureField; Layout.fillWidth: true; placeholderText: qsTr("End pressure"); text: dive && dive.endPressure && dive.endPressure.length ? dive.endPressure[0] : "" }
				}
			}
		}
	}

	footer: ToolBar {
		background: Rectangle { color: tokens.surface }
		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: tokens.space12
			anchors.rightMargin: tokens.space12
			Components.NeoButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: page.saved() }
			Item { Layout.fillWidth: true }
			Components.NeoButton { text: qsTr("Save dive"); variant: "primary"; enabled: !!dive; onClicked: page.save() }
		}
	}
}
