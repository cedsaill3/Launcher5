/*
 * Launcher5 - interface.
 *
 * Mise en page de « game loader » : sections par type de contenu, trois
 * presentations des jaquettes (cover flow en perspective avec reflets,
 * grille, carrousel), illustration du jeu en fond, themes animes.
 * L'accueil est un menu libre avec dossiers, modifiable a la manette.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <dirent.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "menu.h"
#include "platform.h"
#include "theme.h"
#include "i18n.h"
#include "version.h"
#include "sha256.h"

#ifdef HAVE_MIXER
#include <SDL2/SDL_mixer.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <jansson.h>

/* Horloge du launcher : monotone. Un payload qui remet l'heure de la console a jour
   (ps5-date-time-sync…) ne doit pas faire sauter les messages, animations et delais. */
static Uint32 l5_ticks(void) {
  static long long base = -1;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long ms = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  if (base < 0) base = ms;
  return (Uint32)(ms - base + 1);
}
#define SDL_GetTicks() l5_ticks()

extern const unsigned char font_regular[], font_bold[];
extern const unsigned int font_regular_len, font_bold_len;

#define W 1920
#define H 1080
/* Tailles des jaquettes (largeur) : boites verticales ou icones carrees */
#define BOXES (S.menu.boxes)
#define TH(w) (BOXES ? (w) * 3 / 2 : (w))          /* hauteur d'une jaquette de largeur w (2:3) */
#define TILE (BOXES ? 172 : 230)
#define TILE_SEL (BOXES ? 224 : 300)
#define FLOW (BOXES ? 300 : 400)                   /* jaquette centrale du cover flow */
#define FLOW_SIDE (BOXES ? 224 : 300)              /* jaquettes laterales */
#define FLOW_CY 440       /* centre vertical du cover flow */
#define GRID_COLS (BOXES ? 7 : 6)
#define GRID_CELL (BOXES ? 156 : 210)
#define GRID_GAP 40
#define GRID_ROWH (BOXES ? 278 : 274)
#define GRID_Y 170
#define ED_W 820          /* largeur du studio de theme */
#define GAP 30
#define CAR_X 110
#define CAR_Y 690
#define CAR_BOT (CAR_Y + 300)   /* bas des tuiles du carrousel */
#define MAX_DEPTH 8

/* ------------------------------------------------------------------ */
/* Themes                                                              */
/* ------------------------------------------------------------------ */
/* Couleurs proposees dans l'editeur de theme */
typedef struct { const char *name; uint32_t c; } NamedColor;
static const NamedColor ACCENTS[] = {
  { "Cyan", 0x00e5ff }, { "Bleu glacier", 0x7fdbff }, { "Océan", 0x00b4d8 }, { "Lagon", 0x48cae4 },
  { "Turquoise", 0x00f0ff }, { "Menthe", 0x00ffa3 }, { "Vert néon", 0x39ff14 }, { "Vert matrice", 0x00ff41 },
  { "Vert forêt", 0x008f11 }, { "Citron", 0xc6ff00 }, { "Jaune", 0xfcee0a }, { "Sable", 0xffd166 },
  { "Ambre", 0xffb300 }, { "Or", 0xd4af37 }, { "Orange", 0xff9100 }, { "Feu", 0xff4d1a },
  { "Rouge", 0xe50914 }, { "Framboise", 0xff1f5a }, { "Rose", 0xff3cac }, { "Rose sakura", 0xff8fb8 },
  { "Rose pâle", 0xffd1e0 }, { "Magenta", 0xff2bd6 }, { "Violet", 0xb26bff }, { "Indigo", 0x7c4dff },
  { "Bleu", 0x2b86c5 }, { "Bleu électrique", 0x2979ff }, { "Blanc", 0xffffff }, { "Glace", 0xe8f7ff },
  { "Crème", 0xfff1c1 }, { "Argent", 0xc9d1dc }, { "Ardoise", 0x5b6b82 },
  { "Fuchsia", 0xff00c8 }, { "Électrique", 0x00fff0 }, { "Sang", 0xff1f3d }, { "Carmin", 0x8a0012 },
  { "Vert terminal", 0x33ff66 }, { "Vert sombre", 0x00a33a },
};
static const NamedColor DARKS[] = {
  { "Noir", 0x000000 }, { "Charbon", 0x0b0b0b }, { "Nuit", 0x06080c }, { "Graphite", 0x10141c },
  { "Gris bleu", 0x1c2433 }, { "Bleu nuit", 0x05081a }, { "Marine", 0x020c1b }, { "Abysse", 0x03254a },
  { "Acier", 0x05101a }, { "Glacier", 0x10293d }, { "Encre", 0x0d0221 }, { "Violet nuit", 0x170a33 },
  { "Prune", 0x221038 }, { "Aubergine", 0x2d0b3f }, { "Améthyste", 0x0b0517 }, { "Bordeaux", 0x3a1424 },
  { "Cerise noire", 0x1a0710 }, { "Braise", 0x2e0b05 }, { "Cendre", 0x100503 }, { "Brun", 0x1d1608 },
  { "Bronze", 0x15120a }, { "Asphalte", 0x08080d }, { "Forêt", 0x0a2214 }, { "Mousse", 0x030d06 },
  { "Vert nuit", 0x001a07 }, { "Néant", 0x07000f }, { "Pourpre nuit", 0x14002a }, { "Sang noir", 0x0a0000 },
  { "Grenat", 0x220005 }, { "Acide nuit", 0x050805 }, { "Lime nuit", 0x101a05 }, { "Terminal", 0x000a03 },
};
#define NACCENTS (int)(sizeof ACCENTS / sizeof ACCENTS[0])
#define NDARKS (int)(sizeof DARKS / sizeof DARKS[0])

static const uint32_t PALETTE[] = {
  0x3d8bfd, 0x7c4dff, 0xe5484d, 0xf76b15, 0xffc53d,
  0x46a758, 0x12a594, 0x0090ff, 0xd6409f, 0x8d8d8d,
};
#define NPALETTE (int)(sizeof PALETTE / sizeof PALETTE[0])
static const char *PALETTE_NAMES[] = {
  "Bleu", "Violet", "Rouge", "Orange", "Jaune", "Vert", "Turquoise", "Ciel", "Rose", "Gris",
};

/* ------------------------------------------------------------------ */
/* Sections                                                            */
/* ------------------------------------------------------------------ */
enum { SEC_HOME, SEC_FAV, SEC_PS5, SEC_PS4, SEC_MEDIA, SEC_HB, SEC_EMU, SEC_PAYLOAD, SEC_TOP, SEC_RECENT, SEC_SYS, SEC_COUNT };
static const char *SEC_NAMES[SEC_COUNT] = {
  "Mes dossiers", "Favoris", "Jeux PS5", "Jeux PS4", "Médias", "Homebrews", "Émulateurs", "Payloads", "Plus joués",
  "Récents", "Paramètres"
};
/* ordre d'affichage des sections (« Mes dossiers » apres les emulateurs) */
static const int SEC_ORDER[SEC_COUNT] = { SEC_FAV, SEC_PS5, SEC_PS4, SEC_MEDIA, SEC_HB, SEC_EMU, SEC_HOME, SEC_PAYLOAD,
                                          SEC_TOP, SEC_RECENT, SEC_SYS };
static int sec_pos(int s) { int i = 0; while (i < SEC_COUNT - 1 && SEC_ORDER[i] != s) i++; return i; }
/* sections triees par nom : saut par lettre (L2 / R2) */
#define SEC_SORTED_BY_NAME(s) ((s) != SEC_HOME && (s) != SEC_RECENT && (s) != SEC_TOP)
static const char *KIND_LABEL[KIND_COUNT] = { "JEU PS5", "JEU PS4", "HOMEBREW", "PAYLOAD", "MÉDIA", "RÉGLAGE" };
static const char *VIEW_NAMES[VIEW_COUNT] = { "Cover Flow", "Grille", "Carrousel" };

/* Un contenu peut apparaitre dans plusieurs sections (ex. VLC : Homebrews et Medias) */
static int sec_match(int sec, const App *a) {
  switch (sec) {
  case SEC_PS5: return a->kind == KIND_PS5;
  case SEC_PS4: return a->kind == KIND_PS4;
  case SEC_MEDIA: return a->kind == KIND_MEDIA || (a->flags & APPF_MEDIA);
  case SEC_HB: return a->kind == KIND_HOMEBREW;
  case SEC_EMU: return (a->flags & APPF_EMU) != 0;
  case SEC_PAYLOAD: return a->kind == KIND_PAYLOAD;
  case SEC_SYS: return a->kind == KIND_SYSTEM;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Etat                                                                */
/* ------------------------------------------------------------------ */
enum { MODE_BROWSE, MODE_CONTEXT, MODE_MOVE, MODE_KEYBOARD, MODE_PICKFOLDER, MODE_COLOR, MODE_THEME, MODE_INFO, MODE_CATALOG, MODE_HELP };
/* lignes de l'editeur de theme */
enum { R_PRESET, R_LAYOUT, R_AUTO, R_ACCENT, R_ACCENT2, R_TOP, R_BOTTOM, R_BG, R_WALL, R_ART, R_FX, R_GLOW, R_PART, R_PSTYLE,
       R_FALL, R_CASE, R_CORNERS, R_CLOCK, R_SFX, R_MUSIC, R_VOLUME, R_SAVE, R_DELETE, R_COUNT };
enum {
  ACT_LAUNCH, ACT_OPEN, ACT_MOVE, ACT_TO_FOLDER, ACT_OUT_OF_FOLDER, ACT_RENAME, ACT_HIDE,
  ACT_SHOW, ACT_COLOR, ACT_DELETE_FOLDER, ACT_NEW_FOLDER, ACT_TOGGLE_HIDDEN, ACT_RESET_NAME, ACT_THEME, ACT_SAVE_THEME,
  ACT_VIEW, ACT_FAV, ACT_UNFAV, ACT_INFO, ACT_SEARCH, ACT_CATALOG, ACT_BOOT,
  ACT_START_MANUAL, ACT_START_PLUGIN, ACT_START_AUTOLOAD, ACT_FIRST_YES, ACT_FIRST_NO, ACT_RELINK_YES, ACT_RELINK_NO, ACT_K_BOOT, ACT_K_SEND, ACT_HELP_PAY,
  /* menu rapide (touche OPTIONS) */
  ACT_Q_RESTART, ACT_Q_CACHE, ACT_Q_QUIT, ACT_Q_RESUME, ACT_Q_CATALOG,
};

typedef struct { int is_folder; int index; App *app; Node *node; } Entry;
#define GXMODE (S.base && S.base->layout == LAYOUT_GX && S.sec != SEC_SYS)   /* ecran des jeux facon loader GX */
#define XMBMODE (S.base && S.base->layout == LAYOUT_XMB && S.sec != SEC_SYS) /* ecran des jeux facon XMB */
typedef struct { char label[96]; int action; } Choice;
typedef struct { Node *folder; char label[128]; } FolderChoice;

static struct {
  SDL_Window *win;
  SDL_Renderer *r;
  TTF_Font *f22, *f26, *f30, *f36, *f48, *f84, *fbig;
  SDL_Texture *glow, *shade_left, *shade_bottom;
  Menu menu;
  char menu_path[PATH_LEN];
  int running;
  Theme *th;                          /* theme affiche a l'image en cours (copie de base, couleurs auto) */
  Theme *base;                        /* theme de reference : cur, ou edit pendant l'edition */
  Theme fth;                          /* theme de l'image en cours */
  Theme cur, edit;                    /* theme applique / en cours de modification */
  uint32_t acc_now, acc2_now;         /* couleurs affichees (glissent vers celles du jeu) */
  Uint32 sel_since;                   /* moment ou la selection a change */
  int th_ver;                         /* change a chaque modification (caches a refaire) */
  long long menu_sig, theme_sig;      /* fichiers tels que le launcher les connait */
  int tiles_corners;                  /* forme des tuiles deja calculees */

  /* editeur de theme */
  int ed_row, ed_dirty, preset, nbuiltin, npresets;
  float ed_scroll;
  Theme *presets;                     /* themes fournis puis themes de l'utilisateur */
  char (*preset_files)[THEME_FILE_LEN];
  char (*walls)[THEME_FILE_LEN]; int nwalls;
  char (*musics)[THEME_FILE_LEN]; int nmusics;
  SDL_Texture *wall; char wall_key[THEME_FILE_LEN];

  int sec;
  int sec_has[SEC_COUNT];
  float tab_x, tab_w;                 /* soulignement anime des sections */
  int path[MAX_DEPTH], depth;
  int sel[MAX_DEPTH + 1];
  int sec_sel[SEC_COUNT];
  float scroll, sel_anim, hero_anim;
  float flow_cx;                      /* centre du cover flow (se decale pendant l'edition du theme) */
  float grid_scroll; int grid_top;    /* premiere rangee affichee de la grille */
  int show_hidden;

  /* fond : crossfade entre l'ancienne et la nouvelle illustration */
  SDL_Texture *bg_cur, *bg_prev;
  float bg_fade;

  /* mode leger (dessin sans carte graphique, comme sur PS5) */
  int lite;
  SDL_Texture *bgcache, *tile_glow, *btn_glow;
  App *bg_key_app;
  int bg_key_ver;
  uint32_t bg_key_acc;

  Entry *entries; int nentries, capentries;
  int last_sel_key;

  int mode;
  Choice choices[16]; int nchoices, csel, quick;   /* quick : menu rapide ouvert */
  char ctx_title[NAME_LEN];

  char kb_buf[NAME_LEN]; int kb_row, kb_col, kb_shift, kb_action;
  char kb_title[96];

  FolderChoice *folders; int nfolders, fsel;
  int color_sel;

  char toast[200]; Uint32 toast_until;
  App *launching; Uint32 launch_t;    /* ecran de lancement */
  Uint32 frame;
  float dt;                           /* duree de la derniere image, en secondes */

  struct { float x, y, s, v; } part[70];

  /* V2 */
  struct { float x, y, z, px, py; } star[160];      /* hyperespace */
  struct { float y, v; int len, seed; } drop[64];   /* pluie de code */
  SDL_Texture *glyph[36];                           /* lettres de la pluie de code */
  float grid_phase;
  Uint32 last_input;                                /* economiseur d'ecran */
  int saver_on, saver_idx, saver_prev; Uint32 saver_t;
  Uint32 boot_t0; int boot_done;                    /* intro au demarrage */
  char letter[8]; Uint32 letter_until;              /* saut par lettre */
  char sysline[160]; Uint32 sys_t;                  /* IP, espace libre, temperature */
  SDL_GameController *pads[4];
} S;

/* ------------------------------------------------------------------ */
/* Texte                                                               */
/* ------------------------------------------------------------------ */
typedef struct { char key[200]; TTF_Font *font; uint32_t color; SDL_Texture *tex; int w, h; Uint32 used; } TextCache;
static TextCache tcache[400];

static SDL_Texture *text_tex(TTF_Font *f, const char *s, uint32_t color, int *w, int *h) {
  if (!s || !*s) return NULL;
  TextCache *slot = NULL, *victim = NULL;
  for (size_t i = 0; i < sizeof tcache / sizeof tcache[0]; i++) {
    TextCache *c = &tcache[i];
    if (c->tex && c->font == f && c->color == color && !strcmp(c->key, s)) { slot = c; break; }
    if (!c->tex) { if (!victim || victim->tex) victim = c; continue; }
    if (!victim || (victim->tex && c->used < victim->used)) victim = c;
  }
  if (!slot) {
    slot = victim;
    if (slot->tex) SDL_DestroyTexture(slot->tex);
    slot->tex = NULL;
    SDL_Color col = { (color >> 16) & 255, (color >> 8) & 255, color & 255, 255 };
    SDL_Surface *surf = TTF_RenderUTF8_Blended(f, s, col);
    if (!surf) return NULL;
    slot->tex = SDL_CreateTextureFromSurface(S.r, surf);
    slot->w = surf->w; slot->h = surf->h;
    SDL_FreeSurface(surf);
    snprintf(slot->key, sizeof slot->key, "%s", s);
    slot->font = f; slot->color = color;
  }
  slot->used = S.frame;
  if (w) *w = slot->w;
  if (h) *h = slot->h;
  return slot->tex;
}

/* align : 0 gauche, 1 centre, 2 droite ; maxw > 0 : coupe avec « … » */
static int draw_text_a(TTF_Font *f, const char *s, int x, int y, uint32_t color, int align, int maxw, int alpha) {
  /* marge : remplacer un caractere par « … » peut allonger le texte de 2 octets */
  char buf[208];
  snprintf(buf, 200, "%s", s ? tr(s) : "");
  size_t bl = strlen(buf);                   /* pas de caractere coupe en deux a la fin */
  if (bl == 199) {
    size_t k = bl;
    while (k && ((unsigned char)buf[k - 1] & 0xC0) == 0x80) k--;
    if (k && (unsigned char)buf[k - 1] >= 0xC0) buf[k - 1] = 0;
  }
  int w, h;
  if (maxw > 0) {
    TTF_SizeUTF8(f, buf, &w, &h);
    while (w > maxw && strlen(buf) > 1) {
      size_t l = strlen(buf);
      char *end = buf + l - 1;
      if (l > 3 && !strcmp(end - 2, "…")) end -= 3;
      while (end > buf && ((unsigned char)*end & 0xC0) == 0x80) end--;
      strcpy(end, "…");
      TTF_SizeUTF8(f, buf, &w, &h);
    }
  }
  SDL_Texture *t = text_tex(f, buf, color, &w, &h);
  if (!t) return 0;
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  SDL_SetTextureAlphaMod(t, alpha);
  SDL_Rect d = { x, y, w, h };
  SDL_RenderCopy(S.r, t, NULL, &d);
  SDL_SetTextureAlphaMod(t, 255);
  return w;
}
static int draw_text(TTF_Font *f, const char *s, int x, int y, uint32_t c, int align, int maxw) {
  return draw_text_a(f, s, x, y, c, align, maxw, 255);
}
/* lissage independant de la fluidite : k = part parcourue en 1/60 s */
static float ease(float k) { return 1.0f - powf(1.0f - k, S.dt * 60.0f); }

static int text_w(TTF_Font *f, const char *s) { int w = 0, h; TTF_SizeUTF8(f, tr(s), &w, &h); return w; }

/* ------------------------------------------------------------------ */
/* Formes et effets                                                    */
/* ------------------------------------------------------------------ */
static void set_color(uint32_t c, int a) {
  SDL_SetRenderDrawColor(S.r, (c >> 16) & 255, (c >> 8) & 255, c & 255, a);
}

/* Formes arrondies : tout est envoye en un seul appel (bien plus rapide sans
   carte graphique que des centaines de lignes et de points separes). */
static void fill_round(int x, int y, int w, int h, int rad, uint32_t c, int a) {
  if (w <= 0 || h <= 0) return;
  set_color(c, a);
  if (rad * 2 > h) rad = h / 2;
  if (rad * 2 > w) rad = w / 2;
  if (rad > 255) rad = 255;
  if (rad < 0) rad = 0;
  SDL_Rect r[1 + 2 * 255];
  int n = 0;
  r[n++] = (SDL_Rect){ x, y + rad, w, h - 2 * rad };
  for (int i = 0; i < rad; i++) {
    float dy = rad - i - 0.5f;
    int dx = (int)(rad - sqrtf((float)rad * rad - dy * dy) + 0.5f);
    r[n++] = (SDL_Rect){ x + dx, y + i, w - 2 * dx, 1 };
    r[n++] = (SDL_Rect){ x + dx, y + h - 1 - i, w - 2 * dx, 1 };
  }
  SDL_RenderFillRects(S.r, r, n);
}

static void outline_round(int x, int y, int w, int h, int rad, int thick, uint32_t c, int a) {
  enum { MAXP = 4096 };
  static SDL_Point p[MAXP];
  SDL_Rect sides[4 * 8];
  int np = 0, ns = 0;
  set_color(c, a);
  if (thick > 8) thick = 8;
  for (int t = 0; t < thick; t++) {
    int X = x - t, Y = y - t, Wd = w + 2 * t, Hd = h + 2 * t, R = rad + t;
    if (R * 2 > Hd) R = Hd / 2;
    if (R * 2 > Wd) R = Wd / 2;
    sides[ns++] = (SDL_Rect){ X + R, Y, Wd - 2 * R, 1 };
    sides[ns++] = (SDL_Rect){ X + R, Y + Hd - 1, Wd - 2 * R, 1 };
    sides[ns++] = (SDL_Rect){ X, Y + R, 1, Hd - 2 * R };
    sides[ns++] = (SDL_Rect){ X + Wd - 1, Y + R, 1, Hd - 2 * R };
    int steps = R * 2 + 2;                       /* assez de points pour un arc sans trou */
    for (int k = 0; k <= steps && R > 0; k++) {
      if (np + 4 > MAXP) { SDL_RenderDrawPoints(S.r, p, np); np = 0; }
      float ang = k * (3.14159f / 2) / steps;
      int ox = (int)(R - R * cosf(ang) + 0.5f), oy = (int)(R - R * sinf(ang) + 0.5f);
      p[np++] = (SDL_Point){ X + ox, Y + oy };
      p[np++] = (SDL_Point){ X + Wd - 1 - ox, Y + oy };
      p[np++] = (SDL_Point){ X + ox, Y + Hd - 1 - oy };
      p[np++] = (SDL_Point){ X + Wd - 1 - ox, Y + Hd - 1 - oy };
    }
  }
  SDL_RenderFillRects(S.r, sides, ns);
  if (np) SDL_RenderDrawPoints(S.r, p, np);
}

static uint32_t shade(uint32_t c, float k) {
  int r = (int)(((c >> 16) & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)((c & 255) * k);
  return (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b));
}

static uint32_t hash_color(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) h = (h ^ (unsigned char)*s++) * 16777619u;
  return PALETTE[h % NPALETTE];
}

/* Halo lumineux : texture ronde degradee, dessinee en mode additif */
static SDL_Texture *make_glow_n(int n) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, n, n, 32, SDL_PIXELFORMAT_ARGB8888);
  Uint32 *px = s->pixels;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      float dx = (x - n / 2 + 0.5f) / (n / 2), dy = (y - n / 2 + 0.5f) / (n / 2);
      float d = sqrtf(dx * dx + dy * dy), a = d >= 1 ? 0 : (1 - d) * (1 - d);
      px[y * (s->pitch / 4) + x] = (Uint32)(a * 255) << 24 | 0xffffff;
    }
  SDL_Texture *t = SDL_CreateTextureFromSurface(S.r, s);
  SDL_FreeSurface(s);
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
  return t;
}
static SDL_Texture *make_glow(void) { return make_glow_n(256); }

/* Degrade d'assombrissement (gauche->droite ou bas->haut) */
static SDL_Texture *make_shade(int horizontal) {
  int n = 256;
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, horizontal ? n : 1, horizontal ? 1 : n, 32, SDL_PIXELFORMAT_ARGB8888);
  Uint32 *px = s->pixels;
  for (int i = 0; i < n; i++) {
    float t = (float)i / (n - 1);
    float a = horizontal ? powf(1 - t, 1.4f) * 0.92f : powf(t, 1.6f) * 0.95f;
    px[i] = (Uint32)(a * 255) << 24;
  }
  SDL_Texture *t = SDL_CreateTextureFromSurface(S.r, s);
  SDL_FreeSurface(s);
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  return t;
}

/* intensite des halos du theme */
static int ga(int a) { return a * S.th->glow / 100; }

/* arrondi des tuiles (a la taille d'une tuile selectionnee) */
static int tile_rad(void) {                      /* boitiers : coins a peine arrondis, comme le plastique */
  static const int R[] = { 0, 18, 44 };
  return BOXES ? (S.th->corners ? 10 : 0) : R[S.th->corners];
}

static void glow(int cx, int cy, int radius, uint32_t color, int alpha) {
  SDL_SetTextureColorMod(S.glow, (color >> 16) & 255, (color >> 8) & 255, color & 255);
  SDL_SetTextureAlphaMod(S.glow, alpha);
  SDL_Rect d = { cx - radius, cy - radius, radius * 2, radius * 2 };
  SDL_RenderCopy(S.r, S.glow, NULL, &d);
}

/* ------------------------------------------------------------------ */
/* Contenu des sections                                                */
/* ------------------------------------------------------------------ */
static Node *current_node(void) {
  Node *n = &S.menu.root;
  for (int i = 0; i < S.depth; i++) {
    if (S.path[i] < 0 || S.path[i] >= n->count || !n->items[S.path[i]].is_folder) { S.depth = i; break; }
    n = &n->items[S.path[i]];
  }
  return n;
}

static void push_entry(Entry e) {
  if (S.nentries == S.capentries) {
    S.capentries = S.capentries ? S.capentries * 2 : 64;
    S.entries = realloc(S.entries, sizeof(Entry) * S.capentries);
  }
  S.entries[S.nentries++] = e;
}

static int cmp_recent(const void *a, const void *b) {
  Stat *x = menu_stat(&S.menu, ((const Entry *)a)->app->id, 0), *y = menu_stat(&S.menu, ((const Entry *)b)->app->id, 0);
  time_t tx = x ? x->last : 0, ty = y ? y->last : 0;
  return ty > tx ? 1 : ty < tx ? -1 : 0;
}
static int cmp_playtime(const void *a, const void *b) {
  Stat *x = menu_stat(&S.menu, ((const Entry *)a)->app->id, 0), *y = menu_stat(&S.menu, ((const Entry *)b)->app->id, 0);
  long tx = x ? x->playtime : 0, ty = y ? y->playtime : 0;
  return ty > tx ? 1 : ty < tx ? -1 : 0;
}
static int cmp_name(const void *a, const void *b) {
  return strcasecmp(menu_display_name(&S.menu, ((const Entry *)a)->app), menu_display_name(&S.menu, ((const Entry *)b)->app));
}

/* ------------------------------------------------------------------ */
/* Tags automatiques (d'apres les statistiques) et filtre (touche □)   */
/* ------------------------------------------------------------------ */
enum { FIL_ALL, FIL_NEW, FIL_NEVER, FIL_RARE, FIL_RECENT, FIL_ABANDONED, FIL_COUNT };
static const char *FILTER_NAMES[FIL_COUNT] = { "Tous", "Nouveaux", "Jamais joués", "Peu joués", "Joués récemment", "Abandonnés" };
static int filter_now;                           /* filtre en cours (sections de jeux) */

/* dossier ou fichier du jeu sur la console (taille, date d'installation) */
static void app_dir(App *a, char *out, size_t n) {
  out[0] = 0;
  if (!a || a->kind == KIND_SYSTEM) return;
  if (a->path[0]) snprintf(out, n, "%s", a->path);
  else if (a->kind == KIND_PS5 || a->kind == KIND_PS4 || a->kind == KIND_MEDIA) snprintf(out, n, "/user/app/%s", a->id);
}

static time_t app_installed(App *a) {            /* date d'installation (fichiers du jeu), gardee en memoire */
  static struct { App *a; time_t t; } IC[512];
  static int n, napps = -1;
  if (napps != S.menu.napps) { n = 0; napps = S.menu.napps; }
  for (int i = 0; i < n; i++) if (IC[i].a == a) return IC[i].t;
  struct stat sb;
  char d[PATH_LEN];
  app_dir(a, d, sizeof d);
  const char *p = d[0] && !access(d, F_OK) ? d : a->icon;
  time_t t = p[0] && !stat(p, &sb) ? sb.st_mtime : 0;
  if (n < 512) { IC[n].a = a; IC[n].t = t; n++; }
  return t;
}

static int app_tag(App *a, int f) {
  if (f == FIL_ALL) return 1;
  if (!a || a->kind == KIND_SYSTEM) return 0;
  Stat *st = menu_stat(&S.menu, a->id, 0);
  int played = st && st->launches > 0;
  time_t now = time(NULL), t;
  switch (f) {
  case FIL_NEW: t = app_installed(a); return !played && t && now - t < 7 * 86400;
  case FIL_NEVER: return !played;
  case FIL_RARE: return played && st->launches <= 2 && st->playtime < 3600;
  case FIL_RECENT: return played && st->last && now - st->last < 7 * 86400;
  case FIL_ABANDONED: return played && st->last && now - st->last > 60 * 86400;
  }
  return 1;
}

/* --- Recherche (L3 / R3 ou △ › Rechercher) : jeux, homebrews, payloads, reglages --- */
static char search_q[NAME_LEN];                  /* recherche en cours (vide : aucune) */

/* Transition douce : le contenu glisse un court instant (section, vue) */
static Uint32 tr_t;
static int tr_dx, tr_dy;
static void transition(int dx, int dy) { tr_t = SDL_GetTicks(); tr_dx = dx; tr_dy = dy; if (!tr_t) tr_t = 1; }
static int tr_offset(int *dx, int *dy) {          /* 1 tant que la transition dure */
  *dx = *dy = 0;
  if (!tr_t) return 0;
  float k = (SDL_GetTicks() - tr_t) / 240.0f;
  if (k >= 1) { tr_t = 0; return 0; }
  float r = (1 - k) * (1 - k) * (1 - k);          /* ralentit en arrivant */
  *dx = (int)(tr_dx * r); *dy = (int)(tr_dy * r);
  return 1;
}

/* Parallaxe : le fond (prepare un peu plus large que l'ecran) glisse avec la selection */
#define PAR 64
static float par_x;

/* minuscules, accents retires : « Éte » trouve « ete » */
static void fold(const char *s, char *out, size_t n) {
  static const char *const FROM[] = { "à", "â", "ä", "á", "ã", "é", "è", "ê", "ë", "î", "ï", "í", "ô", "ö", "ó", "õ", "ù", "û", "ü", "ú",
                                      "ç", "ñ", "À", "Â", "Ä", "É", "È", "Ê", "Ë", "Î", "Ï", "Ô", "Ö", "Ù", "Û", "Ü", "Ç", "ß" };
  static const char TO[] = "aaaaaeeeeiiioooouuuucnaaaeeeeiioouuucs";
  size_t k = 0;
  while (*s && k + 1 < n) {
    int done = 0;
    if ((unsigned char)*s >= 0xC0)
      for (size_t i = 0; i < sizeof FROM / sizeof FROM[0]; i++) {
        size_t l = strlen(FROM[i]);
        if (!strncmp(s, FROM[i], l)) { out[k++] = TO[i]; s += l; done = 1; break; }
      }
    if (!done) { out[k++] = (char)tolower((unsigned char)*s); s++; }
  }
  out[k] = 0;
}

static int search_match(App *a, const char *q) {
  char fq[NAME_LEN], f[NAME_LEN];
  fold(q, fq, sizeof fq);
  if (!fq[0]) return 0;
  fold(menu_display_name(&S.menu, a), f, sizeof f);
  if (strstr(f, fq)) return 1;
  fold(a->title, f, sizeof f);
  if (strstr(f, fq)) return 1;
  fold(a->id, f, sizeof f);
  return strstr(f, fq) != NULL;
}

static int search_count(const char *q) {
  int n = 0;
  for (int i = 0; i < S.menu.napps; i++)
    if ((S.show_hidden || !menu_is_hidden(&S.menu, S.menu.apps[i].id)) && search_match(&S.menu.apps[i], q)) n++;
  return n;
}

/* payload installe par Launcher5 (/data/payloads) */
static int pay_ours(const App *a) {
  char d[PATH_LEN];
  snprintf(d, sizeof d, "%s/data/payloads/", plat_root());
  return a->kind == KIND_PAYLOAD && !strncmp(a->path, d, strlen(d));
}
/* copie d'un autre gestionnaire remplacee par celle de Launcher5 (meme nom) : masquee */
static int pay_shadowed(const App *a) {
  if (a->kind != KIND_PAYLOAD || pay_ours(a)) return 0;
  for (int i = 0; i < S.menu.napps; i++) {
    const App *o = &S.menu.apps[i];
    if (o != a && pay_ours(o) && !strcasecmp(o->title, a->title)) return 1;
  }
  return 0;
}

static void build_entries(void) {
  if (search_q[0] && S.sec != SEC_SYS) {         /* resultats de la recherche, toutes sections confondues */
    S.nentries = 0;
    for (int i = 0; i < S.menu.napps; i++) {
      App *a = &S.menu.apps[i];
      if (!S.show_hidden && menu_is_hidden(&S.menu, a->id)) continue;
      if (pay_shadowed(a) || !search_match(a, search_q)) continue;
      Entry e = { 0, -1, a, NULL };
      push_entry(e);
    }
    qsort(S.entries, S.nentries, sizeof(Entry), cmp_name);
    return;
  }
  S.nentries = 0;
  if (S.sec == SEC_HOME) {
    Node *n = current_node();
    for (int i = 0; i < n->count; i++) {
      Node *c = &n->items[i];
      Entry e = { c->is_folder, i, NULL, c };
      if (!c->is_folder) {
        e.app = menu_find_app(&S.menu, c->ref);
        if (!e.app) continue;
        if (!S.show_hidden && menu_is_hidden(&S.menu, c->ref)) continue;
      }
      push_entry(e);
    }
    return;
  }
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    if (!S.show_hidden && menu_is_hidden(&S.menu, a->id)) continue;
    if (S.sec == SEC_RECENT || S.sec == SEC_TOP) {
      Stat *st = menu_stat(&S.menu, a->id, 0);
      if (!st || !(S.sec == SEC_RECENT ? st->last : st->playtime)) continue;
    } else if (S.sec == SEC_FAV) {
      if (!menu_is_fav(&S.menu, a->id)) continue;
    } else if (!sec_match(S.sec, a)) continue;
    if (filter_now && S.sec != SEC_SYS && !app_tag(a, filter_now)) continue;
    if (pay_shadowed(a)) continue;
    Entry e = { 0, -1, a, NULL };
    push_entry(e);
  }
  qsort(S.entries, S.nentries, sizeof(Entry),
        S.sec == SEC_RECENT ? cmp_recent : S.sec == SEC_TOP ? cmp_playtime : cmp_name);
  if ((S.sec == SEC_RECENT || S.sec == SEC_TOP) && S.nentries > 20) S.nentries = 20;
}

static void update_sections(void) {
  for (int s = 0; s < SEC_COUNT; s++) S.sec_has[s] = s == SEC_HOME && S.menu.show_folders;
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    if (menu_is_hidden(&S.menu, a->id) && !S.show_hidden) continue;
    for (int s = 1; s < SEC_COUNT; s++) if (sec_match(s, a)) S.sec_has[s] = 1;
    Stat *st = menu_stat(&S.menu, a->id, 0);
    if (st && st->last) S.sec_has[SEC_RECENT] = 1;
    if (st && st->playtime) S.sec_has[SEC_TOP] = 1;
    if (menu_is_fav(&S.menu, a->id)) S.sec_has[SEC_FAV] = 1;
  }
}

static int *cur_sel(void) { return S.sec == SEC_HOME ? &S.sel[S.depth] : &S.sec_sel[S.sec]; }
static void clamp_sel(void) {
  int *s = cur_sel();
  if (*s >= S.nentries) *s = S.nentries - 1;
  if (*s < 0) *s = 0;
}
static Entry *selected(void) {
  int s = *cur_sel();
  return (s >= 0 && s < S.nentries) ? &S.entries[s] : NULL;
}

static long long file_sig(const char *path) {
  struct stat st;
  if (stat(path, &st)) return 0;
  return (long long)st.st_mtime * 1000003LL + (long long)st.st_size;
}

static const char *active_line(void);
static void toast(const char *msg) {
  snprintf(S.toast, sizeof S.toast, "%s", msg);
  S.toast_until = SDL_GetTicks() + 3000;
}
static void save(void) {
  if (menu_save(&S.menu, S.menu_path)) toast("⚠ Impossible d'enregistrer le menu");
  S.menu_sig = file_sig(S.menu_path);
}

/* ------------------------------------------------------------------ */
/* Images                                                              */
/* ------------------------------------------------------------------ */
static SDL_Texture *load_tex(const char *path) {  /* charge une image */
  if (!path || !path[0]) return NULL;
  SDL_Surface *s = IMG_Load(path);
  if (!s) return NULL;
  SDL_Texture *t = SDL_CreateTextureFromSurface(S.r, s);
  SDL_FreeSurface(s);
  return t;
}

/* Couleur dominante et vive d'une image : teintes ponderees par leur eclat */
static uint32_t hsv_rgb(float h, float s, float v) {
  h = fmodf(h, 360.0f); if (h < 0) h += 360.0f;
  float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1)), m = v - c, r, g, b;
  if (h < 60) { r = c; g = x; b = 0; } else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; } else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; } else { r = c; g = 0; b = x; }
  return (uint32_t)((int)((r + m) * 255) << 16 | (int)((g + m) * 255) << 8 | (int)((b + m) * 255));
}

static void compute_vibe(App *a, SDL_Surface *src) {
  SDL_Surface *s = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
  if (!s) return;
  float bins[36] = { 0 };
  int step = s->w > 64 ? s->w / 64 : 1;
  for (int y = 0; y < s->h; y += step) {
    Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
    for (int x = 0; x < s->w; x += step) {
      Uint32 p = row[x];
      float r = ((p >> 16) & 255) / 255.0f, g = ((p >> 8) & 255) / 255.0f, b = (p & 255) / 255.0f;
      float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
      if (d < 0.15f || mx < 0.2f || ((p >> 24) & 255) < 128) continue;   /* gris, sombre, transparent */
      float h = mx == r ? 60 * fmodf((g - b) / d, 6) : mx == g ? 60 * ((b - r) / d + 2) : 60 * ((r - g) / d + 4);
      if (h < 0) h += 360;
      bins[(int)(h / 10) % 36] += (d / mx) * mx;
    }
  }
  SDL_FreeSurface(s);
  int best = 0;
  float total = 0;
  for (int i = 0; i < 36; i++) { total += bins[i]; if (bins[i] > bins[best]) best = i; }
  if (total < 1) return;                       /* image sans couleur vive : on garde le theme */
  float h = best * 10 + 5;
  a->vibe = hsv_rgb(h, 0.85f, 1.0f);
  a->vibe2 = hsv_rgb(h + 150, 0.75f, 1.0f);
  a->has_vibe = 1;
}

/* Icone carree (icon0) */
static SDL_Texture *cover(App *a) {
  if (a->tex || a->tex_failed) return a->tex;
  SDL_Surface *s = a->icon[0] ? IMG_Load(a->icon) : NULL;
  if (s) {
    if (!a->has_vibe) compute_vibe(a, s);
    a->tex = SDL_CreateTextureFromSurface(S.r, s);
    SDL_FreeSurface(s);
  }
  if (!a->tex) a->tex_failed = 1;
  return a->tex;
}

static void draw_card_contents(App *a, int w, int h);

static void mask_corners(int w, int h, int rad) {
  if (rad <= 0) return;
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
  for (int i = 0; i < rad; i++) {
    float dy = rad - i - 0.5f;
    int dx = (int)(rad - sqrtf((float)rad * rad - dy * dy) + 0.5f);
    if (dx <= 0) continue;
    SDL_RenderDrawLine(S.r, 0, i, dx - 1, i);
    SDL_RenderDrawLine(S.r, w - dx, i, w - 1, i);
    SDL_RenderDrawLine(S.r, 0, h - 1 - i, dx - 1, h - 1 - i);
    SDL_RenderDrawLine(S.r, w - dx, h - 1 - i, w - 1, h - 1 - i);
  }
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
}

/* ------------------------------------------------------------------ */
/* Jaquettes « boite » : la PS5 ne fournit qu'une icone carree et une   */
/* illustration large. On fabrique une jaquette verticale (illustration */
/* recadree + icone) sur un fil a part, gardee en cache sur le disque.  */
/* Une jaquette perso dans jaquettes/<ID>.jpg ou .png passe en premier. */
/* ------------------------------------------------------------------ */
#define BOXW 300
#define BOXH 450
static void drop_tiles(App *a);

static struct {
  SDL_mutex *mx;
  int busy, done, custom, online, offline, prefetch;
  char id[ID_LEN], bg[PATH_LEN], icon[PATH_LEN], mine[2][PATH_LEN], cache[PATH_LEN], off[PATH_LEN], none[PATH_LEN];
  SDL_Surface *out;
} BX;

static SDL_Surface *argb(SDL_Surface *s) {       /* charge -> format unique ARGB8888 */
  if (!s) return NULL;
  if (s->format->format == SDL_PIXELFORMAT_ARGB8888) return s;
  SDL_Surface *c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
  SDL_FreeSurface(s);
  return c;
}

/* reduction lissee (par moities puis bilineaire) */
static void stretch_smooth(SDL_Surface *src, SDL_Rect sr, SDL_Surface *dst, SDL_Rect *dr) {
  int dw = dr ? dr->w : dst->w, dh = dr ? dr->h : dst->h;
  SDL_Surface *cur = src, *tmp = NULL;
  while (sr.w > dw * 2 && sr.h > dh * 2) {
    SDL_Surface *n = SDL_CreateRGBSurfaceWithFormat(0, sr.w / 2, sr.h / 2, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!n) break;
    SDL_SoftStretchLinear(cur, &sr, n, NULL);
    if (tmp) SDL_FreeSurface(tmp);
    cur = tmp = n;
    sr = (SDL_Rect){ 0, 0, n->w, n->h };
  }
  SDL_SoftStretchLinear(cur, &sr, dst, dr);
  if (tmp) SDL_FreeSurface(tmp);
}

static SDL_Rect crop_to(SDL_Surface *s, int w, int h) {   /* recadrage centre au format w:h */
  SDL_Rect r = { 0, 0, s->w, s->h };
  if ((long)s->w * h > (long)s->h * w) { r.w = s->h * w / h; r.x = (s->w - r.w) / 2; }
  else { r.h = s->w * h / w; r.y = (s->h - r.h) / 2; }
  return r;
}

static void darken(SDL_Surface *s, int x0, int y0, int w, int h, int rad, float k) {
  Uint32 *px = s->pixels;
  int pitch = s->pitch / 4;
  for (int y = y0; y < y0 + h; y++) {
    if (y < 0 || y >= s->h) continue;
    for (int x = x0; x < x0 + w; x++) {
      if (x < 0 || x >= s->w) continue;
      int dx = x < x0 + rad ? x0 + rad - x : x >= x0 + w - rad ? x - (x0 + w - rad - 1) : 0;
      int dy = y < y0 + rad ? y0 + rad - y : y >= y0 + h - rad ? y - (y0 + h - rad - 1) : 0;
      if (dx * dx + dy * dy > rad * rad) continue;
      Uint32 p = px[y * pitch + x];
      int r = (int)(((p >> 16) & 255) * k), g = (int)(((p >> 8) & 255) * k), b = (int)((p & 255) * k);
      px[y * pitch + x] = 0xff000000u | (Uint32)(r << 16 | g << 8 | b);
    }
  }
}

/* Jaquette fabriquee facon boitier : l'illustration en pleine page, ou a defaut
   l'icone sur toute la largeur devant un fond flou de ses couleurs. Le bas est
   assombri pour le titre, le bandeau PS5 / PS4 est ajoute a l'affichage. */
static SDL_Surface *box_compose(const char *bg, const char *icon) {
  SDL_Surface *art = bg[0] ? argb(IMG_Load(bg)) : NULL;
  SDL_Surface *ic = !art && icon[0] ? argb(IMG_Load(icon)) : NULL;
  if (!art && !ic) return NULL;
  SDL_Surface *d = SDL_CreateRGBSurfaceWithFormat(0, BOXW, BOXH, 32, SDL_PIXELFORMAT_ARGB8888);
  if (d && art) stretch_smooth(art, crop_to(art, BOXW, BOXH), d, NULL);
  else if (d) {
    SDL_Surface *sm = SDL_CreateRGBSurfaceWithFormat(0, 12, 18, 32, SDL_PIXELFORMAT_ARGB8888);
    if (sm) {                                    /* fond : l'icone tres floutee */
      stretch_smooth(ic, crop_to(ic, 12, 18), sm, NULL);
      SDL_SoftStretchLinear(sm, NULL, d, NULL);
      SDL_FreeSurface(sm);
    }
    darken(d, 0, 0, BOXW, BOXH, 0, 0.55f);
    SDL_Rect r = { 0, (BOXH - BOXW) / 2 - 18, BOXW, BOXW };
    stretch_smooth(ic, (SDL_Rect){ 0, 0, ic->w, ic->h }, d, &r);
  }
  if (d) for (int y = BOXH * 58 / 100; y < BOXH; y++) {   /* bas assombri pour le titre */
    float k = 1 - 0.75f * (y - BOXH * 58 / 100) / (float)(BOXH * 42 / 100);
    darken(d, 0, y, BOXW, 1, 0, k);
  }
  if (art) SDL_FreeSurface(art);
  if (ic) SDL_FreeSurface(ic);
  return d;
}

/* Base de jaquettes Launcher5 sur GitHub : index.txt (identifiant -> jeu) puis c/<jeu>.jpg.
   Les serveurs Sony sont souvent bloques (DNS anti mises a jour), GitHub non. */
#define COVERS_URL "https://raw.githubusercontent.com/cedsaill3/Launcher5-covers/main/"
static char *cov_index;                          /* "\nPPSA01234 10000229\n..." */
static int cov_index_tried;
static const char *covers_url(void) {           /* L5_COVERS_URL : base locale pour les essais */
  const char *e = getenv("L5_COVERS_URL");
  return e && *e ? e : COVERS_URL;
}

static int recent_file(const char *path, int days) {
  struct stat st;
  return !stat(path, &st) && time(NULL) - st.st_mtime < (time_t)days * 86400;
}

/* 1 index pret, -1 indisponible (pas de reseau ou base absente) */
static int covers_index(void) {
  if (cov_index) return 1;
  char path[PATH_LEN];
  snprintf(path, sizeof path, "%s/cache/index.txt", plat_data_dir());
  if (!recent_file(path, 7) && !cov_index_tried) {   /* rafraichi chaque semaine */
    cov_index_tried = 1;
    unsigned char *d; size_t n;
    char url[300];
    snprintf(url, sizeof url, "%sindex.txt", covers_url());
    long code = plat_https_get(url, &d, &n, 8 << 20);
    if (code == 200) {
      FILE *f = fopen(path, "wb");
      if (f) { fwrite(d, 1, n, f); fclose(f); }
      free(d);
    }
  }
  FILE *f = fopen(path, "rb");
  if (!f) return -1;                             /* base pas encore en ligne : on reessaiera plus tard */
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = n > 0 ? malloc((size_t)n + 2) : NULL;
  if (buf) {
    buf[0] = '\n';
    buf[1 + fread(buf + 1, 1, (size_t)n, f)] = 0;
  }
  fclose(f);
  cov_index = buf;
  return buf ? 1 : -1;
}

/* Jaquette de la base. 1 : trouvee, 0 : ce jeu n'en a pas, -1 : pas de reseau */
static int official_cover(const char *id, SDL_Surface **out) {
  *out = NULL;
  if (strlen(id) != 9) return 0;                 /* PPSA01234 */
  for (int i = 0; i < 9; i++) if (i < 4 ? !isupper((unsigned char)id[i]) : !isdigit((unsigned char)id[i])) return 0;
  int r = covers_index();
  if (r <= 0) return r;
  char key[16], concept[16] = "";
  snprintf(key, sizeof key, "\n%s ", id);
  const char *l = strstr(cov_index, key);
  if (!l) return 0;
  l += strlen(key);
  for (int i = 0; i < 15 && isdigit((unsigned char)l[i]); i++) { concept[i] = l[i]; concept[i + 1] = 0; }
  if (!concept[0]) return 0;
  char url[256];
  snprintf(url, sizeof url, "%sc/%s.jpg", covers_url(), concept);
  unsigned char *d; size_t n;
  long code = plat_https_get(url, &d, &n, 4 << 20);
  if (!code) return -1;
  if (code != 200) return 0;
  SDL_Surface *s = argb(IMG_Load_RW(SDL_RWFromConstMem(d, (int)n), 1));
  free(d);
  if (!s) return 0;
  SDL_Surface *b = SDL_CreateRGBSurfaceWithFormat(0, BOXW, BOXH, 32, SDL_PIXELFORMAT_ARGB8888);
  if (b) stretch_smooth(s, crop_to(s, BOXW, BOXH), b, NULL);
  SDL_FreeSurface(s);
  if (!b) return 0;
  *out = b;
  return 1;
}

static int box_worker(void *u) {
  (void)u;
  for (;;) {
    char id[ID_LEN], bg[PATH_LEN], icon[PATH_LEN], mine[2][PATH_LEN], cache[PATH_LEN], off[PATH_LEN], none[PATH_LEN];
    SDL_LockMutex(BX.mx);
    int go = BX.busy && !BX.done, online = BX.online;
    if (go) {
      memcpy(id, BX.id, sizeof id); memcpy(bg, BX.bg, sizeof bg); memcpy(icon, BX.icon, sizeof icon);
      memcpy(mine, BX.mine, sizeof mine); memcpy(cache, BX.cache, sizeof cache);
      memcpy(off, BX.off, sizeof off); memcpy(none, BX.none, sizeof none);
    }
    SDL_UnlockMutex(BX.mx);
    if (!go) { SDL_Delay(15); continue; }
    SDL_Surface *s = NULL;
    int custom = 0;
    for (int i = 0; i < 2 && !s; i++)            /* jaquette perso */
      if (access(mine[i], R_OK) == 0 && (s = argb(IMG_Load(mine[i])))) {
        SDL_Surface *d = SDL_CreateRGBSurfaceWithFormat(0, BOXW, BOXH, 32, SDL_PIXELFORMAT_ARGB8888);
        if (d) stretch_smooth(s, crop_to(s, BOXW, BOXH), d, NULL);
        SDL_FreeSurface(s);
        s = d; custom = 1;
      }
    if (!s && online) {                          /* jaquette officielle : en cache, sinon telechargee */
      if (access(off, R_OK) == 0) {
        s = argb(IMG_Load(off));
        if (s && (s->w != BOXW || s->h != BOXH)) { SDL_FreeSurface(s); s = NULL; }
      }
      if (!s && !BX.offline && !recent_file(none, 7)) {   /* « pas de jaquette » redemande chaque semaine */
        int r = official_cover(id, &s);
        if (r > 0) { IMG_SaveJPG(s, off, 90); plat_log("jaquette officielle : %s", id); }
        else if (r == 0) { FILE *f = fopen(none, "w"); if (f) fclose(f); }   /* pas de jaquette : on ne redemande pas */
        else { BX.offline = 1; plat_log("jaquettes officielles : pas d'acces a la base (GitHub)"); }
      }
      if (s) custom = 2;                         /* officielle : sous le bandeau, sans titre */
    }
    if (!s && access(cache, R_OK) == 0) {        /* deja fabriquee */
      s = argb(IMG_Load(cache));
      if (s && (s->w != BOXW || s->h != BOXH)) { SDL_FreeSurface(s); s = NULL; }
    }
    if (!s && (s = box_compose(bg, icon))) IMG_SaveJPG(s, cache, 90);
    SDL_LockMutex(BX.mx);
    if (BX.prefetch && s) { SDL_FreeSurface(s); s = NULL; }
    BX.out = s; BX.custom = custom; BX.done = 1;
    SDL_UnlockMutex(BX.mx);
  }
  return 0;
}

static void safe_id(const char *id, char *out, size_t n) {
  size_t i = 0;
  for (; id[i] && i + 1 < n; i++) out[i] = isalnum((unsigned char)id[i]) || id[i] == '-' ? id[i] : '_';
  out[i] = 0;
}

/* signature de la jaquette perso d'un jeu (0 = aucune) */
static long long cover_sig(App *a) {
  char sid[ID_LEN], path[PATH_LEN];
  long long sig = 0;
  safe_id(a->id, sid, sizeof sid);
  for (int i = 0; i < 2; i++) {
    struct stat st;
    snprintf(path, sizeof path, "%s/jaquettes/%s.%s", plat_data_dir(), sid, i ? "png" : "jpg");
    if (!stat(path, &st)) sig = sig * 31 + (long long)st.st_mtime * 1000003 + st.st_size + i + 1;
  }
  return sig;
}

/* confie un jeu au fil ; prefetch : seulement preparer sa jaquette sur le disque */
static int box_submit(App *a, int prefetch) {
  if (!BX.mx) {
    BX.mx = SDL_CreateMutex();
    SDL_Thread *th = BX.mx ? SDL_CreateThread(box_worker, "boxes", NULL) : NULL;
    if (!th) { plat_log("jaquettes boites : fil impossible"); a->box_state = 3; return 0; }
    SDL_DetachThread(th);
  }
  SDL_LockMutex(BX.mx);
  int ok = !BX.busy;
  if (ok) {
    char sid[ID_LEN];
    safe_id(a->id, sid, sizeof sid);
    snprintf(BX.id, sizeof BX.id, "%s", a->id);
    snprintf(BX.bg, sizeof BX.bg, "%s", a->bg);
    snprintf(BX.icon, sizeof BX.icon, "%s", a->icon);
    snprintf(BX.mine[0], sizeof BX.mine[0], "%s/jaquettes/%s.jpg", plat_data_dir(), sid);
    snprintf(BX.mine[1], sizeof BX.mine[1], "%s/jaquettes/%s.png", plat_data_dir(), sid);
    snprintf(BX.cache, sizeof BX.cache, "%s/cache/%s.box.jpg", plat_data_dir(), sid);   /* modele boitier */
    snprintf(BX.off, sizeof BX.off, "%s/cache/%s.off.jpg", plat_data_dir(), sid);
    snprintf(BX.none, sizeof BX.none, "%s/cache/%s.none", plat_data_dir(), sid);
    BX.online = S.menu.covers_online;
    BX.prefetch = prefetch;
    BX.busy = 1; BX.done = 0;
    if (!prefetch) { a->box_state = 1; a->box_sig = cover_sig(a); }
  }
  SDL_UnlockMutex(BX.mx);
  return ok;
}

/* jaquette boite prete, ou NULL (elle arrive : une version provisoire est affichee) */
static SDL_Texture *box_art(App *a) {
  if (a->box_state == 2) return a->boxtex;
  if (a->box_state && a->box_state != 4) return NULL;
  if (!box_submit(a, 0)) a->box_state = 4;       /* en attente : lancee des que le fil est libre */
  return NULL;
}

/* a chaque image : recupere la jaquette terminee par le fil */
static void box_poll(void) {
  if (!BX.mx) return;
  SDL_LockMutex(BX.mx);
  SDL_Surface *s = NULL;
  char id[ID_LEN] = "";
  int custom = 0, got = BX.busy && BX.done, busy = BX.busy, pref = BX.prefetch;
  if (got) { s = BX.out; BX.out = NULL; custom = BX.custom; snprintf(id, sizeof id, "%s", BX.id); BX.busy = BX.done = 0; }
  SDL_UnlockMutex(BX.mx);
  if (!got) {                                    /* fil libre : jaquette demandee, sinon prechargement */
    if (busy || !BOXES) return;
    for (int i = 0; i < S.menu.napps; i++) if (S.menu.apps[i].box_state == 4) { box_art(&S.menu.apps[i]); return; }
    if (S.launching || !S.boot_done) return;
    for (int i = 0; i < S.menu.napps; i++) {
      App *a = &S.menu.apps[i];
      if (!a->box_state && !a->box_pref && a->kind != KIND_SYSTEM) { if (box_submit(a, 1)) a->box_pref = 1; return; }
    }
    return;
  }
  App *a = menu_find_app(&S.menu, id);
  if (pref) { if (s) SDL_FreeSurface(s); return; }   /* prechargement : rien a afficher */
  if (a && s) {
    a->boxtex = SDL_CreateTextureFromSurface(S.r, s);
    a->box_state = a->boxtex ? 2 : 3;
    a->box_custom = custom;
    drop_tiles(a);                               /* la version provisoire est remplacee */
  } else if (a) a->box_state = 3;
  if (s) SDL_FreeSurface(s);
}

/* jaquette perso ajoutee, changee ou retiree (Forge5, FTP) : on la refait */
static void box_check_custom(void) {
  if (!BOXES) return;
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    if (a->box_state != 2 && a->box_state != 3) continue;
    if (cover_sig(a) == a->box_sig) continue;
    if (a->boxtex) { SDL_DestroyTexture(a->boxtex); a->boxtex = NULL; }
    a->box_state = 0;
    drop_tiles(a);
    plat_log("jaquette mise a jour : %s", a->id);
  }
}

/* titre du jeu en bas de la boite, sur deux lignes au plus */
static void box_title(App *a, int w, int h) {
  const char *n = tr(menu_display_name(&S.menu, a));
  char line[2][200] = { "", "" };
  int ln = 0, maxw = w - 36, tw, th;
  const char *p = n;
  while (*p && ln < 2) {
    const char *sp = strchr(p, ' ');
    size_t wl = sp ? (size_t)(sp - p) : strlen(p);
    char tryb[200];
    snprintf(tryb, sizeof tryb, "%s%s%.*s", line[ln], line[ln][0] ? " " : "", (int)wl, p);
    TTF_SizeUTF8(S.f30, tryb, &tw, &th);
    if (tw > maxw && line[ln][0] && ln == 0) { ln++; continue; }   /* mot suivant : ligne 2 */
    snprintf(line[ln], sizeof line[ln], "%s", tryb);
    p += wl;
    while (*p == ' ') p++;
  }
  if (*p && ln >= 1) {                           /* reste du titre : coupe avec « … » */
    char all[200];
    snprintf(all, sizeof all, "%s %s", line[1], p);
    snprintf(line[1], sizeof line[1], "%s", all);
  }
  int nl = line[1][0] ? 2 : 1, lh = 36, y = h - 30 - nl * lh;
  for (int i = 0; i < nl; i++) {
    draw_text_a(S.f30, line[i], w / 2 + 2, y + i * lh + 2, 0x000000, 1, maxw, 160);   /* ombre */
    draw_text(S.f30, line[i], w / 2, y + i * lh, 0xffffff, 1, maxw);
  }
}

static uint32_t mix_color(uint32_t c, uint32_t t, float k);
/* contenu d'une jaquette boite (w x h) dans la cible de rendu courante :
   un boitier en plastique bleu, la jaquette dedans, bandeau PS5 / PS4 en haut */
/* Boitiers en images : /data/launcher5/boitiers/ps5.png, ps4.png, autre.png (PNG dont la zone
   de la jaquette est transparente). La fenetre est reperee toute seule dans l'image. */
static struct { SDL_Texture *t; float x, y, w, h; uint32_t edge; int tried; } CASEIMG[3];

static void case_img_forget(void) {
  for (int i = 0; i < 3; i++) { if (CASEIMG[i].t) SDL_DestroyTexture(CASEIMG[i].t); memset(&CASEIMG[i], 0, sizeof CASEIMG[i]); }
}

static int case_img(App *a) {                    /* numero de l'image du boitier, -1 si aucune */
  int k = a->kind == KIND_PS5 ? 0 : a->kind == KIND_PS4 ? 1 : a->kind == KIND_HOMEBREW ? -1 : 2;
  if (k < 0) return -1;
  if (!CASEIMG[k].tried) {
    static const char *const NAMES[3] = { "ps5", "ps4", "autre" };
    CASEIMG[k].tried = 1;
    char path[PATH_LEN];
    SDL_Surface *s = NULL;
    for (int e = 0; e < 2 && !s; e++) {
      snprintf(path, sizeof path, "%s/boitiers/%s.%s", plat_data_dir(), NAMES[k], e ? "webp" : "png");
      s = argb(IMG_Load(path));
    }
    if (s) {
      Uint32 *px = s->pixels;
      int pitch = s->pitch / 4, cx = s->w / 2, cy = s->h / 2, t = cy, b = cy, l = cx, r = cx;
      #define AL(x, y) (px[(y) * pitch + (x)] >> 24)
      if (AL(cx, cy) < 40) {                     /* le centre doit etre transparent */
        while (t > 0 && AL(cx, t - 1) < 40) t--;
        while (b < s->h - 1 && AL(cx, b + 1) < 40) b++;
        while (l > 0 && AL(l - 1, cy) < 40) l--;
        while (r < s->w - 1 && AL(r + 1, cy) < 40) r++;
        CASEIMG[k].x = (float)l / s->w; CASEIMG[k].y = (float)t / s->h;
        CASEIMG[k].w = (float)(r - l + 1) / s->w; CASEIMG[k].h = (float)(b - t + 1) / s->h;
        CASEIMG[k].edge = px[cy * pitch + (l > 3 ? l / 2 : 1)] & 0xffffff;
        CASEIMG[k].t = SDL_CreateTextureFromSurface(S.r, s);
        if (CASEIMG[k].t) SDL_SetTextureBlendMode(CASEIMG[k].t, SDL_BLENDMODE_BLEND);
        plat_log("boitier en image : %s (fenetre %d,%d %dx%d)", path, l, t, r - l + 1, b - t + 1);
      } else plat_log("boitier en image : %s sans zone transparente au centre", path);
      #undef AL
      SDL_FreeSurface(s);
    }
  }
  return CASEIMG[k].t ? k : -1;
}

static void box_title(App *a, int w, int h);

/* Jaquette de payload : fond a la couleur de sa categorie, grand symbole (ou l'icone
   de Payload Manager, a ses proportions), version dessous. */
static void payload_style(const App *a, uint32_t *col, const char **glyph);
static void payload_art(App *a, SDL_Texture *ic, int iw, int ih, int top) {
  uint32_t c;
  const char *g;
  payload_style(a, &c, &g);
  for (int y = top; y < ih; y += 4)
    fill_round(0, y, iw, 4, 0, mix_color(shade(c, 0.55f), shade(c, 0.16f), (float)(y - top) / (ih - top)), 255);
  int cx = iw / 2, cy = top + (ih - top) * 40 / 100, r = iw * 30 / 100;
  if (ic) {                                      /* icone du gestionnaire : proportions gardees */
    int tw, th;
    SDL_QueryTexture(ic, NULL, NULL, &tw, &th);
    int dw = iw * 84 / 100, dh = th * dw / (tw ? tw : 1);
    if (dh > (ih - top) * 55 / 100) { dh = (ih - top) * 55 / 100; dw = tw * dh / (th ? th : 1); }
    fill_round(cx - dw / 2 - 6, cy - dh / 2 - 6, dw + 12, dh + 12, 10, 0x000000, 90);
    SDL_Rect d = { cx - dw / 2, cy - dh / 2, dw, dh };
    SDL_RenderCopy(S.r, ic, NULL, &d);
    r = dh / 2;
  } else {
    fill_round(cx - r, cy - r, r * 2, r * 2, r, c, 80);
    outline_round(cx - r, cy - r, r * 2, r * 2, r, iw >= 200 ? 4 : 2, mix_color(c, 0xffffff, 0.45f), 220);
    TTF_Font *f = iw >= 200 ? S.f84 : S.f48;
    draw_text(f, g, cx, cy - TTF_FontHeight(f) / 2, 0xffffff, 1, 0);
  }
  if (a->ver[0] && iw >= 140)
    draw_text(iw >= 200 ? S.f26 : S.f22, a->ver, cx, cy + r + (iw >= 200 ? 18 : 8), mix_color(c, 0xffffff, 0.65f), 1, iw - 16);
}

static void draw_box_contents(App *a, int w, int h) {
  /* le boitier, au style choisi dans le theme ; rouge pour les homebrews (reconnus d'un coup d'oeil) */
  int hb = a->kind == KIND_HOMEBREW, cs = S.base->case_style;
  int ci = cs == CASE_PHOTO ? case_img(a) : -1;
  if (ci >= 0) {                                 /* boitier en image : la jaquette dans la fenetre */
    SDL_Rect in = { (int)(CASEIMG[ci].x * w), (int)(CASEIMG[ci].y * h), (int)(CASEIMG[ci].w * w + 0.5f), (int)(CASEIMG[ci].h * h + 0.5f) };
    SDL_RenderSetViewport(S.r, &in);
    SDL_Texture *art = box_art(a);
    int kind = a->box_state == 2 ? a->box_custom : 0;
    if (art) {
      int tw, th;
      SDL_QueryTexture(art, NULL, NULL, &tw, &th);
      SDL_Rect sr = { 0, 0, tw, th };            /* recadree au format de la fenetre */
      if ((long)tw * in.h > (long)th * in.w) { sr.w = th * in.w / in.h; sr.x = (tw - sr.w) / 2; }
      else { sr.h = tw * in.h / in.w; sr.y = (th - sr.h) / 2; }
      SDL_RenderCopy(S.r, art, &sr, NULL);
      if (!kind && in.w >= 260) box_title(a, in.w, in.h);
    } else if (a->kind == KIND_PAYLOAD) {
      payload_art(a, cover(a), in.w, in.h, 0);
      if (in.w >= 260) box_title(a, in.w, in.h);
    } else {
      uint32_t c = a->has_vibe ? a->vibe : hash_color(a->id);
      fill_round(0, 0, in.w, in.h, 0, shade(c, 0.28f), 255);
      SDL_Texture *ic = cover(a);
      if (ic) { SDL_Rect d = { 0, (in.h - in.w) / 2, in.w, in.w }; SDL_RenderCopy(S.r, ic, NULL, &d); }
      if (in.w >= 260) box_title(a, in.w, in.h);
    }
    SDL_RenderSetViewport(S.r, NULL);
    SDL_RenderCopy(S.r, CASEIMG[ci].t, NULL, NULL);
    return;
  }
  uint32_t cb, cl, cd, cr;
  int ca = 255;                                  /* opacite du plastique */
  switch (hb && cs != CASE_NONE ? -1 : cs) {
  case -1:          cb = 0xc81e24; cl = 0xff8a8a; cd = 0x6e0b0e; cr = 0xffb0b0; break;
  case CASE_BLACK:  cb = 0x15171c; cl = 0x4a505c; cd = 0x050608; cr = 0x6a717e; break;
  case CASE_WHITE:  cb = 0xf1f2f4; cl = 0xffffff; cd = 0xa7aeb8; cr = 0xffffff; break;
  case CASE_CRYSTAL: cb = 0xdfe8f2; cl = 0xffffff; cd = 0x8796a8; cr = 0xffffff; ca = 175; break;
  case CASE_STEEL:  cb = 0x8c959f; cl = 0xe6ebf0; cd = 0x3c434b; cr = 0xc9d0d8; break;
  case CASE_THEME:  cb = shade(S.base->accent, 0.85f); cl = mix_color(S.base->accent, 0xffffff, 0.45f);
                    cd = shade(S.base->accent, 0.4f); cr = mix_color(S.base->accent, 0xffffff, 0.6f); break;
  default:          cb = 0x1a56d6; cl = 0x6fa4ff; cd = 0x0d2f80; cr = 0x8fbaff; break;
  }
  int m = w * 4 / 100 + 1, mt = h * 3 / 100 + 1;
  if (cs == CASE_NONE) {                         /* jaquette seule */
    m = mt = 0;
    if (hb) { m = mt = 4; fill_round(0, 0, w, h, 0, 0xc81e24, 255); }
  } else if (cs == CASE_STEEL) {                 /* metal brosse : degrade vertical et reflets */
    for (int y = 0; y < h; y += 2) {
      float k = (float)y / h, sh = 0.75f + 0.25f * sinf(k * 9.0f);
      fill_round(0, y, w, 2, 0, mix_color(cd, cl, sh * (1 - 0.5f * k)), 255);
    }
  } else {
    for (int y = 0; y < h; y += 3)               /* plastique : plus clair en haut, plus sombre en bas */
      fill_round(0, y, w, 3, 0, mix_color(mix_color(cb, cl, 0.35f), mix_color(cb, cd, 0.35f), (float)y / h), ca);
    fill_round(0, 0, w, 3, 0, cl, 140);
    fill_round(0, h - 3, w, 3, 0, cd, 160);
    for (int i = 1; i < 6; i++) {                /* petits ergots le long des bords */
      fill_round(w * i / 6 - 6, 2, 12, 2, 0, cr, 110);
      fill_round(w * i / 6 - 6, h - 4, 12, 2, 0, cr, 90);
    }
    fill_round(w - m / 2 - 1, h * 45 / 100, 2, h / 10, 0, cr, 120);   /* clip de fermeture */
  }
  SDL_Rect in = { m, mt, w - 2 * m, h - 2 * mt };
  SDL_RenderSetViewport(S.r, &in);
  int iw = in.w, ih = in.h;
  SDL_Texture *art = box_art(a);
  int kind = a->box_state == 2 ? a->box_custom : 0;   /* 0 fabriquee, 1 perso, 2 officielle */
  int band = kind != 1 && cs != CASE_STEEL;      /* le steelbook n'a pas de bandeau */
  int bh = band ? ih * 10 / 100 : 0;
  if (art && kind == 2) {                        /* officielle : l'image sous le bandeau */
    int tw, th;
    SDL_QueryTexture(art, NULL, NULL, &tw, &th);
    int ah = ih - bh, sh = tw * ah / iw;         /* recadrage au format de la place restante */
    if (sh > th) sh = th;
    SDL_Rect sr = { 0, (th - sh) / 2, tw, sh }, dr = { 0, bh, iw, ah };
    SDL_RenderCopy(S.r, art, &sr, &dr);
  } else if (art) SDL_RenderCopy(S.r, art, NULL, NULL);
  else if (a->kind == KIND_PAYLOAD) {            /* payload : jaquette generee */
    payload_art(a, cover(a), iw, ih, bh);
    SDL_Rect bot = { 0, ih * 70 / 100, iw, ih - ih * 70 / 100 };
    if (S.shade_bottom) SDL_RenderCopy(S.r, S.shade_bottom, NULL, &bot);
  } else {                                       /* provisoire : meme mise en page avec l'icone */
    uint32_t c = a->has_vibe ? a->vibe : hash_color(a->id);
    fill_round(0, 0, iw, ih, 0, shade(c, 0.28f), 255);
    SDL_Texture *ic = cover(a);
    if (ic) { SDL_Rect d = { 0, (ih - iw) / 2 - ih * 4 / 100, iw, iw }; SDL_RenderCopy(S.r, ic, NULL, &d); }
    SDL_Rect bot = { 0, ih * 62 / 100, iw, ih - ih * 62 / 100 };
    if (S.shade_bottom) SDL_RenderCopy(S.r, S.shade_bottom, NULL, &bot);
  }
  if (band) {                                    /* jaquette perso, steelbook : sans bandeau */
    if (a->kind == KIND_PS4) {                   /* PS4 : bleu en degrade, texte blanc */
      for (int x = 0; x < iw; x += 3) {
        float t = (float)x / iw;
        uint32_t col = mix_color(0x0a3a9c, 0x168fd9, t);
        fill_round(x, 0, 3, bh, 0, col, 255);
      }
    } else fill_round(0, 0, iw, bh, 0, a->kind == KIND_PS5 ? 0xf6f6f8 : hb ? 0xa3151a : shade(S.th->accent, 0.8f), 255);
    fill_round(0, bh, iw, 2, 0, a->kind == KIND_PS5 ? 0x1a1a22 : 0xffffff, a->kind == KIND_PS5 ? 60 : 200);
    uint32_t tc = a->kind == KIND_PS5 ? 0x0a0a12 : 0xffffff;
    const char *lab = a->kind == KIND_PS5 ? "PS5" : a->kind == KIND_PS4 ? "PS4" : KIND_LABEL[a->kind];
    if (iw >= 260) {
      draw_text(S.f30, lab, 16, (bh - 36) / 2, tc, 0, iw - 32);
      if (!kind) box_title(a, iw, ih);           /* l'officielle a deja son titre */
    }
  } else if (cs == CASE_STEEL && !kind && iw >= 260) box_title(a, iw, ih);
  SDL_RenderSetViewport(S.r, NULL);
  if (m) {                                       /* relief : la jaquette est enfoncee sous le plastique */
    outline_round(in.x - 1, in.y - 1, in.w + 2, in.h + 2, 2, 2, 0x000000, 70);
    outline_round(in.x - 3, in.y - 3, in.w + 6, in.h + 6, 4, 1, 0xffffff, 60);
  }
  if (cs != CASE_NONE) {                         /* reflet brillant en diagonale sur la facade */
    SDL_Vertex v[4];
    memset(v, 0, sizeof v);
    float pts[4][2] = { { 0, 0 }, { w * 0.62f, 0 }, { w * 0.18f, h * 0.55f }, { 0, h * 0.55f } };
    int al[4] = { 46, 0, 0, 22 };
    for (int i = 0; i < 4; i++) {
      v[i].position.x = pts[i][0]; v[i].position.y = pts[i][1];
      v[i].color.r = v[i].color.g = v[i].color.b = 255; v[i].color.a = (Uint8)al[i];
    }
    static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(S.r, NULL, v, 4, idx, 6);
  }
}

/* Tuile complete au format boite (w x h), calculee une seule fois :
   jaquette verticale si on l'a, sinon carte avec l'icone et le nom. */
static SDL_Texture *tile_tex(App *a, int w, int h) {
  void **slot = w == TILE ? &a->tex_s : w == TILE_SEL ? &a->tex_l : w == FLOW ? &a->tex_xl : NULL;
  if (!slot) return NULL;                    /* seules ces tailles sont mises en cache */
  if (*slot) return *slot;
  SDL_Texture *big = BOXES && w != FLOW ? tile_tex(a, FLOW, TH(FLOW)) : NULL;   /* petites boites : reduites */
  SDL_Texture *t = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
  if (!t) return NULL;
  SDL_Texture *prev_rt = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, t);
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
  SDL_RenderClear(S.r);
  if (big) SDL_RenderCopy(S.r, big, NULL, NULL);
  else if (BOXES) draw_box_contents(a, w, h);
  else {
    SDL_Texture *img = cover(a);
    if (img) SDL_RenderCopy(S.r, img, NULL, NULL);
    else draw_card_contents(a, w, h);
  }
  mask_corners(w, h, tile_rad() * w / TILE_SEL);
  SDL_SetRenderTarget(S.r, prev_rt);   /* cible d'avant (dessin imbrique) */
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  *slot = t;
  return t;
}

static void warp_forget(SDL_Texture *src);

/* oublie les tuiles calculees (nom ou forme change) */
static int tiles_gen;                            /* change des qu'une jaquette est refaite */
static void drop_tiles(App *a) {
  tiles_gen++;
  void **slots[] = { &a->tex_s, &a->tex_l, &a->tex_xl };
  for (int i = 0; i < 3; i++) if (*slots[i]) { warp_forget(*slots[i]); SDL_DestroyTexture(*slots[i]); *slots[i] = NULL; }
}

static void draw_card_contents(App *a, int w, int h) {
  uint32_t c = hash_color(a->id);
  int rad = tile_rad() * w / TILE_SEL;
  fill_round(0, 0, w, h, rad, shade(c, 0.4f), 255);
  fill_round(10, 10, w - 20, h / 2, rad * 3 / 4, shade(c, 0.6f), 255);
  const char *n = tr(menu_display_name(&S.menu, a));
  char init[8];
  snprintf(init, sizeof init, (unsigned char)n[0] >= 0xC0 ? "%.2s" : "%.1s", n);
  draw_text(S.fbig, init, w / 2, h / 2 - 78, 0xffffff, 1, 0);
  draw_text(S.f22, n, w / 2, h - 44, 0xffffff, 1, w - 20);
}

/* Fond flou : l'illustration (ou la jaquette) reduite a quelques pixels puis
   agrandie avec filtrage, ce qui donne un flou doux pour presque rien. */
static SDL_Texture *blurred(App *a) {
  if (a->bgtex || a->bg_failed) return a->bgtex;
  SDL_Texture *src = NULL;
  int own = 0;
  if (a->bg[0]) {
    SDL_Surface *s = IMG_Load(a->bg);
    if (s) { src = SDL_CreateTextureFromSurface(S.r, s); SDL_FreeSurface(s); own = 1; }
  }
  if (!src) src = cover(a);
  if (!src) { a->bg_failed = 1; return NULL; }
  SDL_Texture *small = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 48, 27);
  if (!small) { if (own) SDL_DestroyTexture(src); a->bg_failed = 1; return NULL; }
  SDL_Texture *prev_rt = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, small);
  SDL_RenderCopy(S.r, src, NULL, NULL);
  SDL_SetRenderTarget(S.r, prev_rt);   /* cible d'avant (dessin imbrique) */
  if (own) SDL_DestroyTexture(src);
  a->bgtex = small;
  return small;
}

/* ------------------------------------------------------------------ */
/* Dessin                                                              */
/* ------------------------------------------------------------------ */
static void draw_background_full(Entry *e, int animated);

/* Particules du theme : quantite, forme et sens reglables */
static void draw_particles(int lite) {
  int n = S.th->particles == PART_NONE ? 0 : S.th->particles == PART_FEW ? (lite ? 18 : 35) : (lite ? 40 : 70);
  float t = S.frame / 60.0f, speed = (lite ? 2.0f : 1.0f) * S.dt * 60;
  for (int i = 0; i < n; i++) {
    float v = S.part[i].v * speed * (S.th->pstyle == PSTYLE_SNOW ? 0.7f : 1.0f);
    if (S.th->fall) {
      S.part[i].y += v;
      if (S.part[i].y > H + 10) { S.part[i].y = -10; S.part[i].x = (float)(rand() % W); }
    } else {
      S.part[i].y -= v;
      if (S.part[i].y < -10) { S.part[i].y = H + 10; S.part[i].x = (float)(rand() % W); }
    }
    int x = (int)S.part[i].x, y = (int)S.part[i].y, sz = (int)S.part[i].s;
    uint32_t c = i % 3 ? S.th->accent : S.th->accent2;
    int a = (int)(70 + 60 * sinf(t * 2 + i));
    switch (S.th->pstyle) {
    case PSTYLE_SPARKS: {
      set_color(c, a + 40 > 255 ? 255 : a + 40);
      SDL_Rect h1 = { x - sz * 2, y, sz * 4 + 1, 1 }, v1 = { x, y - sz * 2, 1, sz * 4 + 1 };
      SDL_RenderFillRect(S.r, &h1);
      SDL_RenderFillRect(S.r, &v1);
      break;
    }
    case PSTYLE_SNOW: {
      x += (int)(14 * sinf(t * 0.8f + i * 1.7f));
      set_color(c, a * 3 / 4);
      SDL_Rect p = { x, y, sz * 2, sz * 2 };
      SDL_RenderFillRect(S.r, &p);
      break;
    }
    default: {
      set_color(c, a);
      SDL_Rect p = { x, y, sz, sz };
      SDL_RenderFillRect(S.r, &p);
    }
    }
  }
}

/* Image de fond perso (dossier fonds/), chargee une fois */
static SDL_Texture *wallpaper(void) {
  if (S.th->bg_mode != BG_IMAGE || !S.th->wallpaper[0]) return NULL;
  /* image absente ou illisible (envoi depuis le PC en cours) : nouvel essai toutes les 3 s */
  static Uint32 tried;
  int same = !strcmp(S.wall_key, S.th->wallpaper);
  if (!same || (!S.wall && SDL_GetTicks() - tried > 3000)) {
    if (S.wall) SDL_DestroyTexture(S.wall);
    char path[PATH_LEN];
    snprintf(path, sizeof path, "%s/fonds/%s", plat_data_dir(), S.th->wallpaper);
    S.wall = load_tex(path);
    tried = SDL_GetTicks();
    if (!S.wall && !same) plat_log("image de fond absente ou illisible (nouvel essai toutes les 3 s) : %s", path);
    if (S.wall && same) S.th_ver++;              /* arrivee entre-temps : fond a redessiner */
    snprintf(S.wall_key, sizeof S.wall_key, "%s", S.th->wallpaper);
  }
  return S.wall;
}

/* Mode leger (sans carte graphique) : le fond complet est calcule une seule
   fois dans une texture, puis simplement recopie a chaque image. */
static int bg_w = W;                            /* largeur du fond dessine (plus large en cache : parallaxe) */
static void draw_background(Entry *e) {
  if (!S.lite) { draw_background_full(e, 1); return; }
  /* l'illustration du jeu ne compte que si le theme l'affiche */
  App *key_app = (e && !e->is_folder && S.th->bg_mode == BG_ART && S.th->art > 0) ? e->app : NULL;
  /* on attend que la selection soit stable (defilement rapide = pas de recalcul) */
  static App *pending = NULL; static Uint32 since = 0;
  if (key_app != pending) { pending = key_app; since = SDL_GetTicks(); }
  int stable = SDL_GetTicks() - since > 150 || !S.bgcache;
  if (stable && (S.bg_key_app != key_app || S.bg_key_ver != S.th_ver || S.bg_key_acc != (S.th->accent ^ S.th->accent2 << 1) ||
                 (key_app && !key_app->bgtex && !key_app->bg_failed) || !S.bgcache)) {
    if (!S.bgcache) S.bgcache = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W + 2 * PAR, H);
    S.bg_key_app = key_app;
    S.bg_key_ver = S.th_ver;
    S.bg_key_acc = S.th->accent ^ S.th->accent2 << 1;
    SDL_SetRenderTarget(S.r, S.bgcache);
    S.bg_cur = S.bg_prev = NULL;
    S.bg_fade = 1;
    bg_w = W + 2 * PAR;
    draw_background_full(key_app ? e : NULL, 0);
    bg_w = W;
    SDL_SetRenderTarget(S.r, NULL);
    SDL_SetTextureBlendMode(S.bgcache, SDL_BLENDMODE_NONE);
  }
  {                                              /* parallaxe : de gauche a droite le long de la liste */
    float t = S.sec == SEC_SYS || S.nentries < 2 ? 0.5f : (float)*cur_sel() / (S.nentries - 1);
    float want = (t * 2 - 1) * PAR * 0.85f;
    par_x += (want - par_x) * ease(0.06f);
    if (fabsf(want - par_x) < 0.5f) par_x = want;
    SDL_Rect sr = { PAR + (int)par_x, 0, W, H };
    SDL_RenderCopy(S.r, S.bgcache, &sr, NULL);
  }
  draw_particles(1);   /* un peu de vie par-dessus */
}

static void draw_background_full(Entry *e, int animated) {
  /* degrade du theme */
  for (int y = 0; y < H; y += 6) {
    float k = (float)y / H;
    uint32_t t = S.th->top, b = S.th->bottom;
    int r = (int)(((t >> 16) & 255) * (1 - k) + ((b >> 16) & 255) * k);
    int g = (int)(((t >> 8) & 255) * (1 - k) + ((b >> 8) & 255) * k);
    int bl = (int)((t & 255) * (1 - k) + (b & 255) * k);
    SDL_SetRenderDrawColor(S.r, r, g, bl, 255);
    SDL_Rect rr = { 0, y, bg_w, 6 };
    SDL_RenderFillRect(S.r, &rr);
  }

  /* image perso : remplit l'ecran sans deformation */
  SDL_Texture *wall = wallpaper();
  if (wall) {
    int iw, ih;
    SDL_QueryTexture(wall, NULL, NULL, &iw, &ih);
    SDL_Rect src = { 0, 0, iw, ih };
    if ((long)iw * H > (long)ih * W) { src.w = (int)((long)ih * W / H); src.x = (iw - src.w) / 2; }
    else { src.h = (int)((long)iw * H / W); src.y = (ih - src.h) / 2; }
    SDL_SetTextureAlphaMod(wall, 255);
    SDL_RenderCopy(S.r, wall, &src, NULL);
  }

  /* illustration du jeu selectionne, floutee, en fondu */
  int art = S.th->bg_mode == BG_ART ? S.th->art * 255 / 100 : 0;
  SDL_Texture *want = (e && !e->is_folder && art) ? blurred(e->app) : NULL;
  if (want != S.bg_cur) { S.bg_prev = S.bg_cur; S.bg_cur = want; S.bg_fade = animated ? 0 : 1; }
  S.bg_fade += (1 - S.bg_fade) * ease(0.12f);
  SDL_Rect full = { -40, -40, bg_w + 80, H + 80 };
  if (S.bg_prev && animated && art) { SDL_SetTextureAlphaMod(S.bg_prev, (Uint8)(art * (1 - S.bg_fade))); SDL_RenderCopy(S.r, S.bg_prev, NULL, &full); }
  if (S.bg_cur) { SDL_SetTextureAlphaMod(S.bg_cur, (Uint8)(art * S.bg_fade)); SDL_RenderCopy(S.r, S.bg_cur, NULL, &full); }

  /* halos aux couleurs du theme (animes seulement avec carte graphique) */
  float t = animated ? S.frame / 60.0f : 0;
  if (S.th->glow) {
    glow((int)(W * 0.78f + 160 * sinf(t * 0.21f)), (int)(H * 0.30f + 90 * cosf(t * 0.17f)), 720, S.th->accent, ga(70));
    glow((int)(W * 0.22f + 200 * cosf(t * 0.13f)), (int)(H * 0.85f + 70 * sinf(t * 0.19f)), 640, S.th->accent2, ga(55));
  }

  /* assombrissement pour la lisibilite (plus leger sur une image perso) */
  SDL_SetTextureAlphaMod(S.shade_left, wall ? 200 : 255);
  SDL_SetTextureAlphaMod(S.shade_bottom, wall ? 220 : 255);
  SDL_Rect left = { 0, 0, W * 3 / 4 + (bg_w - W) / 2, H };
  SDL_RenderCopy(S.r, S.shade_left, NULL, &left);
  SDL_Rect bottom = { 0, H / 2, bg_w, H / 2 };
  SDL_RenderCopy(S.r, S.shade_bottom, NULL, &bottom);

  if (S.th->fx == FX_GRID) {                     /* soleil retro pose sur l'horizon */
    int cx = W / 2, cy = 560, R = 250;
    for (int y = -R; y < R / 3; y += 2) {
      float k = (float)(y + R) / (R * 4 / 3);
      if (y > -R / 4 && ((y + R) / 2) % (6 + (y + R / 4) / 14) < 2 + (y + R / 4) / 10) continue;   /* bandes */
      int hw = (int)sqrtf((float)R * R - (float)y * y);
      uint32_t c1 = S.th->accent, c2 = S.th->accent2;
      uint32_t c = (uint32_t)(((int)(((c2 >> 16) & 255) * (1 - k) + ((c1 >> 16) & 255) * k)) << 16 |
                              ((int)(((c2 >> 8) & 255) * (1 - k) + ((c1 >> 8) & 255) * k)) << 8 |
                              ((int)((c2 & 255) * (1 - k) + (c1 & 255) * k)));
      set_color(c, 150);
      SDL_Rect rr = { cx - hw, cy + y, hw * 2, 2 };
      SDL_RenderFillRect(S.r, &rr);
    }
  }

  if (animated) draw_particles(0);
}

/* ------------------------------------------------------------------ */
/* Effets de fond animes (V2)                                          */
/* ------------------------------------------------------------------ */
static void fx_grid(void) {                      /* grille synthwave qui avance vers toi */
  int hz = 600, vx = W / 2;
  S.grid_phase += S.dt * 0.6f;
  if (S.grid_phase > 1) S.grid_phase -= 1;
  set_color(S.th->accent, 120);
  SDL_RenderDrawLine(S.r, 0, hz, W, hz);
  int n = S.lite ? 12 : 18;
  for (int k = 0; k < n; k++) {
    float t = (k + S.grid_phase) / n, y = hz + (H - hz) * t * t;
    set_color(S.th->accent, (int)(20 + 90 * t));
    SDL_RenderDrawLine(S.r, 0, (int)y, W, (int)y);
  }
  for (int i = -12; i <= 12; i++) {
    set_color(S.th->accent, 50);
    SDL_RenderDrawLine(S.r, vx + i * 30, hz, vx + i * 260, H);
  }
}

static void fx_warp(void) {                      /* hyperespace : etoiles qui filent */
  int n = S.lite ? 80 : 160;
  float sp = S.dt * 60;
  for (int i = 0; i < n; i++) {
    if (S.star[i].z <= 0.02f) {
      S.star[i].x = (rand() % 2000 - 1000) / 1000.0f; S.star[i].y = (rand() % 2000 - 1000) / 1000.0f;
      S.star[i].z = 1; S.star[i].px = S.star[i].py = -1;
    }
    S.star[i].z -= 0.006f * sp * (0.5f + S.star[i].z);
    if (S.star[i].z <= 0.02f) continue;
    float sx = W / 2 + S.star[i].x / S.star[i].z * W * 0.5f, sy = H / 2 + S.star[i].y / S.star[i].z * H * 0.5f;
    if (sx < 0 || sx > W || sy < 0 || sy > H) { S.star[i].z = 0; continue; }
    if (S.star[i].px >= 0) {
      int a = (int)(255 * (1.15f - S.star[i].z));
      if (a > 255) a = 255;
      set_color(i % 3 ? 0xffffff : S.th->accent, a);
      /* trainee allongee vers le centre, epaisse de 2 pixels quand l'etoile est proche */
      float tx = sx - (sx - W / 2) * 0.06f, ty = sy - (sy - H / 2) * 0.06f;
      SDL_RenderDrawLine(S.r, (int)tx, (int)ty, (int)sx, (int)sy);
      if (S.star[i].z < 0.5f) SDL_RenderDrawLine(S.r, (int)tx + 1, (int)ty, (int)sx + 1, (int)sy);
    }
    S.star[i].px = sx; S.star[i].py = sy;
  }
}

static void fx_rain(void) {                      /* pluie de code */
  static const char CH[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (!S.glyph[0]) {
    for (int i = 0; i < 36; i++) {
      char s[2] = { CH[i], 0 };
      SDL_Color w = { 255, 255, 255, 255 };
      SDL_Surface *g = TTF_RenderUTF8_Blended(S.f22, s, w);
      if (!g) return;
      S.glyph[i] = SDL_CreateTextureFromSurface(S.r, g);
      SDL_FreeSurface(g);
      if (!S.glyph[i]) return;
      SDL_SetTextureBlendMode(S.glyph[i], SDL_BLENDMODE_BLEND);
    }
  }
  int cols = S.lite ? 32 : 48, cw = W / cols, gh = 26;
  for (int c = 0; c < cols; c++) {
    if (S.drop[c].v <= 0) { S.drop[c].v = 3 + rand() % 6; S.drop[c].y = -(rand() % H); S.drop[c].len = 8 + rand() % 14; S.drop[c].seed = rand(); }
    S.drop[c].y += S.drop[c].v * S.dt * 60;
    if (S.drop[c].y - S.drop[c].len * gh > H) S.drop[c].v = 0;
    int head = (int)(S.drop[c].y / gh);
    for (int k = 0; k < S.drop[c].len; k++) {
      int row = head - k, y = row * gh;
      if (y < -gh || y > H) continue;
      SDL_Texture *t = S.glyph[(unsigned)(S.drop[c].seed + row * 7 + (k == 0 ? S.frame / 4 : 0)) % 36];
      uint32_t col = k == 0 ? 0xffffff : S.th->accent;
      SDL_SetTextureColorMod(t, (col >> 16) & 255, (col >> 8) & 255, col & 255);
      int al = k == 0 ? 150 : 95 * (S.drop[c].len - k) / S.drop[c].len;
      if (y < 150) al = al * (y < 0 ? 0 : y) / 150 / 3;      /* presque invisible derriere la barre du haut */
      SDL_SetTextureAlphaMod(t, (Uint8)al);
      int w, h; SDL_QueryTexture(t, NULL, NULL, &w, &h);
      SDL_Rect d = { c * cw + (cw - w) / 2, y, w, h };
      SDL_RenderCopy(S.r, t, NULL, &d);
    }
  }
}

static void draw_fx(void) {
  switch (S.th->fx) {
  case FX_GRID: fx_grid(); break;
  case FX_WARP: fx_warp(); break;
  case FX_RAIN: fx_rain(); break;
  }
}

static void draw_topbar(void) {
  /* logo */
  int lx = CAR_X;
  draw_text(S.f36, "LAUNCHER", lx, 44, 0xffffff, 0, 0);
  int lw = text_w(S.f36, "LAUNCHER");
  draw_text(S.f36, "5", lx + lw + 4, 44, S.th->accent, 0, 0);
  draw_text_a(S.f22, "v" L5_VERSION "  ·  by " L5_AUTHOR, lx + 2, 90, 0x9aa4b5, 0, 0, 150);

  /* sections : la bande defile si elles ne tiennent pas toutes */
  int x0 = lx + lw + 90, x1 = S.mode == MODE_THEME ? W - 880 : W - CAR_X - 230;
  int total = 0, cur_x = 0, cur_w = 0;
  for (int k = 0; k < SEC_COUNT; k++) {
    int s = SEC_ORDER[k];
    if (!S.sec_has[s]) continue;
    int w = text_w(S.f26, SEC_NAMES[s]);
    if (s == S.sec) { cur_x = total; cur_w = w; }
    total += w + 46;
  }
  static float off;
  float want = 0;
  if (total - 46 > x1 - x0) {
    want = cur_x + cur_w / 2.0f - (x1 - x0) / 2.0f;
    if (want < 0) want = 0;
    if (want > total - 46 - (x1 - x0)) want = (float)(total - 46 - (x1 - x0));
  }
  off += (want - off) * ease(0.25f);
  SDL_Rect clip = { x0 - 4, 30, x1 - x0 + 8, 90 };
  SDL_RenderSetClipRect(S.r, &clip);
  int x = x0 - (int)off, tx = 0, tw = 0;
  for (int k = 0; k < SEC_COUNT; k++) {
    int s = SEC_ORDER[k];
    if (!S.sec_has[s]) continue;
    int w = text_w(S.f26, SEC_NAMES[s]);
    int on = s == S.sec;
    draw_text(S.f26, SEC_NAMES[s], x, 52, on ? 0xffffff : 0x9aa4b5, 0, 0);
    if (on) { tx = x; tw = w; }
    x += w + 46;
  }
  if (off > 1) draw_text(S.f26, "‹", x0 - 26, 50, S.th->accent, 0, 0);
  if (want < total - 46 - (x1 - x0) - 1 && total - 46 > x1 - x0) draw_text(S.f26, "›", x1 - 6, 50, S.th->accent, 0, 0);
  if (!S.tab_w) { S.tab_x = (float)tx; S.tab_w = (float)tw; }
  S.tab_x += (tx - S.tab_x) * ease(0.25f);
  S.tab_w += (tw - S.tab_w) * ease(0.25f);
  fill_round((int)S.tab_x, 92, (int)S.tab_w, 5, 2, S.th->accent, 255);
  if (!S.lite) glow((int)(S.tab_x + S.tab_w / 2), 94, (int)(S.tab_w * 0.9f), S.th->accent, ga(90));
  SDL_RenderSetClipRect(S.r, NULL);

  /* heure et date */
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char clk[16], day[32];
  static const char *JOURS[] = { "dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam." };
  if (S.th->clock == CLOCK_HIDDEN || S.mode == MODE_THEME) return;
  if (S.th->clock == CLOCK_12) {
    int h12 = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
    snprintf(clk, sizeof clk, "%d:%02d %s", h12, tm->tm_min, tm->tm_hour < 12 ? "AM" : "PM");
  } else strftime(clk, sizeof clk, "%H:%M", tm);
  snprintf(day, sizeof day, "%s %d/%02d", tr(JOURS[tm->tm_wday]), tm->tm_mday, tm->tm_mon + 1);
  draw_text(S.f36, clk, W - CAR_X, 40, 0xffffff, 2, 0);
  draw_text(S.f22, day, W - CAR_X, 86, 0x9aa4b5, 2, 0);
  /* IP (pratique pour le FTP), espace libre, temperature : mis a jour toutes les 5 s */
  if (!S.sys_t || SDL_GetTicks() - S.sys_t > 5000) {
    char ip[64]; long long mb; int tc;
    plat_sysinfo(ip, sizeof ip, &mb, &tc);
    char part[3][48] = { "", "", "" };
    if (ip[0] && !S.menu.hide_ip) snprintf(part[0], sizeof part[0], "%s", ip);
    if (mb >= 0) snprintf(part[1], sizeof part[1], mb >= 10240 ? tr("%lld Go libres") : tr("%lld Mo libres"),
                          mb >= 10240 ? mb / 1024 : mb);
    if (tc > 0) snprintf(part[2], sizeof part[2], "%d°C", tc);
    S.sysline[0] = 0;
    for (int i = 0; i < 3; i++) if (part[i][0]) {
      if (S.sysline[0]) strncat(S.sysline, "  ·  ", sizeof S.sysline - strlen(S.sysline) - 1);
      strncat(S.sysline, part[i], sizeof S.sysline - strlen(S.sysline) - 1);
    }
    S.sys_t = SDL_GetTicks();
  }
  if (S.sysline[0]) draw_text_a(S.f22, S.sysline, W - CAR_X, 114, 0x9aa4b5, 2, 0, 170);
  const char *act = active_line();               /* payloads actifs, sous l'IP */
  int toasting = S.toast[0] && SDL_GetTicks() < S.toast_until;   /* un message passe devant */
  if (act[0] && !toasting) draw_text_a(S.f22, act, W - CAR_X, S.sysline[0] ? 140 : 114, 0x2ecc71, 2, 0, 200);
}

static void playtime_str(long secs, char *out, size_t n) {
  if (secs < 3600) snprintf(out, n, tr("%ld min de jeu"), secs / 60 > 0 ? secs / 60 : 1);
  else snprintf(out, n, tr("%ld h %02ld de jeu"), secs / 3600, (secs % 3600) / 60);
}

static void ago(time_t when, char *out, size_t n) {
  long d = (long)(time(NULL) - when);
  if (d < 60) snprintf(out, n, "à l'instant");
  else if (d < 3600) snprintf(out, n, tr("il y a %ld min"), d / 60);
  else if (d < 86400) snprintf(out, n, tr("il y a %ld h"), d / 3600);
  else snprintf(out, n, tr("il y a %ld j"), d / 86400);
}

static void pill(int x, int y, const char *txt, uint32_t bg, uint32_t fg, int filled, TTF_Font *f) {
  int w = text_w(f, txt), h = TTF_FontHeight(f);
  if (filled) fill_round(x, y, w + 56, h + 26, (h + 26) / 2, bg, 255);
  else outline_round(x, y, w + 56, h + 26, (h + 26) / 2, 2, bg, 200);
  draw_text(f, txt, x + 28, y + 13, fg, 0, 0);
}

static int pay_active(const App *a);
static int boot_index(const App *a);
/* byepervisor, HyperHEN… : demandent le mode repos. Visibles (mises a jour par le catalogue),
   mais jamais envoyes ni mis au demarrage. */
static int pay_incompatible(const App *a) {
  if (!a || a->kind != KIND_PAYLOAD) return 0;
  const char *b = strrchr(a->path, '/');
  return payload_excluded(a->title) || payload_excluded(b ? b + 1 : a->path);
}
static int payload_sensitive(const App *a);
static const char *pay_label(const App *a);
/* --- Reprendre / Nouveau : le dernier jeu lance et les jeux tout juste installes --- */
static App *resume_app(void) {                   /* dernier jeu lance depuis moins de 14 jours */
  static Uint32 at = (Uint32)-1;
  static App *cached;
  if (at == S.frame) return cached;
  at = S.frame;
  App *best = NULL;
  time_t bt = 0, now = time(NULL);
  for (int i = 0; i < S.menu.nstats; i++) {
    Stat *st = &S.menu.stats[i];
    if (st->last <= bt || now - st->last > 14 * 86400) continue;
    App *a = menu_find_app(&S.menu, st->id);
    if (!a || a->kind == KIND_SYSTEM || a->kind == KIND_PAYLOAD || menu_is_hidden(&S.menu, a->id)) continue;
    best = a;
    bt = st->last;
  }
  return cached = best;
}

static int app_is_new(App *a) {                  /* installe il y a moins de 7 jours, jamais lance */
  if (!a || a->kind == KIND_SYSTEM || a->kind == KIND_PAYLOAD) return 0;
  Stat *st = menu_stat(&S.menu, a->id, 0);
  if (st && st->launches) return 0;
  return app_tag(a, FIL_NEW);
}

/* texte de la pastille (« ▶ REPRENDRE », « NOUVEAU ») ou NULL */
static const char *fresh_tag(Entry *e, char *buf, size_t n) {
  if (!e || e->is_folder) return NULL;
  if (e->app->kind == KIND_PAYLOAD) {
    if (pay_incompatible(e->app)) { snprintf(buf, n, "⊘ %s", tr("INCOMPATIBLE")); return buf; }
    if (pay_active(e->app)) { snprintf(buf, n, "● %s %s", pay_label(e->app), tr("ACTIF")); return buf; }
    int k = boot_index(e->app);
    if (k >= 0) { snprintf(buf, n, tr("AU DÉMARRAGE %d"), k + 1); return buf; }
    if (payload_sensitive(e->app)) { snprintf(buf, n, "⚠ %s", tr("NOYAU")); return buf; }   /* a lancer au demarrage */
    return NULL;
  }
  if (e->app == resume_app()) { snprintf(buf, n, "▶ %s", tr("REPRENDRE")); return buf; }
  if (app_is_new(e->app)) { snprintf(buf, n, "%s", tr("NOUVEAU")); return buf; }
  return NULL;
}

static int fresh_pill_w(const char *t) { return t ? text_w(S.f22, t) + 30 : 0; }

static void fresh_pill(const char *t, int x, int y, int h, int a) {
  if (!t) return;
  int w = fresh_pill_w(t), on = !strncmp(t, "●", 3), off = !strncmp(t, "⊘", 3);   /* actif : vert, incompatible : rouge */
  fill_round(x, y, w, h, h / 2, on ? 0x2ecc71 : off ? 0xd63a3a : 0xffffff, (int)(0.92f * a));
  draw_text_a(S.f22, t, x + 15, y + (h - 26) / 2, off ? 0xffffff : 0x0a0a12, 0, 0, a);
}

/* ------------------------------------------------------------------ */
/* Payloads connus : categorie, description, et port ouvert quand ils   */
/* tournent (statut « ACTIF » verifie en arriere-plan)                  */
/* ------------------------------------------------------------------ */
/* probe : 1 seulement si une connexion de test est sans effet sur le payload (FTP, web) ;
   les autres (shell, journal, debogueur) ne sont jamais contactes. */
typedef struct { const char *key, *cat, *desc; int port, probe; } PayInfo;
/* nom du service verifie (la pastille dit « FTP ACTIF » : un serveur FTP tourne, quel qu'il soit) */
static const char *pay_service(const char *key) { return !strcmp(key, "ftp") ? "FTP" : !strcmp(key, "websrv") ? "WEB" : ""; }
static const PayInfo PAYINFO[] = {
  { "ftp", "Réseau", "Serveur FTP : les fichiers de la console depuis ton PC", 2121, 1 },
  { "klog", "Debug", "Journal du noyau en direct sur le réseau", 3232, 0 },
  { "shsrv", "Outils", "Terminal à distance (telnet)", 2323, 0 },
  { "websrv", "Outils", "Serveur web : homebrews dans le navigateur", 8080, 1 },
  { "gdbsrv", "Debug", "Débogage à distance avec GDB", 2159, 0 },
  { "kstuff", "Jailbreak", "Fonctions avancées du noyau (fself, fpkg)", 0, 0 },
  { "etahen", "Jailbreak", "Menu homebrew et outils de la scène", 0, 0 },
  { "byepervisor", "Jailbreak", "Désactive l'hyperviseur (firmwares compatibles)", 0, 0 },
  { "elfldr", "Outils", "Chargeur ELF : reçoit les payloads sur le port 9021", 0, 0 },
  { "launcher5", "Outils", "Launcher5 lui-même", 0, 0 },
};
#define NPAYINFO (int)(sizeof PAYINFO / sizeof PAYINFO[0])

static const PayInfo *pay_info(const App *a) {
  if (!a || a->kind != KIND_PAYLOAD) return NULL;
  const char *b = strrchr(a->path, '/');
  b = b ? b + 1 : a->path[0] ? a->path : a->id;
  char low[NAME_LEN];
  size_t k = 0;
  for (; b[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)b[k]);
  low[k] = 0;
  for (int i = 0; i < NPAYINFO; i++) if (strstr(low, PAYINFO[i].key)) return &PAYINFO[i];
  return NULL;
}

/* couleur et symbole d'un payload selon sa categorie (fiche du gestionnaire, sinon table) */
static void payload_style(const App *a, uint32_t *col, const char **glyph) {
  const PayInfo *pi = pay_info(a);
  char c[64];
  snprintf(c, sizeof c, "%s", a->cat[0] ? a->cat : pi ? pi->cat : "");
  for (char *p = c; *p; p++) *p = (char)tolower((unsigned char)*p);
  if (strstr(c, "jailbreak") || strstr(c, "system")) { *col = 0xd63a3a; *glyph = "⚡"; }
  else if (strstr(c, "network") || strstr(c, "server") || strstr(c, "réseau")) { *col = 0x2f8fe0; *glyph = "⇅"; }
  else if (strstr(c, "debug")) { *col = 0x9b59b6; *glyph = "◎"; }
  else if (strstr(c, "loader")) { *col = 0xe67e22; *glyph = "⇪"; }
  else if (c[0]) { *col = 0x16a085; *glyph = "⚙"; }
  else { *col = 0x5b6b82; *glyph = "⚙"; }
}

/* un service ecoute-t-il sur ce port de la console ? (connexion locale, 200 ms max) */
static int port_open(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return 0;
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_port = htons((uint16_t)port);
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int ok = 0;
  if (!connect(fd, (struct sockaddr *)&sa, sizeof sa)) ok = 1;
  else if (errno == EINPROGRESS) {
    fd_set w;
    FD_ZERO(&w);
    FD_SET(fd, &w);
    struct timeval tv = { 0, 200000 };
    if (select(fd + 1, NULL, &w, NULL, &tv) > 0) {
      int err = 0;
      socklen_t l = sizeof err;
      ok = !getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &l) && !err;
    }
  }
  close(fd);
  return ok;
}

static volatile int pay_up[NPAYINFO];            /* 1 : le port du payload repond */
static volatile Uint32 pay_want;                 /* derniere fois que l'ecran en avait besoin */
static int pay_thread_on;

static int pay_probe_thread(void *u) {           /* toutes les 2,5 s, seulement quand on regarde */
  (void)u;
  for (;;) {
    if (SDL_GetTicks() - pay_want < 6000)
      for (int i = 0; i < NPAYINFO; i++) if (PAYINFO[i].probe) pay_up[i] = port_open(PAYINFO[i].port);
    SDL_Delay(2500);
  }
  return 0;
}

/* Payloads actifs, affiches sous l'IP : noms des processus (kstuff, ShadowMount, FTP…) et
   port de l'interface web de Kraken (8097), verifies toutes les 4 s en arriere-plan. */
static char act_buf[2][120];
static SDL_atomic_t act_idx;

static int active_thread(void *u) {
  (void)u;
  static char procs[24576];
  static const struct { const char *prefix, *label; } KNOWN[] = {
    { "kstuff", "kstuff" }, { "etahen", "etaHEN" }, { "shadowmount", "ShadowMount" },
    { "ftpsrv", "FTP" }, { "zftpd", "FTP" }, { "websrv", "Web" },
  };
  for (;;) {
    int on[8] = { 0 }, kraken;
    if (!plat_procs(procs, sizeof procs))
      for (char *l = procs; *l;) {
        char *e = strchr(l, '\n');
        if (e) *e = 0;
        char low[24];
        size_t k = 0;
        for (; l[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)l[k]);
        low[k] = 0;
        for (size_t i = 0; i < sizeof KNOWN / sizeof KNOWN[0]; i++)
          if (!strncmp(low, KNOWN[i].prefix, strlen(KNOWN[i].prefix))) on[i] = 1;
        if (!e) break;
        l = e + 1;
      }
    kraken = port_open(8097);
    /* ordre : kstuff, etaHEN, ShadowMount, Kraken, FTP, Web */
    const char *show[8];
    int ns = 0;
    for (int i = 0; i < 3; i++) if (on[i]) show[ns++] = KNOWN[i].label;
    if (kraken) show[ns++] = "Kraken";
    if (on[3] || on[4]) show[ns++] = "FTP";
    if (on[5]) show[ns++] = "Web";
    char line[120] = "";
    for (int i = 0; i < ns; i++) {
      if (i) strcat(line, "  ·  ");
      strcat(line, show[i]);
    }
    int w = !SDL_AtomicGet(&act_idx);
    if (line[0]) snprintf(act_buf[w], sizeof act_buf[w], "●  %s", line);
    else act_buf[w][0] = 0;
    SDL_AtomicSet(&act_idx, w);
    SDL_Delay(4000);
  }
  return 0;
}

static const char *active_line(void) {
  static int started;
  if (!started) {
    started = 1;
    SDL_Thread *t = SDL_CreateThread(active_thread, "l5active", NULL);
    if (t) SDL_DetachThread(t);
  }
  return act_buf[SDL_AtomicGet(&act_idx)];
}

static const char *pay_label(const App *a) { const PayInfo *pi = pay_info(a); return pi ? pay_service(pi->key) : ""; }

static int pay_active(const App *a) {
  const PayInfo *pi = pay_info(a);
  if (!pi || !pi->probe) return 0;
  pay_want = SDL_GetTicks();
  if (!pay_thread_on) {
    pay_thread_on = 1;
    SDL_Thread *t = SDL_CreateThread(pay_probe_thread, "l5ports", NULL);
    if (t) SDL_DetachThread(t);
  }
  return pay_up[pi - PAYINFO];
}

/* ligne d'informations sous le titre */
static void meta_line(Entry *e, char *meta, size_t n) {
  if (e->is_folder) {
    snprintf(meta, n, tr(e->node->count > 1 ? "%d éléments" : "%d élément"), e->node->count);
  } else if (e->app->kind == KIND_SYSTEM) {
    static const char *DESC[][2] = {
      { "cfg:theme", "Couleurs, fond d'écran, particules, musique : crée ton propre thème" },
      { "cfg:view", "Cover Flow, Grille ou Carrousel  ·  aussi avec le pavé tactile" },
      { "cfg:return", "Revenir au launcher tout seul quand tu fermes un jeu" },
      { "cfg:folders", "Affiche la section où tu ranges tes jeux dans tes dossiers" },
      { "cfg:hidden", "Montre ou cache les jeux que tu as masqués" },
      { "cfg:saver", "Diaporama de tes fonds d'écran au repos" },
      { "sys:home", "Ferme le launcher et revient à l'accueil de la PS5" },
    };
    snprintf(meta, n, "Réglage");
    for (size_t i = 0; i < sizeof DESC / sizeof DESC[0]; i++)
      if (!strcmp(DESC[i][0], e->app->id)) snprintf(meta, n, "%s", DESC[i][1]);
  } else if (e->app->kind == KIND_PAYLOAD) {
    const PayInfo *pi = pay_info(e->app);
    Stat *st = menu_stat(&S.menu, e->app->id, 0);
    char when[48] = "";
    if (st && st->last) { char w[40]; ago(st->last, w, sizeof w); snprintf(when, sizeof when, tr("  ·  envoyé %s"), w); }
    const char *cat = e->app->cat[0] ? e->app->cat : pi ? tr(pi->cat) : tr("Payload ELF");
    const char *desc = e->app->desc[0] ? e->app->desc : pi ? tr(pi->desc) : "";
    char ver[32] = "";
    if (e->app->ver[0]) snprintf(ver, sizeof ver, "%s  ·  ", e->app->ver);
    if (pay_incompatible(e->app))
      snprintf(meta, n, "⊘ %s", tr("Incompatible avec Launcher5 : demande le mode repos"));
    else if (payload_sensitive(e->app) && boot_index(e->app) < 0)    /* avertissement permanent */
      snprintf(meta, n, "⚠ %s", tr("Payload noyau : à lancer au démarrage (△ › Envoyer au démarrage)"));
    else if (desc[0]) snprintf(meta, n, "%s%s  ·  %s%s", ver, cat, desc, when);
    else snprintf(meta, n, "%s%s%s", ver, cat, when[0] ? when : tr("  ·  jamais envoyé"));
  } else {
    Stat *st = menu_stat(&S.menu, e->app->id, 0);
    if (st && st->playtime > 0) {
      char when[40], pt[48];
      ago(st->last, when, sizeof when);
      playtime_str(st->playtime, pt, sizeof pt);
      if (st->last) snprintf(meta, n, tr("%s  ·  Dernière session %s"), pt, when);
      else snprintf(meta, n, "%s", pt);
    } else if (st && st->launches) {
      char when[40];
      ago(st->last, when, sizeof when);
      snprintf(meta, n, tr("Lancé %d fois  ·  Dernière session %s"), st->launches, when);
    } else snprintf(meta, n, "Jamais lancé depuis Launcher5");
  }
}

static void draw_hero(Entry *e) {
  S.hero_anim += (1 - S.hero_anim) * ease(0.14f);
  int a = (int)(255 * S.hero_anim), dy = (int)(24 * (1 - S.hero_anim));
  int x = CAR_X, y = 210 + dy;

  if (!e) {
    const char *msg = S.sec == SEC_HOME && S.depth ? "Ce dossier est vide" : "Rien à afficher ici";
    draw_text_a(S.f84, msg, x, y + 60, 0xffffff, 0, 1300, a);
    draw_text_a(S.f30, search_q[0] ? "Aucun résultat  ·  ○ pour effacer la recherche" : S.sec == SEC_HOME ? "Range des jeux dedans avec △ › Ranger dans un dossier." : "Lance des jeux pour remplir cette section.",
                x, y + 180, 0x9aa4b5, 0, 0, a);
    return;
  }

  /* pastille du type */
  const char *kind = e->is_folder ? "DOSSIER" : KIND_LABEL[e->app->kind];
  uint32_t kc = e->is_folder ? e->node->color : S.th->accent;
  int kw = text_w(S.f22, kind);
  fill_round(x, y, kw + 30, 38, 19, kc, (int)(0.9f * a));
  draw_text_a(S.f22, kind, x + 15, y + 6, 0x0a0a12, 0, 0, a);
  char fb[48];
  const char *ft = fresh_tag(e, fb, sizeof fb);
  if (ft) { fresh_pill(ft, x + kw + 42, y, 38, a); kw += fresh_pill_w(ft) + 12; }
  if (S.sec == SEC_HOME && S.depth) {
    char path[300] = "";
    Node *n = &S.menu.root;
    for (int i = 0; i < S.depth; i++) {
      n = &n->items[S.path[i]];
      strncat(path, i ? " › " : "", sizeof path - strlen(path) - 1);
      strncat(path, n->name, sizeof path - strlen(path) - 1);
    }
    draw_text_a(S.f26, path, x + kw + 50, y + 4, 0xc9d1dc, 0, 900, a);
  }

  /* titre */
  const char *title = e->is_folder ? e->node->name : menu_display_name(&S.menu, e->app);
  int maxw = S.mode == MODE_THEME ? 900 : 1500;   /* l'editeur de theme occupe la droite */
  TTF_Font *tf = text_w(S.f84, title) > maxw ? S.f48 : S.f84;
  draw_text_a(tf, title, x - 4, y + 52, 0xffffff, 0, maxw, a);

  /* infos */
  char meta[200];
  meta_line(e, meta, sizeof meta);
  draw_text_a(S.f30, meta, x, y + 170, 0xc9d1dc, 0, maxw, a);

  /* boutons */
  int by = y + 240;
  float pulse = 0.5f + 0.5f * sinf(S.frame * 0.08f);
  const char *main_btn = e->is_folder || e->app->kind == KIND_SYSTEM ? "✕  OUVRIR"
                       : e->app->kind == KIND_PAYLOAD ? "✕  ENVOYER"
                       : e->app->kind == KIND_MEDIA || (e->app->flags & APPF_MEDIA) ? "✕  REGARDER" : "✕  JOUER";
  int bw = text_w(S.f30, main_btn) + 56;
  if (S.lite) {
    SDL_SetTextureColorMod(S.btn_glow, (S.th->accent >> 16) & 255, (S.th->accent >> 8) & 255, S.th->accent & 255);
    SDL_SetTextureAlphaMod(S.btn_glow, (Uint8)(ga((int)(70 + 50 * pulse)) * S.hero_anim));
    SDL_Rect gd = { x + bw / 2 - 180, by + 30 - 180, 360, 360 };
    SDL_RenderCopy(S.r, S.btn_glow, NULL, &gd);
  } else
    glow(x + bw / 2, by + 30, (int)(bw * (0.75f + 0.12f * pulse)), S.th->accent, (int)(ga(110) * S.hero_anim));
  pill(x, by, main_btn, S.th->accent, 0x0a0a12, 1, S.f30);
  pill(x + bw + 24, by, "△  OPTIONS", 0xffffff, 0xffffff, 0, S.f30);
}

static int show_badges(void) { return S.sec == SEC_HOME || S.sec == SEC_RECENT || S.sec == SEC_MEDIA || S.sec == SEC_EMU; }

static void fav_badge(int x, int y, int w) {
  int s = w >= 300 ? 44 : 36;
  fill_round(x + w - s - 10, y + 10, s, s, s / 2, 0x000000, 180);
  draw_text(s >= 44 ? S.f30 : S.f22, "★", x + w - 10 - s / 2, y + (s >= 44 ? 13 : 13), S.th->accent, 1, 0);
}

static void draw_app_tile(App *a, int x, int y, int w, int h) {
  int ts = w <= TILE ? TILE : TILE_SEL;
  SDL_Texture *t = tile_tex(a, ts, TH(ts));
  if (t) {
    SDL_Rect d = { x, y, w, h };
    SDL_RenderCopy(S.r, t, NULL, &d);
  }
  if (show_badges() && a->kind != KIND_PS5 && !BOXES) {
    const char *b = KIND_LABEL[a->kind];
    int bw = text_w(S.f22, b);
    fill_round(x + 10, y + 10, bw + 18, 32, 9, 0x000000, 175);
    draw_text(S.f22, b, x + 19, y + 13, 0xffffff, 0, 0);
  }
  if (menu_is_fav(&S.menu, a->id)) fav_badge(x, y, w);
  if (menu_is_hidden(&S.menu, a->id)) {
    fill_round(x, y, w, h, tile_rad() * w / TILE_SEL, 0x000000, 160);
    draw_text(S.f26, "masqué", x + w / 2, y + h / 2 - 16, 0xcccccc, 1, 0);
  }
}

static void draw_folder_tile(Node *f, int x, int y, int w, int h) {
  uint32_t c = f->color;
  int rad = tile_rad() * w / TILE_SEL;
  fill_round(x, y, w, h, rad, shade(c, 0.35f), 255);
  if (!S.lite) glow(x + w / 2, y + h / 2, w * 3 / 4, c, ga(140));
  fill_round(x + 14, y + 14, w / 2 - 20, 22, 10, shade(c, 1.1f), 255);   /* onglet */
  int mw = (w - 3 * 12) / 2, mh = mw * 3 / 4, shown = 0;
  for (int i = 0; i < f->count && shown < 4; i++) {
    Node *n = &f->items[i];
    int mx = x + 12 + (shown % 2) * (mw + 12), my = y + 46 + (shown / 2) * (mh + 10);
    if (my + mh > y + h - 60) break;
    if (n->is_folder) fill_round(mx, my, mw, mh, 10, n->color, 255);
    else {
      App *a = menu_find_app(&S.menu, n->ref);
      if (!a) continue;
      SDL_Texture *t = tile_tex(a, TILE, TH(TILE));
      if (t) {
        int sh = TILE * mh / mw;
        SDL_Rect sr = { 0, (TH(TILE) - sh) / 2, TILE, sh }, d = { mx, my, mw, mh };
        SDL_RenderCopy(S.r, t, &sr, &d);
      }
    }
    shown++;
  }
  fill_round(x, y + h - 58, w, 58, rad < 20 ? rad : 20, 0x000000, 120);
  draw_text(S.f26, f->name, x + 14, y + h - 46, 0xffffff, 0, w - 74);
  char n[16];
  snprintf(n, sizeof n, "%d", f->count);
  int nw = text_w(S.f22, n);
  fill_round(x + w - nw - 32, y + h - 48, nw + 20, 36, 18, c, 255);
  draw_text(S.f22, n, x + w - nw - 22, y + h - 43, 0x0a0a12, 0, 0);
}

static void draw_carousel(void) {
  int sel = *cur_sel();
  float target = (float)sel;
  S.scroll += (target - S.scroll) * ease(0.22f);
  if (fabsf(target - S.scroll) < 0.002f) S.scroll = target;
  S.sel_anim += (1 - S.sel_anim) * ease(0.25f);
  if (S.lite) S.sel_anim = 1;

  for (int i = 0; i < S.nentries; i++) {
    float rel = i - S.scroll;
    int is_sel = i == sel && S.mode != MODE_KEYBOARD;
    /* l'element selectionne est plus grand : les suivants sont decales d'autant */
    float push = rel < 0 ? 0 : rel > 1 ? 1 : rel;
    float x = CAR_X + rel * (TILE + GAP) + push * (TILE_SEL - TILE);
    if (x > W + 20 || x < -TILE_SEL - 40) continue;
    int w = is_sel ? (int)(TILE + (TILE_SEL - TILE) * S.sel_anim) : TILE;
    int h = TH(w);
    int ix = (int)x, iy = CAR_BOT - h;
    int fade = rel < -0.5f ? (int)(255 * (1 + (rel + 0.5f) * 0.9f)) : 255;
    if (fade < 30) continue;
    Entry *e = &S.entries[i];
    int moving = S.mode == MODE_MOVE && is_sel;
    if (moving) iy += (int)(8 * sinf(S.frame * 0.25f));

    if (is_sel) {
      float pulse = 0.5f + 0.5f * sinf(S.frame * 0.08f);
      uint32_t gc = moving ? 0xffc53d : S.th->accent;
      if (S.lite) {
        SDL_SetTextureColorMod(S.tile_glow, (gc >> 16) & 255, (gc >> 8) & 255, gc & 255);
        SDL_SetTextureAlphaMod(S.tile_glow, (Uint8)ga((int)(110 + 40 * pulse)));
        int gs = TILE_SEL * 8 / 5;
        SDL_Rect gd = { ix + w / 2 - gs / 2, iy + h / 2 - gs / 2, gs, gs };
        SDL_RenderCopy(S.r, S.tile_glow, NULL, &gd);
      } else
        glow(ix + w / 2, iy + h / 2, (int)(h * (0.8f + 0.08f * pulse)), gc, ga(150));
      fill_round(ix + 10, iy + 18, w, h, tile_rad() * w / TILE_SEL, 0x000000, 120);
    }
    if (e->is_folder) draw_folder_tile(e->node, ix, iy, w, h);
    else draw_app_tile(e->app, ix, iy, w, h);
    if (fade < 255) fill_round(ix, iy, w, h, tile_rad() * w / TILE_SEL, S.th->top, 255 - fade);
    if (is_sel) outline_round(ix, iy, w, h, tile_rad() * w / TILE_SEL, 4, moving ? 0xffc53d : 0xffffff, 255);
  }

  if (S.nentries) {
    char pos[32];
    snprintf(pos, sizeof pos, "%d / %d", sel + 1, S.nentries);
    draw_text(S.f22, pos, W - CAR_X, CAR_Y - 40, 0x9aa4b5, 2, 0);
  }
}

/* ------------------------------------------------------------------ */
/* Cover flow : jaquettes en perspective avec reflet                    */
/* ------------------------------------------------------------------ */
/* Texture dessinee en trapeze : bord gauche de hauteur hl, bord droit hr,
   centres sur cy. Decoupee en bandes verticales pour limiter la
   deformation. refl : 0 image seule, 1 image et reflet, 2 reflet seul. */
static void draw_persp(SDL_Texture *t, float x0, float x1, float cy, float hl, float hr, int bright, int alpha, int refl) {
  enum { N = 8 };
  SDL_Vertex v[(N + 1) * 2];
  int idx[N * 6];
  for (int i = 0; i < N; i++) {
    int *q = idx + i * 6;
    q[0] = 2 * i; q[1] = 2 * i + 1; q[2] = 2 * i + 2; q[3] = 2 * i + 1; q[4] = 2 * i + 3; q[5] = 2 * i + 2;
  }
  Uint8 c = (Uint8)bright;
  for (int pass = refl == 2 ? 1 : 0; pass < (refl ? 2 : 1); pass++) {
    for (int i = 0; i <= N; i++) {
      float k = (float)i / N, x = x0 + (x1 - x0) * k, h = hl + (hr - hl) * k;
      float top = cy - h / 2, bot = cy + h / 2;
      SDL_Vertex *a = &v[2 * i], *b = &v[2 * i + 1];
      a->position.x = b->position.x = x;
      a->color.r = a->color.g = a->color.b = b->color.r = b->color.g = b->color.b = c;
      a->tex_coord.x = b->tex_coord.x = k;
      if (!pass) {
        a->position.y = top; b->position.y = bot;
        a->color.a = b->color.a = (Uint8)alpha;
        a->tex_coord.y = 0; b->tex_coord.y = 1;
      } else {                                  /* reflet : image retournee */
        float rh = h * 0.35f;
        a->position.y = bot + 6; b->position.y = bot + 6 + rh;
        a->color.a = (Uint8)(alpha * 80 / 255); b->color.a = 0;
        a->tex_coord.y = 1; b->tex_coord.y = 0.65f;
      }
    }
    SDL_RenderGeometry(S.r, t, v, (N + 1) * 2, idx, N * 6);
  }
}

/* Les dossiers sont dessines une fois dans une texture (cache par contenu) */
static struct { uint32_t key; SDL_Texture *t; Uint32 used; } FCACHE[12];

static uint32_t folder_key(Node *f) {
  uint32_t h = 2166136261u;
#define MIX(x) h = (h ^ (uint32_t)(x)) * 16777619u
  for (const char *s = f->name; *s; s++) MIX((unsigned char)*s);
  MIX(f->color); MIX(f->count); MIX(S.th_ver); MIX(S.th->corners); MIX(BOXES);
  for (int i = 0; i < f->count && i < 4; i++) {
    const char *r = f->items[i].is_folder ? f->items[i].name : f->items[i].ref;
    for (; *r; r++) MIX((unsigned char)*r);
    MIX(f->items[i].color);
  }
#undef MIX
  return h ? h : 1;
}

static SDL_Texture *folder_tex(Node *f) {
  uint32_t key = folder_key(f);
  int slot = 0;
  for (int i = 0; i < 12; i++) {
    if (FCACHE[i].t && FCACHE[i].key == key) { FCACHE[i].used = S.frame; return FCACHE[i].t; }
    if (!FCACHE[i].t || (FCACHE[slot].t && FCACHE[i].used < FCACHE[slot].used)) slot = i;
  }
  if (!FCACHE[slot].t) {
    FCACHE[slot].t = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, FLOW, TH(FLOW));
    if (!FCACHE[slot].t) return NULL;
    SDL_SetTextureBlendMode(FCACHE[slot].t, SDL_BLENDMODE_BLEND);
  }
  SDL_Texture *prev_rt = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, FCACHE[slot].t);
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
  SDL_RenderClear(S.r);
  draw_folder_tile(f, 0, 0, FLOW, TH(FLOW));
  mask_corners(FLOW, TH(FLOW), tile_rad() * FLOW / TILE_SEL);
  SDL_SetRenderTarget(S.r, prev_rt);   /* cible d'avant (dessin imbrique) */
  FCACHE[slot].key = key;
  FCACHE[slot].used = S.frame;
  return FCACHE[slot].t;
}

/* Jaquettes laterales (deja deformees, avec leur reflet) et reflet de la
   jaquette centrale : calcules une fois puis simplement recopies. Sans carte
   graphique (PS5), deformer des images a chaque image coute trop cher. */
#define SIDE_W (FLOW_SIDE * 55 / 100)
#define SPINE (BOXES ? FLOW_SIDE * 10 / 100 : 0)    /* epaisseur visible du boitier (tranche) */
#define SIDE_H (TH(FLOW_SIDE) + 6 + TH(FLOW_SIDE) * 35 / 100 + 1)
#define REFL_H (6 + TH(FLOW) * 35 / 100 + 1)
enum { WARP_LEFT, WARP_RIGHT, WARP_REFL };
static struct { SDL_Texture *src, *t; uint32_t ver; int kind; Uint32 used; } WCACHE[40];

/* couleurs de la tranche d'un boitier (0 : pas de tranche) */
static int case_spine(App *a, uint32_t *body, uint32_t *band) {
  int cs = S.base->case_style;
  if (!BOXES || !a || cs == CASE_NONE) return 0;
  if (a->kind == KIND_HOMEBREW) *body = 0xc81e24;
  else switch (cs) {
  case CASE_BLACK: *body = 0x15171c; break;
  case CASE_WHITE: *body = 0xe4e7eb; break;
  case CASE_CRYSTAL: *body = 0xbfcad8; break;
  case CASE_STEEL: *body = 0x8c959f; break;
  case CASE_THEME: *body = shade(S.base->accent, 0.85f); break;
  case CASE_PHOTO: { int k = case_img(a); *body = k >= 0 ? CASEIMG[k].edge : 0x1a56d6; break; }
  default: *body = 0x1a56d6; break;
  }
  if (cs == CASE_STEEL) *band = *body;
  else *band = a->kind == KIND_PS5 ? 0xf4f4f6 : a->kind == KIND_PS4 ? 0x0a3a9c
             : a->kind == KIND_HOMEBREW ? 0xa3151a : shade(S.base->accent, 0.8f);
  return 1;
}

/* une face plate en perspective : quatre coins, une couleur par bord (ombre) */
static void quad4(float x0, float y0t, float y0b, float x1, float y1t, float y1b, uint32_t c0, uint32_t c1, int a0, int a1) {
  SDL_Vertex v[4];
  float xs[4] = { x0, x1, x1, x0 }, ys[4] = { y0t, y1t, y1b, y0b };
  uint32_t cs[4] = { c0, c1, c1, c0 };
  int as[4] = { a0, a1, a1, a0 };
  for (int i = 0; i < 4; i++) {
    v[i].position.x = xs[i]; v[i].position.y = ys[i];
    v[i].color.r = (cs[i] >> 16) & 255; v[i].color.g = (cs[i] >> 8) & 255; v[i].color.b = cs[i] & 255; v[i].color.a = (Uint8)as[i];
    v[i].tex_coord.x = v[i].tex_coord.y = 0;
  }
  static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
  SDL_RenderGeometry(S.r, NULL, v, 4, idx, 6);
}

/* tranche du boitier collee au bord proche (xn), qui fuit vers xb ; avec son reflet */
static void draw_spine(float xn, float xb, float h, uint32_t body, uint32_t band) {
  float t0 = h * 0.03f, b0 = h * 0.97f;           /* le bord du fond parait plus court */
  quad4(xn, 0, h, xb, t0, b0, shade(body, 0.78f), shade(body, 0.45f), 255, 255);
  float bh = h * 0.09f;                            /* haut de la tranche : couleur du bandeau */
  quad4(xn, 0, bh, xb, t0, t0 + bh * 0.94f, shade(band, 0.85f), shade(band, 0.55f), 255, 255);
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
  quad4(xn, 0, h, xn + (xb > xn ? 2.0f : -2.0f), 0, h, 0xffffff, 0xffffff, 90, 0);   /* arete eclairee */
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_NONE);
  float ry = h + 6, rh = h * 0.35f;                /* reflet au sol */
  quad4(xn, ry, ry + rh, xb, ry + t0, ry + rh * 0.95f, shade(body, 0.6f), shade(body, 0.35f), 70, 55);
}

static void warp_forget(SDL_Texture *src) {
  for (int i = 0; i < 40; i++)
    if (WCACHE[i].t && (!src || WCACHE[i].src == src)) {
      if (src) SDL_DestroyTexture(WCACHE[i].t);
      WCACHE[i].t = NULL; WCACHE[i].src = NULL;
    }
}

static SDL_Texture *warp_tex(SDL_Texture *src, uint32_t ver, int kind, App *app) {
  uint32_t body = 0, band = 0;
  int spine = kind != WARP_REFL && case_spine(app, &body, &band);
  ver = ver * 31 + (uint32_t)S.th_ver;
  ver = (ver * 31 + body) * 31 + band;
  int slot = 0;
  for (int i = 0; i < 40; i++) {
    if (WCACHE[i].t && WCACHE[i].src == src && WCACHE[i].ver == ver && WCACHE[i].kind == kind) { WCACHE[i].used = S.frame; return WCACHE[i].t; }
    if (!WCACHE[i].t || (WCACHE[slot].t && WCACHE[i].used < WCACHE[slot].used)) slot = i;
  }
  int w = kind == WARP_REFL ? FLOW : SIDE_W + SPINE, h = kind == WARP_REFL ? REFL_H : SIDE_H;
  if (WCACHE[slot].t) SDL_DestroyTexture(WCACHE[slot].t);
  WCACHE[slot].t = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
  if (!WCACHE[slot].t) return NULL;
  SDL_SetTextureBlendMode(WCACHE[slot].t, SDL_BLENDMODE_BLEND);
  SDL_Texture *prev_rt = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, WCACHE[slot].t);
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
  SDL_RenderClear(S.r);
  /* copie brute (sans melange) : la transparence est gardee telle quelle */
  SDL_SetTextureBlendMode(src, SDL_BLENDMODE_NONE);
  if (kind == WARP_REFL) draw_persp(src, 0, FLOW, -6.0f - TH(FLOW) / 2.0f, TH(FLOW), TH(FLOW), 255, 255, 2);
  else {
    float near = TH(FLOW_SIDE), far = near * 0.8f, fx = kind == WARP_LEFT ? 0 : SPINE;   /* face avant */
    draw_persp(src, fx, fx + SIDE_W, near / 2.0f, kind == WARP_LEFT ? far : near, kind == WARP_LEFT ? near : far, 255, 255, 1);
    if (spine) {                                 /* la tranche, cote centre de l'ecran */
      SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_NONE);
      if (kind == WARP_LEFT) draw_spine(SIDE_W, SIDE_W + SPINE, near, body, band);
      else draw_spine(SPINE, 0, near, body, band);
      SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
    }
  }
  SDL_SetTextureBlendMode(src, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(S.r, prev_rt);   /* cible d'avant (dessin imbrique) */
  WCACHE[slot].src = src; WCACHE[slot].ver = ver; WCACHE[slot].kind = kind; WCACHE[slot].used = S.frame;
  return WCACHE[slot].t;
}

typedef struct { int i; float rel; } FlowItem;
static int cmp_flow(const void *a, const void *b) {
  float x = fabsf(((const FlowItem *)a)->rel), y = fabsf(((const FlowItem *)b)->rel);
  return x < y ? 1 : x > y ? -1 : 0;
}

/* --- Petits effets : apparition en cascade, ombre, zoom doux, bordure animee --- */
static float casc(int i) {                       /* progression (0..1) du i-eme element apres un changement de jeu */
  float p = ((float)(SDL_GetTicks() - S.sel_since) - i * 70.0f) / 230.0f;
  if (p <= 0) return 0;
  if (p >= 1) return 1;
  return 1 - (1 - p) * (1 - p);
}

static void drop_shadow(int cx, int base, int w, int h, int alpha) {   /* ombre douce au pied du boitier */
  if (!S.glow || alpha <= 0) return;
  SDL_SetTextureColorMod(S.glow, 0, 0, 0);
  SDL_SetTextureBlendMode(S.glow, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(S.glow, (Uint8)(alpha > 255 ? 255 : alpha));
  SDL_Rect d = { cx - w * 5 / 8, base - h / 12, w * 5 / 4, h / 6 };
  SDL_RenderCopy(S.r, S.glow, NULL, &d);
  SDL_SetTextureBlendMode(S.glow, SDL_BLENDMODE_ADD);
  SDL_SetTextureColorMod(S.glow, 255, 255, 255);
  SDL_SetTextureAlphaMod(S.glow, 255);
}

/* point lumineux qui fait le tour du boitier choisi, avec une petite trainee */
static void border_run(int x, int y, int w, int h, uint32_t c) {
  float per = 2.0f * (w + h), p = fmodf(SDL_GetTicks() * 0.45f, per);
  for (int k = 0; k < 14; k++) {
    float q = fmodf(p - k * 7 + per, per), px, py;
    if (q < w) { px = x + q; py = y; }
    else if (q < w + h) { px = x + w; py = y + (q - w); }
    else if (q < 2 * w + h) { px = x + w - (q - w - h); py = y + h; }
    else { px = x; py = y + h - (q - 2 * w - h); }
    int a = (int)(230 * (1 - k / 14.0f)), r = k < 3 ? 5 : 4;
    fill_round((int)px - r, (int)py - r, r * 2, r * 2, r, k < 2 ? 0xffffff : c, a);
  }
}

static void draw_flow(void) {
  int sel = *cur_sel();
  S.scroll += ((float)sel - S.scroll) * ease(0.18f);
  if (fabsf(sel - S.scroll) < 0.002f) S.scroll = (float)sel;
  float target_cx = S.mode == MODE_THEME ? (W - 860) / 2.0f : W / 2.0f;
  if (S.flow_cx == 0) S.flow_cx = target_cx;
  S.flow_cx += (target_cx - S.flow_cx) * ease(0.15f);

  /* sur PS5 (dessin sans carte graphique) on dessine moins de jaquettes */
  float maxrel = S.lite ? 3.6f : 6.5f, reflrel = S.lite ? 1.5f : 7.0f;
  FlowItem items[32]; int n = 0;
  for (int i = (int)S.scroll - 7; i <= (int)S.scroll + 8 && n < 32; i++) {
    if (i < 0 || i >= S.nentries) continue;
    float rel = i - S.scroll;
    if (fabsf(rel) > maxrel) continue;
    items[n].i = i; items[n].rel = rel; n++;
  }
  qsort(items, n, sizeof items[0], cmp_flow);   /* du plus loin au plus proche */

  for (int k = 0; k < n; k++) {
    int i = items[k].i;
    float rel = items[k].rel, a = fabsf(rel), s = rel < 0 ? -1.0f : 1.0f, t = a < 1 ? a : 1;
    float off = a <= 1 ? rel * 330 : s * (330 + (a - 1) * 165);
    float size = FLOW + (FLOW_SIDE - FLOW) * t, hsz = TH(FLOW) + (TH(FLOW_SIDE) - TH(FLOW)) * t;
    float wf = 1 - 0.45f * t, ratio = 1 - 0.2f * t;
    float cx = S.flow_cx + off, w = size * wf;
    if (cx + w / 2 < -20 || cx - w / 2 > W + 20) continue;
    if (S.mode == MODE_THEME && cx - w / 2 > W - ED_W - 30) continue;   /* cachee par le studio de theme */
    float near = hsz, far = hsz * ratio;
    float hl = rel < 0 ? far : near, hr = rel < 0 ? near : far;
    int bright = (int)(255 - 95 * t - (a > 1 ? (a - 1) * 18 : 0));
    if (bright < 90) bright = 90;
    int alpha = a > maxrel - 1 ? (int)(255 * (maxrel - a)) : 255;
    if (alpha <= 4) continue;
    Entry *e = &S.entries[i];
    int center = i == sel && a < 0.5f;
    int moving = S.mode == MODE_MOVE && i == sel;

    if (center) {
      float pulse = 0.5f + 0.5f * sinf(S.frame * 0.08f);
      uint32_t gc = moving ? 0xffc53d : S.th->accent;
      if (S.lite) {
        SDL_SetTextureColorMod(S.tile_glow, (gc >> 16) & 255, (gc >> 8) & 255, gc & 255);
        SDL_SetTextureAlphaMod(S.tile_glow, (Uint8)ga((int)(110 + 40 * pulse)));
        int gs = TILE_SEL * 8 / 5;
        SDL_Rect gd = { (int)cx - gs / 2, FLOW_CY - gs / 2, gs, gs };
        SDL_RenderCopy(S.r, S.tile_glow, NULL, &gd);
      } else glow((int)cx, FLOW_CY, (int)(size * (0.85f + 0.08f * pulse)), gc, ga(150));
    }
    SDL_Texture *tex = e->is_folder ? folder_tex(e->node) : tile_tex(e->app, FLOW, TH(FLOW));
    uint32_t ver = e->is_folder ? folder_key(e->node) : 0;
    float cy = FLOW_CY + (moving ? 8 * sinf(S.frame * 0.25f) : 0);
    drop_shadow((int)cx, (int)(cy + (hl > hr ? hl : hr) / 2), (int)w, (int)hsz, (int)((center ? 120 : 80) * alpha / 255));
    /* zoom doux : le boitier choisi grossit un instant quand il se pose, puis revient */
    static int pop_key = -1;
    static Uint32 pop_t;
    float zoom = 1;
    if (center && a < 0.001f) {
      int pk = S.sec * 100000 + S.depth * 10000 + i;
      if (pk != pop_key) { pop_key = pk; pop_t = SDL_GetTicks(); }
      float zt = (SDL_GetTicks() - pop_t) / 320.0f;
      if (zt < 1) zoom = 1 + 0.045f * sinf(zt * 3.14159f);
    }
    SDL_Texture *cached = NULL;
    if (tex && a >= 0.999f) {                     /* sur le cote : texture deja deformee */
      cached = warp_tex(tex, ver, rel < 0 ? WARP_LEFT : WARP_RIGHT, e->is_folder ? NULL : e->app);
      if (cached) {
        SDL_SetTextureColorMod(cached, bright, bright, bright);
        SDL_SetTextureAlphaMod(cached, alpha);
        int tw; SDL_QueryTexture(cached, NULL, NULL, &tw, NULL);
        SDL_Rect d = { (int)(cx - SIDE_W / 2.0f) - (rel < 0 ? 0 : tw - SIDE_W), (int)(cy - TH(FLOW_SIDE) / 2.0f), tw, SIDE_H };
        SDL_RenderCopy(S.r, cached, NULL, &d);
      }
    } else if (tex && a < 0.001f) {               /* au centre, immobile : image droite + reflet en cache */
      int zw = (int)(FLOW * zoom), zh = (int)(TH(FLOW) * zoom);
      SDL_Rect d = { (int)(cx - zw / 2.0f), (int)(cy - zh / 2.0f), zw, zh };
      SDL_RenderCopy(S.r, tex, NULL, &d);
      cached = warp_tex(tex, ver, WARP_REFL, NULL);
      if (cached) { SDL_Rect r = { d.x, d.y + zh, zw, (int)(REFL_H * zoom) }; SDL_RenderCopy(S.r, cached, NULL, &r); }
    }
    if (cached) {}
    else if (tex && S.lite) {
      /* PS5 : pendant le mouvement (une fraction de seconde), jaquette simplement
         etiree plutot que redessinee en perspective a chaque image */
      SDL_SetTextureColorMod(tex, bright, bright, bright);
      SDL_SetTextureAlphaMod(tex, alpha);
      SDL_Rect d = { (int)(cx - w / 2), (int)(cy - (hl > hr ? hl : hr) / 2), (int)w, (int)(hl > hr ? hl : hr) };
      SDL_RenderCopy(S.r, tex, NULL, &d);
      SDL_SetTextureColorMod(tex, 255, 255, 255);
      SDL_SetTextureAlphaMod(tex, 255);
    }
    else if (tex) draw_persp(tex, cx - w / 2, cx + w / 2, cy, hl, hr, bright, alpha, a < reflrel);
    else if (e->is_folder) draw_folder_tile(e->node, (int)(cx - w / 2), (int)(cy - hsz / 2), (int)w, (int)hsz);

    if (center && a < 0.05f) {
      int sz = (int)(size * zoom), szh = (int)(hsz * zoom), x = (int)(cx - sz / 2.0f), y = (int)(cy - szh / 2.0f), rad = tile_rad() * sz / TILE_SEL;
      if (!e->is_folder && menu_is_hidden(&S.menu, e->app->id)) {
        fill_round(x, y, sz, szh, rad, 0x000000, 160);
        draw_text(S.f26, "masqué", x + sz / 2, y + szh / 2 - 16, 0xcccccc, 1, 0);
      }
      if (!e->is_folder && show_badges() && e->app->kind != KIND_PS5 && !BOXES) {
        const char *b = KIND_LABEL[e->app->kind];
        int bw = text_w(S.f22, b);
        fill_round(x + 12, y + 12, bw + 18, 32, 9, 0x000000, 175);
        draw_text(S.f22, b, x + 21, y + 15, 0xffffff, 0, 0);
      }
      if (!e->is_folder && menu_is_fav(&S.menu, e->app->id)) fav_badge(x, y, sz);
      outline_round(x, y, sz, szh, rad, 4, moving ? 0xffc53d : 0xffffff, 255);
      if (!moving) border_run(x, y, sz, szh, S.th->accent);
    }
  }
}

/* Titre et infos sous le cover flow */
static void draw_flow_info(Entry *e) {
  S.hero_anim += (1 - S.hero_anim) * ease(0.14f);
  int a = (int)(255 * S.hero_anim), cx = (int)S.flow_cx, y = 790 + (int)(16 * (1 - S.hero_anim));
  int maxw = S.mode == MODE_THEME ? 980 : 1500;
  if (!e) {
    draw_text_a(S.f48, S.sec == SEC_HOME && S.depth ? "Ce dossier est vide" : "Rien à afficher ici", cx, FLOW_CY - 40, 0xffffff, 1, maxw, a);
    draw_text_a(S.f26, search_q[0] ? "Aucun résultat  ·  ○ pour effacer la recherche" : S.sec == SEC_HOME ? "Range des jeux dedans avec △ › Ranger dans un dossier." : "Lance des jeux pour remplir cette section.",
                cx, FLOW_CY + 40, 0x9aa4b5, 1, maxw, a);
    return;
  }
  const char *kind = e->is_folder ? "DOSSIER" : KIND_LABEL[e->app->kind];
  uint32_t kc = e->is_folder ? e->node->color : S.th->accent;
  int kw = text_w(S.f22, kind);
  char fb[48];
  const char *ft = fresh_tag(e, fb, sizeof fb);
  int fw = ft ? fresh_pill_w(ft) + 12 : 0, kx = cx - (kw + 30 + fw) / 2;
  float c0 = casc(0), c1 = casc(1), c2 = casc(2);   /* apparition en cascade */
  int a0 = (int)(255 * c0), a1 = (int)(255 * c1), a2 = (int)(255 * c2);
  y = 790;
  int y0 = y + (int)(14 * (1 - c0)), y1 = y + 46 + (int)(14 * (1 - c1)), y2 = y + 124 + (int)(14 * (1 - c2));
  (void)a;
  fill_round(kx, y0, kw + 30, 36, 18, kc, (int)(0.9f * a0));
  draw_text_a(S.f22, kind, kx + 15, y0 + 5, 0x0a0a12, 0, 0, a0);
  fresh_pill(ft, kx + kw + 42, y0, 36, a0);
  const char *title = e->is_folder ? e->node->name : menu_display_name(&S.menu, e->app);
  draw_text_a(S.f48, title, cx, y1, 0xffffff, 1, maxw, a1);
  char meta[200];
  meta_line(e, meta, sizeof meta);
  draw_text_a(S.f26, meta, cx, y2, 0xc9d1dc, 1, maxw, a2);
}

/* ------------------------------------------------------------------ */
/* Grille de jaquettes                                                 */
/* ------------------------------------------------------------------ */
static void draw_grid(void) {
  int sel = *cur_sel(), row = sel / GRID_COLS;
  if (row < S.grid_top) S.grid_top = row;
  if (row > S.grid_top + 2) S.grid_top = row - 2;
  if (S.grid_top < 0) S.grid_top = 0;
  S.grid_scroll += (S.grid_top - S.grid_scroll) * ease(0.2f);
  if (fabsf(S.grid_top - S.grid_scroll) < 0.002f) S.grid_scroll = (float)S.grid_top;
  S.sel_anim += (1 - S.sel_anim) * ease(0.25f);
  if (S.lite) S.sel_anim = 1;

  int x0 = (W - (GRID_COLS * GRID_CELL + (GRID_COLS - 1) * GRID_GAP)) / 2;
  int right = S.mode == MODE_THEME ? W - 860 : W;   /* l'editeur de theme occupe la droite */
  SDL_Rect clip = { 0, GRID_Y - 40, right, 1010 - (GRID_Y - 40) };
  SDL_RenderSetClipRect(S.r, &clip);
  for (int pass = 0; pass < 2; pass++)             /* l'element choisi est dessine en dernier */
    for (int i = 0; i < S.nentries; i++) {
      if ((i == sel) != pass) continue;
      int r = i / GRID_COLS, c = i % GRID_COLS;
      float y = GRID_Y + (r - S.grid_scroll) * GRID_ROWH;
      if (y > 1010 || y + GRID_ROWH < GRID_Y - 40) continue;
      int x = x0 + c * (GRID_CELL + GRID_GAP);
      Entry *e = &S.entries[i];
      int is_sel = i == sel && S.mode != MODE_KEYBOARD, sz = GRID_CELL;
      if (is_sel) sz = (int)(GRID_CELL * (1 + 0.12f * S.sel_anim));
      int szh = TH(sz), ix = x + (GRID_CELL - sz) / 2, iy = (int)y + (TH(GRID_CELL) - szh) / 2;
      int moving = S.mode == MODE_MOVE && is_sel;
      if (is_sel) {
        uint32_t gc = moving ? 0xffc53d : S.th->accent;
        float pulse = 0.5f + 0.5f * sinf(S.frame * 0.08f);
        if (S.lite) {
          SDL_SetTextureColorMod(S.tile_glow, (gc >> 16) & 255, (gc >> 8) & 255, gc & 255);
          SDL_SetTextureAlphaMod(S.tile_glow, (Uint8)ga((int)(110 + 40 * pulse)));
          int gs = sz * 8 / 5;
          SDL_Rect gd = { ix + sz / 2 - gs / 2, iy + szh / 2 - gs / 2, gs, gs };
          SDL_RenderCopy(S.r, S.tile_glow, NULL, &gd);
        } else glow(ix + sz / 2, iy + szh / 2, (int)(szh * (0.8f + 0.08f * pulse)), gc, ga(150));
        fill_round(ix + 8, iy + 14, sz, szh, tile_rad() * sz / TILE_SEL, 0x000000, 120);
      }
      if (e->is_folder) draw_folder_tile(e->node, ix, iy, sz, szh);
      else draw_app_tile(e->app, ix, iy, sz, szh);
      if (is_sel) outline_round(ix, iy, sz, szh, tile_rad() * sz / TILE_SEL, 4, moving ? 0xffc53d : 0xffffff, 255);
      const char *name = e->is_folder ? e->node->name : menu_display_name(&S.menu, e->app);
      draw_text(is_sel ? S.f26 : S.f22, name, x + GRID_CELL / 2, (int)y + TH(GRID_CELL) + (is_sel ? 22 : 14),
                is_sel ? 0xffffff : 0x9aa4b5, 1, GRID_CELL + GRID_GAP - 6);
    }
  SDL_RenderSetClipRect(S.r, NULL);
  if (!S.nentries) {
    draw_text(S.f48, S.sec == SEC_HOME && S.depth ? "Ce dossier est vide" : "Rien à afficher ici", W / 2, 420, 0xffffff, 1, 0);
    return;
  }
  /* barre de defilement */
  int rows = (S.nentries + GRID_COLS - 1) / GRID_COLS;
  if (rows > 3) {
    int th = 3 * GRID_ROWH * 3 / rows, ty = GRID_Y + (int)((3 * GRID_ROWH - th) * S.grid_scroll / (rows - 3));
    fill_round(W - 60, GRID_Y, 6, 3 * GRID_ROWH, 3, 0xffffff, 30);
    fill_round(W - 60, ty, 6, th, 3, S.th->accent, 220);
  }
}

static void draw_footer(void) {
  const char *hint;
  char buf[200];
  switch (S.mode) {
  case MODE_MOVE: hint = S.menu.view == VIEW_GRID ? "◀ ▶  Déplacer      ✕  Poser" : "◀ ▶  Déplacer      ✕  Poser"; break;
  case MODE_CONTEXT: case MODE_PICKFOLDER: case MODE_COLOR: hint = "✕  Choisir      ○  Annuler"; break;
  case MODE_THEME: hint = ""; break;
  case MODE_KEYBOARD: hint = "✕  Touche      □  Effacer      △  Espace      OPTIONS  Valider      ○  Annuler"; break;
  default:
    if (S.sec == SEC_SYS) { hint = "▲▼  Choisir      ◀ ▶  Modifier      ✕  Valider      L1 R1  Sections"; break; }
    snprintf(buf, sizeof buf, tr("%s      L1 R1  Sections      Pavé tactile  Vue : %s"),
             tr(S.sec == SEC_HOME ? (S.depth ? "✕  Ouvrir      ○  Retour      △  Options      □  Déplacer"
                                             : "✕  Ouvrir / Jouer      △  Options      □  Déplacer")
                                  : S.sec == SEC_PAYLOAD ? "✕  Envoyer      △  Options      □  Filtrer"
                                  : "✕  Jouer      △  Options      □  Filtrer"),
             tr(VIEW_NAMES[S.menu.view]));
    hint = buf;
  }
  draw_text(S.f22, hint, CAR_X, H - 56, 0x9aa4b5, 0, 0);
  if (S.menu.view != VIEW_CLASSIC && S.nentries && S.mode != MODE_THEME && S.sec != SEC_SYS) {
    char pos[32];
    snprintf(pos, sizeof pos, "%d / %d", *cur_sel() + 1, S.nentries);
    draw_text(S.f22, pos, W - CAR_X, H - 56, 0x9aa4b5, 2, 0);
  }
}

static void glass_panel(int x, int y, int w, int h) {
  set_color(0x000000, 150);
  SDL_Rect dim = { 0, 0, W, H };
  SDL_RenderFillRect(S.r, &dim);
  glow(x + w / 2, y, w / 2, S.th->accent, 60);
  fill_round(x, y, w, h, 26, 0x0d111c, 238);
  outline_round(x, y, w, h, 26, 2, S.th->accent, 160);
}

enum { SFX_MOVE, SFX_SECTION, SFX_OK, SFX_BACK, SFX_LAUNCH, SFX_ERROR, SFX_BOOT, SFX_COUNT };
static void sfx(int id);
static void draw_settings(void);
static void draw_launching(void);
static void draw_theme_editor(void);
static void refresh_settings(void);
static void open_keyboard(const char *title, const char *initial, int action);
static void check_tile_shape(void);
static void open_theme_editor(void);
static void music_fade(void);
static void theme_editor_input(int in);
static void draw_gx(Entry *e);
static void draw_quick_info(void);
static void draw_info(void);
static void draw_help(void);
static void draw_status(int px, int py, int pw, int ph);
static void status_refresh(void);
static void status_repair(void);
static void status_poll(void);
static void open_help(int kind);
static void draw_catalog(void);
static void open_start_menu(void);
static void first_run_autoload(void);
static void start_mode_set(int m);
static void autoload_mark(int on);
static void autoload_lost_check(void);
static void open_catalog(void);
static void theme_saved_as(const char *name);

static void draw_list_panel(const char *title, int n, const char *(*label)(int), int sel, uint32_t (*swatch)(int)) {
  int pw = 760, ph = 120 + n * 66;
  if (text_w(S.f36, title) + 88 > pw) pw = text_w(S.f36, title) + 88;   /* le titre aussi */
  for (int i = 0; i < n; i++) {                  /* plus large si un choix ne tient pas */
    int need = text_w(S.f30, label(i)) + 130 + (swatch ? 54 : 0);
    if (need > pw) pw = need;
  }
  if (pw > W - 160) pw = W - 160;
  int px = (W - pw) / 2, py = (H - ph) / 2;
  if (ph > H - 140) { ph = H - 140; py = 70; }
  glass_panel(px, py, pw, ph);
  draw_text(S.f36, title, px + 44, py + 32, 0xffffff, 0, pw - 88);
  int first = 0, visible = (ph - 120) / 66;
  if (sel >= visible) first = sel - visible + 1;
  for (int i = first; i < n && i - first < visible; i++) {
    int y = py + 104 + (i - first) * 66;
    if (i == sel) { fill_round(px + 22, y - 8, pw - 44, 58, 16, S.th->accent, 255); }
    int tx = px + 48;
    if (swatch) { fill_round(tx, y + 4, 34, 34, 10, swatch(i), 255); tx += 54; }
    draw_text(S.f30, label(i), tx, y + 4, i == sel ? 0x0a0a12 : 0xffffff, 0, pw - 130);
  }
}

static const char *choice_label(int i) { return S.choices[i].label; }
static const char *folder_label(int i) { return S.folders[i].label; }
static const char *color_label(int i) { return PALETTE_NAMES[i]; }
static uint32_t color_swatch(int i) { return PALETTE[i]; }

/* Clavier a l'ecran */
static const char *const KB_LAYOUTS[L_COUNT][2][5] = {
  [L_FR] = { { "azertyuiop", "qsdfghjklm", "wxcvbn'-_.", "1234567890", "éèàçêù!?&+" },
             { "AZERTYUIOP", "QSDFGHJKLM", "WXCVBN'-_.", "1234567890", "ÉÈÀÇÊÙ!?&+" } },
  [L_EN] = { { "qwertyuiop", "asdfghjkl-", "zxcvbnm'_.", "1234567890", "!?&+()#@:/" },
             { "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM'_.", "1234567890", "!?&+()#@:/" } },
  [L_ES] = { { "qwertyuiop", "asdfghjklñ", "zxcvbnm-_.", "1234567890", "áéíóúü¿¡!?" },
             { "QWERTYUIOP", "ASDFGHJKLÑ", "ZXCVBNM-_.", "1234567890", "ÁÉÍÓÚÜ¿¡!?" } },
  [L_DE] = { { "qwertzuiop", "asdfghjkl-", "yxcvbnm'_.", "1234567890", "äöüß!?&+()" },
             { "QWERTZUIOP", "ASDFGHJKL-", "YXCVBNM'_.", "1234567890", "ÄÖÜß!?&+()" } },
  [L_IT] = { { "qwertyuiop", "asdfghjkl-", "zxcvbnm'_.", "1234567890", "àèéìòù!?&+" },
             { "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM'_.", "1234567890", "ÀÈÉÌÒÙ!?&+" } },
  [L_PT] = { { "qwertyuiop", "asdfghjklç", "zxcvbnm-_.", "1234567890", "áàâãéêíóõú" },
             { "QWERTYUIOP", "ASDFGHJKLÇ", "ZXCVBNM-_.", "1234567890", "ÁÀÂÃÉÊÍÓÕÚ" } },
};
#define KB_ROWS KB_LAYOUTS[i18n_lang()]
#define KB_SPECIAL_ROW 5
static const char *KB_SPECIAL[] = { "Maj", "Espace", "Effacer", "Valider" };

static void utf8_at(const char *s, int idx, char *out) {
  int i = 0;
  while (*s) {
    int len = 1;
    unsigned char c = (unsigned char)*s;
    if (c >= 0xF0) len = 4; else if (c >= 0xE0) len = 3; else if (c >= 0xC0) len = 2;
    if (i == idx) { memcpy(out, s, len); out[len] = 0; return; }
    s += len; i++;
  }
  out[0] = 0;
}

static void draw_keyboard(void) {
  int pw = 1200, ph = 650, px = (W - pw) / 2, py = (H - ph) / 2 - 20;
  glass_panel(px, py, pw, ph);
  if (S.kb_action == ACT_SEARCH && S.kb_buf[0]) {   /* resultats en direct pendant la frappe */
    char t[120];
    int n = search_count(S.kb_buf);
    snprintf(t, sizeof t, tr(n > 1 ? "Rechercher  ·  %d résultats" : "Rechercher  ·  %d résultat"), n);
    draw_text(S.f36, t, px + 44, py + 30, 0xffffff, 0, pw - 88);
  } else draw_text(S.f36, S.kb_title, px + 44, py + 30, 0xffffff, 0, pw - 88);
  fill_round(px + 44, py + 88, pw - 88, 72, 16, 0x05070d, 255);
  outline_round(px + 44, py + 88, pw - 88, 72, 16, 2, S.th->accent, 120);
  char shown[NAME_LEN + 4];
  snprintf(shown, sizeof shown, "%s%s", S.kb_buf, (S.frame / 30) % 2 ? "|" : "");
  draw_text(S.f36, shown, px + 66, py + 102, 0xffffff, 0, pw - 130);
  int key = 92, gap = 12, kx0 = px + (pw - (10 * key + 9 * gap)) / 2, ky0 = py + 186;
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 10; c++) {
      char ch[8];
      utf8_at(KB_ROWS[S.kb_shift][r], c, ch);
      int x = kx0 + c * (key + gap), y = ky0 + r * (64 + gap);
      int on = S.kb_row == r && S.kb_col == c;
      if (on) glow(x + key / 2, y + 32, 90, S.th->accent, 120);
      fill_round(x, y, key, 64, 14, on ? S.th->accent : 0x1c2433, 255);
      draw_text(S.f36, ch, x + key / 2, y + 10, on ? 0x0a0a12 : 0xffffff, 1, 0);
    }
  int y = ky0 + 5 * (64 + gap), x = kx0, total = 10 * key + 9 * gap;
  int widths[4] = { 180, total - 180 * 3 - 3 * gap, 180, 180 };
  for (int i = 0; i < 4; i++) {
    int on = S.kb_row == KB_SPECIAL_ROW && S.kb_col == i;
    uint32_t c = i == 3 ? 0x2f9e5a : (i == 0 && S.kb_shift ? S.th->accent2 : 0x1c2433);
    fill_round(x, y, widths[i], 64, 14, on ? S.th->accent : c, 255);
    draw_text(S.f26, KB_SPECIAL[i], x + widths[i] / 2, y + 18, on ? 0x0a0a12 : 0xffffff, 1, 0);
    x += widths[i] + gap;
  }
}

static void draw_toast(void) {
  if (!S.toast[0] || SDL_GetTicks() > S.toast_until) return;
  int w = text_w(S.f26, S.toast);
  int cx = S.mode == MODE_THEME ? (W - 870) / 2 : W / 2;   /* a cote de l'editeur de theme */
  int x = cx - w / 2 - 34;
  fill_round(x, 120, w + 68, 64, 32, 0x0d111c, 235);
  outline_round(x, 120, w + 68, 64, 32, 2, S.th->accent, 200);
  draw_text(S.f26, S.toast, cx, 137, 0xffffff, 1, 0);
}

/* Theme de l'image en cours : couleurs du jeu choisi si « couleurs auto » */
static uint32_t mix_color(uint32_t c, uint32_t t, float k) {
  uint32_t out = 0;
  for (int sh = 0; sh <= 16; sh += 8) {
    int a = (c >> sh) & 255, b = (t >> sh) & 255, v = a + (int)((b - a) * k);
    if (abs(b - v) < 3) v = b;
    out |= (uint32_t)v << sh;
  }
  return out;
}

static void frame_theme(Entry *e) {
  S.fth = *S.base;
  uint32_t ta = S.base->accent, ta2 = S.base->accent2;
  if (S.base->auto_color && e && !e->is_folder && e->app->kind != KIND_SYSTEM) {
    cover(e->app);
    if (e->app->has_vibe) { ta = e->app->vibe; ta2 = e->app->vibe2; }
  }
  if (!S.base->auto_color || (!S.acc_now && !S.acc2_now)) { S.acc_now = ta; S.acc2_now = ta2; }
  else if (S.lite) {                             /* PS5 : changement net une fois la selection posee */
    if (SDL_GetTicks() - S.sel_since > 150) { S.acc_now = ta; S.acc2_now = ta2; }
  } else {
    S.acc_now = mix_color(S.acc_now, ta, ease(0.08f));
    S.acc2_now = mix_color(S.acc2_now, ta2, ease(0.08f));
  }
  S.fth.accent = S.acc_now;
  S.fth.accent2 = S.acc2_now;
  S.th = &S.fth;
}

/* Intro au demarrage : le logo s'allume lettre par lettre */
static void draw_boot(void) {
  if (S.boot_done) return;
  Uint32 el = SDL_GetTicks() - S.boot_t0;
  if (el > 1700) { S.boot_done = 1; return; }
  int a = el < 1150 ? 255 : (int)(255 * (1 - (el - 1150) / 550.0f));
  set_color(0x030408, a);
  SDL_Rect full = { 0, 0, W, H };
  SDL_RenderFillRect(S.r, &full);
  const char *word = "LAUNCHER";
  int ww = text_w(S.fbig, word), w5 = text_w(S.fbig, "5"), x = (W - ww - w5 - 16) / 2, y = H / 2 - 90;
  if (!S.lite) glow(W / 2, H / 2 - 20, 520, S.th->accent, (int)(a * 0.45f));
  char one[2] = { 0, 0 };
  for (int i = 0; word[i]; i++) {
    one[0] = word[i];
    int t = (int)el - i * 55, la = t <= 0 ? 0 : t > 200 ? a : a * t / 200;
    if (la > 0) draw_text_a(S.fbig, one, x, y + (t < 200 ? (200 - t) / 8 : 0), 0xffffff, 0, 0, la);
    x += text_w(S.fbig, one);
  }
  int t5 = (int)el - 560;
  if (t5 > 0) {
    int a5 = t5 > 160 ? a : a * t5 / 160;
    if (!S.lite) glow(x + 16 + w5 / 2, y + 70, 160, S.th->accent, a5 / 2);
    draw_text_a(S.fbig, "5", x + 16, y, S.th->accent, 0, 0, a5);
  }
  int lw = el < 700 ? 0 : el > 1000 ? 420 : (int)(420 * (el - 700) / 300.0f);
  fill_round(W / 2 - lw / 2, y + 160, lw, 6, 3, S.th->accent2, a);
  draw_text_a(S.f26, "by " L5_AUTHOR, W / 2, y + 190, 0x9aa4b5, 1, 0, el > 900 ? a : 0);
}

/* Economiseur d'ecran : tes jaquettes en diaporama */
static void draw_saver_covers(void) {           /* secours : pas d'image dans fonds/ */
  Uint32 now = SDL_GetTicks();
  if (!S.saver_t || now - S.saver_t > 7000) {
    int cand[256], n = 0;
    for (int i = 0; i < S.menu.napps && n < 256; i++)
      if (S.menu.apps[i].kind != KIND_SYSTEM && S.menu.apps[i].icon[0] && !menu_is_hidden(&S.menu, S.menu.apps[i].id)) cand[n++] = i;
    if (!n) { S.saver_on = 0; return; }
    int pick = cand[rand() % n];
    if (n > 1) while (pick == S.saver_idx) pick = cand[rand() % n];
    S.saver_idx = pick;
    S.saver_t = now;
  }
  float k = (now - S.saver_t) / 7000.0f, fade = k < 0.14f ? k / 0.14f : k > 0.9f ? (1 - k) / 0.1f : 1;
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 255);
  SDL_RenderClear(S.r);
  App *a = &S.menu.apps[S.saver_idx];
  SDL_Texture *bg = blurred(a);
  if (bg) {
    SDL_SetTextureAlphaMod(bg, (Uint8)(180 * fade));
    SDL_Rect full = { -60 - (int)(40 * k), -40, W + 140, H + 80 };
    SDL_RenderCopy(S.r, bg, NULL, &full);
  }
  SDL_SetTextureAlphaMod(S.shade_left, 255);
  SDL_Rect left = { 0, 0, W * 3 / 4, H };
  SDL_RenderCopy(S.r, S.shade_left, NULL, &left);
  SDL_Texture *t = tile_tex(a, FLOW, TH(FLOW));
  if (t) {
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * fade));
    SDL_Rect d = { 200 + (int)(50 * k), (H - TH(FLOW)) / 2, FLOW, TH(FLOW) };
    SDL_RenderCopy(S.r, t, NULL, &d);
    SDL_SetTextureAlphaMod(t, 255);
  }
  int al = (int)(255 * fade);
  draw_text_a(S.f48, menu_display_name(&S.menu, a), 700, H / 2 - 70, 0xffffff, 0, 1100, al);
  Stat *st = menu_stat(&S.menu, a->id, 0);
  if (st && st->playtime > 0) {
    char pt[48];
    playtime_str(st->playtime, pt, sizeof pt);
    draw_text_a(S.f26, pt, 702, H / 2 + 10, S.th->accent, 0, 0, al);
  }
  time_t tnow = time(NULL);
  char clk[16];
  strftime(clk, sizeof clk, S.th->clock == CLOCK_12 ? "%I:%M" : "%H:%M", localtime(&tnow));
  draw_text_a(S.fbig, clk, W - CAR_X, H - 230, 0xffffff, 2, 0, 200);
  draw_text_a(S.f22, "Appuie sur une touche pour revenir", W - CAR_X, H - 80, 0x9aa4b5, 2, 0, 160);
  draw_particles(S.lite);
}

/* Economiseur d'ecran : diaporama des fonds d'ecran (dossier fonds/), lent zoom
   et fondu enchaine, horloge par-dessus. */
static void scan_media_files(void);
static SDL_Texture *SV[2];                       /* image affichee, image precedente (fondu) */
static int sv_wall = -1;

static void saver_free(void) {
  for (int i = 0; i < 2; i++) if (SV[i]) { SDL_DestroyTexture(SV[i]); SV[i] = NULL; }
  sv_wall = -1;
}

/* image recadree, plus large que l'ecran, preparee une fois : ensuite elle glisse
   sans etre redimensionnee. Environ un pixel par image : un mouvement continu
   (plus lent, elle avancerait par a-coups d'un pixel de temps en temps). */
#define SV_W (W + 640)
#define SV_H H
static SDL_Texture *saver_prep(SDL_Texture *t) {
  int tw, th;
  SDL_QueryTexture(t, NULL, NULL, &tw, &th);
  SDL_Rect src = { 0, 0, tw, th };
  if ((long)tw * SV_H > (long)th * SV_W) { src.w = (int)((long)th * SV_W / SV_H); src.x = (tw - src.w) / 2; }
  else { src.h = (int)((long)tw * SV_H / SV_W); src.y = (th - src.h) / 2; }
  SDL_Texture *b = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, SV_W, SV_H);
  if (!b) return t;
  SDL_Texture *prev_rt = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, b);
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
  SDL_RenderCopy(S.r, t, &src, NULL);
  SDL_SetTextureAlphaMod(S.shade_bottom, 255);              /* bas assombri sous l'horloge, inclus une fois */
  SDL_Rect sh = { 0, H + (SV_H - H) / 2 - 420, SV_W, 420 };
  SDL_RenderCopy(S.r, S.shade_bottom, NULL, &sh);
  SDL_SetRenderTarget(S.r, prev_rt);
  SDL_DestroyTexture(t);
  SDL_SetTextureBlendMode(b, SDL_BLENDMODE_BLEND);
  return b;
}

static void draw_saver(void) {
  enum { SHOW = 9000, FADE = 1600 };
  Uint32 now = SDL_GetTicks();
  if (!S.saver_t) scan_media_files();            /* images ajoutees depuis la derniere fois */
  if (!S.nwalls) { draw_saver_covers(); return; }
  if (!S.saver_t || now - S.saver_t > SHOW) {
    int pick = rand() % S.nwalls;
    if (S.nwalls > 1) while (pick == sv_wall) pick = rand() % S.nwalls;
    char path[PATH_LEN];
    snprintf(path, sizeof path, "%s/fonds/%s", plat_data_dir(), S.walls[pick]);
    SDL_Texture *t = load_tex(path);
    if (t) t = saver_prep(t);
    if (t) {
      if (SV[1]) SDL_DestroyTexture(SV[1]);
      SV[1] = SV[0]; SV[0] = t; sv_wall = pick;
    }
    S.saver_t = now;
  }
  if (!SV[0]) { draw_saver_covers(); return; }
  Uint32 el = now - S.saver_t;
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 255);
  SDL_RenderClear(S.r);
  for (int i = 1; i >= 0; i--) {
    SDL_Texture *t = SV[i];
    if (!t || (i && el >= FADE)) continue;
    float pan = (float)(i ? SHOW + el : el) / (SHOW + FADE);   /* 0..1 sur toute sa duree d'affichage */
    if (pan > 1) pan = 1;
    int side = ((sv_wall + i) & 1) ? 1 : -1;
    SDL_Rect d = { -(int)((SV_W - W) * (side > 0 ? pan : 1 - pan)), -(SV_H - H) / 2, SV_W, SV_H };
    int a = i ? 255 : el < FADE ? (int)(255 * el / FADE) : 255;
    SDL_SetTextureBlendMode(t, a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetTextureAlphaMod(t, (Uint8)a);
    SDL_RenderCopy(S.r, t, NULL, &d);
    SDL_SetTextureAlphaMod(t, 255);
  }
  /* horloge et date */
  time_t tnow = time(NULL);
  struct tm *tm = localtime(&tnow);
  char clk[16], day[64];
  strftime(clk, sizeof clk, S.th->clock == CLOCK_12 ? "%I:%M" : "%H:%M", tm);
  static const char *JOURS_L[7] = { "dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam." };
  snprintf(day, sizeof day, "%s %d/%02d", tr(JOURS_L[tm->tm_wday]), tm->tm_mday, tm->tm_mon + 1);
  draw_text_a(S.fbig, clk, W - CAR_X, H - 330, 0xffffff, 2, 0, 235);
  draw_text_a(S.f36, day, W - CAR_X - 4, H - 150, 0xdfe5ee, 2, 0, 225);
  draw_text_a(S.f22, "Appuie sur une touche pour revenir", W - CAR_X - 4, H - 90, 0x9aa4b5, 2, 0, 160);
  draw_particles(S.lite);
}

static void draw_letter(void) {
  if (!S.letter[0] || SDL_GetTicks() > S.letter_until) return;
  int a = (int)(255 * fminf(1.0f, (S.letter_until - SDL_GetTicks()) / 250.0f));
  fill_round(W / 2 - 110, H / 2 - 150, 220, 220, 40, 0x000000, a * 200 / 255);
  outline_round(W / 2 - 110, H / 2 - 150, 220, 220, 40, 3, S.th->accent, a);
  draw_text_a(S.fbig, S.letter, W / 2, H / 2 - 120, 0xffffff, 1, 0, a);
}

/* ------------------------------------------------------------------ */
/* Interface facon loader GX : fond clair, barre basse en vague bleue,  */
/* boutons ronds, horloge a afficheur, carrousel de boitiers en arc.    */
/* Le decor fixe est dessine une fois dans une texture.                 */
/* ------------------------------------------------------------------ */
#define GX_EDGE(x) gx_edge(x)
static float gx_edge(float x) {                  /* bord haut de la barre du bas */
  float side = 770, mid = 862, a = 470, b = 760;   /* plat sur les cotes, creux au centre */
  float d = fabsf(x - W / 2.0f);
  if (d >= b) return side;
  if (d <= a) return mid;
  float k = (d - a) / (b - a);
  return mid + (side - mid) * (0.5f - 0.5f * cosf(k * 3.14159f));
}

static void gx_circle(int cx, int cy, int r, uint32_t c, int a) { fill_round(cx - r, cy - r, r * 2, r * 2, r, c, a); }
static void gx_ring(int cx, int cy, int r, int th, uint32_t c, int a) { outline_round(cx - r, cy - r, r * 2, r * 2, r, th, c, a); }

static void gx_tri(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t c) {
  SDL_Vertex v[3];
  memset(v, 0, sizeof v);
  v[0].position.x = x0; v[0].position.y = y0; v[1].position.x = x1; v[1].position.y = y1;
  v[2].position.x = x2; v[2].position.y = y2;
  for (int i = 0; i < 3; i++) { v[i].color.r = (c >> 16) & 255; v[i].color.g = (c >> 8) & 255; v[i].color.b = c & 255; v[i].color.a = 255; }
  SDL_RenderGeometry(S.r, NULL, v, 3, NULL, 0);
}

/* un chiffre d'afficheur a 7 segments (allumes en gris fonce, eteints en gris clair) */
static void gx_digit(int x, int y, int w, int h, int d, uint32_t on, uint32_t off) {
  static const unsigned char SEG[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
  int t = w / 6, hh = h / 2;
  SDL_Rect r[7] = {
    { x + t, y, w - 2 * t, t }, { x + w - t, y + t, t, hh - t }, { x + w - t, y + hh, t, hh - t },
    { x + t, y + h - t, w - 2 * t, t }, { x, y + hh, t, hh - t }, { x, y + t, t, hh - t }, { x + t, y + hh - t / 2, w - 2 * t, t },
  };
  for (int i = 0; i < 7; i++) fill_round(r[i].x, r[i].y, r[i].w, r[i].h, t / 2, (SEG[d % 10] >> i) & 1 ? on : off, 255);
}

static SDL_Texture *gx_bg_tex, *gx_bar_tex;
static int gx_bg_ver = -1;

static void gx_build_bg(void) {
  if (gx_bg_tex && gx_bar_tex && gx_bg_ver == S.th_ver) return;
  if (!gx_bg_tex) gx_bg_tex = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
  if (!gx_bar_tex) gx_bar_tex = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H - 740);
  if (!gx_bg_tex || !gx_bar_tex) return;
  gx_bg_ver = S.th_ver;
  uint32_t ac = S.base->accent;
  SDL_Texture *prev = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, gx_bg_tex);
  for (int y = 0; y < H; y += 4) {               /* fond clair, legerement degrade */
    set_color(mix_color(0xf6f6f7, 0xd5d7da, (float)y / H), 255);
    SDL_Rect r = { 0, y, W, 4 };
    SDL_RenderFillRect(S.r, &r);
  }
  SDL_Texture *wp = wallpaper();                  /* image choisie dans le studio : a la place du fond clair */
  if (wp) {
    int iw, ih;
    SDL_QueryTexture(wp, NULL, NULL, &iw, &ih);
    SDL_Rect sr = { 0, 0, iw, ih };
    if ((long)iw * H > (long)ih * W) { sr.w = ih * W / H; sr.x = (iw - sr.w) / 2; }
    else { sr.h = iw * H / W; sr.y = (ih - sr.h) / 2; }
    SDL_SetTextureAlphaMod(wp, 255);
    SDL_RenderCopy(S.r, wp, &sr, NULL);
  }
  /* la barre du bas va dans sa propre texture (transparente au-dessus de la vague) */
  SDL_SetRenderTarget(S.r, gx_bar_tex);
  SDL_SetTextureBlendMode(gx_bar_tex, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
  SDL_RenderClear(S.r);
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
  SDL_Rect vp = { 0, -740, W, H };                /* memes coordonnees que l'ecran */
  SDL_RenderSetViewport(S.r, &vp);
  for (int x = 0; x < W; x += 2) {               /* barre du bas : sous la vague */
    int e = (int)GX_EDGE(x);
    for (int y = e; y < H; y += 6) {
      set_color(mix_color(0xeeeff1, 0xd2d4d8, (float)(y - e) / (H - e)), 255);
      SDL_Rect r = { x, y, 2, 6 };
      SDL_RenderFillRect(S.r, &r);
    }
  }
  for (int x = 0; x < W; x++) {                  /* la vague : trait bleu et reflet clair */
    int e = (int)GX_EDGE(x);
    set_color(ac, 255);
    SDL_Rect r = { x, e - 2, 1, 5 };
    SDL_RenderFillRect(S.r, &r);
    set_color(mix_color(ac, 0xffffff, 0.6f), 200);
    SDL_Rect r2 = { x, e + 10, 1, 2 };
    SDL_RenderFillRect(S.r, &r2);
  }
  /* gros boutons ronds : reglages a gauche, Launcher5 a droite */
  int bx[2] = { 300, W - 300 }, by = 905;
  for (int i = 0; i < 2; i++) {
    gx_circle(bx[i], by, 112, 0xe8eaed, 255);
    gx_circle(bx[i], by, 100, 0xf4f5f6, 255);
    gx_ring(bx[i], by, 100, 7, ac, 255);
    gx_circle(bx[i], by - 18, 70, 0xffffff, 120);   /* reflet */
  }
  draw_text(S.f84, "⚙", bx[0], by - 56, 0x9ea3aa, 1, 0);
  draw_text(S.f48, "L5", bx[1], by - 34, 0xa2a7ae, 1, 0);
  /* petits boutons : plus et marche */
  int sx[2] = { 105, W - 105 }, sy = 800;
  for (int i = 0; i < 2; i++) { gx_circle(sx[i], sy, 30, 0xf7f7f8, 255); gx_ring(sx[i], sy, 30, 3, mix_color(ac, 0xffffff, 0.3f), 255); }
  fill_round(sx[0] - 13, sy - 3, 26, 6, 3, 0xa2a7ae, 255);
  fill_round(sx[0] - 3, sy - 13, 6, 26, 3, 0xa2a7ae, 255);
  gx_ring(sx[1], sy + 2, 12, 4, 0xa2a7ae, 255);
  fill_round(sx[1] - 2, sy - 16, 5, 16, 2, 0xa2a7ae, 255);
  /* carte memoire a gauche du texte, carte bleue a droite */
  fill_round(548, 878, 74, 98, 8, 0xffffff, 255);
  outline_round(548, 878, 74, 98, 8, 2, 0xb9bec5, 255);
  fill_round(560, 892, 50, 26, 4, 0xdfe2e6, 255);
  draw_text(S.f22, "SD", 585, 932, 0x9ea3aa, 1, 0);
  int cx = W - 690, cy = 878;
  for (int y = 0; y < 98; y += 2) {
    set_color(mix_color(mix_color(ac, 0xffffff, 0.55f), shade(ac, 0.8f), (float)y / 98), 255);
    SDL_Rect r = { cx, cy + y, 150, 2 };
    SDL_RenderFillRect(S.r, &r);
  }
  for (int x = 0; x < 150; x++) {                /* vague blanche sur la carte */
    int y = cy + 46 + (int)(8 * sinf(x * 0.06f));
    set_color(0xffffff, 200);
    SDL_Rect r = { cx + x, y, 1, 3 };
    SDL_RenderFillRect(S.r, &r);
  }
  outline_round(cx, cy, 150, 98, 12, 3, 0xffffff, 255);
  SDL_RenderSetViewport(S.r, NULL);
  SDL_SetRenderTarget(S.r, prev);
}

/* barre d'icones du haut : une case par section */
static void gx_toolbar(void) {
  static const char *const ICON[SEC_COUNT] = { "▤", "★", "PS5", "PS4", "▶", "HB", "EMU", "ELF", "↑", "◷", "⚙" };
  int n = 0, cw = 92, ch = 62;
  for (int k = 0; k < SEC_COUNT; k++) if (S.sec_has[SEC_ORDER[k]]) n++;
  int tw = n * cw, x = (W - tw) / 2, y = 26;
  fill_round(x - 8, y - 6, tw + 16, ch + 12, 22, 0xc9ccd1, 255);
  fill_round(x - 6, y - 4, tw + 12, ch + 8, 20, 0xe4e6e9, 255);
  int i = 0;
  for (int k = 0; k < SEC_COUNT; k++) {
    int s = SEC_ORDER[k];
    if (!S.sec_has[s]) continue;
    int cxl = x + i * cw;
    if (s == S.sec) {
      fill_round(cxl + 3, y + 2, cw - 6, ch - 4, 14, mix_color(S.base->accent, 0xffffff, 0.65f), 255);
      outline_round(cxl + 3, y + 2, cw - 6, ch - 4, 14, 2, S.base->accent, 255);
    }
    if (i) fill_round(cxl, y + 10, 2, ch - 20, 1, 0xc4c7cc, 255);
    draw_text(strlen(ICON[s]) > 2 ? S.f22 : S.f30, ICON[s], cxl + cw / 2, y + (strlen(ICON[s]) > 2 ? 17 : 12), s == S.sec ? 0x3d4248 : 0x8a9097, 1, cw - 8);
    i++;
  }
}

/* decor, barre d'icones et carrousel : une image complete, refaite seulement quand elle change */
static SDL_Rect gx_sel;
static int gx_sel_ok;
static void gx_scene(int sel) {
  gx_sel_ok = 0;
  if (gx_bg_tex) {
    SDL_SetTextureBlendMode(gx_bg_tex, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(S.r, gx_bg_tex, NULL, NULL);
  }
  uint32_t ac = S.base->accent;
  draw_fx();                                     /* reglages du theme : effet anime et particules */
  draw_particles(S.lite);
  gx_toolbar();
  gx_tri(46, 190, 104, 150, 104, 230, ac);       /* fleches bleues */
  gx_tri(W - 46, 190, W - 104, 150, W - 104, 230, ac);
  int tdx, tdy;
  tr_offset(&tdx, &tdy);
  float maxrel = S.lite ? 4.6f : 5.6f, R = 1350, cx0 = W / 2.0f + tdx, cy0 = 405;
  FlowItem items[32]; int n = 0;
  for (int i = (int)S.scroll - 6; i <= (int)S.scroll + 7 && n < 32; i++) {
    if (i < 0 || i >= S.nentries) continue;
    float rel = i - S.scroll;
    if (fabsf(rel) > maxrel) continue;
    items[n].i = i; items[n].rel = rel; n++;
  }
  qsort(items, n, sizeof items[0], cmp_flow);
  for (int k = 0; k < n; k++) {
    Entry *en = &S.entries[items[k].i];
    float rel = items[k].rel, th = rel * 0.135f, a = fabsf(rel);
    float sc = a < 1 ? 1.0f - 0.18f * a : 0.82f - 0.02f * (a - 1);
    int w = (int)(FLOW * 0.82f * sc), h = BOXES ? w * 3 / 2 : w;
    float x = cx0 + R * sinf(th), y = cy0 + R * (1 - cosf(th));
    SDL_Texture *t = en->is_folder ? folder_tex(en->node) : tile_tex(en->app, FLOW, TH(FLOW));
    SDL_Rect d = { (int)(x - w / 2.0f), (int)(y - h / 2.0f), w, h };
    if (S.glow) {                                /* ombre douce sous le boitier */
      SDL_SetTextureColorMod(S.glow, 0, 0, 0);
      SDL_SetTextureBlendMode(S.glow, SDL_BLENDMODE_BLEND);
      SDL_SetTextureAlphaMod(S.glow, 70);
      SDL_Rect sd = { d.x - w / 6, d.y + h - h / 10, w + w / 3, h / 5 };
      SDL_RenderCopy(S.r, S.glow, NULL, &sd);
      SDL_SetTextureBlendMode(S.glow, SDL_BLENDMODE_ADD);
    }
    if (t) {
      int b = a > 1 ? (int)(255 - 18 * (a - 1)) : 255;
      SDL_SetTextureColorMod(t, b, b, b);
      SDL_RenderCopyEx(S.r, t, NULL, &d, th * 57.2958f, NULL, SDL_FLIP_NONE);
      SDL_SetTextureColorMod(t, 255, 255, 255);
    }
    if (items[k].i == sel && a < 0.05f) {
      outline_round(d.x - 4, d.y - 4, w + 8, h + 8, 10, 4, ac, 255);
      gx_sel = (SDL_Rect){ d.x - 4, d.y - 4, w + 8, h + 8 };   /* bordure animee : dessinee hors du cache */
      gx_sel_ok = 1;
      if (!en->is_folder && menu_is_fav(&S.menu, en->app->id)) fav_badge(d.x, d.y, w);
    }
  }
  if (!S.nentries) {
    draw_text(S.f36, S.sec == SEC_HOME && S.depth ? "Ce dossier est vide" : "Rien à afficher ici", W / 2, 380, 0x6b737c, 1, 1400);
  }
  if (gx_bar_tex) { SDL_Rect bd = { 0, 740, W, H - 740 }; SDL_RenderCopy(S.r, gx_bar_tex, NULL, &bd); }
}

static void draw_gx(Entry *e) {
  static SDL_Texture *scene;
  static uint32_t scene_key;
  gx_build_bg();
  uint32_t ac = S.base->accent;
  int sel = *cur_sel();
  S.scroll += ((float)sel - S.scroll) * ease(0.18f);
  if (fabsf(sel - S.scroll) < 0.002f) S.scroll = (float)sel;
  uint32_t key = (((((uint32_t)sel * 31 + S.sec) * 31 + S.depth) * 31 + S.nentries) * 31 + S.th_ver) * 31 + tiles_gen;
  key = key * 31 + (S.mode == MODE_MOVE) + 2u * (uint32_t)S.th_ver;
  int moving = S.th->fx != FX_NONE || S.th->particles != PART_NONE;   /* effet anime : pas d'image figee */
  if (S.scroll == (float)sel && S.mode != MODE_MOVE && !moving && !tr_t) {   /* immobile : image en cache */
    if (!scene) scene = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
    if (scene && key != scene_key) {
      SDL_Texture *prev = SDL_GetRenderTarget(S.r);
      SDL_SetRenderTarget(S.r, scene);
      gx_scene(sel);
      SDL_SetRenderTarget(S.r, prev);
      scene_key = key;
    }
    if (scene) { SDL_SetTextureBlendMode(scene, SDL_BLENDMODE_NONE); SDL_RenderCopy(S.r, scene, NULL, NULL); }
    else gx_scene(sel);
  } else { gx_scene(sel); scene_key = 0; }
  if (gx_sel_ok && S.mode != MODE_MOVE) border_run(gx_sel.x, gx_sel.y, gx_sel.w, gx_sel.h, ac);

  /* nom du jeu et horloge a afficheur */
  if (e) {
    const char *nm = e->is_folder ? e->node->name : menu_display_name(&S.menu, e->app);
    draw_text(S.f36, nm, W / 2, 688, shade(mix_color(ac, 0x3a4652, 0.55f), 1.0f), 1, 1300);
    char fb[48];
    const char *ft = fresh_tag(e, fb, sizeof fb);
    if (ft) {
      int fw = fresh_pill_w(ft);
      fill_round(W / 2 - fw / 2, 644, fw, 34, 17, ac, 255);
      draw_text(S.f22, ft, W / 2, 648, 0xffffff, 1, 0);
    }
  }
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  int hh = tm->tm_hour, mm = tm->tm_min, dw = 46, dh = 78, gap = 14, total = dw * 4 + gap * 3 + 26;
  int x0 = W / 2 - total / 2, y0 = 752;
  uint32_t on = 0x7d828a, off = 0xd9dbdf;
  if (S.th->clock == CLOCK_12) { hh %= 12; if (!hh) hh = 12; }
  if (S.th->clock != CLOCK_HIDDEN) {
    gx_digit(x0, y0, dw, dh, hh / 10, on, off);
    gx_digit(x0 + dw + gap, y0, dw, dh, hh % 10, on, off);
    int colon = (SDL_GetTicks() / 500) % 2 ? on : off, cxp = x0 + 2 * (dw + gap) + 2;
    fill_round(cxp, y0 + 20, 9, 9, 4, colon, 255);
    fill_round(cxp, y0 + 50, 9, 9, 4, colon, 255);
    gx_digit(x0 + 2 * (dw + gap) + 26, y0, dw, dh, mm / 10, on, off);
    gx_digit(x0 + 3 * (dw + gap) + 26, y0, dw, dh, mm % 10, on, off);
  }
  /* espace libre et nombre de jeux */
  static Uint32 last; static long long free_mb = -1;
  if (!last || SDL_GetTicks() - last > 5000) {
    last = SDL_GetTicks();
    char ip[64]; int temp;
    plat_sysinfo(ip, sizeof ip, &free_mb, &temp);
  }
  char l1[96], l2[64];
  if (free_mb >= 0) snprintf(l1, sizeof l1, tr("%lld Go libres"), free_mb / 1024); else l1[0] = 0;
  snprintf(l2, sizeof l2, tr("%s : %d"), tr(SEC_NAMES[S.sec]), S.nentries);
  draw_text(S.f30, l1, W / 2, 900, 0x4f5862, 1, 900);
  draw_text(S.f30, l2, W / 2, 944, 0x4f5862, 1, 900);
}

/* ------------------------------------------------------------------ */
/* Interface facon XMB (theme Rebug) : categories a l'horizontale,      */
/* contenus a la verticale, vagues qui defilent sur un fond sombre.     */
/* Le fond (degrade + vagues) est dessine une fois, deux ecrans de      */
/* large, puis simplement decale a chaque image.                        */
/* ------------------------------------------------------------------ */
static SDL_Texture *xmb_bg_tex;
static int xmb_bg_ver = -1, xmb_image;

static void xmb_build_bg(void) {
  if (xmb_bg_tex && xmb_bg_ver == S.th_ver) return;
  if (!xmb_bg_tex) xmb_bg_tex = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W * 2, H);
  if (!xmb_bg_tex) return;
  xmb_bg_ver = S.th_ver;
  uint32_t top = S.base->top, bot = S.base->bottom, ac = S.base->accent;
  SDL_Texture *prev = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, xmb_bg_tex);
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_NONE);
  for (int y = 0; y < H; y += 4) {               /* degrade, un peu plus lumineux au niveau des vagues */
    float k = (float)y / H, g = 1.0f - fabsf(k - 0.62f) * 1.7f;
    if (g < 0) g = 0;
    set_color(mix_color(mix_color(top, bot, k), mix_color(bot, ac, 0.3f), g * 0.6f), 255);
    SDL_Rect r = { 0, y, W * 2, 4 };
    SDL_RenderFillRect(S.r, &r);
  }
  SDL_Texture *wp = wallpaper();                  /* image du theme : fixe, assombrie a gauche pour la lecture */
  xmb_image = wp != NULL;
  if (wp) {
    int iw, ih;
    SDL_QueryTexture(wp, NULL, NULL, &iw, &ih);
    SDL_Rect sr = { 0, 0, iw, ih };
    if ((long)iw * H > (long)ih * W) { sr.w = ih * W / H; sr.x = (iw - sr.w) / 2; }
    else { sr.h = iw * H / W; sr.y = (ih - sr.h) / 2; }
    SDL_Rect dr = { 0, 0, W, H };
    SDL_RenderCopy(S.r, wp, &sr, &dr);
    SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
    for (int x = 0; x < W; x += 4) {
      float k = 1.0f - (float)x / (W * 0.75f);
      set_color(0x000000, 60 + (int)(130 * (k > 0 ? k : 0)));
      SDL_Rect r = { x, 0, 4, H };
      SDL_RenderFillRect(S.r, &r);
    }
    SDL_SetRenderTarget(S.r, prev);
    return;
  }
  /* rubans : frequences entieres, le motif se repete tous les W pixels */
  static const struct { float y, a1, f1, p1, a2, f2, p2, th, ft, pt; int al; } RB[4] = {
    { 640, 70, 1, 0.0f, 28, 3, 1.3f, 80, 2, 0.4f, 14 },
    { 665, 55, 2, 2.1f, 20, 1, 0.2f, 44, 1, 2.0f, 18 },
    { 700, 90, 1, 4.0f, 18, 2, 3.1f, 120, 3, 1.1f, 9 },
    { 615, 40, 3, 1.0f, 30, 1, 5.0f, 26, 2, 3.3f, 24 },
  };
  uint32_t lc = mix_color(ac, 0xffffff, 0.35f);
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_ADD);
  for (int i = 0; i < 4; i++)
    for (int x = 0; x < W * 2; x += 2) {
      float t = x * 6.2831853f / W;
      float cy = RB[i].y + RB[i].a1 * sinf(t * RB[i].f1 + RB[i].p1) + RB[i].a2 * sinf(t * RB[i].f2 + RB[i].p2);
      float th = RB[i].th * (0.55f + 0.45f * sinf(t * RB[i].ft + RB[i].pt));
      set_color(lc, RB[i].al);
      SDL_Rect r = { x, (int)(cy - th / 2), 2, (int)th + 1 };
      SDL_RenderFillRect(S.r, &r);
      set_color(0xffffff, RB[i].al * 3);         /* bords plus brillants */
      SDL_Rect e1 = { x, (int)(cy - th / 2), 2, 2 }, e2 = { x, (int)(cy + th / 2) - 1, 2, 2 };
      SDL_RenderFillRect(S.r, &e1);
      SDL_RenderFillRect(S.r, &e2);
    }
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(S.r, prev);
}

static void xmb_glow(int cx, int cy, int rw, int rh, uint32_t c, int a) {
  a = a * S.th->glow / 100;                      /* reglage « Halos lumineux » */
  if (!S.glow || a <= 0) return;
  SDL_SetTextureColorMod(S.glow, (c >> 16) & 255, (c >> 8) & 255, c & 255);
  SDL_SetTextureBlendMode(S.glow, SDL_BLENDMODE_ADD);
  SDL_SetTextureAlphaMod(S.glow, a);
  SDL_Rect d = { cx - rw, cy - rh, rw * 2, rh * 2 };
  SDL_RenderCopy(S.r, S.glow, NULL, &d);
  SDL_SetTextureAlphaMod(S.glow, 255);
}

static void draw_xmb(Entry *e) {
  static const char *const ICON[SEC_COUNT] = { "▤", "★", "PS5", "PS4", "▶", "HB", "EMU", "ELF", "↑", "◷", "⚙" };
  static float wave, colx = -1, fade = 1;
  static int last_sec = -1, last_depth = -1;
  const int CX = 440, ROW = 262, XGAP = 250;      /* colonne choisie, ligne des categories */
  uint32_t ac = S.base->accent, ac2 = S.base->accent2;
  (void)e;
  if (S.th->bg_mode == BG_ART) draw_background(e);   /* illustration du jeu : fond habituel du launcher */
  else xmb_build_bg();
  if (S.th->bg_mode == BG_ART) {
  } else if (xmb_bg_tex) {
    wave = xmb_image ? 0 : fmodf(wave + S.dt * 28.0f, (float)W);
    SDL_Rect sr = { (int)wave, 0, W, H };
    SDL_SetTextureBlendMode(xmb_bg_tex, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(S.r, xmb_bg_tex, &sr, NULL);
  } else {
    set_color(S.base->top, 255);
    SDL_RenderClear(S.r);
  }
  draw_fx();                                     /* reglages du theme : effet anime et particules */
  if (S.th->bg_mode != BG_ART) draw_particles(S.lite);

  /* categories */
  int vis[SEC_COUNT], nv = 0, ci = 0;
  for (int k = 0; k < SEC_COUNT; k++) {
    int s = SEC_ORDER[k];
    if (!S.sec_has[s]) continue;
    if (s == S.sec) ci = nv;
    vis[nv++] = s;
  }
  if (colx < 0) colx = (float)ci;
  colx += ((float)ci - colx) * ease(0.2f);
  if (fabsf(ci - colx) < 0.002f) colx = (float)ci;
  if (S.sec != last_sec || S.depth != last_depth) { last_sec = S.sec; last_depth = S.depth; fade = 0; }
  fade += S.dt * 5.0f;
  if (fade > 1) fade = 1;
  for (int j = 0; j < nv; j++) {
    float x = CX + (j - colx) * XGAP;
    if (x < -150 || x > W + 150) continue;
    int s = vis[j], glyph = (unsigned char)ICON[s][0] >= 0x80, on = s == S.sec;
    float d = fabsf(j - colx), k = d < 1 ? 1 - d : 0;
    TTF_Font *f = glyph ? (k > 0.5f ? S.f84 : S.f48) : (k > 0.5f ? S.f48 : S.f30);
    if (on) xmb_glow((int)x, ROW, 120, 90, ac, (int)(150 * k));
    draw_text_a(f, ICON[s], (int)x, ROW - TTF_FontHeight(f) / 2, 0xffffff, 1, 0, on ? 255 : 150);
  }
  draw_text_a(S.f30, SEC_NAMES[S.sec], CX + (int)((ci - colx) * XGAP), ROW + 58, 0xffffff, 1, 420, (int)(255 * fade));

  /* contenus de la categorie, a la verticale */
  int sel = *cur_sel();
  S.scroll += ((float)sel - S.scroll) * ease(0.2f);
  if (fabsf(sel - S.scroll) < 0.002f) S.scroll = (float)sel;
  int sw = BOXES ? 96 : 124, bw = BOXES ? 172 : 214;   /* petite et grande largeur */
  int sh = TH(sw), bh = TH(bw), P = sh + 22, y0 = ROW + 125, tx = CX + bw / 2 + 40;
  float yup = ROW - 95 - sh;                     /* l'element precedent passe au-dessus des categories */
  int slide = (int)((1 - fade) * 60);
  for (int i = (int)S.scroll - 2; i <= (int)S.scroll + 8; i++) {
    if (i < 0 || i >= S.nentries) continue;
    Entry *en = &S.entries[i];
    float rel = i - S.scroll, k = 1 - fabsf(rel);
    if (k < 0) k = 0;
    if (rel < -1.6f) continue;
    int w = (int)(sw + (bw - sw) * k), h = TH(w);
    float y = rel >= 0 ? y0 + rel * P + (rel > 1 ? 1 : rel) * (bh - sh)
            : rel >= -1 ? yup + (y0 - yup) * (rel + 1) : yup + (rel + 1) * P;
    if (y > H - 170) continue;                   /* l'aide du bas reste lisible */
    int alpha = (int)((rel >= 0 ? 200 + 55 * k : 110 + 145 * (rel + 1 > 0 ? rel + 1 : 0)) * fade);
    SDL_Rect d = { CX - w / 2, (int)y + slide, w, h };
    if (i == sel && k > 0.9f) xmb_glow(d.x + w / 2, d.y + h / 2, w, h * 2 / 3, ac, 90);
    SDL_Texture *t = en->is_folder ? folder_tex(en->node) : tile_tex(en->app, FLOW, TH(FLOW));
    if (t) {
      SDL_SetTextureAlphaMod(t, alpha);
      SDL_RenderCopy(S.r, t, NULL, &d);
      SDL_SetTextureAlphaMod(t, 255);
    }
    const char *nm = en->is_folder ? en->node->name : menu_display_name(&S.menu, en->app);
    if (i == sel) {
      if (S.mode == MODE_MOVE) outline_round(d.x - 5, d.y - 5, w + 10, h + 10, 10, 4, ac, 255);
      if (!en->is_folder && menu_is_fav(&S.menu, en->app->id)) fav_badge(d.x, d.y, w);
      int cy = d.y + h / 2;
      draw_text_a(S.f48, nm, tx, cy - 50, 0xffffff, 0, W - tx - 120, alpha);
      if (!en->is_folder) {
        int kw = draw_text_a(S.f26, KIND_LABEL[en->app->kind], tx + 2, cy + 14, mix_color(ac2, 0xffffff, 0.3f), 0, 0, alpha);
        char fb[48];
        fresh_pill(fresh_tag(en, fb, sizeof fb), tx + kw + 24, cy + 14, 34, alpha);
      }
    } else {
      draw_text_a(S.f30, nm, tx, d.y + h / 2 - 20, 0xd8d8d8, 0, W - tx - 120, alpha);
    }
  }
  if (!S.nentries)
    draw_text_a(S.f36, S.sec == SEC_HOME && S.depth ? "Ce dossier est vide" : "Rien à afficher ici", CX - 60, y0 + 40, 0xc8c8c8, 0, 1200, (int)(255 * fade));

  /* date et heure en haut a droite */
  if (S.th->clock != CLOCK_HIDDEN) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int hh = tm->tm_hour;
    if (S.th->clock == CLOCK_12) { hh %= 12; if (!hh) hh = 12; }
    char buf[32];
    snprintf(buf, sizeof buf, "%02d/%02d   %d:%02d", tm->tm_mday, tm->tm_mon + 1, hh, tm->tm_min);
    int tw = draw_text(S.f30, buf, W - 96, 54, 0xffffff, 2, 0);
    fill_round(W - 96 - tw - 30, 100, tw + 30, 2, 1, 0xffffff, 110);
  }

  /* aide en bas */
  char hint[240];
  if (S.mode == MODE_MOVE) snprintf(hint, sizeof hint, "%s", tr("▲▼  Déplacer      ✕  Poser"));
  else snprintf(hint, sizeof hint, "%s      %s", tr("▲▼  Choisir      ◀ ▶  Sections"),
                tr(S.sec == SEC_HOME ? (S.depth ? "✕  Ouvrir      ○  Retour      △  Options      □  Déplacer"
                                                : "✕  Ouvrir / Jouer      △  Options      □  Déplacer")
                                     : "✕  Jouer      △  Options      □  Filtrer"));
  draw_text_a(S.f22, hint, 96, H - 62, 0xffffff, 0, 0, 150);
  if (S.nentries) {
    char pos[32];
    snprintf(pos, sizeof pos, "%d / %d", sel + 1, S.nentries);
    draw_text_a(S.f22, pos, W - 96, H - 62, 0xffffff, 2, 0, 150);
  }
}

static void draw(void) {
  frame_theme(selected());
  refresh_settings();
  check_tile_shape();
  update_sections();
  static int opened, want_resume;                /* a l'ouverture : premiere section dans l'ordre */
  if (!opened || !S.sec_has[S.sec]) {
    if (!opened) want_resume = 1;
    opened = 1;
    int k = 0;
    while (k < SEC_COUNT - 1 && !S.sec_has[SEC_ORDER[k]]) k++;
    S.sec = SEC_ORDER[k];
    if (want_resume) {                           /* ... ou celle du dernier jeu lance, pret a reprendre */
      App *r = resume_app();
      int s = !r ? -1 : r->kind == KIND_PS5 ? SEC_PS5 : r->kind == KIND_PS4 ? SEC_PS4
            : r->kind == KIND_HOMEBREW ? SEC_HB : r->kind == KIND_MEDIA ? SEC_MEDIA : -1;
      if (s >= 0 && S.sec_has[s]) S.sec = s; else want_resume = 0;
    }
  }
  build_entries();
  {                                              /* premiere visite des Payloads : l'aide s'ouvre une fois */
    static int seen = -1;
    char mk[PATH_LEN];
    snprintf(mk, sizeof mk, "%s/cache/help_payloads_seen", plat_data_dir());
    if (seen < 0) seen = !access(mk, F_OK);
    if (!seen && S.sec == SEC_PAYLOAD && S.mode == MODE_BROWSE && !S.launching) {
      seen = 1;
      FILE *hf = fopen(mk, "w");
      if (hf) { fputs("1\n", hf); fclose(hf); }
      open_help(1);
    }
  }
  if (want_resume) {
    want_resume = 0;
    App *r = resume_app();
    for (int i = 0; i < S.nentries; i++)
      if (!S.entries[i].is_folder && S.entries[i].app == r) { *cur_sel() = i; S.scroll = (float)i; break; }
  }
  clamp_sel();
  Entry *e = selected();
  int key = (S.sec << 20) ^ (S.depth << 16) ^ *cur_sel();
  if (key != S.last_sel_key) { S.last_sel_key = key; S.hero_anim = 0; S.sel_since = SDL_GetTicks(); }
  frame_theme(e);

  if (S.saver_on) {
    draw_saver();
    if (S.saver_on) { SDL_RenderPresent(S.r); return; }
  }
  if (S.launching && SDL_GetTicks() - S.launch_t > 400) {
    draw_launching();
    draw_toast();
    SDL_RenderPresent(S.r);
    return;
  }
  if (GXMODE) {
    draw_gx(e);
    goto overlays;
  }
  if (XMBMODE) {
    draw_xmb(e);
    goto overlays;
  }
  draw_background(e);
  draw_fx();
  draw_topbar();
  {
    int tdx, tdy;
    if (tr_offset(&tdx, &tdy)) { SDL_Rect vp = { tdx, tdy, W, H }; SDL_RenderSetViewport(S.r, &vp); }
    switch (S.menu.view) {
    case VIEW_GRID: if (S.sec == SEC_SYS) draw_settings(); else draw_grid(); break;
    case VIEW_CLASSIC: if (S.sec == SEC_SYS) draw_settings(); else { draw_hero(e); draw_carousel(); } break;
    default: if (S.sec == SEC_SYS) draw_settings(); else { draw_flow(); draw_flow_info(e); } break;
    }
    SDL_RenderSetViewport(S.r, NULL);
  }
  draw_footer();
overlays:
  if (((filter_now && S.sec != SEC_HOME) || search_q[0]) && S.sec != SEC_SYS && S.mode != MODE_THEME) {
    char fl[200];
    if (search_q[0]) snprintf(fl, sizeof fl, tr("« %s »  ·  ○ effacer"), search_q);
    else snprintf(fl, sizeof fl, "□  %s", tr(FILTER_NAMES[filter_now]));
    int fw = text_w(S.f22, fl) + 34, fx = (GXMODE ? W - 60 : W - 96) - fw, fy = GXMODE ? 34 : H - 124;
    fill_round(fx, fy, fw, 38, 19, S.th->accent, 235);
    draw_text(S.f22, fl, fx + 17, fy + 6, 0x0a0a12, 0, 0);
  }
  if (S.mode == MODE_CONTEXT) draw_list_panel(S.ctx_title, S.nchoices, choice_label, S.csel, NULL);
  if (S.mode == MODE_CONTEXT && S.quick) draw_quick_info();
  if (S.mode == MODE_PICKFOLDER) draw_list_panel("Ranger dans…", S.nfolders, folder_label, S.fsel, NULL);
  if (S.mode == MODE_COLOR) draw_list_panel("Couleur du dossier", NPALETTE, color_label, S.color_sel, color_swatch);
  if (S.mode == MODE_INFO) draw_info();
  if (S.mode == MODE_HELP) draw_help();
  if (S.mode == MODE_CATALOG) draw_catalog();
  if (S.mode == MODE_THEME) draw_theme_editor();
  if (S.mode == MODE_KEYBOARD) draw_keyboard();
  draw_launching();
  draw_letter();
  draw_toast();
  draw_boot();
  SDL_RenderPresent(S.r);
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */
static void add_choice(const char *label, int action) {
  if (S.nchoices >= 16) return;
  snprintf(S.choices[S.nchoices].label, sizeof S.choices[0].label, "%s", label);
  S.choices[S.nchoices++].action = action;
}

/* ------------------------------------------------------------------ */
/* Payloads au demarrage (comme l'autoload de Payload Manager) : envoyes */
/* dans l'ordre, une seule fois par allumage de la console. OnionHEN    */
/* n'est pas renvoye s'il tourne deja (Launcher5 lance comme plugin).   */
/* ------------------------------------------------------------------ */
#define BOOT_MAX 16
static char BOOTL[BOOT_MAX][NAME_LEN];           /* noms des payloads (pas des fichiers : mises a jour suivies) */
static int nboot = -1;
static volatile int boot_sent, boot_fail, boot_finished;
typedef struct BootJob BootJob;
static BootJob *boot_job;                 /* en attente (compte a rebours) */
static Uint32 boot_at;
static int boot_cancel;
static int start_mode(void);

static void boot_file(char *o, size_t n) { snprintf(o, n, "%s/autostart.json", plat_data_dir()); }

/* fichier a envoyer pour ce nom : celui affiche dans la section (la copie de Launcher5
   si le catalogue l'a installee ou mise a jour, sinon celle d'un autre gestionnaire) */
static App *boot_resolve(const char *name) {
  App *other = NULL;
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    if (a->kind != KIND_PAYLOAD || strcasecmp(a->title, name)) continue;
    if (pay_ours(a)) return a;
    if (!other && !pay_shadowed(a)) other = a;
  }
  return other;
}

static void boot_load(void) {
  nboot = 0;
  char p[PATH_LEN];
  boot_file(p, sizeof p);
  json_t *j = json_load_file(p, 0, NULL);
  size_t i; json_t *v;
  json_t *names = json_object_get(j, "names");
  if (json_is_array(names)) {
    json_array_foreach(names, i, v) {
      const char *nm = json_string_value(v);
      if (nm && *nm && nboot < BOOT_MAX) snprintf(BOOTL[nboot++], NAME_LEN, "%s", nm);
    }
  } else {                                       /* ancienne liste (identifiants de fichiers) */
    json_array_foreach(json_object_get(j, "payloads"), i, v) {
      const char *id = json_string_value(v);
      App *a = id ? menu_find_app(&S.menu, id) : NULL;
      if (a && nboot < BOOT_MAX) snprintf(BOOTL[nboot++], NAME_LEN, "%s", a->title);
    }
  }
  json_decref(j);
}

/* noms, et fichiers a envoyer au prochain demarrage (lus par le bootstrap) */
static void boot_save(void) {
  json_t *arr = json_array(), *paths = json_array();
  for (int i = 0; i < nboot; i++) {
    json_array_append_new(arr, json_string(BOOTL[i]));
    App *a = boot_resolve(BOOTL[i]);
    if (a && a->path[0]) json_array_append_new(paths, json_string(a->path));
  }
  json_t *j = json_pack("{s:o, s:o}", "names", arr, "paths", paths);
  char p[PATH_LEN], t[PATH_LEN + 8];
  boot_file(p, sizeof p);
  snprintf(t, sizeof t, "%s.tmp", p);
  if (j && !json_dump_file(j, t, JSON_INDENT(2))) rename(t, p);
  json_decref(j);
}

static int boot_index(const App *a) {
  if (nboot < 0) boot_load();
  if (a->kind != KIND_PAYLOAD) return -1;
  for (int i = 0; i < nboot; i++) if (!strcasecmp(BOOTL[i], a->title)) return i;
  return -1;
}

/* fichiers recalcules (mise a jour d'un payload, par le catalogue ou un autre gestionnaire) */
static void boot_refresh(void) {
  if (nboot < 0) boot_load();
  if (nboot > 0) boot_save();
}

static void boot_toggle(App *a) {
  int k = boot_index(a);
  char msg[160];
  if (k >= 0) {
    memmove(BOOTL[k], BOOTL[k + 1], (size_t)(nboot - k - 1) * NAME_LEN);
    nboot--;
    snprintf(msg, sizeof msg, tr("« %s » ne sera plus envoyé au démarrage"), menu_display_name(&S.menu, a));
  } else {
    if (pay_incompatible(a)) { sfx(SFX_ERROR); toast("Incompatible avec Launcher5 : demande le mode repos"); return; }
    if (nboot >= BOOT_MAX) { toast("Liste de démarrage pleine"); return; }
    snprintf(BOOTL[nboot++], NAME_LEN, "%s", a->title);
    snprintf(msg, sizeof msg, tr("« %s » sera envoyé au démarrage (position %d)"), menu_display_name(&S.menu, a), nboot);
  }
  boot_save();
  toast(msg);
}


static int onion_running(void) {                 /* /system_tmp est vide a chaque allumage */
  char p[PATH_LEN];
  struct stat st;
  snprintf(p, sizeof p, "%s/system_tmp/onionhen", plat_root());
  return !stat(p, &st);
}

struct BootJob { int n; App apps[BOOT_MAX]; };

static int boot_thread(void *u) {
  BootJob *j = u;
  SDL_Delay(1500);                               /* le launcher finit de s'installer */
  for (int i = 0; i < j->n; i++) {
    char msg[200] = "";
    if (!plat_launch(&S.menu, &j->apps[i], msg, sizeof msg)) boot_sent++; else boot_fail++;
    plat_log("demarrage : %s -> %s", j->apps[i].path, msg[0] ? msg : "?");
    SDL_Delay(2000);                             /* chaque payload a le temps de s'installer */
  }
  free(j);
  boot_finished = 1;
  return 0;
}

__attribute__((unused)) static void boot_run(void) {   /* remplace par l'envoi du bootstrap */
  if (nboot < 0) boot_load();
  if (!nboot) return;
  char p[PATH_LEN];
  /* la liste n'a de sens que si Launcher5 fait partie de la chaine de demarrage (le chargeur
     ELF nomme pareil un lancement par l'autoloader et un envoi a la main : « payload.elf ») */
  if (start_mode() == 0) { plat_log("demarrage : mode manuel, liste non envoyee"); return; }
  /* une fois par allumage : marqueur dans /system_tmp, que la PS5 vide a chaque demarrage */
  snprintf(p, sizeof p, "%s/system_tmp/launcher5_boot_list", plat_root());
  if (!access(p, F_OK)) { plat_log("demarrage : payloads deja envoyes depuis l'allumage"); return; }
  FILE *f = fopen(p, "w");
  if (!f) { plat_log("demarrage : marqueur impossible dans /system_tmp, liste non envoyee"); return; }
  fprintf(f, "done\n");
  fclose(f);
  BootJob *j = calloc(1, sizeof *j);
  if (!j) return;
  int onion = onion_running();
  for (int i = 0; i < nboot; i++) {
    App *a = menu_find_app(&S.menu, BOOTL[i]);
    if (!a || a->kind != KIND_PAYLOAD) continue;
    char low[NAME_LEN];
    size_t k = 0;
    for (const char *s = a->title; *s && k + 1 < sizeof low; s++) low[k++] = (char)tolower((unsigned char)*s);
    low[k] = 0;
    if (onion && strstr(low, "onion")) { plat_log("demarrage : OnionHEN tourne deja, pas renvoye"); continue; }
    j->apps[j->n++] = *a;
  }
  if (!j->n) { free(j); return; }
  plat_log("demarrage : %d payload(s) a envoyer", j->n);
  boot_job = j;                                  /* envoi apres 5 s : ○ pour annuler */
  boot_at = SDL_GetTicks() + 5000;
}

static void boot_poll(void) {
  if (boot_job) {
    Uint32 now = SDL_GetTicks();
    if (boot_cancel) {
      free(boot_job); boot_job = NULL;
      plat_log("demarrage : envoi annule");
      toast("Payloads au démarrage annulés");
    } else if ((Sint32)(now - boot_at) >= 0) {
      SDL_Thread *t = SDL_CreateThread(boot_thread, "l5boot", boot_job);
      if (t) SDL_DetachThread(t); else free(boot_job);
      boot_job = NULL;
    } else {
      static int shown = -1;
      int left = (int)((boot_at - now + 999) / 1000);
      if (left != shown) {
        shown = left;
        char msg[160];
        snprintf(msg, sizeof msg, tr("Payloads au démarrage dans %d s  ·  ○ pour annuler"), left);
        toast(msg);
      }
    }
    return;
  }
  if (boot_finished != 1) return;
  boot_finished = 2;
  char msg[160];
  if (boot_fail) snprintf(msg, sizeof msg, tr("Démarrage : %d payload(s) envoyé(s), %d en échec"), boot_sent, boot_fail);
  else snprintf(msg, sizeof msg, tr("Démarrage : %d payload(s) envoyé(s)"), boot_sent);
  toast(msg);
}


/* ------------------------------------------------------------------ */
/* Aide : commandes de la manette (Parametres) et payloads (section)   */
/* La manette est un schema simple dessine ici, pas une image.         */
/* ------------------------------------------------------------------ */
static int help_kind;                            /* 0 : manette, 1 : payloads */

static void open_help(int kind) { help_kind = kind; S.mode = MODE_HELP; }

static void pad_button(int cx, int cy, int r, const char *g, uint32_t c) {
  fill_round(cx - r, cy - r, r * 2, r * 2, r, 0x1b2130, 255);
  outline_round(cx - r, cy - r, r * 2, r * 2, r, 2, c, 230);
  draw_text(S.f22, g, cx, cy - 14, c, 1, 0);
}

static void draw_pad(int x, int y) {            /* manette schematique : 560 x 330 */
  uint32_t body = 0x262d3d, edge = 0x4a5468, ac = S.th->accent;
  fill_round(x + 60, y + 40, 440, 200, 90, body, 255);       /* corps */
  fill_round(x, y + 110, 190, 220, 95, body, 255);           /* poignees */
  fill_round(x + 370, y + 110, 190, 220, 95, body, 255);
  outline_round(x + 60, y + 40, 440, 200, 90, 2, edge, 255);
  fill_round(x + 95, y, 120, 26, 12, body, 255);             /* L1 / R1 */
  fill_round(x + 345, y, 120, 26, 12, body, 255);
  draw_text(S.f22, "L1", x + 155, y + 1, 0xffffff, 1, 0);
  draw_text(S.f22, "R1", x + 405, y + 1, 0xffffff, 1, 0);
  draw_text(S.f22, "L2", x + 155, y - 34, 0x9aa4b5, 1, 0);
  draw_text(S.f22, "R2", x + 405, y - 34, 0x9aa4b5, 1, 0);
  fill_round(x + 205, y + 62, 150, 80, 14, 0x1b2130, 255);   /* pave tactile */
  outline_round(x + 205, y + 62, 150, 80, 14, 2, edge, 255);
  draw_text(S.f22, "TOUCH", x + 280, y + 88, 0x9aa4b5, 1, 0);
  fill_round(x + 368, y + 62, 34, 14, 7, edge, 255);         /* OPTIONS */
  draw_text(S.f22, "OPTIONS", x + 385, y + 36, 0x9aa4b5, 1, 0);
  int dx = x + 140, dy = y + 130;                            /* croix */
  fill_round(dx - 14, dy - 44, 28, 88, 6, 0x1b2130, 255);
  fill_round(dx - 44, dy - 14, 88, 28, 6, 0x1b2130, 255);
  int fx = x + 420, fy = y + 130;                            /* boutons d'action */
  pad_button(fx, fy - 40, 22, "△", ac);
  pad_button(fx - 40, fy, 22, "□", ac);
  pad_button(fx + 40, fy, 22, "○", ac);
  pad_button(fx, fy + 40, 22, "✕", ac);
  pad_button(x + 210, y + 210, 30, "L3", 0x9aa4b5);          /* sticks */
  pad_button(x + 350, y + 210, 30, "R3", 0x9aa4b5);
  fill_round(x + 266, y + 156, 28, 28, 14, 0x1b2130, 255);   /* PS */
  draw_text(S.f22, "PS", x + 280, y + 158, 0x9aa4b5, 1, 0);
}

static void draw_help(void) {
  int px = 90, py = 60, pw = W - 180, ph = H - 120;
  glass_panel(px, py, pw, ph);
  draw_text(S.f22, "○  Fermer", px + 52, py + ph - 56, 0x9aa4b5, 0, 0);
  if (help_kind == 2) { draw_status(px, py, pw, ph); return; }
  if (help_kind == 1)
    draw_text(S.f22, "Retrouve cette aide : △ sur un payload, ou OPTIONS", px + pw - 52, py + ph - 56, 0x9aa4b5, 2, 0);
  if (help_kind == 0) {
    static const char *const K[][2] = {
      { "✕", "Jouer · envoyer · ouvrir" },
      { "○", "Retour · effacer la recherche" },
      { "△", "Options : infos, favoris, démarrage…" },
      { "□", "Filtre (Mes dossiers : déplacer)" },
      { "L1 / R1", "Changer de section" },
      { "L2 / R2", "Saut par lettre" },
      { "L3 / R3", "Rechercher" },
      { "Pavé tactile", "Vue : Cover Flow, Grille, Carrousel" },
      { "OPTIONS", "Menu rapide : reprendre, catalogue, relancer…" },
      { "Croix", "Naviguer" },
      { "PS", "Menu de la PS5" },
    };
    draw_text(S.f48, "Commandes de la manette", px + 50, py + 34, 0xffffff, 0, pw - 100);
    draw_pad(px + 70, py + 220);
    int lx = px + 720, ly = py + 140;
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) {
      int y = ly + (int)i * 62;
      fill_round(lx, y, 190, 46, 23, 0x1b2130, 255);
      draw_text(S.f26, tr(K[i][0]), lx + 95, y + 8, S.th->accent, 1, 180);
      draw_text(S.f26, tr(K[i][1]), lx + 214, y + 8, 0xffffff, 0, px + pw - lx - 260);
    }
    return;
  }
  static const char *const STEP[][2] = {
    { "Ajouter des payloads", "OPTIONS › Catalogue de payloads, ou copie tes .elf dans /data/payloads (FTP, Forge5). Une image du même nom devient son icône." },
    { "Les lancer à chaque allumage", "Déjà en place : kstuff-lite et ShadowMountPlus. Pour en ajouter : △ sur un payload › Envoyer au démarrage." },
    { "Démarrer avec le jailbreak", "Paramètres › Démarrage de Launcher5 › Autoloader ou Plugin OnionHEN. Envoi une fois par allumage, avant le launcher." },
    { "Payloads noyau  ⚠", "kstuff, etaHEN… : seulement au démarrage. Les envoyer depuis le launcher fait planter la console." },
    { "Les autres payloads", "✕ pour les envoyer quand tu veux (FTP, outils…). « ● ACTIF » quand le service tourne." },
    { "Mises à jour", "Le catalogue affiche « Mise à jour ». La liste de démarrage suit la nouvelle version toute seule." },
    { "Incompatibles", "Byepervisor, HyperHEN… : demandent le mode repos, incompatibles avec Launcher5." },
  };
  draw_text(S.f48, "Payloads et démarrage", px + 50, py + 34, 0xffffff, 0, pw - 100);
  for (size_t i = 0; i < sizeof STEP / sizeof STEP[0]; i++) {
    int step = (ph - 230) / (int)(sizeof STEP / sizeof STEP[0]);   /* tout tient au-dessus du bas */
    if (step > 118) step = 118;
    int y = py + 130 + (int)i * step, x = px + 60;
    char n[8];
    snprintf(n, sizeof n, "%d", (int)i + 1);
    fill_round(x, y, 56, 56, 28, S.th->accent, 255);
    draw_text(S.f30, n, x + 28, y + 10, 0x0a0a12, 1, 0);
    draw_text(S.f30, tr(STEP[i][0]), x + 84, y + 2, 0xffffff, 0, pw - 200);
    draw_text(S.f22, tr(STEP[i][1]), x + 84, y + 46, 0xc9d1dc, 0, pw - 200);
  }
}

/* ------------------------------------------------------------------ */
/* Fiche du jeu (△ › Infos) : temps de jeu, sessions, taille, tags      */
/* ------------------------------------------------------------------ */
static App *info_app;
static SDL_atomic_t info_gen;                    /* fiche en cours (le calcul de taille s'y rapporte) */
static volatile long long info_bytes;
static volatile int info_bytes_gen = -1;
typedef struct { char path[PATH_LEN]; int gen; } SizeJob;

static long long dir_size(const char *p, int depth) {
  struct stat sb;
  if (lstat(p, &sb)) return 0;
  if (!S_ISDIR(sb.st_mode)) return (long long)sb.st_size;
  if (depth > 12) return 0;
  DIR *d = opendir(p);
  if (!d) return 0;
  long long t = 0;
  struct dirent *de;
  char sub[PATH_LEN];
  while ((de = readdir(d))) {
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
    snprintf(sub, sizeof sub, "%s/%s", p, de->d_name);
    t += dir_size(sub, depth + 1);
  }
  closedir(d);
  return t;
}

static int size_thread(void *u) {                /* taille calculee a part : l'ecran reste fluide */
  SizeJob *j = u;
  long long b = dir_size(j->path, 0);
  if (SDL_AtomicGet(&info_gen) == j->gen) { info_bytes = b; info_bytes_gen = j->gen; }
  free(j);
  return 0;
}

static void open_info(App *a) {
  info_app = a;
  int g = SDL_AtomicAdd(&info_gen, 1) + 1;
  info_bytes_gen = -1;
  char dir[PATH_LEN];
  app_dir(a, dir, sizeof dir);
  if (dir[0]) {
    SizeJob *j = malloc(sizeof *j);
    if (j) {
      snprintf(j->path, sizeof j->path, "%s", dir);
      j->gen = g;
      SDL_Thread *t = SDL_CreateThread(size_thread, "l5size", j);
      if (t) SDL_DetachThread(t); else free(j);
    }
  } else info_bytes_gen = g, info_bytes = 0;
  S.mode = MODE_INFO;
}

static void draw_info(void) {
  App *a = info_app;
  if (!a) return;
  int pw = 1300, ph = 760, px = (W - pw) / 2, py = (H - ph) / 2;
  glass_panel(px, py, pw, ph);
  int cw = 340, ch = TH(cw), cx = px + 60, cy = py + 70;
  if (ch > ph - 140) { ch = ph - 140; cw = BOXES ? ch * 2 / 3 : ch; }
  SDL_Texture *t = tile_tex(a, FLOW, TH(FLOW));
  if (t) { SDL_Rect d = { cx, cy, cw, ch }; SDL_RenderCopy(S.r, t, NULL, &d); }
  int tx = cx + cw + 60, tw = px + pw - 60 - tx, y = py + 60;
  draw_text(S.f48, menu_display_name(&S.menu, a), tx, y, 0xffffff, 0, tw);
  y += 84;
  const char *kind = KIND_LABEL[a->kind];
  int kw = text_w(S.f22, kind);
  fill_round(tx, y, kw + 30, 36, 18, S.th->accent, 230);
  draw_text(S.f22, kind, tx + 15, y + 5, 0x0a0a12, 0, 0);
  char fb[48];
  fresh_pill(fresh_tag(&(Entry){ 0, -1, a, NULL }, fb, sizeof fb), tx + kw + 42, y, 36, 255);
  y += 66;

  Stat *st = menu_stat(&S.menu, a->id, 0);
  char v[7][96];
  const PayInfo *pi = pay_info(a);
  int pay = a->kind == KIND_PAYLOAD;
  if (pay) snprintf(v[0], 96, "%s", a->cat[0] ? a->cat : pi ? tr(pi->cat) : tr("Payload ELF"));
  else if (st && st->playtime > 0) playtime_str(st->playtime, v[0], 96); else snprintf(v[0], 96, "—");
  if (st && st->last) ago(st->last, v[1], 96); else snprintf(v[1], 96, "%s", tr("Jamais"));
  snprintf(v[2], 96, "%d", st ? st->launches : 0);
  if (info_bytes_gen != SDL_AtomicGet(&info_gen)) snprintf(v[3], 96, "%s", tr("Calcul…"));
  else if (info_bytes <= 0) snprintf(v[3], 96, "—");
  else if (info_bytes >= 1024LL * 1024 * 1024) snprintf(v[3], 96, tr("%.1f Go"), info_bytes / (1024.0 * 1024 * 1024));
  else snprintf(v[3], 96, tr("%.0f Mo"), info_bytes / (1024.0 * 1024) < 1 ? 1.0 : info_bytes / (1024.0 * 1024));
  time_t it = app_installed(a);
  if (it) ago(it, v[4], 96); else snprintf(v[4], 96, "—");
  v[5][0] = 0;
  for (int f = FIL_NEW; f < FIL_COUNT; f++)
    if (app_tag(a, f)) {
      if (v[5][0]) strncat(v[5], "  ·  ", 95 - strlen(v[5]));
      strncat(v[5], tr(FILTER_NAMES[f]), 95 - strlen(v[5]));
    }
  if (!v[5][0]) snprintf(v[5], 96, "—");
  snprintf(v[6], 96, "%s", a->id);
  if (pay) {
    const char *desc = a->desc[0] ? a->desc : pi ? tr(pi->desc) : "—";
    if (pi && pi->probe) snprintf(v[5], 96, tr("%s  ·  port %d  ·  %s"), desc, pi->port, tr(pay_active(a) ? "actif" : "inactif"));
    else if (pi && pi->port) snprintf(v[5], 96, tr("%s  ·  port %d"), desc, pi->port);
    else snprintf(v[5], 96, "%s", desc);
    snprintf(v[4], 96, "%s", a->ver[0] ? a->ver : "—");
    snprintf(v[6], 96, "%s", a->path);
  }
  static const char *const KEYS_G[7] = { "Temps de jeu", "Dernière session", "Lancements", "Taille", "Installé", "Tags", "Identifiant" };
  static const char *const KEYS_P[7] = { "Catégorie", "Dernier envoi", "Envois", "Taille", "Version", "Description", "Fichier" };
  const char *const *KEYS = pay ? KEYS_P : KEYS_G;
  for (int i = 0; i < 7; i++) {
    int col = i % 2, row = i / 2, x = tx + col * (tw / 2), yy = y + row * 84;
    if (i == 5) { x = tx; yy = y + 3 * 84; }   /* tags : toute la largeur */
    if (i == 6) { x = tx; yy = y + 4 * 84; }
    draw_text(S.f22, KEYS[i], x, yy, 0x9aa4b5, 0, tw / 2 - 20);
    draw_text(S.f30, v[i], x, yy + 30, i == 6 ? 0x9aa4b5 : 0xffffff, 0, i >= 5 ? tw : tw / 2 - 20);
  }
  draw_text(S.f22, a->kind == KIND_PAYLOAD ? "✕  Envoyer      ○  Fermer" : "✕  Jouer      ○  Fermer", tx, py + ph - 64, 0x9aa4b5, 0, 0);
}

static void open_context(void) {
  Entry *e = selected();
  S.nchoices = 0; S.csel = 0;
  if (e && e->is_folder) {
    snprintf(S.ctx_title, sizeof S.ctx_title, tr("Dossier « %s »"), e->node->name);
    add_choice("Ouvrir", ACT_OPEN);
    add_choice("Renommer", ACT_RENAME);
    add_choice("Changer la couleur", ACT_COLOR);
    add_choice("Déplacer", ACT_MOVE);
    add_choice("Ranger dans un autre dossier…", ACT_TO_FOLDER);
    if (S.depth) add_choice("Sortir du dossier", ACT_OUT_OF_FOLDER);
    add_choice("Supprimer le dossier (garde son contenu)", ACT_DELETE_FOLDER);
  } else if (e) {
    snprintf(S.ctx_title, sizeof S.ctx_title, "%s", menu_display_name(&S.menu, e->app));
    add_choice(e->app->kind == KIND_PAYLOAD ? "Envoyer" : "Jouer", ACT_LAUNCH);
    if (e->app->kind != KIND_SYSTEM) add_choice("Infos", ACT_INFO);
    if (S.sec == SEC_HOME) {
      add_choice("Ranger dans un dossier…", ACT_TO_FOLDER);
      add_choice("Déplacer", ACT_MOVE);
      if (S.depth) add_choice("Sortir du dossier", ACT_OUT_OF_FOLDER);
    }
    add_choice("Renommer", ACT_RENAME);
    if (strcmp(menu_display_name(&S.menu, e->app), e->app->title)) add_choice("Remettre le nom d'origine", ACT_RESET_NAME);
    if (e->app->kind != KIND_SYSTEM) {
      int fav = menu_is_fav(&S.menu, e->app->id);
      add_choice(fav ? "★ Retirer des favoris" : "★ Ajouter aux favoris", fav ? ACT_UNFAV : ACT_FAV);
    }
    if (e->app->kind == KIND_PAYLOAD) {
      if (boot_index(e->app) >= 0 || !pay_incompatible(e->app))
        add_choice(boot_index(e->app) >= 0 ? "Ne plus envoyer au démarrage" : "Envoyer au démarrage", ACT_BOOT);
      add_choice("Aide : payloads et démarrage", ACT_HELP_PAY);
    }
    int hid = menu_is_hidden(&S.menu, e->app->id);
    add_choice(hid ? "Ne plus masquer" : "Masquer", hid ? ACT_SHOW : ACT_HIDE);
  } else {
    snprintf(S.ctx_title, sizeof S.ctx_title, "Options");
  }
  if (S.sec == SEC_HOME) add_choice("+ Nouveau dossier ici", ACT_NEW_FOLDER);
  add_choice("Rechercher…", ACT_SEARCH);
  if (S.sec == SEC_PAYLOAD) add_choice("Catalogue de payloads…", ACT_CATALOG);
  {
    char v[64];
    snprintf(v, sizeof v, tr("Affichage : %s  ›  %s"), tr(VIEW_NAMES[S.menu.view]), tr(VIEW_NAMES[(S.menu.view + 1) % VIEW_COUNT]));
    add_choice(v, ACT_VIEW);
  }
  add_choice("Personnaliser le thème…", ACT_THEME);
  add_choice(S.show_hidden ? "Cacher les éléments masqués" : "Voir les éléments masqués", ACT_TOGGLE_HIDDEN);
  S.mode = MODE_CONTEXT;
}

static void open_keyboard(const char *title, const char *initial, int action) {
  snprintf(S.kb_title, sizeof S.kb_title, "%s", title);
  snprintf(S.kb_buf, sizeof S.kb_buf, "%s", initial);
  S.kb_row = 0; S.kb_col = 0; S.kb_shift = 0; S.kb_action = action;
  S.mode = MODE_KEYBOARD;
  SDL_StartTextInput();
}

static void collect_folders(Node *n, const char *prefix, Node *exclude) {
  for (int i = 0; i < n->count; i++) {
    Node *c = &n->items[i];
    if (!c->is_folder || c == exclude) continue;
    S.folders = realloc(S.folders, sizeof(FolderChoice) * (S.nfolders + 1));
    FolderChoice *fc = &S.folders[S.nfolders++];
    fc->folder = c;
    snprintf(fc->label, sizeof fc->label, "%s%s", prefix, c->name);
    char p[128];
    snprintf(p, sizeof p, "%s%s › ", prefix, c->name);
    collect_folders(c, p, exclude);
  }
}

static void open_pick_folder(void) {
  Entry *e = selected();
  if (!e) return;
  free(S.folders);
  S.folders = malloc(sizeof(FolderChoice));
  S.folders[0].folder = NULL;
  snprintf(S.folders[0].label, sizeof S.folders[0].label, "+ Nouveau dossier…");
  S.nfolders = 1;
  collect_folders(&S.menu.root, "", e->is_folder ? e->node : NULL);
  S.fsel = 0;
  S.mode = MODE_PICKFOLDER;
}

static void move_selected_to(Node *dest_or_null) {
  Entry *e = selected();
  if (!e || S.sec != SEC_HOME) return;
  Node *parent = current_node();
  if (dest_or_null) dest_or_null->mark = 1;
  Node moved;
  node_remove(parent, e->index, &moved);
  Node *dest = NULL, *stack[256];
  int sp = 0;
  stack[sp++] = &S.menu.root;
  while (sp && dest_or_null) {
    Node *n = stack[--sp];
    if (n->mark) { dest = n; break; }
    for (int i = 0; i < n->count && sp < 256; i++) if (n->items[i].is_folder) stack[sp++] = &n->items[i];
  }
  if (!dest) dest = &S.menu.root;
  dest->mark = 0;
  node_insert(dest, -1, &moved);
  save();
}

/* Titres des reglages (affichent leur valeur) */
static void set_title(const char *id, const char *title) {
  App *a = menu_find_app(&S.menu, id);
  if (!a || !strcmp(a->title, title)) return;
  snprintf(a->title, sizeof a->title, "%s", title);
  drop_tiles(a);   /* la tuile mise en cache doit etre redessinee */
}

static void set_view(int v) {
  transition(0, 60);                             /* nouvelle vue : arrive par le bas */
  S.menu.view = ((v % VIEW_COUNT) + VIEW_COUNT) % VIEW_COUNT;
  S.scroll = (float)*cur_sel();
  S.grid_top = *cur_sel() / GRID_COLS; S.grid_scroll = (float)S.grid_top;
  S.sel_anim = 1;
  save();
  char msg[64];
  snprintf(msg, sizeof msg, tr("Affichage : %s"), tr(VIEW_NAMES[S.menu.view]));
  toast(msg);
}

static void refresh_settings(void) {
  char t[96];
  snprintf(t, sizeof t, tr("Thème : %s"), tr(S.th->name));
  set_title("cfg:theme", t);
  snprintf(t, sizeof t, tr("Affichage : %s"), tr(VIEW_NAMES[S.menu.view]));
  set_title("cfg:view", t);
  set_title("cfg:return", plat_auto_return(-1) ? "Retour auto après un jeu : oui" : "Retour auto après un jeu : non");
  set_title("cfg:folders", S.menu.show_folders ? "Section Mes dossiers : oui" : "Section Mes dossiers : non");
  set_title("cfg:hidden", S.show_hidden ? "Éléments masqués : visibles" : "Éléments masqués : cachés");
  if (S.menu.saver > 0) snprintf(t, sizeof t, tr("Économiseur d'écran : %d min"), S.menu.saver);
  else snprintf(t, sizeof t, "%s", tr("Économiseur d'écran : non"));
  set_title("cfg:saver", t);
}

static int launcher_setting(App *a) {
  if (strncmp(a->id, "cfg:", 4)) return 0;
  if (!strcmp(a->id, "cfg:theme")) open_theme_editor();
  else if (!strcmp(a->id, "cfg:view")) set_view(S.menu.view + 1);
  else if (!strcmp(a->id, "cfg:return")) plat_auto_return(!plat_auto_return(-1));
  else if (!strcmp(a->id, "cfg:folders")) { S.menu.show_folders = !S.menu.show_folders; save(); }
  else if (!strcmp(a->id, "cfg:hidden")) S.show_hidden = !S.show_hidden;
  else if (!strcmp(a->id, "cfg:saver")) {
    static const int V[] = { 0, 2, 5, 10, 30 };
    int i = 0;
    while (i < 5 && V[i] != S.menu.saver) i++;
    S.menu.saver = V[(i + 1) % 5];
    save();
  }
  refresh_settings();
  return 1;
}

/* Payload qui touche au noyau ou au systeme (kstuff, HEN, hyperviseur, heure) : un second
   envoi alors qu'il est deja actif, ou un firmware non pris en charge, peut planter la
   console. On demande une confirmation (✕ une seconde fois). */
static int payload_sensitive(const App *a) {
  if (a->kind != KIND_PAYLOAD) return 0;
  char t[NAME_LEN + 48];
  snprintf(t, sizeof t, "%s %s", a->title, a->cat);
  for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
  static const char *const KEYS[] = { "kstuff", "hen", "byepervisor", "hyper", "jailbreak", "system &", "date-time", "linux" };
  for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) if (strstr(t, KEYS[i])) return 1;
  return 0;
}

static App *kernel_pending;                       /* payload noyau en attente d'ajout au demarrage */

static void launch(App *a) {
  if (launcher_setting(a)) return;
  if (pay_incompatible(a)) { sfx(SFX_ERROR); toast("Incompatible avec Launcher5 : demande le mode repos"); return; }
  if (payload_sensitive(a)) {                    /* payload noyau : jamais depuis l'interface (plantage) */
    sfx(SFX_ERROR);
    if (boot_index(a) >= 0) { toast("Déjà au démarrage : il partira au prochain allumage de la console"); return; }
    kernel_pending = a;
    S.nchoices = 0; S.csel = 0; S.quick = 0;
    snprintf(S.ctx_title, sizeof S.ctx_title, "⚠ %s", tr("Payload noyau : seulement au démarrage"));
    add_choice("Ajouter au démarrage", ACT_K_BOOT);
    S.mode = MODE_CONTEXT;
    return;
  }
  char msg[200] = "";
  int rc = plat_launch(&S.menu, a, msg, sizeof msg);
  if (rc == 2) { S.running = 0; return; }        /* quitter vers l'accueil PS5 */
  if (!rc) {
    if (a->kind != KIND_SYSTEM) { menu_record_launch(&S.menu, a->id); save(); }
    /* les jeux et applications ferment le launcher : ecran de transition */
    if (a->kind != KIND_PAYLOAD && a->kind != KIND_SYSTEM) {
      S.launching = a; S.launch_t = SDL_GetTicks(); music_fade();
      sfx(SFX_LAUNCH);
      for (int i = 0; i < 4; i++) if (S.pads[i]) SDL_GameControllerRumble(S.pads[i], 0x3000, 0xC000, 180);
      return;
    }
    sfx(SFX_OK);
  } else sfx(SFX_ERROR);
  if (msg[0]) toast(msg);
}

/* fond et jaquette du lancement, prepares une seule fois : chaque image ne fait
   ensuite que des copies (le dessin logiciel de la PS5 reste fluide) */
static struct { App *app; SDL_Texture *bg, *box; int bw, bh; } LB;

static void launch_prepare(App *a, uint32_t c) {
  if (LB.bg) SDL_DestroyTexture(LB.bg);
  if (LB.box) SDL_DestroyTexture(LB.box);
  LB.bg = LB.box = NULL;
  LB.app = a;
  LB.bw = BOXES ? 405 : 420;
  LB.bh = BOXES ? LB.bw * 3 / 2 : LB.bw;
  SDL_Texture *prev = SDL_GetRenderTarget(S.r);
  int cx = W / 2, cy = H / 2 - 60;
  LB.bg = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
  if (LB.bg && !SDL_SetRenderTarget(S.r, LB.bg)) {
    SDL_SetRenderDrawColor(S.r, 0, 0, 0, 255);
    SDL_RenderClear(S.r);
    SDL_Texture *bl = a->bg[0] ? blurred(a) : NULL;
    if (bl) { SDL_SetTextureAlphaMod(bl, 150); SDL_RenderCopy(S.r, bl, NULL, NULL); SDL_SetTextureAlphaMod(bl, 255); }
    glow(cx, cy, (int)(220 + W * 0.75f), c, 180);   /* couleur du jeu */
    glow(cx, cy, (int)(LB.bh * 0.9f), c, 150);       /* halo derriere la boite */
  }
  SDL_Texture *t = tile_tex(a, FLOW, TH(FLOW));
  if (!t) t = cover(a);
  if (t && (LB.box = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, LB.bw, LB.bh)) &&
      !SDL_SetRenderTarget(S.r, LB.box)) {
    SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
    SDL_RenderClear(S.r);
    SDL_BlendMode bm;
    SDL_GetTextureBlendMode(t, &bm);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);   /* garde la transparence des coins */
    SDL_RenderCopy(S.r, t, NULL, NULL);
    SDL_SetTextureBlendMode(t, bm);
    SDL_SetTextureBlendMode(LB.box, SDL_BLENDMODE_BLEND);
  }
  SDL_SetRenderTarget(S.r, prev);
}

/* Animation de lancement : la jaquette arrive au centre avec un leger rebond,
   l'illustration et la couleur du jeu envahissent l'ecran, un reflet de lumiere
   balaie la boite, puis le nom monte avec une barre d'attente. */
static float clampf(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float out_cubic(float k) { k = 1 - clampf(k); return 1 - k * k * k; }
static float out_back(float k) {                 /* depasse un peu puis revient */
  k = clampf(k) - 1;
  return 1 + k * k * (2.7f * k + 1.7f);
}

static void draw_launching(void) {
  if (!S.launching) return;
  Uint32 el = SDL_GetTicks() - S.launch_t;
  const char *ft = getenv("L5_LAUNCH_T");        /* test : image figee a cet instant */
  if (ft) el = (Uint32)atoi(ft);
  if (el > 12000) { S.launching = NULL; toast("Le lancement prend du temps…"); return; }
  int e = (int)el;
  App *a = S.launching;
  cover(a);                                      /* calcule aussi la couleur du jeu */
  uint32_t c = a->has_vibe ? a->vibe : S.th->accent;
  if (LB.app != a || !LB.bg) launch_prepare(a, c);
  int cx = W / 2, cy = H / 2 - 60;

  /* 1. le menu s'efface sous le fond du jeu */
  if (LB.bg) {
    int fa = (int)(255 * out_cubic(e / 350.0f));
    SDL_SetTextureBlendMode(LB.bg, fa < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetTextureAlphaMod(LB.bg, fa);
    SDL_RenderCopy(S.r, LB.bg, NULL, NULL);
  } else {
    set_color(0x000000, 255);
    SDL_Rect full = { 0, 0, W, H };
    SDL_RenderFillRect(S.r, &full);
  }
  if (e > 180 && e < 700) {                      /* eclair quand la boite se pose */
    float f = (e - 180) / 520.0f;
    glow(cx, cy, (int)(260 + 340 * f), 0xffffff, (int)(120 * (1 - f) * (1 - f)));
  }

  /* 2. la jaquette arrive en grossissant, avec un petit rebond */
  float sc = 0.55f + 0.45f * out_back(e / 520.0f);
  int bw = (int)(LB.bw * sc + 0.5f), bh = (int)(LB.bh * sc + 0.5f);
  if (e >= 520) { bw = LB.bw; bh = LB.bh; }      /* posee : copie a taille reelle */
  SDL_Rect d = { cx - bw / 2, cy - bh / 2, bw, bh };
  int alpha = (int)(255 * clampf(e / 180.0f));
  if (LB.box) {
    SDL_SetTextureAlphaMod(LB.box, alpha);
    SDL_RenderCopy(S.r, LB.box, NULL, &d);
  } else fill_round(d.x, d.y, bw, bh, 24, shade(hash_color(a->id), 0.6f), alpha);
  /* 3. reflet de lumiere qui traverse la boite */
  if (e > 480 && e < 1180) {
    float f = (e - 480) / 700.0f;
    int sw = bw / 3, sx = d.x - sw + (int)((bw + sw * 2) * f);
    SDL_RenderSetClipRect(S.r, &d);
    SDL_SetTextureColorMod(S.glow, 255, 255, 255);
    SDL_SetTextureAlphaMod(S.glow, (int)(150 * sinf(f * 3.14159f)));
    SDL_Rect g = { sx - sw / 2, d.y - bh / 4, sw, bh + bh / 2 };
    SDL_RenderCopy(S.r, S.glow, NULL, &g);
    SDL_RenderSetClipRect(S.r, NULL);
  }
  int rad = tile_rad() * bw / TILE_SEL;
  outline_round(d.x - 2, d.y - 2, bw + 4, bh + 4, rad + 2, 2, 0xffffff, alpha * 120 / 255);

  /* 4. le nom monte, puis la barre d'attente */
  if (e > 320) {
    float tx = out_cubic((e - 320) / 450.0f);
    int ty = cy + LB.bh / 2 + 40 + (int)(30 * (1 - tx));
    draw_text_a(S.f48, menu_display_name(&S.menu, a), W / 2, ty, 0xffffff, 1, 1400, (int)(255 * tx));
    char dots[8] = "", line[40];
    for (int i = 0; i < e / 400 % 4; i++) strcat(dots, ".");
    snprintf(line, sizeof line, tr("Lancement%s"), dots);
    draw_text_a(S.f30, line, W / 2, ty + 78, c, 1, 0, (int)(230 * tx));
    if (e > 600) {                               /* barre d'attente : un trait qui glisse */
      int lw = 360, lx = W / 2 - lw / 2, ly = ty + 132, ba = (int)(200 * clampf((e - 600) / 300.0f));
      fill_round(lx, ly, lw, 4, 2, 0xffffff, ba / 5);
      float m = fmodf(e / 1100.0f, 1.0f);
      int seg = lw / 4, sx = lx - seg + (int)((lw + seg) * m);
      int x0 = sx < lx ? lx : sx, x1 = sx + seg > lx + lw ? lx + lw : sx + seg;
      if (x1 > x0) fill_round(x0, ly, x1 - x0, 4, 2, c, ba);
    }
  }
}

/* ------------------------------------------------------------------ */
/* Menu rapide (touche OPTIONS) : infos de la console, relance du      */
/* launcher, cache des jaquettes.                                       */
/* ------------------------------------------------------------------ */
static void open_quick(void) {
  S.nchoices = 0; S.csel = 0; S.quick = 1;
  snprintf(S.ctx_title, sizeof S.ctx_title, "%s", tr("Menu rapide"));
  App *r = resume_app();
  if (r) {
    char lbl[96];
    snprintf(lbl, sizeof lbl, "▶  %s : %s", tr("Reprendre"), menu_display_name(&S.menu, r));
    add_choice(lbl, ACT_Q_RESUME);
  }
  add_choice("Catalogue de payloads", ACT_Q_CATALOG);
  add_choice("Aide : payloads et démarrage", ACT_HELP_PAY);
  add_choice("Relancer le launcher", ACT_Q_RESTART);
  add_choice("Vider le cache des jaquettes", ACT_Q_CACHE);
  add_choice("Quitter vers l'accueil PS5", ACT_Q_QUIT);
  S.mode = MODE_CONTEXT;
  sfx(SFX_OK);
}

/* infos de la console, sous le menu */
static void draw_quick_info(void) {
  static Uint32 last; static char line[160];
  if (!last || SDL_GetTicks() - last > 2000) {   /* releve toutes les deux secondes */
    last = SDL_GetTicks();
    char ip[64] = ""; long long mb = -1; int temp = -1;
    plat_sysinfo(ip, sizeof ip, &mb, &temp);
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long up = (long)ts.tv_sec / 60;
    char t[24] = "", f[32] = "", u[48];
    if (temp > 0) snprintf(t, sizeof t, "%d °C  ·  ", temp);
    if (mb >= 0) snprintf(f, sizeof f, tr("%lld Go libres"), mb / 1024);
    snprintf(u, sizeof u, tr("allumée depuis %ld h %02ld"), up / 60, up % 60);
    snprintf(line, sizeof line, "%s%s  ·  %s%s%s", t, f, u, S.menu.hide_ip || !ip[0] ? "" : "  ·  ", S.menu.hide_ip ? "" : ip);
  }
  int ph = 120 + S.nchoices * 66, py = (H - ph) / 2;   /* sous le menu, au centre */
  if (ph > H - 140) { ph = H - 140; py = 70; }
  int lw = text_w(S.f26, line) + 48;
  if (lw > 1240) lw = 1240;
  fill_round(W / 2 - lw / 2, py + ph + 12, lw, 46, 23, 0x000000, 190);
  draw_text_a(S.f26, line, W / 2, py + ph + 18, 0xdfe5ee, 1, 1200, 235);
}

/* cache des jaquettes vide : tout est refait (utile si une jaquette s'affiche mal) */
static void quick_clear_cache(void) {
  char dir[PATH_LEN], path[PATH_LEN + 300];
  snprintf(dir, sizeof dir, "%s/cache", plat_data_dir());
  DIR *d = opendir(dir);
  int n = 0;
  struct dirent *de;
  while (d && (de = readdir(d))) {
    const char *x = strrchr(de->d_name, '.');
    if (!x || (strcmp(x, ".jpg") && strcmp(x, ".none"))) continue;
    snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
    if (!remove(path)) n++;
  }
  if (d) closedir(d);
  case_img_forget();
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    if (a->boxtex) { SDL_DestroyTexture(a->boxtex); a->boxtex = NULL; }
    a->box_state = 0; a->box_pref = 0;
    drop_tiles(a);
  }
  char msg[96];
  snprintf(msg, sizeof msg, tr("Cache vidé (%d fichiers) : les jaquettes se refont"), n);
  toast(msg);
  plat_log("menu rapide : cache des jaquettes vide (%d)", n);
}

static void quick_action(int act) {
  char path[PATH_LEN];
  switch (act) {
  case ACT_Q_RESTART:
    save();
    toast("Relance du launcher…");
    draw(); SDL_RenderPresent(S.r);
    if (!plat_restart_ui()) {
      /* fin immediate : les fils (jaquettes, musique) tournent encore, un nettoyage
         complet pourrait les faire planter. Le systeme libere tout a la sortie. */
      plat_log("menu rapide : relance de l'interface");
      plat_quit();
      fflush(NULL);
      _exit(0);
    }
    snprintf(path, sizeof path, "%s/launcher5.elf", plat_data_dir());   /* secours (version de test sur PC) */
    if (plat_send_elf(path)) toast("Chargeur ELF introuvable : envoie /data/launcher5/launcher5.elf");
    break;
  case ACT_Q_CACHE: quick_clear_cache(); break;
  case ACT_Q_RESUME: { App *a = resume_app(); if (a) launch(a); break; }
  case ACT_Q_CATALOG: open_catalog(); break;
  case ACT_HELP_PAY: open_help(1); break;
  case ACT_Q_QUIT: { App *a = menu_find_app(&S.menu, "sys:home"); if (a) launch(a); break; }   /* comme dans les Parametres */
  }
}

static void do_action(int act) {
  Entry *e = selected();
  S.mode = MODE_BROWSE;
  if (S.quick) { S.quick = 0; quick_action(act); return; }
  switch (act) {
  case ACT_LAUNCH: if (e && !e->is_folder) launch(e->app); break;
  case ACT_OPEN:
    if (e && e->is_folder && S.depth < MAX_DEPTH) { S.path[S.depth++] = e->index; S.sel[S.depth] = 0; S.scroll = 0; }
    break;
  case ACT_MOVE: if (e && S.sec == SEC_HOME) S.mode = MODE_MOVE; break;
  case ACT_TO_FOLDER: open_pick_folder(); break;
  case ACT_OUT_OF_FOLDER:
    if (e && S.depth) {
      Node *parent = &S.menu.root;
      for (int i = 0; i < S.depth - 1; i++) parent = &parent->items[S.path[i]];
      move_selected_to(parent == &S.menu.root ? NULL : parent);
      toast("Sorti du dossier");
    }
    break;
  case ACT_RENAME:
    if (e) open_keyboard(e->is_folder ? "Nom du dossier" : "Nouveau nom",
                         e->is_folder ? e->node->name : menu_display_name(&S.menu, e->app), ACT_RENAME);
    break;
  case ACT_RESET_NAME:
    if (e && !e->is_folder) {
      for (int i = 0; i < S.menu.nnames; i++)
        if (!strcmp(S.menu.names[i].id, e->app->id)) {
          memmove(&S.menu.names[i], &S.menu.names[i + 1], sizeof(*S.menu.names) * (S.menu.nnames - i - 1));
          S.menu.nnames--;
          break;
        }
      save();
    }
    break;
  case ACT_HIDE:
    if (e && !e->is_folder) { menu_set_hidden(&S.menu, e->app->id, 1); save(); toast("Masqué  ·  △ › Voir les éléments masqués pour le retrouver"); }
    break;
  case ACT_SHOW: if (e && !e->is_folder) { menu_set_hidden(&S.menu, e->app->id, 0); save(); } break;
  case ACT_COLOR:
    if (e && e->is_folder) {
      S.color_sel = 0;
      for (int i = 0; i < NPALETTE; i++) if (PALETTE[i] == e->node->color) S.color_sel = i;
      S.mode = MODE_COLOR;
    }
    break;
  case ACT_DELETE_FOLDER:
    if (e && e->is_folder) {
      Node *parent = current_node();
      Node f;
      int at = e->index;
      node_remove(parent, at, &f);
      for (int i = 0; i < f.count; i++) node_insert(parent, at + i, &f.items[i]);
      free(f.items);
      save();
      toast("Dossier supprimé, son contenu est revenu ici");
    }
    break;
  case ACT_NEW_FOLDER: open_keyboard("Nom du nouveau dossier", "", ACT_NEW_FOLDER); break;
  case ACT_TOGGLE_HIDDEN: S.show_hidden = !S.show_hidden; break;
  case ACT_THEME: open_theme_editor(); break;
  case ACT_VIEW: set_view(S.menu.view + 1); break;
  case ACT_INFO: if (e && !e->is_folder) open_info(e->app); break;
  case ACT_SEARCH: open_keyboard("Rechercher", search_q, ACT_SEARCH); break;
  case ACT_CATALOG: open_catalog(); break;
  case ACT_BOOT: if (e && !e->is_folder) boot_toggle(e->app); break;
  case ACT_START_MANUAL: case ACT_START_PLUGIN: case ACT_START_AUTOLOAD: start_mode_set(act - ACT_START_MANUAL); break;
  case ACT_FIRST_YES: start_mode_set(2 /* autoloader */); break;
  case ACT_RELINK_YES: start_mode_set(2 /* autoloader */); break;
  case ACT_RELINK_NO: autoload_mark(0); toast("Tu pourras le remettre dans Paramètres › Démarrage de Launcher5"); break;
  case ACT_HELP_PAY: open_help(1); break;
  case ACT_K_BOOT: if (kernel_pending && boot_index(kernel_pending) < 0) boot_toggle(kernel_pending); break;
  case ACT_FIRST_NO: toast("Tu pourras l'activer dans Paramètres › Démarrage de Launcher5"); break;
  case ACT_FAV: case ACT_UNFAV:
    if (e && !e->is_folder) {
      menu_set_fav(&S.menu, e->app->id, act == ACT_FAV);
      save();
      toast(act == ACT_FAV ? "★ Ajouté aux favoris" : "Retiré des favoris");
    }
    break;
  }
}

static void keyboard_done(int ok) {
  SDL_StopTextInput();
  S.mode = S.kb_action == ACT_SAVE_THEME ? MODE_THEME : MODE_BROWSE;
  if (!ok) return;
  char *s = S.kb_buf;
  while (*s == ' ') s++;
  size_t l = strlen(s);
  while (l && s[l - 1] == ' ') s[--l] = 0;
  if (S.kb_action == ACT_SEARCH) {               /* recherche validee (vide : effacee) */
    snprintf(search_q, sizeof search_q, "%s", s);
    *cur_sel() = 0;
    S.scroll = 0;
    if (*s) {
      char msg[120];
      int n = search_count(s);
      snprintf(msg, sizeof msg, tr(n > 1 ? "%d résultats  ·  ○ pour effacer" : "%d résultat  ·  ○ pour effacer"), n);
      toast(msg);
    }
    return;
  }
  if (!*s) { toast("Nom vide : rien n'a été changé"); return; }
  Entry *e = selected();
  static int next = 0;
  if (S.kb_action == ACT_SAVE_THEME) { theme_saved_as(s); return; }
  if (S.kb_action == ACT_RENAME && e) {
    if (e->is_folder) snprintf(e->node->name, NAME_LEN, "%s", s);
    else menu_rename_app(&S.menu, e->app->id, s);
    save();
  } else if (S.kb_action == ACT_NEW_FOLDER) {
    menu_new_folder(&S.menu, current_node(), e ? e->index : -1, s, PALETTE[next++ % NPALETTE]);
    save();
    toast("Dossier créé  ·  range des jeux dedans avec △");
  } else if (S.kb_action == ACT_TO_FOLDER && e) {
    Node *parent = current_node();
    int at = e->index;
    menu_new_folder(&S.menu, parent, at, s, PALETTE[next++ % NPALETTE]);
    Node moved;
    node_remove(parent, at + 1, &moved);
    node_insert(&parent->items[at], -1, &moved);
    save();
    toast("Dossier créé et élément rangé dedans");
  }
}

static void kb_type(const char *t) { if (strlen(S.kb_buf) + strlen(t) < NAME_LEN - 1) strcat(S.kb_buf, t); }
static void kb_backspace(void) {
  size_t l = strlen(S.kb_buf);
  if (!l) return;
  l--;
  while (l && ((unsigned char)S.kb_buf[l] & 0xC0) == 0x80) l--;
  S.kb_buf[l] = 0;
}

/* ------------------------------------------------------------------ */
/* Entrees                                                             */
/* ------------------------------------------------------------------ */
enum { IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_OK, IN_BACK, IN_MENU, IN_MOVE, IN_L1, IN_R1, IN_START, IN_VIEW, IN_L2, IN_R2,
       IN_SEARCH, IN_COUNT };

/* ------------------------------------------------------------------ */
/* Themes : application, editeur a la manette, musique d'ambiance       */
/* ------------------------------------------------------------------ */
static const char *const IMG_EXTS[] = { "png", "jpg", "jpeg", "webp", "bmp", NULL };
static const char *const MUSIC_EXTS[] = { "ogg", "mp3", "flac", "wav", "mod", "xm", "it", "s3m", NULL };

static void data_path(char *out, size_t n, const char *sub) { snprintf(out, n, "%s/%s", plat_data_dir(), sub); }

static void theme_changed(void) { S.th_ver++; }

/* les tuiles deja calculees sont a refaire si leur forme change */
static void check_tile_shape(void) {
  static uint32_t case_key = (uint32_t)-1;       /* boitier : style, et couleur pour « Theme » */
  uint32_t ck = (uint32_t)S.base->case_style << 24 | (S.base->case_style == CASE_THEME ? (S.base->accent & 0xffffff) : 0);
  if (S.tiles_corners == S.th->corners && ck == case_key) return;
  S.tiles_corners = S.th->corners;
  case_key = ck;
  case_img_forget();                             /* images de boitier relues (ajoutees par FTP) */
  for (int i = 0; i < S.menu.napps; i++) drop_tiles(&S.menu.apps[i]);
}

static void scan_media_files(void) {
  char dir[PATH_LEN];
  free(S.walls); free(S.musics);
  data_path(dir, sizeof dir, "fonds");
  S.nwalls = theme_list_files(dir, IMG_EXTS, &S.walls);
  data_path(dir, sizeof dir, "musique");
  S.nmusics = theme_list_files(dir, MUSIC_EXTS, &S.musics);
}

static void load_presets(void) {
  free(S.presets); free(S.preset_files);
  S.nbuiltin = theme_builtin_count();
  char dir[PATH_LEN], path[PATH_LEN + THEME_FILE_LEN];
  data_path(dir, sizeof dir, "themes");
  static const char *const exts[] = { "json", NULL };
  char (*files)[THEME_FILE_LEN];
  int nf = theme_list_files(dir, exts, &files);
  S.presets = malloc(sizeof(Theme) * (S.nbuiltin + nf + 1));
  S.preset_files = malloc(sizeof(*S.preset_files) * (S.nbuiltin + nf + 1));
  S.npresets = 0;
  for (int i = 0; i < S.nbuiltin; i++) {
    S.presets[S.npresets] = *theme_builtin(i);
    S.preset_files[S.npresets++][0] = 0;
  }
  for (int i = 0; i < nf; i++) {
    snprintf(path, sizeof path, "%s/%s", dir, files[i]);
    if (theme_load(path, &S.presets[S.npresets])) { plat_log("theme illisible : %s", path); continue; }
    snprintf(S.preset_files[S.npresets++], THEME_FILE_LEN, "%s", files[i]);
  }
  free(files);
}

static int find_preset(const char *name) {
  for (int i = S.npresets - 1; i >= 0; i--) if (!strcmp(S.presets[i].name, name)) return i;
  return -1;
}

/* theme applique : enregistre dans theme.json (et son nom dans menu.json) */
static void apply_theme(const Theme *t) {
  S.cur = *t;
  S.base = &S.cur;
  theme_changed();
  char path[PATH_LEN];
  data_path(path, sizeof path, "theme.json");
  if (theme_save(path, &S.cur)) plat_log("impossible d'enregistrer %s", path);
  S.theme_sig = file_sig(path);
  snprintf(S.menu.theme, sizeof S.menu.theme, "%s", S.cur.name);
  save();
}

static void open_theme_editor(void) {
  scan_media_files();
  load_presets();
  S.edit = S.cur;
  S.base = &S.edit;
  S.preset = find_preset(S.cur.name);
  S.ed_dirty = S.preset < 0;
  S.ed_row = R_PRESET;
  S.ed_scroll = 0;
  S.mode = MODE_THEME;
}

static int row_visible(int r) {
  const Theme *t = &S.edit;
  switch (r) {
  case R_WALL: return t->bg_mode == BG_IMAGE;
  case R_ART: return t->bg_mode == BG_ART;
  case R_PSTYLE: case R_FALL: return t->particles != PART_NONE;
  case R_VOLUME: return t->music[0] != 0;
  case R_DELETE: return S.preset >= S.nbuiltin && !S.ed_dirty;
  }
  return 1;
}

static const char *ROW_LABEL[R_COUNT] = {
  "Thème de base", "Interface", "Couleurs", "Couleur principale", "Couleur secondaire", "Fond : haut", "Fond : bas", "Arrière-plan",
  "Image", "Illustration du jeu", "Effet animé", "Halos lumineux", "Particules", "Forme", "Sens", "Boîtier", "Coins des jaquettes",
  "Horloge", "Sons d'interface", "Musique", "Volume", "Enregistrer comme mon thème…", "Supprimer ce thème",
};

static int color_index(const NamedColor *list, int n, uint32_t c) {
  int best = 0; long bd = -1;
  for (int i = 0; i < n; i++) {
    long dr = (long)((list[i].c >> 16) & 255) - ((c >> 16) & 255);
    long dg = (long)((list[i].c >> 8) & 255) - ((c >> 8) & 255);
    long db = (long)(list[i].c & 255) - (c & 255);
    long d = dr * dr + dg * dg + db * db;
    if (bd < 0 || d < bd) { bd = d; best = i; }
  }
  return best;
}
static const char *color_name(const NamedColor *list, int n, uint32_t c, char *buf) {
  for (int i = 0; i < n; i++) if (list[i].c == c) return list[i].name;
  sprintf(buf, "#%06X", c & 0xffffff);
  return buf;
}

/* texte de la valeur d'une ligne ; *sw recoit une couleur a montrer (ou -1) */
static const char *row_value(int r, char *buf, long *sw) {
  static const char *BG[] = { "Illustration du jeu", "Dégradé seul", "Image perso" };
  static const char *PART[] = { "Aucune", "Quelques-unes", "Beaucoup" };
  static const char *PST[] = { "Points", "Étincelles", "Flocons" };
  static const char *COR[] = { "Droits", "Arrondis", "Très arrondis" };
  static const char *CLK[] = { "24 h", "12 h", "Masquée" };
  const Theme *t = &S.edit;
  *sw = -1;
  switch (r) {
  case R_PRESET:
    if (S.ed_dirty) snprintf(buf, 96, "%s (modifié)", t->name);
    else snprintf(buf, 96, "%s", t->name);
    return buf;
  case R_ACCENT: *sw = t->accent; return color_name(ACCENTS, NACCENTS, t->accent, buf);
  case R_ACCENT2: *sw = t->accent2; return color_name(ACCENTS, NACCENTS, t->accent2, buf);
  case R_TOP: *sw = t->top; return color_name(DARKS, NDARKS, t->top, buf);
  case R_BOTTOM: *sw = t->bottom; return color_name(DARKS, NDARKS, t->bottom, buf);
  case R_BG: return BG[t->bg_mode];
  case R_WALL: return S.nwalls ? (t->wallpaper[0] ? t->wallpaper : "aucune") : "dossier fonds vide";
  case R_ART: sprintf(buf, "%d %%", t->art); return buf;
  case R_GLOW: sprintf(buf, "%d %%", t->glow); return buf;
  case R_PART: return PART[t->particles];
  case R_PSTYLE: return PST[t->pstyle];
  case R_FALL: return t->fall ? "Tombent" : "Montent";
  case R_CORNERS: return COR[t->corners];
  case R_CLOCK: return CLK[t->clock];
  case R_MUSIC:
    if (!t->music[0]) return S.nmusics ? "Aucune" : "Aucune (dossier vide)";
    if (!strcmp(t->music, "*")) return "Toutes, à la suite";
    return t->music;
  case R_VOLUME: sprintf(buf, "%d %%", t->volume); return buf;
  case R_CASE: {
    static const char *const CASES[CASE_COUNT] = { "Bleu", "Noir", "Blanc", "Cristal", "Steelbook", "Couleur du thème", "Images (boitiers/)", "Aucun" };
    return CASES[t->case_style >= 0 && t->case_style < CASE_COUNT ? t->case_style : 0];
  }
  }
  return "";
}

static int cyc(int v, int d, int n) { return ((v + d) % n + n) % n; }
static int step(int v, int d) { v += d * 10; return v < 0 ? 0 : v > 100 ? 100 : v; }

static void theme_row_change(int d) {
  Theme *t = &S.edit, before = S.edit;
  switch (S.ed_row) {
  case R_PRESET: {
    if (!S.npresets) return;
    int i = S.preset < 0 ? (d > 0 ? 0 : S.npresets - 1) : cyc(S.preset, d, S.npresets);
    Theme keep = *t;
    *t = S.presets[i];
    if (keep.music[0] || i < S.nbuiltin) {  /* ta musique te suit d'un theme a l'autre */
      snprintf(t->music, sizeof t->music, "%s", keep.music);
      t->volume = keep.volume;
    }
    if (i < S.nbuiltin && !t->wallpaper[0]) snprintf(t->wallpaper, sizeof t->wallpaper, "%s", keep.wallpaper);
    S.preset = i;
    S.ed_dirty = 0;
    theme_changed();
    return;
  }
  case R_ACCENT: t->accent = ACCENTS[cyc(color_index(ACCENTS, NACCENTS, t->accent), d, NACCENTS)].c; break;
  case R_ACCENT2: t->accent2 = ACCENTS[cyc(color_index(ACCENTS, NACCENTS, t->accent2), d, NACCENTS)].c; break;
  case R_TOP: t->top = DARKS[cyc(color_index(DARKS, NDARKS, t->top), d, NDARKS)].c; break;
  case R_BOTTOM: t->bottom = DARKS[cyc(color_index(DARKS, NDARKS, t->bottom), d, NDARKS)].c; break;
  case R_BG:
    t->bg_mode = cyc(t->bg_mode, d, BG_COUNT);
    if (t->bg_mode == BG_IMAGE && !t->wallpaper[0] && S.nwalls) snprintf(t->wallpaper, sizeof t->wallpaper, "%s", S.walls[0]);
    if (t->bg_mode == BG_IMAGE && !S.nwalls) toast("Copie tes images dans /data/launcher5/fonds");
    break;
  case R_WALL: {
    if (!S.nwalls) { toast("Copie tes images dans /data/launcher5/fonds"); return; }
    int i = -1;
    for (int k = 0; k < S.nwalls; k++) if (!strcmp(S.walls[k], t->wallpaper)) i = k;
    i = i < 0 ? 0 : cyc(i, d, S.nwalls);
    snprintf(t->wallpaper, sizeof t->wallpaper, "%s", S.walls[i]);
    break;
  }
  case R_ART: t->art = step(t->art, d); break;
  case R_GLOW: t->glow = step(t->glow, d); break;
  case R_PART: t->particles = cyc(t->particles, d, PART_COUNT); break;
  case R_PSTYLE: t->pstyle = cyc(t->pstyle, d, PSTYLE_COUNT); break;
  case R_FALL: t->fall = !t->fall; break;
  case R_CORNERS: t->corners = cyc(t->corners, d, CORNER_COUNT); break;
  case R_CASE: t->case_style = cyc(t->case_style, d, CASE_COUNT); break;
  case R_CLOCK: t->clock = cyc(t->clock, d, CLOCK_COUNT); break;
  case R_MUSIC: {
    if (!S.nmusics) { t->music[0] = 0; toast("Copie tes musiques (ogg, mp3, flac…) dans /data/launcher5/musique"); return; }
    /* 0 : aucune, 1 : toutes, 2.. : un fichier */
    int i = 0;
    if (!strcmp(t->music, "*")) i = 1;
    else for (int k = 0; k < S.nmusics; k++) if (!strcmp(S.musics[k], t->music)) i = k + 2;
    i = cyc(i, d, S.nmusics + 2);
    if (i == 0) t->music[0] = 0;
    else if (i == 1) snprintf(t->music, sizeof t->music, "*");
    else snprintf(t->music, sizeof t->music, "%s", S.musics[i - 2]);
    break;
  }
  case R_VOLUME: t->volume = step(t->volume, d); break;
  case R_AUTO: t->auto_color = !t->auto_color; break;
  case R_LAYOUT: t->layout = cyc(t->layout, d, LAYOUT_COUNT); break;
  case R_FX: t->fx = cyc(t->fx, d, FX_COUNT); break;
  case R_SFX: t->sfx = step(t->sfx, d); if (t->sfx) { S.th = t; sfx(SFX_OK); } break;
  default: return;
  }
  if (!memcmp(&before, t, sizeof before)) return;   /* deja au minimum / maximum */
  S.ed_dirty = 1;
  theme_changed();
}

static void theme_saved_as(const char *name) {
  char file[THEME_FILE_LEN], dir[PATH_LEN], path[PATH_LEN + THEME_FILE_LEN];
  snprintf(S.edit.name, sizeof S.edit.name, "%s", name);
  theme_file_name(name, file, sizeof file);
  data_path(dir, sizeof dir, "themes");
  mkdir(dir, 0755);
  snprintf(path, sizeof path, "%s/%s", dir, file);
  if (theme_save(path, &S.edit)) { toast("⚠ Impossible d'enregistrer le thème"); return; }
  load_presets();
  S.preset = find_preset(S.edit.name);
  S.ed_dirty = 0;
  char msg[160];
  snprintf(msg, sizeof msg, tr("Thème « %s » enregistré (themes/%s)"), name, file);
  toast(msg);
}

static void theme_editor_input(int in) {
  switch (in) {
  case IN_UP: case IN_DOWN: {
    int d = in == IN_UP ? -1 : 1, r = S.ed_row;
    do r = cyc(r, d, R_COUNT); while (!row_visible(r));
    S.ed_row = r;
    return;
  }
  case IN_LEFT: theme_row_change(-1); return;
  case IN_RIGHT: theme_row_change(1); return;
  case IN_L1: case IN_R1: { int keep = S.ed_row; S.ed_row = R_PRESET; theme_row_change(in == IN_L1 ? -1 : 1); S.ed_row = keep; return; }
  case IN_BACK:
    S.base = &S.cur;            /* annule : on revient au theme applique */
    theme_changed();
    S.mode = MODE_BROWSE;
    return;
  case IN_OK: case IN_START:
    if (S.ed_row == R_SAVE && in == IN_OK) {
      char base[64];
      snprintf(base, sizeof base, "%s", S.preset >= S.nbuiltin && !S.ed_dirty ? S.edit.name : tr("Mon thème"));
      open_keyboard("Nom de ton thème", base, ACT_SAVE_THEME);
      return;
    }
    if (S.ed_row == R_DELETE && in == IN_OK) {
      char dir[PATH_LEN], path[PATH_LEN + THEME_FILE_LEN];
      data_path(dir, sizeof dir, "themes");
      snprintf(path, sizeof path, "%s/%s", dir, S.preset_files[S.preset]);
      if (remove(path)) { toast("⚠ Impossible de supprimer ce thème"); return; }
      load_presets();
      S.preset = -1;
      S.ed_dirty = 1;
      S.ed_row = R_SAVE;
      toast("Thème supprimé (les réglages restent affichés)");
      return;
    }
    if (S.ed_dirty && S.preset >= 0 && S.preset < S.nbuiltin && !strstr(S.edit.name, tr(" perso"))) {
      char n[64];
      snprintf(n, sizeof n, "%.38s%s", tr(S.edit.name), tr(" perso"));
      snprintf(S.edit.name, sizeof S.edit.name, "%s", n);
    }
    if (S.ed_dirty) {                            /* modifications gardees dans la liste des themes */
      char file[THEME_FILE_LEN], dir[PATH_LEN], path[PATH_LEN + THEME_FILE_LEN];
      if (S.preset >= S.nbuiltin) snprintf(file, sizeof file, "%s", S.preset_files[S.preset]);
      else theme_file_name(S.edit.name, file, sizeof file);
      data_path(dir, sizeof dir, "themes");
      mkdir(dir, 0755);
      snprintf(path, sizeof path, "%s/%s", dir, file);
      if (!theme_save(path, &S.edit)) { load_presets(); S.preset = find_preset(S.edit.name); S.ed_dirty = 0; }
      else plat_log("impossible d'enregistrer %s", path);
    }
    apply_theme(&S.edit);
    S.mode = MODE_BROWSE;
    {
      char msg[120];
      snprintf(msg, sizeof msg, tr("Thème « %s » appliqué"), tr(S.cur.name));
      toast(msg);
    }
    return;
  }
}

/* --- Studio de theme : panneau a droite, apercu en direct a gauche --- */
#define ED_ROWH 52
#define ED_HEAD 44

/* titre de groupe affiche avant certaines lignes */
static const char *row_group(int r) {
  switch (r) {
  case R_LAYOUT: return "AFFICHAGE";
  case R_AUTO: return "COULEURS";
  case R_BG: return "ARRIÈRE-PLAN";
  case R_GLOW: return "EFFETS";
  case R_CASE: return "BOÎTIERS DES JEUX";
  case R_CLOCK: return "INTERFACE ET SON";
  }
  return NULL;
}

/* options courtes des lignes a choix (puces) ; renvoie leur nombre et la valeur choisie */
static int row_chips(int r, const char *const **opts, int *cur) {
  static const char *const BG[] = { "Jeu", "Dégradé", "Image" };
  static const char *const PART[] = { "Aucune", "Peu", "Beaucoup" };
  static const char *const PST[] = { "Points", "Étincelles", "Flocons" };
  static const char *const FALL[] = { "Montent", "Tombent" };
  static const char *const COR[] = { "Droits", "Arrondis", "Ronds" };
  static const char *const CLK[] = { "24 h", "12 h", "Masquée" };
  static const char *const LAY[] = { "Launcher5", "Loader GX", "Rebug XMB" };
  static const char *const AUTO[] = { "Du thème", "Du jeu" };
  static const char *const FXN[] = { "Aucun", "Grille", "Hyperespace", "Pluie" };
  const Theme *t = &S.edit;
  switch (r) {
  case R_LAYOUT: *opts = LAY; *cur = t->layout; return LAYOUT_COUNT;
  case R_AUTO: *opts = AUTO; *cur = t->auto_color ? 1 : 0; return 2;
  case R_FX: *opts = FXN; *cur = t->fx; return FX_COUNT;
  case R_BG: *opts = BG; *cur = t->bg_mode; return BG_COUNT;
  case R_PART: *opts = PART; *cur = t->particles; return PART_COUNT;
  case R_PSTYLE: *opts = PST; *cur = t->pstyle; return PSTYLE_COUNT;
  case R_FALL: *opts = FALL; *cur = t->fall; return 2;
  case R_CORNERS: *opts = COR; *cur = t->corners; return CORNER_COUNT;
  case R_CLOCK: *opts = CLK; *cur = t->clock; return CLOCK_COUNT;
  }
  return 0;
}

/* mini carte d'un theme : degrade, deux couleurs, nom */
static void theme_card(const Theme *t, int x, int y, int w, int h, int on, int focus) {
  int rad = 12;
  for (int i = 0; i < h; i += 3) {
    float k = (float)i / h;
    uint32_t a = t->top, b = t->bottom;
    uint32_t c = ((uint32_t)(((a >> 16) & 255) * (1 - k) + ((b >> 16) & 255) * k) << 16) |
                 ((uint32_t)(((a >> 8) & 255) * (1 - k) + ((b >> 8) & 255) * k) << 8) |
                 (uint32_t)((a & 255) * (1 - k) + (b & 255) * k);
    int inset = 0;
    if (i < rad || i > h - rad) {               /* coins arrondis */
      float dy = i < rad ? rad - i : i - (h - rad);
      inset = (int)(rad - sqrtf((float)rad * rad - dy * dy));
    }
    set_color(c, 255);
    SDL_Rect rr = { x + inset, y + i, w - 2 * inset, 3 };
    SDL_RenderFillRect(S.r, &rr);
  }
  fill_round(x + 12, y + h - 22, w / 2, 8, 4, t->accent, 255);
  fill_round(x + 12 + w / 2 + 8, y + h - 22, w / 5, 8, 4, t->accent2, 255);
  fill_round(x + w - 34, y + 12, 20, 20, 10, t->accent, 255);
  if (on) {
    if (focus && !S.lite) glow(x + w / 2, y + h / 2, w, t->accent, 90);
    outline_round(x, y, w, h, rad, 3, focus ? 0xffffff : t->accent, 255);
  } else outline_round(x, y, w, h, rad, 1, 0xffffff, 40);
  draw_text(on ? S.f26 : S.f22, t->name, x + w / 2, y + h + 8, on ? 0xffffff : 0x9aa4b5, 1, w + 20);
}

static void draw_theme_editor(void) {
  int px = W - ED_W - 30, py = 24, pw = ED_W, ph = H - 48;
  /* panneau : verre sombre, filet d'accent */
  if (!S.lite) glow(px + pw / 2, py, pw / 2, S.th->accent, 45);
  if (S.lite) {                                  /* PS5 : panneau opaque, sans melange (bien plus rapide) */
    SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_NONE);
    fill_round(px, py, pw, ph, 28, 0x07090f, 255);
    SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
  } else fill_round(px, py, pw, ph, 28, 0x07090f, 236);
  outline_round(px, py, pw, ph, 28, 2, S.th->accent, 150);
  fill_round(px + 40, py + 30, 6, 34, 3, S.th->accent, 255);
  draw_text(S.f36, "STUDIO THÈME", px + 60, py + 26, 0xffffff, 0, 0);
  draw_text(S.f22, S.ed_dirty ? "● modifié" : "aperçu en direct", px + pw - 40, py + 38,
            S.ed_dirty ? S.th->accent2 : 0x9aa4b5, 2, 0);

  /* bande des themes (L1 / R1 ou ◀ ▶ sur la premiere ligne) */
  int cw = 138, ch = 88, gap = 16, sy = py + 92, focus = S.ed_row == R_PRESET;
  if (focus) fill_round(px + 16, sy - 12, pw - 32, ch + 62, 18, S.th->accent, 34);
  int center = S.preset >= 0 ? S.preset : 0, shown = 5;
  int x0 = px + (pw - (shown * cw + (shown - 1) * gap)) / 2;
  for (int k = 0; k < shown && S.npresets; k++) {
    int i = center + k - shown / 2;
    if (S.npresets >= shown) i = ((i % S.npresets) + S.npresets) % S.npresets;
    else if (i < 0 || i >= S.npresets) continue;
    theme_card(&S.presets[i], x0 + k * (cw + gap), sy, cw, ch, i == S.preset, focus);
  }
  if (S.preset < 0) draw_text(S.f22, "Thème perso (non enregistré)", px + pw / 2, sy + ch + 36, S.th->accent2, 1, 0);
  if (focus) {
    draw_text(S.f36, "◀", px + 30, sy + ch / 2 - 22, 0xffffff, 0, 0);
    draw_text(S.f36, "▶", px + pw - 30, sy + ch / 2 - 22, 0xffffff, 2, 0);
  }

  /* lignes de reglages, defilantes */
  int top = sy + ch + 70, bottom = py + ph - 64;
  int total = 0, fy = 0;
  for (int r = R_AUTO; r < R_COUNT; r++) {
    if (!row_visible(r)) continue;
    if (row_group(r) || r == R_SAVE) total += ED_HEAD;
    if (r == S.ed_row) fy = total;
    total += ED_ROWH;
  }
  float want = S.ed_scroll;
  if (fy - want < 0) want = (float)fy - ED_HEAD;
  if (fy + ED_ROWH - want > bottom - top) want = (float)(fy + ED_ROWH - (bottom - top));
  if (want > total - (bottom - top)) want = (float)(total - (bottom - top));
  if (want < 0) want = 0;
  S.ed_scroll += (want - S.ed_scroll) * ease(0.3f);
  SDL_Rect clip = { px, top, pw, bottom - top };
  SDL_RenderSetClipRect(S.r, &clip);
  int y = top - (int)S.ed_scroll;
  int lx = px + 44, cx0 = px + 330, cx1 = px + pw - 44;
  for (int r = R_AUTO; r < R_COUNT; r++) {
    if (!row_visible(r)) continue;
    const char *grp = row_group(r);
    if (grp) {
      draw_text(S.f22, grp, lx, y + 14, S.th->accent, 0, 0);
      set_color(0xffffff, 28);
      SDL_RenderDrawLine(S.r, lx + text_w(S.f22, grp) + 16, y + 28, cx1, y + 28);
      y += ED_HEAD;
    } else if (r == R_SAVE) y += ED_HEAD;
    int on = r == S.ed_row, my = y + ED_ROWH / 2;
    if (y > bottom || y + ED_ROWH < top) { y += ED_ROWH; continue; }
    if (on && r != R_SAVE && r != R_DELETE) {
      fill_round(px + 18, y + 2, pw - 36, ED_ROWH - 4, 14, S.th->accent, 40);
      fill_round(px + 18, y + 12, 5, ED_ROWH - 24, 2, S.th->accent, 255);
    }
    if (r == R_SAVE || r == R_DELETE) {          /* boutons d'action */
      uint32_t c = r == R_DELETE ? 0xff4d5e : S.th->accent;
      if (on) {
        if (!S.lite) glow(px + pw / 2, my, pw / 3, c, 70);
        fill_round(lx, y + 4, cx1 - lx, ED_ROWH - 8, (ED_ROWH - 8) / 2, c, 255);
      } else outline_round(lx, y + 4, cx1 - lx, ED_ROWH - 8, (ED_ROWH - 8) / 2, 2, c, 200);
      draw_text(S.f26, ROW_LABEL[r], px + pw / 2, y + 11, on ? 0x0a0a12 : c, 1, 0);
      y += ED_ROWH;
      continue;
    }
    draw_text(S.f26, ROW_LABEL[r], lx + 14, y + 11, on ? 0xffffff : 0xc9d1dc, 0, cx0 - lx - 24);

    const char *const *opts; int cur, nopt = row_chips(r, &opts, &cur);
    if (r == R_ACCENT || r == R_ACCENT2 || r == R_TOP || r == R_BOTTOM) {
      /* nuancier : la couleur choisie au centre, ses voisines autour */
      const NamedColor *list = r <= R_ACCENT2 ? ACCENTS : DARKS;
      int nl = r <= R_ACCENT2 ? NACCENTS : NDARKS;
      uint32_t val = r == R_ACCENT ? S.edit.accent : r == R_ACCENT2 ? S.edit.accent2 : r == R_TOP ? S.edit.top : S.edit.bottom;
      int ci = color_index(list, nl, val), sw = 34, sg = 9, cnt = 7;
      int sx = cx1 - (cnt * sw + (cnt - 1) * sg) - 4;
      for (int k = 0; k < cnt; k++) {
        int d = k - cnt / 2, c = ((ci + d) % nl + nl) % nl, sx2 = sx + k * (sw + sg);
        uint32_t col = d ? list[c].c : val;
        if (!d) {
          fill_round(sx2 - 6, my - 22, sw + 12, 44, 12, col, 255);
          outline_round(sx2 - 6, my - 22, sw + 12, 44, 12, 2, 0xffffff, on ? 255 : 160);
        } else {
          int a = 255 - abs(d) * 38;
          fill_round(sx2, my - 15, sw, 30, 9, col, a);
          outline_round(sx2, my - 15, sw, 30, 9, 1, 0xffffff, 30);
        }
      }
      if (on) {
        char buf[16];
        /* les noms de couleurs n'existent qu'en francais : code couleur ailleurs */
        if (i18n_lang() == L_FR) draw_text(S.f22, color_name(list, nl, val, buf), sx - 16, y + 14, 0x9aa4b5, 2, 170);
        else { snprintf(buf, sizeof buf, "#%06X", val & 0xffffff); draw_text(S.f22, buf, sx - 16, y + 14, 0x9aa4b5, 2, 170); }
      }
    } else if (r == R_ART || r == R_GLOW || r == R_VOLUME || r == R_SFX) {
      int v = r == R_ART ? S.edit.art : r == R_GLOW ? S.edit.glow : r == R_SFX ? S.edit.sfx : S.edit.volume;
      int tx = cx0 + 10, tw = cx1 - tx - 120;
      fill_round(tx, my - 4, tw, 8, 4, 0xffffff, 40);
      fill_round(tx, my - 4, tw * v / 100, 8, 4, S.th->accent, 255);
      int kx = tx + tw * v / 100;
      if (on && !S.lite) glow(kx, my, 40, S.th->accent, 120);
      fill_round(kx - 11, my - 11, 22, 22, 11, on ? 0xffffff : 0xc9d1dc, 255);
      char buf[16];
      snprintf(buf, sizeof buf, "%d %%", v);
      draw_text(S.f26, buf, cx1, y + 11, 0xffffff, 2, 0);
    } else if (nopt) {
      int widths[4], tot = 0;
      for (int k = 0; k < nopt; k++) { widths[k] = text_w(S.f22, opts[k]) + 30; tot += widths[k] + (k ? 8 : 0); }
      int x = cx1 - tot;
      for (int k = 0; k < nopt; k++) {
        if (k == cur) {
          fill_round(x, my - 18, widths[k], 36, 18, S.th->accent, 255);
          draw_text(S.f22, opts[k], x + widths[k] / 2, my - 13, 0x0a0a12, 1, 0);
        } else {
          outline_round(x, my - 18, widths[k], 36, 18, 1, 0xffffff, on ? 90 : 45);
          draw_text(S.f22, opts[k], x + widths[k] / 2, my - 13, on ? 0xc9d1dc : 0x7d8796, 1, 0);
        }
        x += widths[k] + 8;
      }
    } else {                                     /* fichiers : image, musique */
      char buf[128]; long sw;
      const char *v = row_value(r, buf, &sw);
      int right = cx1;
      if (on) { draw_text(S.f26, "▶", right, y + 11, S.th->accent, 2, 0); right -= 36; }
      int vw = draw_text(S.f26, v, right, y + 11, 0xffffff, 2, cx1 - cx0 - 80);
      if (on) draw_text(S.f26, "◀", right - vw - 14, y + 11, S.th->accent, 2, 0);
    }
    y += ED_ROWH;
  }
  SDL_RenderSetClipRect(S.r, NULL);
  /* indication de defilement */
  if (total > bottom - top) {
    int th = (bottom - top) * (bottom - top) / total;
    int ty = top + (int)((bottom - top - th) * S.ed_scroll / (total - (bottom - top)));
    fill_round(px + pw - 14, ty, 4, th, 2, S.th->accent, 160);
  }
  set_color(0xffffff, 24);
  SDL_RenderDrawLine(S.r, px + 30, bottom + 6, px + pw - 30, bottom + 6);
  draw_text(S.f22, "▲▼ Réglage    ◀▶ Modifier    L1 R1 Thème    ✕ Appliquer    ○ Annuler",
            px + pw / 2, bottom + 18, 0x9aa4b5, 1, pw - 40);
}

/* --- musique d'ambiance ------------------------------------------------ */
#ifdef HAVE_MIXER
static struct { int state; Mix_Music *m; Mix_Chunk *loop; char key[THEME_FILE_LEN]; int pos, vol, paused, gen; volatile int finished; } MU = { .vol = -1 };
#define MUSIC_CH 0                               /* canal reserve a la musique en boucle */
/* morceau en boucle decode en tache de fond (pas d'attente au demarrage) */
static struct { SDL_mutex *mx; int busy, done, gen; char path[PATH_LEN]; Mix_Chunk *c; } LD;
static void music_finished(void) { MU.finished = 1; }

static int music_ready(void) {
  if (MU.state) return MU.state > 0;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) || Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 2048)) {
    plat_log("musique indisponible : %s", Mix_GetError());
    MU.state = -1;
    return 0;
  }
  Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_MOD);
  Mix_HookMusicFinished(music_finished);
  Mix_AllocateChannels(8);
  Mix_ReserveChannels(1);                        /* canal MUSIC_CH : jamais pris par les sons d'interface */
  MU.state = 1;
  plat_log("musique prete");
  return 1;
}

static void music_stop(void) {
  if (MU.m) { Mix_HaltMusic(); Mix_FreeMusic(MU.m); MU.m = NULL; }
  if (MU.loop) { Mix_HaltChannel(MUSIC_CH); Mix_FreeChunk(MU.loop); MU.loop = NULL; }
  MU.gen++;                                      /* un chargement en cours ne sera pas joue */
  MU.finished = 0;
}

/* Boucle sans coupure : le morceau est decode une fois en memoire, ses silences de debut
   et de fin sont retires, puis il est rejoue a l'echantillon pres. */
static void loop_trim(Mix_Chunk *c) {
  int freq, ch; Uint16 fmt;
  if (!Mix_QuerySpec(&freq, &fmt, &ch) || fmt != AUDIO_S16SYS || ch < 1) return;
  Sint16 *smp = (Sint16 *)c->abuf;
  long frames = (long)c->alen / (2 * ch), a = 0, b = frames - 1;
  #define LOUD(f) ({ int _l = 0; for (int _c = 0; _c < ch; _c++) if (abs(smp[(f) * ch + _c]) > 120) _l = 1; _l; })
  while (a < b && !LOUD(a)) a++;
  while (b > a && !LOUD(b)) b--;
  #undef LOUD
  if (b - a < freq / 4) return;                  /* presque vide : on ne touche a rien */
  long n = b - a + 1;
  memmove(smp, smp + a * ch, (size_t)n * ch * 2);
  c->alen = (Uint32)(n * ch * 2);
  long fade = freq / 200;                        /* 5 ms adoucis aux raccords : pas de clic */
  for (long i = 0; i < fade && i < n; i++)
    for (int k = 0; k < ch; k++) {
      smp[i * ch + k] = (Sint16)(smp[i * ch + k] * i / fade);
      smp[(n - 1 - i) * ch + k] = (Sint16)(smp[(n - 1 - i) * ch + k] * i / fade);
    }
}

static int loop_loader(void *u) {
  (void)u;
  Mix_Chunk *c = Mix_LoadWAV(LD.path);
  if (c) loop_trim(c);
  SDL_LockMutex(LD.mx);
  LD.c = c; LD.done = 1;
  SDL_UnlockMutex(LD.mx);
  return 0;
}

/* morceau pret : on le joue s'il est toujours voulu */
static void loop_poll(void) {
  if (!LD.mx || !LD.busy) return;
  SDL_LockMutex(LD.mx);
  int done = LD.done;
  SDL_UnlockMutex(LD.mx);
  if (!done) return;
  LD.busy = 0;
  if (LD.gen != MU.gen || !LD.c) {
    if (LD.c) Mix_FreeChunk(LD.c);
    else if (LD.gen == MU.gen) { plat_log("musique illisible : %s", LD.path); }
    LD.c = NULL;
    return;
  }
  MU.loop = LD.c; LD.c = NULL;
  Mix_Volume(MUSIC_CH, MU.vol < 0 ? MIX_MAX_VOLUME : MU.vol * MIX_MAX_VOLUME / 100);
  if (Mix_PlayChannel(MUSIC_CH, MU.loop, -1) < 0) plat_log("lecture impossible : %s", Mix_GetError());
  else {
    Sint16 *smp = (Sint16 *)MU.loop->abuf;
    long ns = (long)MU.loop->alen / 2, pk = 0;
    for (long i = 0; i < ns; i += 97) if (abs(smp[i]) > pk) pk = abs(smp[i]);
    plat_log("musique en boucle : %s (%.1f s, crete %ld)", LD.path, ns / 96000.0, pk);
  }
  if (MU.paused) Mix_Pause(MUSIC_CH);
}

static void music_play_file(const char *file, int loops) {
  char path[PATH_LEN];
  snprintf(path, sizeof path, "%s/musique/%s", plat_data_dir(), file);
  music_stop();
  struct stat st;
  if (loops < 0 && !stat(path, &st) && st.st_size < (12 << 20) && (LD.mx || (LD.mx = SDL_CreateMutex())) && !LD.busy) {
    snprintf(LD.path, sizeof LD.path, "%s", path);
    LD.gen = MU.gen; LD.done = 0; LD.c = NULL; LD.busy = 1;
    SDL_Thread *th = SDL_CreateThread(loop_loader, "musique", NULL);
    if (th) { SDL_DetachThread(th); return; }
    LD.busy = 0;                                 /* pas de fil : lecture classique */
  }
  MU.m = Mix_LoadMUS(path);
  if (!MU.m) { plat_log("musique illisible : %s (%s)", path, Mix_GetError()); return; }
  if (Mix_PlayMusic(MU.m, loops)) plat_log("lecture impossible : %s", Mix_GetError());
  else plat_log("musique : %s", file);
}

static void music_update(void) {
  const char *want = S.th->music;
  if (!want[0] || (!strcmp(want, "*") && !S.nmusics)) {
    if (MU.m || MU.loop || LD.busy) music_stop();
    MU.key[0] = 0;
    return;
  }
  if (!music_ready()) return;
  loop_poll();
  if (MU.vol != S.th->volume) {
    MU.vol = S.th->volume;
    Mix_VolumeMusic(MU.vol * MIX_MAX_VOLUME / 100);
    Mix_Volume(MUSIC_CH, MU.vol * MIX_MAX_VOLUME / 100);
  }
  if (strcmp(MU.key, want)) {
    snprintf(MU.key, sizeof MU.key, "%s", want);
    if (!strcmp(want, "*")) { MU.pos = rand() % S.nmusics; music_play_file(S.musics[MU.pos], 1); }
    else music_play_file(want, -1);
  } else if (MU.finished && !strcmp(want, "*")) {
    MU.finished = 0;
    MU.pos = (MU.pos + 1) % S.nmusics;
    music_play_file(S.musics[MU.pos], 1);
  }
}

static void music_pause(int on) {
  if (MU.paused == on) return;
  MU.paused = on;
  if (MU.loop) { if (on) Mix_Pause(MUSIC_CH); else Mix_Resume(MUSIC_CH); }
  if (MU.m) { if (on) Mix_PauseMusic(); else Mix_ResumeMusic(); }
}
static void music_fade(void) {
  if (MU.m) Mix_FadeOutMusic(600);
  if (MU.loop) Mix_FadeOutChannel(MUSIC_CH, 600);
}

/* --- sons d'interface : fabriques au premier usage ----------------------- */
static Mix_Chunk *SFXC[SFX_COUNT];
static Sint16 *SFXB[SFX_COUNT];                  /* echantillons (liberes avec le son) */
static int sfx_state;                            /* 0 pas encore, 1 prets, -1 impossibles */
static int sfx_rate, sfx_chans, sfx_pack_built = -1, sfx_boot_built = -1;

#define TAU 6.2831853f
static float osc(float *ph, float f) {           /* oscillateur : phase qui avance a la frequence f */
  *ph += TAU * f / sfx_rate;
  if (*ph >= TAU) *ph -= TAU * floorf(*ph / TAU);
  return *ph;
}
static float sq(float ph) { return ph < TAU / 2 ? 1.0f : -1.0f; }
static float saw(float ph) { return ph / (TAU / 2) - 1; }
/* cloche : fondamentale + partiels inharmoniques qui s'eteignent plus vite */
static float bell(float f, float t, float dec) {
  if (t < 0) return 0;
  float e = expf(-t * dec);
  return (sinf(TAU * f * t) + 0.5f * sinf(TAU * 2.76f * f * t) * e + 0.25f * sinf(TAU * 5.4f * f * t) * e * e) * e;
}

/* duree des sons de chaque style : deplacement, section, valider, retour, lancement, erreur */
static const float SFX_DUR[SFX_PACKS][SFX_BOOT] = {
  { 0.035f, 0.07f, 0.11f, 0.11f, 0.65f, 0.16f },   /* Neon */
  { 0.04f,  0.08f, 0.15f, 0.15f, 0.70f, 0.25f },   /* Retro 8-bit */
  { 0.07f,  0.14f, 0.22f, 0.22f, 0.90f, 0.30f },   /* Doux */
  { 0.05f,  0.09f, 0.14f, 0.14f, 0.85f, 0.25f },   /* Sci-fi */
  { 0.14f,  0.25f, 0.40f, 0.40f, 1.10f, 0.45f },   /* Cristal */
};
static const float BOOT_DUR[BOOT_SOUNDS] = { 1.0f, 1.7f, 1.9f, 1.0f, 1.5f, 0 };

static float sfx_sample(int id, int pack, float t, float k, float *p, float nz) {
  switch (pack) {
  case 1:                                        /* Retro 8-bit : ondes carrees, notes en escalier */
    switch (id) {
    case SFX_MOVE: return sq(osc(&p[0], 1760)) * 0.07f;
    case SFX_SECTION: return sq(osc(&p[0], k < 0.5f ? 988 : 1319)) * 0.07f;
    case SFX_OK: return sq(osc(&p[0], k < 0.33f ? 659 : k < 0.66f ? 988 : 1319)) * 0.07f;
    case SFX_BACK: return sq(osc(&p[0], k < 0.33f ? 1319 : k < 0.66f ? 988 : 659)) * 0.07f;
    case SFX_LAUNCH: return sq(osc(&p[0], 220 * powf(2, floorf(k * 16) / 8))) * 0.07f
                            + (2 * fabsf(saw(osc(&p[1], 110))) - 1) * 0.10f * (1 - k);
    case SFX_ERROR: return sq(osc(&p[0], k < 0.5f ? 196 : 147)) * 0.08f;
    }
    break;
  case 2:                                        /* Doux : sinus feutres, graves */
    switch (id) {
    case SFX_MOVE: return (sinf(osc(&p[0], 600)) + 0.2f * sinf(osc(&p[1], 1200))) * expf(-t * 55) * 0.16f;
    case SFX_SECTION: return (sinf(osc(&p[0], 523)) + 0.5f * sinf(osc(&p[1], 784))) * expf(-t * 22) * 0.13f;
    case SFX_OK: return sinf(TAU * 523 * t) * expf(-t * 14) * 0.15f + (t > 0.08f ? sinf(TAU * 784 * t) * expf(-(t - 0.08f) * 14) * 0.15f : 0);
    case SFX_BACK: return sinf(TAU * 784 * t) * expf(-t * 14) * 0.15f + (t > 0.08f ? sinf(TAU * 523 * t) * expf(-(t - 0.08f) * 14) * 0.15f : 0);
    case SFX_LAUNCH: return (sinf(TAU * 262 * t) + sinf(TAU * 330 * t) + sinf(TAU * 392 * t) + 0.7f * sinf(TAU * 523 * t))
                            * 0.055f * sinf(k * 3.14159f);
    case SFX_ERROR: return sinf(osc(&p[0], 220 * (1 + 0.03f * sinf(TAU * 12 * t)))) * 0.16f * (1 - k);
    }
    break;
  case 3:                                        /* Sci-fi : glissandos et modulation */
    switch (id) {
    case SFX_MOVE: return sinf(osc(&p[0], 2600 - 1800 * k)) * 0.12f * (1 - k);
    case SFX_SECTION: return sinf(osc(&p[0], 900) + 3 * sinf(osc(&p[1], 450))) * expf(-t * 30) * 0.12f;
    case SFX_OK: return sinf(osc(&p[0], 500 + 1500 * k * k)) * 0.15f * (1 - 0.5f * k);
    case SFX_BACK: return sinf(osc(&p[0], 2000 - 1500 * sqrtf(k))) * 0.15f * (1 - 0.5f * k);
    case SFX_LAUNCH: return (sinf(osc(&p[0], 80 + 1600 * k * k * k)) * 0.14f + sinf(osc(&p[1], (80 + 1600 * k * k * k) * 1.01f)) * 0.10f
                             + nz * 0.06f * k) * sinf(k * 3.14159f);
    case SFX_ERROR: return saw(osc(&p[0], 140)) * 0.08f * (0.6f + 0.4f * sinf(TAU * 30 * t)) * (1 - k);
    }
    break;
  case 4:                                        /* Cristal : clochettes */
    switch (id) {
    case SFX_MOVE: return bell(2093, t, 40) * 0.09f;
    case SFX_SECTION: return bell(1568, t, 18) * 0.10f;
    case SFX_OK: return (bell(1319, t, 9) + bell(1976, t - 0.08f, 9)) * 0.08f;
    case SFX_BACK: return (bell(1976, t, 9) + bell(1319, t - 0.08f, 9)) * 0.08f;
    case SFX_LAUNCH: {
      static const float N[5] = { 1047, 1319, 1568, 2093, 2637 };
      float v = 0;
      for (int i = 0; i < 5; i++) v += bell(N[i], t - i * 0.1f, 5);
      return v * 0.06f;
    }
    case SFX_ERROR: return (bell(330, t, 8) + bell(349, t, 8)) * 0.08f;
    }
    break;
  default:                                       /* Neon : bips nets, le son d'origine */
    switch (id) {
    case SFX_MOVE: return sinf(osc(&p[0], 1500 - 400 * k)) * expf(-t * 90) * 0.20f;
    case SFX_SECTION: return (sinf(osc(&p[0], 700)) + 0.6f * sinf(osc(&p[1], 1050))) * expf(-t * 40) * 0.15f;
    case SFX_OK: return sinf(osc(&p[0], k < 0.45f ? 880 : 1320)) * 0.20f * (1 - k * 0.7f);
    case SFX_BACK: return sinf(osc(&p[0], k < 0.45f ? 990 : 660)) * 0.18f * (1 - k * 0.7f);
    case SFX_LAUNCH: {
      float f = 220 + 1000 * k * k;
      return (sinf(osc(&p[0], f)) * 0.16f + sinf(osc(&p[1], f * 1.5f)) * 0.08f + nz * 0.05f * (1 - k)) * sinf(k * 3.14159f);
    }
    case SFX_ERROR: return sq(osc(&p[0], 180)) * 0.10f * (1 - k);
    }
  }
  return 0;
}

static float boot_sample(int v, float t, float k, float *p, float nz, float *lp) {
  switch (v) {
  case 1: {                                      /* Synthwave : nappe qui monte + arpege */
    static const float PAD[3] = { 220, 261.63f, 329.63f };
    float env = (k < 0.3f ? k / 0.3f : 1) * (k > 0.7f ? (1 - k) / 0.3f : 1), x = 0;
    for (int i = 0; i < 3; i++)
      for (int d = -1; d <= 1; d += 2) {
        float ph = osc(&p[i * 2 + (d > 0)], PAD[i] * (1 + d * 0.004f));
        x += sinf(ph) + 0.3f * sinf(2 * ph) + 0.15f * sinf(3 * ph);
      }
    static const float ARP[4] = { 440, 523.25f, 659.25f, 880 };
    float a = 0;
    for (int i = 0; i < 4; i++) { float tn = t - 0.25f - i * 0.18f; if (tn > 0) a += sinf(TAU * ARP[i] * tn) * expf(-tn * 6); }
    return x * 0.022f * env + a * 0.11f;
  }
  case 2: {                                      /* Console : impact grave puis accord lumineux */
    float boom = sinf(osc(&p[0], 40 + 70 * expf(-t * 3))) * 0.38f * expf(-t * 3.2f);
    float sh = 0;
    if (t > 0.3f) {
      float tn = t - 0.3f, env = (tn < 0.4f ? tn / 0.4f : 1) * (1 - k) * (0.85f + 0.15f * sinf(TAU * 5 * t));
      sh = (sinf(TAU * 523.25f * tn) + sinf(TAU * 659.25f * tn) + sinf(TAU * 783.99f * tn) + 0.6f * sinf(TAU * 1046.5f * tn)) * 0.045f * env;
    }
    return boom + sh;
  }
  case 3: {                                      /* 8-bit : petite fanfare en ondes carrees */
    static const float N[7] = { 523.25f, 659.25f, 783.99f, 1046.5f, 1318.5f, 1568, 2093 };
    int i = (int)(t / 0.07f); if (i > 6) i = 6;
    float tn = t - i * 0.07f;
    return sq(osc(&p[0], N[i])) * 0.07f * (i == 6 ? expf(-tn * 3) : 1);
  }
  case 4: {                                      /* Hyperespace : souffle qui accelere puis etincelle */
    *lp += (nz - *lp) * (0.02f + 0.2f * k);
    float sweep = k < 0.7f ? sinf(osc(&p[0], 100 + 1900 * (k / 0.7f) * (k / 0.7f))) * 0.14f * sinf(k / 0.7f * 3.14159f) : 0;
    return sweep + *lp * 0.5f * sinf(k * 3.14159f) + bell(2093, t - 0.95f, 5) * 0.10f + bell(3136, t - 1.02f, 6) * 0.06f;
  }
  default: {                                     /* Arpege neon : le son d'origine */
    static const float NOTES[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
    int note = (int)(t / 0.14f); if (note > 3) note = 3;
    float tn = t - note * 0.14f;
    return sinf(osc(&p[0], NOTES[note])) * 0.18f * expf(-tn * (note == 3 ? 3.0f : 9.0f)) + sinf(osc(&p[1], 130.81f)) * 0.05f * (1 - k);
  }
  }
}

/* fabrique un son : id < SFX_BOOT -> son d'interface du style `var`, sinon son de demarrage `var` */
static void sfx_build(int id, int var) {
  float dur = id == SFX_BOOT ? BOOT_DUR[var] : SFX_DUR[var][id];
  int n = (int)(dur * sfx_rate);
  if (n <= 0) return;
  Sint16 *buf = malloc((size_t)n * sfx_chans * sizeof(Sint16));
  if (!buf) return;
  float p[8] = { 0 }, lp = 0;
  unsigned seed = 0x1234567u;
  for (int i = 0; i < n; i++) {
    float t = (float)i / sfx_rate, k = t / dur;
    seed = seed * 1103515245u + 12345u;
    float nz = ((seed >> 16) & 0x7fff) / 16384.0f - 1;
    float v = id == SFX_BOOT ? boot_sample(var, t, k, p, nz, &lp) : sfx_sample(id, var, t, k, p, nz);
    float atk = t < 0.003f ? t / 0.003f : 1, end = k > 0.92f ? (1 - k) / 0.08f : 1;
    int sv = (int)(v * atk * end * 32767);
    if (sv > 32767) sv = 32767;
    if (sv < -32768) sv = -32768;
    for (int c = 0; c < sfx_chans; c++) buf[i * sfx_chans + c] = (Sint16)sv;
  }
  SFXC[id] = Mix_QuickLoad_RAW((Uint8 *)buf, (Uint32)(n * sfx_chans * sizeof(Sint16)));
  if (SFXC[id]) SFXB[id] = buf; else free(buf);
}

static void sfx_drop(int id) {
  if (SFXC[id]) Mix_FreeChunk(SFXC[id]);         /* coupe aussi le son s'il joue encore */
  SFXC[id] = NULL;
  free(SFXB[id]); SFXB[id] = NULL;
}

static void sfx(int id) {
  int vol = S.th ? S.th->sfx : 0;
  if (vol <= 0 || id < 0 || id >= SFX_COUNT) return;
  if (!sfx_state) {
    sfx_state = -1;
    Uint16 fmt;
    if (!music_ready() || !Mix_QuerySpec(&sfx_rate, &fmt, &sfx_chans) || fmt != AUDIO_S16SYS) { plat_log("sons d'interface indisponibles"); return; }
    Mix_AllocateChannels(8);
    sfx_state = 1;
  }
  if (sfx_state < 0) return;
  if (id == SFX_BOOT) {                          /* son de demarrage choisi dans les parametres */
    if (sfx_boot_built != S.menu.boot_snd) {
      sfx_drop(SFX_BOOT);
      sfx_boot_built = S.menu.boot_snd;
      sfx_build(SFX_BOOT, sfx_boot_built);
    }
  } else if (sfx_pack_built != S.menu.sfx_pack) {  /* style des sons change : on refait les sons */
    for (int i = 0; i < SFX_BOOT; i++) sfx_drop(i);
    sfx_pack_built = S.menu.sfx_pack;
    for (int i = 0; i < SFX_BOOT; i++) sfx_build(i, sfx_pack_built);
  }
  if (!SFXC[id]) return;
  Mix_VolumeChunk(SFXC[id], vol * MIX_MAX_VOLUME / 100);
  Mix_PlayChannel(-1, SFXC[id], 0);
}
#else
static void music_update(void) {}
static void music_pause(int on) { (void)on; }
static void music_fade(void) {}
static void sfx(int id) { (void)id; }
#endif

static char letter_of(Entry *e) {
  const unsigned char *n = (const unsigned char *)menu_display_name(&S.menu, e->app);
  unsigned c = n[0];
  if (c >= 'a' && c <= 'z') return (char)(c - 32);
  if (c >= 'A' && c <= 'Z') return (char)c;
  if (c == 0xC3 && n[1]) {                       /* lettres accentuees courantes */
    unsigned d = n[1] & ~0x20u;
    if (d >= 0x80 && d <= 0x86) return 'A';
    if (d == 0x87) return 'C';
    if (d >= 0x88 && d <= 0x8B) return 'E';
    if (d >= 0x8C && d <= 0x8F) return 'I';
    if (d >= 0x92 && d <= 0x96) return 'O';
    if (d >= 0x99 && d <= 0x9C) return 'U';
  }
  return '#';
}

static void jump_letter(int dir) {
  if (S.mode != MODE_BROWSE || !SEC_SORTED_BY_NAME(S.sec) || S.nentries < 2) return;
  int *sel = cur_sel(), n = S.nentries, i = *sel;
  char cur = letter_of(&S.entries[i]);
  if (dir > 0) {
    while (i < n && letter_of(&S.entries[i]) == cur) i++;
    if (i >= n) i = 0;
  } else {
    int j = i;
    while (j > 0 && letter_of(&S.entries[j - 1]) == cur) j--;
    if (j < i) i = j;
    else {
      i = j == 0 ? n - 1 : j - 1;
      char p = letter_of(&S.entries[i]);
      while (i > 0 && letter_of(&S.entries[i - 1]) == p) i--;
    }
  }
  *sel = i;
  S.letter[0] = letter_of(&S.entries[i]); S.letter[1] = 0;
  S.letter_until = SDL_GetTicks() + 750;
  S.sel_anim = 0.3f;
}

static void change_section(int dir) {
  search_q[0] = 0;                               /* nouvelle section : fin de la recherche */
  transition(dir * 90, 0);
  int k = sec_pos(S.sec);
  for (int i = 0; i < SEC_COUNT; i++) {
    k = (k + dir + SEC_COUNT) % SEC_COUNT;
    if (S.sec_has[SEC_ORDER[k]]) break;
  }
  S.sec = SEC_ORDER[k];
  S.scroll = (float)*cur_sel();
}

static void move_cursor(int dx, int dy) {
  if (S.mode == MODE_CONTEXT) { S.csel = (S.csel + dy + S.nchoices) % S.nchoices; return; }
  if (S.mode == MODE_PICKFOLDER) { S.fsel = (S.fsel + dy + S.nfolders) % S.nfolders; return; }
  if (S.mode == MODE_COLOR) { S.color_sel = (S.color_sel + dy + NPALETTE) % NPALETTE; return; }
  if (S.mode == MODE_KEYBOARD) {
    S.kb_row = (S.kb_row + dy + 6) % 6;
    int max = S.kb_row == KB_SPECIAL_ROW ? 4 : 10;
    if (dy && S.kb_row == KB_SPECIAL_ROW) S.kb_col = S.kb_col * 4 / 10;
    else if (dy && S.kb_row - dy == KB_SPECIAL_ROW) S.kb_col = S.kb_col * 10 / 4 + 1;
    S.kb_col = (S.kb_col + dx + max) % max;
    return;
  }
  int *sel = cur_sel();
  if (XMBMODE) { int t = dx; dx = dy; dy = t; }   /* XMB : haut / bas dans la liste, gauche / droite les categories */
  if (dy && S.menu.view == VIEW_GRID && !GXMODE && !XMBMODE) {
    if (S.mode != MODE_BROWSE) return;            /* en deplacement : gauche / droite seulement */
    int ns = *sel + dy * GRID_COLS;
    if (ns >= S.nentries && *sel / GRID_COLS < (S.nentries - 1) / GRID_COLS) ns = S.nentries - 1;
    if (ns < 0 || ns >= S.nentries) return;
    *sel = ns;
    S.sel_anim = 0.3f;
    return;
  }
  if (dy) { if (S.mode == MODE_BROWSE) change_section(dy); return; }
  int ns = *sel + dx;
  if (ns < 0 || ns >= S.nentries) return;
  if (S.mode == MODE_MOVE) {
    Node *parent = current_node();
    int a = S.entries[*sel].index, b = S.entries[ns].index;
    Node tmp = parent->items[a]; parent->items[a] = parent->items[b]; parent->items[b] = tmp;
  }
  *sel = ns;
  S.sel_anim = 0.3f;
}


/* ------------------------------------------------------------------ */
/* Mise a jour : derniere version publiee sur GitHub, verifiee par son  */
/* empreinte SHA-256, gardee dans /data/launcher5/launcher5.elf puis    */
/* envoyee au chargeur ELF de la console.                               */
/* ------------------------------------------------------------------ */
#define UPDATE_API "https://api.github.com/repos/cedsaill3/Launcher5/releases/latest"
enum { UP_IDLE, UP_CHECKING, UP_LATEST, UP_AVAILABLE, UP_DOWNLOADING, UP_READY, UP_ERROR };
static struct {
  SDL_mutex *mx;
  volatile int state, busy, notify, install;     /* install : envoyer des que la version est prete */
  char ver[24], url[600], digest[72], err[96];
} UP;

static const char *run_version(void) {           /* L5_VERSION_TEST : essais de mise a jour sur PC */
  const char *e = getenv("L5_VERSION_TEST");
  return e && *e ? e : L5_VERSION;
}

/* compare deux versions « v2.1.0 » / « 2.0.1 » */
static int ver_cmp(const char *a, const char *b) {
  if (*a == 'v' || *a == 'V') a++;
  if (*b == 'v' || *b == 'V') b++;
  for (int i = 0; i < 4; i++) {
    long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
    if (x != y) return x < y ? -1 : 1;
    if (*a == '.') a++;
    if (*b == '.') b++;
  }
  return 0;
}

static void up_path(char *out, size_t n, const char *name) { snprintf(out, n, "%s/%s", plat_data_dir(), name); }

static void up_set(int state, const char *err) {
  SDL_LockMutex(UP.mx);
  UP.state = state;
  if (err) snprintf(UP.err, sizeof UP.err, "%s", err);
  UP.notify = 1;
  SDL_UnlockMutex(UP.mx);
}

static void up_check(void) {
  unsigned char *d; size_t n;
  long code = plat_https_get(UPDATE_API, &d, &n, 1 << 20);
  char path[PATH_LEN];
  up_path(path, sizeof path, "cache/update_check");
  FILE *f = fopen(path, "w");                    /* derniere verification (une par jour) */
  if (f) fclose(f);
  if (code != 200) { up_set(UP_ERROR, code ? "GitHub ne répond pas correctement" : "Pas d'accès à Internet"); return; }
  json_t *j = json_loadb((const char *)d, n, 0, NULL), *a;
  free(d);
  const char *tag = json_string_value(json_object_get(j, "tag_name"));
  char ver[24] = "", url[600] = "", dig[72] = "";
  size_t k;
  if (tag) snprintf(ver, sizeof ver, "%s", tag[0] == 'v' ? tag + 1 : tag);
  json_array_foreach(json_object_get(j, "assets"), k, a) {
    const char *nm = json_string_value(json_object_get(a, "name"));
    const char *u = json_string_value(json_object_get(a, "browser_download_url"));
    const char *dg = json_string_value(json_object_get(a, "digest"));
    if (nm && u && dg && !strcmp(nm, "launcher5.elf") && !strncmp(dg, "sha256:", 7) &&
        !strncmp(u, "https://github.com/cedsaill3/Launcher5/releases/download/", 56)) {
      snprintf(url, sizeof url, "%s", u);
      snprintf(dig, sizeof dig, "%s", dg + 7);
    }
  }
  json_decref(j);
  if (!ver[0] || !url[0]) { up_set(UP_ERROR, "Version publiée illisible"); return; }
  SDL_LockMutex(UP.mx);
  snprintf(UP.ver, sizeof UP.ver, "%s", ver);
  snprintf(UP.url, sizeof UP.url, "%s", url);
  snprintf(UP.digest, sizeof UP.digest, "%s", dig);
  SDL_UnlockMutex(UP.mx);
  up_set(ver_cmp(ver, run_version()) > 0 ? UP_AVAILABLE : UP_LATEST, NULL);
  plat_log("mise a jour : version publiee %s, version actuelle %s", ver, run_version());
}

static void up_download(void) {
  char url[600], dig[72], ver[24];
  SDL_LockMutex(UP.mx);
  memcpy(url, UP.url, sizeof url); memcpy(dig, UP.digest, sizeof dig); memcpy(ver, UP.ver, sizeof ver);
  SDL_UnlockMutex(UP.mx);
  up_set(UP_DOWNLOADING, NULL);
  unsigned char *d; size_t n;
  long code = plat_https_get(url, &d, &n, 64 << 20);
  if (code != 200) { up_set(UP_ERROR, code ? "Téléchargement refusé par GitHub" : "Téléchargement interrompu"); return; }
  char hex[65];
  sha256_hex(d, n, hex);
  if (strcmp(hex, dig) || n < 4 || memcmp(d, "\x7f" "ELF", 4)) {
    free(d);
    plat_log("mise a jour : empreinte incorrecte, fichier refuse");
    up_set(UP_ERROR, "Fichier abîmé : mise à jour refusée");
    return;
  }
  char tmp[PATH_LEN], dst[PATH_LEN], vf[PATH_LEN];
  up_path(tmp, sizeof tmp, "launcher5.elf.tmp");
  up_path(dst, sizeof dst, "launcher5.elf");
  up_path(vf, sizeof vf, "launcher5.version");
  FILE *f = fopen(tmp, "wb");
  size_t w = f ? fwrite(d, 1, n, f) : 0;
  if (f) fclose(f);
  free(d);
  if (w != n || rename(tmp, dst)) { remove(tmp); up_set(UP_ERROR, "Écriture impossible dans /data/launcher5"); return; }
  if ((f = fopen(vf, "w"))) { fprintf(f, "%s\n", ver); fclose(f); }
  plat_log("mise a jour : version %s prete (%s)", ver, dst);
  up_set(UP_READY, NULL);
}

static int up_thread(void *u) {
  if (u) up_download(); else up_check();
  UP.busy = 0;
  return 0;
}

static void up_start(int download) {
  if (!UP.mx && !(UP.mx = SDL_CreateMutex())) return;
  if (UP.busy) return;
  UP.busy = 1;
  if (!download) up_set(UP_CHECKING, NULL);
  SDL_Thread *th = SDL_CreateThread(up_thread, "update", download ? (void *)1 : NULL);
  if (th) SDL_DetachThread(th); else { UP.busy = 0; up_set(UP_ERROR, "Pas de fil disponible"); }
}


/* ------------------------------------------------------------------ */
/* Catalogue de payloads : liste publique ps5-payloads-mirror (la meme  */
/* que Payload Manager). Chaque fichier est verifie par son empreinte   */
/* SHA-256 avant d'etre installe dans /data/payloads ; rien n'est       */
/* envoye tout seul, l'envoi reste un choix (✕ dans la section).        */
/* ------------------------------------------------------------------ */
#define PCAT_URL "https://itsplk.github.io/ps5-payloads-mirror/payloads_v2.json"
typedef struct { char name[64], file[96], url[400], sha[72], ver[24], cat[48], desc[160]; SDL_Texture *tex; int tried; } PCat;
static struct {
  PCat *it, *next; int n, next_n;
  SDL_atomic_t ready, job_ready;                 /* resultats du fil, traites par l'ecran */
  volatile int state;                            /* 0 rien, 1 chargement, 2 pret, 3 erreur */
  volatile int job;                              /* 1 : telechargement en cours */
  int job_ok, job_idx, offline, sel, top;
  Uint32 loaded_at;
  char err[120], job_msg[160], job_file[96];
} PCT;

static void pcat_file_name(const char *name, char *out, size_t n) {   /* nom de fichier sur : <nom>.elf */
  size_t k = 0;
  for (const char *p = name; *p && k + 5 < n; p++)
    out[k++] = (isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.') ? *p : '_';
  if (!k) out[k++] = 'p';
  snprintf(out + k, n - k, ".elf");
}

static int pcat_parse(json_t *j, PCat **out) {
  *out = NULL;
  int n = 0;
  size_t i; json_t *v;
  json_array_foreach(json_object_get(j, "payloads"), i, v) {
    const char *nm = json_string_value(json_object_get(v, "name"));
    const char *url = json_string_value(json_object_get(v, "url"));
    const char *sha = json_string_value(json_object_get(v, "checksum"));
    if (!nm || !*nm || !url || strncmp(url, "https://", 8) || !sha || strlen(sha) != 64) continue;
    if (payload_hidden(nm)) continue;            /* inutile dans un launcher */
    int hex = 1;
    for (const char *p = sha; *p; p++) if (!isxdigit((unsigned char)*p)) hex = 0;
    if (!hex) continue;
    PCat *r = realloc(*out, sizeof **out * (n + 1));
    if (!r) break;
    *out = r;
    PCat *c = &r[n++];
    memset(c, 0, sizeof *c);
    snprintf(c->name, sizeof c->name, "%s", nm);
    pcat_file_name(nm, c->file, sizeof c->file);
    snprintf(c->url, sizeof c->url, "%s", url);
    for (int k = 0; k < 64; k++) c->sha[k] = (char)tolower((unsigned char)sha[k]);
    const char *s;
    if ((s = json_string_value(json_object_get(v, "version")))) snprintf(c->ver, sizeof c->ver, "%s", s);
    if ((s = json_string_value(json_object_get(v, "category")))) snprintf(c->cat, sizeof c->cat, "%s", s);
    if ((s = json_string_value(json_object_get(v, "description")))) snprintf(c->desc, sizeof c->desc, "%s", s);
  }
  return n;
}

static void pcat_save(const char *name, const unsigned char *d, size_t n) {
  char p[PATH_LEN], t[PATH_LEN + 8];
  snprintf(p, sizeof p, "%s/cache/%s", plat_data_dir(), name);
  snprintf(t, sizeof t, "%s.tmp", p);
  FILE *f = fopen(t, "wb");
  size_t w = f ? fwrite(d, 1, n, f) : 0;
  if (f) fclose(f);
  if (w != n || rename(t, p)) remove(t);
}

static int pcat_fetch_thread(void *u) {
  (void)u;
  unsigned char *d; size_t n;
  PCat *it = NULL;
  int cnt = 0, offline = 0;
  long code = plat_https_get(PCAT_URL, &d, &n, 2 << 20);
  if (code == 200) {
    json_t *j = json_loadb((const char *)d, n, 0, NULL);
    cnt = pcat_parse(j, &it);
    if (cnt) {
      pcat_save("payloads_index.json", d, n);
      const char *iu = json_string_value(json_object_get(j, "icons_url"));
      char iurl[400] = "";
      if (iu && !strncmp(iu, "https://", 8)) snprintf(iurl, sizeof iurl, "%s", iu);
      else if (iu && *iu && *iu != '/' && !strstr(iu, "..") && !strchr(iu, ':')) {   /* relative a la liste */
        const char *sl = strrchr(PCAT_URL, '/');
        snprintf(iurl, sizeof iurl, "%.*s/%s", (int)(sl - PCAT_URL), PCAT_URL, iu);
      }
      unsigned char *id; size_t in;
      if (iurl[0] && plat_https_get(iurl, &id, &in, 8 << 20) == 200) {
        json_t *ij = json_loadb((const char *)id, in, 0, NULL);
        if (json_is_object(json_object_get(ij, "icons"))) pcat_save("payloads_icons.json", id, in);
        json_decref(ij);
        free(id);
      }
    }
    json_decref(j);
    free(d);
  }
  if (!cnt) {                                    /* pas de reseau : derniere liste connue */
    char p[PATH_LEN];
    snprintf(p, sizeof p, "%s/cache/payloads_index.json", plat_data_dir());
    json_t *j = json_load_file(p, 0, NULL);
    cnt = pcat_parse(j, &it);
    json_decref(j);
    offline = cnt > 0;
  }
  PCT.next = it; PCT.next_n = cnt; PCT.offline = offline;
  if (!cnt) snprintf(PCT.err, sizeof PCT.err, "%s", code ? "La liste des payloads est illisible" : "Pas d'accès à Internet");
  SDL_AtomicSet(&PCT.ready, 1);
  return 0;
}

static void pcat_fetch(void) {
  if (PCT.state == 1) return;
  PCT.state = 1;
  SDL_Thread *t = SDL_CreateThread(pcat_fetch_thread, "l5catalog", NULL);
  if (t) SDL_DetachThread(t); else { PCT.state = 3; snprintf(PCT.err, sizeof PCT.err, "Pas de fil disponible"); }
}

/* telecharge un payload dans /data/payloads, empreinte SHA-256 verifiee (0 : installe) */
static int pcat_download(const PCat *c, const char *source, char *msg, size_t mn) {
  unsigned char *d; size_t n;
  int ok = 0;
  long code = plat_https_get(c->url, &d, &n, 64 << 20);
  if (code != 200) snprintf(msg, mn, "%s", code ? "Téléchargement refusé" : "Téléchargement interrompu");
  else {
    char hex[65];
    sha256_hex(d, n, hex);
    if (strcmp(hex, c->sha) || n < 4 || memcmp(d, "\x7f" "ELF", 4)) {
      snprintf(msg, mn, "%s", "Empreinte incorrecte : fichier refusé");
      plat_log("catalogue : empreinte incorrecte pour %s, fichier refuse", c->name);
    } else {
      char dir[PATH_LEN], dst[PATH_LEN + 100], tmp[PATH_LEN + 110], side[PATH_LEN + 110];
      snprintf(dir, sizeof dir, "%s/data/payloads", plat_root());
      mkdir(dir, 0755);
      snprintf(dst, sizeof dst, "%s/%s", dir, c->file);
      snprintf(tmp, sizeof tmp, "%s.tmp", dst);
      snprintf(side, sizeof side, "%s.json", dst);
      FILE *f = fopen(tmp, "wb");
      size_t w = f ? fwrite(d, 1, n, f) : 0;
      if (f) fclose(f);
      if (w == n && !rename(tmp, dst)) {
        json_t *m = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s}", "name", c->name, "id", c->name, "filename", c->file,
                              "version", c->ver, "category", c->cat, "description", c->desc, "source", source);
        if (m) { json_dump_file(m, side, JSON_INDENT(2)); json_decref(m); }
        ok = 1;
        plat_log("catalogue : %s %s installe (%zu octets, empreinte verifiee)", c->name, c->ver, n);
      } else {
        remove(tmp);
        snprintf(msg, mn, "%s", "Écriture impossible dans /data/payloads");
      }
    }
    free(d);
  }
  return ok ? 0 : -1;
}

static int pcat_job_thread(void *u) {
  PCat *c = u;
  int ok = !pcat_download(c, "ps5-payloads-mirror", PCT.job_msg, sizeof PCT.job_msg);
  PCT.job_ok = ok;
  snprintf(PCT.job_file, sizeof PCT.job_file, "%s", c->file);
  free(c);
  SDL_AtomicSet(&PCT.job_ready, 1);
  return 0;
}

/* ------------------------------------------------------------------ */
/* Premier lancement : console prete a jouer. Le bootstrap lance cette  */
/* partie sans ecran (« --prepare »), avant l'interface : kstuff-lite  */
/* et ShadowMountPlus sont telecharges (empreintes verifiees) et        */
/* ajoutes a la liste « Au demarrage ». Le bootstrap les envoie ensuite */
/* lui-meme, sans interface ouverte.                                    */
/* ------------------------------------------------------------------ */
enum { ST_KSTUFF, ST_SHADOW, ST_N };

static int find_elfldr(char *out, size_t n);
static int is_elf_file(const char *p);
static int copy_file(const char *src, const char *dst);
static void al_path(char *o, size_t n, const char *f);

/* copie deja presente sur la console (Payload Manager, etaHEN, /data/payloads, autoloader),
   quand le telechargement est impossible (console sans Internet) */
static int local_find(const char *dir, const char *prefix, const char *skip, int depth, char *out, size_t n) {
  DIR *d = opendir(dir);
  if (!d) return -1;
  struct dirent *e;
  int found = -1;
  while (found && (e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char p[PATH_LEN + 260];
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    struct stat st;
    if (stat(p, &st)) continue;
    if (S_ISDIR(st.st_mode)) { if (depth > 0) found = local_find(p, prefix, skip, depth - 1, out, n); continue; }
    char low[256];
    size_t k = 0;
    for (; e->d_name[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)e->d_name[k]);
    low[k] = 0;
    if (strncmp(low, prefix, strlen(prefix)) || k < 5 || strcmp(low + k - 4, ".elf")) continue;
    if (skip && !strcasecmp(e->d_name, skip)) continue;
    if (!is_elf_file(p)) continue;
    snprintf(out, n, "%s", p);
    found = 0;
  }
  closedir(d);
  return found;
}

/* installe la copie locale comme si elle venait du catalogue (meme nom, meme chemin) */
static int starter_local(const char *name, const char *prefix, PCat *c) {
  char src[PATH_LEN + 260] = "", dir[PATH_LEN], al[PATH_LEN];
  memset(c, 0, sizeof *c);
  snprintf(c->name, sizeof c->name, "%s", name);
  pcat_file_name(name, c->file, sizeof c->file);
  al_path(al, sizeof al, NULL);
  struct { const char *d; int depth; } D[] = { { "/data/pldmgr/payloads", 3 }, { "/data/etaHEN/payloads", 1 },
                                               { "/data/payloads", 1 }, { NULL, 0 } };
  for (int i = 0; i < 4 + 8 && !src[0]; i++) {
    if (i >= 4) snprintf(dir, sizeof dir, "%s/mnt/usb%d", plat_root(), i - 4);   /* cle USB */
    else if (D[i].d) snprintf(dir, sizeof dir, "%s%s", plat_root(), D[i].d);
    else snprintf(dir, sizeof dir, "%s", al);
    if (local_find(dir, prefix, i == 2 ? c->file : NULL, i >= 4 ? 2 : D[i].depth, src, sizeof src)) src[0] = 0;   /* pas soi-meme */
  }
  if (!src[0]) return -1;
  char dst[PATH_LEN + 120], side[PATH_LEN + 130];
  snprintf(dir, sizeof dir, "%s/data/payloads", plat_root());
  mkdir(dir, 0755);
  snprintf(dst, sizeof dst, "%s/%s", dir, c->file);
  if (copy_file(src, dst)) return -1;
  snprintf(side, sizeof side, "%s.json", dst);
  json_t *m = json_pack("{s:s, s:s, s:s, s:s}", "name", name, "id", name, "filename", c->file, "source", "local");
  if (m) { json_dump_file(m, side, JSON_INDENT(2)); json_decref(m); }
  plat_log("premier lancement : %s pris sur la console (%s)", name, src);
  return 0;
}

static int starter_current(const PCat *c) {      /* deja dans /data/payloads, meme version */
  char p[PATH_LEN + 120];
  snprintf(p, sizeof p, "%s/data/payloads/%s", plat_root(), c->file);
  if (access(p, F_OK)) return 0;
  strcat(p, ".json");
  json_t *m = json_load_file(p, 0, NULL);
  const char *v = json_string_value(json_object_get(m, "version"));
  int same = v && *v && !strcmp(v, c->ver);
  json_decref(m);
  return same;
}

static int name_has(const char *s, const char *k) {
  char a[NAME_LEN];
  size_t i = 0;
  for (; s[i] && i + 1 < sizeof a; i++) a[i] = (char)tolower((unsigned char)s[i]);
  a[i] = 0;
  return strstr(a, k) != NULL;
}

static void starter_status(const char *s) {
  char p[PATH_LEN], t[PATH_LEN + 8];
  snprintf(p, sizeof p, "%s/cache/starter_status", plat_data_dir());
  snprintf(t, sizeof t, "%s.tmp", p);
  FILE *f = fopen(t, "w");
  if (f) { fprintf(f, "%s\n", s); fclose(f); rename(t, p); }
}

static int starter_prepare(void) {
  char dir[PATH_LEN], mark[PATH_LEN];
  snprintf(dir, sizeof dir, "%s/cache", plat_data_dir());
  mkdir(plat_data_dir(), 0755);
  mkdir(dir, 0755);
  snprintf(mark, sizeof mark, "%s/starter_done", dir);
  if (!access(mark, F_OK)) { starter_status("done"); return 0; }
  plat_log("premier lancement : preparation de kstuff-lite et ShadowMountPlus");

  PCat want[ST_N];
  int have[ST_N] = { 0 }, got[ST_N] = { 0 };
  memset(want, 0, sizeof want);
  unsigned char *d; size_t n;
  PCat *it = NULL;
  int cnt = 0;
  if (plat_https_get(PCAT_URL, &d, &n, 2 << 20) == 200) {
    json_t *j = json_loadb((const char *)d, n, 0, NULL);
    cnt = pcat_parse(j, &it);
    if (cnt) pcat_save("payloads_index.json", d, n);
    json_decref(j);
    free(d);
  }
  if (!cnt) {                                    /* pas de reseau : derniere liste connue */
    char p[PATH_LEN];
    snprintf(p, sizeof p, "%s/payloads_index.json", dir);
    json_t *j = json_load_file(p, 0, NULL);
    cnt = pcat_parse(j, &it);
    json_decref(j);
  }
  static const char *const CAT_NAMES[2] = { "kstuff-lite", "ShadowMountPlus" };
  for (int k = 0; k < 2; k++)
    for (int i = 0; i < cnt; i++)
      if (!strcasecmp(it[i].name, CAT_NAMES[k])) { want[k] = it[i]; want[k].tex = NULL; have[k] = 1; break; }
  /* sans chargeur ELF (port 9021), rien ne peut partir au demarrage : on en installe un */
  char ldr[PATH_LEN];
  if (find_elfldr(ldr, sizeof ldr))
    for (int i = 0; i < cnt; i++)
      if (!strcasecmp(it[i].name, "elfldr")) {
        char lm[160] = "";
        if (pcat_download(&it[i], "ps5-payloads-mirror", lm, sizeof lm)) plat_log("premier lancement : elfldr non installe (%s)", lm);
        else plat_log("premier lancement : elfldr installe (aucun chargeur ELF sur la console)");
        break;
      }
  free(it);

  char msg[160];
  for (int k = 0; k < ST_N; k++) {
    if (!have[k]) { plat_log("premier lancement : %s introuvable (catalogue ou reseau)", CAT_NAMES[k]); continue; }
    if (starter_current(&want[k])) { got[k] = 1; continue; }
    msg[0] = 0;
    got[k] = !pcat_download(&want[k], "ps5-payloads-mirror", msg, sizeof msg);
    if (!got[k]) plat_log("premier lancement : %s non installe (%s)", want[k].name, msg);
  }
  /* sans Internet : les copies deja sur la console */
  static const char *const PREFIX[2] = { "kstuff", "shadowmount" };
  for (int k = 0; k < ST_N; k++)
    if (!got[k] && !starter_local(CAT_NAMES[k], PREFIX[k], &want[k])) got[k] = 1;

  /* liste « Au demarrage » : kstuff en premier, puis la liste actuelle, puis les monteurs */
  char lp[PATH_LEN], tp[PATH_LEN + 8];
  snprintf(lp, sizeof lp, "%s/autostart.json", plat_data_dir());
  json_t *old = json_load_file(lp, 0, NULL);
  json_t *on = json_object_get(old, "names"), *op = json_object_get(old, "paths");
  json_t *names = json_array(), *paths = json_array();
  int any_kstuff = 0;
  size_t i; json_t *v;
  json_array_foreach(on, i, v) if (json_string_value(v) && name_has(json_string_value(v), "kstuff")) any_kstuff = 1;
  char path[PATH_LEN + 120];
  int added = 0;
  if (got[ST_KSTUFF] && !any_kstuff) {
    snprintf(path, sizeof path, "%s/data/payloads/%s", plat_root(), want[ST_KSTUFF].file);
    json_array_append_new(names, json_string(want[ST_KSTUFF].name));
    json_array_append_new(paths, json_string(path));
    added++;
  }
  if (json_is_array(on)) json_array_extend(names, on);
  if (json_is_array(op)) json_array_extend(paths, op);
  for (int k = ST_SHADOW; k < ST_N; k++) {
    if (!got[k]) continue;
    int present = 0;
    json_array_foreach(on, i, v) if (json_string_value(v) && !strcasecmp(json_string_value(v), want[k].name)) present = 1;
    if (present) continue;
    snprintf(path, sizeof path, "%s/data/payloads/%s", plat_root(), want[k].file);
    json_array_append_new(names, json_string(want[k].name));
    json_array_append_new(paths, json_string(path));
    added++;
  }
  json_t *j = json_pack("{s:o, s:o}", "names", names, "paths", paths);
  snprintf(tp, sizeof tp, "%s.tmp", lp);
  if (j && !json_dump_file(j, tp, JSON_INDENT(2))) rename(tp, lp);
  json_decref(j);
  json_decref(old);

  int all = got[ST_KSTUFF] && got[ST_SHADOW];
  char tp2[PATH_LEN];
  snprintf(tp2, sizeof tp2, "%s/starter_tries", dir);
  int tries = 0;
  FILE *tf = fopen(tp2, "r");
  if (tf) { if (fscanf(tf, "%d", &tries) != 1) tries = 0; fclose(tf); }
  if ((got[0] || got[1]) && (tf = fopen(tp2, "w"))) { fprintf(tf, "%d\n", ++tries); fclose(tf); }   /* sans reseau : on ne compte pas */
  if (all || tries >= 3) { FILE *m = fopen(mark, "w"); if (m) { fputs("1\n", m); fclose(m); } }
  plat_log("premier lancement : %d/2 payloads prets, %d ajoute(s) a la liste au demarrage",
           got[0] + got[1], added);
  starter_status(all ? "ok" : (got[0] || got[1]) ? "partial" : "fail");
  return 0;
}

/* resultat du premier lancement, ecrit par le bootstrap : affiche une fois */
static void starter_note(void) {
  char p[PATH_LEN], s[16] = "";
  snprintf(p, sizeof p, "%s/cache/starter_note", plat_data_dir());
  FILE *f = fopen(p, "r");
  if (!f) return;
  if (!fgets(s, sizeof s, f)) s[0] = 0;
  fclose(f);
  remove(p);
  s[strcspn(s, "\r\n")] = 0;
  const char *m = !strcmp(s, "sent") ? "Console prête : kstuff et ShadowMount sont lancés, et repartiront à chaque allumage"
                : !strcmp(s, "next") ? "kstuff et ShadowMount partiront à chaque allumage de la console"
                : !strcmp(s, "partial") ? "Préparation incomplète : nouvel essai au prochain lancement"
                : !strcmp(s, "fail") ? "Pas d'accès à Internet : kstuff et ShadowMount seront installés au prochain lancement"
                : NULL;
  if (!m) return;
  toast(m);
  S.toast_until = SDL_GetTicks() + 9000;
}

static App *pcat_installed(const PCat *c) {      /* deja present : copie de Launcher5 d'abord, sinon d'un autre gestionnaire */
  App *other = NULL;
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    if (a->kind != KIND_PAYLOAD) continue;
    const char *b = strrchr(a->path, '/');
    if (!strcasecmp(a->title, c->name) || (b && !strcasecmp(b + 1, c->file))) {
      if (pay_ours(a)) return a;
      if (!other) other = a;
    }
  }
  return other;
}

static void pcat_install(int i) {
  if (i < 0 || i >= PCT.n) return;
  if (PCT.job) { toast("Un téléchargement est déjà en cours"); return; }
  PCat *c = &PCT.it[i];
  App *a = pcat_installed(c);
  if (a && a->ver[0] && !strcmp(a->ver, c->ver)) { toast("Déjà à jour"); return; }
  PCat *copy = malloc(sizeof *copy);
  if (!copy) return;
  *copy = *c;
  copy->tex = NULL;
  PCT.job = 1; PCT.job_idx = i; PCT.job_msg[0] = 0;
  SDL_Thread *t = SDL_CreateThread(pcat_job_thread, "l5install", copy);
  if (t) SDL_DetachThread(t); else { free(copy); PCT.job = 0; toast("Pas de fil disponible"); }
}

static void pcat_free(PCat *it, int n) {
  for (int i = 0; i < n; i++) if (it[i].tex) SDL_DestroyTexture(it[i].tex);
  free(it);
}

/* resultats des fils, dans le fil de l'ecran */
static void pcat_poll(void) {
  if (SDL_AtomicGet(&PCT.ready)) {
    SDL_AtomicSet(&PCT.ready, 0);
    if (PCT.next_n) {
      pcat_free(PCT.it, PCT.n);
      PCT.it = PCT.next; PCT.n = PCT.next_n;
      PCT.state = 2; PCT.loaded_at = SDL_GetTicks();
      if (PCT.sel >= PCT.n) PCT.sel = PCT.n - 1;
      payload_icons_reset();                     /* icones de la nouvelle liste */
    } else PCT.state = 3;
    PCT.next = NULL; PCT.next_n = 0;
  }
  if (SDL_AtomicGet(&PCT.job_ready)) {
    SDL_AtomicSet(&PCT.job_ready, 0);
    PCT.job = 0;
    if (PCT.job_ok) {
      scan_all(&S.menu);                         /* le nouveau payload rejoint la section */
      char id[ID_LEN];
      snprintf(id, sizeof id, "elf:%.35s", PCT.job_file);
      App *a = menu_find_app(&S.menu, id);
      if (a && PCT.job_idx < PCT.n) {            /* deja connu (mise a jour) : fiche et jaquette refaites */
        PCat *c = &PCT.it[PCT.job_idx];
        snprintf(a->title, sizeof a->title, "%s", c->name);
        snprintf(a->ver, sizeof a->ver, "%s", c->ver);
        snprintf(a->cat, sizeof a->cat, "%s", c->cat);
        snprintf(a->desc, sizeof a->desc, "%s", c->desc);
        payload_icon_path(c->file, c->name, a->icon, sizeof a->icon);
        if (a->boxtex) { SDL_DestroyTexture(a->boxtex); a->boxtex = NULL; }
        a->box_state = 0; a->box_pref = 0;
        drop_tiles(a);
      }
      boot_refresh();                            /* la liste au demarrage suit la nouvelle version */
      char msg[200];
      snprintf(msg, sizeof msg, tr("« %s » installé  ·  empreinte vérifiée"), PCT.job_idx < PCT.n ? PCT.it[PCT.job_idx].name : "");
      toast(msg);
      sfx(SFX_OK);
    } else { toast(PCT.job_msg[0] ? PCT.job_msg : "Installation impossible"); sfx(SFX_ERROR); }
  }
}

static void open_catalog(void) {
  S.mode = MODE_CATALOG;
  if (PCT.state != 1 && (PCT.state != 2 || SDL_GetTicks() - PCT.loaded_at > 30 * 60 * 1000)) pcat_fetch();
}

static int pcat_rows(void) { return (H - 120 - 150 - 90) / 100; }

static void catalog_input(int in) {
  int vis = pcat_rows();
  switch (in) {
  case IN_UP: if (PCT.sel > 0) PCT.sel--; break;
  case IN_DOWN: if (PCT.sel < PCT.n - 1) PCT.sel++; break;
  case IN_L1: PCT.sel = PCT.sel > vis ? PCT.sel - vis : 0; break;
  case IN_R1: PCT.sel = PCT.sel + vis < PCT.n ? PCT.sel + vis : PCT.n - 1; break;
  case IN_OK: if (PCT.state == 2) pcat_install(PCT.sel); break;
  case IN_BACK: case IN_MENU: S.mode = MODE_BROWSE; break;
  }
  if (PCT.sel < 0) PCT.sel = 0;
}

static void draw_catalog(void) {
  int px = 90, py = 60, pw = W - 180, ph = H - 120;
  glass_panel(px, py, pw, ph);
  draw_text(S.f48, "Catalogue de payloads", px + 50, py + 34, 0xffffff, 0, pw - 100);
  char sub[200];
  snprintf(sub, sizeof sub, "%s%s", tr("Liste publique ps5-payloads-mirror  ·  chaque fichier est vérifié (SHA-256) avant installation"),
           PCT.offline ? tr("  ·  hors ligne : dernière liste connue") : "");
  draw_text(S.f22, sub, px + 52, py + 104, 0x9aa4b5, 0, pw - 100);
  draw_text(S.f22, "✕  Installer / mettre à jour      ○  Fermer      L1 R1  Page", px + 52, py + ph - 56, 0x9aa4b5, 0, 0);
  if (PCT.state == 1 && !PCT.n) { draw_text(S.f36, "Chargement de la liste…", W / 2, H / 2 - 20, 0xffffff, 1, 0); return; }
  if (PCT.state == 3 && !PCT.n) { draw_text(S.f36, PCT.err, W / 2, H / 2 - 20, 0xffffff, 1, pw - 100); return; }
  int vis = pcat_rows(), rh = 100, y0 = py + 150;
  if (PCT.sel < PCT.top) PCT.top = PCT.sel;
  if (PCT.sel >= PCT.top + vis) PCT.top = PCT.sel - vis + 1;
  for (int r = 0; r < vis && PCT.top + r < PCT.n; r++) {
    int i = PCT.top + r, y = y0 + r * rh, x = px + 40;
    PCat *c = &PCT.it[i];
    if (i == PCT.sel) {
      fill_round(px + 24, y - 6, pw - 48, rh - 8, 16, S.th->accent, 45);
      outline_round(px + 24, y - 6, pw - 48, rh - 8, 16, 2, S.th->accent, 200);
    }
    if (!c->tried) {                             /* icone : celle du depot, sinon symbole de la categorie */
      c->tried = 1;
      char p[PATH_LEN];
      if (!payload_icon_path(c->file, c->name, p, sizeof p)) c->tex = load_tex(p);
    }
    if (c->tex) {
      int tw, th;
      SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
      int dh = 76, dw = tw * dh / (th ? th : 1);
      if (dw > 120) { dw = 120; dh = th * dw / (tw ? tw : 1); }
      SDL_Rect d = { x + (120 - dw) / 2, y + (76 - dh) / 2 + 4, dw, dh };
      SDL_RenderCopy(S.r, c->tex, NULL, &d);
    } else {
      App tmp;
      memset(&tmp, 0, sizeof tmp);
      tmp.kind = KIND_PAYLOAD;
      snprintf(tmp.cat, sizeof tmp.cat, "%s", c->cat);
      uint32_t col; const char *g;
      payload_style(&tmp, &col, &g);
      fill_round(x + 10, y + 4, 100, 76, 14, shade(col, 0.6f), 255);
      draw_text(S.f36, g, x + 60, y + 18, 0xffffff, 1, 0);
    }
    int tx = x + 150, sw = 300;
    draw_text(S.f30, c->name, tx, y + 4, 0xffffff, 0, pw - 150 - sw - 120);
    char line[260];
    int bad = payload_excluded(c->name) || payload_excluded(c->url);   /* mode repos */
    if (bad) snprintf(line, sizeof line, "⊘ %s", tr("Incompatible avec Launcher5 : demande le mode repos"));
    else snprintf(line, sizeof line, "%s%s%s%s%s", c->ver, c->ver[0] && c->cat[0] ? "  ·  " : "", c->cat, c->desc[0] ? "  ·  " : "", c->desc);
    draw_text(S.f22, line, tx, y + 46, bad ? 0xff6b6b : 0xc9d1dc, 0, pw - 150 - sw - 120);
    /* etat */
    App *a = pcat_installed(c);
    char st[96];
    uint32_t sc;
    int filled;
    if (PCT.job && PCT.job_idx == i) { snprintf(st, sizeof st, "%s", tr("Téléchargement…")); sc = 0xffc53d; filled = 0; }
    else if (!a) { snprintf(st, sizeof st, "%s", tr("Installer")); sc = S.th->accent; filled = 1; }
    else if (a->ver[0] && strcmp(a->ver, c->ver)) { snprintf(st, sizeof st, tr("Mise à jour  %s"), c->ver); sc = 0xffc53d; filled = 1; }
    else if (a->ver[0]) { snprintf(st, sizeof st, tr("Installé  %s"), a->ver); sc = 0x2ecc71; filled = 0; }
    else { snprintf(st, sizeof st, "%s", tr("Installé")); sc = 0x2ecc71; filled = 0; }
    int stw = text_w(S.f22, st) + 40, sx = px + pw - 60 - stw;
    if (filled) fill_round(sx, y + 18, stw, 44, 22, sc, 235);
    else outline_round(sx, y + 18, stw, 44, 22, 2, sc, 230);
    draw_text(S.f22, st, sx + stw / 2, y + 26, filled ? 0x0a0a12 : sc, 1, 0);
  }
  if (PCT.n > vis) {                             /* barre de defilement */
    int bx = px + pw - 22, bh = vis * rh, kh = bh * vis / PCT.n, ky = y0 + (bh - kh) * PCT.top / (PCT.n - vis);
    fill_round(bx, y0, 6, bh, 3, 0xffffff, 40);
    fill_round(bx, ky, 6, kh, 3, S.th->accent, 220);
  }
}

static void up_install(void) {
  char path[PATH_LEN];
  up_path(path, sizeof path, "launcher5.elf");
  toast(tr("Installation de la mise à jour : le launcher redémarre…"));
  draw(); SDL_RenderPresent(S.r);
  save();
  char mark[PATH_LEN];                           /* une seule tentative automatique par version */
  up_path(mark, sizeof mark, "cache/update_tried");
  FILE *f = fopen(mark, "w");
  if (f) { fprintf(f, "%s\n", UP.ver); fclose(f); }
  if (plat_send_elf(path)) {
    toast(tr("Chargeur ELF introuvable : envoie /data/launcher5/launcher5.elf"));
    UP.install = 0;
    return;
  }
  plat_log("mise a jour : la version %s prend la main", UP.ver);
  plat_quit();
  fflush(NULL);
  _exit(0);                                      /* la nouvelle version relance l'interface */
}

/* deja essayee sans succes (la version installee ne demarre pas) : pas de nouvel essai automatique */
static int up_already_tried(const char *ver) {
  char mark[PATH_LEN], v[24] = "";
  up_path(mark, sizeof mark, "cache/update_tried");
  FILE *f = fopen(mark, "r");
  if (!f) return 0;
  if (!fgets(v, sizeof v, f)) v[0] = 0;
  fclose(f);
  v[strcspn(v, "\r\n")] = 0;
  return !strcmp(v, ver) && recent_file(mark, 1);
}

/* au demarrage : version deja telechargee, puis verification du jour */
static void up_boot(void) {
  if (!UP.mx && !(UP.mx = SDL_CreateMutex())) return;
  char vf[PATH_LEN], el[PATH_LEN], v[24] = "";
  up_path(vf, sizeof vf, "launcher5.version");
  up_path(el, sizeof el, "launcher5.elf");
  FILE *f = fopen(vf, "r");
  if (f) { if (!fgets(v, sizeof v, f)) v[0] = 0; fclose(f); v[strcspn(v, "\r\n")] = 0; }
  if (v[0] && ver_cmp(v, run_version()) > 0 && !access(el, R_OK)) {
    snprintf(UP.ver, sizeof UP.ver, "%s", v);
    /* plus recente que celle qui tourne (autoload vers un ancien fichier) : on l'installe */
    if (S.menu.auto_update && !up_already_tried(v)) UP.install = 1;
    up_set(UP_READY, NULL);
    return;
  }
  char chk[PATH_LEN];
  up_path(chk, sizeof chk, "cache/update_check");
  if (S.menu.auto_update && !recent_file(chk, 1)) up_start(0);
}

/* a chaque image : messages et installation demandee */
static void up_poll(void) {
  if (!UP.mx || !UP.notify) return;
  UP.notify = 0;
  char msg[160];
  switch (UP.state) {
  case UP_AVAILABLE:
    if (UP.install || (S.menu.auto_update && !up_already_tried(UP.ver))) {   /* automatique : telechargee puis installee */
      UP.install = 1;
      snprintf(msg, sizeof msg, tr("Mise à jour %s : téléchargement…"), UP.ver);
      toast(msg);
      up_start(1);
      break;
    }
    snprintf(msg, sizeof msg, tr("Mise à jour %s disponible : Paramètres › Mettre à jour"), UP.ver);
    toast(msg);
    break;
  case UP_READY:
    if (UP.install) up_install();
    else { snprintf(msg, sizeof msg, tr("Version %s prête : Paramètres › Mettre à jour"), UP.ver); toast(msg); }
    break;
  case UP_LATEST:
    if (UP.install) { toast(tr("Launcher5 est à jour")); UP.install = 0; }
    break;
  case UP_ERROR:
    if (UP.install) { toast(tr(UP.err)); UP.install = 0; }
    break;
  }
}

/* ------------------------------------------------------------------ */
/* Menu Parametres : une vraie liste de reglages + une carte d'infos    */
/* ------------------------------------------------------------------ */
enum { SR_STATUS, SR_THEME, SR_VIEW, SR_COVERS, SR_ONLINE, SR_SAVER, SR_SFX, SR_SFXPACK, SR_BOOT, SR_RETURN, SR_FOLDERS, SR_HIDDEN, SR_HIDEIP,
       SR_AUTOSTART, SR_AUTOUPD, SR_UPDATE, SR_CONTROLS, SR_QUIT, SR_COUNT };
static int autostart_state(void);
static int as_cached;
static int autostart_enable(char *msg, size_t mn);
static int autostart_disable(char *msg, size_t mn);
static void autostart_sync(void);
#define SR_VISIBLE 8                             /* lignes affichees a la fois */
static int set_row, set_top;

static const int SAVER_STEPS[] = { 0, 2, 5, 10, 30 };
static const char *PACK_NAMES[SFX_PACKS] = { "Néon", "Rétro 8-bit", "Doux", "Sci-fi", "Cristal" };
static const char *BOOT_NAMES[BOOT_SOUNDS] = { "Arpège néon", "Synthwave", "Console", "8-bit", "Hyperespace", "Aucun" };

static int wrap_step(int v, int d, int n) { return ((v + (d ? d : 1)) % n + n) % n; }

/* boites <-> icones : toutes les tuiles calculees sont refaites */
static void boxes_changed(void) {
  for (int i = 0; i < S.menu.napps; i++) drop_tiles(&S.menu.apps[i]);
  for (int i = 0; i < 12; i++)
    if (FCACHE[i].t) { warp_forget(FCACHE[i].t); SDL_DestroyTexture(FCACHE[i].t); FCACHE[i].t = NULL; }
  S.flow_cx = 0;
}

static void settings_change(int d, int activate) {
  switch (set_row) {
  case SR_THEME:
    if (activate) open_theme_editor();
    else {                                       /* ◀ ▶ : theme fourni ou enregistre suivant */
      load_presets();
      if (!S.npresets) return;
      int i = find_preset(S.cur.name);
      i = i < 0 ? 0 : ((i + d) % S.npresets + S.npresets) % S.npresets;
      Theme t = S.presets[i];
      if (S.cur.music[0]) {                      /* ta musique te suit d'un theme a l'autre */
        snprintf(t.music, sizeof t.music, "%s", S.cur.music);
        t.volume = S.cur.volume;
      }
      if (i < S.nbuiltin) {                      /* et l'image si le theme fourni n'en a pas */
        if (!t.music[0]) { snprintf(t.music, sizeof t.music, "%s", S.cur.music); t.volume = S.cur.volume; }
        if (!t.wallpaper[0]) snprintf(t.wallpaper, sizeof t.wallpaper, "%s", S.cur.wallpaper);
      }
      apply_theme(&t);
    }
    break;
  case SR_VIEW: set_view(S.menu.view + (d ? d : 1)); break;
  case SR_COVERS: S.menu.boxes = !S.menu.boxes; boxes_changed(); save(); break;
  case SR_ONLINE:                              /* les boites sont refaites avec ou sans les officielles */
    S.menu.covers_online = !S.menu.covers_online;
    for (int i = 0; i < S.menu.napps; i++) {
      App *a = &S.menu.apps[i];
      if (a->box_state == 1) continue;
      if (a->boxtex) { SDL_DestroyTexture(a->boxtex); a->boxtex = NULL; }
      a->box_state = 0;
      drop_tiles(a);
    }
    if (BX.mx) { SDL_LockMutex(BX.mx); BX.offline = 0; SDL_UnlockMutex(BX.mx); }
    save();
    break;
  case SR_SAVER: {
    int i = 0;
    while (i < 5 && SAVER_STEPS[i] != S.menu.saver) i++;
    i = i >= 5 ? 2 : wrap_step(i, d, 5);
    S.menu.saver = SAVER_STEPS[i];
    save();
    break;
  }
  case SR_SFX: {
    Theme t = S.cur;
    int v = t.sfx + (d ? d : 1) * 10;
    if (!d && v > 100) v = 0;                    /* ✕ : fait le tour */
    t.sfx = v < 0 ? 0 : v > 100 ? 100 : v;
    apply_theme(&t);
    sfx(SFX_OK);                                 /* on entend le nouveau volume */
    break;
  }
  case SR_SFXPACK:                               /* on ecoute le style choisi tout de suite */
    S.menu.sfx_pack = wrap_step(S.menu.sfx_pack, d, SFX_PACKS);
    save();
    sfx(SFX_OK);
    break;
  case SR_BOOT:
    S.menu.boot_snd = wrap_step(S.menu.boot_snd, d, BOOT_SOUNDS);
    save();
    sfx(SFX_BOOT);
    break;
  case SR_RETURN: plat_auto_return(!plat_auto_return(-1)); break;
  case SR_FOLDERS: S.menu.show_folders = !S.menu.show_folders; save(); break;
  case SR_HIDDEN: S.show_hidden = !S.show_hidden; break;
  case SR_HIDEIP: S.menu.hide_ip = !S.menu.hide_ip; S.sys_t = 0; save(); break;
  case SR_AUTOUPD: S.menu.auto_update = !S.menu.auto_update; save(); break;
  case SR_AUTOSTART:                             /* ✕ : liste des modes, rien ne change sans choix */
    if (activate) open_start_menu();
    break;
  case SR_UPDATE:                                /* ✕ seulement : verifier, telecharger puis installer */
    if (!activate || UP.busy) break;
    UP.install = 1;
    if (UP.state == UP_READY) up_install();
    else if (UP.state == UP_AVAILABLE) up_start(1);
    else up_start(0);
    break;
  case SR_CONTROLS:
    if (activate) open_help(0);
    break;
  case SR_STATUS:
    if (activate) { status_refresh(); open_help(2); }
    break;
  case SR_QUIT:
    if (activate) { App *a = menu_find_app(&S.menu, "sys:home"); if (a) launch(a); }
    break;
  }
}


/* ------------------------------------------------------------------ */
/* Demarrage automatique : Launcher5 dans la chaine de ps5_autoloader   */
/* (apres l'exploit). autoload.txt est sauvegarde une fois dans         */
/* autoload.txt.l5bak et remis tel quel a la desactivation.             */
/* ------------------------------------------------------------------ */
/* Dossier de l'autoloader reellement lu au demarrage, dans l'ordre des autoloaders (Y2JB,
   WebKit, BD-JB) : cle USB d'abord, puis /data ; dans chacun, un dossier propre a une
   application (ps5_autoloader_PPSA01650…) avant ps5_autoloader. Le premier qui contient un
   autoload.txt gagne ; sinon /data/ps5_autoloader. */
static void al_find(const char *root, char *out, size_t n) {
  char bases[9][64];
  int nb = 0;
  for (int u = 0; u < 8; u++) snprintf(bases[nb++], sizeof bases[0], "%s/mnt/usb%d", root, u);
  snprintf(bases[nb++], sizeof bases[0], "%s/data", root);
  for (int b = 0; b < nb; b++) {
    char p[600];
    struct stat st;
    DIR *d = opendir(bases[b]);
    struct dirent *e;
    int found = 0;
    while (d && !found && (e = readdir(d))) {     /* dossier propre a une application */
      if (strncmp(e->d_name, "ps5_autoloader_", 15)) continue;
      snprintf(p, sizeof p, "%s/%s/autoload.txt", bases[b], e->d_name);
      if (!stat(p, &st)) { snprintf(out, n, "%s/%s", bases[b], e->d_name); found = 1; }
    }
    if (d) closedir(d);
    if (found) return;
    snprintf(p, sizeof p, "%s/ps5_autoloader/autoload.txt", bases[b]);
    if (!stat(p, &st)) { snprintf(out, n, "%s/ps5_autoloader", bases[b]); return; }
  }
  snprintf(out, n, "%s/data/ps5_autoloader", root);
}

static void al_path(char *o, size_t n, const char *f) {
  char dir[512];
  al_find(plat_root(), dir, sizeof dir);
  snprintf(o, n, "%s%s%s", dir, f ? "/" : "", f ? f : "");
}

static int is_elf_file(const char *p) {
  FILE *f = fopen(p, "rb");
  unsigned char m[4] = { 0 };
  size_t r = f ? fread(m, 1, 4, f) : 0;
  if (f) fclose(f);
  return r == 4 && !memcmp(m, "\x7f" "ELF", 4);
}

static int copy_file(const char *src, const char *dst) {   /* via un fichier temporaire : jamais a moitie ecrit */
  char tmp[PATH_LEN + 8];
  snprintf(tmp, sizeof tmp, "%s.tmp", dst);
  FILE *in = fopen(src, "rb"), *out = in ? fopen(tmp, "wb") : NULL;
  int ok = in && out;
  char buf[65536];
  size_t k;
  while (ok && (k = fread(buf, 1, sizeof buf, in)) > 0) ok = fwrite(buf, 1, k, out) == k;
  if (in) fclose(in);
  if (out && fclose(out)) ok = 0;
  ok = ok && !rename(tmp, dst);
  if (!ok) remove(tmp);
  return ok ? 0 : -1;
}

static int line_is(const char *l, const char *word) {     /* ligne egale a « word » (espaces ignores) */
  while (*l == ' ' || *l == '\t') l++;
  size_t n = strlen(word);
  if (strncasecmp(l, word, n)) return 0;
  l += n;
  while (*l == ' ' || *l == '\t' || *l == '\r' || *l == '\n') l++;
  return !*l;
}

/* Plugin OnionHEN : si OnionHEN est sur la console, Launcher5 s'installe dans ses plugins
   (lance au demarrage, visible dans sa Toolbox) au lieu de toucher a autoload.txt. */
static int onion_present(void) {
  char p[PATH_LEN];
  struct stat st;
  snprintf(p, sizeof p, "%s/data/OnionHEN", plat_root());
  return !stat(p, &st) && S_ISDIR(st.st_mode);
}
static void onion_plugin_path(char *o, size_t n) { snprintf(o, n, "%s/data/OnionHEN/plugins/LNCH00005.elf", plat_root()); }
static int onion_plugin_installed(void) { char p[PATH_LEN]; onion_plugin_path(p, sizeof p); return !access(p, F_OK); }

static int al_has_launcher(void) {                /* autoload.txt lance deja Launcher5 */
  char p[PATH_LEN], l[512];
  al_path(p, sizeof p, "autoload.txt");
  FILE *f = fopen(p, "r");
  int on = 0;
  while (f && fgets(l, sizeof l, f)) if (line_is(l, "launcher5.elf")) on = 1;
  if (f) fclose(f);
  return on;
}

static int onion_enable(char *msg, size_t mn) {
  char me[PATH_LEN], dir[PATH_LEN], dst[PATH_LEN];
  snprintf(me, sizeof me, "%s/launcher5.elf", plat_data_dir());
  if (!is_elf_file(me)) { snprintf(msg, mn, "%s", tr("launcher5.elf introuvable dans /data/launcher5")); return -1; }
  snprintf(dir, sizeof dir, "%s/data/OnionHEN/plugins", plat_root());
  mkdir(dir, 0755);
  onion_plugin_path(dst, sizeof dst);
  if (copy_file(me, dst)) { snprintf(msg, mn, "%s", tr("Copie impossible dans /data/OnionHEN/plugins")); return -1; }
  plat_log("demarrage automatique : plugin OnionHEN installe (%s)", dst);
  snprintf(msg, mn, "%s", tr("Launcher5 installé comme plugin OnionHEN : il démarrera avec le jailbreak"));
  return 0;
}

/* -1 : pas de ps5_autoloader sur la console ; 0 : desactive ; 1 : Launcher5 y demarre */
static int as_cached = -2;                       /* etat relu au plus toutes les 3 s (-2 : a relire) */
static int autostart_state(void) {
  static Uint32 at;
  int cached = as_cached;
  if (cached != -2 && SDL_GetTicks() - at < 3000) return cached;
  at = SDL_GetTicks();
  if (onion_plugin_installed()) return as_cached = 1;
  char p[PATH_LEN];
  al_path(p, sizeof p, "autoload.txt");
  FILE *f = fopen(p, "r");
  if (!f) return as_cached = onion_present() ? 0 : -1;
  char l[512];
  int on = 0;
  while (fgets(l, sizeof l, f)) if (line_is(l, "launcher5.elf")) on = 1;
  fclose(f);
  return as_cached = on;
}

/* chargeur ELF officiel deja present sur la console (celui de l'utilisateur d'abord) */
static int find_elfldr(char *out, size_t n) {
  const char *cand[] = { "/data/ps5_autoloader/elfldr.elf", "/data/pldmgr/payloads/local/elfldr/elfldr-ps5.elf",
                         "/data/payloads/elfldr.elf", "/data/payloads/elfldr-ps5.elf" };
  for (size_t i = 0; i < sizeof cand / sizeof cand[0]; i++) {
    snprintf(out, n, "%s%s", plat_root(), cand[i]);
    if (is_elf_file(out)) return 0;
  }
  char dir[PATH_LEN];
  snprintf(dir, sizeof dir, "%s/data/pldmgr/payloads/repo", plat_root());
  DIR *d = opendir(dir);
  struct dirent *e;
  while (d && (e = readdir(d))) {
    if (strncasecmp(e->d_name, "elfldr@", 7)) continue;
    char sub[PATH_LEN + 80];
    snprintf(sub, sizeof sub, "%s/%s", dir, e->d_name);
    DIR *s = opendir(sub);
    struct dirent *f;
    while (s && (f = readdir(s))) {
      size_t l = strlen(f->d_name);
      if (l > 4 && !strcasecmp(f->d_name + l - 4, ".elf")) {
        snprintf(out, n, "%s/%s", sub, f->d_name);
        if (is_elf_file(out)) { closedir(s); closedir(d); return 0; }
      }
    }
    if (s) closedir(s);
  }
  if (d) closedir(d);
  return -1;
}

static int autostart_enable(char *msg, size_t mn) {
  char alp[PATH_LEN];
  al_path(alp, sizeof alp, "autoload.txt");
  if (access(alp, F_OK) && onion_present()) return onion_enable(msg, mn);   /* pas d'autoloader : plugin */
  char al[PATH_LEN], bak[PATH_LEN], me[PATH_LEN], dst[PATH_LEN], ldr[PATH_LEN], eld[PATH_LEN];
  al_path(al, sizeof al, "autoload.txt");
  al_path(bak, sizeof bak, "autoload.txt.l5bak");
  snprintf(me, sizeof me, "%s/launcher5.elf", plat_data_dir());
  if (!is_elf_file(me)) { snprintf(msg, mn, "%s", tr("launcher5.elf introuvable dans /data/launcher5")); return -1; }
  int have_ldr = !find_elfldr(ldr, sizeof ldr);
  FILE *f = fopen(al, "r");
  if (!f) {                                      /* pas encore d'autoload.txt : on le cree */
    char dir[PATH_LEN];
    struct stat ds;
    al_path(dir, sizeof dir, NULL);
    if (stat(dir, &ds) || !S_ISDIR(ds.st_mode)) { snprintf(msg, mn, "%s", tr("ps5_autoloader introuvable sur la console")); return -1; }
  }
  /* nouvelle liste : commentaires gardes, Payload Manager retire, elfldr d'abord, Launcher5 a la fin */
  char *keep = NULL, l[512];
  size_t kl = 0;
  int has_ldr = 0;
  while (f && fgets(l, sizeof l, f)) {
    const char *t = l;
    while (*t == ' ' || *t == '\t') t++;
    int payload = *t && *t != '#' && *t != '\r' && *t != '\n' && *t != '!';
    char low[512];
    size_t k = 0;
    for (; t[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)t[k]);
    low[k] = 0;
    if (payload && (strstr(low, "pldmgr") || line_is(l, "launcher5.elf"))) continue;
    if (payload && line_is(l, "elfldr.elf")) { has_ldr = 1; continue; }
    size_t ll = strlen(l);
    char *r = realloc(keep, kl + ll + 2);
    if (!r) { free(keep); fclose(f); return -1; }
    keep = r;
    memcpy(keep + kl, l, ll);
    kl += ll;
    if (ll && l[ll - 1] != '\n') keep[kl++] = '\n';
  }
  int existed = f != NULL;
  if (f) fclose(f);
  /* pauses (!ms) et lignes vides laissees en fin de fichier : on remet la notre seule */
  while (kl > 0) {
    size_t st = kl - 1;
    while (st > 0 && keep[st - 1] != '\n') st--;
    const char *t = keep + st;
    while (t < keep + kl && (*t == ' ' || *t == '\t')) t++;
    if (t < keep + kl && *t != '!' && *t != '\r' && *t != '\n') break;
    kl = st;
  }
  (void)has_ldr;
  if (existed && access(bak, F_OK) && copy_file(al, bak)) { free(keep); snprintf(msg, mn, "%s", tr("Sauvegarde de autoload.txt impossible")); return -1; }
  al_path(eld, sizeof eld, "elfldr.elf");
  al_path(dst, sizeof dst, "launcher5.elf");
  if ((have_ldr && strcmp(ldr, eld) && copy_file(ldr, eld)) || copy_file(me, dst)) {
    free(keep); snprintf(msg, mn, "%s", tr("Copie impossible dans ps5_autoloader")); return -1;
  }
  char tmp[PATH_LEN + 8];
  snprintf(tmp, sizeof tmp, "%s.tmp", al);
  FILE *o = fopen(tmp, "w");
  int ok = o != NULL;
  if (ok) {
    /* commentaires de tete, puis la chaine : elfldr, autres payloads gardes, Launcher5 */
    size_t i = 0;
    while (i < kl) {                             /* lignes de commentaire / vides du debut */
      size_t e = i;
      while (e < kl && keep[e] != '\n') e++;
      const char *t = keep + i;
      while (t < keep + e && (*t == ' ' || *t == '\t')) t++;
      if (t < keep + e && *t != '#' && *t != '\r') break;
      fwrite(keep + i, 1, e - i + (e < kl), o);
      i = e + (e < kl);
    }
    if (have_ldr) fputs("elfldr.elf\n", o);
    if (i < kl) fwrite(keep + i, 1, kl - i, o);
    fputs(have_ldr ? "!3000\nlauncher5.elf\n" : "launcher5.elf\n", o);   /* elfldr doit avoir ouvert le port 9021 */
    ok = !fclose(o) && !rename(tmp, al);
  }
  free(keep);
  if (!ok) { remove(tmp); snprintf(msg, mn, "%s", tr("Écriture de autoload.txt impossible")); return -1; }
  plat_log("demarrage automatique active (elfldr : %s, sauvegarde : %s)", have_ldr ? ldr : "aucun", existed ? bak : "aucune");
  if (onion_plugin_installed()) {                /* un seul demarrage : le plugin OnionHEN est retire */
    char pl[PATH_LEN];
    onion_plugin_path(pl, sizeof pl);
    remove(pl);
  }
  if (!have_ldr) snprintf(msg, mn, "%s", tr("Launcher5 démarrera avec le jailbreak  ·  installe elfldr depuis le catalogue pour envoyer des payloads"));
  else snprintf(msg, mn, "%s", tr(existed ? "Launcher5 démarrera avec le jailbreak  ·  ancien autoload.txt sauvegardé" : "Launcher5 démarrera avec le jailbreak"));
  return 0;
}

static int autostart_disable(char *msg, size_t mn) {
  if (onion_plugin_installed()) {
    char pl[PATH_LEN];
    onion_plugin_path(pl, sizeof pl);
    if (remove(pl)) { snprintf(msg, mn, "%s", tr("Suppression du plugin impossible")); return -1; }
    plat_log("demarrage automatique : plugin OnionHEN retire");
    snprintf(msg, mn, "%s", tr("Démarrage automatique désactivé  ·  plugin OnionHEN retiré"));
    if (!al_has_launcher()) return 0;
  }
  char al[PATH_LEN], bak[PATH_LEN], dst[PATH_LEN];
  al_path(al, sizeof al, "autoload.txt");
  al_path(bak, sizeof bak, "autoload.txt.l5bak");
  al_path(dst, sizeof dst, "launcher5.elf");
  if (!access(bak, F_OK)) {                      /* l'ancien fichier, tel quel */
    if (copy_file(bak, al)) { snprintf(msg, mn, "%s", tr("Écriture de autoload.txt impossible")); return -1; }
    remove(bak);
  } else {                                       /* pas de sauvegarde : on retire juste Launcher5 */
    FILE *f = fopen(al, "r");
    char tmp[PATH_LEN + 8], l[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", al);
    FILE *o = f ? fopen(tmp, "w") : NULL;
    while (f && o && fgets(l, sizeof l, f)) if (!line_is(l, "launcher5.elf")) fputs(l, o);
    if (f) fclose(f);
    if (!o || fclose(o) || rename(tmp, al)) { remove(tmp); snprintf(msg, mn, "%s", tr("Écriture de autoload.txt impossible")); return -1; }
  }
  remove(dst);
  plat_log("demarrage automatique desactive : autoload.txt remis");
  snprintf(msg, mn, "%s", tr("Démarrage automatique désactivé  ·  autoload.txt d'origine remis"));
  return 0;
}


/* --- Mode de demarrage : un choix explicite (jamais une bascule a chaque appui) --- */
enum { SM_MANUAL, SM_PLUGIN, SM_AUTOLOAD };
static int start_mode(void) {
  char al[PATH_LEN];
  al_path(al, sizeof al, "autoload.txt");
  if (!access(al, F_OK) && al_has_launcher()) return SM_AUTOLOAD;
  if (onion_plugin_installed()) return SM_PLUGIN;
  return SM_MANUAL;
}
static int start_mode_available(int m) {
  char al[PATH_LEN];
  al_path(al, sizeof al, "autoload.txt");
  return m == SM_MANUAL || (m == SM_PLUGIN && onion_present()) || (m == SM_AUTOLOAD && !access(al, F_OK));
}
static const char *START_MODE_NAMES[3] = { "Manuel", "Plugin OnionHEN", "Autoloader" };

/* Launcher5 en mode autoloader : on note quel autoload.txt le lance. Si un autre outil
   (Payload Manager…) le reecrit sans Launcher5, on le dit une fois a l'ouverture. */
static void autoload_mark(int on) {
  char mk[PATH_LEN], al[PATH_LEN];
  snprintf(mk, sizeof mk, "%s/cache/autoload_on", plat_data_dir());
  if (!on) { remove(mk); return; }
  al_path(al, sizeof al, "autoload.txt");
  FILE *f = fopen(mk, "w");
  if (f) { fprintf(f, "%s\n", al); fclose(f); }
}

static void autoload_lost_check(void) {
  char mk[PATH_LEN], was[PATH_LEN] = "", al[PATH_LEN];
  snprintf(mk, sizeof mk, "%s/cache/autoload_on", plat_data_dir());
  int mode = start_mode();
  FILE *f = fopen(mk, "r");
  if (f) { if (!fgets(was, sizeof was, f)) was[0] = 0; fclose(f); was[strcspn(was, "\r\n")] = 0; }
  if (mode == SM_AUTOLOAD) { autoload_mark(1); return; }
  if (!was[0] || S.mode != MODE_BROWSE) return;
  al_path(al, sizeof al, "autoload.txt");
  if (strcmp(was, al) || access(al, F_OK)) return;   /* cle USB retiree… : rien a dire */
  plat_log("autoload.txt modifie par un autre outil : Launcher5 n'y est plus");
  S.nchoices = 0; S.csel = 0; S.quick = 0;
  snprintf(S.ctx_title, sizeof S.ctx_title, "%s", tr("Launcher5 n'est plus lancé au démarrage"));
  add_choice("Le remettre (autoload.txt sauvegardé)", ACT_RELINK_YES);
  add_choice("Non, ne plus demander", ACT_RELINK_NO);
  S.mode = MODE_CONTEXT;
}

static void start_mode_set(int m) {
  char msg[200] = "";
  int cur = start_mode(), rc = 0;
  if (m == cur) { toast("Déjà dans ce mode"); return; }
  if (cur != SM_MANUAL) rc = autostart_disable(msg, sizeof msg);   /* on revient d'abord a l'etat d'origine */
  if (!rc && m == SM_PLUGIN) rc = onion_enable(msg, sizeof msg);
  if (!rc && m == SM_AUTOLOAD) rc = autostart_enable(msg, sizeof msg);
  if (!rc && m == SM_MANUAL) snprintf(msg, sizeof msg, "%s", tr("Démarrage manuel : la chaîne d'origine est remise"));
  if (!rc) autoload_mark(m == SM_AUTOLOAD);      /* choix de l'utilisateur : on suit */
  as_cached = -2;
  if (rc) sfx(SFX_ERROR);
  toast(msg);
}

/* Premier lancement : Launcher5 s'inscrit dans ps5_autoloader. Sans autoload.txt (ou vide), tout
   seul ; si une autre chaine y est deja (Payload Manager…), on demande une seule fois. */
static int al_has_payloads(void) {
  char p[PATH_LEN], l[512];
  al_path(p, sizeof p, "autoload.txt");
  FILE *f = fopen(p, "r");
  int n = 0;
  while (f && fgets(l, sizeof l, f)) {
    const char *t = l;
    while (*t == ' ' || *t == '\t') t++;
    if (!*t || *t == '#' || *t == '\r' || *t == '\n' || *t == '!') continue;
    char low[512];
    size_t k = 0;
    for (; t[k] && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)t[k]);
    low[k] = 0;
    if (strstr(low, "pldmgr")) n++;
  }
  if (f) fclose(f);
  return n;                                      /* Payload Manager : seul payload retire de la chaine */
}

static void first_run_autoload(void) {
  char mark[PATH_LEN], dir[PATH_LEN];
  snprintf(mark, sizeof mark, "%s/cache/autoload_setup", plat_data_dir());
  if (!access(mark, F_OK)) return;
  FILE *m = fopen(mark, "w");
  if (m) { fputs("1\n", m); fclose(m); }
  al_path(dir, sizeof dir, NULL);
  struct stat st;
  if (start_mode() != SM_MANUAL) return;         /* deja dans la chaine */
  if ((stat(dir, &st) || !S_ISDIR(st.st_mode)) && !onion_present()) {
    char data[PATH_LEN];                         /* ni autoloader ni OnionHEN : on prepare l'autoloader */
    snprintf(data, sizeof data, "%s/data", plat_root());
    mkdir(data, 0755);
    mkdir(dir, 0755);
    plat_log("premier lancement : dossier ps5_autoloader cree");
  }
  if (!al_has_payloads()) {                      /* lignes gardees, ancien fichier sauvegarde : inscription directe */
    char msg[200];
    if (autostart_enable(msg, sizeof msg)) sfx(SFX_ERROR);
    plat_log("premier lancement : %s", msg);
    as_cached = -2;
    toast(msg);
    return;
  }
  S.nchoices = 0; S.csel = 0; S.quick = 0;
  snprintf(S.ctx_title, sizeof S.ctx_title, "%s", tr("Démarrage automatique ?"));
  add_choice("Oui, remplacer (sauvegardé)", ACT_FIRST_YES);
  add_choice("Non, garder ma chaîne", ACT_FIRST_NO);
  S.mode = MODE_CONTEXT;
}

static void open_start_menu(void) {
  static const char *const DESC[3] = { "Manuel · remet ton autoload d'origine", "Plugin OnionHEN", "Autoloader (sans Payload Manager)" };
  S.nchoices = 0; S.csel = 0; S.quick = 0;
  snprintf(S.ctx_title, sizeof S.ctx_title, "%s", tr("Démarrage de Launcher5"));
  int cur = start_mode();
  for (int m = 0; m < 3; m++) {
    if (!start_mode_available(m)) continue;
    char l[120];
    snprintf(l, sizeof l, "%s%s", m == cur ? "✓  " : "     ", tr(DESC[m]));
    if (m == cur) S.csel = S.nchoices;
    add_choice(l, ACT_START_MANUAL + m);
  }
  S.mode = MODE_CONTEXT;
}

/* copie dans ps5_autoloader rafraichie apres une mise a jour de Launcher5 */
static void autostart_sync(void) {
  if (autostart_state() != 1) return;
  char me[PATH_LEN], dst[PATH_LEN];
  snprintf(me, sizeof me, "%s/launcher5.elf", plat_data_dir());
  struct stat a, b;
  if (stat(me, &a) || !is_elf_file(me)) return;
  if (onion_plugin_installed()) {                /* plugin OnionHEN */
    onion_plugin_path(dst, sizeof dst);
    if (stat(dst, &b) || b.st_size != a.st_size || b.st_mtime < a.st_mtime)
      if (!copy_file(me, dst)) plat_log("demarrage automatique : plugin OnionHEN mis a jour");
  }
  if (!al_has_launcher()) return;
  al_path(dst, sizeof dst, "launcher5.elf");
  if (!stat(dst, &b) && b.st_size == a.st_size && b.st_mtime >= a.st_mtime) return;
  if (!copy_file(me, dst)) plat_log("demarrage automatique : copie de Launcher5 mise a jour");
}

/* renvoie 1 si la touche est prise par le menu */
/* ------------------------------------------------------------------ */
/* Etat de la console (Parametres) : ce qui tourne, ce qui manque, et  */
/* « Tout reparer » qui refait l'installation du premier lancement.    */
/* ------------------------------------------------------------------ */
enum { ST_OK, ST_WARN, ST_BAD };
static struct { int lvl; char what[64], detail[200]; } STL[6];
static int nstl;
static SDL_atomic_t repair_done;
static volatile int repairing;

static int proc_running(const char *procs, const char *prefix) {
  for (const char *l = procs; *l;) {
    const char *e = strchr(l, '\n');
    size_t len = e ? (size_t)(e - l) : strlen(l);
    char low[24];
    size_t k = 0;
    for (; k < len && k + 1 < sizeof low; k++) low[k] = (char)tolower((unsigned char)l[k]);
    low[k] = 0;
    if (!strncmp(low, prefix, strlen(prefix))) return 1;
    if (!e) break;
    l = e + 1;
  }
  return 0;
}

static void st_add(int lvl, const char *what, const char *detail) {
  if (nstl >= 6) return;
  STL[nstl].lvl = lvl;
  snprintf(STL[nstl].what, sizeof STL[nstl].what, "%s", tr(what));
  snprintf(STL[nstl].detail, sizeof STL[nstl].detail, "%s", tr(detail));
  nstl++;
}

static void status_refresh(void) {
  static char procs[24576];
  nstl = 0;
  if (plat_procs(procs, sizeof procs)) procs[0] = 0;
  if (nboot < 0) boot_load();
  int in_ks = 0, in_sm = 0;
  for (int i = 0; i < nboot; i++) {
    if (name_has(BOOTL[i], "kstuff")) in_ks = 1;
    if (name_has(BOOTL[i], "shadowmount") || name_has(BOOTL[i], "kraken")) in_sm = 1;
  }
  int ks = proc_running(procs, "kstuff") || proc_running(procs, "etahen");
  int sm = proc_running(procs, "shadowmount") || port_open(8097);
  st_add(ks ? ST_OK : in_ks ? ST_WARN : ST_BAD, "kstuff (jeux et FPKG)",
         ks ? "Actif" : in_ks ? "Pas lancé : il partira au prochain allumage de la console" : "Absent : ✕ Tout réparer");
  st_add(sm ? ST_OK : in_sm ? ST_WARN : ST_BAD, "Monteur de jeux (ShadowMount)",
         sm ? "Actif" : in_sm ? "Pas lancé : il partira au prochain allumage de la console" : "Absent : ✕ Tout réparer");
  st_add(in_ks && in_sm ? ST_OK : ST_BAD, "Liste au démarrage",
         in_ks && in_sm ? "kstuff et ShadowMount partent à chaque allumage" : "Incomplète : ✕ Tout réparer");
  int mode = start_mode();
  st_add(mode != SM_MANUAL ? ST_OK : ST_BAD, "Démarrage avec le jailbreak",
         mode == SM_AUTOLOAD ? "Autoloader : Launcher5 démarre tout seul" : mode == SM_PLUGIN ? "Plugin OnionHEN : Launcher5 démarre tout seul"
                             : "Manuel : ✕ Tout réparer pour démarrer tout seul");
  char ldr[PATH_LEN];
  int has_ldr = !find_elfldr(ldr, sizeof ldr);
  st_add(has_ldr ? ST_OK : ST_WARN, "Chargeur ELF (elfldr)",
         has_ldr ? "Présent" : "Absent : ✕ Tout réparer pour l'installer");
}

static int repair_thread(void *u) {
  (void)u;
  char p[PATH_LEN];
  snprintf(p, sizeof p, "%s/cache/starter_done", plat_data_dir());
  remove(p);
  snprintf(p, sizeof p, "%s/cache/starter_tries", plat_data_dir());
  remove(p);
  starter_prepare();                             /* payloads, elfldr, liste au demarrage */
  SDL_AtomicSet(&repair_done, 1);
  return 0;
}

static void status_repair(void) {
  if (repairing) return;
  repairing = 1;
  plat_log("etat de la console : reparation demandee");
  SDL_Thread *t = SDL_CreateThread(repair_thread, "l5repair", NULL);
  if (t) SDL_DetachThread(t); else repairing = 0;
}

/* fin de la reparation, dans le fil de l'ecran */
static void status_poll(void) {
  if (!SDL_AtomicGet(&repair_done)) return;
  SDL_AtomicSet(&repair_done, 0);
  repairing = 0;
  scan_all(&S.menu);                             /* payloads installes */
  nboot = -1;
  boot_refresh();
  if (start_mode() == SM_MANUAL) {               /* demarrage automatique, comme au premier lancement */
    char dir[PATH_LEN];
    struct stat st;
    al_path(dir, sizeof dir, NULL);
    if ((stat(dir, &st) || !S_ISDIR(st.st_mode)) && !onion_present()) {
      char data[PATH_LEN];
      snprintf(data, sizeof data, "%s/data", plat_root());
      mkdir(data, 0755);
      mkdir(dir, 0755);
    }
    char msg[200];
    if (!autostart_enable(msg, sizeof msg)) autoload_mark(1);
    as_cached = -2;
  }
  status_refresh();
  int bad = 0;
  for (int i = 0; i < nstl; i++) if (STL[i].lvl == ST_BAD && i > 0) bad = 1;   /* kstuff seul : redemarrage */
  if (!STL[0].lvl && !bad) toast("Tout est en place");
  else if (!bad) toast("Réparé : redémarre la console pour lancer kstuff et ShadowMount");
  else toast("Réparation incomplète (Internet ?) : réessaie plus tard");
  sfx(bad ? SFX_ERROR : SFX_OK);
}

static void draw_status(int px, int py, int pw, int ph) {
  draw_text(S.f48, "État de la console", px + 50, py + 34, 0xffffff, 0, pw - 100);
  for (int i = 0; i < nstl; i++) {
    int y = py + 140 + i * 112, x = px + 60;
    uint32_t c = STL[i].lvl == ST_OK ? 0x2ecc71 : STL[i].lvl == ST_WARN ? 0xf5a623 : 0xe74c3c;
    fill_round(x, y + 8, 34, 34, 17, c, 255);
    draw_text(S.f30, STL[i].what, x + 60, y, 0xffffff, 0, pw - 200);
    draw_text(S.f22, STL[i].detail, x + 60, y + 44, 0xc9d1dc, 0, pw - 200);
  }
  draw_text(S.f22, repairing ? "Réparation en cours…" : "✕  Tout réparer", px + pw - 52, py + ph - 56,
            repairing ? 0xf5a623 : 0xffffff, 2, 0);
}

static int settings_input(int in) {
  switch (in) {
  case IN_UP: if (set_row > 0) set_row--; return 1;
  case IN_DOWN: if (set_row < SR_COUNT - 1) set_row++; return 1;
  case IN_LEFT: settings_change(-1, 0); return 1;
  case IN_RIGHT: settings_change(1, 0); return 1;
  case IN_OK: settings_change(0, 1); return 1;
  }
  return 0;
}

static void settings_value(int r, char *v, size_t n, int *toggle) {
  *toggle = -1;
  switch (r) {
  case SR_THEME: snprintf(v, n, "%s", tr(S.cur.name)); break;
  case SR_VIEW: snprintf(v, n, "%s", tr(VIEW_NAMES[S.menu.view])); break;
  case SR_COVERS: snprintf(v, n, "%s", tr(S.menu.boxes ? "Boîtes" : "Icônes")); break;
  case SR_SAVER:
    if (S.menu.saver > 0) snprintf(v, n, tr("%d min"), S.menu.saver);
    else snprintf(v, n, "%s", tr("Jamais"));
    break;
  case SR_SFX:
    if (S.cur.sfx > 0) snprintf(v, n, "%d %%", S.cur.sfx);
    else snprintf(v, n, "%s", tr("Coupés"));
    break;
  case SR_SFXPACK: snprintf(v, n, "%s", tr(PACK_NAMES[S.menu.sfx_pack])); break;
  case SR_BOOT: snprintf(v, n, "%s", tr(BOOT_NAMES[S.menu.boot_snd])); break;
  case SR_RETURN: *toggle = plat_auto_return(-1); break;
  case SR_FOLDERS: *toggle = S.menu.show_folders; break;
  case SR_HIDDEN: *toggle = S.show_hidden; break;
  case SR_HIDEIP: *toggle = S.menu.hide_ip; break;
  case SR_ONLINE: *toggle = S.menu.covers_online; break;
  case SR_AUTOUPD: *toggle = S.menu.auto_update; break;
  case SR_AUTOSTART: snprintf(v, n, "%s", tr(START_MODE_NAMES[start_mode()])); break;
  case SR_UPDATE:
    switch (UP.state) {
    case UP_CHECKING: snprintf(v, n, "%s", tr("Vérification…")); break;
    case UP_LATEST: snprintf(v, n, tr("À jour (v%s)"), run_version()); break;
    case UP_AVAILABLE: snprintf(v, n, tr("v%s disponible"), UP.ver); break;
    case UP_DOWNLOADING: snprintf(v, n, "%s", tr("Téléchargement…")); break;
    case UP_READY: snprintf(v, n, tr("v%s prête"), UP.ver); break;
    case UP_ERROR: snprintf(v, n, "%s", tr("Échec")); break;
    default: snprintf(v, n, "v%s", run_version());
    }
    break;
  default: v[0] = 0;
  }
}

/* --- apercu de l'affichage des jeux, dans la carte de droite --- */
static App *pv_app(int i) {                      /* i-eme jeu de la console (en boucle) */
  int n = 0;
  for (int k = 0; k < S.menu.napps; k++) if (S.menu.apps[k].kind != KIND_SYSTEM) n++;
  if (!n) return NULL;
  i = ((i % n) + n) % n;
  for (int k = 0; k < S.menu.napps; k++)
    if (S.menu.apps[k].kind != KIND_SYSTEM && !i--) return &S.menu.apps[k];
  return NULL;
}

static void pv_tile(int i, int x, int y, int s, int alpha) {   /* s : largeur, hauteur TH(s) */
  App *a = pv_app(i);
  SDL_Texture *t = a ? tile_tex(a, TILE, TH(TILE)) : NULL;
  if (t) {
    SDL_Rect d = { x, y, s, TH(s) };
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    SDL_RenderCopy(S.r, t, NULL, &d);
    SDL_SetTextureAlphaMod(t, 255);
  } else {
    static const uint32_t C[4] = { 0x2b86c5, 0xb26bff, 0xff3cac, 0x00b4d8 };
    fill_round(x, y, s, TH(s), s / 10, shade(C[((i % 4) + 4) % 4], 0.6f), alpha);
  }
}

static void draw_view_preview(int cx, int cy, int cw, int ch) {
  static const char *DESC[VIEW_COUNT] = {
    "Jaquettes en 3D, le jeu choisi au centre",
    "Toute ta collection d'un coup d'œil",
    "Une rangée façon accueil PS5",
  };
  int v = S.menu.view, tx = cx + 40;
  draw_text(S.f22, tr("APERÇU"), tx, cy + 30, S.th->accent, 0, 0);
  draw_text(S.f48, tr(VIEW_NAMES[v]), tx, cy + 58, 0xffffff, 0, cw - 80);

  /* petit ecran 16/9 */
  int sx = cx + 30, sw = cw - 60, sh = sw * 9 / 16, sy = cy + 150;
  float sc = (float)sw / W;
  fill_round(sx - 6, sy - 6, sw + 12, sh + 12, 18, 0x000000, 200);
  SDL_Rect clip = { sx, sy, sw, sh };
  SDL_RenderSetClipRect(S.r, &clip);
  fill_round(sx, sy, sw, sh, 0, S.th->top, 255);
  fill_round(sx, sy + sh / 2, sw, sh - sh / 2, 0, S.th->bottom, 255);
  SDL_Texture *wp = S.th->bg_mode == BG_IMAGE ? wallpaper() : NULL;
  if (wp) { SDL_SetTextureAlphaMod(wp, 150); SDL_RenderCopy(S.r, wp, NULL, &clip); SDL_SetTextureAlphaMod(wp, 255); }

  /* defilement : toutes les 1,6 s le choix avance d'un jeu, en douceur */
  float tt = SDL_GetTicks() / 1000.0f, ph = fmodf(tt, 1.6f) / 1.6f;
  int base = (int)(tt / 1.6f);
  float sh_k = ph < 0.7f ? 0 : (ph - 0.7f) / 0.3f;
  sh_k = sh_k * sh_k * (3 - 2 * sh_k);
  if (S.lite) sh_k = 0;
  float center = base + sh_k;
  uint32_t ac = S.th->accent;

  /* barre du haut : sections */
  for (int i = 0; i < 4; i++)
    fill_round(sx + (int)(CAR_X * sc) + i * (int)(150 * sc), sy + (int)(40 * sc), (int)(120 * sc), (int)(22 * sc), 4,
               i == 0 ? ac : 0xffffff, i == 0 ? 255 : 70);

  if (v == VIEW_FLOW) {
    int cs = (int)(FLOW * sc), ss = (int)(FLOW_SIDE * sc), mx = sx + sw / 2, my = sy + (int)(FLOW_CY * sc);
    for (int dist = 4; dist >= 0; dist--)        /* du plus loin au plus pres */
      for (int sgn = -1; sgn <= 1; sgn += 2) {
        if (!dist && sgn > 0) continue;
        int j = (int)floorf(center) + sgn * dist;
        float d = j - center, ad = fabsf(d);
        if (ad > 4) continue;
        float m = ad < 1 ? ad : 1;
        int s = (int)(cs + (ss - cs) * m);
        float xo = (d < 0 ? -1 : 1) * (m * (cs * 0.5f + ss * 0.55f) + (ad > 1 ? (ad - 1) * ss * 0.5f : 0));
        int x = mx + (int)xo - s / 2, y = my - TH(s) / 2;
        if (ad < 0.5f) glow(x + s / 2, y + TH(s) / 2, TH(s), ac, 70);
        pv_tile(j, x, y, s, 255 - (int)(fminf(ad, 3) * 55));
      }
    fill_round(mx - (int)(260 * sc), my + TH(cs) / 2 + (int)(50 * sc), (int)(520 * sc), (int)(26 * sc), 4, 0xffffff, 200);
  } else if (v == VIEW_GRID) {
    int cell = (int)(GRID_CELL * sc), gap = (int)(GRID_GAP * sc), rowh = (int)(GRID_ROWH * sc);
    int x0 = sx + (sw - (GRID_COLS * cell + (GRID_COLS - 1) * gap)) / 2;
    int sel = base % (GRID_COLS * 3);
    for (int i = 0; i < GRID_COLS * 3; i++) {
      int x = x0 + (i % GRID_COLS) * (cell + gap), y = sy + (int)(GRID_Y * sc) + (i / GRID_COLS) * rowh;
      if (i == sel) {
        int s = cell * 112 / 100, ox = x - (s - cell) / 2, oy = y - (TH(s) - TH(cell)) / 2;
        glow(x + cell / 2, y + TH(cell) / 2, TH(s), ac, 90);
        pv_tile(i, ox, oy, s, 255);
        outline_round(ox - 2, oy - 2, s + 4, TH(s) + 4, 6, 2, ac, 255);
      } else pv_tile(i, x, y, cell, 210);
    }
  } else {                                       /* carrousel facon accueil PS5 */
    App *a = pv_app(base);
    SDL_Texture *bl = a && a->bg[0] ? blurred(a) : NULL;   /* illustration du jeu seulement */
    if (bl) { SDL_SetTextureAlphaMod(bl, 200); SDL_RenderCopy(S.r, bl, NULL, &clip); SDL_SetTextureAlphaMod(bl, 255); }
    fill_round(sx, sy + sh / 2, sw, sh - sh / 2, 0, 0x000000, 90);
    int ts = (int)(TILE * sc), sel = (int)(TILE_SEL * sc), gap = (int)(GAP * sc);
    int x = sx + (int)(CAR_X * sc), bot = sy + (int)(CAR_BOT * sc), y = bot - TH(sel);
    glow(x + sel / 2, y + TH(sel) / 2, TH(sel), ac, 80);
    pv_tile(base, x, y, sel, 255);
    outline_round(x - 2, y - 2, sel + 4, TH(sel) + 4, 6, 2, ac, 255);
    for (int i = 1; i < 8; i++) pv_tile(base + i, x + sel + gap + (i - 1) * (ts + gap), bot - TH(ts), ts, 220);
    fill_round(x, y - (int)(70 * sc), (int)(460 * sc), (int)(34 * sc), 4, 0xffffff, 220);
  }
  SDL_RenderSetClipRect(S.r, NULL);
  outline_round(sx, sy, sw, sh, 4, 1, 0xffffff, 40);

  /* les trois choix, celui en cours en couleur */
  int py = sy + sh + 40, bw = (sw - 20) / VIEW_COUNT;
  for (int i = 0; i < VIEW_COUNT; i++) {
    int bx = sx + i * (bw + 10), on = i == v;
    fill_round(bx, py, bw, 52, 26, on ? ac : 0xffffff, on ? 255 : 22);
    draw_text(S.f22, tr(VIEW_NAMES[i]), bx + bw / 2, py + 12, on ? 0x05070b : 0xc0c8d4, 1, bw - 16);
  }
  draw_text(S.f22, tr(DESC[v]), tx, py + 80, 0xc0c8d4, 0, cw - 80);
  draw_text(S.f22, tr("◀ ▶  pour essayer un autre affichage"), tx, py + 118, 0x9aa4b5, 0, cw - 80);
  (void)ch;
}

static void draw_settings(void) {
  static const char *ICON[SR_COUNT] = { "✓", "◐", "▦", "▯", "⬇", "☾", "♪", "♫", "★", "↺", "▤", "◌", "⊘", "⚡", "◷", "↻", "✚", "⌂" };
  static const char *LABEL[SR_COUNT] = {
    "État de la console", "Thème", "Affichage des jeux", "Jaquettes", "Jaquettes officielles", "Économiseur d'écran", "Sons d'interface", "Style des sons", "Son de démarrage",
    "Retour automatique", "Section Mes dossiers", "Éléments masqués", "Masquer l'IP", "Démarrage de Launcher5", "Mises à jour automatiques", "Mettre à jour",
    "Commandes de la manette", "Quitter vers l'accueil PS5",
  };
  static const char *HELP[SR_COUNT] = {
    "✕ vérifie kstuff, ShadowMount, le démarrage  ·  et répare tout",
    "◀ ▶ change de thème  ·  ✕ ouvre le Studio Thème",
    "Cover Flow, Grille ou Carrousel  ·  aperçu à droite",
    "Boîtes verticales ou icônes carrées",
    "Téléchargées depuis la base Launcher5 sur Internet",
    "Diaporama de tes fonds d'écran au repos",
    "Volume des clics et du lancement de jeu",
    "Néon, Rétro 8-bit, Doux, Sci-fi, Cristal",
    "Joué à l'ouverture  ·  écoute en changeant",
    "Revenir au launcher tout seul quand tu fermes un jeu",
    "Affiche la section où tu ranges tes jeux dans tes dossiers",
    "Montre ou cache les jeux que tu as masqués",
    "Pour tes captures et tes lives",
    "✕ pour choisir : manuel, plugin OnionHEN ou autoloader",
    "Vérifie une fois par jour sur GitHub",
    "✕ vérifie, télécharge et installe la dernière version",
    "✕ affiche la manette et les raccourcis du launcher",
    "Ferme le launcher et revient à l'accueil de la PS5",
  };
  /* liste des reglages (defile si elle depasse l'ecran) */
  if (set_row < set_top) set_top = set_row;
  if (set_row >= set_top + SR_VISIBLE) set_top = set_row - SR_VISIBLE + 1;
  int px = CAR_X, py = 165, pw = 1010, rh = 96;
  draw_text(S.f22, tr("PARAMÈTRES"), px + 4, py - 8, S.th->accent, 0, 0);
  for (int r = set_top; r < set_top + SR_VISIBLE && r < SR_COUNT; r++) {
    int y = py + 26 + (r - set_top) * rh, on = r == set_row, quit = r == SR_QUIT;
    if (quit) y += 14;
    uint32_t ac = quit ? 0xff4d6a : S.th->accent;
    if (on) {
      if (!S.lite) glow(px + 70, y + rh / 2 - 4, 140, ac, 70);
      fill_round(px, y, pw, rh - 10, 18, ac, 46);
      fill_round(px, y + 14, 6, rh - 38, 3, ac, 255);
    } else fill_round(px, y, pw, rh - 10, 18, 0x05070b, 165);
    /* icone ronde */
    fill_round(px + 26, y + 15, 56, 56, 28, on ? ac : 0x1a2030, 255);
    draw_text(S.f30, ICON[r], px + 54, y + 22, on ? 0x05070b : ac, 1, 0);
    draw_text(S.f30, tr(LABEL[r]), px + 106, y + 12, quit ? 0xff8a9a : 0xffffff, 0, 560);
    draw_text(S.f22, tr(HELP[r]), px + 108, y + 52, 0x9aa4b5, 0, 600);
    /* valeur */
    char v[96]; int tog;
    settings_value(r, v, sizeof v, &tog);
    int right = px + pw - 30, my = y + (rh - 10) / 2;
    if (tog >= 0) {                              /* interrupteur */
      int sw = 92, sh = 44, sx = right - sw;
      fill_round(sx, my - sh / 2, sw, sh, sh / 2, tog ? ac : 0x2a3142, 255);
      fill_round(tog ? sx + sw - sh + 4 : sx + 4, my - sh / 2 + 4, sh - 8, sh - 8, (sh - 8) / 2, 0xffffff, 255);
      draw_text(S.f22, tr(tog ? "OUI" : "NON"), sx - 16, my - 14, tog ? 0xffffff : 0x9aa4b5, 2, 0);
    } else if (v[0]) {
      int vw = text_w(S.f26, v);
      if (on && r != SR_UPDATE) {
        draw_text(S.f26, "▶", right, my - 17, ac, 2, 0);
        draw_text(S.f26, v, right - 40, my - 17, 0xffffff, 2, 300);
        draw_text(S.f26, "◀", right - 54 - (vw < 300 ? vw : 300), my - 17, ac, 2, 0);
      } else draw_text(S.f26, v, right, my - 17, ac, 2, 300);
    }
  }
  /* indicateur de defilement */
  int lh = SR_VISIBLE * rh - 10, ly = py + 26;
  fill_round(px + pw + 16, ly, 6, lh, 3, 0xffffff, 30);
  int kh = lh * SR_VISIBLE / SR_COUNT, ky = ly + (lh - kh) * set_top / (SR_COUNT - SR_VISIBLE);
  fill_round(px + pw + 16, ky, 6, kh, 3, S.th->accent, 220);

  /* carte de droite : apercu de l'affichage, sinon infos */
  int cx = px + pw + 50, cw = W - CAR_X - cx, cy = py + 26, ch = SR_VISIBLE * rh + 4;
  fill_round(cx, cy, cw, ch, 22, 0x05070b, 185);
  outline_round(cx, cy, cw, ch, 22, 1, 0xffffff, 30);
  if (set_row == SR_VIEW) { draw_view_preview(cx, cy, cw, ch); return; }
  int tx = cx + 40, ty = cy + 36;
  draw_text(S.f48, "LAUNCHER", tx, ty, 0xffffff, 0, 0);
  draw_text(S.f48, "5", tx + text_w(S.f48, "LAUNCHER") + 6, ty, S.th->accent, 0, 0);
  draw_text(S.f26, "v" L5_VERSION "  ·  by " L5_AUTHOR, tx + 2, ty + 70, 0x9aa4b5, 0, 0);
  fill_round(tx, ty + 116, 120, 5, 2, S.th->accent2, 255);

  static char ip[64]; static long long mb = -1; static int tc = -1; static Uint32 t0;
  if (!t0 || SDL_GetTicks() - t0 > 5000) { plat_sysinfo(ip, sizeof ip, &mb, &tc); t0 = SDL_GetTicks(); }
  long total = 0; int napps = 0;
  for (int i = 0; i < S.menu.napps; i++) if (S.menu.apps[i].kind != KIND_SYSTEM) napps++;
  for (int i = 0; i < S.menu.nstats; i++) total += S.menu.stats[i].playtime;
  static const char *LANGS[L_COUNT] = { "Français", "English", "Español", "Deutsch", "Italiano", "Português" };
  char val[6][64];
  if (S.menu.hide_ip) snprintf(val[0], 64, "%s", tr("Masquée"));
  else snprintf(val[0], 64, "%s", ip[0] ? ip : "—");
  if (mb >= 0) snprintf(val[1], 64, mb >= 10240 ? tr("%lld Go libres") : tr("%lld Mo libres"), mb >= 10240 ? mb / 1024 : mb);
  else snprintf(val[1], 64, "—");
  if (tc > 0) snprintf(val[2], 64, "%d°C", tc); else snprintf(val[2], 64, "—");
  snprintf(val[3], 64, "%s", LANGS[i18n_lang()]);
  snprintf(val[4], 64, "%d", napps);
  if (total > 0) playtime_str(total, val[5], 64); else snprintf(val[5], 64, "—");
  static const char *KEYS[6] = { "Adresse IP (FTP)", "Stockage", "Température", "Langue", "Jeux et applications", "Temps de jeu total" };
  for (int i = 0; i < 6; i++) {
    int y = ty + 160 + i * 86;
    draw_text(S.f22, tr(KEYS[i]), tx, y, 0x9aa4b5, 0, cw - 80);
    draw_text(S.f30, val[i], tx, y + 30, i == 0 && S.menu.hide_ip ? 0x9aa4b5 : 0xffffff, 0, cw - 80);
  }
}

static void handle_inner(int in);

/* Chaque appui : sortie de l'intro / de l'economiseur, puis son selon ce qui a change */
static void handle(int in) {
  S.last_input = SDL_GetTicks();
  if (!S.boot_done) { S.boot_done = 1; return; }
  if (S.saver_on) { S.saver_on = 0; S.saver_t = 0; saver_free(); return; }
  if (S.launching) return;
  struct { int mode, sec, depth, sel, csel, fsel, color, kbr, kbc, row; size_t len; } a, b;
  Theme before = S.edit;
#define SNAP(x) x.mode = S.mode; x.sec = S.sec; x.depth = S.depth; x.sel = *cur_sel(); x.csel = S.csel; x.fsel = S.fsel; \
  x.color = S.color_sel; x.kbr = S.kb_row; x.kbc = S.kb_col; x.row = S.ed_row * 64 + set_row; x.len = strlen(S.kb_buf)
  SNAP(a);
  handle_inner(in);
  SNAP(b);
#undef SNAP
  if (S.launching) return;                       /* son de lancement deja joue */
  if (b.mode != a.mode) sfx(in == IN_BACK ? SFX_BACK : SFX_OK);
  else if (b.sec != a.sec || b.depth != a.depth) sfx(b.depth < a.depth ? SFX_BACK : SFX_SECTION);
  else if (memcmp(&a, &b, sizeof a) || memcmp(&before, &S.edit, sizeof before)) sfx(in == IN_OK ? SFX_OK : SFX_MOVE);
}

static void handle_inner(int in) {
  if (boot_job && in == IN_BACK) { boot_cancel = 1; return; }   /* ○ : annule les payloads au demarrage */
  if (S.mode == MODE_BROWSE && S.sec == SEC_SYS) {     /* menu Parametres */
    if (settings_input(in)) return;
    if (in == IN_MENU || in == IN_START || in == IN_MOVE) return;
  }
  if (S.mode == MODE_CATALOG) { catalog_input(in); return; }   /* avant la navigation normale */
  if (S.mode == MODE_THEME) { theme_editor_input(in); return; }
  if (S.mode == MODE_KEYBOARD) {
    switch (in) {
    case IN_UP: move_cursor(0, -1); break;
    case IN_DOWN: move_cursor(0, 1); break;
    case IN_LEFT: move_cursor(-1, 0); break;
    case IN_RIGHT: move_cursor(1, 0); break;
    case IN_BACK: keyboard_done(0); break;
    case IN_MOVE: kb_backspace(); break;
    case IN_MENU: kb_type(" "); break;
    case IN_START: keyboard_done(1); break;
    case IN_OK:
      if (S.kb_row == KB_SPECIAL_ROW) {
        if (S.kb_col == 0) S.kb_shift = !S.kb_shift;
        else if (S.kb_col == 1) kb_type(" ");
        else if (S.kb_col == 2) kb_backspace();
        else keyboard_done(1);
      } else {
        char ch[8];
        utf8_at(KB_ROWS[S.kb_shift][S.kb_row], S.kb_col, ch);
        kb_type(ch);
        if (S.kb_shift && strlen(S.kb_buf) == strlen(ch)) S.kb_shift = 0;
      }
      break;
    }
    return;
  }
  switch (in) {
  case IN_UP: move_cursor(0, -1); return;
  case IN_DOWN: move_cursor(0, 1); return;
  case IN_LEFT: if (S.mode == MODE_BROWSE || S.mode == MODE_MOVE) move_cursor(-1, 0); return;
  case IN_RIGHT: if (S.mode == MODE_BROWSE || S.mode == MODE_MOVE) move_cursor(1, 0); return;
  }
  if (S.mode == MODE_MOVE) {
    if (in == IN_OK || in == IN_BACK || in == IN_MOVE) { S.mode = MODE_BROWSE; save(); }
    return;
  }
  if (S.mode == MODE_CONTEXT) {
    if (in == IN_OK) do_action(S.choices[S.csel].action);
    else if (in == IN_BACK || in == IN_MENU) S.mode = MODE_BROWSE;
    return;
  }
  if (S.mode == MODE_PICKFOLDER) {
    if (in == IN_OK) {
      if (!S.folders[S.fsel].folder) { open_keyboard("Nom du nouveau dossier", "", ACT_TO_FOLDER); return; }
      char msg[200];
      snprintf(msg, sizeof msg, tr("Rangé dans « %s »"), S.folders[S.fsel].folder->name);
      move_selected_to(S.folders[S.fsel].folder);
      toast(msg);
      S.mode = MODE_BROWSE;
    } else if (in == IN_BACK) S.mode = MODE_BROWSE;
    return;
  }
  if (S.mode == MODE_HELP) {
    if (help_kind == 2 && in == IN_OK) { status_repair(); return; }   /* Etat de la console : reparer */
    if (in == IN_BACK || in == IN_OK || in == IN_MENU) S.mode = MODE_BROWSE;
    return;
  }
  if (S.mode == MODE_INFO) {
    if (in == IN_OK && info_app) { S.mode = MODE_BROWSE; launch(info_app); }
    else if (in == IN_BACK || in == IN_MENU) S.mode = MODE_BROWSE;
    return;
  }
  if (S.mode == MODE_COLOR) {
    Entry *e = selected();
    if (in == IN_OK && e && e->is_folder) { e->node->color = PALETTE[S.color_sel]; save(); }
    if (in == IN_OK || in == IN_BACK) S.mode = MODE_BROWSE;
    return;
  }
  Entry *e = selected();
  switch (in) {
  case IN_OK:
    if (!e) break;
    if (e->is_folder) do_action(ACT_OPEN);
    else launch(e->app);
    break;
  case IN_BACK:
    if (search_q[0]) { search_q[0] = 0; *cur_sel() = 0; S.scroll = 0; toast("Recherche effacée"); break; }
    if (S.sec == SEC_HOME && S.depth) { S.depth--; S.scroll = (float)S.sel[S.depth]; }
    break;
  case IN_SEARCH: if (S.sec != SEC_SYS) open_keyboard("Rechercher", search_q, ACT_SEARCH); break;
  case IN_MENU: S.quick = 0; open_context(); break;
  case IN_START: open_quick(); break;
  case IN_MOVE:
    if (S.sec == SEC_HOME) { if (e) do_action(ACT_MOVE); break; }
    if (S.sec == SEC_SYS) break;
    filter_now = (filter_now + 1) % FIL_COUNT;   /* □ : filtre suivant */
    *cur_sel() = 0;
    S.scroll = 0;
    {
      char msg[96];
      snprintf(msg, sizeof msg, tr("Filtre : %s"), tr(FILTER_NAMES[filter_now]));
      toast(msg);
    }
    break;
  case IN_L1: change_section(-1); break;
  case IN_R1: change_section(1); break;
  case IN_VIEW: set_view(S.menu.view + 1); break;
  case IN_L2: jump_letter(-1); break;
  case IN_R2: jump_letter(1); break;
  }
}

static int key_to_input(SDL_Keycode k) {
  switch (k) {
  case SDLK_UP: return IN_UP;
  case SDLK_DOWN: return IN_DOWN;
  case SDLK_LEFT: return IN_LEFT;
  case SDLK_RIGHT: return IN_RIGHT;
  case SDLK_RETURN: case SDLK_KP_ENTER: return IN_OK;
  case SDLK_ESCAPE: case SDLK_BACKSPACE: return IN_BACK;
  case SDLK_TAB: return IN_MENU;
  case SDLK_SPACE: return IN_MOVE;
  case SDLK_PAGEUP: return IN_L1;
  case SDLK_PAGEDOWN: return IN_R1;
  case SDLK_v: case SDLK_F2: return IN_VIEW;
  case SDLK_HOME: return IN_L2;
  case SDLK_END: return IN_R2;
  case SDLK_f: case SDLK_F3: return IN_SEARCH;
  }
  return -1;
}

static int button_to_input(int b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_DPAD_UP: return IN_UP;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return IN_DOWN;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return IN_LEFT;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return IN_RIGHT;
  case SDL_CONTROLLER_BUTTON_A: return IN_OK;
  case SDL_CONTROLLER_BUTTON_B: return IN_BACK;
  case SDL_CONTROLLER_BUTTON_Y: return IN_MENU;
  case SDL_CONTROLLER_BUTTON_X: return IN_MOVE;
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return IN_L1;
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return IN_R1;
  case SDL_CONTROLLER_BUTTON_START: return IN_START;
  case SDL_CONTROLLER_BUTTON_LEFTSTICK: case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return IN_SEARCH;
  case SDL_CONTROLLER_BUTTON_TOUCHPAD: case SDL_CONTROLLER_BUTTON_BACK: return IN_VIEW;
  }
  return -1;
}

static struct { int input; Uint32 next; } held = { -1, 0 };

static void poll(void) {
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
    case SDL_QUIT: S.running = 0; break;
    case SDL_TEXTINPUT: if (S.mode == MODE_KEYBOARD) kb_type(ev.text.text); break;
    case SDL_KEYDOWN: {
      SDL_Keycode k = ev.key.keysym.sym;
      if (S.mode == MODE_KEYBOARD) {
        if (k == SDLK_BACKSPACE) kb_backspace();
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) keyboard_done(1);
        else if (k == SDLK_ESCAPE) keyboard_done(0);
        else if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT) handle(key_to_input(k));
        break;
      }
      int in = key_to_input(k);
      if (in >= 0) handle(in);
      break;
    }
    case SDL_CONTROLLERDEVICEADDED: {
      SDL_GameController *c = SDL_GameControllerOpen(ev.cdevice.which);
      for (int i = 0; i < 4 && c; i++) if (!S.pads[i] || S.pads[i] == c) { S.pads[i] = c; break; }
      break;
    }
    case SDL_CONTROLLERBUTTONDOWN: {
      int in = button_to_input(ev.cbutton.button);
      if (in >= 0) {
        handle(in);
        if (in <= IN_RIGHT) { held.input = in; held.next = SDL_GetTicks() + 380; }
      }
      break;
    }
    case SDL_CONTROLLERBUTTONUP:
      if (button_to_input(ev.cbutton.button) == held.input) held.input = -1;
      break;
    case SDL_CONTROLLERAXISMOTION: {
      static int last_dir = -1;
      int v = ev.caxis.value, dir = -1;
      if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) dir = v < -16000 ? IN_LEFT : v > 16000 ? IN_RIGHT : -2;
      else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) dir = v < -16000 ? IN_UP : v > 16000 ? IN_DOWN : -2;
      else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        static int down[2];                      /* L2 / R2 : gachettes, un appui = un saut */
        int k = ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
        if (v > 20000 && !down[k]) { down[k] = 1; handle(k ? IN_R2 : IN_L2); }
        else if (v < 8000) down[k] = 0;
        break;
      }
      else break;
      if (dir >= 0 && dir != last_dir) { handle(dir); held.input = dir; held.next = SDL_GetTicks() + 380; last_dir = dir; }
      if (dir == -2) { if (held.input == last_dir) held.input = -1; last_dir = -1; }
      break;
    }
    }
  }
  if (held.input >= 0 && SDL_GetTicks() >= held.next) {
    handle(held.input);
    held.next = SDL_GetTicks() + 90;
  }
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* Affichage : ouvert au demarrage, libere quand la PS5 passe le        */
/* launcher en arriere-plan (profil, parametres…) et repris au retour.  */
/* ------------------------------------------------------------------ */
static int video_open(void) {
#ifdef __PS5__
  S.win = SDL_CreateWindow("Launcher5", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, SDL_WINDOW_FULLSCREEN);
#else
  if (getenv("L5_SOFTWARE"))  /* meme taille que la TV pour mesurer comme sur la PS5 */
    S.win = SDL_CreateWindow("Launcher5", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
  else
    S.win = SDL_CreateWindow("Launcher5", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
#endif
  if (!S.win) { plat_log("SDL_CreateWindow : %s", SDL_GetError()); return 1; }
  static int first = 1;
  if (first) { plat_after_window(); first = 0; }
  if (!getenv("L5_SOFTWARE"))   /* L5_SOFTWARE=1 : simule la PS5 (dessin sans carte graphique) */
    S.r = SDL_CreateRenderer(S.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  if (!S.r) S.r = SDL_CreateRenderer(S.win, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
  if (!S.r) S.r = SDL_CreateRenderer(S.win, -1, SDL_RENDERER_SOFTWARE);
  if (!S.r) { plat_log("SDL_CreateRenderer : %s", SDL_GetError()); return 1; }
  {
    SDL_RendererInfo info;
    SDL_GetRendererInfo(S.r, &info);
    plat_log("rendu : %s (flags %x)", info.name, info.flags);
    S.lite = (info.flags & SDL_RENDERER_SOFTWARE) != 0;
    if (S.lite) plat_log("mode leger active (dessin sans carte graphique)");
  }
  SDL_RenderSetLogicalSize(S.r, W, H);
  SDL_SetRenderDrawBlendMode(S.r, SDL_BLENDMODE_BLEND);
  S.glow = make_glow();
  S.tile_glow = make_glow_n(TILE_SEL * 8 / 5);
  S.btn_glow = make_glow_n(360);
  S.shade_left = make_shade(1);
  S.shade_bottom = make_shade(0);
  return 0;
}

__attribute__((unused)) static void video_close(void) {   /* plus utilise : l'arriere-plan ferme l'interface par le systeme */
  /* detruire le moteur de rendu libere toutes ses textures : on oublie les pointeurs */
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    a->tex = a->bgtex = a->tex_s = a->tex_l = a->tex_xl = NULL;
    a->tex_failed = a->bg_failed = 0;
  }
  for (size_t i = 0; i < sizeof tcache / sizeof tcache[0]; i++) tcache[i].tex = NULL;
  S.glow = S.tile_glow = S.btn_glow = S.shade_left = S.shade_bottom = S.bgcache = NULL;
  S.bg_cur = S.bg_prev = NULL;
  S.bg_key_app = NULL;
  for (int i = 0; i < 36; i++) S.glyph[i] = NULL;                 /* lettres de la pluie de code */
  S.wall = NULL; S.wall_key[0] = 0;
  warp_forget(NULL);                                      /* caches du cover flow */
  for (int i = 0; i < 12; i++) FCACHE[i].t = NULL;
  SDL_DestroyRenderer(S.r);
  SDL_DestroyWindow(S.win);
  S.r = NULL;
  S.win = NULL;
}

/* Menu ou theme modifie depuis le PC (par FTP) : on recharge sans redemarrer */
static void import_sessions(void) {
  char path[PATH_LEN];
  data_path(path, sizeof path, "sessions.txt");
  int n = menu_import_sessions(&S.menu, path);
  if (n > 0) { plat_log("temps de jeu : %d partie(s) ajoutee(s)", n); save(); }
}

/* liste des jeux reecrite par le bootstrap (jeu installe, FPKG, dump monte par
   ShadowMount ou Kraken) : les nouveaux arrivent sans relancer le launcher */
static long long apps_sig;
static void apps_path(char *p, size_t n) { snprintf(p, n, "%s/apps.json", plat_data_dir()); }

static void check_new_games(void) {
  char p[PATH_LEN];
  apps_path(p, sizeof p);
  long long sig = file_sig(p);
  if (!sig || sig == apps_sig || S.mode != MODE_BROWSE || S.launching) return;
  apps_sig = sig;
  int before = S.menu.napps;
  scan_all(&S.menu);
  int added = 0;
  const char *name = NULL;
  for (int i = before; i < S.menu.napps; i++)
    if (S.menu.apps[i].kind == KIND_PS5 || S.menu.apps[i].kind == KIND_PS4) { added++; name = S.menu.apps[i].title; }
  if (S.menu.napps == before) return;
  menu_merge_new_apps(&S.menu);
  plat_log("liste des jeux : %d application(s) ajoutee(s)", S.menu.napps - before);
  if (!added) return;
  char msg[200];
  if (added == 1) snprintf(msg, sizeof msg, tr("« %s » ajouté"), name);
  else snprintf(msg, sizeof msg, tr("%d nouveaux jeux ajoutés"), added);
  toast(msg);
  sfx(SFX_OK);
}

static void check_external_changes(void) {
  check_new_games();
  import_sessions();
  box_check_custom();
  long long sig = file_sig(S.menu_path);
  if (sig && sig != S.menu_sig && S.mode == MODE_BROWSE) {
    /* nouveau menu lu a part : si le fichier est illisible (envoi en cours),
       on garde le menu actuel et on reessaie un peu plus tard */
    Menu tmp = S.menu;           /* memes applications trouvees sur la console */
    memset(&tmp.root, 0, sizeof tmp.root);
    tmp.hidden = NULL; tmp.nhidden = 0;
    tmp.names = NULL; tmp.nnames = 0;
    tmp.stats = NULL; tmp.nstats = 0;
    tmp.show_folders = 0; tmp.view = VIEW_FLOW; tmp.theme[0] = 0;
    tmp.favs = NULL; tmp.nfavs = 0; tmp.saver = 5;
    tmp.sfx_pack = 0; tmp.boot_snd = 0; tmp.hide_ip = 0; tmp.boxes = 1; tmp.covers_online = 1; tmp.auto_update = 1;
    if (!menu_load(&tmp, S.menu_path)) {
      S.menu_sig = sig;
      menu_reset(&S.menu);       /* libere l'ancien menu (pas les applications) */
      S.menu = tmp;
      menu_prune_missing(&S.menu);
      menu_merge_new_apps(&S.menu);
      S.depth = 0;
      S.grid_top = 0; S.grid_scroll = 0;
      plat_log("menu recharge (modifie depuis le PC)");
      toast("Menu mis à jour depuis le PC");
    } else {
      menu_reset(&tmp);
      static long long warned;
      if (warned != sig) { warned = sig; plat_log("menu.json illisible pour l'instant, nouvel essai bientot"); }
    }
  }
  char path[PATH_LEN];
  data_path(path, sizeof path, "theme.json");
  sig = file_sig(path);
  if (sig && sig != S.theme_sig && S.mode != MODE_THEME) {
    Theme t;
    if (!theme_load(path, &t)) {   /* illisible (envoi en cours) : nouvel essai plus tard */
      S.theme_sig = sig;
      S.cur = t;
      S.base = &S.cur;
      theme_changed();
      plat_log("theme recharge (modifie depuis le PC) : %s", t.name);
      char msg[120];
      snprintf(msg, sizeof msg, tr("Thème « %s » reçu depuis le PC"), tr(t.name));
      toast(msg);
    }
  }
}

int main(int argc, char **argv) {
  if (argc > 1 && argv[1] && !strcmp(argv[1], "--prepare")) return starter_prepare();   /* sans ecran */
  if (plat_init()) return 1;
  i18n_init(plat_sys_lang());   /* langue de la console */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  if (getenv("L5_NOBATCH")) SDL_SetHint(SDL_HINT_RENDER_BATCHING, "0");   /* mesures precises */
#ifdef __PS5__
  SDL_SetHint("SDL_VIDEODRIVER", "ps5");
#endif
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS)) {
    plat_log("SDL_Init : %s", SDL_GetError());
    return 1;
  }
  TTF_Init();
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

  if (video_open()) return 1;

#define FONT(var, data, size) var = TTF_OpenFontRW(SDL_RWFromConstMem(data, data##_len), 1, size)
  FONT(S.f22, font_regular, 22);
  FONT(S.f26, font_regular, 26);
  FONT(S.f30, font_bold, 30);
  FONT(S.f36, font_bold, 36);
  FONT(S.f48, font_bold, 56);
  FONT(S.f84, font_bold, 88);
  FONT(S.fbig, font_bold, 120);

  for (int i = 0; i < 70; i++) {
    S.part[i].x = (float)(rand() % W); S.part[i].y = (float)(rand() % H);
    S.part[i].s = 2 + rand() % 3; S.part[i].v = 0.2f + (rand() % 100) / 160.0f;
  }

  scan_all(&S.menu);
  { char ap[PATH_LEN]; apps_path(ap, sizeof ap); apps_sig = file_sig(ap); }
  snprintf(S.menu_path, sizeof S.menu_path, "%s/menu.json", plat_data_dir());
  S.menu.saver = 5;                     /* economiseur apres 5 min par defaut */
  S.menu.boxes = 1;                     /* jaquettes boites par defaut */
  S.menu.covers_online = 1;
  S.menu.auto_update = 1;
  menu_load(&S.menu, S.menu_path);
  menu_prune_missing(&S.menu);
  menu_merge_new_apps(&S.menu);
  /* theme : theme.json (complet), sinon le theme fourni du meme nom */
  {
    char dir[PATH_LEN];
    static const char *const SUBS[] = { "themes", "fonds", "musique", "jaquettes", "cache" };
    for (int i = 0; i < 5; i++) { data_path(dir, sizeof dir, SUBS[i]); mkdir(dir, 0755); }
    data_path(dir, sizeof dir, "theme.json");
    S.theme_sig = file_sig(dir);
    if (theme_load(dir, &S.cur)) {
      S.cur = *theme_builtin(0);
      for (int i = 0; i < theme_builtin_count(); i++)
        if (!strcmp(theme_builtin(i)->name, S.menu.theme)) S.cur = *theme_builtin(i);
    }
    S.base = &S.cur;
    S.th = S.base;
    S.tiles_corners = S.cur.corners;
    scan_media_files();
    data_path(dir, sizeof dir, "catalog.json");
    if (menu_export_catalog(&S.menu, dir)) plat_log("impossible d'ecrire %s", dir);
  }
  save();

  for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);
  SDL_StopTextInput();

  /* Mode test : L5_SCRIPT="R,R,D,OK,TXT:texte,..." puis capture dans L5_SHOT */
  const char *script = getenv("L5_SCRIPT"), *shot = getenv("L5_SHOT");
  char *steps = script ? strdup(script) : NULL, *cursor = steps;
  int wait = 0;

  import_sessions();
  S.running = 1;
  S.boot_t0 = S.last_input = SDL_GetTicks();
  S.boot_done = steps && !getenv("L5_INTRO");    /* pas d'intro pendant les tests automatiques */
  if (!S.boot_done) sfx(SFX_BOOT);
  int saver_secs = getenv("L5_SAVER") ? atoi(getenv("L5_SAVER")) : 0;   /* test : economiseur plus rapide */
  Uint64 last = SDL_GetPerformanceCounter();
  up_boot();
  autostart_sync();                              /* copie a jour dans ps5_autoloader */
  /* les payloads au demarrage sont envoyes par le bootstrap, avant l'ouverture de l'interface ;
     on lui ecrit ici les fichiers a jour (un payload mis a jour change de fichier) */
  boot_refresh();
  first_run_autoload();                          /* premier lancement : inscription dans l'autoloader */
  starter_note();
  autoload_lost_check();                         /* Launcher5 retire de autoload.txt par un autre outil ? */
  while (S.running) {
    box_poll();
    up_poll();
    pcat_poll();
    status_poll();
    boot_poll();
    Uint64 now = SDL_GetPerformanceCounter();
    S.dt = (float)(now - last) / SDL_GetPerformanceFrequency();
    if (S.dt > 0.1f) S.dt = 0.1f;
    last = now;
    /* menu PS ouvert ou launcher en arriere-plan : on laisse la main au systeme */
    int overlaid = 0;
    plat_poll_system(&overlaid);
    static int in_bg, closing;
    static Uint32 bg_since;
    if (overlaid == 1) {
      /* la PS5 affiche son propre ecran (profil, parametres, accueil) : l'application
         relais ne sait pas lui ceder l'ecran (ecran noir). Apres une demi-seconde en
         arriere-plan, on se fait fermer proprement par le systeme ; la tuile Launcher5
         rouvre le launcher. */
      music_pause(1);
      if (!in_bg) { plat_log("arriere-plan : dessin suspendu"); in_bg = 1; bg_since = SDL_GetTicks(); }
      if (!closing && SDL_GetTicks() - bg_since > 500) {
        save();
        if (!plat_close_ui()) { closing = 1; plat_log("arriere-plan : fermeture par le systeme, la tuile rouvrira le launcher"); }
      }
      SDL_Delay(50); SDL_Event ev; while (SDL_PollEvent(&ev)) {}
      continue;
    }
    if (in_bg) { plat_log("retour au premier plan : dessin repris"); in_bg = 0; }
    if (!S.r) {
      plat_log("retour au premier plan : affichage repris");
      if (video_open()) { plat_log("reprise de l'affichage impossible"); break; }
    }
    music_pause(0);
    music_update();
    if (S.frame % 90 == 45) check_external_changes();
    {
      Uint32 limit = saver_secs ? (Uint32)saver_secs * 1000 : (Uint32)S.menu.saver * 60000;
      if (!S.saver_on && limit && S.mode == MODE_BROWSE && !S.launching && S.boot_done &&
          SDL_GetTicks() - S.last_input > limit) { S.saver_on = 1; S.saver_t = 0; }
    }   /* environ toutes les 1,5 s */
    if (overlaid == 2) SDL_Delay(30);                                                          /* menu PS ouvert */
    poll();
    if (steps && S.frame % 6 == 5) {
      char *tok = cursor ? strsep(&cursor, ",") : NULL;
      if (tok) {
        static const char *names[IN_COUNT] = { "U", "D", "L", "R", "OK", "BACK", "MENU", "MOVE", "L1", "R1", "START", "VIEW",
                                               "L2", "R2", "SEARCH" };
        if (!strncmp(tok, "TXT:", 4)) kb_type(tok + 4);
        else for (int i = 0; i < IN_COUNT; i++) if (!strcmp(tok, names[i])) handle(i);
      } else if (++wait > 8) {
        draw();
        if (shot) {
          int ow, oh;
          SDL_GetRendererOutputSize(S.r, &ow, &oh);
          SDL_Surface *full = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
          SDL_RenderReadPixels(S.r, NULL, SDL_PIXELFORMAT_ARGB8888, full->pixels, full->pitch);
          IMG_SavePNG(full, shot);
          SDL_FreeSurface(full);
        }
        S.running = 0;
      }
    }
    draw();
    S.frame++;
    /* mesure de fluidite (images par seconde) dans le journal */
    static Uint32 t0 = 0;
    if (!t0) t0 = SDL_GetTicks();
    if (S.frame % 600 == 0) {
      Uint32 t1 = SDL_GetTicks();
      double fps = 600000.0 / (t1 - t0 + 1);
      plat_log("fluidite : %.1f images/s", fps);
      if (getenv("L5_FPS")) { printf("fluidite : %.1f images/s\n", fps); fflush(stdout); }
      t0 = t1;
    }
  }
  free(steps);

  save();
  plat_quit();
  /* sortie directe : les fils (jaquettes, musique, mises a jour) peuvent encore tourner,
     un nettoyage complet de SDL pourrait les faire planter. Le systeme libere tout. */
  fflush(NULL);
  _exit(0);
}
