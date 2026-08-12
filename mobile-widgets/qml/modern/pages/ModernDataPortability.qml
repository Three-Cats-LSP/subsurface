// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import org.kde.kirigami as Kirigami
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Export & portability")
	background: Rectangle { color: tokens.background }
	signal openCloudBackup()
	Modern.DesignTokens { id: tokens }
	property int selectedExport: ExportType.EX_DIVES_XML
	property bool anonymize: false
	property string selectedCache: ""
	property string selectedBackup: ""
	property var backupInspection: ({})
	FolderDialog {
		id: exportFolder
		currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
		onAccepted: manager.exportToFile(page.selectedExport, selectedFolder, page.anonymize)
	}
	FileDialog {
		id: backupFile
		nameFilters: [qsTr("Subsurface dive logs (*.xml *.ssrf *.xml.gz)"), qsTr("All files (*)")]
		onAccepted: {
			page.selectedBackup = selectedFile.toString()
			page.backupInspection = manager.inspectDiveLogFile(page.selectedBackup)
			backupConfirm.open()
		}
	}
	Dialog {
		id: backupConfirm
		parent: page
		modal: true
		title: qsTr("Merge backup into current log?")
		contentItem: Label { width: 360; wrapMode: Text.WordWrap; text: backupInspection.error ? backupInspection.error : qsTr("%1 contains %2 dives and %3 sites. Imported dives will be merged with the current log using Subsurface deduplication; nothing is replaced automatically.").arg(backupInspection.fileName).arg(backupInspection.dives).arg(backupInspection.sites) }
		footer: DialogButtonBox { Button { text: qsTr("Merge backup"); enabled: !backupInspection.error; onClicked: { if (manager.importDiveLogFile(page.selectedBackup)) backupConfirm.close() } }; Button { text: qsTr("Cancel"); onClicked: backupConfirm.close() } }
	}
	Dialog {
		id: cacheConfirm
		parent: page
		modal: true
		title: qsTr("Import cached log?")
		standardButtons: Dialog.Cancel
		contentItem: Label { width: 360; wrapMode: Text.WordWrap; text: qsTr("Import '%1' as a recovery source? Review the resulting log before saving or syncing.").arg(page.selectedCache) }
		footer: DialogButtonBox { Button { text: qsTr("Import cache"); onClicked: { manager.importCacheRepo(page.selectedCache); cacheConfirm.close() } }; Button { text: qsTr("Cancel"); onClicked: cacheConfirm.close() } }
	}
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Keep your log portable"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Your Subsurface XML remains canonical. Neo data stays alongside it, so an exported log remains usable in Subsurface."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Compatible export / local backup"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Create a portable XML copy of the complete dive log or the dive-site catalogue through Subsurface's fidelity-preserving exporter."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			ComboBox { Layout.fillWidth: true; model: [qsTr("Complete dive log (Subsurface XML)"), qsTr("Dive sites (Subsurface XML)")]; onActivated: page.selectedExport = currentIndex === 0 ? ExportType.EX_DIVES_XML : ExportType.EX_DIVE_SITES_XML }
			CheckBox { text: qsTr("Anonymize export"); checked: page.anonymize; onToggled: page.anonymize = checked }
			Button { Layout.fillWidth: true; text: Qt.platform.os === "android" || Qt.platform.os === "ios" ? qsTr("Share export") : qsTr("Choose folder and export"); onClicked: { if (Qt.platform.os === "android" || Qt.platform.os === "ios") manager.shareViaEmail(page.selectedExport, page.anonymize); else exportFolder.open() } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Restore from a local backup"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Inspect a Subsurface-compatible backup before merging its dives and sites. Existing dives are protected by the mature import/deduplication workflow."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Button { Layout.fillWidth: true; text: qsTr("Choose backup to inspect"); onClicked: backupFile.open() }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Cloud backup"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Create an explicit provider backup through Cloud & Sync. Neo never silently replaces a diverged local log."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Button { Layout.fillWidth: true; text: qsTr("Open Cloud & Sync"); onClicked: page.openCloudBackup() }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Recover a cloud cache"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { visible: manager.cloudCacheList.length === 0; text: qsTr("No recoverable cloud caches are currently known on this device."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Repeater { model: manager.cloudCacheList; delegate: Button { required property string modelData; Layout.fillWidth: true; text: modelData; onClicked: { page.selectedCache = modelData; cacheConfirm.open() } } }
		}
		Text { text: qsTr("Restoring data is always an explicit action. Inspect the imported log and resolve any differences before saving or syncing."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
