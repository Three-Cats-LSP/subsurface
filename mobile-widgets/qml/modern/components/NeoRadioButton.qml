// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

RadioButton {
	id: control
	Modern.DesignTokens { id: tokens }

	implicitHeight: 38
	spacing: 8

	indicator: Rectangle {
		implicitWidth: 20
		implicitHeight: 20
		x: control.mirrored ? control.width - width : 0
		y: (control.height - height) / 2
		radius: 10
		color: tokens.background
		border.width: 2
		border.color: control.checked ? tokens.accent : tokens.border
		Rectangle {
			anchors.centerIn: parent
			width: 10
			height: 10
			radius: 5
			color: tokens.accent
			visible: control.checked
		}
	}

	contentItem: Text {
		leftPadding: control.mirrored ? 0 : control.indicator.width + control.spacing
		rightPadding: control.mirrored ? control.indicator.width + control.spacing : 0
		text: control.text
		color: control.enabled ? tokens.textPrimary : tokens.textMuted
		font.pixelSize: 11
		verticalAlignment: Text.AlignVCenter
		elide: Text.ElideRight
	}
}
