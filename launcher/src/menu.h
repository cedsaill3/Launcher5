/*
 * Launcher5 - modele du menu : applications, dossiers, ordre, noms
 * personnalises, elements masques et statistiques d'utilisation.
 *
 * Le menu est enregistre en JSON (menu.json) et peut etre modifie a la
 * manette sur la TV ou depuis le PC.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#pragma once
#include <stdint.h>
#include <time.h>

#define ID_LEN    40
#define NAME_LEN  96
#define PATH_LEN  512

/* Type de contenu */
enum {
  KIND_PS5 = 0,     /* jeu ou application PS5 installe */
  KIND_PS4,         /* jeu PS4 installe */
  KIND_HOMEBREW,    /* homebrew (/data/homebrew) */
  KIND_PAYLOAD,     /* fichier .elf a envoyer au chargeur */
  KIND_MEDIA,       /* application officielle (Netflix, YouTube…) */
  KIND_SYSTEM,      /* raccourci systeme (parametres, accueil PS5) */
  KIND_COUNT
};

/* Presentation des jeux */
enum { VIEW_FLOW, VIEW_GRID, VIEW_CLASSIC, VIEW_COUNT };

/* Precisions sur le contenu */
#define APPF_MEDIA     1   /* lecture video / musique (aussi pour les homebrews) */
#define APPF_EMU       2   /* emulateur */
#define APPF_INSTALLED 4   /* installe comme application : se lance par son identifiant */

typedef struct App {
  char id[ID_LEN];          /* identifiant unique (ex. PPSA01234, hb:nom, elf:nom) */
  char title[NAME_LEN];     /* nom d'origine */
  char icon[PATH_LEN];      /* chemin de la jaquette (peut etre vide) */
  char path[PATH_LEN];      /* chemin utile au lancement (elf, dossier du homebrew) */
  int  kind;
  int  flags;               /* APPF_... */
  char bg[PATH_LEN];        /* image de fond (illustration du jeu, peut etre vide) */
  void *tex;                /* texture de la jaquette (geree par l'interface) */
  void *bgtex;              /* fond flou (gere par l'interface) */
  void *tex_s, *tex_l, *tex_xl; /* tuiles pre-calculees (petite, selectionnee, cover flow) */
  int  tex_failed, bg_failed;
} App;

typedef struct Node {
  int is_folder;
  char ref[ID_LEN];         /* application referencee (si ce n'est pas un dossier) */
  char name[NAME_LEN];      /* nom du dossier */
  uint32_t color;           /* couleur du dossier 0xRRGGBB */
  struct Node *items;       /* contenu du dossier */
  int count, cap;
  int mark;                 /* marqueur temporaire (deplacements) */
} Node;

typedef struct Stat {
  char id[ID_LEN];
  int launches;
  time_t last;
} Stat;

typedef struct Menu {
  App *apps; int napps, capapps;      /* tout ce qui a ete trouve sur la console */
  Node root;                          /* l'accueil (dossier racine) */
  char (*hidden)[ID_LEN]; int nhidden;
  struct { char id[ID_LEN]; char name[NAME_LEN]; } *names; int nnames;
  Stat *stats; int nstats;
  char theme[32];                     /* theme choisi */
  int show_folders;                   /* section « Mes dossiers » visible */
  int view;                           /* VIEW_* : presentation des jeux */
} Menu;

/* Applications */
App *menu_add_app(Menu *m, const App *a);
App *menu_find_app(Menu *m, const char *id);
const char *menu_display_name(Menu *m, const App *a);

/* Noeuds (accueil et dossiers) */
Node *node_insert(Node *parent, int index, const Node *n);
void  node_remove(Node *parent, int index, Node *out);
void  node_free(Node *n);
Node *menu_new_folder(Menu *m, Node *parent, int index, const char *name, uint32_t color);

/* Personnalisation */
int  menu_is_hidden(Menu *m, const char *id);
void menu_set_hidden(Menu *m, const char *id, int hidden);
void menu_rename_app(Menu *m, const char *id, const char *name);

/* Statistiques (sections intelligentes) */
Stat *menu_stat(Menu *m, const char *id, int create);
void  menu_record_launch(Menu *m, const char *id);

/* Chargement / enregistrement */
int  menu_load(Menu *m, const char *path);
int  menu_save(Menu *m, const char *path);
void menu_merge_new_apps(Menu *m);    /* ajoute a l'accueil les applications pas encore rangees */
void menu_prune_missing(Menu *m);     /* retire les references vers des applications desinstallees */
void menu_reset(Menu *m);             /* oublie dossiers, noms, masques et stats (garde les applications) */
int  menu_export_catalog(Menu *m, const char *path);  /* liste complete pour le PC (catalog.json) */
