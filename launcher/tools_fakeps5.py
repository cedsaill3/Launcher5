"""Cree fake_ps5/ : une fausse bibliotheque PS5 pour tester le launcher sur PC."""
import json, struct, zlib, colorsys, random
from pathlib import Path

ROOT = Path("fake_ps5")

def png(path, w, h, seed):
    """PNG degrade (sans dependance) pour simuler une jaquette."""
    rnd = random.Random(seed)
    h1, h2 = rnd.random(), rnd.random()
    rows = []
    for y in range(h):
        row = bytearray([0])
        for x in range(w):
            t = (x + y) / (w + h)
            hue = h1 + (h2 - h1) * t
            r, g, b = colorsys.hsv_to_rgb(hue % 1, 0.65, 0.35 + 0.55 * (1 - t))
            if ((x // 32) + (y // 32)) % 2 and rnd.random() < 0.002: pass
            row += bytes((int(r * 255), int(g * 255), int(b * 255)))
        rows.append(bytes(row))
    raw = zlib.compress(b"".join(rows), 6)
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", raw) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)

def sfo(path, title):
    """param.sfo minimal (jeu PS4) avec la cle TITLE."""
    key = b"TITLE\0"; val = title.encode() + b"\0"
    keys_off = 20 + 16; data_off = keys_off + len(key) + (4 - (len(key) % 4)) % 4
    hdr = b"\0PSF" + struct.pack("<IIII", 0x101, keys_off, data_off, 1)
    entry = struct.pack("<HHIII", 0, 0x0204, len(val), len(val), 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(hdr + entry + key.ljust(data_off - keys_off, b"\0") + val)

ps5 = ["Astral Drift", "Neon Runner", "Les Royaumes Oubliés", "Turbo Kart Arena", "Silent Harbor", "Mecha Siege",
       "Ocean Depths", "Pixel Brawlers", "Shadow Protocol", "Desert Nomad", "Ciel de Feu", "Frozen Peaks"]
for i, t in enumerate(ps5):
    tid = f"PPSA{10000 + i * 37:05d}"
    p = ROOT / "user/app" / tid / "sce_sys/param.json"
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps({"titleId": tid, "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": t}}}))
    if i % 5 != 4:  # quelques jeux sans jaquette pour tester la tuile de remplacement
        png(ROOT / "user/appmeta" / tid / "icon0.png", 256, 256, tid)
# illustrations de fond (pic1) pour quelques jeux
for i in (0, 1, 3, 6, 8):
    tid = f"PPSA{10000 + i * 37:05d}"
    png(ROOT / "user/appmeta" / tid / "pic1.png", 384, 216, tid + "bg")
# applications multimedia
for i, t in enumerate(["Streaming Vidéo", "Musique Live", "Ciné Club", "Podcast Radio"]):
    tid = f"PPSA{30000 + i * 13:05d}"
    pj = ROOT / "user/app" / tid / "sce_sys/param.json"
    pj.parent.mkdir(parents=True, exist_ok=True)
    pj.write_text(json.dumps({"titleId": tid, "applicationCategoryType": 65536,
                              "localizedParameters": {"defaultLanguage": "fr-FR", "fr-FR": {"titleName": t}}}))
    png(ROOT / "user/appmeta" / tid / "icon0.png", 256, 256, tid)
for i, t in enumerate(["Old Kingdom HD", "Racing Legends", "Space Hunter"]):
    tid = f"CUSA{20000 + i * 11:05d}"
    sfo(ROOT / "user/app" / tid / "sce_sys/param.sfo", t)
    png(ROOT / "user/appmeta" / tid / "icon0.png", 256, 256, tid)
for hb in ["RetroArch", "Lecteur Video", "Explorateur"]:
    d = ROOT / "data/homebrew" / hb / "sce_sys"
    d.mkdir(parents=True, exist_ok=True)
    png(d / "icon0.png", 256, 256, hb)
(ROOT / "data/payloads").mkdir(parents=True, exist_ok=True)
for el in ["ftpsrv", "klogsrv", "gdbsrv"]:
    (ROOT / "data/payloads" / f"{el}.elf").write_bytes(b"\x7fELF")
print("fake_ps5 cree :", sum(1 for _ in ROOT.rglob("*") if _.is_file()), "fichiers")
