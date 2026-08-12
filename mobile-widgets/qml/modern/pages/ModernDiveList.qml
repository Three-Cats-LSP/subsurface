// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.Page {
	id: page
	title: qsTr("Dives")
	background: Rectangle { color: tokens.background }

	property QtObject diveListModel: null
	property bool filterVisible: false
	property bool advancedFiltersVisible: false

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

	Components.DiveActionSheet {
		id: diveActions
		onOpenDive: function(row) { page.openDive(row) }
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
					text: qsTr("Dive log")
					color: tokens.textPrimary
					font.pixelSize: 26
					font.weight: Font.DemiBold
				}
				Text {
					text: diveListModel ? qsTr("%1 shown").arg(diveListModel.shown) : ""
					color: tokens.textSecondary
					font.pixelSize: 12
				}
			}

			Button {
				text: page.filterVisible ? qsTr("Close filters") : qsTr("Filter")
				onClicked: {
					page.filterVisible = !page.filterVisible
					if (!page.filterVisible)
						page.clearFilters()
				}
			}
		}

		RowLayout {
			visible: page.filterVisible
			Layout.fillWidth: true
			spacing: tokens.space8

			TextField {
				id: searchField
				Layout.fillWidth: true
				placeholderText: qsTr("Search all dive data")
				onTextEdited: filterTimer.restart()
				onAccepted: page.applyFilters()
			}
			Button { text: page.advancedFiltersVisible ? qsTr("Less") : qsTr("More"); onClicked: page.advancedFiltersVisible = !page.advancedFiltersVisible }
			Button { text: qsTr("Clear"); onClicked: page.clearFilters() }

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
				TextField { id: locationField; Layout.fillWidth: true; placeholderText: qsTr("Site or location"); onTextEdited: filterTimer.restart() }
				TextField { id: peopleField; Layout.fillWidth: true; placeholderText: qsTr("Buddy or guide"); onTextEdited: filterTimer.restart() }
				TextField { id: tagsField; Layout.fillWidth: true; placeholderText: qsTr("Tag"); onTextEdited: filterTimer.restart() }
				TextField { id: suitField; Layout.fillWidth: true; placeholderText: qsTr("Suit / gear"); onTextEdited: filterTimer.restart() }
				TextField { id: computerField; Layout.fillWidth: true; placeholderText: qsTr("Dive computer"); onTextEdited: filterTimer.restart() }
				TextField { id: yearField; Layout.fillWidth: true; placeholderText: qsTr("Year"); inputMethodHints: Qt.ImhDigitsOnly; onTextEdited: filterTimer.restart() }
				TextField { id: depthField; Layout.fillWidth: true; placeholderText: qsTr("Minimum depth"); inputMethodHints: Qt.ImhFormattedNumbersOnly; onTextEdited: filterTimer.restart() }
				TextField { id: durationField; Layout.fillWidth: true; placeholderText: qsTr("Minimum duration"); inputMethodHints: Qt.ImhFormattedNumbersOnly; onTextEdited: filterTimer.restart() }
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
				width: listView.width
				height: modelData.isTrip ? 64 : diveCard.implicitHeight

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
					visible: !delegateRoot.modelData.isTrip
					width: parent.width
					padding: tokens.space12
					border.width: delegateRoot.modelData.current ? 1 : 0
					border.color: tokens.accent

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space12

						ColumnLayout {
							Layout.fillWidth: true
							spacing: tokens.space4

							Text {
								Layout.fillWidth: true
								text: delegateRoot.modelData.location && delegateRoot.modelData.location.length > 0 ? delegateRoot.modelData.location : qsTr("Unnamed dive site")
								color: tokens.textPrimary
								font.pixelSize: 17
								font.weight: Font.DemiBold
								font.strikeout: delegateRoot.modelData.isInvalid === true
								elide: Text.ElideRight
							}

							Text {
								Layout.fillWidth: true
								text: delegateRoot.modelData.dateTime || ""
								color: tokens.textSecondary
								font.pixelSize: 12
							}
						}

						Text {
							text: delegateRoot.modelData.number > 0 ? "#" + delegateRoot.modelData.number : ""
							color: tokens.textSecondary
							font.pixelSize: 12
						}
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: tokens.space16

						ColumnLayout {
							spacing: 1
							Text { text: qsTr("Depth"); color: tokens.textMuted; font.pixelSize: 10 }
							Text { text: delegateRoot.modelData.depth || "—"; color: tokens.accent; font.pixelSize: 15; font.weight: Font.DemiBold }
						}
						ColumnLayout {
							spacing: 1
							Text { text: qsTr("Time"); color: tokens.textMuted; font.pixelSize: 10 }
							Text { text: delegateRoot.modelData.duration || "—"; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.Medium }
						}
						ColumnLayout {
							visible: delegateRoot.modelData.waterTemp !== undefined && delegateRoot.modelData.waterTemp.length > 0
							spacing: 1
							Text { text: qsTr("Water"); color: tokens.textMuted; font.pixelSize: 10 }
							Text { text: delegateRoot.modelData.waterTemp || ""; color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.Medium }
						}
						Item { Layout.fillWidth: true }
					}

					RowLayout {
						Layout.fillWidth: true
						visible: (delegateRoot.modelData.firstGas && delegateRoot.modelData.firstGas.length > 0) || (delegateRoot.modelData.suit && delegateRoot.modelData.suit.length > 0)
						spacing: tokens.space12

						Text {
							visible: delegateRoot.modelData.firstGas && delegateRoot.modelData.firstGas.length > 0
							text: delegateRoot.modelData.firstGas || ""
							color: tokens.textSecondary
							font.pixelSize: 12
						}
						Text {
							visible: delegateRoot.modelData.suit && delegateRoot.modelData.suit.length > 0
							text: delegateRoot.modelData.suit || ""
							color: tokens.textSecondary
							font.pixelSize: 12
							elide: Text.ElideRight
							Layout.fillWidth: true
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
			visible: listView.count === 0
			Layout.fillWidth: true
			Layout.fillHeight: true
			text: manager.diveListProcessing ? qsTr("Updating dive log…") : qsTr("No dives in dive log")
			color: tokens.textSecondary
			font.pixelSize: 14
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}

		RowLayout {
			Layout.fillWidth: true
			spacing: tokens.space8

			Button {
				Layout.fillWidth: true
				text: qsTr("Import")
				onClicked: page.downloadRequested()
			}
			Button {
				Layout.fillWidth: true
				text: qsTr("Add dive")
				onClicked: page.addDiveRequested()
			}
		}
	}
}
