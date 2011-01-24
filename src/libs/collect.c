/*
    This file is part of darktable,
    copyright (c) 2009--2010 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "common/darktable.h"
#include "common/film.h"
#include "common/collection.h"
#include "common/debug.h"
#include "control/conf.h"
#include "control/control.h"
#include "gui/gtk.h"
#include "dtgtk/button.h"
#include "libs/lib.h"
#include "common/metadata.h"
#include "common/utility.h"
#include <assert.h>

DT_MODULE(1)

#define MAX_RULES 10

typedef enum dt_lib_collect_mode_t
{
  DT_LIB_COLLECT_MODE_AND=0,
  DT_LIB_COLLECT_MODE_OR,
  DT_LIB_COLLECT_MODE_AND_NOT
}
dt_lib_collect_mode_t;

typedef struct dt_lib_collect_rule_t
{
  long int num;
  GtkWidget *hbox;
  GtkComboBox *combo;
  GtkWidget *text;
  GtkWidget *button;
}
dt_lib_collect_rule_t;

typedef struct dt_lib_collect_t
{
  dt_lib_collect_rule_t rule[MAX_RULES];
  int active_rule;
  GtkTreeView *view;
  GtkScrolledWindow *scrolledwindow;
}
dt_lib_collect_t;

typedef enum dt_lib_collect_cols_t
{
  DT_LIB_COLLECT_COL_TEXT=0,
  DT_LIB_COLLECT_COL_ID,
  DT_LIB_COLLECT_NUM_COLS
}
dt_lib_collect_cols_t;

const char*
name ()
{
  return _("collect images");
}

uint32_t views() 
{
  return DT_LIGHTTABLE_VIEW;
}

static dt_lib_collect_t*
get_collect (dt_lib_collect_rule_t *r)
{
  dt_lib_collect_t *d = (dt_lib_collect_t *)(((char *)r) - r->num*sizeof(dt_lib_collect_rule_t));
  return d;
}

static gboolean
entry_key_press (GtkEntry *entry, GdkEventKey *event, dt_lib_collect_rule_t *dr)
{ // update related list
  dt_lib_collect_t *d = get_collect(dr);
  sqlite3_stmt *stmt;
  GtkTreeIter iter;
  GtkTreeView *view = d->view;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
  g_object_ref(model);
  gtk_tree_view_set_model(GTK_TREE_VIEW(view), NULL);
  gtk_list_store_clear(GTK_LIST_STORE(model));
  char query[1024];
  int property = gtk_combo_box_get_active(dr->combo);
  const gchar *text = gtk_entry_get_text(GTK_ENTRY(dr->text));
  gchar *escaped_text = dt_util_str_replace(text, "'", "''");
  char confname[200];
  snprintf(confname, 200, "plugins/lighttable/collect/string%1ld", dr->num);
  dt_conf_set_string (confname, text);
  snprintf(confname, 200, "plugins/lighttable/collect/item%1ld", dr->num);
  dt_conf_set_int (confname, property);
  
  switch(property)
  {
    case 0: // film roll
      snprintf(query, 1024, "select distinct folder, id from film_rolls where folder like '%%%s%%'", escaped_text);
      break;
    case 1: // camera
      snprintf(query, 1024, "select distinct maker || ' ' || model, 1 from images where maker || ' ' || model like '%%%s%%'", escaped_text);
      break;
    case 2: // tag
      snprintf(query, 1024, "select distinct name, id from tags where name like '%%%s%%'", escaped_text);
      break;
    case 4: // History, 2 hardcoded alternatives
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
        DT_LIB_COLLECT_COL_TEXT,_("altered"),
        DT_LIB_COLLECT_COL_ID, 0,
        -1);
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
        DT_LIB_COLLECT_COL_TEXT,_("not altered"),
        DT_LIB_COLLECT_COL_ID, 1,
        -1);
      goto entry_key_press_exit;
    break;
    
    case 5: // colorlabels
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
        DT_LIB_COLLECT_COL_TEXT,_("red"),
        DT_LIB_COLLECT_COL_ID, 0,
        -1);
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
        DT_LIB_COLLECT_COL_TEXT,_("yellow"),
        DT_LIB_COLLECT_COL_ID, 1,
        -1);
     gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
        DT_LIB_COLLECT_COL_TEXT,_("green"),
        DT_LIB_COLLECT_COL_ID, 2,
        -1);
     gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
        DT_LIB_COLLECT_COL_TEXT,_("blue"),
        DT_LIB_COLLECT_COL_ID, 3,
        -1);
     gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
        DT_LIB_COLLECT_COL_TEXT,_("purple"),
        DT_LIB_COLLECT_COL_ID, 4,
        -1);
      goto entry_key_press_exit;
    break;
    
    // TODO: Add empty string for metadata?
    // TODO: Autogenerate this code?
    case 6: // title
        snprintf(query, 1024, "select distinct value, 1 from meta_data where key = %d and value like '%%%s%%'",
        DT_METADATA_XMP_DC_TITLE, escaped_text);
        break;
    case 7: // description
        snprintf(query, 1024, "select distinct value, 1 from meta_data where key = %d and value like '%%%s%%'",
        DT_METADATA_XMP_DC_DESCRIPTION, escaped_text);
        break;
    case 8: // creator
        snprintf(query, 1024, "select distinct value, 1 from meta_data where key = %d and value like '%%%s%%'",
        DT_METADATA_XMP_DC_CREATOR, escaped_text);
        break;
    case 9: // publisher
        snprintf(query, 1024, "select distinct value, 1 from meta_data where key = %d and value like '%%%s%%'",
        DT_METADATA_XMP_DC_PUBLISHER, escaped_text);
        break;
    case 10: // rights
        snprintf(query, 1024, "select distinct value, 1 from meta_data where key = %d and value like '%%%s%%'",
        DT_METADATA_XMP_DC_RIGHTS, escaped_text);
        break;

    default: // case 3: // day
      snprintf(query, 1024, "select distinct datetime_taken, 1 from images where datetime_taken like '%%%s%%'", escaped_text);
      break;
  }
  g_free(escaped_text);
  DT_DEBUG_SQLITE3_PREPARE_V2(darktable.db, query, -1, &stmt, NULL);
  while(sqlite3_step(stmt) == SQLITE_ROW)
  {
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    const char *folder = (const char*)sqlite3_column_text(stmt, 0);
    if(property == 0) // film roll
    {
      folder = dt_image_film_roll_name(folder);
    }
    gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                        DT_LIB_COLLECT_COL_TEXT, folder,
                        DT_LIB_COLLECT_COL_ID, sqlite3_column_int(stmt, 1),
                        -1);
  }
  sqlite3_finalize(stmt);
entry_key_press_exit:
  gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
  g_object_unref(model);
  return FALSE;
}

static void
gui_update (dt_lib_collect_t *d)
{
  const int old = darktable.gui->reset;
  darktable.gui->reset = 1;
  const int active = CLAMP(dt_conf_get_int("plugins/lighttable/collect/num_rules") - 1, 0, 9);
  char confname[200];
  for(int i=0;i<MAX_RULES;i++)
  {
    gtk_widget_set_no_show_all(d->rule[i].hbox, TRUE);
    gtk_widget_set_visible(d->rule[i].hbox, FALSE);
  }
  for(int i=0;i<=active;i++)
  {
    gtk_widget_set_no_show_all(d->rule[i].hbox, FALSE);
    gtk_widget_set_visible(d->rule[i].hbox, TRUE);
    gtk_widget_show_all(d->rule[i].hbox);
    snprintf(confname, 200, "plugins/lighttable/collect/item%1d", i);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->rule[i].combo), dt_conf_get_int(confname));
    snprintf(confname, 200, "plugins/lighttable/collect/string%1d", i);
    gchar *text = dt_conf_get_string(confname);
    if(text)
    {
      gtk_entry_set_text(GTK_ENTRY(d->rule[i].text), text);
      g_free(text);
    }

    GtkDarktableButton *button = DTGTK_BUTTON(d->rule[i].button);
    if(i == MAX_RULES - 1)
    {
      // only clear
      button->icon = dtgtk_cairo_paint_cancel;
      gtk_object_set(GTK_OBJECT(button), "tooltip-text", _("clear this rule"), (char *)NULL);
    }
    else if(i == active)
    {
      button->icon = dtgtk_cairo_paint_dropdown; 
      gtk_object_set(GTK_OBJECT(button), "tooltip-text", _("clear this rule or add new rules"), (char *)NULL);
    }
    else
    {
      snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", i+1);
      const int mode = dt_conf_get_int(confname);
      if(mode == DT_LIB_COLLECT_MODE_AND)     button->icon = dtgtk_cairo_paint_and;
      if(mode == DT_LIB_COLLECT_MODE_OR)      button->icon = dtgtk_cairo_paint_or;
      if(mode == DT_LIB_COLLECT_MODE_AND_NOT) button->icon = dtgtk_cairo_paint_andnot;
      gtk_object_set(GTK_OBJECT(button), "tooltip-text", _("clear this rule"), (char *)NULL);
    }
  }
  // update list of proposals
  entry_key_press (NULL, NULL, d->rule + d->active_rule);
  darktable.gui->reset = old;
}

void
gui_reset (dt_lib_module_t *self)
{
  dt_conf_set_int("plugins/lighttable/collect/num_rules", 1);
  dt_conf_set_int("plugins/lighttable/collect/item0", 0);
  dt_conf_set_string("plugins/lighttable/collect/string0", "");
  dt_lib_collect_t *d = (dt_lib_collect_t *)self->data;
  gui_update(d);
  // entry_key_press (NULL, NULL, d->rule + d->active_rule);
}

static void
combo_changed (GtkComboBox *combo, dt_lib_collect_rule_t *d)
{
  if(darktable.gui->reset) return;
  gtk_entry_set_text(GTK_ENTRY(d->text), "");
  dt_lib_collect_t *c = get_collect(d);
  c->active_rule = d->num;
  entry_key_press (NULL, NULL, d);
  dt_collection_update_query(darktable.collection);
}

static void
row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, dt_lib_collect_t *d)
{
  GtkTreeIter iter;
  GtkTreeModel *model = NULL;
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  if(!gtk_tree_selection_get_selected(selection, &model, &iter)) return;
  gchar *text;
  gtk_tree_model_get (model, &iter, 
                      DT_LIB_COLLECT_COL_TEXT, &text,
                      -1);
  const int active = d->active_rule;
  gtk_entry_set_text(GTK_ENTRY(d->rule[active].text), text);
  g_free(text);
  entry_key_press (NULL, NULL, d->rule + active);
  dt_collection_update_query(darktable.collection);
}

static void
entry_activated (GtkWidget *entry, dt_lib_collect_rule_t *d)
{
  entry_key_press (NULL, NULL, d);
  dt_collection_update_query(darktable.collection);
}

int
position ()
{
  return 400;
}

static void
entry_focus_in_callback (GtkWidget *w, GdkEventFocus *event, dt_lib_collect_rule_t *d)
{
  dt_lib_collect_t *c = get_collect(d);
  c->active_rule = d->num;
  entry_key_press (NULL, NULL, c->rule + c->active_rule);
}

#if 0
static void
focus_in_callback (GtkWidget *w, GdkEventFocus *event, dt_lib_module_t *self)
{
  GtkWidget *win = glade_xml_get_widget (darktable.gui->main_window, "main_window");
  GtkEntry *entry = GTK_ENTRY(self->text);
  GtkTreeView *view;
  int count = 1 + count_film_rolls(gtk_entry_get_text(entry));
  int ht = get_font_height(view, "Dreggn");
  const int size = MAX(2*ht, MIN(win->allocation.height/2, count*ht));
  gtk_widget_set_size_request(view, -1, size);
}

static void
hide_callback (GObject    *object,
                   GParamSpec *param_spec,
                   GtkWidget *view)
{
  GtkExpander *expander;
  expander = GTK_EXPANDER (object);
  if (!gtk_expander_get_expanded (expander))
    gtk_widget_set_size_request(view, -1, -1);
}
#endif

static void
menuitem_and (GtkMenuItem *menuitem, dt_lib_collect_rule_t *d)
{
  // add next row with and operator
  const int active = CLAMP(dt_conf_get_int("plugins/lighttable/collect/num_rules"), 1, 10);
  if(active < 10)
  {
    char confname[200];
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", active);
    dt_conf_set_int(confname, DT_LIB_COLLECT_MODE_AND);
    snprintf(confname, 200, "plugins/lighttable/collect/string%1d", active);
    dt_conf_set_string(confname, "");
    dt_conf_set_int("plugins/lighttable/collect/num_rules", active+1);
    dt_lib_collect_t *c = get_collect(d);
    c->active_rule = active;
  }
  gui_update(get_collect(d));
  dt_collection_update_query(darktable.collection);
}

static void
menuitem_or (GtkMenuItem *menuitem, dt_lib_collect_rule_t *d)
{
  // add next row with or operator
  const int active = CLAMP(dt_conf_get_int("plugins/lighttable/collect/num_rules"), 1, 10);
  if(active < 10)
  {
    char confname[200];
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", active);
    dt_conf_set_int(confname, DT_LIB_COLLECT_MODE_OR);
    snprintf(confname, 200, "plugins/lighttable/collect/string%1d", active);
    dt_conf_set_string(confname, "");
    dt_conf_set_int("plugins/lighttable/collect/num_rules", active+1);
  }
  gui_update(get_collect(d));
  dt_collection_update_query(darktable.collection);
}

static void
menuitem_and_not (GtkMenuItem *menuitem, dt_lib_collect_rule_t *d)
{
  // add next row with and not operator
  const int active = CLAMP(dt_conf_get_int("plugins/lighttable/collect/num_rules"), 1, 10);
  if(active < 10)
  {
    char confname[200];
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", active);
    dt_conf_set_int(confname, DT_LIB_COLLECT_MODE_AND_NOT);
    snprintf(confname, 200, "plugins/lighttable/collect/string%1d", active);
    dt_conf_set_string(confname, "");
    dt_conf_set_int("plugins/lighttable/collect/num_rules", active+1);
  }
  gui_update(get_collect(d));
  dt_collection_update_query(darktable.collection);
}

static void
menuitem_change_and (GtkMenuItem *menuitem, dt_lib_collect_rule_t *d)
{
  // add next row with and operator
  const int num = d->num + 1;
  if(num < 10 && num > 0)
  {
    char confname[200];
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", num);
    dt_conf_set_int(confname, DT_LIB_COLLECT_MODE_AND);
  }
  gui_update(get_collect(d));
  dt_collection_update_query(darktable.collection);
}

static void
menuitem_change_or (GtkMenuItem *menuitem, dt_lib_collect_rule_t *d)
{
  // add next row with or operator
  const int num = d->num + 1;
  if(num < 10 && num > 0)
  {
    char confname[200];
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", num);
    dt_conf_set_int(confname, DT_LIB_COLLECT_MODE_OR);
  }
  gui_update(get_collect(d));
  dt_collection_update_query(darktable.collection);
}

static void
menuitem_change_and_not (GtkMenuItem *menuitem, dt_lib_collect_rule_t *d)
{
  // add next row with and not operator
  const int num = d->num + 1;
  if(num < 10 && num > 0)
  {
    char confname[200];
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", num);
    dt_conf_set_int(confname, DT_LIB_COLLECT_MODE_AND_NOT);
  }
  gui_update(get_collect(d));
  dt_collection_update_query(darktable.collection);
}

static void
collection_updated(void *d)
{
  gui_update((dt_lib_collect_t *)d);
}


static void
menuitem_clear (GtkMenuItem *menuitem, dt_lib_collect_rule_t *d)
{
  // remove this row, or if 1st, clear text entry box
  const int active = CLAMP(dt_conf_get_int("plugins/lighttable/collect/num_rules"), 1, 10);
  if(active > 1)
  {
    dt_conf_set_int("plugins/lighttable/collect/num_rules", active-1);
  }
  else
  {
    dt_conf_set_int("plugins/lighttable/collect/mode0", DT_LIB_COLLECT_MODE_AND);
    dt_conf_set_int("plugins/lighttable/collect/item0", 0);
    dt_conf_set_string("plugins/lighttable/collect/string0", "");
  }
  // move up all still active rules by one.
  for(int i=d->num;i<MAX_RULES-1;i++)
  {
    char confname[200];
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", i+1);
    const int mode = dt_conf_get_int(confname);
    snprintf(confname, 200, "plugins/lighttable/collect/item%1d", i+1);
    const int item = dt_conf_get_int(confname);
    snprintf(confname, 200, "plugins/lighttable/collect/string%1d", i+1);
    gchar *string = dt_conf_get_string(confname);
    if(string)
    {
      snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", i);
      dt_conf_set_int(confname, mode);
      snprintf(confname, 200, "plugins/lighttable/collect/item%1d", i);
      dt_conf_set_int(confname, item);
      snprintf(confname, 200, "plugins/lighttable/collect/string%1d", i);
      dt_conf_set_string(confname, string);
      g_free(string);
    }
  }
  gui_update(get_collect(d));
  dt_collection_update_query(darktable.collection);
}

static gboolean
popup_button_callback(GtkWidget *widget, GdkEventButton *event, dt_lib_collect_rule_t *d)
{
  if(event->button != 1) return FALSE;

  GtkWidget *menu = gtk_menu_new();
  GtkWidget *mi;
  const int active = CLAMP(dt_conf_get_int("plugins/lighttable/collect/num_rules"), 1, 10);

  mi = gtk_menu_item_new_with_label("clear this rule");
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
  g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menuitem_clear), d);
  
  if(d->num == active - 1)
  {
    mi = gtk_menu_item_new_with_label("and new rule");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menuitem_and), d);

    mi = gtk_menu_item_new_with_label("or new rule");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menuitem_or), d);

    mi = gtk_menu_item_new_with_label("and not new rule");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menuitem_and_not), d);
  }
  else if(d->num < active - 1)
  {
    mi = gtk_menu_item_new_with_label("change to and");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menuitem_change_and), d);

    mi = gtk_menu_item_new_with_label("change to or");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menuitem_change_or), d);

    mi = gtk_menu_item_new_with_label("change to and not");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menuitem_change_and_not), d);
  }

  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
  gtk_widget_show_all(menu);

  return TRUE;
}

void
gui_init (dt_lib_module_t *self)
{
  dt_lib_collect_t *d = (dt_lib_collect_t *)malloc(sizeof(dt_lib_collect_t));
  dt_collection_listener_register(collection_updated, d);
  self->data = (void *)d;
  self->widget = gtk_vbox_new(FALSE, 5);
  gtk_widget_set_size_request(self->widget, 100, -1);
  d->active_rule = 0;

  GtkBox *box;
  GtkWidget *w;
  GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
  GtkTreeView *view = GTK_TREE_VIEW(gtk_tree_view_new());
  d->view = view;
  GtkListStore *liststore;

  for(int i=0;i<MAX_RULES;i++)
  {
    d->rule[i].num = i;
    box = GTK_BOX(gtk_hbox_new(FALSE, 5));
    d->rule[i].hbox = GTK_WIDGET(box);
    gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(box), TRUE, TRUE, 0);
    w = gtk_combo_box_new_text();
    d->rule[i].combo = GTK_COMBO_BOX(w);
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("film roll"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("camera"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("tag"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("date"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("history"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("colorlabel"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("title"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("description"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("creator"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("publisher"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(w), _("rights"));
    g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(combo_changed), d->rule + i);
    gtk_box_pack_start(box, w, FALSE, FALSE, 0);
    w = gtk_entry_new();
    dt_gui_key_accel_block_on_focus(w);
    d->rule[i].text = w;
    gtk_widget_add_events(w, GDK_FOCUS_CHANGE_MASK);
    g_signal_connect(G_OBJECT(w), "focus-in-event", G_CALLBACK(entry_focus_in_callback), d->rule + i);

    /* xgettext:no-c-format */
    gtk_object_set(GTK_OBJECT(w), "tooltip-text", _("type your query, use `%' as wildcard"), (char *)NULL);
    gtk_widget_add_events(w, GDK_KEY_RELEASE_MASK);
    g_signal_connect(G_OBJECT(w), "key-release-event", G_CALLBACK(entry_key_press), d->rule + i);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(entry_activated), d->rule + i);
    gtk_box_pack_start(box, w, TRUE, TRUE, 0);
    w = dtgtk_button_new(dtgtk_cairo_paint_presets, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER);
    d->rule[i].button = w;
    gtk_widget_set_events(w, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(w), "button-press-event", G_CALLBACK(popup_button_callback), d->rule + i);
    gtk_box_pack_start(box, w, FALSE, FALSE, 0);
    gtk_widget_set_size_request(w, 13, 13);
  }

  d->scrolledwindow = GTK_SCROLLED_WINDOW(sw);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(view));
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(sw), TRUE, TRUE, 0);
  gtk_tree_view_set_headers_visible(view, FALSE);
  liststore = gtk_list_store_new(DT_LIB_COLLECT_NUM_COLS, G_TYPE_STRING, G_TYPE_UINT);
  GtkTreeViewColumn *col = gtk_tree_view_column_new();
  gtk_tree_view_append_column(view, col);
  gtk_widget_set_size_request(GTK_WIDGET(view), -1, 300);
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer, "text", DT_LIB_COLLECT_COL_TEXT);
  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view), GTK_SELECTION_SINGLE);
  gtk_tree_view_set_model(view, GTK_TREE_MODEL(liststore));
  gtk_object_set(GTK_OBJECT(view), "tooltip-text", _("doubleclick to select"), (char *)NULL);
  g_signal_connect(G_OBJECT (view), "row-activated", G_CALLBACK (row_activated), d);

  gui_update(d);
}

void
gui_cleanup (dt_lib_module_t *self)
{
  dt_collection_listener_unregister(collection_updated);
  free(self->data);
  self->data = NULL;
}

#undef MAX_RULES
