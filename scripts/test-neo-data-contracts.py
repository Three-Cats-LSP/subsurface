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

print("Neo populated-log and transactional site-edit contracts validated")
