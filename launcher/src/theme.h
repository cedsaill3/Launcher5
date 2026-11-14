/*
 * Launcher5 - themes.
 *
 * Un theme regle les couleurs, le fond (illustration du jeu, degrade ou
 * image perso), les halos, les particules, la forme des tuiles, l'horloge
 * et la musique d'ambiance. Il s'enregistre dans un petit fichier JSON
 * (dossier themes/) que l'on peut echanger et copier d'une console a l'autre.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#pragma once
#include <stdint.h>

#define THEME_FILE_LEN 128

enum { BG_ART, BG_GRADIENT, BG_IMAGE, BG_COUNT };           /* arriere-plan */
enum { PART_NONE, PART_FEW, PART_MANY, PART_COUNT };         /* quantite de particules */
enum { PSTYLE_DOTS, PSTYLE_SPARKS, PSTYLE_SNOW, PSTYLE_COUNT };
enum { CORNER_SQUARE, CORNER_ROUND, CORNER_SOFT, CORNER_COUNT };
enum { CLOCK_24, CLOCK_12, CLOCK_HIDDEN, CLOCK_COUNT };
enum { FX_NONE, FX_GRID, FX_WARP, FX_RAIN, FX_COUNT };       /* effet anime du fond */

typedef struct Theme {
  char name[48];
  uint32_t accent, accent2;       /* couleur principale et secondaire */
  uint32_t top, bottom;           /* degrade du fond */
  int bg_mode;                    /* BG_* */
  char wallpaper[THEME_FILE_LEN]; /* image perso (dans fonds/) */
  int art;                        /* intensite de l'illustration du jeu, 0..100 */
  int glow;                       /* intensite des halos, 0..100 */
  int particles;                  /* PART_* */
  int pstyle;                     /* PSTYLE_* */
  int fall;                       /* 0 : les particules montent, 1 : elles tombent */
  int corners;                    /* CORNER_* */
  int clock;                      /* CLOCK_* */
  char music[THEME_FILE_LEN];     /* "" aucune, "*" toutes a la suite, sinon un fichier de musique/ */
  int volume;                     /* 0..100 */
  int fx;                         /* FX_* : grille synthwave, hyperespace, pluie de code */
  int auto_color;                 /* 1 : couleurs prises sur la jaquette du jeu choisi */
  int sfx;                        /* volume des sons d'interface, 0 = muets */
} Theme;

int theme_builtin_count(void);
const Theme *theme_builtin(int i);

int theme_load(const char *path, Theme *t);        /* 0 si lu */
int theme_save(const char *path, const Theme *t);  /* 0 si enregistre */

/* Fichiers d'un dossier dont l'extension est dans exts (NULL final), tries par nom.
   Renvoie le nombre trouve ; *out est a liberer avec free(). */
int theme_list_files(const char *dir, const char *const *exts, char (**out)[THEME_FILE_LEN]);

/* Nom de fichier sans risque a partir d'un nom de theme */
void theme_file_name(const char *name, char *out, int n);
