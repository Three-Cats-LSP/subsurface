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


def forbid(text: str, fragments: tuple[str, ...], label: str) -> None:
	present = [fragment for fragment in fragments if fragment in text]
	if present:
		raise SystemExit(f"{label} contains incompatible contract fragments: {present}")


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
		'function neoPageUsesOwnHeader(page)',
		'pageStack.globalToolBar.preferredHeight: neoHeaderSuppressed ? 0',
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
		'activeFocusOnTab: height > 0',
		'Keys.onReturnPressed: activateDelegate()',
		'Accessible.onPressAction: activateDelegate()',
	),
	"Neo populated dive list",
)
forbid(
	dive_list,
	('\t\t\tdelegate: Item {\n\t\t\t\tid: delegateRoot\n\t\t\t\trequired property int index',),
	"Neo populated dive list",
)

dive_details = source("mobile-widgets/qml/modern/pages/ModernDiveDetails.qml")
require(
	dive_details,
	(
		'property var modelData: ({',
		'"getCylinder": model.getCylinder',
		'"cylinderList": model.cylinderList',
		'"diveMode": model.diveMode',
		'function primaryGas()',
		'function cylinderGasSummary()',
		'label: qsTr("Gas & cylinder")',
		'label: qsTr("Gear")',
		'iconName: "type"',
		'Accessible.name: qsTr("Dive details for %1")',
	),
	"Neo populated dive details",
)
forbid(
	dive_details,
	(
		'\t\tdelegate: Item {\n\t\t\tid: delegateRoot\n\t\t\trequired property int index',
		'onPinchCanceled:',
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
		'model: page.profileLabels',
		'return analysis[name] !== undefined ? analysis[name] : fallback',
		'property var timeline: []',
		'property int runtimeSeconds: 0',
		'text: qsTr("Full plan")',
		'qsTr("Switch → %1")',
		'qsTr("Runtime: %1  Bottom profile: %2  Decompression stops: %3")',
		'text: qsTr("O₂ %")',
		'text: qsTr("He %")',
		'qsTr("Deco switch %1 (pO₂ %2 bar)")',
		'qsTr("Bottom MOD %1 (pO₂ %2 bar)")',
		'decoReference.decoSwitch || decoReference.mod || "—"',
		'placeholderText: qsTr("Runtime at segment end (min)")',
		'text: qsTr("Oxygen %:")',
		'text: qsTr("Helium %:")',
	),
	"Neo planner accessibility",
)

neo_spin_box = source("mobile-widgets/qml/modern/components/NeoSpinBox.qml")
neo_combo_box = source("mobile-widgets/qml/modern/components/NeoComboBox.qml")
neo_text_field = source("mobile-widgets/qml/modern/components/NeoTextField.qml")
neo_icon = source("mobile-widgets/qml/modern/components/NeoDiveIcon.qml")
require(neo_spin_box, ('Accessible.name: accessibleName.length > 0 ? accessibleName : qsTr("Numeric value")',), "Neo numeric accessibility")
require(
	neo_combo_box,
	(
		'Accessible.role: Accessible.ComboBox',
		'MouseArea {',
		'anchors.fill: parent',
		'preventStealing: true',
		'z: 1000',
		'enabled: control.enabled',
		'fieldMouseArea.mapToItem(control.contentItem',
		'control.popup.open()',
	),
	"Neo selection accessibility",
)
require(
	neo_text_field,
	(
		'property bool floatingLabelVisible:',
		'placeholderTextColor: control.floatingLabelVisible ? "transparent"',
		'id: floatingLabelBackground',
		'color: control.activeFocus ? tokens.accent : tokens.textMuted',
	),
	"Neo outlined field labels",
)
require(
	neo_icon,
	(
		'import Qt5Compat.GraphicalEffects',
		'source: "qrc:/qml/container-16494765.png"',
		'source: "qrc:/qml/regulator-5158240.png"',
		'source: "qrc:/qml/sports-15710848.png"',
		'source: "qrc:/qml/no-diving-2483459.png"',
		'source: "qrc:/qml/water-14053108.png"',
		'source: "qrc:/qml/dive-computer-1922948.png"',
		'source: "qrc:/qml/air-tank-17916416.png"',
		'visible: icon.name === "dives" || icon.name === "tank"',
		'source: "qrc:/qml/tank-14116551.png"',
		'ColorOverlay {',
	),
	"Neo attributed dive icon",
)

dive_details = source("mobile-widgets/qml/modern/pages/ModernDiveDetails.qml")
require(
	dive_details,
	(
		'"visibilityDistance": model.visibilityDistance',
		'text: qsTr("Rating: %1/5")',
		'text: qsTr("Visibility: %1")',
		'label: qsTr("Gas & cylinder")',
		'value: delegateRoot.cylinderGasSummary()',
		'label: qsTr("Gear")',
		'qsTr("Weights: %1")',
		'iconName: "gear"',
	),
	"Neo combined cylinder and gear cards",
)

for dive_count_surface in (
	"mobile-widgets/qml/MapPage.qml",
	"mobile-widgets/qml/modern/pages/ModernDashboard.qml",
	"mobile-widgets/qml/modern/pages/ModernSitesHub.qml",
	"mobile-widgets/qml/modern/pages/ModernStatisticsHub.qml",
	"mobile-widgets/qml/modern/components/NeoDesktopSidebar.qml",
	"mobile-widgets/qml/modern/components/NeoBottomNavigation.qml",
):
	content = source(dive_count_surface)
	if '"tank"' in content:
		raise AssertionError(f"Old dive-count tank icon remains in {dive_count_surface}")
	if '"dives"' not in content:
		raise AssertionError(f"New dive-count icon missing from {dive_count_surface}")

dive_editor = source("mobile-widgets/qml/modern/pages/ModernDiveEditor.qml")
require(
	dive_editor,
	(
		'Text { text: qsTr("Dive classification")',
		'id: modeBox',
		'id: typeBox',
		'composedTags(), modeBox.currentText',
		'name: "regulator"',
		'name: "type"',
		'id: diveGuideField',
		'id: weightField',
		'id: ratingBox',
		'id: visibilityField',
		'text: qsTr("Underwater visibility:")',
		'dive.visibilityDistance',
	),
	"Neo dive classification editor",
)
for removed_editor_route in (
	'advancedEditorRequested',
	'Open full equipment editor',
):
	if removed_editor_route in dive_editor:
		raise AssertionError(f"Neo dive editor still exposes legacy full editor route: {removed_editor_route}")

mobile_model = source("qt-models/mobilelistmodel.cpp")
manager = source("mobile-widgets/qmlmanager.cpp")
require(mobile_model, ('roles[DiveModeRole] = "diveMode"',), "Neo dive mode role")
require(mobile_model, ('roles[VisibilityDistanceRole] = "visibilityDistance"',), "Neo visibility distance role")
require(
	manager,
	(
		'QString tags, QString diveMode, QString weight',
		'd->dcs[0].divemode = static_cast<divemode_t>(requestedMode)',
		'd->visibility_distance.mm = requestedVisibilityMm',
	),
	"Neo dive mode persistence",
)

profile_scene = source("profile-widget/profilescene.cpp")
qml_profile = source("profile-widget/qmlprofile.cpp")
require(
	profile_scene,
	(
		'ScopedProfileColorTheme neoColors(neoTheme);',
		'image.fill(getColor(::BACKGROUND, isGrayscale));',
	),
	"Neo profile palette",
)
require(qml_profile, ('ScopedProfileColorTheme neoColors(true);', 'm_profileWidget->setNeoTheme(true);'), "Neo QML profile theme bridge")

color_source = source("core/color.cpp")
require(
	color_source,
	(
		'static thread_local bool use_neo_profile_colors = false;',
		'static QColor neoProfileColor(color_index_t i)',
		'return QColor("#06111E");',
		'if (use_neo_profile_colors && !isGrayscale)',
	),
	"Neo native profile renderer palette",
)

cmake = source("CMakeLists.txt")
require(
	cmake,
	(
		'elseif(WIN32)',
		'add_executable(${SUBSURFACE_TARGET} WIN32 ${SUBSURFACE_PKG} ${SUBSURFACE_RESOURCES})',
	),
	"Neo Windows GUI target",
)

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

print("Neo responsive, form, profile, icon, and Windows packaging contracts validated")
