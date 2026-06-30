#!/usr/bin/env bash
set -euo pipefail

python3 scripts/minify_web.py --check

python3 - <<'PY'
from html.parser import HTMLParser
from pathlib import Path

source = Path('scripts/controller.html')
output = Path('data/controller.html')

class Parser(HTMLParser):
    pass

Parser().feed(output.read_text(encoding='utf-8'))

if output.stat().st_size >= source.stat().st_size:
    raise SystemExit('minified controller.html is not smaller than source')

print(f'{source.stat().st_size} -> {output.stat().st_size} bytes')
PY
