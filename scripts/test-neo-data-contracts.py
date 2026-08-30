#!/usr/bin/env python3
"""Fast checks for the populated Neo UI fixture and safe site editing."""

from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "dives" / "neo-ui-regression.ssrf"

root = ET.parse(FIXTURE).getroot()
dives = root.findall("./dives/dive")
sites = root.findall("./divesites/site")

if len(dives) != 12:
	raise SystemExit(f"Expected 12 Neo UI dives, found {len(dives)}")
if len(sites) != 4:
	raise SystemExit(f"Expected 4 Neo UI sites, found {len(sites)}")
if sum(bool(site.get("gps")) for site in sites) != 3:
	raise SystemExit("Neo UI fixture must contain three mapped sites and one no-GPS state")

site_ids = {site.get("uuid") for site in sites}
if any(dive.get("divesiteid") not in site_ids for dive in dives):
	raise SystemExit("Every Neo UI regression dive must reference a fixture site")

years = {dive.get("date", "")[:4] for dive in dives}
if years != {"2024", "2025", "2026"}:
	raise SystemExit(f"Neo statistics fixture lost its year distribution: {sorted(years)}")

manager = (ROOT / "mobile-widgets" / "qmlmanager.cpp").read_text(encoding="utf-8")
method_start = manager.index("bool QMLManager::updateSite(")
method_end = manager.index("double QMLManager::plannerSurfacePressureForAltitude", method_start)
method = manager[method_start:method_end]
validation = method.index("if (!parseGpsText(trimmedGps")
first_edit = min(
	method.index("Command::editDiveSiteDescription"),
	method.index("Command::editDiveSiteNotes"),
	method.index("Command::editDiveSiteLocation"),
)
if validation > first_edit:
	raise SystemExit("Dive-site edits must not be applied before GPS validation")

windows = (ROOT / "core" / "windows.cpp").read_text(encoding="utf-8")
for fragment in (
	'if (!settings_suffix.empty())',
	'applicationDirectory += L"-"',
	'!std::isalnum(value)',
):
	if fragment not in windows:
		raise SystemExit(f"Windows --user data isolation is missing: {fragment}")

for fragment in (
	"maximumDiveLogSize",
	"maximumManifestSize",
	"maximumMetadataSize",
	'QCryptographicHash::hash(contents->divelogXml, QCryptographicHash::Sha256)',
	'{ "divelogSha256", QString::fromLatin1(QCryptographicHash::hash(xmlData, QCryptographicHash::Sha256).toHex()) }',
):
	if fragment not in manager:
		raise SystemExit(f"Neo backup size/integrity contract is missing: {fragment}")

visibility_contracts = {
	"core/dive.h": "depth_t visibility_distance",
	"core/save-xml.cpp": "visibilitydistance='",
	"core/parse-xml.cpp": 'MATCH_STATE("visibilitydistance.dive", depth',
	"core/save-git.cpp": '"visibilitydistance "',
	"core/load-git.cpp": "parse_dive_visibilitydistance",
	"qt-models/mobilelistmodel.cpp": 'roles[VisibilityDistanceRole] = "visibilityDistance"',
	"mobile-widgets/qmlmanager.cpp": "d->visibility_distance.mm = requestedVisibilityMm",
}
for relative_path, fragment in visibility_contracts.items():
	contents = (ROOT / relative_path).read_text(encoding="utf-8")
	if fragment not in contents:
		raise SystemExit(f"Neo visibility-distance persistence contract is missing from {relative_path}: {fragment}")

print("Neo populated-log and transactional site-edit contracts validated")
