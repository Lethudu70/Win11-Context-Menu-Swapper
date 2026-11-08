"""Regenerate vector-based installer artwork using the existing application icon.
Requires librsvg (rsvg-convert) and Pillow only when regenerating the assets.
"""
from pathlib import Path
import argparse
import subprocess
from PIL import Image

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--renderer', default='C:/msys64/ucrt64/bin/rsvg-convert.exe')
args = parser.parse_args()
assets = root / 'assets'
assets.mkdir(exist_ok=True)
icon = (root.parent / 'resources/app.svg').read_text(encoding='utf-8')
icon_content = icon[icon.index('>') + 1:icon.rindex('</svg>')]

banner = f'''<svg xmlns="http://www.w3.org/2000/svg" width="492" height="942" viewBox="0 0 492 942">
<defs>
 <linearGradient id="bg" x2="1" y2="1"><stop stop-color="#07130e"/><stop offset="1" stop-color="#163c2b"/></linearGradient>
 <radialGradient id="glow"><stop stop-color="#66eea2" stop-opacity=".13"/><stop offset="1" stop-color="#66eea2" stop-opacity="0"/></radialGradient>
 <pattern id="grid" width="36" height="36" patternUnits="userSpaceOnUse"><path d="M36 0H0V36" fill="none" stroke="#70e6a6" stroke-opacity=".045"/></pattern>
</defs>
<rect width="492" height="942" fill="url(#bg)"/><rect width="492" height="942" fill="url(#grid)"/>
<ellipse cx="246" cy="365" rx="330" ry="345" fill="url(#glow)"/>
<path d="M46 64H100" stroke="#66eea2" stroke-width="6" stroke-linecap="round"/>
<text x="46" y="104" fill="#a5c8b4" font-family="Segoe UI, sans-serif" font-size="23" letter-spacing="5">LETH TOOLS</text>
<g transform="translate(104 198) scale(1.11)">{icon_content}</g>
<text x="44" y="585" fill="#f1fff6" font-family="Segoe UI, sans-serif" font-size="39" font-weight="700">CONTEXT MENU</text>
<text x="44" y="639" fill="#f1fff6" font-family="Segoe UI, sans-serif" font-size="52" font-weight="700">SWAPPER</text>
<rect x="46" y="684" width="172" height="55" rx="15" fill="#10281c" stroke="#4b9568"/>
<text x="132" y="722" text-anchor="middle" fill="#83f3b3" font-family="Segoe UI, sans-serif" font-size="29" font-weight="600">v4.0</text>
<path d="M46 811H446" stroke="#4d8b65" stroke-opacity=".5"/>
<text x="46" y="855" fill="#b5d3c1" font-family="Segoe UI, sans-serif" font-size="21" letter-spacing="2">WINDOWS · x64</text>
<text x="46" y="894" fill="#6d9e83" font-family="Segoe UI, sans-serif" font-size="18" letter-spacing="3">LETH EDITION</text>
</svg>'''

background = '''<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1100" viewBox="0 0 1600 1100">
<defs><linearGradient id="bg" x2="1" y2="1"><stop stop-color="#0b1110"/><stop offset="1" stop-color="#10251a"/></linearGradient>
<radialGradient id="g"><stop stop-color="#3d9862" stop-opacity=".12"/><stop offset="1" stop-color="#3d9862" stop-opacity="0"/></radialGradient></defs>
<rect width="1600" height="1100" fill="url(#bg)"/><ellipse cx="1560" cy="1000" rx="720" ry="620" fill="url(#g)"/>
<g fill="none" stroke="#80e7a8" stroke-opacity=".035" stroke-width="2"><rect x="1080" y="580" width="420" height="360" rx="48"/>
<path d="M1140 660h190M1140 730h300M1140 800h150" stroke-width="14" stroke-linecap="round"/></g></svg>'''

for name, svg in [('wizard-banner', banner), ('wizard-background', background)]:
    (assets / f'{name}.svg').write_text(svg, encoding='utf-8')
    subprocess.run([args.renderer, '-o', str(assets / f'{name}.png'), str(assets / f'{name}.svg')], check=True)
with Image.open(root.parent / 'resources/app.png') as im:
    im.resize((180, 180), Image.Resampling.LANCZOS).save(assets / 'wizard-small.png')
print(assets)
