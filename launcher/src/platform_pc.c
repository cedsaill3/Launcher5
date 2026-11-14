/*
 * Launcher5 - plateforme PC (test sans console).
 *
 * Le contenu est lu dans le dossier fake_ps5/ qui reproduit l'arborescence
 * de la PS5. Le lancement est simule.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <stdio.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/statvfs.h>

#include "platform.h"
#include "i18n.h"

/* L5_LANG=fr|en|es|de|it|pt pour tester une langue, sinon LANG du systeme */
int plat_sys_lang(void) {
  const char *l = getenv("L5_LANG");
  if (!l || !*l) l = getenv("LANG");
  if (!l) return 2;
  static const struct { const char *p; int code; } M[] = { { "fr", 2 }, { "en", 1 }, { "es", 3 }, { "de", 4 }, { "it", 5 }, { "pt", 7 } };
  for (size_t i = 0; i < sizeof M / sizeof M[0]; i++) if (!strncmp(l, M[i].p, 2)) return M[i].code;
  return 1;
}

const char *plat_root(void) { return "fake_ps5"; }
const char *plat_data_dir(void) { return "fake_ps5/data/launcher5"; }

/* comme le bootstrap sur PS5 : fonds fournis copies s'ils manquent */
static void install_wallpapers(void) {
  char src[256], dst[256];
  mkdir("fake_ps5/data/launcher5/fonds", 0755);
  for (int n = 1; n <= 7; n++) {
    snprintf(src, sizeof src, "assets/fonds/launcher5-%02d.jpg", n);
    snprintf(dst, sizeof dst, "fake_ps5/data/launcher5/fonds/launcher5-%02d.jpg", n);
    FILE *in = fopen(src, "rb"), *out;
    if (!in) continue;
    if ((out = fopen(dst, "rb"))) { fclose(out); fclose(in); continue; }
    if ((out = fopen(dst, "wb"))) {
      char buf[65536]; size_t k;
      while ((k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, out);
      fclose(out);
    }
    fclose(in);
  }
  mkdir("fake_ps5/data/launcher5/sons", 0755);
  FILE *in = fopen("assets/sons/demarrage-techno.mp3", "rb"), *out = NULL;
  if (in && !(out = fopen("fake_ps5/data/launcher5/sons/demarrage-techno.mp3", "rb")) &&
      (out = fopen("fake_ps5/data/launcher5/sons/demarrage-techno.mp3", "wb"))) {
    char buf[65536]; size_t k;
    while ((k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, out);
  }
  if (out) fclose(out);
  if (in) fclose(in);
}

int plat_init(void) {
  mkdir("fake_ps5/data", 0755);
  mkdir(plat_data_dir(), 0755);
  install_wallpapers();
  return 0;
}

void plat_quit(void) {}
void plat_after_window(void) {}
void plat_poll_system(int *overlaid) { *overlaid = 0; }
int plat_auto_return(int set) { static int v = 1; if (set >= 0) v = set; return v; }
void plat_log(const char *fmt, ...) { (void)fmt; }

int plat_launch(Menu *m, App *a, char *msg, size_t n) {
  static const char *what[KIND_COUNT] = {
    "Jeu PS5", "Jeu PS4", "Homebrew", "Payload", "Application", "Raccourci"
  };
  snprintf(msg, n, tr("Simulation : %s « %s » lancé"), tr(what[a->kind]), menu_display_name(m, a));
  printf("[launch] %s (%s)\n", a->id, a->path);
  fflush(stdout);
  return 0;
}

/* Adresse IP locale : on « vise » une adresse publique en UDP (rien n'est envoye)
   et on lit l'adresse choisie par le systeme pour sortir. */
static void local_ip(char *ip, size_t n) {
  ip[0] = 0;
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) return;
  struct sockaddr_in a = { 0 };
  a.sin_family = AF_INET;
  a.sin_port = htons(53);
  a.sin_addr.s_addr = htonl(0x08080808);
  socklen_t len = sizeof a;
  if (!connect(s, (struct sockaddr *)&a, sizeof a) && !getsockname(s, (struct sockaddr *)&a, &len))
    inet_ntop(AF_INET, &a.sin_addr, ip, (socklen_t)n);
  close(s);
  if (!strcmp(ip, "0.0.0.0")) ip[0] = 0;
}

static long long free_mb_of(const char *path) {
  struct statvfs v;
  if (statvfs(path, &v)) return -1;
  return (long long)v.f_bavail * (long long)v.f_frsize / (1024 * 1024);
}

void plat_sysinfo(char *ip, size_t iplen, long long *free_mb, int *temp_c) {
  local_ip(ip, iplen);
  *free_mb = free_mb_of(".");
  *temp_c = -1;
  const char *t = getenv("L5_TEMP");   /* test : temperature simulee */
  if (t) *temp_c = atoi(t);
}

/* HTTPS sur PC (tests) : passe par la commande curl */
long plat_https_get(const char *url, unsigned char **data, size_t *len, size_t max) {
  *data = NULL; *len = 0;
  if (strchr(url, '\'') || strlen(url) > 900) return 0;
  char tmp[64], cmd[1200];
  snprintf(tmp, sizeof tmp, "/tmp/l5net_%d_%p", (int)getpid(), (void *)data);
  snprintf(cmd, sizeof cmd, "curl -s -L --compressed -m 25 -A Mozilla/5.0 -o '%s' -w '%%{http_code}' '%s'", tmp, url);
  FILE *f = popen(cmd, "r");
  if (!f) return 0;
  long code = 0;
  if (fscanf(f, "%ld", &code) != 1) code = 0;
  pclose(f);
  FILE *in = fopen(tmp, "rb");
  if (in && code == 200) {
    fseek(in, 0, SEEK_END);
    long n = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (n > 0 && (size_t)n <= max && (*data = malloc((size_t)n + 1))) {
      *len = fread(*data, 1, (size_t)n, in);
      (*data)[*len] = 0;
    } else code = 413;
  }
  if (in) fclose(in);
  remove(tmp);
  return code;
}

/* PC : pas de chargeur ELF */
int plat_send_elf(const char *path) {
  plat_log("mise a jour (PC) : %s serait envoye au chargeur ELF", path);
  return -1;
}
