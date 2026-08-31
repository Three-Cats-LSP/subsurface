// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Layouts
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern

ColumnLayout {
	id: schedule
	property var rows: []
	property var profileData: []
	property real maximumWidth: 760
	property bool decoOnly: false
	readonly property string depthUnit: Backend.length === Enums.METERS ? qsTr("m") : qsTr("ft")

	Modern.DesignTokens { id: tokens }

	function clock(seconds) {
		seconds = Math.max(0, Number(seconds || 0))
		var remainder = seconds % 60
		return Math.floor(seconds / 60) + ":" + (remainder < 10 ? "0" : "") + remainder
	}
	function phaseSymbol(phase) {
		if (phase === "descent") return "↓"
		if (phase === "ascent") return "↑"
		return "●"
	}
	function sampleAtTime(seconds) {
		if (!profileData || profileData.length === 0)
			return null
		var nearest = profileData[0]
		for (var i = 1; i < profileData.length; ++i) {
			if (Math.abs(profileData[i].time - seconds) < Math.abs(nearest.time - seconds))
				nearest = profileData[i]
		}
		return nearest
	}
	function line(row) {
		var divisor = Backend.length === Enums.METERS ? 1000 : 304.8
		var sample = sampleAtTime(row.runTime)
		var action = phaseSymbol(row.phase) + " " + (Number(row.depth || 0) / divisor).toFixed(1) + " " + depthUnit
		var gas = String(row.gas || "—").toUpperCase()
		if (row.gasSwitch)
			gas = ">> " + gas
		var po2Value = sample && sample.po2 > 0 ? sample.po2 : row.po2
		var eadValue = sample && sample.ead !== undefined && sample.ead >= 0 ? sample.ead : row.ead
		var po2 = po2Value > 0 ? (po2Value / 1000.0).toFixed(2) : "—"
		var ead = eadValue !== undefined && eadValue >= 0 ? (eadValue / divisor).toFixed(1) + " " + depthUnit : "—"
		var text = action + "  ·  " + clock(row.duration) + "  ·  " + gas + "  ·  RT " + clock(row.runTime) + "  ·  pO₂ " + po2 + "  ·  EAD " + ead
		if (row.setpoint > 0)
			text += "  ·  SP " + (row.setpoint / 1000.0).toFixed(2) + " bar"
		return text
	}

	Repeater {
		model: schedule.rows || []
		delegate: Text {
			required property var modelData
			Layout.fillWidth: true
			Layout.maximumWidth: schedule.maximumWidth
			text: schedule.line(modelData)
			color: schedule.decoOnly || modelData.phase === "deco" ? "#F87171" : modelData.gasSwitch ? tokens.accent : tokens.textPrimary
			font.family: "monospace"
			font.pixelSize: 13
			wrapMode: Text.Wrap
		}
	}
}
