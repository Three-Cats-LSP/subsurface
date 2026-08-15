// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

TextArea {
	id: control
	Modern.DesignTokens { id: tokens }

	implicitHeight: 92
	leftPadding: tokens.space12
	rightPadding: tokens.space12
	topPadding: tokens.space12
	bottomPadding: tokens.space12
	color: tokens.textPrimary
	placeholderTextColor: tokens.textMuted
	selectionColor: tokens.accentStrong
	selectedTextColor: tokens.textPrimary
	font.pixelSize: 13
	wrapMode: TextEdit.Wrap

	background: Rectangle {
		color: tokens.background
		radius: tokens.radiusSmall
		border.width: 1
		border.color: control.activeFocus ? tokens.accent : tokens.border
	}
}
