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
int  plat_sys_lang(void);               /* code de langue du systeme PS5 (2 = francais, 1 = anglais…) */
void plat_poll_system(int *overlaid);   /* evenements du systeme (menu PS, arriere-plan) */
int  plat_auto_return(int set);         /* -1 : lire ; 0/1 : regler le retour automatique */

/* Lance une application. Renvoie 0 si c'est parti ; msg recoit un message a afficher. */
int plat_launch(Menu *m, App *a, char *msg, size_t msglen);

/* Recherche du contenu (commun aux deux plateformes) */
void scan_all(Menu *m);
