"""
Classement des applications de la PS5 a partir de sa base appinfo.db.
Meme regles que le bootstrap (src/bootstrap.c) : sert a les verifier sur PC
et a generer fake_ps5/data/launcher5/apps.json pour tester l'interface.

    py tools_catalog.py chemin/appinfo.db [sortie.json]
"""
import collections, json, re, sqlite3, sys

MEDIA_WORDS = re.compile(r"tv|radio|video|vidéo|music|musique|player|youtube|netflix|vlc|jelly|plex|kodi|"
                         r"stream|twitch|spotify|disney|prime|crunchyroll|ciné|cine|movie|media|cd ", re.I)
EMU_WORDS = re.compile(r"emu|pcsx|ps5sx2|dolphin|porpoise|retroarch|eden|switch|xps|mame|fbneo|snes|nes|gba|"
                       r"genesis|duck|ppsspp|mednafen|scumm|ryujinx|citra|cemu", re.I)


def classify(tid, r):
    """Renvoie (type, media, emu) ou None pour ignorer."""
    if tid.startswith("NPXS") or tid == "FAKE00000":
        return None
    cid = r.get("CONTENT_ID", "") or ""
    pub = cid[:6]
    app_type = int(r.get("APP_TYPE", "0") or 0)
    cat_type = int(r.get("CATEGORY_TYPE", "0") or 0)
    ps4 = r.get("_ps_platform") == "1" or (r.get("CATEGORY", "") or "").startswith("gd") or tid.startswith("CUSA")
    title = r.get("TITLE", "")
    homebrew = cid in ("", "0") or pub == "IV9999" or (pub == "UP9000" and app_type == 5)
    if homebrew:
        kind = "homebrew"
    elif ps4:
        kind = "ps4"
    elif app_type == 1 or cat_type == 0:
        kind = "ps5"
    else:
        kind = "media"            # application officielle (Netflix, YouTube…)
    media = kind == "media" or (homebrew and bool(MEDIA_WORDS.search(title)))
    emu = homebrew and bool(EMU_WORDS.search(title))
    return kind, media, emu


def load(db):
    rows = collections.defaultdict(dict)
    for t, k, v in sqlite3.connect(db).execute("select titleId, key, val from tbl_appinfo"):
        rows[t][k] = v
    return rows


def main():
    rows = load(sys.argv[1])
    out = []
    for tid in sorted(rows):
        r = rows[tid]
        c = classify(tid, r)
        if not c or ("TITLE" not in r):
            continue
        kind, media, emu = c
        title = r.get("TITLE_02") or r.get("TITLE")   # 02 = francais
        out.append({"id": tid, "title": title, "kind": kind, "media": media, "emu": emu})
    for kind in ("ps5", "ps4", "media", "homebrew"):
        names = [f"{a['title']}{' [média]' if a['media'] and kind == 'homebrew' else ''}{' [émul.]' if a['emu'] else ''}"
                 for a in out if a["kind"] == kind]
        print(f"\n{kind.upper()} ({len(names)}) : " + " · ".join(names))
    if len(sys.argv) > 2:
        json.dump(out, open(sys.argv[2], "w", encoding="utf-8"), ensure_ascii=False, indent=1)


if __name__ == "__main__":
    main()
