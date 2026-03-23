#!/usr/bin/env python3
import struct
import json
import sys
from pathlib import Path

try:
    from babel import Locale
except ImportError:
    print("Error: babel library not found.")
    print("Install it with: pip install babel")
    sys.exit(1)

try:
    import gi
    gi.require_version('GLib', '2.0')
    from gi.repository import GLib
except ImportError:
    print("Error: PyGObject not found.")
    sys.exit(1)

verbose = False

def log(msg):
    if verbose:
        print(msg)

def read_linguas(linguas_path):
    languages = []
    with open(linguas_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                languages.append(line)
    return languages


def normalize_locale_code(lang_code):
    locale_map = {"zh_CN": "zh_Hans_CN", "zh_TW": "zh_Hant_TW", "fa_IR": "fa_IR"}
    return locale_map.get(lang_code, lang_code)


def get_country_translation(country_code, lang_code):
    try:
        normalized = normalize_locale_code(lang_code)
        if "_" in normalized:
            parts = normalized.split("_")
            if len(parts) == 3:
                locale = Locale(parts[0], script=parts[1], territory=parts[2])
            else:
                lang, territory = parts[0], parts[1]
                locale = Locale(lang, territory=territory)
        else:
            locale = Locale(normalized)
        territory_name = locale.territories.get(country_code.upper())
        return territory_name if territory_name else None
    except Exception as e:
        log(f"Warning: Could not get translation for {country_code} in {lang_code}: {e}")
        return None


def build_variant(json_path, linguas_path):
    languages = read_linguas(linguas_path)
    log(f"Found {len(languages)} languages: {', '.join(languages)}")

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    builder = GLib.VariantBuilder(GLib.VariantType.new("a(ssa{ss}aaa(dd))"))

    for feature in data.get("features", []):
        country_name = feature.get("N", "")
        country_code = feature.get("I", "")
        if not country_code:
            continue

        log(f"Processing: {country_name} ({country_code})")

        translations = {}
        for lang in languages:
            translation = get_country_translation(country_code, lang)
            translations[lang] = translation if translation else country_name

        coords_raw = feature.get("C", [])

        poly_builder = GLib.VariantBuilder(GLib.VariantType.new("aaa(dd)"))
        for polygon in coords_raw:
            ring_builder = GLib.VariantBuilder(GLib.VariantType.new("aa(dd)"))
            for ring in polygon:
                point_builder = GLib.VariantBuilder(GLib.VariantType.new("a(dd)"))
                for point in ring:
                    point_builder.add_value(GLib.Variant("(dd)", (float(point[0]), float(point[1]))))
                ring_builder.add_value(point_builder.end())
            poly_builder.add_value(ring_builder.end())

        trans_variant = GLib.Variant("a{ss}", translations)
        coords_variant = poly_builder.end()

        entry = GLib.Variant.new_tuple(
            GLib.Variant("s", country_name),
            GLib.Variant("s", country_code),
            trans_variant,
            coords_variant,
        )
        builder.add_value(entry)

    return builder.end()

def main():
    global verbose

    linguas_path = "../po/LINGUAS"
    json_path = "countries.json.in"
    output_path = "../src/countries.gvariant"

    if "--verbose" in sys.argv:
        verbose = True
        sys.argv.remove("--verbose")
    if len(sys.argv) > 1:
        json_path = sys.argv[1]
    if len(sys.argv) > 2:
        linguas_path = sys.argv[2]
    if len(sys.argv) > 3:
        output_path = sys.argv[3]
    if not Path(linguas_path).exists():
        print(f"Error: LINGUAS file not found at {linguas_path}")
        sys.exit(1)
    if not Path(json_path).exists():
        print(f"Error: JSON file not found at {json_path}")
        sys.exit(1)

    variant = build_variant(json_path, linguas_path)
    data = variant.get_data_as_bytes()

    with open(output_path, "wb") as f:
        f.write(data.get_data())

    print(f"GVariant saved to: {output_path} ({GLib.format_size(data.get_size())})")

if __name__ == "__main__":
    main()

