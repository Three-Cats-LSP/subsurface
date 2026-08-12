// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Export & portability")
	background: Rectangle { color: tokens.background }
	signal openExport()
	signal openRecovery()
	signal openCloudBackup()
	Modern.DesignTokens { id: tokens }
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Keep your log portable"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Subsurface XML remains your canonical dive data. Neo’s collections and equipment metadata remain separate so exports stay compatible with Subsurface."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Components.ModernCard { Layout.fillWidth: true; Text { text: qsTr("Export a compatible copy"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }; Text { text: qsTr("Export the complete dive log or dive-site XML using Subsurface’s existing fidelity-preserving exporters."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }; Button { Layout.fillWidth: true; text: qsTr("Open export options"); onClicked: page.openExport() } }
		Components.ModernCard { Layout.fillWidth: true; Text { text: qsTr("Cloud backup"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }; Text { text: qsTr("Create an explicit provider backup through Neo Cloud & Sync. This never silently replaces the local log."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }; Button { Layout.fillWidth: true; text: qsTr("Open Cloud & Sync"); onClicked: page.openCloudBackup() } }
		Components.ModernCard { Layout.fillWidth: true; Text { text: qsTr("Recover a cached copy"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }; Text { text: qsTr("Inspect and import a known cloud cache through the mature recovery process."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }; Button { Layout.fillWidth: true; text: qsTr("Open recovery"); onClicked: page.openRecovery() } }
		Text { text: qsTr("Restore and replace workflows remain explicitly confirmed operations. Neo will not overwrite a diverged log automatically."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
