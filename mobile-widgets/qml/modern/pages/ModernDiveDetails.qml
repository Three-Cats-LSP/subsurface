// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
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
	property bool editOnReady: false
	property bool editOpened: false
	property alias currentIndex: diveView.currentIndex
	property var currentItem: diveView.currentItem
	property string diveReportPdfExport: ""
	property string diveReportTextExport: ""

	signal editRequested(var dive)
	FolderDialog {
		id: diveReportFolder
		currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
		onAccepted: {
			if (page.currentItem && page.currentItem.modelData)
				page.diveReportPdfExport = manager.exportNeoDiveReportPdf(selectedFolder, page.diveReportText(page.currentItem.modelData))
		}
	}
	FolderDialog {
		id: diveReportTextFolder
		currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
		onAccepted: {
			if (page.currentItem && page.currentItem.modelData)
				page.diveReportTextExport = manager.exportNeoDiveReportText(selectedFolder, page.diveReportText(page.currentItem.modelData))
		}
	}
	function diveReportText(dive) {
		var lines = [qsTr("SUBSURFACE NEO DIVE REPORT"), qsTr("Dive: %1").arg(dive.number > 0 ? "#" + dive.number : qsTr("Unnumbered")), qsTr("Date: %1").arg(dive.dateTime || "—"), qsTr("Site: %1").arg(dive.location || qsTr("Unnamed dive site")), qsTr("Maximum depth: %1").arg(dive.depth || "—"), qsTr("Duration: %1").arg(dive.duration || "—"), qsTr("Water temperature: %1").arg(dive.waterTemp || "—"), qsTr("Equipment: %1").arg(dive.suit || "—")]
		if (dive.notes && dive.notes.length > 0)
			lines.push("", qsTr("NOTES"), dive.notes)
		lines.push("", qsTr("This report is generated from the current canonical Subsurface dive record."))
		return lines.join("\n")
	}

	Modern.DesignTokens { id: tokens }

	function refreshCurrentProfile() {
		if (page.currentItem)
			page.currentItem.refreshProfile()
	}

	function openEditorWhenReady() {
		if (editOnReady && !editOpened && currentItem && currentItem.modelData) {
			editOpened = true
			editRequested(currentItem.modelData)
		}
	}

	Component.onCompleted: {
		if (initialRow >= 0)
			manager.selectRow(initialRow)
		Qt.callLater(openEditorWhenReady)
	}

	Connections {
		target: swipeModel
		function onCurrentDiveChanged(index) {
			diveView.currentIndex = index.row
			if (!diveView.swipeInProgress)
				diveView.contentX = diveView.originX + index.row * diveView.width
			Qt.callLater(page.openEditorWhenReady)
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
			// A graph gesture owns the pointer until it ends. This keeps inspection,
			// pan/zoom, vertical page scroll and horizontal dive swiping separate.
			property bool profileGestureActive: false
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

			function clampProfileOffsets() {
				if (profile.scale <= 1.0) {
					profile.xOffset = 0
					profile.yOffset = 0
					return
				}
				var maxX = profileFrame.width * (profile.scale - 1.0) / (2.0 * profile.scale)
				var maxY = profileFrame.height * (profile.scale - 1.0) / (2.0 * profile.scale)
				profile.xOffset = Math.max(-maxX, Math.min(maxX, profile.xOffset))
				profile.yOffset = Math.max(-maxY, Math.min(maxY, profile.yOffset))
			}

			function zoomProfileAt(nextScale, focusX, focusY, previousFocusX, previousFocusY) {
				var oldScale = profile.scale
				nextScale = Math.max(1.0, Math.min(4.0, nextScale))
				if (Math.abs(nextScale - oldScale) < 0.001 &&
					Math.abs(focusX - previousFocusX) < 0.1 && Math.abs(focusY - previousFocusY) < 0.1)
					return
				var centerX = profileFrame.width / 2.0
				var centerY = profileFrame.height / 2.0
				profile.xOffset += (focusX - centerX) / nextScale - (previousFocusX - centerX) / oldScale
				profile.yOffset += (focusY - centerY) / nextScale - (previousFocusY - centerY) / oldScale
				profile.scale = nextScale
				clampProfileOffsets()
				profile.triggerUpdate()
			}

			function refreshProfile() {
				profile.triggerUpdate()
			}

			function pressureSummary() {
				var start = modelData.startPressure || ""
				var end = modelData.endPressure || ""
				var pressure = start.length > 0 && end.length > 0 ? start + " → " + end : start || end
				var sac = modelData.sac || ""
				return pressure.length > 0 && sac.length > 0 ? pressure + "  ·  SAC " + sac : pressure || sac
			}

			function decoModelSummary() {
				if (PrefTechnicalDetails.display_deco_mode === Enums.VPMB)
					return qsTr("VPM-B")
				if (PrefTechnicalDetails.display_deco_mode === Enums.BUEHLMANN)
					return qsTr("Bühlmann ZHL-16C  ·  GF %1/%2").arg(PrefTechnicalDetails.gflow).arg(PrefTechnicalDetails.gfhigh)
				return qsTr("Recreational (NDL)")
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

						RowLayout {
							Layout.fillWidth: true
							spacing: tokens.space8

							Rectangle {
								visible: delegateRoot.modelData.number > 0
								Layout.preferredWidth: 64
								Layout.preferredHeight: 48
								radius: tokens.radiusSmall
								color: "transparent"
								border.width: 1
								border.color: tokens.accentStrong
								Text { anchors.centerIn: parent; text: "#" + delegateRoot.modelData.number; color: tokens.accent; font.pixelSize: 19; font.weight: Font.Medium }
							}

							ColumnLayout {
								Layout.fillWidth: true
								spacing: 2
								Text {
									Layout.fillWidth: true
									text: delegateRoot.modelData.location && delegateRoot.modelData.location.length > 0
										  ? delegateRoot.modelData.location : qsTr("Unnamed dive site")
									color: tokens.textPrimary
									font.pixelSize: page.width >= 760 ? 28 : 21
									font.weight: Font.DemiBold
									elide: Text.ElideRight
								}
								Text { Layout.fillWidth: true; text: delegateRoot.modelData.dateTime || ""; color: tokens.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
							}

							ToolButton { text: "⋯"; accessibleName: qsTr("Dive actions"); onClicked: diveActions.open() }
							Menu {
								id: diveActions
								MenuItem { text: qsTr("Edit dive"); onTriggered: page.editRequested(delegateRoot.modelData) }
								MenuSeparator {}
								MenuItem { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save PDF report"); onTriggered: diveReportFolder.open() }
								MenuItem { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save text report"); onTriggered: diveReportTextFolder.open() }
							}
						}
						Text { visible: page.diveReportPdfExport.length > 0; text: qsTr("PDF saved: %1").arg(page.diveReportPdfExport); color: tokens.success; wrapMode: Text.Wrap; Layout.fillWidth: true }
						Text { visible: page.diveReportTextExport.length > 0; text: qsTr("Text saved: %1").arg(page.diveReportTextExport); color: tokens.success; wrapMode: Text.Wrap; Layout.fillWidth: true }
					}

					GridLayout {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						columns: 3
						columnSpacing: tokens.space8
						rowSpacing: tokens.space8

						Components.MetricCard { Layout.fillWidth: true; label: qsTr("Max depth"); value: delegateRoot.modelData.depth || "—"; iconName: "depth" }
						Components.MetricCard { Layout.fillWidth: true; label: qsTr("Dive time"); value: delegateRoot.modelData.duration || "—"; iconName: "time" }
						Components.MetricCard {
							Layout.fillWidth: true
							label: qsTr("Water temp")
							value: delegateRoot.modelData.waterTemp && delegateRoot.modelData.waterTemp.length > 0 ? delegateRoot.modelData.waterTemp : "—"
							iconName: "temperature"
						}
					}

					Components.ModernCard {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						contentPadding: 0

						ColumnLayout {
							Layout.fillWidth: true
							spacing: 0

							RowLayout {
								Layout.fillWidth: true
								Layout.margins: tokens.space12
								spacing: tokens.space8

								Components.NeoDiveIcon {
									name: "tank"
									iconColor: tokens.accent
									Layout.preferredWidth: 22
									Layout.preferredHeight: 22
								}
								Text {
									Layout.fillWidth: true
									color: tokens.textSecondary
									font.pixelSize: 11
									elide: Text.ElideRight
									text: {
										var parts = []
										parts.push(profile.computerName.length > 0 ? profile.computerName : qsTr("Dive computer"))
										if (profile.diveMode.length > 0)
											parts.push(profile.diveMode)
										parts.push(delegateRoot.decoModelSummary())
										if (profile.numDC > 1)
											parts.push(qsTr("Computer %1/%2").arg(profile.currentDC + 1).arg(profile.numDC))
										return parts.join("  ·  ")
									}
								}

								ToolButton { text: "☷"; accessibleName: qsTr("Profile controls"); onClicked: profileControls.open() }
								ToolButton { visible: profile.scale > 1.02; text: qsTr("Reset"); onClicked: delegateRoot.resetProfileZoom() }
								ComboBox {
									visible: profile.numDC > 1
									model: profile.numDC
									currentIndex: profile.currentDC
									delegate: ItemDelegate { required property int index; required property var modelData; text: qsTr("Computer %1").arg(index + 1) }
									displayText: qsTr("DC %1/%2").arg(profile.currentDC + 1).arg(profile.numDC)
									onActivated: function(index) { profile.setCurrentDC(index); profileInspector.clear() }
								}
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
									property real lastPinchCenterX: 0
									property real lastPinchCenterY: 0

									PinchArea {
										anchors.fill: parent
										pinch.dragAxis: Pinch.XAndYAxis
										onPinchStarted: {
											delegateRoot.profileGestureActive = true
											profileInspector.clear()
											profile.lastPinchCenterX = pinch.center.x
											profile.lastPinchCenterY = pinch.center.y
										}
										onPinchUpdated: {
											var nextScale = pinch.scale * profile.lastScale
											delegateRoot.zoomProfileAt(nextScale, pinch.center.x, pinch.center.y,
												profile.lastPinchCenterX, profile.lastPinchCenterY)
											profile.lastPinchCenterX = pinch.center.x
											profile.lastPinchCenterY = pinch.center.y
										}
										onPinchFinished: { profile.lastScale = profile.scale; delegateRoot.profileGestureActive = false }
										onPinchCanceled: { profile.lastScale = profile.scale; delegateRoot.profileGestureActive = false }

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
												delegateRoot.profileGestureActive = true
												oldXOffset = profile.xOffset
												oldYOffset = profile.yOffset
												initialX = mouse.x
												initialY = mouse.y
												profile.opacity = 0.65
											}
											onPositionChanged: function(mouse) {
												if (!dragging)
													return
												profile.xOffset = oldXOffset + (mouse.x - initialX) / profile.scale
												profile.yOffset = oldYOffset + (mouse.y - initialY) / profile.scale
												delegateRoot.clampProfileOffsets()
												profile.triggerUpdate()
											}
											onReleased: {
												dragging = false
												delegateRoot.panningProfile = false
												delegateRoot.profileGestureActive = false
												profile.opacity = 1.0
											}
											onCanceled: {
												dragging = false
												delegateRoot.panningProfile = false
												delegateRoot.profileGestureActive = false
												profile.opacity = 1.0
											}
											onWheel: function(wheel) {
												profileInspector.clear()
												var delta = wheel.angleDelta.y > 0 ? 0.2 : -0.2
												delegateRoot.zoomProfileAt(profile.scale + delta, wheel.x, wheel.y, wheel.x, wheel.y)
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
									preventStealing: touchInspection
									property var sampleInfo: ({})
									property real cursorX: 0
									property real cursorY: 0
									property bool activeSample: false
									property bool touchInspection: false
									pressAndHoldInterval: 280

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

									onPressAndHold: function(mouse) {
										touchInspection = true
										delegateRoot.profileGestureActive = true
										inspect(mouse.x, mouse.y)
									}
									onPositionChanged: function(mouse) {
										if (touchInspection || (!pressed && containsMouse))
											inspect(mouse.x, mouse.y)
									}
									onReleased: { touchInspection = false; delegateRoot.profileGestureActive = false }
									onCanceled: { touchInspection = false; delegateRoot.profileGestureActive = false; clear() }
									onExited: if (!pressed) clear()

									Rectangle {
										visible: profileInspector.activeSample
										x: profileInspector.cursorX
										y: 0
										width: 1
										height: parent.height
										color: tokens.accent
									}

									Repeater {
										model: profile.profileMarkers
										delegate: Item {
											id: markerDelegate
											required property var modelData
											property bool pinned: false
											width: 24
											height: parent.height
											x: Math.max(-width / 2, Math.min(parent.width - width / 2, modelData.fraction * parent.width - width / 2))
											Accessible.name: qsTr("%1 at %2").arg(modelData.label).arg(modelData.time)
											Accessible.role: Accessible.Button
											ToolTip.visible: markerHover.hovered || pinned
											ToolTip.text: qsTr("%1 at %2").arg(modelData.label).arg(modelData.time)
											HoverHandler { id: markerHover }
											TapHandler { onTapped: markerDelegate.pinned = !markerDelegate.pinned }

											Rectangle {
												anchors.horizontalCenter: parent.horizontalCenter
												width: 2
												height: parent.height
												color: parent.modelData.gasSwitch ? tokens.warning : tokens.accent
												opacity: parent.pinned ? 1.0 : 0.7
											}
										}
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
										width: Math.min(280, profileInspector.width - tokens.space16)
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

											Text {
												text: (profileInspector.sampleInfo.time || "—") + "  ·  " + (profileInspector.sampleInfo.depth || "—")
												color: tokens.textPrimary
												font.pixelSize: 14
												font.weight: Font.DemiBold
											}
											Text { visible: !!profileInspector.sampleInfo.temperature; text: qsTr("Water %1").arg(profileInspector.sampleInfo.temperature || ""); color: tokens.textSecondary; font.pixelSize: 11 }
											Text {
												visible: profileInspector.sampleInfo.inDeco || profileInspector.sampleInfo.ndl !== undefined
												text: profileInspector.sampleInfo.inDeco
													  ? qsTr("Deco %1").arg(profileInspector.sampleInfo.decoStop || qsTr("required"))
													  : qsTr("NDL %1").arg(profileInspector.sampleInfo.ndl || "—")
												color: tokens.textSecondary
												font.pixelSize: 11
											}
											Text { visible: !!profileInspector.sampleInfo.tts; text: qsTr("TTS %1").arg(profileInspector.sampleInfo.tts || ""); color: tokens.textSecondary; font.pixelSize: 11 }

											Rectangle {
												visible: profileInspector.sampleInfo.gf !== undefined || profileInspector.sampleInfo.surfaceGf !== undefined || !!profileInspector.sampleInfo.calculatedCeiling || !!profileInspector.sampleInfo.calculatedNdl || !!profileInspector.sampleInfo.calculatedTts
												width: parent.width
												height: 1
												color: tokens.border
											}
											Text { visible: profileInspector.sampleInfo.gf !== undefined; text: qsTr("GF %1").arg(profileInspector.sampleInfo.gf || "—"); color: tokens.textSecondary; font.pixelSize: 11 }
											Text { visible: profileInspector.sampleInfo.surfaceGf !== undefined; text: qsTr("Surface GF %1").arg(profileInspector.sampleInfo.surfaceGf || "—"); color: tokens.textSecondary; font.pixelSize: 11 }
											Text { visible: !!profileInspector.sampleInfo.calculatedCeiling; text: qsTr("Calculated ceiling %1").arg(profileInspector.sampleInfo.calculatedCeiling || ""); color: tokens.textSecondary; font.pixelSize: 11 }
											Text { visible: !!profileInspector.sampleInfo.calculatedNdl; text: qsTr("Calculated NDL %1").arg(profileInspector.sampleInfo.calculatedNdl || ""); color: tokens.textSecondary; font.pixelSize: 11 }
											Text { visible: !!profileInspector.sampleInfo.calculatedTts; text: qsTr("Calculated TTS %1").arg(profileInspector.sampleInfo.calculatedTts || ""); color: tokens.textSecondary; font.pixelSize: 11 }

											Rectangle {
												visible: !!profileInspector.sampleInfo.pressure || !!profileInspector.sampleInfo.setpoint || !!profileInspector.sampleInfo.cns
												width: parent.width
												height: 1
												color: tokens.border
											}
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
						columns: page.width >= 760 ? 5 : 6
						columnSpacing: tokens.space8
						rowSpacing: tokens.space8

						Components.DiveInfoCard {
							Layout.fillWidth: true
							Layout.columnSpan: page.width >= 760 ? 1 : 3
							label: qsTr("Gas")
							value: delegateRoot.modelData.firstGas && delegateRoot.modelData.firstGas.length > 0 ? delegateRoot.modelData.firstGas : qsTr("Not recorded")
							detail: delegateRoot.pressureSummary()
							iconName: "gas"
						}
						Components.DiveInfoCard {
							Layout.fillWidth: true
							Layout.columnSpan: page.width >= 760 ? 1 : 3
							label: qsTr("Gear")
							value: delegateRoot.modelData.cylinder && delegateRoot.modelData.cylinder.length > 0 ? delegateRoot.modelData.cylinder : qsTr("Not recorded")
							detail: delegateRoot.modelData.suit || ""
							iconName: "gear"
						}
						Components.DiveInfoCard {
							Layout.fillWidth: true
							Layout.columnSpan: page.width >= 760 ? 1 : 2
							label: qsTr("Mode")
							value: profile.diveMode.length > 0 ? profile.diveMode : qsTr("Not recorded")
							detail: profile.diveMode.length > 0 ? qsTr("Recorded dive mode") : ""
							iconName: "regulator"
						}
						Components.DiveInfoCard {
							Layout.fillWidth: true
							Layout.columnSpan: page.width >= 760 ? 1 : 2
							label: qsTr("Type")
							value: delegateRoot.modelData.tags && delegateRoot.modelData.tags.length > 0 ? delegateRoot.modelData.tags : qsTr("Not recorded")
							iconName: "boat"
						}
						Components.DiveInfoCard {
							Layout.fillWidth: true
							Layout.columnSpan: page.width >= 760 ? 1 : 2
							label: qsTr("Buddy")
							value: delegateRoot.modelData.buddy && delegateRoot.modelData.buddy.length > 0 ? delegateRoot.modelData.buddy : qsTr("Not recorded")
							detail: delegateRoot.modelData.diveGuide || ""
							iconName: "buddy"
						}
					}

					Components.ModernCard {
						Layout.fillWidth: true
						Layout.leftMargin: tokens.space16
						Layout.rightMargin: tokens.space16
						Layout.bottomMargin: tokens.space24
						RowLayout {
							Layout.fillWidth: true
							Layout.alignment: Qt.AlignTop
							spacing: tokens.space8

							Components.NeoDiveIcon { name: "notes"; iconColor: tokens.accent; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
							ColumnLayout {
								Layout.fillWidth: true
								spacing: 3
								Text { text: qsTr("NOTES"); color: tokens.textMuted; font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 0.7 }
								Text {
									Layout.fillWidth: true
									text: delegateRoot.modelData.notes && delegateRoot.modelData.notes.length > 0 ? delegateRoot.modelData.notes : qsTr("No notes for this dive.")
									color: tokens.textPrimary
									font.pixelSize: 13
									wrapMode: Text.WordWrap
									textFormat: Text.PlainText
								}
							}
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
			enabled: !diveView.currentItem || !diveView.currentItem.profileGestureActive
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
		property bool desktopPanel: page.width >= 720
		modal: !desktopPanel
		focus: true
		closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
		width: desktopPanel ? Math.min(380, page.width - tokens.space32) : Math.min(page.width - tokens.space16 * 2, 460)
		height: Math.min(implicitHeight, page.height - tokens.space24 * 2)
		x: desktopPanel ? Math.max(tokens.space16, page.width - width - tokens.space16) : Math.max(tokens.space8, (page.width - width) / 2)
		y: desktopPanel ? tokens.space16 : Math.max(tokens.space12, page.height - height - tokens.space16)
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
				Switch { Layout.fillWidth: true; text: qsTr("Analysis model and GF"); checked: ProfilePrefs.decoinfo; onToggled: { ProfilePrefs.decoinfo = checked; page.refreshCurrentProfile() } }
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
				onClicked: page.editRequested(page.currentItem.modelData)
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
