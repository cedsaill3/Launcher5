/*
 * Launcher5 - ce qui depend de la plateforme (PS5 ou PC de test).
 *
 * Launcher5 (c) 2026 cedsaill
 */
#pragma once
#include <stddef.h>
#include "menu.h"

/* Prefixe ajoute aux chemins de la console ("" sur PS5, "fake_ps5" sur PC) */
const char *plat_root(void);

/* Dossier ou le launcher enregistre son menu */
const char *plat_data_dir(void);

int  plat_init(void);
void plat_after_window(void);           /* apres creation de la fenetre (ecran de demarrage) */
void plat_log(const char *fmt, ...);    /* journal de diagnostic */
void plat_quit(void);
int  plat_sys_lang(void);
/* Infos de la console : adresse IP locale (vide si inconnue), espace libre en Mo
   (-1 si inconnu), temperature du processeur en degres (-1 si inconnue) */
/* telechargement HTTPS (base de jaquettes) : renvoie le code HTTP (200 = ok, data a liberer),
   0 si pas de reseau. max : taille maximale acceptee. Appelable depuis un autre fil. */
long plat_https_get(const char *url, unsigned char **data, size_t *len, size_t max);
/* envoie un ELF au chargeur de la console (port 9021) : 0 si envoye */
int plat_send_elf(const char *path);
int plat_restart_ui(void);               /* relance l'interface (0 : demande envoyee) */
int plat_close_ui(void);                 /* fermeture par le systeme, rouverte par la tuile (0 : demande envoyee) */
int plat_procs(char *out, size_t n);      /* noms des processus, un par ligne */
void plat_sysinfo(char *ip, size_t iplen, long long *free_mb, int *temp_c);               /* code de langue du systeme PS5 (2 = francais, 1 = anglais…) */
void plat_poll_system(int *overlaid);   /* evenements du systeme (menu PS, arriere-plan) */
int  plat_auto_return(int set);         /* -1 : lire ; 0/1 : regler le retour automatique */

/* Lance une application. Renvoie 0 si c'est parti ; msg recoit un message a afficher. */
int plat_launch(Menu *m, App *a, char *msg, size_t msglen);

/* Recherche du contenu (commun aux deux plateformes) */
void scan_all(Menu *m);
int payload_excluded(const char *name);
int payload_hidden(const char *name);     /* outils de developpement… : masques */   /* byepervisor, HyperHEN… : jamais affiches ni lances */
int payload_icon_path(const char *file, const char *id, char *out, size_t n);   /* icone d'un payload connu */
void payload_icons_reset(void);                  /* listes d'icones a relire */
