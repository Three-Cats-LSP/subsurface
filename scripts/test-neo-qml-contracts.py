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
		'if (neoDesktopShellActive) {',
		'pageStack.push(page)',
		'pageStack.lastItem?.objectName',
	),
	"Neo navigation",
)

stats_manager = source("mobile-widgets/statsmanager.cpp")
require(
	stats_manager,
	(
		"void StatsManager::setDarkThemeOverride(bool enabled)",
		"darkThemeOverride || theme == \"Dark\"",
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
