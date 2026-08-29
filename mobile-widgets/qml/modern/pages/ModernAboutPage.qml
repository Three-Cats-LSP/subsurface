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
	title: qsTr("About Subsurface Neo")
	background: Rectangle { color: tokens.background }
	property bool wideLayout: width >= 760
	property bool manualUpdateCheck: false
	property bool keyboardEntryPending: true
	Modern.DesignTokens { id: tokens }

	function focusPrimaryAction() {
		keyboardEntryPending = false
		Qt.callLater(function() { updateButton.forceActiveFocus(Qt.TabFocusReason) })
	}

	function prepareKeyboardEntry() {
		keyboardEntryPending = true
	}

	onVisibleChanged: {
		if (visible)
			prepareKeyboardEntry()
	}
	StackView.onActivated: prepareKeyboardEntry()
	Component.onCompleted: if (visible) prepareKeyboardEntry()

	function openLink(url) { Qt.openUrlExternally(url) }

	Connections {
		target: NeoUpdate
		function onStateChanged() {
			if (!page.manualUpdateCheck || NeoUpdate.checking)
				return
			page.manualUpdateCheck = false
			if (NeoUpdate.lastError !== "")
				showPassiveNotification(qsTr("Unable to check for updates: %1").arg(NeoUpdate.lastError), 6000)
			else if (NeoUpdate.updateAvailable)
				showPassiveNotification(qsTr("Subsurface Neo %1 is available.").arg(NeoUpdate.latestVersion), 6000)
			else
				showPassiveNotification(qsTr("Subsurface Neo is up to date."), 4000)
		}
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16

		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout {
				Layout.fillWidth: true
				spacing: tokens.space16
				Image {
					source: "qrc:/qml/subsurface-neo-icon.svg"
					Layout.preferredWidth: page.wideLayout ? 96 : 72
					Layout.preferredHeight: page.wideLayout ? 96 : 72
					fillMode: Image.PreserveAspectFit
				}
				ColumnLayout {
					Layout.fillWidth: true; spacing: 3
					Text { text: qsTr("SUBSURFACE NEO"); color: tokens.accent; font.pixelSize: 12; font.letterSpacing: 2; font.weight: Font.DemiBold }
					Text { text: manager.getVersion(); color: tokens.textPrimary; font.pixelSize: page.wideLayout ? 25 : 20; font.weight: Font.DemiBold; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
					Text { text: qsTr("Modern interface. Mature Subsurface engine."); color: tokens.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
			}
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.wideLayout ? 2 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Components.ModernCard {
				Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Updates"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text {
					text: NeoUpdate.updateAvailable ? qsTr("Version %1 is ready to download.").arg(NeoUpdate.latestVersion) : qsTr("Check the signed Neo update manifest for a newer development or release build.")
					color: NeoUpdate.updateAvailable ? tokens.success : tokens.textSecondary
					font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
				}
				Button {
					id: updateButton
					Layout.fillWidth: true
					activeFocusOnTab: true
					enabled: !NeoUpdate.checking
					text: NeoUpdate.checking ? qsTr("Checking…") : (NeoUpdate.updateAvailable ? qsTr("Download %1").arg(NeoUpdate.latestVersion) : qsTr("Check for updates"))
					onClicked: {
						if (NeoUpdate.updateAvailable && NeoUpdate.downloadUrl !== "")
							page.openLink(NeoUpdate.downloadUrl)
						else {
							page.manualUpdateCheck = true
							NeoUpdate.checkForUpdates(true)
						}
					}
				}
			}

			Components.ModernCard {
				Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
				Text { text: qsTr("Open source"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
				Text { text: qsTr("Subsurface Neo is based on Subsurface and distributed under GPL-2.0. The source, history, and build workflows are public."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				RowLayout {
					Layout.fillWidth: true
					Button { text: qsTr("Neo source"); onClicked: page.openLink("https://github.com/Three-Cats-LSP/subsurface") }
					Button { text: qsTr("GPL-2.0"); onClicked: page.openLink("https://www.gnu.org/licenses/old-licenses/gpl-2.0.html") }
				}
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Help & diagnostics"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout {
				Layout.fillWidth: true; columns: page.wideLayout ? 3 : 1; columnSpacing: tokens.space8; rowSpacing: tokens.space8
				Button { Layout.fillWidth: true; text: qsTr("User manual"); onClicked: page.openLink("https://www.subsurface-divelog.org/subsurface-mobile-user-manual/") }
				Button { Layout.fillWidth: true; text: qsTr("Project website"); onClicked: page.openLink("https://threecats-lsp.com/subsurface-neo/") }
				Button { Layout.fillWidth: true; text: qsTr("Copy diagnostic log"); onClicked: { manager.copyAppLogToClipboard(); showPassiveNotification(qsTr("Diagnostic log copied"), 3000) } }
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Interface artwork"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Regulator and oxygen tank icons by Magnific, container icon by Three musketeers, sea icon by Anditii Creative, marine icon by IconBaandar, and sports and scuba-diving artwork provided through Flaticon."); color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Flow {
				Layout.fillWidth: true; spacing: tokens.space8
				Button { text: qsTr("Regulator credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/regulator_4864503") }
				Button { text: qsTr("Gas icon credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/sea_14546011") }
				Button { text: qsTr("Gear icon credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/marine_14836868") }
				Button { text: qsTr("Dive tank credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/oxygen-tank_5232839") }
				Button { text: qsTr("Gas container credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/container_16494765") }
				Button { text: qsTr("Mode icon credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/regulator_5158240") }
				Button { text: qsTr("Gear icon credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/sports_15710848") }
				Button { text: qsTr("Type icon credit"); onClicked: page.openLink("https://www.flaticon.com/free-icon/scuba-diving_18643383") }
			}
		}

		Text {
			text: qsTr("Subsurface is a community-developed dive log. Neo changes the interface and product workflow while preserving the established data and calculation foundations.")
			color: tokens.textMuted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true
		}
	}
}
