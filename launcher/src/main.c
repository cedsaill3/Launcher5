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

#ifdef HAVE_MIXER
#include <SDL2/SDL_mixer.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

extern const unsigned char font_regular[], font_bold[];
extern const unsigned int font_regular_len, font_bold_len;

#define W 1920
#define H 1080
#define TILE 230          /* tuiles carrees : image du jeu fournie par la PS5 */
#define TILE_SEL 300
#define FLOW 400          /* jaquette centrale du cover flow */
#define FLOW_SIDE 300     /* jaquettes laterales */
#define FLOW_CY 440       /* centre vertical du cover flow */
#define GRID_COLS 6
#define GRID_CELL 210
#define GRID_GAP 40
#define GRID_ROWH 274
#define GRID_Y 170
#define ED_W 820          /* largeur du studio de theme */
#define GAP 30
#define CAR_X 110
#define CAR_Y 690
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
enum { SEC_HOME, SEC_PS5, SEC_PS4, SEC_MEDIA, SEC_HB, SEC_EMU, SEC_PAYLOAD, SEC_RECENT, SEC_SYS, SEC_COUNT };
static const char *SEC_NAMES[SEC_COUNT] = {
  "Mes dossiers", "Jeux PS5", "Jeux PS4", "Médias", "Homebrews", "Émulateurs", "Payloads", "Récents", "Paramètres"
};
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
enum { R_PRESET, R_ACCENT, R_ACCENT2, R_TOP, R_BOTTOM, R_BG, R_WALL, R_ART, R_GLOW, R_PART, R_PSTYLE, R_FALL,
       R_CORNERS, R_CLOCK, R_MUSIC, R_VOLUME, R_SAVE, R_DELETE, R_COUNT };
enum {
  ACT_LAUNCH, ACT_OPEN, ACT_MOVE, ACT_TO_FOLDER, ACT_OUT_OF_FOLDER, ACT_RENAME, ACT_HIDE,
  ACT_SHOW, ACT_COLOR, ACT_DELETE_FOLDER, ACT_NEW_FOLDER, ACT_TOGGLE_HIDDEN, ACT_RESET_NAME, ACT_THEME, ACT_SAVE_THEME,
  ACT_VIEW,
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
  Theme *th;                          /* theme affiche (cur, ou edit pendant l'edition) */
  Theme cur, edit;                    /* theme applique / en cours de modification */
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
    if (S.sec == SEC_RECENT) {
      Stat *st = menu_stat(&S.menu, a->id, 0);
      if (!st || !st->last) continue;
    } else if (!sec_match(S.sec, a)) continue;
    Entry e = { 0, -1, a, NULL };
    push_entry(e);
  }
  qsort(S.entries, S.nentries, sizeof(Entry), S.sec == SEC_RECENT ? cmp_recent : cmp_name);
  if (S.sec == SEC_RECENT && S.nentries > 20) S.nentries = 20;
}

static void update_sections(void) {
  for (int s = 0; s < SEC_COUNT; s++) S.sec_has[s] = s == SEC_HOME && S.menu.show_folders;
  for (int i = 0; i < S.menu.napps; i++) {
    App *a = &S.menu.apps[i];
    if (menu_is_hidden(&S.menu, a->id) && !S.show_hidden) continue;
    for (int s = 1; s < SEC_COUNT; s++) if (s != SEC_RECENT && sec_match(s, a)) S.sec_has[s] = 1;
    Stat *st = menu_stat(&S.menu, a->id, 0);
    if (st && st->last) S.sec_has[SEC_RECENT] = 1;
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

/* Icone carree (icon0) */
static SDL_Texture *cover(App *a) {
  if (a->tex || a->tex_failed) return a->tex;
  a->tex = load_tex(a->icon);
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

/* Tuile complete au format boite (w x h), calculee une seule fois :
   jaquette verticale si on l'a, sinon carte avec l'icone et le nom. */
static SDL_Texture *tile_tex(App *a, int w, int h) {
  void **slot = w == TILE ? &a->tex_s : w == TILE_SEL ? &a->tex_l : w == FLOW ? &a->tex_xl : NULL;
  if (!slot) return NULL;                    /* seules ces tailles sont mises en cache */
  if (*slot) return *slot;
  SDL_Texture *t = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
  if (!t) return NULL;
  SDL_Texture *prev_rt = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, t);
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
  SDL_RenderClear(S.r);
  SDL_Texture *img = cover(a);
  if (img) SDL_RenderCopy(S.r, img, NULL, NULL);
  else draw_card_contents(a, w, h);
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
  if (stable && (S.bg_key_app != key_app || S.bg_key_ver != S.th_ver || (key_app && !key_app->bgtex && !key_app->bg_failed) || !S.bgcache)) {
    if (!S.bgcache) S.bgcache = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
    S.bg_key_app = key_app;
    S.bg_key_ver = S.th_ver;
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

  if (animated) draw_particles(0);
}

static void draw_topbar(void) {
  /* logo */
  int lx = CAR_X;
  draw_text(S.f36, "LAUNCHER", lx, 44, 0xffffff, 0, 0);
  int lw = text_w(S.f36, "LAUNCHER");
  draw_text(S.f36, "5", lx + lw + 4, 44, S.th->accent, 0, 0);
  draw_text_a(S.f22, "v" L5_VERSION "  ·  by " L5_AUTHOR, lx + 2, 90, 0x9aa4b5, 0, 0, 150);

  /* sections */
  int x = lx + lw + 90, tx = 0, tw = 0;
  for (int s = 0; s < SEC_COUNT; s++) {
    if (!S.sec_has[s]) continue;
    int w = text_w(S.f26, SEC_NAMES[s]);
    if (S.mode == MODE_THEME && x + w > W - 880) break;   /* sous le studio de theme */
    int on = s == S.sec;
    draw_text(S.f26, SEC_NAMES[s], x, 52, on ? 0xffffff : 0x9aa4b5, 0, 0);
    if (on) { tx = x; tw = w; }
    x += w + 46;
  }
  if (!S.tab_w) { S.tab_x = (float)tx; S.tab_w = (float)tw; }
  S.tab_x += (tx - S.tab_x) * ease(0.25f);
  S.tab_w += (tw - S.tab_w) * ease(0.25f);
  fill_round((int)S.tab_x, 92, (int)S.tab_w, 5, 2, S.th->accent, 255);
  if (!S.lite) glow((int)(S.tab_x + S.tab_w / 2), 94, (int)(S.tab_w * 0.9f), S.th->accent, ga(90));

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
      { "sys:home", "Ferme le launcher et revient à l'accueil de la PS5" },
    };
    snprintf(meta, n, "Réglage");
    for (size_t i = 0; i < sizeof DESC / sizeof DESC[0]; i++)
      if (!strcmp(DESC[i][0], e->app->id)) snprintf(meta, n, "%s", DESC[i][1]);
  } else {
    Stat *st = menu_stat(&S.menu, e->app->id, 0);
    if (st && st->launches) {
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

static void draw_app_tile(App *a, int x, int y, int w, int h) {
  int ts = w <= TILE ? TILE : TILE_SEL;
  SDL_Texture *t = tile_tex(a, ts, ts);
  if (t) {
    SDL_Rect d = { x, y, w, h };
    SDL_RenderCopy(S.r, t, NULL, &d);
  }
  if (show_badges() && a->kind != KIND_PS5) {
    const char *b = KIND_LABEL[a->kind];
    int bw = text_w(S.f22, b);
    fill_round(x + 10, y + 10, bw + 18, 32, 9, 0x000000, 175);
    draw_text(S.f22, b, x + 19, y + 13, 0xffffff, 0, 0);
  }
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
      SDL_Texture *t = tile_tex(a, TILE, TILE);
      if (t) { SDL_Rect d = { mx, my, mw, mh }; SDL_RenderCopy(S.r, t, NULL, &d); }
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
    int h = w;
    int ix = (int)x, iy = CAR_Y + (TILE_SEL - h);
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
  MIX(f->color); MIX(f->count); MIX(S.th_ver); MIX(S.th->corners);
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
    FCACHE[slot].t = SDL_CreateTexture(S.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, FLOW, FLOW);
    if (!FCACHE[slot].t) return NULL;
    SDL_SetTextureBlendMode(FCACHE[slot].t, SDL_BLENDMODE_BLEND);
  }
  SDL_Texture *prev_rt = SDL_GetRenderTarget(S.r);
  SDL_SetRenderTarget(S.r, FCACHE[slot].t);
  SDL_SetRenderDrawColor(S.r, 0, 0, 0, 0);
  SDL_RenderClear(S.r);
  draw_folder_tile(f, 0, 0, FLOW, FLOW);
  mask_corners(FLOW, FLOW, tile_rad() * FLOW / TILE_SEL);
  SDL_SetRenderTarget(S.r, prev_rt);   /* cible d'avant (dessin imbrique) */
  FCACHE[slot].key = key;
  FCACHE[slot].used = S.frame;
  return FCACHE[slot].t;
}

/* Jaquettes laterales (deja deformees, avec leur reflet) et reflet de la
   jaquette centrale : calcules une fois puis simplement recopies. Sans carte
   graphique (PS5), deformer des images a chaque image coute trop cher. */
#define SIDE_W (FLOW_SIDE * 55 / 100)
#define SIDE_H (FLOW_SIDE + 6 + FLOW_SIDE * 35 / 100 + 1)
#define REFL_H (6 + FLOW * 35 / 100 + 1)
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
  if (kind == WARP_REFL) draw_persp(src, 0, FLOW, -6.0f - FLOW / 2.0f, FLOW, FLOW, 255, 255, 2);
  else {
    float near = FLOW_SIDE, far = FLOW_SIDE * 0.8f;
    draw_persp(src, 0, SIDE_W, FLOW_SIDE / 2.0f, kind == WARP_LEFT ? far : near, kind == WARP_LEFT ? near : far, 255, 255, 1);
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
    float size = FLOW + (FLOW_SIDE - FLOW) * t;
    float wf = 1 - 0.45f * t, ratio = 1 - 0.2f * t;
    float cx = S.flow_cx + off, w = size * wf;
    if (cx + w / 2 < -20 || cx - w / 2 > W + 20) continue;
    if (S.mode == MODE_THEME && cx - w / 2 > W - ED_W - 30) continue;   /* cachee par le studio de theme */
    float near = size, far = size * ratio;
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
    SDL_Texture *tex = e->is_folder ? folder_tex(e->node) : tile_tex(e->app, FLOW, FLOW);
    uint32_t ver = e->is_folder ? folder_key(e->node) : 0;
    float cy = FLOW_CY + (moving ? 8 * sinf(S.frame * 0.25f) : 0);
    SDL_Texture *cached = NULL;
    if (tex && a >= 0.999f) {                     /* sur le cote : texture deja deformee */
      cached = warp_tex(tex, ver, rel < 0 ? WARP_LEFT : WARP_RIGHT);
      if (cached) {
        SDL_SetTextureColorMod(cached, bright, bright, bright);
        SDL_SetTextureAlphaMod(cached, alpha);
        SDL_Rect d = { (int)(cx - SIDE_W / 2.0f), (int)(cy - FLOW_SIDE / 2.0f), SIDE_W, SIDE_H };
        SDL_RenderCopy(S.r, cached, NULL, &d);
      }
    } else if (tex && a < 0.001f) {               /* au centre, immobile : image droite + reflet en cache */
      SDL_Rect d = { (int)(cx - FLOW / 2.0f), (int)(cy - FLOW / 2.0f), FLOW, FLOW };
      SDL_RenderCopy(S.r, tex, NULL, &d);
      cached = warp_tex(tex, ver, WARP_REFL);
      if (cached) { SDL_Rect r = { d.x, d.y + FLOW, FLOW, REFL_H }; SDL_RenderCopy(S.r, cached, NULL, &r); }
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
    else if (e->is_folder) draw_folder_tile(e->node, (int)(cx - w / 2), (int)(cy - size / 2), (int)w, (int)size);

    if (center && a < 0.05f) {
      int x = (int)(cx - size / 2), y = (int)(cy - size / 2), sz = (int)size, rad = tile_rad() * sz / TILE_SEL;
      if (!e->is_folder && menu_is_hidden(&S.menu, e->app->id)) {
        fill_round(x, y, sz, sz, rad, 0x000000, 160);
        draw_text(S.f26, "masqué", x + sz / 2, y + sz / 2 - 16, 0xcccccc, 1, 0);
      }
      if (!e->is_folder && show_badges() && e->app->kind != KIND_PS5) {
        const char *b = KIND_LABEL[e->app->kind];
        int bw = text_w(S.f22, b);
        fill_round(x + 12, y + 12, bw + 18, 32, 9, 0x000000, 175);
        draw_text(S.f22, b, x + 21, y + 15, 0xffffff, 0, 0);
      }
      outline_round(x, y, sz, sz, rad, 4, moving ? 0xffc53d : 0xffffff, 255);
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
      int ix = x + (GRID_CELL - sz) / 2, iy = (int)y + (GRID_CELL - sz) / 2;
      int moving = S.mode == MODE_MOVE && is_sel;
      if (is_sel) {
        uint32_t gc = moving ? 0xffc53d : S.th->accent;
        float pulse = 0.5f + 0.5f * sinf(S.frame * 0.08f);
        if (S.lite) {
          SDL_SetTextureColorMod(S.tile_glow, (gc >> 16) & 255, (gc >> 8) & 255, gc & 255);
          SDL_SetTextureAlphaMod(S.tile_glow, (Uint8)ga((int)(110 + 40 * pulse)));
          int gs = sz * 8 / 5;
          SDL_Rect gd = { ix + sz / 2 - gs / 2, iy + sz / 2 - gs / 2, gs, gs };
          SDL_RenderCopy(S.r, S.tile_glow, NULL, &gd);
        } else glow(ix + sz / 2, iy + sz / 2, (int)(sz * (0.8f + 0.08f * pulse)), gc, ga(150));
        fill_round(ix + 8, iy + 14, sz, sz, tile_rad() * sz / TILE_SEL, 0x000000, 120);
      }
      if (e->is_folder) draw_folder_tile(e->node, ix, iy, sz, sz);
      else draw_app_tile(e->app, ix, iy, sz, sz);
      if (is_sel) outline_round(ix, iy, sz, sz, tile_rad() * sz / TILE_SEL, 4, moving ? 0xffc53d : 0xffffff, 255);
      const char *name = e->is_folder ? e->node->name : menu_display_name(&S.menu, e->app);
      draw_text(is_sel ? S.f26 : S.f22, name, x + GRID_CELL / 2, (int)y + GRID_CELL + (is_sel ? 22 : 14),
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
    snprintf(buf, sizeof buf, tr("%s      L1 R1  Sections      Pavé tactile  Vue : %s"),
             tr(S.sec == SEC_HOME ? (S.depth ? "✕  Ouvrir      ○  Retour      △  Options      □  Déplacer"
                                             : "✕  Ouvrir / Jouer      △  Options      □  Déplacer")
                                  : "✕  Jouer      △  Options"),
             tr(VIEW_NAMES[S.menu.view]));
    hint = buf;
  }
  draw_text(S.f22, hint, CAR_X, H - 56, 0x9aa4b5, 0, 0);
  if (S.menu.view != VIEW_CLASSIC && S.nentries && S.mode != MODE_THEME) {
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

static void draw(void) {
  refresh_settings();
  check_tile_shape();
  update_sections();
  if (!S.sec_has[S.sec]) for (S.sec = 0; S.sec < SEC_COUNT - 1 && !S.sec_has[S.sec]; S.sec++) {}
  build_entries();
  clamp_sel();
  Entry *e = selected();
  int key = (S.sec << 20) ^ (S.depth << 16) ^ *cur_sel();
  if (key != S.last_sel_key) { S.last_sel_key = key; S.hero_anim = 0; }

  draw_background(e);
  draw_topbar();
  switch (S.menu.view) {
  case VIEW_GRID: draw_grid(); break;
  case VIEW_CLASSIC: draw_hero(e); draw_carousel(); break;
  default: draw_flow(); draw_flow_info(e); break;
  }
  draw_footer();
  if (S.mode == MODE_CONTEXT) draw_list_panel(S.ctx_title, S.nchoices, choice_label, S.csel, NULL);
  if (S.mode == MODE_PICKFOLDER) draw_list_panel("Ranger dans…", S.nfolders, folder_label, S.fsel, NULL);
  if (S.mode == MODE_COLOR) draw_list_panel("Couleur du dossier", NPALETTE, color_label, S.color_sel, color_swatch);
  if (S.mode == MODE_THEME) draw_theme_editor();
  if (S.mode == MODE_KEYBOARD) draw_keyboard();
  draw_launching();
  draw_toast();
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
}

static int launcher_setting(App *a) {
  if (strncmp(a->id, "cfg:", 4)) return 0;
  if (!strcmp(a->id, "cfg:theme")) open_theme_editor();
  else if (!strcmp(a->id, "cfg:view")) set_view(S.menu.view + 1);
  else if (!strcmp(a->id, "cfg:return")) plat_auto_return(!plat_auto_return(-1));
  else if (!strcmp(a->id, "cfg:folders")) { S.menu.show_folders = !S.menu.show_folders; save(); }
  else if (!strcmp(a->id, "cfg:hidden")) S.show_hidden = !S.show_hidden;
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
    if (a->kind != KIND_PAYLOAD && a->kind != KIND_SYSTEM) { S.launching = a; S.launch_t = SDL_GetTicks(); music_fade(); return; }
  }
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
enum { IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_OK, IN_BACK, IN_MENU, IN_MOVE, IN_L1, IN_R1, IN_START, IN_VIEW, IN_COUNT };

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
  S.th = &S.cur;
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
  S.th = &S.edit;
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
  "Thème de base", "Couleur principale", "Couleur secondaire", "Fond : haut", "Fond : bas", "Arrière-plan",
  "Image", "Illustration du jeu", "Halos lumineux", "Particules", "Forme", "Sens", "Coins des jaquettes",
  "Horloge", "Musique", "Volume", "Enregistrer comme mon thème…", "Supprimer ce thème",
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
    if (i < S.nbuiltin) {  /* les themes fournis gardent ta musique et ton image */
      snprintf(t->music, sizeof t->music, "%s", keep.music);
      t->volume = keep.volume;
      snprintf(t->wallpaper, sizeof t->wallpaper, "%s", keep.wallpaper);
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
    S.th = &S.cur;            /* annule : on revient au theme applique */
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
  case R_ACCENT: return "COULEURS";
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
  const Theme *t = &S.edit;
  switch (r) {
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
  for (int r = R_ACCENT; r < R_COUNT; r++) {
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
  for (int r = R_ACCENT; r < R_COUNT; r++) {
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
    } else if (r == R_ART || r == R_GLOW || r == R_VOLUME) {
      int v = r == R_ART ? S.edit.art : r == R_GLOW ? S.edit.glow : S.edit.volume;
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
#else
static void music_update(void) {}
static void music_pause(int on) { (void)on; }
static void music_fade(void) {}
#endif

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

static void handle(int in) {
  if (S.launching) return;
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
    case SDL_CONTROLLERDEVICEADDED: SDL_GameControllerOpen(ev.cdevice.which); break;
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
  S.wall = NULL; S.wall_key[0] = 0;
  warp_forget(NULL);                                      /* caches du cover flow */
  for (int i = 0; i < 12; i++) FCACHE[i].t = NULL;
  SDL_DestroyRenderer(S.r);
  SDL_DestroyWindow(S.win);
  S.r = NULL;
  S.win = NULL;
}

/* Menu ou theme modifie depuis le PC (par FTP) : on recharge sans redemarrer */
static void check_external_changes(void) {
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
      S.th = &S.cur;
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
  menu_load(&S.menu, S.menu_path);
  menu_prune_missing(&S.menu);
  menu_merge_new_apps(&S.menu);
  /* theme : theme.json (complet), sinon le theme fourni du meme nom */
  {
    char dir[PATH_LEN];
    static const char *const SUBS[] = { "themes", "fonds", "musique" };
    for (int i = 0; i < 3; i++) { data_path(dir, sizeof dir, SUBS[i]); mkdir(dir, 0755); }
    data_path(dir, sizeof dir, "theme.json");
    S.theme_sig = file_sig(dir);
    if (theme_load(dir, &S.cur)) {
      S.cur = *theme_builtin(0);
      for (int i = 0; i < theme_builtin_count(); i++)
        if (!strcmp(theme_builtin(i)->name, S.menu.theme)) S.cur = *theme_builtin(i);
    }
    S.th = &S.cur;
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

  S.running = 1;
  Uint64 last = SDL_GetPerformanceCounter();
  while (S.running) {
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
    if (S.frame % 90 == 45) check_external_changes();   /* environ toutes les 1,5 s */
    if (overlaid == 2) SDL_Delay(30);                                                          /* menu PS ouvert */
    poll();
    if (steps && S.frame % 6 == 5) {
      char *tok = cursor ? strsep(&cursor, ",") : NULL;
      if (tok) {
        static const char *names[IN_COUNT] = { "U", "D", "L", "R", "OK", "BACK", "MENU", "MOVE", "L1", "R1", "START", "VIEW" };
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
