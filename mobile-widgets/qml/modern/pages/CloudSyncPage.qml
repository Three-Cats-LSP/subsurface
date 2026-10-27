// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Cloud & Sync")
	background: Rectangle { color: tokens.background }
	property string lastBackupProvider: ""
	property string lastSyncProvider: ""
	property string lastSyncResult: ""
	property string conflictProvider: ""
	property string initialChoiceProvider: ""
	property string pendingDisconnectProvider: ""
	property string pendingDisconnectName: ""
	property string historyProvider: ""
	property string historyRevision: ""

	Modern.DesignTokens { id: tokens }

	function storageSize(bytes) {
		if (bytes >= 1048576) return qsTr("%1 MiB").arg((bytes / 1048576).toFixed(1))
		if (bytes >= 1024) return qsTr("%1 KiB").arg((bytes / 1024).toFixed(1))
		return qsTr("%1 bytes").arg(bytes)
	}

	function storageCategory(id) {
		if (id === "current") return qsTr("Current log files")
		if (id === "recovery") return qsTr("Recovery files")
		if (id === "legacy") return qsTr("Legacy log")
		if (id === "unknown") return qsTr("Other snapshots")
		return qsTr("Other items")
	}

	function resultText(result) {
		if (result === "up-to-date") return qsTr("Up to date")
		if (result === "uploaded") return qsTr("Local changes uploaded")
		if (result === "downloaded") return qsTr("Cloud changes downloaded")
		return result
	}

	function lastSyncText(value) {
		if (!value)
			return ""
		var when = new Date(value)
		return isNaN(when.getTime()) ? "" : qsTr("Last synced %1").arg(Qt.locale().toLocaleString(when, Locale.ShortFormat))
	}

	Connections {
		target: CloudSync
		function onProviderAccountChanged(providerId) { onProviderDisconnected(providerId) }
		function onProviderDisconnected(providerId) {
			if (page.lastSyncProvider === providerId) page.lastSyncProvider = ""
			if (page.lastBackupProvider === providerId) page.lastBackupProvider = ""
			if (page.conflictProvider === providerId) page.conflictProvider = ""
			if (page.initialChoiceProvider === providerId) page.initialChoiceProvider = ""
		}
		function onDiveLogBackupFinished(providerId) {
			page.lastBackupProvider = providerId
			page.conflictProvider = ""
			page.initialChoiceProvider = ""
		}
		function onDiveLogSyncFinished(providerId, result) {
			page.lastSyncProvider = providerId
			page.lastSyncResult = result
			page.conflictProvider = ""
			page.initialChoiceProvider = ""
		}
		function onDiveLogSyncConflict(providerId) {
			page.conflictProvider = providerId
			page.lastSyncProvider = ""
		}
		function onDiveLogInitialChoiceRequired(providerId) {
			page.initialChoiceProvider = providerId
			page.lastSyncProvider = ""
		}
		function onHistoryFinished(providerId) {
			page.lastSyncProvider = ""
			page.lastBackupProvider = ""
		}
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16

		ColumnLayout {
			Layout.fillWidth: true
			spacing: tokens.space4
			Text {
				text: qsTr("Cloud & Sync")
				color: tokens.textPrimary
				font.pixelSize: 28
				font.weight: Font.DemiBold
			}
			Text {
				text: qsTr("Connect Google Drive or Dropbox to keep your Subsurface Neo data available across devices. Sync uses revision manifests and SHA-256 checksums so divergent logs are never silently overwritten.")
				color: tokens.textSecondary
				font.pixelSize: 14
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout {
				Layout.fillWidth: true
				ColumnLayout {
					Layout.fillWidth: true
					Text { text: qsTr("Automatic primary sync"); color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
					Text {
						text: qsTr("Sync an established primary provider after startup and when Neo returns to the foreground.")
						color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
					}
				}
				Components.NeoSwitch {
					checked: CloudSync.autoSyncEnabled
					enabled: CloudSync.primaryProviderId.length > 0
					onToggled: CloudSync.autoSyncEnabled = checked
				}
			}
		}

		Repeater {
			model: CloudSync.providers
			delegate: Components.ModernCard {
				id: providerCard
				required property var modelData
				Layout.fillWidth: true

				ColumnLayout {
					Layout.fillWidth: true
					spacing: tokens.space12

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space12
						Rectangle {
							Layout.preferredWidth: 42; Layout.preferredHeight: 42; radius: 21
							color: modelData.connected ? "#083449" : tokens.surfaceRaised
							Components.NeoDiveIcon { anchors.centerIn: parent; width: 24; height: 24; name: "cloud"; iconColor: modelData.connected ? tokens.accent : tokens.textMuted }
						}

						ColumnLayout {
							Layout.fillWidth: true
							spacing: tokens.space4
							Text {
								text: modelData.name
								color: tokens.textPrimary
								font.pixelSize: 18
								font.weight: Font.DemiBold
							}
							Text {
								text: modelData.connected ? (CloudSync.storageReviewInProgress || CloudSync.historyInProgress ? qsTr("Connected") : CloudSync.syncInProgress ? qsTr("Syncing…") : qsTr("Connected")) :
									  modelData.configured ? qsTr("Ready to connect") : qsTr("Setup required")
								color: modelData.connected ? tokens.accent : tokens.textSecondary
								font.pixelSize: 14
							}
							Text {
								visible: modelData.connected
								text: modelData.checkingAccount ? qsTr("Checking account…") :
									modelData.accountEmail || modelData.accountName || (modelData.accountVerified ? qsTr("Account verified") : qsTr("Account not checked yet"))
								textFormat: Text.PlainText
								color: tokens.textSecondary
								font.pixelSize: 14
								wrapMode: Text.WrapAnywhere
								Layout.fillWidth: true
							}
							Text {
								visible: modelData.connected && !modelData.accountVerified && !modelData.checkingAccount && (modelData.accountEmail.length > 0 || modelData.accountName.length > 0)
								text: qsTr("Last connected account. Checked again before syncing.")
								color: tokens.textMuted
								font.pixelSize: 12
								wrapMode: Text.WordWrap
								Layout.fillWidth: true
							}
							Text {
								visible: modelData.primary || modelData.backup
								text: modelData.primary ? qsTr("Primary sync provider") : qsTr("Backup provider")
								color: tokens.accent
								font.pixelSize: 12
							}
							Text {
								visible: modelData.connected && modelData.lastSyncAt.length > 0
								text: page.lastSyncText(modelData.lastSyncAt)
								color: tokens.textSecondary
								font.pixelSize: 12
							}
							Text {
								visible: page.lastSyncProvider === modelData.id
								text: page.resultText(page.lastSyncResult)
								color: tokens.success
								font.pixelSize: 12
							}
							Text {
								visible: page.lastBackupProvider === modelData.id
								text: qsTr("Backup uploaded successfully")
								color: tokens.success
								font.pixelSize: 12
							}
							Text {
								visible: page.conflictProvider === modelData.id
								text: qsTr("Sync conflict: both this device and the cloud changed since the last sync. Nothing was overwritten.")
								color: "#FFB84D"
								font.pixelSize: 12
								wrapMode: Text.WordWrap
								Layout.fillWidth: true
							}
							Text {
								visible: page.initialChoiceProvider === modelData.id
								text: qsTr("This is the first sync and different data already exists in the cloud. Choose which copy should become the baseline.")
								color: "#FFB84D"
								font.pixelSize: 12
								wrapMode: Text.WordWrap
								Layout.fillWidth: true
							}
							Text {
								visible: !modelData.configured
								text: modelData.configurationError || qsTr("This provider needs OAuth configuration for this platform before it can connect.")
								color: tokens.textSecondary
								font.pixelSize: 12
								wrapMode: Text.WordWrap
								Layout.fillWidth: true
							}
						}
					}

					RowLayout {
						visible: (page.conflictProvider === modelData.id || page.initialChoiceProvider === modelData.id) && modelData.connected
						Layout.fillWidth: true
						Components.NeoButton {
							Layout.fillWidth: true; Layout.minimumWidth: 0
							text: qsTr("Keep this device")
							variant: "primary"; compact: true
							enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
							onClicked: {
								page.conflictProvider = ""
								page.initialChoiceProvider = ""
								CloudSync.backupDiveLog(modelData.id)
							}
						}
						Components.NeoButton {
							Layout.fillWidth: true; Layout.minimumWidth: 0
							text: qsTr("Use cloud copy")
							compact: true
							enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
							onClicked: {
								page.conflictProvider = ""
								page.initialChoiceProvider = ""
								CloudSync.useCloudDiveLog(modelData.id)
							}
						}
					}

					RowLayout {
						visible: modelData.connected
						Layout.fillWidth: true
						Components.NeoButton {
							text: modelData.primary ? qsTr("Primary") : qsTr("Set primary")
							compact: true; variant: modelData.primary ? "primary" : "secondary"
							enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
							onClicked: CloudSync.setPrimaryProvider(modelData.primary ? "" : modelData.id)
						}
						Components.NeoButton {
							text: modelData.backup ? qsTr("Backup") : qsTr("Set backup")
							compact: true; variant: modelData.backup ? "primary" : "secondary"
							enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress && !modelData.primary
							onClicked: CloudSync.setBackupProvider(modelData.backup ? "" : modelData.id)
						}
						Item { Layout.fillWidth: true }
					}

					GridLayout {
						visible: modelData.connected
						Layout.fillWidth: true
						columns: page.width >= 600 ? 3 : 2
						columnSpacing: tokens.space8
						rowSpacing: tokens.space8
						Components.NeoButton {
							Layout.fillWidth: true; Layout.minimumWidth: 0
							text: qsTr("Sync now")
							variant: "primary"; compact: true
							enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
							onClicked: {
								page.lastSyncProvider = ""
								page.conflictProvider = ""
								page.initialChoiceProvider = ""
								CloudSync.syncDiveLog(modelData.id)
							}
						}
						Components.NeoButton {
							Layout.fillWidth: true; Layout.minimumWidth: 0
							text: qsTr("Backup now")
							compact: true
							enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
							onClicked: {
								page.lastBackupProvider = ""
								CloudSync.backupDiveLog(modelData.id)
							}
						}
						Components.NeoButton {
							Layout.fillWidth: true
							Layout.minimumWidth: 0
							Layout.columnSpan: page.width >= 600 ? 1 : 2
							text: qsTr("Disconnect")
							variant: "danger"; compact: true
							enabled: !CloudSync.syncInProgress
							onClicked: {
								page.pendingDisconnectProvider = modelData.id
								page.pendingDisconnectName = modelData.name + (modelData.accountEmail ? " (" + modelData.accountEmail + ")" : "")
								disconnectDialog.open()
							}
						}
					}

					Components.NeoButton {
						visible: modelData.connected
						text: qsTr("Check account")
						compact: true
						enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
						onClicked: CloudSync.checkAccount(modelData.id)
					}

					ColumnLayout {
						visible: modelData.connected
						Layout.fillWidth: true
						spacing: tokens.space8
						Text {
							text: qsTr("Cloud storage")
							color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold
						}
						Text {
							text: qsTr("Review the space used by your cloud log and recovery files. This keeps every file and does not change your dive log.")
							color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
						}
						RowLayout {
							Layout.fillWidth: true
							Components.NeoButton {
								objectName: "reviewStorage-" + modelData.id
								text: CloudSync.storageReviewProviderId === modelData.id ? qsTr("Reviewing…") : qsTr("Review storage")
								compact: true
								enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
								onClicked: CloudSync.reviewStorage(modelData.id)
							}
							Components.NeoButton {
								objectName: "cancelStorageReview-" + modelData.id
								visible: CloudSync.storageReviewProviderId === modelData.id
								text: qsTr("Cancel"); compact: true; variant: "ghost"
								onClicked: CloudSync.cancelStorageReview()
							}
							Item { Layout.fillWidth: true }
						}
						ColumnLayout {
							objectName: "storageReviewResult-" + modelData.id
							property var report: modelData.storageReview
							visible: !!report.reviewedAt
							Layout.fillWidth: true
							Text {
								text: parent.report.reviewedAt ? qsTr("Reviewed %1 · %2 items · %3")
									.arg(Qt.locale().toLocaleString(new Date(parent.report.reviewedAt), Locale.ShortFormat))
									.arg(parent.report.entryCount).arg(page.storageSize(parent.report.bytes)) : ""
								textFormat: Text.PlainText
								color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
							}
							Repeater {
								model: parent.report.categories || []
								delegate: Text {
									required property var modelData
									text: qsTr("%1: %2 items · %3").arg(page.storageCategory(modelData.id)).arg(modelData.count).arg(page.storageSize(modelData.bytes))
									color: tokens.textPrimary; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true
								}
							}
							Text {
								text: qsTr("All files are kept. Other snapshots may contain older versions or unfinished uploads. Sizes describe stored files; this review does not check the contents of each dive log.")
								color: tokens.textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
							}
							Text {
								visible: modelData.id === "dropbox" && parent.report.folders > 0
								text: qsTr("Folder contents are not included in this review.")
								color: tokens.textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
							}
						}
					}

					ColumnLayout {
						visible: CloudSync.historyEnabled && modelData.connected
						objectName: "historySection-" + modelData.id
						Layout.fillWidth: true
						spacing: tokens.space8
						Text {
							text: qsTr("Experimental history")
							color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold
						}
						Text {
							text: qsTr("Use an isolated test profile with automatic sync off and no provider assignments. Initialization copies the current cloud log into a separate history. Save version adds the local log. Existing sync files stay unchanged; cleanup is disabled.")
							color: tokens.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
						}
						GridLayout {
							columns: page.width >= 600 ? 3 : 1
							Layout.fillWidth: true
							Components.NeoButton {
								objectName: "loadHistory-" + modelData.id
								text: qsTr("Load history"); compact: true; Layout.fillWidth: true
								enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
								onClicked: CloudSync.runHistory(modelData.id, "list")
							}
							Components.NeoButton {
								objectName: "initializeHistory-" + modelData.id
								text: qsTr("Initialize history"); compact: true; Layout.fillWidth: true
								visible: !modelData.history.initialized
								enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
								onClicked: CloudSync.runHistory(modelData.id, "migrate")
							}
							Components.NeoButton {
								objectName: "saveHistory-" + modelData.id
								text: qsTr("Save version"); compact: true; Layout.fillWidth: true
								visible: !!modelData.history.initialized
								enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress && !modelData.history.legacyChanged
								onClicked: CloudSync.runHistory(modelData.id, "save")
							}
							Components.NeoButton {
								text: qsTr("Cancel"); compact: true; variant: "ghost"
								visible: CloudSync.historyInProgress
								onClicked: CloudSync.cancelHistory()
							}
						}
						Text {
							visible: !!modelData.history.legacyChanged
							text: qsTr("The existing sync log has changed. Copy it into history to preserve that branch and resume history writes. Your local log and earlier history versions are kept.")
							color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true
						}
						Components.NeoButton {
							objectName: "importLegacyHistory-" + modelData.id
							text: qsTr("Copy changed cloud log"); compact: true; Layout.fillWidth: true
							visible: !!modelData.history.initialized && !!modelData.history.legacyChanged
							enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
							onClicked: { page.historyProvider = modelData.id; historyLegacyDialog.open() }
						}
						Repeater {
							model: modelData.history.versions || []
							delegate: RowLayout {
								required property var modelData
								Layout.fillWidth: true
								Text {
									text: qsTr("%1 · %2%3").arg(Qt.locale().toLocaleString(new Date(modelData.createdAt), Locale.ShortFormat)).arg(page.storageSize(modelData.bytes)).arg((modelData.current ? qsTr(" · Current") : "") + (modelData.legacyImported ? qsTr(" · Cloud copy") : ""))
									color: tokens.textPrimary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
								}
								Components.NeoButton {
									objectName: "restoreHistory-" + modelData.id
									text: qsTr("Restore"); compact: true
									enabled: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress && !providerCard.modelData.history.legacyChanged
									onClicked: {
										page.historyProvider = providerCard.modelData.id
										page.historyRevision = modelData.id
										historyRestoreDialog.open()
									}
								}
							}
						}
					}

					Components.NeoButton {
						visible: !modelData.connected
						text: qsTr("Connect")
						variant: "primary"
						enabled: modelData.configured && !CloudSync.authorizationInProgress && !CloudSync.syncInProgress && !CloudSync.accountCheckInProgress
						onClicked: CloudSync.beginAuthorization(modelData.id)
					}
				}
			}
		}

		Components.ModernCard {
			visible: CloudSync.lastError.length > 0
			Layout.fillWidth: true
			Text {
				text: CloudSync.lastError
				color: tokens.textPrimary
				font.pixelSize: 13
				wrapMode: Text.WordWrap
				Layout.fillWidth: true
			}
		}

		Text {
			text: qsTr("Choose at most one primary sync provider and one optional backup provider. Sync automatically uploads local-only changes and downloads cloud-only changes. If both sides changed, Neo stops and asks which copy to keep. Every downloaded log is checksum-verified before it replaces the local copy. Backup now is an explicit one-way snapshot operation.")
			color: tokens.textSecondary
			font.pixelSize: 12
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}
		Text {
			text: qsTr("Cloud sync and Backup now transfer the Subsurface XML dive log. Equipment library, manual collections, and planner presets are not included. Use a portable .subsurface-neo backup to preserve those separately.")
			color: tokens.textSecondary
			font.pixelSize: 12
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}
	}

	Dialog {
		id: historyLegacyDialog
		parent: Overlay.overlay
		anchors.centerIn: parent
		modal: true
		width: Math.min(page.width - tokens.space32, 500)
		title: qsTr("Copy the changed cloud log?")
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Text {
			text: qsTr("This adds the changed cloud log as the newest history version. Your local log, earlier versions and existing sync files are kept. To restore the copied version locally afterward, first save your local log as a history version, then choose Restore on the cloud copy.")
			color: tokens.textSecondary; wrapMode: Text.WordWrap
		}
		footer: RowLayout {
			Item { Layout.fillWidth: true }
			Components.NeoButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: historyLegacyDialog.close() }
			Components.NeoButton { objectName: "confirmLegacyHistoryCopy"; text: qsTr("Copy"); onClicked: { CloudSync.runHistory(page.historyProvider, "import-legacy"); historyLegacyDialog.close() } }
		}
	}

	Dialog {
		id: historyRestoreDialog
		parent: Overlay.overlay
		anchors.centerIn: parent
		modal: true
		width: Math.min(page.width - tokens.space32, 500)
		title: qsTr("Restore this history version?")
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Text {
			text: qsTr("This creates a new history version and replaces the local dive log. Save your current local log as a version first. Earlier versions and existing sync files are kept.")
			color: tokens.textSecondary; wrapMode: Text.WordWrap
		}
		footer: RowLayout {
			Item { Layout.fillWidth: true }
			Components.NeoButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: historyRestoreDialog.close() }
			Components.NeoButton { objectName: "confirmHistoryRestore"; text: qsTr("Restore"); onClicked: { CloudSync.runHistory(page.historyProvider, "restore", page.historyRevision); historyRestoreDialog.close() } }
		}
	}

	Dialog {
		id: disconnectDialog
		parent: Overlay.overlay
		anchors.centerIn: parent
		modal: true
		width: Math.min(page.width - tokens.space32, 500)
		title: qsTr("Disconnect %1?").arg(page.pendingDisconnectName)
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Text {
			text: qsTr("OAuth tokens and saved synchronization state for this provider will be removed from this device. Remote files will not be deleted.")
			color: tokens.textSecondary; wrapMode: Text.WordWrap
		}
		footer: RowLayout { spacing: tokens.space8; Item { Layout.fillWidth: true } Components.NeoButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: disconnectDialog.close() } Components.NeoButton { text: qsTr("Disconnect"); variant: "danger"; onClicked: { CloudSync.disconnectProvider(page.pendingDisconnectProvider); disconnectDialog.close() } } }
	}
}
