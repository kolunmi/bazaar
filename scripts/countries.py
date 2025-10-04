#!/usr/bin/env python3
import json
import sys
from pathlib import Path

try:
    from babel import Locale
except ImportError:
    print("Error: babel library not found.")
    print("Install it with: pip install babel")
    sys.exit(1)


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
        print(f"Warning: Could not get translation for {country_code} in {lang_code}: {e}")
        return None


def add_translations_to_json(json_path, linguas_path, output_path=None, minify=False):
    languages = read_linguas(linguas_path)
    print(f"Found {len(languages)} languages: {', '.join(languages)}")
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    for feature in data.get("features", []):
        country_name = feature.get("N")
        country_code = feature.get("I")
        if not country_code:
            continue
        print(f"Processing: {country_name} ({country_code})")
        translations = {}
        for lang in languages:
            translation = get_country_translation(country_code, lang)
            if translation:
                translations[lang] = translation
            else:
                translations[lang] = country_name
        feature["translations"] = translations
    if output_path is None:
        output_path = json_path.replace(".json", "_translated.json")
    with open(output_path, "w", encoding="utf-8") as f:
        if minify:
            json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
        else:
            json.dump(data, f, ensure_ascii=False, indent=2)
    print(f"\nTranslated JSON saved to: {output_path}")


def main():
    linguas_path = "../po/LINGUAS"
    json_path = "countries.json.in"
    output_path = "../src/countries.json"
    minify = True
    if "--no-minify" in sys.argv:
        minify = False
        sys.argv.remove("--no-minify")
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
    add_translations_to_json(json_path, linguas_path, output_path, minify)


if __name__ == "__main__":
    main()

