// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

SpinBox {
	id: control
	Modern.DesignTokens { id: tokens }

	implicitHeight: 44
	editable: false
	Accessible.name: accessibleName
	Accessible.role: Accessible.SpinBox

	contentItem: TextInput {
		z: 2
		text: control.textFromValue(control.value, control.locale)
		color: tokens.textPrimary
		selectionColor: tokens.accentStrong
		selectedTextColor: tokens.textPrimary
		horizontalAlignment: Text.AlignHCenter
		verticalAlignment: Text.AlignVCenter
		readOnly: true
		validator: control.validator
		inputMethodHints: Qt.ImhFormattedNumbersOnly
		font.pixelSize: 13
	}

	up.indicator: Rectangle {
		x: control.mirrored ? 0 : parent.width - width
		height: parent.height
		implicitWidth: 40
		color: control.up.pressed ? tokens.surfaceRaised : "transparent"
		Text { anchors.centerIn: parent; text: "+"; color: control.up.enabled ? tokens.accent : tokens.textMuted; font.pixelSize: 18 }
	}

	down.indicator: Rectangle {
		x: control.mirrored ? parent.width - width : 0
		height: parent.height
		implicitWidth: 40
		color: control.down.pressed ? tokens.surfaceRaised : "transparent"
		Text { anchors.centerIn: parent; text: "−"; color: control.down.enabled ? tokens.accent : tokens.textMuted; font.pixelSize: 18 }
	}

	background: Rectangle {
		color: tokens.background
		radius: tokens.radiusSmall
		border.width: 1
		border.color: control.activeFocus ? tokens.accent : tokens.border
	}
}
