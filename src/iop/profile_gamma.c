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
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#ifdef HAVE_GEGL
  #include <gegl.h>
#endif
#include "iop/profile_gamma.h"
#include "develop/develop.h"
#include "control/control.h"
#include "gui/gtk.h"

DT_MODULE(1)

const char *name()
{
  return _("unbreak input profile");
}

int
groups ()
{
  return IOP_GROUP_COLOR;
}

void process (struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, void *i, void *o, const dt_iop_roi_t *roi_in, const dt_iop_roi_t *roi_out)
{
  dt_iop_profile_gamma_data_t *d = (dt_iop_profile_gamma_data_t *)piece->data;
  float *in = (float *)i;
  float *out = (float *)o;
  for(int k=0;k<roi_out->width*roi_out->height;k++)
  {
    out[0] = d->table[CLAMP((int)(in[0]*0x10000ul), 0, 0xffff)];
    out[1] = d->table[CLAMP((int)(in[1]*0x10000ul), 0, 0xffff)];
    out[2] = d->table[CLAMP((int)(in[2]*0x10000ul), 0, 0xffff)];
    in += 3; out += 3;
  }
}

void commit_params (struct dt_iop_module_t *self, dt_iop_params_t *p1, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  dt_iop_profile_gamma_params_t *p = (dt_iop_profile_gamma_params_t *)p1;
#ifdef HAVE_GEGL
  // pull in new params to gegl
  gegl_node_set(piece->input, "linear_value", p->linear, "gamma_value", p->gamma, NULL);
  // gegl_node_set(piece->input, "value", p->gamma, NULL);
#else
  // build gamma table in pipeline piece from committed params:
  dt_iop_profile_gamma_data_t *d = (dt_iop_profile_gamma_data_t *)piece->data;
  float a, b, c, g;
  if(p->gamma == 1.0)
  {
    for(int k=0;k<0x10000;k++) d->table[k] = 1.0*k/0x10000;
  }
  else 
  {
    if(p->linear == 0.0)
    {
      for(int k=0;k<0x10000;k++)
        d->table[k] = powf(1.00*k/0x10000, p->gamma);
    }
    else
    {
      if(p->linear<1.0)
      {
        g = p->gamma*(1.0-p->linear)/(1.0-p->gamma*p->linear);
        a = 1.0/(1.0+p->linear*(g-1));
        b = p->linear*(g-1)*a;
        c = powf(a*p->linear+b, g)/p->linear;
      }
      else
      {
        a = b = g = 0.0;
        c = 1.0;
      }
      for(int k=0;k<0x10000;k++)
      {
        float tmp;
        if (k<0x10000*p->linear) tmp = c*k/0x10000;
        else tmp = powf(a*k/0x10000+b, g);
        d->table[k] = tmp;
      }
    }
  }
#endif
}

void init_pipe (struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
#ifdef HAVE_GEGL
  // create part of the gegl pipeline
  piece->data = NULL;
  dt_iop_profile_gamma_params_t *default_params = (dt_iop_profile_gamma_params_t *)self->default_params;
  piece->input = piece->output = gegl_node_new_child(pipe->gegl, "operation", "gegl:dt-profile_gamma", "linear_value", default_params->linear, "gamma_value", default_params->gamma, NULL);
  // piece->input = piece->output = gegl_node_new_child(pipe->gegl, "operation", "gegl:gamma", "value", default_params->gamma, NULL);
#else
  piece->data = malloc(sizeof(dt_iop_profile_gamma_data_t));
  self->commit_params(self, self->default_params, pipe, piece);
#endif
}

void cleanup_pipe (struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
#ifdef HAVE_GEGL
  // clean up everything again.
  (void)gegl_node_remove_child(pipe->gegl, piece->input);
  // no free necessary, no data is alloc'ed
#else
  free(piece->data);
#endif
}

void gui_update(struct dt_iop_module_t *self)
{
  dt_iop_module_t *module = (dt_iop_module_t *)self;
  dt_iop_profile_gamma_gui_data_t *g = (dt_iop_profile_gamma_gui_data_t *)self->gui_data;
  dt_iop_profile_gamma_params_t *p = (dt_iop_profile_gamma_params_t *)module->params;
  dtgtk_slider_set_value(g->scale1, p->linear);
  dtgtk_slider_set_value(g->scale2, p->gamma);
}

void init(dt_iop_module_t *module)
{
  // module->data = malloc(sizeof(dt_iop_profile_gamma_data_t));
  module->params = malloc(sizeof(dt_iop_profile_gamma_params_t));
  module->default_params = malloc(sizeof(dt_iop_profile_gamma_params_t));
  module->default_enabled = 0;
  module->params_size = sizeof(dt_iop_profile_gamma_params_t);
  module->gui_data = NULL;
  module->priority = 299; // colorin module->priority - 1
  dt_iop_profile_gamma_params_t tmp = (dt_iop_profile_gamma_params_t){1.0, 1.0};
  memcpy(module->params, &tmp, sizeof(dt_iop_profile_gamma_params_t));
  memcpy(module->default_params, &tmp, sizeof(dt_iop_profile_gamma_params_t));
}

void cleanup(dt_iop_module_t *module)
{
  free(module->gui_data);
  module->gui_data = NULL;
  free(module->params);
  module->params = NULL;
}

void gui_init(struct dt_iop_module_t *self)
{
  self->gui_data = malloc(sizeof(dt_iop_profile_gamma_gui_data_t));
  dt_iop_profile_gamma_gui_data_t *g = (dt_iop_profile_gamma_gui_data_t *)self->gui_data;
  dt_iop_profile_gamma_params_t *p = (dt_iop_profile_gamma_params_t *)self->params;

  self->widget = GTK_WIDGET(gtk_hbox_new(FALSE, 0));
  g->vbox1 = GTK_VBOX(gtk_vbox_new(FALSE, DT_GUI_IOP_MODULE_CONTROL_SPACING));
  g->vbox2 = GTK_VBOX(gtk_vbox_new(FALSE, DT_GUI_IOP_MODULE_CONTROL_SPACING));
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(g->vbox1), FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(g->vbox2), TRUE, TRUE, 5);
  g->label1 = GTK_LABEL(gtk_label_new(C_("gammaslider", "linear")));
  g->label2 = GTK_LABEL(gtk_label_new(C_("gammaslider", "gamma")));
  gtk_misc_set_alignment(GTK_MISC(g->label1), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(g->label2), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(g->vbox1), GTK_WIDGET(g->label1), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(g->vbox1), GTK_WIDGET(g->label2), TRUE, TRUE, 0);
  g->scale1 = DTGTK_SLIDER(dtgtk_slider_new_with_range(DARKTABLE_SLIDER_BAR,0.0, 1.0, 0.0001,p->linear,4));
  g->scale2 = DTGTK_SLIDER(dtgtk_slider_new_with_range(DARKTABLE_SLIDER_BAR,0.0, 1.0, 0.0001,p->gamma,4));
  gtk_box_pack_start(GTK_BOX(g->vbox2), GTK_WIDGET(g->scale1), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(g->vbox2), GTK_WIDGET(g->scale2), TRUE, TRUE, 0);

  g_signal_connect (G_OBJECT (g->scale1), "value-changed",
      G_CALLBACK (linear_callback), self);
  g_signal_connect (G_OBJECT (g->scale2), "value-changed",
      G_CALLBACK (gamma_callback), self);
}

void gui_cleanup(struct dt_iop_module_t *self)
{
  free(self->gui_data);
  self->gui_data = NULL;
}

void gamma_callback (GtkDarktableSlider *slider, gpointer user_data)
{
  dt_iop_module_t *self = (dt_iop_module_t *)user_data;
  if(self->dt->gui->reset) return;
  dt_iop_profile_gamma_params_t *p = (dt_iop_profile_gamma_params_t *)self->params;
  p->gamma = dtgtk_slider_get_value(slider);
  dt_dev_add_history_item(darktable.develop, self);
}

void linear_callback (GtkDarktableSlider *slider, gpointer user_data)
{
  dt_iop_module_t *self = (dt_iop_module_t *)user_data;
  if(self->dt->gui->reset) return;
  dt_iop_profile_gamma_params_t *p = (dt_iop_profile_gamma_params_t *)self->params;
  p->linear = dtgtk_slider_get_value(slider);
  dt_dev_add_history_item(darktable.develop, self);
}
