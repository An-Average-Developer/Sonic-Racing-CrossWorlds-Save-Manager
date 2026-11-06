r"""Copies the game's own UI icons (characters, machines, parts, stickers, gadgets, album jackets, course
photos and emblems, the speed classes, the Red Star Ring, player icons) from your Sonic Racing
CrossWorlds install into the Save Editor's picture cache:
    %LOCALAPPDATA%\SonicRacingSaveManager\GameImages\<kind>\<id>.png
The honor titles are drawn as the game shows them: their plate (from the game's title table) with
their name on it, in the game's font, once per language (title\<id>_<lang>.png; title\<id>.png is
the bare plate). The horns get their sound too (horn\<HORNTYPE_..>.wav): the base game's from its
CRI ADX audio; the festivals' are CRI HCA, decoded by vgmstream's clHCA (Tools\hca, built with the
Visual C++ tools on first use), and only when --cri-key gives the game's CRI keycode. Every other UI
texture of the game goes to GameImages\all (--no-all skips it).
The machines' colour presets get a swatch (color\<ColorPresetNN>.png, their body's three colours from the
game's palette) and the auras the game's aura icon (aura\<MACHINE_AURA_..>.png, tinted for the plain
coloured ones). Tools\PackGameImages.py then packs what the Save Editor uses into Assets\GameImages, embedded in the
app (give it --out <folder> here and that folder there). Run both again after a game update.

    python ExtractGameImages.py --paks "<game>\UNION\Content\Paks" --key <pak AES-256 key, 64 hex>
                                --oodle <path to an oo2core_*_win64.dll> [--cri-key <16 hex>]

Needs Pillow and pycryptodome (pip install pillow pycryptodome). The paks are AES encrypted and
Oodle compressed: the key is the game's (not included here), the Oodle DLL comes with many games."""
import argparse, ctypes, glob, io, os, re, struct, sys
from Crypto.Cipher import AES
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument('--paks', required=True)
ap.add_argument('--key', required=True)
ap.add_argument('--oodle', required=True)
ap.add_argument('--cri-key', help="the game's CRI keycode (hex), for the festival horns' sounds")
ap.add_argument('--catalog', help="writes the colour presets (Source\\Data\\ColorCatalog.cpp) and the jukebox (MusicCatalog.cpp) the game has into this folder")
ap.add_argument('--no-all', action='store_true', help="don't copy every UI texture to GameImages\\all")
ap.add_argument('--out', default=os.path.join(os.environ.get('LOCALAPPDATA', '.'), 'SonicRacingSaveManager', 'GameImages'))
args = ap.parse_args()
PAKS, OUT = args.paks, args.out
KEY = bytes.fromhex(args.key)

def dec(b):
    pad = (-len(b)) % 16
    return AES.new(KEY, AES.MODE_ECB).decrypt(b + b'\0' * pad)[:len(b)]

_dll = ctypes.WinDLL(os.path.abspath(args.oodle))
_fn = _dll.OodleLZ_Decompress; _fn.restype = ctypes.c_int64
_fn.argtypes = [ctypes.c_void_p, ctypes.c_int64, ctypes.c_void_p, ctypes.c_int64] + [ctypes.c_int] * 3 + [ctypes.c_void_p, ctypes.c_int64, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int64, ctypes.c_int]
def oodle(blk, n):
    buf = ctypes.create_string_buffer(n)
    got = _fn(blk, len(blk), buf, n, 1, 0, 0, None, 0, None, None, None, 0, 3)
    if got != n: raise RuntimeError(f'oodle {got} != {n}')
    return buf.raw

class R:
    def __init__(s, d): s.d, s.p = d, 0
    def i32(s): v = struct.unpack_from('<i', s.d, s.p)[0]; s.p += 4; return v
    def u32(s): v = struct.unpack_from('<I', s.d, s.p)[0]; s.p += 4; return v
    def i64(s): v = struct.unpack_from('<q', s.d, s.p)[0]; s.p += 8; return v
    def raw(s, n): v = s.d[s.p:s.p + n]; s.p += n; return v
    def fstr(s):
        n = s.i32()
        if n == 0: return ''
        if n > 0: v = s.d[s.p:s.p + n - 1].decode('latin1'); s.p += n; return v
        v = s.d[s.p:s.p - 2 * n - 2].decode('utf-16le'); s.p += -2 * n; return v

def be40(b): return int.from_bytes(b, 'big')

class Toc:
    def __init__(self, utoc):
        d = open(utoc, 'rb').read()
        (self.version,) = struct.unpack_from('<B', d, 16)
        hsize, n, nblocks, blocksz, nmeth, methlen, self.blocksize, dirsize, nparts = struct.unpack_from('<9I', d, 20)
        self.flags = d[80]
        seeds, = struct.unpack_from('<I', d, 84)
        partsize, = struct.unpack_from('<Q', d, 88)
        noperfect, = struct.unpack_from('<I', d, 96)
        p = hsize
        self.ids = [d[p + 12 * i:p + 12 * i + 12] for i in range(n)]; p += 12 * n
        self.offlen = []
        for i in range(n):
            e = d[p + 10 * i:p + 10 * i + 10]
            self.offlen.append((be40(e[:5]), be40(e[5:])))
        p += 10 * n
        p += 4 * seeds + 4 * noperfect
        self.blocks = []
        for i in range(nblocks):
            e = d[p + 12 * i:p + 12 * i + 12]
            off = int.from_bytes(e[:5], 'little'); csz = int.from_bytes(e[5:8], 'little'); usz = int.from_bytes(e[8:11], 'little'); m = e[11]
            self.blocks.append((off, csz, usz, m))
        p += 12 * nblocks
        self.methods = [d[p + methlen * i:p + methlen * (i + 1)].rstrip(b'\0').decode() for i in range(nmeth)]; p += nmeth * methlen
        if self.flags & 4:   # signed
            hs, = struct.unpack_from('<i', d, p); p += 4 + 2 * hs + 20 * nblocks
        self.files = {}
        if dirsize:
            r = R(dec(d[p:p + dirsize]) if self.flags & 2 else d[p:p + dirsize])
            mount = r.fstr()
            nd = r.i32(); dirs = [struct.unpack_from('<4I', r.d, r.p + 16 * i) for i in range(nd)]; r.p += 16 * nd
            nf = r.i32(); files = [struct.unpack_from('<3I', r.d, r.p + 12 * i) for i in range(nf)]; r.p += 12 * nf
            ns = r.i32(); strings = [r.fstr() for _ in range(ns)]
            NONE = 0xFFFFFFFF
            def walk(di, path):
                name, child, sib, ffile = dirs[di]
                here = path + (strings[name] + '/' if name != NONE else '')
                f = ffile
                while f != NONE:
                    fname, nxt, user = files[f]
                    self.files[(mount + here + strings[fname]).replace('../../../', '')] = user
                    f = nxt
                c = child
                while c != NONE:
                    walk(c, here); c = dirs[c][2]
            walk(0, '')
        base = utoc[:-5]
        self.ucas = [open(base + '.ucas', 'rb')] + [open(f'{base}_s{i}.ucas', 'rb') for i in range(1, nparts) if os.path.exists(f'{base}_s{i}.ucas')]
        self.partsize = partsize

    def chunk_type(self, i): return self.ids[i][11] & 0x3f

    def read(self, index):
        off, length = self.offlen[index]
        out = bytearray()
        first = off // self.blocksize; last = (off + length - 1) // self.blocksize
        for b in range(first, last + 1):
            boff, csz, usz, m = self.blocks[b]
            part = boff // self.partsize if self.partsize and self.partsize != 0xFFFFFFFFFFFFFFFF else 0
            f = self.ucas[part]; f.seek(boff - part * self.partsize if part else boff)
            raw = f.read((csz + 15) // 16 * 16 if self.flags & 2 else csz)
            if self.flags & 2: raw = dec(raw)
            raw = raw[:csz]
            out += raw if m == 0 else oodle(raw, usz)
        start = off - first * self.blocksize
        return bytes(out[start:start + length])

MAX = 192   # thumbnails: the rows show them small
SIZES = {'stage': 400, 'stageicon': 128}   # the course photos fill a wide tile

# Pixel formats: bytes per 4x4 block (block formats) or per pixel; DXGI format for the DDS wrapper
FORMATS = {'PF_BC7': (16, True, 98), 'PF_DXT5': (16, True, 77), 'PF_DXT1': (8, True, 71), 'PF_BC5': (16, True, 83), 'PF_BC4': (8, True, 80),
           'PF_B8G8R8A8': (4, False, 87), 'PF_G8': (1, False, 61)}

def dds(fmt, w, h, data):
    _, block, dxgi = FORMATS[fmt]
    hdr = struct.pack('<4sI', b'DDS ', 124) + struct.pack('<6I', 0x1007 | 0x80000, h, w, len(data), 0, 1) + b'\0' * 44
    hdr += struct.pack('<2I4s5I', 32, 4, b'DX10', 0, 0, 0, 0, 0) + struct.pack('<5I', 0x1000, 0, 0, 0, 0)
    hdr += struct.pack('<5I', dxgi, 3, 0, 1, 0)
    return Image.open(io.BytesIO(hdr + data))

def texture(pkg):
    """The largest mip of a cooked Texture2D zen package (inline mips), as an image"""
    hsize, = struct.unpack_from('<I', pkg, 4)
    for fmt in FORMATS:
        k = pkg.find(fmt.encode() + b'\0', hsize)
        if k < 0: continue
        w, h, _ = struct.unpack_from('<iiI', pkg, k - 16)
        k += len(fmt) + 1
        nmips, index = struct.unpack_from('<ii', pkg, k + 4)
        bpb, block, _ = FORMATS[fmt]
        size = (max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * bpb) if block else w * h * bpb
        start = k + 12
        data = pkg[start:start + size]
        tw, th, tz = struct.unpack_from('<iii', pkg, start + size)
        if (tw, th) != (w, h) or index != 0: return None   # not inline / unexpected layout
        if block: return dds(fmt, w, h, data)
        if fmt == 'PF_B8G8R8A8': return Image.frombytes('RGBA', (w, h), data, 'raw', 'BGRA')
        return Image.frombytes('L', (w, h), data)
    return None

files = {}
tocs = {}
for u in sorted(glob.glob(os.path.join(PAKS, '*.utoc'))):
    try: t = Toc(u)
    except Exception: continue
    tocs[u] = t
    for n, i in t.files.items(): files[n.split('/')[-1]] = (u, i)

files_lower = {}

def asset(name):
    # (asset names are FNames: a table can name one in another case, UI_GadgetIcon_run60 for ..._Run60)
    hit = files.get(name + '.uasset')
    if not hit:
        if not files_lower: files_lower.update({f.lower(): v for f, v in files.items()})
        hit = files_lower.get((name + '.uasset').lower())
    if not hit: return None
    u, i = hit
    try: return texture(tocs[u].read(i))
    except Exception as e: print('  fail', name, e); return None

# ids -> asset names
base = ['son', 'tai', 'knu', 'amy', 'egg', 'sha', 'rou', 'sil', 'bla', 'met', 'ome', 'vec', 'esp', 'cha', 'zav', 'zaz', 'cre', 'big', 'egp', 'sag', 'jet', 'wav', 'sto']
names = ['Sonic', 'Tails', 'Knuckles', 'Amy', 'DrEggman', 'Shadow', 'Rouge', 'Silver', 'Blaze', 'MetalSonic', 'Omega', 'Vector', 'Espio', 'Charmy', 'Zavok', 'Zazz',
         'Cream', 'Big', 'Eggpawn', 'Sage', 'Jet', 'Wave', 'Storm']
jobs = []
for code, n in zip(base, names):
    jobs.append(('driver', n, f'UI_CharaIcon_{code}_01'))
    jobs.append(('driver', n + 'Racer', f'UI_CharaIcon_{code}_02'))
for d, a in {'SuperSonic': 'sonSp_01', 'SageBlack': 'sag_03', 'Werehog': 'whg_01', 'TailsPrime': 'taiPr_01', 'AmyPrime': 'amyPr_01', 'KnucklesPrime': 'knuPr_01',
             'EggpawnBlue': 'egp_01', 'EggpawnRed': 'egp_01', 'EggpawnYellow': 'egp_01', 'EggpawnGreen': 'egp_01'}.items():
    jobs.append(('driver', d, 'UI_CharaIcon_' + a))

def zen_names(d):
    """A zen package's header size and name map"""
    hasver, hsize = struct.unpack_from('<II', d, 0)
    p = 52
    if hasver:
        p += 16
        n, = struct.unpack_from('<i', d, p); p += 4 + 20 * n
    count, _ = struct.unpack_from('<II', d, p); p += 16 + 8 * count
    lens = []
    for _ in range(count):
        b0, b1 = d[p], d[p + 1]; p += 2
        lens.append((((b0 & 0x7f) << 8) | b1, b0 & 0x80))
    names = []
    for n, wide in lens:
        if wide: names.append(d[p:p + 2 * n].decode('utf-16le')); p += 2 * n
        else: names.append(d[p:p + n].decode('latin1')); p += n
    return hsize, names

def racer_colors():
    """Season 2's AI racers in colour (<X>RacerColor01..03, the Donpa Ticket shop's): their icons
    (UI_CharaIcon_<code>AI_<colour>) in DT_DriverData01's row order, which is the enum's"""
    d = files.get('DT_DriverData01.uasset')
    if not d: return {}
    data = tocs[d[0]].read(d[1])
    h, N = zen_names(data); b = data[h:]
    order = {}
    for o in range(len(b) - 8):
        idx, num = struct.unpack_from('<ii', b, o)
        if num == 0 and 0 <= idx < len(N) and (m := re.match(r'UI_CharaIcon_([a-z]{3})AI_(\w+)$', N[idx])):
            order.setdefault(m.group(1), [])
            if N[idx] not in order[m.group(1)]: order[m.group(1)].append(N[idx])
    return {f'{n}RacerColor{k + 1:02d}': icon for code, n in zip(base, names) for k, icon in enumerate(order.get(code, []))}

for d, icon in racer_colors().items(): jobs.append(('driver', d, icon))

# Textures the game ships as stand-ins, not art: the "GadgetTmb Item" stripes some thumbnails are, and the
# red "Item 23".."Item 26" icons of the four Summon ... Item gadgets (which have no art at all: they show
# the unknown gadget's picture)
GADGET_PLACEHOLDERS = {'UI_GadgetTmb_Item23', 'UI_GadgetTmb_Item24', 'UI_GadgetTmb_Item25', 'UI_GadgetTmb_Item26', 'UI_GadgetTmb_Ring12',
                       'UI_GadgetIcon_Item23', 'UI_GadgetIcon_Item24', 'UI_GadgetIcon_Item25', 'UI_GadgetIcon_Item26'}

def gadget_rows():
    """gadget key ("run45") -> the textures its own row of DT_GadgetData names (placeholders left out). A row
    is its guide text (gadget_guide_<key>) and then its icon and thumbnail, often another gadget's: Speed
    Mod (run45) shows Speed Tune's UI_GadgetTmb_Run26, Mini Quick Recovery (run61) UI_GadgetIcon_Run16"""
    d = files.get('DT_GadgetData.uasset')
    if not d: return {}
    data = tocs[d[0]].read(d[1])
    h, N = zen_names(data); b = data[h:]
    marks = []
    for o in range(len(b) - 8):
        idx, num = struct.unpack_from('<ii', b, o)
        if 0 <= idx < len(N) and 0 <= num < 100 and re.match(r'UI_Gadget(Tmb|Icon)_\w+$', N[idx]):
            marks.append((o, 'tex', N[idx] + (f'_{num - 1}' if num else '')))
    for m in re.finditer(rb'gadget_guide_([a-z]+)(\d+)\x00', b): marks.append((m.start(), 'key', f'{m.group(1).decode()}{int(m.group(2))}'))
    rows, key = {}, None
    for o, kind, s in sorted(marks):
        if kind == 'key':
            key = None if s in rows else s   # a key seen again (another row's text) starts nothing
            if key: rows[key] = []
        elif key: rows[key].append(s)
    return {k: [t for t in texs if t not in GADGET_PLACEHOLDERS] for k, texs in rows.items()}

def row_icon(k, texs):
    """a row's icon: the one named like the row (a stray number can read as another gadget's icon), else the
    one of the thumbnail it names (UI_GadgetTmb_Run16 -> UI_GadgetIcon_Run16), else any it names"""
    icons = [t for t in texs if t.startswith('UI_GadgetIcon_')]
    own = [t for t in icons if t.lower().endswith('_' + k)]
    if own: return own[0]
    for t in texs:
        if t.startswith('UI_GadgetTmb_'):
            m = re.fullmatch(r'UI_GadgetTmb_([A-Za-z]+)0*(\d+)', t)
            if m:
                for cand in (f'UI_GadgetIcon_{m.group(1).capitalize()}{m.group(2)}', f'UI_GadgetIcon_{m.group(1)}{m.group(2)}'):
                    if cand + '.uasset' in files and cand not in GADGET_PLACEHOLDERS: return cand
    return icons[0] if icons else None

GADGET_ROWS = gadget_rows()

def gadget_pictures():
    """gadget key -> its thumbnail as its row names it, else its row's icon"""
    out = {}
    for k, texs in GADGET_ROWS.items():
        tmb = [t for t in texs if t.startswith('UI_GadgetTmb_')]
        pic = tmb[0] if tmb else row_icon(k, texs)
        if pic: out[k] = pic
    return out

# Every gadget its table's picture; the thumbnails named by number (below) cover what the table doesn't
gadget_jobs = gadget_pictures()
for k, tex in gadget_jobs.items(): jobs.append(('gadget', k, tex))
# What the game shows for a gadget it has no art for (the hidden ones, the unused ones): gadget\unknown.png
jobs.append(('gadget', 'unknown', 'UI_ItemIcon_UnknownGadget'))
# The currencies' icons, for the Save Editor's Currencies section
jobs.append(('currency', 'tickets', 'UI_ItemIcon_DonpaTicket_128'))
jobs.append(('currency', 'medals', 'UI_ItemIcon_DuelMadel'))   # (the game's own spelling)

# ── The jukebox: DT_AlbumData_NN (albums) and DT_TrackData_NN (tracks), in the base game's pak and the
# festivals' (a later table's row of an album replaces the earlier one) ─────────────────────────────

def jukebox_tables(prefix):
    """(the table's name map, its rows: (the row's bytes, from its id on)), table by table"""
    for t in range(1, 20):
        hit = files.get(f'{prefix}_{t:02d}.uasset')
        if not hit: continue
        d = tocs[hit[0]].read(hit[1])
        hs, names = zen_names(d)
        yield names, d[hs:]

def album_rows():
    """album id -> (its order in the jukebox, its jacket: the last texture its row names)"""
    out = {}
    for names, b in jukebox_tables('DT_AlbumData'):
        starts = [m.end() + 1 for m in re.finditer(rb'\x80\x17[\x80-\x8f]', b)]
        for k, p in enumerate(starts):
            if p + 8 > len(b): continue
            aid, order = struct.unpack_from('<ii', b, p)
            if not 1000 <= aid < 10000: continue
            end = starts[k + 1] if k + 1 < len(starts) else len(b)
            jacket = None
            # (an FName: its index, then its number 0)
            for q in range(p + 8, end - 8):
                v, n = struct.unpack_from('<iI', b, q)
                if n == 0 and 0 <= v < len(names) and names[v].startswith('UI_') and 'acket' in names[v].lower(): jacket = names[v]
            out[aid] = (order, jacket)
    return out

def track_rows():
    """track id -> (its album, its name's text key, its artist's)"""
    out = {}
    for names, b in jukebox_tables('DT_TrackData'):
        for m in re.finditer(rb'\x80\x21\x00.', b, re.S):
            if m.end() + 8 > len(b): continue
            tid, aid = struct.unpack_from('<ii', b, m.end())
            if not 10000 <= tid < 100000: continue
            tail = b[m.end():m.end() + 260]
            name = re.search(rb'bgm_name_\d+', tail)
            artist = re.search(rb'artist_name_\d+', tail)
            out[tid] = (aid, name.group().decode() if name else '', artist.group().decode() if artist else '')
    return out

# The albums' jackets, by album id (album\<id>.png); the ones without one show the base game's
albums = album_rows()
for aid, (order, jacket) in albums.items(): jobs.append(('album', str(aid), jacket or 'UI_Albumjacket_01001'))

# A festival horn's assets go by its horn type's number (Spectrum Cube is HORNTYPE_EXTND_39)
def horn_extnd(n): return f'HORNTYPE_EXTND_{int(n):02d}'

for f in files:
    if m := re.match(r'UI_CharaIcon_Extnd(\d\d)_Character(\d{3})\.uasset$', f):
        jobs.append(('driver', f'Extnd{m.group(1)}_Character{m.group(1)}{m.group(2)}', f[:-7]))
    elif m := re.match(r'UI_MachineIcon_([A-Z]{3}_\d\d)\.uasset$', f): jobs.append(('machine', m.group(1), f[:-7]))
    elif m := re.match(r'UI_PartsIcon_([A-Z]{3}_\d\d)_([FRW])\.uasset$', f):
        jobs.append(({'F': 'front', 'R': 'rear', 'W': 'tire'}[m.group(2)], m.group(1), f[:-7]))
    elif (m := re.match(r'UI_Gadget(Tmb|Icon)_([A-Za-z]+)(\d+)\.uasset$', f)) and f'{m.group(2).lower()}{int(m.group(3))}' not in gadget_jobs \
            and f[:-7] not in GADGET_PLACEHOLDERS:
        # Not in DT_GadgetData (older gadgets): its thumbnail, else its icon
        k = f'{m.group(2).lower()}{int(m.group(3))}'
        if m.group(1) == 'Tmb' or f'UI_GadgetTmb_{m.group(2)}{m.group(3)}.uasset' not in files: jobs.append(('gadget', k, f[:-7]))
    elif m := re.match(r'UI_PlayerIcon_(\d+)\.uasset$', f): jobs.append(('playericon', str(int(m.group(1))), f[:-7]))
    # The festival horns: their festival's picture (the base game's horns have none)
    elif m := re.match(r'UI_HornTmb_Extnd(\d+)_01\.uasset$', f): jobs.append(('horn', horn_extnd(m.group(1)), f[:-7]))
    # The Duel Medal exchange's horns (2.0): rows 0201..0210 of DT_HornData_06 are HORNTYPE_40..49
    elif m := re.match(r'UI_HornTmb_Exchange_02(\d\d)\.uasset$', f): jobs.append(('horn', f'HORNTYPE_{39 + int(m.group(1))}', f[:-7]))
    # Courses: the photo (UI_StageRing) and the emblem (UI_StageIcon), by EStageId; the collab courses'
    # carry their pack ("UI_StageRing1503_Extnd04")
    elif m := re.match(r'UI_StageRing(\d{4})(_Extnd\d+)?\.uasset$', f): jobs.append(('stage', 'STG' + m.group(1), f[:-7]))
    elif m := re.match(r'UI_StageIcon(\d{4})(_Extnd\d+)?\.uasset$', f): jobs.append(('stageicon', 'STG' + m.group(1), f[:-7]))
    # Speed classes, by EMenuSpeedClassId: Normal, High, Sonic, Super Sonic, Mirror
    elif m := re.match(r'UI_ClassSelect_BtnIcon_Active_0([1-5])\.uasset$', f): jobs.append(('speed', str(int(m.group(1)) - 1), f[:-7]))
done = 0
for kind, ident, name in jobs:
    img = asset(name)
    if img is None: continue
    img = img.convert('RGBA')
    if kind == 'speed': img = img.crop(img.getbbox())   # the icons sit in a wide empty frame
    size = SIZES.get(kind, MAX)
    if max(img.size) > size: img.thumbnail((size, size), Image.LANCZOS)
    os.makedirs(os.path.join(OUT, kind), exist_ok=True)
    img.save(os.path.join(OUT, kind, ident + '.png'), optimize=True)
    done += 1
print('extracted', done, 'of', len(jobs))

# ── Gadgets as the game's gadget screen shows them: their icon on a hexagon ──────────────────────
# UI_GadgetIcon_<kind><n> over UI_GadgetIcon_Base_01 (blue), _Kit_001 (gold: the kits, Various..) or
# _Mini (silver: the "..Mini" ones); the ones without an icon of their own (or only a stand-in) the
# bare hexagon UI_GadgetIcon_00. These replace the thumbnails above, gadget\<kind><n>.png.

def gadget_icon_name(k):
    """the icon texture of gadget key "run45": its row's (shared ones too), else UI_GadgetIcon_Run45 by name
    (the kits' are "various30" in lower case)"""
    if k in GADGET_ROWS and (icon := row_icon(k, GADGET_ROWS[k])): return icon
    kind, n = re.fullmatch(r'([a-z]+)(\d+)', k).groups()
    for cand in (f'UI_GadgetIcon_{kind.capitalize()}{n}', f'UI_GadgetIcon_{kind}{n}', f'UI_GadgetIcon_{kind.capitalize()}{int(n):02d}'):
        if cand + '.uasset' in files and cand not in GADGET_PLACEHOLDERS: return cand
    return None

# Gadgets the game has no icon for at all (hidden, test or unused ones, and the four Summon .. Item, whose
# icons are stand-ins): the icon of the gadget that does the same, on their own frame
GADGET_STAND_INS = {
    'item23': 'item8', 'item24': 'item7', 'item25': 'item6', 'item26': 'item9',   # Summon Speed / Defense / Attack / Hazard Item
    'item33': 'item9',                    # Trap Get Item: Hazard Item Chance UP
    'hidden1': 'item30', 'hidden2': 'item30',   # 2Lap / 3Lap Get Item: Item Mercy
    'hidden3': 'ring19',                  # Drift Dash Bonus: Perfect Charge Bounty
    'hidden4': 'run43',                   # Immune Weight And Boom Boo Once: Invincible Start
    'hidden5': 'run42', 'run5': 'run42',  # Plane Form Param UP, Add Action Boat Plane: Air Drift Mobility UP
    'hidden6': 'run44',                   # Boat Form Param UP: Charge Jump Mobility UP
    'hidden7': 'run16', 'shortendamagetime': 'run16',   # Shorten Damage Motion Mini / Time: Quick Recovery
    'hidden9': 'run19',                   # Suppress Runoff: Bumper Guard
    'hidden10': 'run9',                   # Fourth Fast Charge: Lv3 Quick Charge
    'hidden11': 'various38', 'various39': 'various38',  # Spin Dash, Spin Start Dash Kit: Spin Dash Kit
    'run12': 'run11',                     # Ultimate Charge Attack: Lv4 Quick Charge
    'run18': 'run6',                      # Low Charge: Lv1 Quick Charge
    'run58': 'run57',                     # Air Trick Charge: Ultimate Air Trick
    'various1': 'various2',               # Team Friend Boost: Collision Boost
    'lowrankringbonus': 'ring17',         # Low Rank Ring Bonus: 130 Ring Limit
    'shortenitemslottime': 'item2',       # Shorten Item Slot Time: Inventory Boost
    'item4': 'item3',                     # Add Splash: Warp Ring Specialist
}

def gadget_key(gid):
    """the picture name of an EGadgetId value: "Run45_SpeedConverter" -> run45, "ShortenDamageTime" -> shortendamagetime"""
    m = re.fullmatch(r'([A-Za-z]+?)(\d+)_\w+', gid)
    return m.group(1).lower() + str(int(m.group(2))) if m else gid.lower()

def gadget_slot_counts():
    """gadget key -> the plate slots it takes, from DT_GadgetData: an int32 40 bytes after the row's last
    guide text (gadget_guide_<k>_sp since 2.0; a row without one has it 49 after the plain one)"""
    d = files.get('DT_GadgetData.uasset')
    if not d: return {}
    data = tocs[d[0]].read(d[1])
    h, N = zen_names(data); b = data[h:]
    out = {}
    for m in re.finditer(rb'gadget_guide_([a-z]+)(\d+)\x00', b):
        k = m.group(1).decode() + str(int(m.group(2)))
        if k in out: continue
        sp = re.compile(rb'gadget_guide_' + m.group(1) + m.group(2) + rb'_sp\x00').search(b, m.end(), m.end() + 64)
        n, = struct.unpack_from('<i', b, sp.end() + 40 if sp else m.end() + 49)
        if 1 <= n <= 4: out[k] = n
    return out

def gadget_hexes(ids):
    """gadget key -> its picture (RGBA) as the gadget screen shows it, for the EGadgetId value names `ids`: its icon
    on the frame of its size, a hexagon for 1 slot (gold for a kit, silver for a "..Mini"), the wide 2-slot one, the
    wide 3-slot one (gold for a kit); the icon in the middle"""
    bases = {name: asset('UI_GadgetIcon_' + name) for name in ('Base_01', 'Base_01_Kit_001', 'Base_01_Mini', 'Base_02', 'Base_03', 'Base_03_Kit_001', '00')}
    bases = {k: v.convert('RGBA') for k, v in bases.items() if v is not None}
    slots = gadget_slot_counts()
    out = {}
    for gid in ids:
        k = gadget_key(gid)
        if k in out: continue
        own = gadget_icon_name(k) if re.fullmatch(r'[a-z]+\d+', k) else None
        icon_name = own or (gadget_icon_name(GADGET_STAND_INS[k]) if k in GADGET_STAND_INS else None)
        icon = asset(icon_name) if icon_name else None
        if icon is None:
            if '00' in bases: out[k] = bases['00'].copy()
            continue
        kit = k.startswith('various')
        size = slots.get(k, slots.get(GADGET_STAND_INS.get(k, ''), 1))
        icon = icon.convert('RGBA')
        def on(frame):
            img = bases.get(frame, bases.get('Base_01')).copy()
            img.alpha_composite(icon.resize((img.height, img.height), Image.LANCZOS), ((img.width - img.height) // 2, 0))
            return img
        # The lists' hexagon; on the plate, the frame as wide as its slots (key "plate/<k>")
        out[k] = on('Base_01_Kit_001' if kit else 'Base_01_Mini' if 'Mini' in gid else 'Base_01')
        if size >= 2: out['plate/' + k] = on(('Base_03_Kit_001' if kit else 'Base_03') if size >= 3 else 'Base_02')
    return out

def gadget_ids():
    """every EGadgetId value the game's tables name"""
    ids = set()
    d = files.get('DT_GadgetData.uasset')
    if d:
        h, N = zen_names(tocs[d[0]].read(d[1]))
        ids |= {n.split('::')[1] for n in N if n.lower().startswith('egadgetid::')}
    return ids

# (the gadgets the table only has a row for, without their id's name: by their key, "run45_")
hexes = gadget_hexes(sorted(gadget_ids() | {f'{k}_' + 'Row' for k in gadget_jobs}))
os.makedirs(os.path.join(OUT, 'gadget'), exist_ok=True)
for k, img in hexes.items():
    img.thumbnail((MAX * 2, MAX) if img.width > img.height else (MAX, MAX), Image.LANCZOS)   # (the wide ones as tall as the rest)
    path = os.path.join(OUT, 'gadget', *k.split('/')) + '.png'
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)
bare = asset('UI_GadgetIcon_00')
if bare is not None:
    bare = bare.convert('RGBA'); bare.thumbnail((MAX, MAX), Image.LANCZOS)
    bare.save(os.path.join(OUT, 'gadget', 'unknown.png'), optimize=True)
print('gadget hexagons', len(hexes))

# The Red Star Ring, lit and unlit: the first ring of the hint picture's row of five, cut out round
for state in ('on', 'off'):
    img = asset(f'UI_Tips_10016_RSR_{state.upper()}')
    if img is None: continue
    img = img.convert('RGBA')
    w, h = img.size
    cx, cy, r = round(w * 163 / 960), round(h * 150 / 300), round(h * 74 / 300)
    ring = img.crop((cx - r, cy - r, cx + r, cy + r))
    mask = Image.new('L', (r * 8, r * 8), 0)
    from PIL import ImageDraw
    ImageDraw.Draw(mask).ellipse((0, 0, r * 8 - 1, r * 8 - 1), fill=255)
    ring.putalpha(mask.resize(ring.size, Image.LANCZOS))
    os.makedirs(os.path.join(OUT, 'rsr'), exist_ok=True)
    ring.save(os.path.join(OUT, 'rsr', state + '.png'), optimize=True)
    print('rsr', state)

# ── The classic .pak (texts, fonts) ─────────────────────────────────────────────────────────────

class Pak:
    """UE pak v11: encrypted index, full directory index, encoded entries"""
    def __init__(self, path):
        self.f = open(path, 'rb')
        size = os.path.getsize(path)
        self.f.seek(size - 0x400); tail = self.f.read()
        i = tail.rfind(struct.pack('<I', 0x5A6F12E1))
        ioff, isz = struct.unpack_from('<QQ', tail, i + 8)
        self.f.seek(ioff)
        r = R(dec(self.f.read(isz)))
        mount = r.fstr(); r.i32(); r.i64()
        if r.i32(): r.i64(); r.i64(); r.raw(20)
        has_full = r.i32()
        fdi_off = fdi_size = 0
        if has_full: fdi_off = r.i64(); fdi_size = r.i64(); r.raw(20)
        self.encoded = r.raw(r.i32())
        self.files = {}
        if has_full:
            self.f.seek(fdi_off)
            d = R(dec(self.f.read(fdi_size)))
            for _ in range(d.i32()):
                dname = d.fstr()
                for _ in range(d.i32()):
                    fname = d.fstr(); loc = d.i32()
                    self.files[(mount + dname + fname).replace('../../../', '')] = loc

    def read(self, name):
        e = self.encoded; p = self.files[name]
        bits, = struct.unpack_from('<I', e, p); p += 4
        bsize = (bits & 0x3f) << 11
        if (bits & 0x3f) == 0x3f: bsize, = struct.unpack_from('<I', e, p); p += 4
        nblocks = (bits >> 6) & 0xffff; encrypted = bool(bits & (1 << 22)); method = (bits >> 23) & 0x3f
        def num(fits):
            nonlocal p
            if fits: v, = struct.unpack_from('<I', e, p); p += 4
            else: v, = struct.unpack_from('<Q', e, p); p += 8
            return v
        offset = num(bits & (1 << 31)); usize = num(bits & (1 << 30)); size = num(bits & (1 << 29)) if method else usize
        blocks = []
        if method and nblocks:
            if nblocks == 1 and not encrypted: blocks = [size]
            else:
                for _ in range(nblocks): v, = struct.unpack_from('<I', e, p); p += 4; blocks.append(v)
        pos = offset + 8 + 8 + 8 + 4 + 20 + (4 + 16 * len(blocks) if method else 0) + 1 + 4
        if not method:
            self.f.seek(pos); data = self.f.read((size + 15) // 16 * 16 if encrypted else size)
            return (dec(data) if encrypted else data)[:usize]
        out = b''; left = usize
        for bs in blocks:
            self.f.seek(pos); n = (bs + 15) // 16 * 16 if encrypted else bs
            blk = self.f.read(n); pos += n
            if encrypted: blk = dec(blk)
            want = min(bsize or usize, left)
            out += oodle(blk[:bs], want); left -= want
        return out

def locres(d):
    """Game.locres: (namespace, key) -> text"""
    p = 16; ver = d[p]; p += 1
    off, = struct.unpack_from('<q', d, p); p += 8
    def fstr():
        nonlocal p
        n, = struct.unpack_from('<i', d, p); p += 4
        if n >= 0: v = d[p:p + max(n - 1, 0)].decode('utf-8', 'replace'); p += n
        else: v = d[p:p - 2 * n - 2].decode('utf-16le'); p += -2 * n
        return v
    keep = p; p = off
    count, = struct.unpack_from('<i', d, p); p += 4
    strings = []
    for _ in range(count):
        strings.append(fstr())
        if ver >= 2: p += 4
    p = keep
    if ver >= 2: p += 4
    out = {}
    nns, = struct.unpack_from('<I', d, p); p += 4
    for _ in range(nns):
        if ver >= 2: p += 4
        ns = fstr()
        nk, = struct.unpack_from('<I', d, p); p += 4
        for _ in range(nk):
            if ver >= 2: p += 4
            k = fstr(); p += 4
            idx, = struct.unpack_from('<i', d, p); p += 4
            out[(ns, k)] = strings[idx]
    return out

# ── Honor titles: the plate with the name on it ─────────────────────────────────────────────────

def honor_titles():
    """title id -> (text key, plate texture, text fill texture) from DT_HonorTitleListDataTable_*"""
    out = {}
    for f, (u, i) in files.items():
        if not re.match(r'DT_HonorTitleListDataTable_\d+\.uasset$', f): continue
        d = tocs[u].read(i)
        h, N = zen_names(d)
        b = d[h:]
        keys = [(m.start(), m.group().decode()) for m in re.finditer(rb'honortitle_title_\d+', b)]
        for k, (at, key) in enumerate(keys):
            # The row's id: an FName 37 bytes before the text's key (a struct header and the FText between)
            # (39 since the game's 2.0 update: its rows have more fields)
            for back in (39, 37):
                if at < back: continue
                idx, = struct.unpack_from('<i', b, at - back)
                if 0 <= idx < len(N) and N[idx].isdigit(): break
            else: continue
            plate = fill = None
            end = keys[k + 1][0] - 40 if k + 1 < len(keys) else len(b) - 8
            for o in range(at + len(key) + 5, max(at + len(key) + 5, end - 4)):
                v, number = struct.unpack_from('<ii', b, o)
                if not (0 <= v < len(N) and 0 <= number < 100000): continue
                # Soft paths: the asset's FName (its number is the "_123" suffix + 1)
                name = N[v] + (f'_{number - 1}' if number else '')
                if not plate and re.match(r'UI_(CMN_HonorTitle_Plate|HonorPlate)', N[v]): plate = name
                elif not fill and re.match(r'UI_HonorTitle_(Gold|Silver|Bronze|Rainbow)', N[v]): fill = name
            out[N[idx]] = (key, plate, fill)
    # The World Championship titles have no plate in the table: WBP_HonorTitle_Plate picks it by the
    # result, through the M_UI_TitlePlateR_ChampionShipHonor_* materials (1st, 2nd, 3rd, 12th, 36th_2,
    # 36th_1) and, for the one who watched, plate 048 itself
    championship = { '2000': 42, '2001': 43, '2002': 44, '2003': 45, '2004': 46, '2005': 47, '2006': 48 }
    for tid, number in championship.items():
        if tid in out: out[tid] = (out[tid][0], f'UI_CMN_HonorTitle_Plate_{number:03d}', out[tid][2])
    return out

def draw_title(plate, text, font_path, fill=None):
    from PIL import ImageDraw, ImageFont, ImageFilter
    img = plate.convert('RGBA')
    w, h = img.size
    # As big as fits 72% of the plate, centred, with a soft shadow and a dark outline
    size = int(h * 0.42)
    while True:
        font = ImageFont.truetype(font_path, size)
        box = font.getbbox(text)
        if box[2] - box[0] <= w * 0.72 or size <= 12: break
        size -= 2
    tw, th = box[2] - box[0], box[3] - box[1]
    x = (w - tw) // 2 - box[0]
    y = (h - th) // 2 - box[1] - int(h * 0.02)
    shadow = Image.new('RGBA', img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text((x + 3, y + 5), text, font=font, fill=(0, 0, 0, 150))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(3)))
    ImageDraw.Draw(img).text((x, y), text, font=font, fill=(255, 255, 255, 255), stroke_width=max(2, size // 12), stroke_fill=(20, 24, 40, 255))
    if fill is not None:
        # The game paints some names with a gradient texture (Rainbow, Gold...)
        mask = Image.new('L', img.size, 0)
        ImageDraw.Draw(mask).text((x, y), text, font=font, fill=255)
        layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
        layer.paste(fill.convert('RGBA').resize((max(1, tw), max(1, th))), (x + box[0], y + box[1]))
        img.paste(layer, (0, 0), mask)
    return img

pak_path = os.path.join(PAKS, 'pakchunk0-Windows.pak')
if os.path.exists(pak_path):
    pk = Pak(pak_path)
    font_path = os.path.join(OUT, 'fonts', 'DINNextCYR-Heavy.otf')
    os.makedirs(os.path.dirname(font_path), exist_ok=True)
    open(font_path, 'wb').write(pk.read('UNION/Content/01_Union/UI/Font/Europe_America/DINNextCYR-Heavy.ufont'))
    texts = {lang: locres(pk.read(f'UNION/Content/Localization/Game/{lang}/Game.locres')) for lang in ('en', 'es', 'fr', 'it')}
    titles = honor_titles()
    cache = {}
    def texture_of(name):
        if name not in cache: cache[name] = asset(name) if name else None
        return cache[name]
    # Start clean: older versions of this tool named the plates by their texture number
    import shutil
    shutil.rmtree(os.path.join(OUT, 'title'), ignore_errors=True)
    os.makedirs(os.path.join(OUT, 'title'), exist_ok=True)
    count = 0
    for tid, (key, plate_name, fill_name) in titles.items():
        plate = texture_of(plate_name)
        if plate is None: continue
        bare = plate.convert('RGBA'); bare.thumbnail((512, 512), Image.LANCZOS)
        bare.save(os.path.join(OUT, 'title', f'{tid}.png'), optimize=True)
        fill = texture_of(fill_name)
        for lang, table in texts.items():
            # (Season 2's titles are in S2_DB_Title)
            text = next((t for ns in ('DB_Title', 'S2_DB_Title') for t in (table.get((ns, key)), texts['en'].get((ns, key))) if t), '').strip()
            if not text: continue
            img = draw_title(plate, text, font_path, fill)
            img.thumbnail((512, 512), Image.LANCZOS)
            img.save(os.path.join(OUT, 'title', f'{tid}_{lang}.png'), optimize=True)
        count += 1
    print('honor titles', count, 'of', len(titles))

# ── Every other UI texture ──────────────────────────────────────────────────────────────────────

if not args.no_all:
    os.makedirs(os.path.join(OUT, 'all'), exist_ok=True)
    n = 0
    for f in sorted(files):
        if not (f.startswith(('UI_', 'T_UI_')) and f.endswith('.uasset')): continue
        img = asset(f[:-7])
        if img is None: continue
        img = img.convert('RGBA')
        if max(img.size) > 1024: img.thumbnail((1024, 1024), Image.LANCZOS)
        img.save(os.path.join(OUT, 'all', f[:-7] + '.png'))
        n += 1
    print('all UI textures', n)

# ── Stickers: by the game's table ───────────────────────────────────────────────────────────────

def sticker_rows():
    """sticker id (what the save keeps) -> its thumbnail texture (or texture), from DT_StickerData*:
    the texture's number is not always the sticker's (21700 Sonic (Classic) is ..._Extnd19_20300)"""
    out = {}
    for f, (u, i) in files.items():
        if not re.match(r'DT_StickerData\d+\.uasset$', f): continue
        d = tocs[u].read(i)
        h, N = zen_names(d)
        b = d[h:]
        # Every row starts with its name (a number) right before a struct header 80 25 (80 27 since 2.0)
        starts = [(m.start(), struct.unpack_from('<i', b, m.start())[0]) for m in re.finditer(rb'(?=(....)\x00\x00\x00\x00\x80[\x20-\x2f])', b, re.S)]
        starts = [(o, x) for o, x in starts if 0 <= x < len(N) and N[x].isdigit()]
        for k, (o, x) in enumerate(starts):
            seg = b[o:starts[k + 1][0] if k + 1 < len(starts) else len(b)]
            tmb = tex = None
            for q in range(8, len(seg) - 8):
                v, number = struct.unpack_from('<ii', seg, q)
                if not (0 <= v < len(N) and 0 <= number < 1000000): continue
                name = N[v] + (f'_{number - 1}' if number else '')
                if not tmb and N[v].startswith('UI_StickerTmb'): tmb = name
                elif not tex and N[v].startswith('UI_Sticker') and not N[v].startswith('UI_StickerTmb'): tex = name
            # The base game's designs name no texture in the table: theirs has their number
            number = int(N[x])
            out[str(number)] = [n for n in (tmb, tex) if n] or [f'UI_StickerTmb_{number:05d}', f'UI_Sticker_{number:05d}']
    return out

import shutil
shutil.rmtree(os.path.join(OUT, 'sticker'), ignore_errors=True)
os.makedirs(os.path.join(OUT, 'sticker'), exist_ok=True)
count = 0
rows = sticker_rows()
for sid, textures in rows.items():
    for name in textures:
        img = asset(name)
        if img is None: continue
        img = img.convert('RGBA')
        if max(img.size) > MAX: img.thumbnail((MAX, MAX), Image.LANCZOS)
        img.save(os.path.join(OUT, 'sticker', sid + '.png'), optimize=True)
        count += 1
        break
print('stickers', count, 'of', len(rows))

# ── Horn sounds: CRI ADX in the cue sheet SE_MENU.acb ───────────────────────────────────────────

def cri_table(d, base=0):
    """A CRI @UTF table: its rows as dicts"""
    t = base + 8
    _, rows_off, str_off, data_off, name_off, ncols, row_w, nrows = struct.unpack_from('>HHIIIHHI', d, t)
    strings, datas = t + str_off, t + data_off
    def cstr(o): return d[strings + o:d.index(b'\0', strings + o)].decode('utf-8', 'replace')
    sizes = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 8, 7: 8, 8: 4, 9: 8, 0xA: 4, 0xB: 8}
    fmts = {0: '>B', 1: '>b', 2: '>H', 3: '>h', 4: '>I', 5: '>i', 6: '>Q', 7: '>q', 8: '>f', 9: '>d'}
    def value(kind, o):
        if kind == 0xA: return cstr(struct.unpack_from('>I', d, o)[0])
        if kind == 0xB:
            off, n = struct.unpack_from('>II', d, o)
            return d[datas + off:datas + off + n]
        return struct.unpack_from(fmts[kind], d, o)[0]
    p, cols = t + 24, []
    for _ in range(ncols):
        flags = d[p]; name = cstr(struct.unpack_from('>I', d, p + 1)[0]); p += 5
        const = None
        if flags & 0xF0 in (0x30, 0x70): const = value(flags & 0xF, p); p += sizes[flags & 0xF]
        cols.append((name, flags & 0xF0, flags & 0xF, const))
    rows = []
    for r in range(nrows):
        o, row = t + rows_off + r * row_w, {}
        for name, storage, kind, const in cols:
            if storage == 0x50: row[name] = value(kind, o); o += sizes[kind]
            else: row[name] = const
        rows.append(row)
    return rows

def acb_cue_waves(d):
    """cue name -> its waveforms: (MemoryAwbId, EncodeType, start in ms), following cue -> sequence ->
    track events (note ons after their delays) -> synth -> waveform, or another cue the synth plays"""
    h = cri_table(d)[0]
    get = lambda k: cri_table(h[k]) if isinstance(h.get(k), bytes) and h[k][:4] == b'@UTF' else []
    cues, names, synths, waves = get('CueTable'), get('CueNameTable'), get('SynthTable'), get('WaveformTable')
    seqs, tracks, events = get('SequenceTable'), get('TrackTable'), get('TrackEventTable')
    u16s = lambda b: [struct.unpack_from('>H', b, i)[0] for i in range(0, len(b or b'') - 1, 2)]
    def synth(i, at, depth):
        items, out = u16s(synths[i]['ReferenceItems']), []
        for k in range(0, len(items) - 1, 2):
            if items[k] == 1: out.append((items[k + 1], at))
            elif items[k] == 2 and depth < 4: out += synth(items[k + 1], at, depth + 1)
            elif items[k] == 7 and depth < 4: out += cue(items[k + 1], at, depth + 1)   # another cue (a horn's voice)
        return out
    def seq(i, at, depth):
        out = []
        for t in u16s(seqs[i]['TrackIndex']):
            ev = tracks[t]['EventIndex']
            if ev == 0xFFFF: continue
            blob, p, when = events[ev]['Command'] or b'', 0, at
            while p + 3 <= len(blob):
                code, size = struct.unpack_from('>HB', blob, p)
                if code == 2001 and size >= 4: when += struct.unpack_from('>I', blob, p + 3)[0]   # delay (ms)
                elif code in (2000, 2003) and size >= 4:   # note on: (2 synth, index)
                    kind, idx = struct.unpack_from('>HH', blob, p + 3)
                    if kind == 2: out += synth(idx, when, depth + 1)
                p += 3 + size
        return out
    def cue(i, at=0, depth=0):
        kind, idx = cues[i]['ReferenceType'], cues[i]['ReferenceIndex']
        return [(idx, at)] if kind == 1 else synth(idx, at, depth) if kind == 2 else seq(idx, at, depth) if kind == 3 else []
    return {n['CueName']: [(waves[w]['MemoryAwbId'], waves[w]['EncodeType'], at) for w, at in cue(n['CueIndex'])] for n in names}

def afs2_entries(d):
    """AWB (AFS2) inside an ACB: file id -> its bytes"""
    a = d.find(b'AFS2'); b = d[a:]
    osz, isz = b[5], b[6]
    count, align = struct.unpack_from('<II', b, 8); align &= 0xFFFF
    ids = [int.from_bytes(b[16 + isz * i:16 + isz * i + isz], 'little') for i in range(count)]
    p = 16 + isz * count
    offs = [int.from_bytes(b[p + osz * i:p + osz * i + osz], 'little') for i in range(count + 1)]
    return { ids[i]: b[(offs[i] + align - 1) // align * align:offs[i + 1]] for i in range(count) }

# The game's ADX key (type 9: start, mult, add), read off the streams' silent openings
ADX_KEY = (0x1599, 0x27A1, 0x2CBB)

def adx_key(keycode, subkey):
    """A cue sheet's ADX type-9 key: the CRI keycode times its AWB's subkey (SE_MENU's 0x30a6 gives ADX_KEY;
    the multiplier's bits above 0x1fff never reach a decoded scale, so 0x07a1 and 0x27a1 decode the same)"""
    k = (keycode * ((subkey << 16) | ((~subkey + 2) & 0xFFFF)) & 0xFFFFFFFFFFFFFFFF) - 1
    return ((k >> 27) & 0x7fff, ((k >> 12) & 0x7ffc) | 1, ((k << 1) & 0x7fff) | 1)

def adx_to_wav(adx, key=ADX_KEY):
    import math, wave
    off, = struct.unpack_from('>H', adx, 2)
    bsize, ch = adx[5], adx[7]
    rate, total = struct.unpack_from('>II', adx, 8)
    cutoff, = struct.unpack_from('>H', adx, 0x10)
    enc = adx[0x13]
    z = math.cos(2.0 * math.pi * cutoff / rate)
    a = math.sqrt(2.0) - z; b = math.sqrt(2.0) - 1.0
    c = (a - math.sqrt((a + b) * (a - b))) / b
    c1, c2 = int(c * 8192), int(c * c * -4096)
    hist = [[0, 0] for _ in range(ch)]
    out = [[] for _ in range(ch)]
    x, mult, add = key
    p, done = off + 4, 0
    while done < total and p + bsize * ch <= len(adx):
        for k in range(ch):
            frame = adx[p:p + bsize]; p += bsize
            scale, = struct.unpack_from('>H', frame, 0)
            if enc in (8, 9):
                scale = ((scale ^ x) & 0x1FFF) + 1
                x = (x * mult + add) & 0x7FFF
            h1, h2 = hist[k]
            o = out[k]
            for byte in frame[2:]:
                for nib in (byte >> 4, byte & 15):
                    if nib >= 8: nib -= 16
                    v = nib * scale + ((c1 * h1 + c2 * h2) >> 12)
                    v = -32768 if v < -32768 else 32767 if v > 32767 else v
                    o.append(v); h2, h1 = h1, v
            hist[k] = [h1, h2]
        done += (bsize - 2) * 2
    n = min(total, len(out[0]))
    frames = struct.pack('<%dh' % (n * ch), *[out[k][i] for i in range(n) for k in range(ch)])
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate); w.writeframes(frames)
    return buf.getvalue()

if os.path.exists(pak_path):
    menu = pk.read('UNION/Content/CriWareData/SE_MENU.acb')
    awb = afs2_entries(menu)
    os.makedirs(os.path.join(OUT, 'horn'), exist_ok=True)
    count = 0
    for cue, waves in acb_cue_waves(menu).items():
        m = re.match(r'SE_HORN_(\d+)$', cue)
        if not m or not waves or waves[0][1] != 0: continue   # ADX only (the festivals' horns are HCA)
        open(os.path.join(OUT, 'horn', f'HORNTYPE_{int(m.group(1)):02d}.wav'), 'wb').write(adx_to_wav(awb[waves[0][0]]))
        count += 1
    print('horn sounds', count)

# ── Festival horn sounds: CRI HCA in SE_EXTNDxx_HORN.acb (pakchunk0 and the festivals' paks) ───────

def hca2wav_exe():
    r"""Tools\hca\build\hca2wav.exe, built with the Visual C++ tools when missing"""
    import subprocess
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hca')
    build = os.path.join(src, 'build')
    exe = os.path.join(build, 'hca2wav.exe')
    if os.path.exists(exe): return exe
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'), 'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    if not os.path.exists(vswhere): return None
    vs = subprocess.run([vswhere, '-latest', '-products', '*', '-property', 'installationPath'], capture_output=True, text=True).stdout.strip()
    vcvars = os.path.join(vs, 'VC', 'Auxiliary', 'Build', 'vcvars64.bat')
    if not os.path.exists(vcvars): return None
    os.makedirs(build, exist_ok=True)
    subprocess.run(f'call "{vcvars}" >nul && cl /O2 /nologo /Fehca2wav.exe "{src}\\hca2wav.c" "{src}\\clhca.c"', shell=True, cwd=build, capture_output=True)
    return exe if os.path.exists(exe) else None

def mix_wavs(parts):
    """Plays the layers (path, start in ms) together as the game does (the first one's rate; mono layers go to both sides)"""
    import array, wave
    layers = []
    for path, at in parts:
        with wave.open(path) as w:
            ch, rate = w.getnchannels(), w.getframerate()
            pcm = array.array('h', w.readframes(w.getnframes()))
        layers.append((ch, rate, pcm, at))
    ch = max(c for c, _, _, _ in layers)
    rate = layers[0][1]
    n = max(at * rate // 1000 + len(pcm) // c * rate // r for c, r, pcm, at in layers)
    acc = [0] * (n * ch)
    for c, r, pcm, at in layers:
        frames, start = len(pcm) // c, at * rate // 1000
        for i in range(start, n):
            src = (i - start) * r // rate   # nearest frame at this layer's own rate
            if src >= frames: break
            for k in range(ch):
                acc[i * ch + k] += pcm[src * c + min(k, c - 1)]
    peak = max(1, max(map(abs, acc)))
    out = array.array('h', (v * 32767 // peak for v in acc) if peak > 32767 else acc)   # scaled down rather than clipped
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate); w.writeframes(out.tobytes())
    return buf.getvalue()

if args.cri_key and os.path.exists(pak_path):
    import subprocess, tempfile
    exe = hca2wav_exe()
    if not exe: print('festival horn sounds: no Visual C++ tools to build Tools\\hca\\hca2wav.exe')
    count = 0
    for path in sorted(glob.glob(os.path.join(PAKS, '*.pak'))) if exe else []:
        try: pak = pk if path == pak_path else Pak(path)
        except Exception: continue
        for name in pak.files:
            # A festival's horn (SE_EXTNDxx_HORN_01), or since 2.0 the Duel Medal exchange's ten in one sheet:
            # SE_DUEL_HORN_01..10 are HORNTYPE_40..49
            if m := re.search(r'/SE_EXTND(\d+)_HORN\.acb$', name): horns = {f'SE_EXTND{m.group(1)}_HORN_01': horn_extnd(m.group(1))}
            elif re.search(r'/SE_DUEL_HORN\.acb$', name): horns = {f'SE_DUEL_HORN_{n:02d}': f'HORNTYPE_{39 + n}' for n in range(1, 11)}
            else: continue
            acb = pak.read(name)
            cues = acb_cue_waves(acb)
            subkey, = struct.unpack_from('<H', acb, acb.find(b'AFS2') + 14)   # the AWB's key modifier
            streams = afs2_entries(acb)
            for cue_name, horn in horns.items():
                # The horn's cue: its sound, and on some festivals' horns a voice it starts a little later
                waves = cues.get(cue_name)
                if not waves: continue
                with tempfile.TemporaryDirectory() as tmp:
                    layers = []
                    for k, (awb_id, enc, at) in enumerate(waves):
                        hca, wav = os.path.join(tmp, f'{k}.hca'), os.path.join(tmp, f'{k}.wav')
                        if enc == 0:   # ADX (most of the festivals' since 2.0): its sheet's own key
                            open(wav, 'wb').write(adx_to_wav(streams[awb_id], adx_key(int(args.cri_key, 16), subkey)))
                            layers.append((wav, at))
                            continue
                        open(hca, 'wb').write(streams[awb_id])
                        if subprocess.run([exe, hca, wav, args.cri_key, f'{subkey:x}'], capture_output=True).returncode == 0: layers.append((wav, at))
                    if len(layers) == len(waves):
                        open(os.path.join(OUT, 'horn', horn + '.wav'), 'wb').write(mix_wavs(layers))
                        count += 1
    print('festival horn sounds', count)

# ── Colour presets: a swatch each, from DT_MachineColorPreset* and the palette DT_MachineColorParam ──

def fstring(b, p):
    n, = struct.unpack_from('<i', b, p); p += 4
    if n >= 0: return b[p:p + n - 1].decode('latin1'), p + n
    return b[p:p - 2 * n - 2].decode('utf-16le'), p - 2 * n

def package(name):
    hit = files.get(name + '.uasset')
    return tocs[hit[0]].read(hit[1]) if hit else None

def srgb(c):
    c = max(0.0, min(1.0, c))
    return round(255 * (12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055))

def color_palette():
    """palette index -> (r, g, b): its first colour, sRGB"""
    d = package('DT_MachineColorParam'); h, N = zen_names(d); b = d[h:]
    out = {}
    for m in re.finditer(rb'\x80\x0b\x02\x00\x00\x00\x00\x00', b):
        idx, _ = struct.unpack_from('<ii', b, m.start() - 8)
        if not (0 <= idx < len(N)) or not N[idx].isdigit(): continue
        q = m.end()
        for _ in range(3): _, q = fstring(b, q)   # its name, as an FText
        r, g, bl, _ = struct.unpack_from('<4f', b, q)
        out[int(N[idx])] = (srgb(r), srgb(g), srgb(bl))
    return out

def color_presets(palette):
    """ColorPresetNN -> (its name's key: "machine_SPD_01" / "FestaTeamColor101" / "", body / cockpit / tyre
    groups of (colour, surface) x 3 + glow); rows without colours (empty ones) are left out"""
    def plausible(b, q):
        if q + 28 > len(b): return None
        v = struct.unpack_from('<7i', b, q)
        return list(v) if all(0 <= x <= 255 for x in v) and all(v[i] in palette or v[i] == 0 for i in (0, 2, 4)) else None
    out = {}
    for f in sorted(files):
        if not re.match(r'DT_MachineColorPreset\d*\.uasset$', f): continue
        d = package(f[:-7]); h, N = zen_names(d); b = d[h:]
        # Every row: its name (an EMachineColorPresetId value) before the struct's header (.. 17)
        heads = []
        for m in re.finditer(rb'[\x00\x80][\x10-\x1f]', b):   # .. 17 (80 1b since 2.0)
            if m.start() < 8: continue
            idx, num = struct.unpack_from('<ii', b, m.start() - 8)
            if 0 <= idx < len(N) and num == 0 and N[idx].startswith('EMachineColorPresetId::'): heads.append((m.start(), N[idx].split('::')[1]))
        for k, (at, rid) in enumerate(heads):
            end = heads[k + 1][0] - 8 if k + 1 < len(heads) else len(b)
            keys = re.findall(rb'(machine_[A-Z]{3}_\d\d|FestaTeamColor\d+)\x00', b[at:end])
            for q in range(at + 2, end - 2):
                if b[q] == 0x0f and b[q - 1] in (0x00, 0xff):
                    g1 = plausible(b, q + 1)
                    if g1 and b[q + 29:q + 31] == b'\x00\x0f':
                        g2 = plausible(b, q + 31); g3 = plausible(b, q + 61) if g2 else None
                        if g2 and g3:
                            out[rid] = (keys[0].decode() if keys else '', [g1, g2, g3])
                            break
    return out

def swatch(colors, size=128):
    """A round chip of the body's colours: the first on top, the others in bands under it"""
    from PIL import ImageDraw
    big = size * 4
    img = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    bands = [(0, 0.55), (0.55, 0.8), (0.8, 1.0)]
    for (a, z), c in zip(bands, colors):
        draw.rectangle((0, round(a * big), big, round(z * big)), fill=c + (255,))
    mask = Image.new('L', (big, big), 0)
    ImageDraw.Draw(mask).ellipse((8, 8, big - 9, big - 9), fill=255)
    img.putalpha(mask)
    ring = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    ImageDraw.Draw(ring).ellipse((8, 8, big - 9, big - 9), outline=(255, 255, 255, 140), width=10)
    img.alpha_composite(ring)
    return img.resize((size, size), Image.LANCZOS)

palette = color_palette()
presets = color_presets(palette)
os.makedirs(os.path.join(OUT, 'color'), exist_ok=True)
for rid, (key, groups) in presets.items():
    body = groups[0]
    swatch([palette.get(body[i], (128, 128, 128)) for i in (0, 2, 4)]).save(os.path.join(OUT, 'color', rid + '.png'), optimize=True)
print('colour presets', len(presets))

if args.catalog:
    def number(rid): return int(rid.replace('ColorPreset', ''))
    lines = ['// Generated by Tools\\ExtractGameImages.py --catalog: the colour presets the game has (DT_MachineColorPreset*),',
             '// each with what names it (the machine whose colours they are, or the festival team colour) and its values. Do not edit by hand.',
             '#include "Data/ColorCatalog.h"', '', 'namespace SonicRacingSaveManager::Data::ColorCatalog {', '',
             'const std::vector<ColorPreset>& Presets() {', '    static const std::vector<ColorPreset> list{']
    for rid in sorted(presets, key=number):
        key = presets[rid][0]
        machine = key[len('machine_'):] if key.startswith('machine_') else ''
        festival = int(key[len('FestaTeamColor'):]) if key.startswith('FestaTeamColor') else 0
        flat = ', '.join(str(v) for g in presets[rid][1] for v in g)
        lines.append(f'        {{ {number(rid)}, "{machine}", {festival}, {{ {flat} }} }},')
    lines += ['    };', '    return list;', '}', '', '} // namespace SonicRacingSaveManager::Data::ColorCatalog', '']
    open(os.path.join(args.catalog, 'ColorCatalog.cpp'), 'w', encoding='utf-8', newline='\r\n').write('\n'.join(lines))
    print('ColorCatalog.cpp', len(presets))

# ── The jukebox's catalog (Source\Data\MusicCatalog.cpp): every album and track, the album's name and the
# track's name (en / es / fr / it) and artist ──────────────────────────────────────────────────────────

# The game names no album (its jacket shows its name): the base game's by what they hold, the others by
# their series; one not named here takes its first track's name
ALBUM_NAMES = {
    1001: ('Courses', 'Circuitos', 'Circuits', 'Circuiti'),
    2001: ('CrossWorlds',) * 4,
    2002: ('Duels', 'Duelos', 'Duels', 'Duelli'),
    3001: ('Menus and themes', 'Menús y temas', 'Menus et thèmes', 'Menu e temi'),
    3002: ('Favourites', 'Favoritos', 'Favoris', 'Preferiti'),
    4001: ('Sonic series', 'Saga Sonic', 'Série Sonic', 'Serie Sonic'),
    4002: ('Creepy Nuts',) * 4, 4003: ('Crazy Raccoon',) * 4, 4004: ('Inugami Korone',) * 4,
    4005: ('SEGA Racing & Variety',) * 4, 4006: ('maimai DX',) * 4, 4007: ('CHUNITHM',) * 4, 4008: ('ONGEKI',) * 4,
    4009: ('Crush 40',) * 4, 4010: ('Rhythm & Burning',) * 4, 4011: ('Fighting Beats',) * 4, 5012: ('Like a Dragon',) * 4, 5019: ('Sonic 35th Anniversary',) * 4,
    5001: ('Werehog',) * 4, 5002: ('Sonic Prime',) * 4, 5004: ('Minecraft',) * 4, 5005: ('SpongeBob SquarePants',) * 4,
    5006: ('PAC-MAN',) * 4, 5007: ('Avatar Legends',) * 4, 5008: ('Teenage Mutant Ninja Turtles',) * 4, 5009: ('Mega Man',) * 4,
    5010: ('Hatsune Miku',) * 4, 5011: ('Persona 5',) * 4, 5013: ('Tangle & Whisper',) * 4, 5014: ('Super Monkey Ball',) * 4,
    5015: ('NiGHTS',) * 4, 5016: ('Angry Birds',) * 4, 5018: ('Puyo Puyo',) * 4, 5020: ('Samba de Amigo',) * 4,
    5022: ('Bayonetta',) * 4, 5023: ('Godzilla',) * 4, 5024: ('Evangelion',) * 4, 5025: ('Persona 3 Reload',) * 4,
}

def track_text(lang, key):
    t = texts[lang].get(('DB_BGM', key)) or texts[lang].get(('S2_DB_BGM', key)) or texts['en'].get(('DB_BGM', key)) or texts['en'].get(('S2_DB_BGM', key)) or ''
    # "Get Higher　　【for Final Lap】" as the other final laps: "Get Higher (for Final Lap)"
    t = re.sub(r'[　 ]+【(.+?)】\s*$', lambda m: ' (' + m.group(1).strip() + ')', t.strip())
    return t.replace('　', ' ').strip()

def wide(t):
    return 'L"' + ''.join(c if 32 <= ord(c) < 127 and c not in '"\\' else ('\\"' if c == '"' else '\\\\' if c == '\\' else (f'\\u{ord(c):04X}' if ord(c) < 0x10000 else f'\\U{ord(c):08X}')) for c in t) + '"'

if args.catalog and os.path.exists(pak_path):
    tracks = track_rows()
    langs = ('en', 'es', 'fr', 'it')
    lines = ['// Generated by Tools\\ExtractGameImages.py --catalog: the jukebox\'s albums (DT_AlbumData_NN) and tracks',
             '// (DT_TrackData_NN), with their names (DB_BGM) and artists. Do not edit by hand.',
             '#include "Data/MusicCatalog.h"', '', 'namespace SonicRacingSaveManager::Data::MusicCatalog {', '',
             'const std::vector<Album>& Albums() {', '    static const std::vector<Album> list{']
    for aid in sorted(albums, key=lambda a: (albums[a][0], a)):
        names = ALBUM_NAMES.get(aid)
        if not names:
            first = sorted(t for t, v in tracks.items() if v[0] == aid)
            names = tuple(track_text(l, tracks[first[0]][1]) if first else f'Album {aid}' for l in langs)
        # The collab characters' albums come with their packs, like the characters (Werehog, 5004 and up)
        dlc = aid == 5001 or aid >= 5004
        lines.append(f'        {{ {aid}, {albums[aid][0]}, {"true" if dlc else "false"}, {{ {", ".join(wide(n) for n in names)} }} }},')
    lines += ['    };', '    return list;', '}', '', 'const std::vector<Track>& Tracks() {', '    static const std::vector<Track> list{']
    for tid in sorted(tracks):
        aid, name, artist = tracks[tid]
        lines.append(f'        {{ {tid}, {aid}, {{ {", ".join(wide(track_text(l, name)) for l in langs)} }}, {wide(track_text("en", artist))} }},')
    lines += ['    };', '    return list;', '}', '', '} // namespace SonicRacingSaveManager::Data::MusicCatalog', '']
    open(os.path.join(args.catalog, 'MusicCatalog.cpp'), 'w', encoding='utf-8', newline='\r\n').write('\n'.join(lines))
    print('MusicCatalog.cpp', len(albums), 'albums', len(tracks), 'tracks')

# ── Auras: the game's aura icon, tinted for the plain coloured ones (NS_FX_Aura_001_Normal) ─────────

AURA_TINTS = { 'MACHINE_AURA_001': (64, 140, 255), 'MACHINE_AURA_002': (255, 214, 40), 'MACHINE_AURA_003': (255, 64, 64),
               'MACHINE_AURA_004': (255, 105, 190), 'MACHINE_AURA_005': (240, 244, 255),
               # The effects the game has no picture for (only the exchange's have one), in their effect's colour: by
               # DT_AuraData_01's rows (id 006 is Burning, 009 Lightning, 011 Virtual Image... not the text numbers);
               # 017 Rainbow keeps the icon's own colours
               'MACHINE_AURA_006': (255, 120, 30),    # Burning
               'MACHINE_AURA_007': (90, 230, 200),    # Wind
               'MACHINE_AURA_008': (255, 150, 200),   # Cherry Blossom
               'MACHINE_AURA_009': (255, 230, 60),    # Lightning
               'MACHINE_AURA_010': (230, 120, 40),    # Autumn
               'MACHINE_AURA_011': (60, 230, 90),     # Virtual Image (green numbers)
               'MACHINE_AURA_012': (40, 220, 255),    # Cyber Image
               'MACHINE_AURA_013': (255, 210, 90),    # Shooting Star
               'MACHINE_AURA_014': (240, 120, 210),   # Floral
               'MACHINE_AURA_015': (150, 220, 255),   # Ice
               'MACHINE_AURA_016': (150, 70, 220),    # Dark
               'MACHINE_AURA_018': (110, 190, 255),   # Timer
               'MACHINE_AURA_019': (255, 70, 130),    # Heart
               'MACHINE_AURA_020': (170, 170, 185) }  # Noise

def aura_ids():
    out = set()
    for f in sorted(files):
        if not re.match(r'DT_AuraData_\d+\.uasset$', f): continue
        d = package(f[:-7]); h, N = zen_names(d)
        out |= {x.split('::')[1] for x in N if x.startswith('EMachineAuraId::MACHINE_AURA_0')}
    return sorted(out)

icon = asset('UI_ItemIcon_AuraIcon')
if icon is not None:
    icon = icon.convert('RGBA')
    icon.thumbnail((MAX, MAX), Image.LANCZOS)
    os.makedirs(os.path.join(OUT, 'aura'), exist_ok=True)
    gray = icon.convert('L')
    n = 0
    for aid in aura_ids():
        img = icon
        if aid in AURA_TINTS:
            # The rainbow glow in the aura's own colour (the car keeps its shape: light stays light)
            r, g, bl = AURA_TINTS[aid]
            img = Image.merge('RGBA', [gray.point(lambda v, c=c: round(v * c / 255)) for c in (r, g, bl)] + [icon.getchannel('A')])
        img.save(os.path.join(OUT, 'aura', aid + '.png'), optimize=True)
        n += 1
    print('auras', n)

# The Duel Medal exchange's auras (001..005, 031..): its own thumbnail, a machine wearing it (framed, like the
# horns' pictures), in place of the tinted icon
n = 0
for f in sorted(files):
    if m := re.match(r'UI_AuraTmb_Exchange_(\d{4})\.uasset$', f):
        img = asset(f[:-7])
        if img is None: continue
        img = img.convert('RGBA'); img.thumbnail((MAX, MAX), Image.LANCZOS)
        os.makedirs(os.path.join(OUT, 'aura'), exist_ok=True)
        img.save(os.path.join(OUT, 'aura', f'MACHINE_AURA_{int(m.group(1)):03d}.png'), optimize=True)
        n += 1
print('aura thumbnails', n)
