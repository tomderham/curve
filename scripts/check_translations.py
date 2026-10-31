#!/usr/bin/env python3
"""Checks each language in localization/ against docs/guide and the app's Info.plist strings.

For every page, a translation must keep the English page's structure and links, and every label
in localization/app-terms.txt that the English page uses (the app itself isn't translated yet).
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GUIDE = ROOT / "docs" / "guide"
LOCALIZATION = ROOT / "localization"
LINK = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
CODE = re.compile(r"`([^`]+)`")
# Only on each language's home page, in the note that the translation is machine made
CORRECTIONS_LINK = "https://github.com/tomderham/curve/issues"
PLIST_KEYS = ("NSMicrophoneUsageDescription", "NSAudioCaptureUsageDescription",
              "NSBluetoothAlwaysUsageDescription", "NSBluetoothPeripheralUsageDescription")


def labels():
    lines = (LOCALIZATION / "app-terms.txt").read_text(encoding="utf-8").splitlines()
    return [line for line in lines if line and not line.startswith("#")]


def used_labels(text, terms):
    """Removes each label once matched, so that 'Audio Settings' doesn't also count as 'Settings'."""
    found = []

    for term in terms:
        if term in text:
            found.append(term)
            text = text.replace(term, "\0")

    return found


def structure(text):
    lines = text.splitlines()
    return {
        "headings": [len(m.group(1)) for line in lines if (m := re.match(r"^(#+) ", line))],
        "list items": sum(bool(re.match(r"^ *([-*]|\d+\.) ", line)) for line in lines),
        "table rows": sum(line.startswith("|") for line in lines),
        "links": sorted(LINK.findall(text)),
        "code": sorted(CODE.findall(text)),
    }


def check_page(source, translated, terms):
    english = source.read_text(encoding="utf-8")
    text = translated.read_text(encoding="utf-8")
    problems = []

    expected = structure(english)
    actual = structure(text)

    if source.name == "README.md" and CORRECTIONS_LINK in actual["links"]:
        actual["links"].remove(CORRECTIONS_LINK)
    elif source.name == "README.md":
        problems.append("the machine translation note is missing")

    for name, value in expected.items():
        if actual[name] != value:
            problems.append(f"{name} differ: {actual[name]} vs English {value}")

    for term in used_labels(english, terms):
        if term not in text:
            problems.append(f"missing app label: {term}")

    return problems


def check_strings(folder):
    path = folder / "InfoPlist.strings"

    if not path.exists():
        return ["InfoPlist.strings is missing"]

    strings = dict(re.findall(r'^(\w+) = "(.*)";$', path.read_text(encoding="utf-8"), re.MULTILINE))

    return [f"InfoPlist.strings has no {key}" for key in PLIST_KEYS if not strings.get(key)]


def main():
    terms = labels()
    failed = False

    for folder in sorted(LOCALIZATION.iterdir()):
        if not (folder / "guide").is_dir():
            continue

        problems = check_strings(folder)

        for source in sorted(GUIDE.glob("*.md")):
            translated = folder / "guide" / source.name

            if not translated.exists():
                problems.append(f"{source.name}: missing")
                continue

            problems += [f"{source.name}: {p}" for p in check_page(source, translated, terms)]

        print(f"{folder.name}: {'OK' if not problems else f'{len(problems)} problem(s)'}")

        for problem in problems:
            print(f"  {problem}")

        failed = failed or bool(problems)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
