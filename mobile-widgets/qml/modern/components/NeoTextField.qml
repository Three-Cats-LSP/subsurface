// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

TextField {
	id: control
	property string accessibleName: ""
	Modern.DesignTokens { id: tokens }

	implicitHeight: 46
	leftPadding: tokens.space12
	rightPadding: tokens.space12
	color: tokens.textPrimary
	placeholderTextColor: tokens.textMuted
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
}
