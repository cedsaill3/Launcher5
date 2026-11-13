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

int plat_init(void) {
  mkdir("fake_ps5/data", 0755);
  mkdir(plat_data_dir(), 0755);
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
