#!/usr/bin/env python3
"""Check repository-local Markdown links, anchors and documentation reachability.

No network access or third-party dependencies. Fenced examples are ignored;
GitHub-style headings and explicit HTML id anchors are supported.
"""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


def prose(text: str) -> str:
    lines = []
    fence = ""
    for line in text.splitlines():
        match = re.match(r"^\s*(`{3,}|~{3,})", line)
        if match:
            token = match.group(1)
            if not fence:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = ""
            lines.append("")
        else:
            lines.append("" if fence else line)
    if fence:
        raise ValueError("unclosed fenced code block")
    return "\n".join(lines)


def anchors(text: str) -> set[str]:
    result = set(re.findall(r'<[^>]+\b(?:id|name)=[\"\']([^\"\']+)[\"\']', text))
    counts: Counter[str] = Counter()
    for heading in re.findall(r"^#{1,6}\s+(.+?)\s*#*\s*$", text, re.M):
        heading = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", heading)
        heading = re.sub(r"<[^>]+>", "", heading)
        slug = re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-")
        suffix = f"-{counts[slug]}" if counts[slug] else ""
        result.add(slug + suffix)
        counts[slug] += 1
    return result


def links(text: str):
    for line_no, line in enumerate(text.splitlines(), 1):
        for match in re.finditer(r"\]\(<?([^\s)>]+)>?(?:\s+[\"'][^\n]*?[\"'])?\)", line):
            yield line_no, match.group(1)
        # Reference-style definitions (the target is validated even if unused).
        match = re.match(r"^\s*\[[^\]]+\]:\s*<?([^\s>]+)>?", line)
        if match:
            yield line_no, match.group(1)


def check_docs(root: Path = ROOT) -> None:
    root = root.resolve()
    paths = set(root.glob("*.md")) | set((root / "docs").rglob("*.md"))
    paths |= set((root / "rk-local-sdk").glob("*.md"))
    raw_texts = {p.resolve(): p.read_text(encoding="utf-8") for p in paths}
    unlisted = {p for p, text in raw_texts.items()
                if "<!-- documentation-index: unlisted -->" in text}
    texts = {p: prose(text) for p, text in raw_texts.items()}
    ids = {p: anchors(text) for p, text in texts.items()}
    graph = {p: set() for p in texts}
    errors = []
    checked = 0
    for source, text in texts.items():
        for line_no, url in links(text):
            parsed = urlsplit(url)
            if parsed.scheme or parsed.netloc:
                continue
            target = (source.parent / unquote(parsed.path)).resolve() if parsed.path else source
            location = f"{source.relative_to(root)}:{line_no}"
            try:
                target.relative_to(root)
            except ValueError:
                errors.append(f"{location}: link escapes SDK package: {url}")
                continue
            if not target.exists():
                errors.append(f"{location}: missing target: {url}")
                continue
            if target in graph:
                graph[source].add(target)
            if parsed.fragment and target.suffix == ".md":
                if target not in ids:
                    ids[target] = anchors(prose(target.read_text(encoding="utf-8")))
                if unquote(parsed.fragment) not in ids[target]:
                    errors.append(f"{location}: missing anchor: {url}")
            checked += 1
    for name in ("README.md", "README.zh-CN.md"):
        start = root / "docs" / name
        seen = set()
        pending = [start]
        while pending:
            page = pending.pop()
            if page in seen:
                continue
            seen.add(page)
            pending.extend(graph.get(page, ()))
        for page in sorted((root / "docs").rglob("*.md")):
            if page.resolve() not in seen and page.resolve() not in unlisted:
                errors.append(f"docs/{name}: unreachable page: {page.relative_to(root)}")
    if errors:
        raise RuntimeError("\n".join(errors))
    print(f"Verified {checked} local links/anchors in {len(texts)} Markdown files; "
          f"both indexes reach all listed pages ({len(unlisted)} unlisted pages checked).")


if __name__ == "__main__":
    try:
        check_docs()
    except (OSError, ValueError, RuntimeError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
