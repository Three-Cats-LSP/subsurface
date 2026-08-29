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

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Planner & decompression lab")
	background: Rectangle { color: tokens.background }
	signal openGasTools()
	signal openPlannerSettings()
	Modern.DesignTokens { id: tokens }
	property string depthUnit: Backend.length === Enums.METERS ? qsTr("m") : qsTr("ft")
	property string pressureUnit: Backend.pressure === Enums.BAR ? qsTr("bar") : qsTr("psi")
	property string sacUnit: Backend.volume === Enums.LITER ? qsTr("L/min") : qsTr("cu ft/min")
	property string speedUnit: Backend.length === Enums.METERS ? qsTr("m/min") : qsTr("ft/min")
	property string plannedDate: Qt.formatDate(new Date(), "yyyy-MM-dd")
	property string plannedTime: Qt.formatTime(new Date(), "hh:mm:ss")
	property real surfacePressureBar: 1.013
	property int customSalinity: 10300
	property string planNotes: ""
	property var profileData: []
	property var schedule: []
	property var gasAnalysis: []
	property bool exceedsNDL: false
	property bool planSaveAllowed: false
	property int planOtu: 0
	property var cylinderTypes: manager.cylinderListInit
	property var gasNames: []
	property var gasReference: []
	property var ndlReference: []
	property real conversionValue: 1
	property string plannerTextExport: ""
	property string plannerPdfExport: ""
	property string planPackageTextExport: ""
	property string planPackagePdfExport: ""
	property int contingencyScenario: 0
	property int contingencyDelta: 5
	property int contingencyGasIndex: 0
	property var contingencyResult: null
	property string contingencyTextExport: ""
	property string contingencyPdfExport: ""
	property var inspectedProfileSample: null
	property string activeProfileName: ""
	property bool activeProfileModified: false
	property string activeProfileId: ""
	property int activeProfileVersion: 0
	property bool activeProfileBuiltIn: false
	property var builtInProfiles: [
		{ "id": "subsurface-native", "name": qsTr("Subsurface native"), "version": 1,
			"summary": qsTr("Native defaults: Buhlmann GF 30/75 · 18 m/min descent · 9 m/min ascent tiers") },
		{ "id": "lsp-default", "name": qsTr("LSP Default (native mapping)"), "version": 1,
			"summary": qsTr("GF 20/85 · 20 m/min descent · 10/3/3 m/min ascent · 3 m last stop") },
		{ "id": "lsp-multideco", "name": qsTr("MultiDeco (native mapping)"), "version": 1,
			"summary": qsTr("GF 30/85 · 22 m/min descent · 9 m/min ascent tiers · 3 m last stop") },
		{ "id": "lsp-abysner", "name": qsTr("Abysner (native mapping)"), "version": 1,
			"summary": qsTr("GF 60/70 · 25 m/min descent · 9/3/3 m/min ascent · 3 m last stop") },
		{ "id": "lsp-subsurface", "name": qsTr("LSP Subsurface (native mapping)"), "version": 1,
			"summary": qsTr("GF 30/70 · 20 m/min descent · 9/6/3 m/min ascent · 3 m last stop") },
		{ "id": "lsp-gue", "name": qsTr("GUE DecPlanner (native mapping)"), "version": 1,
			"summary": qsTr("GF 20/85 · 20 m/min descent · 9/3/3 m/min ascent · 3 m last stop · pO2 1.2") },
		{ "id": "lsp-divekit", "name": qsTr("DiveKit (native mapping)"), "version": 1,
			"summary": qsTr("GF 50/80 · 20 m/min descent · 9 m/min ascent tiers · 6 m last stop") }
	]
	ListModel { id: cylinders }
	ListModel { id: segments }
	Settings { id: plannerStorage; category: "subsurface-neo/planner"; property var presets: [] }
	FolderDialog {
		id: plannerTextFolder
		currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
		onAccepted: page.plannerTextExport = manager.exportNeoPlannerText(selectedFolder, page.decoSlate())
	}
	FolderDialog { id: planPackageTextFolder; currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation); onAccepted: page.planPackageTextExport = manager.exportNeoPlannerText(selectedFolder, page.planPackage()) }
	FolderDialog { id: planPackagePdfFolder; currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation); onAccepted: page.planPackagePdfExport = manager.exportNeoPlannerPdf(selectedFolder, page.planPackage(), page.profileData) }
	FolderDialog {
		id: plannerPdfFolder
		currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
		onAccepted: page.plannerPdfExport = manager.exportNeoPlannerPdf(selectedFolder, page.decoSlate(), page.profileData)
	}
	FolderDialog { id: contingencyTextFolder; currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation); onAccepted: page.contingencyTextExport = manager.exportNeoPlannerText(selectedFolder, page.contingencySlate()) }
	FolderDialog { id: contingencyPdfFolder; currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation); onAccepted: page.contingencyPdfExport = manager.exportNeoPlannerPdf(selectedFolder, page.contingencySlate(), page.contingencyResult ? page.contingencyResult.profile || [] : []) }
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
		var preset = { "name": name.trim(), "cylinders": modelData(cylinders), "segments": modelData(segments), "diveMode": diveMode.currentIndex, "waterType": waterType.currentIndex, "customSalinity": customSalinity, "plannedDate": plannedDate, "plannedTime": plannedTime, "surfacePressureBar": surfacePressureBar,
			"decoMode": Backend.planner_deco_mode, "gflow": Backend.planner_gflow, "gfhigh": Backend.planner_gfhigh, "vpmbConservatism": Backend.vpmb_conservatism,
			"bottomSac": Backend.bottomsac, "decoSac": Backend.decosac, "reserveGas": Backend.reserve_gas, "bottomPo2": Backend.bottompo2 / 100, "decoPo2": Backend.decopo2 / 100,
			"descentRate": Backend.descrate, "deepAscentRate": Backend.ascrate75, "midAscentRate": Backend.ascrate50, "decoAscentRate": Backend.ascratestops, "finalAscentRate": Backend.ascratelast6m,
			"dropToFirstDepth": Backend.drop_stone_mode, "lastStop6m": Backend.last_stop6m, "switchAtRequiredStop": Backend.switch_at_req_stop, "minSwitchDuration": Backend.min_switch_duration,
			"surfaceSegment": Backend.surface_segment, "problemSolvingTime": Backend.problemsolvingtime, "backGasBreaks": Backend.doo2breaks, "bailout": Backend.dobailout,
			"defaultSetpoint": Backend.default_setpoint, "o2Narcotic": Backend.o2narcotic, "sacFactor": Backend.sacfactor }
		for (var i = 0; i < saved.length; ++i) {
			if (saved[i].name === preset.name) {
				saved[i] = preset
				plannerStorage.presets = saved
				activeProfileName = preset.name
				activeProfileId = ""
				activeProfileVersion = 0
				activeProfileBuiltIn = false
				activeProfileModified = false
				return
			}
		}
		saved.push(preset)
		plannerStorage.presets = saved
		activeProfileName = preset.name
		activeProfileId = ""
		activeProfileVersion = 0
		activeProfileBuiltIn = false
		activeProfileModified = false
	}
	function profileLabel() {
		if (activeProfileName.length === 0)
			return qsTr("Custom")
		var name = activeProfileBuiltIn ? qsTr("%1 v%2").arg(activeProfileName).arg(activeProfileVersion) : activeProfileName
		return activeProfileModified ? qsTr("%1 — Modified").arg(name) : name
	}
	function markProfileModified() {
		if (activeProfileName.length > 0)
			activeProfileModified = true
	}
	function resetActiveProfile() {
		if (activeProfileName.length === 0)
			return
		if (activeProfileBuiltIn) {
			for (var builtInIndex = 0; builtInIndex < builtInProfiles.length; ++builtInIndex) {
				if (builtInProfiles[builtInIndex].id === activeProfileId) {
					loadBuiltInProfile(builtInIndex)
					return
				}
			}
		}
		var saved = plannerStorage.presets || []
		for (var i = 0; i < saved.length; ++i) {
			if (saved[i].name === activeProfileName) {
				loadPreset(i)
				return
			}
		}
	}
	function renamePreset(index, name) {
		var trimmedName = name.trim()
		var saved = plannerStorage.presets || []
		if (trimmedName.length === 0 || !saved[index])
			return
		for (var i = 0; i < saved.length; ++i) {
			if (i !== index && saved[i].name === trimmedName)
				return
		}
		var oldName = saved[index].name
		saved[index].name = trimmedName
		plannerStorage.presets = saved
		if (activeProfileName === oldName)
			activeProfileName = trimmedName
	}
	function duplicatePreset(index) {
		var saved = plannerStorage.presets || []
		var source = saved[index]
		if (!source)
			return
		var copy = {}
		for (var key in source)
			copy[key] = source[key]
		var baseName = qsTr("%1 copy").arg(source.name)
		copy.name = baseName
		var suffix = 2
		var hasName = true
		while (hasName) {
			hasName = false
			for (var i = 0; i < saved.length; ++i) {
				if (saved[i].name === copy.name) {
					hasName = true
					copy.name = baseName + " " + suffix++
					break
				}
			}
		}
		saved.push(copy)
		plannerStorage.presets = saved
	}
	function nativePlannerSettings() {
		return {
			"decoMode": Enums.BUEHLMANN, "gflow": 30, "gfhigh": 75, "vpmbConservatism": 3,
			"bottomSac": Backend.volume === Enums.LITER ? 20 : 71, "decoSac": Backend.volume === Enums.LITER ? 17 : 60,
			"reserveGas": Backend.pressure === Enums.BAR ? 40 : 580, "bottomPo2": 1.4, "decoPo2": 1.6,
			"descentRate": Backend.length === Enums.METERS ? 18 : 60, "deepAscentRate": Backend.length === Enums.METERS ? 9 : 30,
			"midAscentRate": Backend.length === Enums.METERS ? 9 : 30, "decoAscentRate": Backend.length === Enums.METERS ? 9 : 30,
			"finalAscentRate": Backend.length === Enums.METERS ? 9 : 30, "dropToFirstDepth": false, "lastStop6m": false,
			"switchAtRequiredStop": false, "minSwitchDuration": 1, "surfaceSegment": 0, "problemSolvingTime": 4,
			"backGasBreaks": false, "bailout": false, "defaultSetpoint": 1100, "o2Narcotic": true, "sacFactor": 40
		}
	}
	function plannerRate(metersPerMinute) {
		return Backend.length === Enums.METERS ? metersPerMinute : Math.round(metersPerMinute * 3.28084)
	}
	function plannerSac(litresPerMinute) {
		return Backend.volume === Enums.LITER ? litresPerMinute : Math.round(litresPerMinute * 3.53147)
	}
	function lspMappedPlannerSettings(profileId) {
		var settings = nativePlannerSettings()
		settings.bottomSac = plannerSac(22)
		settings.decoSac = plannerSac(20)
		settings.descentRate = plannerRate(20)
		settings.deepAscentRate = plannerRate(10)
		settings.midAscentRate = plannerRate(3)
		settings.decoAscentRate = plannerRate(3)
		settings.finalAscentRate = plannerRate(3)
		settings.gflow = 20
		settings.gfhigh = 85
		if (profileId === "lsp-multideco") {
			settings.gflow = 30; settings.gfhigh = 85; settings.descentRate = plannerRate(22)
			settings.deepAscentRate = plannerRate(9); settings.midAscentRate = plannerRate(9)
			settings.decoAscentRate = plannerRate(9); settings.finalAscentRate = plannerRate(9)
		} else if (profileId === "lsp-abysner") {
			settings.gflow = 60; settings.gfhigh = 70; settings.descentRate = plannerRate(25)
			settings.deepAscentRate = plannerRate(9)
		} else if (profileId === "lsp-subsurface") {
			settings.gflow = 30; settings.gfhigh = 70; settings.deepAscentRate = plannerRate(9)
			settings.midAscentRate = plannerRate(6)
		} else if (profileId === "lsp-gue") {
			settings.bottomPo2 = 1.2; settings.decoPo2 = 1.2; settings.deepAscentRate = plannerRate(9)
		} else if (profileId === "lsp-divekit") {
			settings.gflow = 50; settings.gfhigh = 80; settings.deepAscentRate = plannerRate(9)
			settings.midAscentRate = plannerRate(9); settings.decoAscentRate = plannerRate(9)
			settings.lastStop6m = true
		} else if (profileId !== "lsp-default") {
			return null
		}
		return settings
	}
	function activeProfileLimitations() {
		if (!activeProfileBuiltIn || activeProfileId.indexOf("lsp-") !== 0)
			return ""
		return qsTr("This preset contains only native Neo planner settings.")
	}
	function profileExportIdentity() {
		var lines = [qsTr("Profile: %1").arg(profileLabel())]
		var limitations = activeProfileLimitations()
		if (limitations.length > 0)
			lines.push(qsTr("Profile mapping: %1").arg(limitations))
		return lines
	}
	function applyPlannerSettings(preset) {
		if (preset.decoMode !== undefined) Backend.planner_deco_mode = preset.decoMode
		if (preset.gflow !== undefined) Backend.planner_gflow = preset.gflow
		if (preset.gfhigh !== undefined) Backend.planner_gfhigh = preset.gfhigh
		if (preset.vpmbConservatism !== undefined) Backend.vpmb_conservatism = preset.vpmbConservatism
		if (preset.bottomSac !== undefined) Backend.bottomsac = preset.bottomSac
		if (preset.decoSac !== undefined) Backend.decosac = preset.decoSac
		if (preset.reserveGas !== undefined) Backend.reserve_gas = preset.reserveGas
		if (preset.bottomPo2 !== undefined) Backend.bottompo2 = preset.bottomPo2
		if (preset.decoPo2 !== undefined) Backend.decopo2 = preset.decoPo2
		if (preset.descentRate !== undefined) Backend.descrate = preset.descentRate
		if (preset.deepAscentRate !== undefined) Backend.ascrate75 = preset.deepAscentRate
		if (preset.midAscentRate !== undefined) Backend.ascrate50 = preset.midAscentRate
		if (preset.decoAscentRate !== undefined) Backend.ascratestops = preset.decoAscentRate
		if (preset.finalAscentRate !== undefined) Backend.ascratelast6m = preset.finalAscentRate
		if (preset.dropToFirstDepth !== undefined) Backend.drop_stone_mode = preset.dropToFirstDepth
		if (preset.lastStop6m !== undefined) Backend.last_stop6m = preset.lastStop6m
		if (preset.switchAtRequiredStop !== undefined) Backend.switch_at_req_stop = preset.switchAtRequiredStop
		if (preset.minSwitchDuration !== undefined) Backend.min_switch_duration = preset.minSwitchDuration
		if (preset.surfaceSegment !== undefined) Backend.surface_segment = preset.surfaceSegment
		if (preset.problemSolvingTime !== undefined) Backend.problemsolvingtime = preset.problemSolvingTime
		if (preset.backGasBreaks !== undefined) Backend.doo2breaks = preset.backGasBreaks
		if (preset.bailout !== undefined) Backend.dobailout = preset.bailout
		if (preset.defaultSetpoint !== undefined) Backend.default_setpoint = preset.defaultSetpoint
		if (preset.o2Narcotic !== undefined) Backend.o2narcotic = preset.o2Narcotic
		if (preset.sacFactor !== undefined) Backend.sacfactor = preset.sacFactor
	}
	function loadBuiltInProfile(index) {
		var profile = builtInProfiles[index]
		if (!profile)
			return
		var settings = profile.id === "subsurface-native" ? nativePlannerSettings() : lspMappedPlannerSettings(profile.id)
		if (!settings)
			return
		applyPlannerSettings(settings)
		activeProfileName = profile.name
		activeProfileId = profile.id
		activeProfileVersion = profile.version
		activeProfileBuiltIn = true
		activeProfileModified = false
		generatePlan(false, true)
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
		if (preset.customSalinity !== undefined) customSalinity = preset.customSalinity
		if (preset.plannedDate !== undefined) plannedDate = preset.plannedDate
		if (preset.plannedTime !== undefined) plannedTime = preset.plannedTime
		if (preset.surfacePressureBar !== undefined) surfacePressureBar = preset.surfacePressureBar
		applyPlannerSettings(preset)
		activeProfileName = preset.name
		activeProfileId = ""
		activeProfileVersion = 0
		activeProfileBuiltIn = false
		activeProfileModified = false
		generatePlan(false, true)
	}

	function updateGasNames() {
		var names = []
		for (var i = 0; i < cylinders.count; ++i)
			names.push(qsTr("Gas %1").arg(i + 1))
		gasNames = names
	}
	function updateGasReference() {
		var references = []
		for (var i = 0; i < cylinders.count; ++i) {
			var cylinder = cylinders.get(i)
			var parts = String(cylinder.mix).split("/")
			var parsedO2 = Number(parts[0])
			var o2 = isNaN(parsedO2) ? 21 : Math.max(0, Math.min(100, parsedO2))
			var parsedHe = Number(parts.length > 1 ? parts[1] : 0)
			var he = isNaN(parsedHe) ? 0 : Math.max(0, Math.min(100 - o2, parsedHe))
			var values = Backend.divePlannerPointsModel.calculateGasInfo(cylinder.type, Math.round(o2 * 10), Math.round(he * 10))
			var reference = values.length > 4 ? values[4] : ({})
			references.push({ "name": qsTr("Gas %1").arg(i + 1), "mix": o2 + "/" + he, "mod": reference.mod || "—", "ead": reference.ead || "—" })
		}
		gasReference = references
	}
	function calculateNdlReference() {
		var depths = Backend.length === Enums.METERS ? [12, 18, 24, 30, 36, 42] : [40, 60, 80, 100, 120, 140]
		var cylinderData = modelData(cylinders)
		var salinity = waterType.currentIndex === 0 ? 10300 : waterType.currentIndex === 1 ? 10000 : waterType.currentIndex === 2 ? 10200 : customSalinity
		var table = []
		for (var i = 0; i < depths.length; ++i) {
			var segment = { "depth": depths[i], "duration": 1, "gas": 0, "setpoint": diveMode.currentIndex === 1 ? Backend.default_setpoint : 0, "divemode": diveMode.currentIndex }
			var result = Backend.divePlannerPointsModel.calculatePlan(cylinderData, [segment], plannedDate, plannedTime, diveMode.currentIndex, salinity, Math.round(surfacePressureBar * 1000), false)
			var samples = result.profile || []
			var sample = samples.length > 0 ? samples[samples.length - 1] : null
			var ndl = sample && sample.ndl !== undefined ? sample.ndl : -1
			table.push({ "depth": depths[i], "ndl": ndl, "available": ndl >= 0, "saveAllowed": result.planSaveAllowed === true })
		}
		ndlReference = table
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
	function generatePlan(savePlan, retainActiveProfile) {
		if (cylinders.count === 0 || segments.count === 0)
			return
		if (!retainActiveProfile)
			markProfileModified()
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
				"setpoint": segment.setpoint, "divemode": diveMode.currentIndex === 2 ? 2 : segment.divemode })
		}
		var salinity = waterType.currentIndex === 0 ? 10300 : waterType.currentIndex === 1 ? 10000 : waterType.currentIndex === 2 ? 10200 : customSalinity
		var result = Backend.divePlannerPointsModel.calculatePlan(cylinderData, segmentData,
			plannedDate, plannedTime,
			diveMode.currentIndex, salinity, Math.round(surfacePressureBar * 1000), savePlan === true)
		planNotes = result.notes || ""
		updateGasReference()
		profileData = result.profile || []
		schedule = result.schedule || []
		gasAnalysis = result.gasAnalysis || []
		exceedsNDL = result.exceedsNDL === true
		planSaveAllowed = result.planSaveAllowed === true
		planOtu = result.otu || 0
		if (savePlan === true && result.newDiveId !== undefined && result.newDiveId !== -1) {
			manager.selectDive(result.newDiveId)
			showPage(diveList)
		}
	}
	function calculateContingency() {
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
				"setpoint": segment.setpoint, "divemode": diveMode.currentIndex === 2 ? 2 : segment.divemode })
		}
		if (contingencyScenario === 0) {
			segmentData[segmentData.length - 1].duration += contingencyDelta
		} else if (contingencyScenario === 1) {
			var deepest = 0
			for (var k = 1; k < segmentData.length; ++k)
				if (segmentData[k].depth > segmentData[deepest].depth) deepest = k
			segmentData[deepest].depth += contingencyDelta
		} else if (contingencyGasIndex >= 0 && contingencyGasIndex < cylinderData.length) {
			cylinderData[contingencyGasIndex].use = 3 // NOT_USED: remove this gas from the mature planner input
		}
		var salinity = waterType.currentIndex === 0 ? 10300 : waterType.currentIndex === 1 ? 10000 : waterType.currentIndex === 2 ? 10200 : customSalinity
		contingencyResult = Backend.divePlannerPointsModel.calculatePlan(cylinderData, segmentData, plannedDate, plannedTime,
			diveMode.currentIndex, salinity, Math.round(surfacePressureBar * 1000), false)
		generatePlan(false, true)
	}
	function contingencyName() { return contingencyScenario === 0 ? qsTr("Extra bottom time (+%1 min)").arg(contingencyDelta) : contingencyScenario === 1 ? qsTr("Deeper profile (+%1 %2)").arg(contingencyDelta).arg(depthUnit) : qsTr("Lost gas: %1").arg(gasNames[contingencyGasIndex] || qsTr("selected gas")) }
	function contingencySlate() {
		if (!contingencyResult) return ""
		var modelSettings = Backend.planner_deco_mode === Enums.BUEHLMANN ? qsTr("GF %1/%2").arg(Backend.planner_gflow).arg(Backend.planner_gfhigh) : Backend.planner_deco_mode === Enums.VPMB ? qsTr("Conservatism %1").arg(Backend.vpmb_conservatism) : qsTr("NDL planning")
		var lines = [qsTr("SUBSURFACE NEO CONTINGENCY PLAN"), qsTr("Scenario: %1").arg(contingencyName()), qsTr("This is a separately calculated contingency; it does not replace the main plan."), qsTr("Model: %1 — %2").arg(algorithmName()).arg(modelSettings), qsTr("Surface pressure: %1 bar · Altitude: %2 m").arg(surfacePressureBar.toFixed(3)).arg(manager.plannerAltitudeForSurfacePressure(surfacePressureBar).toFixed(0)), qsTr("Mode: %1 · Water: %2").arg(diveMode.currentText).arg(waterDescription()), "", qsTr("DECOMPRESSION SCHEDULE")]
		var stops = contingencyResult.schedule || []
		for (var i = 0; i < stops.length; ++i) { var stop = stops[i]; lines.push((stop.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + depthUnit + "  " + formatDuration(stop.duration) + (stop.gas !== undefined ? "  " + stop.gas : "")) }
		var gases = contingencyResult.gasAnalysis || []
		lines.push("", qsTr("GAS STATUS"))
		for (var j = 0; j < gases.length; ++j) lines.push(qsTr("%1: remaining %2; end %3").arg(gases[j].mix).arg(gases[j].remaining).arg(gases[j].endPressure))
		if (contingencyResult.notes) lines.push("", qsTr("PLANNER NOTES AND WARNINGS"), contingencyResult.notes)
		lines.push("", qsTr("Planning aid only. Review all settings, gases, schedule and warnings before diving."))
		return lines.join("\n")
	}
	function planPackage() {
		var mainPlan = decoSlate()
		if (!contingencyResult)
			return mainPlan
		return mainPlan + "\n\n" + "=".repeat(64) + "\n\n" + contingencySlate()
	}
	function finalSampleValue(name, fallback) {
		return profileData.length > 0 && profileData[profileData.length - 1][name] !== undefined ? profileData[profileData.length - 1][name] : fallback
	}
	function profileMaxTime() {
		var maximum = 0
		for (var i = 0; i < profileData.length; ++i)
			maximum = Math.max(maximum, profileData[i].time)
		return maximum
	}
	function inspectProfileAt(x, profileWidth) {
		if (profileData.length === 0 || profileWidth <= 32)
			return
		var targetTime = Math.max(0, Math.min(profileMaxTime(), (x - 16) / (profileWidth - 32) * profileMaxTime()))
		var nearest = profileData[0]
		for (var i = 1; i < profileData.length; ++i) {
			if (Math.abs(profileData[i].time - targetTime) < Math.abs(nearest.time - targetTime))
				nearest = profileData[i]
		}
		inspectedProfileSample = nearest
	}
	function profileSampleX(sample, profileWidth) {
		return sample && profileMaxTime() > 0 ? 16 + sample.time / profileMaxTime() * (profileWidth - 32) : 0
	}
	function formatDuration(seconds) {
		if (seconds === undefined || seconds < 0)
			return "—"
		return Math.floor(seconds / 60) + qsTr(" min") + (seconds % 60 ? " " + (seconds % 60) + qsTr(" s") : "")
	}
	function algorithmName() {
		if (Backend.planner_deco_mode === Enums.VPMB)
			return qsTr("VPM-B")
		if (Backend.planner_deco_mode === Enums.RECREATIONAL)
			return qsTr("Recreational (NDL)")
		return qsTr("Buhlmann ZHL-16C + GF")
	}
	function sacText(value) {
		return Backend.volume === Enums.LITER ? String(value) : (value / 100.0).toFixed(2)
	}
	function sacValue(text) {
		return Backend.volume === Enums.LITER ? Math.round(Number(text)) : Math.round(Number(text) * 100)
	}
	function formatSetpoint(mbar) {
		return mbar && mbar > 0 ? (mbar / 1000.0).toFixed(2) + " bar" : "—"
	}
	function waterDescription() {
		return waterType.currentIndex === 3 ? qsTr("Custom (%1 kg/10,000 L)").arg(customSalinity) : waterType.currentText
	}
	function decoSlate() {
		var modelSettings = Backend.planner_deco_mode === Enums.BUEHLMANN ? qsTr("GF %1/%2").arg(Backend.planner_gflow).arg(Backend.planner_gfhigh) : Backend.planner_deco_mode === Enums.VPMB ? qsTr("Conservatism %1").arg(Backend.vpmb_conservatism) : qsTr("NDL planning")
		var lines = [qsTr("SUBSURFACE NEO DIVE PLAN"), qsTr("Planned start: %1 %2").arg(plannedDate).arg(plannedTime), qsTr("Surface pressure: %1 bar").arg(surfacePressureBar.toFixed(3)), qsTr("Model: %1 — %2").arg(algorithmName()).arg(modelSettings)].concat(profileExportIdentity(), [qsTr("Mode: %1").arg(diveMode.currentText), qsTr("Water: %1").arg(waterDescription()), qsTr("Bottom/deco SAC: %1 / %2 %3").arg(sacText(Backend.bottomsac)).arg(sacText(Backend.decosac)).arg(sacUnit), qsTr("Reserve: %1 %2").arg(Backend.reserve_gas).arg(pressureUnit), "", qsTr("GASES")])
		lines.splice(3, 0, qsTr("Altitude: %1 m").arg(manager.plannerAltitudeForSurfacePressure(surfacePressureBar).toFixed(0)))
		for (var gasIndex = 0; gasIndex < cylinders.count; ++gasIndex) {
			var cylinder = cylinders.get(gasIndex)
			lines.push(qsTr("Gas %1: %2 — %3, %4 %5").arg(gasIndex + 1).arg(cylinder.mix).arg(cylinder.type).arg(cylinder.pressure).arg(pressureUnit))
		}
		for (var analysisIndex = 0; analysisIndex < gasAnalysis.length; ++analysisIndex) {
			var gas = gasAnalysis[analysisIndex]
			lines.push(qsTr("%1: used %2; remaining %3; end %4").arg(gas.mix).arg(gas.used).arg(gas.remaining).arg(gas.endPressure))
		}
		lines.push("", qsTr("CALCULATED ANALYSIS"))
		lines.push(qsTr("NDL: %1  TTS: %2  Ceiling: %3").arg(formatDuration(finalSampleValue("ndl", -1))).arg(formatDuration(finalSampleValue("tts", -1))).arg(finalSampleValue("ceiling", 0) > 0 ? (finalSampleValue("ceiling", 0) / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + depthUnit : "—"))
		lines.push(qsTr("Current GF: %1%  Surface GF: %2%  pO₂: %3 bar  Tissue: %4%").arg(finalSampleValue("gf", 0).toFixed(0)).arg(finalSampleValue("surfaceGf", 0).toFixed(0)).arg(finalSampleValue("po2", 0) > 0 ? (finalSampleValue("po2", 0) / 1000.0).toFixed(2) : "—").arg(finalSampleValue("tissueLoad", 0).toFixed(0)))
		lines.push(qsTr("CNS: %1%  OTU: %2").arg(finalSampleValue("cns", 0)).arg(planOtu), "", qsTr("DECOMPRESSION SCHEDULE"))
		if (schedule.length === 0)
			lines.push(qsTr("No decompression stops generated."))
		for (var i = 0; i < schedule.length; ++i) {
			var stop = schedule[i]
			lines.push((stop.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + depthUnit + "  " + formatDuration(stop.duration) + (stop.gas !== undefined ? "  " + stop.gas : "") + (stop.runTime !== undefined ? "  RT " + formatDuration(stop.runTime) : "") + (stop.tts !== undefined ? "  TTS " + formatDuration(stop.tts) : "") + (stop.setpoint !== undefined ? "  SP " + formatSetpoint(stop.setpoint) : ""))
		}
		if (planNotes.length > 0)
			lines.push("", qsTr("PLANNER NOTES AND WARNINGS"), planNotes)
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
			Text { text: page.algorithmName(); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				Components.NeoComboBox { id: decoAlgorithm; Layout.fillWidth: true; accessibleName: qsTr("Decompression model"); model: [qsTr("Recreational (NDL)"), qsTr("Buhlmann ZHL-16C + GF"), qsTr("VPM-B")]; currentIndex: Backend.planner_deco_mode === Enums.VPMB ? 2 : Backend.planner_deco_mode === Enums.BUEHLMANN ? 1 : 0; onActivated: { Backend.planner_deco_mode = currentIndex === 2 ? Enums.VPMB : currentIndex === 1 ? Enums.BUEHLMANN : Enums.RECREATIONAL; page.generatePlan() } }
				Text { text: qsTr("Subsurface's established planner is the calculation source."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				RowLayout { visible: Backend.planner_deco_mode === Enums.BUEHLMANN; Layout.fillWidth: true; Label { text: qsTr("GF low"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("GF low"); from: 1; to: 150; value: Backend.planner_gflow; onValueModified: { Backend.planner_gflow = value; page.generatePlan() } }
 Label { text: qsTr("GF high"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("GF high"); from: 1; to: 150; value: Backend.planner_gfhigh; onValueModified: { Backend.planner_gfhigh = value; page.generatePlan() } } }
				RowLayout { visible: Backend.planner_deco_mode === Enums.VPMB; Layout.fillWidth: true; Label { text: qsTr("VPM-B conservatism"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("VPM-B conservatism"); from: 0; to: 4; value: Backend.vpmb_conservatism; onValueModified: { Backend.vpmb_conservatism = value; page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Bottom SAC (%1)").arg(page.sacUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoTextField { Layout.preferredWidth: 86; accessibleName: qsTr("Bottom SAC (%1)").arg(page.sacUnit); text: page.sacText(Backend.bottomsac); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: { Backend.bottomsac = page.sacValue(text); page.generatePlan() } }
 Label { text: qsTr("Deco SAC (%1)").arg(page.sacUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoTextField { Layout.preferredWidth: 86; accessibleName: qsTr("Deco SAC (%1)").arg(page.sacUnit); text: page.sacText(Backend.decosac); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: { Backend.decosac = page.sacValue(text); page.generatePlan() } } }
				Text { text: qsTr("Active settings: %1").arg(Backend.planner_deco_mode === Enums.BUEHLMANN ? qsTr("GF %1/%2").arg(Backend.planner_gflow).arg(Backend.planner_gfhigh) : Backend.planner_deco_mode === Enums.VPMB ? qsTr("Conservatism %1").arg(Backend.vpmb_conservatism) : qsTr("NDL planning")); color: tokens.textPrimary }
				Text { text: qsTr("Bottom/deco SAC: %1 / %2 %3").arg(page.sacText(Backend.bottomsac)).arg(page.sacText(Backend.decosac)).arg(page.sacUnit); color: tokens.textPrimary }
				Components.NeoButton { text: qsTr("More planner settings"); onClicked: page.openPlannerSettings() }
			}
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Profile presets"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Active profile: %1").arg(page.profileLabel()); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { text: qsTr("Native profiles and external native mappings contain only settings Subsurface supports. Mappings do not claim schedule compatibility."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoButton { visible: page.activeProfileModified; Layout.fillWidth: true; text: qsTr("Reset to %1").arg(page.activeProfileName); onClicked: page.resetActiveProfile() }
			Text { text: qsTr("App reference presets"); color: tokens.textMuted; font.weight: Font.DemiBold; Layout.fillWidth: true }
			Repeater { model: page.builtInProfiles; delegate: RowLayout {
				required property int index
				required property var modelData
				Layout.fillWidth: true
				ColumnLayout { Layout.fillWidth: true; Text { text: qsTr("%1 v%2").arg(modelData.name).arg(modelData.version); color: tokens.textPrimary; font.weight: Font.DemiBold }
 Text { text: modelData.summary; color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
 Text { visible: modelData.id.indexOf("lsp-") === 0; text: qsTr("Native settings only."); color: tokens.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
				Components.NeoButton { text: qsTr("Load"); onClicked: page.loadBuiltInProfile(index) }
			} }
			Text { text: qsTr("My profiles"); color: tokens.textMuted; font.weight: Font.DemiBold; Layout.fillWidth: true }
			RowLayout { Layout.fillWidth: true; TextField { id: presetName; Layout.fillWidth: true; placeholderText: qsTr("Preset name") }
 Components.NeoButton { text: qsTr("Save current profile"); enabled: presetName.text.trim().length > 0; onClicked: { page.savePreset(presetName.text); presetName.clear() } } }
			Repeater { model: plannerStorage.presets; delegate: RowLayout {
				required property int index
				required property var modelData
				Layout.fillWidth: true
				Components.NeoTextField { id: savedPresetName; Layout.fillWidth: true; text: modelData.name }
				Components.NeoButton { text: qsTr("Load"); onClicked: page.loadPreset(index) }
				Components.NeoButton { text: qsTr("Rename"); enabled: savedPresetName.text.trim().length > 0 && savedPresetName.text !== modelData.name; onClicked: page.renamePreset(index, savedPresetName.text) }
				Components.NeoButton { text: qsTr("Duplicate"); onClicked: page.duplicatePreset(index) }
				Components.NeoButton { text: qsTr("Remove"); onClicked: { var removedName = modelData.name; var saved = plannerStorage.presets || []; saved.splice(index, 1); plannerStorage.presets = saved; if (page.activeProfileName === removedName) { page.activeProfileName = ""; page.activeProfileModified = false } } }
			} }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Travel and stop settings"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("These rates and stop choices are sent directly to the established Subsurface planner."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Descent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Descent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: Backend.descrate; enabled: Backend.drop_stone_mode; onValueModified: { Backend.descrate = value; page.generatePlan() } } }
				Components.NeoCheckBox { text: qsTr("Drop to first depth"); checked: Backend.drop_stone_mode; onToggled: { Backend.drop_stone_mode = checked; page.generatePlan() } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Deep ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Deep ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: Backend.ascrate75; onValueModified: { Backend.ascrate75 = value; page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Mid ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Mid ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: Backend.ascrate50; onValueModified: { Backend.ascrate50 = value; page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Deco ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Deco ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: Backend.ascratestops; onValueModified: { Backend.ascratestops = value; page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Final ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Final ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: Backend.ascratelast6m; onValueModified: { Backend.ascratelast6m = value; page.generatePlan() } } }
				Components.NeoCheckBox { text: qsTr("Last stop at 6 m / 20 ft"); checked: Backend.last_stop6m; onToggled: { Backend.last_stop6m = checked; page.generatePlan() } }
				Components.NeoCheckBox { text: qsTr("Switch gas only at required stops"); checked: Backend.switch_at_req_stop; onToggled: { Backend.switch_at_req_stop = checked; page.generatePlan() } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Minimum gas-switch time (min)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Minimum gas-switch time in minutes"); from: 0; to: 4; value: Backend.min_switch_duration; onValueModified: { Backend.min_switch_duration = value; page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Surface segment (min)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Surface segment in minutes"); from: 0; to: 4; value: Backend.surface_segment; onValueModified: { Backend.surface_segment = value; page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Problem-solving time (min)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Problem-solving time in minutes"); from: 1; to: 9; value: Backend.problemsolvingtime; onValueModified: { Backend.problemsolvingtime = value; page.generatePlan() } } }
				Components.NeoCheckBox { text: qsTr("Plan back-gas breaks"); checked: Backend.doo2breaks; onToggled: { Backend.doo2breaks = checked; page.generatePlan() } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Bottom pO₂ limit (bar)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox {
					accessibleName: qsTr("Bottom pO₂ limit in bar")
					from: 50; to: 250; stepSize: 5; value: Backend.bottompo2
					textFromValue: function(value) { return (value / 100).toFixed(2) }
					valueFromText: function(text) { return Math.round(Number(text) * 100) }
					onValueModified: { Backend.bottompo2 = value / 100; page.generatePlan() }
				} }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Deco pO₂ limit (bar)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox {
					accessibleName: qsTr("Deco pO₂ limit in bar")
					from: 50; to: 250; stepSize: 5; value: Backend.decopo2
					textFromValue: function(value) { return (value / 100).toFixed(2) }
					valueFromText: function(text) { return Math.round(Number(text) * 100) }
					onValueModified: { Backend.decopo2 = value / 100; page.generatePlan() }
				} }
				RowLayout { visible: diveMode.currentIndex === 1; Layout.fillWidth: true; Label { text: qsTr("CCR default setpoint (bar)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox {
					accessibleName: qsTr("CCR default setpoint in bar")
					from: 160; to: 2000; stepSize: 50; value: Backend.default_setpoint
					textFromValue: function(value) { return (value / 1000).toFixed(2) }
					valueFromText: function(text) { return Math.round(Number(text) * 1000) }
					onValueModified: { Backend.default_setpoint = value; page.generatePlan() }
				} }
			}
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Plan mode & environment"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				Components.NeoTextField { Layout.fillWidth: true; text: page.plannedDate; inputMask: "0000-00-00"; placeholderText: qsTr("Planned date (YYYY-MM-DD)"); onEditingFinished: { page.plannedDate = text; page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: page.plannedTime; inputMask: "00:00:00"; placeholderText: qsTr("Planned time (HH:MM:SS)"); onEditingFinished: { page.plannedTime = text; page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: page.surfacePressureBar.toFixed(3); inputMethodHints: Qt.ImhFormattedNumbersOnly; placeholderText: qsTr("Surface pressure (bar)"); onEditingFinished: { var pressure = Number(text); if (!isNaN(pressure) && pressure > 0) { page.surfacePressureBar = pressure; page.generatePlan() } } }
				Components.NeoTextField { Layout.fillWidth: true; text: manager.plannerAltitudeForSurfacePressure(page.surfacePressureBar).toFixed(0); inputMethodHints: Qt.ImhFormattedNumbersOnly; placeholderText: qsTr("Altitude (-500 to 12,000 m)"); onEditingFinished: { var altitude = Number(text); if (!isNaN(altitude) && altitude >= -500 && altitude <= 12000) { page.surfacePressureBar = manager.plannerSurfacePressureForAltitude(altitude); page.generatePlan() } } }
				Components.NeoComboBox { id: diveMode; Layout.fillWidth: true; accessibleName: qsTr("Dive mode"); model: [qsTr("Open circuit"), qsTr("CCR"), qsTr("pSCR")]; onActivated: page.generatePlan() }
				Components.NeoComboBox { id: waterType; Layout.fillWidth: true; accessibleName: qsTr("Water type"); model: [qsTr("Sea water"), qsTr("Fresh water"), qsTr("EN13319"), qsTr("Custom water density")]; onActivated: page.generatePlan() }
				RowLayout { visible: waterType.currentIndex === 3; Layout.fillWidth: true; Label { text: qsTr("Water density (kg/10,000 L)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Water density in kilograms per 10,000 litres"); from: 9000; to: 12000; value: page.customSalinity; onValueModified: { page.customSalinity = value; page.generatePlan() } } }
			}
			Text { text: qsTr("For a later planned start, Subsurface initializes tissues from compatible logged dives and their surface intervals."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { text: qsTr("Altitude and surface pressure describe the same native planner input. Changing either updates the pressure used to calculate this plan."); color: tokens.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoCheckBox { visible: diveMode.currentIndex !== 0; text: qsTr("Deco on OC bailout"); checked: Backend.dobailout; onToggled: { Backend.dobailout = checked; page.generatePlan() } }
			Components.NeoCheckBox { visible: Backend.planner_deco_mode !== Enums.RECREATIONAL; text: qsTr("Calculate contingency variations"); checked: Backend.display_variations; onToggled: { Backend.display_variations = checked; page.generatePlan() } }
			Text { visible: Backend.planner_deco_mode !== Enums.RECREATIONAL && Backend.display_variations; text: qsTr("Subsurface adds the calculated contingencies to the plan notes below; the main schedule remains unchanged."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			RowLayout { Layout.fillWidth: true; Label { text: qsTr("Reserve gas (%1)").arg(page.pressureUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Reserve gas (%1)").arg(page.pressureUnit); from: 0; to: Backend.pressure === Enums.BAR ? 400 : 6000; value: Backend.reserve_gas; onValueModified: { Backend.reserve_gas = value; page.generatePlan() } } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Contingency scenario"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Calculate a separate mature-planner result; the main plan remains unchanged."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			RowLayout { Layout.fillWidth: true; ComboBox { Layout.fillWidth: true; model: [qsTr("Extra bottom time"), qsTr("Deeper profile"), qsTr("Lost deco/travel gas")]; currentIndex: page.contingencyScenario; onActivated: page.contingencyScenario = currentIndex }
 Components.NeoSpinBox { visible: page.contingencyScenario !== 2; accessibleName: page.contingencyScenario === 0 ? qsTr("Extra bottom time in minutes") : qsTr("Extra depth (%1)").arg(page.depthUnit); from: 1; to: 30; value: page.contingencyDelta; onValueModified: page.contingencyDelta = value }
 Components.NeoComboBox { visible: page.contingencyScenario === 2; Layout.fillWidth: true; accessibleName: qsTr("Lost decompression or travel gas"); model: page.gasNames; currentIndex: page.contingencyGasIndex; onActivated: page.contingencyGasIndex = currentIndex }
 Label { visible: page.contingencyScenario !== 2; text: page.contingencyScenario === 0 ? qsTr("minutes") : page.depthUnit; color: tokens.textMuted } }
			Components.NeoButton { Layout.fillWidth: true; text: qsTr("Calculate %1").arg(page.contingencyName()); onClicked: page.calculateContingency() }
			ColumnLayout { visible: page.contingencyResult !== null; Layout.fillWidth: true
				Text { text: qsTr("Separate result: %1").arg(page.contingencyName()); color: tokens.accent; font.weight: Font.DemiBold }
				Text { text: qsTr("Save allowed: %1 · NDL exceeded: %2 · OTU: %3").arg(page.contingencyResult && page.contingencyResult.planSaveAllowed ? qsTr("Yes") : qsTr("No")).arg(page.contingencyResult && page.contingencyResult.exceedsNDL ? qsTr("Yes") : qsTr("No")).arg(page.contingencyResult ? page.contingencyResult.otu : 0); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				Repeater { model: page.contingencyResult ? page.contingencyResult.schedule || [] : []; delegate: Text { required property var modelData; text: qsTr("%1 %2 · %3%4").arg((modelData.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1)).arg(page.depthUnit).arg(page.formatDuration(modelData.duration)).arg(modelData.gas !== undefined ? " · " + modelData.gas : ""); color: tokens.textSecondary; Layout.fillWidth: true } }
				Repeater { model: page.contingencyResult ? page.contingencyResult.gasAnalysis || [] : []; delegate: Text { required property var modelData; text: qsTr("%1: remaining %2; end %3").arg(modelData.mix).arg(modelData.remaining).arg(modelData.endPressure); color: modelData.belowMinimum || modelData.belowReserve ? "#F87171" : tokens.textSecondary; Layout.fillWidth: true } }
				RowLayout { Layout.fillWidth: true; Button { text: qsTr("Copy contingency"); onClicked: manager.copyToClipboard(page.contingencySlate()) }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save TXT"); onClicked: contingencyTextFolder.open() }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save PDF"); onClicked: contingencyPdfFolder.open() } }
			}
		}
		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout { Layout.fillWidth: true; Text { text: qsTr("Gases & cylinders"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
 Components.NeoButton { text: qsTr("Add gas"); onClicked: page.addCylinder() } }
			Repeater { model: cylinders; delegate: GridLayout {
				required property int index; required property string type; required property string mix; required property real pressure; required property int use
				Layout.fillWidth: true; columns: page.width >= 700 ? 5 : 2
				Label { text: qsTr("Gas %1").arg(index + 1); color: tokens.textMuted }
				Components.NeoComboBox { Layout.fillWidth: true; accessibleName: qsTr("Cylinder type for gas %1").arg(index + 1); model: page.cylinderTypes; currentIndex: page.cylinderTypes.indexOf(type); onActivated: { cylinders.setProperty(index, "type", currentText); page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: mix; placeholderText: qsTr("O₂/He e.g. 32/0"); onEditingFinished: { cylinders.setProperty(index, "mix", text); page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: pressure; inputMethodHints: Qt.ImhDigitsOnly; placeholderText: page.pressureUnit; onEditingFinished: { cylinders.setProperty(index, "pressure", Number(text)); page.generatePlan() } }
				RowLayout { CheckBox { visible: diveMode.currentIndex === 1; text: qsTr("Diluent"); checked: use === 1; onToggled: { cylinders.setProperty(index, "use", checked ? 1 : 0); page.generatePlan() } }
 Components.NeoButton { text: qsTr("Remove"); enabled: cylinders.count > 1; onClicked: { cylinders.remove(index); page.updateGasNames(); page.generatePlan() } } }
			} }
			Text { visible: page.gasReference.length > 0; text: qsTr("Gas reference at pO₂ 1.4 bar"); color: tokens.textSecondary; font.weight: Font.DemiBold }
			Repeater { model: page.gasReference; delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Label { text: modelData.name + " " + modelData.mix; color: tokens.textMuted; Layout.fillWidth: true }
 Label { text: qsTr("MOD %1").arg(modelData.mod); color: tokens.textPrimary }
 Label { text: (Backend.o2narcotic ? qsTr("END %1") : qsTr("EAD %1")).arg(modelData.ead); color: tokens.textPrimary } } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout { Layout.fillWidth: true; Text { text: qsTr("Profile segments"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
 Components.NeoButton { text: qsTr("Add segment"); onClicked: page.addSegment() } }
			Text { text: qsTr("Each row is a depth/time waypoint. Use separate rows for multilevel profiles and gas switches."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Repeater { model: segments; delegate: GridLayout {
				required property int index; required property real depth; required property real duration; required property int gas; required property real setpoint; required property int divemode
				Layout.fillWidth: true; columns: page.width >= 700 ? 6 : 2
				Label { text: qsTr("%1").arg(index + 1); color: tokens.textMuted }
				Components.NeoTextField { Layout.fillWidth: true; text: depth; placeholderText: qsTr("Depth (%1)").arg(page.depthUnit); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { segments.setProperty(index, "depth", Number(text)); page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: duration; placeholderText: qsTr("Minutes"); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { segments.setProperty(index, "duration", Number(text)); page.generatePlan() } }
				Components.NeoComboBox { Layout.fillWidth: true; accessibleName: qsTr("Gas for segment %1").arg(index + 1); model: page.gasNames; currentIndex: gas; onActivated: { segments.setProperty(index, "gas", currentIndex); page.generatePlan() } }
				Components.NeoTextField { visible: diveMode.currentIndex === 1; Layout.fillWidth: true; text: (setpoint / 1000.0).toFixed(2); placeholderText: qsTr("Setpoint bar"); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: { segments.setProperty(index, "setpoint", Math.round(Number(text) * 1000)); page.generatePlan() } }
				Components.NeoButton { text: qsTr("Remove"); enabled: segments.count > 1; onClicked: { segments.remove(index); page.generatePlan() } }
			} }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Calculated profile"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 8 : 2
				Components.MetricCard { label: qsTr("NDL"); value: page.formatDuration(page.finalSampleValue("ndl", -1)); Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("TTS"); value: page.formatDuration(page.finalSampleValue("tts", -1)); Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("Ceiling"); value: page.finalSampleValue("ceiling", 0) > 0 ? (page.finalSampleValue("ceiling", 0) / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) : "—"; suffix: page.finalSampleValue("ceiling", 0) > 0 ? page.depthUnit : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("CNS"); value: page.finalSampleValue("cns", 0) > 0 ? String(page.finalSampleValue("cns", 0)) : "—"; suffix: page.finalSampleValue("cns", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("Current GF"); value: page.finalSampleValue("gf", 0) > 0 ? page.finalSampleValue("gf", 0).toFixed(0) : "—"; suffix: page.finalSampleValue("gf", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("Surface GF"); value: page.finalSampleValue("surfaceGf", 0) > 0 ? page.finalSampleValue("surfaceGf", 0).toFixed(0) : "—"; suffix: page.finalSampleValue("surfaceGf", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("pO₂"); value: page.finalSampleValue("po2", 0) > 0 ? (page.finalSampleValue("po2", 0) / 1000.0).toFixed(2) : "—"; suffix: page.finalSampleValue("po2", 0) > 0 ? "bar" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("Max tissue loading"); value: page.finalSampleValue("tissueLoad", 0) > 0 ? page.finalSampleValue("tissueLoad", 0).toFixed(0) : "—"; suffix: page.finalSampleValue("tissueLoad", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("OTU"); value: page.planOtu > 0 ? String(page.planOtu) : "—"; Layout.fillWidth: true }
			}
			Canvas { id: profileCanvas; Layout.fillWidth: true; Layout.preferredHeight: 190; onPaint: {
				var ctx = getContext("2d"); ctx.reset(); if (page.profileData.length < 2) return
				var maxTime = 0, maxDepth = 0; for (var i = 0; i < page.profileData.length; ++i) { maxTime = Math.max(maxTime, page.profileData[i].time); maxDepth = Math.max(maxDepth, page.profileData[i].depth) }
				if (maxTime <= 0 || maxDepth <= 0) return; var margin = 16, w = width - margin * 2, h = height - margin * 2
				ctx.strokeStyle = page.exceedsNDL ? "#F87171" : tokens.accent; ctx.lineWidth = 2; ctx.beginPath()
				for (var j = 0; j < page.profileData.length; ++j) { var p = page.profileData[j]; var x = margin + p.time / maxTime * w; var y = margin + p.depth / maxDepth * h; if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y) }; ctx.stroke()
				ctx.strokeStyle = "#FB923C"; ctx.lineWidth = 1.5; ctx.beginPath(); var ceilingStarted = false
				for (var k = 0; k < page.profileData.length; ++k) { var ceilingPoint = page.profileData[k]; if (ceilingPoint.ceiling <= 0) { ceilingStarted = false; continue }; var ceilingX = margin + ceilingPoint.time / maxTime * w; var ceilingY = margin + ceilingPoint.ceiling / maxDepth * h; if (!ceilingStarted) { ctx.moveTo(ceilingX, ceilingY); ceilingStarted = true } else ctx.lineTo(ceilingX, ceilingY) }; ctx.stroke()
			}
				Rectangle {
					visible: page.inspectedProfileSample !== null
					x: page.profileSampleX(page.inspectedProfileSample, profileCanvas.width)
					y: 0
					width: 1
					height: parent.height
					color: tokens.textPrimary
					opacity: 0.55
				}
				MouseArea {
					anchors.fill: parent
					hoverEnabled: true
					onPositionChanged: function(mouse) { page.inspectProfileAt(mouse.x, width) }
					onPressed: function(mouse) { page.inspectProfileAt(mouse.x, width) }
					onExited: page.inspectedProfileSample = null
				}
			}
			Connections {
				target: page

				function onProfileDataChanged() {
					page.inspectedProfileSample = null
					profileCanvas.requestPaint()
				}

				function onExceedsNDLChanged() {
					profileCanvas.requestPaint()
				}
			}
			Rectangle {
				visible: page.inspectedProfileSample !== null
				Layout.fillWidth: true
				color: tokens.surface
				radius: 10
				border.color: tokens.border
				implicitHeight: plannerSampleInfo.implicitHeight + tokens.space12 * 2
				ColumnLayout {
					id: plannerSampleInfo
					anchors.fill: parent
					anchors.margins: tokens.space12
					spacing: 2
					Text { text: qsTr("Profile sample at %1  ·  %2 %3").arg(page.formatDuration(page.inspectedProfileSample ? page.inspectedProfileSample.time : 0)).arg(page.inspectedProfileSample ? (page.inspectedProfileSample.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) : "—").arg(page.depthUnit); color: tokens.textPrimary; font.weight: Font.DemiBold }
					Text { text: qsTr("NDL %1   TTS %2   Ceiling %3").arg(page.formatDuration(page.inspectedProfileSample ? page.inspectedProfileSample.ndl : -1)).arg(page.formatDuration(page.inspectedProfileSample ? page.inspectedProfileSample.tts : -1)).arg(page.inspectedProfileSample && page.inspectedProfileSample.ceiling > 0 ? (page.inspectedProfileSample.ceiling / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + page.depthUnit : "—"); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					Text { text: qsTr("GF %1%   Surface GF %2%   pO₂ %3 bar   Tissue %4%").arg(page.inspectedProfileSample && page.inspectedProfileSample.gf !== undefined ? page.inspectedProfileSample.gf.toFixed(0) : "—").arg(page.inspectedProfileSample && page.inspectedProfileSample.surfaceGf !== undefined ? page.inspectedProfileSample.surfaceGf.toFixed(0) : "—").arg(page.inspectedProfileSample && page.inspectedProfileSample.po2 > 0 ? (page.inspectedProfileSample.po2 / 1000.0).toFixed(2) : "—").arg(page.inspectedProfileSample && page.inspectedProfileSample.tissueLoad !== undefined ? page.inspectedProfileSample.tissueLoad.toFixed(0) : "—"); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
					Text { visible: page.inspectedProfileSample && page.inspectedProfileSample.cns > 0; text: qsTr("CNS %1%").arg(page.inspectedProfileSample ? page.inspectedProfileSample.cns : 0); color: tokens.textSecondary }
				}
			}
			RowLayout { Layout.fillWidth: true; spacing: tokens.space8; Rectangle { width: 18; height: 3; color: page.exceedsNDL ? "#F87171" : tokens.accent }
 Text { text: qsTr("Profile"); color: tokens.textSecondary }
 Rectangle { width: 18; height: 3; color: "#FB923C" }
 Text { text: qsTr("Calculated ceiling"); color: tokens.textSecondary }
 Item { Layout.fillWidth: true } }
			Label { visible: page.exceedsNDL; text: qsTr("This recreational plan exceeds the NDL. Review the schedule and warnings before saving."); color: "#F87171"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Label { visible: !page.planSaveAllowed && !page.exceedsNDL; text: qsTr("The planner could not create a valid saveable plan. Correct the gas, bailout, or planner warnings before continuing."); color: "#F87171"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoTextArea { Layout.fillWidth: true; readOnly: true; text: page.planNotes; wrapMode: Text.Wrap; color: tokens.textPrimary; background: null }
			Text { visible: page.schedule.length > 0; text: qsTr("Decompression schedule"); color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
			Repeater { model: page.schedule; delegate: GridLayout {
				required property var modelData
				Layout.fillWidth: true
				columns: page.width >= 700 ? 8 : 2
				Label { text: qsTr("Deco stop"); color: tokens.textMuted; Layout.fillWidth: true }
				Label { text: (modelData.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + page.depthUnit; color: tokens.textPrimary }
				Label { text: qsTr("Stop %1").arg(page.formatDuration(modelData.duration)); color: tokens.textPrimary }
				Label { visible: modelData.gas !== undefined; text: modelData.gas || ""; color: tokens.textSecondary }
				Label { visible: modelData.runTime !== undefined; text: qsTr("Run %1").arg(page.formatDuration(modelData.runTime)); color: tokens.textSecondary }
				Label { visible: modelData.tts !== undefined; text: qsTr("TTS %1").arg(page.formatDuration(modelData.tts)); color: tokens.textSecondary }
				Label { visible: modelData.setpoint !== undefined && modelData.setpoint > 0; text: qsTr("SP %1").arg(page.formatSetpoint(modelData.setpoint)); color: tokens.textSecondary }
				Label { visible: modelData.cns !== undefined && modelData.cns > 0; text: qsTr("CNS %1%").arg(modelData.cns); color: tokens.textSecondary }
			} }
			RowLayout { Layout.fillWidth: true; Button { text: qsTr("Recalculate"); onClicked: page.generatePlan(false, true) }
 Components.NeoButton { text: qsTr("Copy deco slate"); onClicked: manager.copyToClipboard(page.decoSlate()) }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save as TXT"); onClicked: plannerTextFolder.open() }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save as PDF"); onClicked: plannerPdfFolder.open() }
 Item { Layout.fillWidth: true }
 Components.NeoButton { text: qsTr("Save plan"); enabled: page.planSaveAllowed; onClicked: page.generatePlan(true, true) } }
			RowLayout { visible: page.contingencyResult !== null; Layout.fillWidth: true; Text { text: qsTr("Plan package includes the main plan and this selected contingency."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
 Components.NeoButton { text: qsTr("Copy package"); onClicked: manager.copyToClipboard(page.planPackage()) }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Package TXT"); onClicked: planPackageTextFolder.open() }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Package PDF"); onClicked: planPackagePdfFolder.open() } }
			Label { visible: page.plannerTextExport.length > 0; text: qsTr("Planner text saved: %1").arg(page.plannerTextExport); color: tokens.success; wrapMode: Text.Wrap; Layout.fillWidth: true }
			Label { visible: page.plannerPdfExport.length > 0; text: qsTr("Planner PDF saved: %1").arg(page.plannerPdfExport); color: tokens.success; wrapMode: Text.Wrap; Layout.fillWidth: true }
			Label { visible: page.planPackageTextExport.length > 0; text: qsTr("Plan package text saved: %1").arg(page.planPackageTextExport); color: tokens.success; wrapMode: Text.Wrap; Layout.fillWidth: true }
			Label { visible: page.planPackagePdfExport.length > 0; text: qsTr("Plan package PDF saved: %1").arg(page.planPackagePdfExport); color: tokens.success; wrapMode: Text.Wrap; Layout.fillWidth: true }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Gas sufficiency"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Consumption and remaining pressure are produced by Subsurface's planner for this exact profile."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Repeater { model: page.gasAnalysis; delegate: GridLayout {
				required property var modelData
				Layout.fillWidth: true
				columns: page.width >= 700 ? 6 : 2
				Label { text: modelData.mix; color: modelData.belowMinimum || modelData.belowReserve ? "#F87171" : tokens.textPrimary; font.weight: Font.DemiBold; Layout.fillWidth: true }
				Label { text: qsTr("Start %1").arg(modelData.startPressure); color: tokens.textSecondary; Layout.fillWidth: true }
				Label { text: qsTr("Used %1").arg(modelData.used); color: tokens.textSecondary; Layout.fillWidth: true }
				Label { text: qsTr("Deco %1").arg(modelData.decoUsed); color: tokens.textSecondary; Layout.fillWidth: true }
				Label { text: qsTr("Remain %1").arg(modelData.remaining); color: tokens.textPrimary; Layout.fillWidth: true }
				Label { text: modelData.belowMinimum ? qsTr("Insufficient gas") : modelData.belowReserve ? qsTr("Below reserve") : qsTr("End %1").arg(modelData.endPressure); color: modelData.belowMinimum || modelData.belowReserve ? "#F87171" : tokens.success; Layout.fillWidth: true }
			} }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Technical tools"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Use the established gas calculator for MOD, Best Mix, END/EAD, CNS and OTU reference calculations. The NDL reference below runs the active native planner—there is no separate table formula."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoButton { Layout.fillWidth: true; text: qsTr("Open gas calculator"); onClicked: page.openGasTools() }
			Components.NeoButton { Layout.fillWidth: true; text: qsTr("Calculate NDL reference"); onClicked: page.calculateNdlReference() }
			Repeater { model: page.ndlReference; delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Label { text: qsTr("%1 %2").arg(modelData.depth).arg(page.depthUnit); color: tokens.textPrimary; Layout.fillWidth: true }
 Label { text: modelData.available ? page.formatDuration(modelData.ndl) : qsTr("Not available"); color: modelData.available ? tokens.textSecondary : tokens.textMuted } } }
			Text { visible: page.ndlReference.length > 0; text: qsTr("Reference only: uses the current model, gases, water, surface pressure, and one-minute level profile at each shown depth. Review the generated plan before diving."); color: tokens.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { text: qsTr("Unit reference"); color: tokens.textPrimary; font.weight: Font.DemiBold }
			Components.NeoTextField { Layout.fillWidth: true; text: page.conversionValue.toString(); inputMethodHints: Qt.ImhFormattedNumbersOnly; placeholderText: qsTr("Value to convert"); onEditingFinished: { var value = Number(text); if (!isNaN(value)) page.conversionValue = value } }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 3 : 1
				Label { text: qsTr("%1 m = %2 ft").arg(page.conversionValue.toFixed(2)).arg((page.conversionValue * 3.28084).toFixed(2)); color: tokens.textSecondary }
				Label { text: qsTr("%1 bar = %2 psi").arg(page.conversionValue.toFixed(2)).arg((page.conversionValue * 14.5038).toFixed(2)); color: tokens.textSecondary }
				Label { text: qsTr("%1 L = %2 US gal").arg(page.conversionValue.toFixed(2)).arg((page.conversionValue * 0.264172).toFixed(2)); color: tokens.textSecondary }
			}
		}
		Text { text: qsTr("Planning aid only. Confirm the active algorithm, units, gases, environmental assumptions, schedule and warnings before diving."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
