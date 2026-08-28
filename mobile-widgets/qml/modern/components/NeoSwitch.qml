// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

Switch {
	id: control
	Modern.DesignTokens { id: tokens }

	implicitHeight: 40
	spacing: 10
	Accessible.name: accessibleName.length > 0 ? accessibleName : text
	Accessible.role: Accessible.CheckBox

	indicator: Rectangle {
		implicitWidth: 42
		implicitHeight: 24
		x: control.mirrored ? control.width - width : 0
		y: (control.height - height) / 2
		radius: height / 2
		color: control.checked ? tokens.accentStrong : tokens.surfaceRaised
		border.width: 1
		border.color: control.checked ? tokens.accent : tokens.border

		Rectangle {
			width: 18
			height: 18
			y: 3
			x: control.checked ? parent.width - width - 3 : 3
			radius: 9
			color: control.checked ? tokens.textPrimary : tokens.textMuted
			Behavior on x { NumberAnimation { duration: 120 } }
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
