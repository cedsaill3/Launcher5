/*
 * Launcher5 - interface GPU (application native) : complements du module d'execution.
 *
 * Une application native n'a pas acces a certaines fonctions disponibles pour les payloads
 * (lecture des dossiers, resolution DNS, sortie du processus...). Elles sont fournies ici,
 * a partir des fonctions du noyau et des bibliotheques systeme accessibles aux applications.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>

/* tas de l'application : textures et images des jaquettes */
const size_t ps5_opengl_heap_size = 1024u * 1024u * 1024u;

/* ------------------------------------------------------------------ */
/* Sortie : retour a l'accueil comme une application (l'appel systeme  */
/* _exit est interdit aux applications)                                */
/* ------------------------------------------------------------------ */
int sceSystemServiceLoadExec(const char *path, char *const argv[]);
int sceSystemServiceGetAppIdOfRunningBigApp(void);
int sceSystemServiceKillApp(int app, int how, int a, int b);
int sceKernelUsleep(unsigned us);

__attribute__((noreturn)) void l5_app_exit(int status) {
  (void)status;
  fflush(NULL);
  sceSystemServiceLoadExec("exit", NULL);
  for (int i = 0; i < 20; i++) sceKernelUsleep(100000);
  int app = sceSystemServiceGetAppIdOfRunningBigApp();
  if (app > 0) sceSystemServiceKillApp(app, -1, 0, 0);
  for (;;) sceKernelUsleep(1000000);
}
__attribute__((noreturn)) void catchReturnFromMain(int status) { l5_app_exit(status); }

__attribute__((noreturn)) void __assert(const char *function, const char *file, int line, const char *expression) {
  fprintf(stderr, "assertion failed: %s (%s:%d, %s)\n", expression, file, line, function);
  abort();
}

/* fcntl : la console refuse FD_CLOEXEC aux applications ; sans importance ici (pas d'exec) */
int __real_fcntl(int fd, int cmd, ...);
int __wrap_fcntl(int fd, int cmd, ...) {
  va_list a;
  va_start(a, cmd);
  long arg = va_arg(a, long);
  va_end(a);
  int r = __real_fcntl(fd, cmd, arg);
  if (r < 0 && cmd == F_SETFD) return 0;
  return r;
}

/* ------------------------------------------------------------------ */
/* Lecture des dossiers par le noyau                                   */
/* ------------------------------------------------------------------ */
/* Une application n'a pas le droit de lister un dossier : la liste est demandee au
   bootstrap (payload) par une connexion locale, port 5051, 127.0.0.1 seulement. */
struct _dirdesc { int fd; int n, pos; struct dirent *ents; };

/* liste gardee 2 s par dossier : une interface peut relire le meme dossier a chaque image */
static struct { char path[256]; char *txt; size_t len; unsigned long long at; long long stamp; } DCACHE[16];
static pthread_mutex_t dcache_mx = PTHREAD_MUTEX_INITIALIZER;
static unsigned long long l5_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (unsigned long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static int l5_dir_fetch_net(const char *path, char **out, size_t *len);
static long long l5_dir_stamp(const char *path) {   /* change si un element est ajoute, retire ou renomme */
  struct stat st;
  if (stat(path, &st)) return -1;
  return (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec + (long long)st.st_size * 7;
}
static int l5_dir_fetch(const char *path, char **out, size_t *len) {
  unsigned long long now = l5_ms();
  long long stamp = l5_dir_stamp(path);
  pthread_mutex_lock(&dcache_mx);
  for (int i = 0; i < 16; i++)
    if (DCACHE[i].txt && now - DCACHE[i].at < 2000 && DCACHE[i].stamp == stamp && !strcmp(DCACHE[i].path, path)) {
      *out = malloc(DCACHE[i].len + 1);
      if (*out) { memcpy(*out, DCACHE[i].txt, DCACHE[i].len + 1); *len = DCACHE[i].len; }
      pthread_mutex_unlock(&dcache_mx);
      return *out ? 0 : -1;
    }
  pthread_mutex_unlock(&dcache_mx);
  if (l5_dir_fetch_net(path, out, len)) return -1;
  if (strlen(path) < sizeof DCACHE[0].path) {
    pthread_mutex_lock(&dcache_mx);
    int v = 0;
    for (int i = 1; i < 16; i++) if (DCACHE[i].at < DCACHE[v].at) v = i;
    free(DCACHE[v].txt);
    DCACHE[v].txt = malloc(*len + 1);
    if (DCACHE[v].txt) {
      memcpy(DCACHE[v].txt, *out, *len + 1);
      DCACHE[v].len = *len;
      DCACHE[v].at = now;
      DCACHE[v].stamp = stamp;
      snprintf(DCACHE[v].path, sizeof DCACHE[v].path, "%s", path);
    }
    pthread_mutex_unlock(&dcache_mx);
  }
  return 0;
}
static int l5_dir_fetch_net(const char *path, char **out, size_t *len) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return -1;
  struct timeval tv = { 2, 0 };                     /* jamais d'attente sans fin */
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_len = sizeof a;
  a.sin_family = AF_INET;
  a.sin_port = htons(5051);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(s, (struct sockaddr *)&a, sizeof a)) { close(s); return -1; }
  size_t pl = strlen(path);
  if (send(s, path, pl, 0) != (ssize_t)pl || send(s, "\n", 1, 0) != 1) { close(s); return -1; }
  size_t cap = 8192, n = 0;
  char *buf = malloc(cap);
  for (;;) {
    if (!buf) { close(s); return -1; }
    if (n + 4096 > cap) { char *nb = realloc(buf, cap *= 2); if (!nb) { free(buf); close(s); return -1; } buf = nb; }
    ssize_t r = recv(s, buf + n, cap - n - 1, 0);
    if (r <= 0) break;
    n += (size_t)r;
  }
  close(s);
  buf[n] = 0;
  *out = buf;
  *len = n;
  return 0;
}

static DIR *l5_opendir(const char *path);
/* taille d'un dossier (calculee par le bootstrap ; un gros jeu peut prendre quelques secondes) */
int l5_du(const char *path, long long *size, long long *files) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return -1;
  struct timeval tv = { 60, 0 };
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_len = sizeof a;
  a.sin_family = AF_INET;
  a.sin_port = htons(5051);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  char q[1100], buf[128];
  int ql = snprintf(q, sizeof q, "?du %s\n", path);
  if (connect(s, (struct sockaddr *)&a, sizeof a) || send(s, q, ql, 0) != ql) { close(s); return -1; }
  size_t n = 0;
  for (ssize_t r; n < sizeof buf - 1 && (r = recv(s, buf + n, sizeof buf - 1 - n, 0)) > 0;) n += (size_t)r;
  close(s);
  buf[n] = 0;
  return sscanf(buf, "%lld %lld", size, files) == 2 && *size >= 0 ? 0 : -1;
}
/* espace libre et taille d'un stockage (statvfs plante dans l'application) */
int l5_df(const char *path, long long *avail, long long *total) {
  char q[1100], *txt;
  size_t len;
  snprintf(q, sizeof q, "?df %s", path);
  if (l5_dir_fetch_net(q, &txt, &len)) return -1;
  int ok = sscanf(txt, "%lld %lld", avail, total) == 2 && *avail >= 0;
  free(txt);
  return ok ? 0 : -1;
}
extern volatile const char *l5_where;
DIR *opendir(const char *path) {
  static char where[300];
  const char *before = (const char *)l5_where;
  snprintf(where, sizeof where, "liste du dossier %s (bootstrap)", path);
  l5_where = where;
  DIR *r = l5_opendir(path);
  l5_where = before;
  return r;
}
static DIR *l5_opendir(const char *path) {
  struct stat st;
  if (stat(path, &st)) return NULL;                     /* errno du systeme (ENOENT...) */
  if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return NULL; }
  char *txt;
  size_t len;
  if (l5_dir_fetch(path, &txt, &len)) { errno = EACCES; return NULL; }
  DIR *d = calloc(1, sizeof *d);
  int cnt = 0;
  for (size_t i = 0; i < len; i++) if (txt[i] == '\n') cnt++;
  if (!d || !(d->ents = calloc(cnt ? cnt : 1, sizeof *d->ents))) { free(d); free(txt); errno = ENOMEM; return NULL; }
  for (char *line = txt, *nl; (nl = strchr(line, '\n')); line = nl + 1) {
    *nl = 0;
    if (strlen(line) < 3 || line[1] != ' ') continue;
    struct dirent *e = &d->ents[d->n];
    snprintf(e->d_name, sizeof e->d_name, "%s", line + 2);
    e->d_namlen = (uint8_t)strlen(e->d_name);
    e->d_type = line[0] == 'd' ? DT_DIR : DT_REG;
    e->d_fileno = (uint32_t)(d->n + 1);
    e->d_reclen = sizeof *e;
    d->n++;
  }
  free(txt);
  d->fd = -1;
  return d;
}
struct dirent *readdir(DIR *d) { return d->pos < d->n ? &d->ents[d->pos++] : NULL; }
int closedir(DIR *d) { free(d->ents); free(d); return 0; }
void rewinddir(DIR *d) { d->pos = 0; }
int dirfd(DIR *d) { return d->fd; }

/* ------------------------------------------------------------------ */
/* Resolution DNS par le resolveur de la console (IPv4)                */
/* ------------------------------------------------------------------ */
int sceNetInit(void);
int sceNetPoolCreate(const char *name, int size, int flags);
int sceNetResolverCreate(const char *name, int pool, int flags);
int sceNetResolverStartNtoa(int rid, const char *host, struct in_addr *addr, int timeout, int retry, int flags);
int sceNetResolverDestroy(int rid);

static int l5_resolve(const char *host, struct in_addr *out) {
  static int pool = -1, inited;
  if (inet_pton(AF_INET, host, out) == 1) return 0;
  if (!inited) { inited = 1; sceNetInit(); }
  if (pool < 0) pool = sceNetPoolCreate("l5dns", 4 * 4096, 0);
  if (pool < 0) return -1;
  int rid = sceNetResolverCreate("l5dns", pool, 0);
  if (rid < 0) return -1;
  int r = sceNetResolverStartNtoa(rid, host, out, 0, 0, 0);
  sceNetResolverDestroy(rid);
  return r < 0 ? -1 : 0;
}
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
  struct in_addr a;
  *res = NULL;
  if (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET) return EAI_FAMILY;
  if (!node) a.s_addr = htonl((hints && (hints->ai_flags & AI_PASSIVE)) ? INADDR_ANY : INADDR_LOOPBACK);
  else if (l5_resolve(node, &a)) return EAI_NONAME;
  struct addrinfo *ai = calloc(1, sizeof *ai + sizeof(struct sockaddr_in));
  if (!ai) return EAI_MEMORY;
  struct sockaddr_in *sa = (struct sockaddr_in *)(ai + 1);
  sa->sin_len = sizeof *sa;
  sa->sin_family = AF_INET;
  sa->sin_addr = a;
  sa->sin_port = htons(service ? (unsigned short)atoi(service) : 0);
  ai->ai_family = AF_INET;
  ai->ai_socktype = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
  ai->ai_protocol = hints ? hints->ai_protocol : 0;
  ai->ai_addrlen = sizeof *sa;
  ai->ai_addr = (struct sockaddr *)sa;
  *res = ai;
  return 0;
}
void freeaddrinfo(struct addrinfo *ai) {
  while (ai) { struct addrinfo *n = ai->ai_next; free(ai); ai = n; }
}
const char *gai_strerror(int e) { (void)e; return "resolution impossible"; }
struct hostent *gethostbyname(const char *name) {
  static struct in_addr a;
  static char *list[2];
  static struct hostent h;
  if (l5_resolve(name, &a)) return NULL;
  list[0] = (char *)&a;
  list[1] = NULL;
  h.h_name = (char *)name;
  h.h_addrtype = AF_INET;
  h.h_length = 4;
  h.h_addr_list = list;
  return &h;
}
int getnameinfo(const struct sockaddr *sa, socklen_t sl, char *host, size_t hl, char *serv, size_t svl, int flags) {
  (void)sl; (void)flags;
  const struct sockaddr_in *in = (const void *)sa;
  if (host && hl) inet_ntop(AF_INET, &in->sin_addr, host, hl);
  if (serv && svl) snprintf(serv, svl, "%u", ntohs(in->sin_port));
  return 0;
}

/* ------------------------------------------------------------------ */
/* Petites fonctions manquantes                                        */
/* ------------------------------------------------------------------ */
char *strcasestr(const char *h, const char *n) {
  size_t l = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, l)) return (char *)h;
  return l ? NULL : (char *)h;
}
int fnmatch(const char *p, const char *s, int flags) {
  for (; *p; p++, s++) {
    if (*p == '*') {
      while (p[1] == '*') p++;
      if (!p[1]) return 0;
      for (; *s; s++) if (!fnmatch(p + 1, s, flags)) return 0;
      return 1;
    }
    if (!*s) return 1;
    if (*p != '?' && *p != *s) return 1;
  }
  return *s ? 1 : 0;
}
int isatty(int fd) { (void)fd; errno = ENOTTY; return 0; }
int mkstemp(char *t) {
  size_t l = strlen(t);
  if (l < 6) { errno = EINVAL; return -1; }
  static unsigned k = 12345;
  for (int tries = 0; tries < 100; tries++) {
    unsigned v = (k = k * 1103515245u + 12345u);
    for (int i = 0; i < 6; i++) { t[l - 6 + i] = "abcdefghijklmnopqrstuvwxyz0123456789"[v % 36]; v /= 36; }
    int fd = open(t, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0 || errno != EEXIST) return fd;
  }
  return -1;
}
struct tm *gmtime_r(const time_t *t, struct tm *r) {
  long long s = *t, d = s / 86400, rem = s % 86400;
  if (rem < 0) { rem += 86400; d--; }
  memset(r, 0, sizeof *r);
  r->tm_hour = rem / 3600;
  r->tm_min = rem % 3600 / 60;
  r->tm_sec = rem % 60;
  r->tm_wday = (int)((d + 4) % 7);
  if (r->tm_wday < 0) r->tm_wday += 7;
  long long z = d + 719468, era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
  long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, y = yoe + era * 400;
  long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  int dd = (int)(doy - (153 * mp + 2) / 5 + 1), mm = (int)(mp < 10 ? mp + 3 : mp - 9);
  if (mm <= 2) y++;
  r->tm_year = (int)(y - 1900);
  r->tm_mon = mm - 1;
  r->tm_mday = dd;
  static const int cum[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  r->tm_yday = cum[mm - 1] + dd - 1 + (leap && mm > 2);
  return r;
}

/* parties inutilisees de curl, openssl et zstd : jamais appelees par Launcher5 */
struct passwd;
ssize_t sendmmsg(int s, struct mmsghdr *__restrict m, size_t n, int f) { (void)s; (void)m; (void)n; (void)f; errno = ENOSYS; return -1; }
ssize_t recvmmsg(int s, struct mmsghdr *__restrict m, size_t n, int f, const struct timespec *__restrict t) { (void)s; (void)m; (void)n; (void)f; (void)t; errno = ENOSYS; return -1; }
int pipe2(int fd[2], int f) { (void)f; return pipe(fd); }
unsigned if_nametoindex(const char *n) { (void)n; return 0; }
int getpwuid_r(unsigned u, struct passwd *p, char *b, size_t n, struct passwd **r) { (void)u; (void)p; (void)b; (void)n; *r = NULL; return ENOENT; }
typedef struct { const char *a; void *b; const char *c; void *d; } l5_dlinfo;
int dladdr(const void *a, l5_dlinfo *i) { (void)a; (void)i; return 0; }
int mkstemps(char *t, int n) { (void)t; (void)n; errno = ENOSYS; return -1; }
void openlog(const char *i, int o, int f) { (void)i; (void)o; (void)f; }
FILE *popen(const char *c, const char *m) { (void)c; (void)m; errno = ENOSYS; return NULL; }
int pclose(FILE *s) { (void)s; errno = ENOSYS; return -1; }
int fork(void) { errno = ENOSYS; return -1; }
int _setjmp(void *e) { (void)e; return 0; }
__attribute__((noreturn)) void _longjmp(void *e, int v) { (void)e; (void)v; abort(); }
unsigned long long ZSTD_trace_decompress_begin(const void *c) { (void)c; return 0; }
void ZSTD_trace_decompress_end(unsigned long long c, const void *t) { (void)c; (void)t; }
unsigned long long ZSTD_trace_compress_begin(const void *c) { (void)c; return 0; }
void ZSTD_trace_compress_end(unsigned long long c, const void *t) { (void)c; (void)t; }
