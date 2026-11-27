# Help content structure

The in-app help (RES-HELP-001..012, ADR-000215) has one source: the Markdown
pages under `help/src/`. `tools/help/build_help.py` turns them into the HTML,
navigation and search index the application shows
([DOCUMENTATION_BUILD.md](DOCUMENTATION_BUILD.md)). Never edit generated HTML.

## Layout

```text
help/src/categories.json        the categories, in navigation order (folder, id prefix, title)
help/src/index.md               the landing page (id: home)
help/src/<folder>/<page>.md     one page per topic
help/src/images/screenshots/    screenshots captured from the running application (never drawn)
help/src/images/figures/        explanatory figures (diagrams), labelled as such
help/screenshots/manifest.json  every screenshot: name, page, what it shows, the steps that capture it
```

## A page

```markdown
---
id: acoustics.focusing
title: Focusing
category: Acoustic Engineering
order: 50
status: available
summary: Make an array's waves arrive in phase at a point, by phase or by time delay.
keywords: [focus, phase, delay, steering, focal spot]
related: [acoustics.arrays, concepts.targets, acoustics.field-analysis]
---

Body in Markdown.
```

* `id` is the stable topic id: `<category prefix>.<name>`, lower case, digits,
  `.` and `-`. Context help, links, demos and screenshots refer to it; never
  rename one (add a new page and keep the old id if a topic moves).
* `category` is the title of the page's folder's category.
* `status` is one of `available`, `experimental`, `planned`, `not-implemented`
  and is shown as a badge. A page about something Resonant does not do says so
  plainly; it never shows an application screenshot.
* `order` sorts the pages of a category (10, 20, ...). `related` ids must exist.

## The Markdown subset

| Write | For |
|---|---|
| `## Heading` ... `#### Heading` | sections (the title is the only top heading; each heading gets an anchor: its words in lower case joined by `-`) |
| blank-line separated text | paragraphs |
| `**bold**`, `*italic*`, `` `code` `` | emphasis, names of menus, fields and code |
| `- item`, `1. item` (indent two spaces to nest) | lists |
| `| a | b |` + `|---|---|` | tables |
| ```` ```python ```` ... ```` ``` ```` | code examples |
| ```` ```math ```` ... ```` ``` ```` | displayed equations, one per line, in readable Unicode notation (`p(r, t) = (A / r) · cos(ωt − kr)`, `k = 2π / λ`); inline: `$k = ω / c$` |
| a link to another page, relative to the page as in any Markdown (from `help/src/acoustics/`: `../mechanics/modal.md`, `../electromagnetics/patterns.md#directivity`), or `#section` | links: only to pages and sections (checked at build time and by `scripts/doc_integrity.py`) |
| an image on its own line, relative to the page (`../images/screenshots/name.png`), its alt text being the caption | a screenshot (click to enlarge) |
| `> **Note** ...`, `> **Warning** ...` | notes and warnings |

External web links are shown as text: the help works offline. Write links
and images as standard relative Markdown so that they work in the help, in an
editor's preview and on the repository site; `topic:<id>` links are still
understood by the build but are not used in the sources.

For example, [the modal analysis page](../../help/src/mechanics/modal.md) is
linked from a page in `help/src/acoustics/` as `../mechanics/modal.md`.

## What a feature page contains

For each major feature: what it does; when to use it; how to configure it
(the real menu, panel and field names); the important parameters with units;
what results to expect (with a worked example from `examples/`); common
mistakes; limitations and assumptions; related features; screenshots from the
manifest. Physics pages give the governing equations and what each symbol
means. Use the application's terminology (as in its menus and panels).

## Statuses and honesty

Document what the application does. Where a feature is partial, say what
works and what does not; where it is absent, mark the page `planned` or
`not-implemented` and describe the current alternative. Do not invent
screenshots, results, menu entries or options.
