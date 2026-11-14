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
#include <sys/statvfs.h>
#include <unistd.h>

#include "platform.h"
#include "i18n.h"

#define DATA_DIR "/data/launcher5"
#define REQUEST_PATH DATA_DIR "/request"
#define LOG_PATH DATA_DIR "/launcher.log"

int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceParamGetInt(int id, int *value);
int sceKernelGetCpuTemperature(int *temp);

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
  *free_mb = free_mb_of("/user");
  int t = -1;
  if (sceKernelGetCpuTemperature(&t) < 0) t = -1;
  *temp_c = t;
}

/* ------------------------------------------------------------------ */
/* HTTPS (libcurl) : base de jaquettes Launcher5 (GitHub)               */
/* ------------------------------------------------------------------ */
#include <curl/curl.h>

/* certificats racines publics (Let's Encrypt, USERTrust, DigiCert) : ceux qui signent
   les serveurs de GitHub, ou est hebergee la base de jaquettes */
static const char ROOTS_PEM[] =
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
  "CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
  "R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
  "MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
  "ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
  "EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
  "+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
  "ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
  "AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
  "zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
  "tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
  "/q4AaOeMSQ+2b1tbFfLn\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB\n"
  "iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl\n"
  "cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV\n"
  "BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw\n"
  "MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV\n"
  "BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n"
  "aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy\n"
  "dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK\n"
  "AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B\n"
  "3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY\n"
  "tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/\n"
  "Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2\n"
  "VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT\n"
  "79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6\n"
  "c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT\n"
  "Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l\n"
  "c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee\n"
  "UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE\n"
  "Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd\n"
  "BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G\n"
  "A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF\n"
  "Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO\n"
  "VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3\n"
  "ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs\n"
  "8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR\n"
  "iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze\n"
  "Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ\n"
  "XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/\n"
  "qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB\n"
  "VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB\n"
  "L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG\n"
  "jjxDah2nGN59PRbxYvnKkKj9\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL\n"
  "MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n"
  "eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n"
  "JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx\n"
  "MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT\n"
  "Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg\n"
  "VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm\n"
  "aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo\n"
  "I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng\n"
  "o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G\n"
  "A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD\n"
  "VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB\n"
  "zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW\n"
  "RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
  "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
  "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
  "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
  "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
  "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
  "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
  "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
  "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
  "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
  "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
  "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
  "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
  "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
  "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
  "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
  "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
  "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
  "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
  "MrY=\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIICPzCCAcWgAwIBAgIQBVVWvPJepDU1w6QP1atFcjAKBggqhkjOPQQDAzBhMQsw\n"
  "CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu\n"
  "ZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe\n"
  "Fw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUw\n"
  "EwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20x\n"
  "IDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEczMHYwEAYHKoZIzj0CAQYF\n"
  "K4EEACIDYgAE3afZu4q4C/sLfyHS8L6+c/MzXRq8NOrexpu80JX28MzQC7phW1FG\n"
  "fp4tn+6OYwwX7Adw9c+ELkCDnOg/QW07rdOkFFk2eJ0DQ+4QE2xy3q6Ip6FrtUPO\n"
  "Z9wj/wMco+I+o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAd\n"
  "BgNVHQ4EFgQUs9tIpPmhxdiuNkHMEWNpYim8S8YwCgYIKoZIzj0EAwMDaAAwZQIx\n"
  "AK288mw/EkrRLTnDCgmXc/SINoyIJ7vmiI1Qhadj+Z4y3maTD/HMsQmP3Wyr+mt/\n"
  "oAIwOWZbwmSNuJ5Q3KjVSaLtx9zRSX8XAbjIho9OjIgrqJqpisXRAL34VOKa5Vt8\n"
  "sycX\n"
  "-----END CERTIFICATE-----\n"
;

typedef struct { unsigned char *p; size_t n, max; } NetBuf;

static size_t net_write(char *d, size_t sz, size_t nm, void *u) {
  NetBuf *b = u;
  size_t k = sz * nm;
  if (b->n + k > b->max) return 0;              /* trop gros : on arrete */
  unsigned char *q = realloc(b->p, b->n + k + 1);
  if (!q) return 0;
  memcpy(q + b->n, d, k);
  b->p = q; b->n += k; b->p[b->n] = 0;
  return k;
}

long plat_https_get(const char *url, unsigned char **data, size_t *len, size_t max) {
  static int ready;
  if (!ready) { curl_global_init(CURL_GLOBAL_DEFAULT); ready = 1; }
  *data = NULL; *len = 0;
  CURL *c = curl_easy_init();
  if (!c) return 0;
  NetBuf b = { NULL, 0, max };
  struct curl_blob ca = { (void *)ROOTS_PEM, sizeof ROOTS_PEM - 1, CURL_BLOB_NOCOPY };
  curl_easy_setopt(c, CURLOPT_URL, url);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, net_write);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
  curl_easy_setopt(c, CURLOPT_CAINFO_BLOB, &ca);
  curl_easy_setopt(c, CURLOPT_CAPATH, NULL);
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 6L);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 25L);
  curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(c, CURLOPT_USERAGENT, "Mozilla/5.0");
  long code = 0;
  CURLcode r = curl_easy_perform(c);
  if (r == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
  else {
    char *ip = NULL;
    curl_easy_getinfo(c, CURLINFO_PRIMARY_IP, &ip);
    plat_log("reseau : %s (adresse %s)", curl_easy_strerror(r), ip && *ip ? ip : "introuvable");
  }
  curl_easy_cleanup(c);
  if (code == 200 && b.p) { *data = b.p; *len = b.n; }
  else free(b.p);
  return code;
}
