// SPDX-License-Identifier: GPL-2.0
import QtQuick
import Qt5Compat.GraphicalEffects

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
			// Rendered by the attributed Flaticon source below.
		} else if (name === "gear") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "device") {
			ctx.beginPath(); ctx.roundedRect(6, 5, 16, 18, 4, 4); ctx.stroke()
			ctx.beginPath(); ctx.roundedRect(9, 8, 10, 8, 1.5, 1.5); ctx.stroke()
			line(ctx, [11, 13, 13, 11, 15, 14, 18, 10])
			ctx.beginPath(); ctx.arc(11, 20, 1, 0, Math.PI * 2); ctx.fill()
			ctx.beginPath(); ctx.arc(17, 20, 1, 0, Math.PI * 2); ctx.fill()
			line(ctx, [10, 5, 10, 2, 18, 2, 18, 5]); line(ctx, [10, 23, 10, 26, 18, 26, 18, 23])
		} else if (name === "tank") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "dives") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "equipmentItem") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "planner") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "slate") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "diveComputer") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "regulator") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "type") {
			// Rendered by the attributed Flaticon source below.
		} else if (name === "boat") {
			line(ctx, [5, 16, 23, 16, 20, 21, 9, 21, 5, 16])
			line(ctx, [9, 16, 11, 10, 19, 10, 21, 16]); line(ctx, [13, 10, 13, 7, 18, 10])
		} else if (name === "buddy") {
			ctx.beginPath(); ctx.arc(14, 9, 4, 0, Math.PI * 2); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 22, 8, Math.PI, Math.PI * 2); ctx.stroke()
		} else if (name === "notes") {
			ctx.strokeRect(6, 5, 16, 18)
			line(ctx, [10, 10, 18, 10]); line(ctx, [10, 14, 18, 14]); line(ctx, [10, 18, 16, 18])
		} else if (name === "cloud") {
			ctx.beginPath()
			ctx.moveTo(7, 20)
			ctx.bezierCurveTo(3, 20, 3, 14, 8, 13)
			ctx.bezierCurveTo(9, 7, 18, 6, 20, 12)
			ctx.bezierCurveTo(26, 12, 27, 20, 21, 20)
			ctx.closePath()
			ctx.stroke()
			line(ctx, [14, 22, 14, 13, 10, 17])
			line(ctx, [14, 13, 18, 17])
		} else if (name === "lock") {
			ctx.beginPath(); ctx.roundedRect(7, 12, 14, 11, 2, 2); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 12, 5, Math.PI, 0); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 17, 1.5, 0, Math.PI * 2); ctx.fill()
			line(ctx, [14, 18.5, 14, 21])
		} else if (name === "home") {
			line(ctx, [5, 13, 14, 5, 23, 13])
			ctx.beginPath(); ctx.moveTo(8, 12); ctx.lineTo(8, 23); ctx.lineTo(20, 23); ctx.lineTo(20, 12); ctx.stroke()
			ctx.strokeRect(12, 17, 4, 6)
		} else if (name === "site") {
			ctx.beginPath(); ctx.arc(14, 11, 7, Math.PI, 0); ctx.quadraticCurveTo(21, 18, 14, 25); ctx.quadraticCurveTo(7, 18, 7, 11); ctx.stroke()
			ctx.beginPath(); ctx.arc(14, 11, 2.5, 0, Math.PI * 2); ctx.stroke()
		} else if (name === "map") {
			line(ctx, [4, 7, 10, 4, 18, 7, 24, 4, 24, 21, 18, 24, 10, 21, 4, 24, 4, 7])
			line(ctx, [10, 4, 10, 21]); line(ctx, [18, 7, 18, 24])
		} else if (name === "stats") {
			line(ctx, [6, 23, 6, 15]); line(ctx, [12, 23, 12, 8]); line(ctx, [18, 23, 18, 12]); line(ctx, [24, 23, 24, 5])
		} else if (name === "more") {
			ctx.beginPath(); ctx.arc(7, 14, 2, 0, Math.PI * 2); ctx.fill()
			ctx.beginPath(); ctx.arc(14, 14, 2, 0, Math.PI * 2); ctx.fill()
			ctx.beginPath(); ctx.arc(21, 14, 2, 0, Math.PI * 2); ctx.fill()
		} else if (name === "search") {
			ctx.beginPath(); ctx.arc(12, 12, 7, 0, Math.PI * 2); ctx.stroke()
			line(ctx, [17, 17, 23, 23])
		} else if (name === "settings") {
			line(ctx, [5, 8, 23, 8]); line(ctx, [5, 14, 23, 14]); line(ctx, [5, 20, 23, 20])
			ctx.beginPath(); ctx.arc(10, 8, 2.5, 0, Math.PI * 2); ctx.fill()
			ctx.beginPath(); ctx.arc(18, 14, 2.5, 0, Math.PI * 2); ctx.fill()
			ctx.beginPath(); ctx.arc(12, 20, 2.5, 0, Math.PI * 2); ctx.fill()
		} else if (name === "export") {
			line(ctx, [6, 16, 6, 23, 22, 23, 22, 16])
			line(ctx, [14, 19, 14, 5, 9, 10]); line(ctx, [14, 5, 19, 10])
		} else if (name === "import") {
			line(ctx, [6, 16, 6, 23, 22, 23, 22, 16])
			line(ctx, [14, 5, 14, 19, 9, 14]); line(ctx, [14, 19, 19, 14])
		} else {
			ctx.beginPath(); ctx.arc(14, 14, 8, 0, Math.PI * 2); ctx.stroke()
		}
		ctx.restore()
	}

	Image {
		id: gasContainerSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.08))
		visible: icon.name === "gas"
		source: "qrc:/qml/container-16494765.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: gasContainerSource
		visible: gasContainerSource.visible
		source: gasContainerSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: slateSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		visible: icon.name === "slate"
		source: "qrc:/qml/slate-7717132.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: slateSource
		visible: slateSource.visible
		source: slateSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: regulatorSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		visible: icon.name === "regulator"
		source: "qrc:/qml/regulator-5158240.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: regulatorSource
		visible: regulatorSource.visible
		source: regulatorSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: gearSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		visible: icon.name === "gear"
		source: "qrc:/qml/sports-15710848.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: gearSource
		visible: gearSource.visible
		source: gearSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: diveTypeSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		visible: icon.name === "type"
		source: "qrc:/qml/no-diving-2483459.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
		mirror: true
	}

	ColorOverlay {
		anchors.fill: diveTypeSource
		visible: diveTypeSource.visible
		source: diveTypeSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: plannerSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		visible: icon.name === "planner"
		source: "qrc:/qml/water-14053108.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: plannerSource
		visible: plannerSource.visible
		source: plannerSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: diveComputerSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		visible: icon.name === "diveComputer"
		source: "qrc:/qml/dive-computer-1922948.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: diveComputerSource
		visible: diveComputerSource.visible
		source: diveComputerSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: divesSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		// Keep "tank" as a compatibility alias, but always render the current
		// Dives artwork. This prevents a dynamic legacy key from reviving the old
		// oxygen-tank icon in an otherwise updated Neo surface.
		visible: icon.name === "dives" || icon.name === "tank"
		source: "qrc:/qml/air-tank-17916416.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: divesSource
		visible: divesSource.visible
		source: divesSource
		color: icon.iconColor
		cached: true
	}

	Image {
		id: equipmentItemSource
		anchors.fill: parent
		anchors.margins: Math.max(1, Math.round(parent.width * 0.06))
		visible: icon.name === "equipmentItem"
		source: "qrc:/qml/tank-14116551.png"
		fillMode: Image.PreserveAspectFit
		smooth: true
		mipmap: true
	}

	ColorOverlay {
		anchors.fill: equipmentItemSource
		visible: equipmentItemSource.visible
		source: equipmentItemSource
		color: icon.iconColor
		cached: true
	}
}
