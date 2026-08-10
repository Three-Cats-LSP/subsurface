// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.subsurfacedivelog.mobile 1.0

Kirigami.ScrollablePage {
	id: aboutPage
	property int pageWidth: aboutPage.width - aboutPage.leftPadding - aboutPage.rightPadding
	title: qsTr("About Subsurface Neo")
	background: Rectangle { color: subsurfaceTheme.backgroundColor }

	function openCloudSyncPage() {
		var component = Qt.createComponent("qrc:/qml/modern/pages/CloudSyncPage.qml")
		if (component.status !== Component.Ready) {
			showPassiveNotification(qsTr("Unable to load Cloud & Sync: %1").arg(component.errorString()), 6000)
			return
		}
		var cloudPage = component.createObject(rootItem)
		if (cloudPage === null) {
			showPassiveNotification(qsTr("Unable to create Cloud & Sync page"), 6000)
			return
		}
		showPage(cloudPage)
	}

	function openModernPreview() {
		var component = Qt.createComponent("qrc:/qml/modern/pages/ModernDashboard.qml")
		if (component.status !== Component.Ready) {
			showPassiveNotification(qsTr("Unable to load Subsurface Neo Preview: %1").arg(component.errorString()), 6000)
			return
		}

		var dashboard = component.createObject(rootItem)
		if (dashboard === null) {
			showPassiveNotification(qsTr("Unable to create Subsurface Neo Preview"), 6000)
			return
		}

		dashboard.openDiveList.connect(function() {
			rootItem.returnTopPage()
		})
		dashboard.openImport.connect(function() {
			downloadFromDc.dcImportModel.clearTable()
			showPageFromDrawer(downloadFromDc)
		})
		dashboard.openCloudSync.connect(function() {
			aboutPage.openCloudSyncPage()
		})
		showPage(dashboard)
	}

	ColumnLayout {
		spacing: Kirigami.Units.largeSpacing
		width: aboutPage.width
		Layout.margins: Kirigami.Units.gridUnit / 2

		Kirigami.Heading {
			text: qsTr("About Subsurface Neo")
			color: subsurfaceTheme.textColor
			Layout.topMargin: Kirigami.Units.gridUnit
			Layout.alignment: Qt.AlignHCenter
			Layout.maximumWidth: pageWidth
			wrapMode: TextEdit.NoWrap
			fontSizeMode: Text.Fit
		}
		Image {
			id: image
			source: "qrc:/qml/subsurface-mobile-icon.png"
			fillMode: Image.PreserveAspectCrop
			Layout.alignment: Qt.AlignHCenter + Qt.AlignVCenter
			Layout.maximumWidth: pageWidth / 2
			Layout.maximumHeight: Layout.maximumWidth
		}

		Kirigami.Heading {
			text: qsTr("A modern cross-platform interface built on the mature Subsurface dive log engine.\n") +
				qsTr("View, edit, analyze and synchronize your dives.")
			level: 4
			color: subsurfaceTheme.textColor
			Layout.alignment: Qt.AlignHCenter
			Layout.topMargin: Kirigami.Units.largeSpacing * 3
			Layout.maximumWidth: pageWidth
			wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
			anchors.horizontalCenter: parent.Center
			horizontalAlignment: Text.AlignHCenter
		}

		Kirigami.Heading {
			text: qsTr("Version: %1\n\nBased on Subsurface\nGPL-2.0").arg(manager.getVersion())
			level: 5
			color: subsurfaceTheme.textColor
			font.pointSize: subsurfaceTheme.smallPointSize + 1
			Layout.alignment: Qt.AlignHCenter
			Layout.topMargin: Kirigami.Units.largeSpacing
			Layout.maximumWidth: pageWidth
			wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
			anchors.horizontalCenter: parent.Center
			horizontalAlignment: Text.AlignHCenter
		}
		TemplateButton {
			id: modernPreviewButton
			Layout.alignment: Qt.AlignHCenter
			text: qsTr("Open Subsurface Neo Preview")
			onClicked: aboutPage.openModernPreview()
		}
		TemplateButton {
			id: copyAppLogToClipboard
			Layout.alignment: Qt.AlignHCenter
			text: qsTr("Copy logs to clipboard")
			onClicked: {
				manager.copyAppLogToClipboard()
				rootItem.returnTopPage()
			}
		}
	}
}
