/*
 * Launcher5 - plateforme PS5.
 *
 * L'interface tourne dans l'application relais (FAKE00000). Pour lancer un
 * jeu ou un homebrew, elle ecrit une demande que le bootstrap (resident en
 * arriere-plan) execute : il ferme l'interface puis lance le contenu choisi.
 * Les payloads sont envoyes directement au chargeur ELF de la console.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "platform.h"
#include "i18n.h"

#define DATA_DIR "/data/launcher5"
#define REQUEST_PATH DATA_DIR "/request"
#define LOG_PATH DATA_DIR "/launcher.log"

int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceParamGetInt(int id, int *value);

int plat_sys_lang(void) {
  int v = 1;
  if (sceSystemServiceParamGetInt(1 /* langue */, &v) < 0) v = 1;
  return v;
}

const char *plat_root(void) { return ""; }
const char *plat_data_dir(void) { return DATA_DIR; }

void plat_log(const char *fmt, ...) {
  FILE *f = fopen(LOG_PATH, "a");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  fprintf(f, "[ui] ");
  vfprintf(f, fmt, ap);
  fprintf(f, "\n");
  va_end(ap);
  fclose(f);
}

int plat_init(void) {
  mkdir(DATA_DIR, 0755);
  plat_log("demarrage de l'interface");
  return 0;
}

void plat_after_window(void) { sceSystemServiceHideSplashScreen(); }

void plat_quit(void) { plat_log("fin de l'interface"); }

static int write_request(const char *type, const char *value, const char *cwd) {
  char tmp[] = DATA_DIR "/request.tmp";
  FILE *f = fopen(tmp, "w");
  if (!f) return -1;
  fprintf(f, "%s\n%s\n%s\n", type, value, cwd ? cwd : "/data");
  fclose(f);
  return rename(tmp, REQUEST_PATH);
}

static int send_payload(const char *path, char *msg, size_t n) {
  FILE *f = fopen(path, "rb");
  if (!f) { snprintf(msg, n, tr("Impossible de lire %s"), path); return -1; }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size <= 0 || size > 64L * 1024 * 1024) { fclose(f); snprintf(msg, n, "%s", tr("Fichier payload vide ou invalide")); return -1; }
  char *buf = malloc((size_t)size);
  if (!buf) { fclose(f); snprintf(msg, n, "%s", tr("Mémoire insuffisante")); return -1; }
  size_t got = fread(buf, 1, (size_t)size, f);
  fclose(f);

  int s = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr = { 0 };
  addr.sin_family = AF_INET;
  addr.sin_port = htons(9021);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (s < 0 || connect(s, (struct sockaddr *)&addr, sizeof addr)) {
    snprintf(msg, n, "%s", tr("Chargeur ELF injoignable (port 9021)"));
    if (s >= 0) close(s);
    free(buf);
    return -1;
  }
  size_t off = 0;
  while (off < got) {
    ssize_t w = write(s, buf + off, got - off);
    if (w <= 0) break;
    off += (size_t)w;
  }
  close(s);
  free(buf);
  return off == got ? 0 : -1;
}

/* Cherche l'ELF principal d'un homebrew : eboot.elf, sinon le premier .elf */
static int homebrew_elf(const char *dir, char *out, size_t n) {
  snprintf(out, n, "%s/eboot.elf", dir);
  struct stat st;
  if (!stat(out, &st)) return 0;
  DIR *d = opendir(dir);
  if (!d) return -1;
  struct dirent *e;
  int found = -1;
  while ((e = readdir(d))) {
    size_t l = strlen(e->d_name);
    if (l > 4 && !strcasecmp(e->d_name + l - 4, ".elf")) {
      snprintf(out, n, "%s/%s", dir, e->d_name);
      found = 0;
      break;
    }
  }
  closedir(d);
  return found;
}

/* ------------------------------------------------------------------ */
/* Evenements du systeme : menu PS, passage en arriere-plan            */
/* ------------------------------------------------------------------ */
int sceSystemServiceGetStatus(void *status);
int sceSystemServiceReceiveEvent(void *event);

void plat_poll_system(int *overlaid) {
  static unsigned char st[256], prev_over = 0xff, prev_bg = 0xff;
  static unsigned char ev[0x2010];
  *overlaid = 0;
  memset(st, 0, sizeof st);
  if (sceSystemServiceGetStatus(st) < 0) return;
  int nevents = *(int *)st;
  unsigned char over = st[4], bg = st[5];
  if (over != prev_over || bg != prev_bg) {
    plat_log("etat systeme : menu PS %s, arriere-plan %s", over ? "ouvert" : "ferme", bg ? "oui" : "non");
    prev_over = over; prev_bg = bg;
  }
  /* on vide la file d'evenements du systeme (et on les note pour le diagnostic) */
  for (int i = 0; i < nevents && i < 8; i++) {
    memset(ev, 0, sizeof ev);
    if (sceSystemServiceReceiveEvent(ev) < 0) break;
    plat_log("evenement systeme : 0x%x", *(unsigned int *)ev);
  }
  *overlaid = bg ? 1 : over ? 2 : 0;
}

/* Retour automatique au launcher apres un jeu (lu par le bootstrap) */
int plat_auto_return(int set) {
  struct stat st;
  if (set == 0) { FILE *f = fopen(DATA_DIR "/no_auto_return", "w"); if (f) fclose(f); }
  if (set == 1) remove(DATA_DIR "/no_auto_return");
  return stat(DATA_DIR "/no_auto_return", &st) != 0;
}

/* Raccourcis systeme */
static int system_shortcut(App *a, char *msg, size_t n) {
  if (!strcmp(a->id, "sys:home")) {
    /* une appli PS5 ne doit pas s'arreter elle-meme (sinon « violation systeme ») :
       on demande au bootstrap de la fermer par le systeme, comme un jeu qu'on quitte */
    if (write_request("close", "launcher", NULL)) return 2;   /* secours : arret direct */
    plat_log("fermeture demandee au systeme");
    snprintf(msg, n, "%s", tr("Retour à l'accueil PS5…"));
    return 0;
  }
  snprintf(msg, n, "%s", tr("Raccourci inconnu"));
  return -1;
}

int plat_launch(Menu *m, App *a, char *msg, size_t n) {
  const char *name = menu_display_name(m, a);
  char path[PATH_LEN];
  switch (a->kind) {
  case KIND_PAYLOAD:
    if (send_payload(a->path, msg, n)) return -1;
    snprintf(msg, n, tr("Payload « %s » envoyé"), name);
    plat_log("payload envoye : %s", a->path);
    return 0;
  case KIND_SYSTEM:
    return system_shortcut(a, msg, n);
  case KIND_PS5: case KIND_PS4: case KIND_MEDIA:
    if (write_request("title", a->id, NULL)) { snprintf(msg, n, tr("Impossible de lancer %s"), name); return -1; }
    snprintf(msg, n, tr("Lancement de « %s »…"), name);
    plat_log("demande de lancement : %s", a->id);
    return 0;
  case KIND_HOMEBREW:
    /* homebrew installe comme application : il se lance par son identifiant */
    if (a->flags & APPF_INSTALLED) {
      if (write_request("title", a->id, NULL)) { snprintf(msg, n, tr("Impossible de lancer %s"), name); return -1; }
      snprintf(msg, n, tr("Lancement de « %s »…"), name);
      plat_log("demande de lancement homebrew installe : %s", a->id);
      return 0;
    }
    if (homebrew_elf(a->path, path, sizeof path)) { snprintf(msg, n, "%s", tr("Aucun fichier .elf dans ce homebrew")); return -1; }
    if (write_request("hb", path, a->path)) { snprintf(msg, n, tr("Impossible de lancer %s"), name); return -1; }
    snprintf(msg, n, tr("Lancement de « %s »…"), name);
    plat_log("demande de lancement homebrew : %s", path);
    return 0;
  default:
    snprintf(msg, n, "%s", tr("Ce type de contenu n'est pas encore pris en charge sur la console"));
    return -1;
  }
}
