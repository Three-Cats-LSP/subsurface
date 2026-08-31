// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Layouts
import org.subsurfacedivelog.mobile 1.0
import org.kde.kirigami as Kirigami
import QtQuick.Templates as QtQuickTemplates
import "modern/pages" as NeoPages
import "modern/components" as NeoComponents

Kirigami.ApplicationWindow {
	id: rootItem
	title: qsTr("Subsurface Neo")
	wideScreen: false // workaround for probably Kirigami bug. See commits.

	// the documentation claims that the ApplicationWindow should pick up the font set on
	// the C++ side. But as a matter of fact, it doesn't, unless you add this line:
	font: Qt.application.font
	background: Rectangle { color: "#0B1220" }
	readonly property int neoAvailableWidth: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : Screen.width
	readonly property int neoAvailableHeight: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : Screen.height
	// Keep Qt Quick Controls in step with the Neo shell. Several mature pages
	// use these controls for forms and actions.
	Material.theme: Material.Dark
	Material.primary: "#111B2E"
	Material.accent: "#44C7F4"
	Overlay.modal: Rectangle { color: "#99050A14" }
	Overlay.modeless: Rectangle { color: "#66050A14" }

	// Keep the application frame visually closed even when the page or desktop
	// sidebar reaches the native window edge.
	Rectangle {
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.bottom: parent.bottom
		height: 2
		color: "#1E3B50"
		z: 10000
	}

	Shortcut {
		sequence: "Tab"
		enabled: pageStack.currentItem === neoAboutPage && neoAboutPage.keyboardEntryPending
		onActivated: neoAboutPage.focusPrimaryAction()
	}

	footer: NeoComponents.NeoBottomNavigation {
		id: neoBottomNavigation
		visible: initialized && !neoDesktopShellActive && !neoSubsurfaceCloudSetup.visible && !neoOnboarding.visible &&
			rootItem.neoShowsBottomNavigation(pageStack.currentItem)
		currentSection: rootItem.neoBottomSectionForPage(pageStack.currentItem)
		onDivesRequested: showPageFromDrawer(modernDiveList)
		onSitesRequested: showPageFromDrawer(neoSitesHub)
		onStatsRequested: showPageFromDrawer(neoStatisticsHub)
		onMoreRequested: showPageFromDrawer(neoMorePage)
	}

	NeoComponents.NeoDesktopSidebar {
		id: neoDesktopSidebar
		visible: rootItem.neoDesktopShellActive
		width: rootItem.neoSidebarWidth
		anchors.left: parent.left
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		z: 100
		currentSection: rootItem.neoSectionForPage(pageStack.currentItem)
		accountText: PrefCloudStorage.cloud_storage_email
		statusText: manager.syncState
		onDivesRequested: showPageFromDrawer(modernDiveList)
		onPlansRequested: showPageFromDrawer(modernPlansList)
		onSitesRequested: showPageFromDrawer(neoSitesHub)
		onMapRequested: showPageFromDrawer(mapPage)
		onStatisticsRequested: showPageFromDrawer(neoStatisticsHub)
		onEquipmentRequested: showPageFromDrawer(neoEquipmentLibrary)
		onPlannerRequested: showPageFromDrawer(neoPlannerLab)
		onImportRequested: showPageFromDrawer(neoDiveComputerCenter)
		onPortabilityRequested: showPageFromDrawer(neoDataPortability)
		onSettingsRequested: showPageFromDrawer(neoSettingsHub)
		onCloudRequested: showPageFromDrawer(cloudSyncPage)
	}

	// we want to use our own colors for Kirigami, so let's define our colorset
	Kirigami.Theme.inherit: false
	Kirigami.Theme.colorSet: Kirigami.Theme.Button
	Kirigami.Theme.backgroundColor: "#0B1220"
	Kirigami.Theme.textColor: "#F7FAFC"
	Kirigami.Theme.highlightColor: "#169DD0"
	Kirigami.Theme.highlightedTextColor: "#F7FAFC"

	function neoPageUsesOwnHeader(page) {
		if (!page)
			return false
		if (page === modernDiveList || page === modernPlansList || page === neoMorePage ||
			page === neoSettingsHub || page === neoAboutPage || page === neoAccountSecurityPage ||
			page === neoDiveComputerCenter || page === neoPlannerLab || page === neoOperationsHub ||
			page === neoEquipmentLibrary || page === neoDataPortability || page === neoSitesHub ||
			page === neoStatisticsHub || page === cloudSyncPage || page === mapPage)
			return true
		return String(page.objectName || "").indexOf("Modern") === 0
	}

	readonly property bool neoHeaderSuppressed: neoPageUsesOwnHeader(pageStack.currentItem)

	// Neo pages carry their own responsive title and actions. Keep Kirigami's
	// toolbar only for mature compatibility pages that still rely on it.
	pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.ToolBar
	pageStack.globalToolBar.showNavigationButtons: Kirigami.ApplicationHeaderStyle.NoNavigationButtons
	pageStack.globalToolBar.canContainHandles: true
	pageStack.globalToolBar.minimumHeight: neoHeaderSuppressed ? 0 : Kirigami.Units.gridUnit * 1.6
	pageStack.globalToolBar.preferredHeight: neoHeaderSuppressed ? 0 : Math.round(Kirigami.Units.gridUnit * (Qt.platform.os == "ios" ? 2.5 : 2))
	pageStack.globalToolBar.maximumHeight: pageStack.globalToolBar.preferredHeight
	pageStack.anchors.leftMargin: neoDesktopShellActive ? neoSidebarWidth : 0

	// expose header colors so Kirigami's AbstractApplicationHeader can read them
	// (on iOS, items with inherit:false get system palette colors instead of app theme)
	property color headerBackgroundColor: "#111B2E"
	property color headerTextColor: "#F7FAFC"

	property alias notificationText: manager.notificationText
	property alias pluggedInDeviceName: manager.pluggedInDeviceName
	property alias defaultCylinderIndex: settingsWindow.defaultCylinderIndex
	property bool filterToggle: false
	property string filterPattern: ""
	property int colWidth: undefined
	property bool neoSubsurfaceCloudSetupRequested: false
	property bool neoSubsurfaceCloudSetupFromSettings: false
	readonly property int neoDesktopBreakpoint: 820
	readonly property int neoSidebarWidth: 232
	readonly property bool neoDesktopShellActive: initialized &&
		Qt.platform.os !== "android" && Qt.platform.os !== "ios" &&
		width >= neoDesktopBreakpoint && !neoSubsurfaceCloudSetup.visible && !neoOnboarding.visible
	readonly property int neoContentWidth: Math.max(1, width - (neoDesktopShellActive ? neoSidebarWidth : 0))

	function neoSectionForPage(page) {
		if (page?.objectName === "ModernDiveDetails" && page.navigationSection === "plans")
			return "plans"
		if (page === modernDiveList || page?.objectName === "ModernDiveDetails" || page?.objectName === "ModernDiveEditor")
			return "dives"
		if (page === modernPlansList)
			return "plans"
		if (page === neoSitesHub)
			return "sites"
		if (page === mapPage)
			return "map"
		if (page === neoStatisticsHub || page === statistics)
			return "statistics"
		if (page === neoEquipmentLibrary)
			return "equipment"
		if (page === neoPlannerLab)
			return "planner"
		if (page === neoDiveComputerCenter)
			return "import"
		if (page === neoDataPortability)
			return "portability"
		if (page === neoSettingsHub || page === settingsWindow || page === neoAboutPage || page === neoAccountSecurityPage)
			return "settings"
		return ""
	}

	function neoShowsBottomNavigation(page) {
		return page === modernDiveList || page === modernPlansList || page === neoSitesHub ||
			page === neoStatisticsHub || page === mapPage || page === statistics ||
			page === neoMorePage || page === neoPlannerLab || page === cloudSyncPage ||
			page === neoDiveComputerCenter || page === neoEquipmentLibrary || page === neoDataPortability ||
			page === neoSettingsHub || page === neoAboutPage || page === neoAccountSecurityPage
	}

	function neoBottomSectionForPage(page) {
		if (page === modernDiveList)
			return "dives"
		if (page === neoSitesHub || page === mapPage)
			return "sites"
		if (page === neoStatisticsHub || page === statistics)
			return "stats"
		return "more"
	}

	function isBackgroundProgressMessage(message) {
		return message === "Open local dive data file" ||
			message === "populate data model" ||
			message === "finish populating data store" ||
			message === "Create full text index" ||
			/^Processing \d+ dives$/.test(message)
	}

	// signal that the profile (and possibly other code) listens to so they
	// can redraw if settings are changed
	signal settingsChanged()

	// Force Kirigami's Material theme sync after QML initialization.
	// The ThemeInterface constructor fires color signals before QML is loaded,
	// so the Material style onSync handler never sees the initial values.
	// Use Qt.callLater so the re-trigger runs after all components (including
	// the toolbar header) have finished construction and are listening.
	Component.onCompleted: {
		Qt.callLater(function() {
			subsurfaceTheme.currentTheme = subsurfaceTheme.currentTheme
		})
	}

	onNotificationTextChanged: {
		// once the app is fully initialized and the UI is running, we use passive
		// notifications to show the notification text, but during initialization
		// we instead dump the information into the textBlock below
		if (initialized) {
			// Background loading remains in the diagnostic log. It must not cover
			// the home screen with a stack of implementation progress messages.
			if (isBackgroundProgressMessage(notificationText))
				return
			if (notificationText !== "") {
				var actionEnd = notificationText.indexOf("]")
				if (notificationText.startsWith("[") && actionEnd !== -1) {
					// we have a notification text that starts with our special syntax to indication
					// an action that the user can take (the actual action is always opening the context drawer
					// so the action text should always be something that can then be found in the context drawer)
					showPassiveNotification(notificationText.substring(actionEnd + 1), 5000, notificationText.substring(1,actionEnd),
								function() { contextDrawer.open() })
				} else {
					showPassiveNotification(notificationText, 5000)
				}
			}
		} else {
			textBlock.text = textBlock.text + "\n" + notificationText
		}
	}
	visible: false

	BusyIndicator {
		id: busy
		running: false
		height: 6 * Kirigami.Units.gridUnit
		width: 6 * Kirigami.Units.gridUnit
		anchors.centerIn: parent
	}

	function showBusy(msg) {
		if (msg !== undefined && msg !== "")
			showPassiveNotification(msg, 15000) // show for 15 seconds
		busy.running = true
	}

	function hideBusy() {
		busy.running = false
		// hiding notifications is no longer supported???
		// hidePassiveNotification()
	}

	function returnTopPage() {
		showNeoHome()
	}

	function scrollToTop() {
		diveList.scrollToTop()
	}

	// Navigate from the Neo shell or legacy drawer. Neo Home is the
	// stable root; mature Subsurface pages continue to be pushed above it.
	function showPageFromDrawer(page) {
		detailsWindow.endEditMode()
		pageStack.clear()
		// The desktop sidebar is the navigation root. Keeping the dive list under
		// every destination lets Kirigami expose both pages side-by-side on very
		// wide windows, which breaks the single-workspace Neo layout.
		if (neoDesktopShellActive) {
			pageStack.push(page)
			return
		}
		pageStack.push(modernDiveList)
		if (page !== modernDiveList)
			showPage(page)
	}

	function showNeoHome() {
		detailsWindow.endEditMode()
		pageStack.clear()
		pageStack.push(modernDiveList)
	}

	function showPage(page) {
		if (page === statistics) {
			manager.appendTextToLog("switching to statistics page, clearing out stack")
			pageStack.clear()
		}
		if (pageStack.currentItem === statistics) {
			manager.appendTextToLog("switching away from statistics page, clearing out stack")
			pageStack.clear()
		}

		if (page !== mapPage)
			hackToOpenMap = 0 // we really want a different page
		if (globalDrawer.drawerOpen)
			globalDrawer.close()
		var i=pageIndex(page)
		if (i === -1)
			pageStack.push(page)
		else
			pageStack.currentIndex = i
		manager.appendTextToLog("switched to page " + page.title)
	}

	function showMap() {
		showPage(mapPage)
	}

	function showDiveList() {
		showPageFromDrawer(modernDiveList)
	}

	function pageIndex(pageToFind) {
		if (pageStack.contentItem !== null) {
			for (var i = 0; i < pageStack.contentItem.contentChildren.length; i++) {
				if (pageStack.contentItem.contentChildren[i] === pageToFind)
					return i
			}
		}
		return -1
	}

	function startAddDive() {
		detailsWindow.state = "add"
		detailsWindow.dive_id = manager.addDive();
		detailsWindow.number = manager.getNumber(detailsWindow.dive_id)
		detailsWindow.date = manager.getDate(detailsWindow.dive_id)
		detailsWindow.airtemp = ""
		detailsWindow.watertemp = ""
		detailsWindow.buddyModel = manager.buddyList
		detailsWindow.buddyIndex = -1
		detailsWindow.buddyText = ""
		detailsWindow.depth = ""
		detailsWindow.diveguideModel = manager.diveguideList
		detailsWindow.diveguideIndex = -1
		detailsWindow.diveguideText = ""
		detailsWindow.notes = ""
		detailsWindow.location = ""
		detailsWindow.gps = ""
		detailsWindow.duration = ""
		detailsWindow.suitModel = manager.suitList
		detailsWindow.suitIndex = -1
		detailsWindow.suitText = ""
		detailsWindow.cylinderModel0 = manager.cylinderListInit
		detailsWindow.cylinderModel1 = manager.cylinderListInit
		detailsWindow.cylinderModel2 = manager.cylinderListInit
		detailsWindow.cylinderModel3 = manager.cylinderListInit
		detailsWindow.cylinderModel4 = manager.cylinderListInit
		detailsWindow.cylinderIndex0 = PrefEquipment.default_cylinder == "" ? -1 : detailsWindow.cylinderModel0.indexOf(PrefEquipment.default_cylinder)
		detailsWindow.usedCyl = ["",]
		detailsWindow.weight = ""
		detailsWindow.usedGas = []
		detailsWindow.startpressure = []
		detailsWindow.endpressure = []
		showPage(detailsWindow)
	}

	contextDrawer: Kirigami.ContextDrawer {
		id: contextDrawer
		closePolicy: QtQuickTemplates.Popup.CloseOnPressOutside
		enabled: visibleActions().length > 0
		handleClosedIcon.name: ""
		handleClosedIcon.source: "qrc:/icons/overflow-menu.svg"
		handleOpenIcon.name: "window-close-symbolic"
		handleOpenIcon.source: ""
		actions: pageStack.currentItem?.contextualActions ?? []
		Kirigami.Theme.textColor: subsurfaceTheme.textColor
		Kirigami.Theme.backgroundColor: subsurfaceTheme.drawerColor
		Kirigami.Theme.highlightColor: subsurfaceTheme.darkerPrimaryColor
		Kirigami.Theme.highlightedTextColor: subsurfaceTheme.primaryTextColor
		background: Rectangle { color: subsurfaceTheme.drawerColor }
		// Override theme on contentItem too - OverlayDrawer propagates colorSet
		// to contentItem, creating a separate PlatformThemeData that gets iOS
		// system palette colors instead of our app theme colors
		Component.onCompleted: {
			contentItem.Kirigami.Theme.textColor = Qt.binding(function() { return subsurfaceTheme.textColor; });
			contentItem.Kirigami.Theme.backgroundColor = Qt.binding(function() { return subsurfaceTheme.drawerColor; });
			contentItem.Kirigami.Theme.highlightColor = Qt.binding(function() { return subsurfaceTheme.darkerPrimaryColor; });
			contentItem.Kirigami.Theme.highlightedTextColor = Qt.binding(function() { return subsurfaceTheme.primaryTextColor; });
		}
	}

	globalDrawer: Kirigami.GlobalDrawer {
		id: globalDrawer
		// Neo's primary navigation lives in the bottom bar and More page. Keep
		// this drawer for compatibility pages, but do not present it as the app's
		// normal navigation surface.
		handleVisible: false
		handleClosedIcon.name: ""
		handleClosedIcon.source: "qrc:/icons/application-menu.svg"
		handleOpenIcon.name: "window-close-symbolic"
		handleOpenIcon.source: ""
		height: rootItem.height
		rightPadding: 0
		Kirigami.Theme.textColor: subsurfaceTheme.textColor
		Kirigami.Theme.backgroundColor: subsurfaceTheme.drawerColor
		Kirigami.Theme.highlightColor: subsurfaceTheme.darkerPrimaryColor
		Kirigami.Theme.highlightedTextColor: subsurfaceTheme.primaryTextColor
		background: Rectangle { color: subsurfaceTheme.drawerColor }
		// Override theme on contentItem too - OverlayDrawer propagates colorSet
		// to contentItem, creating a separate PlatformThemeData that gets iOS
		// system palette colors instead of our app theme colors
		Component.onCompleted: {
			contentItem.Kirigami.Theme.textColor = Qt.binding(function() { return subsurfaceTheme.textColor; });
			contentItem.Kirigami.Theme.backgroundColor = Qt.binding(function() { return subsurfaceTheme.drawerColor; });
			contentItem.Kirigami.Theme.highlightColor = Qt.binding(function() { return subsurfaceTheme.darkerPrimaryColor; });
			contentItem.Kirigami.Theme.highlightedTextColor = Qt.binding(function() { return subsurfaceTheme.primaryTextColor; });
		}
		enabled: (Backend.cloud_verification_status === Enums.CS_NOCLOUD ||
				  Backend.cloud_verification_status === Enums.CS_VERIFIED)
		topContent: Image {
			source: "qrc:/qml/icons/dive.jpg"
			// it's a 4x3 image, but clip if it takes too much space (making sure the text fits)
			property int myHeight: Math.min(Math.max(rootItem.height * 0.3, textblock.height + Kirigami.Units.largeSpacing), parent.width * 0.75)
			Layout.fillWidth: true
			Layout.maximumHeight: myHeight
			sourceSize.width: parent.width
			fillMode: Image.PreserveAspectCrop
			Rectangle {
				anchors {
					left: parent.left
					right: parent.right
					top: parent.top
				}
				height: Math.min(textblock.height * 2, parent.myHeight)
				gradient: Gradient {
					GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.8) }
					GradientStop { position: 1.0; color: "transparent" }
				}
			}
			ColumnLayout {
				id: textblock
				anchors {
					left: parent.left
					right: parent.right
					top: parent.top
				}
				RowLayout {
					width: Math.min(implicitWidth, parent.width)
					Layout.margins: Kirigami.Units.smallSpacing
					Image {
						source: "qrc:/qml/subsurface-neo-icon.svg"
						fillMode: Image.PreserveAspectCrop
						sourceSize.width: Kirigami.Units.iconSizes.large
						width: Kirigami.Units.iconSizes.large
						Layout.margins: Kirigami.Units.smallSpacing
					}
					Kirigami.Heading {
						Layout.fillWidth: true
						visible: text.length > 0
						level: 1
						color: "white"
						text: "Subsurface"
						wrapMode: Text.NoWrap
						elide: Text.ElideRight
						font.weight: Font.Normal
						Layout.margins: Kirigami.Units.smallSpacing
					}
				}
				RowLayout {
					Layout.margins: Kirigami.Units.smallSpacing
					Kirigami.Heading {
						Layout.fillWidth: true
						visible: text.length > 0
						level: 3
						color: "white"
						text: PrefCloudStorage.cloud_storage_email
						wrapMode: Text.WrapAnywhere
						font.weight: Font.Normal
					}
				}
				RowLayout {
					Layout.leftMargin: Kirigami.Units.smallSpacing
					Layout.topMargin: 0
					Kirigami.Heading {
						Layout.fillWidth: true
						Layout.topMargin: 0
						visible: text.length > 0
						level: 5
						color: "white"
						text: manager.passwordState
						wrapMode: Text.NoWrap
						elide: Text.ElideRight
						font.weight: Font.Normal
					}
				}

				RowLayout {
					Layout.leftMargin: Kirigami.Units.smallSpacing
					Layout.topMargin: 0
					Kirigami.Heading {
						Layout.fillWidth: true
						Layout.topMargin: 0
						visible: text.length > 0
						level: 5
						color: "white"
						text: manager.syncState
						wrapMode: Text.NoWrap
						elide: Text.ElideRight
						font.weight: Font.Normal
					}
				}
			}
		}

		resetMenuOnTriggered: false

		actions: [
			Kirigami.Action {
				icon {
					name: ":/icons/ic_home.svg"
					color: subsurfaceTheme.textColor
				}
				text: qsTr("Dive list")
				onTriggered: {
					manager.appendTextToLog("requested dive list with credential status " + Backend.cloud_verification_status)
					globalDrawer.close()
					showPageFromDrawer(modernDiveList)
				}
			},
			Kirigami.Action {
				icon {
					name: ":/icons/ic_sync.svg"
					color: subsurfaceTheme.textColor
				}
				text: qsTr("Dive management")
				Kirigami.Action {
					icon {
						name: ":/icons/ic_add.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Add dive manually")
					onTriggered: {
						globalDrawer.close()
						var diveId = manager.addDive()
						var row = manager.swipeRowForDive(diveId)
						if (row >= 0)
							rootItem.openNeoDiveDetails(row, true)
						else
							startAddDive()
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/downloadDC-black.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Download from DC")
					enabled: true
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(neoDiveComputerCenter)
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/cloud_sync.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Manual sync with cloud")
					visible: Backend.cloud_verification_status !== Enums.CS_NOCLOUD
					onTriggered: {
						globalDrawer.close()
						detailsWindow.endEditMode()
						manager.saveChangesCloud(true);
						showPassiveNotification(qsTr("Completed manual sync with cloud\n") + manager.syncState)
						globalDrawer.close()
					}
				}
				Kirigami.Action {
					icon {
						name: PrefCloudStorage.cloud_auto_sync ?  ":/icons/ic_cloud_done.svg" : ":/icons/ic_cloud_off.svg"
						color: subsurfaceTheme.textColor
					}
					text: (PrefCloudStorage.cloud_auto_sync ? "\u2611 " : "\u2610 ") + qsTr("Auto cloud sync")
					visible: Backend.cloud_verification_status !== Enums.CS_NOCLOUD
					onTriggered: {
						PrefCloudStorage.cloud_auto_sync = !PrefCloudStorage.cloud_auto_sync
						manager.setGitLocalOnly(!PrefCloudStorage.cloud_auto_sync)
						if (!PrefCloudStorage.cloud_auto_sync) {
							showPassiveNotification(qsTr("Turning off automatic sync to cloud causes all data to only be \
stored locally. This can be very useful in situations with limited or no network access. Please choose 'Manual sync with cloud' \
if you have network connectivity and want to sync your data to cloud storage."), 10000)
						}
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/sigma.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Dive summary")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(diveSummaryWindow)
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/ic_cloud_upload.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Export")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(neoDataPortability)
					}
				}
			},
			Kirigami.Action {
				icon {
					name: ":/icons/map-globe.svg"
					color: subsurfaceTheme.textColor
				}
				text: qsTr("Location")
				visible: true
				Kirigami.Action {
					icon {
						name: ":/icons/map-globe.svg"
						color: subsurfaceTheme.textColor
					}
					text: mapPage.title
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(neoSitesHub)
					}
				}
			},
			Kirigami.Action {
				icon {
					name: ":/icons/office-chart-bar-stacked.svg"
					color: subsurfaceTheme.textColor
				}

				text: qsTr("Statistics")
				onTriggered: {
					globalDrawer.close()
					showPageFromDrawer(neoStatisticsHub)
				}
			},
			Kirigami.Action {
				icon {
					name: ":/icons/dashboard-show.svg"
					color: subsurfaceTheme.textColor
				}
				text: qsTr("Technical Diving")
				Kirigami.Action {
					icon.name: ":/icons/document-edit-sign.svg" // Using an existing icon for now
					icon.color: subsurfaceTheme.textColor
					text: qsTr("Dive Planner")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(neoPlannerLab)
					}
				}
				Kirigami.Action {
					icon.name: ":/icons/measure.svg" // Using an existing icon for now
					icon.color: subsurfaceTheme.textColor
					text: qsTr("Gas Calculator")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(divePlannerCalculatorWindow)
					}
				}
			},
			Kirigami.Action {
				icon {
					name: ":/icons/ic_settings.svg"
					color: subsurfaceTheme.textColor
				}
				text: qsTr("Settings")
				onTriggered: {
					globalDrawer.close()
					showPageFromDrawer(neoSettingsHub)
				}
			},
			Kirigami.Action {
				icon {
					name: ":/icons/ic_help_outline.svg"
					color: subsurfaceTheme.textColor
				}
				text: qsTr("Help")
				Kirigami.Action {
					icon {
						name: ":/icons/ic_info_outline.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("About")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(neoAboutPage)
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/ic_help_outline.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Show user manual")
					onTriggered: {
						Qt.openUrlExternally("https://www.subsurface-divelog.org/subsurface-mobile-user-manual/")
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/recycle.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Contribute to Subsurface")
					onTriggered: {
						Qt.openUrlExternally("https://www.subsurface-divelog.org/contribute/")
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/contact_support.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Ask for support")
					onTriggered: {
						if (!manager.createSupportEmail()) {
							manager.copyAppLogToClipboard()
							showPassiveNotification(qsTr("failed to open email client, please manually create support email to support@subsurface-divelog.org - the logs have been copied to the clipboard and can be pasted into that email."), 6000)
						} else {
							globalDrawer.close()
						}
					}
				}
				Kirigami.Action{
					icon {
						name: ":/icons/account_circle.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Reset forgotten Subsurface Cloud password")
					onTriggered: {
						Qt.openUrlExternally("https://cloud.subsurface-divelog.org/passwordreset")
						globalDrawer.close()
					}
				}
			},
			Kirigami.Action {
				icon {
					name: ":/icons/ic_adb.svg"
					color: subsurfaceTheme.textColor
				}
				text: qsTr("Developer")
				visible: PrefDisplay.show_developer
				Kirigami.Action {
					icon {
						name: ":/icons/ic_info_outline.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("App log")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(logWindow)
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/ic_sync.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Test busy indicator (toggle)")
					onTriggered: {
						if (busy.running) {
							hideBusy()
						} else {
							showBusy()
						}
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/ic_help_outline.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Test notification text")
					onTriggered: {
						showPassiveNotification(qsTr("Test notification text"), 5000)
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/ic_settings.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Theme information")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(themetest)
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/ic_adb.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Enable verbose logging (currently: %1)").arg(manager.verboseEnabled)
					onTriggered: {
						showPassiveNotification(qsTr("Not persistent"), 3000)
						globalDrawer.close()
						manager.verboseEnabled = true
					}
				}
				Kirigami.Action {
					icon {
						name: ":/icons/ic_cloud_download.svg"
						color: subsurfaceTheme.textColor
					}
					text: qsTr("Access local cloud cache dirs")
					onTriggered: {
						globalDrawer.close()
						showPageFromDrawer(recoverCache)
					}
				}
			}
		] // end actions
	}

	property double regularFontsize: subsurfaceTheme.regularPointSize

	FontMetrics {
		id: fontMetrics
		font.pointSize: regularFontsize
	}

	onRegularFontsizeChanged: {
		manager.appendTextToLog("regular font size changed to " + regularFontsize)
		rootItem.font.pointSize = regularFontsize
	}

	function setupUnits() {
		// since Kirigami was initially instantiated, the font size may have
		// changed, so recalculate the gridUnit
		var kirigamiGridUnit = fontMetrics.height

		// some screens are too narrow for Subsurface-mobile to render well;
		// pages like Settings and DiveDetailsEdit need at least ~24 gridUnits
		// to avoid clipping content in two-column mode
		var numColumns = Math.max(Math.floor(rootItem.neoContentWidth / (24 * kirigamiGridUnit)), 1)
		// Neo's Windows shell is a single-workspace application. Let each page use
		// the full content area instead of pinning the dashboard beside every
		// destination as an unrelated narrow column.
		if (Qt.platform.os === "windows")
			numColumns = 1
		if (Screen.primaryOrientation === Qt.PortraitOrientation && PrefDisplay.singleColumnPortrait) {
			manager.appendTextToLog("show only one column in portrait mode");
			numColumns = 1;
		}
		rootItem.colWidth = numColumns > 1 ? Math.floor(rootItem.neoContentWidth / numColumns) : rootItem.neoContentWidth;

		// If we can't fit 21 gridUnits into a line, let the user know and suggest using a smaller font
		var widthInGridUnits = Math.floor(rootItem.colWidth / kirigamiGridUnit)
		if (widthInGridUnits < 21) {
			showPassiveNotification(qsTr("Font size likely too big for the display, switching to smaller font suggested"), 3000)
		}
		manager.appendTextToLog(numColumns + " columns with column width of " + rootItem.colWidth)
		manager.appendTextToLog("width in Grid Units " + widthInGridUnits + " original gridUnit " + Kirigami.Units.gridUnit + " now " + kirigamiGridUnit)


		pageStack.defaultColumnWidth = rootItem.colWidth
		manager.appendTextToLog("Done setting up sizes width " + rootItem.width + " gridUnit " + kirigamiGridUnit)
	}

	QtObject {
		id: screenSizeObject

		property int initialWidth: rootItem.width
		property int initialHeight: rootItem.height
		property bool firstChange: true
		property int lastOrientation: undefined

		Component.onCompleted: {
			// break the binding
			initialWidth = initialWidth * 1
			manager.appendTextToLog("[screensetup] screenSizeObject constructor completed, initial width " + initialWidth)
			setupUnits()
		}
	}

	onWidthChanged: {
		manager.appendTextToLog("[screensetup] width changed now " + width + " x " + height + " vs screen " + Screen.width + " x " + Screen.height)

		if (screenSizeObject.lastOrientation === undefined) {
			manager.appendTextToLog("[screensetup] found initial orientation " + Screen.orientation)
			screenSizeObject.lastOrientation = Screen.orientation
		}
		manager.appendTextToLog("[screensetup] window width changed to " + width + " orientation " + Screen.orientation)
		// on Android devices we often get incorrect size updates during startup from Kirigami and we need to ignore those,
		// or more specifically, reset the Kirigami sizes when we notice them
		if (Screen.orientation === screenSizeObject.lastOrientation) {
			// not rotation
			if (width > neoAvailableWidth || height > neoAvailableHeight) {
				manager.appendTextToLog("[screensetup] received size update that exceeds screen size")
				if (screenSizeObject.initialWidth !== undefined) {
					manager.appendTextToLog("[screensetup] resetting to initial size " + screenSizeObject.initialWidth + " x " + screenSizeObject.initialHeight)
					rootItem.width = screenSizeObject.initialWidth
					rootItem.height = screenSizeObject.initialHeight
				} else {
					// we don't have a size that we believe, yet - using Screen size is almost certainly wrong
					manager.appendTextToLog("[screensetup] restricting to screen size " + Screen.width + " x " + Screen.height)
					rootItem.width = Screen.width
					rootItem.height = neoAvailableHeight
				}
			} else {
				// this could be a realistic size
				if (screenSizeObject.initialWidth !== undefined) {
					if (screenSizeObject.initialHeight < height) {
						manager.appendTextToLog("[screensetup] remembering better height")
						screenSizeObject.initialHeight = height
					}
					if (screenSizeObject.initialWidth < width) {
						manager.appendTextToLog("[screensetup] remembering better height")
						screenSizeObject.initialWidth = width
					}
					setupUnits()
				}
			}
		} else {
			manager.appendTextToLog("[screensetup] remembering new orientation")
			screenSizeObject.lastOrientation = Screen.orientation
			setupUnits()
		}
	}

	onNeoDesktopShellActiveChanged: {
		if (initialized)
			Qt.callLater(setupUnits)
	}

	property int hackToOpenMap: 0 /* Otherpage */
	/* I really want an enum, but those are painful in QML, so let's use numbers
	 * 0 (Otherpage)   - the last page selected was a non-map page
	 * 1 (MapSelected) - the map page was selected by the user
	 * 2 (MapForced)   - the map page was forced by this hack
	 */

	pageStack.onCurrentItemChanged: {
		// This is called whenever the user navigates using the breadcrumbs in the header

		if (pageStack.currentItem === null) {
			manager.appendTextToLog("there's no current page")
		} else {
			// horrible, insane hack to make picking the mapPage work
			// for some reason I cannot figure out, whenever the mapPage is selected
			// we immediately switch back to the page before it - so force-prevent
			// that undersired behavior
			if (pageStack.currentItem.objectName === mapPage.objectName) {
				// remember that we actively picked the mapPage
				if (hackToOpenMap !== 2 /* MapForced */ ) {
					manager.appendTextToLog("pageStack switched to map")
					hackToOpenMap = 1 /* MapSelected */
				} else {
					manager.appendTextToLog("pageStack forced back to map")
				}
			} else if (pageStack.currentItem.objectName !== mapPage.objectName &&
					   pageStack.lastItem?.objectName === mapPage.objectName &&
					   hackToOpenMap === 1 /* MapSelected */) {
				// if we just picked the mapPage and are suddenly back on a different page
				// force things back to the mapPage
				manager.appendTextToLog("pageStack wrong page, switching back to map")
				pageStack.currentIndex = pageStack.contentItem.contentChildren.length - 1
				hackToOpenMap = 2 /* MapForced */
			} else {
				// if we picked a different page reset the mapPage hack
				manager.appendTextToLog("pageStack switched to " + pageStack.currentItem.objectName)
				hackToOpenMap = 0 /* Otherpage */
			}

			// disable the left swipe to go back when on the map page
			pageStack.interactive = pageStack.currentItem.objectName !== mapPage.objectName

			// is there a better way to reload the map markers instead of doing that
			// every time the map page is shown - e.g. link to the dive list model somehow?
			if (pageStack.currentItem.objectName === mapPage.objectName)
				mapPage.reloadMap()

			// In case we land on any page, not being the DiveDetails (which can be
			// in multiple states, such as add, edit or view), just end the edit/add mode
			if (pageStack.currentItem.objectName !== "DiveDetails" &&
					(detailsWindow.state === 'edit' || detailsWindow.state === 'add')) {
				detailsWindow.endEditMode()
			}
		}
	}

	QMLManager {
		id: manager
	}

	property bool initialized: manager.initialized

	onInitializedChanged: {
		if (initialized) {
			hideBusy()
			manager.appendTextToLog("initialization completed - showing Subsurface Neo home")
			diveList.diveListModel = diveModel
			modernDiveList.diveListModel = diveModel
			modernPlansList.diveListModel = swipeModel
			showNeoHome()

			if (Qt.platform.os === "android") {
				manager.appendTextToLog("if we got started by a plugged in device, switch to download page -- pluggedInDeviceName = " + pluggedInDeviceName)
				if (pluggedInDeviceName !== "")
					// if we were started with a dive computer plugged in,
					// immediately switch to download page
					showDownloadForPluggedInDevice()
			}
		}
	}

	Label {
		id: textBlock
		visible: !initialized
		color: subsurfaceTheme.textColor
		text: qsTr("Subsurface-mobile starting up")
		font.pointSize: subsurfaceTheme.headingPointSize
		topPadding: 2 * Kirigami.Units.gridUnit
		leftPadding: Kirigami.Units.gridUnit
	}

	NeoPages.ModernOnboarding {
		id: neoOnboarding
		anchors.fill: parent
		visible: initialized && !neoSubsurfaceCloudSetupRequested &&
			Backend.cloud_verification_status !== Enums.CS_NOCLOUD &&
			Backend.cloud_verification_status !== Enums.CS_VERIFIED
		onVisibleChanged: {
			if (visible)
				pageStack.clear()
			else if (initialized && !neoSubsurfaceCloudSetup.visible)
				showNeoHome()
		}
		onUseLocalLog: {
			manager.setGitLocalOnly(true)
			PrefCloudStorage.cloud_auto_sync = false
			manager.oldStatus = Backend.cloud_verification_status
			Backend.cloud_verification_status = Enums.CS_NOCLOUD
			manager.saveCloudCredentials("", "", "")
			manager.openNoCloudRepo()
		}
		onOpenSubsurfaceCloudSetup: {
			rootItem.neoSubsurfaceCloudSetupFromSettings = false
			rootItem.neoSubsurfaceCloudSetupRequested = true
		}
	}

	function openNeoDiveDetails(row, editOnReady, returnToPlans) {
		var component = Qt.createComponent("qrc:/qml/modern/pages/ModernDiveDetails.qml")
		if (component.status !== Component.Ready) {
			showPassiveNotification(qsTr("Unable to load Neo dive details: %1").arg(component.errorString()), 6000)
			return
		}
		var detailsPage = component.createObject(rootItem, { "initialRow": row, "editOnReady": editOnReady || false,
			"navigationSection": returnToPlans ? "plans" : "dives" })
		if (detailsPage === null) {
			showPassiveNotification(qsTr("Unable to create Neo dive details"), 6000)
			return
		}
		detailsPage.editRequested.connect(function(diveData) {
			var editorComponent = Qt.createComponent("qrc:/qml/modern/pages/ModernDiveEditor.qml")
			if (editorComponent.status !== Component.Ready) {
				showPassiveNotification(qsTr("Unable to load Neo dive editor: %1").arg(editorComponent.errorString()), 6000)
				return
			}
			var editorPage = editorComponent.createObject(rootItem, { "dive": diveData, "newDive": detailsPage.editOnReady })
			if (editorPage === null) {
				showPassiveNotification(qsTr("Unable to create Neo dive editor"), 6000)
				return
			}
			editorPage.saved.connect(function() { showPage(detailsPage) })
			showPage(editorPage)
		})
		detailsPage.deleteRequested.connect(function(diveId) {
			manager.deleteDive(diveId)
			showPageFromDrawer(returnToPlans ? modernPlansList : modernDiveList)
		})
		showPage(detailsPage)
	}

	NeoPages.ModernSubsurfaceCloudSetup {
		id: neoSubsurfaceCloudSetup
		anchors.fill: parent
		visible: initialized && neoSubsurfaceCloudSetupRequested &&
			 Backend.cloud_verification_status !== Enums.CS_NOCLOUD &&
			 Backend.cloud_verification_status !== Enums.CS_VERIFIED
		onUseLocalLog: {
			manager.setGitLocalOnly(true)
			PrefCloudStorage.cloud_auto_sync = false
			manager.oldStatus = Backend.cloud_verification_status
			Backend.cloud_verification_status = Enums.CS_NOCLOUD
			manager.saveCloudCredentials("", "", "")
			manager.openNoCloudRepo()
		}
		onReturnRequested: {
			rootItem.neoSubsurfaceCloudSetupRequested = false
			manager.startPageText = ""
			if (rootItem.neoSubsurfaceCloudSetupFromSettings) {
				Backend.cloud_verification_status = manager.oldStatus
				rootItem.neoSubsurfaceCloudSetupFromSettings = false
				showPageFromDrawer(neoSettingsHub)
			} else {
				Backend.cloud_verification_status = Enums.CS_UNKNOWN
			}
		}
		onVisibleChanged: {
			manager.appendTextToLog("Neo Subsurface Cloud setup visibility changed to " + visible)
			if (!initialized) {
				manager.appendTextToLog("not yet initialized, show busy spinner")
				showBusy()
			}
			if (visible) {
				pageStack.clear()
			} else if (initialized && Backend.cloud_verification_status === Enums.CS_VERIFIED) {
				showNeoHome()
			}
		}
		Component.onCompleted: {
			if (Screen.manufacturer + " " + Screen.model + " " + Screen.name !== "  ")
				manager.appendTextToLog("Running on " + Screen.manufacturer + " " + Screen.model + " " + Screen.name)
			manager.appendTextToLog("StartPage completed -- initialized is " + initialized)
		}
	}

	NeoPages.ModernDiveList {
		id: modernDiveList
		visible: false
		onOpenDive: function(row) {
			rootItem.openNeoDiveDetails(row)
		}
		onDownloadRequested: showPageFromDrawer(neoDiveComputerCenter)
		onCloudRequested: showPageFromDrawer(cloudSyncPage)
		onAddDiveRequested: {
			var diveId = manager.addDive()
			var row = manager.swipeRowForDive(diveId)
			if (row >= 0)
				rootItem.openNeoDiveDetails(row, true)
			else
				startAddDive()
		}
	}

	NeoPages.ModernDiveList {
		id: modernPlansList
		visible: false
		plansOnly: true
		onOpenDive: function(row) { rootItem.openNeoDiveDetails(row, false, true) }
		onCloudRequested: showPageFromDrawer(cloudSyncPage)
	}

	NeoPages.ModernMorePage {
		id: neoMorePage
		visible: false
		onOpenPlanner: showPageFromDrawer(neoPlannerLab)
		onOpenCloudSync: showPageFromDrawer(cloudSyncPage)
		onOpenImport: showPageFromDrawer(neoDiveComputerCenter)
		onOpenEquipment: showPageFromDrawer(neoEquipmentLibrary)
		onOpenPortability: showPageFromDrawer(neoDataPortability)
		onOpenSettings: showPageFromDrawer(neoSettingsHub)
		onOpenAbout: showPageFromDrawer(neoAboutPage)
	}

	NeoPages.ModernSettingsHub {
		id: neoSettingsHub
		visible: false
		onOpenCloudSync: showPageFromDrawer(cloudSyncPage)
		onOpenAccountSecurity: showPageFromDrawer(neoAccountSecurityPage)
		onOpenSubsurfaceCloud: {
			manager.oldStatus = Backend.cloud_verification_status
			Backend.cloud_verification_status = Enums.CS_UNKNOWN
			manager.startPageText = qsTr("Enter your Subsurface Cloud credentials")
			rootItem.neoSubsurfaceCloudSetupFromSettings = true
			rootItem.neoSubsurfaceCloudSetupRequested = true
		}
		onOpenImport: showPageFromDrawer(neoDiveComputerCenter)
		onOpenAdvancedSettings: {
			settingsWindow.defaultCylinderModel = manager.defaultCylinderListInit
			PrefEquipment.default_cylinder === "" ? defaultCylinderIndex = "-1" : defaultCylinderIndex = settingsWindow.defaultCylinderModel.indexOf(PrefEquipment.default_cylinder)
			showPageFromDrawer(settingsWindow)
		}
		onOpenAbout: showPageFromDrawer(neoAboutPage)
	}

	NeoPages.ModernAboutPage {
		id: neoAboutPage
		visible: false
	}

	NeoPages.ModernAccountSecurityPage {
		id: neoAccountSecurityPage
		visible: false
		onOpenCloudSync: showPageFromDrawer(cloudSyncPage)
		onOpenSubsurfaceCloud: {
			manager.oldStatus = Backend.cloud_verification_status
			Backend.cloud_verification_status = Enums.CS_UNKNOWN
			manager.startPageText = qsTr("Enter your Subsurface Cloud credentials")
			rootItem.neoSubsurfaceCloudSetupFromSettings = true
			rootItem.neoSubsurfaceCloudSetupRequested = true
		}
	}

	NeoPages.ModernDiveComputerCenter {
		id: neoDiveComputerCenter
		visible: false
		onOpenNativeImport: function(vendor, product, connection) {
			var component = Qt.createComponent("qrc:/qml/modern/pages/ModernImportReview.qml")
			if (component.status !== Component.Ready) {
				showPassiveNotification(qsTr("Unable to load import review: %1").arg(component.errorString()), 6000)
				return
			}
			var review = component.createObject(rootItem, { "vendor": vendor, "product": product, "connection": connection })
			if (review === null) {
				showPassiveNotification(qsTr("Unable to create import review"), 6000)
				return
			}
			review.finished.connect(function() { showPageFromDrawer(modernDiveList) })
			showPage(review)
		}
	}

	NeoPages.ModernPlannerLab {
		id: neoPlannerLab
		visible: false
	}

	NeoPages.ModernOperationsHub {
		id: neoOperationsHub
		visible: false
		onOpenSites: showPage(neoSitesHub)
		onOpenStatistics: showPage(neoStatisticsHub)
		onOpenEquipment: showPage(neoEquipmentLibrary)
		onOpenExport: showPage(neoDataPortability)
		onOpenRecovery: showPage(recoverCache)
	}

	NeoPages.ModernEquipmentLibrary {
		id: neoEquipmentLibrary
		visible: false
	}

	NeoPages.ModernDataPortability {
		id: neoDataPortability
		visible: false
		onOpenCloudBackup: showPage(cloudSyncPage)
	}

	NeoPages.ModernSitesHub {
		id: neoSitesHub
		visible: false
		onOpenMap: function(siteName) {
			showPage(mapPage)
			if (siteName.length > 0)
				mapPage.centerOnDiveSite(manager.siteObject(siteName))
		}
		onOpenDive: function(diveId) {
			var row = manager.swipeRowForDive(diveId)
			if (row >= 0)
				rootItem.openNeoDiveDetails(row)
		}
	}

	NeoPages.ModernStatisticsHub {
		id: neoStatisticsHub
		visible: false
	}

	NeoPages.CloudSyncPage {
		id: cloudSyncPage
		visible: false
	}

	DiveList {
		id: diveList
		visible: false
	}

	StatisticsPage {
		id: statistics
		visible: false
	}

	Settings {
		id: settingsWindow
	}

	CopySettings {
		id: settingsCopyWindow
		visible: false
	}

	Export {
		id: exportWindow
		visible: false
	}

	DiveDetails {
		id: detailsWindow
		visible: false
	}

	TripDetails {
		id: tripEditWindow
		visible: false
	}

	Log {
		id: logWindow
		visible: false
	}

	DownloadFromDiveComputer {
		id: downloadFromDc
		visible: false
	}

	MapPage {
		id: mapPage
		visible: false
		onOpenSites: showPageFromDrawer(neoSitesHub)
	}

	RecoverCache {
		id: recoverCache
		visible: false
	}

	DivePlannerSetup {
		id: divePlannerSetupWindow
		visible: false
	}

	DivePlannerGasCalculator {
		id: divePlannerCalculatorWindow
		visible: false
	}

	DivePlannerEdit {
		id: divePlannerEditWindow
		visible: false
	}

	DiveSummary {
		id: diveSummaryWindow
		visible: false
	}

	ThemeTest {
		id: themetest
		visible: false
	}

	function showDownloadPage(vendor, product, connection) {
		manager.appendTextToLog("show download page for " + vendor + " / " + product + " / " + connection)
		downloadFromDc.dcImportModel.clearTable()
		if (vendor !== undefined && product !== undefined && connection !== undefined) {
			downloadFromDc.setupUSB = true
			// set up the correct values on the download page
			// setting the currentIndex to -1, first, helps to ensure
			// that the comboBox does get updated in the UI
			if (vendor !== -1) {
				downloadFromDc.vendor = -1
				downloadFromDc.vendor = vendor
			}
			if (product !== -1) {
				downloadFromDc.product = -1
				downloadFromDc.product = product
			}
			if (connection !== -1) {
				downloadFromDc.connection = -1
				downloadFromDc.connection = connection
			}
		} else {
			downloadFromDc.setupUSB = false
		}

		showPage(downloadFromDc)
	}

	function showDownloadForPluggedInDevice() {
		// Ignore early Android intents until the application model is ready.
		if (!initialized)
			return
		manager.appendTextToLog("plugged in device name changed to " + pluggedInDeviceName)
		/* if we recognized the device, we'll pass in a triple of ComboBox indeces as "vendor;product;connection" */
		var vendorProductConnection = pluggedInDeviceName.split(';')
		if (vendorProductConnection.length === 3)
			showDownloadPage(vendorProductConnection[0], vendorProductConnection[1], vendorProductConnection[2])
		else
			showDownloadPage()
	}

	onPluggedInDeviceNameChanged: {
		if (detailsWindow.state === 'edit' || detailsWindow.state === 'add') {
			/* we're in the middle of editing / adding a dive */
			manager.appendTextToLog("Download page requested by Android Intent, but adding/editing dive; no action taken")
		} else {
			// we want to show the downloads page
			// note that if Subsurface-mobile was started because a USB device was plugged in, this is run too early;
			// we catch this in the function below and instead switch to the download page in the completion signal
			// handler for the startPage
			showDownloadForPluggedInDevice()
		}
	}
	// Android 13+ predictive back navigates the page stack (changing
	// currentIndex) BEFORE delivering the Key_Back / close event to QML.
	// This means onClosing cannot distinguish "user was at root and wants
	// to exit" from "user was deeper and just navigated back" by looking
	// at currentIndex alone. Track the timestamp of the last navigation
	// so onClosing can tell the two apart.
	property real lastPageNavigationTime: 0
	Connections {
		target: pageStack
		function onCurrentIndexChanged() {
			lastPageNavigationTime = Date.now()
		}
	}

	onClosing: function(close) {
		if (globalDrawer.visible) {
			globalDrawer.close()
			close.accepted = false
		} else if (contextDrawer.visible) {
			contextDrawer.close()
			close.accepted = false
		} else if (Date.now() - lastPageNavigationTime < 500) {
			// The page stack just changed — this close event is a
			// side-effect of Android's predictive back animation
			// navigating away from a deeper page, not a "quit from
			// root" gesture. Reject it.
			close.accepted = false
		} else {
			manager.quit()
		}
	}
}
