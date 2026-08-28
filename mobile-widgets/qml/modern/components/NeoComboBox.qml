// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

ComboBox {
	id: control
	Modern.DesignTokens { id: tokens }
	implicitHeight: 44
	leftPadding: 12
	rightPadding: 34
	font.pixelSize: 13
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
		Accessible.name: control.accessibleName.length > 0 ? control.accessibleName : control.displayText
	}
	indicator: Text {
		x: control.width - width - 12
		y: (control.height - height) / 2
		text: "⌄"
		color: tokens.accent
		font.pixelSize: 16
	}
	background: Rectangle {
		color: control.down ? tokens.surfaceRaised : tokens.background
		radius: tokens.radiusSmall
		border.width: 1
		border.color: control.activeFocus ? tokens.accent : tokens.border
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
