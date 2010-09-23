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
#include "develop/develop.h"
#include "develop/imageop.h"
#include "control/control.h"
#include "gui/gtk.h"
#include "dtgtk/slider.h"
#include <gtk/gtk.h>
#include <inttypes.h>

DT_MODULE(1)

typedef enum dt_iop_highlights_mode_t
{
  DT_IOP_HIGHLIGHTS_CLIP = 0,
  DT_IOP_HIGHLIGHTS_LCH = 1
}
dt_iop_highlights_mode_t;

typedef struct dt_iop_highlights_params_t
{
  dt_iop_highlights_mode_t mode;
  float blendL, blendC, blendh;
}
dt_iop_highlights_params_t;

typedef struct dt_iop_highlights_gui_data_t
{
  GtkDarktableSlider *blendL;
  GtkDarktableSlider *blendC;
  GtkDarktableSlider *blendh;
  GtkComboBox        *mode;
}
dt_iop_highlights_gui_data_t;

typedef struct dt_iop_highlights_data_t
{
  dt_iop_highlights_mode_t mode;
  float blendL, blendC, blendh;
}
dt_iop_highlights_data_t;

const char *name()
{
  return _("highlight reconstruction");
}

int 
groups () 
{
	return IOP_GROUP_BASIC;
}



static const float xyz_rgb[3][3] = {  /* XYZ from RGB */
  { 0.412453, 0.357580, 0.180423 },
  { 0.212671, 0.715160, 0.072169 },
  { 0.019334, 0.119193, 0.950227 }};
static const float rgb_xyz[3][3] = {  /* RGB from XYZ */
  { 3.24048, -1.53715, -0.498536 },
  { -0.969255, 1.87599, 0.0415559 },
  { 0.0556466, -0.204041, 1.05731 }};

// convert linear RGB to CIE-LCh
static void
rgb_to_lch(float rgb[3], float lch[3])
{
  float xyz[3], lab[3];
  xyz[0] = xyz[1] = xyz[2] = 0.0;
  for (int c=0; c<3; c++)
    for (int cc=0; cc<3; cc++)
      xyz[cc] += xyz_rgb[cc][c] * rgb[c];
  for (int c=0; c<3; c++)
    xyz[c] = xyz[c] > 0.008856 ? powf(xyz[c], 1/3.0) : 7.787*xyz[c] + 16/116.0;
  lab[0] = 116 * xyz[1] - 16;
  lab[1] = 500 * (xyz[0] - xyz[1]);
  lab[2] = 200 * (xyz[1] - xyz[2]);

  lch[0] = lab[0];
  lch[1] = sqrtf(lab[1]*lab[1]+lab[2]*lab[2]);
  lch[2] = atan2f(lab[2], lab[1]);
}

// convert CIE-LCh to linear RGB
static void
lch_to_rgb(float lch[3], float rgb[3])
{
  float xyz[3], fx, fy, fz, kappa, epsilon, tmpf, lab[3];
  epsilon = 0.008856; kappa = 903.3;
  lab[0] = lch[0];
  lab[1] = lch[1] * cosf(lch[2]);
  lab[2] = lch[1] * sinf(lch[2]);
  xyz[1] = (lab[0]<=kappa*epsilon) ?
    (lab[0]/kappa) : (powf((lab[0]+16.0)/116.0, 3.0));
  fy = (xyz[1]<=epsilon) ? ((kappa*xyz[1]+16.0)/116.0) : ((lab[0]+16.0)/116.0);
  fz = fy - lab[2]/200.0;
  fx = lab[1]/500.0 + fy;
  xyz[2] = (powf(fz, 3.0)<=epsilon) ? ((116.0*fz-16.0)/kappa) : (powf(fz, 3.0));
  xyz[0] = (powf(fx, 3.0)<=epsilon) ? ((116.0*fx-16.0)/kappa) : (powf(fx, 3.0));

  for (int c=0; c<3; c++)
  {
    tmpf = 0;
    for (int cc=0; cc<3; cc++)
      tmpf += rgb_xyz[c][cc] * xyz[cc];
    rgb[c] = MAX(tmpf, 0);
  }
}


void process (struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, void *ivoid, void *ovoid, const dt_iop_roi_t *roi_in, const dt_iop_roi_t *roi_out)
{
  dt_iop_highlights_data_t *data = (dt_iop_highlights_data_t *)piece->data;
  float *in  = (float *)ivoid;
  float *out = (float *)ovoid;

  const float clip = self->dev->image->flags & DT_IMAGE_THUMBNAIL ? 1.0 :
    fminf(piece->pipe->processed_maximum[0], fminf(piece->pipe->processed_maximum[1], piece->pipe->processed_maximum[2]));
  float inc[3], lch[3], lchc[3], lchi[3];

  switch(data->mode)
  {
    case DT_IOP_HIGHLIGHTS_LCH:
#ifdef _OPENMP
  #pragma omp parallel for schedule(dynamic) default(none) shared(ovoid, ivoid, roi_out, data, piece) private(in, out, inc, lch, lchc, lchi)
#endif
      for(int j=0;j<roi_out->height;j++)
      {
        out = (float *)ovoid + 3*roi_out->width*j;
        in  = (float *)ivoid + 3*roi_out->width*j;
        for(int i=0;i<roi_out->width;i++)
        {
          if(in[0] <= piece->pipe->processed_maximum[0] &&
             in[1] <= piece->pipe->processed_maximum[1] &&
             in[2] <= piece->pipe->processed_maximum[2]) 
          { // fast path for well-exposed pixels.
            for(int c=0;c<3;c++) out[c] = in[c];
          }
          else
          {
            for(int c=0;c<3;c++) inc[c] = fminf(clip, in[c]);
            rgb_to_lch(in, lchi);
            rgb_to_lch(inc, lchc);
            lch[0] = lchc[0] + data->blendL * (lchi[0] - lchc[0]);
            lch[1] = lchc[1] + data->blendC * (lchi[1] - lchc[1]);
            lch[2] = lchc[2] + data->blendh * (lchi[2] - lchc[2]);
            lch_to_rgb(lch, out);
          }
          out += 3; in += 3;
        }
      }
      break;
    default: case DT_IOP_HIGHLIGHTS_CLIP:
#ifdef _OPENMP
  #pragma omp parallel for schedule(dynamic) default(none) shared(ovoid, ivoid, roi_out) private(in, out, inc, lch, lchc, lchi)
#endif
      for(int j=0;j<roi_out->height;j++)
      {
        out = (float *)ovoid + 3*roi_out->width*j;
        in  = (float *)ivoid + 3*roi_out->width*j;
        for(int i=0;i<roi_out->width;i++)
        {
          for(int c=0;c<3;c++) out[c] = fminf(clip, in[c]);
          out += 3; in += 3;
        }
      }
      break;
  }
}

static void
blend_callback (GtkDarktableSlider *slider, dt_iop_module_t *self)
{
  if(self->dt->gui->reset) return;
  dt_iop_highlights_params_t *p = (dt_iop_highlights_params_t *)self->params;
  dt_iop_highlights_gui_data_t *g = (dt_iop_highlights_gui_data_t *)self->gui_data;
  if      (slider == g->blendL) p->blendL = dtgtk_slider_get_value(slider);
  else if (slider == g->blendC) p->blendC = dtgtk_slider_get_value(slider);
  else if (slider == g->blendh) p->blendh = dtgtk_slider_get_value(slider);
  dt_dev_add_history_item(darktable.develop, self);
}

static void
mode_changed (GtkComboBox *combo, dt_iop_module_t *self)
{
  dt_iop_highlights_params_t *p = (dt_iop_highlights_params_t *)self->params;
  dt_iop_highlights_gui_data_t *g = (dt_iop_highlights_gui_data_t *)self->gui_data;
  int active = gtk_combo_box_get_active(combo);

  switch(active)
  {
    case DT_IOP_HIGHLIGHTS_CLIP:
      p->mode = DT_IOP_HIGHLIGHTS_CLIP;
      gtk_widget_set_sensitive(GTK_WIDGET(g->blendL), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(g->blendC), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(g->blendh), FALSE);
      break;
    default: case DT_IOP_HIGHLIGHTS_LCH:
      p->mode = DT_IOP_HIGHLIGHTS_LCH;
      gtk_widget_set_sensitive(GTK_WIDGET(g->blendL), TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(g->blendC), TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(g->blendh), TRUE);
      break;
  }
  dt_dev_add_history_item(darktable.develop, self);
}

void commit_params (struct dt_iop_module_t *self, dt_iop_params_t *p1, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  dt_iop_highlights_params_t *p = (dt_iop_highlights_params_t *)p1;
#ifdef HAVE_GEGL
  fprintf(stderr, "[sharpen] TODO: implement gegl version!\n");
  // pull in new params to gegl
#else
  dt_iop_highlights_data_t *d = (dt_iop_highlights_data_t *)piece->data;
  d->blendL = p->blendL;
  d->blendC = p->blendC;
  d->blendh = p->blendh;
  d->mode   = p->mode;
#endif
}

void init_pipe (struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
#ifdef HAVE_GEGL
  // create part of the gegl pipeline
  piece->data = NULL;
#else
  piece->data = malloc(sizeof(dt_iop_highlights_data_t));
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
  dt_iop_highlights_gui_data_t *g = (dt_iop_highlights_gui_data_t *)self->gui_data;
  dt_iop_highlights_params_t *p = (dt_iop_highlights_params_t *)module->params;
  dtgtk_slider_set_value(g->blendL, p->blendL);
  dtgtk_slider_set_value(g->blendC, p->blendC);
  dtgtk_slider_set_value(g->blendh, p->blendh);
  if(p->mode == DT_IOP_HIGHLIGHTS_CLIP)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(g->blendL), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(g->blendC), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(g->blendh), FALSE);
  }
  else
  {
    gtk_widget_set_sensitive(GTK_WIDGET(g->blendL), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(g->blendC), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(g->blendh), TRUE);
  }
  gtk_combo_box_set_active(g->mode, p->mode);
}

void init(dt_iop_module_t *module)
{
  // module->data = malloc(sizeof(dt_iop_highlights_data_t));
  module->params = malloc(sizeof(dt_iop_highlights_params_t));
  module->default_params = malloc(sizeof(dt_iop_highlights_params_t));
  if(dt_image_is_ldr(module->dev->image)) module->default_enabled = 0;
  else                                    module->default_enabled = 1;
  module->priority = 250;
  module->params_size = sizeof(dt_iop_highlights_params_t);
  module->gui_data = NULL;
  dt_iop_highlights_params_t tmp = (dt_iop_highlights_params_t){0, 1.0, 0.0, 0.0};
  memcpy(module->params, &tmp, sizeof(dt_iop_highlights_params_t));
  memcpy(module->default_params, &tmp, sizeof(dt_iop_highlights_params_t));
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
  self->gui_data = malloc(sizeof(dt_iop_highlights_gui_data_t));
  dt_iop_highlights_gui_data_t *g = (dt_iop_highlights_gui_data_t *)self->gui_data;
  dt_iop_highlights_params_t *p = (dt_iop_highlights_params_t *)self->params;

  self->widget = GTK_WIDGET(gtk_vbox_new(FALSE, 5));

  GtkBox *hbox  = GTK_BOX(gtk_hbox_new(FALSE, 5));
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(hbox), FALSE, FALSE, 0);
  GtkWidget *label;
  label = gtk_label_new(_("method"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start(hbox, label, FALSE, FALSE, 0);
  g->mode = GTK_COMBO_BOX(gtk_combo_box_new_text());
  gtk_combo_box_append_text(g->mode, _("clip highlights"));
  gtk_combo_box_append_text(g->mode, _("reconstruct in LCh"));
  gtk_object_set(GTK_OBJECT(g->mode), "tooltip-text", _("highlight reconstruction method"), NULL);
  gtk_box_pack_start(hbox, GTK_WIDGET(g->mode), TRUE, TRUE, 0);

  hbox  = GTK_BOX(gtk_hbox_new(FALSE, 5));
  GtkBox *vbox1 = GTK_BOX(gtk_vbox_new(TRUE, DT_GUI_IOP_MODULE_CONTROL_SPACING));
  GtkBox *vbox2 = GTK_BOX(gtk_vbox_new(TRUE, DT_GUI_IOP_MODULE_CONTROL_SPACING));
  gtk_box_pack_start(hbox, GTK_WIDGET(vbox1), FALSE, FALSE, 0);
  gtk_box_pack_start(hbox, GTK_WIDGET(vbox2), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(hbox), FALSE, FALSE, 0);
  label = gtk_label_new(_("blend L"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start(vbox1, label, FALSE, FALSE, 0);
  label = gtk_label_new(_("blend C"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start(vbox1, label, FALSE, FALSE, 0);
  label = gtk_label_new(_("blend h"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start(vbox1, label, FALSE, FALSE, 0);

  g->blendL = DTGTK_SLIDER(dtgtk_slider_new_with_range(DARKTABLE_SLIDER_BAR,0.0, 1.0, 0.01, p->blendL, 3));
  g->blendC = DTGTK_SLIDER(dtgtk_slider_new_with_range(DARKTABLE_SLIDER_BAR,0.0, 1.0, 0.01, p->blendC, 3));
  g->blendh = DTGTK_SLIDER(dtgtk_slider_new_with_range(DARKTABLE_SLIDER_BAR,0.0, 1.0, 0.01, p->blendh, 3));
  gtk_object_set(GTK_OBJECT(g->blendL), "tooltip-text", _("blend lightness (0 is same as clipping)"), NULL);
  gtk_object_set(GTK_OBJECT(g->blendC), "tooltip-text", _("blend colorness (0 is same as clipping)"), NULL);
  gtk_object_set(GTK_OBJECT(g->blendh), "tooltip-text", _("blend hue (0 is same as clipping)"), NULL);
  gtk_box_pack_start(vbox2, GTK_WIDGET(g->blendL), TRUE, TRUE, 0);
  gtk_box_pack_start(vbox2, GTK_WIDGET(g->blendC), TRUE, TRUE, 0);
  gtk_box_pack_start(vbox2, GTK_WIDGET(g->blendh), TRUE, TRUE, 0);

  g_signal_connect (G_OBJECT (g->blendL), "value-changed",
                    G_CALLBACK (blend_callback), self);
  g_signal_connect (G_OBJECT (g->blendC), "value-changed",
                    G_CALLBACK (blend_callback), self);
  g_signal_connect (G_OBJECT (g->blendh), "value-changed",
                    G_CALLBACK (blend_callback), self);
  g_signal_connect (G_OBJECT (g->mode), "changed",
                    G_CALLBACK (mode_changed), self);
}

void gui_cleanup(struct dt_iop_module_t *self)
{
  free(self->gui_data);
  self->gui_data = NULL;
}

