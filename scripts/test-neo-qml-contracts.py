#!/usr/bin/env python3
"""Fast structural regression checks for the responsive Neo native shell."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def source(relative: str) -> str:
	return (ROOT / relative).read_text(encoding="utf-8")


def require(text: str, fragments: tuple[str, ...], label: str) -> None:
	missing = [fragment for fragment in fragments if fragment not in text]
	if missing:
		raise SystemExit(f"{label} is missing responsive contract fragments: {missing}")


map_qml = source("mobile-widgets/qml/MapPage.qml")
require(
	map_qml,
	(
		'objectName: "MapPage"',
		'property bool wideLayout: width >= 760',
		'objectName: "NeoMapMetrics"',
		'objectName: "NeoMapSitePanel"',
		'visible: mapPage.wideLayout',
		'objectName: "NeoMapCanvas"',
		'Layout.fillHeight: true',
		'signal openSites()',
	),
	"Neo map",
)

statistics_qml = source("mobile-widgets/qml/modern/pages/ModernStatisticsHub.qml")
require(
	statistics_qml,
	(
		'objectName: "ModernStatisticsHub"',
		'property bool wideLayout: width >= 760',
		'objectName: "NeoStatisticsMetrics"',
		'columns: page.wideLayout ? 4 : 2',
		'objectName: "NeoStatisticsChartCard"',
		'objectName: "NeoStatisticsControls"',
		'columns: page.wideLayout ? 3 : 2',
		'statsManager.setDarkThemeOverride(true)',
		'statsManager.chartTitle.length > 0',
		'statsManager.chartSubtype.length > 0',
	),
	"Neo statistics",
)

main_qml = source("mobile-widgets/qml/main.qml")
require(
	main_qml,
	(
		'onMapRequested: showPageFromDrawer(mapPage)',
		'onStatisticsRequested: showPageFromDrawer(neoStatisticsHub)',
		'onOpenSites: showPageFromDrawer(neoSitesHub)',
		'onPortabilityRequested: showPageFromDrawer(neoDataPortability)',
		'if (neoDesktopShellActive) {',
		'pageStack.push(page)',
		'pageStack.lastItem?.objectName',
	),
	"Neo navigation",
)

desktop_sidebar = source("mobile-widgets/qml/modern/components/NeoDesktopSidebar.qml")
require(
	desktop_sidebar,
	(
		'signal portabilityRequested()',
		'{ key: "portability", label: qsTr("Data & Backup"), icon: "export" }',
		'case "portability": sidebar.portabilityRequested(); break',
	),
	"Neo desktop data navigation",
)

dive_list = source("mobile-widgets/qml/modern/pages/ModernDiveList.qml")
require(
	dive_list,
	(
		'property var modelData: ({',
		'"location": model.location',
		'"tripTitle": model.tripTitle',
		'Accessible.role: Accessible.ListItem',
	),
	"Neo populated dive list",
)

dive_details = source("mobile-widgets/qml/modern/pages/ModernDiveDetails.qml")
require(
	dive_details,
	(
		'property var modelData: ({',
		'"getCylinder": model.getCylinder',
		'"cylinderList": model.cylinderList',
		'Accessible.name: qsTr("Dive details for %1")',
	),
	"Neo populated dive details",
)

portability = source("mobile-widgets/qml/modern/pages/ModernDataPortability.qml")
require(
	portability,
	(
		'function backupSummary()',
		'if (backupInspection.neoPackage)',
		'.arg(backupInspection.fileName).arg(backupInspection.dives).arg(backupInspection.sites)',
		'text: page.backupSummary()',
	),
	"Neo backup inspection summary",
)

planner = source("mobile-widgets/qml/modern/pages/ModernPlannerLab.qml")
require(
	planner,
	(
		'accessibleName: qsTr("Decompression model")',
		'accessibleName: qsTr("GF low")',
		'accessibleName: qsTr("Dive mode")',
		'accessibleName: qsTr("Reserve gas (%1)").arg(page.pressureUnit)',
		'accessibleName: qsTr("Gas for segment %1").arg(index + 1)',
	),
	"Neo planner accessibility",
)

neo_spin_box = source("mobile-widgets/qml/modern/components/NeoSpinBox.qml")
neo_combo_box = source("mobile-widgets/qml/modern/components/NeoComboBox.qml")
require(neo_spin_box, ('Accessible.name: accessibleName.length > 0 ? accessibleName : qsTr("Numeric value")',), "Neo numeric accessibility")
require(neo_combo_box, ('Accessible.role: Accessible.ComboBox',), "Neo selection accessibility")

stats_manager = source("mobile-widgets/statsmanager.cpp")
require(
	stats_manager,
	(
		"void StatsManager::setDarkThemeOverride(bool enabled)",
		"darkThemeOverride || theme == \"Dark\"",
		"chart.id == uiState.charts.selected",
	),
	"Neo statistics theme bridge",
)

stats_colors = source("stats/statscolors.cpp")
require(
	stats_colors,
	(
		'backgroundColor = QColor("#06111E")',
		'borderColor = QColor("#22D4EB")',
		'gridColor = QColor("#1E3B50")',
	),
	"Neo statistics palette",
)

print("Neo map, statistics, and navigation responsive contracts validated")
