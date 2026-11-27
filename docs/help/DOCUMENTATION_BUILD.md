# Documentation build

**Requirements:** RES-HELP-002, RES-HELP-011. **Tool:** `tools/help/build_help.py` (standard library only).

```bash
python3 tools/help/build_help.py --out build/debug/help           # strict (tests, packaging)
python3 tools/help/build_help.py --out build/debug/help --draft   # missing screenshots are warnings
```

Both build systems run it: qmake `help/help.pro` (an `aux` project, part of
`make`), CMake `help/CMakeLists.txt` (`resonant_help`, part of `all`). The
development build uses `--draft`; `tests/core/test_help_index.cpp` runs the
strict build and fails on any error.

## Steps

1. **Read and validate structure**: front matter (`id`, `title`, `category`,
   `status`, `summary`), unique well-formed ids, a status from the four
   allowed, folders matching categories, a home page.
2. **Validate references**: `topic:` links and `#anchors`, related topics,
   images exist and lie inside the sources, screenshots are in the manifest,
   manifest entries name existing pages, no screenshot on a planned or
   not-implemented page, a warning for an available page linking to a planned
   one, external links reported (they are shown as text: the help is offline).
3. **Generate HTML**: one file per topic id, breadcrumbs, status badge,
   summary, body, related topics, previous / next, build metadata (version,
   time, topic id); the style is inlined for the in-app viewer.
4. **Copy assets**: `help/src/images`.
5. **Navigation**: `index.json` (categories in order, topics with headings,
   related, screenshots, previous / next).
6. **Search index**: `search.json` (weighted words per topic).
7. **Report**: `build-report.json` (steps, errors, warnings) and a PASS / FAIL
   line. On any error nothing is written to `html/` and the tool exits 1:
   no step is silently skipped.

## Related generators

- `tools/help/generate_api_reference.py` writes `help/src/reference/api.md`
  from the Python package (checked current by `python/tests/test_api_reference.py`).
- `tools/help/coverage.py` writes `docs/help/DOCUMENTATION_COVERAGE.md`
  (checked current by the same test file).

## Packaging

`make install` / `cmake --install` installs `html/`, `index.json`,
`search.json` and `build-report.json` to `<prefix>/share/resonant/help`, with
the demonstrations, examples, example plugins and Python package beside it;
`resonant --check-help` verifies an installation (loads the help, opens the
home page, searches, highlights) and exits 0 when it works.

## When a feature changes

Update its help page (and the pages the coverage report lists for it),
re-capture its screenshots (`--capture-screenshots --only ...`), run its
demonstration, rebuild the help strictly. The build's version stamp and the
coverage report make stale content visible.
