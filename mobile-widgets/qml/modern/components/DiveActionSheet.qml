// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern

Dialog {
	id: sheet
	modal: true
	focus: true
	anchors.centerIn: Overlay.overlay
	width: Math.min(Overlay.overlay ? Overlay.overlay.width - tokens.space24 : 420, 420)
	padding: tokens.space16
	standardButtons: Dialog.NoButton

	property var dive: null
	signal openDive(int row)

	Modern.DesignTokens { id: tokens }

	function openForDive(data) {
		dive = data
		if (!dive || dive.isTrip)
			return
		manager.selectRow(dive.row)
		open()
	}

	function closeAndRun(callback) {
		close()
		callback()
	}

	background: Rectangle {
		color: tokens.surfaceRaised
		radius: tokens.radius16
		border.width: 1
		border.color: tokens.border
	}

	contentItem: ColumnLayout {
		spacing: tokens.space8

		Text {
			Layout.fillWidth: true
			text: sheet.dive && sheet.dive.number > 0 ? qsTr("Dive #%1").arg(sheet.dive.number) : qsTr("Dive actions")
			color: tokens.textPrimary
			font.pixelSize: 18
			font.weight: Font.DemiBold
			elide: Text.ElideRight
		}

		Text {
			Layout.fillWidth: true
			visible: sheet.dive && sheet.dive.location && sheet.dive.location.length > 0
			text: visible ? sheet.dive.location : ""
			color: tokens.textSecondary
			font.pixelSize: 12
			elide: Text.ElideRight
		}

		Button {
			Layout.fillWidth: true
			text: qsTr("Open dive")
			onClicked: sheet.closeAndRun(function() { sheet.openDive(sheet.dive.row) })
		}

		Button {
			Layout.fillWidth: true
			visible: sheet.dive && sheet.dive.diveInTrip === true
			text: qsTr("Remove from trip")
			onClicked: sheet.closeAndRun(function() { manager.removeDiveFromTrip(sheet.dive.id) })
		}

		Button {
			Layout.fillWidth: true
			visible: sheet.dive && sheet.dive.diveInTrip === false && sheet.dive.tripAbove !== -1
			text: qsTr("Add to trip above")
			onClicked: sheet.closeAndRun(function() { manager.addDiveToTrip(sheet.dive.id, sheet.dive.tripAbove) })
		}

		Button {
			Layout.fillWidth: true
			visible: sheet.dive && sheet.dive.diveInTrip === false && sheet.dive.tripBelow !== -1
			text: qsTr("Add to trip below")
			onClicked: sheet.closeAndRun(function() { manager.addDiveToTrip(sheet.dive.id, sheet.dive.tripBelow) })
		}

		Button {
			Layout.fillWidth: true
			visible: sheet.dive && sheet.dive.diveInTrip === false
			text: qsTr("Create trip with this dive")
			onClicked: sheet.closeAndRun(function() { manager.addTripForDive(sheet.dive.id) })
		}

		Button {
			Layout.fillWidth: true
			visible: sheet.dive && sheet.dive.diveAbove !== -1 && manager.canMerge(sheet.dive.id, sheet.dive.diveAbove)
			text: qsTr("Merge with dive above")
			onClicked: sheet.closeAndRun(function() { manager.mergeDives(sheet.dive.id, sheet.dive.diveAbove) })
		}

		Button {
			Layout.fillWidth: true
			visible: sheet.dive && sheet.dive.diveBelow !== -1 && manager.canMerge(sheet.dive.id, sheet.dive.diveBelow)
			text: qsTr("Merge with dive below")
			onClicked: sheet.closeAndRun(function() { manager.mergeDives(sheet.dive.id, sheet.dive.diveBelow) })
		}

		Button {
			Layout.fillWidth: true
			text: sheet.dive && sheet.dive.isInvalid === true ? qsTr("Mark as valid") : qsTr("Mark as invalid")
			onClicked: sheet.closeAndRun(function() { manager.toggleDiveInvalid(sheet.dive.id) })
		}

		Button {
			Layout.fillWidth: true
			text: qsTr("Delete dive")
			onClicked: confirmDelete.open()
		}

		Button {
			Layout.fillWidth: true
			text: qsTr("Cancel")
			onClicked: sheet.close()
		}
	}

	Dialog {
		id: confirmDelete
		modal: true
		focus: true
		anchors.centerIn: Overlay.overlay
		title: qsTr("Delete dive?")
		standardButtons: Dialog.Cancel | Dialog.Ok
		onAccepted: {
			const diveId = sheet.dive ? sheet.dive.id : -1
			close()
			sheet.close()
			if (diveId !== -1)
				manager.deleteDive(diveId)
		}

		contentItem: Text {
			text: qsTr("This removes the selected dive from the log. You can use Undo immediately afterwards if needed.")
			color: tokens.textPrimary
			wrapMode: Text.WordWrap
			width: 300
		}
	}
}
