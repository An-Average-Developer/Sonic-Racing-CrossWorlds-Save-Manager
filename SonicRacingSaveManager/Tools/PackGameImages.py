r"""Packs the Save Editor's pictures and horn sounds, made by ExtractGameImages.py, into Assets\GameImages
(embedded in the exe like every other asset):

    python ExtractGameImages.py ... --cri-key <16 hex> --no-all --out <folder>
    python PackGameImages.py <folder>

The pictures are made smaller: 256-colour PNGs (with their transparency), and the course photos JPEG
(a palette bands their skies). The horn sounds are copied as they are.

Needs Pillow (pip install pillow)."""
import os, shutil, sys
from PIL import Image

KINDS = ['album', 'aura', 'color', 'currency', 'driver', 'front', 'gadget', 'horn', 'machine', 'playericon', 'rear', 'rsr', 'speed', 'stage', 'stageicon',
         'sticker', 'tire', 'title']

src = sys.argv[1]
dst = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Assets', 'GameImages')
shutil.rmtree(dst, ignore_errors=True)
total = 0
for kind in KINDS:
    folder = os.path.join(src, kind)
    if not os.path.isdir(folder): continue
    os.makedirs(os.path.join(dst, kind), exist_ok=True)
    # (with its subfolders: gadget\plate holds the gadgets' wide plate pictures)
    names = sorted(os.path.relpath(os.path.join(d, f), folder) for d, _, fs in os.walk(folder) for f in fs)
    for name in names:
        path = os.path.join(folder, name)
        os.makedirs(os.path.dirname(os.path.join(dst, kind, name)), exist_ok=True)
        stem, ext = os.path.splitext(name)
        if ext == '.wav':
            out = os.path.join(dst, kind, name)
            shutil.copyfile(path, out)
        elif ext == '.png':
            image = Image.open(path).convert('RGBA')
            if kind == 'stage':
                out = os.path.join(dst, kind, stem + '.jpg')
                image.convert('RGB').save(out, 'JPEG', quality=88, optimize=True)
            else:
                out = os.path.join(dst, kind, name)
                image.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.FLOYDSTEINBERG).save(out, 'PNG', optimize=True)
                # Kept as it was when the palette doesn't make it smaller
                if os.path.getsize(out) > os.path.getsize(path): shutil.copyfile(path, out)
        else:
            continue
        total += os.path.getsize(out)
print(f'Assets\\GameImages: {total / 1e6:.1f} MB')
