/*
    This file is part of darktable,
    copyright (c) 2009--2010 Henrik Andersson.

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

#include <glade/glade.h>

#include "gui/gtk.h"
#include "common/darktable.h"
#include "develop/develop.h"
#include "control/control.h"
#include "gui/iop_history.h"

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
  
  GtkWidget *hbody =  glade_xml_get_widget (darktable.gui->main_window, "history_expander_body");
  GtkWidget *hbox = g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hbody)), 0);
  reset = 1;
  
  /* inactivate all toggle buttons */
  GList *children = gtk_container_get_children (GTK_CONTAINER (hbox));
  for(int i=0;i<g_list_length (children);i++)
  {
    GtkToggleButton *b = GTK_TOGGLE_BUTTON( g_list_nth_data (children,i));
    if(b != GTK_TOGGLE_BUTTON(widget)) 
      gtk_object_set(GTK_OBJECT(b), "active", FALSE, NULL);
  }
  
  reset = 0;
  if(darktable.gui->reset) return;
  
  /* revert to given history item. */
  long int num = (long int)user_data;
  dt_dev_pop_history_items (darktable.develop, num);
}


void 
dt_gui_iop_history_init ()
{
  GtkWidget *hbody =  glade_xml_get_widget (darktable.gui->main_window, "history_expander_body");
  GtkWidget *hvbox = gtk_vbox_new (FALSE,0);
  GtkWidget *hbutton = gtk_button_new_with_label (_("compress history stack"));  
  g_object_set (G_OBJECT (hbutton), "tooltip-text", _("create a minimal history stack which produces the same image"),NULL);
  gtk_box_pack_start (GTK_BOX (hbody),hvbox,FALSE,FALSE,0);
  gtk_box_pack_start (GTK_BOX (hbody),hbutton,FALSE,FALSE,0);
  g_signal_connect (G_OBJECT (hbutton), "clicked", G_CALLBACK (history_compress_clicked),(gpointer)0);
  gtk_widget_show_all (hbody);
}

void 
dt_gui_iop_history_reset ()
{
  GtkWidget *hbody =  glade_xml_get_widget (darktable.gui->main_window, "history_expander_body");
  GtkWidget *hvbox =  g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hbody)), 0);
  
  /* clear from history items */
  gtk_container_foreach (GTK_CONTAINER (hvbox),(GtkCallback)gtk_widget_destroy,NULL);
  
  /* add default history entry */
  GtkWidget *b=dt_gui_iop_history_add_item (-1, "orginal");
  gtk_button_set_label (GTK_BUTTON (b), _("0 - original"));
}

GtkWidget *
dt_gui_iop_history_add_item (long int num, const gchar *label)
{
  GtkWidget *hbody =  glade_xml_get_widget (darktable.gui->main_window, "history_expander_body");
  GtkWidget *hvbox = g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hbody)), 0);
 
  GList *items = gtk_container_get_children (GTK_CONTAINER (hvbox));
  
  /* let's check if top item has same label as this hist change label */
  if( g_list_nth_data (items,0) && strcmp (gtk_button_get_label ( GTK_BUTTON (g_list_nth_data (items,0))),label) ==0 ) 
    return GTK_WIDGET(g_list_nth_data (items,0));
  
  /* add a new */
  num++;
  
  /* create label */
  GtkWidget *widget = NULL;
  gchar numlabel[256];
  g_snprintf(numlabel, 256, "%ld - %s", num, label);
  
  /* create toggle button */
  widget =  dtgtk_togglebutton_new_with_label (numlabel,NULL,CPF_STYLE_FLAT);
  g_object_set_data (G_OBJECT (widget),"history_number",(gpointer)num);
  g_object_set_data (G_OBJECT (widget),"label",(gpointer) g_strdup(label));
  
  g_signal_connect (G_OBJECT (widget), "clicked",
                      G_CALLBACK (history_button_clicked),
                      (gpointer)num);
  
  gtk_box_pack_start (GTK_BOX (hvbox),widget,FALSE,FALSE,0);
  gtk_box_reorder_child (GTK_BOX (hvbox),widget,0);
  gtk_widget_show(widget);
  
  /* */
  darktable.gui->reset = 1;
  gtk_object_set(GTK_OBJECT(widget), "active", TRUE, NULL);
  darktable.gui->reset = 0;
  return widget;
}

long int
dt_gui_iop_history_get_top() 
{
  GtkWidget *hbody =  glade_xml_get_widget (darktable.gui->main_window, "history_expander_body");
  GtkWidget *hvbox = g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hbody)), 0);
  
  GtkWidget *th = g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hvbox)), 0);
  return (long int)g_object_get_data (G_OBJECT (th),"history_number");
}

void 
dt_gui_iop_history_pop_top()
{
  GtkWidget *hbody =  glade_xml_get_widget (darktable.gui->main_window, "history_expander_body");
  GtkWidget *hvbox = g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hbody)), 0);
  
  /* remove top */
  gtk_widget_destroy (GTK_WIDGET (g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hvbox)), 0)) );
  
  /* activate new top */
  gtk_object_set(GTK_OBJECT (g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hvbox)), 0)) , "active", TRUE, NULL);
}
  
void 
dt_gui_iop_history_update_labels ()
{
  GtkWidget *hbody =  glade_xml_get_widget (darktable.gui->main_window, "history_expander_body");
  GtkWidget *hvbox = g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (hbody)), 0);
  GList *items = gtk_container_get_children (GTK_CONTAINER (hvbox));
  
  /* update labels for all hist items excluding oringal */
  int hsize = g_list_length(darktable.develop->history);
  for(int i=0;i<hsize;i++) {
    gchar numlabel[256]={0}, numlabel2[256]={0};
    dt_dev_history_item_t *hist = (dt_dev_history_item_t *)g_list_nth_data (darktable.develop->history, i);
    if( !hist ) break;

    /* get new label */
    dt_dev_get_history_item_label (hist, numlabel2, 256);
    snprintf(numlabel, 256, "%d - %s", i+1, numlabel2);
 
    /* update ui hist item label from bottom to top */
    GtkWidget *button=g_list_nth_data (items,(hsize-1)-i);
    gtk_button_set_label (GTK_BUTTON (button),numlabel);
  }
}
