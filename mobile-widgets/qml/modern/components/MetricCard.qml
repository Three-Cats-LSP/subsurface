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
	property string iconName: ""

	implicitWidth: 150
	implicitHeight: 86
	contentPadding: tokens.space12

	Text {
		Layout.fillWidth: true
		text: metric.label.toUpperCase()
		color: tokens.textMuted
		font.pixelSize: 9
		font.weight: Font.DemiBold
		font.letterSpacing: 0.7
		horizontalAlignment: Text.AlignHCenter
	}

	RowLayout {
		spacing: tokens.space8
		Layout.alignment: Qt.AlignHCenter

		NeoDiveIcon {
			visible: metric.iconName.length > 0
			name: metric.iconName
			iconColor: tokens.accent
			Layout.preferredWidth: 26
			Layout.preferredHeight: 26
		}

		Text {
			text: metric.value
			color: tokens.textPrimary
			font.pixelSize: 22
			font.weight: Font.DemiBold
		}
		Text {
			visible: metric.suffix.length > 0
			text: metric.suffix
			color: tokens.accent
			font.pixelSize: 10
			Layout.alignment: Qt.AlignBottom
			Layout.bottomMargin: 3
		}
	}
}
