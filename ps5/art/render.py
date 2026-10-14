#!/usr/bin/env python3
"""Compose native SVG layouts around Delapouite's CC BY 3.0 island emblem."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
root = Path(__file__).resolve().parent
source = ET.parse(root / 'island.svg').getroot()
emblem = ''.join(ET.tostring(p, encoding='unicode') for p in list(source)[1:]).replace('#fff', '#f4cf75')

def render(name, width, height, contents):
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
<defs><linearGradient id="sea" x2="1" y2="1"><stop stop-color="#0b2737"/><stop offset="1" stop-color="#17686a"/></linearGradient></defs>
<rect width="{width}" height="{height}" fill="url(#sea)"/>{contents}</svg>'''
    path = root / (name + '.svg')
    path.write_text(svg)
    subprocess.run(['rsvg-convert', str(path), '-o', str(root.parent / 'sce_sys' / (name + '.png'))], check=True)

render('icon0',512,512,f'''<circle cx="256" cy="198" r="164" fill="#0b2836" fill-opacity=".4" stroke="#85b9b0" stroke-opacity=".35" stroke-width="2"/>
<g transform="translate(105,42) scale(.59)">{emblem}</g>
<text x="256" y="410" text-anchor="middle" font-family="DejaVu Sans" font-size="45" font-weight="bold" fill="#fff5db">LINK’S</text>
<text x="256" y="462" text-anchor="middle" font-family="DejaVu Sans" font-size="39" font-weight="bold" fill="#fff5db">AWAKENING</text>''')
render('pic0',1920,1080,f'''<path d="M0 770 Q350 660 680 790 T1300 790 T1920 740 V1080 H0Z" fill="#237c7d" opacity=".35"/>
<path d="M0 870 Q350 760 680 890 T1300 890 T1920 840 V1080 H0Z" fill="#3da29a" opacity=".18"/>
<circle cx="1420" cy="485" r="310" fill="#103d49" stroke="#96b9a4" stroke-opacity=".24" stroke-width="2"/>
<g transform="translate(1160,205) scale(1.02)">{emblem}</g>
<path d="M170 340 H280" stroke="#f4cf75" stroke-width="6"/>
<text x="164" y="480" font-family="DejaVu Sans" font-size="108" font-weight="bold" fill="#fff5db">Link’s</text>
<text x="164" y="610" font-family="DejaVu Sans" font-size="108" font-weight="bold" fill="#fff5db">Awakening</text>
<text x="172" y="690" font-family="DejaVu Sans" font-size="28" letter-spacing="7" fill="#bdd6cc">PS5 NATIVE PORT</text>''')
# The PS5 shell uses BC7 DDS for selection/launch backgrounds.
import struct
from PIL import Image
import ispc_texcomp
background = root / 'background.png'
subprocess.run(['rsvg-convert', '-w', '3840', '-h', '2160', str(root / 'pic0.svg'), '-o', str(background)], check=True)
with Image.open(background) as source:
    rgba = source.convert('RGBA')
    width, height = rgba.size
    surface = ispc_texcomp.RGBASurface(rgba.tobytes(), width, height, width * 4)
    blocks = ispc_texcomp.compress_blocks_bc7(surface, ispc_texcomp.BC7EncSettings.from_profile('slow'))
assert len(blocks) == width * height
header = bytearray(148)
header[:4] = b'DDS '
struct.pack_into('<7I', header, 4, 124, 0x81007, height, width, len(blocks), 0, 1)
struct.pack_into('<2I4s', header, 76, 32, 4, b'DX10')
struct.pack_into('<I', header, 108, 0x1000)
struct.pack_into('<5I', header, 128, 98, 3, 0, 1, 3)
for name in ('pic0', 'pic1'):
    (root.parent / 'sce_sys' / (name + '.dds')).write_bytes(header + blocks)
    (root.parent / 'sce_sys' / (name + '.png')).unlink(missing_ok=True)
