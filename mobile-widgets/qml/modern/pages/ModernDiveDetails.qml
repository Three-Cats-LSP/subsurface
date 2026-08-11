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
	title: currentItem && currentItem.modelData && currentItem.modelData.location && currentItem.modelData.location.length > 0
		   ? currentItem.modelData.location : qsTr("Dive details")
	background: Rectangle { color: tokens.background }

	property int initialRow: -1
	property alias currentIndex: diveView.currentIndex
	property var currentItem: diveView.currentItem

	signal editRequested(int diveId)

	Modern.DesignTokens { id: tokens }

	Component.onCompleted: {
		if (initialRow >= 0)
			manager.selectRow(initialRow)
	}

	Connections {
		target: swipeModel
		function onCurrentDiveChanged(index) {
			diveView.currentIndex = index.row
			diveView.positionViewAtIndex(index.row, ListView.Contain)
		}
	}

	ListView {
		id: diveView
		anchors.fill: parent
		model: swipeModel
		orientation: ListView.Horizontal
		interactive: false
		clip: true
		currentIndex: -1
		highlightFollowsCurrentItem: false

		delegate: Item {
			id: delegateRoot
			required property int index
			property var modelData: model
			width: diveView.width
			height: diveView.height

			Flickable {
				anchors.fill: parent
				contentWidth: width
				contentHeight: contentColumn.implicitHeight + tokens.space24 * 2
				flickableDirection: Flickable.VerticalFlick
				boundsBehavior: Flickable.StopAtBounds
				clip: true

				ColumnLayout {
					id: contentColumn
					width: parent.width
					spacing: tokens.space16

					ColumnLayout {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						Layout.topMargin: tokens.space12
						spacing: tokens.space4

						Text {
							Layout.fillWidth: true
							text: delegateRoot.modelData.location && delegateRoot.modelData.location.length > 0
								  ? delegateRoot.modelData.location : qsTr("Unnamed dive site")
							color: tokens.textPrimary
							font.pixelSize: 26
							font.weight: Font.DemiBold
							wrapMode: Text.WordWrap
						}
						Text {
							Layout.fillWidth: true
							text: (delegateRoot.modelData.dateTime || "") +
								  (delegateRoot.modelData.number > 0 ? qsTr("  ·  Dive #%1").arg(delegateRoot.modelData.number) : "")
							color: tokens.textSecondary
							font.pixelSize: 13
						}
					}

					GridLayout {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						columns: page.width >= 760 ? 5 : 2
						columnSpacing: tokens.space8
						rowSpacing: tokens.space8

						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Max depth")
							value: delegateRoot.modelData.depth || "—"
						}
						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Dive time")
							value: delegateRoot.modelData.duration || "—"
						}
						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Water temp")
							value: delegateRoot.modelData.waterTemp && delegateRoot.modelData.waterTemp.length > 0
								   ? delegateRoot.modelData.waterTemp : "—"
						}
						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Mode")
							value: delegateRoot.modelData.diveMode !== undefined && delegateRoot.modelData.diveMode.length > 0
								   ? delegateRoot.modelData.diveMode : "—"
						}
						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Gear")
							value: delegateRoot.modelData.suit && delegateRoot.modelData.suit.length > 0
								   ? delegateRoot.modelData.suit : "—"
						}
					}

					Components.ModernCard {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						padding: 0

						ColumnLayout {
							Layout.fillWidth: true
							spacing: 0

							RowLayout {
								Layout.fillWidth: true
								Layout.margins: tokens.space12
								spacing: tokens.space8

								ColumnLayout {
									Layout.fillWidth: true
									spacing: 2
									Text {
										text: qsTr("Dive profile")
										color: tokens.textPrimary
										font.pixelSize: 18
										font.weight: Font.DemiBold
									}
									Text {
										text: profile.numDC > 1 ? qsTr("%1 dive computers · use arrows to switch").arg(profile.numDC)
														  : qsTr("Profile calculated by the mature Subsurface engine")
										color: tokens.textSecondary
										font.pixelSize: 11
									}
								}

								ToolButton {
									visible: profile.numDC > 1
									text: "‹"
									onClicked: profile.prevDC()
								}
								ToolButton {
									visible: profile.numDC > 1
									text: "›"
									onClicked: profile.nextDC()
								}
							}

							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: Math.max(260, Math.min(440, page.height * 0.46))
								color: tokens.surfaceRaised
								clip: true

								QMLProfile {
									id: profile
									anchors.fill: parent
									diveId: delegateRoot.modelData.id
									clip: true
								}
							}
						}
					}

					GridLayout {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						columns: page.width >= 700 ? 3 : 1
						columnSpacing: tokens.space8
						rowSpacing: tokens.space8

						Components.ModernCard {
							Layout.fillWidth: true
							Text { text: qsTr("Gas"); color: tokens.textMuted; font.pixelSize: 10 }
							Text {
								Layout.fillWidth: true
								text: delegateRoot.modelData.firstGas && delegateRoot.modelData.firstGas.length > 0
									  ? delegateRoot.modelData.firstGas : qsTr("Not recorded")
								color: tokens.textPrimary
								font.pixelSize: 15
								wrapMode: Text.WordWrap
							}
						}
						Components.ModernCard {
							Layout.fillWidth: true
							Text { text: qsTr("Buddy"); color: tokens.textMuted; font.pixelSize: 10 }
							Text {
								Layout.fillWidth: true
								text: delegateRoot.modelData.buddy && delegateRoot.modelData.buddy.length > 0
									  ? delegateRoot.modelData.buddy : qsTr("Not recorded")
								color: tokens.textPrimary
								font.pixelSize: 15
								wrapMode: Text.WordWrap
							}
						}
						Components.ModernCard {
							Layout.fillWidth: true
							Text { text: qsTr("Tags / type"); color: tokens.textMuted; font.pixelSize: 10 }
							Text {
								Layout.fillWidth: true
								text: delegateRoot.modelData.tags && delegateRoot.modelData.tags.length > 0
									  ? delegateRoot.modelData.tags : qsTr("Not recorded")
								color: tokens.textPrimary
								font.pixelSize: 15
								wrapMode: Text.WordWrap
							}
						}
					}

					Components.ModernCard {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						Layout.bottomMargin: tokens.space24

						Text {
							text: qsTr("Notes")
							color: tokens.textMuted
							font.pixelSize: 10
						}
						Text {
							Layout.fillWidth: true
							text: delegateRoot.modelData.notes && delegateRoot.modelData.notes.length > 0
								  ? delegateRoot.modelData.notes : qsTr("No notes for this dive.")
							color: tokens.textPrimary
							font.pixelSize: 14
							wrapMode: Text.WordWrap
							textFormat: Text.PlainText
						}
					}
				}
			}
		}
	}

	footer: ToolBar {
		background: Rectangle { color: tokens.surface }
		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: tokens.space12
			anchors.rightMargin: tokens.space12
			Button {
				text: qsTr("Edit dive")
				enabled: page.currentItem && page.currentItem.modelData
				onClicked: page.editRequested(page.currentItem.modelData.id)
			}
			Item { Layout.fillWidth: true }
			Text {
				text: page.currentItem && page.currentItem.modelData && page.currentItem.modelData.isInvalid
					  ? qsTr("Marked invalid") : ""
				color: tokens.warning
				font.pixelSize: 12
			}
		}
	}
}
