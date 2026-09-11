// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Modern

Control {
	id: control
	Modern.DesignTokens { id: tokens }
	implicitWidth: 70
	implicitHeight: 34
	padding: 3
	Accessible.role: Accessible.Button
	Accessible.name: qsTr("Switch between light and dark theme")

	contentItem: RowLayout {
		spacing: 2
		ToolButton {
			Layout.preferredWidth: 30
			Layout.preferredHeight: 28
			text: "☀"
			font.pixelSize: 16
			Accessible.name: qsTr("Light theme")
			onClicked: subsurfaceTheme.currentTheme = "Blue"
			contentItem: Text { text: parent.text; color: subsurfaceTheme.currentTheme !== "Dark" ? "#FFFFFF" : tokens.textSecondary; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
			background: Rectangle { radius: 14; color: subsurfaceTheme.currentTheme !== "Dark" ? "#F9C846" : "transparent" }
		}
		ToolButton {
			Layout.preferredWidth: 30
			Layout.preferredHeight: 28
			text: "☾"
			font.pixelSize: 17
			Accessible.name: qsTr("Dark theme")
			onClicked: subsurfaceTheme.currentTheme = "Dark"
			contentItem: Text { text: parent.text; color: subsurfaceTheme.currentTheme === "Dark" ? "#FFFFFF" : tokens.textSecondary; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
			background: Rectangle { radius: 14; color: subsurfaceTheme.currentTheme === "Dark" ? "#30445D" : "transparent" }
		}
	}

	background: Rectangle {
		radius: height / 2
		color: tokens.surface
		border.width: 1
		border.color: tokens.border
	}
}
