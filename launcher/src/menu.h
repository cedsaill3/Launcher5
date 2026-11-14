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
#define SFX_PACKS 5
#define BOOT_SOUNDS 6

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
  uint32_t vibe, vibe2;     /* couleurs tirees de la jaquette (theme « couleurs auto ») */
  int  has_vibe;
  void *boxtex;              /* jaquette boite fabriquee (geree par l'interface) */
  int  box_state, box_custom; /* 0 a faire, 1 en cours, 2 prete, 3 impossible */
  long long box_sig;         /* jaquette perso vue lors de la fabrication */
  int  box_pref;             /* jaquette deja preparee sur le disque (prechargement) */
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
  long playtime;            /* temps de jeu cumule, en secondes */
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
  char (*favs)[ID_LEN]; int nfavs;    /* favoris */
  int saver;                          /* economiseur d'ecran apres N minutes (0 = jamais) */
  int sfx_pack;                       /* style des sons d'interface (0..SFX_PACKS-1) */
  int boot_snd;                       /* son de demarrage (0..BOOT_SOUNDS-1, le dernier = aucun) */
  int hide_ip;                        /* ne pas afficher l'adresse IP a l'ecran */
  int boxes;                          /* jaquettes boites verticales (sinon icones carrees) */
  int covers_online;                  /* jaquettes officielles telechargees (catalogue PlayStation) */
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
int  menu_is_fav(Menu *m, const char *id);
void menu_set_fav(Menu *m, const char *id, int fav);

/* Statistiques (sections intelligentes) */
Stat *menu_stat(Menu *m, const char *id, int create);
void  menu_record_launch(Menu *m, const char *id);
int   menu_import_sessions(Menu *m, const char *path);   /* temps de jeu note par le bootstrap */

/* Chargement / enregistrement */
int  menu_load(Menu *m, const char *path);
int  menu_save(Menu *m, const char *path);
void menu_merge_new_apps(Menu *m);    /* ajoute a l'accueil les applications pas encore rangees */
void menu_prune_missing(Menu *m);     /* retire les references vers des applications desinstallees */
void menu_reset(Menu *m);             /* oublie dossiers, noms, masques et stats (garde les applications) */
int  menu_export_catalog(Menu *m, const char *path);  /* liste complete pour le PC (catalog.json) */
