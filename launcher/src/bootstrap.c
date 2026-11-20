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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>
#include <ps5/kernel.h>

#include "../gpl/hbldr.h"
#include "../gpl/elfldr.h"
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
INCBIN(wall8, "assets/fonds/launcher5-crane.jpg")
/* images de boitiers PS5 / PS4 (style « Images ») */
INCBIN(case_ps5, "assets/boitiers/ps5.png")
INCBIN(case_ps4, "assets/boitiers/ps4.png")
/* musique d'ambiance fournie (libre de droits) */
INCBIN(music1, "assets/musique/phonk-menu.mp3")
#ifdef L5_HAVE_GPU
/* interface dessinee par la carte graphique : une vraie application, installee sous l'identifiant
   de la tuile Launcher5 (une seule tuile sur l'accueil). LNCH00006 : ancienne tuile GPU, retiree. */
#define GPU_ID        "LNCH00005"
#define GPU_BUILD     "build/gpu"
#define OLD_GPU_ID    "LNCH00006"
#define GPU_DIR       "/system_ex/app/" GPU_ID
#define GPU_OK        DATA_DIR "/cache/gpu_ok"       /* ecrit par l'interface GPU apres ses premieres images */
#define GPU_FAILED    DATA_DIR "/cache/gpu_failed"   /* version ou l'interface GPU n'a pas demarre */
INCBIN(gpu_eboot, GPU_BUILD "/eboot.bin")
INCBIN(gpu_libc, GPU_BUILD "/sce_module/libc.prx")
INCBIN(gpu_param, GPU_BUILD "/sce_sys/param.json")
#endif

/* Plugin OnionHEN : description lue dans la section « .onion_plugin » (128 octets).
   Pose dans /data/OnionHEN/plugins/, le launcher est lance par OnionHEN au demarrage
   et apparait dans la page Plugins de sa Toolbox. Sans effet pour les autres chargeurs. */
struct onion_plugin_desc {
  uint32_t struct_size, abi_version, capabilities, flags;   /* flags : 2 = reste en marche */
  char id[32], version[16], name[64];
};
_Static_assert(sizeof(struct onion_plugin_desc) == 128, "description OnionHEN : 128 octets");
__attribute__((section(".onion_plugin"), used, aligned(8)))
static const struct onion_plugin_desc ONION_PLUGIN = { 128, 1, 0, 2, "LNCH00005", L5_PLUGIN_VERSION, "Launcher5" };

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
int  sceAppInstUtilAppUnInstall(const char *title_id);
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
  { "launcher5-07.jpg", wall7, wall7_end },
  { "launcher5-crane.jpg", wall8, wall8_end }
  };
  char path[256];
  struct stat st;
  mkdir(DATA_DIR "/fonds", 0755);
  for (size_t i = 0; i < sizeof W / sizeof W[0]; i++) {
    snprintf(path, sizeof path, DATA_DIR "/fonds/%s", W[i].name);
    if (stat(path, &st) && write_file(path, W[i].start, (size_t)(W[i].end - W[i].start), 0644))
      logf("fond d'ecran non installe : %s", W[i].name);
  }
  mkdir(DATA_DIR "/boitiers", 0755);             /* les images deja la (perso) sont gardees */
  if (stat(DATA_DIR "/boitiers/ps5.png", &st) && write_file(DATA_DIR "/boitiers/ps5.png", case_ps5, (size_t)(case_ps5_end - case_ps5), 0644))
    logf("boitier ps5 non installe");
  if (stat(DATA_DIR "/boitiers/ps4.png", &st) && write_file(DATA_DIR "/boitiers/ps4.png", case_ps4, (size_t)(case_ps4_end - case_ps4), 0644))
    logf("boitier ps4 non installe");
  mkdir(DATA_DIR "/musique", 0755);              /* musique fournie : posee si elle manque */
  if (stat(DATA_DIR "/musique/phonk-menu.mp3", &st) && write_file(DATA_DIR "/musique/phonk-menu.mp3", music1, (size_t)(music1_end - music1), 0644))
    logf("musique non installee");
  remove(DATA_DIR "/sons/demarrage-techno.mp3");  /* son retire en 2.1.2 */
  rmdir(DATA_DIR "/sons");
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

/* copie de la base des applications, avec son journal (WAL) s'il y en a un : un jeu tout
   juste installe ou monte peut n'etre encore que dans le journal */
static int copy_appinfo(void) {
  struct stat st;
  remove(DB_COPY "-wal");
  remove(DB_COPY "-shm");
  if (copy_file(APPINFO_DB, DB_COPY)) return -1;
  if (!stat(APPINFO_DB "-wal", &st) && st.st_size > 0 && copy_file(APPINFO_DB "-wal", DB_COPY "-wal"))
    remove(DB_COPY "-wal");
  return 0;
}

/* signature de la base : change quand une application est installee, montee ou supprimee */
static long long appinfo_sig(void) {
  struct stat a, w;
  long long s = 0;
  if (!stat(APPINFO_DB, &a)) s = (long long)a.st_mtime * 1000003LL + (long long)a.st_size;
  if (!stat(APPINFO_DB "-wal", &w)) s = s * 31 + (long long)w.st_mtime * 1000003LL + (long long)w.st_size;
  return s;
}
static long long catalog_sig;

/* La tuile est-elle enregistree dans la base des applications ? 1 oui, 0 non, -1 on ne sait pas */
static int app_registered(const char *id) {
  if (copy_appinfo()) return -1;
  sqlite3 *db;
  if (sqlite3_open_v2(DB_COPY, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) { sqlite3_close(db); return -1; }
  sqlite3_stmt *st;
  int found = -1;
  if (sqlite3_prepare_v2(db, "select 1 from tbl_appinfo where titleId = ? limit 1", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_STATIC);
    found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
  }
  sqlite3_close(db);
  return found;
}
static int relay_registered(void) { return app_registered(RELAY_ID); }

/* Desinstallation : une appli installee par dossier (sans PKG) n'est pas retiree par le systeme
   (il repond OK sans rien faire) et l'accueil refuse de la supprimer. On efface donc ses lignes
   des deux bases ; la tuile disparait au redemarrage suivant. Seul cet identifiant est vise. */
static int sql_run(sqlite3 *db, const char *sql, const char *a, const char *b) {
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
  if (a) sqlite3_bind_text(st, 1, a, -1, SQLITE_TRANSIENT);
  if (b) sqlite3_bind_text(st, 2, b, -1, SQLITE_TRANSIENT);
  sqlite3_step(st);
  sqlite3_finalize(st);
  return sqlite3_changes(db);
}
static int forget_app(const char *id) {
  char loc[64], flag[64], cid[96] = "";
  snprintf(loc, sizeof loc, "cid:local:%s", id);
  snprintf(flag, sizeof flag, "nsx_content_flag_%s", id);
  int n = 0;
  sqlite3 *db;
  if (sqlite3_open_v2(APPINFO_DB, &db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK) {
    sqlite3_busy_timeout(db, 5000);
    n += sql_run(db, "DELETE FROM tbl_appinfo WHERE titleId = ?", id, NULL);
    n += sql_run(db, "DELETE FROM tbl_conceptinfo WHERE local_concept_id = ?", loc, NULL);
  }
  sqlite3_close(db);
  if (sqlite3_open_v2("/system_data/priv/mms/app.db", &db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK) {
    sqlite3_busy_timeout(db, 5000);
    /* tables par utilisateur (suffixe) : on les trouve par leur nom */
    char tabs[16][64];
    int nt = 0;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND (name LIKE 'tbl_iconinfo_%' "
                           "OR name LIKE 'tbl_concepticoninfo_%' OR name LIKE 'tbl_info_%')", -1, &st, NULL) == SQLITE_OK) {
      while (nt < 16 && sqlite3_step(st) == SQLITE_ROW)
        snprintf(tabs[nt++], sizeof tabs[0], "%s", (const char *)sqlite3_column_text(st, 0));
      sqlite3_finalize(st);
    }
    char q[256];
    for (int i = 0; i < nt; i++) {               /* concept propre a l'icone (cid:scp:... des anciennes versions) */
      if (strncmp(tabs[i], "tbl_iconinfo_", 13)) continue;
      snprintf(q, sizeof q, "SELECT localConceptId FROM \"%s\" WHERE titleId = ?", tabs[i]);
      if (sqlite3_prepare_v2(db, q, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
          snprintf(cid, sizeof cid, "%s", (const char *)sqlite3_column_text(st, 0));
        sqlite3_finalize(st);
      }
      if (cid[0]) {                              /* concept partage avec une autre appli : on n'y touche pas */
        snprintf(q, sizeof q, "SELECT 1 FROM \"%s\" WHERE localConceptId = ? AND titleId <> ? LIMIT 1", tabs[i]);
        if (sqlite3_prepare_v2(db, q, -1, &st, NULL) == SQLITE_OK) {
          sqlite3_bind_text(st, 1, cid, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(st, 2, id, -1, SQLITE_TRANSIENT);
          if (sqlite3_step(st) == SQLITE_ROW) cid[0] = 0;
          sqlite3_finalize(st);
        }
      }
    }
    n += sql_run(db, "DELETE FROM tbl_contentinfo WHERE titleId = ?", id, NULL);
    n += sql_run(db, "DELETE FROM tbl_conceptmetadata WHERE localConceptId IN (?, ?)", loc, cid[0] ? cid : loc);
    for (int i = 0; i < nt; i++) {
      if (!strncmp(tabs[i], "tbl_iconinfo_", 13)) snprintf(q, sizeof q, "DELETE FROM \"%s\" WHERE titleId = ?", tabs[i]);
      else if (!strncmp(tabs[i], "tbl_concepticoninfo_", 20)) snprintf(q, sizeof q, "DELETE FROM \"%s\" WHERE localConceptId IN (?, ?)", tabs[i]);
      else snprintf(q, sizeof q, "DELETE FROM \"%s\" WHERE key = ?", tabs[i]);
      if (!strncmp(tabs[i], "tbl_iconinfo_", 13)) n += sql_run(db, q, id, NULL);
      else if (!strncmp(tabs[i], "tbl_concepticoninfo_", 20)) n += sql_run(db, q, loc, cid[0] ? cid : loc);
      else n += sql_run(db, q, flag, NULL);
    }
  }
  sqlite3_close(db);
  return n;
}

/* Tuile « Launcher5 » sur l'accueil de la PS5 : installee si elle manque,
   reinstallee si elle a ete supprimee de l'accueil */
/* remplace l'icone de la tuile si elle a change (fichier present uniquement) */
static int refresh_icon(const char *path) {
  size_t n = fake_icon_end - fake_icon;
  struct stat st;
  if (stat(path, &st)) return 0;
  if ((size_t)st.st_size == n) {
    FILE *f = fopen(path, "rb");
    if (f) {
      uint8_t *b = malloc(n);
      int same = b && fread(b, 1, n, f) == n && !memcmp(b, fake_icon, n);
      free(b);
      fclose(f);
      if (same) return 0;
    }
  }
  logf("icone de la tuile mise a jour : %s (%d)", path, write_file(path, fake_icon, n, 0644));
  return 1;
}

static int install_relay_app(void) {
  struct stat st;
  int files = !stat("/user/app/" RELAY_ID "/sce_sys/param.json", &st) || !stat("/user/appmeta/" RELAY_ID "/param.json", &st);
  int reg = relay_registered();
  if (files && reg != 0) {           /* deja installee : l'icone suit la version */
    int changed = refresh_icon("/user/app/" RELAY_ID "/sce_sys/icon0.png");
    changed |= refresh_icon("/user/app/" RELAY_ID "/icon0.png");
    if (changed) {                   /* l'accueil garde l'ancienne image : on reinscrit la tuile */
      sceAppInstUtilInitialize();
      logf("tuile reinscrite pour la nouvelle icone : %d", install_app(RELAY_ID, "/user/app/"));
    }
    return 0;
  }
  logf("installation de la tuile Launcher5 (" RELAY_ID ")");
  if (mkdir("/user/app/" RELAY_ID, 0755) && errno != EEXIST) return -1;
  if (mkdir("/user/app/" RELAY_ID "/sce_sys", 0755) && errno != EEXIST) return -1;
  if (write_file("/user/app/" RELAY_ID "/sce_sys/param.json", fake_param, fake_param_end - fake_param, 0644)) return -1;
  if (write_file("/user/app/" RELAY_ID "/sce_sys/icon0.png", fake_icon, fake_icon_end - fake_icon, 0644)) return -1;
  sceAppInstUtilInitialize();
  return install_app(RELAY_ID, "/user/app/");
}

#ifdef L5_HAVE_GPU
static pid_t pid_by_name(const char *name);
static int alive(pid_t pid);
/* fichier identique a celui integre ? */
static int same_content(const char *path, const uint8_t *d, size_t n) {
  struct stat st;
  if (stat(path, &st) || (size_t)st.st_size != n) return 0;
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  uint8_t *b = malloc(n);
  int same = b && fread(b, 1, n, f) == n && !memcmp(b, d, n);
  free(b);
  fclose(f);
  return same;
}

/* ecrit le fichier s'il a change : -1 erreur, 0 deja a jour, 1 ecrit */
static int put_file(const char *path, const uint8_t *d, size_t n, int mode) {
  if (same_content(path, d, n)) return 0;
  char tmp[256];
  snprintf(tmp, sizeof tmp, "%s.l5new", path);
  if (write_file(tmp, d, n, mode)) { remove(tmp); return -1; }
  if (rename(tmp, path)) { remove(tmp); return -1; }
  return 1;
}

/* /system_ex en ecriture (comme le fait le lanceur de homebrews) */
static int remount_system_ex_rw(void) {
#define IOV(s) { (void *)(s), (s) ? strlen(s) + 1 : 0 }
  struct iovec iov[] = {
    IOV("from"), IOV("/dev/ssd0.system_ex"), IOV("fspath"), IOV("/system_ex"),
    IOV("fstype"), IOV("exfatfs"), IOV("large"), IOV("yes"), IOV("timezone"), IOV("static"),
    IOV("async"), IOV(NULL), IOV("ignoreacl"), IOV(NULL),
  };
#undef IOV
  return nmount(iov, sizeof iov / sizeof iov[0], MNT_UPDATE);
}

static int xp_delete(const char *p);
/* ancienne tuile GPU (LNCH00006, versions precedentes) : desinstallee, une seule tuile reste */
static void remove_old_gpu_tile(void) {
  struct stat st;
  int reg = app_registered(OLD_GPU_ID);
  if (reg <= 0 && stat("/user/app/" OLD_GPU_ID, &st) && stat("/system_ex/app/" OLD_GPU_ID, &st)) return;
  int app = sceSystemServiceGetAppIdOfRunningBigApp();   /* encore ouverte (version precedente) : fermee d'abord */
  char tid[32] = { 0 };
  if (app > 0 && !sceSystemServiceGetAppTitleId(app, tid) && !strcmp(tid, OLD_GPU_ID)) {
    sceSystemServiceKillApp(app, -1, 0, 0);
    usleep(800 * 1000);
  }
  if (reg > 0) { sceAppInstUtilInitialize(); logf("ancienne tuile GPU retiree (%d)", sceAppInstUtilAppUnInstall(OLD_GPU_ID)); }
  xp_delete("/user/app/" OLD_GPU_ID);
  xp_delete("/system_ex/app/" OLD_GPU_ID);
  xp_delete("/user/appmeta/" OLD_GPU_ID);
}

/* Interface GPU : application native (eboot.bin) installee par le bootstrap, sous la tuile Launcher5 */
static int gpu_installed;
#define GPU_ICON fake_icon
#define GPU_ICON_END fake_icon_end
static int install_gpu_app(void) {
  remove_old_gpu_tile();
  if (mkdir(GPU_DIR, 0755) && errno != EEXIST) {
    if (remount_system_ex_rw() || (mkdir(GPU_DIR, 0755) && errno != EEXIST)) {
      logf("interface GPU : /system_ex inaccessible (%d)", errno);
      return -1;
    }
  }
  /* empreinte de la version installee : on ne relit pas les 35 Mo a chaque ouverture */
  char stamp[96], cur[96] = "";
  snprintf(stamp, sizeof stamp, "%s %zu %zu %s %s", L5_VERSION, (size_t)(gpu_eboot_end - gpu_eboot),
           (size_t)(gpu_libc_end - gpu_libc), __DATE__, __TIME__);
  struct stat st;
  FILE *sf = fopen(GPU_DIR "/l5stamp", "r");
  if (sf) { if (!fgets(cur, sizeof cur, sf)) cur[0] = 0; fclose(sf); }
  cur[strcspn(cur, "\r\n")] = 0;
  if (!strcmp(cur, stamp) && !stat(GPU_DIR "/eboot.bin", &st) && !stat("/user/app/" GPU_ID "/sce_sys/param.json", &st)
      && app_registered(GPU_ID) != 0) {
    gpu_installed = 1;
    return 0;
  }
  mkdir(GPU_DIR "/sce_module", 0755);
  mkdir(GPU_DIR "/sce_sys", 0755);
  mkdir("/user/app/" GPU_ID, 0755);
  mkdir("/user/app/" GPU_ID "/sce_sys", 0755);
  int r[6] = {
    put_file(GPU_DIR "/eboot.bin", gpu_eboot, gpu_eboot_end - gpu_eboot, 0755),
    put_file(GPU_DIR "/sce_module/libc.prx", gpu_libc, gpu_libc_end - gpu_libc, 0755),
    put_file(GPU_DIR "/sce_sys/param.json", gpu_param, gpu_param_end - gpu_param, 0644),
    put_file(GPU_DIR "/sce_sys/icon0.png", GPU_ICON, GPU_ICON_END - GPU_ICON, 0644),
    put_file("/user/app/" GPU_ID "/sce_sys/param.json", gpu_param, gpu_param_end - gpu_param, 0644),
    put_file("/user/app/" GPU_ID "/sce_sys/icon0.png", GPU_ICON, GPU_ICON_END - GPU_ICON, 0644),
  };
  int changed = 0;
  for (int i = 0; i < 6; i++) {
    if (r[i] < 0) { logf("interface GPU : ecriture impossible (fichier %d)", i); return -1; }
    changed |= r[i];
  }
  int reg = app_registered(GPU_ID);
  if (changed || reg == 0) {
    sceAppInstUtilInitialize();
    int e = install_app(GPU_ID, "/user/app/");
    logf("interface GPU : application %s (%d)", reg == 0 ? "installee" : "mise a jour", e);
  }
  sf = fopen(GPU_DIR "/l5stamp", "w");
  if (sf) { fprintf(sf, "%s\n", stamp); fclose(sf); }
  gpu_installed = 1;
  return 0;
}

/* GPU utilisable ? (pas d'echec deja constate avec cette version) */
static int gpu_wanted(void) {
  if (!gpu_installed || !stat(DATA_DIR "/no_gpu", &(struct stat){0})) return 0;
  char v[32] = "";
  FILE *f = fopen(GPU_FAILED, "r");
  if (f) { if (!fgets(v, sizeof v, f)) v[0] = 0; fclose(f); }
  v[strcspn(v, "\r\n")] = 0;
  return strcmp(v, L5_VERSION) != 0;
}

/* droits complets pour l'interface GPU (hors du bac a sable, comme l'interface logicielle) */
/* l'interface GPU ecrit « attente des droits » dans le journal une fois demarree : avant,
   elle charge encore ses modules depuis /app0 et ne les trouverait plus hors du bac a sable */
static long log_size(void) { struct stat st; return stat(LOG_PATH, &st) ? 0 : (long)st.st_size; }
static int ui_waiting(long from, pid_t pid) {
  FILE *f = fopen(LOG_PATH, "r");
  if (!f) return 0;
  fseek(f, from, SEEK_SET);
  char line[256], want[64];
  snprintf(want, sizeof want, "attente des droits (pid %d)", (int)pid);
  int found = 0;
  while (!found && fgets(line, sizeof line, f)) found = strstr(line, want) != NULL;
  fclose(f);
  return found;
}
static long gpu_log_from;
static int raise_app(pid_t pid) {
  for (int i = 0; i < 100 && !ui_waiting(gpu_log_from, pid) && alive(pid); i++) usleep(100 * 1000);
  int e = elfldr_raise_privileges(pid);
  if (!e) e = kernel_set_ucred_authid(pid, 0x4801000000000013L);
  return e;
}

/* Lecture des dossiers pour l'interface GPU : une application n'a pas le droit de lister
   un dossier (appel refuse par le systeme). Elle demande la liste au bootstrap par une
   connexion locale (127.0.0.1 seulement) : chemin en entree, une ligne par element
   (« d nom » pour un dossier, « f nom » sinon) en sortie. */
#define DIR_PORT 5051
static long long xp_size(const char *p, int *files);
static int xp_allowed(const char *p);
static void *dir_client(void *arg) {
  int c = (int)(intptr_t)arg;
  struct timeval tv = { 3, 0 };                  /* jamais de fil bloque sans fin */
  setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
  char path[1024];
  size_t n = 0;
  while (n + 1 < sizeof path) {
    ssize_t r = recv(c, path + n, 1, 0);
    if (r <= 0 || path[n] == '\n') break;
    n++;
  }
  path[n] = 0;
  if (!strncmp(path, "?df ", 4)) {              /* espace libre et taille d'un stockage */
    struct statvfs v;
    char out[64];
    int len = statvfs(path + 4, &v) ? snprintf(out, sizeof out, "-1 -1\n")
      : snprintf(out, sizeof out, "%lld %lld\n", (long long)v.f_bavail * (long long)v.f_frsize, (long long)v.f_blocks * (long long)v.f_frsize);
    send(c, out, len, 0);
    close(c);
    return NULL;
  }
  if (!strncmp(path, "?du ", 4)) {              /* taille d'un jeu ou d'un dossier, nombre de fichiers */
    struct timeval tv = { 60, 0 };
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    char out[64];
    int files = 0, len;
    const char *q = path + 4;
    if (!xp_allowed(q) || !strcmp(q, "/") || !strcmp(q, "/mnt"))   /* /dev compris */
      len = snprintf(out, sizeof out, "-1 -1\n");
    else { long long sz = xp_size(path + 4, &files); len = snprintf(out, sizeof out, "%lld %d\n", sz, files); }
    send(c, out, len, 0);
    close(c);
    return NULL;
  }
  DIR *d = n ? opendir(path) : NULL;
  if (d) {
    struct dirent *e;
    char line[600];
    while ((e = readdir(d))) {
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
      int isdir = e->d_type == DT_DIR;
      if (e->d_type == DT_UNKNOWN) {             /* certains disques (exFAT) ne le disent pas */
        char full[1400];
        struct stat st;
        snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        isdir = !stat(full, &st) && S_ISDIR(st.st_mode);
      }
      int len = snprintf(line, sizeof line, "%c %s\n", isdir ? 'd' : 'f', e->d_name);
      if (send(c, line, len, 0) != len) break;
    }
    closedir(d);
  }
  close(c);
  return NULL;
}
static void *dir_server(void *arg) {
  (void)arg;
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return NULL;
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  struct sockaddr_in a = { 0 };
  a.sin_family = AF_INET;
  a.sin_port = htons(DIR_PORT);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int bound = 0;
  for (int i = 0; i < 40 && !(bound = !bind(s, (struct sockaddr *)&a, sizeof a)); i++) usleep(250 * 1000);   /* ancien bootstrap */
  if (!bound || listen(s, 16)) { logf("dossiers : service local indisponible (%d)", errno); close(s); return NULL; }
  for (;;) {
    int c = accept(s, NULL, NULL);
    if (c < 0) { usleep(100 * 1000); continue; }
    pthread_t t;
    if (pthread_create(&t, NULL, dir_client, (void *)(intptr_t)c)) close(c);
    else pthread_detach(t);
  }
  return NULL;
}

static pid_t gpu_pid(void) {
  int app = sceSystemServiceGetAppIdOfRunningBigApp();
  char tid[32] = { 0 };
  if (app <= 0 || sceSystemServiceGetAppTitleId(app, tid) || strcmp(tid, GPU_ID)) return -1;
  return pid_by_name("eboot.bin");
}
#endif

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

static int same_file(const char *a, const char *b) {
  FILE *x = fopen(a, "rb"), *y = fopen(b, "rb");
  int same = x && y;
  while (same) {
    int c = fgetc(x), d = fgetc(y);
    if (c != d) same = 0;
    if (c == EOF || d == EOF) break;
  }
  if (x) fclose(x);
  if (y) fclose(y);
  return same;
}

static int export_catalog(void) {
  catalog_sig = appinfo_sig();
  if (copy_appinfo()) { logf("copie de appinfo.db impossible"); return -1; }
  sqlite3 *db;
  if (sqlite3_open_v2(DB_COPY, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) { logf("ouverture appinfo.db impossible"); return -1; }
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
  if (same_file(CATALOG_PATH ".tmp", CATALOG_PATH)) { remove(CATALOG_PATH ".tmp"); return 0; }
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

/* Lance un ELF comme un vrai payload (processus a part, comme le fait l'elfldr du
   port 9021), avec l'elfldr integre. « --spawned » dit a la nouvelle version
   qu'elle a ete lancee correctement. */
static pid_t spawn_elf(const char *path, const char *arg) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *buf = n > 0 ? malloc((size_t)n) : NULL;
  size_t got = buf ? fread(buf, 1, (size_t)n, f) : 0;
  fclose(f);
  if (!buf || got != (size_t)n) { free(buf); return -1; }
  char *av[] = { (char *)path, (char *)arg, NULL };
  char *ev[] = { NULL };
  pid_t p = elfldr_spawn(DATA_DIR, -1, buf, av, ev);
  free(buf);
  logf("payload lance : %s %s (pid %d)", path, arg, p);
  return p < 0 ? -1 : p;
}

static int spawn_payload(const char *path) { return spawn_elf(path, "--spawned") < 0 ? -1 : 0; }

/* envoie un ELF au chargeur de la console (port 9021) */
static int send_to_loader(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_port = htons(9021);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof a)) {
    if (fd >= 0) close(fd);
    fclose(f);
    logf("chargeur ELF (port 9021) injoignable");
    return -1;
  }
  char buf[65536];
  size_t n;
  int ok = 1;
  while (ok && (n = fread(buf, 1, sizeof buf, f)) > 0)
    for (size_t o = 0; o < n;) {
      ssize_t w = send(fd, buf + o, n - o, 0);
      if (w <= 0) { ok = 0; break; }
      o += (size_t)w;
    }
  fclose(f);
  close(fd);
  return ok ? 0 : -1;
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

/* Le Lecteur de disques reste ouvert, dans un etat instable, apres un exploit qui passe
   par le lecteur multimedia. Tant qu'il est la, un payload qui touche au noyau (kstuff)
   peut faire planter la console : on le ferme proprement avant tout le reste
   (suspension, arret du processus, puis fermeture par le systeme). */
#define DISC_PLAYER_ID "NPXS40140"
int sceLncUtilSuspendApp(uint32_t app_id);

static pid_t pid_by_name(const char *name) {      /* liste des processus du noyau */
  int mib[4] = { 1 /* CTL_KERN */, 14 /* KERN_PROC */, 8 /* KERN_PROC_PROC */, 0 };
  size_t len = 0;
  pid_t found = -1;
  for (int tries = 0; tries < 4; tries++) {
    if (sysctl(mib, 4, NULL, &len, NULL, 0)) return -1;
    len += len / 2 + 65536;                      /* la liste peut grandir entre les deux appels */
    unsigned char *buf = malloc(len);
    if (!buf) return -1;
    if (sysctl(mib, 4, buf, &len, NULL, 0)) { free(buf); continue; }
    for (unsigned char *p = buf; p + 460 < buf + len;) {
      int size = *(int *)p;
      if (size <= 0) break;
      if (!strcmp((const char *)p + 447, name)) found = *(pid_t *)(p + 72);
      p += size;
    }
    free(buf);
    break;
  }
  return found;
}

static void close_disc_player(void) {
  int app = sceSystemServiceGetAppIdOfRunningBigApp();
  char tid[16] = "";
  if (app <= 0 || sceSystemServiceGetAppTitleId(app, tid) || strcmp(tid, DISC_PLAYER_ID)) return;
  logf("lecteur de disques ouvert (exploit) : fermeture avant tout le reste");
  logf("lecteur de disques : suspension (%d)", sceLncUtilSuspendApp((uint32_t)app));
  sleep(2);                                       /* l'accueil se stabilise */
  pid_t p = pid_by_name("SceDiscPlayer");
  if (p > 0) {
    logf("lecteur de disques : arret du processus %d (%d)", p, kill(p, SIGKILL));
    sleep(1);
  }
  if (sceSystemServiceGetAppIdOfRunningBigApp() == app)
    logf("lecteur de disques : fermeture par le systeme (%d)", sceSystemServiceKillApp(app, -1, 0, 0));
  else logf("lecteur de disques : ferme");
}

/* ------------------------------------------------------------------ */
/* Envoi sur : les payloads qui touchent au noyau (kstuff, HEN, hyper-  */
/* viseur…) partent d'ici, sans application detournee au premier plan   */
/* (comme avec un gestionnaire de payloads en arriere-plan).            */
/* ------------------------------------------------------------------ */
static int safe_send(const char *path, int settle) {
  const char *name = strrchr(path, '/');
  name = name ? name + 1 : path;
  /* le chargeur ELF peut mettre quelques secondes a ecouter : on reessaie l'envoi lui-meme
     (jamais de connexion vide, que le chargeur prendrait pour un payload) */
  int rc = -1;
  for (int i = 0; i < 40 && rc; i++) {
    rc = send_to_loader(path);
    if (rc) usleep(500 * 1000);
  }
  char msg[200];
  snprintf(msg, sizeof msg, tr(rc ? "Launcher5 : %s n'a pas pu être envoyé" : "Launcher5 : %s envoyé"), name);
  notify(msg);
  logf("envoi sur : %s %s", path, rc ? "en echec" : "envoye");
  if (!rc) sleep(settle);                        /* le payload s'installe avant la suite */
  return rc;
}

/* Liste « Au demarrage » (autostart.json, ecrite par l'interface) : une fois par allumage,
   seulement si Launcher5 fait partie de la chaine de demarrage. */
static int find_payload(const char *dir, const char *want, size_t wl, int depth, char *out, size_t n) {
  DIR *d = opendir(dir);
  if (!d) return -1;
  struct dirent *e;
  int found = -1;
  while (found && (e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char p[768];
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    struct stat st;
    if (stat(p, &st)) continue;
    if (S_ISDIR(st.st_mode)) { if (depth > 0) found = find_payload(p, want, wl, depth - 1, out, n); continue; }
    /* l'interface garde les 35 premiers caracteres du nom : prefixe si le nom est coupe */
    if ((wl < 35 && !strcmp(e->d_name, want)) || (wl >= 35 && !strncmp(e->d_name, want, wl))) {
      snprintf(out, n, "%s", p);
      found = 0;
    }
  }
  closedir(d);
  return found;
}

/* kstuff deja actif ? Le test de kstuff lui-meme : l'appel noyau 0xffffffff00000027, passe
   par la passerelle de getpid (libkernel), repond 0 seulement si kstuff est charge.
   1 : charge, 0 : absent, -1 : test impossible. */
void *l5_kek_gate;
__asm__(".text\n.globl l5_kekcall\nl5_kekcall:\n  mov 8(%rsp), %rax\n  jmp *l5_kek_gate(%rip)\n");
uint64_t l5_kekcall(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t nr);

static int kstuff_test(void) {
  intptr_t g = kernel_dynlib_dlsym(-1, 0x1, "getpid");
  if (!g) g = kernel_dynlib_dlsym(-1, 0x2001, "getpid");
  if (!g) return -1;
  l5_kek_gate = (void *)(g + 7);
  return l5_kekcall(0, 0, 0, 0, 0, 0, 0xffffffff00000027ULL) == 0;
}

static int kstuff_loaded(void) {
  int on = kstuff_test();
  logf("kstuff : %s", on > 0 ? "deja actif" : on ? "test impossible" : "pas encore charge");
  return on;
}

/* apres l'envoi de kstuff : on attend qu'il reponde (15 s au plus) au lieu d'un delai fixe */
static void kstuff_wait(void) {
  sleep(2);                                      /* on le laisse s'installer avant de lui parler */
  for (int i = 4; i < 30; i++) {
    int on = kstuff_test();
    if (on > 0) { logf("kstuff : pret en %d.%d s", i / 2, i % 2 ? 5 : 0); sleep(1); return; }
    if (on < 0) break;
    usleep(500 * 1000);
  }
  logf("kstuff : pas de reponse, delai fixe");
  sleep(5);
}

/* Dossier de l'autoloader reellement lu au demarrage, dans l'ordre des autoloaders (Y2JB,
   WebKit, BD-JB) : cle USB d'abord, puis /data ; dans chacun, un dossier propre a une
   application (ps5_autoloader_PPSA01650…) avant ps5_autoloader. Le premier qui contient un
   autoload.txt gagne ; sinon /data/ps5_autoloader. */
static void al_find(const char *root, char *out, size_t n) {
  char bases[9][64];
  int nb = 0;
  for (int u = 0; u < 8; u++) snprintf(bases[nb++], sizeof bases[0], "%s/mnt/usb%d", root, u);
  snprintf(bases[nb++], sizeof bases[0], "%s/data", root);
  for (int b = 0; b < nb; b++) {
    char p[600];
    struct stat st;
    DIR *d = opendir(bases[b]);
    struct dirent *e;
    int found = 0;
    while (d && !found && (e = readdir(d))) {     /* dossier propre a une application */
      if (strncmp(e->d_name, "ps5_autoloader_", 15)) continue;
      snprintf(p, sizeof p, "%s/%s/autoload.txt", bases[b], e->d_name);
      if (!stat(p, &st)) { snprintf(out, n, "%s/%s", bases[b], e->d_name); found = 1; }
    }
    if (d) closedir(d);
    if (found) return;
    snprintf(p, sizeof p, "%s/ps5_autoloader/autoload.txt", bases[b]);
    if (!stat(p, &st)) { snprintf(out, n, "%s/ps5_autoloader", bases[b]); return; }
  }
  snprintf(out, n, "%s/data/ps5_autoloader", root);
}

static FILE *open_autoload(void) {
  char dir[512], p[600];
  al_find("", dir, sizeof dir);
  snprintf(p, sizeof p, "%s/autoload.txt", dir);
  return fopen(p, "r");
}

static int in_boot_chain(void) {
  struct stat st;
  if (!stat("/data/OnionHEN/plugins/LNCH00005.elf", &st)) return 1;
  FILE *f = open_autoload();
  char l[512];
  int on = 0;
  while (f && fgets(l, sizeof l, f)) {
    char *t = l;
    while (*t == ' ' || *t == '\t') t++;
    if (!strncmp(t, "launcher5.elf", 13) && (t[13] == 0 || t[13] == '\r' || t[13] == '\n' || t[13] == ' ')) on = 1;
  }
  if (f) fclose(f);
  return on;
}

/* nom de famille d'un payload : les lettres du debut du nom de fichier, en minuscules
   (« ShadowMountPlus_1.7.elf » -> shadowmountplus, « kstuff-lite.elf » -> kstuff) */
static void payload_key(const char *path, char *k, size_t n) {
  const char *b = strrchr(path, '/');
  b = b ? b + 1 : path;
  while (*b == ' ' || *b == '\t') b++;
  size_t i = 0;
  for (; isalpha((unsigned char)b[i]) && i + 1 < n; i++) k[i] = (char)tolower((unsigned char)b[i]);
  k[i] = 0;
}

/* deja lance par autoload.txt (les payloads de l'utilisateur gardes avant Launcher5) ? */
/* meme payload sous un autre nom (shadowmount / shadowmountplus, ftpsrv / ftpsrvdrakmor),
   ou meme role : un seul monteur de jeux a la fois (ShadowMount, Kraken) */
static int same_family(const char *a, const char *b) {
  static const char *const MOUNTERS[] = { "shadowmount", "kraken" };
  int ma = 0, mb = 0;
  for (size_t i = 0; i < sizeof MOUNTERS / sizeof MOUNTERS[0]; i++) {
    size_t l = strlen(MOUNTERS[i]);
    if (!strncmp(a, MOUNTERS[i], l)) ma = 1;
    if (!strncmp(b, MOUNTERS[i], l)) mb = 1;
  }
  if (ma && mb) return 1;
  size_t la = strlen(a), lb = strlen(b), m = la < lb ? la : lb;
  return m >= 4 && !strncmp(a, b, m);
}

static int in_autoload(const char *key) {
  FILE *f = open_autoload();
  if (!f) return 0;
  char l[512], k[64];
  int found = 0;
  while (!found && fgets(l, sizeof l, f)) {
    const char *t = l;
    while (*t == ' ' || *t == '\t') t++;
    if (!*t || *t == '#' || *t == '!' || *t == '\r' || *t == '\n') continue;
    payload_key(t, k, sizeof k);
    found = k[0] && same_family(k, key);
  }
  fclose(f);
  return found;
}

/* Payloads exclus : exploits de l'hyperviseur qui demandent le mode repos (byepervisor,
   HyperHEN, chargeur Linux). Ils n'ont rien a faire dans un launcher, ni au demarrage. */
static int payload_excluded(const char *name) {
  static const char *const K[] = { "byepervisor", "hyperhen", "linuxloader", "ps5linux" };
  char n[256];
  size_t k = 0;
  for (const char *p = name ? name : ""; *p && k + 1 < sizeof n; p++)
    if (isalnum((unsigned char)*p)) n[k++] = (char)tolower((unsigned char)*p);
  n[k] = 0;
  for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) if (strstr(n, K[i])) return 1;
  return 0;
}

/* un payload de la liste a ne pas envoyer : incompatible, ou deja la (OnionHEN, autoload.txt, kstuff actif) */
static int boot_skip(const char *path, int onion, int *ks) {
  char low[768], key[64];
  size_t k = 0;
  for (; path[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)path[k]);
  low[k] = 0;
  const char *base = strrchr(path, '/');
  if (payload_excluded(base ? base + 1 : path)) { logf("demarrage : %s demande le mode repos, jamais envoye", path); return 1; }
  if (onion && strstr(low, "onion")) { logf("demarrage : OnionHEN tourne deja, pas renvoye"); return 1; }
  payload_key(path, key, sizeof key);
  if (key[0] && in_autoload(key)) { logf("demarrage : %s deja lance par autoload.txt, pas renvoye", path); return 1; }
  if (strstr(low, "kstuff")) {                   /* jamais deux kstuff (la console plante) */
    if (*ks == -2) *ks = kstuff_loaded();
    if (*ks > 0) { logf("demarrage : kstuff deja actif, %s pas renvoye", path); return 1; }
  }
  return 0;
}

/* envoi d'un payload de la liste : kstuff, on attend qu'il soit pret ; un HEN (noyau, sans
   test de fin) garde le delai long ; les autres (monteurs, serveurs) tournent en arriere-plan */
static void boot_send(const char *path) {
  char low[768];
  size_t k = 0;
  for (; path[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)path[k]);
  low[k] = 0;
  const char *b = strrchr(low, '/');
  b = b ? b + 1 : low;
  int is_kstuff = strstr(b, "kstuff") != NULL, is_hen = strstr(b, "hen") != NULL;
  if (safe_send(path, is_kstuff ? 0 : is_hen ? 10 : 2)) return;
  if (is_kstuff) kstuff_wait();
}

/* Une fois par allumage. Lance par la chaine de demarrage : la liste part. Premier lancement
   ou lancement a la main : seulement si kstuff n'est pas encore charge (sinon l'utilisateur
   a deja sa propre chaine, on n'y touche pas). Renvoie le nombre envoye. */
static int boot_list(int first) {
  const char *marker = "/system_tmp/launcher5_boot_list";
  struct stat st;
  int strict = first || !in_boot_chain();
  int ks = strict ? kstuff_loaded() : -2;        /* etat de kstuff, teste au besoin */
  if (!stat(marker, &st)) { logf("demarrage : liste deja envoyee depuis l'allumage"); return 0; }
  FILE *f = fopen(DATA_DIR "/autostart.json", "r");
  if (!f) return 0;
  char json[8192];
  size_t n = fread(json, 1, sizeof json - 1, f);
  fclose(f);
  json[n] = 0;
  if (strict && ks != 0) {
    logf("%s : kstuff %s, liste non envoyee", first ? "premier lancement" : "lance a la main", ks > 0 ? "deja actif" : "non verifiable");
    return 0;
  }
  FILE *m = fopen(marker, "w");                  /* une fois par allumage, meme si un envoi plante */
  if (!m) { logf("demarrage : marqueur impossible dans /system_tmp, liste non envoyee"); return 0; }
  fputs("done\n", m);
  fclose(m);
  int onion = !stat("/system_tmp/onionhen", &st);
  int sent = 0;
  /* fichiers ecrits par l'interface (« paths ») : la version a jour de chaque payload */
  char *paths = strstr(json, "\"paths\"");
  if (paths) {
    char *end = strchr(paths, ']');
    for (char *p = strstr(paths, "\"/data/"); p && (!end || p < end); p = strstr(p + 1, "\"/data/")) {
      char path[768];
      size_t k = 0;
      for (char *q = p + 1; *q && *q != '"' && k + 1 < sizeof path; q++) path[k++] = *q;
      path[k] = 0;
      if (stat(path, &st)) { logf("demarrage : %s introuvable", path); continue; }
      if (boot_skip(path, onion, &ks)) continue;
      if (!sent++) logf("demarrage : envoi de la liste avant l'interface");
      boot_send(path);
    }
    return sent;
  }
  for (char *p = strstr(json, "\"elf:"); p; p = strstr(p + 1, "\"elf:")) {
    char want[64];
    size_t k = 0;
    for (char *q = p + 5; *q && *q != '"' && k + 1 < sizeof want; q++) want[k++] = *q;
    want[k] = 0;
    if (!k) continue;
    static const char *const DIRS[][2] = { { "/data/payloads", "1" }, { "/data/pldmgr/payloads", "3" },
                                           { "/data/etaHEN/payloads", "1" }, { "/data/ps5_autoloader", "0" } };
    char path[768];
    int ok = -1;
    for (size_t d = 0; d < 4 && ok; d++) ok = find_payload(DIRS[d][0], want, k, DIRS[d][1][0] - '0', path, sizeof path);
    if (ok) { logf("demarrage : %s introuvable", want); continue; }
    if (boot_skip(path, onion, &ks)) continue;
    if (!sent++) logf("demarrage : envoi de la liste avant l'interface");
    boot_send(path);
  }
  return sent;
}

/* Premier lancement : l'interface, lancee sans ecran (« --prepare »), telecharge kstuff-lite,
   ShadowMountPlus et prepare la liste « Au demarrage ». 1 : liste prete. */
#define STARTER_DONE DATA_DIR "/cache/starter_done"
#define STARTER_STATUS DATA_DIR "/cache/starter_status"
#define STARTER_NOTE DATA_DIR "/cache/starter_note"
static pid_t spawn_elf(const char *path, const char *arg);

static int read_word(const char *path, char *out, size_t n) {
  FILE *f = fopen(path, "r");
  out[0] = 0;
  if (!f) return -1;
  if (!fgets(out, (int)n, f)) out[0] = 0;
  fclose(f);
  out[strcspn(out, "\r\n")] = 0;
  return 0;
}

static void write_word(const char *path, const char *w) {
  FILE *f = fopen(path, "w");
  if (f) { fprintf(f, "%s\n", w); fclose(f); }
}

static int starter_setup(char *status, size_t sn) {
  struct stat st;
  status[0] = 0;
  if (!stat(STARTER_DONE, &st)) return 0;
  notify("Launcher5 : préparation de la console (kstuff, ShadowMount)…");
  remove(STARTER_STATUS);
  pid_t p = spawn_elf(UI_PATH, "--prepare");
  if (p < 0) { logf("premier lancement : preparation impossible"); return 0; }
  for (int i = 0; i < 1800 && stat(STARTER_STATUS, &st); i++) usleep(100 * 1000);   /* 3 min max */
  read_word(STARTER_STATUS, status, sn);
  remove(STARTER_STATUS);
  logf("premier lancement : preparation %s", status[0] ? status : "sans reponse");
  if (!status[0] && alive(p)) kill(p, SIGKILL);
  return !strcmp(status, "ok") || !strcmp(status, "partial");
}

static time_t t_start;                           /* mesure du temps d'ouverture */
static int after_update;                          /* lance par une mise a jour (tuile peut-etre encore ouverte) */

#ifdef L5_HAVE_GPU
static int gpu_pending;                  /* interface GPU lancee : a-t-elle affiche ses images ? */
static time_t gpu_started;               /* heure du lancement de l'interface GPU */
#endif
static pid_t start_ui(void) {
  export_catalog();                      /* a jour a chaque ouverture */
  remove(REQUEST_PATH);
#ifdef L5_HAVE_GPU
  struct stat gs;
  if (gpu_pending && stat(GPU_OK, &gs)) {
    FILE *f = fopen(GPU_FAILED, "w");
    if (f) { fprintf(f, "%s\n", L5_VERSION); fclose(f); }
    logf("interface GPU : pas d'image au dernier lancement, retour au dessin classique");
    notify("Launcher5 : affichage GPU indisponible, mode classique");
  }
  gpu_pending = 0;
  if (gpu_wanted()) {
    remove(GPU_OK);
    gpu_log_from = log_size();
    if (!launch_title(GPU_ID)) {
      pid_t p = -1;
      for (int i = 0; i < 80 && p <= 0; i++) { usleep(100 * 1000); p = gpu_pid(); }
      if (p > 0) {
        int e = raise_app(p);
        gpu_pending = 1;
        gpu_started = time(NULL);
        logf("interface GPU lancee (pid %d, droits %d, %ld s apres le lancement)", p, e, (long)(time(NULL) - t_start));
        return p;
      }
    }
    logf("interface GPU : lancement impossible, dessin classique");
  }
#endif
  char *argv[] = { (char *)UI_PATH, NULL };
  char *envp[] = { NULL };
  pid_t ui = -1;
  for (int tries = 0; tries < 4; tries++) {
    ui = hbldr_launch(DATA_DIR, UI_PATH, -1, argv, envp);
    if (ui >= 0) break;
    /* echec : souvent la tuile Launcher5 restee ouverte apres un plantage. On ne ferme
       que celle-la (jamais un jeu), puis on reessaie. */
    int app = sceSystemServiceGetAppIdOfRunningBigApp();
    char tid[16] = "";
    if (app > 0 && !sceSystemServiceGetAppTitleId(app, tid) && !strcmp(tid, RELAY_ID)) {
      logf("tuile Launcher5 bloquee : fermeture (%d)", sceSystemServiceKillApp(app, -1, 0, 0));
    } else if (app > 0) {
      logf("interface : une autre application est ouverte (%s), on ne la ferme pas", tid[0] ? tid : "?");
      break;
    }
    sleep(2);
  }
  logf("interface lancee (pid %d, %ld s apres le lancement)", ui, (long)(time(NULL) - t_start));
  return ui;
}


/* ------------------------------------------------------------------ */
/* Launcher5 Xplorer : copies, deplacements et suppressions entre les   */
/* stockages, faits ici (le bootstrap continue meme si l'ecran se ferme */
/* ou qu'un jeu est lance). Demande : fichier xjob (operation, source,  */
/* destination). Progression : cache/xprog. Annulation : cache/xcancel. */
/* ------------------------------------------------------------------ */
#define XJOB    DATA_DIR "/xjob"
#define XPROG   DATA_DIR "/cache/xprog"
#define XCANCEL DATA_DIR "/cache/xcancel"

/* Launcher5 public : seulement les stockages des jeux, jamais le systeme, jamais « .. » */
static int xp_allowed(const char *p) {
  static const char *const ROOTS[] = { "/data/", "/mnt/usb", "/mnt/ext", "/user/data/" };
  if (!p || p[0] != '/' || strstr(p, "/../") || strstr(p, "/./") || strstr(p, "//")) return 0;
  size_t l = strlen(p);
  if (l >= 3 && !strcmp(p + l - 3, "/..")) return 0;
  for (size_t i = 0; i < sizeof ROOTS / sizeof ROOTS[0]; i++)
    if (!strncmp(p, ROOTS[i], strlen(ROOTS[i]))) return 1;
  return 0;
}
static int xp_protected(const char *p) { (void)p; return 0; }   /* deja limite par xp_allowed */
/* chemin reel (liens symboliques suivis) aussi dans un stockage autorise : un lien pose dans
   /data ne doit pas ouvrir le reste du disque */
static int xp_real_ok(const char *p) {
  char rp[1024];
  if (!realpath(p, rp)) return 0;
  size_t l = strlen(rp);
  if (l + 2 > sizeof rp) return 0;
  rp[l] = '/'; rp[l + 1] = 0;                    /* « /data » -> « /data/ » : la racine du stockage compte */
  return xp_allowed(rp);
}
/* racine d'un stockage (/data, /mnt/usb0, /mnt/ext1...) : on ne la supprime jamais */
static int xp_is_root(const char *p) {
  if (!strcmp(p, "/data") || !strcmp(p, "/user/data")) return 1;
  if (!strncmp(p, "/mnt/", 5) && !strchr(p + 5, '/')) return 1;
  return 0;
}

static struct { long long done, total; time_t t0; char file[256]; int files; } XP;
static time_t xp_last;
static void xp_write(const char *state) {
  char tmp[] = XPROG ".tmp";
  FILE *f = fopen(tmp, "w");
  if (!f) return;
  long sec = (long)(time(NULL) - XP.t0);
  long long speed = sec > 0 ? XP.done / sec : 0;
  fprintf(f, "%s\n%lld\n%lld\n%lld\n%d\n%s\n", state, XP.done, XP.total, speed, XP.files, XP.file);
  fclose(f);
  rename(tmp, XPROG);
}
static int xp_cancelled(void) { struct stat st; return !stat(XCANCEL, &st); }

static long long xp_size(const char *p, int *files) {        /* taille totale (dossier : recursif) */
  struct stat st;
  if (lstat(p, &st)) return 0;
  if (!S_ISDIR(st.st_mode)) { (*files)++; return st.st_size; }
  long long t = 0;
  DIR *d = opendir(p);
  struct dirent *e;
  while (d && (e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    char c[1024];
    snprintf(c, sizeof c, "%s/%s", p, e->d_name);
    t += xp_size(c, files);
  }
  if (d) closedir(d);
  return t;
}

static int xp_copy_file(const char *src, const char *dst, mode_t mode) {
  int in = open(src, O_RDONLY), out = in < 0 ? -1 : open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode & 0777);
  if (in < 0 || out < 0) { if (in >= 0) close(in); return -1; }
  static char *buf;
  if (!buf) buf = malloc(4 << 20);
  int rc = buf ? 0 : -1;
  snprintf(XP.file, sizeof XP.file, "%s", strrchr(src, '/') ? strrchr(src, '/') + 1 : src);
  while (!rc) {
    ssize_t n = read(in, buf, 4 << 20);
    if (n == 0) break;
    if (n < 0) { rc = -1; break; }
    for (ssize_t w = 0; w < n;) {
      ssize_t k = write(out, buf + w, n - w);
      if (k <= 0) { rc = -1; break; }        /* disque plein ou retire */
      w += k;
    }
    XP.done += n;
    if (time(NULL) != xp_last) { xp_last = time(NULL); xp_write("encours"); }
    if (xp_cancelled()) rc = -2;
  }
  close(in);
  if (close(out)) rc = rc ? rc : -1;
  if (rc) remove(dst);                         /* pas de fichier a moitie copie */
  return rc;
}

static int xp_copy(const char *src, const char *dst) {      /* 0, -1 erreur, -2 annule */
  struct stat st;
  if (lstat(src, &st)) return -1;
  if (!S_ISDIR(st.st_mode)) { int r = xp_copy_file(src, dst, st.st_mode); if (!r) XP.files++; return r; }
  if (mkdir(dst, 0777) && errno != EEXIST) return -1;
  DIR *d = opendir(src);
  if (!d) return -1;
  struct dirent *e;
  int rc = 0;
  while (!rc && (e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    char a[1024], b[1024];
    snprintf(a, sizeof a, "%s/%s", src, e->d_name);
    snprintf(b, sizeof b, "%s/%s", dst, e->d_name);
    rc = xp_copy(a, b);
  }
  closedir(d);
  return rc;
}

static int xp_delete(const char *p) {
  struct stat st;
  if (lstat(p, &st)) return 0;
  if (!S_ISDIR(st.st_mode)) return unlink(p);
  /* dossier aux droits fermes (cree par une appli sans droits complets) : on l'ouvre d'abord,
     sinon son contenu ne peut pas etre efface */
  if ((st.st_mode & 0700) != 0700 || access(p, W_OK | X_OK)) chmod(p, 0777);
  DIR *d = opendir(p);
  struct dirent *e;
  int rc = 0;
  while (d && (e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    char c[1024];
    snprintf(c, sizeof c, "%s/%s", p, e->d_name);
    rc |= xp_delete(c);
  }
  if (d) closedir(d);
  return rmdir(p) | rc;
}



static void xp_run(const char *op, const char *src, const char *dst_dir) {
  memset(&XP, 0, sizeof XP);
  XP.t0 = time(NULL);
  remove(XCANCEL);
  const char *base = strrchr(src, '/') ? strrchr(src, '/') + 1 : src;
  char dst[1024];
  snprintf(dst, sizeof dst, "%s/%s", dst_dir, base);
  struct stat st;
  int modifies_src = strcmp(op, "copy") != 0;      /* deplacer ou supprimer touchent la source */
  if (!xp_allowed(src) || (strcmp(op, "delete") && !xp_allowed(dst))) {
    xp_write("erreur:emplacement non autorise"); logf("xplorer : refuse (%s %s)", op, src); return;
  }
  if ((modifies_src && (xp_is_root(src) || xp_protected(src))) || (strcmp(op, "delete") && xp_protected(dst))) {
    xp_write("erreur:zone systeme protegee"); logf("xplorer : zone systeme refusee (%s %s)", op, src); return;
  }
  if (strcmp(op, "copy") && (!strcmp(src, DATA_DIR) || !strcmp(src, "/data/ps5_autoloader"))) {
    xp_write("erreur:dossier du launcher protege"); return;   /* jamais le launcher lui-meme */
  }
  if (lstat(src, &st)) { xp_write("erreur:introuvable"); return; }
  {                                              /* dossier parent reel de la source (un lien lui-meme reste supprimable) */
    char par[1024];
    snprintf(par, sizeof par, "%s", src);
    char *sl = strrchr(par, '/');
    if (sl && sl != par) *sl = 0;
    if (!xp_real_ok(par) || (!S_ISLNK(st.st_mode) && !xp_real_ok(src))) {
      xp_write("erreur:emplacement non autorise"); logf("xplorer : chemin reel refuse (%s %s)", op, src); return;
    }
  }
  logf("xplorer : %s %s -> %s", op, src, strcmp(op, "delete") ? dst : "-");
  if (!strcmp(op, "delete")) {
    snprintf(XP.file, sizeof XP.file, "%s", base);
    xp_write("encours");
    xp_write(xp_delete(src) ? "erreur:suppression incomplete" : "fini");
    return;
  }
  size_t sl = strlen(src);
  if (!strncmp(dst, src, sl) && (dst[sl] == '/' || !dst[sl])) { xp_write("erreur:destination dans la source"); return; }
  if (!lstat(dst, &st)) { xp_write("erreur:existe deja a destination"); return; }
  {                                              /* dossier de destination cree au besoin */
    char m[1024];
    snprintf(m, sizeof m, "%s", dst_dir);
    for (char *q = m + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(m, 0777); *q = '/'; }
    mkdir(m, 0777);
  }
  if (!xp_real_ok(dst_dir)) { xp_write("erreur:emplacement non autorise"); logf("xplorer : destination reelle refusee (%s)", dst_dir); return; }
  if (!strcmp(op, "move") && !rename(src, dst)) { xp_write("fini"); return; }   /* meme disque : instantane */
  int files = 0;
  XP.total = xp_size(src, &files);
  struct statvfs v;
  if (!statvfs(dst_dir, &v) && (long long)v.f_bavail * (long long)v.f_frsize < XP.total) {
    xp_write("erreur:pas assez de place"); return;
  }
  xp_write("encours");
  int rc = xp_copy(src, dst);
  if (rc == -2) { xp_delete(dst); xp_write("annule"); logf("xplorer : annule"); return; }
  if (rc) { xp_delete(dst); xp_write("erreur:copie impossible (disque plein ou retire ?)"); logf("xplorer : echec de copie"); return; }
  if (!strcmp(op, "move")) {
    /* deplacement entre deux disques : l'original n'est supprime qu'apres verification de
       la copie (meme taille totale, meme nombre de fichiers), puis on verifie qu'il a disparu */
    int sf = 0, df = 0;
    long long ss = xp_size(src, &sf), ds = xp_size(dst, &df);
    if (ss != ds || sf != df) {
      xp_delete(dst);
      xp_write("erreur:copie incomplete, original garde");
      logf("xplorer : verification echouee (%lld/%lld octets, %d/%d fichiers), original garde", ds, ss, df, sf);
      return;
    }
    snprintf(XP.file, sizeof XP.file, "%s", base);
    xp_write("suppression");
    xp_delete(src);
    if (!lstat(src, &st)) { usleep(500 * 1000); xp_delete(src); }   /* un second essai */
    if (!lstat(src, &st)) {
      int rf = 0;
      long long rs = xp_size(src, &rf);
      xp_write("erreur:copie faite, original pas entierement supprime");
      logf("xplorer : original pas entierement supprime (%d fichiers, %lld octets restent)", rf, rs);
      return;
    }
  }
  xp_write("fini");
  logf("xplorer : termine (%lld octets, %ld s)", XP.done, (long)(time(NULL) - XP.t0));
}

static void *xp_thread(void *u) {
  (void)u;
  for (;;) {
    usleep(250 * 1000);
    FILE *f = fopen(XJOB, "r");
    if (!f) continue;
    char op[32] = "", src[1024] = "", dst[1024] = "";
    if (fgets(op, sizeof op, f) && fgets(src, sizeof src, f)) { if (!fgets(dst, sizeof dst, f)) dst[0] = 0; }
    fclose(f);
    remove(XJOB);
    op[strcspn(op, "\r\n")] = 0; src[strcspn(src, "\r\n")] = 0; dst[strcspn(dst, "\r\n")] = 0;
    xp_run(op, src, dst);
  }
  return NULL;
}



/* arrete l'interface : l'application GPU est fermee par le systeme, l'interface logicielle tuee */
static void stop_ui(pid_t ui) {
#ifdef L5_HAVE_GPU
  int app = sceSystemServiceGetAppIdOfRunningBigApp();
  char tid[32] = { 0 };
  if (app > 0 && !sceSystemServiceGetAppTitleId(app, tid) && !strcmp(tid, GPU_ID) && gpu_pid() == ui) {
    sceSystemServiceKillApp(app, -1, 0, 0);
    for (int i = 0; i < 30 && alive(ui); i++) usleep(100 * 1000);
    if (!alive(ui)) return;
  }
#endif
  kill(ui, SIGKILL);
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

int main(int argc, char **argv) {
  mkdir(DATA_DIR, 0755);
  t_start = time(NULL);
  logf("--- demarrage --- Launcher5 v" L5_VERSION " par " L5_AUTHOR);
  {
    int lang = 1;
    if (sceSystemServiceParamGetInt(1 /* langue */, &lang) < 0) lang = 1;
    i18n_init(lang);
    logf("langue du systeme : %d (%s)", lang, i18n_locale());
  }
  if (sceUserServiceInitialize(0)) { logf("UserService indisponible"); return EXIT_FAILURE; }
  if (elevate_self()) { notify("Launcher5 : droits insuffisants (jailbreak actif ?)"); return EXIT_FAILURE; }
  /* Mise a jour lancee par la v2.5.1 / 2.5.2 : elle demarrait dans la tuile (application)
     au lieu d'un payload, et ne pouvait pas ouvrir l'interface. Dans ce cas on s'arrete
     tout de suite : ces versions renvoient alors le fichier au chargeur ELF. */
  {
    int spawned = argc > 1 && argv[1] && !strcmp(argv[1], "--spawned");
    after_update = spawned;
    /* l'ancienne methode passait le chemin du fichier en argument ; le chargeur ELF, non */
    int in_tile = !spawned && argc > 0 && argv[0] && !strcmp(argv[0], DATA_DIR "/launcher5.elf");
    logf("lancement : %d argument(s), %s", argc, argc > 0 && argv[0] ? argv[0] : "-");
    if (in_tile) {                               /* l'ancienne version la relance par le chargeur ELF */
      logf("demarre dans la tuile : on laisse l'ancienne version relancer correctement");
      return EXIT_SUCCESS;
    }
    /* Lance par la chaine de demarrage (plugin OnionHEN, autoloader) ou a la main (envoi au
       chargeur ELF : « payload.elf », mise a jour : --spawned) ? Seule la chaine de
       demarrage autorise l'envoi de la liste « Au demarrage » (sinon : double kstuff). */
    const char *a0 = argc > 0 && argv[0] ? argv[0] : "";
    const char *base = strrchr(a0, '/');
    base = base ? base + 1 : a0;
    int chain = !spawned && *base && strcmp(base, "payload.elf");
    mkdir(DATA_DIR "/cache", 0755);
    if (chain) {
      FILE *cf = fopen(DATA_DIR "/cache/boot_chain", "w");
      if (cf) { fprintf(cf, "%s\n", base); fclose(cf); }
    } else remove(DATA_DIR "/cache/boot_chain");
    logf("lance %s", chain ? "par la chaine de demarrage" : "a la main");
  }
  single_instance();
  if (install_relay_app()) { notify("Launcher5 : impossible d'installer l'application relais"); return EXIT_FAILURE; }
  install_wallpapers();
  { pthread_t t; if (!pthread_create(&t, NULL, xp_thread, NULL)) pthread_detach(t); }   /* Xplorer */
#ifdef L5_HAVE_GPU
  if (!after_update) install_gpu_app();          /* tuile de l'ancienne version peut-etre encore ouverte */
  { pthread_t t; if (!pthread_create(&t, NULL, dir_server, NULL)) pthread_detach(t); }   /* dossiers pour l'interface GPU */
#endif
  if (write_file(UI_PATH, ui_elf, ui_elf_end - ui_elf, 0755)) {
    notify("Launcher5 : impossible d'écrire l'interface dans /data");
    return EXIT_FAILURE;
  }

  close_disc_player();                           /* avant l'interface et les payloads au demarrage */
  char status[16];
  int first = starter_setup(status, sizeof status);   /* premier lancement : console prete a jouer */
  /* juste apres une mise a jour, la tuile de l'ancienne version peut etre encore ouverte : un
     payload noyau envoye maintenant ferait planter la console. La liste attend l'allumage. */
  int sent = 0;
  if (after_update) logf("mise a jour : la liste partira au prochain allumage");
  else sent = boot_list(first);                  /* payloads au demarrage, sans interface ouverte */
  if (status[0] && strcmp(status, "done")) {
    if (!strcmp(status, "fail") || !status[0]) write_word(STARTER_NOTE, "fail");
    else if (sent > 0) write_word(STARTER_NOTE, strcmp(status, "ok") ? "partial" : "sent");
    else write_word(STARTER_NOTE, strcmp(status, "ok") ? "partial" : "next");
  }
  pid_t ui = start_ui();
  if (ui < 0) { notify("Launcher5 : impossible de lancer l'interface"); return EXIT_FAILURE; }

  char type[1024], value[1024], cwd[1024];
  struct stat st;
  for (;;) {
    /* 1) l'interface est ouverte : on attend une demande */
    int launched = 0, tick = 0;
    while (ui > 0 && alive(ui)) {
      if (++tick % 30 == 0 && appinfo_sig() != catalog_sig) {   /* toutes les 3 s */
        logf("base des applications modifiee : liste des jeux mise a jour");
        export_catalog();
      }
      if (!stat(REQUEST_PATH, &st) && !read_request(type, value, cwd, sizeof type)) {
        logf("demande : %s %s", type, value);
        if (!strcmp(type, "close")) {
          /* fermeture propre par le systeme : la PS5 revient a son accueil */
          int app = sceSystemServiceGetAppIdOfRunningBigApp();
          logf("fermeture du launcher par le systeme : %d", app > 0 ? sceSystemServiceKillApp(app, -1, 0, 0) : -1);
          break;
        }
        if (!strcmp(type, "elf")) {
          /* mise a jour : la nouvelle version est lancee ici ; au demarrage elle arrete
             ce bootstrap. Si elle ne prend pas la main, le chargeur ELF (port 9021) essaie. */
          for (int i = 0; i < 30 && alive(ui); i++) usleep(100 * 1000);
          if (alive(ui)) stop_ui(ui);
          if (spawn_payload(value)) logf("mise a jour : lancement direct impossible");
          else sleep(8);                              /* la nouvelle version nous arrete avant la fin */
          logf("mise a jour : pas de relais, essai par le chargeur ELF");
          if (!send_to_loader(value)) sleep(8);
          logf("mise a jour impossible : on garde cette version");
          notify("Launcher5 : la mise à jour n'a pas pu démarrer");
          ui = start_ui();
          continue;
        }
        if (!strcmp(type, "uninstall")) {
          /* Parametres › Desinstaller : la tuile est fermee puis desinstallee (une appli de
             /system_ex ne peut pas etre supprimee depuis l'accueil de la PS5) */
          int app = sceSystemServiceGetAppIdOfRunningBigApp();
          char tid[32] = { 0 };
          if (app > 0) sceSystemServiceGetAppTitleId(app, tid);
          if (app > 0 && !strcmp(tid, RELAY_ID)) sceSystemServiceKillApp(app, -1, 0, 0);
          for (int i = 0; i < 30 && alive(ui); i++) usleep(100 * 1000);
          if (alive(ui)) stop_ui(ui);
          /* la tuile doit etre completement fermee : sinon le systeme refuse (0x80f50005) */
          for (int i = 0; i < 50; i++) {
            int a2 = sceSystemServiceGetAppIdOfRunningBigApp();
            char t2[32] = { 0 };
            if (a2 > 0) sceSystemServiceGetAppTitleId(a2, t2);
            if (a2 <= 0 || strcmp(t2, RELAY_ID)) break;
            usleep(100 * 1000);
          }
          usleep(1500 * 1000);
          sceAppInstUtilInitialize();
          logf("desinstallation : demande au systeme (%d)", sceAppInstUtilAppUnInstall(RELAY_ID));
          sleep(2);
          xp_delete("/user/app/" RELAY_ID);
          xp_delete("/user/appmeta/" RELAY_ID);
#ifdef L5_HAVE_GPU
          remount_system_ex_rw();
          xp_delete(GPU_DIR);
#endif
          /* le systeme repond OK sans retirer une appli installee par dossier : on efface
             nous-memes son inscription, la tuile disparait au prochain redemarrage */
          logf("desinstallation : inscription retiree des bases (%d lignes)", forget_app(RELAY_ID));
          if (!strcmp(value, "tout")) {
            logf("desinstallation : dossier " DATA_DIR " efface");
            xp_delete(DATA_DIR);
          }
          notify("Launcher5 désinstallé : redémarre la console pour retirer la tuile");
          goto fin;
        }
        if (!strcmp(type, "stop")) {
          /* menu rapide « Fermer le launcher » : l'interface et ce bootstrap s'arretent ; la
             tuile rouvre plus rien tant que launcher5.elf n'est pas renvoye (ou l'allumage) */
          int app = sceSystemServiceGetAppIdOfRunningBigApp();
          char tid[32] = { 0 };
          if (app > 0) sceSystemServiceGetAppTitleId(app, tid);
          if (app > 0 && !strcmp(tid, RELAY_ID)) sceSystemServiceKillApp(app, -1, 0, 0);
          for (int i = 0; i < 30 && alive(ui); i++) usleep(100 * 1000);
          if (alive(ui)) stop_ui(ui);
          logf("launcher ferme depuis le menu rapide");
          goto fin;
        }
        if (!strcmp(type, "restart")) {
          /* menu rapide : l'interface se ferme, on la relance aussitot */
          for (int i = 0; i < 30 && alive(ui); i++) usleep(100 * 1000);
          if (alive(ui)) stop_ui(ui);
          logf("interface relancee");
          ui = start_ui();
          continue;
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
#ifdef L5_HAVE_GPU
    {
      struct stat gk;
      if (gpu_pending && !launched && stat(GPU_OK, &gk) && time(NULL) - gpu_started < 60) {
        /* l'interface GPU s'est arretee avant sa premiere image : dessin classique tout de suite */
        FILE *f = fopen(GPU_FAILED, "w");
        if (f) { fprintf(f, "%s\n", L5_VERSION); fclose(f); }
        gpu_pending = 0;
        logf("interface GPU arretee sans image : relance immediate en dessin classique");
        notify("Launcher5 : affichage GPU indisponible, mode classique");
        ui = start_ui();
        continue;
      }
    }
#endif
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
#ifdef L5_HAVE_GPU
          pid_t p = gpu_wanted() ? gpu_pid() : -1;
          if (p > 0) {                           /* la tuile ouvre l'interface GPU elle-meme */
            gpu_log_from = log_size() > 65536 ? log_size() - 65536 : 0;   /* la fin du journal suffit */
            logf("tuile Launcher5 ouverte : interface GPU (pid %d, droits %d)", p, raise_app(p));
            gpu_pending = 1;
            gpu_started = time(NULL);
            ui = p;
          } else
#endif
          {
            logf("tuile Launcher5 ouverte : affichage du launcher");
            sceSystemServiceKillApp(app, -1, 0, 0);
            usleep(400 * 1000);
            ui = start_ui();
          }
        }
      }
      usleep(300 * 1000);
    }
  }
fin:
  logf("fin du bootstrap");
  remove(PID_PATH);
  sceUserServiceTerminate();
  return EXIT_SUCCESS;
}
