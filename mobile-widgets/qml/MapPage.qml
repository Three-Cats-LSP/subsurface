// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning
import org.subsurfacedivelog.mobile 1.0
import org.kde.kirigami as Kirigami
import "modern" as Modern
import "modern/components" as Components

Kirigami.Page {
	id: mapPage
	objectName: "MapPage"
	title: qsTr("Map")
	leftPadding: tokens.space16
	topPadding: tokens.space12
	rightPadding: tokens.space16
	bottomPadding: tokens.space12
	background: Rectangle { color: tokens.background }

	property bool firstRun: true
	property bool wideLayout: width >= 760
	property string siteFilter: ""
	signal openSites()

	Modern.DesignTokens { id: tokens }

	function mappedSiteCount() {
		var count = 0
		for (var i = 0; i < manager.locationList.length; ++i) {
			var summary = manager.siteSummary(manager.locationList[i])
			if (summary.gps && summary.gps.length > 0)
				++count
		}
		return count
	}

	function loggedDiveCount() {
		var count = 0
		for (var i = 0; i < manager.locationList.length; ++i)
			count += manager.siteSummary(manager.locationList[i]).diveCount || 0
		return count
	}

	ColumnLayout {
		anchors.fill: parent
		spacing: tokens.space12

		RowLayout {
			Layout.fillWidth: true
			spacing: tokens.space12
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 2
				Text {
					text: qsTr("Dive map")
					color: tokens.textPrimary
					font.pixelSize: mapPage.wideLayout ? 30 : 24
					font.weight: Font.DemiBold
				}
				Text {
					Layout.fillWidth: true
					text: qsTr("Explore the locations stored in your dive log")
					color: tokens.textSecondary
					font.pixelSize: 12
					elide: Text.ElideRight
				}
			}
			Components.NeoButton {
				text: mapPage.wideLayout ? qsTr("Manage sites") : qsTr("Sites")
				variant: "secondary"
				compact: !mapPage.wideLayout
				onClicked: mapPage.openSites()
			}
		}

		GridLayout {
			objectName: "NeoMapMetrics"
			Layout.fillWidth: true
			columns: 3
			columnSpacing: mapPage.wideLayout ? tokens.space12 : tokens.space8
			Components.MetricCard {
				label: qsTr("Sites")
				value: String(manager.locationList.length)
				iconName: "site"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				label: qsTr("Mapped")
				value: String(mapPage.mappedSiteCount())
				iconName: "map"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
			Components.MetricCard {
				label: qsTr("Logged dives")
				value: String(mapPage.loggedDiveCount())
				iconName: "dives"
				Layout.fillWidth: true
				Layout.minimumWidth: 0
			}
		}

		RowLayout {
			Layout.fillWidth: true
			Layout.fillHeight: true
			Layout.minimumHeight: 260
			spacing: tokens.space12

			Components.ModernCard {
				objectName: "NeoMapSitePanel"
				visible: mapPage.wideLayout
				Layout.preferredWidth: 286
				Layout.fillHeight: true
				contentPadding: tokens.space12

				Text {
					text: qsTr("Dive sites")
					color: tokens.textPrimary
					font.pixelSize: 17
					font.weight: Font.DemiBold
				}

				Components.NeoTextField {
					Layout.fillWidth: true
					placeholderText: qsTr("Search mapped sites")
					onTextEdited: mapPage.siteFilter = text.trim().toLowerCase()
				}

				Text {
					visible: manager.locationList.length === 0
					Layout.fillWidth: true
					text: qsTr("No locations have been recorded yet.")
					color: tokens.textSecondary
					font.pixelSize: 12
					wrapMode: Text.WordWrap
				}

				ListView {
					Layout.fillWidth: true
					Layout.fillHeight: true
					clip: true
					spacing: tokens.space8
					model: manager.locationList
					delegate: Rectangle {
						id: siteRow
						required property string modelData
						property var summary: manager.siteSummary(modelData)
						property bool matches: mapPage.siteFilter.length === 0 || modelData.toLowerCase().indexOf(mapPage.siteFilter) >= 0
						width: ListView.view.width
						height: matches ? 66 : 0
						visible: matches
						color: siteHover.hovered ? tokens.surfaceRaised : tokens.background
						radius: tokens.radiusSmall
						border.width: 1
						border.color: summary.gps && summary.gps.length > 0 ? tokens.border : "transparent"
						RowLayout {
							anchors.fill: parent
							anchors.margins: tokens.space8
							spacing: tokens.space8
							Components.NeoDiveIcon {
								name: "site"
								iconColor: siteRow.summary.gps && siteRow.summary.gps.length > 0 ? tokens.accent : tokens.textMuted
								Layout.preferredWidth: 24
								Layout.preferredHeight: 24
							}
							ColumnLayout {
								Layout.fillWidth: true
								spacing: 2
								Text { Layout.fillWidth: true; text: siteRow.modelData; color: tokens.textPrimary; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
								Text {
									text: siteRow.summary.gps && siteRow.summary.gps.length > 0 ? siteRow.summary.gps : qsTr("Location unavailable")
									color: siteRow.summary.gps && siteRow.summary.gps.length > 0 ? tokens.textSecondary : tokens.textMuted
									font.pixelSize: 10
									elide: Text.ElideRight
								}
							}
							Text { text: String(siteRow.summary.diveCount || 0); color: tokens.accent; font.pixelSize: 12; font.weight: Font.DemiBold }
						}
						HoverHandler { id: siteHover }
						TapHandler {
							enabled: siteRow.summary.gps && siteRow.summary.gps.length > 0
							onTapped: mapPage.centerOnDiveSite(manager.siteObject(siteRow.modelData))
						}
					}
				}
			}

			Rectangle {
				objectName: "NeoMapCanvas"
				Layout.fillWidth: true
				Layout.fillHeight: true
				Layout.minimumWidth: 0
				color: tokens.surface
				radius: tokens.radiusMedium
				border.width: 1
				border.color: tokens.border
				clip: true

				MapWidget {
					id: mapWidget
					anchors.fill: parent
					anchors.margins: 1
					onSelectedDivesChanged: {
						if (list.length === 0) {
							console.warn("main.qml: onSelectedDivesChanged(): received empty list!")
							return
						}
						var id = list[0]
						var idx = diveModel.getIdxForId(id)
						if (idx === -1) {
							console.warn("main.qml: onSelectedDivesChanged(): cannot find list index for dive id:", id)
							return
						}
						diveList.setCurrentDiveListIndex(idx, true)
					}
					Component.onCompleted: {
						mapWidget.map.zoomLevel = mapWidget.map.defaultZoomOut
						mapWidget.map.center = mapWidget.map.defaultCenter
					}
				}

				Rectangle {
					anchors.fill: parent
					color: "#2606111E"
					enabled: false
				}

				Rectangle {
					anchors.left: parent.left
					anchors.right: parent.right
					anchors.bottom: parent.bottom
					height: 38
					color: "#D906111E"
					RowLayout {
						anchors.fill: parent
						anchors.leftMargin: tokens.space12
						anchors.rightMargin: tokens.space12
						Components.NeoDiveIcon { name: "map"; iconColor: tokens.accent; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
						Text { text: qsTr("Native offline-capable map"); color: tokens.textPrimary; font.pixelSize: 11; Layout.fillWidth: true }
						Text { visible: mapPage.wideLayout; text: qsTr("Scroll or pinch to zoom"); color: tokens.textMuted; font.pixelSize: 10 }
					}
				}
			}
		}
	}

	function reloadMap() {
		mapWidget.mapHelper.reloadMapLocations()
	}

	function centerOnDiveSite(ds) {
		if (!ds) {
			console.warn("main.qml: centerOnDiveSite(): dive site is undefined!")
			return
		}
		if (firstRun) {
			var coord = mapWidget.mapHelper.getCoordinates(ds)
			centerOnLocationHard(coord.latitude, coord.longitude)
			firstRun = false
		}
		mapWidget.mapHelper.centerOnDiveSite(ds)
	}

	function centerOnLocation(lat, lon) {
		if (firstRun) {
			centerOnLocationHard(lat, lon)
			firstRun = false
			return
		}
		mapWidget.map.centerOnCoordinate(QtPositioning.coordinate(lat, lon))
	}

	function centerOnLocationHard(lat, lon) {
		mapWidget.map.zoomLevel = mapWidget.map.defaultZoomIn
		mapWidget.map.center = QtPositioning.coordinate(lat, lon)
	}
}
