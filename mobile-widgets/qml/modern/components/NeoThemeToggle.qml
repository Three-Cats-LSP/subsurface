// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Modern

Control {
	id: control
	Modern.DesignTokens { id: tokens }
	implicitWidth: 44
	implicitHeight: 22
	padding: 2
	Accessible.role: Accessible.Button
	Accessible.name: qsTr("Switch between light and dark theme")

	contentItem: RowLayout {
		spacing: 2
		ToolButton {
			Layout.preferredWidth: 18
			Layout.preferredHeight: 18
			text: "☀"
			font.pixelSize: 11
			Accessible.name: qsTr("Light theme")
			onClicked: subsurfaceTheme.currentTheme = "Blue"
			contentItem: Text { text: parent.text; color: subsurfaceTheme.currentTheme !== "Dark" ? "#FFFFFF" : tokens.textSecondary; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
			background: Rectangle { radius: 9; color: subsurfaceTheme.currentTheme !== "Dark" ? "#F9C846" : "transparent" }
		}
		ToolButton {
			Layout.preferredWidth: 18
			Layout.preferredHeight: 18
			text: "☾"
			font.pixelSize: 12
			Accessible.name: qsTr("Dark theme")
			onClicked: subsurfaceTheme.currentTheme = "Dark"
			contentItem: Text { text: parent.text; color: subsurfaceTheme.currentTheme === "Dark" ? "#FFFFFF" : tokens.textSecondary; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
			background: Rectangle { radius: 9; color: subsurfaceTheme.currentTheme === "Dark" ? "#30445D" : "transparent" }
		}
	}

	background: Rectangle {
		radius: height / 2
		color: tokens.surface
		border.width: 1
		border.color: tokens.border
	}
}
