// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

ComboBox {
	id: control
	property string accessibleName: ""
	Modern.DesignTokens { id: tokens }
	implicitHeight: 44
	leftPadding: 12
	rightPadding: 34
	font.pixelSize: 13
	Accessible.name: accessibleName.length > 0 ? accessibleName : displayText
	Accessible.role: Accessible.ComboBox
	contentItem: TextInput {
		text: control.editable ? control.editText : control.displayText
		font: control.font
		color: control.enabled ? tokens.textPrimary : tokens.textMuted
		verticalAlignment: Text.AlignVCenter
		readOnly: !control.editable
		selectByMouse: control.editable
		clip: true
		selectionColor: tokens.accentStrong
		selectedTextColor: tokens.textPrimary
		validator: control.validator
		inputMethodHints: control.inputMethodHints
	}
	indicator: Text {
		x: control.width - width - 12
		y: (control.height - height) / 2
		text: "⌄"
		color: tokens.accent
		font.pixelSize: 16
	}
	background: Rectangle {
		color: control.down || control.popup.visible ? tokens.surfaceRaised : tokens.background
		radius: tokens.radiusSmall
		border.width: 1
		border.color: control.activeFocus || control.popup.visible ? tokens.accent : tokens.border
	}
	// A read-only TextInput consumes pointer events on some Qt/Material builds.
	// Cover the complete non-editable control so the field body and chevron
	// have identical dropdown behaviour.
	MouseArea {
		id: fieldMouseArea
		anchors.fill: parent
		enabled: control.enabled && !control.editable
		hoverEnabled: true
		preventStealing: true
		z: 1000
		cursorShape: Qt.PointingHandCursor
		onPressed: function(mouse) {
			mouse.accepted = true
			control.forceActiveFocus()
			if (control.popup.visible)
				control.popup.close()
			else
				control.popup.open()
		}
	}
	// Editable combos keep normal text selection/typing, while a tap anywhere in
	// the field also reveals the available presets instead of requiring the
	// narrow chevron target.
	TapHandler {
		enabled: control.enabled && control.editable
		acceptedButtons: Qt.LeftButton
		gesturePolicy: TapHandler.ReleaseWithinBounds
		onTapped: {
			control.forceActiveFocus()
			control.popup.open()
		}
	}
	delegate: ItemDelegate {
		id: optionDelegate
		required property var modelData
		width: control.width
		contentItem: Text {
			text: optionDelegate.modelData
			color: tokens.textPrimary
			font.pixelSize: 13
			verticalAlignment: Text.AlignVCenter
			elide: Text.ElideRight
		}
		background: Rectangle { color: optionDelegate.highlighted ? tokens.surfaceRaised : tokens.surface }
	}
	popup.background: Rectangle {
		color: tokens.surface
		radius: tokens.radiusSmall
		border.width: 1
		border.color: tokens.border
	}
}
