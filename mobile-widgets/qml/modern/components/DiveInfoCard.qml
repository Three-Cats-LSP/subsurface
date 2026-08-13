// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Layouts
import ".." as Modern

ModernCard {
	id: infoCard
	Modern.DesignTokens { id: tokens }

	property string label: ""
	property string value: "—"
	property string detail: ""
	property string iconName: ""

	implicitHeight: 84
	contentPadding: tokens.space12

	RowLayout {
		Layout.fillWidth: true
		Layout.fillHeight: true
		spacing: tokens.space8

		Rectangle {
			Layout.preferredWidth: 38
			Layout.preferredHeight: 38
			Layout.alignment: Qt.AlignVCenter
			radius: width / 2
			color: Qt.rgba(0.13, 0.83, 0.92, 0.09)

			NeoDiveIcon {
				anchors.centerIn: parent
				width: 24
				height: 24
				name: infoCard.iconName
				iconColor: tokens.accent
			}
		}

		ColumnLayout {
			Layout.fillWidth: true
			spacing: 2

			Text {
				Layout.fillWidth: true
				text: infoCard.label.toUpperCase()
				color: tokens.textMuted
				font.pixelSize: 9
				font.weight: Font.DemiBold
				font.letterSpacing: 0.7
			}
			Text {
				Layout.fillWidth: true
				text: infoCard.value
				color: tokens.accent
				font.pixelSize: 15
				font.weight: Font.DemiBold
				elide: Text.ElideRight
			}
			Text {
				visible: infoCard.detail.length > 0
				Layout.fillWidth: true
				text: infoCard.detail
				color: tokens.textSecondary
				font.pixelSize: 10
				elide: Text.ElideRight
			}
		}
	}
}
