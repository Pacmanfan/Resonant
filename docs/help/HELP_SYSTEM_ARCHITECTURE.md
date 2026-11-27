# Help system architecture

**Requirements:** RES-HELP-001..012. **Decision:** ADR-000215.
**User help:** `ui.help-window`.

```text
help/src/                     the single source (Markdown + front matter, categories.json, images)
help/screenshots/manifest.json  every screenshot: name, page, description, capture steps
tools/help/build_help.py      validate -> HTML, navigation (index.json), search index (search.json), report
tools/help/generate_api_reference.py   help/src/reference/api.md from the Python package
<build>/help/                 generated (never edited, never committed)
src/core/help/HelpIndex       navigation, breadcrumbs, reading order, ranked search (Qt-free)
src/app/HelpWindow            the window (forms/HelpWindow.ui, forms/HelpImageViewer.ui)
src/app/HelpTopics            the central help-topic registry (F1)
src/app/ResourcePaths         finds the help: $RESONANT_HELP_DIR, <prefix>/share/resonant/help, the build tree
```

## Integration choices

- **QTextBrowser, not a web engine**: QtWebEngine is not available on the
  build platforms, and a help browser needs no JavaScript. The build emits
  HTML that QTextBrowser renders (inlined style sheet, simple tables,
  `<img width>`). Mathematics is written as readable Unicode in ```math blocks
  (serif, italic), so equations render offline without MathJax.
- **Generated at build time** by both build systems (qmake `help/help.pro`,
  CMake `help/CMakeLists.txt`) into the build tree, and installed with the
  application; nothing generated is committed.
- **Offline**: no network access; external links are reported by the build
  and shown as text.

## Window

Contents tree (categories in reading order, statuses shown), search tab
(results while typing, ranked; Enter opens the first; matched words
highlighted in the article), breadcrumbs, back / forward, previous / next in
reading order, "On this page" sections, related topics at the end of each
page, screenshots that open in a zoomable viewer, a "Topic not found" page
with suggestions, a clear "Help unavailable" page when the help was not
built.

## Context help

`HelpTopics::topicFor(widget)` walks from the widget with the focus (else the
widget under the mouse) to the window: a widget's own `helpTopic` property,
then `<PanelClass>/<tabObjectName>`, then the class; the registry is one table
in `src/app/HelpTopics.cpp`. Dialogs set `helpTopic` and have Help buttons;
`topicForMessage` maps failure messages to troubleshooting topics. A test
checks that every registry topic exists in the built help and that F1 in a
panel, tab and property editor opens its topic.

## Search

The build indexes words with weights (title 10, keyword 6, heading 4,
summary 3, body 1 + ln count); `HelpIndex::search` requires every query word
(the last also as a prefix), sums weights, ranks, keeps reading order for
ties, and returns the matched index words for highlighting.
