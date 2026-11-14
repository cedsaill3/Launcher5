/*
 * Launcher5 - modele du menu et enregistrement JSON.
 *
 * Launcher5 (c) 2026 cedsaill
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#include "menu.h"

static void copy(char *dst, const char *src, size_t n) {
  snprintf(dst, n, "%s", src ? src : "");
}

/* ------------------------------------------------------------------ */
/* Applications                                                        */
/* ------------------------------------------------------------------ */
App *menu_find_app(Menu *m, const char *id) {
  for (int i = 0; i < m->napps; i++)
    if (!strcmp(m->apps[i].id, id)) return &m->apps[i];
  return NULL;
}

App *menu_add_app(Menu *m, const App *a) {
  App *ex = menu_find_app(m, a->id);
  if (ex) return ex;
  if (m->napps == m->capapps) {
    m->capapps = m->capapps ? m->capapps * 2 : 64;
    m->apps = realloc(m->apps, sizeof(App) * m->capapps);
  }
  m->apps[m->napps] = *a;
  return &m->apps[m->napps++];
}

const char *menu_display_name(Menu *m, const App *a) {
  for (int i = 0; i < m->nnames; i++)
    if (!strcmp(m->names[i].id, a->id)) return m->names[i].name;
  return a->title;
}

/* ------------------------------------------------------------------ */
/* Noeuds                                                              */
/* ------------------------------------------------------------------ */
Node *node_insert(Node *parent, int index, const Node *n) {
  if (parent->count == parent->cap) {
    parent->cap = parent->cap ? parent->cap * 2 : 16;
    parent->items = realloc(parent->items, sizeof(Node) * parent->cap);
  }
  if (index < 0 || index > parent->count) index = parent->count;
  memmove(&parent->items[index + 1], &parent->items[index], sizeof(Node) * (parent->count - index));
  parent->items[index] = *n;
  parent->count++;
  return &parent->items[index];
}

void node_remove(Node *parent, int index, Node *out) {
  if (index < 0 || index >= parent->count) return;
  if (out) *out = parent->items[index];
  else node_free(&parent->items[index]);
  memmove(&parent->items[index], &parent->items[index + 1], sizeof(Node) * (parent->count - index - 1));
  parent->count--;
}

void node_free(Node *n) {
  for (int i = 0; i < n->count; i++) node_free(&n->items[i]);
  free(n->items);
  n->items = NULL;
  n->count = n->cap = 0;
}

Node *menu_new_folder(Menu *m, Node *parent, int index, const char *name, uint32_t color) {
  (void)m;
  Node f = {0};
  f.is_folder = 1;
  copy(f.name, name, sizeof f.name);
  f.color = color;
  return node_insert(parent, index, &f);
}

/* ------------------------------------------------------------------ */
/* Personnalisation                                                    */
/* ------------------------------------------------------------------ */
int menu_is_hidden(Menu *m, const char *id) {
  for (int i = 0; i < m->nhidden; i++)
    if (!strcmp(m->hidden[i], id)) return 1;
  return 0;
}

void menu_set_hidden(Menu *m, const char *id, int hidden) {
  for (int i = 0; i < m->nhidden; i++) {
    if (!strcmp(m->hidden[i], id)) {
      if (!hidden) {
        memmove(&m->hidden[i], &m->hidden[i + 1], ID_LEN * (m->nhidden - i - 1));
        m->nhidden--;
      }
      return;
    }
  }
  if (hidden) {
    m->hidden = realloc(m->hidden, ID_LEN * (m->nhidden + 1));
    copy(m->hidden[m->nhidden++], id, ID_LEN);
  }
}

int menu_is_fav(Menu *m, const char *id) {
  for (int i = 0; i < m->nfavs; i++)
    if (!strcmp(m->favs[i], id)) return 1;
  return 0;
}

void menu_set_fav(Menu *m, const char *id, int fav) {
  for (int i = 0; i < m->nfavs; i++) {
    if (!strcmp(m->favs[i], id)) {
      if (!fav) {
        memmove(&m->favs[i], &m->favs[i + 1], ID_LEN * (m->nfavs - i - 1));
        m->nfavs--;
      }
      return;
    }
  }
  if (fav) {
    char (*p)[ID_LEN] = realloc(m->favs, ID_LEN * (m->nfavs + 1));
    if (!p) return;
    m->favs = p;
    copy(m->favs[m->nfavs++], id, ID_LEN);
  }
}

void menu_rename_app(Menu *m, const char *id, const char *name) {
  for (int i = 0; i < m->nnames; i++) {
    if (!strcmp(m->names[i].id, id)) {
      copy(m->names[i].name, name, NAME_LEN);
      return;
    }
  }
  m->names = realloc(m->names, sizeof(*m->names) * (m->nnames + 1));
  copy(m->names[m->nnames].id, id, ID_LEN);
  copy(m->names[m->nnames].name, name, NAME_LEN);
  m->nnames++;
}

/* ------------------------------------------------------------------ */
/* Statistiques                                                        */
/* ------------------------------------------------------------------ */
Stat *menu_stat(Menu *m, const char *id, int create) {
  for (int i = 0; i < m->nstats; i++)
    if (!strcmp(m->stats[i].id, id)) return &m->stats[i];
  if (!create) return NULL;
  m->stats = realloc(m->stats, sizeof(Stat) * (m->nstats + 1));
  Stat *s = &m->stats[m->nstats++];
  memset(s, 0, sizeof *s);
  copy(s->id, id, ID_LEN);
  return s;
}

void menu_record_launch(Menu *m, const char *id) {
  Stat *s = menu_stat(m, id, 1);
  s->launches++;
  s->last = time(NULL);
}

/* Le bootstrap ajoute une ligne « ID secondes » a chaque partie terminee.
   Le fichier est mis de cote avant lecture : une partie qui se termine
   pendant ce temps est simplement notee dans un nouveau fichier. */
int menu_import_sessions(Menu *m, const char *path) {
  char work[PATH_LEN + 8];
  snprintf(work, sizeof work, "%s.lu", path);
  if (rename(path, work)) return 0;
  FILE *f = fopen(work, "r");
  int n = 0;
  if (f) {
    char id[ID_LEN + 8];
    long secs;
    while (fscanf(f, "%47s %ld", id, &secs) == 2) {
      if (secs <= 0 || secs > 7L * 24 * 3600 || strlen(id) >= ID_LEN) continue;
      menu_stat(m, id, 1)->playtime += secs;
      n++;
    }
    fclose(f);
  }
  remove(work);
  return n;
}

/* ------------------------------------------------------------------ */
/* Fusion avec ce qui est installe                                     */
/* ------------------------------------------------------------------ */
static int node_contains_app(const Node *n, const char *id) {
  for (int i = 0; i < n->count; i++) {
    const Node *c = &n->items[i];
    if (c->is_folder ? node_contains_app(c, id) : !strcmp(c->ref, id)) return 1;
  }
  return 0;
}

void menu_merge_new_apps(Menu *m) {
  for (int i = 0; i < m->napps; i++) {
    if (m->apps[i].kind == KIND_SYSTEM) continue;   /* raccourcis : section Systeme */
    if (!node_contains_app(&m->root, m->apps[i].id)) {
      Node n = {0};
      copy(n.ref, m->apps[i].id, ID_LEN);
      node_insert(&m->root, -1, &n);
    }
  }
}

static void prune(Menu *m, Node *n) {
  for (int i = n->count - 1; i >= 0; i--) {
    Node *c = &n->items[i];
    if (c->is_folder) prune(m, c);
    else if (!menu_find_app(m, c->ref)) node_remove(n, i, NULL);
  }
}

void menu_prune_missing(Menu *m) { prune(m, &m->root); }

void menu_reset(Menu *m) {
  node_free(&m->root);
  free(m->hidden); m->hidden = NULL; m->nhidden = 0;
  free(m->names); m->names = NULL; m->nnames = 0;
  free(m->stats); m->stats = NULL; m->nstats = 0;
  free(m->favs); m->favs = NULL; m->nfavs = 0;
  m->saver = 5;
  m->sfx_pack = 0; m->boot_snd = 0; m->hide_ip = 0; m->boxes = 1; m->covers_online = 1;
  m->show_folders = 0;
  m->view = VIEW_FLOW;
  m->theme[0] = 0;
}

/* Tout ce que le launcher a trouve, pour editer le menu depuis le PC */
int menu_export_catalog(Menu *m, const char *path) {
  static const char *KINDS[KIND_COUNT] = { "PS5", "PS4", "HOMEBREW", "PAYLOAD", "MEDIA", "SYSTEM" };
  json_t *arr = json_array();
  for (int i = 0; i < m->napps; i++) {
    const App *a = &m->apps[i];
    if (a->kind == KIND_SYSTEM) continue;
    json_array_append_new(arr, json_pack("{s:s, s:s, s:s, s:s, s:b, s:b}", "id", a->id, "title", a->title,
                                         "kind", KINDS[a->kind], "icon", a->icon,
                                         "media", (a->flags & APPF_MEDIA) != 0, "emu", (a->flags & APPF_EMU) != 0));
  }
  char tmp[PATH_LEN + 8];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  int rc = json_dump_file(arr, tmp, JSON_INDENT(1));
  json_decref(arr);
  if (rc) return -1;
  return rename(tmp, path);
}

/* ------------------------------------------------------------------ */
/* JSON                                                                */
/* ------------------------------------------------------------------ */
static uint32_t parse_color(const char *s) {
  if (s && s[0] == '#') return (uint32_t)strtoul(s + 1, NULL, 16) & 0xffffff;
  return 0x3d8bfd;
}

static void load_nodes(Node *parent, json_t *arr) {
  size_t i; json_t *v;
  json_array_foreach(arr, i, v) {
    Node n = {0};
    json_t *app = json_object_get(v, "app");
    json_t *folder = json_object_get(v, "folder");
    if (json_is_string(app)) {
      copy(n.ref, json_string_value(app), ID_LEN);
      node_insert(parent, -1, &n);
    } else if (json_is_string(folder)) {
      n.is_folder = 1;
      copy(n.name, json_string_value(folder), NAME_LEN);
      n.color = parse_color(json_string_value(json_object_get(v, "color")));
      Node *f = node_insert(parent, -1, &n);
      load_nodes(f, json_object_get(v, "items"));
    }
  }
}

int menu_load(Menu *m, const char *path) {
  json_error_t err;
  json_t *j = json_load_file(path, 0, &err);
  if (!j) return -1;
  load_nodes(&m->root, json_object_get(j, "root"));

  size_t i; json_t *v; const char *k;
  json_array_foreach(json_object_get(j, "hidden"), i, v)
    if (json_is_string(v)) menu_set_hidden(m, json_string_value(v), 1);
  json_object_foreach(json_object_get(j, "names"), k, v)
    if (json_is_string(v)) menu_rename_app(m, k, json_string_value(v));
  json_object_foreach(json_object_get(j, "stats"), k, v) {
    Stat *s = menu_stat(m, k, 1);
    s->launches = (int)json_integer_value(json_object_get(v, "n"));
    s->last = (time_t)json_integer_value(json_object_get(v, "last"));
    s->playtime = (long)json_integer_value(json_object_get(v, "t"));
  }
  json_array_foreach(json_object_get(j, "favorites"), i, v)
    if (json_is_string(v)) menu_set_fav(m, json_string_value(v), 1);
  json_t *sv = json_object_get(j, "saver");
  m->saver = json_is_integer(sv) ? (int)json_integer_value(sv) : 5;
  if (m->saver < 0 || m->saver > 120) m->saver = 5;
  m->sfx_pack = (int)json_integer_value(json_object_get(j, "sfx_pack"));
  if (m->sfx_pack < 0 || m->sfx_pack >= SFX_PACKS) m->sfx_pack = 0;
  m->boot_snd = (int)json_integer_value(json_object_get(j, "boot_sound"));
  if (m->boot_snd < 0 || m->boot_snd >= BOOT_SOUNDS) m->boot_snd = 0;
  m->hide_ip = json_is_true(json_object_get(j, "hide_ip"));
  json_t *bx = json_object_get(j, "boxes");
  m->boxes = bx ? json_is_true(bx) : 1;
  json_t *co = json_object_get(j, "covers_online");
  m->covers_online = co ? json_is_true(co) : 1;
  m->show_folders = json_is_true(json_object_get(j, "folders"));
  const char *vw = json_string_value(json_object_get(j, "view"));
  m->view = !vw ? VIEW_FLOW : !strcmp(vw, "grid") ? VIEW_GRID : !strcmp(vw, "classic") ? VIEW_CLASSIC : VIEW_FLOW;
  const char *th = json_string_value(json_object_get(j, "theme"));
  if (th) copy(m->theme, th, sizeof m->theme);
  json_decref(j);
  return 0;
}

static json_t *save_nodes(const Node *parent) {
  json_t *arr = json_array();
  for (int i = 0; i < parent->count; i++) {
    const Node *n = &parent->items[i];
    if (n->is_folder) {
      char col[8];
      snprintf(col, sizeof col, "#%06x", n->color & 0xffffff);
      json_array_append_new(arr, json_pack("{s:s, s:s, s:o}", "folder", n->name, "color", col,
                                           "items", save_nodes(n)));
    } else {
      json_array_append_new(arr, json_pack("{s:s}", "app", n->ref));
    }
  }
  return arr;
}

int menu_save(Menu *m, const char *path) {
  json_t *j = json_object();
  json_object_set_new(j, "version", json_integer(1));
  json_object_set_new(j, "theme", json_string(m->theme));
  json_object_set_new(j, "folders", json_boolean(m->show_folders));
  static const char *VIEWS[VIEW_COUNT] = { "flow", "grid", "classic" };
  json_object_set_new(j, "view", json_string(VIEWS[m->view >= 0 && m->view < VIEW_COUNT ? m->view : 0]));
  json_object_set_new(j, "root", save_nodes(&m->root));

  json_t *hidden = json_array();
  for (int i = 0; i < m->nhidden; i++) json_array_append_new(hidden, json_string(m->hidden[i]));
  json_object_set_new(j, "hidden", hidden);

  json_t *names = json_object();
  for (int i = 0; i < m->nnames; i++) json_object_set_new(names, m->names[i].id, json_string(m->names[i].name));
  json_object_set_new(j, "names", names);

  json_t *stats = json_object();
  for (int i = 0; i < m->nstats; i++)
    json_object_set_new(stats, m->stats[i].id,
                        json_pack("{s:i, s:I, s:I}", "n", m->stats[i].launches, "last", (json_int_t)m->stats[i].last,
                                  "t", (json_int_t)m->stats[i].playtime));
  json_object_set_new(j, "stats", stats);

  json_t *favs = json_array();
  for (int i = 0; i < m->nfavs; i++) json_array_append_new(favs, json_string(m->favs[i]));
  json_object_set_new(j, "favorites", favs);
  json_object_set_new(j, "saver", json_integer(m->saver));
  json_object_set_new(j, "sfx_pack", json_integer(m->sfx_pack));
  json_object_set_new(j, "boot_sound", json_integer(m->boot_snd));
  json_object_set_new(j, "hide_ip", json_boolean(m->hide_ip));
  json_object_set_new(j, "boxes", json_boolean(m->boxes));
  json_object_set_new(j, "covers_online", json_boolean(m->covers_online));

  /* ecriture sure : fichier temporaire puis renommage */
  char tmp[PATH_LEN + 8];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  int rc = json_dump_file(j, tmp, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
  json_decref(j);
  if (rc) return -1;
  return rename(tmp, path);
}
