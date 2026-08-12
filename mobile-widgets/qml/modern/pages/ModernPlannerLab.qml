// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import org.kde.kirigami as Kirigami
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Planner & decompression lab")
	background: Rectangle { color: tokens.background }
	signal openGasTools()
	signal openPlannerSettings()
	Modern.DesignTokens { id: tokens }
	property string depthUnit: Backend.length === Enums.METERS ? qsTr("m") : qsTr("ft")
	property string pressureUnit: Backend.pressure === Enums.BAR ? qsTr("bar") : qsTr("psi")
	property string planNotes: ""
	property var profileData: []
	property var schedule: []
	property bool exceedsNDL: false
	property var cylinderTypes: manager.cylinderListInit
	property var gasNames: []
	ListModel { id: cylinders }
	ListModel { id: segments }
	Settings { id: plannerStorage; category: "subsurface-neo/planner"; property var presets: [] }
	function modelData(model) {
		var values = []
		for (var i = 0; i < model.count; ++i)
			values.push(model.get(i))
		return values
	}
	function savePreset(name) {
		if (name.trim().length === 0)
			return
		var saved = plannerStorage.presets || []
		var preset = { "name": name.trim(), "cylinders": modelData(cylinders), "segments": modelData(segments), "diveMode": diveMode.currentIndex, "waterType": waterType.currentIndex }
		for (var i = 0; i < saved.length; ++i) {
			if (saved[i].name === preset.name) { saved[i] = preset; plannerStorage.presets = saved; return }
		}
		saved.push(preset)
		plannerStorage.presets = saved
	}
	function loadPreset(index) {
		var preset = plannerStorage.presets[index]
		if (!preset)
			return
		cylinders.clear(); segments.clear()
		for (var i = 0; i < preset.cylinders.length; ++i) cylinders.append(preset.cylinders[i])
		for (var j = 0; j < preset.segments.length; ++j) segments.append(preset.segments[j])
		updateGasNames()
		diveMode.currentIndex = preset.diveMode
		waterType.currentIndex = preset.waterType
		generatePlan()
	}

	function updateGasNames() {
		var names = []
		for (var i = 0; i < cylinders.count; ++i)
			names.push(qsTr("Gas %1").arg(i + 1))
		gasNames = names
	}
	function addCylinder() {
		cylinders.append({ "type": PrefEquipment.default_cylinder || "AL80", "mix": "21/0",
			"pressure": Backend.pressure === Enums.BAR ? 200 : 3000, "use": 0 })
		updateGasNames()
		generatePlan()
	}
	function addSegment() {
		var last = segments.get(segments.count - 1)
		segments.append({ "depth": last ? last.depth : (Backend.length === Enums.METERS ? 18 : 60),
			"duration": 10, "gas": last ? last.gas : 0, "setpoint": last ? last.setpoint : Backend.default_setpoint,
			"divemode": last ? last.divemode : 0 })
		generatePlan()
	}
	function generatePlan(savePlan) {
		if (cylinders.count === 0 || segments.count === 0)
			return
		var cylinderData = []
		for (var i = 0; i < cylinders.count; ++i) {
			var cylinder = cylinders.get(i)
			cylinderData.push({ "type": cylinder.type, "mix": cylinder.mix, "pressure": cylinder.pressure,
				"use": diveMode.currentIndex === 1 ? cylinder.use : 0 })
		}
		var segmentData = []
		for (var j = 0; j < segments.count; ++j) {
			var segment = segments.get(j)
			segmentData.push({ "depth": segment.depth, "duration": segment.duration, "gas": segment.gas,
				"setpoint": segment.setpoint, "divemode": segment.divemode })
		}
		var salinity = waterType.currentIndex === 0 ? 10300 : waterType.currentIndex === 1 ? 10000 : 10200
		var result = Backend.divePlannerPointsModel.calculatePlan(cylinderData, segmentData,
			Qt.formatDate(new Date(), "yyyy-MM-dd"), Qt.formatTime(new Date(), "hh:mm:ss"),
			diveMode.currentIndex, salinity, savePlan === true)
		planNotes = result.notes || ""
		profileData = result.profile || []
		schedule = result.schedule || []
		exceedsNDL = result.exceedsNDL === true
		if (savePlan === true && result.newDiveId !== undefined && result.newDiveId !== -1) {
			manager.selectDive(result.newDiveId)
			showPage(diveList)
		}
	}
	function finalSampleValue(name, fallback) {
		return profileData.length > 0 && profileData[profileData.length - 1][name] !== undefined ? profileData[profileData.length - 1][name] : fallback
	}
	function formatDuration(seconds) {
		if (seconds === undefined || seconds < 0)
			return "—"
		return Math.floor(seconds / 60) + qsTr(" min") + (seconds % 60 ? " " + (seconds % 60) + qsTr(" s") : "")
	}
	function decoSlate() {
		var lines = [qsTr("SUBSURFACE NEO DIVE PLAN"), qsTr("Model: Buhlmann GF %1/%2").arg(PrefTechnicalDetails.gflow).arg(PrefTechnicalDetails.gfhigh), qsTr("Mode: %1").arg(diveMode.currentText), qsTr("Water: %1").arg(waterType.currentText), qsTr("Reserve: %1 %2").arg(Backend.reserve_gas).arg(pressureUnit), "", qsTr("DECOMPRESSION SCHEDULE")]
		if (schedule.length === 0)
			lines.push(qsTr("No decompression stops generated."))
		for (var i = 0; i < schedule.length; ++i)
			lines.push((schedule[i].depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + depthUnit + "  " + formatDuration(schedule[i].duration))
		lines.push("", qsTr("Planning aid only. Review all settings, gases, schedule and warnings before diving."))
		return lines.join("\n")
	}
	Component.onCompleted: {
		Backend.planner_gflow = PrefTechnicalDetails.gflow
		Backend.planner_gfhigh = PrefTechnicalDetails.gfhigh
		addCylinder()
		segments.append({ "depth": Backend.length === Enums.METERS ? 18 : 60, "duration": 30,
			"gas": 0, "setpoint": Backend.default_setpoint, "divemode": 0 })
		generatePlan()
	}

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Build a validated dive plan"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: qsTr("Neo sends this profile directly to Subsurface's mature planner. No decompression or gas calculation is duplicated here."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Decompression model"); color: tokens.textMuted; font.pixelSize: 11 }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 3 : 1
				Text { text: qsTr("Bühlmann / GF: %1 / %2").arg(PrefTechnicalDetails.gflow).arg(PrefTechnicalDetails.gfhigh); color: tokens.textPrimary }
				Text { text: qsTr("Bottom SAC: %1").arg(Backend.bottomsac); color: tokens.textPrimary }
				Button { text: qsTr("Advanced settings"); onClicked: page.openPlannerSettings() }
			}
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Profile presets"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			RowLayout { Layout.fillWidth: true; TextField { id: presetName; Layout.fillWidth: true; placeholderText: qsTr("Preset name") }; Button { text: qsTr("Save current profile"); enabled: presetName.text.trim().length > 0; onClicked: { page.savePreset(presetName.text); presetName.clear() } } }
			Repeater { model: plannerStorage.presets; delegate: RowLayout { required property int index; required property var modelData; Layout.fillWidth: true; Label { text: modelData.name; color: tokens.textPrimary; Layout.fillWidth: true }; Button { text: qsTr("Load"); onClicked: page.loadPreset(index) }; Button { text: qsTr("Remove"); onClicked: { var saved = plannerStorage.presets || []; saved.splice(index, 1); plannerStorage.presets = saved } } } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Plan mode & environment"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				ComboBox { id: diveMode; Layout.fillWidth: true; model: [qsTr("Open circuit"), qsTr("CCR"), qsTr("pSCR")]; onActivated: page.generatePlan() }
				ComboBox { id: waterType; Layout.fillWidth: true; model: [qsTr("Sea water"), qsTr("Fresh water"), qsTr("EN13319")]; onActivated: page.generatePlan() }
			}
			CheckBox { visible: diveMode.currentIndex !== 0; text: qsTr("Deco on OC bailout"); checked: Backend.dobailout; onToggled: { Backend.dobailout = checked; page.generatePlan() } }
			CheckBox { visible: diveMode.currentIndex !== 0; text: qsTr("Calculate contingency variations"); checked: Backend.display_variations; onToggled: { Backend.display_variations = checked; page.generatePlan() } }
			RowLayout { Layout.fillWidth: true; Label { text: qsTr("Reserve gas (%1)").arg(page.pressureUnit); color: tokens.textMuted; Layout.fillWidth: true }; SpinBox { from: 0; to: Backend.pressure === Enums.BAR ? 400 : 6000; value: Backend.reserve_gas; onValueModified: { Backend.reserve_gas = value; page.generatePlan() } } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout { Layout.fillWidth: true; Text { text: qsTr("Gases & cylinders"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }; Button { text: qsTr("Add gas"); onClicked: page.addCylinder() } }
			Repeater { model: cylinders; delegate: GridLayout {
				required property int index; required property string type; required property string mix; required property real pressure; required property int use
				Layout.fillWidth: true; columns: page.width >= 700 ? 5 : 2
				Label { text: qsTr("Gas %1").arg(index + 1); color: tokens.textMuted }
				ComboBox { Layout.fillWidth: true; model: page.cylinderTypes; currentIndex: page.cylinderTypes.indexOf(type); onActivated: { cylinders.setProperty(index, "type", currentText); page.generatePlan() } }
				TextField { Layout.fillWidth: true; text: mix; placeholderText: qsTr("O₂/He e.g. 32/0"); onEditingFinished: { cylinders.setProperty(index, "mix", text); page.generatePlan() } }
				TextField { Layout.fillWidth: true; text: pressure; inputMethodHints: Qt.ImhDigitsOnly; placeholderText: page.pressureUnit; onEditingFinished: { cylinders.setProperty(index, "pressure", Number(text)); page.generatePlan() } }
				RowLayout { CheckBox { visible: diveMode.currentIndex === 1; text: qsTr("Diluent"); checked: use === 1; onToggled: { cylinders.setProperty(index, "use", checked ? 1 : 0); page.generatePlan() } }; Button { text: qsTr("Remove"); enabled: cylinders.count > 1; onClicked: { cylinders.remove(index); page.updateGasNames(); page.generatePlan() } } }
			} }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout { Layout.fillWidth: true; Text { text: qsTr("Profile segments"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }; Button { text: qsTr("Add segment"); onClicked: page.addSegment() } }
			Text { text: qsTr("Each row is a depth/time waypoint. Use separate rows for multilevel profiles and gas switches."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Repeater { model: segments; delegate: GridLayout {
				required property int index; required property real depth; required property real duration; required property int gas; required property real setpoint; required property int divemode
				Layout.fillWidth: true; columns: page.width >= 700 ? 6 : 2
				Label { text: qsTr("%1").arg(index + 1); color: tokens.textMuted }
				TextField { Layout.fillWidth: true; text: depth; placeholderText: qsTr("Depth (%1)").arg(page.depthUnit); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { segments.setProperty(index, "depth", Number(text)); page.generatePlan() } }
				TextField { Layout.fillWidth: true; text: duration; placeholderText: qsTr("Minutes"); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { segments.setProperty(index, "duration", Number(text)); page.generatePlan() } }
				ComboBox { Layout.fillWidth: true; model: page.gasNames; currentIndex: gas; onActivated: { segments.setProperty(index, "gas", currentIndex); page.generatePlan() } }
				TextField { visible: diveMode.currentIndex === 1; Layout.fillWidth: true; text: (setpoint / 1000.0).toFixed(2); placeholderText: qsTr("Setpoint bar"); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: { segments.setProperty(index, "setpoint", Math.round(Number(text) * 1000)); page.generatePlan() } }
				Button { text: qsTr("Remove"); enabled: segments.count > 1; onClicked: { segments.remove(index); page.generatePlan() } }
			} }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Calculated profile"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 4 : 2
				Components.MetricCard { label: qsTr("NDL"); value: page.formatDuration(page.finalSampleValue("ndl", -1)); Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("TTS"); value: page.formatDuration(page.finalSampleValue("tts", -1)); Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("Ceiling"); value: page.finalSampleValue("ceiling", 0) > 0 ? (page.finalSampleValue("ceiling", 0) / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) : "—"; suffix: page.finalSampleValue("ceiling", 0) > 0 ? page.depthUnit : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("CNS"); value: page.finalSampleValue("cns", 0) > 0 ? String(page.finalSampleValue("cns", 0)) : "—"; suffix: page.finalSampleValue("cns", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
			}
			Canvas { id: profileCanvas; Layout.fillWidth: true; Layout.preferredHeight: 190; onPaint: {
				var ctx = getContext("2d"); ctx.reset(); if (page.profileData.length < 2) return
				var maxTime = 0, maxDepth = 0; for (var i = 0; i < page.profileData.length; ++i) { maxTime = Math.max(maxTime, page.profileData[i].time); maxDepth = Math.max(maxDepth, page.profileData[i].depth) }
				if (maxTime <= 0 || maxDepth <= 0) return; var margin = 16, w = width - margin * 2, h = height - margin * 2
				ctx.strokeStyle = page.exceedsNDL ? "#F87171" : tokens.accent; ctx.lineWidth = 2; ctx.beginPath()
				for (var j = 0; j < page.profileData.length; ++j) { var p = page.profileData[j]; var x = margin + p.time / maxTime * w; var y = margin + p.depth / maxDepth * h; if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y) }; ctx.stroke()
			} }
			Connections { target: page; function onProfileDataChanged() { profileCanvas.requestPaint() }; function onExceedsNDLChanged() { profileCanvas.requestPaint() } }
			Label { visible: page.exceedsNDL; text: qsTr("This recreational plan exceeds the NDL. Review the schedule and warnings before saving."); color: "#F87171"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			TextArea { Layout.fillWidth: true; readOnly: true; text: page.planNotes; wrapMode: Text.Wrap; color: tokens.textPrimary; background: null }
			Text { visible: page.schedule.length > 0; text: qsTr("Decompression schedule"); color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
			Repeater { model: page.schedule; delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Label { text: qsTr("Stop"); color: tokens.textMuted; Layout.fillWidth: true }; Label { text: (modelData.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + page.depthUnit; color: tokens.textPrimary; Layout.preferredWidth: 100 }; Label { text: page.formatDuration(modelData.duration); color: tokens.textPrimary; Layout.preferredWidth: 100 } } }
			RowLayout { Layout.fillWidth: true; Button { text: qsTr("Recalculate"); onClicked: page.generatePlan() }; Button { text: qsTr("Copy deco slate"); onClicked: manager.copyToClipboard(page.decoSlate()) }; Item { Layout.fillWidth: true }; Button { text: qsTr("Save plan"); enabled: !page.exceedsNDL; onClicked: page.generatePlan(true) } }
		}
		Components.ModernCard { Layout.fillWidth: true; Text { text: qsTr("Technical tools"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }; Text { text: qsTr("Use the established gas calculator for MOD, Best Mix, END/EAD, CNS and OTU reference calculations."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }; Button { Layout.fillWidth: true; text: qsTr("Open gas calculator"); onClicked: page.openGasTools() } }
		Text { text: qsTr("Planning aid only. Confirm the active algorithm, units, gases, environmental assumptions, schedule and warnings before diving."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
