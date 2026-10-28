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
import "../LspSettings.js" as LspSettings

Kirigami.ScrollablePage {
	id: page
	objectName: "NeoPlannerLab"
	title: qsTr("Planner & decompression lab")
	background: Rectangle { color: tokens.background }
	Modern.DesignTokens { id: tokens }
	property bool advancedSettingsExpanded: false
	property bool useLspPlanner: true
	property var lspParameters: ({})
	property var lastLspRequest: ({})
	property real totalPlanCns: -1
	property bool plannerReady: false
	property int observedLengthUnit: Backend.length
	property int observedPressureUnit: Backend.pressure
	property int renderedLengthUnit: Backend.length
	property int renderedPressureUnit: Backend.pressure
	onObservedLengthUnitChanged: {
		if(plannerReady&&renderedLengthUnit!==observedLengthUnit) {
			var factor=observedLengthUnit===Enums.METERS?0.3048:1/0.3048
			for(var i=0;i<segments.count;i++)segments.setProperty(i,"depth",segments.get(i).depth*factor)
		}
		renderedLengthUnit=observedLengthUnit
		if(plannerReady)generatePlan(false,true)
	}
	onObservedPressureUnitChanged: {
		if(plannerReady&&renderedPressureUnit!==observedPressureUnit) {
			var factor=observedPressureUnit===Enums.BAR?1/14.5037738:14.5037738
			for(var i=0;i<cylinders.count;i++)cylinders.setProperty(i,"pressure",cylinders.get(i).pressure*factor)
		}
		renderedPressureUnit=observedPressureUnit
		if(plannerReady)generatePlan(false,true)
	}
	property bool gasToolsExpanded: false
	property int selectedProfileIndex: 0
	property int gasOxygen: 21
	property int gasHelium: 0
	property var gasCalculatorResults: []
	property var profileOptions: {
		var options = []
		for (var builtInIndex = 0; builtInIndex < builtInProfiles.length; ++builtInIndex)
			options.push({ "label": qsTr("%1 v%2").arg(builtInProfiles[builtInIndex].name).arg(builtInProfiles[builtInIndex].version), "kind": "builtIn", "sourceIndex": builtInIndex, "summary": builtInProfiles[builtInIndex].summary })
		var saved = plannerStorage.presets || []
		for (var savedIndex = 0; savedIndex < saved.length; ++savedIndex)
			options.push({ "label": saved[savedIndex].name, "kind": "saved", "sourceIndex": savedIndex, "summary": qsTr("Saved Neo profile") })
		return options
	}
	property var profileLabels: {
		var labels = []
		var options = profileOptions
		for (var i = 0; i < options.length; ++i)
			labels.push(options[i].label)
		return labels
	}
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
	property var analysis: ({})
	property var timeline: []
	property var schedule: []
	property var gasAnalysis: []
	property bool exceedsNDL: false
	property bool planSaveAllowed: false
	property int planOtu: 0
	property int runtimeSeconds: 0
	property int bottomTimeSeconds: 0
	property int decoTimeSeconds: 0
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
	property real plannerReadingWidth: Math.min(page.availableWidth, 1120)
	readonly property real profileChartLeft: 52
	readonly property real profileChartRight: 18
	readonly property real profileChartTop: 12
	readonly property real profileChartBottom: 34
	property string activeProfileName: ""
	property bool activeProfileModified: false
	property string activeProfileId: ""
	property int activeProfileVersion: 0
	property bool activeProfileBuiltIn: false
    property var builtInProfiles: {
        var definitions=[{id:"subsurface-native",name:qsTr("Subsurface native"),version:1,engine:"subsurface",summary:qsTr("Existing Subsurface planner and settings")}]
        var source=JSON.parse(Backend.divePlannerPointsModel.lspPlannerPresetsJson()),ids=["lsp-default","lsp-multideco","lsp-abysner","lsp-subsurface","lsp-gue","lsp-divekit"]
        for(var i=0;i<source.length;i++)definitions.push({id:ids[i],name:source[i].name,version:2,engine:"lsp",summary:source[i].summary,parameters:source[i].values})
        return definitions
    }
	ListModel { id: cylinders }
	ListModel { id: segments }
	Settings { id: plannerStorage; category: "subsurface-neo/planner"; property var presets: [] }
	FolderDialog {
		id: plannerTextFolder
		currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
		onAccepted: page.plannerTextExport = manager.exportNeoPlannerText(selectedFolder, page.fullPlanExport())
	}
	FolderDialog { id: planPackageTextFolder; currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation); onAccepted: page.planPackageTextExport = manager.exportNeoPlannerText(selectedFolder, page.planPackage()) }
	FolderDialog { id: planPackagePdfFolder; currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation); onAccepted: page.planPackagePdfExport = manager.exportNeoPlannerPdf(selectedFolder, page.planPackage(), page.profileData) }
	FolderDialog {
		id: plannerPdfFolder
		currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
		onAccepted: page.plannerPdfExport = manager.exportNeoPlannerPdf(selectedFolder, page.fullPlanExport(), page.profileData)
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
		var preset = { "schema":2,"engine":useLspPlanner?"lsp":"subsurface","lspParameters":JSON.parse(JSON.stringify(lspParameters)),"units":{length:Backend.length,pressure:Backend.pressure}, "name": name.trim(), "cylinders": modelData(cylinders), "segments": modelData(segments), "diveMode": diveMode.currentIndex, "waterType": waterType.currentIndex, "customSalinity": customSalinity, "plannedDate": plannedDate, "plannedTime": plannedTime, "surfacePressureBar": surfacePressureBar,
			"decoMode": page.plannerValue("planner_deco_mode"), "gflow": page.plannerValue("planner_gflow"), "gfhigh": page.plannerValue("planner_gfhigh"), "vpmbConservatism": page.plannerValue("vpmb_conservatism"),
			"bottomSac": page.plannerValue("bottomsac"), "decoSac": page.plannerValue("decosac"), "reserveGas": page.plannerValue("reserve_gas"), "bottomPo2": page.plannerValue("bottompo2") / 100, "decoPo2": page.plannerValue("decopo2") / 100,
			"descentRate": page.plannerValue("descrate"), "deepAscentRate": page.plannerValue("ascrate75"), "midAscentRate": page.plannerValue("ascrate50"), "decoAscentRate": page.plannerValue("ascratestops"), "finalAscentRate": page.plannerValue("ascratelast6m"),
			"dropToFirstDepth": page.plannerValue("drop_stone_mode"), "lastStop6m": page.plannerValue("last_stop6m"), "switchAtRequiredStop": page.plannerValue("switch_at_req_stop"), "minSwitchDuration": page.plannerValue("min_switch_duration"),
			"surfaceSegment": page.plannerValue("surface_segment"), "problemSolvingTime": page.plannerValue("problemsolvingtime"), "backGasBreaks": page.plannerValue("doo2breaks"), "bailout": page.plannerValue("dobailout"),
			"defaultSetpoint": page.plannerValue("default_setpoint"), "o2Narcotic": page.plannerValue("o2narcotic"), "sacFactor": page.plannerValue("sacfactor") }
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

    function activeProfileLimitations() { return "" }
    function profileExportIdentity() {
        var lines=[qsTr("Profile: %1").arg(profileLabel()),useLspPlanner?qsTr("Engine: LSP D-Planner+ %1").arg(Backend.divePlannerPointsModel.lspPlannerIdentity().version):qsTr("Engine: Subsurface native")]
        if(useLspPlanner&&lastLspRequest.schema===1)lines.push(qsTr("Effective LSP calculation: %1").arg(JSON.stringify(lastLspRequest)))
        return lines
    }
    function plannerValue(name) {
        if(!useLspPlanner)return Backend[name]
        var value=LspSettings.read(name,lspParameters,Backend.length===Enums.METERS,Backend.pressure===Enums.BAR,Backend.volume===Enums.LITER,Enums)
        return value===undefined?Backend[name]:value
    }
    function setLspParameter(key,value) {
        var values=JSON.parse(JSON.stringify(lspParameters));values[key]=value;lspParameters=values
    }
    function setPlannerValue(name,value) {
        if(!useLspPlanner){Backend[name]=value;return}
        lspParameters=LspSettings.write(name,value,lspParameters,Backend.length===Enums.METERS,Backend.pressure===Enums.BAR,Backend.volume===Enums.LITER,Enums)
        if(name==="default_setpoint")for(var i=0;i<segments.count;i++)segments.setProperty(i,"setpoint",Math.round(Number(value)))
    }
    function calculateEnginePlan(cylinderData,segmentData,savePlan) {
        if(useLspPlanner) {
            for(var i=0;i<segmentData.length;i++)segmentData[i].divemode=diveMode.currentIndex
            return Backend.divePlannerPointsModel.calculateNeoLspPlan({date:plannedDate,time:plannedTime,diveMode:diveMode.currentIndex,waterType:waterType.currentIndex,customSalinity:customSalinity,surfacePressureBar:surfacePressureBar,cylinders:cylinderData,segments:segmentData,lspParameters:lspParameters,profileId:activeProfileId,profileVersion:activeProfileVersion},savePlan===true)
        }
        var salinity=waterType.currentIndex===0?10300:waterType.currentIndex===1?10000:waterType.currentIndex===2?10200:customSalinity
        return Backend.divePlannerPointsModel.calculatePlan(cylinderData,segmentData,plannedDate,plannedTime,diveMode.currentIndex,salinity,Math.round(surfacePressureBar*1000),savePlan===true)
    }
	function applyPlannerSettings(preset) {
		if (preset.decoMode !== undefined) page.setPlannerValue("planner_deco_mode", preset.decoMode)
		if (preset.gflow !== undefined) page.setPlannerValue("planner_gflow", preset.gflow)
		if (preset.gfhigh !== undefined) page.setPlannerValue("planner_gfhigh", preset.gfhigh)
		if (preset.vpmbConservatism !== undefined) page.setPlannerValue("vpmb_conservatism", preset.vpmbConservatism)
		if (preset.bottomSac !== undefined) page.setPlannerValue("bottomsac", preset.bottomSac)
		if (preset.decoSac !== undefined) page.setPlannerValue("decosac", preset.decoSac)
		if (preset.reserveGas !== undefined) page.setPlannerValue("reserve_gas", preset.reserveGas)
		if (preset.bottomPo2 !== undefined) page.setPlannerValue("bottompo2", preset.bottomPo2)
		if (preset.decoPo2 !== undefined) page.setPlannerValue("decopo2", preset.decoPo2)
		if (preset.descentRate !== undefined) page.setPlannerValue("descrate", preset.descentRate)
		if (preset.deepAscentRate !== undefined) page.setPlannerValue("ascrate75", preset.deepAscentRate)
		if (preset.midAscentRate !== undefined) page.setPlannerValue("ascrate50", preset.midAscentRate)
		if (preset.decoAscentRate !== undefined) page.setPlannerValue("ascratestops", preset.decoAscentRate)
		if (preset.finalAscentRate !== undefined) page.setPlannerValue("ascratelast6m", preset.finalAscentRate)
		if (preset.dropToFirstDepth !== undefined) page.setPlannerValue("drop_stone_mode", preset.dropToFirstDepth)
		if (preset.lastStop6m !== undefined) page.setPlannerValue("last_stop6m", preset.lastStop6m)
		if (preset.switchAtRequiredStop !== undefined) page.setPlannerValue("switch_at_req_stop", preset.switchAtRequiredStop)
		if (preset.minSwitchDuration !== undefined) page.setPlannerValue("min_switch_duration", preset.minSwitchDuration)
		if (preset.surfaceSegment !== undefined) page.setPlannerValue("surface_segment", preset.surfaceSegment)
		if (preset.problemSolvingTime !== undefined) page.setPlannerValue("problemsolvingtime", preset.problemSolvingTime)
		if (preset.backGasBreaks !== undefined) page.setPlannerValue("doo2breaks", preset.backGasBreaks)
		if (preset.bailout !== undefined) page.setPlannerValue("dobailout", preset.bailout)
		if (preset.defaultSetpoint !== undefined) page.setPlannerValue("default_setpoint", preset.defaultSetpoint)
		if (preset.o2Narcotic !== undefined) page.setPlannerValue("o2narcotic", preset.o2Narcotic)
		if (preset.sacFactor !== undefined) page.setPlannerValue("sacfactor", preset.sacFactor)
	}
	function loadBuiltInProfile(index) {
		var profile = builtInProfiles[index]
		if (!profile)
			return
        useLspPlanner=profile.engine==="lsp"
        if(useLspPlanner) {
            lspParameters=LspSettings.presetParameters(profile.parameters)
            for(var j=0;j<segments.count;j++)segments.setProperty(j,"setpoint",Math.round(lspParameters.bottomSetpoint*1000))
        } else applyPlannerSettings(nativePlannerSettings())
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
        useLspPlanner=preset.engine==="lsp"
        if(useLspPlanner)lspParameters=JSON.parse(JSON.stringify(preset.lspParameters))
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
        if(!useLspPlanner)applyPlannerSettings(preset)
        if(preset.units) {
            if(preset.units.length!==Backend.length)for(var si=0;si<segments.count;si++)segments.setProperty(si,"depth",segments.get(si).depth*(Backend.length===Enums.METERS?0.3048:1/0.3048))
            if(preset.units.pressure!==Backend.pressure)for(var ci=0;ci<cylinders.count;ci++)cylinders.setProperty(ci,"pressure",cylinders.get(ci).pressure*(Backend.pressure===Enums.BAR?1/14.5037738:14.5037738))
        }
		activeProfileName = preset.name
		activeProfileId = ""
		activeProfileVersion = 0
		activeProfileBuiltIn = false
		activeProfileModified = false
		generatePlan(false, true)
	}
	function loadSelectedProfile() {
		var option = profileOptions[selectedProfileIndex]
		if (!option)
			return
		if (option.kind === "builtIn")
			loadBuiltInProfile(option.sourceIndex)
		else
			loadPreset(option.sourceIndex)
	}
	function selectedSavedProfile() {
		var option = profileOptions[selectedProfileIndex]
		return option && option.kind === "saved" ? option : null
	}
	function calculateGasTools() {
		gasHelium = Math.min(gasHelium, 100 - gasOxygen)
		var cylinderType = cylinders.count > 0 ? cylinders.get(0).type : (PrefEquipment.default_cylinder || "AL80")
		gasCalculatorResults = Backend.divePlannerPointsModel.calculateGasInfo(cylinderType, gasOxygen * 10, gasHelium * 10)
	}

	function updateGasNames() {
		var names = []
		for (var i = 0; i < cylinders.count; ++i)
			names.push(qsTr("Gas %1").arg(i + 1))
		gasNames = names
	}
	function mixOxygen(mix) {
		var value = Number(String(mix).split("/")[0])
		return isNaN(value) ? 21 : Math.max(0, Math.min(100, Math.round(value)))
	}
	function mixHelium(mix) {
		var parts = String(mix).split("/")
		var value = Number(parts.length > 1 ? parts[1] : 0)
		return isNaN(value) ? 0 : Math.max(0, Math.min(100 - mixOxygen(mix), Math.round(value)))
	}
	function setCylinderMix(index, oxygen, helium) {
		var safeOxygen = Math.max(0, Math.min(100, Math.round(oxygen)))
		var safeHelium = Math.max(0, Math.min(100 - safeOxygen, Math.round(helium)))
		cylinders.setProperty(index, "mix", safeOxygen + "/" + safeHelium)
		generatePlan()
	}
	function gasDisplayLabel(mix) {
		var oxygen = mixOxygen(mix)
		var helium = mixHelium(mix)
		if (oxygen === 21 && helium === 0) return "Air"
		if (oxygen === 100 && helium === 0) return "100%"
		return oxygen + "/" + helium
	}
	function gasInfoAt(values, po2Bar) {
		var index = Math.max(0, Math.min(values.length - 1, Math.round((po2Bar - 1.0) * 10)))
		return values.length > 0 ? values[index] : ({})
	}
	function updateGasReference(calculation) {
        if(useLspPlanner&&calculation) {
            var references=[],gases=calculation.gasAnalysis||[]
            for(var n=0;n<gases.length;n++)references.push({name:qsTr("Gas %1").arg(n+1),mix:gases[n].mix,bottomMod:gases[n].bottomMod||"—",decoSwitch:gases[n].decoSwitch||"—",ead:"—"})
            gasReference=references;return
        }
		var references = []
		for (var i = 0; i < cylinders.count; ++i) {
			var cylinder = cylinders.get(i)
			var parts = String(cylinder.mix).split("/")
			var parsedO2 = Number(parts[0])
			var o2 = isNaN(parsedO2) ? 21 : Math.max(0, Math.min(100, parsedO2))
			var parsedHe = Number(parts.length > 1 ? parts[1] : 0)
			var he = isNaN(parsedHe) ? 0 : Math.max(0, Math.min(100 - o2, parsedHe))
			var values = Backend.divePlannerPointsModel.calculateGasInfo(cylinder.type, Math.round(o2 * 10), Math.round(he * 10))
			var bottomReference = gasInfoAt(values, page.plannerValue("bottompo2") / 100.0)
			var decoReference = gasInfoAt(values, page.plannerValue("decopo2") / 100.0)
			references.push({ "name": qsTr("Gas %1").arg(i + 1), "mix": o2 + "/" + he,
				"bottomMod": bottomReference.mod || "—", "decoSwitch": decoReference.decoSwitch || decoReference.mod || "—", "ead": bottomReference.ead || "—" })
		}
		gasReference = references
	}
	function calculateNdlReference() {
		var depths = Backend.length === Enums.METERS ? [12, 18, 24, 30, 36, 42] : [40, 60, 80, 100, 120, 140]
		if(useLspPlanner) {
			var metres=[];for(var di=0;di<depths.length;di++)metres.push(depths[di]*(Backend.length===Enums.METERS?1:0.3048))
			var response=Backend.divePlannerPointsModel.calculateLspNdlReference({request:lastLspRequest,depthsMetres:metres})
			var rows=response.ok?response.result.rows:[],output=[]
			for(var ri=0;ri<depths.length;ri++)output.push({depth:depths[ri],ndl:rows[ri]?rows[ri].ndl:-1,available:rows[ri]?rows[ri].available:false,capped:rows[ri]?rows[ri].capped:false})
			ndlReference=output;return
		}
		var cylinderData = modelData(cylinders)
		var salinity = waterType.currentIndex === 0 ? 10300 : waterType.currentIndex === 1 ? 10000 : waterType.currentIndex === 2 ? 10200 : customSalinity
		var table = []
		for (var i = 0; i < depths.length; ++i) {
			var segment = { "depth": depths[i], "duration": 1, "gas": 0, "setpoint": diveMode.currentIndex === 1 ? page.plannerValue("default_setpoint") : 0, "divemode": diveMode.currentIndex }
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
			"duration": last ? last.duration + 10 : 10, "gas": last ? last.gas : 0, "setpoint": last ? last.setpoint : page.plannerValue("default_setpoint"),
			"divemode": last ? last.divemode : 0 })
		generatePlan()
	}
	function selectDiveMode(mode) {
		diveMode.currentIndex = mode
		if (useLspPlanner && cylinders.count > 0) {
			var first = cylinders.get(0)
			if (mode === 1 && first.use === 0) cylinders.setProperty(0, "use", 1)
			if (mode !== 1 && first.use === 1) cylinders.setProperty(0, "use", 0)
			for (var i = 0; i < segments.count; ++i) {
				segments.setProperty(i, "divemode", mode)
				segments.setProperty(i, "setpoint", mode === 1 ? page.plannerValue("default_setpoint") : 0)
			}
		}
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
				"use": useLspPlanner || diveMode.currentIndex === 1 ? cylinder.use : 0 })
		}
		var segmentData = []
		for (var j = 0; j < segments.count; ++j) {
			var segment = segments.get(j)
			segmentData.push({ "depth": segment.depth, "duration": segment.duration, "gas": segment.gas,
				"setpoint": segment.setpoint, "divemode": diveMode.currentIndex === 2 ? 2 : segment.divemode })
		}
		var salinity = waterType.currentIndex === 0 ? 10300 : waterType.currentIndex === 1 ? 10000 : waterType.currentIndex === 2 ? 10200 : customSalinity
        var result=calculateEnginePlan(cylinderData,segmentData,savePlan)
        lastLspRequest=useLspPlanner?(result.engineRequest||{}):({})
        totalPlanCns=useLspPlanner&&result.cns!==undefined?result.cns:-1
		planNotes = result.notes || ""
		updateGasReference(result)
		profileData = result.profile || []
		analysis = result.analysis || ({})
		timeline = result.timeline || []
		schedule = result.schedule || []
		gasAnalysis = result.gasAnalysis || []
		exceedsNDL = result.exceedsNDL === true
		planSaveAllowed = result.planSaveAllowed === true
		planOtu = result.otu || 0
		runtimeSeconds = result.runtimeSeconds || 0
		bottomTimeSeconds = result.bottomTimeSeconds || 0
		decoTimeSeconds = result.decoTimeSeconds || 0
		if (savePlan === true && result.newDiveId !== undefined && result.newDiveId !== -1) {
			manager.selectDive(result.newDiveId)
			showPageFromDrawer(modernPlansList)
		}
	}
	function calculateContingency() {
		if (cylinders.count === 0 || segments.count === 0)
			return
		var cylinderData = []
		for (var i = 0; i < cylinders.count; ++i) {
			var cylinder = cylinders.get(i)
			cylinderData.push({ "type": cylinder.type, "mix": cylinder.mix, "pressure": cylinder.pressure,
				"use": useLspPlanner || diveMode.currentIndex === 1 ? cylinder.use : 0 })
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
        contingencyResult=calculateEnginePlan(cylinderData,segmentData,false)
		generatePlan(false, true)
	}
	function contingencyName() { return contingencyScenario === 0 ? qsTr("Extra bottom time (+%1 min)").arg(contingencyDelta) : contingencyScenario === 1 ? qsTr("Deeper profile (+%1 %2)").arg(contingencyDelta).arg(depthUnit) : qsTr("Lost gas: %1").arg(gasNames[contingencyGasIndex] || qsTr("selected gas")) }
	function contingencySlate() {
		if (!contingencyResult) return ""
		var modelSettings = page.plannerValue("planner_deco_mode") === Enums.BUEHLMANN ? qsTr("GF %1/%2").arg(page.plannerValue("planner_gflow")).arg(page.plannerValue("planner_gfhigh")) : page.plannerValue("planner_deco_mode") === Enums.VPMB ? qsTr("Conservatism %1").arg(page.plannerValue("vpmb_conservatism")) : qsTr("NDL planning")
		var lines = [qsTr("SUBSURFACE NEO CONTINGENCY PLAN"), qsTr("Scenario: %1").arg(contingencyName()), qsTr("This is a separately calculated contingency; it does not replace the main plan."), qsTr("Model: %1 — %2").arg(algorithmName()).arg(modelSettings), qsTr("Surface pressure: %1 bar · Altitude: %2 m").arg(surfacePressureBar.toFixed(3)).arg(manager.plannerAltitudeForSurfacePressure(surfacePressureBar).toFixed(0)), qsTr("Mode: %1 · Water: %2").arg(diveMode.currentText).arg(waterDescription()), "", qsTr("DECOMPRESSION SCHEDULE")]
		var stops = contingencyResult.schedule || []
		for (var i = 0; i < stops.length; ++i) { var stop = stops[i]; lines.push((stop.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + depthUnit + "  " + formatDuration(stop.duration) + (stop.gas !== undefined ? "  " + stop.gas : "")) }
		var gases = contingencyResult.gasAnalysis || []
		lines.push("", qsTr("GAS STATUS"))
		for (var j = 0; j < gases.length; ++j) lines.push(qsTr("%1: remaining %2; end %3").arg(gases[j].mix).arg(gases[j].remaining).arg(gases[j].endPressure))
		var contingencyWarnings = []
		if (contingencyResult.planSaveAllowed !== true)
			contingencyWarnings.push(qsTr("The selected planner could not create a valid saveable contingency plan."))
		for (var gasIndex = 0; gasIndex < gases.length; ++gasIndex) {
			if (gases[gasIndex].belowMinimum)
				contingencyWarnings.push(qsTr("%1 has insufficient gas.").arg(gases[gasIndex].mix))
			else if (gases[gasIndex].belowReserve)
				contingencyWarnings.push(qsTr("%1 finishes below the configured reserve.").arg(gases[gasIndex].mix))
		}
		if (contingencyWarnings.length > 0)
			lines.push("", qsTr("PLANNER WARNINGS"), contingencyWarnings.join("\n"))
		lines.push("", qsTr("Planning aid only. Review all settings, gases, schedule and warnings before diving."))
		return lines.join("\n")
	}
	function planPackage() {
		var mainPlan = fullPlanExport()
		if (!contingencyResult)
			return mainPlan
		return mainPlan + "\n\n" + "=".repeat(64) + "\n\n" + contingencySlate()
	}
	function finalSampleValue(name, fallback) {
		if(useLspPlanner&&name==="cns"&&totalPlanCns>=0)return totalPlanCns
        return analysis[name] !== undefined ? analysis[name] : fallback
	}
	function surfacedSampleValue(name, fallback) {
		if (profileData.length === 0)
			return fallback
		var sample = profileData[profileData.length - 1]
		return sample[name] !== undefined ? sample[name] : fallback
	}
	function profileMaxTime() {
		var maximum = 0
		for (var i = 0; i < profileData.length; ++i)
			maximum = Math.max(maximum, profileData[i].time)
		return maximum
	}
	function inspectProfileAt(x, profileWidth) {
		if (profileData.length === 0 || profileWidth <= profileChartLeft + profileChartRight)
			return
		var targetTime = Math.max(0, Math.min(profileMaxTime(), (x - profileChartLeft) / (profileWidth - profileChartLeft - profileChartRight) * profileMaxTime()))
		var nearest = profileData[0]
		for (var i = 1; i < profileData.length; ++i) {
			if (Math.abs(profileData[i].time - targetTime) < Math.abs(nearest.time - targetTime))
				nearest = profileData[i]
		}
		inspectedProfileSample = nearest
	}
	function profileSampleX(sample, profileWidth) {
		return sample && profileMaxTime() > 0 ? profileChartLeft + sample.time / profileMaxTime() * (profileWidth - profileChartLeft - profileChartRight) : 0
	}
	function profileSampleAtTime(seconds) {
		if (profileData.length === 0)
			return null
		var nearest = profileData[0]
		for (var i = 1; i < profileData.length; ++i) {
			if (Math.abs(profileData[i].time - seconds) < Math.abs(nearest.time - seconds))
				nearest = profileData[i]
		}
		return nearest
	}
	function hasCalculatedCeiling() {
		for (var i = 0; i < profileData.length; ++i) {
			if (profileData[i].ceiling > 0)
				return true
		}
		return false
	}
	function formatDuration(seconds) {
		if (seconds === undefined || seconds < 0)
			return "—"
		return Math.floor(seconds / 60) + qsTr(" min") + (seconds % 60 ? " " + (seconds % 60) + qsTr(" s") : "")
	}
	function formatPlanDuration(seconds) {
		if (seconds === undefined || seconds < 0)
			return "—"
		var remainder = seconds % 60
		return Math.floor(seconds / 60) + ":" + (remainder < 10 ? "0" : "") + remainder
	}
	function algorithmName() {
		if(useLspPlanner&&!lspParameters.recreational&&String(lspParameters.model||"").indexOf("VPM")===0)
			return {VPMA:"VPM-A",VPMB:"VPM-B",VPMBE:"VPM-B/E",VPMB_GFS:"VPM-B+GFS",VPMBFBO:"VPM-B/FBO"}[lspParameters.model]
		if (page.plannerValue("planner_deco_mode") === Enums.VPMB)
			return qsTr("VPM-B")
		if (page.plannerValue("planner_deco_mode") === Enums.RECREATIONAL)
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
	function phaseLabel(phase) {
		if (phase === "descent") return qsTr("Descend to")
		if (phase === "ascent") return qsTr("Ascend to")
		if (phase === "level") return qsTr("Stay at")
		if (phase === "deco") return qsTr("Deco stop")
        if(phase==="safety")return qsTr("Safety stop")
		if (phase === "switch") return qsTr("Gas switch")
		return qsTr("Surface")
	}
	function phaseSymbol(phase) {
		if (phase === "descent") return "↓"
		if (phase === "ascent") return "↑"
		if (phase === "switch") return "●"
		if (phase === "level") return "●"
		if (phase === "deco") return "●"
		return "↑"
	}
	function timelineLine(row) {
		var sample = profileSampleAtTime(row.runTime)
		var depth = (row.depth / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + depthUnit
		var action = phaseSymbol(row.phase) + " " + depth
		var gas = (row.gas || "—").toUpperCase()
		if (row.gasSwitch)
			gas = ">> " + gas
		var po2 = sample && sample.po2 > 0 ? (sample.po2 / 1000.0).toFixed(2) : "—"
		var ead = sample && sample.ead !== undefined && sample.ead >= 0 ? (sample.ead / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + depthUnit : "—"
		return action + "  ·  " + formatPlanDuration(row.duration) + "  ·  " + gas + "  ·  RT " + formatPlanDuration(row.runTime) + "  ·  pO₂ " + po2 + "  ·  EAD " + ead + (row.setpoint > 0 ? "  ·  SP " + formatSetpoint(row.setpoint) : "")
	}
	function plannerWarningLines() {
		var warnings = []
		if (exceedsNDL)
			warnings.push(qsTr("This recreational plan exceeds the no-decompression limit."))
		else if (!planSaveAllowed)
			warnings.push(qsTr("The selected planner could not create a valid saveable plan."))
		for (var i = 0; i < gasAnalysis.length; ++i) {
			var gas = gasAnalysis[i]
			if (gas.belowMinimum)
				warnings.push(qsTr("%1 has insufficient gas.").arg(gas.mix))
			else if (gas.belowReserve)
				warnings.push(qsTr("%1 finishes below the configured reserve.").arg(gas.mix))
		}
		return warnings
	}
	function waterDescription() {
		return waterType.currentIndex === 3 ? qsTr("Custom (%1 kg/10,000 L)").arg(customSalinity) : waterType.currentText
	}
	function fullPlanExport() {
		var modelSettings = page.plannerValue("planner_deco_mode") === Enums.BUEHLMANN ? qsTr("GF %1/%2").arg(page.plannerValue("planner_gflow")).arg(page.plannerValue("planner_gfhigh")) : page.plannerValue("planner_deco_mode") === Enums.VPMB ? qsTr("Conservatism %1").arg(page.plannerValue("vpmb_conservatism")) : qsTr("NDL planning")
		var lines = [qsTr("SUBSURFACE NEO DIVE PLAN"), qsTr("Planned start: %1 %2").arg(plannedDate).arg(plannedTime), qsTr("Surface pressure: %1 bar").arg(surfacePressureBar.toFixed(3)), qsTr("Model: %1 — %2").arg(algorithmName()).arg(modelSettings)].concat(profileExportIdentity(), [qsTr("Mode: %1").arg(diveMode.currentText), qsTr("Water: %1").arg(waterDescription()), qsTr("Bottom/deco SAC: %1 / %2 %3").arg(sacText(page.plannerValue("bottomsac"))).arg(sacText(page.plannerValue("decosac"))).arg(sacUnit), qsTr("Reserve: %1 %2").arg(page.plannerValue("reserve_gas")).arg(pressureUnit), "", qsTr("GASES")])
		lines.splice(3, 0, qsTr("Altitude: %1 m").arg(manager.plannerAltitudeForSurfacePressure(surfacePressureBar).toFixed(0)))
		for (var gasIndex = 0; gasIndex < cylinders.count; ++gasIndex) {
			var cylinder = cylinders.get(gasIndex)
			lines.push(qsTr("Gas %1: %2 — %3, %4 %5").arg(gasIndex + 1).arg(gasDisplayLabel(cylinder.mix)).arg(cylinder.type).arg(cylinder.pressure).arg(pressureUnit))
		}
		for (var analysisIndex = 0; analysisIndex < gasAnalysis.length; ++analysisIndex) {
			var gas = gasAnalysis[analysisIndex]
			lines.push(qsTr("%1: used %2; remaining %3; end %4").arg(gas.mix).arg(gas.used).arg(gas.remaining).arg(gas.endPressure))
		}
		lines.push("", qsTr("CALCULATED ANALYSIS"))
		lines.push(qsTr("Runtime: %1  Bottom profile: %2  Decompression stops: %3").arg(formatDuration(runtimeSeconds)).arg(formatDuration(bottomTimeSeconds)).arg(formatDuration(decoTimeSeconds)))
		lines.push(qsTr("NDL: %1  Surface GF: %2  Tissue: %3").arg(formatDuration(finalSampleValue("ndl", -1))).arg(surfacedSampleValue("surfaceGf", -1)>=0?surfacedSampleValue("surfaceGf", -1).toFixed(0)+"%":"—").arg(finalSampleValue("tissueLoad", -1)>=0?finalSampleValue("tissueLoad", -1).toFixed(0)+"%":"—"))
		lines.push(qsTr("CNS: %1%  OTU: %2").arg(finalSampleValue("cns", 0).toFixed(1)).arg(planOtu), "", qsTr("FULL PLAN"))
		if (timeline.length === 0)
			lines.push(qsTr("No plan segments generated."))
		for (var timelineIndex = 0; timelineIndex < timeline.length; ++timelineIndex)
			lines.push(timelineLine(timeline[timelineIndex]))
		lines.push("", qsTr("DECOMPRESSION STOPS"))
		if (schedule.length === 0)
			lines.push(qsTr("No decompression stops generated."))
		for (var i = 0; i < schedule.length; ++i) {
			var stop = schedule[i]
			lines.push(timelineLine(stop))
		}
		var warnings = plannerWarningLines()
		if (warnings.length > 0)
			lines.push("", qsTr("PLANNER WARNINGS"), warnings.join("\n"))
		lines.push("", qsTr("Planning aid only. Review all settings, gases, schedule and warnings before diving."))
		return lines.join("\n")
	}
	function decoSlate() {
		var lines = [qsTr("DIVE PLAN")]
		if (timeline.length === 0)
			lines.push(qsTr("No plan segments generated."))
		for (var i = 0; i < timeline.length; ++i)
			lines.push(timelineLine(timeline[i]))
		return lines.join("\n")
	}
    Component.onCompleted: {
        renderedLengthUnit=Backend.length;renderedPressureUnit=Backend.pressure
        loadBuiltInProfile(1);selectedProfileIndex=1
        addCylinder()
        segments.append({depth:Backend.length===Enums.METERS?18:60,duration:30,gas:0,setpoint:plannerValue("default_setpoint"),divemode:0})
        plannerReady=true;generatePlan(false,true)
    }

	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Build a validated dive plan"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
		Text { text: useLspPlanner?qsTr("LSP D-Planner+ calculates this profile with its recorded settings. Runtime entries remain the end of each level."):qsTr("Subsurface calculates this profile with its established native settings."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Decompression model"); color: tokens.textMuted; font.pixelSize: 11 }
			Text { text: page.algorithmName(); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				Components.NeoComboBox { id: decoAlgorithm; Layout.fillWidth: true; accessibleName: qsTr("Decompression model"); model: [qsTr("Recreational (NDL)"), qsTr("Buhlmann ZHL-16C + GF"), qsTr("VPM-B")]; currentIndex: page.plannerValue("planner_deco_mode") === Enums.VPMB ? 2 : page.plannerValue("planner_deco_mode") === Enums.BUEHLMANN ? 1 : 0; onActivated: { page.setPlannerValue("planner_deco_mode", currentIndex === 2 ? Enums.VPMB : currentIndex === 1 ? Enums.BUEHLMANN : Enums.RECREATIONAL); page.generatePlan() } }
				Text { text: useLspPlanner?qsTr("Calculation source: LSP D-Planner+ 3.2.0"):qsTr("Calculation source: Subsurface native"); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
				RowLayout { visible: page.plannerValue("planner_deco_mode") === Enums.BUEHLMANN; Layout.fillWidth: true; Label { text: qsTr("GF low"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("GF low"); from: 1; to: page.useLspPlanner ? 100 : 150; value: page.plannerValue("planner_gflow"); onValueModified: { page.setPlannerValue("planner_gflow", value); page.generatePlan() } }
 Label { text: qsTr("GF high"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("GF high"); from: 1; to: page.useLspPlanner ? 100 : 150; value: page.plannerValue("planner_gfhigh"); onValueModified: { page.setPlannerValue("planner_gfhigh", value); page.generatePlan() } } }
				RowLayout { visible: page.plannerValue("planner_deco_mode") === Enums.VPMB; Layout.fillWidth: true; Label { text: qsTr("VPM-B conservatism"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("VPM-B conservatism"); from: 0; to: 4; value: page.plannerValue("vpmb_conservatism"); onValueModified: { page.setPlannerValue("vpmb_conservatism", value); page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Bottom SAC (%1)").arg(page.sacUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoTextField { Layout.preferredWidth: 86; accessibleName: qsTr("Bottom SAC (%1)").arg(page.sacUnit); text: page.sacText(page.plannerValue("bottomsac")); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: { page.setPlannerValue("bottomsac", page.sacValue(text)); page.generatePlan() } }
 Label { text: qsTr("Deco SAC (%1)").arg(page.sacUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoTextField { Layout.preferredWidth: 86; accessibleName: qsTr("Deco SAC (%1)").arg(page.sacUnit); text: page.sacText(page.plannerValue("decosac")); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: { page.setPlannerValue("decosac", page.sacValue(text)); page.generatePlan() } } }
				Text { text: qsTr("Active settings: %1").arg(page.plannerValue("planner_deco_mode") === Enums.BUEHLMANN ? qsTr("GF %1/%2").arg(page.plannerValue("planner_gflow")).arg(page.plannerValue("planner_gfhigh")) : page.plannerValue("planner_deco_mode") === Enums.VPMB ? qsTr("Conservatism %1").arg(page.plannerValue("vpmb_conservatism")) : qsTr("NDL planning")); color: tokens.textPrimary }
				Text { text: qsTr("Bottom/deco SAC: %1 / %2 %3").arg(page.sacText(page.plannerValue("bottomsac"))).arg(page.sacText(page.plannerValue("decosac"))).arg(page.sacUnit); color: tokens.textPrimary }
				Components.NeoButton { text: page.advancedSettingsExpanded ? qsTr("Hide planner settings") : qsTr("More planner settings"); onClicked: page.advancedSettingsExpanded = !page.advancedSettingsExpanded }
			}
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Profile presets"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Active profile: %1").arg(page.profileLabel()); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { text: qsTr("LSP presets apply their complete original parameters. Existing native profiles keep the Subsurface engine."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoButton { visible: page.activeProfileModified; Layout.fillWidth: true; text: qsTr("Reset to %1").arg(page.activeProfileName); onClicked: page.resetActiveProfile() }
			RowLayout {
				Layout.fillWidth: true
				Components.NeoComboBox { id: profilePresetBox; Layout.fillWidth: true; accessibleName: qsTr("Profile preset"); model: page.profileLabels; currentIndex: page.selectedProfileIndex; onActivated: page.selectedProfileIndex = currentIndex }
				Components.NeoButton { text: qsTr("Load"); enabled: profilePresetBox.currentIndex >= 0; onClicked: page.loadSelectedProfile() }
			}
			Text { Layout.fillWidth: true; text: page.profileOptions[page.selectedProfileIndex] ? page.profileOptions[page.selectedProfileIndex].summary : ""; color: tokens.textSecondary; wrapMode: Text.WordWrap }
			Text { text: qsTr("Save or manage my profiles"); color: tokens.textMuted; font.weight: Font.DemiBold; Layout.fillWidth: true }
			RowLayout { Layout.fillWidth: true; Components.NeoTextField { id: presetName; Layout.fillWidth: true; placeholderText: qsTr("Preset name") }
 Components.NeoButton { text: qsTr("Save current profile"); enabled: presetName.text.trim().length > 0; onClicked: { page.savePreset(presetName.text); presetName.clear() } } }
			RowLayout {
				visible: page.selectedSavedProfile() !== null
				Layout.fillWidth: true
				Components.NeoTextField { id: selectedPresetName; Layout.fillWidth: true; text: page.selectedSavedProfile() ? plannerStorage.presets[page.selectedSavedProfile().sourceIndex].name : "" }
				Components.NeoButton { text: qsTr("Rename"); onClicked: page.renamePreset(page.selectedSavedProfile().sourceIndex, selectedPresetName.text) }
				Components.NeoButton { text: qsTr("Duplicate"); onClicked: page.duplicatePreset(page.selectedSavedProfile().sourceIndex) }
				Components.NeoButton { text: qsTr("Remove"); variant: "danger"; onClicked: { var option = page.selectedSavedProfile(); if (!option) return; var saved = plannerStorage.presets || []; var removedName = saved[option.sourceIndex].name; saved.splice(option.sourceIndex, 1); plannerStorage.presets = saved; page.selectedProfileIndex = 0; if (page.activeProfileName === removedName) { page.activeProfileName = ""; page.activeProfileModified = false } } }
			}
		}
		Components.ModernCard {
			visible: page.advancedSettingsExpanded
			Layout.fillWidth: true
			Text { text: qsTr("Travel and stop settings"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: useLspPlanner?qsTr("These parameters are sent to LSP D-Planner+. Saved plans retain the resolved settings."):qsTr("These parameters are sent to the Subsurface planner."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Descent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Descent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: page.plannerValue("descrate"); enabled: page.useLspPlanner || page.plannerValue("drop_stone_mode"); onValueModified: { page.setPlannerValue("descrate", value); page.generatePlan() } } }
				Components.NeoCheckBox { visible: !page.useLspPlanner; text: qsTr("Drop to first depth"); checked: page.plannerValue("drop_stone_mode"); onToggled: { page.setPlannerValue("drop_stone_mode", checked); page.generatePlan() } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Deep ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Deep ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: page.plannerValue("ascrate75"); onValueModified: { page.setPlannerValue("ascrate75", value); page.generatePlan() } } }
				RowLayout { visible: !page.useLspPlanner; Layout.fillWidth: true; Label { text: qsTr("Mid ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Mid ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: page.plannerValue("ascrate50"); onValueModified: { page.setPlannerValue("ascrate50", value); page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Deco ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Deco ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: page.plannerValue("ascratestops"); onValueModified: { page.setPlannerValue("ascratestops", value); page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Final ascent (%1)").arg(page.speedUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Final ascent rate (%1)").arg(page.speedUnit); from: 1; to: 99; value: page.plannerValue("ascratelast6m"); onValueModified: { page.setPlannerValue("ascratelast6m", value); page.generatePlan() } } }
				Components.NeoCheckBox { text: qsTr("Last stop at 6 m / 20 ft"); checked: page.plannerValue("last_stop6m"); onToggled: { page.setPlannerValue("last_stop6m", checked); page.generatePlan() } }
				Components.NeoCheckBox { visible: !page.useLspPlanner; text: qsTr("Switch gas only at required stops"); checked: page.plannerValue("switch_at_req_stop"); onToggled: { page.setPlannerValue("switch_at_req_stop", checked); page.generatePlan() } }
				RowLayout { visible: !page.useLspPlanner; Layout.fillWidth: true; Label { text: qsTr("Minimum gas-switch time (min)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Minimum gas-switch time in minutes"); from: 0; to: 4; value: page.plannerValue("min_switch_duration"); onValueModified: { page.setPlannerValue("min_switch_duration", value); page.generatePlan() } } }
				RowLayout { visible: !page.useLspPlanner; Layout.fillWidth: true; Label { text: qsTr("Surface segment (min)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Surface segment in minutes"); from: 0; to: 4; value: page.plannerValue("surface_segment"); onValueModified: { page.setPlannerValue("surface_segment", value); page.generatePlan() } } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Problem-solving time (min)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Problem-solving time in minutes"); from: 1; to: 9; value: page.plannerValue("problemsolvingtime"); onValueModified: { page.setPlannerValue("problemsolvingtime", value); page.generatePlan() } } }
				Components.NeoCheckBox { visible: !page.useLspPlanner; text: qsTr("Plan back-gas breaks"); checked: page.plannerValue("doo2breaks"); onToggled: { page.setPlannerValue("doo2breaks", checked); page.generatePlan() } }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Bottom pO₂ limit (bar)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox {
					accessibleName: qsTr("Bottom pO₂ limit in bar")
					from: 50; to: 250; stepSize: 5; value: page.plannerValue("bottompo2")
					textFromValue: function(value) { return (value / 100).toFixed(2) }
					valueFromText: function(text) { return Math.round(Number(text) * 100) }
					onValueModified: { page.setPlannerValue("bottompo2", value / 100); page.generatePlan() }
				} }
				RowLayout { Layout.fillWidth: true; Label { text: qsTr("Deco pO₂ limit (bar)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox {
					accessibleName: qsTr("Deco pO₂ limit in bar")
					from: 50; to: 250; stepSize: 5; value: page.plannerValue("decopo2")
					textFromValue: function(value) { return (value / 100).toFixed(2) }
					valueFromText: function(text) { return Math.round(Number(text) * 100) }
					onValueModified: { page.setPlannerValue("decopo2", value / 100); page.generatePlan() }
				} }
				RowLayout { visible: diveMode.currentIndex === 1; Layout.fillWidth: true; Label { text: qsTr("CCR default setpoint (bar)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox {
					accessibleName: qsTr("CCR default setpoint in bar")
					from: 160; to: 2000; stepSize: 50; value: page.plannerValue("default_setpoint")
					textFromValue: function(value) { return (value / 1000).toFixed(2) }
					valueFromText: function(text) { return Math.round(Number(text) * 1000) }
					onValueModified: { page.setPlannerValue("default_setpoint", value); page.generatePlan() }
				} }
			}
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Plan mode & environment"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout {
				visible: page.useLspPlanner; Layout.fillWidth: true; columns: page.width>=700?2:1
				Repeater {
					model: [
						{key:"waterVapor",label:qsTr("Water vapour pressure (bar)"),minimum:0,maximum:0.1,group:-1},
						{key:"minDeco9m",label:qsTr("Minimum deco at 9 m (min)"),minimum:0,maximum:30,group:-1},
						{key:"minDeco6m",label:qsTr("Minimum deco at 6 m (min)"),minimum:0,maximum:30,group:-1},
						{key:"minStopTime",label:qsTr("Minimum stop time (min)"),minimum:1/60,maximum:30,group:-1},
						{key:"decoStep",label:qsTr("Stop spacing (m)"),minimum:1,maximum:6,group:-1},
						{key:"lastDecoStop",label:qsTr("Last stop (m)"),minimum:1,maximum:12,group:-1},
						{key:"descentSetpoint",label:qsTr("Descent setpoint (bar)"),minimum:0.5,maximum:1,group:1},
						{key:"decoSetpoint",label:qsTr("Deco setpoint (bar)"),minimum:0.7,maximum:1.6,group:1},
						{key:"scrLoopVolume",label:qsTr("pSCR loop volume (L)"),minimum:1,maximum:30,group:2},
						{key:"scrMetabolicO2",label:qsTr("Metabolic oxygen (L/min)"),minimum:0.1,maximum:5,group:2},
						{key:"sacStress",label:qsTr("Bailout stress SAC (L/min)"),minimum:10,maximum:100,group:1},
						{key:"stressTimeMin",label:qsTr("Bailout stress time (min)"),minimum:0,maximum:30,group:1}
					]
					delegate: RowLayout {
						required property var modelData
						visible: modelData.group<0||modelData.group===diveMode.currentIndex
						Layout.fillWidth: true
						Label { text: modelData.label; color: tokens.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
						Components.NeoTextField {
							Layout.preferredWidth: 100; accessibleName: modelData.label; text: String(page.lspParameters[modelData.key])
							inputMethodHints: Qt.ImhFormattedNumbersOnly
							onEditingFinished: { var n=Number(text);if(isFinite(n)&&n>=modelData.minimum&&n<=modelData.maximum){page.setLspParameter(modelData.key,n);page.generatePlan()} }
						}
					}
				}
				Components.NeoComboBox { accessibleName: qsTr("Helium half-time convention"); model:[qsTr("Baker helium half-times"),qsTr("Buhlmann 2003 helium half-times")]; currentIndex:page.lspParameters.heHalfTimeMode==="buhl2003"?1:0; onActivated:{page.setLspParameter("heHalfTimeMode",currentIndex===1?"buhl2003":"baker");page.generatePlan()} }
				Components.NeoComboBox { accessibleName: qsTr("Stop rounding");model:[qsTr("Whole-minute stops"),qsTr("Fractional stops")];currentIndex:page.lspParameters.stopRounding==="fractional"?1:0;onActivated:{page.setLspParameter("stopRounding",currentIndex===1?"fractional":"wholeminute");page.generatePlan()} }
				Components.NeoComboBox { accessibleName: qsTr("Transit model");model:[qsTr("LSP MultiDeco transit"),qsTr("Schreiner transit")];currentIndex:page.lspParameters.decoTransitMode==="schreiner"?1:0;onActivated:{page.setLspParameter("decoTransitMode",currentIndex===1?"schreiner":"multideco");page.generatePlan()} }
				Components.NeoComboBox { visible:String(page.lspParameters.model||"").indexOf("VPM")===0;accessibleName:qsTr("VPM variant");model:["VPM-A","VPM-B","VPM-B/E","VPM-B+GFS","VPM-B/FBO"];currentIndex:["VPMA","VPMB","VPMBE","VPMB_GFS","VPMBFBO"].indexOf(page.lspParameters.model);onActivated:{page.setLspParameter("model",["VPMA","VPMB","VPMBE","VPMB_GFS","VPMBFBO"][currentIndex]);page.generatePlan()} }
				Components.NeoCheckBox { text:qsTr("Minimum deco profile");checked:page.lspParameters.minDecoProfileEnable==="yes";onToggled:{page.setLspParameter("minDecoProfileEnable",checked?"yes":"no");page.generatePlan()} }
				Components.NeoCheckBox { text:qsTr("Allow oxygen at its MOD");checked:page.lspParameters.o2AtMODSelect==="on";onToggled:{page.setLspParameter("o2AtMODSelect",checked?"on":"off");page.generatePlan()} }
				Components.NeoCheckBox { text:qsTr("Acclimatized to this altitude");checked:page.lspParameters.acclimatized!==false;onToggled:{page.setLspParameter("acclimatized",checked);page.generatePlan()} }
				Components.NeoCheckBox { text:qsTr("Shallow gradient policy");checked:page.lspParameters.shallowGradient==="on";onToggled:{page.setLspParameter("shallowGradient",checked?"on":"off");page.generatePlan()} }
				Components.NeoCheckBox { visible:diveMode.currentIndex!==0;text:qsTr("Use diluent supply as OC bailout");checked:page.lspParameters.diluentUseAsBailout===true;onToggled:{page.setLspParameter("diluentUseAsBailout",checked);page.generatePlan()} }
			}
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 2 : 1
				Components.NeoTextField { Layout.fillWidth: true; text: page.plannedDate; inputMask: "0000-00-00"; placeholderText: qsTr("Planned date (YYYY-MM-DD)"); onEditingFinished: { page.plannedDate = text; page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: page.plannedTime; inputMask: "00:00:00"; placeholderText: qsTr("Planned time (HH:MM:SS)"); onEditingFinished: { page.plannedTime = text; page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: page.surfacePressureBar.toFixed(3); inputMethodHints: Qt.ImhFormattedNumbersOnly; placeholderText: qsTr("Surface pressure (bar)"); onEditingFinished: { var pressure = Number(text); if (!isNaN(pressure) && pressure > 0) { page.surfacePressureBar = pressure; page.generatePlan() } } }
				Components.NeoTextField { Layout.fillWidth: true; text: manager.plannerAltitudeForSurfacePressure(page.surfacePressureBar).toFixed(0); inputMethodHints: Qt.ImhFormattedNumbersOnly; placeholderText: qsTr("Altitude (-500 to 12,000 m)"); onEditingFinished: { var altitude = Number(text); if (!isNaN(altitude) && altitude >= -500 && altitude <= 12000) { page.surfacePressureBar = manager.plannerSurfacePressureForAltitude(altitude); page.generatePlan() } } }
				Components.NeoComboBox { id: diveMode; Layout.fillWidth: true; accessibleName: qsTr("Dive mode"); model: [qsTr("Open circuit"), qsTr("CCR"), qsTr("pSCR")]; onActivated: page.selectDiveMode(currentIndex) }
				Components.NeoComboBox { id: waterType; Layout.fillWidth: true; accessibleName: qsTr("Water type"); model: [qsTr("Sea water"), qsTr("Fresh water"), qsTr("EN13319"), qsTr("Custom water density")]; onActivated: page.generatePlan() }
				RowLayout { visible: waterType.currentIndex === 3; Layout.fillWidth: true; Label { text: qsTr("Water density (kg/10,000 L)"); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Water density in kilograms per 10,000 litres"); from: 9000; to: 12000; value: page.customSalinity; onValueModified: { page.customSalinity = value; page.generatePlan() } } }
			}
			Text { text: useLspPlanner?qsTr("Recent sampled dives initialize LSP tissues. Incomplete or unsupported history is reported before saving."):qsTr("Subsurface initializes tissues from logged dives and their surface intervals."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { text: qsTr("Altitude and surface pressure describe the same native planner input. Changing either updates the pressure used to calculate this plan."); color: tokens.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoCheckBox { visible: diveMode.currentIndex !== 0; text: qsTr("Deco on OC bailout"); checked: page.plannerValue("dobailout"); onToggled: { page.setPlannerValue("dobailout", checked); page.generatePlan() } }
			Components.NeoCheckBox { visible: !page.useLspPlanner && page.plannerValue("planner_deco_mode") !== Enums.RECREATIONAL; text: qsTr("Calculate contingency variations"); checked: page.plannerValue("display_variations"); onToggled: { page.setPlannerValue("display_variations", checked); page.generatePlan() } }
			Text { visible: !page.useLspPlanner && page.plannerValue("planner_deco_mode") !== Enums.RECREATIONAL && page.plannerValue("display_variations"); text: qsTr("Subsurface adds the calculated contingencies to the plan notes below; the main schedule remains unchanged."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			RowLayout { Layout.fillWidth: true; Label { text: qsTr("Reserve gas (%1)").arg(page.pressureUnit); color: tokens.textMuted; Layout.fillWidth: true }
 Components.NeoSpinBox { accessibleName: qsTr("Reserve gas (%1)").arg(page.pressureUnit); from: 0; to: Backend.pressure === Enums.BAR ? 400 : 6000; value: page.plannerValue("reserve_gas"); onValueModified: { page.setPlannerValue("reserve_gas", value); page.generatePlan() } } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout { Layout.fillWidth: true; Text { text: qsTr("Gases & cylinders"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
 Components.NeoButton { text: qsTr("Add gas"); onClicked: page.addCylinder() } }
			Repeater { model: cylinders; delegate: GridLayout {
				required property int index; required property string type; required property string mix; required property real pressure; required property int use
				property int oxygen: page.mixOxygen(mix)
				property int helium: page.mixHelium(mix)
				Layout.fillWidth: true; columns: page.width >= 700 ? 6 : 2
				Label { text: qsTr("Gas %1").arg(index + 1); color: tokens.textMuted }
				Components.NeoComboBox { Layout.fillWidth: true; accessibleName: qsTr("Cylinder type for gas %1").arg(index + 1); model: page.cylinderTypes; currentIndex: page.cylinderTypes.indexOf(type); onActivated: { cylinders.setProperty(index, "type", currentText); page.generatePlan() } }
				RowLayout { Layout.preferredWidth: 126; Layout.maximumWidth: 126; spacing: tokens.space8; Label { text: qsTr("O₂ %"); color: tokens.textMuted } Components.NeoTextField { Layout.preferredWidth: 78; Layout.maximumWidth: 78; accessibleName: qsTr("Oxygen percentage for gas %1").arg(index + 1); text: String(oxygen); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { var value = Number(text); if (!isNaN(value)) page.setCylinderMix(index, value, helium) } } }
				RowLayout { Layout.preferredWidth: 126; Layout.maximumWidth: 126; spacing: tokens.space8; Label { text: qsTr("He %"); color: tokens.textMuted } Components.NeoTextField { Layout.preferredWidth: 78; Layout.maximumWidth: 78; accessibleName: qsTr("Helium percentage for gas %1").arg(index + 1); text: String(helium); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { var value = Number(text); if (!isNaN(value)) page.setCylinderMix(index, oxygen, value) } } }
				Components.NeoTextField { Layout.preferredWidth: 150; Layout.maximumWidth: 150; text: pressure; inputMethodHints: Qt.ImhDigitsOnly; placeholderText: page.pressureUnit; onEditingFinished: { cylinders.setProperty(index, "pressure", Number(text)); page.generatePlan() } }
				RowLayout { CheckBox { visible: diveMode.currentIndex === 1; text: qsTr("Diluent"); checked: use === 1; onToggled: { cylinders.setProperty(index, "use", checked ? 1 : 0); page.generatePlan() } }
 Components.NeoButton { text: qsTr("Remove"); enabled: cylinders.count > 1; onClicked: { cylinders.remove(index); page.updateGasNames(); page.generatePlan() } } }
			} }
			Text { visible: page.gasReference.length > 0; text: qsTr("Gas depth reference"); color: tokens.textSecondary; font.weight: Font.DemiBold }
			Repeater { model: page.gasReference; delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Label { text: modelData.name + " " + modelData.mix; color: tokens.textMuted; Layout.fillWidth: true }
 Label { text: qsTr("Deco switch %1 (pO₂ %2 bar)").arg(modelData.decoSwitch).arg((page.plannerValue("decopo2") / 100.0).toFixed(1)); color: tokens.accent }
 Label { text: qsTr("Bottom MOD %1 (pO₂ %2 bar)").arg(modelData.bottomMod).arg((page.plannerValue("bottompo2") / 100.0).toFixed(1)); color: tokens.textPrimary }
 Label { text: (page.plannerValue("o2narcotic") ? qsTr("END %1") : qsTr("EAD %1")).arg(modelData.ead); color: tokens.textPrimary } } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			RowLayout { Layout.fillWidth: true; Text { text: qsTr("Profile segments"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
 Components.NeoButton { text: qsTr("Add segment"); onClicked: page.addSegment() } }
			Text { text: qsTr("Each row is a target depth and the runtime when that segment ends. Neo inserts travel within that runtime, matching Subsurface's native planner. Use separate rows for multilevel profiles and requested gas changes."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Repeater { model: segments; delegate: GridLayout {
				required property int index; required property real depth; required property real duration; required property int gas; required property real setpoint; required property int divemode
				Layout.fillWidth: true; columns: page.width >= 700 ? 6 : 2
				Label { text: qsTr("%1").arg(index + 1); color: tokens.textMuted }
				Components.NeoTextField { Layout.fillWidth: true; text: depth; placeholderText: qsTr("Depth (%1)").arg(page.depthUnit); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { segments.setProperty(index, "depth", Number(text)); page.generatePlan() } }
				Components.NeoTextField { Layout.fillWidth: true; text: duration; placeholderText: qsTr("Runtime at segment end (min)"); inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: { segments.setProperty(index, "duration", Number(text)); page.generatePlan() } }
				Components.NeoComboBox { Layout.fillWidth: true; accessibleName: qsTr("Gas for segment %1").arg(index + 1); model: page.gasNames; currentIndex: gas; onActivated: { segments.setProperty(index, "gas", currentIndex); page.generatePlan() } }
				Components.NeoTextField { visible: !page.useLspPlanner && diveMode.currentIndex === 1; Layout.fillWidth: true; text: (setpoint / 1000.0).toFixed(2); placeholderText: qsTr("Setpoint bar"); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: { segments.setProperty(index, "setpoint", Math.round(Number(text) * 1000)); page.generatePlan() } }
				Components.NeoButton { text: qsTr("Remove"); enabled: segments.count > 1; onClicked: { segments.remove(index); page.generatePlan() } }
			} }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Calculated profile"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 3 : 2
				Components.MetricCard { label: qsTr("Runtime"); value: page.formatDuration(page.runtimeSeconds); Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("NDL"); value: page.formatDuration(page.finalSampleValue("ndl", -1)); Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("CNS"); value: page.finalSampleValue("cns", 0) > 0 ? page.finalSampleValue("cns", 0).toFixed(1) : "—"; suffix: page.finalSampleValue("cns", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("Surface GF"); value: page.surfacedSampleValue("surfaceGf", 0) > 0 ? page.surfacedSampleValue("surfaceGf", 0).toFixed(0) : "—"; suffix: page.surfacedSampleValue("surfaceGf", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("Max tissue loading"); value: page.finalSampleValue("tissueLoad", 0) > 0 ? page.finalSampleValue("tissueLoad", 0).toFixed(0) : "—"; suffix: page.finalSampleValue("tissueLoad", 0) > 0 ? "%" : ""; Layout.fillWidth: true }
				Components.MetricCard { label: qsTr("OTU"); value: page.planOtu > 0 ? String(page.planOtu) : "—"; Layout.fillWidth: true }
			}
			Canvas { id: profileCanvas; Layout.fillWidth: true; Layout.preferredHeight: Math.max(280, Math.min(420, page.height * 0.42)); onPaint: getContext("2d").reset()
                Loader {
                    anchors.fill: parent; active: !page.useLspPlanner
                    sourceComponent: Component { QMLProfile { id: plannerNativeProfile; anchors.fill: parent; plannerPreview: true; clip: true } }
                }
                Components.LspPlanProfile { objectName: "NeoLspPreviewProfile"; anchors.fill: parent; visible: page.useLspPlanner; points: page.profileData; inspectable: false }
				Rectangle {
					visible: page.inspectedProfileSample !== null
					x: page.profileSampleX(page.inspectedProfileSample, profileCanvas.width)
					y: page.profileChartTop
					width: 1
					height: parent.height - page.profileChartTop - page.profileChartBottom
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
					Text { text: qsTr("Gas %1   EAD %2   CNS %3%").arg(page.inspectedProfileSample && page.inspectedProfileSample.gas ? page.inspectedProfileSample.gas : "—").arg(page.inspectedProfileSample && page.inspectedProfileSample.ead !== undefined ? (page.inspectedProfileSample.ead / (Backend.length === Enums.METERS ? 1000 : 304.8)).toFixed(1) + " " + page.depthUnit : "—").arg(page.inspectedProfileSample && page.inspectedProfileSample.cns > 0 ? page.inspectedProfileSample.cns.toFixed(1) : "—"); color: tokens.textSecondary }
				}
			}
			RowLayout { Layout.fillWidth: true; spacing: tokens.space8; Rectangle { width: 18; height: 3; color: page.exceedsNDL ? "#F87171" : tokens.accent }
 Text { text: qsTr("Profile"); color: tokens.textSecondary }
 Rectangle { visible: page.hasCalculatedCeiling(); width: 18; height: 3; color: "#FB923C" }
 Text { visible: page.hasCalculatedCeiling(); text: qsTr("Calculated ceiling"); color: tokens.textSecondary }
 Text { visible: !page.hasCalculatedCeiling(); text: qsTr("No calculated ceiling"); color: tokens.textMuted }
	Item { Layout.fillWidth: true } }
			Label { visible: page.exceedsNDL; text: qsTr("This recreational plan exceeds the NDL. Review the schedule and warnings before saving."); color: "#F87171"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Label { visible: !page.planSaveAllowed && !page.exceedsNDL; text: qsTr("The planner could not create a valid saveable plan. Correct the gas, bailout, or planner warnings before continuing."); color: "#F87171"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { visible: page.timeline.length > 0; text: qsTr("Full plan"); color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
			Components.PlanSchedule { rows: page.timeline; profileData: page.profileData; maximumWidth: page.plannerReadingWidth; Layout.fillWidth: true }
			Text { visible: page.schedule.length > 0; text: qsTr("Decompression schedule"); color: tokens.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
			Components.PlanSchedule { rows: page.schedule; profileData: page.profileData; maximumWidth: page.plannerReadingWidth; decoOnly: true; Layout.fillWidth: true }
			RowLayout { Layout.fillWidth: true; Button { text: qsTr("Recalculate"); onClicked: page.generatePlan(false, true) }
	Components.NeoButton { objectName: "NeoSavePlannerPlan"; text: qsTr("Save plan"); variant: "primary"; enabled: page.planSaveAllowed; onClicked: page.generatePlan(true, true) }
 Components.NeoButton { text: qsTr("Copy deco slate"); onClicked: manager.copyToClipboard(page.decoSlate()) }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save as TXT"); onClicked: plannerTextFolder.open() }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Save as PDF"); onClicked: plannerPdfFolder.open() }
 Item { Layout.fillWidth: true } }
			RowLayout { visible: page.contingencyResult !== null; Layout.fillWidth: true; Text { text: qsTr("Plan package includes the main plan and this selected contingency."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
 Components.NeoButton { text: qsTr("Copy package"); onClicked: manager.copyToClipboard(page.planPackage()) }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Package TXT"); onClicked: planPackageTextFolder.open() }
 Components.NeoButton { visible: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; text: qsTr("Package PDF"); onClicked: planPackagePdfFolder.open() } }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Gas sufficiency"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Consumption and remaining pressure are produced by Subsurface's planner for this exact profile."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Repeater { model: page.gasAnalysis; delegate: GridLayout {
				required property var modelData
				Layout.fillWidth: true
				columns: page.width >= 700 ? 6 : 2
				Label { text: modelData.mix; color: modelData.belowMinimum || modelData.belowReserve ? "#F87171" : tokens.textPrimary; font.weight: Font.DemiBold; Layout.fillWidth: true; Layout.preferredWidth: 1 }
				Label { text: qsTr("Start %1").arg(modelData.startPressure); color: tokens.textSecondary; Layout.fillWidth: true; Layout.preferredWidth: 1 }
				Label { text: qsTr("Used %1").arg(modelData.used); color: tokens.textSecondary; Layout.fillWidth: true; Layout.preferredWidth: 1 }
				Label { text: qsTr("Deco %1").arg(modelData.decoUsed); color: tokens.textSecondary; Layout.fillWidth: true; Layout.preferredWidth: 1 }
				Label { text: qsTr("Remain %1").arg(modelData.remaining); color: tokens.textPrimary; Layout.fillWidth: true; Layout.preferredWidth: 1 }
				Label { text: modelData.belowMinimum ? qsTr("Insufficient gas") : modelData.belowReserve ? qsTr("Below reserve") : qsTr("End %1").arg(modelData.endPressure); color: modelData.belowMinimum || modelData.belowReserve ? "#F87171" : tokens.success; Layout.fillWidth: true; Layout.preferredWidth: 1 }
			} }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Contingency scenario"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft; text: qsTr("Calculate a separate mature-planner result; the main plan remains unchanged."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			RowLayout { Layout.fillWidth: true; Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft; ComboBox { Layout.fillWidth: true; model: [qsTr("Extra bottom time"), qsTr("Deeper profile"), qsTr("Lost deco/travel gas")]; currentIndex: page.contingencyScenario; onActivated: page.contingencyScenario = currentIndex }
 Components.NeoSpinBox { visible: page.contingencyScenario !== 2; accessibleName: page.contingencyScenario === 0 ? qsTr("Extra bottom time in minutes") : qsTr("Extra depth (%1)").arg(page.depthUnit); from: 1; to: 30; value: page.contingencyDelta; onValueModified: page.contingencyDelta = value }
 Components.NeoComboBox { visible: page.contingencyScenario === 2; Layout.fillWidth: true; accessibleName: qsTr("Lost decompression or travel gas"); model: page.gasNames; currentIndex: page.contingencyGasIndex; onActivated: page.contingencyGasIndex = currentIndex }
 Label { visible: page.contingencyScenario !== 2; text: page.contingencyScenario === 0 ? qsTr("minutes") : page.depthUnit; color: tokens.textMuted } }
			Components.NeoButton { Layout.fillWidth: true; Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft; text: qsTr("Calculate %1").arg(page.contingencyName()); onClicked: page.calculateContingency() }
			ColumnLayout { visible: page.contingencyResult !== null; Layout.fillWidth: true; Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft
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
			Text { text: qsTr("Technical tools"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft; text: qsTr("Use the established gas calculator for MOD, Best Mix, END/EAD, CNS and OTU reference calculations. The NDL reference below uses the selected planner’s own calculation."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Components.NeoButton { Layout.fillWidth: true; Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft; text: page.gasToolsExpanded ? qsTr("Hide gas calculator") : qsTr("Gas calculator"); onClicked: { page.gasToolsExpanded = !page.gasToolsExpanded; if (page.gasToolsExpanded && page.gasCalculatorResults.length === 0) page.calculateGasTools() } }
			ColumnLayout {
				visible: page.gasToolsExpanded
				Layout.fillWidth: true
				Layout.maximumWidth: page.plannerReadingWidth
				Layout.alignment: Qt.AlignLeft
				GridLayout {
					Layout.fillWidth: true
					columns: page.width >= 700 ? 3 : 1
					RowLayout { Layout.fillWidth: true; spacing: tokens.space8; Label { text: qsTr("Oxygen %:"); color: tokens.textMuted; Layout.preferredWidth: 76 } Components.NeoSpinBox { Layout.preferredWidth: 140; from: 0; to: 100; value: page.gasOxygen; onValueModified: page.gasOxygen = value } Item { Layout.fillWidth: true } }
					RowLayout { Layout.fillWidth: true; spacing: tokens.space8; Label { text: qsTr("Helium %:"); color: tokens.textMuted; Layout.preferredWidth: 76 } Components.NeoSpinBox { Layout.preferredWidth: 140; from: 0; to: 100; value: page.gasHelium; onValueModified: page.gasHelium = value } Item { Layout.fillWidth: true } }
					Components.NeoButton { text: qsTr("Calculate"); onClicked: page.calculateGasTools() }
				}
				GridLayout { Layout.fillWidth: true; columns: 3; Label { text: qsTr("pO₂"); color: tokens.textPrimary; font.weight: Font.DemiBold; Layout.fillWidth: true } Label { text: qsTr("MOD"); color: tokens.textPrimary; font.weight: Font.DemiBold; Layout.fillWidth: true } Label { text: page.plannerValue("o2narcotic") ? qsTr("END @ MOD") : qsTr("EAD @ MOD"); color: tokens.textPrimary; font.weight: Font.DemiBold; Layout.fillWidth: true } }
				Repeater { model: page.gasCalculatorResults; delegate: GridLayout { required property var modelData; Layout.fillWidth: true; columns: 3; Label { text: modelData.po2 || "—"; color: tokens.textSecondary; Layout.fillWidth: true } Label { text: modelData.mod || "—"; color: tokens.textSecondary; Layout.fillWidth: true } Label { text: modelData.ead || "—"; color: tokens.textSecondary; Layout.fillWidth: true } } }
			}
			Components.NeoButton { Layout.fillWidth: true; Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft; text: qsTr("Calculate NDL reference"); onClicked: page.calculateNdlReference() }
			Repeater { model: page.ndlReference; delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Layout.maximumWidth: page.plannerReadingWidth; Label { text: qsTr("%1 %2").arg(modelData.depth).arg(page.depthUnit); color: tokens.textPrimary; Layout.fillWidth: true }
 Label { text: modelData.available ? (modelData.capped ? "≥ " : "") + page.formatDuration(modelData.ndl) : qsTr("Not available"); color: modelData.available ? tokens.textSecondary : tokens.textMuted } } }
			Text { visible: page.ndlReference.length > 0; Layout.maximumWidth: page.plannerReadingWidth; text: page.useLspPlanner ? qsTr("LSP fresh-tissue OC NDL reference, using the first gas and current environment. CCR, pSCR, VPM and repetitive references are unavailable. A ≥ value reaches the LSP 500-minute limit.") : qsTr("Reference only: uses the current model, gases, water, surface pressure, and one-minute level profile at each shown depth. Review the generated plan before diving."); color: tokens.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Text { text: qsTr("Unit reference"); color: tokens.textPrimary; font.weight: Font.DemiBold }
			Components.NeoTextField { Layout.fillWidth: true; Layout.maximumWidth: 360; Layout.alignment: Qt.AlignLeft; text: page.conversionValue.toString(); inputMethodHints: Qt.ImhFormattedNumbersOnly; placeholderText: qsTr("Value to convert"); onEditingFinished: { var value = Number(text); if (!isNaN(value)) page.conversionValue = value } }
			GridLayout { Layout.fillWidth: true; Layout.maximumWidth: page.plannerReadingWidth; Layout.alignment: Qt.AlignLeft; columns: page.width >= 700 ? 3 : 1
				Label { text: qsTr("%1 m = %2 ft").arg(page.conversionValue.toFixed(2)).arg((page.conversionValue * 3.28084).toFixed(2)); color: tokens.textSecondary }
				Label { text: qsTr("%1 bar = %2 psi").arg(page.conversionValue.toFixed(2)).arg((page.conversionValue * 14.5038).toFixed(2)); color: tokens.textSecondary }
				Label { text: qsTr("%1 L = %2 US gal").arg(page.conversionValue.toFixed(2)).arg((page.conversionValue * 0.264172).toFixed(2)); color: tokens.textSecondary }
			}
		}
		Text { text: qsTr("Planning aid only. Confirm the active algorithm, units, gases, environmental assumptions, schedule and warnings before diving."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
