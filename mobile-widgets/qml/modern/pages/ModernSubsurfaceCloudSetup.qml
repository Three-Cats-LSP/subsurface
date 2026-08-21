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
	title: qsTr("Subsurface Cloud")
	background: Rectangle { color: tokens.background }
	signal useLocalLog()
	signal returnRequested()
	property bool needsPin: Backend.cloud_verification_status === Enums.CS_NEED_TO_VERIFY
	property bool credentialsRejected: Backend.cloud_verification_status === Enums.CS_INCORRECT_USER_PASSWD
	property bool passwordVisible: false
	Modern.DesignTokens { id: tokens }

	function submitCredentials() {
		PrefCloudStorage.cloud_auto_sync = true
		manager.saveCloudCredentials(emailField.text.trim().toLowerCase(), passwordField.text, "")
	}
	function submitPin() {
		manager.saveCloudCredentials(emailField.text.trim().toLowerCase(), passwordField.text, pinField.text.trim())
	}

	ColumnLayout {
		width: page.availableWidth; spacing: tokens.space16
		ColumnLayout {
			Layout.fillWidth: true; spacing: 3
			Text { text: qsTr("Subsurface Cloud"); color: tokens.textPrimary; font.pixelSize: 28; font.weight: Font.DemiBold }
			Text { text: qsTr("Compatibility access for existing Subsurface Cloud logs"); color: tokens.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}

		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout {
				Layout.fillWidth: true; spacing: tokens.space12
				Rectangle {
					Layout.preferredWidth: 44; Layout.preferredHeight: 44; radius: 22
					color: page.credentialsRejected ? "#38212B" : "#083449"
					Components.NeoDiveIcon { anchors.centerIn: parent; width: 24; height: 24; name: page.needsPin ? "lock" : "cloud"; iconColor: page.credentialsRejected ? tokens.warning : tokens.accent }
				}
				ColumnLayout {
					Layout.fillWidth: true; spacing: 2
					Text { text: page.needsPin ? qsTr("Verify your account") : qsTr("Sign in or register"); color: tokens.textPrimary; font.pixelSize: 19; font.weight: Font.DemiBold }
					Text { text: page.needsPin ? qsTr("One final security step") : qsTr("Subsurface Cloud compatibility account"); color: page.credentialsRejected ? tokens.warning : tokens.accent; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
			}
			Text {
				text: page.needsPin ? qsTr("A verification PIN was sent to %1. Enter it below to complete registration.").arg(PrefCloudStorage.cloud_storage_email) : qsTr("Use the same lowercase email and password as the original Subsurface applications. New accounts are registered with the entered address.")
				color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
			}
			ColumnLayout {
				visible: !page.needsPin; Layout.fillWidth: true; spacing: tokens.space8
				Text { text: qsTr("EMAIL"); color: tokens.textMuted; font.pixelSize: 9 }
				Components.NeoTextField {
					id: emailField; Layout.fillWidth: true
					text: PrefCloudStorage.cloud_storage_email; placeholderText: qsTr("Email")
					inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
					onAccepted: passwordField.forceActiveFocus()
				}
				Text { text: qsTr("PASSWORD"); color: tokens.textMuted; font.pixelSize: 9 }
				RowLayout {
					Layout.fillWidth: true; spacing: tokens.space8
					Components.NeoTextField {
						id: passwordField; Layout.fillWidth: true; placeholderText: qsTr("Password")
						echoMode: page.passwordVisible ? TextInput.Normal : TextInput.Password
						inputMethodHints: Qt.ImhSensitiveData | Qt.ImhHiddenText | Qt.ImhNoAutoUppercase
						onAccepted: if (emailField.text.trim().length > 0 && passwordField.text.length > 0) page.submitCredentials()
					}
					Components.NeoButton { compact: true; text: page.passwordVisible ? qsTr("Hide") : qsTr("Show"); variant: "ghost"; onClicked: page.passwordVisible = !page.passwordVisible }
				}
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Sign in or register"); variant: "primary"; enabled: emailField.text.trim().length > 0 && passwordField.text.length > 0; onClicked: page.submitCredentials() }
			}
			ColumnLayout {
				visible: page.needsPin; Layout.fillWidth: true; spacing: tokens.space8
				Text { text: qsTr("VERIFICATION PIN"); color: tokens.textMuted; font.pixelSize: 9 }
				Components.NeoTextField { id: pinField; Layout.fillWidth: true; placeholderText: qsTr("Verification PIN"); inputMethodHints: Qt.ImhDigitsOnly; onAccepted: if (pinField.text.trim().length > 0) page.submitPin() }
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Verify account"); variant: "primary"; enabled: pinField.text.trim().length > 0; onClicked: page.submitPin() }
			}
		}

		Components.ModernCard {
			visible: manager.startPageText.length > 0
			Layout.fillWidth: true
			RowLayout {
				Layout.fillWidth: true; spacing: tokens.space8
				Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: page.credentialsRejected ? tokens.warning : tokens.accent }
				Text { text: manager.startPageText; textFormat: Text.RichText; color: page.credentialsRejected ? tokens.warning : tokens.accent; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Other choices"); color: tokens.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
			Text { text: qsTr("Google Drive and Dropbox are connected from Neo Cloud & Sync after a local log has been opened. They do not require a Subsurface Cloud account."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			GridLayout {
				Layout.fillWidth: true; columns: page.width >= 600 ? 3 : 1; columnSpacing: tokens.space8; rowSpacing: tokens.space8
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Use local log"); onClicked: page.useLocalLog() }
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Back"); variant: "ghost"; onClicked: page.returnRequested() }
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Forgot password"); onClicked: Qt.openUrlExternally("https://cloud.subsurface-divelog.org/passwordreset") }
			}
		}

		Text { text: qsTr("Existing cloud logs can take several minutes to download after successful sign-in. Neo keeps this service for compatibility with the wider Subsurface ecosystem."); color: tokens.textMuted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
