/*
 * Launcher5 - themes : themes fournis, lecture et ecriture des fichiers.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <jansson.h>

#include "theme.h"

#define T(n, a, a2, t, b, bg, ar, gl, p, ps, fa, co, fxv, auv) \
  { .name = n, .accent = a, .accent2 = a2, .top = t, .bottom = b, .bg_mode = bg, .art = ar, \
    .glow = gl, .particles = p, .pstyle = ps, .fall = fa, .corners = co, .clock = CLOCK_24, .volume = 60, \
    .fx = fxv, .auto_color = auv, .sfx = 60 }
/* theme avec un fond d'ecran fourni (assets/fonds, installe avec le launcher) */
#define TW(n, a, a2, t, b, ar, gl, p, ps, fa, co, fxv, auv, img)   { .name = n, .accent = a, .accent2 = a2, .top = t, .bottom = b, .bg_mode = BG_IMAGE, .wallpaper = img, .art = ar,     .glow = gl, .particles = p, .pstyle = ps, .fall = fa, .corners = co, .clock = CLOCK_24, .volume = 60,     .fx = fxv, .auto_color = auv, .sfx = 60 }

static const Theme BUILTIN[] = {
  { .name = "Launcher5", .accent = 0xff4d1a, .accent2 = 0x00a33a, .top = 0x100503, .bottom = 0x2e0b05, .bg_mode = BG_IMAGE,
    .wallpaper = "launcher5-03.jpg", .art = 60, .glow = 100, .particles = PART_NONE, .pstyle = PSTYLE_SPARKS, .fall = 0,
    .corners = CORNER_SOFT, .clock = CLOCK_24, .volume = 60, .fx = FX_WARP, .auto_color = 1, .sfx = 50 },
  TW("Launcher5 Néon", 0x00e5ff, 0xff2bd6, 0x05081a, 0x170a33, 45, 80,  PART_FEW,  PSTYLE_DOTS,   0, CORNER_ROUND, FX_NONE, 0, "launcher5-02.jpg"),
  T("Néon",       0x00e5ff, 0xff2bd6, 0x05081a, 0x170a33, BG_ART,      45, 100, PART_FEW,  PSTYLE_DOTS,   0, CORNER_ROUND, FX_NONE, 0),
  T("Glitch",     0xff00c8, 0x00fff0, 0x07000f, 0x14002a, BG_ART,      40, 100, PART_MANY, PSTYLE_SPARKS, 0, CORNER_SQUARE, FX_WARP, 0),
  T("Acid",       0xc6ff00, 0xff2bd6, 0x050805, 0x101a05, BG_ART,      40, 100, PART_FEW,  PSTYLE_SPARKS, 0, CORNER_SQUARE, FX_NONE, 0),
  T("Blood",      0xff1f3d, 0x8a0012, 0x0a0000, 0x220005, BG_ART,      50, 90,  PART_FEW,  PSTYLE_DOTS,   1, CORNER_SQUARE, FX_NONE, 0),
  T("Terminal",   0x33ff66, 0x00a33a, 0x000000, 0x000a03, BG_GRADIENT, 20, 50,  PART_MANY, PSTYLE_DOTS,   1, CORNER_SQUARE, FX_RAIN, 0),
  T("Outrun",     0xff2bd6, 0x00e5ff, 0x0d0221, 0x2a0845, BG_GRADIENT, 30, 100, PART_FEW,  PSTYLE_SPARKS, 0, CORNER_ROUND, FX_GRID, 0),
  T("Hyperdrive", 0x7fdbff, 0xb26bff, 0x000000, 0x05081a, BG_ART,      35, 90,  PART_NONE, PSTYLE_DOTS,   0, CORNER_ROUND, FX_WARP, 0),
  T("Caméléon",   0x00e5ff, 0xff2bd6, 0x05060a, 0x10121a, BG_ART,      60, 100, PART_FEW,  PSTYLE_DOTS,   0, CORNER_SOFT,  FX_NONE, 1),
  T("Cyberpunk",  0xfcee0a, 0x00f0ff, 0x08080d, 0x1d1608, BG_ART,      45, 100, PART_FEW,  PSTYLE_SPARKS, 0, CORNER_SQUARE, FX_GRID, 0),
  T("Synthwave",  0xff3cac, 0x2b86c5, 0x0d0221, 0x2d0b3f, BG_GRADIENT, 30, 100, PART_MANY, PSTYLE_SPARKS, 0, CORNER_ROUND, FX_GRID, 0),
  T("Inferno",    0xff4d1a, 0xffb300, 0x100503, 0x2e0b05, BG_ART,      50, 100, PART_MANY, PSTYLE_SPARKS, 0, CORNER_ROUND, FX_NONE, 0),
  T("Toxic",      0x39ff14, 0x00ffa3, 0x030d06, 0x0a2214, BG_ART,      45, 90,  PART_FEW,  PSTYLE_DOTS,   0, CORNER_ROUND, FX_NONE, 0),
  T("Matrix",     0x00ff41, 0x008f11, 0x000000, 0x001a07, BG_GRADIENT, 20, 60,  PART_MANY, PSTYLE_DOTS,   1, CORNER_SQUARE, FX_RAIN, 0),
  T("Royal",      0xb26bff, 0xffd166, 0x0b0517, 0x221038, BG_ART,      45, 100, PART_FEW,  PSTYLE_DOTS,   0, CORNER_SOFT, FX_NONE, 0),
  T("Glace",      0x7fdbff, 0xe8f7ff, 0x05101a, 0x10293d, BG_ART,      45, 80,  PART_MANY, PSTYLE_SNOW,   1, CORNER_SOFT, FX_NONE, 0),
  T("Sakura",     0xff8fb8, 0xffd1e0, 0x1a0710, 0x3a1424, BG_ART,      40, 80,  PART_MANY, PSTYLE_SNOW,   1, CORNER_SOFT, FX_NONE, 0),
  T("Océan",      0x00b4d8, 0x48cae4, 0x020c1b, 0x03254a, BG_ART,      50, 80,  PART_FEW,  PSTYLE_SNOW,   0, CORNER_ROUND, FX_NONE, 0),
  T("Noir et or", 0xd4af37, 0xfff1c1, 0x050505, 0x15120a, BG_ART,      35, 60,  PART_FEW,  PSTYLE_SPARKS, 0, CORNER_SQUARE, FX_NONE, 0),
  T("Minuit",     0xc9d1dc, 0x5b6b82, 0x06080c, 0x10141c, BG_ART,      55, 30,  PART_NONE, PSTYLE_DOTS,   0, CORNER_ROUND, FX_NONE, 0),
  T("Cinéma",     0xe50914, 0xffffff, 0x000000, 0x0b0b0b, BG_ART,      75, 40,  PART_NONE, PSTYLE_DOTS,   0, CORNER_ROUND, FX_NONE, 0),
};
#define NBUILTIN (int)(sizeof BUILTIN / sizeof BUILTIN[0])

int theme_builtin_count(void) { return NBUILTIN; }
const Theme *theme_builtin(int i) { return &BUILTIN[i < 0 || i >= NBUILTIN ? 0 : i]; }

/* --- valeurs en clair dans le fichier --------------------------------- */
static const char *BG_KEYS[] = { "illustration", "degrade", "image" };
static const char *PART_KEYS[] = { "aucune", "peu", "beaucoup" };
static const char *PSTYLE_KEYS[] = { "points", "etincelles", "flocons" };
static const char *CORNER_KEYS[] = { "droits", "arrondis", "tres_arrondis" };
static const char *CLOCK_KEYS[] = { "24h", "12h", "masquee" };
static const char *FX_KEYS[] = { "aucun", "grille", "hyperespace", "pluie" };
static const char *CASE_KEYS[] = { "bleu", "noir", "cristal", "steelbook", "couleur", "aucun" };

static int key_index(json_t *v, const char **keys, int n, int def) {
  const char *s = json_string_value(v);
  if (!s) return def;
  for (int i = 0; i < n; i++) if (!strcasecmp(s, keys[i])) return i;
  return def;
}

static uint32_t color_of(json_t *v, uint32_t def) {
  const char *s = json_string_value(v);
  if (!s) return def;
  if (*s == '#') s++;
  char *end;
  unsigned long c = strtoul(s, &end, 16);
  return end != s && strlen(s) == 6 ? (uint32_t)c : def;
}

static int num_of(json_t *v, int def, int lo, int hi) {
  if (!json_is_integer(v)) return def;
  int x = (int)json_integer_value(v);
  return x < lo ? lo : x > hi ? hi : x;
}

static void str_of(json_t *v, char *out, size_t n) {
  const char *s = json_string_value(v);
  if (!s) return;
  /* un simple nom de fichier : pas de dossier */
  const char *slash = strrchr(s, '/');
  snprintf(out, n, "%s", (slash && strcmp(s, "*")) ? slash + 1 : s);
}

int theme_load(const char *path, Theme *t) {
  json_error_t err;
  json_t *j = json_load_file(path, 0, &err);
  if (!j || !json_is_object(j)) { if (j) json_decref(j); return -1; }
  Theme d = BUILTIN[0];
  const char *n = json_string_value(json_object_get(j, "nom"));
  snprintf(d.name, sizeof d.name, "%s", n && *n ? n : "Sans nom");
  d.accent = color_of(json_object_get(j, "couleur"), d.accent);
  d.accent2 = color_of(json_object_get(j, "couleur2"), d.accent2);
  d.top = color_of(json_object_get(j, "fond_haut"), d.top);
  d.bottom = color_of(json_object_get(j, "fond_bas"), d.bottom);
  d.bg_mode = key_index(json_object_get(j, "arriere_plan"), BG_KEYS, BG_COUNT, d.bg_mode);
  d.wallpaper[0] = 0;
  str_of(json_object_get(j, "image"), d.wallpaper, sizeof d.wallpaper);
  d.art = num_of(json_object_get(j, "illustration"), d.art, 0, 100);
  d.glow = num_of(json_object_get(j, "halos"), d.glow, 0, 100);
  d.particles = key_index(json_object_get(j, "particules"), PART_KEYS, PART_COUNT, d.particles);
  d.pstyle = key_index(json_object_get(j, "forme_particules"), PSTYLE_KEYS, PSTYLE_COUNT, d.pstyle);
  d.fall = json_is_string(json_object_get(j, "sens")) && !strcasecmp(json_string_value(json_object_get(j, "sens")), "tombent");
  d.corners = key_index(json_object_get(j, "coins"), CORNER_KEYS, CORNER_COUNT, d.corners);
  d.clock = key_index(json_object_get(j, "horloge"), CLOCK_KEYS, CLOCK_COUNT, d.clock);
  d.music[0] = 0;
  str_of(json_object_get(j, "musique"), d.music, sizeof d.music);
  d.volume = num_of(json_object_get(j, "volume"), d.volume, 0, 100);
  /* V2 : absents des anciens fichiers -> valeurs neutres */
  d.fx = key_index(json_object_get(j, "effet"), FX_KEYS, FX_COUNT, FX_NONE);
  d.auto_color = json_is_true(json_object_get(j, "couleur_auto"));
  d.sfx = num_of(json_object_get(j, "sons"), 60, 0, 100);
  d.case_style = key_index(json_object_get(j, "boitier"), CASE_KEYS, CASE_COUNT, CASE_BLUE);
  json_decref(j);
  *t = d;
  return 0;
}

int theme_save(const char *path, const Theme *t) {
  char c[4][8];
  snprintf(c[0], 8, "#%06x", t->accent & 0xffffff);
  snprintf(c[1], 8, "#%06x", t->accent2 & 0xffffff);
  snprintf(c[2], 8, "#%06x", t->top & 0xffffff);
  snprintf(c[3], 8, "#%06x", t->bottom & 0xffffff);
  json_t *j = json_object();
  json_object_set_new(j, "nom", json_string(t->name));
  json_object_set_new(j, "couleur", json_string(c[0]));
  json_object_set_new(j, "couleur2", json_string(c[1]));
  json_object_set_new(j, "fond_haut", json_string(c[2]));
  json_object_set_new(j, "fond_bas", json_string(c[3]));
  json_object_set_new(j, "arriere_plan", json_string(BG_KEYS[t->bg_mode]));
  json_object_set_new(j, "image", json_string(t->wallpaper));
  json_object_set_new(j, "illustration", json_integer(t->art));
  json_object_set_new(j, "halos", json_integer(t->glow));
  json_object_set_new(j, "particules", json_string(PART_KEYS[t->particles]));
  json_object_set_new(j, "forme_particules", json_string(PSTYLE_KEYS[t->pstyle]));
  json_object_set_new(j, "sens", json_string(t->fall ? "tombent" : "montent"));
  json_object_set_new(j, "coins", json_string(CORNER_KEYS[t->corners]));
  json_object_set_new(j, "horloge", json_string(CLOCK_KEYS[t->clock]));
  json_object_set_new(j, "musique", json_string(t->music));
  json_object_set_new(j, "volume", json_integer(t->volume));
  json_object_set_new(j, "effet", json_string(FX_KEYS[t->fx >= 0 && t->fx < FX_COUNT ? t->fx : 0]));
  json_object_set_new(j, "couleur_auto", json_boolean(t->auto_color));
  json_object_set_new(j, "sons", json_integer(t->sfx));
  json_object_set_new(j, "boitier", json_string(CASE_KEYS[t->case_style >= 0 && t->case_style < CASE_COUNT ? t->case_style : 0]));
  char tmp[600];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  int rc = json_dump_file(j, tmp, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
  json_decref(j);
  if (rc) return -1;
  return rename(tmp, path);
}

static int cmp_files(const void *a, const void *b) { return strcasecmp(a, b); }

int theme_list_files(const char *dir, const char *const *exts, char (**out)[THEME_FILE_LEN]) {
  *out = NULL;
  DIR *d = opendir(dir);
  if (!d) return 0;
  int n = 0, cap = 0;
  struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    const char *dot = strrchr(e->d_name, '.');
    if (!dot || strlen(e->d_name) >= THEME_FILE_LEN) continue;
    int ok = 0;
    for (int i = 0; exts[i]; i++) if (!strcasecmp(dot + 1, exts[i])) ok = 1;
    if (!ok) continue;
    if (n == cap) {
      cap = cap ? cap * 2 : 16;
      *out = realloc(*out, sizeof(**out) * cap);
    }
    snprintf((*out)[n++], THEME_FILE_LEN, "%s", e->d_name);
  }
  closedir(d);
  if (n) qsort(*out, n, sizeof(**out), cmp_files);
  return n;
}

void theme_file_name(const char *name, char *out, int n) {
  int k = 0;
  for (const unsigned char *s = (const unsigned char *)name; *s && k < n - 6; s++) {
    if (isalnum(*s) || *s == '-' || *s == '_') out[k++] = (char)*s;
    else if (*s == ' ' && k && out[k - 1] != '_') out[k++] = '_';
    else if (*s >= 0xC0) {                        /* lettre accentuee : copiee entiere ou pas du tout */
      int len = *s >= 0xF0 ? 4 : *s >= 0xE0 ? 3 : 2, ok = k + len < n - 6;
      for (int i = 1; i < len && ok; i++) ok = (s[i] & 0xC0) == 0x80;
      if (!ok) break;
      for (int i = 0; i < len; i++) out[k++] = (char)s[i];
      s += len - 1;
    }
  }
  if (!k) { snprintf(out, n, "theme.json"); return; }
  snprintf(out + k, n - k, ".json");
}
