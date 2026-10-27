#!/usr/bin/env python3
"""Validate Brazilian Portuguese Android resources and locale registration."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "app/src/main/res/values"
PT_BR = ROOT / "app/src/main/res/values-b+pt+BR"
RESOURCE_FILES = (
    "strings.xml",
    "setup.xml",
    "emulation_menu.xml",
    "strings_mali_library.xml",
    "settings_design.xml",
    "resolution.xml",
)
PLACEHOLDER = re.compile(r"%(?:\d+\$)?[sdif]")


def named_resources(path: Path, tags: tuple[str, ...]) -> dict[tuple[str, str], ET.Element]:
    root = ET.parse(path).getroot()
    return {
        (item.tag, item.attrib["name"]): item
        for item in root
        if item.tag in tags and "name" in item.attrib
    }


def text_content(element: ET.Element) -> str:
    return "".join(element.itertext())


def main() -> None:
    for filename in RESOURCE_FILES:
        base_path = DEFAULT / filename
        localized_path = PT_BR / filename
        assert localized_path.is_file(), f"Missing pt-BR resource file: {localized_path}"
        base = named_resources(base_path, ("string",))
        localized = named_resources(localized_path, ("string",))
        expected = {
            key: value for key, value in base.items()
            if value.attrib.get("translatable", "true") != "false"
        }
        missing = set(expected) - set(localized)
        unexpected = set(localized) - set(expected)
        assert not missing, f"{filename}: missing translations: {sorted(missing)}"
        assert not unexpected, f"{filename}: unknown localized keys: {sorted(unexpected)}"
        for key, source in expected.items():
            target = localized[key]
            source_placeholders = sorted(PLACEHOLDER.findall(text_content(source)))
            target_placeholders = sorted(PLACEHOLDER.findall(text_content(target)))
            assert source_placeholders == target_placeholders, (
                f"{filename}:{key[1]} placeholder mismatch: "
                f"{source_placeholders} != {target_placeholders}"
            )

    base_arrays = named_resources(DEFAULT / "array.xml", ("string-array", "integer-array"))
    pt_arrays = named_resources(PT_BR / "array.xml", ("string-array", "integer-array"))
    assert set(base_arrays) == set(pt_arrays), "pt-BR array names differ from default resources"
    for key, source in base_arrays.items():
        source_items = [text_content(item).strip() for item in source.findall("item")]
        target_items = [text_content(item).strip() for item in pt_arrays[key].findall("item")]
        assert len(source_items) == len(target_items), f"{key[1]} item count differs"
        if key[0] == "integer-array":
            assert source_items == target_items, f"{key[1]} numeric values changed"

    resolution_entries = named_resources(DEFAULT / "resolution.xml", ("string-array",))[("string-array", "resolution_scale_entries")]
    localized_resolution = named_resources(PT_BR / "resolution.xml", ("string-array",))[("string-array", "resolution_scale_entries")]
    source_entries = [text_content(item).strip() for item in resolution_entries.findall("item")]
    pt_entries = [text_content(item).strip() for item in localized_resolution.findall("item")]
    assert len(source_entries) == len(pt_entries) and pt_entries[2] == "1x (nativa)"
    assert source_entries[:2] == pt_entries[:2] and source_entries[3:] == pt_entries[3:]

    locales_config = ET.parse(ROOT / "app/src/main/res/xml/locales_config.xml").getroot()
    locale_names = {item.attrib.get("{http://schemas.android.com/apk/res/android}name") for item in locales_config}
    assert "pt-BR" in locale_names, "pt-BR is not exposed in Android localeConfig"
    gradle = (ROOT / "app/build.gradle").read_text(encoding="utf-8")
    assert '"pt-BR"' in gradle, "pt-BR is not included in BuildConfig.AVAILABLE_APP_LANGUAGES"

    palette_names = {"StratoPaletteEmerald", "StratoPaletteViolet", "StratoPaletteAmoled"}
    for palette_file in ("values/theme_palettes.xml", "values-night/theme_palettes.xml"):
        palette_root = ET.parse(ROOT / "app/src/main/res" / palette_file).getroot()
        defined = {item.attrib.get("name") for item in palette_root.findall("style")}
        assert defined == palette_names, f"{palette_file}: palette style names differ"
    application = (ROOT / "app/src/main/java/org/stratoemu/strato/StratoApplication.kt").read_text(encoding="utf-8")
    for name in palette_names:
        assert f"R.style.{name}" in application, f"Palette style is not mapped: {name}"

    print(
        f"pt-BR resource validation passed: {sum(len(named_resources(PT_BR / f, ('string',))) for f in RESOURCE_FILES)} strings, "
        f"{len(pt_arrays)} arrays, locale registered"
    )


if __name__ == "__main__":
    main()
