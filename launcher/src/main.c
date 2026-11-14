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
#include <ctype.h>
#include <jansson.h>

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
enum { MODE_BROWSE, MODE_CONTEXT, MODE_MOVE, MODE_KEYBOARD, MODE_PICKFOLDER, MODE_COLOR, MODE_THEME };
/* lignes de l'editeur de theme */
enum { R_PRESET, R_AUTO, R_ACCENT, R_ACCENT2, R_TOP, R_BOTTOM, R_BG, R_WALL, R_ART, R_FX, R_GLOW, R_PART, R_PSTYLE,
       R_FALL, R_CORNERS, R_CLOCK, R_SFX, R_MUSIC, R_VOLUME, R_SAVE, R_DELETE, R_COUNT };
enum {
  ACT_LAUNCH, ACT_OPEN, ACT_MOVE, ACT_TO_FOLDER, ACT_OUT_OF_FOLDER, ACT_RENAME, ACT_HIDE,
  ACT_SHOW, ACT_COLOR, ACT_DELETE_FOLDER, ACT_NEW_FOLDER, ACT_TOGGLE_HIDDEN, ACT_RESET_NAME, ACT_THEME, ACT_SAVE_THEME,
  ACT_VIEW, ACT_FAV, ACT_UNFAV,
};

typedef struct { int is_folder; int index; App *app; Node *node; } Entry;
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
  Choice choices[16]; int nchoices, csel;
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
static int tile_rad(void) { static const int R[] = { 0, 18, 44 }; return R[S.th->corners]; }

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

static void build_entries(void) {
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

static SDL_Surface *box_compose(const char *bg, const char *icon) {
  SDL_Surface *ic = icon[0] ? argb(IMG_Load(icon)) : NULL;
  SDL_Surface *art = bg[0] ? argb(IMG_Load(bg)) : NULL;
  SDL_Surface *src = art ? art : ic;
  if (!src) return NULL;
  SDL_Surface *d = SDL_CreateRGBSurfaceWithFormat(0, BOXW, BOXH, 32, SDL_PIXELFORMAT_ARGB8888);
  if (d) {
    stretch_smooth(src, crop_to(src, BOXW, BOXH), d, NULL);
    if (!art) darken(d, 0, 0, BOXW, BOXH, 0, 0.45f);   /* pas d'illustration : l'icone agrandie en fond */
    /* bas assombri pour faire ressortir l'icone */
    for (int y = BOXH * 45 / 100; y < BOXH; y++) {
      float k = 1 - 0.7f * (y - BOXH * 45 / 100) / (float)(BOXH * 55 / 100);
      darken(d, 0, y, BOXW, 1, 0, k);
    }
    if (ic) {
      int is = BOXW * 60 / 100, x = (BOXW - is) / 2, y = BOXH - is - BOXW * 9 / 100, rad = is / 10;
      darken(d, x + 4, y + 8, is, is, rad, 0.45f);  /* ombre */
      SDL_Surface *t = SDL_CreateRGBSurfaceWithFormat(0, is, is, 32, SDL_PIXELFORMAT_ARGB8888);
      if (t) {
        stretch_smooth(ic, (SDL_Rect){ 0, 0, ic->w, ic->h }, t, NULL);
        Uint32 *px = t->pixels;                  /* coins arrondis */
        for (int yy = 0; yy < is; yy++)
          for (int xx = 0; xx < is; xx++) {
            int dx = xx < rad ? rad - xx : xx >= is - rad ? xx - (is - rad - 1) : 0;
            int dy = yy < rad ? rad - yy : yy >= is - rad ? yy - (is - rad - 1) : 0;
            if (dx * dx + dy * dy > rad * rad) px[yy * (t->pitch / 4) + xx] = 0;
          }
        SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_Rect dr = { x, y, is, is };
        SDL_BlitSurface(t, NULL, d, &dr);
        SDL_FreeSurface(t);
      }
    }
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
      if (s) custom = 1;                         /* telle quelle, sans bandeau */
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
    snprintf(BX.cache, sizeof BX.cache, "%s/cache/%s.jpg", plat_data_dir(), sid);
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

/* contenu d'une jaquette boite (w x h) dans la cible de rendu courante */
static void draw_box_contents(App *a, int w, int h) {
  SDL_Texture *art = box_art(a);
  if (art) SDL_RenderCopy(S.r, art, NULL, NULL);
  else {                                         /* provisoire : icone sur fond sombre */
    uint32_t c = a->has_vibe ? a->vibe : hash_color(a->id);
    fill_round(0, 0, w, h, 0, shade(c, 0.22f), 255);
    SDL_Texture *ic = cover(a);
    int is = w * 80 / 100;
    if (ic) { SDL_Rect d = { (w - is) / 2, (h - is) / 2, is, is }; SDL_RenderCopy(S.r, ic, NULL, &d); }
  }
  if (a->box_state == 2 && a->box_custom) return; /* jaquette perso : telle quelle */
  /* bandeau du haut, facon boitier */
  int bh = h * 9 / 100;
  uint32_t bc = a->kind == KIND_PS5 ? 0xf4f4f4 : a->kind == KIND_PS4 ? 0x0a3d91 : S.th->accent;
  uint32_t tc = a->kind == KIND_PS5 ? 0x0a0a12 : 0xffffff;
  fill_round(0, 0, w, bh, 0, bc, 255);
  const char *lab = a->kind == KIND_PS5 ? "PS5" : a->kind == KIND_PS4 ? "PS4" : KIND_LABEL[a->kind];
  if (w >= 300) draw_text(S.f26, lab, w / 2, (bh - 30) / 2, tc, 1, w - 60);
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
static void drop_tiles(App *a) {
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
    if (!S.bgcache) S.bgcache = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
    S.bg_key_app = key_app;
    S.bg_key_ver = S.th_ver;
    S.bg_key_acc = S.th->accent ^ S.th->accent2 << 1;
    SDL_SetRenderTarget(S.r, S.bgcache);
    S.bg_cur = S.bg_prev = NULL;
    S.bg_fade = 1;
    draw_background_full(key_app ? e : NULL, 0);
    SDL_SetRenderTarget(S.r, NULL);
    SDL_SetTextureBlendMode(S.bgcache, SDL_BLENDMODE_NONE);
  }
  SDL_RenderCopy(S.r, S.bgcache, NULL, NULL);
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
    SDL_Rect rr = { 0, y, W, 6 };
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
  SDL_Rect full = { -40, -40, W + 80, H + 80 };
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
  SDL_Rect left = { 0, 0, W * 3 / 4, H };
  SDL_RenderCopy(S.r, S.shade_left, NULL, &left);
  SDL_Rect bottom = { 0, H / 2, W, H / 2 };
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
  for (int s = 0; s < SEC_COUNT; s++) {
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
  for (int s = 0; s < SEC_COUNT; s++) {
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
    draw_text_a(S.f30, S.sec == SEC_HOME ? "Range des jeux dedans avec △ › Ranger dans un dossier." : "Lance des jeux pour remplir cette section.",
                x, y + 180, 0x9aa4b5, 0, 0, a);
    return;
  }

  /* pastille du type */
  const char *kind = e->is_folder ? "DOSSIER" : KIND_LABEL[e->app->kind];
  uint32_t kc = e->is_folder ? e->node->color : S.th->accent;
  int kw = text_w(S.f22, kind);
  fill_round(x, y, kw + 30, 38, 19, kc, (int)(0.9f * a));
  draw_text_a(S.f22, kind, x + 15, y + 6, 0x0a0a12, 0, 0, a);
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
#define SIDE_H (TH(FLOW_SIDE) + 6 + TH(FLOW_SIDE) * 35 / 100 + 1)
#define REFL_H (6 + TH(FLOW) * 35 / 100 + 1)
enum { WARP_LEFT, WARP_RIGHT, WARP_REFL };
static struct { SDL_Texture *src, *t; uint32_t ver; int kind; Uint32 used; } WCACHE[40];

static void warp_forget(SDL_Texture *src) {
  for (int i = 0; i < 40; i++)
    if (WCACHE[i].t && (!src || WCACHE[i].src == src)) {
      if (src) SDL_DestroyTexture(WCACHE[i].t);
      WCACHE[i].t = NULL; WCACHE[i].src = NULL;
    }
}

static SDL_Texture *warp_tex(SDL_Texture *src, uint32_t ver, int kind) {
  ver = ver * 31 + (uint32_t)S.th_ver;
  int slot = 0;
  for (int i = 0; i < 40; i++) {
    if (WCACHE[i].t && WCACHE[i].src == src && WCACHE[i].ver == ver && WCACHE[i].kind == kind) { WCACHE[i].used = S.frame; return WCACHE[i].t; }
    if (!WCACHE[i].t || (WCACHE[slot].t && WCACHE[i].used < WCACHE[slot].used)) slot = i;
  }
  int w = kind == WARP_REFL ? FLOW : SIDE_W, h = kind == WARP_REFL ? REFL_H : SIDE_H;
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
    float near = TH(FLOW_SIDE), far = near * 0.8f;
    draw_persp(src, 0, SIDE_W, near / 2.0f, kind == WARP_LEFT ? far : near, kind == WARP_LEFT ? near : far, 255, 255, 1);
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
    SDL_Texture *cached = NULL;
    if (tex && a >= 0.999f) {                     /* sur le cote : texture deja deformee */
      cached = warp_tex(tex, ver, rel < 0 ? WARP_LEFT : WARP_RIGHT);
      if (cached) {
        SDL_SetTextureColorMod(cached, bright, bright, bright);
        SDL_SetTextureAlphaMod(cached, alpha);
        SDL_Rect d = { (int)(cx - SIDE_W / 2.0f), (int)(cy - TH(FLOW_SIDE) / 2.0f), SIDE_W, SIDE_H };
        SDL_RenderCopy(S.r, cached, NULL, &d);
      }
    } else if (tex && a < 0.001f) {               /* au centre, immobile : image droite + reflet en cache */
      SDL_Rect d = { (int)(cx - FLOW / 2.0f), (int)(cy - TH(FLOW) / 2.0f), FLOW, TH(FLOW) };
      SDL_RenderCopy(S.r, tex, NULL, &d);
      cached = warp_tex(tex, ver, WARP_REFL);
      if (cached) { SDL_Rect r = { d.x, d.y + TH(FLOW), FLOW, REFL_H }; SDL_RenderCopy(S.r, cached, NULL, &r); }
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
      int x = (int)(cx - size / 2), y = (int)(cy - hsz / 2), sz = (int)size, szh = (int)hsz, rad = tile_rad() * sz / TILE_SEL;
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
    draw_text_a(S.f26, S.sec == SEC_HOME ? "Range des jeux dedans avec △ › Ranger dans un dossier." : "Lance des jeux pour remplir cette section.",
                cx, FLOW_CY + 40, 0x9aa4b5, 1, maxw, a);
    return;
  }
  const char *kind = e->is_folder ? "DOSSIER" : KIND_LABEL[e->app->kind];
  uint32_t kc = e->is_folder ? e->node->color : S.th->accent;
  int kw = text_w(S.f22, kind);
  fill_round(cx - (kw + 30) / 2, y, kw + 30, 36, 18, kc, (int)(0.9f * a));
  draw_text_a(S.f22, kind, cx, y + 5, 0x0a0a12, 1, 0, a);
  const char *title = e->is_folder ? e->node->name : menu_display_name(&S.menu, e->app);
  draw_text_a(S.f48, title, cx, y + 46, 0xffffff, 1, maxw, a);
  char meta[200];
  meta_line(e, meta, sizeof meta);
  draw_text_a(S.f26, meta, cx, y + 124, 0xc9d1dc, 1, maxw, a);
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
                                  : "✕  Jouer      △  Options"),
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
static void theme_saved_as(const char *name);

static void draw_list_panel(const char *title, int n, const char *(*label)(int), int sel, uint32_t (*swatch)(int)) {
  int pw = 760, ph = 120 + n * 66, px = (W - pw) / 2, py = (H - ph) / 2;
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
  draw_text(S.f36, S.kb_title, px + 44, py + 30, 0xffffff, 0, pw - 88);
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

/* image recadree au format de l'ecran et un peu plus grande, preparee une fois :
   ensuite elle glisse sans etre redimensionnee (fluide sans carte graphique) */
#define SV_W (W * 106 / 100)
#define SV_H (H * 106 / 100)
static SDL_Texture *saver_prep(SDL_Texture *t) {
  int tw, th;
  SDL_QueryTexture(t, NULL, NULL, &tw, &th);
  SDL_Rect src = { 0, 0, tw, th };
  if ((long)tw * H > (long)th * W) { src.w = th * W / H; src.x = (tw - src.w) / 2; }
  else { src.h = tw * H / W; src.y = (th - src.h) / 2; }
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
    float pan = ((i ? 1.0f : 0.0f) + (float)el / SHOW) / 2;   /* 0..1 : glisse doucement d'un cote */
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

static void draw(void) {
  frame_theme(selected());
  refresh_settings();
  check_tile_shape();
  update_sections();
  if (!S.sec_has[S.sec]) for (S.sec = 0; S.sec < SEC_COUNT - 1 && !S.sec_has[S.sec]; S.sec++) {}
  build_entries();
  clamp_sel();
  Entry *e = selected();
  int key = (S.sec << 20) ^ (S.depth << 16) ^ *cur_sel();
  if (key != S.last_sel_key) { S.last_sel_key = key; S.hero_anim = 0; S.sel_since = SDL_GetTicks(); }
  frame_theme(e);

  if (S.saver_on) {
    draw_saver();
    if (S.saver_on) { SDL_RenderPresent(S.r); return; }
  }
  draw_background(e);
  draw_fx();
  draw_topbar();
  switch (S.menu.view) {
  case VIEW_GRID: if (S.sec == SEC_SYS) draw_settings(); else draw_grid(); break;
  case VIEW_CLASSIC: if (S.sec == SEC_SYS) draw_settings(); else { draw_hero(e); draw_carousel(); } break;
  default: if (S.sec == SEC_SYS) draw_settings(); else { draw_flow(); draw_flow_info(e); } break;
  }
  draw_footer();
  if (S.mode == MODE_CONTEXT) draw_list_panel(S.ctx_title, S.nchoices, choice_label, S.csel, NULL);
  if (S.mode == MODE_PICKFOLDER) draw_list_panel("Ranger dans…", S.nfolders, folder_label, S.fsel, NULL);
  if (S.mode == MODE_COLOR) draw_list_panel("Couleur du dossier", NPALETTE, color_label, S.color_sel, color_swatch);
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
    int hid = menu_is_hidden(&S.menu, e->app->id);
    add_choice(hid ? "Ne plus masquer" : "Masquer", hid ? ACT_SHOW : ACT_HIDE);
  } else {
    snprintf(S.ctx_title, sizeof S.ctx_title, "Options");
  }
  if (S.sec == SEC_HOME) add_choice("+ Nouveau dossier ici", ACT_NEW_FOLDER);
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

static void launch(App *a) {
  if (launcher_setting(a)) return;
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

static void draw_launching(void) {
  if (!S.launching) return;
  Uint32 el = SDL_GetTicks() - S.launch_t;
  if (el > 12000) { S.launching = NULL; toast("Le lancement prend du temps…"); return; }
  set_color(0x000000, el > 300 ? 225 : (int)(225 * el / 300));
  SDL_Rect full = { 0, 0, W, H };
  SDL_RenderFillRect(S.r, &full);
  App *a = S.launching;
  int size = 420, x = (W - size) / 2, y = H / 2 - size / 2 - 70;
  float pulse = 0.5f + 0.5f * sinf(S.frame * 0.1f);
  glow(W / 2, y + size / 2, size, S.th->accent, (int)(90 + 60 * pulse));
  SDL_Texture *t = cover(a);
  if (t) { SDL_Rect d = { x, y, size, size }; SDL_RenderCopy(S.r, t, NULL, &d); }
  else fill_round(x, y, size, size, 24, shade(hash_color(a->id), 0.6f), 255);
  outline_round(x, y, size, size, 16, 3, 0xffffff, 200);
  draw_text(S.f48, menu_display_name(&S.menu, a), W / 2, y + size + 34, 0xffffff, 1, 1400);
  char dots[8] = "";
  for (int i = 0; i < (int)(el / 400) % 4; i++) strcat(dots, ".");
  char line[40];
  snprintf(line, sizeof line, tr("Lancement%s"), dots);
  draw_text(S.f30, line, W / 2, y + size + 116, S.th->accent, 1, 0);
}

static void do_action(int act) {
  Entry *e = selected();
  S.mode = MODE_BROWSE;
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
       IN_COUNT };

/* ------------------------------------------------------------------ */
/* Themes : application, editeur a la manette, musique d'ambiance       */
/* ------------------------------------------------------------------ */
static const char *const IMG_EXTS[] = { "png", "jpg", "jpeg", "webp", "bmp", NULL };
static const char *const MUSIC_EXTS[] = { "ogg", "mp3", "flac", "wav", "mod", "xm", "it", "s3m", NULL };

static void data_path(char *out, size_t n, const char *sub) { snprintf(out, n, "%s/%s", plat_data_dir(), sub); }

static void theme_changed(void) { S.th_ver++; }

/* les tuiles deja calculees sont a refaire si leur forme change */
static void check_tile_shape(void) {
  if (S.tiles_corners == S.th->corners) return;
  S.tiles_corners = S.th->corners;
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
  "Thème de base", "Couleurs", "Couleur principale", "Couleur secondaire", "Fond : haut", "Fond : bas", "Arrière-plan",
  "Image", "Illustration du jeu", "Effet animé", "Halos lumineux", "Particules", "Forme", "Sens", "Coins des jaquettes",
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
    if (i < S.nbuiltin) {  /* les themes fournis gardent ta musique, et ton image s'ils n'ont pas la leur */
      snprintf(t->music, sizeof t->music, "%s", keep.music);
      t->volume = keep.volume;
      if (!t->wallpaper[0]) snprintf(t->wallpaper, sizeof t->wallpaper, "%s", keep.wallpaper);
    }
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
    if (S.ed_dirty && S.preset >= 0 && !strstr(S.edit.name, tr(" perso"))) {
      char n[64];
      snprintf(n, sizeof n, "%.38s%s", tr(S.edit.name), tr(" perso"));
      snprintf(S.edit.name, sizeof S.edit.name, "%s", n);
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
  case R_AUTO: return "COULEURS";
  case R_BG: return "ARRIÈRE-PLAN";
  case R_GLOW: return "EFFETS";
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
  static const char *const AUTO[] = { "Du thème", "Du jeu" };
  static const char *const FXN[] = { "Aucun", "Grille", "Hyperespace", "Pluie" };
  const Theme *t = &S.edit;
  switch (r) {
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
static struct { int state; Mix_Music *m; char key[THEME_FILE_LEN]; int pos, vol, paused; volatile int finished; } MU = { .vol = -1 };
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
  MU.state = 1;
  plat_log("musique prete");
  return 1;
}

static void music_stop(void) {
  if (MU.m) { Mix_HaltMusic(); Mix_FreeMusic(MU.m); MU.m = NULL; }
  MU.finished = 0;
}

static void music_play_file(const char *file, int loops) {
  char path[PATH_LEN];
  snprintf(path, sizeof path, "%s/musique/%s", plat_data_dir(), file);
  music_stop();
  MU.m = Mix_LoadMUS(path);
  if (!MU.m) { plat_log("musique illisible : %s (%s)", path, Mix_GetError()); return; }
  if (Mix_PlayMusic(MU.m, loops)) plat_log("lecture impossible : %s", Mix_GetError());
  else plat_log("musique : %s", file);
}

static void music_update(void) {
  const char *want = S.th->music;
  if (!want[0] || (!strcmp(want, "*") && !S.nmusics)) {
    if (MU.m) music_stop();
    MU.key[0] = 0;
    return;
  }
  if (!music_ready()) return;
  if (MU.vol != S.th->volume) { MU.vol = S.th->volume; Mix_VolumeMusic(MU.vol * MIX_MAX_VOLUME / 100); }
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
  if (!MU.m || MU.paused == on) return;
  MU.paused = on;
  if (on) Mix_PauseMusic(); else Mix_ResumeMusic();
}
static void music_fade(void) { if (MU.m) Mix_FadeOutMusic(600); }

/* --- sons d'interface : fabriques au premier usage ----------------------- */
#define BOOT_TECHNO 6                            /* son de demarrage : morceau fourni */
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
static const float BOOT_DUR[BOOT_SOUNDS] = { 1.0f, 1.7f, 1.9f, 1.0f, 1.5f, 0, 0 };   /* 5 aucun, 6 fichier */

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
      if (sfx_boot_built == BOOT_TECHNO) {         /* morceau fourni (sons/) */
        char path[PATH_LEN];
        snprintf(path, sizeof path, "%s/sons/demarrage-techno.mp3", plat_data_dir());
        if (!(SFXC[SFX_BOOT] = Mix_LoadWAV(path))) plat_log("son de demarrage illisible : %s", Mix_GetError());
      } else sfx_build(SFX_BOOT, sfx_boot_built);
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
  int s = S.sec;
  for (int i = 0; i < SEC_COUNT; i++) {
    s = (s + dir + SEC_COUNT) % SEC_COUNT;
    if (S.sec_has[s]) break;
  }
  S.sec = s;
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
  if (dy && S.menu.view == VIEW_GRID) {
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

static void up_install(void) {
  char path[PATH_LEN];
  up_path(path, sizeof path, "launcher5.elf");
  toast(tr("Installation de la mise à jour : le launcher redémarre…"));
  draw(); SDL_RenderPresent(S.r);
  save();
  if (plat_send_elf(path)) {
    toast(tr("Chargeur ELF introuvable : envoie /data/launcher5/launcher5.elf"));
    UP.install = 0;
  }
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
    up_set(UP_READY, NULL);                      /* plus recente que celle qui tourne */
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
    if (UP.install) { up_start(1); break; }
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
enum { SR_THEME, SR_VIEW, SR_COVERS, SR_ONLINE, SR_SAVER, SR_SFX, SR_SFXPACK, SR_BOOT, SR_RETURN, SR_FOLDERS, SR_HIDDEN, SR_HIDEIP,
       SR_AUTOUPD, SR_UPDATE, SR_QUIT, SR_COUNT };
#define SR_VISIBLE 8                             /* lignes affichees a la fois */
static int set_row, set_top;

static const int SAVER_STEPS[] = { 0, 2, 5, 10, 30 };
static const char *PACK_NAMES[SFX_PACKS] = { "Néon", "Rétro 8-bit", "Doux", "Sci-fi", "Cristal" };
static const char *BOOT_NAMES[BOOT_SOUNDS] = { "Arpège néon", "Synthwave", "Console", "8-bit", "Hyperespace", "Aucun", "Techno" };
static const int BOOT_ORDER[BOOT_SOUNDS] = { 6, 0, 1, 2, 3, 4, 5 };   /* ordre dans la liste (« Aucun » en dernier) */

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
      if (i < S.nbuiltin) {                      /* garde la musique, et l'image si le theme n'en a pas */
        snprintf(t.music, sizeof t.music, "%s", S.cur.music);
        t.volume = S.cur.volume;
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
    {
      int pos = 0;
      while (pos < BOOT_SOUNDS - 1 && BOOT_ORDER[pos] != S.menu.boot_snd) pos++;
      S.menu.boot_snd = BOOT_ORDER[wrap_step(pos, d, BOOT_SOUNDS)];
    }
    save();
    sfx(SFX_BOOT);
    break;
  case SR_RETURN: plat_auto_return(!plat_auto_return(-1)); break;
  case SR_FOLDERS: S.menu.show_folders = !S.menu.show_folders; save(); break;
  case SR_HIDDEN: S.show_hidden = !S.show_hidden; break;
  case SR_HIDEIP: S.menu.hide_ip = !S.menu.hide_ip; S.sys_t = 0; save(); break;
  case SR_AUTOUPD: S.menu.auto_update = !S.menu.auto_update; save(); break;
  case SR_UPDATE:                                /* ✕ seulement : verifier, telecharger puis installer */
    if (!activate || UP.busy) break;
    UP.install = 1;
    if (UP.state == UP_READY) up_install();
    else if (UP.state == UP_AVAILABLE) up_start(1);
    else up_start(0);
    break;
  case SR_QUIT:
    if (activate) { App *a = menu_find_app(&S.menu, "sys:home"); if (a) launch(a); }
    break;
  }
}

/* renvoie 1 si la touche est prise par le menu */
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
  static const char *ICON[SR_COUNT] = { "◐", "▦", "▯", "⬇", "☾", "♪", "♫", "★", "↺", "▤", "◌", "⊘", "◷", "↻", "⌂" };
  static const char *LABEL[SR_COUNT] = {
    "Thème", "Affichage des jeux", "Jaquettes", "Jaquettes officielles", "Économiseur d'écran", "Sons d'interface", "Style des sons", "Son de démarrage",
    "Retour automatique", "Section Mes dossiers", "Éléments masqués", "Masquer l'IP", "Mises à jour automatiques", "Mettre à jour",
    "Quitter vers l'accueil PS5",
  };
  static const char *HELP[SR_COUNT] = {
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
    "Vérifie une fois par jour sur GitHub",
    "✕ vérifie, télécharge et installe la dernière version",
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
  if (S.mode == MODE_BROWSE && S.sec == SEC_SYS) {     /* menu Parametres */
    if (settings_input(in)) return;
    if (in == IN_MENU || in == IN_START || in == IN_MOVE) return;
  }
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
  case IN_BACK: if (S.sec == SEC_HOME && S.depth) { S.depth--; S.scroll = (float)S.sel[S.depth]; } break;
  case IN_MENU: case IN_START: open_context(); break;
  case IN_MOVE: if (e && S.sec == SEC_HOME) do_action(ACT_MOVE); break;
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

static void video_close(void) {
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

static void check_external_changes(void) {
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
  (void)argc; (void)argv;
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
  while (S.running) {
    box_poll();
    up_poll();
    Uint64 now = SDL_GetPerformanceCounter();
    S.dt = (float)(now - last) / SDL_GetPerformanceFrequency();
    if (S.dt > 0.1f) S.dt = 0.1f;
    last = now;
    /* menu PS ouvert ou launcher en arriere-plan : on laisse la main au systeme */
    int overlaid = 0;
    plat_poll_system(&overlaid);
    if (overlaid == 1) {
      /* la PS5 affiche son propre ecran (profil, parametres…) : on lui rend
         l'affichage sans fermer le launcher */
      music_pause(1);
      if (S.r) { plat_log("arriere-plan : affichage libere"); video_close(); }
      SDL_Delay(50); SDL_Event ev; while (SDL_PollEvent(&ev)) {}
      continue;
    }
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
                                               "L2", "R2" };
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
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
  plat_quit();
  return 0;
}
