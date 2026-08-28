// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern
import "../components" as Components

Kirigami.Page {
	id: page
	title: qsTr("Dives")
	background: Rectangle { color: tokens.background }

	property QtObject diveListModel: null
	property bool wideLayout: width >= 760
	property bool filterVisible: false
	property bool advancedFiltersVisible: false
	property string activeCollection: ""
	property var activeCollectionDiveIds: []

	signal openDive(int row)
	signal downloadRequested()
	signal addDiveRequested()

	Modern.DesignTokens { id: tokens }

	function applyFilters() {
		manager.setModernDiveFilter(searchField.text, peopleField.text, tagsField.text, locationField.text,
			suitField.text, computerField.text, depthField.text, durationField.text, yearField.text)
	}

	function clearFilters() {
		searchField.clear()
		peopleField.clear()
		tagsField.clear()
		locationField.clear()
		suitField.clear()
		computerField.clear()
		depthField.clear()
		durationField.clear()
		yearField.clear()
		applyFilters()
	}

	function saveCurrentFilter(name) {
		manager.saveModernDiveFilter(name, searchField.text, peopleField.text, tagsField.text, locationField.text,
			suitField.text, computerField.text, depthField.text, durationField.text, yearField.text)
	}

	function selectCollection(name) {
		activeCollection = name
		activeCollectionDiveIds = name.length > 0 ? NeoDiveCollections.diveIds(name) : []
	}

	function openCollections(diveId) {
		collectionsDialog.collectionDiveId = diveId === undefined ? -1 : diveId
		collectionsDialog.open()
	}

	Components.DiveActionSheet {
		id: diveActions
		onOpenDive: function(row) { page.openDive(row) }
		onAddToCollectionRequested: function(diveId) { page.openCollections(diveId) }
	}

	Connections {
		target: NeoDiveCollections
		function onCollectionsChanged() {
			if (page.activeCollection.length > 0)
				page.activeCollectionDiveIds = NeoDiveCollections.diveIds(page.activeCollection)
		}
	}

	ColumnLayout {
		anchors.fill: parent
		anchors.margins: tokens.space12
		spacing: tokens.space12

		RowLayout {
			Layout.fillWidth: true
			spacing: tokens.space8

			ColumnLayout {
				Layout.fillWidth: true
				spacing: 2
				Text {
					text: qsTr("Dives")
					color: tokens.textPrimary
					font.pixelSize: 26
					font.weight: Font.DemiBold
				}
				Text {
					text: page.activeCollection.length > 0 ? qsTr("Collection: %1").arg(page.activeCollection) :
						(diveListModel ? qsTr("%1 shown").arg(diveListModel.shown) : "")
					color: tokens.textSecondary
					font.pixelSize: 12
				}
			}

			Components.NeoButton {
				text: page.filterVisible ? qsTr("Close") : qsTr("Filter")
				onClicked: page.filterVisible = !page.filterVisible
			}
			Components.NeoButton { visible: page.wideLayout; compact: true; text: qsTr("Saved"); onClicked: savedFiltersDialog.open() }
			Components.NeoButton { visible: page.wideLayout; compact: true; text: page.activeCollection.length > 0 ? qsTr("Collection") : qsTr("Collections"); onClicked: page.openCollections() }
			Components.NeoButton { visible: page.wideLayout; compact: true; text: qsTr("Import"); onClicked: page.downloadRequested() }
			Components.NeoButton {
				variant: "primary"
				text: page.wideLayout ? qsTr("+ New dive") : "+"
				accessibleName: qsTr("Add a new dive")
				onClicked: page.addDiveRequested()
			}
			ToolButton {
				visible: !page.wideLayout
				text: "⋯"
				accessibleName: qsTr("More dive-list actions")
				onClicked: listActions.open()
			}
			Menu {
				id: listActions
				MenuItem { text: qsTr("Saved filters"); onTriggered: savedFiltersDialog.open() }
				MenuItem { text: page.activeCollection.length > 0 ? qsTr("Current collection") : qsTr("Collections"); onTriggered: page.openCollections() }
				MenuSeparator {}
				MenuItem { text: qsTr("Import dives"); onTriggered: page.downloadRequested() }
			}
		}

		Dialog {
			id: collectionsDialog
			parent: Overlay.overlay
			anchors.centerIn: parent
			width: Math.min(page.width - tokens.space24 * 2, 460)
			title: qsTr("Collections")
			modal: true
			standardButtons: Dialog.Close
			property int collectionDiveId: -1
			ColumnLayout {
				width: parent.width
				spacing: tokens.space8
				RowLayout {
					Layout.fillWidth: true
					Components.NeoTextField { id: collectionName; Layout.fillWidth: true; placeholderText: qsTr("New collection") }
					Components.NeoButton { text: qsTr("Create"); variant: "primary"; enabled: collectionName.text.trim().length > 0; onClicked: { NeoDiveCollections.create(collectionName.text); collectionName.clear() } }
				}
				Components.NeoButton { Layout.fillWidth: true; text: qsTr("Show all dives"); checkable: true; checked: page.activeCollection.length === 0; onClicked: { page.selectCollection(""); collectionsDialog.close() } }
				Text { visible: NeoDiveCollections.names.length === 0; text: qsTr("Collections keep your chosen dive IDs in Neo metadata without changing the dive log."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Repeater {
					model: NeoDiveCollections.names
					delegate: RowLayout {
						required property string modelData
						Layout.fillWidth: true
						Components.NeoButton {
							text: collectionsDialog.collectionDiveId >= 0 ? qsTr("Add to %1").arg(modelData) : modelData
							Layout.fillWidth: true
							onClicked: {
								if (collectionsDialog.collectionDiveId >= 0)
									NeoDiveCollections.addDive(modelData, collectionsDialog.collectionDiveId)
								else
									page.selectCollection(modelData)
								collectionsDialog.close()
							}
						}
						Components.NeoButton { text: qsTr("Remove"); variant: "danger"; onClicked: { if (page.activeCollection === modelData) page.selectCollection(""); NeoDiveCollections.remove(modelData) } }
					}
				}
			}
		}

		Dialog {
			id: savedFiltersDialog
			parent: Overlay.overlay
			anchors.centerIn: parent
			width: Math.min(page.width - tokens.space24 * 2, 460)
			title: qsTr("Saved filters")
			modal: true
			standardButtons: Dialog.Close
			ColumnLayout {
				width: parent.width
				spacing: tokens.space8
				RowLayout {
					Layout.fillWidth: true
					Components.NeoTextField { id: savedFilterName; Layout.fillWidth: true; placeholderText: qsTr("Name this filter") }
					Components.NeoButton { text: qsTr("Save"); variant: "primary"; enabled: savedFilterName.text.trim().length > 0; onClicked: { page.saveCurrentFilter(savedFilterName.text); savedFilterName.clear() } }
				}
				Text { visible: manager.savedDiveFilters.length === 0; text: qsTr("Save the current filters to reuse them here or in the desktop filter tool."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Repeater {
					model: manager.savedDiveFilters
					delegate: RowLayout {
						required property string modelData
						Layout.fillWidth: true
						Components.NeoButton { text: modelData; Layout.fillWidth: true; onClicked: { manager.applySavedDiveFilter(modelData); savedFiltersDialog.close() } }
						Components.NeoButton { text: qsTr("Remove"); variant: "danger"; onClicked: manager.removeSavedDiveFilter(modelData) }
					}
				}
			}
		}

		RowLayout {
			visible: page.filterVisible
			Layout.fillWidth: true
			spacing: tokens.space8

			Components.NeoTextField {
				id: searchField
				Layout.fillWidth: true
				placeholderText: qsTr("Search all dive data")
				onTextEdited: filterTimer.restart()
				onAccepted: page.applyFilters()
			}
			Components.NeoButton { compact: true; text: page.advancedFiltersVisible ? qsTr("Less") : qsTr("More"); onClicked: page.advancedFiltersVisible = !page.advancedFiltersVisible }
			Components.NeoButton { compact: true; variant: "ghost"; text: qsTr("Clear"); onClicked: page.clearFilters() }

			Timer {
				id: filterTimer
				interval: 250
				repeat: false
				onTriggered: page.applyFilters()
			}
		}

		Components.ModernCard {
			visible: page.filterVisible && page.advancedFiltersVisible
			Layout.fillWidth: true
			GridLayout {
				Layout.fillWidth: true
				columns: page.width >= 700 ? 3 : 2
				columnSpacing: tokens.space8
				rowSpacing: tokens.space8
				Components.NeoTextField { id: locationField; Layout.fillWidth: true; placeholderText: qsTr("Site or location"); onTextEdited: filterTimer.restart() }
				Components.NeoTextField { id: peopleField; Layout.fillWidth: true; placeholderText: qsTr("Buddy or guide"); onTextEdited: filterTimer.restart() }
				Components.NeoTextField { id: tagsField; Layout.fillWidth: true; placeholderText: qsTr("Tag"); onTextEdited: filterTimer.restart() }
				Components.NeoTextField { id: suitField; Layout.fillWidth: true; placeholderText: qsTr("Suit / gear"); onTextEdited: filterTimer.restart() }
				Components.NeoTextField { id: computerField; Layout.fillWidth: true; placeholderText: qsTr("Dive computer"); onTextEdited: filterTimer.restart() }
				Components.NeoTextField { id: yearField; Layout.fillWidth: true; placeholderText: qsTr("Year"); inputMethodHints: Qt.ImhDigitsOnly; onTextEdited: filterTimer.restart() }
				Components.NeoTextField { id: depthField; Layout.fillWidth: true; placeholderText: qsTr("Minimum depth"); inputMethodHints: Qt.ImhFormattedNumbersOnly; onTextEdited: filterTimer.restart() }
				Components.NeoTextField { id: durationField; Layout.fillWidth: true; placeholderText: qsTr("Minimum duration"); inputMethodHints: Qt.ImhFormattedNumbersOnly; onTextEdited: filterTimer.restart() }
			}
		}

		ListView {
			id: listView
			Layout.fillWidth: true
			Layout.fillHeight: true
			model: page.diveListModel
			spacing: tokens.space8
			clip: true
			boundsBehavior: Flickable.DragOverBounds
			maximumFlickVelocity: height * 5

			delegate: Item {
				id: delegateRoot
				required property int index
				property var modelData: model
				property bool longPressTriggered: false
				property bool collectionMatch: page.activeCollection.length === 0 || page.activeCollectionDiveIds.indexOf(modelData.id) >= 0
				width: listView.width
				height: modelData.isTrip ? 64 : (collectionMatch ? diveCard.implicitHeight : 0)

				Rectangle {
					anchors.fill: parent
					visible: delegateRoot.modelData.isTrip
					color: tokens.surfaceRaised
					radius: tokens.radius12
					border.width: 1
					border.color: tokens.border

					RowLayout {
						anchors.fill: parent
						anchors.margins: tokens.space12
						spacing: tokens.space12

						Text {
							text: delegateRoot.modelData.tripShortDate || ""
							color: tokens.accent
							font.pixelSize: 12
							font.weight: Font.DemiBold
						}
						Text {
							Layout.fillWidth: true
							text: delegateRoot.modelData.tripTitle || qsTr("Dive trip")
							color: tokens.textPrimary
							font.pixelSize: 15
							font.weight: Font.DemiBold
							elide: Text.ElideRight
						}
						Text {
							text: qsTr("%1 dives").arg(delegateRoot.modelData.tripNrDives || 0)
							color: tokens.textSecondary
							font.pixelSize: 12
						}
					}

					TapHandler {
						onTapped: page.diveListModel.toggle(delegateRoot.modelData.row)
					}
				}

				Components.ModernCard {
					id: diveCard
					visible: !delegateRoot.modelData.isTrip && delegateRoot.collectionMatch
					width: parent.width
					contentPadding: tokens.space12
					border.width: delegateRoot.modelData.current ? 1 : 0
					border.color: tokens.accent

					GridLayout {
						Layout.fillWidth: true
						columns: page.wideLayout ? 2 : 1
						columnSpacing: tokens.space16
						rowSpacing: tokens.space12

						ColumnLayout {
							Layout.preferredWidth: page.wideLayout ? 310 : -1
							Layout.fillWidth: true
							spacing: tokens.space8

							RowLayout {
								Layout.fillWidth: true
								spacing: tokens.space8
								Rectangle {
									visible: delegateRoot.modelData.number > 0
									Layout.preferredWidth: 54
									Layout.preferredHeight: 42
									color: "transparent"
									radius: tokens.radiusSmall
									border.width: 1
									border.color: tokens.accentStrong
									Text { anchors.centerIn: parent; text: "#" + delegateRoot.modelData.number; color: tokens.accent; font.pixelSize: 15; font.weight: Font.DemiBold }
								}
								ColumnLayout {
									Layout.fillWidth: true
									spacing: 2
									Text {
										Layout.fillWidth: true
										text: delegateRoot.modelData.location && delegateRoot.modelData.location.length > 0 ? delegateRoot.modelData.location : qsTr("Unnamed dive site")
										color: tokens.textPrimary
										font.pixelSize: 17
										font.weight: Font.DemiBold
										font.strikeout: delegateRoot.modelData.isInvalid === true
										elide: Text.ElideRight
									}
									Text { Layout.fillWidth: true; text: delegateRoot.modelData.dateTime || ""; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
							}

							GridLayout {
								Layout.fillWidth: true
								columns: 3
								columnSpacing: tokens.space8
								ColumnLayout {
									Layout.fillWidth: true
									spacing: 1
									Text { text: qsTr("MAX DEPTH"); color: tokens.textMuted; font.pixelSize: 8 }
									Text { text: delegateRoot.modelData.depth || "—"; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
								}
								ColumnLayout {
									Layout.fillWidth: true
									spacing: 1
									Text { text: qsTr("DURATION"); color: tokens.textMuted; font.pixelSize: 8 }
									Text { text: delegateRoot.modelData.duration || "—"; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
								}
								ColumnLayout {
									Layout.fillWidth: true
									spacing: 1
									Text { text: qsTr("WATER TEMP"); color: tokens.textMuted; font.pixelSize: 8 }
									Text { text: delegateRoot.modelData.waterTemp || "—"; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
								}
							}

							GridLayout {
								Layout.fillWidth: true
								columns: 2
								columnSpacing: tokens.space8
								rowSpacing: tokens.space4
								RowLayout {
									visible: delegateRoot.modelData.firstGas && delegateRoot.modelData.firstGas.length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "gas"; iconColor: tokens.accent; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: delegateRoot.modelData.firstGas || ""; color: tokens.accent; font.pixelSize: 11; elide: Text.ElideRight }
								}
								RowLayout {
									visible: miniProfile.diveMode.length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "regulator"; iconColor: tokens.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: miniProfile.diveMode; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
								RowLayout {
									visible: delegateRoot.modelData.cylinder && delegateRoot.modelData.cylinder.length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "gear"; iconColor: tokens.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: delegateRoot.modelData.cylinder || ""; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
								RowLayout {
									visible: delegateRoot.modelData.tags && delegateRoot.modelData.tags.length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "boat"; iconColor: tokens.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: delegateRoot.modelData.tags || ""; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
							}
						}

						Rectangle {
							Layout.fillWidth: true
							Layout.preferredHeight: page.wideLayout ? 112 : 132
							color: tokens.background
							radius: tokens.radiusSmall
							clip: true
							QMLProfile {
								id: miniProfile
								anchors.fill: parent
								diveId: delegateRoot.modelData.id
								Component.onCompleted: setMargin(3)
							}
						}
					}

					TapHandler {
						onLongPressed: {
							delegateRoot.longPressTriggered = true
							diveActions.openForDive(delegateRoot.modelData)
						}
						onTapped: {
							if (delegateRoot.longPressTriggered) {
								delegateRoot.longPressTriggered = false
								return
							}
							page.openDive(delegateRoot.modelData.row)
						}
					}
				}
			}

			footer: Item { height: tokens.space8 }
		}

		Text {
			visible: listView.count === 0 || (page.activeCollection.length > 0 && page.activeCollectionDiveIds.length === 0)
			Layout.fillWidth: true
			Layout.fillHeight: true
			text: manager.diveListProcessing ? qsTr("Updating dive log…") :
				(page.activeCollection.length > 0 ? qsTr("No dives in this collection") : qsTr("No dives in dive log"))
			color: tokens.textSecondary
			font.pixelSize: 14
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}

	}
}
