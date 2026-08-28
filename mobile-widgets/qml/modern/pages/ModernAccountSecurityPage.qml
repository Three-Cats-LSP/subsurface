// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Accounts & security")
	background: Rectangle { color: tokens.background }
	property bool wideLayout: width >= 760
	signal openCloudSync()
	signal openSubsurfaceCloud()
	Modern.DesignTokens { id: tokens }

	function subsurfaceStatus() {
		if (Backend.cloud_verification_status === Enums.CS_VERIFIED)
			return qsTr("Connected as %1").arg(PrefCloudStorage.cloud_storage_email)
		if (Backend.cloud_verification_status === Enums.CS_NOCLOUD)
			return qsTr("Not connected")
		if (Backend.cloud_verification_status === Enums.CS_NEED_TO_VERIFY)
			return qsTr("Verification required")
		return qsTr("Setup incomplete")
	}
	function connectedProviderCount() {
		var count = 0
		for (var i = 0; i < CloudSync.providers.length; ++i)
			if (CloudSync.providers[i].connected)
				++count
		return count
	}
	function credentialProtection() {
		if (Qt.platform.os === "android")
			return qsTr("OAuth tokens are encrypted with an AES-GCM key held by Android Keystore.")
		if (Qt.platform.os === "windows")
			return qsTr("OAuth tokens are stored by Windows Credential Manager.")
		return qsTr("OAuth tokens are kept only in memory when a secure native credential store is unavailable.")
	}

	ColumnLayout {
		width: page.availableWidth; spacing: tokens.space16
		ColumnLayout {
			Layout.fillWidth: true; spacing: 2
			Text { text: qsTr("Accounts & security"); color: tokens.textPrimary; font.pixelSize: page.wideLayout ? 30 : 25; font.weight: Font.DemiBold }
			Text { text: qsTr("Connections, credentials, privacy, and destructive actions"); color: tokens.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}

		GridLayout {
			Layout.fillWidth: true; columns: page.wideLayout ? 2 : 1; columnSpacing: tokens.space12; rowSpacing: tokens.space12
			Components.ModernCard {
				Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Google Drive & Dropbox"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: qsTr("%1 of %2 providers connected").arg(page.connectedProviderCount()).arg(CloudSync.providers.length); color: page.connectedProviderCount() > 0 ? tokens.success : tokens.textSecondary; font.pixelSize: 12 }
				Text { text: page.credentialProtection(); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Manage providers"); variant: "primary"; onClicked: page.openCloudSync() }
			}

			Components.ModernCard {
				Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Subsurface Cloud"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: page.subsurfaceStatus(); color: Backend.cloud_verification_status === Enums.CS_VERIFIED ? tokens.success : tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
				Text { text: Qt.platform.os === "android" || Qt.platform.os === "windows" ? qsTr("Compatibility service for the original Subsurface ecosystem. Its saved password uses the same secure native credential store described above.") : qsTr("Compatibility service for the original Subsurface ecosystem. Secure persistent password storage is unavailable on this platform."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Components.NeoButton { Layout.fillWidth: true; text: Backend.cloud_verification_status === Enums.CS_VERIFIED ? qsTr("Change account") : qsTr("Connect"); onClicked: page.openSubsurfaceCloud() }
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Privacy & data flow"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout {
				Layout.fillWidth: true; columns: page.wideLayout ? 2 : 1; columnSpacing: tokens.space16; rowSpacing: tokens.space12
				ColumnLayout { Layout.fillWidth: true; Text { text: qsTr("YOUR LOG"); color: tokens.accent; font.pixelSize: 10; font.weight: Font.DemiBold } Text { text: qsTr("Dive data stays in your chosen local file or connected cloud provider. Provider synchronization uses revision manifests and checksums to prevent silent conflict overwrites."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
				ColumnLayout { Layout.fillWidth: true; Text { text: qsTr("NETWORK USE"); color: tokens.accent; font.pixelSize: 10; font.weight: Font.DemiBold } Text { text: qsTr("Network access is used for services you invoke, cloud synchronization, maps, and update checks. Neo does not add a separate usage-analytics service."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
				ColumnLayout { Layout.fillWidth: true; Text { text: qsTr("DISCONNECT"); color: tokens.accent; font.pixelSize: 10; font.weight: Font.DemiBold } Text { text: qsTr("Disconnecting Google Drive or Dropbox removes its OAuth tokens and saved sync-state credentials from this device. It does not delete the provider's remote app folder."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
				ColumnLayout { Layout.fillWidth: true; Text { text: qsTr("EXPORTS"); color: tokens.accent; font.pixelSize: 10; font.weight: Font.DemiBold } Text { text: qsTr("Exported and backup files are normal user-controlled files. Their safety depends on where you save or share them."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
			}
		}

		Components.ModernCard {
			visible: Backend.cloud_verification_status === Enums.CS_VERIFIED
			Layout.fillWidth: true
			Text { text: qsTr("Danger zone"); color: tokens.warning; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Deleting the Subsurface Cloud account permanently removes that server account. This is different from disconnecting Google Drive or Dropbox and cannot be undone."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoButton { text: qsTr("Delete Subsurface Cloud account"); variant: "danger"; onClicked: { confirmationField.text = ""; deleteDialog.open() } }
		}
	}

	Dialog {
		id: deleteDialog
		parent: Overlay.overlay
		anchors.centerIn: parent
		modal: true
		width: Math.min(page.width - tokens.space32, 520)
		title: qsTr("Permanently delete account?")
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.warning }
		contentItem: ColumnLayout {
			spacing: tokens.space12
			Text { text: qsTr("Account: %1").arg(PrefCloudStorage.cloud_storage_email); color: tokens.textPrimary; font.weight: Font.DemiBold; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
			Text { text: qsTr("Make a local backup first. Then type DELETE to confirm permanent server-side account deletion."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoTextField { id: confirmationField; Layout.fillWidth: true; placeholderText: qsTr("Type DELETE") }
		}
		footer: RowLayout {
			spacing: tokens.space8
			Item { Layout.fillWidth: true }
			Components.NeoButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: deleteDialog.close() }
			Components.NeoButton {
				text: qsTr("Delete permanently")
				variant: "danger"
				enabled: confirmationField.text.trim() === "DELETE"
				onClicked: {
					deleteDialog.close()
					manager.appendTextToLog("Subsurface Cloud account deletion confirmed in Neo")
					manager.deleteAccount()
					if (Backend.cloud_verification_status === Enums.CS_NOCLOUD)
						showPassiveNotification(qsTr("Subsurface Cloud account deleted"), 5000)
					else
						showPassiveNotification(qsTr("Account deletion did not complete: %1").arg(manager.startPageText), 7000)
				}
			}
		}
	}
}
