/*
 * Launcher5 - langues de l'interface.
 *
 * Les textes sont ecrits en francais dans le code ; tr() renvoie leur
 * traduction dans la langue de la console (anglais si elle n'est pas prise
 * en charge). Langues : francais, anglais, espagnol, allemand, italien,
 * portugais.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#pragma once

enum { L_FR, L_EN, L_ES, L_DE, L_IT, L_PT, L_COUNT };

/* code de langue du systeme PS5 (parametre SCE_SYSTEM_SERVICE_PARAM_ID_LANG) */
void i18n_init(int ps5_code);
int  i18n_lang(void);                 /* L_* choisie */
const char *i18n_locale(void);        /* "de-DE", "pt-BR"… : pour les noms des jeux */
const char *i18n_title_key(void);     /* "TITLE_04"… : nom localise dans la base de la PS5 */

/* traduction d'un texte francais (renvoie le texte tel quel s'il est inconnu) */
const char *tr(const char *fr);
