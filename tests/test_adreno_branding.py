#!/usr/bin/env python3
"""Static checks for the Strato Adreno app identity and CI workflow."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "app/src/main/res/values"
ANDROID_NS = "{http://schemas.android.com/apk/res/android}"


class AdrenoBrandingTests(unittest.TestCase):
    def test_visible_app_names_use_adreno_brand(self):
        strings = ET.parse(RES / "strings.xml").getroot()
        setup = ET.parse(RES / "setup.xml").getroot()
        menu = ET.parse(RES / "emulation_menu.xml").getroot()
        values = {item.attrib["name"]: item.text or "" for item in strings.findall("string")}
        self.assertEqual(values["app_name"], "Strato Adreno")
        self.assertIn("Strato Adreno", setup.find("string[@name='setup_welcome']").text)
        self.assertEqual(menu.find("string[@name='emulation_menu']").text, "Strato Adreno")
        self.assertNotIn("Strato Mali", " ".join(values.values()))

    def test_package_id_and_previous_fundraising_prompt(self):
        gradle = (ROOT / "app/build.gradle").read_text()
        main_activity = (ROOT / "app/src/main/java/org/stratoemu/strato/MainActivity.kt").read_text()
        app = ET.parse(ROOT / "app/src/main/AndroidManifest.xml").getroot().find("application")
        self.assertIn('applicationId "org.stratoemu.stratoadreno"', gradle)
        self.assertEqual(app.attrib[ANDROID_NS + "label"], "@string/app_name")
        self.assertNotIn("ko-fi.com/lukevalen", main_activity)
        self.assertNotIn("showKofiSupportDialog", main_activity)

    def test_adreno_ci_builds_mainline_and_does_not_bundle_panvk(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text()
        self.assertIn("pull_request:", workflow)
        self.assertIn("- main", workflow)
        self.assertIn("assembleMainlineRelease", workflow)
        self.assertIn("Strato-Adreno-APK", workflow)
        self.assertNotIn("PanVK", workflow)
        app = (ROOT / "app/src/main/java/org/stratoemu/strato/StratoApplication.kt").read_text()
        self.assertNotIn("ensureBundledPanVk", app)


if __name__ == "__main__":
    unittest.main()
