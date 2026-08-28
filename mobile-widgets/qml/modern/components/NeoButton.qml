// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import ".." as Modern

Button {
	id: control

	property string variant: "secondary"
	property bool compact: false

	Modern.DesignTokens { id: tokens }

	implicitHeight: compact ? 34 : 40
	implicitWidth: Math.max(compact ? 70 : 92, contentItem.implicitWidth + leftPadding + rightPadding)
	leftPadding: compact ? tokens.space12 : tokens.space16
	rightPadding: leftPadding
	topPadding: 0
	bottomPadding: 0
	Accessible.name: accessibleName.length > 0 ? accessibleName : text
	Accessible.role: Accessible.Button

	contentItem: Text {
		text: control.text
		color: control.variant === "primary" ? tokens.background
			: control.variant === "danger" ? "#FF7B86"
			: control.variant === "ghost" ? tokens.accent
			: tokens.textPrimary
		font.pixelSize: control.compact ? 11 : 13
		font.weight: Font.DemiBold
		horizontalAlignment: Text.AlignHCenter
		verticalAlignment: Text.AlignVCenter
		elide: Text.ElideRight
	}

	background: Rectangle {
		radius: control.compact ? tokens.radiusSmall : tokens.radius12
		color: {
			if (!control.enabled)
				return tokens.surfaceRaised
			if (control.variant === "primary")
				return control.down ? tokens.accentStrong : tokens.accent
			if (control.variant === "ghost")
				return control.down || control.hovered ? tokens.surfaceRaised : "transparent"
			if (control.variant === "danger")
				return control.down || control.hovered ? "#38212B" : "transparent"
			return control.down || control.hovered ? tokens.surfaceRaised : tokens.surface
		}
		border.width: control.variant === "primary" ? 0 : 1
		border.color: control.variant === "danger" ? "#713746"
			: control.variant === "ghost" ? "transparent" : tokens.border
		opacity: control.enabled ? 1 : 0.45
	}
}
