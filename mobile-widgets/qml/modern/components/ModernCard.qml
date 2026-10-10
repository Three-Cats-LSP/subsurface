// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Layouts
import ".." as Modern

Rectangle {
	id: card
	Modern.DesignTokens { id: tokens }

	default property alias contentData: content.data
	property int contentPadding: tokens.space16

	color: tokens.surface
	property Gradient oceanGradient: Gradient {
		GradientStop { position: 0; color: tokens.surfaceRaised }
		GradientStop { position: 1; color: tokens.surface }
	}
	gradient: tokens.modernStyle ? oceanGradient : null
	radius: tokens.radiusMedium
	border.width: 1
	border.color: tokens.border
	implicitHeight: content.implicitHeight + contentPadding * 2

	ColumnLayout {
		id: content
		anchors.fill: parent
		anchors.margins: card.contentPadding
		spacing: tokens.space12
	}
}
