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

#include "common/collection.h"
#include "common/darktable.h"
#include "common/image_cache.h"
#include "control/conf.h"
#include "control/control.h"
#include "develop/develop.h"
#include "views/view.h"
#include "gui/gtk.h"
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <strings.h>
#include <glade/glade.h>
#include <math.h>
/*
* events
*/

void _view_manager_initialize_event_object (dt_view_manager_t *vm)
{
  GType typecode;
  GTypeInfo _view_manager_event_type_info = { 0,
      (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) NULL, (GClassFinalizeFunc) NULL,
      NULL, 0,0, (GInstanceInitFunc) NULL
  };
  GTypeQuery query;

  g_type_query(G_TYPE_OBJECT, &query);
  _view_manager_event_type_info.class_size = query.class_size;
  _view_manager_event_type_info.instance_size = query.instance_size;

  /* increase the size of the class or instance structs here if necessary */

  typecode = g_type_register_static(G_TYPE_OBJECT, "ViewManagerEvents",&_view_manager_event_type_info, 0);
  
  vm->events = g_object_new (typecode,NULL);
  g_signal_new( "film-strip-view-toggled",G_TYPE_OBJECT, G_SIGNAL_RUN_LAST,0,NULL,NULL,g_cclosure_marshal_VOID__VOID,GTK_TYPE_NONE,0);
  g_signal_new( "view-manager-switched-mode",G_TYPE_OBJECT, G_SIGNAL_RUN_LAST,0,NULL,NULL,g_cclosure_marshal_VOID__VOID,GTK_TYPE_NONE,0);
  
}



void dt_view_manager_init(dt_view_manager_t *vm)
{
  _view_manager_initialize_event_object(vm);
  vm->film_strip_dragging = 0;
  vm->film_strip_on = 0;
  vm->film_strip_size = 0.15f;
  vm->film_strip_scroll_to = -1;
  vm->film_strip_active_image = -1;
  vm->num_views = 0;
  if(dt_view_load_module(&vm->film_strip, "filmstrip"))
    fprintf(stderr, "[view_manager_init] failed to load film strip view!\n");
  
  int res=0, midx=0;
  char *modules[] = {"darkroom","lighttable","capture",NULL};
  char *module = modules[midx];
  while(module != NULL)
  {
    if((res=dt_view_manager_load_module(vm, module))<0) 
      fprintf(stderr,"[view_manager_init] failed to load view module '%s'\n",module);
    else
    { // Module loaded lets handle specific cases
      if(strcmp(module,"darkroom") == 0) 
        darktable.develop = (dt_develop_t *)vm->view[res].data;
    }
    
    module = modules[++midx];
  }
  vm->current_view=-1;
}

void dt_view_manager_cleanup(dt_view_manager_t *vm)
{
  for(int k=0;k<vm->num_views;k++) dt_view_unload_module(vm->view + k);
}

const dt_view_t *dt_view_manager_get_current_view(dt_view_manager_t *vm) 
{
  return &vm->view[vm->current_view];
}


int dt_view_manager_load_module(dt_view_manager_t *vm, const char *mod)
{
  if(vm->num_views >= DT_VIEW_MAX_MODULES) return -1;
  if(dt_view_load_module(vm->view+vm->num_views, mod)) return -1;
  return vm->num_views++;
}

/** load a view module */
int dt_view_load_module(dt_view_t *view, const char *module)
{
  int retval = -1;
  bzero(view, sizeof(dt_view_t));
  view->data = NULL;
  view->vscroll_size = view->vscroll_viewport_size = 1.0;
  view->hscroll_size = view->hscroll_viewport_size = 1.0;
  view->vscroll_pos = view->hscroll_pos = 0.0;
  view->height = view->width = 100; // set to non-insane defaults before first expose/configure.
  strncpy(view->module_name, module, 64);
  char plugindir[1024];
  dt_get_plugindir(plugindir, 1024);
  strcpy(plugindir + strlen(plugindir), "/views");
  gchar *libname = g_module_build_path(plugindir, (const gchar *)module);
  view->module = g_module_open(libname, G_MODULE_BIND_LAZY);
  if(!view->module)
  {
    fprintf(stderr, "[view_load_module] could not open %s (%s)!\n", libname, g_module_error());
    retval = -1;
    goto out;
  }
  int (*version)();
  if(!g_module_symbol(view->module, "dt_module_dt_version", (gpointer)&(version))) goto out;
  if(version() != dt_version())
  {
    fprintf(stderr, "[view_load_module] `%s' is compiled for another version of dt (module %d != dt %d) !\n", libname, version(), dt_version());
    goto out;
  }
  if(!g_module_symbol(view->module, "name",            (gpointer)&(view->name)))            view->name = NULL;
  if(!g_module_symbol(view->module, "init",            (gpointer)&(view->init)))            view->init = NULL;
  if(!g_module_symbol(view->module, "cleanup",         (gpointer)&(view->cleanup)))         view->cleanup = NULL;
  if(!g_module_symbol(view->module, "expose",          (gpointer)&(view->expose)))          view->expose = NULL;
  if(!g_module_symbol(view->module, "try_enter",       (gpointer)&(view->try_enter)))       view->try_enter = NULL;
  if(!g_module_symbol(view->module, "enter",           (gpointer)&(view->enter)))           view->enter = NULL;
  if(!g_module_symbol(view->module, "leave",           (gpointer)&(view->leave)))           view->leave = NULL;
  if(!g_module_symbol(view->module, "reset",           (gpointer)&(view->reset)))           view->reset = NULL;
  if(!g_module_symbol(view->module, "mouse_enter",     (gpointer)&(view->mouse_enter)))     view->mouse_enter= NULL;
  if(!g_module_symbol(view->module, "mouse_leave",     (gpointer)&(view->mouse_leave)))     view->mouse_leave = NULL;
  if(!g_module_symbol(view->module, "mouse_moved",     (gpointer)&(view->mouse_moved)))     view->mouse_moved = NULL;
  if(!g_module_symbol(view->module, "button_released", (gpointer)&(view->button_released))) view->button_released = NULL;
  if(!g_module_symbol(view->module, "button_pressed",  (gpointer)&(view->button_pressed)))  view->button_pressed = NULL;
  if(!g_module_symbol(view->module, "key_pressed",     (gpointer)&(view->key_pressed)))     view->key_pressed = NULL;
  if(!g_module_symbol(view->module, "key_released",    (gpointer)&(view->key_released)))    view->key_released = NULL;
  if(!g_module_symbol(view->module, "configure",       (gpointer)&(view->configure)))       view->configure = NULL;
  if(!g_module_symbol(view->module, "scrolled",        (gpointer)&(view->scrolled)))        view->scrolled = NULL;
  if(!g_module_symbol(view->module, "border_scrolled", (gpointer)&(view->border_scrolled))) view->border_scrolled = NULL;
  if(!g_module_symbol(view->module, "toggle_film_strip", (gpointer)&(view->toggle_film_strip))) view->border_scrolled = NULL;

  if(view->init) view->init(view);

  /* success */
  retval = 0;

out:
  g_free(libname);
  return retval;
}

/** unload, cleanup */
void dt_view_unload_module (dt_view_t *view)
{
  if(view->cleanup) view->cleanup(view);
  if(view->module) g_module_close(view->module);
}

void dt_vm_remove_child(GtkWidget *widget, gpointer data)
{
  gtk_container_remove(GTK_CONTAINER(data), widget);
}

int dt_view_manager_switch (dt_view_manager_t *vm, int k)
{
  // destroy old module list
  GtkContainer *table = GTK_CONTAINER(glade_xml_get_widget (darktable.gui->main_window, "module_list"));
  gtk_container_foreach(table, (GtkCallback)dt_vm_remove_child, (gpointer)table);

  int error = 0;
  dt_view_t *v = vm->view + vm->current_view;
  int newv = vm->current_view;
  if(k < vm->num_views) newv = k;
  dt_view_t *nv = vm->view + newv;
  if(nv->try_enter) error = nv->try_enter(nv);
  if(!error)
  {
    if(vm->current_view >= 0 && v->leave) v->leave(v);
    vm->current_view = newv;
    if(newv >= 0 && nv->enter) nv->enter(nv);
    
    g_signal_emit_by_name(G_OBJECT(vm->events),"view-manager-switched-mode");

  }
  return error;
}

const char *dt_view_manager_name (dt_view_manager_t *vm)
{
  if(vm->current_view < 0) return "";
  dt_view_t *v = vm->view + vm->current_view;
  if(v->name) return v->name(v);
  else return v->module_name;
}

void dt_view_manager_expose (dt_view_manager_t *vm, cairo_t *cr, int32_t width, int32_t height, int32_t pointerx, int32_t pointery)
{
  if(vm->current_view < 0)
  {
    cairo_set_source_rgb(cr, darktable.gui->bgcolor[0], darktable.gui->bgcolor[1], darktable.gui->bgcolor[2]);
    cairo_paint(cr);
    return;
  }
  dt_view_t *v = vm->view + vm->current_view;
  v->width = width; v->height = height;
  if(vm->film_strip_on)
  {
    const float tb = darktable.control->tabborder;
    cairo_save(cr);
    v->height = height*(1.0-vm->film_strip_size) - tb;
    vm->film_strip.height = height * vm->film_strip_size;
    vm->film_strip.width  = width;
    cairo_rectangle(cr, -10, v->height, width+20, tb);
    cairo_set_source_rgb (cr, darktable.gui->bgcolor[0]+0.04, darktable.gui->bgcolor[1]+0.04, darktable.gui->bgcolor[2]+0.04);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgb (cr, .1, .1, .1);
    cairo_stroke(cr);
    cairo_translate(cr, 0, v->height+tb);
    cairo_rectangle(cr, 0, 0, vm->film_strip.width, vm->film_strip.height);
    cairo_clip(cr);
    cairo_new_path(cr);
    float px = pointerx, py = pointery;
    if(pointery <= v->height+darktable.control->tabborder) { px = 10000.0; py = -1.0; }
    vm->film_strip.expose(&(vm->film_strip), cr, vm->film_strip.width, vm->film_strip.height, px, py);
    cairo_restore(cr);
  }
  if(v->expose)
  {
    cairo_rectangle(cr, 0, 0, v->width, v->height);
    cairo_clip(cr);
    cairo_new_path(cr);
    float px = pointerx, py = pointery;
    if(pointery > v->height) { px = 10000.0; py = -1.0; }
    v->expose(v, cr, v->width, v->height, px, py);
  }
}

void dt_view_manager_reset (dt_view_manager_t *vm)
{
  if(vm->current_view < 0) return;
  dt_view_t *v = vm->view + vm->current_view;
  if(v->reset) v->reset(v);
}

void dt_view_manager_mouse_leave (dt_view_manager_t *vm)
{
  if(vm->current_view < 0) return;
  dt_view_t *v = vm->view + vm->current_view;
  if(v->mouse_leave) v->mouse_leave(v);
  if(vm->film_strip_on && vm->film_strip.mouse_leave) vm->film_strip.mouse_leave(&vm->film_strip);
}

void dt_view_manager_mouse_enter (dt_view_manager_t *vm)
{
  if(vm->current_view < 0) return;
  dt_view_t *v = vm->view + vm->current_view;
  if(vm->film_strip_on && vm->film_strip.mouse_enter) vm->film_strip.mouse_enter(&vm->film_strip);
  if(v->mouse_enter) v->mouse_enter(v);
}

void dt_view_manager_mouse_moved (dt_view_manager_t *vm, double x, double y, int which)
{
  static int oldstate = 0;
  const float tb = darktable.control->tabborder;
  if(vm->current_view < 0) return;
  dt_view_t *v = vm->view + vm->current_view;
  if(vm->film_strip_on && vm->film_strip_dragging)
  {
    vm->film_strip_size = fmaxf(0.1, fminf(0.6, (darktable.control->height - y - .5*tb)/darktable.control->height));
    const int wd = darktable.control->width  - 2*tb;
    const int ht = darktable.control->height - 2*tb;
    dt_view_manager_configure (vm, wd, ht);
  }
  else if(vm->film_strip_on && v->height + tb < y && vm->film_strip.mouse_moved)
    vm->film_strip.mouse_moved(&vm->film_strip, x, y - v->height - tb, which);
  else if(v->mouse_moved) v->mouse_moved(v, x, y, which);

  int state = vm->film_strip_on && (v->height + tb > y) && (y > v->height);
  if(state && !oldstate)      dt_control_change_cursor(GDK_SB_V_DOUBLE_ARROW);
  else if(oldstate && !state) dt_control_change_cursor(GDK_LEFT_PTR);
  oldstate = state;
}

int dt_view_manager_button_released (dt_view_manager_t *vm, double x, double y, int which, uint32_t state)
{
  if(vm->current_view < 0) return 0;
  dt_view_t *v = vm->view + vm->current_view;
  vm->film_strip_dragging = 0;
  dt_control_change_cursor(GDK_LEFT_PTR);
  if(vm->film_strip_on && v->height + darktable.control->tabborder < y && vm->film_strip.button_released)
    return vm->film_strip.button_released(&vm->film_strip, x, y - v->height - darktable.control->tabborder, which, state);
  else if(v->button_released) return v->button_released(v, x, y, which, state);
  return 0;
}

int dt_view_manager_button_pressed (dt_view_manager_t *vm, double x, double y, int which, int type, uint32_t state)
{
  if(vm->current_view < 0) return 0;
  dt_view_t *v = vm->view + vm->current_view;

  if(vm->film_strip_on && y > v->height && y < v->height + darktable.control->tabborder)
  {
    vm->film_strip_dragging = 1;
    dt_control_change_cursor(GDK_SB_V_DOUBLE_ARROW);
  }
  else if(vm->film_strip_on && v->height + darktable.control->tabborder < y && vm->film_strip.button_pressed)
    return vm->film_strip.button_pressed(&vm->film_strip, x, y - v->height - darktable.control->tabborder, which, type, state);
  else if(v->button_pressed) return v->button_pressed(v, x, y, which, type, state);
  return 0;
}

int dt_view_manager_key_pressed (dt_view_manager_t *vm, uint16_t which)
{
  if(vm->current_view < 0) return 0;
  dt_view_t *v = vm->view + vm->current_view;
  if(v->key_pressed) return v->key_pressed(v, which);
  return 0;
}

int dt_view_manager_key_released (dt_view_manager_t *vm, uint16_t which)
{
  if(vm->current_view < 0) return 0;
  dt_view_t *v = vm->view + vm->current_view;
  if(v->key_released) return v->key_released(v, which);
  return 0;
}

void dt_view_manager_configure (dt_view_manager_t *vm, int width, int height)
{
  if(vm->film_strip_on) height = height*(1.0-vm->film_strip_size)-darktable.control->tabborder;
  for(int k=0;k<vm->num_views;k++)
  { // this is necessary for all
    dt_view_t *v = vm->view + k;
    v->width = width;
    v->height = height;
    if(v->configure) v->configure(v, width, height);
  }
}

void dt_view_manager_scrolled (dt_view_manager_t *vm, double x, double y, int up, int state)
{
  if(vm->current_view < 0) return;
  dt_view_t *v = vm->view + vm->current_view;
  if(vm->film_strip_on && v->height + darktable.control->tabborder < y && vm->film_strip.scrolled)
    return vm->film_strip.scrolled(&vm->film_strip, x, y - v->height - darktable.control->tabborder, up, state);
  if(v->scrolled) v->scrolled(v, x, y, up, state);
}

void dt_view_manager_border_scrolled (dt_view_manager_t *vm, double x, double y, int which, int up)
{
  if(vm->current_view < 0) return;
  dt_view_t *v = vm->view + vm->current_view;
  if(v->border_scrolled) v->border_scrolled(v, x, y, which, up);
}

void dt_view_manager_toggle_film_strip (dt_view_manager_t *vm)
{
  if(vm->current_view < 0) return;
  dt_view_t *v = vm->view + vm->current_view;
  if(v->toggle_film_strip) v->toggle_film_strip(v);
}

void dt_view_set_scrollbar(dt_view_t *view, float hpos, float hsize, float hwinsize, float vpos, float vsize, float vwinsize)
{
  view->vscroll_pos = vpos;
  view->vscroll_size = vsize;
  view->vscroll_viewport_size = vwinsize;
  view->hscroll_pos = hpos;
  view->hscroll_size = hsize;
  view->hscroll_viewport_size = hwinsize;
  GtkWidget *widget;
  widget = glade_xml_get_widget (darktable.gui->main_window, "leftborder");
  gtk_widget_queue_draw(widget);
  widget = glade_xml_get_widget (darktable.gui->main_window, "rightborder");
  gtk_widget_queue_draw(widget);
  widget = glade_xml_get_widget (darktable.gui->main_window, "bottomborder");
  gtk_widget_queue_draw(widget);
  widget = glade_xml_get_widget (darktable.gui->main_window, "topborder");
  gtk_widget_queue_draw(widget);
}

static inline void
dt_view_draw_altered(cairo_t *cr, const float x, const float y, const float r)
{
  cairo_arc(cr, x, y, r, 0, 2.0f*M_PI);
  const float dx = r*cosf(M_PI/8.0f), dy = r*sinf(M_PI/8.0f);
  cairo_move_to(cr, x-dx, y-dy);
  cairo_curve_to(cr, x, y-2*dy, x, y+2*dy, x+dx, y+dy);
  cairo_move_to(cr, x-.20*dx, y+.8*dy);
  cairo_line_to(cr, x-.80*dx, y+.8*dy);
  cairo_move_to(cr, x+.20*dx, y-.8*dy);
  cairo_line_to(cr, x+.80*dx, y-.8*dy);
  cairo_move_to(cr, x+.50*dx, y-.8*dy-0.3*dx);
  cairo_line_to(cr, x+.50*dx, y-.8*dy+0.3*dx);
  cairo_stroke(cr);
}

static inline void
dt_view_star(cairo_t *cr, float x, float y, float r1, float r2)
{
  const float d = 2.0*M_PI*0.1f;
  const float dx[10] = {sinf(0.0), sinf(d), sinf(2*d), sinf(3*d), sinf(4*d), sinf(5*d), sinf(6*d), sinf(7*d), sinf(8*d), sinf(9*d)};
  const float dy[10] = {cosf(0.0), cosf(d), cosf(2*d), cosf(3*d), cosf(4*d), cosf(5*d), cosf(6*d), cosf(7*d), cosf(8*d), cosf(9*d)};
  cairo_move_to(cr, x+r1*dx[0], y-r1*dy[0]);
  for(int k=1;k<10;k++)
    if(k&1) cairo_line_to(cr, x+r2*dx[k], y-r2*dy[k]);
    else    cairo_line_to(cr, x+r1*dx[k], y-r1*dy[k]);
  cairo_close_path(cr);
}

void dt_view_image_expose(dt_image_t *img, dt_view_image_over_t *image_over, int32_t index, cairo_t *cr, int32_t width, int32_t height, int32_t zoom, int32_t px, int32_t py)
{
  cairo_save (cr);
  const int32_t imgid = img ? img->id : index; // in case of locked image, use id to draw basic stuff.
  float bgcol = 0.4, fontcol = 0.5, bordercol = 0.1, outlinecol = 0.2;
  int selected = 0, altered = 0, imgsel;
  DT_CTL_GET_GLOBAL(imgsel, lib_image_mouse_over_id);
  // if(img->flags & DT_IMAGE_SELECTED) selected = 1;
  sqlite3_stmt *stmt;
  int rc;
  rc = sqlite3_prepare_v2(darktable.db, "select * from selected_images where imgid = ?1", -1, &stmt, NULL);
  rc = sqlite3_bind_int (stmt, 1, imgid);
  if(sqlite3_step(stmt) == SQLITE_ROW) selected = 1;
  sqlite3_finalize(stmt);
  rc = sqlite3_prepare_v2(darktable.db, "select num from history where imgid = ?1", -1, &stmt, NULL);
  rc = sqlite3_bind_int (stmt, 1, imgid);
  if(sqlite3_step(stmt) == SQLITE_ROW) altered = 1;
  sqlite3_finalize(stmt);
  if(selected == 1)
  {
    outlinecol = 0.4;
    bgcol = 0.6; fontcol = 0.5;
  }
  if(imgsel == imgid) { bgcol = 0.8; fontcol = 0.7; outlinecol = 0.6; } // mouse over
  float imgwd = 0.8f;
  if(zoom == 1)
  {
    imgwd = .97f;
    // cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
  }
  else
  {
    double x0 = 0.007*width, y0 = 0.007*height, rect_width = 0.986*width, rect_height = 0.986*height, radius = 0.04*width;
    // double x0 = 0.*width, y0 = 0.*height, rect_width = 1.*width, rect_height = 1.*height, radius = 0.08*width;
    double x1, y1, off, off1;

    x1=x0+rect_width;
    y1=y0+rect_height;
    off=radius*0.666;
    off1 = radius-off;
    cairo_move_to  (cr, x0, y0 + radius);
    cairo_curve_to (cr, x0, y0+off1, x0+off1 , y0, x0 + radius, y0);
    cairo_line_to (cr, x1 - radius, y0);
    cairo_curve_to (cr, x1-off1, y0, x1, y0+off1, x1, y0 + radius);
    cairo_line_to (cr, x1 , y1 - radius);
    cairo_curve_to (cr, x1, y1-off1, x1-off1, y1, x1 - radius, y1);
    cairo_line_to (cr, x0 + radius, y1);
    cairo_curve_to (cr, x0+off1, y1, x0, y1-off1, x0, y1- radius);
    cairo_close_path (cr);
    cairo_set_source_rgb(cr, bgcol, bgcol, bgcol);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 0.005*width);
    cairo_set_source_rgb(cr, outlinecol, outlinecol, outlinecol);
    cairo_stroke(cr);

#if defined(__MACH__) || defined(__APPLE__) // dreggn
#else
    if(img)
    {
      const char *ext = img->filename + strlen(img->filename);
      while(ext > img->filename && *ext != '.') ext--;
      ext++;
      cairo_set_source_rgb(cr, fontcol, fontcol, fontcol);
      cairo_select_font_face (cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size (cr, .25*width);

      cairo_move_to (cr, .01*width, .24*height);
      cairo_show_text (cr, ext);
    }
#endif
  }

#if 1
  int32_t iwd = width*imgwd, iht = height*imgwd, stride;
  float fwd=0, fht=0;
  float scale = 1.0;
  dt_image_buffer_t mip = DT_IMAGE_NONE;
  if(img)
  {
    mip = dt_image_get_matching_mip_size(img, imgwd*width, imgwd*height, &iwd, &iht);
    mip = dt_image_get(img, mip, 'r');
    iwd = img->mip_width[mip]; iht = img->mip_height[mip];
    fwd = img->mip_width_f[mip]; fht = img->mip_height_f[mip];
    // dt_image_get_mip_size(img, mip, &iwd, &iht);
    // dt_image_get_exact_mip_size(img, mip, &fwd, &fht);
  }
  cairo_surface_t *surface = NULL;
  if(mip != DT_IMAGE_NONE)
  {
    stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, iwd);
    surface = cairo_image_surface_create_for_data (img->mip[mip], CAIRO_FORMAT_RGB24, iwd, iht, stride); 
    scale = fminf(width*imgwd/fwd, height*imgwd/fht);
  }

  // draw centered and fitted:
  cairo_save(cr);
  cairo_translate(cr, width/2.0, height/2.0f);
  cairo_scale(cr, scale, scale);
  cairo_translate(cr, -.5f*fwd, -.5f*fht);

  if(mip != DT_IMAGE_NONE)
  {
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_rectangle(cr, 0, 0, fwd, fht);
    cairo_fill(cr);
    cairo_surface_destroy (surface);
    dt_image_release(img, mip, 'r');

    if(zoom == 1) cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
    cairo_rectangle(cr, 0, 0, fwd, fht);
  }

  // border around image
  const float border = zoom == 1 ? 16/scale : 2/scale;
  cairo_set_source_rgb(cr, bordercol, bordercol, bordercol);
  if(mip != DT_IMAGE_NONE && selected)
  {
    cairo_set_line_width(cr, 1./scale);
    if(zoom == 1)
    { // draw shadow around border
      cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
      cairo_stroke(cr);
      // cairo_new_path(cr);
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      float alpha = 1.0f;
      for(int k=0;k<16;k++)
      {
        cairo_rectangle(cr, 0, 0, fwd, fht);
        cairo_new_sub_path(cr);
        cairo_rectangle(cr, -k/scale, -k/scale, fwd+2.*k/scale, fht+2.*k/scale);
        cairo_set_source_rgba(cr, 0, 0, 0, alpha);
        alpha *= 0.6f;
        cairo_fill(cr);
      }
    }
    else
    {
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      cairo_new_sub_path(cr);
      cairo_rectangle(cr, -border, -border, fwd+2.*border, fht+2.*border);
      cairo_stroke_preserve(cr);
      cairo_set_source_rgb(cr, 1.0-bordercol, 1.0-bordercol, 1.0-bordercol);
      cairo_fill(cr);
    }
  }
  else if(mip != DT_IMAGE_NONE)
  {
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
  }
  cairo_restore(cr);

  const float fscale = fminf(width, height);
  if(imgsel == imgid)
  { // draw mouseover hover effects, set event hook for mouse button down!
    *image_over = DT_VIEW_DESERT;
    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgb(cr, outlinecol, outlinecol, outlinecol);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    float r1, r2;
    if(zoom != 1) 
    {
      r1 = 0.06*width;
      r2 = 0.025*width;
    }
    else
    {
      r1 = 0.02*fscale;
      r2 = 0.0083*fscale;
    }
    for(int k=0;k<5;k++)
    {
      float x, y;
      if(zoom != 1) 
      {
        x = (0.15+k*0.15)*width;
        y = 0.88*height;
      }
      else
      {
        x = (.04+k*0.04)*fscale;
        y = .12*fscale;
      }
      if(k == 4)
      {
        if(altered) 
        {
          // Align to right
          float s = (r1+r2)*.5;
          if(zoom != 1) x = width*0.85;
          dt_view_draw_altered(cr, x, y, s);
        }
      }
      else
      {
        dt_view_star(cr, x, y, r1, r2);
        if((px - x)*(px - x) + (py - y)*(py - y) < r1*r1)
        {
          *image_over = DT_VIEW_STAR_1 + k;
          cairo_fill(cr);
        }
        else if(img && ((img->flags & 0x7) > k))
        {
          cairo_fill_preserve(cr);
          cairo_set_source_rgb(cr, 1.0-bordercol, 1.0-bordercol, 1.0-bordercol);
          cairo_stroke(cr);
          cairo_set_source_rgb(cr, outlinecol, outlinecol, outlinecol);
        }
        else cairo_stroke(cr);
      }
    }
  }

  // kill all paths, in case img was not loaded yet, or is blocked:
  cairo_new_path(cr);

  // TODO: make mouse sensitive, just as stars!
  { // color labels:
    const int x = zoom == 1 ? (0.04+5*0.04)*fscale : .7*width;
    const int y = zoom == 1 ? 0.12*fscale: 0.1*height;
    const int r = zoom == 1 ? 0.01*fscale : 0.03*width;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(darktable.db, "select color from color_labels where imgid=?1", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, imgid);
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
      cairo_save(cr);
      int col = sqlite3_column_int(stmt, 0);
    // see src/dtgtk/paint.c
    dtgtk_cairo_paint_label(cr, x+(4*r*col)-r, y-r, r*2, r*2, col);
      cairo_restore(cr);
    }
    sqlite3_finalize(stmt);
  }

  if(img && (zoom == 1))
  { // some exif data
    cairo_set_source_rgb(cr, .7, .7, .7);
    cairo_select_font_face (cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, .025*fscale);

    cairo_move_to (cr, .02*fscale, .04*fscale);
    // cairo_show_text(cr, img->filename);
    cairo_text_path(cr, img->filename);
    char exifline[50];
    cairo_move_to (cr, .02*fscale, .08*fscale);
    dt_image_print_exif(img, exifline, 50);
    cairo_text_path(cr, exifline);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_stroke(cr);
  }

  cairo_restore(cr);
  // if(zoom == 1) cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
#endif
}



void dt_view_toggle_selection(int iid)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(darktable.db, "select * from selected_images where imgid = ?1", -1, &stmt, NULL);
  sqlite3_bind_int (stmt, 1, iid);
  if(sqlite3_step(stmt) == SQLITE_ROW)
  {
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(darktable.db, "delete from selected_images where imgid = ?1", -1, &stmt, NULL);
    sqlite3_bind_int (stmt, 1, iid);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
  else
  {
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(darktable.db, "insert into selected_images values (?1)", -1, &stmt, NULL);
    sqlite3_bind_int (stmt, 1, iid);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
}

uint32_t dt_view_film_strip_get_active_image(dt_view_manager_t *vm)
{
  return vm->film_strip_active_image;
}

void dt_view_film_strip_set_active_image(dt_view_manager_t *vm,int iid)
{
  sqlite3_stmt *stmt;
  // First off clear all selected images...
  sqlite3_exec(darktable.db, "delete from selected_images", NULL, NULL, NULL);
 
  // Then insert a selection of image id
  sqlite3_prepare_v2(darktable.db, "insert into selected_images values (?1)", -1, &stmt, NULL);
  sqlite3_bind_int (stmt, 1, iid);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  vm->film_strip_scroll_to=vm->film_strip_active_image=iid;
}

void dt_view_film_strip_open(dt_view_manager_t *vm, void (*activated)(const int imgid, void*), void *data)
{
  vm->film_strip_activated = activated;
  vm->film_strip_data = data;
  vm->film_strip_on = 1;
  if(vm->film_strip.enter) vm->film_strip.enter(&vm->film_strip);
  const int tb = darktable.control->tabborder;
  const int wd = darktable.control->width  - 2*tb;
  const int ht = darktable.control->height - 2*tb;
  dt_view_manager_configure (vm, wd, ht);
}

void dt_view_film_strip_close(dt_view_manager_t *vm)
{
  if(vm->film_strip.leave) vm->film_strip.leave(&vm->film_strip);
  vm->film_strip_on = 0;
  const int tb = darktable.control->tabborder;
  const int wd = darktable.control->width  - 2*tb;
  const int ht = darktable.control->height - 2*tb;
  dt_view_manager_configure (vm, wd, ht);
}

void dt_view_film_strip_toggle(dt_view_manager_t *vm, void (*activated)(const int imgid, void*), void *data)
{
  if(dt_conf_get_bool("plugins/filmstrip/on"))
  {
    dt_view_film_strip_close(vm);
    dt_conf_set_bool("plugins/filmstrip/on", FALSE);
  }
  else
  {
    dt_view_film_strip_open(vm, activated, data);
    dt_conf_set_bool("plugins/filmstrip/on", TRUE);
  }
  
  g_signal_emit_by_name(G_OBJECT(vm->events),"film-strip-view-toggled");
}

void dt_view_film_strip_scroll_to(dt_view_manager_t *vm, const int st)
{
  vm->film_strip_scroll_to = st;
}

void dt_view_film_strip_prefetch()
{
  char query[1024];
  const gchar *qin = dt_collection_get_query (darktable.collection);
  int offset = 0;
  if(qin)
  {
    int imgid = -1;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(darktable.db, "select id from selected_images", -1, &stmt, NULL);
    if(sqlite3_step(stmt) == SQLITE_ROW)
      imgid = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    snprintf(query, 1024, "select rowid from (%s) where id=?3", qin);
    sqlite3_prepare_v2(darktable.db, query, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1,  0);
    sqlite3_bind_int(stmt, 2, -1);
    sqlite3_bind_int(stmt, 3, imgid);
    if(sqlite3_step(stmt) == SQLITE_ROW)
      offset = sqlite3_column_int(stmt, 0) - 1;
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(darktable.db, qin, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, offset+1);
    sqlite3_bind_int(stmt, 2, 2);
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
      imgid = sqlite3_column_int(stmt, 0);
      dt_image_t *image = dt_image_cache_get(imgid, 'r');
      dt_image_prefetch(image, DT_IMAGE_MIPF);
      dt_image_cache_release(image, 'r');
    }
    sqlite3_finalize(stmt);
  }
}

