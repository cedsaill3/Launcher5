/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Launcher5 (c) 2026 cedsaill
 * Chargeurs ELF et installation de l'application (gpl/, websrv) : (c) 2024 John Tornblom
 *
 * Launcher5 - bootstrap (le fichier a envoyer a la PS5).
 *
 *  1. installe la tuile « Launcher5 » (LNCH00005) sur l'accueil de la PS5
 *     si elle manque ou a ete supprimee (meme methode que websrv) ;
 *  2. lit la base des applications de la console (copie en lecture seule)
 *     et ecrit un catalogue classe : jeux PS5, PS4, medias, homebrews ;
 *  3. lance l'interface (integree a ce fichier) dans l'application relais ;
 *  4. reste en arriere-plan : execute les demandes de lancement, puis
 *     relance l'interface quand le jeu est ferme (retour automatique).
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>
#include <ps5/kernel.h>

#include "../gpl/hbldr.h"
#include "i18n.h"
#include "version.h"

#define DATA_DIR      "/data/launcher5"
#define UI_PATH       DATA_DIR "/launcher5-ui.elf"
#define REQUEST_PATH  DATA_DIR "/request"
#define LOG_PATH      DATA_DIR "/launcher.log"
#define CATALOG_PATH  DATA_DIR "/apps.json"
#define DB_COPY       DATA_DIR "/appinfo.db"
#define PID_PATH      DATA_DIR "/bootstrap.pid"
#define NO_RETURN     DATA_DIR "/no_auto_return"
#define SESSIONS_PATH DATA_DIR "/sessions.txt"
#define APPINFO_DB    "/system_data/priv/mms/appinfo.db"
#define RELAY_ID      "LNCH00005"      /* tuile « Launcher5 » sur l'accueil de la PS5 */

/* Fichiers integres au moment de la compilation */
#define INCBIN(name, file)                                   \
  __asm__(".section .rodata\n.global " #name "\n.global " #name "_end\n" \
          ".align 16\n" #name ":\n.incbin \"" file "\"\n" #name "_end:\n.previous\n"); \
  extern const uint8_t name[], name##_end[];

INCBIN(ui_elf, "build/launcher5-ui.elf")
INCBIN(fake_param, "assets/relay/param.json")
INCBIN(fake_icon, "assets/relay/icon0.png")
/* fonds d'ecran fournis avec Launcher5 */
INCBIN(wall1, "assets/fonds/launcher5-01.jpg")
INCBIN(wall2, "assets/fonds/launcher5-02.jpg")
INCBIN(wall3, "assets/fonds/launcher5-03.jpg")
INCBIN(wall4, "assets/fonds/launcher5-04.jpg")
INCBIN(wall5, "assets/fonds/launcher5-05.jpg")
INCBIN(wall6, "assets/fonds/launcher5-06.jpg")
INCBIN(wall7, "assets/fonds/launcher5-07.jpg")

typedef struct app_launch_ctx {
  uint32_t structsize;
  uint32_t user_id;
  uint32_t app_opt;
  uint64_t crash_report;
  uint32_t check_flag;
} app_launch_ctx_t;

int  sceUserServiceInitialize(void *);
int  sceUserServiceGetForegroundUser(uint32_t *user_id);
void sceUserServiceTerminate(void);
int  sceSystemServiceGetAppIdOfRunningBigApp(void);
int  sceSystemServiceKillApp(int app_id, int how, int reason, int core_dump);
int  sceSystemServiceLaunchApp(const char *title_id, char **argv, app_launch_ctx_t *ctx);
int  sceAppInstUtilInitialize(void);
int  sceAppInstUtilAppInstallAll(void *);
int  sceKernelSendNotificationRequest(int, void *, size_t, int);
int  sceSystemServiceGetAppTitleId(int app_id, char *title_id);
int  sceSystemServiceParamGetInt(int id, int *value);

static void logf(const char *fmt, ...) {
  FILE *f = fopen(LOG_PATH, "a");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  fprintf(f, "[bootstrap] ");
  vfprintf(f, fmt, ap);
  fprintf(f, "\n");
  va_end(ap);
  fclose(f);
}

static void notify(const char *msg) {
  struct { char pad[45]; char message[3075]; } req;
  memset(&req, 0, sizeof req);
  snprintf(req.message, sizeof req.message, "%s", tr(msg));
  sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
  logf("%s", msg);
}

static int write_file(const char *path, const uint8_t *data, size_t size, int mode);

/* Fonds d'ecran fournis : poses dans fonds/ s'ils manquent (jamais d'ecrasement) */
static void install_wallpapers(void) {
  static const struct { const char *name; const uint8_t *start, *end; } W[] = {
  { "launcher5-01.jpg", wall1, wall1_end },
  { "launcher5-02.jpg", wall2, wall2_end },
  { "launcher5-03.jpg", wall3, wall3_end },
  { "launcher5-04.jpg", wall4, wall4_end },
  { "launcher5-05.jpg", wall5, wall5_end },
  { "launcher5-06.jpg", wall6, wall6_end },
  { "launcher5-07.jpg", wall7, wall7_end }
  };
  char path[256];
  struct stat st;
  mkdir(DATA_DIR "/fonds", 0755);
  for (size_t i = 0; i < sizeof W / sizeof W[0]; i++) {
    snprintf(path, sizeof path, DATA_DIR "/fonds/%s", W[i].name);
    if (stat(path, &st) && write_file(path, W[i].start, (size_t)(W[i].end - W[i].start), 0644))
      logf("fond d'ecran non installe : %s", W[i].name);
  }
}

static int write_file(const char *path, const uint8_t *data, size_t size, int mode) {
  int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, mode);
  if (fd < 0) return -1;
  size_t off = 0;
  while (off < size) {
    ssize_t w = write(fd, data + off, size - off);
    if (w <= 0) { close(fd); return -1; }
    off += (size_t)w;
  }
  close(fd);
  return 0;
}

static int copy_file(const char *src, const char *dst) {
  int in = open(src, O_RDONLY);
  if (in < 0) return -1;
  int out = open(dst, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (out < 0) { close(in); return -1; }
  char buf[65536];
  ssize_t n;
  int rc = 0;
  while ((n = read(in, buf, sizeof buf)) > 0)
    if (write(out, buf, n) != n) { rc = -1; break; }
  close(in);
  close(out);
  return rc;
}

static int elevate_self(void) {
  static const uint8_t caps[16] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  };
  if (kernel_set_ucred_authid(-1, 0x4801000000000013L)) return -1;
  if (kernel_set_ucred_uid(-1, 0)) return -1;
  if (kernel_set_ucred_caps(-1, caps)) return -1;
  return 0;
}

/* ------------------------------------------------------------------ */
/* Application relais (repris de websrv)                               */
/* ------------------------------------------------------------------ */
static int install_app(const char *title_id, const char *dir) {
  int (*install_title_dir)(const char *, const char *, void *) = 0;
  uint32_t handle;
  if (!kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &handle))
    install_title_dir = (void *)kernel_dynlib_resolve(-1, handle, "Wudg3Xe3heE");
  if (install_title_dir) return install_title_dir(title_id, dir, 0);
  return sceAppInstUtilAppInstallAll(0);
}

/* La tuile est-elle enregistree dans la base des applications ? 1 oui, 0 non, -1 on ne sait pas */
static int relay_registered(void) {
  if (copy_file(APPINFO_DB, DB_COPY)) return -1;
  sqlite3 *db;
  if (sqlite3_open_v2(DB_COPY, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) { sqlite3_close(db); return -1; }
  sqlite3_stmt *st;
  int found = -1;
  if (sqlite3_prepare_v2(db, "select 1 from tbl_appinfo where titleId = '" RELAY_ID "' limit 1", -1, &st, NULL) == SQLITE_OK) {
    found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
  }
  sqlite3_close(db);
  return found;
}

/* Tuile « Launcher5 » sur l'accueil de la PS5 : installee si elle manque,
   reinstallee si elle a ete supprimee de l'accueil */
static int install_relay_app(void) {
  struct stat st;
  int files = !stat("/user/app/" RELAY_ID "/sce_sys/param.json", &st) || !stat("/user/appmeta/" RELAY_ID "/param.json", &st);
  int reg = relay_registered();
  if (files && reg != 0) return 0;   /* deja installee */
  logf("installation de la tuile Launcher5 (" RELAY_ID ")");
  if (mkdir("/user/app/" RELAY_ID, 0755) && errno != EEXIST) return -1;
  if (mkdir("/user/app/" RELAY_ID "/sce_sys", 0755) && errno != EEXIST) return -1;
  if (write_file("/user/app/" RELAY_ID "/sce_sys/param.json", fake_param, fake_param_end - fake_param, 0644)) return -1;
  if (write_file("/user/app/" RELAY_ID "/sce_sys/icon0.png", fake_icon, fake_icon_end - fake_icon, 0644)) return -1;
  sceAppInstUtilInitialize();
  return install_app(RELAY_ID, "/user/app/");
}

/* ------------------------------------------------------------------ */
/* Catalogue des applications (base appinfo.db de la console)          */
/* ------------------------------------------------------------------ */
typedef struct {
  char id[16], title[128], title_loc[128], cid[64], category[16];
  int app_type, cat_type, ps4;
} Rec;

static Rec *recs;
static int nrecs, caprecs;

static Rec *rec_get(const char *id) {
  for (int i = 0; i < nrecs; i++) if (!strcmp(recs[i].id, id)) return &recs[i];
  if (nrecs == caprecs) { caprecs = caprecs ? caprecs * 2 : 128; recs = realloc(recs, sizeof(Rec) * caprecs); }
  Rec *r = &recs[nrecs++];
  memset(r, 0, sizeof *r);
  snprintf(r->id, sizeof r->id, "%s", id);
  return r;
}

static int has_word(const char *title, const char *const *words) {
  char low[256];
  size_t i = 0;
  for (; title[i] && i < sizeof low - 1; i++) low[i] = (char)tolower((unsigned char)title[i]);
  low[i] = 0;
  for (int w = 0; words[w]; w++) if (strstr(low, words[w])) return 1;
  return 0;
}

static const char *const MEDIA_WORDS[] = {
  "tv", "radio", "video", "vidéo", "music", "musique", "player", "youtube", "netflix", "vlc", "jelly", "plex",
  "kodi", "stream", "twitch", "spotify", "disney", "prime", "crunchyroll", "ciné", "cine", "movie", "media", "cd ", NULL
};
static const char *const EMU_WORDS[] = {
  "emu", "pcsx", "ps5sx2", "dolphin", "porpoise", "retroarch", "eden", "switch", "xps", "mame", "fbneo", "snes",
  "nes", "gba", "genesis", "duck", "ppsspp", "mednafen", "scumm", "ryujinx", "citra", "cemu", NULL
};

static void json_str(FILE *f, const char *s) {
  fputc('"', f);
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') fprintf(f, "\\%c", c);
    else if (c < 0x20) fprintf(f, "\\u%04x", c);
    else fputc(c, f);
  }
  fputc('"', f);
}

static int export_catalog(void) {
  if (copy_file(APPINFO_DB, DB_COPY)) { logf("copie de appinfo.db impossible"); return -1; }
  sqlite3 *db;
  if (sqlite3_open_v2(DB_COPY, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) { logf("ouverture appinfo.db impossible"); return -1; }
  sqlite3_stmt *st;
  const char *q = "select titleId, key, val from tbl_appinfo where key in "
                  "('TITLE',?1,'CONTENT_ID','APP_TYPE','CATEGORY_TYPE','CATEGORY','_ps_platform')";
  if (sqlite3_prepare_v2(db, q, -1, &st, NULL) != SQLITE_OK) { sqlite3_close(db); return -1; }
  sqlite3_bind_text(st, 1, i18n_title_key(), -1, SQLITE_STATIC);   /* nom dans la langue de la console */
  nrecs = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(st, 0);
    const char *k = (const char *)sqlite3_column_text(st, 1);
    const char *v = (const char *)sqlite3_column_text(st, 2);
    if (!id || !k) continue;
    if (!v) v = "";
    Rec *r = rec_get(id);
    if (!strcmp(k, "TITLE")) snprintf(r->title, sizeof r->title, "%s", v);
    else if (!strcmp(k, i18n_title_key())) snprintf(r->title_loc, sizeof r->title_loc, "%s", v);
    else if (!strcmp(k, "CONTENT_ID")) snprintf(r->cid, sizeof r->cid, "%s", v);
    else if (!strcmp(k, "CATEGORY")) snprintf(r->category, sizeof r->category, "%s", v);
    else if (!strcmp(k, "APP_TYPE")) r->app_type = atoi(v);
    else if (!strcmp(k, "CATEGORY_TYPE")) r->cat_type = atoi(v);
    else if (!strcmp(k, "_ps_platform")) r->ps4 = atoi(v) == 1;
  }
  sqlite3_finalize(st);
  sqlite3_close(db);

  FILE *f = fopen(CATALOG_PATH ".tmp", "w");
  if (!f) return -1;
  fprintf(f, "[\n");
  int first = 1, count = 0;
  for (int i = 0; i < nrecs; i++) {
    Rec *r = &recs[i];
    if (!r->title[0] || !strncmp(r->id, "NPXS", 4) || !strcmp(r->id, "FAKE00000") || !strcmp(r->id, RELAY_ID)) continue;
    char pub[8] = "";
    snprintf(pub, sizeof pub, "%.6s", r->cid);
    int homebrew = !r->cid[0] || !strcmp(r->cid, "0") || !strcmp(pub, "IV9999") ||
                   (!strcmp(pub, "UP9000") && r->app_type == 5);
    int ps4 = r->ps4 || !strncmp(r->category, "gd", 2) || !strncmp(r->id, "CUSA", 4);
    const char *kind = homebrew ? "homebrew" : ps4 ? "ps4" : (r->app_type == 1 || r->cat_type == 0) ? "ps5" : "media";
    const char *title = r->title_loc[0] ? r->title_loc : r->title;
    int media = !strcmp(kind, "media") || (homebrew && has_word(title, MEDIA_WORDS));
    int emu = homebrew && has_word(title, EMU_WORDS);
    fprintf(f, "%s {\"id\": ", first ? "" : ",\n");
    json_str(f, r->id);
    fprintf(f, ", \"title\": ");
    json_str(f, title);
    fprintf(f, ", \"kind\": \"%s\", \"media\": %s, \"emu\": %s}", kind, media ? "true" : "false", emu ? "true" : "false");
    first = 0;
    count++;
  }
  fprintf(f, "\n]\n");
  fclose(f);
  rename(CATALOG_PATH ".tmp", CATALOG_PATH);
  logf("catalogue : %d applications", count);
  return 0;
}

/* ------------------------------------------------------------------ */
/* Lancements                                                          */
/* ------------------------------------------------------------------ */
static int launch_title(const char *title_id) {
  app_launch_ctx_t ctx = { 0 };
  char *argv[] = { NULL };
  int app_id, err;
  if ((err = sceUserServiceGetForegroundUser(&ctx.user_id))) return err;
  if ((app_id = sceSystemServiceGetAppIdOfRunningBigApp()) > 0) {
    sceSystemServiceKillApp(app_id, -1, 0, 0);
    usleep(300 * 1000);
  }
  ctx.structsize = sizeof ctx;
  err = sceSystemServiceLaunchApp(title_id, argv, &ctx);
  logf("lancement de %s : %d", title_id, err);
  return err < 0 ? err : 0;
}

static int read_request(char *type, char *value, char *cwd, size_t n) {
  FILE *f = fopen(REQUEST_PATH, "r");
  if (!f) return -1;
  char *bufs[3] = { type, value, cwd };
  for (int i = 0; i < 3; i++) {
    if (!fgets(bufs[i], (int)n, f)) bufs[i][0] = 0;
    bufs[i][strcspn(bufs[i], "\r\n")] = 0;
  }
  fclose(f);
  remove(REQUEST_PATH);
  return type[0] && value[0] ? 0 : -1;
}

static int alive(pid_t pid) { return pid > 0 && (kill(pid, 0) == 0 || errno != ESRCH); }

static pid_t start_ui(void) {
  export_catalog();                      /* a jour a chaque ouverture */
  remove(REQUEST_PATH);
  char *argv[] = { (char *)UI_PATH, NULL };
  char *envp[] = { NULL };
  pid_t ui = hbldr_launch(DATA_DIR, UI_PATH, -1, argv, envp);
  logf("interface lancee (pid %d)", ui);
  return ui;
}

/* Un seul bootstrap a la fois : l'ancien s'arrete si on renvoie le launcher */
static void single_instance(void) {
  FILE *f = fopen(PID_PATH, "r");
  if (f) {
    int old = 0;
    if (fscanf(f, "%d", &old) == 1 && old > 0 && old != getpid()) {
      kill(old, SIGKILL);
      logf("ancien bootstrap arrete (pid %d)", old);
    }
    fclose(f);
  }
  f = fopen(PID_PATH, "w");
  if (f) { fprintf(f, "%d\n", getpid()); fclose(f); }
}

int main(void) {
  mkdir(DATA_DIR, 0755);
  logf("--- demarrage --- Launcher5 v" L5_VERSION " par " L5_AUTHOR);
  {
    int lang = 1;
    if (sceSystemServiceParamGetInt(1 /* langue */, &lang) < 0) lang = 1;
    i18n_init(lang);
    logf("langue du systeme : %d (%s)", lang, i18n_locale());
  }
  if (sceUserServiceInitialize(0)) { logf("UserService indisponible"); return EXIT_FAILURE; }
  if (elevate_self()) { notify("Launcher5 : droits insuffisants (jailbreak actif ?)"); return EXIT_FAILURE; }
  single_instance();
  if (install_relay_app()) { notify("Launcher5 : impossible d'installer l'application relais"); return EXIT_FAILURE; }
  install_wallpapers();
  if (write_file(UI_PATH, ui_elf, ui_elf_end - ui_elf, 0755)) {
    notify("Launcher5 : impossible d'écrire l'interface dans /data");
    return EXIT_FAILURE;
  }

  pid_t ui = start_ui();
  if (ui < 0) { notify("Launcher5 : impossible de lancer l'interface"); return EXIT_FAILURE; }

  char type[1024], value[1024], cwd[1024];
  struct stat st;
  for (;;) {
    /* 1) l'interface est ouverte : on attend une demande */
    int launched = 0;
    while (ui > 0 && alive(ui)) {
      if (!stat(REQUEST_PATH, &st) && !read_request(type, value, cwd, sizeof type)) {
        logf("demande : %s %s", type, value);
        if (!strcmp(type, "close")) {
          /* fermeture propre par le systeme : la PS5 revient a son accueil */
          int app = sceSystemServiceGetAppIdOfRunningBigApp();
          logf("fermeture du launcher par le systeme : %d", app > 0 ? sceSystemServiceKillApp(app, -1, 0, 0) : -1);
          break;
        }
        if (!strcmp(type, "title")) {
          if (launch_title(value)) notify("Launcher5 : le lancement a échoué");
          else launched = 1;
        } else if (!strcmp(type, "hb")) {
          char *hb_argv[] = { value, NULL };
          char *envp[] = { NULL };
          if (hbldr_launch(cwd, value, -1, hb_argv, envp) < 0) notify("Launcher5 : le homebrew n'a pas pu démarrer");
          else launched = 1;
        }
        break;
      }
      usleep(100 * 1000);
    }
    ui = -1;

    /* 2) un jeu a ete lance : retour automatique quand il est ferme */
    if (launched) {
      time_t t0 = time(NULL);
      while (sceSystemServiceGetAppIdOfRunningBigApp() <= 0 && time(NULL) - t0 < 60) usleep(250 * 1000);
      time_t started = time(NULL), ended = 0;
      int idle = 0;
      while (idle < 12) {                       /* 3 s sans application ouverte */
        int open = sceSystemServiceGetAppIdOfRunningBigApp() > 0;
        if (!open && !idle) ended = time(NULL);
        idle = open ? 0 : idle + 1;
        usleep(250 * 1000);
      }
      /* temps de jeu : note pour l'interface (jeux et applications lances par leur identifiant) */
      if (!strcmp(type, "title") && ended > started) {
        FILE *f = fopen(SESSIONS_PATH, "a");
        if (f) { fprintf(f, "%s %ld\n", value, (long)(ended - started)); fclose(f); }
        logf("partie terminee : %s, %ld s", value, (long)(ended - started));
      }
      if (stat(NO_RETURN, &st)) {
        logf("jeu ferme : retour au launcher");
        ui = start_ui();
        continue;
      }
    }

    /* 3) en attente : la tuile « Launcher5 » de l'accueil rouvre le launcher */
    logf("en attente de la tuile Launcher5");
    while (ui <= 0) {
      int app = sceSystemServiceGetAppIdOfRunningBigApp();
      if (app > 0) {
        char tid[32] = { 0 };
        sceSystemServiceGetAppTitleId(app, tid);
        if (!strcmp(tid, RELAY_ID)) {
          logf("tuile Launcher5 ouverte : affichage du launcher");
          sceSystemServiceKillApp(app, -1, 0, 0);
          usleep(400 * 1000);
          ui = start_ui();
        }
      }
      usleep(300 * 1000);
    }
  }
  logf("fin du bootstrap");
  remove(PID_PATH);
  sceUserServiceTerminate();
  return EXIT_SUCCESS;
}
