// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
	id: window
	visible: true
	width: 1180
	height: 760
	minimumWidth: 360
	minimumHeight: 620
	title: qsTr("Subsurface Neo Web")
	color: "#06111f"
	property bool compact: width < 760
	property color accent: "#23d4e8"
	property color surface: "#0b1b2d"
	property color surfaceRaised: "#10243a"
	property color border: "#1b3c55"
	property color primaryText: "#f5f9fc"
	property color secondaryText: "#8fa7ba"

	FileDialog {
		id: localLogDialog
		title: qsTr("Open a Subsurface dive log")
		nameFilters: [qsTr("Dive logs (*.xml *.ssrf *.uddf *.subsurface-neo)"), qsTr("All files (*)")]
		onAccepted: webCapabilities.inspectLocalFile(selectedFile)
	}

	component NeoButton: Button {
		id: control
		implicitHeight: 44
		font.pixelSize: 13
		font.weight: Font.DemiBold
		contentItem: Text {
			text: control.text
			color: control.enabled ? window.primaryText : "#566b7d"
			font: control.font
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
		background: Rectangle {
			color: control.down ? "#12344a" : window.surfaceRaised
			radius: 11
			border.width: 1
			border.color: control.enabled ? window.accent : window.border
		}
	}

	component StatusPill: Rectangle {
		property string label
		property bool available
		implicitWidth: statusText.implicitWidth + 24
		implicitHeight: 30
		radius: 15
		color: available ? "#103d3c" : "#17283a"
		border.width: 1
		border.color: available ? "#2bbf91" : window.border
		Text {
			id: statusText
			anchors.centerIn: parent
			text: (parent.available ? "●  " : "○  ") + parent.label
			color: parent.available ? "#5ee2ac" : window.secondaryText
			font.pixelSize: 11
		}
	}

	Rectangle {
		id: sidebar
		visible: !window.compact
		width: 220
		anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
		color: "#04101c"
		border.color: window.border

		ColumnLayout {
			anchors.fill: parent
			anchors.margins: 22
			spacing: 10
			RowLayout {
				Layout.bottomMargin: 24
				Rectangle { width: 38; height: 38; radius: 19; color: "#0b3345"; Text { anchors.centerIn: parent; text: "S"; color: window.accent; font.pixelSize: 26; font.italic: true; font.weight: Font.Bold } }
				Text { text: "SUBSURFACE"; color: window.primaryText; font.pixelSize: 13; font.letterSpacing: 3; font.weight: Font.DemiBold }
			}
			Repeater {
				model: [qsTr("Dashboard"), qsTr("Dives"), qsTr("Dive Sites"), qsTr("Statistics"), qsTr("Equipment"), qsTr("Planner"), qsTr("Settings")]
				delegate: Rectangle {
					required property string modelData
					required property int index
					Layout.fillWidth: true
					height: 45
					radius: 10
					color: index === 0 ? "#0c3043" : "transparent"
					Text { anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }; text: modelData; color: index === 0 ? window.accent : window.secondaryText; font.pixelSize: 13 }
				}
			}
			Item { Layout.fillHeight: true }
			StatusPill { label: qsTr("WebAssembly"); available: webCapabilities.webAssemblyRuntime }
			Text { text: qsTr("Milestone 14 development build"); color: window.secondaryText; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}
	}

	Rectangle {
		id: mobileHeader
		visible: window.compact
		height: 62
		anchors { left: parent.left; right: parent.right; top: parent.top }
		color: "#04101c"
		border.color: window.border
		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: 18
			anchors.rightMargin: 18
			Text { text: "S"; color: window.accent; font.pixelSize: 28; font.italic: true; font.weight: Font.Bold }
			Text { text: "SUBSURFACE"; color: window.primaryText; font.pixelSize: 12; font.letterSpacing: 2; font.weight: Font.DemiBold }
			Item { Layout.fillWidth: true }
			Rectangle { width: 12; height: 12; radius: 6; color: webCapabilities.secureContext ? "#40d58a" : "#f1ae45" }
		}
	}

	Flickable {
		id: contentFlick
		anchors {
			left: window.compact ? parent.left : sidebar.right
			right: parent.right
			top: window.compact ? mobileHeader.bottom : parent.top
			bottom: window.compact ? bottomNav.top : parent.bottom
		}
		contentWidth: width
		contentHeight: contentColumn.implicitHeight + 64
		clip: true
		ScrollBar.vertical: ScrollBar { }

		ColumnLayout {
			id: contentColumn
			x: window.compact ? 16 : 34
			y: window.compact ? 24 : 36
			width: parent.width - (window.compact ? 32 : 68)
			spacing: 18

			RowLayout {
				Layout.fillWidth: true
				ColumnLayout {
					spacing: 2
					Text { text: qsTr("Good evening"); color: window.primaryText; font.pixelSize: window.compact ? 25 : 32; font.weight: Font.DemiBold }
					Text { text: qsTr("Your diving workspace"); color: window.secondaryText; font.pixelSize: 13 }
				}
				Item { Layout.fillWidth: true }
				StatusPill { visible: !window.compact; label: webCapabilities.secureContext ? qsTr("Secure browser") : qsTr("HTTPS required"); available: webCapabilities.secureContext }
			}

			GridLayout {
				Layout.fillWidth: true
				columns: 3
				columnSpacing: 10
				Repeater {
					model: [{ label: qsTr("DIVES"), value: "0" }, { label: qsTr("DIVE TIME"), value: "0 h" }, { label: qsTr("MAX DEPTH"), value: "—" }]
					delegate: Rectangle {
						required property var modelData
						required property int index
						Layout.fillWidth: true
						Layout.preferredHeight: window.compact ? 105 : 128
						radius: 14
						color: window.surface
						border.width: 1
						border.color: window.border
						Column {
							anchors { left: parent.left; leftMargin: window.compact ? 12 : 20; verticalCenter: parent.verticalCenter }
							spacing: 4
							Text { text: modelData.value; color: window.primaryText; font.pixelSize: window.compact ? 24 : 34; font.weight: Font.DemiBold }
							Text { text: modelData.label; color: window.accent; font.pixelSize: window.compact ? 8 : 10; font.letterSpacing: 1 }
						}
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true
				implicitHeight: capabilityContent.implicitHeight + 40
				radius: 16
				color: window.surface
				border.width: 1
				border.color: window.border
				ColumnLayout {
					id: capabilityContent
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
					spacing: 12
					Text { text: qsTr("Browser readiness"); color: window.primaryText; font.pixelSize: 19; font.weight: Font.DemiBold }
					Text { text: qsTr("Neo detects browser capabilities before offering hardware actions. Unsupported mobile browsers will use local files or cloud sync instead of showing a broken connect button."); color: window.secondaryText; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					Flow {
						Layout.fillWidth: true
						spacing: 8
						StatusPill { label: qsTr("Secure context"); available: webCapabilities.secureContext }
						StatusPill { label: qsTr("Web Bluetooth"); available: webCapabilities.webBluetoothAvailable }
						StatusPill { label: qsTr("Web Serial"); available: webCapabilities.webSerialAvailable }
						StatusPill { label: qsTr("Local files"); available: true }
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true
				implicitHeight: importContent.implicitHeight + 40
				radius: 16
				color: window.surface
				border.width: 1
				border.color: window.border
				ColumnLayout {
					id: importContent
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
					spacing: 10
					Text { text: qsTr("Start with your real dive log"); color: window.primaryText; font.pixelSize: 19; font.weight: Font.DemiBold }
					Text { text: qsTr("The browser picker keeps the file under your control. This bootstrap validates the import boundary; canonical parsing and editing will be connected to the shared Subsurface core next."); color: window.secondaryText; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					NeoButton { text: qsTr("Choose local dive log"); Layout.preferredWidth: window.compact ? importContent.width : 230; onClicked: localLogDialog.open() }
					Text { visible: webCapabilities.selectedFileStatus.length > 0; text: webCapabilities.selectedFileStatus; color: window.accent; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
			}

			Text { text: qsTr("Recent dives"); color: window.primaryText; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.topMargin: 4 }
			Rectangle {
				Layout.fillWidth: true
				implicitHeight: 110
				radius: 16
				color: window.surface
				border.width: 1
				border.color: window.border
				Column {
					anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 16 }
					spacing: 6
					Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("No dives loaded"); color: window.primaryText; font.pixelSize: 15; font.weight: Font.DemiBold }
					Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: qsTr("Open a local log or connect a cloud provider when the shared engine bridge is ready."); color: window.secondaryText; font.pixelSize: 11 }
				}
			}
		}
	}

	Rectangle {
		id: bottomNav
		visible: window.compact
		height: 66
		anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
		color: "#04101c"
		border.color: window.border
		RowLayout {
			anchors.fill: parent
			Repeater {
				model: [qsTr("Home"), qsTr("Dives"), qsTr("Sites"), qsTr("Stats"), qsTr("More")]
				delegate: ColumnLayout {
					required property string modelData
					required property int index
					Layout.fillWidth: true
					spacing: 2
					Text { Layout.alignment: Qt.AlignHCenter; text: index === 0 ? "●" : "○"; color: index === 0 ? window.accent : window.secondaryText; font.pixelSize: 14 }
					Text { Layout.alignment: Qt.AlignHCenter; text: modelData; color: index === 0 ? window.accent : window.secondaryText; font.pixelSize: 9 }
				}
			}
		}
	}
}
