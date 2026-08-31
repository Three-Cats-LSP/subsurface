// SPDX-License-Identifier: GPL-2.0
import QtQuick

QtObject {
	// Neo's day palette deliberately mirrors LSP+: a low-glare blue-gray page,
	// white surfaces, dark text and a deeper cyan accent that remains legible in sun.
	readonly property bool lightTheme: subsurfaceTheme.currentTheme !== "Dark"
	readonly property color background: lightTheme ? "#F0F4F8" : "#06111E"
	readonly property color surface: lightTheme ? "#FFFFFF" : "#0A1E2F"
	readonly property color surfaceRaised: lightTheme ? "#F7F9FC" : "#0D263C"
	readonly property color textPrimary: lightTheme ? "#1A202C" : "#F2F8FB"
	readonly property color textSecondary: lightTheme ? "#4A5568" : "#A9BAC8"
	readonly property color textMuted: lightTheme ? "#718096" : "#718CA1"
	readonly property color accent: lightTheme ? "#0891B2" : "#22D4EB"
	readonly property color accentStrong: lightTheme ? "#0E7490" : "#0EAAC7"
	readonly property color border: lightTheme ? "#D8E0E8" : "#1E3B50"
	readonly property color success: "#43D17A"
	readonly property color warning: "#FFB84D"

	readonly property int space4: 4
	readonly property int space8: 8
	readonly property int space12: 12
	readonly property int space16: 16
	readonly property int space24: 24
	readonly property int space32: 32

	readonly property int radiusSmall: 10
	readonly property int radius12: 12
	readonly property int radius16: 16
	readonly property int radiusMedium: 16
	readonly property int radiusLarge: 24
}
