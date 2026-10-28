// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import org.subsurfacedivelog.mobile 1.0
import ".." as Modern

Canvas {
	id: chart
	property var points: []
	property bool inspectable: true
	property var selectedPoint: null
	readonly property int lengthUnit: Backend.length
	onLengthUnitChanged: requestPaint()
	Modern.DesignTokens { id: tokens }
	Connections {
		target: tokens
		function onAccentChanged() { chart.requestPaint() }
		function onTextMutedChanged() { chart.requestPaint() }
		function onBorderChanged() { chart.requestPaint() }
	}
	readonly property real leftPadding: 52
	readonly property real topPadding: 12
	readonly property real rightPadding: 18
	readonly property real bottomPadding: 34
	function clock(seconds) { return Math.floor(seconds / 60) + ":" + (Math.round(seconds) % 60 < 10 ? "0" : "") + Math.round(seconds) % 60 }
	function maximumTime() { return points.length ? Math.max(1, points[points.length - 1].time) : 1 }
	function maximumDepth() { var m = 1000; for (var p of points) m = Math.max(m, p.depth); return m * 1.1 }
	onPointsChanged: { selectedPoint = null; requestPaint() }
	onWidthChanged: requestPaint()
	onHeightChanged: requestPaint()
	onPaint: {
		var c = getContext("2d"); c.reset()
		var w = Math.max(1, width-leftPadding-rightPadding), h = Math.max(1, height-topPadding-bottomPadding)
		var tm=maximumTime(), dm=maximumDepth(), divisor=Backend.length===Enums.METERS?1000:304.8
		c.font="11px sans-serif";c.fillStyle=tokens.textMuted;c.strokeStyle=tokens.border;c.lineWidth=1
		for(var i=0;i<=4;i++) {
			var y=topPadding+h*i/4;c.beginPath();c.moveTo(leftPadding,y);c.lineTo(leftPadding+w,y);c.stroke()
			c.textAlign="right";c.fillText((dm*i/4/divisor).toFixed(0),leftPadding-7,y+4)
			var x=leftPadding+w*i/4;c.textAlign="center";c.fillText(clock(tm*i/4),x,topPadding+h+20)
		}
		c.strokeStyle=tokens.accent;c.lineWidth=2;c.beginPath()
		for(var j=0;j<points.length;j++) {
			var p=points[j],px=leftPadding+p.time/tm*w,py=topPadding+p.depth/dm*h
			if(j===0)c.moveTo(px,py);else c.lineTo(px,py)
		}
		c.stroke();c.fillStyle="#FB923C"
		for(var checkpoint of points) if(checkpoint.ceiling!==undefined&&checkpoint.ceiling>0) {
			c.beginPath();c.arc(leftPadding+checkpoint.time/tm*w,topPadding+checkpoint.ceiling/dm*h,3,0,Math.PI*2);c.fill()
		}
	}
	MouseArea {
		anchors.fill: parent; enabled: chart.inspectable; hoverEnabled: true
		function inspect(x) {
			var target=(x-chart.leftPadding)/Math.max(1,width-chart.leftPadding-chart.rightPadding)*chart.maximumTime(),nearest=null
			for(var p of chart.points)if(!nearest||Math.abs(p.time-target)<Math.abs(nearest.time-target))nearest=p
			chart.selectedPoint=nearest
		}
		onPositionChanged: function(mouse) { inspect(mouse.x) }
		onClicked: function(mouse) { inspect(mouse.x) }
		onExited: chart.selectedPoint=null
	}
	ToolTip {
		visible: chart.inspectable&&chart.selectedPoint!==null
		text: chart.selectedPoint ? chart.clock(chart.selectedPoint.time)+" · "+(chart.selectedPoint.depth/(Backend.length===Enums.METERS?1000:304.8)).toFixed(1)+" "+(Backend.length===Enums.METERS?qsTr("m"):qsTr("ft"))+" · "+(chart.selectedPoint.gas||"") : ""
	}
}
