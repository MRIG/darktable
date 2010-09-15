/*
		This file is part of darktable,
		copyright (c) 2010 henrik andersson.

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

/** this is the view for the capture module.  
	The capture module purpose is to allow a workflow for capturing images
	which is module extendable but main purpos is to support tethered capture
	using gphoto library.
	
	When entered a session is constructed = one empty filmroll might be same filmroll
	as earlier created dependent on capture filesystem structure...
	
	TODO: How to pass initialized data such as dt_camera_t ?

*/


#include "libs/lib.h"
#include "control/jobs.h"
#include "control/settings.h"
#include "control/control.h"
#include "control/conf.h"
#include "common/image_cache.h"
#include "common/darktable.h"
#include "common/camera_control.h"
#include "common/variables.h"
#include "gui/gtk.h"
#include "gui/draw.h"
#include "capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <gdk/gdkkeysyms.h>

DT_MODULE(1)

/** module data for the capture view */
typedef struct dt_capture_t
{
	/** The current image activated in capture view, either latest tethered shoot
		or manually picked from filmstrip view...
	*/
	uint32_t image_id;
	
	dt_view_image_over_t image_over;
	
	/** The capture mode, for now only supports TETHERED */
	dt_capture_mode_t mode;
	
	dt_variables_params_t *vp;
	gchar *basedirectory;
	gchar *subdirectory;
	gchar *filenamepattern;
	gchar *path;
	
	/** The jobcode name used for session initialization etc..*/
	char *jobcode;
	dt_film_t *film;
	
}
dt_capture_t;

const char *name(dt_view_t *self)
{
	return _("capture");
}

static void
film_strip_activated(const int imgid, void *data)
{
	dt_view_film_strip_set_active_image(darktable.view_manager,imgid);
	dt_control_queue_draw_all();
	dt_view_film_strip_prefetch();
}

void capture_view_switch_key_accel(void *p)
{
	// dt_view_t *self=(dt_view_t*)p;
	// dt_capture_t *lib=(dt_capture_t*)self->data;
	dt_ctl_gui_mode_t oldmode = dt_conf_get_int("ui_last/view");
	if(oldmode==DT_CAPTURE)
		dt_ctl_switch_mode_to( DT_LIBRARY );
	else
		dt_ctl_switch_mode_to( DT_CAPTURE );
}

void film_strip_key_accel(void *data)
{
	dt_view_film_strip_toggle(darktable.view_manager, film_strip_activated, data);
	dt_control_queue_draw_all();
}

void init(dt_view_t *self)
{
	self->data = malloc(sizeof(dt_capture_t));
	memset(self->data,0,sizeof(dt_capture_t));
	dt_capture_t *lib = (dt_capture_t *)self->data;
 
	// initialize capture data struct
	const int i = dt_conf_get_int("plugins/capture/mode");
	lib->mode = i;
	
	// Setup variable expanding, shares configuration as camera import uses...
	dt_variables_params_init(&lib->vp);
	lib->basedirectory = dt_conf_get_string("plugins/capture/storage/basedirectory");
	lib->subdirectory = dt_conf_get_string("plugins/capture/storage/subpath");
	lib->filenamepattern = dt_conf_get_string("plugins/capture/storage/namepattern");
	
}

void cleanup(dt_view_t *self)
{
	free(self->data);
}

uint32_t dt_capture_view_get_film_id(const dt_view_t *view) {
	g_assert( view != NULL );
	dt_capture_t *cv=(dt_capture_t *)view->data;
	if(cv->film) 
		return cv->film->id;
	// else return standard "single images"
	/// @todo maybe return 0 and check error in caller...
	return 1;
}

const gchar *dt_capture_view_get_session_path(const dt_view_t *view) 
{
	g_assert( view != NULL );
	dt_capture_t *cv=(dt_capture_t *)view->data;
	return cv->film->dirname;
}

const gchar *dt_capture_view_get_session_filename(const dt_view_t *view,const char *filename) 
{
	g_assert( view != NULL );
	dt_capture_t *cv=(dt_capture_t *)view->data;
	
	cv->vp->filename = filename;
	
	dt_variables_expand( cv->vp, cv->path, FALSE );
	const gchar *storage=dt_variables_get_result(cv->vp);
	
	dt_variables_expand( cv->vp, cv->filenamepattern, TRUE );
	const gchar *file = dt_variables_get_result(cv->vp);
	
	// Start check if file exist if it does, increase sequence and check again til we know that file doesnt exists..
	gchar *fullfile=g_build_path(G_DIR_SEPARATOR_S,storage,file,NULL);
	if( g_file_test(fullfile, G_FILE_TEST_EXISTS) == TRUE )
	{
		do
		{
			g_free(fullfile);
			dt_variables_expand( cv->vp, cv->filenamepattern, TRUE );
			file = dt_variables_get_result(cv->vp);
			fullfile=g_build_path(G_DIR_SEPARATOR_S,storage,file,NULL);
		} while( g_file_test(fullfile, G_FILE_TEST_EXISTS) == TRUE);
	}
	
	return file;
}

void dt_capture_view_set_jobcode(const dt_view_t *view, const char *name) {
	g_assert( view != NULL );
	dt_capture_t *cv=(dt_capture_t *)view->data;
	
	/* take care of previous capture filmroll */
	if( cv->film ) {
		if( dt_film_is_empty(cv->film->id) )
			dt_film_remove(cv->film->id );
		else
			dt_film_cleanup( cv->film );
	}
	
	/* lets initialize a new filmroll for the capture... */
	cv->film=(dt_film_t*)g_malloc(sizeof(dt_film_t));
	dt_film_init(cv->film);
	
	int current_filmroll = dt_conf_get_int("plugins/capture/current_filmroll");
	if(current_filmroll >= 0) 
	{
		/* open existing filmroll and import captured images into this roll */
		cv->film->id = current_filmroll;
		if (dt_film_open2 (cv->film) !=0)
		{
			/* failed to open the current filmroll, let's reset and create a new one */
			dt_conf_set_int ("plugins/capture/current_filmroll",-1);
		}
		else
			cv->path = g_strdup(cv->film->dirname);
		
	}
	
	if (dt_conf_get_int ("plugins/capture/current_filmroll") == -1)
	{
		if(cv->jobcode) 
			g_free(cv->jobcode);
		cv->jobcode = g_strdup(name);
		
		// Setup variables jobcode...
		cv->vp->jobcode = cv->jobcode;
		
		/* reset session sequence number */
		dt_variables_reset_sequence (cv->vp);
		
		// Construct the directory for filmroll...
		cv->path = g_build_path(G_DIR_SEPARATOR_S,cv->basedirectory,cv->subdirectory,NULL);
		dt_variables_expand( cv->vp, cv->path, FALSE );
		sprintf(cv->film->dirname,"%s",dt_variables_get_result(cv->vp));
			
		// Create recursive directories, abort if no access
		if( g_mkdir_with_parents(cv->film->dirname,0755) == -1 )
		{
			dt_control_log(_("failed to create session path %s."), cv->film->dirname);
			g_free( cv->film );
			return;
		}
		
		if(dt_film_new(cv->film,cv->film->dirname) > 0)
		{
			// Switch to new filmroll
			dt_film_open(cv->film->id);
			
			/* store current filmroll */
			dt_conf_set_int("plugins/capture/current_filmroll",cv->film->id);
		}

		dt_control_log(_("new session initiated '%s'"),cv->jobcode,cv->film->id);
	} 
	
	
}

const char *dt_capture_view_get_jobcode(const dt_view_t *view) {
	g_assert( view != NULL );
	dt_capture_t *cv=(dt_capture_t *)view->data;
	
	return cv->jobcode;
}

void configure(dt_view_t *self, int wd, int ht)
{
	//dt_capture_t *lib=(dt_capture_t*)self->data;
}

#define TOP_MARGIN		20
#define BOTTOM_MARGIN	20
void _expose_tethered_mode(dt_view_t *self, cairo_t *cr, int32_t width, int32_t height, int32_t pointerx, int32_t pointery)
{
	dt_capture_t *lib=(dt_capture_t*)self->data;
	lib->image_over = DT_VIEW_DESERT;
	int32_t mouse_over_id;
	DT_CTL_GET_GLOBAL(mouse_over_id, lib_image_mouse_over_id);
	lib->image_id=dt_view_film_strip_get_active_image(darktable.view_manager);
	
	// First of all draw image if availble
	if( lib->image_id >= 0 )
	{
		dt_image_t *image = dt_image_cache_get(lib->image_id, 'r');
		if( image )
		{
			const float wd = width/1.0;
			cairo_translate(cr,0.0f, TOP_MARGIN);
			dt_view_image_expose(image, &(lib->image_over), lib->image_id, cr, wd, height-TOP_MARGIN-BOTTOM_MARGIN, 1, pointerx, pointery);
			cairo_translate(cr,0.0f, -BOTTOM_MARGIN);
			dt_image_cache_release(image, 'r');
		}
	}
}


void expose(dt_view_t *self, cairo_t *cri, int32_t width_i, int32_t height_i, int32_t pointerx, int32_t pointery)
{
	dt_capture_t *lib = (dt_capture_t *)self->data;

	int32_t width  = MIN(width_i,  DT_IMAGE_WINDOW_SIZE);
	int32_t height = MIN(height_i, DT_IMAGE_WINDOW_SIZE);

	cairo_set_source_rgb (cri, .2, .2, .2);
	cairo_rectangle(cri, 0, 0, width_i, height_i);
	cairo_fill (cri);
 
	
	if(width_i  > DT_IMAGE_WINDOW_SIZE) cairo_translate(cri, -(DT_IMAGE_WINDOW_SIZE-width_i) *.5f, 0.0f);
	if(height_i > DT_IMAGE_WINDOW_SIZE) cairo_translate(cri, 0.0f, -(DT_IMAGE_WINDOW_SIZE-height_i)*.5f);
	
	// Mode dependent expose of center view
	switch(lib->mode)
	{
		case DT_CAPTURE_MODE_TETHERED: // tethered mode
		default: 
			_expose_tethered_mode(self, cri, width, height, pointerx, pointery);
			break;
	}
	
	// post expose to modules
	GList *modules = darktable.lib->plugins;
	
	while(modules)
	{
		dt_lib_module_t *module = (dt_lib_module_t *)(modules->data);
		if( (module->views() & DT_CAPTURE_VIEW) && module->gui_post_expose )
			module->gui_post_expose(module,cri,width,height,pointerx,pointery);
		modules = g_list_next(modules);
	}
	
}

void enter(dt_view_t *self)
{
	dt_capture_t *lib = (dt_capture_t *)self->data;

	lib->mode = dt_conf_get_int("plugins/capture/mode");

	// add expanders
	GtkBox *box = GTK_BOX(glade_xml_get_widget (darktable.gui->main_window, "plugins_vbox"));
	
	// adjust gui:
	GtkWidget *widget;
	widget = glade_xml_get_widget (darktable.gui->main_window, "histogram_expander");
	gtk_widget_set_visible(widget, FALSE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "devices_expander");
	gtk_widget_set_visible(widget, FALSE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "library_eventbox");
	gtk_widget_set_visible(widget, FALSE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "module_list_eventbox");
	gtk_widget_set_visible(widget, FALSE);
	 
	gtk_widget_set_visible(glade_xml_get_widget (darktable.gui->main_window, "modulegroups_eventbox"), FALSE);
	 
	GList *modules = g_list_last(darktable.lib->plugins);
	while(modules!=darktable.lib->plugins)
	{
		dt_lib_module_t *module = (dt_lib_module_t *)(modules->data);
		if( module->views() & DT_CAPTURE_VIEW )
		{ 
			// Module does support this view let's add it to plugin box
			// soo here goes the special cases for capture view
			if( !( strcmp(module->name(),"tethered shoot")==0 && lib->mode != DT_CAPTURE_MODE_TETHERED ) )
			{
				module->gui_init(module);
				// add the widget created by gui_init to an expander and both to list.
				GtkWidget *expander = dt_lib_gui_get_expander(module);
				gtk_box_pack_start(box, expander, FALSE, FALSE, 0);
			}
		}
		modules = g_list_previous(modules);
	}

	// end marker widget:
	GtkWidget *endmarker = gtk_drawing_area_new();
	gtk_box_pack_start(box, endmarker, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (endmarker), "expose-event",
										G_CALLBACK (dt_control_expose_endmarker), 0);
	g_signal_connect (G_OBJECT (endmarker), "size-allocate",
                    G_CALLBACK (dt_control_size_allocate_endmarker), self);
	
	gtk_widget_show_all(GTK_WIDGET(box));
	 // close expanders
	modules = darktable.lib->plugins;
	while(modules)
	{
		dt_lib_module_t *module = (dt_lib_module_t *)(modules->data);
		if( module->views() & DT_CAPTURE_VIEW )
		{ 
			char var[1024];
			snprintf(var, 1024, "plugins/capture/%s/expanded", module->plugin_name);
			gboolean expanded = dt_conf_get_bool(var);
			gtk_expander_set_expanded (module->expander, expanded);
			if(expanded) gtk_widget_show_all(module->widget);
			else         gtk_widget_hide_all(module->widget);
		}
		modules = g_list_next(modules);
	}
	
	// Check if we should enable view of the filmstrip
	if(dt_conf_get_bool("plugins/filmstrip/on"))
	{
		dt_view_film_strip_scroll_to(darktable.view_manager, lib->image_id);
		dt_view_film_strip_open(darktable.view_manager, film_strip_activated, self);
		dt_view_film_strip_prefetch();
	}
	
	// Setup key accelerators in capture view...	
	dt_gui_key_accel_register(GDK_CONTROL_MASK, GDK_f, film_strip_key_accel, self);

	// initialize a default session...
	dt_capture_view_set_jobcode(self, dt_conf_get_string("plugins/capture/jobcode"));
	
	
}

void dt_lib_remove_child(GtkWidget *widget, gpointer data)
{
  gtk_container_remove(GTK_CONTAINER(data), widget);
}

void leave(dt_view_t *self)
{
	dt_capture_t *cv = (dt_capture_t *)self->data;
	if(dt_conf_get_bool("plugins/filmstrip/on"))
		dt_view_film_strip_close(darktable.view_manager);
	
	if( dt_film_is_empty(cv->film->id) != 0)
		dt_film_remove(cv->film->id );
	
	dt_gui_key_accel_unregister(film_strip_key_accel);
	
	// Restore user interface
	GtkWidget *widget;
	widget = glade_xml_get_widget (darktable.gui->main_window, "devices_expander");
	gtk_widget_set_visible(widget, TRUE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "navigation_expander");
	gtk_widget_set_visible(widget, FALSE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "histogram_expander");
	gtk_widget_set_visible(widget, FALSE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "snapshots_eventbox");
	gtk_widget_set_visible(widget, FALSE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "history_eventbox");
	gtk_widget_set_visible(widget, FALSE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "library_eventbox");
	gtk_widget_set_visible(widget, TRUE);
	widget = glade_xml_get_widget (darktable.gui->main_window, "module_list_eventbox");
	gtk_widget_set_visible(widget, FALSE);
	
	// cleanup the DT_CAPTURE_VIEW modules
	GList *it = darktable.lib->plugins;
	while(it)
	{
		dt_lib_module_t *module = (dt_lib_module_t *)(it->data);
		if( (module->views() & DT_CAPTURE_VIEW) )
			module->gui_cleanup(module);
		it = g_list_next(it);
	}
	GtkBox *box = GTK_BOX(glade_xml_get_widget (darktable.gui->main_window, "plugins_vbox"));
	gtk_container_foreach(GTK_CONTAINER(box), (GtkCallback)dt_lib_remove_child, (gpointer)box);
	
}

void reset(dt_view_t *self)
{
	dt_capture_t *lib = (dt_capture_t *)self->data;
	lib->mode=DT_CAPTURE_MODE_TETHERED;
	//DT_CTL_SET_GLOBAL(lib_image_mouse_over_id, -1);
}


void mouse_leave(dt_view_t *self)
{
	/*dt_library_t *lib = (dt_library_t *)self->data;
	if(!lib->pan && dt_conf_get_int("plugins/lighttable/images_in_row") != 1)
	{
		DT_CTL_SET_GLOBAL(lib_image_mouse_over_id, -1);
		dt_control_queue_draw_all(); // remove focus
	}*/
}


void mouse_moved(dt_view_t *self, double x, double y, int which)
{
	// update stars/etc :(
	//dt_control_queue_draw_all();
}


int button_released(dt_view_t *self, double x, double y, int which, uint32_t state)
{
	/*dt_library_t *lib = (dt_library_t *)self->data;
	lib->pan = 0;
	if(which == 1) dt_control_change_cursor(GDK_LEFT_PTR);*/
	return 1;
}


int button_pressed(dt_view_t *self, double x, double y, int which, int type, uint32_t state)
{
	
	return 1;
}


int key_pressed(dt_view_t *self, uint16_t which)
{

	return 1;
}

void border_scrolled(dt_view_t *view, double x, double y, int which, int up)
{
	/*dt_library_t *lib = (dt_library_t *)view->data;
		if(up) lib->track = -1;
		else   lib->track =  1;
	*/
	dt_control_queue_draw_all();
}

void scrolled(dt_view_t *view, double x, double y, int up)
{
	/*
	if(up) lib->track = -DT_LIBRARY_MAX_ZOOM;
	else   lib->track =  DT_LIBRARY_MAX_ZOOM;
	*/
}

