#!/usr/bin/env python3
"""Builds Resonant's in-app HTML help from its Markdown sources
(REQ-CHAT-20261126-010, RES-HELP-*, docs/help/DOCUMENTATION_BUILD.md).

    python3 tools/help/build_help.py [--source help/src] [--out build/debug/help] [--draft]

Steps (each one reported; any error fails the build with exit status 1):
  1. read every page (front matter + Markdown) and validate its structure;
  2. validate the cross-references: topic links, anchors, related topics,
     images, screenshot references, links from available pages to topics
     that are planned or not implemented;
  3. generate the HTML pages (one per topic id), the navigation (index.json)
     and the search index (search.json);
  4. copy the static assets (images, style sheet);
  5. write build-report.json (and print it).

--draft turns missing screenshots into warnings (for writing pages before
the screenshots are captured); a release build never uses it.

Only the Python standard library is used. The Markdown dialect is a small,
strict subset (docs/help/HELP_CONTENT_STRUCTURE.md); anything it does not
understand is shown as text, never dropped.
"""
import argparse
import datetime
import html
import json
import math
import os
import re
import shutil
import sys
from collections import Counter, defaultdict

STATUSES = ("available", "experimental", "planned", "not-implemented")
STATUS_LABEL = {"available": "Available", "experimental": "Experimental", "planned": "Planned", "not-implemented": "Not implemented"}
REQUIRED = ("id", "title", "category", "status", "summary")
WORD = re.compile(r"[a-z0-9][a-z0-9_\-]*")
STOP = set("a an and are as at be by can for from has have in is it its of on or that the this to was were will with you your".split())

STYLE = """
body { font-family: sans-serif; font-size: 10.5pt; color: #1d2530; background-color: #ffffff; margin: 8px; }
h1 { font-size: 19pt; color: #13406b; margin-bottom: 4px; }
h2 { font-size: 14pt; color: #13406b; margin-top: 18px; }
h3 { font-size: 12pt; color: #1f4f7f; margin-top: 12px; }
h4 { font-size: 11pt; color: #1f4f7f; }
p.crumbs { color: #5a6778; font-size: 9pt; }
p.summary { color: #3b4a5c; font-style: italic; }
span.status-available { color: #1b6e2e; font-weight: bold; }
span.status-experimental { color: #8a5a00; font-weight: bold; }
span.status-planned { color: #6b3fa0; font-weight: bold; }
span.status-not-implemented { color: #a12a2a; font-weight: bold; }
pre { font-family: monospace; background-color: #f2f4f7; padding: 6px; }
code { font-family: monospace; background-color: #f2f4f7; }
p.math { font-family: serif; font-style: italic; font-size: 12pt; margin-left: 24px; background-color: #f7f8fb; padding: 4px; }
span.math { font-family: serif; font-style: italic; }
table.grid { border-collapse: collapse; }
table.grid td, table.grid th { border: 1px solid #c8cfd8; padding: 3px; }
table.grid th { background-color: #e8edf3; }
p.caption { color: #5a6778; font-size: 9pt; }
div.note { background-color: #eef5fc; padding: 6px; }
div.warning { background-color: #fdf1e6; padding: 6px; }
p.related { color: #3b4a5c; }
p.nav { color: #5a6778; font-size: 9pt; }
"""


class BuildError(Exception):
    pass


class Report:
    def __init__(self):
        self.errors, self.warnings, self.steps = [], [], []

    def error(self, where, text):
        self.errors.append(f"{where}: {text}")

    def warn(self, where, text):
        self.warnings.append(f"{where}: {text}")

    def step(self, name, detail):
        self.steps.append({"step": name, "detail": detail})
        print(f"[help] {name}: {detail}")


# --- reading ----------------------------------------------------------------

def parse_front_matter(text, where, report):
    if not text.startswith("---\n"):
        report.error(where, "the page does not start with front matter (---)")
        return {}, text
    end = text.find("\n---\n", 4)
    if end < 0:
        report.error(where, "the front matter is not closed (---)")
        return {}, text
    meta = {}
    for number, line in enumerate(text[4:end].splitlines(), start=2):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if ":" not in line:
            report.error(where, f"front matter line {number} is not 'key: value'")
            continue
        key, value = line.split(":", 1)
        value = value.strip()
        if value.startswith("[") and value.endswith("]"):
            meta[key.strip()] = [v.strip().strip("'\"") for v in value[1:-1].split(",") if v.strip()]
        else:
            meta[key.strip()] = value.strip("'\"")
    return meta, text[end + 5:]


def read_pages(source, categories, report):
    pages, ids = [], {}
    folders = {c["folder"]: c for c in categories}
    for root, _dirs, files in os.walk(source):
        for name in sorted(files):
            if not name.endswith(".md"):
                continue
            path = os.path.join(root, name)
            rel = os.path.relpath(path, source)
            with open(path, encoding="utf-8") as f:
                meta, body = parse_front_matter(f.read(), rel, report)
            for key in REQUIRED:
                if not meta.get(key):
                    report.error(rel, f"the front matter has no '{key}'")
            if meta.get("status") and meta["status"] not in STATUSES:
                report.error(rel, f"the status '{meta['status']}' is not one of {', '.join(STATUSES)}")
            folder = os.path.dirname(rel)
            if folder and folder not in folders:
                report.error(rel, f"the folder '{folder}' is not a category in categories.json")
            elif folder and meta.get("category") and meta["category"] != folders[folder]["title"]:
                report.error(rel, f"the category '{meta['category']}' does not match the folder's category '{folders[folder]['title']}'")
            pid = meta.get("id", rel)
            if not re.fullmatch(r"[a-z0-9][a-z0-9.\-]*", pid):
                report.error(rel, f"the id '{pid}' is not lower-case letters, digits, '.' and '-'")
            if pid in ids:
                report.error(rel, f"the id '{pid}' is also used by {ids[pid]}")
            ids[pid] = rel
            try:
                order = float(meta.get("order", 100))
            except ValueError:
                report.error(rel, "the order is not a number")
                order = 100
            pages.append({"id": pid, "file": rel, "folder": folder, "meta": meta, "body": body, "order": order})
    return pages


# --- Markdown subset ----------------------------------------------------------

def slug(text):
    s = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return s or "section"


def inline(text, ctx):
    """Inline Markdown: code, math, images, links, bold, italic (HTML-escaped)."""
    out, i = [], 0
    pattern = re.compile(r"`([^`]+)`|\$([^$\n]+)\$|!\[([^\]]*)\]\(([^)\s]+)(?:\s+\"([^\"]*)\")?\)|\[([^\]]+)\]\(([^)\s]+)\)|\*\*([^*]+)\*\*|\*([^*\s][^*]*)\*")
    for m in pattern.finditer(text):
        out.append(html.escape(text[i:m.start()]))
        i = m.end()
        if m.group(1) is not None:
            out.append(f"<code>{html.escape(m.group(1))}</code>")
        elif m.group(2) is not None:
            out.append(f'<span class="math">{html.escape(m.group(2))}</span>')
        elif m.group(4) is not None:
            out.append(ctx.image(m.group(4), m.group(3), m.group(5), inline=True))
        elif m.group(7) is not None:
            out.append(ctx.link(m.group(7), inline(m.group(6), ctx)))
        elif m.group(8) is not None:
            out.append(f"<b>{inline(m.group(8), ctx)}</b>")
        elif m.group(9) is not None:
            out.append(f"<i>{inline(m.group(9), ctx)}</i>")
    out.append(html.escape(text[i:]))
    return "".join(out)


class PageContext:
    def __init__(self, page, build):
        self.page, self.build = page, build
        self.links, self.images, self.headings = [], [], []

    def link(self, target, label):
        if target.startswith(("http://", "https://")):
            self.build.report.warn(self.page["file"], f"an external link ({target}) does not work offline; it is shown as text")
            return f"{label} (<code>{html.escape(target)}</code>)"
        path, _, fragment = target.partition("#")
        if target.startswith("topic:"):
            ref = target[6:]
        elif target.startswith("#"):
            ref = self.page["id"] + target
        elif path.endswith(".md"):
            # A relative link to another page, as in any Markdown (checked by the repository's own audits too).
            file = os.path.normpath(os.path.join(self.page["folder"], path)).replace(os.sep, "/")
            topic = self.build.by_file.get(file)
            if topic is None:
                self.build.report.error(self.page["file"], f"the link '{target}' points to a missing page ({file})")
                return label
            ref = topic + (f"#{fragment}" if fragment else "")
        else:
            self.build.report.error(self.page["file"], f"the link '{target}' is neither a relative .md page, #anchor nor topic:<id>")
            return label
        self.links.append(ref)
        topic, _, anchor = ref.partition("#")
        href = f"{topic}.html" + (f"#{anchor}" if anchor else "")
        return f'<a href="{html.escape(href)}">{label}</a>'

    def image(self, src, alt, caption, inline=False):
        # Relative to the page, as in any Markdown; kept as a path relative to the sources.
        src = os.path.normpath(os.path.join(self.page["folder"], src)).replace(os.sep, "/")
        self.images.append(src)
        width = 620 if not inline else 300
        tag = f'<a href="zoom:{html.escape(src)}"><img src="{html.escape(src)}" width="{width}" alt="{html.escape(alt)}"/></a>'
        if inline:
            return tag
        cap = f'<p class="caption">{html.escape(caption or alt)} (click to enlarge)</p>' if (caption or alt) else ""
        return f"<p>{tag}</p>{cap}"


def markdown(body, ctx):
    lines = body.splitlines()
    out, i, n = [], 0, len(lines)
    para = []

    def flush():
        if para:
            out.append(f"<p>{inline(' '.join(para), ctx)}</p>")
            para.clear()

    while i < n:
        line = lines[i]
        stripped = line.strip()
        if stripped.startswith("```"):
            flush()
            lang = stripped[3:].strip()
            block = []
            i += 1
            while i < n and not lines[i].strip().startswith("```"):
                block.append(lines[i])
                i += 1
            if i >= n:
                ctx.build.report.error(ctx.page["file"], "a code block is not closed (```)")
            i += 1
            if lang == "math":
                for row in block:
                    if row.strip():
                        out.append(f'<p class="math">{html.escape(row.strip())}</p>')
            else:
                out.append(f"<pre>{html.escape(chr(10).join(block))}</pre>")
            continue
        if not stripped:
            flush()
            i += 1
            continue
        heading = re.match(r"(#{1,4})\s+(.*)", stripped)
        if heading:
            flush()
            level = len(heading.group(1)) + 1  # the page title is the only <h1>
            text = heading.group(2).strip()
            anchor = slug(text)
            ctx.headings.append({"text": text, "anchor": anchor, "level": level})
            out.append(f'<a name="{anchor}"></a><h{min(level, 4)}>{inline(text, ctx)}</h{min(level, 4)}>')
            i += 1
            continue
        image = re.fullmatch(r'!\[([^\]]*)\]\(([^)\s]+)(?:\s+"([^"]*)")?\)', stripped)
        if image:
            flush()
            out.append(ctx.image(image.group(2), image.group(1), image.group(3)))
            i += 1
            continue
        if stripped in ("---", "***"):
            flush()
            out.append("<hr/>")
            i += 1
            continue
        if stripped.startswith("|"):
            flush()
            rows = []
            while i < n and lines[i].strip().startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            header = rows[0]
            body_rows = rows[2:] if len(rows) > 1 and all(re.fullmatch(r":?-{2,}:?", c) for c in rows[1]) else rows[1:]
            t = ['<table class="grid" cellspacing="0" cellpadding="3"><tr>' + "".join(f"<th>{inline(c, ctx)}</th>" for c in header) + "</tr>"]
            for r in body_rows:
                t.append("<tr>" + "".join(f"<td>{inline(c, ctx)}</td>" for c in r) + "</tr>")
            out.append("".join(t) + "</table>")
            continue
        if stripped.startswith(">"):
            flush()
            quote = []
            while i < n and lines[i].strip().startswith(">"):
                quote.append(lines[i].strip()[1:].strip())
                i += 1
            text = " ".join(quote)
            kind = "warning" if text.lower().startswith(("**warning", "**caution", "**safety")) else "note"
            out.append(f'<div class="{kind}"><p>{inline(text, ctx)}</p></div>')
            continue
        bullet = re.match(r"(\s*)([-*]|\d+\.)\s+(.*)", line)
        if bullet:
            flush()
            ordered = bullet.group(2)[0].isdigit()
            items, depth_stack = [], []
            tag = "ol" if ordered else "ul"
            html_list = [f"<{tag}>"]
            base = len(bullet.group(1))
            while i < n:
                m = re.match(r"(\s*)([-*]|\d+\.)\s+(.*)", lines[i])
                if not m:
                    if lines[i].strip() and lines[i].startswith("  ") and items:
                        items[-1] += " " + lines[i].strip()
                        html_list[-1] = f"<li>{inline(items[-1], ctx)}</li>"
                        i += 1
                        continue
                    break
                indent = len(m.group(1))
                sub = "ol" if m.group(2)[0].isdigit() else "ul"
                if indent > base and (not depth_stack or indent > depth_stack[-1][0]):
                    depth_stack.append((indent, sub))
                    html_list.append(f"<{sub}>")
                while depth_stack and indent < depth_stack[-1][0]:
                    html_list.append(f"</{depth_stack.pop()[1]}>")
                items.append(m.group(3))
                html_list.append(f"<li>{inline(m.group(3), ctx)}</li>")
                i += 1
            while depth_stack:
                html_list.append(f"</{depth_stack.pop()[1]}>")
            html_list.append(f"</{tag}>")
            out.append("".join(html_list))
            continue
        para.append(stripped)
        i += 1
    flush()
    return "\n".join(out)


def plain_text(body):
    text = re.sub(r"```.*?```", " ", body, flags=re.S)
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", " ", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    return re.sub(r"[#*`>|_$]", " ", text)


def words(text):
    return [w for w in WORD.findall(text.lower()) if w not in STOP and len(w) > 1]


# --- build ------------------------------------------------------------------

class Build:
    def __init__(self, source, out, draft, screenshot_manifest):
        self.source, self.out, self.draft = source, out, draft
        self.report = Report()
        self.screenshot_manifest = screenshot_manifest

    def run(self):
        with open(os.path.join(self.source, "categories.json"), encoding="utf-8") as f:
            categories = json.load(f)["categories"]
        pages = read_pages(self.source, categories, self.report)
        self.report.step("read", f"{len(pages)} pages in {len(categories)} categories")
        by_id = {p["id"]: p for p in pages}
        self.by_file = {p["file"].replace(os.sep, "/"): p["id"] for p in pages}
        if "home" not in by_id:
            self.report.error("index.md", "there is no landing page (id: home)")

        # Navigation order: categories in their order, pages by (order, title).
        cat_order = {c["title"]: k for k, c in enumerate(categories)}
        nav = sorted((p for p in pages if p["id"] != "home"),
                     key=lambda p: (cat_order.get(p["meta"].get("category"), 99), p["order"], p["meta"].get("title", "")))
        sequence = ([by_id["home"]] if "home" in by_id else []) + nav

        shots = self.load_screenshot_manifest()
        contexts = {}
        for p in pages:
            ctx = PageContext(p, self)
            p["html_body"] = markdown(p["body"], ctx)
            contexts[p["id"]] = ctx
        self.report.step("convert", f"{len(pages)} pages converted")

        # Cross-references.
        anchors = {pid: {h["anchor"] for h in c.headings} for pid, c in contexts.items()}
        image_count = 0
        for pid, ctx in contexts.items():
            p = by_id[pid]
            for ref in ctx.links:
                topic, _, anchor = ref.partition("#")
                if topic not in by_id:
                    self.report.error(p["file"], f"the link to topic '{topic}' points to a missing page")
                elif anchor and anchor not in anchors[topic]:
                    self.report.error(p["file"], f"the link '{ref}' points to a missing section of '{topic}'")
                elif p["meta"].get("status") == "available" and by_id[topic]["meta"].get("status") in ("planned", "not-implemented"):
                    self.report.warn(p["file"], f"links to '{topic}', which is {by_id[topic]['meta']['status']}: say so in the text")
            for rel in p["meta"].get("related", []) if isinstance(p["meta"].get("related"), list) else []:
                if rel not in by_id:
                    self.report.error(p["file"], f"the related topic '{rel}' does not exist")
            for src in ctx.images:
                image_count += 1
                path = os.path.normpath(os.path.join(self.source, src))
                if src.startswith(".."):
                    self.report.error(p["file"], f"the image '{src}' lies outside the help sources")
                    continue
                if not os.path.exists(path):
                    shot = os.path.basename(src)
                    if src.startswith("images/screenshots/") and shot in shots and self.draft:
                        self.report.warn(p["file"], f"the screenshot '{shot}' has not been captured yet")
                    else:
                        self.report.error(p["file"], f"the image '{src}' does not exist")
                if src.startswith("images/screenshots/"):
                    shot = os.path.basename(src)
                    if shot.endswith("-annotated.png"):
                        shot = shot[: -len("-annotated.png")] + ".png"
                    if shot not in shots:
                        self.report.error(p["file"], f"the screenshot '{shot}' is not in the screenshot manifest")
            status = p["meta"].get("status")
            if status in ("planned", "not-implemented") and any(s.startswith("images/screenshots/") for s in ctx.images):
                self.report.error(p["file"], f"a page whose status is {status} shows an application screenshot")
        for name, shot in shots.items():
            if shot.get("page") and shot["page"] not in by_id:
                self.report.error("screenshots manifest", f"'{name}' is associated with the missing page '{shot['page']}'")
        self.report.step("validate", f"{sum(len(c.links) for c in contexts.values())} links, {image_count} images, "
                                     f"{len(self.report.errors)} errors, {len(self.report.warnings)} warnings")
        if self.report.errors:
            return False

        # Output.
        html_dir = os.path.join(self.out, "html")
        if os.path.isdir(html_dir):
            shutil.rmtree(html_dir)
        os.makedirs(html_dir)
        with open(os.path.join(html_dir, "style.css"), "w", encoding="utf-8") as f:
            f.write(STYLE)
        cat_by_title = {c["title"]: c for c in categories}
        built = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        version = self.version()
        position = {p["id"]: k for k, p in enumerate(sequence)}
        for p in pages:
            self.write_page(p, html_dir, by_id, sequence, position, cat_by_title, built, version)
        self.report.step("html", f"{len(pages)} pages written to {html_dir}")
        copied = 0
        images_src = os.path.join(self.source, "images")
        if os.path.isdir(images_src):
            shutil.copytree(images_src, os.path.join(html_dir, "images"))
            copied = sum(len(files) for _r, _d, files in os.walk(images_src))
        self.report.step("assets", f"{copied} image files copied")

        # Navigation and search.
        toc = []
        for c in categories:
            members = [p for p in nav if p["meta"].get("category") == c["title"]]
            toc.append({"id": c["id"], "title": c["title"], "summary": c.get("summary", ""), "topics": [p["id"] for p in members]})
        topics = []
        for p in sequence:
            m = p["meta"]
            k = position[p["id"]]
            topics.append({
                "id": p["id"], "title": m.get("title"), "category": m.get("category"), "status": m.get("status"),
                "summary": m.get("summary"), "keywords": m.get("keywords", []) if isinstance(m.get("keywords"), list) else [],
                "related": m.get("related", []) if isinstance(m.get("related"), list) else [],
                "file": f"{p['id']}.html", "source": p["file"],
                "headings": contexts[p["id"]].headings,
                "previous": sequence[k - 1]["id"] if k > 0 else None,
                "next": sequence[k + 1]["id"] if k + 1 < len(sequence) else None,
                "screenshots": sorted({os.path.basename(s) for s in contexts[p["id"]].images if s.startswith("images/screenshots/")}),
            })
        index = {"format": "resonant.help-index", "version": 1, "built": built, "applicationVersion": version,
                 "home": "home", "categories": toc, "topics": topics}
        with open(os.path.join(self.out, "index.json"), "w", encoding="utf-8") as f:
            json.dump(index, f, indent=1)
        postings = defaultdict(dict)
        for p in pages:
            m = p["meta"]
            weights = Counter()
            for w in words(m.get("title", "")):
                weights[w] += 10
            for kw in m.get("keywords", []) if isinstance(m.get("keywords"), list) else []:
                for w in words(kw):
                    weights[w] += 6
            for h in contexts[p["id"]].headings:
                for w in words(h["text"]):
                    weights[w] += 4
            for w in words(m.get("summary", "")):
                weights[w] += 3
            body = Counter(words(plain_text(p["body"])))
            for w, count in body.items():
                weights[w] += 1.0 + math.log(count)
            for w, weight in weights.items():
                postings[w][p["id"]] = round(weight, 2)
        with open(os.path.join(self.out, "search.json"), "w", encoding="utf-8") as f:
            json.dump({"format": "resonant.help-search", "version": 1, "documents": len(pages), "terms": postings}, f, separators=(",", ":"))
        self.report.step("index", f"navigation of {len(topics)} topics, search index of {len(postings)} terms")
        return True

    def load_screenshot_manifest(self):
        if not self.screenshot_manifest or not os.path.exists(self.screenshot_manifest):
            self.report.error("screenshots manifest", f"not found: {self.screenshot_manifest}")
            return {}
        with open(self.screenshot_manifest, encoding="utf-8") as f:
            manifest = json.load(f)
        return {s["name"]: s for s in manifest.get("screenshots", [])}

    def version(self):
        header = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "core", "foundation", "Version.h")
        try:
            with open(header, encoding="utf-8") as f:
                m = re.search(r'kApplicationVersion\s*=\s*"([^"]+)"', f.read())
                return m.group(1) if m else "unknown"
        except OSError:
            return "unknown"

    def write_page(self, p, html_dir, by_id, sequence, position, cats, built, version):
        m = p["meta"]
        title = html.escape(m.get("title", p["id"]))
        status = m.get("status", "available")
        crumbs = ['<a href="home.html">Help</a>']
        if p["id"] != "home":
            crumbs.append(html.escape(m.get("category", "")))
        crumbs.append(title)
        k = position[p["id"]]
        prev_id = sequence[k - 1]["id"] if k > 0 else None
        next_id = sequence[k + 1]["id"] if k + 1 < len(sequence) else None
        nav = []
        if prev_id:
            nav.append(f'Previous: <a href="{prev_id}.html">{html.escape(by_id[prev_id]["meta"].get("title", prev_id))}</a>')
        if next_id:
            nav.append(f'Next: <a href="{next_id}.html">{html.escape(by_id[next_id]["meta"].get("title", next_id))}</a>')
        related = [r for r in (m.get("related") or []) if r in by_id] if isinstance(m.get("related"), list) else []
        related_html = ""
        if related:
            related_html = '<h2>Related topics</h2><p class="related">' + " · ".join(
                f'<a href="{r}.html">{html.escape(by_id[r]["meta"].get("title", r))}</a>' for r in related) + "</p>"
        badge = "" if p["id"] == "home" else f'<p><span class="status-{status}">Status: {STATUS_LABEL.get(status, status)}</span></p>'
        # The style is inlined: the in-app viewer (QTextBrowser) applies <style> reliably; style.css is kept for browsers.
        doc = (f'<html><head><meta charset="utf-8"/><title>{title}</title><style>{STYLE}</style></head><body>'
               f'<p class="crumbs">{" › ".join(crumbs)}</p><h1>{title}</h1>{badge}'
               f'<p class="summary">{html.escape(m.get("summary", ""))}</p>{p["html_body"]}{related_html}'
               f'<hr/><p class="nav">{" | ".join(nav)}</p>'
               f'<p class="nav">Resonant {html.escape(version)} help, built {built}. Topic id: <code>{html.escape(p["id"])}</code></p>'
               f'</body></html>')
        with open(os.path.join(html_dir, f"{p['id']}.html"), "w", encoding="utf-8") as f:
            f.write(doc)


def main(argv=None):
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", ".."))
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--source", default=os.path.join(root, "help", "src"))
    ap.add_argument("--out", default=os.path.join(root, "build", "debug", "help"))
    ap.add_argument("--screenshots", default=os.path.join(root, "help", "screenshots", "manifest.json"))
    ap.add_argument("--draft", action="store_true", help="missing screenshots are warnings (never for a release)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)
    os.makedirs(args.out, exist_ok=True)
    build = Build(os.path.abspath(args.source), os.path.abspath(args.out), args.draft, args.screenshots)
    ok = build.run()
    report = {"format": "resonant.help-build-report", "version": 1, "ok": ok, "steps": build.report.steps,
              "errors": build.report.errors, "warnings": build.report.warnings}
    with open(os.path.join(args.out, "build-report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    for e in build.report.errors:
        print(f"[help] ERROR {e}", file=sys.stderr)
    if not args.quiet:
        for w in build.report.warnings:
            print(f"[help] warning {w}")
    print(f"[help] {'PASS' if ok else 'FAIL'}: {len(build.report.errors)} errors, {len(build.report.warnings)} warnings")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
