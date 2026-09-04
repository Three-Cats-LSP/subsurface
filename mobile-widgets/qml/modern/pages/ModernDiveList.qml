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
	property bool plansOnly: false
	property bool wideLayout: width >= 760
	property bool filterVisible: false
	property bool advancedFiltersVisible: false
	property string activeCollection: ""
	property var activeCollectionDiveIds: []
	property bool selectionMode: false
	property var selectedDiveIds: []
	property var pendingDeleteIds: []

	signal openDive(int diveId)
	signal downloadRequested()
	signal addDiveRequested()
	signal cloudRequested()

	Modern.DesignTokens { id: tokens }

	function greeting() {
		var hour = new Date().getHours()
		if (hour < 12) return qsTr("Good morning")
		if (hour < 18) return qsTr("Good afternoon")
		return qsTr("Good evening")
	}

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

	function isDiveSelected(diveId) {
		return selectedDiveIds.indexOf(diveId) >= 0
	}

	function toggleDiveSelection(diveId) {
		var next = selectedDiveIds.slice()
		var position = next.indexOf(diveId)
		if (position >= 0)
			next.splice(position, 1)
		else
			next.push(diveId)
		selectedDiveIds = next
	}

	function startSelection(diveId) {
		selectionMode = true
		selectedDiveIds = diveId === undefined ? [] : [diveId]
	}

	function cancelSelection() {
		selectionMode = false
		selectedDiveIds = []
	}

	function confirmDelete(ids) {
		if (!ids || ids.length === 0)
			return
		pendingDeleteIds = ids.slice()
		deleteDivesDialog.open()
	}

	Components.DiveActionSheet {
		id: diveActions
		onOpenDive: function(diveId) { page.openDive(diveId) }
		onAddToCollectionRequested: function(diveId) { page.openCollections(diveId) }
	}

	Dialog {
		id: deleteDivesDialog
		parent: Overlay.overlay
		anchors.centerIn: parent
		modal: true
		focus: true
		title: page.pendingDeleteIds.length === 1 ? qsTr("Delete dive?") : qsTr("Delete %1 dives?").arg(page.pendingDeleteIds.length)
		standardButtons: Dialog.Cancel | Dialog.Ok
		onAccepted: {
			manager.deleteDives(page.pendingDeleteIds)
			page.pendingDeleteIds = []
			page.cancelSelection()
		}
		onRejected: page.pendingDeleteIds = []
		contentItem: Text {
			width: 320
			text: page.pendingDeleteIds.length === 1
				? qsTr("This removes the selected dive from the log. You can use Undo immediately afterwards if needed.")
				: qsTr("This removes all selected dives from the log in one action. You can use Undo immediately afterwards if needed.")
			color: tokens.textPrimary
			wrapMode: Text.WordWrap
		}
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
			spacing: tokens.space12
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 2
				Text { visible: !page.plansOnly; text: page.greeting(); color: tokens.textPrimary; font.pixelSize: page.wideLayout ? 30 : 25; font.weight: Font.DemiBold }
				Text { visible: page.plansOnly; text: qsTr("Plans"); color: tokens.textPrimary; font.pixelSize: page.wideLayout ? 30 : 25; font.weight: Font.DemiBold }
				Text { text: page.plansOnly ? qsTr("Saved decompression and dive plans") : qsTr("Here’s your diving summary"); color: tokens.textSecondary; font.pixelSize: 13 }
			}
			ToolButton {
				Accessible.name: qsTr("Cloud & Sync")
				contentItem: Components.NeoDiveIcon { name: "cloud"; iconColor: tokens.accent; width: 24; height: 24 }
				ToolTip.visible: hovered
				ToolTip.text: qsTr("Cloud & Sync")
				onClicked: page.cloudRequested()
			}
		}

		GridLayout {
			visible: !page.plansOnly
			Layout.fillWidth: true
			columns: page.wideLayout ? 4 : 3
			columnSpacing: page.wideLayout ? tokens.space16 : tokens.space8
			rowSpacing: tokens.space8
			Components.MetricCard { label: qsTr("Dives"); value: String(NeoDashboard.diveCount); iconName: "dives"; Layout.fillWidth: true; Layout.minimumWidth: 0 }
			Components.MetricCard { label: qsTr("Dive time"); value: NeoDashboard.totalTimeHours; suffix: qsTr("h"); iconName: "time"; Layout.fillWidth: true; Layout.minimumWidth: 0 }
			Components.MetricCard { label: qsTr("Max depth"); value: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepth : "—"; suffix: NeoDashboard.maxDepth.length > 0 ? NeoDashboard.maxDepthUnit : ""; iconName: "depth"; Layout.fillWidth: true; Layout.minimumWidth: 0 }
			Components.MetricCard { visible: page.wideLayout; label: qsTr("Avg water"); value: NeoDashboard.averageWaterTemp.length > 0 ? NeoDashboard.averageWaterTemp : "—"; iconName: "temperature"; Layout.fillWidth: true; Layout.minimumWidth: 0 }
		}

		RowLayout {
			Layout.fillWidth: true
			spacing: tokens.space8

			ColumnLayout {
				Layout.fillWidth: true
				spacing: 2
				Text {
					text: page.plansOnly ? qsTr("Saved plans") : qsTr("Dives")
					color: tokens.textPrimary
					font.pixelSize: 26
					font.weight: Font.DemiBold
				}
				Text {
					text: page.plansOnly ? qsTr("Plans are kept separate from completed and imported dives") :
						(page.activeCollection.length > 0 ? qsTr("Collection: %1").arg(page.activeCollection) : qsTr("Completed, imported and manually entered dives"))
					color: tokens.textSecondary
					font.pixelSize: 12
				}
			}

			Components.NeoButton {
				visible: !page.selectionMode
				text: page.filterVisible ? qsTr("Close") : qsTr("Filter")
				onClicked: page.filterVisible = !page.filterVisible
			}
			Components.NeoButton { visible: page.wideLayout && !page.selectionMode && !page.plansOnly; compact: true; text: qsTr("Saved"); onClicked: savedFiltersDialog.open() }
			Components.NeoButton { visible: page.wideLayout && !page.selectionMode && !page.plansOnly; compact: true; text: page.activeCollection.length > 0 ? qsTr("Collection") : qsTr("Collections"); onClicked: page.openCollections() }
			Components.NeoButton { visible: page.wideLayout && !page.selectionMode && !page.plansOnly; compact: true; text: qsTr("Import"); onClicked: page.downloadRequested() }
			Components.NeoButton { visible: page.wideLayout && !page.selectionMode; compact: true; text: qsTr("Select"); onClicked: page.startSelection() }
			Components.NeoButton { visible: page.selectionMode; compact: true; text: qsTr("Cancel"); onClicked: page.cancelSelection() }
			Components.NeoButton { visible: page.selectionMode; compact: true; variant: "danger"; enabled: page.selectedDiveIds.length > 0; text: qsTr("Delete (%1)").arg(page.selectedDiveIds.length); onClicked: page.confirmDelete(page.selectedDiveIds) }
			Components.NeoButton {
				visible: !page.selectionMode && !page.plansOnly
				variant: "primary"
				text: page.wideLayout ? qsTr("+ New dive") : "+"
				accessibleName: qsTr("Add a new dive")
				onClicked: page.addDiveRequested()
			}
			ToolButton {
				visible: !page.wideLayout && !page.selectionMode
				text: "⋯"
				Accessible.name: qsTr("More dive-list actions")
				onClicked: listActions.open()
			}
			Menu {
				id: listActions
				MenuItem { text: qsTr("Saved filters"); onTriggered: savedFiltersDialog.open() }
				MenuItem { text: page.activeCollection.length > 0 ? qsTr("Current collection") : qsTr("Collections"); onTriggered: page.openCollections() }
				MenuSeparator {}
				MenuItem { text: qsTr("Select dives"); onTriggered: page.startSelection() }
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
			standardButtons: Dialog.NoButton
			closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
			background: Rectangle { color: tokens.surface; radius: tokens.radius16; border.width: 1; border.color: tokens.border }
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
				Components.NeoButton { Layout.alignment: Qt.AlignRight; text: qsTr("Close"); variant: "ghost"; onClicked: collectionsDialog.close() }
			}
		}

		Dialog {
			id: savedFiltersDialog
			parent: Overlay.overlay
			anchors.centerIn: parent
			width: Math.min(page.width - tokens.space24 * 2, 460)
			title: qsTr("Saved filters")
			modal: true
			standardButtons: Dialog.NoButton
			closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
			background: Rectangle { color: tokens.surface; radius: tokens.radius16; border.width: 1; border.color: tokens.border }
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
				Components.NeoButton { Layout.alignment: Qt.AlignRight; text: qsTr("Close"); variant: "ghost"; onClicked: savedFiltersDialog.close() }
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
				// QAbstractItemModel's transient `model` wrapper is not a stable value to
				// retain in a var property on Qt 6. Bind the roles explicitly so compact
				// delegates receive updates instead of rendering empty, overlapping cards.
				// Do not declare any required delegate properties here: Qt switches to
				// required-property role injection when one is present, which suppresses
				// the `model.<role>` context object used below.
				property var modelData: ({
					"row": model.row,
					"id": model.id,
					"isTrip": model.isTrip,
					"current": model.current,
					"number": model.number,
					"location": model.location,
					"dateTime": model.dateTime,
					"depth": model.depth,
					"duration": model.duration,
					"waterTemp": model.waterTemp,
					"firstGas": model.firstGas,
					"cylinder": model.cylinder,
					"suit": model.suit,
					"tags": model.tags,
					"isPlanned": model.isPlanned,
					"planTitle": model.planTitle,
					"planRuntimeSeconds": model.planRuntimeSeconds,
					"planBottomTimeSeconds": model.planBottomTimeSeconds,
					"planDecoTimeSeconds": model.planDecoTimeSeconds,
					"isInvalid": model.isInvalid,
					"tripShortDate": model.tripShortDate,
					"tripTitle": model.tripTitle,
					"tripNrDives": model.tripNrDives
				})
				property bool longPressTriggered: false
				property bool typeMatch: !modelData.isTrip && modelData.isPlanned === page.plansOnly
				property bool collectionMatch: typeMatch && (page.activeCollection.length === 0 || page.activeCollectionDiveIds.indexOf(modelData.id) >= 0)
				function planClock(seconds) {
					seconds = Math.max(0, Number(seconds || 0)); var remainder = seconds % 60
					return Math.floor(seconds / 60) + ":" + (remainder < 10 ? "0" : "") + remainder
				}
				function firstListValue(value) {
					if (typeof value === "string")
						return value
					if (value && value.length > 0)
						return String(value[0])
					return ""
				}
				function gasAndCylinderSummary() {
					var gas = firstListValue(modelData.firstGas)
					if (gas.toUpperCase() === "AIR")
						gas = qsTr("Air")
					var cylinder = modelData.cylinder || ""
					return gas.length > 0 && cylinder.length > 0 ? gas + "  ·  " + cylinder : gas || cylinder
				}
				function activateDelegate() {
					if (modelData.isTrip)
						page.diveListModel.toggle(modelData.row)
					else if (page.selectionMode)
						page.toggleDiveSelection(modelData.id)
					else
						page.openDive(modelData.id)
				}
				activeFocusOnTab: height > 0
				Keys.onReturnPressed: activateDelegate()
				Keys.onEnterPressed: activateDelegate()
				Keys.onSpacePressed: activateDelegate()
				Accessible.role: Accessible.ListItem
				Accessible.name: modelData.isTrip ? qsTr("Dive trip: %1, %2 dives").arg(modelData.tripTitle || qsTr("Unnamed trip")).arg(modelData.tripNrDives || 0)
					: qsTr("Dive %1: %2, %3, %4, %5").arg(modelData.number > 0 ? "#" + modelData.number : qsTr("unnumbered")).arg(modelData.location || qsTr("Unnamed dive site")).arg(modelData.dateTime || qsTr("date unknown")).arg(modelData.depth || qsTr("depth unknown")).arg(modelData.duration || qsTr("duration unknown"))
				Accessible.onPressAction: activateDelegate()
				width: listView.width
				height: modelData.isTrip ? (page.plansOnly ? 0 : 64) : (collectionMatch ? diveCard.implicitHeight : 0)

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
						onTapped: delegateRoot.activateDelegate()
					}
				}

				Components.ModernCard {
					id: diveCard
					visible: !delegateRoot.modelData.isTrip && delegateRoot.collectionMatch
					width: parent.width
					contentPadding: tokens.space12
					border.width: delegateRoot.modelData.current || delegateRoot.activeFocus ? 1 : 0
					border.color: tokens.accent

					GridLayout {
						Layout.fillWidth: true
						columns: delegateRoot.modelData.isPlanned ? 1 : (page.wideLayout ? 2 : 1)
						columnSpacing: tokens.space16
						rowSpacing: tokens.space12

						ColumnLayout {
							Layout.preferredWidth: page.wideLayout && !delegateRoot.modelData.isPlanned ? 310 : -1
							Layout.fillWidth: true
							spacing: tokens.space8

							RowLayout {
								Layout.fillWidth: true
								spacing: tokens.space8
								CheckBox {
									visible: page.selectionMode
									checked: page.isDiveSelected(delegateRoot.modelData.id)
									Accessible.name: qsTr("Select dive")
									onClicked: page.toggleDiveSelection(delegateRoot.modelData.id)
								}
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
										text: delegateRoot.modelData.isPlanned ? delegateRoot.modelData.planTitle : (delegateRoot.modelData.location && delegateRoot.modelData.location.length > 0 ? delegateRoot.modelData.location : qsTr("Unnamed dive site"))
										color: tokens.textPrimary
										font.pixelSize: 17
										font.weight: Font.DemiBold
										font.strikeout: delegateRoot.modelData.isInvalid === true
										elide: Text.ElideRight
									}
									Text { Layout.fillWidth: true; text: delegateRoot.modelData.dateTime || ""; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
								ToolButton {
									visible: !page.selectionMode
									text: "⋯"
									Accessible.name: qsTr("Dive actions")
									onClicked: diveActions.openForDive(delegateRoot.modelData)
								}
							}

							GridLayout {
								Layout.fillWidth: true
								columns: delegateRoot.modelData.isPlanned ? 5 : 3
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
									Text { text: delegateRoot.modelData.isPlanned ? qsTr("RUN TIME") : qsTr("DURATION"); color: tokens.textMuted; font.pixelSize: 8 }
									Text { text: delegateRoot.modelData.isPlanned ? delegateRoot.planClock(delegateRoot.modelData.planRuntimeSeconds) : (delegateRoot.modelData.duration || "—"); color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
								}
								ColumnLayout {
									visible: delegateRoot.modelData.isPlanned
									Layout.fillWidth: true; spacing: 1
									Text { text: qsTr("BOTTOM TIME"); color: tokens.textMuted; font.pixelSize: 8 }
									Text { text: delegateRoot.planClock(delegateRoot.modelData.planBottomTimeSeconds); color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
								}
								ColumnLayout {
									visible: delegateRoot.modelData.isPlanned
									Layout.fillWidth: true; spacing: 1
									Text { text: qsTr("DECO TIME"); color: tokens.textMuted; font.pixelSize: 8 }
									Text { text: delegateRoot.planClock(delegateRoot.modelData.planDecoTimeSeconds); color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
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
								visible: !delegateRoot.modelData.isPlanned
								columns: 2
								columnSpacing: tokens.space8
								rowSpacing: tokens.space4
								RowLayout {
									visible: delegateRoot.gasAndCylinderSummary().length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "gas"; iconColor: tokens.accent; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: delegateRoot.gasAndCylinderSummary(); color: tokens.accent; font.pixelSize: 11; elide: Text.ElideRight }
								}
								RowLayout {
									visible: miniProfile.diveMode.length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "regulator"; iconColor: tokens.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: miniProfile.diveMode; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
								RowLayout {
									visible: delegateRoot.modelData.suit && delegateRoot.modelData.suit.length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "gear"; iconColor: tokens.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: delegateRoot.modelData.suit || ""; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
								RowLayout {
									visible: delegateRoot.modelData.tags && delegateRoot.modelData.tags.length > 0
									Layout.fillWidth: true
									spacing: tokens.space4
									Components.NeoDiveIcon { name: "type"; iconColor: tokens.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
									Text { Layout.fillWidth: true; text: delegateRoot.modelData.tags || ""; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
								}
							}
						}

						Rectangle {
							visible: !delegateRoot.modelData.isPlanned
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
							if (page.selectionMode)
								return
							delegateRoot.longPressTriggered = true
							diveActions.openForDive(delegateRoot.modelData)
						}
						onTapped: {
							if (delegateRoot.longPressTriggered) {
								delegateRoot.longPressTriggered = false
								return
							}
							delegateRoot.activateDelegate()
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
