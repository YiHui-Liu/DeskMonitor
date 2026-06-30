#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path


SCRIPT_PATH = Path(globals().get("__file__", "scripts/minify_web.py")).resolve()
ROOT = SCRIPT_PATH.parents[1]
DEFAULT_SOURCE = ROOT / "scripts" / "controller.html"
DEFAULT_OUTPUT = ROOT / "data" / "controller.html"

SPECIAL_BLOCK_RE = re.compile(r"(<(script|style|pre)\b[^>]*>)(.*?)(</\2>)", re.IGNORECASE | re.DOTALL)
HTML_COMMENT_RE = re.compile(r"<!--.*?-->", re.DOTALL)
CSS_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)


def minify_html_text(text: str) -> str:
    text = HTML_COMMENT_RE.sub("", text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r">\s+<", "><", text)
    return text.strip()


def minify_style(text: str) -> str:
    text = CSS_COMMENT_RE.sub("", text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"\s*([{}:;,>+])\s*", r"\1", text)
    text = re.sub(r";}", "}", text)
    return text.strip()


def minify_script(text: str) -> str:
    lines = [line.strip() for line in text.splitlines()]
    return "\n".join(line for line in lines if line)


def minify_html(source: str) -> str:
    parts: list[str] = []
    pos = 0

    for match in SPECIAL_BLOCK_RE.finditer(source):
        parts.append(minify_html_text(source[pos : match.start()]))

        open_tag = match.group(1).strip()
        tag = match.group(2).lower()
        body = match.group(3)
        close_tag = match.group(4).strip()

        if tag == "style":
            body = minify_style(body)
        elif tag == "script":
            body = minify_script(body)
        else:
            body = body.strip()

        parts.append(f"{open_tag}{body}{close_tag}")
        pos = match.end()

    parts.append(minify_html_text(source[pos:]))
    html = "".join(part for part in parts if part)
    return html + "\n"


def generate(source: Path = DEFAULT_SOURCE, output: Path = DEFAULT_OUTPUT) -> None:
    source = source.resolve()
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.stat().st_mtime >= source.stat().st_mtime:
        return
    output.write_text(minify_html(source.read_text(encoding="utf-8")), encoding="utf-8")


def check(source: Path = DEFAULT_SOURCE, output: Path = DEFAULT_OUTPUT) -> None:
    generate(source, output)
    source_text = source.read_text(encoding="utf-8")
    output_text = output.read_text(encoding="utf-8")

    assert output.exists(), "missing generated controller.html"
    assert output.stat().st_size < source.stat().st_size, "generated controller.html is not smaller"
    assert "<!--" not in output_text, "HTML comments were not removed"
    assert "qweather_result" in output_text, "qweather result block missing"
    assert "loadDiagnostics" in output_text, "diagnostics script missing"
    assert "epoch_sec" in output_text, "time sync script missing"
    assert source_text != output_text, "output should differ from editable source"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate minified web assets for LittleFS")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true", help="generate and run basic assertions")
    return parser.parse_args()


def cli() -> None:
    args = parse_args()
    if args.check:
        check(args.source, args.output)
    else:
        generate(args.source, args.output)


try:
    Import("env")
except NameError:
    if __name__ == "__main__":
        cli()
else:
    generate()
