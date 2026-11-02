#!/usr/bin/env python3
"""Builds site/<language>/index.html from site/index.html and localization/<language>/site.json.

Each site.json maps exact English text or attribute values from site/index.html to a translation.
The build fails if a translation no longer matches the page, or if English text on the page has
no translation, so run this after changing the English page. Commit the generated pages.
"""

import json
import re
import sys
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / "site"
LOCALIZATION = ROOT / "localization"
BASE_URL = "https://tomderham.github.io/curve/"
GUIDE_URL = "/blob/main/docs/guide/"
NOTE_KEY = "_note"

# Names used as they are in every language
KEEP = {
    "Curve", "GitHub", "AGPLv3", "JUCE", "AutoEQ", "English", "Deutsch", "Español", "Français",
    "हिन्दी", "日本語", "한국어", "Português", "简体中文",
    "Parametric EQ, Crossfeed, Speaker Emulation", "Gain, Clipper, Invert Phase",
    "Pan, Stereo Utility, Mid/Side, Delay",
}
NATIVE_NAMES = {"de": "Deutsch", "es": "Español", "fr": "Français", "hi": "हिन्दी", "ja": "日本語",
                "ko": "한국어", "pt": "Português", "zh-Hans": "简体中文"}


class TextFinder(HTMLParser):
    """Collects text nodes and the attributes people read or search engines show."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.found = set()
        self.skip = 0

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)

        if tag in ("style", "script"):
            self.skip += 1

        if tag == "img" and attrs.get("alt"):
            self.found.add(attrs["alt"])

        if tag == "meta" and (attrs.get("name") == "description"
                              or attrs.get("property") in ("og:title", "og:description")):
            self.found.add(attrs["content"])

        if attrs.get("aria-label"):
            self.found.add(attrs["aria-label"])

    def handle_endtag(self, tag):
        if tag in ("style", "script"):
            self.skip -= 1

    def handle_data(self, data):
        if data.strip() and not self.skip:
            self.found.add(" ".join(data.split()))


def page_text(html):
    finder = TextFinder()
    finder.feed(html)
    return finder.found


def tags(fragment):
    return sorted(re.findall(r"<(/?\w+)", fragment))


def localize(source, language, strings):
    problems = []
    text = source

    for key in sorted((k for k in strings if k != NOTE_KEY), key=len, reverse=True):
        value = strings[key]

        if tags(key) != tags(value):
            problems.append(f"markup differs: {key[:60]}")

        patterns = (f">{key}<", f'="{key}"')

        if not any(p in source for p in patterns):
            problems.append(f"no longer on the page: {key[:60]}")

        text = text.replace(patterns[0], f">{value}<").replace(patterns[1], f'="{value}"')

    # Punctuation is shared, and a translator can choose to keep an English word
    leftovers = sorted(item for item in (page_text(source) & page_text(text)) - KEEP - set(strings)
                       if re.search("[A-Za-z]", item))
    problems += [f"not translated: {item[:60]}" for item in leftovers]

    url = f"{BASE_URL}{language}/"
    text = text.replace('<html lang="en">', f'<html lang="{language}">')
    text = re.sub(rf'(<link rel="canonical" href=")[^"]*', rf"\g<1>{url}", text)
    text = re.sub(rf'(<meta property="og:url" content=")[^"]*', rf"\g<1>{url}", text)
    text = text.replace("<summary>English</summary>", f"<summary>{NATIVE_NAMES[language]}</summary>")
    text = text.replace(GUIDE_URL, f"/blob/main/localization/{language}/guide/")
    text = re.sub(r'(<(?:a|img|link) [^>]*?(?:href|src)=")(?!https?:|#|\.\./)',
                  r"\g<1>../", text).replace('href=".././"', 'href="../"')
    text = text.replace("<footer>\n", f"<footer>\n  <p>{strings[NOTE_KEY]}</p>\n")

    return text, problems


def main():
    source = (SITE / "index.html").read_text(encoding="utf-8")
    failed = False

    for language in NATIVE_NAMES:
        strings = json.loads((LOCALIZATION / language / "site.json").read_text(encoding="utf-8"))
        text, problems = localize(source, language, strings)

        output = SITE / language / "index.html"
        output.parent.mkdir(exist_ok=True)
        output.write_text(text, encoding="utf-8")

        print(f"{language}: {'OK' if not problems else f'{len(problems)} problem(s)'}")

        for problem in problems:
            print(f"  {problem}")

        failed = failed or bool(problems)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
