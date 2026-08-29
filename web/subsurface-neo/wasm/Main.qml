// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
	id: window
	objectName: "neoWindow"
	visible: true
	width: 1180
	height: 760
	minimumWidth: 360
	minimumHeight: 620
	title: qsTr("Subsurface Neo Web")
	color: "#06111f"
	property bool compact: width < 760
	property color accent: "#23d4e8"
	property color surface: "#0b1b2d"
	property color surfaceRaised: "#10243a"
	property color border: "#1b3c55"
	property color primaryText: "#f5f9fc"
	property color secondaryText: "#8fa7ba"
	property int activePage: 0 // 0 dashboard, 1 dive list, 2 dive detail, 3 planner
	property bool detailMode: activePage === 2 && diveLog.hasSelectedDive
	property int activeNavigationIndex: activePage === 0 ? 0 : activePage === 3 ? (compact ? 4 : 5) : 1
	property bool editingDive: false

	function openDive(sourceIndex) {
		diveLog.selectDive(sourceIndex)
		activePage = 2
	}

	function openNavigation(index) {
		if (index === 0 || index === 1) {
			if (detailMode)
				diveLog.clearSelectedDive()
			activePage = index
		} else if (index === 5 || (compact && index === 4)) {
			if (detailMode)
				diveLog.clearSelectedDive()
			activePage = 3
		}
	}

	component NeoButton: Button {
		id: control
		implicitHeight: 44
		font.pixelSize: 13
		font.weight: Font.DemiBold
		contentItem: Text {
			text: control.text
			color: control.enabled ? window.primaryText : "#566b7d"
			font: control.font
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
		background: Rectangle {
			color: control.down ? "#12344a" : window.surfaceRaised
			radius: 11
			border.width: 1
			border.color: control.enabled ? window.accent : window.border
		}
	}

	component NeoField: TextField {
		id: field
		implicitHeight: 44
		color: window.primaryText
		placeholderTextColor: window.secondaryText
		selectionColor: window.accent
		selectedTextColor: "#03101d"
		font.pixelSize: 12
		leftPadding: 14
		rightPadding: 14
		background: Rectangle {
			color: window.surfaceRaised
			radius: 11
			border.width: 1
			border.color: field.activeFocus ? window.accent : window.border
		}
	}

	component NeoTextArea: TextArea {
		id: area
		implicitHeight: 96
		color: window.primaryText
		placeholderTextColor: window.secondaryText
		selectionColor: window.accent
		selectedTextColor: "#03101d"
		font.pixelSize: 12
		wrapMode: TextEdit.Wrap
		padding: 14
		background: Rectangle {
			color: window.surfaceRaised
			radius: 11
			border.width: 1
			border.color: area.activeFocus ? window.accent : window.border
		}
	}

	component NeoCombo: ComboBox {
		id: combo
		implicitHeight: 44
		font.pixelSize: 12
		contentItem: Text {
			leftPadding: 14
			rightPadding: 30
			text: combo.displayText
			color: window.primaryText
			verticalAlignment: Text.AlignVCenter
			elide: Text.ElideRight
		}
		background: Rectangle {
			color: window.surfaceRaised
			radius: 11
			border.width: 1
			border.color: combo.activeFocus ? window.accent : window.border
		}
		delegate: ItemDelegate {
			width: combo.width
			height: 40
			contentItem: Text {
				text: modelData
				color: highlighted ? window.accent : window.primaryText
				font.pixelSize: 12
				verticalAlignment: Text.AlignVCenter
			}
			background: Rectangle { color: highlighted ? "#12344a" : window.surfaceRaised }
		}
		popup: Popup {
			y: combo.height - 1
			width: combo.width
			implicitHeight: Math.min(contentItem.implicitHeight + 2, 260)
			padding: 1
			contentItem: ListView {
				clip: true
				implicitHeight: contentHeight
				model: combo.popup.visible ? combo.delegateModel : null
				currentIndex: combo.highlightedIndex
				ScrollIndicator.vertical: ScrollIndicator { }
			}
			background: Rectangle {
				color: window.surfaceRaised
				radius: 8
				border.width: 1
				border.color: window.border
			}
		}
	}

	component StatusPill: Rectangle {
		property string label
		property bool available
		implicitWidth: statusText.implicitWidth + 24
		implicitHeight: 30
		radius: 15
		color: available ? "#103d3c" : "#17283a"
		border.width: 1
		border.color: available ? "#2bbf91" : window.border
		Text {
			id: statusText
			anchors.centerIn: parent
			text: (parent.available ? "●  " : "○  ") + parent.label
			color: parent.available ? "#5ee2ac" : window.secondaryText
			font.pixelSize: 11
		}
	}

	component ProfileChart: Rectangle {
		id: chart
		property var samples: []
		property bool showDepth: true
		property bool showTemperature: true
		property bool showNdl: true
		property bool showGf: true
		property bool showCeiling: true
		property bool showPressure: false
		property int selectedIndex: -1
		property real markerX: 0
		radius: 12
		color: "#071828"
		border.width: 1
		border.color: window.border

		function selectAt(positionX) {
			if (!samples || samples.length === 0)
				return
			const left = 42
			const right = 14
			const usable = Math.max(1, width - left - right)
			const lastTime = Math.max(1, samples[samples.length - 1].timeSeconds)
			const target = Math.max(0, Math.min(1, (positionX - left) / usable)) * lastTime
			let nearest = 0
			let distance = Math.abs(samples[0].timeSeconds - target)
			for (let i = 1; i < samples.length; ++i) {
				const candidate = Math.abs(samples[i].timeSeconds - target)
				if (candidate < distance) {
					distance = candidate
					nearest = i
				}
			}
			selectedIndex = nearest
			markerX = left + samples[nearest].timeSeconds / lastTime * usable
		}

		function sampleText(sample) {
			if (!sample)
				return ""
			const minutes = Math.floor(sample.timeSeconds / 60)
			const seconds = sample.timeSeconds % 60
			let lines = [minutes + ":" + (seconds < 10 ? "0" : "") + seconds,
				qsTr("Depth") + "  " + Number(sample.depthMeters).toFixed(1) + " m"]
			if (sample.hasTemperature)
				lines.push(qsTr("Water") + "  " + Number(sample.temperatureCelsius).toFixed(1) + " °C")
			if (sample.hasCalculatedNdl)
				lines.push(qsTr("Calculated NDL") + "  " + Math.round(sample.calculatedNdlMinutes) + " min")
			else if (sample.hasNdl)
				lines.push(qsTr("Recorded NDL") + "  " + Math.round(sample.ndlMinutes) + " min")
			if (sample.hasCalculatedTts)
				lines.push(qsTr("Calculated TTS") + "  " + Math.round(sample.calculatedTtsMinutes) + " min")
			else if (sample.hasTts)
				lines.push(qsTr("Recorded TTS") + "  " + Math.round(sample.ttsMinutes) + " min")
			if (sample.hasCalculatedCeiling)
				lines.push(qsTr("Calculated ceiling") + "  " + Number(sample.calculatedCeilingMeters).toFixed(1) + " m")
			if (sample.hasCurrentGf)
				lines.push(qsTr("GF") + "  " + Math.round(sample.currentGfPercent) + "%")
			if (sample.hasSurfaceGf)
				lines.push(qsTr("Surface GF") + "  " + Math.round(sample.surfaceGfPercent) + "%")
			if (sample.hasStopDepth && sample.stopDepthMeters > 0)
				lines.push(qsTr("Deco stop") + "  " + Number(sample.stopDepthMeters).toFixed(1) + " m")
			if (sample.hasPressure)
				lines.push(qsTr("Pressure") + "  " + Math.round(sample.pressureBar) + " bar")
			if (sample.hasSetpoint)
				lines.push(qsTr("Setpoint") + "  " + Number(sample.setpointBar).toFixed(2) + " bar")
			if (sample.hasCns)
				lines.push(qsTr("CNS") + "  " + Math.round(sample.cnsPercent) + "%")
			return lines.join("\n")
		}

		Canvas {
			id: chartCanvas
			anchors.fill: parent
			onPaint: {
				const ctx = getContext("2d")
				ctx.reset()
				const left = 42
				const right = 14
				const top = 16
				const bottom = 30
				const plotWidth = width - left - right
				const plotHeight = height - top - bottom
				ctx.strokeStyle = "#143149"
				ctx.lineWidth = 1
				ctx.fillStyle = window.secondaryText
				ctx.font = "10px sans-serif"
				for (let grid = 0; grid <= 4; ++grid) {
					const y = top + plotHeight * grid / 4
					ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(width - right, y); ctx.stroke()
				}
				if (!chart.samples || chart.samples.length === 0) {
					ctx.fillStyle = window.secondaryText
					ctx.fillText(qsTr("No recorded profile samples"), left + 10, top + 28)
					return
				}
				const lastTime = Math.max(1, chart.samples[chart.samples.length - 1].timeSeconds)
				let maxDepth = 1, minTemp = 1000, maxTemp = -1000, maxNdl = 1, maxPressure = 1, maxGf = 100
				for (let i = 0; i < chart.samples.length; ++i) {
					const sample = chart.samples[i]
					if (sample.hasDepth) maxDepth = Math.max(maxDepth, sample.depthMeters)
					if (sample.hasTemperature) { minTemp = Math.min(minTemp, sample.temperatureCelsius); maxTemp = Math.max(maxTemp, sample.temperatureCelsius) }
					if (sample.hasDisplayNdl) maxNdl = Math.max(maxNdl, sample.displayNdlMinutes)
					if (sample.hasPressure) maxPressure = Math.max(maxPressure, sample.pressureBar)
					if (sample.hasCurrentGf) maxGf = Math.max(maxGf, sample.currentGfPercent)
				}
				if (minTemp > maxTemp) { minTemp = 0; maxTemp = 1 }
				if (maxTemp - minTemp < 0.5) maxTemp = minTemp + 0.5
				function drawSeries(color, valueKey, presentKey, minimum, maximum, invert) {
					ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath()
					let started = false
					for (let i = 0; i < chart.samples.length; ++i) {
						const sample = chart.samples[i]
						if (!sample[presentKey]) continue
						const x = left + sample.timeSeconds / lastTime * plotWidth
						let ratio = (sample[valueKey] - minimum) / Math.max(0.0001, maximum - minimum)
						if (invert) ratio = 1 - ratio
						const y = top + ratio * plotHeight
						if (!started) { ctx.moveTo(x, y); started = true } else ctx.lineTo(x, y)
					}
					if (started) ctx.stroke()
				}
				if (chart.showDepth) drawSeries("#2f9df4", "depthMeters", "hasDepth", 0, maxDepth, false)
				if (chart.showTemperature) drawSeries("#18d5df", "temperatureCelsius", "hasTemperature", minTemp, maxTemp, true)
				if (chart.showNdl) drawSeries("#f0c51b", "displayNdlMinutes", "hasDisplayNdl", 0, maxNdl, true)
				if (chart.showGf) drawSeries("#d35be0", "currentGfPercent", "hasCurrentGf", 0, maxGf, true)
				if (chart.showCeiling) drawSeries("#ff7b54", "calculatedCeilingMeters", "hasCalculatedCeiling", 0, maxDepth, false)
				if (chart.showPressure) drawSeries("#87a6bd", "pressureBar", "hasPressure", 0, maxPressure, true)
				ctx.fillStyle = window.secondaryText
				ctx.fillText("0", left - 4, height - 10)
				ctx.fillText(Math.round(lastTime / 60) + " min", width - right - 34, height - 10)
				ctx.fillText(Math.ceil(maxDepth) + " m", 5, height - bottom)
				if (chart.selectedIndex >= 0 && chart.selectedIndex < chart.samples.length) {
					ctx.strokeStyle = "#8299ad"; ctx.lineWidth = 1; ctx.setLineDash([4, 4])
					ctx.beginPath(); ctx.moveTo(chart.markerX, top); ctx.lineTo(chart.markerX, height - bottom); ctx.stroke(); ctx.setLineDash([])
				}
			}
			onWidthChanged: requestPaint()
			onHeightChanged: requestPaint()
		}
		onSamplesChanged: chartCanvas.requestPaint()
		onShowDepthChanged: chartCanvas.requestPaint()
		onShowTemperatureChanged: chartCanvas.requestPaint()
		onShowNdlChanged: chartCanvas.requestPaint()
		onShowGfChanged: chartCanvas.requestPaint()
		onShowCeilingChanged: chartCanvas.requestPaint()
		onShowPressureChanged: chartCanvas.requestPaint()
		onSelectedIndexChanged: chartCanvas.requestPaint()

		MouseArea {
			anchors.fill: parent
			hoverEnabled: true
			onPositionChanged: function(mouse) { chart.selectAt(mouse.x) }
			onPressed: function(mouse) { chart.selectAt(mouse.x) }
		}

		Rectangle {
			visible: chart.selectedIndex >= 0 && chart.selectedIndex < chart.samples.length
			width: window.compact ? 154 : 178
			height: tooltipText.implicitHeight + 20
			x: chart.markerX > chart.width / 2 ? Math.max(8, chart.markerX - width - 12) : Math.min(chart.width - width - 8, chart.markerX + 12)
			y: 12
			z: 3
			radius: 9
			color: "#0b1d2d"
			border.width: 1
			border.color: "#31516b"
			Text {
				id: tooltipText
				anchors.fill: parent
				anchors.margins: 10
				text: chart.selectedIndex >= 0 ? chart.sampleText(chart.samples[chart.selectedIndex]) : ""
				color: window.primaryText
				font.pixelSize: window.compact ? 9 : 10
				lineHeight: 1.2
			}
		}
	}

	Rectangle {
		id: sidebar
		objectName: "sidebar"
		visible: !window.compact
		width: 220
		anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
		color: "#04101c"
		border.color: window.border

		ColumnLayout {
			anchors.fill: parent
			anchors.margins: 22
			spacing: 10
			RowLayout {
				Layout.bottomMargin: 24
				Rectangle { width: 38; height: 38; radius: 19; color: "#0b3345"; Text { anchors.centerIn: parent; text: "S"; color: window.accent; font.pixelSize: 26; font.italic: true; font.weight: Font.Bold } }
				Text { text: "SUBSURFACE"; color: window.primaryText; font.pixelSize: 13; font.letterSpacing: 3; font.weight: Font.DemiBold }
			}
			Repeater {
				model: [qsTr("Dashboard"), qsTr("Dives"), qsTr("Dive Sites"), qsTr("Statistics"), qsTr("Equipment"), qsTr("Planner"), qsTr("Settings")]
				delegate: Rectangle {
					required property string modelData
					required property int index
					Layout.fillWidth: true
					height: 45
					radius: 10
					color: index === window.activeNavigationIndex ? "#0c3043" : "transparent"
					Text {
						anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
						text: modelData
						color: index === window.activeNavigationIndex ? window.accent : window.secondaryText
						font.pixelSize: 13
					}
					MouseArea { anchors.fill: parent; cursorShape: index < 2 ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: window.openNavigation(index) }
				}
			}
			Item { Layout.fillHeight: true }
			StatusPill { label: qsTr("WebAssembly"); available: webCapabilities.webAssemblyRuntime }
			Text { text: qsTr("Milestone 14 development build"); color: window.secondaryText; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		}
	}

	Rectangle {
		id: mobileHeader
		objectName: "mobileHeader"
		visible: window.compact
		height: 62
		anchors { left: parent.left; right: parent.right; top: parent.top }
		color: "#04101c"
		border.color: window.border
		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: 18
			anchors.rightMargin: 18
			Text { text: "S"; color: window.accent; font.pixelSize: 28; font.italic: true; font.weight: Font.Bold }
			Text { text: "SUBSURFACE"; color: window.primaryText; font.pixelSize: 12; font.letterSpacing: 2; font.weight: Font.DemiBold }
			Item { Layout.fillWidth: true }
			Rectangle { width: 12; height: 12; radius: 6; color: webCapabilities.secureContext ? "#40d58a" : "#f1ae45" }
		}
	}

	Flickable {
		id: contentFlick
		objectName: "dashboardPage"
		visible: window.activePage === 0
		anchors {
			left: window.compact ? parent.left : sidebar.right
			right: parent.right
			top: window.compact ? mobileHeader.bottom : parent.top
			bottom: window.compact ? bottomNav.top : parent.bottom
		}
		contentWidth: width
		contentHeight: contentColumn.implicitHeight + 64
		clip: true
		ScrollBar.vertical: ScrollBar { }

		ColumnLayout {
			id: contentColumn
			x: window.compact ? 16 : 34
			y: window.compact ? 24 : 36
			width: parent.width - (window.compact ? 32 : 68)
			spacing: 18

			RowLayout {
				Layout.fillWidth: true
				ColumnLayout {
					spacing: 2
					Text { text: qsTr("Good evening"); color: window.primaryText; font.pixelSize: window.compact ? 25 : 32; font.weight: Font.DemiBold }
					Text { text: qsTr("Your diving workspace"); color: window.secondaryText; font.pixelSize: 13 }
				}
				Item { Layout.fillWidth: true }
				StatusPill { visible: !window.compact; label: webCapabilities.secureContext ? qsTr("Secure browser") : qsTr("HTTPS required"); available: webCapabilities.secureContext }
			}

			GridLayout {
				Layout.fillWidth: true
				columns: 3
				columnSpacing: 10
				Repeater {
					model: [{ label: qsTr("DIVES"), value: diveLog.diveCount.toString() }, { label: qsTr("DIVE TIME"), value: diveLog.totalTime }, { label: qsTr("MAX DEPTH"), value: diveLog.maxDepth }]
					delegate: Rectangle {
						required property var modelData
						required property int index
						Layout.fillWidth: true
						Layout.preferredHeight: window.compact ? 105 : 128
						radius: 14
						color: window.surface
						border.width: 1
						border.color: window.border
						Column {
							anchors { left: parent.left; leftMargin: window.compact ? 12 : 20; verticalCenter: parent.verticalCenter }
							spacing: 4
							Text { text: modelData.value; color: window.primaryText; font.pixelSize: window.compact ? 24 : 34; font.weight: Font.DemiBold }
							Text { text: modelData.label; color: window.accent; font.pixelSize: window.compact ? 8 : 10; font.letterSpacing: 1 }
						}
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true
				implicitHeight: capabilityContent.implicitHeight + 40
				radius: 16
				color: window.surface
				border.width: 1
				border.color: window.border
				ColumnLayout {
					id: capabilityContent
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
					spacing: 12
					Text { text: qsTr("Browser readiness"); color: window.primaryText; font.pixelSize: 19; font.weight: Font.DemiBold }
					Text { text: qsTr("Neo detects browser capabilities before offering hardware actions. Unsupported mobile browsers will use local files or cloud sync instead of showing a broken connect button."); color: window.secondaryText; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					Flow {
						Layout.fillWidth: true
						spacing: 8
						StatusPill { label: qsTr("Secure context"); available: webCapabilities.secureContext }
						StatusPill { label: qsTr("Web Bluetooth"); available: webCapabilities.webBluetoothAvailable }
						StatusPill { label: qsTr("Web Serial"); available: webCapabilities.webSerialAvailable }
						StatusPill { label: qsTr("Local files"); available: true }
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true
				implicitHeight: importContent.implicitHeight + 40
				radius: 16
				color: window.surface
				border.width: 1
				border.color: window.border
				ColumnLayout {
					id: importContent
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
					spacing: 10
					Text { text: qsTr("Start with your real dive log"); color: window.primaryText; font.pixelSize: 19; font.weight: Font.DemiBold }
					Text { text: qsTr("Open a native Subsurface XML log to populate this dashboard. The file is read by shared C++ core code and is never uploaded; you can keep an origin-isolated browser workspace on this device."); color: window.secondaryText; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					NeoButton { text: qsTr("Choose local dive log"); Layout.preferredWidth: window.compact ? importContent.width : 230; onClicked: diveLog.chooseLocalFile() }
					GridLayout {
						visible: diveLog.browserStorageAvailable && (diveLog.loaded || diveLog.durableSessionStored)
						Layout.fillWidth: true
						columns: window.compact ? 1 : 3
						NeoButton { visible: diveLog.loaded; Layout.fillWidth: true; text: qsTr("Save browser workspace"); onClicked: diveLog.saveBrowserSession() }
						NeoButton { visible: diveLog.durableSessionStored; Layout.fillWidth: true; text: qsTr("Restore saved workspace"); onClicked: diveLog.restoreBrowserSession() }
						NeoButton { visible: diveLog.durableSessionStored; Layout.fillWidth: true; text: qsTr("Forget saved workspace"); onClicked: diveLog.clearBrowserSession() }
					}
					Text { visible: diveLog.durableSessionStatus.length > 0; text: diveLog.durableSessionStatus; color: diveLog.browserStorageAvailable ? window.secondaryText : "#f1ae45"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					GridLayout {
						visible: diveLog.loaded
						Layout.fillWidth: true
						columns: window.compact ? 1 : 2
						NeoButton { Layout.fillWidth: true; text: qsTr("Download browser-session XML backup"); onClicked: diveLog.exportNativeXmlBackup() }
						NeoButton { Layout.fillWidth: true; text: qsTr("Export dive list CSV"); onClicked: diveLog.exportDiveListCsv() }
					}
					Text { visible: diveLog.fileStatus.length > 0; text: diveLog.fileStatus; color: diveLog.error ? "#ff8f8f" : window.accent; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
			}

			Text { text: qsTr("Recent dives"); color: window.primaryText; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.topMargin: 4 }
			Rectangle {
				visible: !diveLog.loaded || diveLog.diveCount === 0
				Layout.fillWidth: true
				implicitHeight: 110
				radius: 16
				color: window.surface
				border.width: 1
				border.color: window.border
				Column {
					anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 16 }
					spacing: 6
					Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("No dives loaded"); color: window.primaryText; font.pixelSize: 15; font.weight: Font.DemiBold }
					Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: diveLog.loaded ? qsTr("This log does not contain any valid dives.") : qsTr("Open a native Subsurface XML log to see your recent dives."); color: window.secondaryText; font.pixelSize: 11 }
				}
			}

			Repeater {
				model: diveLog.recentDives
				delegate: Rectangle {
					required property var modelData
					required property int index
					Layout.fillWidth: true
					Layout.preferredHeight: window.compact ? 168 : 122
					radius: 16
					color: window.surface
					border.width: 1
					border.color: window.border
					Text {
						anchors { right: parent.right; bottom: parent.bottom; margins: 14 }
						text: qsTr("View profile →"); color: window.accent; font.pixelSize: 10
					}
					MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.openDive(modelData.sourceIndex) }

					ColumnLayout {
						anchors.fill: parent
						anchors.margins: window.compact ? 16 : 20
						spacing: 10
						RowLayout {
							Layout.fillWidth: true
							Text { text: modelData.number > 0 ? "#" + modelData.number : qsTr("Dive"); color: window.accent; font.pixelSize: 17; font.weight: Font.DemiBold }
							ColumnLayout {
								Layout.fillWidth: true
								spacing: 1
								Text { text: modelData.location; color: window.primaryText; font.pixelSize: window.compact ? 15 : 17; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
								Text { text: modelData.date; color: window.secondaryText; font.pixelSize: 10 }
							}
							StatusPill { label: modelData.mode; available: true }
						}
						GridLayout {
							Layout.fillWidth: true
							columns: window.compact ? 3 : 6
							columnSpacing: window.compact ? 8 : 18
							rowSpacing: 8
							Repeater {
								model: [
									{ label: qsTr("DEPTH"), value: modelData.depth },
									{ label: qsTr("DURATION"), value: modelData.duration },
									{ label: qsTr("WATER"), value: modelData.temperature },
									{ label: qsTr("GAS"), value: modelData.gas },
									{ label: qsTr("GEAR"), value: modelData.gear.length > 0 ? modelData.gear : "—" },
									{ label: qsTr("BUDDY"), value: modelData.buddy.length > 0 ? modelData.buddy : "—" }
								]
								delegate: ColumnLayout {
									required property var modelData
									spacing: 1
									Text { text: modelData.value; color: window.primaryText; font.pixelSize: window.compact ? 10 : 12; elide: Text.ElideRight; Layout.preferredWidth: window.compact ? 90 : 118 }
									Text { text: modelData.label; color: window.secondaryText; font.pixelSize: 8; font.letterSpacing: 0.7 }
								}
							}
						}
					}
				}
			}
		}
	}

	Flickable {
		id: diveListFlick
		objectName: "diveListPage"
		visible: window.activePage === 1
		anchors {
			left: window.compact ? parent.left : sidebar.right
			right: parent.right
			top: window.compact ? mobileHeader.bottom : parent.top
			bottom: window.compact ? bottomNav.top : parent.bottom
		}
		contentWidth: width
		contentHeight: diveListColumn.implicitHeight + 64
		clip: true
		ScrollBar.vertical: ScrollBar { }

		ColumnLayout {
			id: diveListColumn
			x: window.compact ? 16 : 34
			y: window.compact ? 22 : 34
			width: parent.width - (window.compact ? 32 : 68)
			spacing: 16

			RowLayout {
				Layout.fillWidth: true
				ColumnLayout {
					spacing: 2
					Text { text: qsTr("Dives"); color: window.primaryText; font.pixelSize: window.compact ? 26 : 32; font.weight: Font.DemiBold }
					Text { text: qsTr("%1 of %2 dives").arg(diveLog.filteredDives.length).arg(diveLog.diveCount); color: window.secondaryText; font.pixelSize: 12 }
				}
				Item { Layout.fillWidth: true }
				NeoButton { visible: !window.compact; text: qsTr("Open another log"); Layout.preferredWidth: 170; onClicked: diveLog.chooseLocalFile() }
			}

			GridLayout {
				Layout.fillWidth: true
				columns: window.compact ? 1 : 3
				columnSpacing: 10
				rowSpacing: 10
				NeoField {
					Layout.fillWidth: true
					placeholderText: qsTr("Search site, buddy, gas, gear, notes…")
					text: diveLog.searchText
					onTextEdited: diveLog.searchText = text
				}
				NeoCombo {
					id: yearCombo
					Layout.fillWidth: true
					model: [qsTr("All years")].concat(diveLog.availableYears)
					onActivated: diveLog.yearFilter = currentIndex > 0 ? currentText : ""
				}
				NeoCombo {
					id: modeCombo
					Layout.fillWidth: true
					model: [qsTr("All modes")].concat(diveLog.availableModes)
					onActivated: diveLog.modeFilter = currentIndex > 0 ? currentText : ""
				}
			}

			RowLayout {
				visible: diveLog.searchText.length > 0 || diveLog.yearFilter.length > 0 || diveLog.modeFilter.length > 0
				Layout.fillWidth: true
				Text { text: qsTr("Filters active"); color: window.accent; font.pixelSize: 11 }
				Item { Layout.fillWidth: true }
				NeoButton {
					text: qsTr("Clear filters")
					Layout.preferredWidth: 130
					onClicked: {
						diveLog.searchText = ""
						diveLog.yearFilter = ""
						diveLog.modeFilter = ""
						yearCombo.currentIndex = 0
						modeCombo.currentIndex = 0
					}
				}
			}

			Rectangle {
				visible: diveLog.filteredDives.length === 0
				Layout.fillWidth: true
				Layout.preferredHeight: 120
				radius: 16; color: window.surface; border.width: 1; border.color: window.border
				Column {
					anchors.centerIn: parent; spacing: 6
					Text { anchors.horizontalCenter: parent.horizontalCenter; text: diveLog.loaded ? qsTr("No dives match these filters") : qsTr("No dive log loaded"); color: window.primaryText; font.pixelSize: 15; font.weight: Font.DemiBold }
					Text { anchors.horizontalCenter: parent.horizontalCenter; text: diveLog.loaded ? qsTr("Change or clear the search filters.") : qsTr("Open a local Subsurface XML log from Dashboard."); color: window.secondaryText; font.pixelSize: 11 }
				}
			}

			Repeater {
				model: diveLog.filteredDives
				delegate: Rectangle {
					required property var modelData
					required property int index
					Layout.fillWidth: true
					Layout.preferredHeight: window.compact ? 172 : 126
					radius: 16; color: window.surface; border.width: 1; border.color: listHover.hovered ? window.accent : window.border
					HoverHandler { id: listHover }
					MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.openDive(modelData.sourceIndex) }
					ColumnLayout {
						anchors.fill: parent; anchors.margins: window.compact ? 16 : 20; spacing: 11
						RowLayout {
							Layout.fillWidth: true
							Text { text: modelData.number > 0 ? "#" + modelData.number : qsTr("Dive"); color: window.accent; font.pixelSize: 18; font.weight: Font.DemiBold }
							ColumnLayout {
								Layout.fillWidth: true; spacing: 1
								Text { text: modelData.location; color: window.primaryText; font.pixelSize: window.compact ? 16 : 18; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
								Text { text: modelData.date + (modelData.time ? "  ·  " + modelData.time : ""); color: window.secondaryText; font.pixelSize: 10 }
							}
							StatusPill { label: modelData.mode || qsTr("Unknown"); available: true }
						}
						GridLayout {
							Layout.fillWidth: true; columns: window.compact ? 3 : 6; columnSpacing: window.compact ? 8 : 18; rowSpacing: 8
							Repeater {
								model: [{label: qsTr("DEPTH"), value: modelData.depth}, {label: qsTr("DURATION"), value: modelData.duration}, {label: qsTr("WATER"), value: modelData.temperature}, {label: qsTr("GAS"), value: modelData.gas || "—"}, {label: qsTr("GEAR"), value: modelData.gear || "—"}, {label: qsTr("BUDDY"), value: modelData.buddy || "—"}]
								delegate: ColumnLayout {
									required property var modelData; spacing: 1
									Text { text: modelData.value; color: window.primaryText; font.pixelSize: window.compact ? 10 : 12; elide: Text.ElideRight; Layout.preferredWidth: window.compact ? 92 : 120 }
									Text { text: modelData.label; color: window.secondaryText; font.pixelSize: 8; font.letterSpacing: 0.7 }
								}
							}
						}
					}
				}
			}
		}
	}

	Flickable {
		id: detailFlick
		objectName: "diveDetailPage"
		visible: window.detailMode
		anchors {
			left: window.compact ? parent.left : sidebar.right
			right: parent.right
			top: window.compact ? mobileHeader.bottom : parent.top
			bottom: window.compact ? bottomNav.top : parent.bottom
		}
		contentWidth: width
		contentHeight: detailColumn.implicitHeight + 64
		clip: true
		ScrollBar.vertical: ScrollBar { }

		ColumnLayout {
			id: detailColumn
			x: window.compact ? 16 : 34
			y: window.compact ? 20 : 30
			width: parent.width - (window.compact ? 32 : 68)
			spacing: 16

			NeoButton { text: qsTr("← Back to dives"); Layout.preferredWidth: 170; onClicked: { diveLog.clearSelectedDive(); window.activePage = 1 } }
			RowLayout {
				Layout.fillWidth: true
				Text { text: diveLog.selectedDive.number > 0 ? "#" + diveLog.selectedDive.number : qsTr("Dive"); color: window.accent; font.pixelSize: 25; font.weight: Font.DemiBold }
				ColumnLayout {
					Layout.fillWidth: true; spacing: 2
					Text { text: diveLog.selectedDive.location || ""; color: window.primaryText; font.pixelSize: window.compact ? 22 : 29; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
					Text { text: (diveLog.selectedDive.date || "") + (diveLog.selectedDive.time ? "  ·  " + diveLog.selectedDive.time : ""); color: window.secondaryText; font.pixelSize: 11 }
				}
				StatusPill { label: diveLog.selectedDive.mode || "OC"; available: true }
			}
			RowLayout {
				Layout.fillWidth: true
				spacing: 8
				NeoButton {
					text: window.editingDive ? qsTr("Cancel editing") : qsTr("Edit dive")
					onClicked: {
						if (!window.editingDive) {
							editLocation.text = diveLog.selectedDive.location || ""
							editBuddy.text = diveLog.selectedDive.buddy || ""
							editMode.text = diveLog.selectedDive.mode || "OC"
							editGas.text = diveLog.selectedDive.gas || ""
							editGear.text = diveLog.selectedDive.gear || ""
							editNotes.text = diveLog.selectedDive.notes || ""
						}
						window.editingDive = !window.editingDive
					}
				}
				NeoButton { text: qsTr("Export JSON"); onClicked: diveLog.exportSelectedDiveJson() }
				NeoButton { visible: !window.compact; text: qsTr("Export dive list CSV"); onClicked: diveLog.exportDiveListCsv() }
				Item { Layout.fillWidth: true }
				Text {
					visible: diveLog.selectedDiveDirty
					text: diveLog.durableSessionStored ?
						qsTr("Edits saved in this browser; download a backup to replace the source file") :
						qsTr("Browser edits not written to the source XML")
					color: diveLog.durableSessionStored ? window.accent : "#f1ae45"
					font.pixelSize: 10
				}
			}

			Rectangle {
				visible: window.editingDive
				Layout.fillWidth: true
				implicitHeight: editLayout.implicitHeight + 32
				radius: 16
				color: window.surface
				border.width: 1
				border.color: window.accent
				ColumnLayout {
					id: editLayout
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
					spacing: 10
					Text { text: qsTr("Edit browser-session dive details"); color: window.primaryText; font.pixelSize: 18; font.weight: Font.DemiBold }
					GridLayout {
						Layout.fillWidth: true
						columns: window.compact ? 1 : 2
						columnSpacing: 10
						rowSpacing: 10
						NeoField { id: editLocation; Layout.fillWidth: true; placeholderText: qsTr("Dive site") }
						NeoField { id: editBuddy; Layout.fillWidth: true; placeholderText: qsTr("Buddy") }
						NeoField { id: editMode; Layout.fillWidth: true; placeholderText: qsTr("Mode") }
						NeoField { id: editGas; Layout.fillWidth: true; placeholderText: qsTr("Gas") }
						NeoField { id: editGear; Layout.fillWidth: true; placeholderText: qsTr("Gear") }
					}
					NeoTextArea { id: editNotes; Layout.fillWidth: true; placeholderText: qsTr("Notes") }
					NeoButton {
						text: qsTr("Apply browser edit")
						onClicked: {
							if (diveLog.updateSelectedDive(editLocation.text, editBuddy.text, editNotes.text, editMode.text, editGas.text, editGear.text))
								window.editingDive = false
						}
					}
				}
			}

			GridLayout {
				Layout.fillWidth: true; columns: 3; columnSpacing: 10
				Repeater {
					model: [{label: qsTr("MAX DEPTH"), value: diveLog.selectedDive.depth || "—"}, {label: qsTr("DIVE TIME"), value: diveLog.selectedDive.duration || "—"}, {label: qsTr("WATER TEMP"), value: diveLog.selectedDive.temperature || "—"}]
					delegate: Rectangle {
						required property var modelData
						Layout.fillWidth: true; Layout.preferredHeight: window.compact ? 88 : 104
						radius: 13; color: window.surface; border.width: 1; border.color: window.border
						Column {
							anchors.centerIn: parent; spacing: 4
							Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: window.primaryText; font.pixelSize: window.compact ? 17 : 25; font.weight: Font.DemiBold }
							Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: window.accent; font.pixelSize: window.compact ? 7 : 9; font.letterSpacing: 0.8 }
						}
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true
				implicitHeight: profileContent.implicitHeight + 32
				radius: 16; color: window.surface; border.width: 1; border.color: window.border
				ColumnLayout {
					id: profileContent
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
					spacing: 10
					RowLayout {
						Layout.fillWidth: true
						Text { text: qsTr("Recorded profile"); color: window.primaryText; font.pixelSize: 18; font.weight: Font.DemiBold }
						Item { Layout.fillWidth: true }
						Text { text: qsTr("%1 samples").arg(diveLog.selectedDive.sampleCount || 0); color: window.secondaryText; font.pixelSize: 10 }
					}
					Text { text: qsTr("Recorded samples enriched by Subsurface's native profile and decompression pipeline."); color: window.secondaryText; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					RowLayout {
						Layout.fillWidth: true; spacing: 7
						NeoButton { Layout.fillWidth: true; text: qsTr("Depth"); onClicked: profileChart.showDepth = !profileChart.showDepth; opacity: profileChart.showDepth ? 1 : 0.45 }
						NeoButton { Layout.fillWidth: true; text: qsTr("Temp"); onClicked: profileChart.showTemperature = !profileChart.showTemperature; opacity: profileChart.showTemperature ? 1 : 0.45 }
						NeoButton { Layout.fillWidth: true; text: qsTr("NDL"); onClicked: profileChart.showNdl = !profileChart.showNdl; opacity: profileChart.showNdl ? 1 : 0.45 }
						NeoButton { Layout.fillWidth: true; text: qsTr("GF"); onClicked: profileChart.showGf = !profileChart.showGf; opacity: profileChart.showGf ? 1 : 0.45 }
						NeoButton { Layout.fillWidth: true; text: qsTr("Ceiling"); onClicked: profileChart.showCeiling = !profileChart.showCeiling; opacity: profileChart.showCeiling ? 1 : 0.45 }
						NeoButton { Layout.fillWidth: true; text: qsTr("Pressure"); onClicked: profileChart.showPressure = !profileChart.showPressure; opacity: profileChart.showPressure ? 1 : 0.45 }
					}
					ProfileChart { id: profileChart; Layout.fillWidth: true; Layout.preferredHeight: window.compact ? 330 : 350; samples: diveLog.profileSamples }
				}
			}

			GridLayout {
				visible: !window.compact; Layout.fillWidth: true; columns: 5; columnSpacing: 10
				Repeater {
					model: [{label: qsTr("GAS"), value: diveLog.selectedDive.gas || "—"}, {label: qsTr("GEAR"), value: diveLog.selectedDive.gear || "—"}, {label: qsTr("MODE"), value: diveLog.selectedDive.mode || "—"}, {label: qsTr("TYPE"), value: qsTr("Not recorded")}, {label: qsTr("BUDDY"), value: diveLog.selectedDive.buddy || "—"}]
					delegate: Rectangle {
						required property var modelData
						Layout.fillWidth: true; Layout.preferredHeight: 92; radius: 13; color: window.surface; border.width: 1; border.color: window.border
						Column {
							anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 14 }
							spacing: 4
							Text { text: modelData.label; color: window.accent; font.pixelSize: 8; font.letterSpacing: 0.8 }
							Text { width: parent.width; text: modelData.value; color: window.primaryText; font.pixelSize: 14; elide: Text.ElideRight }
						}
					}
				}
			}
			GridLayout {
				visible: window.compact; Layout.fillWidth: true; columns: 2; columnSpacing: 10
				Repeater {
					model: [{label: qsTr("GAS"), value: diveLog.selectedDive.gas || "—"}, {label: qsTr("GEAR"), value: diveLog.selectedDive.gear || "—"}]
					delegate: Rectangle {
						required property var modelData
						Layout.fillWidth: true; Layout.preferredHeight: 82; radius: 13; color: window.surface; border.width: 1; border.color: window.border
						Column {
							anchors.centerIn: parent; spacing: 3
							Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: window.accent; font.pixelSize: 8 }
							Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: window.primaryText; font.pixelSize: 13 }
						}
					}
				}
			}
			GridLayout {
				visible: window.compact; Layout.fillWidth: true; columns: 3; columnSpacing: 8
				Repeater {
					model: [{label: qsTr("MODE"), value: diveLog.selectedDive.mode || "—"}, {label: qsTr("TYPE"), value: qsTr("Not recorded")}, {label: qsTr("BUDDY"), value: diveLog.selectedDive.buddy || "—"}]
					delegate: Rectangle {
						required property var modelData
						Layout.fillWidth: true; Layout.preferredHeight: 82; radius: 13; color: window.surface; border.width: 1; border.color: window.border
						Column {
							anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
							spacing: 3
							Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: window.accent; font.pixelSize: 7 }
							Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: modelData.value; color: window.primaryText; font.pixelSize: 10; elide: Text.ElideRight }
						}
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true; implicitHeight: notesColumn.implicitHeight + 30; radius: 13; color: window.surface; border.width: 1; border.color: window.border
				ColumnLayout {
					id: notesColumn
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 15 }
					spacing: 5
					Text { text: qsTr("NOTES"); color: window.accent; font.pixelSize: 8; font.letterSpacing: 0.8 }
					Text { text: diveLog.selectedDive.notes || qsTr("No notes recorded for this dive."); color: window.primaryText; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				}
			}
		}
	}

	Flickable {
		id: plannerFlick
		objectName: "plannerPage"
		visible: window.activePage === 3
		anchors {
			left: window.compact ? parent.left : sidebar.right
			right: parent.right
			top: window.compact ? mobileHeader.bottom : parent.top
			bottom: window.compact ? bottomNav.top : parent.bottom
		}
		contentWidth: width
		contentHeight: plannerColumn.implicitHeight + 64
		clip: true
		ScrollBar.vertical: ScrollBar { }
		ColumnLayout {
			id: plannerColumn
			x: window.compact ? 16 : 34
			y: window.compact ? 22 : 34
			width: parent.width - (window.compact ? 32 : 68)
			spacing: 16
			Text { text: qsTr("Planner workspace"); color: window.primaryText; font.pixelSize: window.compact ? 26 : 32; font.weight: Font.DemiBold }
			Text { text: qsTr("Build a portable draft and then validate decompression, gas use, and contingencies in Subsurface's native planner."); color: window.secondaryText; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Rectangle {
				Layout.fillWidth: true
				implicitHeight: plannerForm.implicitHeight + 34
				radius: 16; color: window.surface; border.width: 1; border.color: window.border
				ColumnLayout {
					id: plannerForm
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 17 }
					spacing: 10
					Text { text: qsTr("Dive draft"); color: window.primaryText; font.pixelSize: 19; font.weight: Font.DemiBold }
					GridLayout {
						Layout.fillWidth: true; columns: window.compact ? 1 : 4; columnSpacing: 10; rowSpacing: 10
						NeoField { Layout.fillWidth: true; text: webPlanner.depthMeters.toFixed(1); placeholderText: qsTr("Depth (m)"); validator: DoubleValidator { bottom: 1; top: 150 } onEditingFinished: webPlanner.depthMeters = Number(text) }
						NeoField { Layout.fillWidth: true; text: webPlanner.bottomTimeMinutes.toString(); placeholderText: qsTr("Bottom time (min)"); validator: IntValidator { bottom: 1; top: 600 } onEditingFinished: webPlanner.bottomTimeMinutes = Number(text) }
						NeoField { Layout.fillWidth: true; text: webPlanner.ascentRate.toFixed(1); placeholderText: qsTr("Ascent rate (m/min)"); validator: DoubleValidator { bottom: 1; top: 30 } onEditingFinished: webPlanner.ascentRate = Number(text) }
						NeoField { Layout.fillWidth: true; text: webPlanner.gas; placeholderText: qsTr("Gas"); onEditingFinished: webPlanner.gas = text }
						NeoField { Layout.fillWidth: true; text: webPlanner.sacRate.toFixed(1); placeholderText: qsTr("SAC (L/min)"); validator: DoubleValidator { bottom: 5; top: 100 } onEditingFinished: webPlanner.sacRate = Number(text) }
						NeoField { Layout.fillWidth: true; text: webPlanner.cylinderVolume.toFixed(1); placeholderText: qsTr("Cylinder (L)"); validator: DoubleValidator { bottom: 1; top: 40 } onEditingFinished: webPlanner.cylinderVolume = Number(text) }
						NeoField { Layout.fillWidth: true; text: webPlanner.startPressure.toString(); placeholderText: qsTr("Start pressure (bar)"); validator: IntValidator { bottom: 50; top: 400 } onEditingFinished: webPlanner.startPressure = Number(text) }
						NeoField { Layout.fillWidth: true; text: webPlanner.reservePressure.toString(); placeholderText: qsTr("Reserve (bar)"); validator: IntValidator { bottom: 0; top: 399 } onEditingFinished: webPlanner.reservePressure = Number(text) }
					}
					NeoButton { text: webPlanner.safetyStop ? qsTr("Safety stop: 3 min at 5 m") : qsTr("Safety stop: Off"); onClicked: webPlanner.safetyStop = !webPlanner.safetyStop }
					Text { text: webPlanner.summary; color: webPlanner.valid ? window.accent : "#ff8f8f"; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					Text { visible: webPlanner.gasSummary.length > 0; text: webPlanner.gasSummary; color: webPlanner.gasAdequate ? window.accent : "#ff8f8f"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					Text { text: webPlanner.warning; color: "#f1ae45"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					Repeater {
						model: webPlanner.waypoints
						delegate: RowLayout {
							required property var modelData
							Layout.fillWidth: true
							Text { text: Number(modelData.timeMinutes).toFixed(1) + qsTr(" min"); color: window.accent; font.pixelSize: 11; Layout.preferredWidth: 72 }
							Text { text: Number(modelData.depthMeters).toFixed(1) + qsTr(" m"); color: window.primaryText; font.pixelSize: 12; Layout.preferredWidth: 72 }
							Text { text: modelData.label; color: window.secondaryText; font.pixelSize: 11 }
						}
					}
					NeoButton { text: qsTr("Reset draft"); onClicked: webPlanner.reset() }
				}
			}
			Rectangle {
				Layout.fillWidth: true
				implicitHeight: syncForm.implicitHeight + 34
				radius: 16; color: window.surface; border.width: 1; border.color: window.border
				ColumnLayout {
					id: syncForm
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 17 }
					spacing: 10
					Text { text: qsTr("Cloud manifest safety preview"); color: window.primaryText; font.pixelSize: 19; font.weight: Font.DemiBold }
					Text { text: qsTr("Exercise the same revision/checksum decisions used before OAuth provider transfer. No file is uploaded from this preview."); color: window.secondaryText; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					GridLayout {
						Layout.fillWidth: true; columns: window.compact ? 2 : 4; columnSpacing: 10; rowSpacing: 10
						NeoField { id: localRevision; Layout.fillWidth: true; text: "2"; placeholderText: qsTr("Local revision"); validator: IntValidator { bottom: 0 } }
						NeoField { id: localChecksum; Layout.fillWidth: true; text: "local-a"; placeholderText: qsTr("Local checksum") }
						NeoField { id: remoteRevision; Layout.fillWidth: true; text: "2"; placeholderText: qsTr("Remote revision"); validator: IntValidator { bottom: 0 } }
						NeoField { id: remoteChecksum; Layout.fillWidth: true; text: "remote-b"; placeholderText: qsTr("Remote checksum") }
					}
					NeoButton { text: qsTr("Compare manifests"); onClicked: webSync.evaluate(Number(localRevision.text), localChecksum.text, Number(remoteRevision.text), remoteChecksum.text) }
					Text { text: webSync.status; color: webSync.conflict ? "#f1ae45" : window.accent; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					RowLayout {
						visible: webSync.conflict
						NeoButton { text: qsTr("Keep local"); onClicked: webSync.keepLocal() }
						NeoButton { text: qsTr("Keep remote"); onClicked: webSync.keepRemote() }
					}
				}
			}
		}
	}

	Rectangle {
		id: bottomNav
		objectName: "bottomNav"
		visible: window.compact
		height: 66
		anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
		color: "#04101c"
		border.color: window.border
		RowLayout {
			anchors.fill: parent
			Repeater {
				model: [qsTr("Home"), qsTr("Dives"), qsTr("Sites"), qsTr("Stats"), qsTr("More")]
				delegate: Item {
					required property string modelData
					required property int index
					Layout.fillWidth: true
					Layout.fillHeight: true
					ColumnLayout {
						anchors.centerIn: parent
						spacing: 2
						Text { Layout.alignment: Qt.AlignHCenter; text: index === window.activeNavigationIndex ? "●" : "○"; color: index === window.activeNavigationIndex ? window.accent : window.secondaryText; font.pixelSize: 14 }
						Text { Layout.alignment: Qt.AlignHCenter; text: modelData; color: index === window.activeNavigationIndex ? window.accent : window.secondaryText; font.pixelSize: 9 }
					}
					MouseArea { anchors.fill: parent; cursorShape: index < 2 ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: window.openNavigation(index) }
				}
			}
		}
	}
}
