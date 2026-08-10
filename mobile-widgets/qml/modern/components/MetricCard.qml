// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Layouts
import ".." as Modern

ModernCard {
	id: metric
	Modern.DesignTokens { id: tokens }

	property string label: ""
	property string value: "—"
	property string suffix: ""

	implicitWidth: 150

	Text {
		text: metric.label.toUpperCase()
		color: tokens.textSecondary
		font.pixelSize: 12
		font.weight: Font.DemiBold
		font.letterSpacing: 0.8
		Layout.fillWidth: true
	}

	RowLayout {
		spacing: tokens.space4
		Layout.fillWidth: true

		Text {
			text: metric.value
			color: tokens.textPrimary
			font.pixelSize: 28
			font.weight: Font.DemiBold
		}
		Text {
			visible: metric.suffix.length > 0
			text: metric.suffix
			color: tokens.textSecondary
			font.pixelSize: 13
			Layout.alignment: Qt.AlignBottom
			Layout.bottomMargin: 4
		}
	}
}
