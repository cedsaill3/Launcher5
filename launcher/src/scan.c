/*
 * Launcher5 - recherche du contenu installe sur la console.
 *
 *   /user/app/<ID>/sce_sys/param.json   jeux et applications PS5
 *   /user/app/<ID>/sce_sys/param.sfo    jeux PS4
 *   /user/appmeta/<ID>/icon0.png        jaquettes
 *   /data/homebrew/<nom>/               homebrews
 *   /data/payloads/(nom).elf            payloads
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void scan_payloads(Menu *m) {
  char dir[PATH_LEN];
  P(dir, sizeof dir, "/data/payloads", NULL, NULL);
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d))) {
    size_t l = strlen(e->d_name);
    if (l < 5 || strcasecmp(e->d_name + l - 4, ".elf")) continue;
    App a = {0};
    snprintf(a.id, sizeof a.id, "elf:%.35s", e->d_name);
    snprintf(a.title, sizeof a.title, "%.*s", (int)(l - 4), e->d_name);
    P(a.path, sizeof a.path, "/data/payloads/%s", e->d_name, NULL);
    a.kind = KIND_PAYLOAD;
    menu_add_app(m, &a);
  }
  closedir(d);
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
