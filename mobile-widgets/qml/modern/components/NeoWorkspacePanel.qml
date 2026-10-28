// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Modern

ColumnLayout {
	id: panel
	required property var provider
	property string draft: ""
	property var pendingCopy: ({})
	property var pendingAcceptance: ({})
	property var pendingActivation: ({})
	property var pendingRestore: ({})
	signal draftEdited(string value)
	readonly property var report: provider.workspace || ({})
	readonly property bool available: !CloudSync.syncInProgress && !CloudSync.authorizationInProgress && !CloudSync.accountCheckInProgress
	readonly property bool busy: CloudSync.workspaceProviderId === provider.id
	spacing: 10
	Modern.DesignTokens { id: tokens }
	Text {
		text: qsTr("Experimental workspaces")
		color: tokens.textPrimary; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap
	}
	Text {
		text: panel.report.routeActive ? qsTr("This profile is routed to the selected workspace. You can review versions and cloud branches; turn automatic sync off and clear provider assignments before changing them.") : qsTr("Use an isolated test profile with automatic sync off and no provider assignments. Creating or joining a workspace preserves your local log. Review and activate it before using workspace sync.")
		color: tokens.textSecondary; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap
	}
	GridLayout {
		Layout.fillWidth: true; columns: width > 560 ? 3 : 1; rowSpacing: 8; columnSpacing: 8
		NeoButton {
			objectName: "createWorkspace-" + panel.provider.id; text: qsTr("Create empty workspace"); compact: true; Layout.fillWidth: true
			enabled: panel.available && !panel.report.recoveryRequired && !panel.report.setupLimitReached
			onClicked: CloudSync.runWorkspace(panel.provider.id, "create")
		}
		NeoButton {
			objectName: "refreshWorkspaceChoices-" + panel.provider.id; text: qsTr("Refresh choices"); compact: true; Layout.fillWidth: true
			enabled: panel.available; onClicked: CloudSync.runWorkspace(panel.provider.id, "refresh")
		}
		NeoButton {
			objectName: "cancelWorkspace-" + panel.provider.id; text: qsTr("Cancel"); variant: "ghost"; compact: true; Layout.fillWidth: true
			visible: panel.busy; onClicked: CloudSync.cancelWorkspace()
		}
	}
	Text {
		visible: panel.busy; text: qsTr("Working with workspace…")
		color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
	}
	Text {
		visible: !!panel.report.recoveryRequired || !!panel.report.setupLimitReached
		text: panel.report.recoveryRequired ? qsTr("Saved workspace records need recovery. Existing records and your local log were kept.") : qsTr("Too many saved setup attempts. Existing records were kept.")
		color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
	}
	ColumnLayout {
		Layout.fillWidth: true; visible: !!panel.report.selected
		Text {
			objectName: "workspaceStatus-" + panel.provider.id
			text: panel.report.verified ? (panel.report.empty ? qsTr("Selected workspace verified · Empty") : qsTr("Selected workspace verified · %1 versions").arg(panel.report.versions)) : qsTr("A workspace is selected on this device. Verify it before use.")
			color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		NeoButton {
			objectName: "verifyWorkspace-" + panel.provider.id; text: qsTr("Verify selected workspace"); compact: true; Layout.fillWidth: true
			enabled: panel.available; onClicked: CloudSync.runWorkspace(panel.provider.id, "review")
		}
		GridLayout {
			Layout.fillWidth: true; columns: width > 560 ? 2 : 1; rowSpacing: 8; columnSpacing: 8
			NeoButton {
				objectName: "saveWorkspaceVersion-" + panel.provider.id; text: qsTr("Save current log as a version"); compact: true; Layout.fillWidth: true
				enabled: panel.available && !panel.report.uploadLimitReached
				onClicked: CloudSync.runWorkspaceVersion(panel.provider.id, "save")
			}
			NeoButton {
				objectName: "listWorkspaceVersions-" + panel.provider.id; text: qsTr("Review versions"); compact: true; Layout.fillWidth: true
				enabled: panel.available; onClicked: CloudSync.runWorkspaceVersion(panel.provider.id, "list")
			}
		}
		Text {
			objectName: "workspaceSaveStatus-" + panel.provider.id
			visible: !!panel.report.savedVersion || !!panel.report.uploadLimitReached || !!panel.report.uploadRecoveryRequired
			text: panel.report.uploadLimitReached ? qsTr("Saved version attempts have reached their safety limit. All versions and attempts were kept.") :
				panel.report.uploadRecoveryRequired ? qsTr("Some saved version records need recovery. All versions and your local log were kept.") :
				panel.report.savedVersionIsHead ? qsTr("Current log saved and verified. Workspace sync is not yet active.") : qsTr("Saved version verified. A later version is now current; all versions were kept.")
			color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		Text {
			visible: !!panel.report.versionReview && !panel.report.empty && !panel.report.localProtected
			text: qsTr("Save the current local log as a version before restoring an earlier version.")
			color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		Text {
			objectName: "workspaceRestoreStatus-" + panel.provider.id
			visible: !!panel.report.restoredVersion || !!panel.report.restoreRecoveryRequired || !!panel.report.restoreLimitReached
			text: panel.report.restoreRecoveryRequired || panel.report.restoreLimitReached ? qsTr("Some restore records need review or have reached their safety limit. All recovery copies were kept.") :
				panel.report.restoreHeadMatches && panel.report.restoreLocalMatches ? qsTr("Version restored and saved on this device.") : qsTr("The saved restore was verified. Newer local or workspace changes were kept.")
			color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		Repeater {
			model: panel.report.versionRows || []
			delegate: RowLayout {
				required property var modelData
				Layout.fillWidth: true
				Text {
					objectName: "workspaceVersionRow-" + panel.provider.id + "-" + modelData.number
					text: qsTr("Version %1 · %2 bytes%3%4").arg(modelData.number).arg(modelData.bytes).arg(modelData.head ? qsTr(" · Current") : "").arg(modelData.restored ? qsTr(" · Restored copy") : "")
					color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
				}
				NeoButton {
					objectName: "restoreWorkspaceVersion-" + panel.provider.id + "-" + modelData.number; text: qsTr("Restore…"); compact: true
					enabled: panel.available && !!panel.report.versionReview && !!panel.report.localProtected && !panel.report.restoreLimitReached
					onClicked: { panel.pendingRestore = { source: modelData.id, number: modelData.number, review: panel.report.versionReview }; restoreDialog.open() }
				}
			}
		}
		NeoButton {
			objectName: "reviewWorkspaceCopy-" + panel.provider.id; text: qsTr("Review cloud log to copy"); compact: true; Layout.fillWidth: true
			enabled: panel.available && !panel.report.copyRecoveryRequired && !panel.report.copyLimitReached
			onClicked: CloudSync.runWorkspace(panel.provider.id, "inspect-copy")
		}
		Text {
			visible: !!panel.report.copyReview
			text: (panel.report.copyReview || {}).missing
				? qsTr("No existing cloud log was found. Save your local log as a workspace version, then confirm the empty cloud account.")
				: qsTr("Cloud log reviewed · %1 bytes. Keep a verified copy in this workspace while preserving your local log and original cloud files.").arg((panel.report.copyReview || {}).bytes || 0)
			color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		NeoButton {
			objectName: "copyCloudToWorkspace-" + panel.provider.id; text: qsTr("Copy reviewed cloud log…"); compact: true; Layout.fillWidth: true
			visible: !!panel.report.copyReview && !(panel.report.copyReview || {}).missing; enabled: panel.available
			onClicked: { panel.pendingCopy = panel.report.copyReview; copyDialog.open() }
		}
		NeoButton {
			objectName: "acceptWorkspaceLegacy-" + panel.provider.id; text: (panel.report.copyReview || {}).missing ? qsTr("Confirm empty cloud account…") : qsTr("Accept copied cloud branch…"); compact: true; Layout.fillWidth: true
			visible: !!panel.report.copyReview && !!(panel.report.copyReview || {}).acceptancePossible && !(panel.report.copyReview || {}).alreadyAccepted
			enabled: panel.available && !panel.report.acceptanceRecoveryRequired && !panel.report.acceptanceLimitReached
			onClicked: { panel.pendingAcceptance = panel.report.copyReview; acceptanceDialog.open() }
		}
		Text {
			visible: !!panel.report.copyReview && !!(panel.report.copyReview || {}).alreadyAccepted
			text: (panel.report.copyReview || {}).missing ? qsTr("The empty cloud account is already the latest accepted observation.") : qsTr("This copied cloud branch is already the latest accepted branch.")
			color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		NeoButton {
			objectName: "activateWorkspace-" + panel.provider.id; text: qsTr("Activate workspace sync…"); compact: true; Layout.fillWidth: true
			visible: !!panel.report.copyReview && !!(panel.report.copyReview || {}).alreadyAccepted && !panel.report.routeActive
			enabled: panel.available && !panel.report.activationRecoveryRequired && !panel.report.activationLimitReached
			onClicked: { panel.pendingActivation = panel.report.copyReview; activationDialog.open() }
		}
		Text {
			objectName: "workspaceActivationStatus-" + panel.provider.id
			visible: !!panel.report.routeActive || !!panel.report.routeRecoveryRequired || !!panel.report.activationRecoveryRequired || !!panel.report.activationLimitReached
			text: panel.report.routeRecoveryRequired || panel.report.activationRecoveryRequired || panel.report.activationLimitReached ? qsTr("Workspace routing needs review. Existing local and cloud data were kept.") : qsTr("This workspace is selected for sync on this device. Choose this provider as primary to use automatic sync.")
			color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		Text {
			objectName: "workspaceAcceptanceStatus-" + panel.provider.id
			visible: !!panel.report.acceptedLegacy || !!panel.report.acceptanceRecoveryRequired || !!panel.report.acceptanceLimitReached
			text: panel.report.acceptanceRecoveryRequired || panel.report.acceptanceLimitReached ? qsTr("Saved branch acceptance records need review or reached their safety limit. All records were kept.") :
				panel.report.legacyChanged || !panel.report.acceptanceLatest ? qsTr("The accepted branch was recorded, but a newer branch or acceptance appeared. Review it before syncing.") : qsTr("Copied cloud branch accepted. Workspace sync is not yet active.")
			color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		Text {
			objectName: "workspaceCopyStatus-" + panel.provider.id
			visible: !!panel.report.copied || !!panel.report.copyRecoveryRequired || !!panel.report.copyLimitReached
			text: panel.report.copyRecoveryRequired || panel.report.copyLimitReached ? qsTr("Saved copy records need recovery or have reached their safety limit. All attempts and files were kept.") :
				panel.report.legacyChanged ? qsTr("The copied version is protected. The original cloud log changed again; review it as a separate branch.") : qsTr("Cloud log copied and verified. Your local log is unchanged. Workspace sync is not yet active.")
			color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		Text {
			text: qsTr("Copy this reference to join from another device using the same account.")
			color: tokens.textSecondary; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap
		}
		NeoTextArea {
			objectName: "workspaceReference-" + panel.provider.id; Layout.fillWidth: true; Layout.minimumWidth: 0
			text: panel.report.reference || ""; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; textFormat: TextEdit.PlainText
			implicitHeight: Math.max(92, contentHeight + topPadding + bottomPadding)
			color: tokens.textPrimary; font.pixelSize: 11
		}
		NeoButton {
			objectName: "copyWorkspaceReference-" + panel.provider.id; text: qsTr("Copy reference"); compact: true; Layout.fillWidth: true
			onClicked: manager.copyToClipboard(panel.report.reference || "")
		}
	}
	Repeater {
		model: panel.report.known || []
		delegate: RowLayout {
			required property var modelData
			required property int index
			Layout.fillWidth: true
			Text {
				text: qsTr("Saved workspace %1%2").arg(index + 1).arg(modelData.current ? qsTr(" · Selected") : "")
				color: tokens.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap
			}
			NeoButton {
				objectName: "selectSavedWorkspace-" + panel.provider.id + "-" + index; text: qsTr("Select"); compact: true
				enabled: panel.available && !modelData.current; onClicked: CloudSync.runWorkspace(panel.provider.id, "join", modelData.reference)
			}
		}
	}
	Repeater {
		model: panel.report.restores || []
		delegate: ColumnLayout {
			required property var modelData
			required property int index
			Layout.fillWidth: true
			Text {
				text: modelData.uncertain ? qsTr("Restore %1 has no confirmed upload identity. Its recovery files were kept; review versions to start a separate restore.").arg(index + 1) :
					modelData.choiceChanged ? qsTr("Restore %1 belongs to an earlier workspace choice. Its recovery files were kept.").arg(index + 1) : qsTr("Restore %1 has saved recovery information and can be checked again.").arg(index + 1)
				color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
			}
			NeoButton {
				objectName: "resumeWorkspaceRestore-" + panel.provider.id + "-" + index; text: qsTr("Check and resume restore"); compact: true; Layout.fillWidth: true
				enabled: panel.available && modelData.canResume
				onClicked: CloudSync.runWorkspaceVersion(panel.provider.id, "resume-restore", modelData.id)
			}
		}
	}
	Repeater {
		model: panel.report.acceptances || []
		delegate: ColumnLayout {
			required property var modelData
			required property int index
			Layout.fillWidth: true
			Text {
				text: modelData.choiceChanged ? qsTr("Branch acceptance %1 belongs to an earlier workspace choice. Its record was kept.").arg(index + 1) : qsTr("Branch acceptance %1 has saved recovery information and can be checked again.").arg(index + 1)
				color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
			}
			NeoButton {
				objectName: "resumeWorkspaceAcceptance-" + panel.provider.id + "-" + index; text: qsTr("Check and resume acceptance"); compact: true; Layout.fillWidth: true
				enabled: panel.available && modelData.canResume
				onClicked: CloudSync.runWorkspace(panel.provider.id, "resume-accept", modelData.id)
			}
		}
	}
	Repeater {
		model: panel.report.activations || []
		delegate: ColumnLayout {
			required property var modelData
			required property int index
			Layout.fillWidth: true
			Text {
				text: modelData.choiceChanged ? qsTr("Sync setup %1 belongs to an earlier workspace choice. Its recovery records were kept.").arg(index + 1) : qsTr("Sync setup %1 has saved recovery information and can be checked again.").arg(index + 1)
				color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
			}
			NeoButton {
				objectName: "resumeWorkspaceActivation-" + panel.provider.id + "-" + index; text: qsTr("Check and resume sync setup"); compact: true; Layout.fillWidth: true
				enabled: panel.available && modelData.canResume
				onClicked: CloudSync.runWorkspace(panel.provider.id, "resume-activation", modelData.id)
			}
		}
	}
	Repeater {
		model: panel.report.uploads || []
		delegate: ColumnLayout {
			required property var modelData
			required property int index
			Layout.fillWidth: true
			Text {
				text: modelData.uncertain ? qsTr("Version attempt %1 has no confirmed payload identity. Its files are kept. Save the current log as a separate version.").arg(index + 1) : qsTr("Saved version attempt %1 can be checked with the same local contents.").arg(index + 1)
				color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
			}
			NeoButton {
				objectName: "resumeWorkspaceVersion-" + panel.provider.id + "-" + index; text: qsTr("Check and resume version"); compact: true; Layout.fillWidth: true
				enabled: panel.available && modelData.canResume
				onClicked: CloudSync.runWorkspaceVersion(panel.provider.id, "resume-save", modelData.id)
			}
		}
	}
	Repeater {
		model: panel.report.copies || []
		delegate: ColumnLayout {
			required property var modelData
			required property int index
			Layout.fillWidth: true
			Text {
				text: modelData.uncertain ? qsTr("Copy %1 has no confirmed payload identity. Its attempt and files are kept. Review the source to start a separate copy.").arg(index + 1) : qsTr("Saved copy %1 can be checked and resumed. Your local log stays unchanged.").arg(index + 1)
				color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
			}
			NeoButton {
				objectName: "resumeWorkspaceCopy-" + panel.provider.id + "-" + index; text: qsTr("Check and resume copy"); compact: true; Layout.fillWidth: true
				enabled: panel.available && modelData.canResume && !panel.report.copyRecoveryRequired
				onClicked: CloudSync.runWorkspace(panel.provider.id, "resume-copy", modelData.id)
			}
		}
	}
	Repeater {
		model: panel.report.setups || []
		delegate: ColumnLayout {
			required property var modelData
			required property int index
			Layout.fillWidth: true
			Text {
				text: modelData.uncertain ? qsTr("Setup %1 could not be confirmed. Its files are kept; creating a separate workspace uses a new identity.").arg(index + 1) : qsTr("Saved setup %1 can be checked again.").arg(index + 1)
				color: tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.WordWrap
			}
			NeoButton {
				objectName: "resumeWorkspaceSetup-" + panel.provider.id + "-" + index; text: qsTr("Resume setup"); compact: true; Layout.fillWidth: true
				enabled: panel.available && modelData.canResume; onClicked: CloudSync.runWorkspace(panel.provider.id, "resume", modelData.id)
			}
		}
	}
	NeoTextArea {
		id: referenceInput; objectName: "joinWorkspaceReference-" + panel.provider.id
		Layout.fillWidth: true; Layout.minimumWidth: 0; placeholderText: qsTr("Paste a workspace reference")
		text: panel.draft; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; textFormat: TextEdit.PlainText
		color: tokens.textPrimary; font.pixelSize: 12
		onTextChanged: if (text !== panel.draft) panel.draftEdited(text)
	}
	NeoButton {
		objectName: "joinWorkspace-" + panel.provider.id; text: qsTr("Verify and join"); compact: true; Layout.fillWidth: true
		enabled: panel.available && !panel.report.recoveryRequired && referenceInput.text.trim().length > 0 && referenceInput.text.length <= 2048
		onClicked: CloudSync.runWorkspace(panel.provider.id, "join", referenceInput.text.trim())
	}
	Popup {
		id: activationDialog; objectName: "workspaceActivationDialog-" + panel.provider.id
		parent: Overlay.overlay; anchors.centerIn: parent; modal: true; focus: true; padding: 16
		closePolicy: Popup.CloseOnEscape
		width: Math.max(280, Math.min(460, parent ? parent.width - 32 : panel.width))
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Rectangle {
			objectName: "workspaceActivationBody-" + panel.provider.id; color: tokens.surfaceRaised; implicitHeight: activationContent.implicitHeight
			Accessible.role: Accessible.Dialog; Accessible.name: activationTitle.text
			ColumnLayout {
				id: activationContent; width: parent.width; spacing: 12
				Text { id: activationTitle; text: qsTr("Activate this workspace for sync?"); color: tokens.textPrimary; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
				Text { text: qsTr("Neo will verify that the current local log matches the workspace version and that the reviewed cloud branch is still current. Future sync for this provider will use this workspace. Choose it as primary to enable automatic sync."); color: tokens.textSecondary; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
				NeoButton { objectName: "cancelWorkspaceActivation-" + panel.provider.id; text: qsTr("Cancel"); variant: "ghost"; Layout.fillWidth: true; onClicked: activationDialog.close() }
				NeoButton {
					objectName: "confirmWorkspaceActivation-" + panel.provider.id; text: qsTr("Activate workspace"); Layout.fillWidth: true; enabled: panel.available
					onClicked: { var review = panel.pendingActivation.id || ""; activationDialog.close(); CloudSync.runWorkspace(panel.provider.id, "activate", review) }
				}
			}
		}
	}
	Popup {
		id: acceptanceDialog; objectName: "workspaceAcceptanceDialog-" + panel.provider.id
		parent: Overlay.overlay; anchors.centerIn: parent; modal: true; focus: true; padding: 16
		closePolicy: Popup.CloseOnEscape
		width: Math.max(280, Math.min(460, parent ? parent.width - 32 : panel.width))
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Rectangle {
			objectName: "workspaceAcceptanceBody-" + panel.provider.id; color: tokens.surfaceRaised; implicitHeight: acceptanceContent.implicitHeight
			Accessible.role: Accessible.Dialog; Accessible.name: acceptanceTitle.text
			ColumnLayout {
				id: acceptanceContent; width: parent.width; spacing: 12
				Text { id: acceptanceTitle; text: panel.pendingAcceptance.missing ? qsTr("Confirm empty cloud account?") : qsTr("Accept this cloud branch?"); color: tokens.textPrimary; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
				Text { text: panel.pendingAcceptance.missing ? qsTr("Neo will verify that no existing cloud log is present and record this observation. Your local log stays unchanged; workspace sync is not started. If a cloud log appears later, review it before syncing.") : qsTr("Neo will verify the original cloud log and its protected workspace copy, then record this branch as accepted. Your local log stays unchanged; workspace sync is not started."); color: tokens.textSecondary; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
				NeoButton { objectName: "cancelWorkspaceAcceptance-" + panel.provider.id; text: qsTr("Cancel"); variant: "ghost"; Layout.fillWidth: true; onClicked: acceptanceDialog.close() }
				NeoButton {
					objectName: "confirmWorkspaceAcceptance-" + panel.provider.id; text: panel.pendingAcceptance.missing ? qsTr("Confirm empty account") : qsTr("Accept branch"); Layout.fillWidth: true; enabled: panel.available
					onClicked: { var review = panel.pendingAcceptance.id || ""; var action = panel.pendingAcceptance.missing ? "accept-missing" : "accept-copy"; acceptanceDialog.close(); CloudSync.runWorkspace(panel.provider.id, action, review) }
				}
			}
		}
	}
	Popup {
		id: restoreDialog; objectName: "workspaceRestoreDialog-" + panel.provider.id
		parent: Overlay.overlay; anchors.centerIn: parent; modal: true; focus: true; padding: 16
		closePolicy: Popup.CloseOnEscape
		width: Math.max(280, Math.min(460, parent ? parent.width - 32 : panel.width))
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Rectangle {
			objectName: "workspaceRestoreBody-" + panel.provider.id; color: tokens.surfaceRaised; implicitHeight: restoreContent.implicitHeight
			Accessible.role: Accessible.Dialog; Accessible.name: restoreTitle.text
			ColumnLayout {
				id: restoreContent; width: parent.width; spacing: 12
				Text { id: restoreTitle; text: qsTr("Restore version %1?").arg(panel.pendingRestore.number || 0); color: tokens.textPrimary; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
				Text { text: qsTr("The current log is kept as a workspace version. The selected version will be saved as a new version and replace the local log on this device."); color: tokens.textSecondary; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
				NeoButton { objectName: "cancelWorkspaceRestore-" + panel.provider.id; text: qsTr("Cancel"); variant: "ghost"; Layout.fillWidth: true; onClicked: restoreDialog.close() }
				NeoButton {
					objectName: "confirmWorkspaceRestore-" + panel.provider.id; text: qsTr("Restore version"); Layout.fillWidth: true; enabled: panel.available
					onClicked: { var request = JSON.stringify({review: panel.pendingRestore.review || "", source: panel.pendingRestore.source || ""}); restoreDialog.close(); CloudSync.runWorkspaceVersion(panel.provider.id, "restore", request) }
				}
			}
		}
	}
	Popup {
		id: copyDialog; objectName: "workspaceCopyDialog-" + panel.provider.id
		parent: Overlay.overlay; anchors.centerIn: parent; modal: true; focus: true; padding: 16
		closePolicy: Popup.CloseOnEscape
		width: Math.max(280, Math.min(460, parent ? parent.width - 32 : panel.width))
		background: Rectangle { color: tokens.surfaceRaised; radius: tokens.radius16; border.color: tokens.border }
		contentItem: Rectangle {
			objectName: "workspaceCopyBody-" + panel.provider.id
			color: tokens.surfaceRaised; implicitHeight: copyContent.implicitHeight
			Accessible.role: Accessible.Dialog; Accessible.name: copyTitle.text
			ColumnLayout {
				id: copyContent; width: parent.width; spacing: 12
				Text {
					id: copyTitle; text: qsTr("Copy cloud log into this workspace?")
					color: tokens.textPrimary; font.pixelSize: 18; font.bold: true; textFormat: Text.PlainText
					Layout.fillWidth: true; wrapMode: Text.WordWrap
				}
				Text {
					text: qsTr("Keep a verified copy of the reviewed cloud log (%1 bytes) in this workspace. Your local dive log and original cloud files are kept. This does not activate workspace synchronization.").arg(panel.pendingCopy.bytes || 0)
					color: tokens.textSecondary; textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true
				}
				NeoButton { objectName: "cancelWorkspaceCopy-" + panel.provider.id; text: qsTr("Cancel"); variant: "ghost"; Layout.fillWidth: true; onClicked: copyDialog.close() }
				NeoButton {
					objectName: "confirmWorkspaceCopy-" + panel.provider.id; text: qsTr("Copy cloud log"); Layout.fillWidth: true; enabled: panel.available
					onClicked: { var id = panel.pendingCopy.id || ""; copyDialog.close(); CloudSync.runWorkspace(panel.provider.id, "copy", id) }
				}
			}
		}
	}
}
