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
		} else {
			ctx.beginPath(); ctx.arc(14, 14, 8, 0, Math.PI * 2); ctx.stroke()
		}
		ctx.restore()
	}
}
