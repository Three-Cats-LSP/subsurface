// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Dive sites")
	background: Rectangle { color: tokens.background }
	signal openMap(string siteName)
	signal openDive(int diveId)
	property string editingSite: ""
	property string siteFilter: ""
	Modern.DesignTokens { id: tokens }
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Sites, locations, and maps"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Your log currently contains %1 saved locations.").arg(manager.locationList.length); color: tokens.textSecondary; Layout.fillWidth: true }
		Components.ModernCard { Layout.fillWidth: true; Text { text: qsTr("Explore dive sites"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }; TextField { Layout.fillWidth: true; placeholderText: qsTr("Search sites"); onTextEdited: page.siteFilter = text.toLowerCase() }; Button { Layout.fillWidth: true; text: qsTr("Open map"); onClicked: page.openMap("") } }
		Repeater {
			model: manager.locationList
			delegate: Components.ModernCard {
				required property string modelData
				visible: page.siteFilter.length === 0 || modelData.toLowerCase().indexOf(page.siteFilter) >= 0
				Layout.fillWidth: true
				Text { text: modelData; color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.Medium; Layout.fillWidth: true }
				property var summary: manager.siteSummary(modelData)
				property bool relatedDivesVisible: false
				Text { visible: summary.diveCount > 0; text: qsTr("%1 logged dives").arg(summary.diveCount); color: tokens.textSecondary; font.pixelSize: 13 }
				Text { visible: summary.gps && summary.gps.length > 0; text: summary.gps; color: tokens.textMuted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Text { visible: summary.description && summary.description.length > 0; text: summary.description; color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				RowLayout { Layout.fillWidth: true; Button { text: qsTr("Show on map"); onClicked: page.openMap(modelData) }; Button { visible: summary.diveCount > 0; text: relatedDivesVisible ? qsTr("Hide dives") : qsTr("Show dives"); onClicked: relatedDivesVisible = !relatedDivesVisible }; Button { text: qsTr("Edit"); onClicked: { page.editingSite = modelData; siteDescription.text = summary.description || ""; siteNotes.text = summary.notes || ""; siteGps.text = summary.gps || ""; siteEditor.open() } }; Item { Layout.fillWidth: true }; Label { visible: !summary.gps || summary.gps.length === 0; text: qsTr("No GPS"); color: tokens.textMuted; font.pixelSize: 12 } }
				Repeater { model: relatedDivesVisible ? manager.siteDives(modelData) : []; delegate: Button { required property var modelData; Layout.fillWidth: true; text: qsTr("Dive #%1 · %2 · %3 · %4").arg(modelData.number).arg(modelData.date).arg(modelData.depth).arg(modelData.duration); onClicked: page.openDive(modelData.id) } }
			}
		}
		Text { text: qsTr("Site edits remain stored in canonical Subsurface site data so they appear consistently in every compatible client."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
	Dialog {
		id: siteEditor
		parent: page
		modal: true
		width: Math.min(page.width - tokens.space32, 560)
		x: (page.width - width) / 2
		y: tokens.space24
		title: qsTr("Edit %1").arg(page.editingSite)
		contentItem: ColumnLayout {
			width: 500
			spacing: tokens.space12
			TextField { id: siteGps; Layout.fillWidth: true; placeholderText: qsTr("GPS coordinates") }
			TextArea { id: siteDescription; Layout.fillWidth: true; placeholderText: qsTr("Description") }
			TextArea { id: siteNotes; Layout.fillWidth: true; placeholderText: qsTr("Notes") }
		}
		footer: DialogButtonBox { Button { text: qsTr("Save site"); onClicked: { if (manager.updateSite(page.editingSite, siteDescription.text, siteNotes.text, siteGps.text)) siteEditor.close() } }; Button { text: qsTr("Cancel"); onClicked: siteEditor.close() } }
	}
}
