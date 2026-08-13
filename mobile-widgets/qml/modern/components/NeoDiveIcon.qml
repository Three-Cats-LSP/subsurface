// SPDX-License-Identifier: GPL-2.0
import QtQuick

Canvas {
	id: icon
	property string name: "depth"
	property color iconColor: "#22D4EB"
	implicitWidth: 28
	implicitHeight: 28
	onNameChanged: requestPaint()
	onIconColorChanged: requestPaint()
	onWidthChanged: requestPaint()
	onHeightChanged: requestPaint()

	function line(ctx, points) {
		ctx.beginPath()
		ctx.moveTo(points[0], points[1])
		for (var i = 2; i < points.length; i += 2)
			ctx.lineTo(points[i], points[i + 1])
		ctx.stroke()
	}

	onPaint: {
		var ctx = getContext("2d")
		ctx.reset()
		ctx.save()
		ctx.scale(width / 28, height / 28)
		ctx.strokeStyle = iconColor
		ctx.fillStyle = iconColor
		ctx.lineWidth = 1.8
		ctx.lineCap = "round"
		ctx.lineJoin = "round"

		if (name === "depth") {
			line(ctx, [14, 4, 14, 18, 10, 14, 14, 18, 18, 14])
			ctx.beginPath(); ctx.arc(8, 21, 3, 0.2, 2.9); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 21, 3, 0.2, 2.9); ctx.stroke()
			ctx.beginPath(); ctx.arc(20, 21, 3, 0.2, 2.9); ctx.stroke()
		} else if (name === "time") {
			ctx.beginPath(); ctx.arc(14, 14, 9, 0, Math.PI * 2); ctx.stroke()
			line(ctx, [14, 8, 14, 14, 18, 16])
		} else if (name === "temperature") {
			ctx.beginPath(); ctx.arc(14, 20, 4, 0, Math.PI * 2); ctx.stroke()
			ctx.beginPath(); ctx.moveTo(12, 17); ctx.lineTo(12, 7); ctx.arc(14, 7, 2, Math.PI, 0); ctx.lineTo(16, 17); ctx.stroke()
			line(ctx, [14, 10, 14, 20])
		} else if (name === "gas") {
			ctx.beginPath(); ctx.moveTo(10, 8); ctx.lineTo(10, 23); ctx.quadraticCurveTo(10, 26, 13, 26); ctx.lineTo(17, 26); ctx.quadraticCurveTo(20, 26, 20, 23); ctx.lineTo(20, 10); ctx.quadraticCurveTo(20, 8, 17, 7); ctx.lineTo(13, 7); ctx.quadraticCurveTo(10, 8, 10, 10); ctx.stroke()
			line(ctx, [13, 7, 13, 4, 17, 4, 17, 7]); line(ctx, [12, 4, 18, 4]); line(ctx, [15, 4, 15, 2])
			ctx.beginPath(); ctx.arc(15, 2, 1.3, 0, Math.PI * 2); ctx.stroke()
			ctx.beginPath(); ctx.roundedRect(6, 3, 7, 4, 2, 2); ctx.stroke()
			line(ctx, [17, 3, 20, 3, 20, 6, 17, 6]); line(ctx, [18, 11, 19, 12, 19, 14]); line(ctx, [19, 16, 19, 20])
		} else if (name === "gear") {
			ctx.beginPath(); ctx.ellipse(10, 2, 8, 3); ctx.stroke()
			ctx.beginPath(); ctx.moveTo(10, 5); ctx.quadraticCurveTo(5, 7, 5, 14); ctx.lineTo(5, 18); ctx.lineTo(23, 18); ctx.lineTo(23, 14); ctx.quadraticCurveTo(23, 7, 18, 5); ctx.stroke()
			ctx.beginPath(); ctx.ellipse(2, 10, 5, 7); ctx.stroke(); ctx.beginPath(); ctx.ellipse(21, 10, 5, 7); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 12, 5, 0, Math.PI * 2); ctx.stroke()
			line(ctx, [10.5, 8.5, 17.5, 15.5]); line(ctx, [17.5, 8.5, 10.5, 15.5])
			ctx.strokeRect(5, 18, 18, 3); line(ctx, [8, 21, 6, 25, 22, 25, 20, 21])
			ctx.beginPath(); ctx.arc(10, 23, 1, 0, Math.PI * 2); ctx.stroke(); ctx.beginPath(); ctx.arc(18, 23, 1, 0, Math.PI * 2); ctx.stroke()
			ctx.beginPath(); ctx.moveTo(5, 24); ctx.quadraticCurveTo(14, 30, 23, 24); ctx.stroke()
		} else if (name === "tank") {
			ctx.strokeRect(10, 7, 8, 16)
			line(ctx, [12, 7, 12, 4, 16, 4, 16, 7])
			line(ctx, [9, 11, 19, 11]); line(ctx, [9, 19, 19, 19])
		} else if (name === "regulator") {
			ctx.beginPath(); ctx.arc(10, 11, 5, 0, Math.PI * 2); ctx.stroke()
			ctx.beginPath(); ctx.arc(10, 11, 1.5, 0, Math.PI * 2); ctx.stroke()
			ctx.beginPath(); ctx.moveTo(15, 11); ctx.bezierCurveTo(23, 11, 23, 18, 18, 19); ctx.stroke()
			ctx.beginPath(); ctx.rect(15, 17, 7, 5); ctx.stroke()
			line(ctx, [17, 19.5, 20, 19.5])
		} else if (name === "boat") {
			line(ctx, [5, 16, 23, 16, 20, 21, 9, 21, 5, 16])
			line(ctx, [9, 16, 11, 10, 19, 10, 21, 16]); line(ctx, [13, 10, 13, 7, 18, 10])
		} else if (name === "buddy") {
			ctx.beginPath(); ctx.arc(14, 9, 4, 0, Math.PI * 2); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 22, 8, Math.PI, Math.PI * 2); ctx.stroke()
		} else if (name === "notes") {
			ctx.strokeRect(6, 5, 16, 18)
			line(ctx, [10, 10, 18, 10]); line(ctx, [10, 14, 18, 14]); line(ctx, [10, 18, 16, 18])
		} else if (name === "home") {
			line(ctx, [5, 13, 14, 5, 23, 13])
			ctx.beginPath(); ctx.moveTo(8, 12); ctx.lineTo(8, 23); ctx.lineTo(20, 23); ctx.lineTo(20, 12); ctx.stroke()
			ctx.strokeRect(12, 17, 4, 6)
		} else if (name === "site") {
			ctx.beginPath(); ctx.arc(14, 11, 7, Math.PI, 0); ctx.quadraticCurveTo(21, 18, 14, 25); ctx.quadraticCurveTo(7, 18, 7, 11); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 11, 2.5, 0, Math.PI * 2); ctx.stroke()
		} else if (name === "stats") {
			line(ctx, [6, 23, 6, 15]); line(ctx, [12, 23, 12, 8]); line(ctx, [18, 23, 18, 12]); line(ctx, [24, 23, 24, 5])
		} else if (name === "more") {
			ctx.beginPath(); ctx.arc(7, 14, 2, 0, Math.PI * 2); ctx.fill()
			ctx.beginPath(); ctx.arc(14, 14, 2, 0, Math.PI * 2); ctx.fill()
			ctx.beginPath(); ctx.arc(21, 14, 2, 0, Math.PI * 2); ctx.fill()
		} else if (name === "search") {
			ctx.beginPath(); ctx.arc(12, 12, 7, 0, Math.PI * 2); ctx.stroke()
			line(ctx, [17, 17, 23, 23])
		} else {
			ctx.beginPath(); ctx.arc(14, 14, 8, 0, Math.PI * 2); ctx.stroke()
		}
		ctx.restore()
	}
}
