import argparse, os, sys, re
from PIL import Image, ImageDraw, ImageFont

# Výchozí znaky, které se použijí pro tvorbu písma
# Toto je vlastně naše znaková sada, která může obsahovat libovolný znak Unicode
DEFAULT_CHARSET = (
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "ěščřžýáíéúůóňť+šď"
    "ĚŠČŘŽÝÁÍÉÚŮŇŤŠĎ"
    "@#$%^&!?"
    "*/-+:="
    ",.;()[]{}<>|\\/°'|\"_"
)

HEADER_TMPL = r"""#ifndef FONT_{uname}_H
#define FONT_{uname}_H

#include <stdint.h>
#include "gfx.h"

static const uint8_t {name}_bitmap[] = {{
{bitmap_rows}
}};

static const GfxGlyphAA {name}_glyphs[] = {{
{glyph_rows}
}};

static const GfxFontAA {name} = {{
    .bitmap = {name}_bitmap,
    .glyphs = {name}_glyphs,
    .glyph_count = {glyph_count},
    .ascent = {ascent},
    .descent = {descent},
    .line_height = {line_height}
}};

#endif
"""

def sanitize_name(path, size):
    base = os.path.splitext(os.path.basename(path))[0]
    base = re.sub(r'[^A-Za-z0-9_]', '_', base)
    return f"font_{base}_{size}px"

def ustr_to_codepoints(s: str):
    cps = []
    for ch in s:
        cps.append(ord(ch))
    seen = set()
    out = []
    for cp in cps:
        if cp not in seen:
            seen.add(cp)
            out.append(cp)
    return out

def main():
    ap = argparse.ArgumentParser(description="Generátor 8bitového bitmapového písma s jemným vyhlazováním pro komponentu gfx.h od @pesvklobouku")
    ap.add_argument("--ttf", required=True, help="Cesta k souboru písma (*.ttf/*.otf)")
    ap.add_argument("--size", required=True, type=int, help="Velikost písma v pixelech")
    ap.add_argument("--out",  required=True, help="Výstupní hlavičkový soubor .h")
    ap.add_argument("--name", default=None, help="Název písma (výchozí: font_<file>_<size>px)")
    ap.add_argument("--charset", default=DEFAULT_CHARSET, help="Znaky UTF-8, které se použijí pro tvorbu písma")
    ap.add_argument("--charset_file", default=None, help="Volitelný soubor se znaky UTF-8, které se použijí pro tvorbu písma")
    args = ap.parse_args()

    charset = args.charset
    if args.charset_file:
        with open(args.charset_file, "r", encoding="utf-8") as f:
            charset = f.read()
    codepoints = ustr_to_codepoints(charset)

    try:
        font = ImageFont.truetype(args.ttf, args.size)
    except Exception as e:
        print(f"Nemohu nahrát font '{args.ttf}': {e}", file=sys.stderr)
        sys.exit(1)

    ascent, descent = font.getmetrics()
    line_height = ascent + descent

    pad = max(4, args.size // 4)
    CANVAS_W = args.size * 4 + pad * 2
    CANVAS_H = args.size * 4 + pad * 2

    glyphs = []
    bitmap_bytes = bytearray()

    for cp in sorted(codepoints):
        ch = chr(cp)
        img = Image.new("L", (CANVAS_W, CANVAS_H), 0)
        draw = ImageDraw.Draw(img)
        draw.text((pad, pad), ch, fill=255, font=font)
        bbox = img.getbbox()

        try:
            xAdvance = int(round(font.getlength(ch)))
        except Exception:
            xAdvance = font.getsize(ch)[0]

        if bbox is None:
            width = height = 0
            xOffset = 0
            yOffset = -ascent
            bmp_off = len(bitmap_bytes)
        else:
            left, top, right, bottom = bbox
            width  = max(0, right - left)
            height = max(0, bottom - top)
            xOffset = left - pad
            baseline_y = pad + ascent
            yOffset = top - baseline_y 
            crop = img.crop((left, top, right, bottom))
            bitmap_bytes.extend(crop.tobytes())
            bmp_off = len(bitmap_bytes) - (width * height)

        glyphs.append(dict(
            codepoint=cp,
            width=width, height=height,
            xAdvance=xAdvance,
            xOffset=xOffset, yOffset=yOffset,
            bitmap_offset=bmp_off
        ))

    name = args.name or sanitize_name(args.ttf, args.size)

    BPR = 16
    bm_rows = []
    b = bitmap_bytes
    for i in range(0, len(b), BPR):
        chunk = b[i:i+BPR]
        bm_rows.append("    " + ", ".join(f"0x{v:02X}" for v in chunk) + ",")
    if not bm_rows:
        bm_rows = ["    /* empty */"]

    # glyphs
    gl_rows = []
    for g in glyphs:
        gl_rows.append(
            f"    {{ .codepoint={g['codepoint']}, .width={g['width']}, .height={g['height']}, "
            f".xAdvance={g['xAdvance']}, .xOffset={g['xOffset']}, .yOffset={g['yOffset']}, "
            f".bitmap_offset={g['bitmap_offset']} }},"
        )

    hdr = HEADER_TMPL.format(
        name=name,
        uname=name.upper(),
        bitmap_rows="\n".join(bm_rows),
        glyph_rows="\n".join(gl_rows),
        glyph_count=len(glyphs),
        ascent=ascent,
        descent=descent,
        line_height=line_height
    )

    with open(args.out, "w", encoding="utf-8") as f:
        f.write(hdr)

    print(f"Vytvořil jsem bitmapový AA font {args.out}")
    print(f"Glyfy: {len(glyphs)} Bajty: {len(bitmap_bytes)}  Ascent: {ascent}  Descent: {descent} Řádkový krok: {line_height}")

if __name__ == "__main__":
    main()
