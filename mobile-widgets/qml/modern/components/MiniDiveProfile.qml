// SPDX-License-Identifier: GPL-2.0
import QtQuick
import ".." as Modern

Canvas {
	id: preview
	property var samples: []
	Modern.DesignTokens { id: tokens }
	implicitHeight: 120
	Accessible.role: Accessible.Graphic
	Accessible.name: qsTr("Recorded dive depth preview")
	onSamplesChanged: requestPaint()
	onWidthChanged: requestPaint()
	onHeightChanged: requestPaint()
	Connections {
		target: tokens
		function onAccentChanged() { preview.requestPaint() }
		function onBorderChanged() { preview.requestPaint() }
	}
	onPaint: {
		var ctx = getContext("2d")
		ctx.reset()
		ctx.strokeStyle = tokens.border
		ctx.lineWidth = 1
		for (var row = 1; row <= 3; ++row) {
			ctx.beginPath(); ctx.moveTo(0, height * row / 4); ctx.lineTo(width, height * row / 4); ctx.stroke()
		}
		if (!samples || samples.length < 2) return
		var maxTime = 1, maxDepth = 1
		for (var i = 0; i < samples.length; ++i) {
			maxTime = Math.max(maxTime, samples[i].time)
			maxDepth = Math.max(maxDepth, samples[i].depth)
		}
		ctx.beginPath()
		for (var j = 0; j < samples.length; ++j) {
			var x = samples[j].time / maxTime * width
			var y = 8 + samples[j].depth / maxDepth * (height - 20)
			if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
		}
		ctx.strokeStyle = tokens.accent; ctx.lineWidth = 2; ctx.stroke()
		ctx.lineTo(width, height); ctx.lineTo(0, height); ctx.closePath()
		var fill = ctx.createLinearGradient(0, 0, 0, height)
		fill.addColorStop(0, Qt.rgba(tokens.accent.r, tokens.accent.g, tokens.accent.b, 0.28))
		fill.addColorStop(1, Qt.rgba(tokens.accent.r, tokens.accent.g, tokens.accent.b, 0.01))
		ctx.fillStyle = fill; ctx.fill()
	}
}
