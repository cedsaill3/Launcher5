/*
 * Launcher5 - recherche du contenu installe sur la console.
 *
 *   /user/app/<ID>/sce_sys/param.json   jeux et applications PS5
 *   /user/app/<ID>/sce_sys/param.sfo    jeux PS4
 *   /user/appmeta/<ID>/icon0.png        jaquettes
 *   /data/homebrew/<nom>/               homebrews
 *   /data/payloads/(nom).elf            payloads (et dossiers de Payload Manager, etaHEN)
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <jansson.h>

#include "platform.h"
#include "i18n.h"

/* Applications systeme a ne pas montrer */
static const char *IGNORED[] = { "FAKE00000", "NPXS", NULL };

static void P(char *out, size_t n, const char *fmt, const char *a, const char *b) {
  char tmp[PATH_LEN];
  snprintf(tmp, sizeof tmp, fmt, a ? a : "", b ? b : "");
  snprintf(out, n, "%s%s", plat_root(), tmp);
}

static int exists(const char *p) {
  struct stat st;
  return stat(p, &st) == 0;
}

static int ignored(const char *id) {
  for (int i = 0; IGNORED[i]; i++)
    if (!strncmp(id, IGNORED[i], strlen(IGNORED[i]))) return 1;
  return 0;
}

/* Nom du jeu dans param.json (PS5) : langue de la console, sinon celle du jeu, sinon anglais */
static int read_param_json(const char *path, char *title, size_t n, int *media) {
  json_t *j = json_load_file(path, 0, NULL);
  if (!j) return -1;
  /* applicationCategoryType : 0 pour un jeu, autre valeur pour une appli (video, musique…) */
  if (media) *media = json_integer_value(json_object_get(j, "applicationCategoryType")) != 0;
  json_t *loc = json_object_get(j, "localizedParameters");
  const char *lang = json_string_value(json_object_get(loc, "defaultLanguage"));
  const char *langs[] = { i18n_locale(), lang, "en-US", "en-GB" };
  const char *name = NULL;
  for (int i = 0; i < 4; i++) {
    if (!langs[i]) continue;
    name = json_string_value(json_object_get(json_object_get(loc, langs[i]), "titleName"));
    if (name) break;
  }
  if (!name) name = json_string_value(json_object_get(j, "titleName"));
  if (name) snprintf(title, n, "%s", name);
  json_decref(j);
  return name ? 0 : -1;
}

/* Nom du jeu dans param.sfo (PS4) : format binaire « PSF » */
static int read_param_sfo(const char *path, char *title, size_t n);
/* nom du jeu contenu dans un dossier (PS5 : param.json, PS4 : param.sfo) */
int scan_folder_title(const char *dir, char *title, size_t n) {
  char p[1200];
  snprintf(p, sizeof p, "%s/sce_sys/param.json", dir);
  if (!access(p, F_OK)) return read_param_json(p, title, n, NULL);
  snprintf(p, sizeof p, "%s/sce_sys/param.sfo", dir);
  if (!access(p, F_OK)) return read_param_sfo(p, title, n);
  return -1;
}
static int read_param_sfo(const char *path, char *title, size_t n) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  unsigned char buf[65536];
  size_t len = fread(buf, 1, sizeof buf, f);
  fclose(f);
  if (len < 20 || memcmp(buf, "\0PSF", 4)) return -1;
  uint32_t keys = buf[8] | buf[9] << 8 | buf[10] << 16 | (uint32_t)buf[11] << 24;
  uint32_t data = buf[12] | buf[13] << 8 | buf[14] << 16 | (uint32_t)buf[15] << 24;
  uint32_t count = buf[16] | buf[17] << 8 | buf[18] << 16 | (uint32_t)buf[19] << 24;
  for (uint32_t i = 0; i < count && 20 + i * 16 + 16 <= len; i++) {
    unsigned char *e = buf + 20 + i * 16;
    uint16_t koff = e[0] | e[1] << 8;
    uint32_t dlen = e[4] | e[5] << 8 | e[6] << 16 | (uint32_t)e[7] << 24;
    uint32_t doff = e[12] | e[13] << 8 | e[14] << 16 | (uint32_t)e[15] << 24;
    if (keys + koff >= len || data + doff + dlen > len) continue;
    if (!strcmp((char *)buf + keys + koff, "TITLE")) {
      snprintf(title, n, "%.*s", (int)dlen, (char *)buf + data + doff);
      return 0;
    }
  }
  return -1;
}

/* Jaquette et illustration de fond d'une application installee */
static void set_images(App *a, const char *id) {
  P(a->icon, sizeof a->icon, "/user/appmeta/%s/icon0.png", id, NULL);
  if (!exists(a->icon)) P(a->icon, sizeof a->icon, "/user/app/%s/sce_sys/icon0.png", id, NULL);
  const char *pics[] = { "/user/appmeta/%s/pic1.png", "/user/appmeta/%s/pic0.png",
                         "/user/app/%s/sce_sys/pic1.png", "/user/app/%s/sce_sys/pic0.png" };
  for (int i = 0; i < 4 && !a->bg[0]; i++) {
    P(a->bg, sizeof a->bg, pics[i], id, NULL);
    if (!exists(a->bg)) a->bg[0] = 0;
  }
}

/* Catalogue ecrit par le bootstrap a partir de la base de la console :
   classement fiable (jeux PS5, PS4, medias, homebrews, emulateurs). */
static int scan_catalog(Menu *m) {
  char path[PATH_LEN];
  snprintf(path, sizeof path, "%s/apps.json", plat_data_dir());
  json_t *j = json_load_file(path, 0, NULL);
  if (!json_is_array(j)) { json_decref(j); return -1; }
  size_t i; json_t *v;
  json_array_foreach(j, i, v) {
    const char *id = json_string_value(json_object_get(v, "id"));
    const char *title = json_string_value(json_object_get(v, "title"));
    const char *kind = json_string_value(json_object_get(v, "kind"));
    if (!id || !title || !kind) continue;
    App a = {0};
    snprintf(a.id, sizeof a.id, "%s", id);
    snprintf(a.title, sizeof a.title, "%s", title);
    a.kind = !strcmp(kind, "ps4") ? KIND_PS4 : !strcmp(kind, "media") ? KIND_MEDIA
           : !strcmp(kind, "homebrew") ? KIND_HOMEBREW : KIND_PS5;
    a.flags = APPF_INSTALLED;
    if (json_is_true(json_object_get(v, "media"))) a.flags |= APPF_MEDIA;
    if (json_is_true(json_object_get(v, "emu"))) a.flags |= APPF_EMU;
    if (a.kind == KIND_MEDIA) a.flags |= APPF_MEDIA;
    set_images(&a, id);
    menu_add_app(m, &a);
  }
  json_decref(j);
  return 0;
}

static void scan_installed(Menu *m) {
  if (!scan_catalog(m)) return;
  char dir[PATH_LEN];
  P(dir, sizeof dir, "/user/app", NULL, NULL);
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.' || ignored(e->d_name)) continue;
    App a = {0};
    char p[PATH_LEN];
    snprintf(a.id, sizeof a.id, "%s", e->d_name);
    P(p, sizeof p, "/user/app/%s/sce_sys/param.json", e->d_name, NULL);
    int media = 0;
    if (!read_param_json(p, a.title, sizeof a.title, &media)) {
      a.kind = media ? KIND_MEDIA : KIND_PS5;
    } else {
      P(p, sizeof p, "/user/app/%s/sce_sys/param.sfo", e->d_name, NULL);
      if (read_param_sfo(p, a.title, sizeof a.title)) continue;
      a.kind = KIND_PS4;
    }
    a.flags = APPF_INSTALLED | (a.kind == KIND_MEDIA ? APPF_MEDIA : 0);
    set_images(&a, e->d_name);
    menu_add_app(m, &a);
  }
  closedir(d);
}

static void scan_homebrew(Menu *m) {
  char dir[PATH_LEN];
  P(dir, sizeof dir, "/data/homebrew", NULL, NULL);
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    /* deja present comme application installee (meme identifiant) : pas de doublon */
    if (menu_find_app(m, e->d_name)) continue;
    App a = {0};
    char p[PATH_LEN];
    snprintf(a.id, sizeof a.id, "hb:%.36s", e->d_name);
    P(a.path, sizeof a.path, "/data/homebrew/%s", e->d_name, NULL);
    struct stat st;
    if (stat(a.path, &st) || !S_ISDIR(st.st_mode)) continue;
    /* seuls les dossiers qui contiennent un programme (.elf) sont lancables */
    DIR *hd = opendir(a.path);
    int has_elf = 0;
    struct dirent *he;
    while (hd && (he = readdir(hd))) {
      size_t l = strlen(he->d_name);
      if (l > 4 && !strcasecmp(he->d_name + l - 4, ".elf")) has_elf = 1;
    }
    if (hd) closedir(hd);
    if (!has_elf) continue;
    P(p, sizeof p, "/data/homebrew/%s/sce_sys/param.json", e->d_name, NULL);
    if (read_param_json(p, a.title, sizeof a.title, NULL)) snprintf(a.title, sizeof a.title, "%s", e->d_name);
    P(a.icon, sizeof a.icon, "/data/homebrew/%s/sce_sys/icon0.png", e->d_name, NULL);
    a.kind = KIND_HOMEBREW;
    menu_add_app(m, &a);
  }
  closedir(d);
}

/* Icones des payloads : Payload Manager garde la liste de son depot (repo_cache_*.json :
   fichier, nom, icon_key) et les icones (repo_icons_*.json : icon_key -> PNG en base64).
   L'icone est extraite une fois dans cache/ et sert de jaquette. */
typedef struct { char file[96], name[64], key[72]; } RepoIcon;
static RepoIcon *RI;
static int nRI, ri_loaded;
static json_t *ICONS, *ICONS2;                   /* objets icon_key -> « data:image/png;base64,… » */

static int has_affix(const char *s, const char *pre, const char *suf) {
  size_t l = strlen(s), lp = strlen(pre), ls = strlen(suf);
  return l > lp + ls && !strncmp(s, pre, lp) && !strcmp(s + l - ls, suf);
}

static void repo_list_add(const char *path) {     /* liste au format du depot : payloads[] */
  json_t *j = json_load_file(path, 0, NULL);
  size_t i; json_t *v;
  json_array_foreach(json_object_get(j, "payloads"), i, v) {
    const char *f = json_string_value(json_object_get(v, "filename"));
    const char *n = json_string_value(json_object_get(v, "name"));
    const char *k = json_string_value(json_object_get(v, "icon_key"));
    if (!k || (!f && !n)) continue;
    RepoIcon *r = realloc(RI, sizeof *RI * (nRI + 1));
    if (!r) break;
    RI = r;
    snprintf(RI[nRI].file, sizeof RI[nRI].file, "%s", f ? f : "");
    snprintf(RI[nRI].name, sizeof RI[nRI].name, "%s", n ? n : "");
    snprintf(RI[nRI].key, sizeof RI[nRI].key, "%s", k);
    nRI++;
  }
  json_decref(j);
}

/* listes et icones a relire (apres une mise a jour du catalogue) */
void payload_icons_reset(void) {
  free(RI); RI = NULL; nRI = 0; ri_loaded = 0;
  if (ICONS) json_decref(ICONS);
  if (ICONS2) json_decref(ICONS2);
  ICONS = ICONS2 = NULL;
}

static void repo_icons_load(void) {
  ri_loaded = 1;
  char dir[PATH_LEN], p[PATH_LEN + 300];
  /* catalogue de Launcher5 (meme format que le depot de Payload Manager) */
  snprintf(p, sizeof p, "%s/cache/payloads_index.json", plat_data_dir());
  repo_list_add(p);
  snprintf(p, sizeof p, "%s/cache/payloads_icons.json", plat_data_dir());
  json_t *mine = json_load_file(p, 0, NULL);
  json_t *mic = json_object_get(mine, "icons");
  if (json_is_object(mic)) { ICONS2 = mic; json_incref(mic); }
  json_decref(mine);
  P(dir, sizeof dir, "/data/pldmgr", NULL, NULL);
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d))) {
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    if (has_affix(e->d_name, "repo_cache_", ".json")) repo_list_add(p);
    else if (has_affix(e->d_name, "repo_icons_", ".json") && !ICONS) {
      json_t *j = json_load_file(p, 0, NULL);
      json_t *ic = json_object_get(j, "icons");
      if (json_is_object(ic)) { ICONS = ic; json_incref(ic); }
      json_decref(j);
    }
  }
  closedir(d);
}

static int b64v(int c) {
  return c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
       : c == '+' ? 62 : c == '/' ? 63 : -1;
}

/* icone d'un payload (meme fichier, sinon meme nom dans une liste) ; 0 si trouvee */
int payload_icon_path(const char *file, const char *id, char *res, size_t rn) {
  if (!ri_loaded) repo_icons_load();
  if ((!ICONS && !ICONS2) || !nRI) return -1;
  const char *key = NULL;
  for (int i = 0; i < nRI && !key; i++) if (RI[i].file[0] && !strcasecmp(RI[i].file, file)) key = RI[i].key;
  for (int i = 0; i < nRI && !key; i++) if (RI[i].name[0] && id && !strcasecmp(RI[i].name, id)) key = RI[i].key;
  if (!key) return -1;
  char out[PATH_LEN];
  snprintf(out, sizeof out, "%s/cache/pl_%.24s.png", plat_data_dir(), key);   /* PNG ou JPEG selon l'icone */
  if (!exists(out)) snprintf(out, sizeof out, "%s/cache/pl_%.24s.jpg", plat_data_dir(), key);
  if (!exists(out)) {
    const char *uri = json_string_value(json_object_get(ICONS, key));
    if (!uri) uri = json_string_value(json_object_get(ICONS2, key));
    const char *b = uri ? strstr(uri, "base64,") : NULL;
    if (!b) return -1;
    b += 7;
    size_t n = strlen(b);
    unsigned char *buf = malloc(n / 4 * 3 + 3);
    if (!buf) return -1;
    size_t k = 0;
    unsigned acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
      int v = b64v((unsigned char)b[i]);
      if (v < 0) continue;
      acc = ((acc << 6) | (unsigned)v) & 0xffffffu;
      bits += 6;
      if (bits >= 8) { bits -= 8; buf[k++] = (unsigned char)((acc >> bits) & 0xff); }
    }
    int png = k > 8 && !memcmp(buf, "\x89PNG", 4), jpg = k > 8 && buf[0] == 0xff && buf[1] == 0xd8;
    int ok = png || jpg;
    snprintf(out, sizeof out, "%s/cache/pl_%.24s.%s", plat_data_dir(), key, png ? "png" : "jpg");
    if (ok) {
      char tmp[PATH_LEN + 8];
      snprintf(tmp, sizeof tmp, "%s.tmp", out);
      FILE *f = fopen(tmp, "wb");
      ok = f && fwrite(buf, 1, k, f) == k;
      if (f) fclose(f);
      ok = ok && !rename(tmp, out);
      if (!ok) remove(tmp);
    }
    free(buf);
    if (!ok) return -1;
  }
  snprintf(res, rn, "%s", out);
  return 0;
}

static int payload_icon(App *a, const char *file, const char *id) {
  return payload_icon_path(file, id, a->icon, sizeof a->icon);
}

/* Fiche d'un payload laissee par Payload Manager : meta.json dans son dossier
   (si elle parle bien de ce fichier) ou <fichier>.json a cote. */
static void payload_meta(App *a, const char *dir, const char *file) {
  char p[PATH_LEN];
  json_t *j = NULL;
  snprintf(p, sizeof p, "%s/meta.json", dir);
  json_t *m = json_load_file(p, 0, NULL);
  const char *fn = json_string_value(json_object_get(m, "filename"));
  if (m && fn && !strcmp(fn, file)) j = m; else json_decref(m);
  if (!j) { snprintf(p, sizeof p, "%s/%s.json", dir, file); j = json_load_file(p, 0, NULL); }
  if (!json_is_object(j)) { json_decref(j); payload_icon(a, file, NULL); return; }
  const char *name = json_string_value(json_object_get(j, "name"));
  const char *desc = json_string_value(json_object_get(j, "description"));
  const char *cat = json_string_value(json_object_get(j, "category"));
  const char *ver = json_string_value(json_object_get(j, "version"));
  const char *pid = json_string_value(json_object_get(j, "id"));
  payload_icon(a, file, pid ? pid : name);
  if (name && *name) snprintf(a->title, sizeof a->title, "%s", name);
  if (desc) snprintf(a->desc, sizeof a->desc, "%s", desc);
  if (cat) snprintf(a->cat, sizeof a->cat, "%s", cat);
  if (ver) snprintf(a->ver, sizeof a->ver, "%s", ver);
  json_decref(j);
}

/* Payloads : /data/payloads, puis les dossiers des gestionnaires courants (Payload
   Manager, etaHEN, autoloader), en lecture seule. Seuls les .elf sont gardes (le
   chargeur du port 9021 n'accepte que ce format) ; Launcher5 lui-meme est ignore. */
static int seen_payload(Menu *m, const char *file) {
  for (int i = 0; i < m->napps; i++) {
    const App *x = &m->apps[i];
    if (x->kind != KIND_PAYLOAD) continue;
    const char *b = strrchr(x->path, '/');
    if (b && !strcasecmp(b + 1, file)) return 1;
  }
  return 0;
}

static void scan_payload_dir(Menu *m, const char *dir, int depth) {
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char p[PATH_LEN];
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    struct stat st;
    if (stat(p, &st)) continue;
    if (S_ISDIR(st.st_mode)) { if (depth > 0) scan_payload_dir(m, p, depth - 1); continue; }
    size_t l = strlen(e->d_name);
    if (l < 5 || strcasecmp(e->d_name + l - 4, ".elf")) continue;
    char low[256];
    size_t k = 0;
    for (; e->d_name[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)e->d_name[k]);
    low[k] = 0;
    if (strstr(low, "launcher5")) continue;   /* Launcher5 lui-meme : pas dans la liste */
    if (seen_payload(m, e->d_name)) continue;    /* meme fichier deja trouve ailleurs */
    App a = {0};
    snprintf(a.id, sizeof a.id, "elf:%.35s", e->d_name);
    snprintf(a.title, sizeof a.title, "%.*s", (int)(l - 4), e->d_name);
    snprintf(a.path, sizeof a.path, "%s", p);
    a.kind = KIND_PAYLOAD;
    payload_meta(&a, dir, e->d_name);
    if (payload_hidden(e->d_name) || payload_hidden(a.title)) continue;   /* inutile dans un launcher */
    {                                            /* icone perso : image du meme nom a cote du payload */
      static const char *const EXT[] = { "png", "jpg", "jpeg", "webp" };
      char img[PATH_LEN];
      for (size_t x = 0; x < sizeof EXT / sizeof EXT[0]; x++) {
        snprintf(img, sizeof img, "%.*s.%s", (int)(strlen(p) - 4), p, EXT[x]);
        if (exists(img)) { snprintf(a.icon, sizeof a.icon, "%s", img); break; }
      }
    }
    /* meme nom deja trouve : ignore, sauf la copie de Launcher5 (/data/payloads), qui l'emporte */
    char mine[PATH_LEN];
    P(mine, sizeof mine, "/data/payloads/", NULL, NULL);
    int dup = 0;
    if (strncmp(p, mine, strlen(mine)))
      for (int i = 0; i < m->napps && !dup; i++)
        dup = m->apps[i].kind == KIND_PAYLOAD && !strcasecmp(m->apps[i].title, a.title);
    if (dup) continue;
    menu_add_app(m, &a);
  }
  closedir(d);
}

/* Payloads exclus : exploits de l'hyperviseur qui demandent le mode repos (byepervisor,
   HyperHEN, chargeur Linux). Ils n'ont rien a faire dans un launcher, ni au demarrage. */
int payload_excluded(const char *name) {
  static const char *const K[] = { "byepervisor", "hyperhen", "linuxloader", "ps5linux" };
  char n[256];
  size_t k = 0;
  for (const char *p = name ? name : ""; *p && k + 1 < sizeof n; p++)
    if (isalnum((unsigned char)*p)) n[k++] = (char)tolower((unsigned char)*p);
  n[k] = 0;
  for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) if (strstr(n, K[i])) return 1;
  return 0;
}

/* Payloads sans interet pour un launcher (outils de developpement, debogage, statistiques,
   habillage de l'accueil, autre gestionnaire de payloads) : ni dans la section, ni dans le
   catalogue. Restent : jailbreak, monteurs de jeux, FTP et envoi de jeux, fichiers,
   sauvegardes, triche, correctifs, heure, DNS. */
int payload_hidden(const char *name) {
  static const char *const K[] = { "klogsrv", "gdbsrv", "shsrv", "ps5debug", "ps5exporter",
                                   "liveshell", "welcomehub", "pldmgr" };
  char n[256];
  size_t k = 0;
  for (const char *p = name ? name : ""; *p && k + 1 < sizeof n; p++)
    if (isalnum((unsigned char)*p)) n[k++] = (char)tolower((unsigned char)*p);
  n[k] = 0;
  for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) if (strstr(n, K[i])) return 1;
  return 0;
}

static void scan_payloads(Menu *m) {
  static const struct { const char *dir; int depth; } DIRS[] = {
    { "/data/payloads", 1 }, { "/data/pldmgr/payloads", 3 }, { "/data/etaHEN/payloads", 1 }, { "/data/ps5_autoloader", 0 },
  };
  for (size_t i = 0; i < sizeof DIRS / sizeof DIRS[0]; i++) {
    char dir[PATH_LEN];
    P(dir, sizeof dir, DIRS[i].dir, NULL, NULL);
    scan_payload_dir(m, dir, DIRS[i].depth);
  }
}

static void add_system(Menu *m, const char *id, const char *title) {
  App a = {0};
  snprintf(a.id, sizeof a.id, "%s", id);
  snprintf(a.title, sizeof a.title, "%s", title);
  a.kind = KIND_SYSTEM;
  menu_add_app(m, &a);
}

void scan_all(Menu *m) {
  /* section Parametres du launcher (les titres sont mis a jour par l'interface) */
  add_system(m, "cfg:theme", "Thème");
  add_system(m, "cfg:view", "Affichage");
  add_system(m, "cfg:saver", "Économiseur d'écran");
  add_system(m, "cfg:return", "Retour automatique");
  add_system(m, "cfg:folders", "Mes dossiers");
  add_system(m, "cfg:hidden", "Éléments masqués");
  add_system(m, "sys:home", "Quitter vers l'accueil PS5");
  scan_installed(m);
  scan_homebrew(m);
  scan_payloads(m);
}
