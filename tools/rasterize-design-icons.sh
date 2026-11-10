#!/usr/bin/env bash
# Optional asset regeneration only; committed PNGs keep normal builds self-contained.
# Uses librsvg's rsvg-convert, preserving the original SVG root size and aspect ratio.
set -euo pipefail
cd "$(dirname "$0")/.."
for asset in content/ui/icons/*.svg; do
    rsvg-convert --zoom 2 "$asset" --output "${asset%.svg}.png"
done
rsvg-convert content/ui/hero-mask.svg --output content/ui/hero-mask.png
