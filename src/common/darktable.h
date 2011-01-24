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
#ifndef DARKTABLE_H
#define DARKTABLE_H

#ifndef _XOPEN_SOURCE
  #define _XOPEN_SOURCE 600 // for localtime_r
#endif
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif
#include "common/dtpthread.h"
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sqlite3.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <math.h>
#ifdef _OPENMP
  #include <omp.h>
#endif

#define DT_MODULE_VERSION 3   // version of dt's module interface
#define DT_VERSION 36         // version of dt's database tables
#define DT_CONFIG_VERSION 34  // dt gconf var version

// every module has to define this:
#ifdef _DEBUG
#define DT_MODULE(MODVER) \
int dt_module_dt_version() \
{\
  return -DT_MODULE_VERSION; \
}\
int dt_module_mod_version() \
{\
  return MODVER; \
}
#else
#define DT_MODULE(MODVER) \
int dt_module_dt_version() \
{\
  return DT_MODULE_VERSION; \
}\
int dt_module_mod_version() \
{\
  return MODVER; \
}
#endif

// ..to be able to compare it against this:
static inline int dt_version()
{
#ifdef _DEBUG
  return -DT_MODULE_VERSION;
#else
  return DT_MODULE_VERSION;
#endif
}

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// NaN-safe clamping (NaN compares false, and will thus result in H)
#define CLAMPS(A, L, H) ((A) > (L) ? ((A) < (H) ? (A) : (H)) : (L))

struct dt_gui_gtk_t;
struct dt_control_t;
struct dt_develop_t;
struct dt_mipmap_cache_t;
struct dt_image_cache_t;
struct dt_lib_t;
struct dt_conf_t;
struct dt_points_t;
struct dt_imageio_t;

typedef enum dt_debug_thread_t
{ // powers of two, masking
  DT_DEBUG_CACHE = 1,
  DT_DEBUG_CONTROL = 2,
  DT_DEBUG_DEV = 4,
  DT_DEBUG_FSWATCH = 8,
  DT_DEBUG_PERF = 16,
  DT_DEBUG_CAMCTL = 32,
  DT_DEBUG_PWSTORAGE = 64,
  DT_DEBUG_OPENCL = 128
}
dt_debug_thread_t;

typedef struct darktable_t
{
  int32_t thumbnail_size;
  int32_t unmuted;
  GList                          *iop;
  GList                          *collection_listeners;
  struct dt_conf_t               *conf;
  struct dt_develop_t            *develop;
  struct dt_lib_t                *lib;
  struct dt_view_manager_t       *view_manager;
  struct dt_control_t            *control;
  struct dt_gui_gtk_t            *gui;
  struct dt_mipmap_cache_t       *mipmap_cache;
  struct dt_image_cache_t        *image_cache;
  sqlite3                        *db;
  const struct dt_fswatch_t	     *fswatch;
  const struct dt_pwstorage_t    *pwstorage;
  const struct dt_camctl_t       *camctl;
  const struct dt_collection_t   *collection;
  struct dt_points_t             *points;
  struct dt_imageio_t            *imageio;
  struct dt_opencl_t             *opencl;
  dt_pthread_mutex_t db_insert;
  dt_pthread_mutex_t plugin_threadsafe;
  char *progname;
}
darktable_t;

extern darktable_t darktable;
extern const char dt_supported_extensions[];

int dt_init(int argc, char *argv[]);
void dt_cleanup();
void dt_print(dt_debug_thread_t thread, const char *msg, ...);
void dt_gettime_t(char *datetime, time_t t);
void dt_gettime(char *datetime);
void *dt_alloc_align(size_t alignment, size_t size);
void dt_get_datadir(char *datadir, size_t bufsize);
void dt_get_plugindir(char *datadir, size_t bufsize);
/** get the user directory of darktable, ~/.config/darktable */
void dt_get_user_config_dir(char *data, size_t bufsize);
/** get the user directory of darktable, ~/.cache/darktable */
void dt_get_user_cache_dir(char *data, size_t bufsize);
/** get the user local directory , ~/.local */
void dt_get_user_local_dir(char *data, size_t bufsize);

static inline double dt_get_wtime()
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return time.tv_sec - 1290608000 + (1.0/1000000.0)*time.tv_usec;
}

static inline int dt_get_num_threads()
{
#ifdef _OPENMP
  return omp_get_num_procs();
#else
  return 1;
#endif
}

static inline int dt_get_thread_num()
{
#ifdef _OPENMP
  return omp_get_thread_num();
#else
  return 0;
#endif
}

static inline float dt_log2f(const float f)
{
#ifdef __GLIBC__
  return log2f(f);
#else
  return logf(f)/logf(2.0f);
#endif
}

static inline float dt_fast_expf(const float x)
{
  // meant for the range [-100.0f, 0.0f]. largest error ~ -0.06 at 0.0f.
  // will get _a_lot_ worse for x > 0.0f (9000 at 10.0f)..
  const int i1 = 0x3f800000u;
  // e^x, the comment would be 2^x
  const int i2 = 0x402DF854u;//0x40000000u;
  // const int k = CLAMPS(i1 + x * (i2 - i1), 0x0u, 0x7fffffffu);
  // without max clamping (doesn't work for large x, but is faster:
  const int k0 = i1 + x * (i2 - i1);
  const int k = k0 > 0 ? k0 : 0;
  const float f = *(const float *)&k;
  return f;
}

#endif
