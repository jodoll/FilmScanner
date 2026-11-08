#!/usr/bin/env python3
"""Check references in the repository's Markdown files.

Usage:
    ./scripts/check_docs.py [PATH ...]

With no PATH, every *.md file in the repository is checked. PATH may name a
Markdown file or a directory to scan recursively.

The script checks:

- relative link targets exist,
- "#anchor" fragments match a heading (GitHub-style slugs),
- code fences are balanced.

Exit status: 0 = clean, 1 = problems found, 2 = usage error.

External URLs are not fetched. Reference-style links and unusual Markdown link
destinations are not parsed.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^#{1,6}\s+(.*)$")
FENCE_RE = re.compile(r"^[ \t]*(```|~~~)")
EXTERNAL_RE = re.compile(r"^(?:[a-zA-Z][a-zA-Z0-9+.-]*:|//)")
SKIP_DIRS = {".git", ".pio", ".venv", "node_modules"}


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def display(path: Path, root: Path) -> str:
    try:
        return str(path.relative_to(root))
    except ValueError:
        return str(path)


def content_lines(text: str):
    """Yield (line number, line) outside fenced code blocks."""
    in_fence = False
    for line_number, line in enumerate(text.splitlines(), 1):
        if FENCE_RE.match(line):
            in_fence = not in_fence
            continue
        if not in_fence:
            yield line_number, line


def iter_links(text: str):
    for line_number, line in content_lines(text):
        for match in LINK_RE.finditer(line):
            destination = match.group(1).strip()
            if destination.startswith("<") and destination.endswith(">"):
                destination = destination[1:-1]
            yield line_number, destination


def slug(text: str) -> str:
    text = text.strip().lower()
    text = "".join(ch for ch in text if ch.isalnum() or ch in " -_")
    return text.replace(" ", "-")


def heading_anchors(text: str) -> set[str]:
    seen: dict[str, int] = {}
    anchors: set[str] = set()
    for _, line in content_lines(text):
        match = HEADING_RE.match(line)
        if not match:
            continue
        base = slug(match.group(1))
        count = seen.get(base, 0)
        seen[base] = count + 1
        anchors.add(base if count == 0 else f"{base}-{count}")
    return anchors


def markdown_files(root: Path, paths: list[str]):
    if not paths:
        yield from sorted(
            path for path in root.rglob("*.md")
            if not SKIP_DIRS.intersection(path.parts)
        )
        return

    for raw_path in paths:
        path = Path(raw_path)
        if path.is_dir():
            yield from sorted(
                child for child in path.rglob("*.md")
                if not SKIP_DIRS.intersection(child.parts)
            )
        elif path.is_file() and path.suffix == ".md":
            yield path
        else:
            raise ValueError(f"not a Markdown file or directory: {raw_path}")


def check_file(path: Path, root: Path, anchor_cache: dict[Path, set[str]]):
    text = path.read_text(encoding="utf-8")
    problems: list[str] = []
    links = 0

    for line_number, destination in iter_links(text):
        if not destination or EXTERNAL_RE.match(destination):
            continue

        links += 1
        file_part, _, anchor = destination.partition("#")
        target = (path.parent / file_part).resolve() if file_part else path

        if not target.exists():
            problems.append(
                f"{display(path, root)}:{line_number}: "
                f"broken link: {destination}"
            )
            continue

        if anchor and target.suffix == ".md" and target.is_file():
            if target not in anchor_cache:
                anchor_cache[target] = heading_anchors(
                    target.read_text(encoding="utf-8")
                )
            if anchor not in anchor_cache[target]:
                problems.append(
                    f"{display(path, root)}:{line_number}: "
                    f"missing anchor: {destination}"
                )

    fence_count = sum(
        1 for line in text.splitlines() if FENCE_RE.match(line)
    )
    if fence_count % 2:
        problems.append(
            f"{display(path, root)}: unbalanced code fence "
            f"({fence_count} fence lines)"
        )

    return links, problems


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Check Markdown links, heading anchors and code fences. "
            "With no PATH, every *.md file in the repository is checked."
        )
    )
    parser.add_argument(
        "paths",
        nargs="*",
        metavar="PATH",
        help="Markdown file or directory to check (default: whole repository)",
    )
    args = parser.parse_args(argv)

    root = repo_root()
    try:
        files = sorted(set(markdown_files(root, args.paths)))
    except ValueError as error:
        parser.error(str(error))

    anchor_cache: dict[Path, set[str]] = {}
    total_links = 0
    problems: list[str] = []

    for path in files:
        links, file_problems = check_file(path.resolve(), root, anchor_cache)
        total_links += links
        problems.extend(file_problems)

    for problem in problems:
        print(problem)

    print(
        f"Docs check: {len(files)} file(s), "
        f"{total_links} link(s), {len(problems)} problem(s)."
    )

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
