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
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gdk/gdkkeysyms.h>
#include <pthread.h>

#include "common/darktable.h"
#include "develop/develop.h"
#include "develop/imageop.h"
#include "gui/gtk.h"
#include "gui/metadata.h"
#include "gui/filmview.h"
#include "control/control.h"
#include "control/jobs.h"
#include "control/conf.h"
#include "views/view.h"


static gboolean
borders_button_pressed (GtkWidget *w, GdkEventButton *event, gpointer user_data)
{
  GtkWidget *widget;
  long int which = (long int)user_data;
  int32_t bit = 0;
  int mode = dt_conf_get_int("ui_last/view");
  switch(which)
  {
    case 0:
      bit = dt_conf_get_int("ui_last/panel_left");
      widget = glade_xml_get_widget (darktable.gui->main_window, "left");
      break;
    case 1:
      bit = dt_conf_get_int("ui_last/panel_right");
      widget = glade_xml_get_widget (darktable.gui->main_window, "right");
      break;
    case 2:
      bit = dt_conf_get_int("ui_last/panel_top");
      widget = glade_xml_get_widget (darktable.gui->main_window, "top");
      break;
    default:
      bit = dt_conf_get_int("ui_last/panel_bottom");
      widget = glade_xml_get_widget (darktable.gui->main_window, "bottom");
      break;
  }

  if(GTK_WIDGET_VISIBLE(widget))
  {
    gtk_widget_hide(widget);
    bit &= ~(1<<mode);
  }
  else
  {
    gtk_widget_show(widget);
    bit |=   1<<mode;
  }

  switch(which)
  {
    case 0:
      dt_conf_set_int("ui_last/panel_left", bit);
      break;
    case 1:
      dt_conf_set_int("ui_last/panel_right", bit);
      break;
    case 2:
      dt_conf_set_int("ui_last/panel_top", bit);
      break;
    default:
      dt_conf_set_int("ui_last/panel_bottom", bit);
      break;
  }
  gtk_widget_queue_draw(w);
  return TRUE;
}

static gboolean
expose_borders (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{ // draw arrows on borders
  if(!darktable.control->running) return TRUE;
  long int which = (long int)user_data;
  float width = widget->allocation.width, height = widget->allocation.height;
  cairo_surface_t *cst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cairo_t *cr = cairo_create(cst);
  cairo_set_source_rgb (cr, .13, .13, .13);
  cairo_paint(cr);

  // draw scrollbar indicators
  int v = darktable.view_manager->current_view;
  dt_view_t *view = NULL;
  if(v >= 0 && v < darktable.view_manager->num_views) view = darktable.view_manager->view + v;
  cairo_set_source_rgb (cr, .16, .16, .16);
  if(!view) cairo_paint(cr);
  else
  {
    switch(which)
    {
      case 0: case 1: // left, right: vertical
        cairo_rectangle(cr, 0.0, view->vscroll_pos/view->vscroll_size * height, width, view->vscroll_viewport_size/view->vscroll_size * height);
        break;
      default:        // bottom, top: horizontal
        cairo_rectangle(cr, view->hscroll_pos/view->hscroll_size * width, 0.0, view->hscroll_viewport_size/view->hscroll_size * width, height);
        break;
    }
    cairo_fill_preserve(cr);
    cairo_set_source_rgb (cr, .1, .1, .1);
    cairo_stroke(cr);
  }

  // draw gui arrows.
  cairo_set_source_rgb (cr, .6, .6, .6);

  int32_t mask = 1<<dt_conf_get_int("ui_last/view");
  switch(which)
  {
    case 0: // left
      if(dt_conf_get_int("ui_last/panel_left") & mask)
      {
        cairo_move_to (cr, width, height/2-width);
        cairo_rel_line_to (cr, 0.0, 2*width);
        cairo_rel_line_to (cr, -width, -width);
      }
      else
      {
        cairo_move_to (cr, 0.0, height/2-width);
        cairo_rel_line_to (cr, 0.0, 2*width);
        cairo_rel_line_to (cr, width, -width);
      }
      break;
    case 1: // right
      if(dt_conf_get_int("ui_last/panel_right") & mask)
      {
        cairo_move_to (cr, 0.0, height/2-width);
        cairo_rel_line_to (cr, 0.0, 2*width);
        cairo_rel_line_to (cr, width, -width);
      }
      else
      {
        cairo_move_to (cr, width, height/2-width);
        cairo_rel_line_to (cr, 0.0, 2*width);
        cairo_rel_line_to (cr, -width, -width);
      }
      break;
    case 2: // top
      if(dt_conf_get_int("ui_last/panel_top") & mask)
      {
        cairo_move_to (cr, width/2-height, height);
        cairo_rel_line_to (cr, 2*height, 0.0);
        cairo_rel_line_to (cr, -height, -height);
      }
      else
      {
        cairo_move_to (cr, width/2-height, 0.0);
        cairo_rel_line_to (cr, 2*height, 0.0);
        cairo_rel_line_to (cr, -height, height);
      }
      break;
    default: // bottom
      if(dt_conf_get_int("ui_last/panel_bottom") & mask)
      {
        cairo_move_to (cr, width/2-height, 0.0);
        cairo_rel_line_to (cr, 2*height, 0.0);
        cairo_rel_line_to (cr, -height, height);
      }
      else
      {
        cairo_move_to (cr, width/2-height, height);
        cairo_rel_line_to (cr, 2*height, 0.0);
        cairo_rel_line_to (cr, -height, -height);
      }
      break;
  }
  cairo_close_path (cr);
  cairo_fill(cr);

  cairo_destroy(cr);
  cairo_t *cr_pixmap = gdk_cairo_create(gtk_widget_get_window(widget));
  cairo_set_source_surface (cr_pixmap, cst, 0, 0);
  cairo_paint(cr_pixmap);
  cairo_destroy(cr_pixmap);
  cairo_surface_destroy(cst);
  return TRUE;
}

static dt_iop_module_t *get_colorout_module()
{
  GList *modules = darktable.develop->iop;
  while(modules)
  {
    dt_iop_module_t *module = (dt_iop_module_t *)modules->data;
    if(!strcmp(module->op, "colorout")) return module;
    modules = g_list_next(modules);
  }
  return NULL;
}

static void
update_colorpicker_panel()
{
  // synch bottom panel for develop mode
  dt_iop_module_t *module = get_colorout_module();
  if(module)
  {
    char colstring[512];
    GtkWidget *w;
    w = glade_xml_get_widget (darktable.gui->main_window, "colorpicker_module_label");
    snprintf(colstring, 512, C_("color picker module", "`%s'"), module->name());
    gtk_label_set_label(GTK_LABEL(w), colstring);
    w = glade_xml_get_widget (darktable.gui->main_window, "colorpicker_togglebutton");
    darktable.gui->reset = 1;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), module->request_color_pick);
    darktable.gui->reset = 0;

    // int input_color = 1; // Lab
    // always adjust picked color:
    int m = dt_conf_get_int("ui_last/colorpicker_mean");
    float *col;
    switch(m)
    {
      case 0: // mean
        //if(input_color == 1)
        col = module->picked_color_Lab;
        //else col = module->picked_color;
        break;
      case 1: //min
        //if(input_color == 1)
        col = module->picked_color_min_Lab;
        //else col = module->picked_color_min;
        break;
      default:
        //if(input_color == 1)
        col = module->picked_color_max_Lab;
        //else col = module->picked_color_max;
        break;
    }
    w = glade_xml_get_widget (darktable.gui->main_window, "colorpicker_Lab_label");
    // switch(input_color)
    // {
    //   case 0: // linear rgb
    //     snprintf(colstring, 512, "%s: (%.03f, %.03f, %.03f)", _("linear rgb"), col[0], col[1], col[2]);
    //     break;
    //   case 1: // Lab
        snprintf(colstring, 512, "%s: (%.02f, %.02f, %.02f)", _("Lab"), col[0], col[1], col[2]);
    //     break;
    //   default: // output color profile
    //     snprintf(colstring, 512, "%s: (%.03f, %.03f, %.03f)", _("output profile"), col[0], col[1], col[2]);
    //     break;
    // }
    gtk_label_set_label(GTK_LABEL(w), colstring);
  }
}

static gboolean
expose (GtkWidget *da, GdkEventExpose *event, gpointer user_data)
{
  dt_control_expose(NULL);
  gdk_draw_drawable(da->window,
      da->style->fg_gc[GTK_WIDGET_STATE(da)], darktable.gui->pixmap,
      // Only copy the area that was exposed.
      event->area.x, event->area.y,
      event->area.x, event->area.y,
      event->area.width, event->area.height);

  // update other widgets
  GtkWidget *widget = glade_xml_get_widget (darktable.gui->main_window, "navigation");
  gtk_widget_queue_draw(widget);

  GList *wdl = darktable.gui->redraw_widgets;
  while(wdl)
  {
    GtkWidget *w = (GtkWidget *)wdl->data;
    gtk_widget_queue_draw(w);
    wdl = g_list_next(wdl);
  }

  update_colorpicker_panel();

  // test quit cond (thread safe, 2nd pass)
  if(!darktable.control->running)
  {
    dt_cleanup();
    gtk_main_quit();
  }
  else
  {
    widget = glade_xml_get_widget (darktable.gui->main_window, "metadata_expander");
    if(gtk_expander_get_expanded(GTK_EXPANDER(widget))) dt_gui_metadata_update();
  }

	return TRUE;
}

gboolean
view_label_clicked (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  if(event->type == GDK_2BUTTON_PRESS && event->button == 1)
  {
    dt_ctl_switch_mode();
    return TRUE;
  }
  return FALSE;
}

static void
colorpicker_mean_changed (GtkComboBox *widget, gpointer p)
{
  dt_conf_set_int("ui_last/colorpicker_mean", gtk_combo_box_get_active(widget));
  update_colorpicker_panel();
}

static void
colorpicker_toggled (GtkToggleButton *button, gpointer p)
{
  if(darktable.gui->reset) return;
  dt_iop_module_t *module = get_colorout_module();
  if(module)
  {
    darktable.develop->gui_module = module;
    module->request_color_pick = gtk_toggle_button_get_active(button);
  }
  else
  {
    darktable.develop->gui_module = NULL;
  }
  dt_control_gui_queue_draw();
}

static void
lighttable_zoom_changed (GtkSpinButton *widget, gpointer user_data)
{
  const int i = gtk_spin_button_get_value(widget);
  dt_conf_set_int("plugins/lighttable/images_in_row", i);
  dt_control_gui_queue_draw();
}

static void
lighttable_layout_changed (GtkComboBox *widget, gpointer user_data)
{
  const int i = gtk_combo_box_get_active(widget);
  dt_conf_set_int("plugins/lighttable/layout", i);
  dt_control_gui_queue_draw();
}

static void
update_query()
{
  const int i = dt_conf_get_int("ui_last/combo_sort");
  const int j = dt_conf_get_int("ui_last/combo_filter");
  // replace sort part
  char *sortstring[5] = {"datetime_taken, filename", "flags & 7 desc", "filename", "id", "color, filename"};
  int sortindex = 3;
  if     (i == 1) sortindex = 0;
  else if(i == 2) sortindex = 1;
  else if(i == 0) sortindex = 2;
  else if(i == 4) sortindex = 4;
  // else (i == 3)
  gchar *query = dt_conf_get_string("plugins/lighttable/query");
  if (query == NULL)
    return;
  gchar *q = query;
  if(!strncmp(query, "select * from (", 15)) q = query + 15;
  gchar **split = g_regex_split_simple("flags & 7", q, 0, 0);
  char newquery[1024], filter[512];
  if(j == 1) snprintf(filter, 512, "flags & 7) < 1");
  else       snprintf(filter, 512, "flags & 7) >= %d", j-1);
  if(i == 4)
    snprintf(newquery, 1024, "select * from (%s %s) as a join color_labels as b on a.id = b.imgid order by %s limit ?1, ?2", split[0], filter, sortstring[sortindex]);
  else
    snprintf(newquery, 1024, "%s %s order by %s limit ?1, ?2", split[0], filter, sortstring[sortindex]);
  g_strfreev(split);
  g_free(query);
  dt_conf_set_string("plugins/lighttable/query", newquery);
  GtkWidget *win = glade_xml_get_widget (darktable.gui->main_window, "center");
  gtk_widget_queue_draw(win);
}

static void
image_filter_changed (GtkComboBox *widget, gpointer user_data)
{
  // image_filter
  int i = gtk_combo_box_get_active(widget);
  if     (i == 0)  dt_conf_set_int("ui_last/combo_filter",     DT_LIB_FILTER_ALL);
  else if(i == 1)  dt_conf_set_int("ui_last/combo_filter",     DT_LIB_FILTER_STAR_NO);
  else if(i == 2)  dt_conf_set_int("ui_last/combo_filter",     DT_LIB_FILTER_STAR_1);
  else if(i == 3)  dt_conf_set_int("ui_last/combo_filter",     DT_LIB_FILTER_STAR_2);
  else if(i == 4)  dt_conf_set_int("ui_last/combo_filter",     DT_LIB_FILTER_STAR_3);
  else if(i == 5)  dt_conf_set_int("ui_last/combo_filter",     DT_LIB_FILTER_STAR_4);

  update_query();
}


static void
image_sort_changed (GtkComboBox *widget, gpointer user_data)
{
  // image_sort
  int i = gtk_combo_box_get_active(widget);
  if     (i == 0)  dt_conf_set_int("ui_last/combo_sort",     DT_LIB_SORT_FILENAME);
  else if(i == 1)  dt_conf_set_int("ui_last/combo_sort",     DT_LIB_SORT_DATETIME);
  else if(i == 2)  dt_conf_set_int("ui_last/combo_sort",     DT_LIB_SORT_RATING);
  else if(i == 3)  dt_conf_set_int("ui_last/combo_sort",     DT_LIB_SORT_ID);
  else if(i == 4)  dt_conf_set_int("ui_last/combo_sort",     DT_LIB_SORT_COLOR);

  update_query();
}


static void
snapshot_add_button_clicked (GtkWidget *widget, gpointer user_data)
{
  if(!darktable.develop->image) return;
  char wdname[64], oldfilename[30];
  GtkWidget *wid;
  snprintf(wdname, 64, "snapshot_1_togglebutton");
  wid = glade_xml_get_widget (darktable.gui->main_window, wdname);
  gchar *label1 = g_strdup(gtk_button_get_label(GTK_BUTTON(wid)));
  snprintf(oldfilename, 30, "%s", darktable.gui->snapshot[3].filename);
  for(int k=1;k<4;k++)
  {
    snprintf(wdname, 64, "snapshot_%d_togglebutton", k+1);
    wid = glade_xml_get_widget (darktable.gui->main_window, wdname);
    if(k<MIN(4,darktable.gui->num_snapshots+1)) gtk_widget_set_visible(wid, TRUE);
    gchar *label2 = g_strdup(gtk_button_get_label(GTK_BUTTON(wid)));
    gtk_button_set_label(GTK_BUTTON(wid), label1);
    g_free(label1);
    label1 = label2;
    darktable.gui->snapshot[k] = darktable.gui->snapshot[k-1];
  }
  // rotate filenames, so we don't waste hd space
  snprintf(darktable.gui->snapshot[0].filename, 30, "%s", oldfilename);
  g_free(label1);
  snprintf(wdname, 64, "snapshot_%d_togglebutton", 1);
  wid = glade_xml_get_widget (darktable.gui->main_window, wdname);
  char *fname = darktable.develop->image->filename + strlen(darktable.develop->image->filename);
  while(fname > darktable.develop->image->filename && *fname != '/') fname--;
  snprintf(wdname, 64, "%s", fname);
  fname = wdname + strlen(wdname);
  while(fname > wdname && *fname != '.') fname --;
  if(*fname != '.') fname = wdname + strlen(wdname);
  if(wdname + 64 - fname > 4) sprintf(fname, "(%d)", darktable.develop->history_end);
  // snprintf(wdname, 64, _("snapshot %d"), darktable.gui->num_snapshots+1);
  gtk_button_set_label(GTK_BUTTON(wid), wdname);
  gtk_widget_set_visible(wid, TRUE);
  
  // get zoom pos from develop
  dt_gui_snapshot_t *s = darktable.gui->snapshot + 0;
  DT_CTL_GET_GLOBAL(s->zoom_y, dev_zoom_y);
  DT_CTL_GET_GLOBAL(s->zoom_x, dev_zoom_x);
  DT_CTL_GET_GLOBAL(s->zoom, dev_zoom);
  DT_CTL_GET_GLOBAL(s->closeup, dev_closeup);
  DT_CTL_GET_GLOBAL(s->zoom_scale, dev_zoom_scale);
  // set take snap bit for darkroom
  darktable.gui->request_snapshot = 1;
  darktable.gui->num_snapshots ++;
  dt_control_gui_queue_draw();
}

static void
snapshot_toggled (GtkToggleButton *widget, long int which)
{
  if(!gtk_toggle_button_get_active(widget) && darktable.gui->selected_snapshot == which)
  {
    if(darktable.gui->snapshot_image)
    {
      cairo_surface_destroy(darktable.gui->snapshot_image);
      darktable.gui->snapshot_image = NULL;
      dt_control_gui_queue_draw();
    }
  }
  else if(gtk_toggle_button_get_active(widget))
  {
    char wdname[64];
    for(int k=0;k<4;k++)
    {
      snprintf(wdname, 64, "snapshot_%d_togglebutton", k+1);
      GtkWidget *w = glade_xml_get_widget (darktable.gui->main_window, wdname);
      if(GTK_TOGGLE_BUTTON(w) != widget)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
    }
    if(darktable.gui->snapshot_image)
    {
      cairo_surface_destroy(darktable.gui->snapshot_image);
      darktable.gui->snapshot_image = NULL;
    }
    darktable.gui->selected_snapshot = which;
    dt_gui_snapshot_t *s = darktable.gui->snapshot + which;
    DT_CTL_SET_GLOBAL(dev_zoom_y,     s->zoom_y);
    DT_CTL_SET_GLOBAL(dev_zoom_x,     s->zoom_x);
    DT_CTL_SET_GLOBAL(dev_zoom,       s->zoom);
    DT_CTL_SET_GLOBAL(dev_closeup,    s->closeup);
    DT_CTL_SET_GLOBAL(dev_zoom_scale, s->zoom_scale);
    dt_dev_invalidate(darktable.develop);
    darktable.gui->snapshot_image = cairo_image_surface_create_from_png(s->filename);
    dt_control_gui_queue_draw();
  }
}

static void
film_button_clicked (GtkWidget *widget, gpointer user_data)
{
  long int num = (long int)user_data;
  (void)dt_film_open_recent(num);
  dt_ctl_switch_mode_to(DT_LIBRARY);
}

static void
history_compress_clicked (GtkWidget *widget, gpointer user_data)
{
  const int imgid = darktable.develop->image ? darktable.develop->image->id : 0;
  if(!imgid) return;
  // make sure the right history is in there:
  dt_dev_write_history(darktable.develop);
  sqlite3_stmt *stmt;
  sqlite3_exec(darktable.db, "create temp table temp_history (imgid integer, num integer, module integer, operation varchar(256), op_params blob, enabled integer)", NULL, NULL, NULL);
  // sqlite3_prepare_v2(darktable.db, "insert into temp_history select * from history as a where imgid = ?1 and enabled = 1 and num in (select MAX(num) from history as b where imgid = ?1 and a.operation = b.operation) order by num", -1, &stmt, NULL);
  sqlite3_prepare_v2(darktable.db, "insert into temp_history select * from history as a where imgid = ?1 and num in (select MAX(num) from history as b where imgid = ?1 and a.operation = b.operation) order by num", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, imgid);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(darktable.db, "delete from history where imgid = ?1", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, imgid);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_exec(darktable.db, "insert into history select imgid,rowid-1,module,operation,op_params,enabled from temp_history", NULL, NULL, NULL);
  sqlite3_exec(darktable.db, "delete from temp_history", NULL, NULL, NULL);
  sqlite3_exec(darktable.db, "drop table temp_history", NULL, NULL, NULL);
  dt_dev_reload_history_items(darktable.develop);
}

static void
history_button_clicked (GtkWidget *widget, gpointer user_data)
{
  static int reset = 0;
  if(reset) return;
  if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) return;
  // toggle all buttons:
  reset = 1;
  for(int i=0;i<10;i++)
  {
    char wdname[30];
    snprintf(wdname, 30, "history_%02d", i);
    GtkToggleButton *b = GTK_TOGGLE_BUTTON(glade_xml_get_widget (darktable.gui->main_window, wdname));
    if(b != GTK_TOGGLE_BUTTON(widget)) gtk_object_set(GTK_OBJECT(b), "active", FALSE, NULL);
    // else gtk_object_set(GTK_OBJECT(b), "active", TRUE, NULL);
  }
  reset = 0;
  if(darktable.gui->reset) return;
  // revert to given history item.
  long int num = (long int)user_data;
  if(num != 0) num += darktable.control->history_start;
  dt_dev_pop_history_items(darktable.develop, num);
}

void
capture_button_clicked (GtkWidget *widget, gpointer user_data)
{
	dt_ctl_switch_mode_to(DT_CAPTURE);
}

void
import_button_clicked (GtkWidget *widget, gpointer user_data)
{
  GtkWidget *win = glade_xml_get_widget (darktable.gui->main_window, "main_window");
  GtkWidget *filechooser = gtk_file_chooser_dialog_new (_("import film"),
				      GTK_WINDOW (win),
				      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, //GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);

  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), TRUE);

  if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    GSList *list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (filechooser));
    GSList *it = list;
    int id = 0;
    while(it)
    {
      filename = (char *)it->data;
      id = dt_film_import(filename);
      g_free (filename);
      it = g_slist_next(it);
    }
    if(id)
    {
      dt_film_open(id);
      dt_ctl_switch_mode_to(DT_LIBRARY);
    }
    g_slist_free (list);
  }
  gtk_widget_destroy (filechooser);
  win = glade_xml_get_widget (darktable.gui->main_window, "center");
  gtk_widget_queue_draw(win);
}

void
import_single_button_clicked (GtkWidget *widget, gpointer user_data)
{
  GtkWidget *win = glade_xml_get_widget (darktable.gui->main_window, "main_window");
  GtkWidget *filechooser = gtk_file_chooser_dialog_new (_("import image"),
				      GTK_WINDOW (win),
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);

  char *cp, **extensions, ext[1024];
  GtkFileFilter *filter;
  filter = GTK_FILE_FILTER(gtk_file_filter_new());
  extensions = g_strsplit(dt_supported_extensions, ",", 100);
  for(char **i=extensions;*i!=NULL;i++)
  {
    snprintf(ext, 1024, "*.%s", *i);
    gtk_file_filter_add_pattern(filter, ext);
    gtk_file_filter_add_pattern(filter, cp=g_ascii_strup(ext, -1));
    g_free(cp);
  }
  g_strfreev(extensions);
  gtk_file_filter_set_name(filter, _("supported images"));
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), filter);

  filter = GTK_FILE_FILTER(gtk_file_filter_new());
  gtk_file_filter_add_pattern(filter, "*");
  gtk_file_filter_set_name(filter, _("all files"));
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), filter);

  if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
    int id = dt_image_import(1, filename);
    if(id)
    {
      dt_film_open(1);
      DT_CTL_SET_GLOBAL(lib_image_mouse_over_id, id);
      dt_ctl_switch_mode_to(DT_DEVELOP);
    }
    else
    {
      GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(win),
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE,
                                  _("error loading file '%s'"),
                                  filename);
       gtk_dialog_run (GTK_DIALOG (dialog));
       gtk_widget_destroy (dialog);
    }
    g_free (filename);
  }
  gtk_widget_destroy (filechooser);
  win = glade_xml_get_widget (darktable.gui->main_window, "center");
  gtk_widget_queue_draw(win);
}

static gboolean
scrolled (GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
  dt_view_manager_scrolled(darktable.view_manager, event->x, event->y, event->direction == GDK_SCROLL_UP);
  gtk_widget_queue_draw(widget);
  return TRUE;
}

static gboolean
borders_scrolled (GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
  dt_view_manager_border_scrolled(darktable.view_manager, event->x, event->y, (long int)user_data, event->direction == GDK_SCROLL_UP);
  gtk_widget_queue_draw(widget);
  return TRUE;
}

void quit()
{
  // thread safe quit, 1st pass:
  GtkWindow *win = GTK_WINDOW(glade_xml_get_widget (darktable.gui->main_window, "main_window"));
  gtk_window_iconify(win);

  pthread_mutex_lock(&darktable.control->cond_mutex);
  darktable.control->running = 0;
  pthread_mutex_unlock(&darktable.control->cond_mutex);
  GtkWidget *widget = glade_xml_get_widget (darktable.gui->main_window, "center");
  gtk_widget_queue_draw(widget);
}

static void quit_callback(void *p)
{
  quit();
}

static gboolean
configure (GtkWidget *da, GdkEventConfigure *event, gpointer user_data)
{
  static int oldw = 0;
  static int oldh = 0;
  //make our selves a properly sized pixmap if our window has been resized
  if (oldw != event->width || oldh != event->height)
  {
    //create our new pixmap with the correct size.
    GdkPixmap *tmppixmap = gdk_pixmap_new(da->window, event->width,  event->height, -1);
    //copy the contents of the old pixmap to the new pixmap.  This keeps ugly uninitialized
    //pixmaps from being painted upon resize
    int minw = oldw, minh = oldh;
    if(event->width  < minw) minw = event->width;
    if(event->height < minh) minh = event->height;
    gdk_draw_drawable(tmppixmap, da->style->fg_gc[GTK_WIDGET_STATE(da)], darktable.gui->pixmap, 0, 0, 0, 0, minw, minh);
    //we're done with our old pixmap, so we can get rid of it and replace it with our properly-sized one.
    g_object_unref(darktable.gui->pixmap); 
    darktable.gui->pixmap = tmppixmap;
  }
  oldw = event->width;
  oldh = event->height;

  return dt_control_configure(da, event, user_data);
}

void dt_gui_key_accel_register(guint state, guint keyval, void (*callback)(void *), void *data)
{
  dt_gui_key_accel_t *a = (dt_gui_key_accel_t *)malloc(sizeof(dt_gui_key_accel_t));
  a->state = state;
  a->keyval = keyval;
  a->callback = callback;
  a->data = data;
  darktable.gui->key_accels = g_list_append(darktable.gui->key_accels, a);
}

void dt_gui_key_accel_unregister(void (*callback)(void *))
{
  GList *i = darktable.gui->key_accels;
  while(i)
  {
    dt_gui_key_accel_t *a = (dt_gui_key_accel_t *)i->data;
    GList *ii = g_list_next(i);
    if(a->callback == callback)
      darktable.gui->key_accels = g_list_delete_link(darktable.gui->key_accels, i);
    i = ii;
  }
}

static gboolean
key_pressed_override (GtkWidget *w, GdkEventKey *event, gpointer user_data)
{
  GList *i = darktable.gui->key_accels;
  while(i)
  {
    dt_gui_key_accel_t *a = (dt_gui_key_accel_t *)i->data;
    if(a->state == (a->state & event->state) && a->keyval == event->keyval)
    {
      a->callback(a->data);
      return TRUE;
    }
    i = g_list_next(i);
  }
  return dt_control_key_pressed_override(event->hardware_keycode);
}

static gboolean
key_pressed (GtkWidget *w, GdkEventKey *event, gpointer user_data)
{
  return dt_control_key_pressed(event->hardware_keycode);
}

static gboolean
button_pressed (GtkWidget *w, GdkEventButton *event, gpointer user_data)
{
  dt_control_button_pressed(event->x, event->y, event->button, event->type, event->state);
  gtk_widget_grab_focus(w);
  gtk_widget_queue_draw(w);
  return TRUE;
}

static gboolean
button_released (GtkWidget *w, GdkEventButton *event, gpointer user_data)
{
  dt_control_button_released(event->x, event->y, event->button, event->state);
  gtk_widget_queue_draw(w);
  return TRUE;
}

static gboolean
mouse_moved (GtkWidget *w, GdkEventMotion *event, gpointer user_data)
{
  dt_control_mouse_moved(event->x, event->y, event->state);
  gint x, y;
  gdk_window_get_pointer(event->window, &x, &y, NULL);
  return TRUE;
}

gboolean center_leave(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
  dt_control_mouse_leave();
  return TRUE;
}


int
dt_gui_gtk_init(dt_gui_gtk_t *gui, int argc, char *argv[])
{
  GtkWidget *widget;

  gui->num_snapshots = 0;
  gui->request_snapshot = 0;
  gui->selected_snapshot = 0;
  gui->snapshot_image = NULL;
  bzero(gui->snapshot, sizeof(gui->snapshot));
  for(int k=0;k<4;k++) snprintf(gui->snapshot[k].filename, 30, "/tmp/dt_snapshot_%d.png", k);
  if (!g_thread_supported ()) g_thread_init(NULL);
  gdk_threads_init();
  gdk_threads_enter();
  gtk_init (&argc, &argv);

  char path[1024], datadir[1024];
  dt_get_datadir(datadir, 1024);
  snprintf(path, 1023, "%s/darktable.gtkrc", datadir);
  if(g_file_test(path, G_FILE_TEST_EXISTS)) gtk_rc_parse (path);
  else
  {
    snprintf(path, 1023, "%s/darktable.gtkrc", DATADIR);
    if(g_file_test(path, G_FILE_TEST_EXISTS)) gtk_rc_parse (path);
    else
    {
      fprintf(stderr, "[gtk_init] could not find darktable.gtkrc in . or %s!\n", DATADIR);
      return 1;
    }
  }

  /* load the interface */
  snprintf(path, 1023, "%s/darktable.glade", datadir);
  if(g_file_test(path, G_FILE_TEST_EXISTS)) darktable.gui->main_window = glade_xml_new (path, NULL, NULL);
  else
  {
    snprintf(path, 1023, "%s/darktable.glade", DATADIR);
    if(g_file_test(path, G_FILE_TEST_EXISTS)) darktable.gui->main_window = glade_xml_new (path, NULL, NULL);
    else
    {
      fprintf(stderr, "[gtk_init] could not find darktable.glade in . or %s!\n", DATADIR);
      return 1;
    }
  }

  
  /* connect the signals in the interface */
  widget = glade_xml_get_widget (darktable.gui->main_window, "capture_import");
  g_signal_connect (G_OBJECT (widget), "clicked",
                    G_CALLBACK (capture_button_clicked),
                    NULL);

  widget = glade_xml_get_widget (darktable.gui->main_window, "button_import");
  g_signal_connect (G_OBJECT (widget), "clicked",
                    G_CALLBACK (import_button_clicked),
                    NULL);

  widget = glade_xml_get_widget (darktable.gui->main_window, "button_import_single");
  g_signal_connect (G_OBJECT (widget), "clicked",
                    G_CALLBACK (import_single_button_clicked),
                    NULL);

  /* Have the delete event (window close) end the program */
  dt_get_datadir(datadir, 1024);
  snprintf(path, 1024, "%s/pixmaps/darktable-16.png", datadir);
  widget = glade_xml_get_widget (darktable.gui->main_window, "main_window");
  gtk_window_set_icon_from_file(GTK_WINDOW(widget), path, NULL);
  gtk_window_set_title(GTK_WINDOW(widget), PACKAGE_NAME"-"PACKAGE_VERSION);

  g_signal_connect (G_OBJECT (widget), "delete_event",
                    G_CALLBACK (quit), NULL);
  g_signal_connect (G_OBJECT (widget), "key-press-event",
                    G_CALLBACK (key_pressed_override), NULL);
  // g_signal_connect (G_OBJECT (widget), "key-press-event",
                    // G_CALLBACK (key_pressed), NULL);

  gtk_widget_show_all(widget);

  widget = glade_xml_get_widget (darktable.gui->main_window, "darktable_label");
  gtk_label_set_label(GTK_LABEL(widget), "<span color=\"#7f7f7f\"><big><b>"PACKAGE_NAME"-"PACKAGE_VERSION"</b></big></span>");
  widget = glade_xml_get_widget (darktable.gui->main_window, "center");

  g_signal_connect (G_OBJECT (widget), "key-press-event",
                    G_CALLBACK (key_pressed), NULL);
	g_signal_connect (G_OBJECT (widget), "configure-event",
                    G_CALLBACK (configure), NULL);
	g_signal_connect (G_OBJECT (widget), "expose-event",
                    G_CALLBACK (expose), NULL);
	g_signal_connect (G_OBJECT (widget), "motion-notify-event",
                    G_CALLBACK (mouse_moved), NULL);
	g_signal_connect (G_OBJECT (widget), "leave-notify-event",
                    G_CALLBACK (center_leave), NULL);
	g_signal_connect (G_OBJECT (widget), "button-press-event",
                    G_CALLBACK (button_pressed), NULL);
	g_signal_connect (G_OBJECT (widget), "button-release-event",
                    G_CALLBACK (button_released), NULL);
	g_signal_connect (G_OBJECT (widget), "scroll-event",
                    G_CALLBACK (scrolled), NULL);
  // TODO: left, right, top, bottom:
  //leave-notify-event

  widget = glade_xml_get_widget (darktable.gui->main_window, "leftborder");
	g_signal_connect (G_OBJECT (widget), "expose-event", G_CALLBACK (expose_borders), (gpointer)0);
	g_signal_connect (G_OBJECT (widget), "button-press-event", G_CALLBACK (borders_button_pressed), (gpointer)0);
	g_signal_connect (G_OBJECT (widget), "scroll-event", G_CALLBACK (borders_scrolled), (gpointer)0);
  widget = glade_xml_get_widget (darktable.gui->main_window, "rightborder");
	g_signal_connect (G_OBJECT (widget), "expose-event", G_CALLBACK (expose_borders), (gpointer)1);
	g_signal_connect (G_OBJECT (widget), "button-press-event", G_CALLBACK (borders_button_pressed), (gpointer)1);
	g_signal_connect (G_OBJECT (widget), "scroll-event", G_CALLBACK (borders_scrolled), (gpointer)1);
  widget = glade_xml_get_widget (darktable.gui->main_window, "topborder");
	g_signal_connect (G_OBJECT (widget), "expose-event", G_CALLBACK (expose_borders), (gpointer)2);
	g_signal_connect (G_OBJECT (widget), "button-press-event", G_CALLBACK (borders_button_pressed), (gpointer)2);
	g_signal_connect (G_OBJECT (widget), "scroll-event", G_CALLBACK (borders_scrolled), (gpointer)2);
  widget = glade_xml_get_widget (darktable.gui->main_window, "bottomborder");
	g_signal_connect (G_OBJECT (widget), "expose-event", G_CALLBACK (expose_borders), (gpointer)3);
	g_signal_connect (G_OBJECT (widget), "button-press-event", G_CALLBACK (borders_button_pressed), (gpointer)3);
	g_signal_connect (G_OBJECT (widget), "scroll-event", G_CALLBACK (borders_scrolled), (gpointer)3);

  widget = glade_xml_get_widget (darktable.gui->main_window, "navigation");
  dt_gui_navigation_init(&gui->navigation, widget);

  widget = glade_xml_get_widget (darktable.gui->main_window, "histogram");
  dt_gui_histogram_init(&gui->histogram, widget);

  dt_gui_filmview_init();

  // film history
  for(long int k=1;k<5;k++)
  {
    char wdname[20];
    snprintf(wdname, 20, "recent_film_%ld", k);
    widget = glade_xml_get_widget (darktable.gui->main_window, wdname);
    g_signal_connect (G_OBJECT (widget), "clicked",
                      G_CALLBACK (film_button_clicked),
                      (gpointer)(k-1));
  }

  // image op history
  for(long int k=0;k<10;k++)
  {
    char wdname[20];
    snprintf(wdname, 20, "history_%02ld", k);
    widget = glade_xml_get_widget (darktable.gui->main_window, wdname);
    g_signal_connect (G_OBJECT (widget), "clicked",
                      G_CALLBACK (history_button_clicked),
                      (gpointer)k);
  }
  widget = glade_xml_get_widget (darktable.gui->main_window, "history_compress_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
      G_CALLBACK (history_compress_clicked),
      (gpointer)0);

  // image filtering/sorting
  widget = glade_xml_get_widget (darktable.gui->main_window, "image_filter");
  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (image_filter_changed),
                    (gpointer)0);
  widget = glade_xml_get_widget (darktable.gui->main_window, "image_sort");
  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (image_sort_changed),
                    (gpointer)0);

  // snapshot management
  widget = glade_xml_get_widget (darktable.gui->main_window, "snapshot_take_button");
  g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(snapshot_add_button_clicked), NULL);

  widget = glade_xml_get_widget (darktable.gui->main_window, "snapshot_1_togglebutton");
  g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(snapshot_toggled), (gpointer)0);
  widget = glade_xml_get_widget (darktable.gui->main_window, "snapshot_2_togglebutton");
  g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(snapshot_toggled), (gpointer)1);
  widget = glade_xml_get_widget (darktable.gui->main_window, "snapshot_3_togglebutton");
  g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(snapshot_toggled), (gpointer)2);
  widget = glade_xml_get_widget (darktable.gui->main_window, "snapshot_4_togglebutton");
  g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(snapshot_toggled), (gpointer)3);

  // color picker
  widget = glade_xml_get_widget (darktable.gui->main_window, "colorpicker_mean_combobox");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), dt_conf_get_int("ui_last/colorpicker_mean"));
  g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(colorpicker_mean_changed), NULL);
  widget = glade_xml_get_widget (darktable.gui->main_window, "colorpicker_togglebutton");
  g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(colorpicker_toggled), NULL);

  // lighttable layout
  widget = glade_xml_get_widget (darktable.gui->main_window, "lighttable_layout_combobox");
  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (lighttable_layout_changed),
                    (gpointer)0);

  widget = glade_xml_get_widget (darktable.gui->main_window, "lighttable_zoom_spinbutton");
  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (lighttable_zoom_changed),
                    (gpointer)0);

  // nice endmarker drawing.
  widget = glade_xml_get_widget (darktable.gui->main_window, "endmarker_left");
  g_signal_connect (G_OBJECT (widget), "expose-event",
                    G_CALLBACK (dt_control_expose_endmarker), (gpointer)1);

  // switch modes in gui by double-clicking label
  widget = glade_xml_get_widget (darktable.gui->main_window, "view_label_eventbox");
  g_signal_connect (G_OBJECT (widget), "button-press-event",
                    G_CALLBACK (view_label_clicked),
                    (gpointer)0);


  widget = glade_xml_get_widget (darktable.gui->main_window, "center");
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_DOUBLE_BUFFERED);
  // GTK_WIDGET_SET_FLAGS (widget, GTK_DOUBLE_BUFFERED);
  GTK_WIDGET_SET_FLAGS   (widget, GTK_APP_PAINTABLE);

  // TODO: make this work as: libgnomeui testgnome.c
  GtkContainer *box = GTK_CONTAINER(glade_xml_get_widget (darktable.gui->main_window, "plugins_vbox"));
  GtkScrolledWindow *swin = GTK_SCROLLED_WINDOW(glade_xml_get_widget (darktable.gui->main_window, "right_scrolledwindow"));
  gtk_container_set_focus_vadjustment (box, gtk_scrolled_window_get_vadjustment (swin));

  dt_ctl_get_display_profile(widget, &darktable.control->xprofile_data, &darktable.control->xprofile_size);

  darktable.gui->redraw_widgets = NULL;
  darktable.gui->key_accels = NULL;

  // register ctrl-q to quit:
  dt_gui_key_accel_register(GDK_CONTROL_MASK, GDK_q, quit_callback, (void *)0);
  darktable.gui->reset = 0;
  for(int i=0;i<3;i++) darktable.gui->bgcolor[i] = 0.1333;
  return 0;
}

void dt_gui_gtk_cleanup(dt_gui_gtk_t *gui)
{
  g_free(darktable.control->xprofile_data);
  darktable.control->xprofile_size = 0;
  dt_gui_navigation_cleanup(&gui->navigation);
  dt_gui_histogram_cleanup(&gui->histogram);
}

void dt_gui_gtk_run(dt_gui_gtk_t *gui)
{
  GtkWidget *widget = glade_xml_get_widget (darktable.gui->main_window, "center");
  darktable.gui->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
  /* start the event loop */
  gtk_main ();
  gdk_threads_leave();
}

