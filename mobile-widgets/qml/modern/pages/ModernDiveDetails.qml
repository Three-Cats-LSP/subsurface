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

	function refreshCurrentProfile() {
		if (page.currentItem)
			page.currentItem.refreshProfile()
	}

	Component.onCompleted: {
		if (initialRow >= 0)
			manager.selectRow(initialRow)
	}

	Connections {
		target: swipeModel
		function onCurrentDiveChanged(index) {
			diveView.currentIndex = index.row
			if (!diveView.swipeInProgress)
				diveView.contentX = diveView.originX + index.row * diveView.width
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
		property bool swipeInProgress: false

		onWidthChanged: {
			if (currentIndex >= 0 && !swipeInProgress)
				contentX = originX + currentIndex * width
		}

		delegate: Item {
			id: delegateRoot
			required property int index
			property var modelData: model
			property bool panningProfile: false
			width: diveView.width
			height: diveView.height

			function resetProfileZoom() {
				profile.scale = 1.0
				profile.lastScale = 1.0
				profile.xOffset = 0
				profile.yOffset = 0
				profile.opacity = 1.0
				profileMouseArea.dragging = false
				panningProfile = false
				profile.triggerUpdate()
			}

			function refreshProfile() {
				profile.triggerUpdate()
			}

			ListView.onIsCurrentItemChanged: {
				if (!ListView.isCurrentItem) {
					resetProfileZoom()
					profileInspector.clear()
				}
			}

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

						Components.MetricCard { Layout.fillWidth: true; label: qsTr("Max depth"); value: delegateRoot.modelData.depth || "—" }
						Components.MetricCard { Layout.fillWidth: true; label: qsTr("Dive time"); value: delegateRoot.modelData.duration || "—" }
						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Water temp")
							value: delegateRoot.modelData.waterTemp && delegateRoot.modelData.waterTemp.length > 0 ? delegateRoot.modelData.waterTemp : "—"
						}
						Components.MetricCard { Layout.fillWidth: true; label: qsTr("Mode"); value: profile.diveMode.length > 0 ? profile.diveMode : "—" }
						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Gear")
							value: delegateRoot.modelData.suit && delegateRoot.modelData.suit.length > 0 ? delegateRoot.modelData.suit : "—"
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
									Text { text: qsTr("Dive profile"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
									Text {
										Layout.fillWidth: true
										color: tokens.textSecondary
										font.pixelSize: 11
										elide: Text.ElideRight
										text: {
											var device = profile.computerName.length > 0 ? profile.computerName : qsTr("Dive computer")
											if (profile.computerSerial.length > 0)
												device += " · " + profile.computerSerial
											if (profile.numDC > 1)
												device += qsTr(" · %1 of %2").arg(profile.currentDC + 1).arg(profile.numDC)
											return device
										}
									}
								}

								ToolButton { text: "☷"; accessibleName: qsTr("Profile controls"); onClicked: profileControls.open() }
								ToolButton { visible: profile.scale > 1.02; text: qsTr("Reset"); onClicked: delegateRoot.resetProfileZoom() }
								ToolButton { visible: profile.numDC > 1; text: "‹"; onClicked: { profile.prevDC(); profileInspector.clear() } }
								ToolButton { visible: profile.numDC > 1; text: "›"; onClicked: { profile.nextDC(); profileInspector.clear() } }
							}

							Rectangle {
								id: profileFrame
								Layout.fillWidth: true
								Layout.preferredHeight: Math.max(260, Math.min(440, page.height * 0.46))
								color: tokens.surfaceRaised
								clip: true

								QMLProfile {
									id: profile
									anchors.fill: parent
									diveId: delegateRoot.modelData.id
									clip: true
									property real lastScale: 1.0

									PinchArea {
										anchors.fill: parent
										pinch.dragAxis: Pinch.XAndYAxis
										onPinchStarted: profileInspector.clear()
										onPinchUpdated: {
											var nextScale = pinch.scale * profile.lastScale
											profile.scale = Math.max(1.0, Math.min(4.0, nextScale))
										}
										onPinchFinished: profile.lastScale = profile.scale

										MouseArea {
											id: profileMouseArea
											anchors.fill: parent
											property bool isZoomed: profile.scale > 1.02
											property bool dragging: false
											property real initialX
											property real initialY
											property real oldXOffset
											property real oldYOffset
											pressAndHoldInterval: isZoomed ? 50 : 50000
											propagateComposedEvents: true
											scrollGestureEnabled: true

											onPressed: function(mouse) {
												profileInspector.clear()
												if (!isZoomed)
													mouse.accepted = false
											}
											onPressAndHold: function(mouse) {
												dragging = true
												delegateRoot.panningProfile = true
												oldXOffset = profile.xOffset
												oldYOffset = profile.yOffset
												initialX = mouse.x
												initialY = mouse.y
												profile.opacity = 0.65
											}
											onPositionChanged: function(mouse) {
												if (!dragging)
													return
												profile.xOffset = oldXOffset + mouse.x - initialX
												profile.yOffset = oldYOffset + mouse.y - initialY
												profile.triggerUpdate()
											}
											onReleased: {
												dragging = false
												delegateRoot.panningProfile = false
												profile.opacity = 1.0
											}
											onCanceled: {
												dragging = false
												delegateRoot.panningProfile = false
												profile.opacity = 1.0
											}
											onWheel: function(wheel) {
												profileInspector.clear()
												var delta = wheel.angleDelta.y > 0 ? 0.2 : -0.2
												profile.scale = Math.max(1.0, Math.min(4.0, profile.scale + delta))
												profile.lastScale = profile.scale
												wheel.accepted = true
											}
										}
									}
								}

								MouseArea {
									id: profileInspector
									anchors.fill: parent
									enabled: profile.scale <= 1.02
									hoverEnabled: true
									preventStealing: true
									property var sampleInfo: ({})
									property real cursorX: 0
									property real cursorY: 0
									property bool activeSample: false

									function inspect(x, y) {
										var f = Math.max(0, Math.min(1, x / Math.max(1, width)))
										var info = profile.sampleAtFraction(f)
										if (!info || info.time === undefined) {
											clear()
											return
										}
										sampleInfo = info
										cursorX = Math.max(0, Math.min(width, info.fraction * width))
										cursorY = Math.max(0, Math.min(height, y))
										activeSample = true
									}
									function clear() {
										activeSample = false
										sampleInfo = ({})
									}

									onPressed: function(mouse) { inspect(mouse.x, mouse.y) }
									onPositionChanged: function(mouse) {
										if (pressed || containsMouse)
											inspect(mouse.x, mouse.y)
									}
									onExited: if (!pressed) clear()

									Rectangle {
										visible: profileInspector.activeSample
										x: profileInspector.cursorX
										y: 0
										width: 1
										height: parent.height
										color: tokens.accent
									}
									Rectangle {
										visible: profileInspector.activeSample
										x: 0
										y: profileInspector.cursorY
										width: parent.width
										height: 1
										color: tokens.accent
										opacity: 0.45
									}

									Rectangle {
										id: sampleTooltip
										visible: profileInspector.activeSample
										width: Math.min(250, profileInspector.width - tokens.space16)
										height: sampleColumn.implicitHeight + tokens.space12 * 2
										x: Math.max(tokens.space8, Math.min(profileInspector.width - width - tokens.space8,
											profileInspector.cursorX + 12 + width < profileInspector.width ? profileInspector.cursorX + 12 : profileInspector.cursorX - width - 12))
										y: Math.max(tokens.space8, Math.min(profileInspector.height - height - tokens.space8, profileInspector.cursorY - height / 2))
										color: tokens.surface
										radius: 12
										border.width: 1
										border.color: tokens.border

										Column {
											id: sampleColumn
											anchors.left: parent.left
											anchors.right: parent.right
											anchors.top: parent.top
											anchors.margins: tokens.space12
											spacing: 3
											Text { text: (profileInspector.sampleInfo.time || "—") + "  ·  " + (profileInspector.sampleInfo.depth || "—"); color: tokens.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
											Text { visible: !!profileInspector.sampleInfo.temperature; text: qsTr("Water %1").arg(profileInspector.sampleInfo.temperature || ""); color: tokens.textSecondary; font.pixelSize: 11 }
											Text {
												visible: profileInspector.sampleInfo.inDeco || profileInspector.sampleInfo.ndl !== undefined
												text: profileInspector.sampleInfo.inDeco
													  ? qsTr("Deco %1").arg(profileInspector.sampleInfo.decoStop || qsTr("required"))
													  : qsTr("NDL %1").arg(profileInspector.sampleInfo.ndl || "—")
												color: tokens.textSecondary; font.pixelSize: 11
											}
											Text { visible: !!profileInspector.sampleInfo.tts; text: qsTr("TTS %1").arg(profileInspector.sampleInfo.tts || ""); color: tokens.textSecondary; font.pixelSize: 11 }
											Text { visible: !!profileInspector.sampleInfo.pressure; text: qsTr("Pressure %1").arg(profileInspector.sampleInfo.pressure || ""); color: tokens.textSecondary; font.pixelSize: 11 }
											Text { visible: !!profileInspector.sampleInfo.setpoint; text: qsTr("Setpoint %1").arg(profileInspector.sampleInfo.setpoint || ""); color: tokens.textSecondary; font.pixelSize: 11 }
											Text { visible: !!profileInspector.sampleInfo.cns; text: qsTr("CNS %1").arg(profileInspector.sampleInfo.cns || ""); color: tokens.textSecondary; font.pixelSize: 11 }
										}
									}
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
							Text { Layout.fillWidth: true; text: delegateRoot.modelData.firstGas && delegateRoot.modelData.firstGas.length > 0 ? delegateRoot.modelData.firstGas : qsTr("Not recorded"); color: tokens.textPrimary; font.pixelSize: 15; wrapMode: Text.WordWrap }
						}
						Components.ModernCard {
							Layout.fillWidth: true
							Text { text: qsTr("Buddy"); color: tokens.textMuted; font.pixelSize: 10 }
							Text { Layout.fillWidth: true; text: delegateRoot.modelData.buddy && delegateRoot.modelData.buddy.length > 0 ? delegateRoot.modelData.buddy : qsTr("Not recorded"); color: tokens.textPrimary; font.pixelSize: 15; wrapMode: Text.WordWrap }
						}
						Components.ModernCard {
							Layout.fillWidth: true
							Text { text: qsTr("Tags / type"); color: tokens.textMuted; font.pixelSize: 10 }
							Text { Layout.fillWidth: true; text: delegateRoot.modelData.tags && delegateRoot.modelData.tags.length > 0 ? delegateRoot.modelData.tags : qsTr("Not recorded"); color: tokens.textPrimary; font.pixelSize: 15; wrapMode: Text.WordWrap }
						}
					}

					Components.ModernCard {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						Layout.bottomMargin: tokens.space24
						Text { text: qsTr("Notes"); color: tokens.textMuted; font.pixelSize: 10 }
						Text {
							Layout.fillWidth: true
							text: delegateRoot.modelData.notes && delegateRoot.modelData.notes.length > 0 ? delegateRoot.modelData.notes : qsTr("No notes for this dive.")
							color: tokens.textPrimary
							font.pixelSize: 14
							wrapMode: Text.WordWrap
							textFormat: Text.PlainText
						}
					}
				}
			}
		}

		NumberAnimation {
			id: snapAnimation
			target: diveView
			property: "contentX"
			duration: 250
			easing.type: Easing.OutCubic
			onRunningChanged: if (!running) diveView.swipeInProgress = false
		}

		DragHandler {
			id: horizontalSwipeHandler
			enabled: !diveView.currentItem || !diveView.currentItem.panningProfile
			yAxis.enabled: false
			target: null
			property real startContentX
			property real startFingerX
			property real lastTranslationX: 0

			onActiveChanged: {
				if (active) {
					startContentX = diveView.contentX
					startFingerX = centroid.position.x
					lastTranslationX = 0
					diveView.swipeInProgress = true
				} else if (diveView.swipeInProgress) {
					var dx = lastTranslationX
					var targetIndex = diveView.currentIndex
					if (dx < -diveView.width / 4 && targetIndex < diveView.count - 1)
						targetIndex++
					else if (dx > diveView.width / 4 && targetIndex > 0)
						targetIndex--
					snapAnimation.to = diveView.originX + targetIndex * diveView.width
					snapAnimation.start()
					diveView.currentIndex = targetIndex
					manager.selectSwipeRow(targetIndex)
				}
			}

			onActiveTranslationChanged: {
				if (!active)
					return
				var dx = centroid.position.x - startFingerX
				lastTranslationX = dx
				diveView.contentX = startContentX - dx
			}
		}
	}

	Popup {
		id: profileControls
		parent: Overlay.overlay
		modal: true
		focus: true
		closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
		width: Math.min(page.width - tokens.space16 * 2, 460)
		height: Math.min(implicitHeight, page.height - tokens.space24 * 2)
		x: Math.max(tokens.space8, (page.width - width) / 2)
		y: Math.max(tokens.space12, page.height - height - tokens.space16)
		padding: tokens.space16
		background: Rectangle { color: tokens.surface; radius: 18; border.width: 1; border.color: tokens.border }

		contentItem: Flickable {
			implicitHeight: Math.min(controlsColumn.implicitHeight, page.height * 0.72)
			contentWidth: width
			contentHeight: controlsColumn.implicitHeight
			clip: true
			flickableDirection: Flickable.VerticalFlick

			ColumnLayout {
				id: controlsColumn
				width: parent.width
				spacing: tokens.space8

				RowLayout {
					Layout.fillWidth: true
					Text { Layout.fillWidth: true; text: qsTr("Profile controls"); color: tokens.textPrimary; font.pixelSize: 20; font.weight: Font.DemiBold }
					ToolButton { text: "×"; onClicked: profileControls.close() }
				}

				Text { text: qsTr("Decompression"); color: tokens.textMuted; font.pixelSize: 11 }
				Switch { Layout.fillWidth: true; text: qsTr("NDL / TTS"); checked: ProfilePrefs.calcndltts; onToggled: { ProfilePrefs.calcndltts = checked; page.refreshCurrentProfile() } }
				Switch { Layout.fillWidth: true; text: qsTr("Ceiling"); checked: ProfilePrefs.calcceiling; onToggled: { ProfilePrefs.calcceiling = checked; page.refreshCurrentProfile() } }
				Switch { Layout.fillWidth: true; text: qsTr("Tissue ceiling"); checked: ProfilePrefs.calcalltissues; onToggled: { ProfilePrefs.calcalltissues = checked; page.refreshCurrentProfile() } }

				Text { text: qsTr("Gases"); color: tokens.textMuted; font.pixelSize: 11 }
				Switch { Layout.fillWidth: true; text: qsTr("Tissue saturation"); checked: ProfilePrefs.percentagegraph; onToggled: { ProfilePrefs.percentagegraph = checked; page.refreshCurrentProfile() } }
				Switch { Layout.fillWidth: true; text: qsTr("MOD"); checked: ProfilePrefs.mod; onToggled: { ProfilePrefs.mod = checked; page.refreshCurrentProfile() } }

				Text { text: qsTr("Cylinder"); color: tokens.textMuted; font.pixelSize: 11 }
				Switch { Layout.fillWidth: true; text: qsTr("Tank pressure"); checked: ProfilePrefs.tankbar; onToggled: { ProfilePrefs.tankbar = checked; page.refreshCurrentProfile() } }
				Switch { Layout.fillWidth: true; text: qsTr("SAC"); checked: ProfilePrefs.show_sac; onToggled: { ProfilePrefs.show_sac = checked; page.refreshCurrentProfile() } }

				Text { text: qsTr("Events & overlays"); color: tokens.textMuted; font.pixelSize: 11 }
				Switch { Layout.fillWidth: true; text: qsTr("Dive-computer ceiling"); checked: ProfilePrefs.dcceiling; onToggled: { ProfilePrefs.dcceiling = checked; page.refreshCurrentProfile() } }
				Switch { Layout.fillWidth: true; text: qsTr("Pictures"); checked: ProfilePrefs.show_pictures_in_profile; onToggled: { ProfilePrefs.show_pictures_in_profile = checked; page.refreshCurrentProfile() } }
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
				text: page.currentItem && page.currentItem.modelData && page.currentItem.modelData.isInvalid ? qsTr("Marked invalid") : ""
				color: tokens.warning
				font.pixelSize: 12
			}
		}
	}
}
