// SPDX-License-Identifier: GPL-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".." as Modern
import "../components" as Components

Kirigami.ScrollablePage {
	id: page
	title: qsTr("Planner & decompression lab")
	background: Rectangle { color: tokens.background }
	signal openPlanner()
	signal openGasTools()

	Modern.DesignTokens { id: tokens }
	ColumnLayout {
		width: page.availableWidth
		spacing: tokens.space16
		Text { text: qsTr("Plan with the proven Subsurface engine"); color: tokens.textPrimary; font.pixelSize: 26; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Text { text: qsTr("Neo keeps the mature BÃ¼hlmann/GF, VPM-B, OC, CCR, pSCR, gas, bailout, and planner-warning calculations intact. The planner below is the canonical calculation workspace."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Active planning assumptions"); color: tokens.textMuted; font.pixelSize: 10 }
			GridLayout { Layout.fillWidth: true; columns: page.width >= 700 ? 3 : 1
				Text { text: qsTr("Gradient factors: %1 / %2").arg(PrefTechnicalDetails.gflow).arg(PrefTechnicalDetails.gfhigh); color: tokens.textPrimary }
				Text { text: qsTr("Bottom SAC: %1").arg(Backend.bottomsac); color: tokens.textPrimary }
				Text { text: qsTr("Deco SAC: %1").arg(Backend.decosac); color: tokens.textPrimary }
			}
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Build a dive plan"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Set depths, times, cylinders, gases, setpoints, water type, ascent settings, and save only when the canonical planner validates the schedule."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Button { Layout.fillWidth: true; text: qsTr("Open planner"); onClicked: page.openPlanner() }
		}
		Components.ModernCard {
			Layout.fillWidth: true
			Text { text: qsTr("Gas tools"); color: tokens.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
			Text { text: qsTr("Use the same calculation primitives for MOD, best mix, END/EAD, CNS/OTU, and gas reference work."); color: tokens.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
			Button { Layout.fillWidth: true; text: qsTr("Open gas calculator"); onClicked: page.openGasTools() }
		}
		Text { text: qsTr("Planning aid only. Always review the generated schedule, gas requirements, warnings, algorithm, units, and environmental assumptions before diving."); color: tokens.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
	}
}
