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
