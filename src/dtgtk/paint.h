/*
    This file is part of darktable,
    copyright (c) 2010 Henrik Andersson.

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
#ifndef DTGTK_PAINT_H
#define DTGTK_PAINT_H

#include <gtk/gtk.h>
#include <cairo.h>

#define CPF_USER_DATA	0x1000	

typedef enum dtgtk_cairo_paint_flags_t {
	CPF_DIRECTION_UP=1,
	CPF_DIRECTION_DOWN=2,
	CPF_DIRECTION_LEFT=4,
	CPF_DIRECTION_RIGHT=8,
	CPF_ACTIVE=16,
	CPF_PRELIGHT=32,
	CPF_IGNORE_FG_STATE=64,	// Ignore state when setting foregroundcolor 
	CPF_BG_TRANSPARENT=128,	// Transparent background..
	CPF_STYLE_FLAT=256,		// flat styled widget
	CPF_STYLE_BOX=512,		// boxed styled widget (as standard gtk buttons)
	CPF_DO_NOT_USE_BORDER=1024,	// do not use inner border for painting 
	CPF_SPECIAL_FLAG=2048	// Special flag
	
} dtgtk_cairo_paint_flags_t;


typedef void (*DTGTKCairoPaintIconFunc)(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);

/** Paint a arrow left or right */
void dtgtk_cairo_paint_arrow(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a store icon */
void dtgtk_cairo_paint_store(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a reset icon */
void dtgtk_cairo_paint_reset(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a reset icon */
void dtgtk_cairo_paint_presets(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a flip icon */
void dtgtk_cairo_paint_flip(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a switch icon */
void dtgtk_cairo_paint_switch(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a color rect icon */
void dtgtk_cairo_paint_color(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a eye icon */
void dtgtk_cairo_paint_eye(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a timer icon */
void dtgtk_cairo_paint_timer(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a filmstrip icon */
void dtgtk_cairo_paint_filmstrip(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a directory icon */
void dtgtk_cairo_paint_directory(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a refresh/reload icon */
void dtgtk_cairo_paint_refresh(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a cancel X icon */
void dtgtk_cairo_paint_cancel(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** paint two boxes indicating portrait/landscape flip */
void dtgtk_cairo_paint_aspectflip(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a color label icon */
void dtgtk_cairo_paint_label(cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a star used for ratings */
void dtgtk_cairo_paint_star (cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
/** Paint a window used as header */
void dtgtk_cairo_paint_header (cairo_t *cr,gint x,gint y,gint w,gint h,gint flags);
#endif
