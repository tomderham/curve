# Localization

Each language has a folder named after its `.lproj` folder, holding:

- `InfoPlist.strings`: the macOS permission prompts.
- `guide/`: a translation of every page in `docs/guide/`.

`en/` holds only `InfoPlist.strings`, whose text must match `CMakeLists.txt`. The English guide stays in `docs/guide/`.

`scripts/build_help.py` builds these into the app when it's compiled.

## Rules for translating the guide

- **The app itself is English only for now**, so anything the guide quotes from the app stays in English, exactly as it appears on screen. `app-terms.txt` lists those labels. Translate the sentence around them, not the label.
- Keep the same headings, lists, tables, links and inline code as the English page.
- Keep the note on the home page (`README.md`) saying the guide is machine translated and that corrections are welcome on GitHub. Remove it for a language once a person has reviewed that translation, and update `scripts/check_translations.py` to match.
- Keep the book title, Curve Help, in English, because it's a Settings menu item.

## Adding a language

1. Add a folder and translate the files above.
2. Add its sidebar text to `WELCOME` in `scripts/build_help.py`.
3. Add it to `CFBundleLocalizations` in `CMakeLists.txt`.
4. Run `python3 scripts/check_translations.py`.

When a page in `docs/guide/` changes, update it in every language. The checker catches changed structure, links and labels, but not changed wording.

## When the app is translated

Replace each label in the translated pages with the app's translated label, using `app-terms.txt` as the list of what to replace.
