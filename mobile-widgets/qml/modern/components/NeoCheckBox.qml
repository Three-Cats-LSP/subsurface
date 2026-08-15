// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

CheckBox {
	id: control
	Modern.DesignTokens { id: tokens }

	implicitHeight: 38
	spacing: 9

	indicator: Rectangle {
		implicitWidth: 21
		implicitHeight: 21
		x: control.mirrored ? control.width - width : 0
		y: (control.height - height) / 2
		radius: 5
		color: control.checked ? tokens.accentStrong : tokens.background
		border.width: 2
		border.color: control.checked ? tokens.accent : tokens.border
		Text {
			anchors.centerIn: parent
			text: "✓"
			visible: control.checked
			color: tokens.textPrimary
			font.pixelSize: 14
			font.weight: Font.Bold
		}
	}

	contentItem: Text {
		leftPadding: control.mirrored ? 0 : control.indicator.width + control.spacing
		rightPadding: control.mirrored ? control.indicator.width + control.spacing : 0
		text: control.text
		color: control.enabled ? tokens.textPrimary : tokens.textMuted
		font.pixelSize: 12
		verticalAlignment: Text.AlignVCenter
		wrapMode: Text.WordWrap
	}
}
