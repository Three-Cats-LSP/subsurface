// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Modern

ColumnLayout {
	id: panel
	required property var provider
	property string draft: ""
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
		text: qsTr("Use an isolated test profile with automatic sync off and no provider assignments. Create an empty workspace or join one with its reference. Your local dive log stays unchanged. These workspaces do not yet replace existing cloud sync.")
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
		visible: panel.busy; text: qsTr("Verifying workspace…")
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
}
