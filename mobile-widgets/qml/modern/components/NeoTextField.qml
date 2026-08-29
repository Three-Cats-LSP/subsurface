// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

TextField {
	id: control
	property string accessibleName: ""
	Modern.DesignTokens { id: tokens }
	readonly property bool floatingLabelVisible: placeholderText.length > 0 && (activeFocus || text.length > 0)

	implicitHeight: 46
	leftPadding: tokens.space12
	rightPadding: tokens.space12
	color: tokens.textPrimary
	// The Material style floats placeholders onto the outline, but our custom
	// background cannot cut a gap in that outline. Hide the style label while
	// floating and draw a backed Neo label below instead.
	placeholderTextColor: control.floatingLabelVisible ? "transparent" : tokens.textMuted
	selectionColor: tokens.accentStrong
	selectedTextColor: tokens.textPrimary
	font.pixelSize: 13
	Accessible.name: accessibleName.length > 0 ? accessibleName : placeholderText
	Accessible.role: Accessible.EditableText

	background: Rectangle {
		color: tokens.background
		radius: tokens.radiusSmall
		border.width: 1
		border.color: control.activeFocus ? tokens.accent : tokens.border
	}

	Rectangle {
		id: floatingLabelBackground
		visible: control.floatingLabelVisible
		z: 3
		x: tokens.space8
		y: -Math.round(height / 2) + 1
		width: Math.min(floatingLabel.implicitWidth, Math.max(0, control.width - tokens.space24)) + tokens.space8
		height: floatingLabel.implicitHeight
		color: tokens.background

		Text {
			id: floatingLabel
			anchors.centerIn: parent
			text: control.placeholderText
			color: control.activeFocus ? tokens.accent : tokens.textMuted
			font.pixelSize: 10
			elide: Text.ElideRight
			width: Math.max(0, parent.width - tokens.space8)
		}
	}
}
