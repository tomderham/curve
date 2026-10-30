#!/usr/bin/env python3
"""Builds Curve's Apple Help book and plain-text guide from docs/guide/*.md.

Uses only the Python standard library, plus Xcode's hiutil for the search indexes.
The converter supports just the Markdown the guide uses: headings, paragraphs, bold,
italics, inline code, links, images, bullet and numbered lists (nested by two spaces),
tables and fenced code blocks.
"""

import argparse
import html
import json
import plistlib
import re
import shutil
import subprocess
from pathlib import Path

HELP_BUNDLE_ID = "com.thomasderham.curve.help"
BOOK_TITLE = "Curve Help"

PAGE_TEMPLATE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
{apple_title}<title>{title}</title>
<link rel="stylesheet" href="style.css">
</head>
<body>
<aside id="toc"></aside>
<section>
{body}
</section>
<script src="toc.js"></script>
</body>
</html>
"""

STYLESHEET = """:root {
    color-scheme: light dark;
    --sidebar: #ececec;
    --selected: rgba(0, 0, 0, 0.11);
    --link: #0066cc;
    --line: rgba(127, 127, 127, 0.35);
}
@media (prefers-color-scheme: dark) {
    :root { --sidebar: #2b2b2d; --selected: rgba(255, 255, 255, 0.14); --link: #4da3ff; }
}
html, body { height: 100%; margin: 0; }
body { display: flex; font: 15px/1.5 -apple-system, sans-serif; }
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }

#toc { flex: 0 0 220px; box-sizing: border-box; height: 100%; padding: 8px; overflow-y: auto;
       background: var(--sidebar); border-right: 1px solid var(--line); font-size: 14px; line-height: 1.3; }
#toc ul { list-style: none; margin: 0 0 12px; padding: 0; }
#toc p { margin: 8px 10px 4px; font-size: 12px; font-weight: 600; opacity: 0.6; }
#toc a { display: block; padding: 5px 10px; border-radius: 7px; color: inherit; }
#toc a:hover { text-decoration: none; }
#toc .current { background: var(--selected); }
@media (max-width: 640px) { #toc { display: none; } }

section { flex: 1; min-width: 0; height: 100%; overflow-y: auto; box-sizing: border-box; padding: 24px 40px 48px; }
section > * { max-width: 660px; }
h1 { font-size: 2em; line-height: 1.15; }
h2 { font-size: 1.3em; margin-top: 1.8em; }
h3 { font-size: 1.05em; margin-top: 1.4em; }
code { font: 0.92em ui-monospace, Menlo, monospace; }
pre { padding: 10px 12px; overflow-x: auto; background: rgba(127, 127, 127, 0.14); border-radius: 6px; }
table { border-collapse: collapse; width: 100%; }
th, td { text-align: left; vertical-align: top; padding: 6px 10px; border-bottom: 1px solid var(--line); }
img { max-width: 100%; height: auto; }
"""

# Fills in the sidebar. It's a script so that the sidebar's text isn't indexed on every page,
# which would make every page match a search for any page's title.
TOC_SCRIPT = """document.getElementById("toc").innerHTML = {toc};
var page = location.pathname.split("/").pop();
document.querySelectorAll("#toc a").forEach(function (a) {{
    if (a.getAttribute("href") === page) a.className = "current";
}});
"""


def inline(text):
    code_spans = []

    def stash_code(match):
        code_spans.append(f"<code>{html.escape(match.group(1), quote=False)}</code>")
        return f"\0{len(code_spans) - 1}\0"

    def convert_link(match):
        target = match.group(2)

        # Guide pages link to each other as .md, so they work on GitHub too
        if not re.match(r"[a-z]+:", target):
            target = re.sub(r"(?:^|(?<=/))README\.md", "index.html", target)
            target = re.sub(r"\.md(#|$)", r".html\1", target)

        return f'<a href="{html.escape(target)}">{match.group(1)}</a>'

    text = re.sub(r"`([^`]+)`", stash_code, text)
    text = html.escape(text, quote=False)
    text = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)", r'<img alt="\1" src="\2">', text)
    text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", convert_link, text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<![\w*])\*([^*\s][^*]*)\*(?![\w*])", r"<em>\1</em>", text)
    return re.sub(r"\0(\d+)\0", lambda match: code_spans[int(match.group(1))], text)


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", re.sub(r"[*`]", "", text).lower()).strip("-")


def parse_list(lines, start, indent):
    pattern = re.compile(r"^( *)([-*]|\d+\.) +(.*)$")
    tag = "ol" if pattern.match(lines[start]).group(2)[0].isdigit() else "ul"
    output = [f"<{tag}>"]
    i = start

    while i < len(lines):
        match = pattern.match(lines[i])

        if match is None or len(match.group(1)) < indent:
            break

        if len(match.group(1)) > indent:
            nested, i = parse_list(lines, i, len(match.group(1)))
            output[-1] = output[-1].removesuffix("</li>") + "\n" + nested + "</li>"
            continue

        output.append(f"<li>{inline(match.group(3))}</li>")
        i += 1

    output.append(f"</{tag}>")
    return "\n".join(output), i


def table_cells(line):
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def convert(markdown):
    lines = markdown.splitlines()
    output = []
    i = 0

    while i < len(lines):
        line = lines[i]

        if not line.strip():
            i += 1
        elif line.startswith("```"):
            end = lines.index("```", i + 1)
            code = html.escape("\n".join(lines[i + 1:end]), quote=False)
            output.append(f"<pre><code>{code}</code></pre>")
            i = end + 1
        elif (heading := re.match(r"^(#{1,3}) +(.*)$", line)):
            level = len(heading.group(1))
            text = heading.group(2)
            output.append(f'<h{level} id="{slug(text)}">{inline(text)}</h{level}>')
            i += 1
        elif re.match(r"^ *([-*]|\d+\.) +", line):
            block, i = parse_list(lines, i, 0)
            output.append(block)
        elif line.startswith("|"):
            header = table_cells(line)
            i += 2
            rows = []

            while i < len(lines) and lines[i].startswith("|"):
                rows.append(table_cells(lines[i]))
                i += 1

            head = "".join(f"<th>{inline(cell)}</th>" for cell in header)
            body = "".join("<tr>" + "".join(f"<td>{inline(cell)}</td>" for cell in row) + "</tr>\n" for row in rows)
            output.append(f"<table>\n<thead><tr>{head}</tr></thead>\n<tbody>\n{body}</tbody>\n</table>")
        else:
            paragraph = []

            while i < len(lines) and lines[i].strip() and not re.match(r"^(#{1,3} |```|\||( *)([-*]|\d+\.) +)", lines[i]):
                paragraph.append(lines[i].strip())
                i += 1

            output.append(f"<p>{inline(' '.join(paragraph))}</p>")

    return "\n".join(output)


def toc_html(guide_dir):
    """The index page's headings and links are the sidebar's sections and pages."""
    output = ['<ul><li><a href="index.html">Welcome</a></li></ul>']

    for line in (guide_dir / "README.md").read_text(encoding="utf-8").splitlines():
        if line.startswith("## "):
            output.append(f"<p>{html.escape(line[3:])}</p><ul>")
        elif (entry := re.match(r"^- \[([^\]]+)\]\(([^)]+)\.md\)", line)):
            output.append(f'<li><a href="{entry.group(2)}.html">{html.escape(entry.group(1))}</a></li>')
        elif not line.strip() and output[-1].endswith("</li>") and len(output) > 1:
            output.append("</ul>")

    return "".join(output) + "</ul>"


def page_title(markdown, fallback):
    match = re.search(r"^# +(.*)$", markdown, re.MULTILINE)
    return match.group(1) if match else fallback


def build_book(guide_dir, resources_dir, version):
    book = resources_dir / "Curve.help"
    shutil.rmtree(book, ignore_errors=True)

    pages_dir = book / "Contents" / "Resources" / "en.lproj"
    pages_dir.mkdir(parents=True)

    with open(book / "Contents" / "Info.plist", "wb") as info:
        plistlib.dump({
            "CFBundleDevelopmentRegion": "en",
            "CFBundleIdentifier": HELP_BUNDLE_ID,
            "CFBundleInfoDictionaryVersion": "6.0",
            "CFBundleName": BOOK_TITLE,
            "CFBundlePackageType": "BNDL",
            "CFBundleShortVersionString": version,
            "CFBundleSignature": "hbwr",
            "CFBundleVersion": version,
            "HPDBookAccessPath": "index.html",
            "HPDBookIndexPath": "Curve.helpindex",
            "HPDBookTitle": BOOK_TITLE,
            "HPDBookType": "3",
        }, info)

    (pages_dir / "style.css").write_text(STYLESHEET, encoding="utf-8")
    (pages_dir / "toc.js").write_text(TOC_SCRIPT.format(toc=json.dumps(toc_html(guide_dir))), encoding="utf-8")

    for source in sorted(guide_dir.glob("*.md")):
        markdown = source.read_text(encoding="utf-8")
        title = page_title(markdown, source.stem)
        is_index = source.name == "README.md"
        target = pages_dir / ("index.html" if is_index else source.with_suffix(".html").name)

        target.write_text(PAGE_TEMPLATE.format(
            # Help Viewer uses this tag to find the book's title page
            apple_title=f'<meta name="AppleTitle" content="{BOOK_TITLE}">\n' if is_index else "",
            title=html.escape(title),
            body=convert(markdown)), encoding="utf-8")

    images = guide_dir / "images"

    if images.is_dir():
        shutil.copytree(images, pages_dir / "images")

    # Spotlight-format index (required since macOS 10.14), plus the older format for search in Help Viewer
    subprocess.run(["hiutil", "-I", "corespotlight", "-Caf", str(pages_dir / "Curve.cshelpindex"), str(pages_dir)], check=True)
    subprocess.run(["hiutil", "-I", "lsm", "-Caf", str(pages_dir / "Curve.helpindex"), str(pages_dir)], check=True)


def copy_plain_text_guide(guide_dir, resources_dir):
    target = resources_dir / "guide"
    shutil.rmtree(target, ignore_errors=True)
    shutil.copytree(guide_dir, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--guide", type=Path, required=True, help="folder containing the guide's Markdown files")
    parser.add_argument("--resources", type=Path, required=True, help="the app bundle's Resources folder")
    parser.add_argument("--version", required=True, help="Curve version")
    args = parser.parse_args()

    build_book(args.guide, args.resources, args.version)
    copy_plain_text_guide(args.guide, args.resources)


if __name__ == "__main__":
    main()
