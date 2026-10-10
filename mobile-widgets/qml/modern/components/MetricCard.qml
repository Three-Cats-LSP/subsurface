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
	property bool showcase: false

	implicitWidth: 150
	implicitHeight: showcase ? 146 : 86
	contentPadding: tokens.space12

	Text {
		visible: !metric.showcase
		Layout.fillWidth: true
		text: metric.label.toUpperCase()
		color: tokens.textMuted
		font.pixelSize: 9
		font.weight: Font.DemiBold
		font.letterSpacing: 0.7
		horizontalAlignment: Text.AlignHCenter
	}
	Rectangle {
		visible: metric.showcase
		Layout.preferredWidth: 38
		Layout.preferredHeight: 38
		radius: 19
		color: tokens.surfaceRaised
		NeoDiveIcon { anchors.centerIn: parent; name: metric.iconName; iconColor: tokens.accent; width: 22; height: 22 }
	}

	RowLayout {
		spacing: tokens.space8
		Layout.alignment: metric.showcase ? Qt.AlignLeft : Qt.AlignHCenter

		NeoDiveIcon {
			visible: !metric.showcase && metric.iconName.length > 0
			name: metric.iconName
			iconColor: tokens.accent
			Layout.preferredWidth: 26
			Layout.preferredHeight: 26
		}

		Text {
			text: metric.value
			color: tokens.textPrimary
			font.pixelSize: metric.showcase ? (metric.iconName === "temperature" ? Math.min(22, Math.max(13, metric.width / 14)) : 34) : 22
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
	Text {
		visible: metric.showcase
		Layout.fillWidth: true
		text: metric.label.toUpperCase()
		color: tokens.accent
		font.pixelSize: 9
		font.letterSpacing: 0.7
		wrapMode: Text.WordWrap
	}
}
