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

	property bool wideLayout: width >= 760
	property string editingSite: ""
	property string siteFilter: ""
	signal openMap(string siteName)
	signal openDive(int diveId)

	Modern.DesignTokens { id: tokens }

	function gpsSiteCount() {
		var count = 0
		for (var i = 0; i < manager.locationList.length; ++i) {
			var summary = manager.siteSummary(manager.locationList[i])
			if (summary.gps && summary.gps.length > 0)
				++count
		}
		return count
	}

	function loggedDiveCount() {
		var count = 0
		for (var i = 0; i < manager.locationList.length; ++i)
			count += manager.siteSummary(manager.locationList[i]).diveCount || 0
		return count
	}

	function filteredSiteCount() {
		var count = 0
		for (var i = 0; i < manager.locationList.length; ++i) {
			if (page.siteFilter.length === 0 || manager.locationList[i].toLowerCase().indexOf(page.siteFilter) >= 0)
				++count
		}
		return count
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16

		RowLayout {
			Layout.fillWidth: true
			spacing: tokens.space12
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 2
				Text {
					text: qsTr("Dive sites")
					color: tokens.textPrimary
					font.pixelSize: page.wideLayout ? 30 : 25
					font.weight: Font.DemiBold
				}
				Text {
					Layout.fillWidth: true
					text: qsTr("Places recorded in your canonical dive log")
					color: tokens.textSecondary
					font.pixelSize: 13
					elide: Text.ElideRight
				}
			}
			Button { text: qsTr("Open map"); onClicked: page.openMap("") }
		}

		GridLayout {
			Layout.fillWidth: true
			columns: 3
			columnSpacing: page.wideLayout ? tokens.space16 : tokens.space8
			Components.MetricCard {
				label: qsTr("Sites")
				value: String(manager.locationList.length)
				iconName: "site"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				label: qsTr("Logged dives")
				value: String(page.loggedDiveCount())
				iconName: "tank"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				label: qsTr("Mapped")
				value: String(page.gpsSiteCount())
				iconName: "site"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
		}

		Components.ModernCard {
			Layout.fillWidth: true
			contentPadding: tokens.space12
			RowLayout {
				Layout.fillWidth: true
				spacing: tokens.space8
				Components.NeoDiveIcon {
					name: "search"
					iconColor: tokens.textMuted
					Layout.preferredWidth: 22
					Layout.preferredHeight: 22
				}
				TextField {
					Layout.fillWidth: true
					placeholderText: qsTr("Search dive sites")
					color: tokens.textPrimary
					onTextEdited: page.siteFilter = text.trim().toLowerCase()
					background: Rectangle { color: "transparent" }
				}
				Text {
					visible: page.wideLayout
					text: qsTr("%1 shown").arg(page.filteredSiteCount())
					color: tokens.textMuted
					font.pixelSize: 12
				}
			}
		}

		Text {
			visible: manager.locationList.length === 0
			Layout.fillWidth: true
			text: qsTr("No dive sites yet. Imported and manually added dives will appear here when they have a location.")
			color: tokens.textSecondary
			wrapMode: Text.WordWrap
		}

		Text {
			visible: manager.locationList.length > 0 && page.filteredSiteCount() === 0
			Layout.fillWidth: true
			text: qsTr("No sites match this search.")
			color: tokens.textSecondary
			horizontalAlignment: Text.AlignHCenter
		}

		GridLayout {
			Layout.fillWidth: true
			columns: page.wideLayout ? 2 : 1
			columnSpacing: tokens.space12
			rowSpacing: tokens.space12

			Repeater {
				model: manager.locationList

				delegate: Components.ModernCard {
					id: siteCard
					required property string modelData
					property var summary: manager.siteSummary(modelData)
					property bool relatedDivesVisible: false
					visible: page.siteFilter.length === 0 || modelData.toLowerCase().indexOf(page.siteFilter) >= 0
					Layout.fillWidth: true
					Layout.alignment: Qt.AlignTop
					contentPadding: tokens.space12

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space8
						Rectangle {
							Layout.preferredWidth: 40
							Layout.preferredHeight: 40
							color: tokens.surfaceRaised
							radius: 20
							Components.NeoDiveIcon { anchors.centerIn: parent; width: 23; height: 23; name: "site"; iconColor: tokens.accent }
						}
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2
							Text {
								Layout.fillWidth: true
								text: siteCard.modelData
								color: tokens.textPrimary
								font.pixelSize: 17
								font.weight: Font.DemiBold
								elide: Text.ElideRight
							}
							Text {
								text: qsTr("%1 logged dives").arg(siteCard.summary.diveCount || 0)
								color: tokens.accent
								font.pixelSize: 11
							}
						}
						Text {
							text: siteCard.summary.gps && siteCard.summary.gps.length > 0 ? qsTr("Mapped") : qsTr("No GPS")
							color: siteCard.summary.gps && siteCard.summary.gps.length > 0 ? tokens.success : tokens.textMuted
							font.pixelSize: 11
						}
					}

					Text {
						visible: siteCard.summary.gps && siteCard.summary.gps.length > 0
						text: siteCard.summary.gps || ""
						color: tokens.textSecondary
						font.pixelSize: 12
						wrapMode: Text.WordWrap
						Layout.fillWidth: true
					}

					Text {
						visible: siteCard.summary.description && siteCard.summary.description.length > 0
						text: siteCard.summary.description || ""
						color: tokens.textSecondary
						font.pixelSize: 13
						wrapMode: Text.WordWrap
						maximumLineCount: siteCard.relatedDivesVisible ? 8 : 3
						elide: Text.ElideRight
						Layout.fillWidth: true
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space8
						Button { text: qsTr("Map"); onClicked: page.openMap(siteCard.modelData) }
						Button {
							visible: siteCard.summary.diveCount > 0
							text: siteCard.relatedDivesVisible ? qsTr("Hide dives") : qsTr("View dives")
							onClicked: siteCard.relatedDivesVisible = !siteCard.relatedDivesVisible
						}
						Item { Layout.fillWidth: true }
						Button {
							text: qsTr("Edit")
							flat: true
							onClicked: {
								page.editingSite = siteCard.modelData
								siteDescription.text = siteCard.summary.description || ""
								siteNotes.text = siteCard.summary.notes || ""
								siteGps.text = siteCard.summary.gps || ""
								siteEditor.open()
							}
						}
					}

					Repeater {
						model: siteCard.relatedDivesVisible ? manager.siteDives(siteCard.modelData) : []
						delegate: Rectangle {
							required property var modelData
							Layout.fillWidth: true
							implicitHeight: diveRow.implicitHeight + tokens.space16
							color: tokens.background
							radius: tokens.radiusSmall
							RowLayout {
								id: diveRow
								anchors.fill: parent
								anchors.margins: tokens.space8
								Text { text: modelData.number > 0 ? "#" + modelData.number : qsTr("Dive"); color: tokens.accent; font.weight: Font.DemiBold }
								Text { text: modelData.date; color: tokens.textSecondary; font.pixelSize: 11; Layout.fillWidth: true }
								Text { text: modelData.depth; color: tokens.textPrimary; font.pixelSize: 11 }
								Text { visible: page.wideLayout; text: modelData.duration; color: tokens.textSecondary; font.pixelSize: 11 }
							}
							TapHandler { onTapped: page.openDive(modelData.id) }
						}
					}
				}
			}
		}
	}

	Dialog {
		id: siteEditor
		parent: Overlay.overlay
		modal: true
		anchors.centerIn: parent
		width: Math.min(page.width - tokens.space32, 560)
		title: qsTr("Edit %1").arg(page.editingSite)
		standardButtons: Dialog.NoButton
		contentItem: ColumnLayout {
			width: Math.min(500, siteEditor.width - tokens.space32)
			spacing: tokens.space12
			Text { text: qsTr("GPS coordinates"); color: tokens.textMuted; font.pixelSize: 11 }
			TextField { id: siteGps; Layout.fillWidth: true; color: tokens.textPrimary; placeholderText: qsTr("Latitude, longitude") }
			Text { text: qsTr("Description"); color: tokens.textMuted; font.pixelSize: 11 }
			TextArea { id: siteDescription; Layout.fillWidth: true; Layout.preferredHeight: 100; color: tokens.textPrimary; wrapMode: TextEdit.Wrap }
			Text { text: qsTr("Notes"); color: tokens.textMuted; font.pixelSize: 11 }
			TextArea { id: siteNotes; Layout.fillWidth: true; Layout.preferredHeight: 100; color: tokens.textPrimary; wrapMode: TextEdit.Wrap }
			RowLayout {
				Layout.fillWidth: true
				Item { Layout.fillWidth: true }
				Button { text: qsTr("Cancel"); onClicked: siteEditor.close() }
				Button {
					text: qsTr("Save site")
					onClicked: {
						if (manager.updateSite(page.editingSite, siteDescription.text, siteNotes.text, siteGps.text))
							siteEditor.close()
					}
				}
			}
		}
	}
}
