#!/usr/bin/env python3
"""Guard theme-aware library surfaces and lightweight UI-only motion."""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "app/src/main/res"
ANDROID = "{http://schemas.android.com/apk/res/android}"
APP = "{http://schemas.android.com/apk/res-auto}"

main = ET.parse(RES / "layout/main_activity.xml").getroot()
assert main.attrib.get(ANDROID + "background") == "?android:attr/colorBackground"
main_text = (RES / "layout/main_activity.xml").read_text(encoding="utf-8")
assert 'android:text="@string/app_name"' in main_text
assert 'android:text=" Mali"' not in main_text
assert "@color/mali_" not in main_text

for filename, attrs in {
    "mali_dock_background.xml": ("?attr/colorSurfaceVariant", "?attr/colorOutlineVariant"),
    "mali_dock_active.xml": ("?attr/colorPrimaryContainer", "?attr/colorPrimary"),
    "mali_circle_button.xml": ("?attr/colorSurfaceVariant", "?attr/colorOutline"),
    "mali_chip.xml": ("?attr/colorSurfaceVariant", "?attr/colorOutline"),
}.items():
    content = (RES / "drawable" / filename).read_text(encoding="utf-8")
    for attr in attrs:
        assert attr in content, f"{filename} does not use {attr}"

for filename in ("app_item_grid.xml", "app_item_grid_compact.xml"):
    root = ET.parse(RES / "layout" / filename).getroot()
    cards = [node for node in root.iter() if node.tag.endswith("MaterialCardView")]
    assert cards, f"{filename}: MaterialCardView missing"
    assert cards[0].attrib.get(APP + "cardBackgroundColor") == "?attr/colorSurfaceVariant"
    assert cards[0].attrib.get(APP + "strokeColor") == "?attr/colorPrimary"

search_bar = (RES / "layout/view_search_bar.xml").read_text(encoding="utf-8")
assert "@color/mali_" not in search_bar
assert "?attr/colorOnSurface" in search_bar and "?attr/colorOnSurfaceVariant" in search_bar

item_source = (ROOT / "app/src/main/java/org/stratoemu/strato/adapter/AppViewItem.kt").read_text(encoding="utf-8")
assert "MaterialColors.getColor(binding.root, MaterialR.attr.colorPrimary" in item_source
main_source = (ROOT / "app/src/main/java/org/stratoemu/strato/MainActivity.kt").read_text(encoding="utf-8")
assert "DefaultItemAnimator()" in main_source and "animateDockPress()" in main_source
assert "notifyItemChanged(position)" in main_source and "ColorUtils.calculateLuminance" in main_source
settings_source = (ROOT / "app/src/main/java/org/stratoemu/strato/settings/SettingsActivity.kt").read_text(encoding="utf-8")
assert "animateSectionDescription" in settings_source and "animateSettingsContent" in settings_source
global_settings = (ROOT / "app/src/main/java/org/stratoemu/strato/settings/GlobalSettingsFragment.kt").read_text(encoding="utf-8")
assert "DefaultItemAnimator()" in global_settings

print("library colors follow the active theme; settings/list motion is present and bounded")
