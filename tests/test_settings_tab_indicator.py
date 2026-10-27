#!/usr/bin/env python3
"""Guard the rounded, palette-aware selected indicator on the settings tabs."""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ANDROID = "{http://schemas.android.com/apk/res/android}"
APP = "{http://schemas.android.com/apk/res-auto}"

layout = ET.parse(ROOT / "app/src/main/res/layout/settings_activity.xml").getroot()
tab_layout = next(
    element for element in layout.iter()
    if element.tag.endswith("TabLayout")
)
assert tab_layout.attrib.get(APP + "tabIndicator") == "@drawable/settings_tab_indicator_rounded"
assert tab_layout.attrib.get(APP + "tabSelectedTextColor") == "?attr/colorOnPrimary"

indicator = ET.parse(ROOT / "app/src/main/res/drawable/settings_tab_indicator_rounded.xml").getroot()
assert indicator.attrib.get(ANDROID + "shape") == "rectangle"
solid = indicator.find("solid")
corners = indicator.find("corners")
assert solid is not None and solid.attrib.get(ANDROID + "color") == "?attr/colorPrimary"
assert corners is not None and corners.attrib.get(ANDROID + "radius") == "28dp"

print("settings tab indicator is rounded and palette-aware")
