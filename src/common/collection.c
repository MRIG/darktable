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

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>

#include "control/conf.h"
#include "control/control.h"
#include "common/collection.h"
#include "common/debug.h"
#include "common/metadata.h"
#include "common/utility.h"

#define SELECT_QUERY "select distinct * from %s"
#define ORDER_BY_QUERY "order by %s"
#define LIMIT_QUERY "limit ?1, ?2"

#define MAX_QUERY_STRING_LENGTH 4096
/* Stores the collection query, returns 1 if changed.. */
static int _dt_collection_store (const dt_collection_t *collection, gchar *query);

typedef struct dt_collection_listener_t
{
  void (*callback)(void *);
  void *data;
}
dt_collection_listener_t;

const dt_collection_t * 
dt_collection_new (const dt_collection_t *clone)
{
  dt_collection_t *collection = g_malloc (sizeof (dt_collection_t));
  memset (collection,0,sizeof (dt_collection_t));
  
  /* initialize collection context*/
  if (clone)   /* if clone is provided let's copy it into this context */
  {
    memcpy (&collection->params,&clone->params,sizeof (dt_collection_params_t));
    memcpy (&collection->store,&clone->store,sizeof (dt_collection_params_t));
    collection->where_ext = g_strdup(clone->where_ext);
    collection->query = g_strdup(clone->query);
    collection->clone =1;
  }
  else  /* else we just initialize using the reset */
    dt_collection_reset (collection);
    
  return collection;
}

void 
dt_collection_free (const dt_collection_t *collection)
{
  if (collection->query)
    g_free (collection->query);
  if (collection->where_ext)
    g_free (collection->where_ext);
  g_free ((dt_collection_t *)collection);
}

const dt_collection_params_t * 
dt_collection_params (const dt_collection_t *collection)
{
  return &collection->params;
}

int 
dt_collection_update (const dt_collection_t *collection)
{
  uint32_t result;
  gchar sq[512]   = {0};   // sort query
  gchar selq[512] = {0};   // selection query
  gchar wq[2048]  = {0};   // where query
  gchar *query=g_malloc (MAX_QUERY_STRING_LENGTH);

  dt_lib_sort_t sort = dt_conf_get_int ("ui_last/combo_sort");
  
  /* build where part */
  if (!(collection->params.query_flags&COLLECTION_QUERY_USE_ONLY_WHERE_EXT))
  {
    int need_operator = 0;
    
    /* add default filters */
    if (collection->params.filter_flags & COLLECTION_FILTER_FILM_ID)
    {
      g_snprintf (wq,2048,"(film_id = %d)",collection->params.film_id);
      need_operator = 1;
    }
    
    if (collection->params.filter_flags & COLLECTION_FILTER_ATLEAST_RATING)
      g_snprintf (wq+strlen(wq),2048-strlen(wq)," %s (flags & 7) >= %d", (need_operator)?"and":((need_operator=1)?"":"") , collection->params.rating);
    else if (collection->params.filter_flags & COLLECTION_FILTER_EQUAL_RATING)
      g_snprintf (wq+strlen(wq),2048-strlen(wq)," %s (flags & 7) == %d", (need_operator)?"and":((need_operator=1)?"":"") , collection->params.rating);

    if (collection->params.filter_flags & COLLECTION_FILTER_ALTERED)
      g_snprintf (wq+strlen(wq),2048-strlen(wq)," %s id in (select imgid from history where imgid=id)", (need_operator)?"and":((need_operator=1)?"":"") );
    else if (collection->params.filter_flags & COLLECTION_FILTER_UNALTERED)
      g_snprintf (wq+strlen(wq),2048-strlen(wq)," %s id not in (select imgid from history where imgid=id)", (need_operator)?"and":((need_operator=1)?"":"") );
    
    /* add where ext if wanted */
    if ((collection->params.query_flags&COLLECTION_QUERY_USE_WHERE_EXT))
       g_snprintf (wq+strlen(wq),2048-strlen(wq)," %s %s",(need_operator)?"and":((need_operator=1)?"":""), collection->where_ext);
  } 
  else
    g_snprintf (wq,512,"%s",collection->where_ext);
  
    
  
  /* build select part includes where */
  if (sort == DT_LIB_SORT_COLOR && (collection->params.query_flags & COLLECTION_QUERY_USE_SORT))
    g_snprintf (selq,512,"select distinct id from (select * from images where %s) as a left outer join color_labels as b on a.id = b.imgid",wq);
  else
    g_snprintf(selq,512, "select distinct id from images where %s",wq);
  
  
  /* build sort order part */
  if ((collection->params.query_flags&COLLECTION_QUERY_USE_SORT))
  {
    if (sort == DT_LIB_SORT_DATETIME)           g_snprintf (sq, 512, ORDER_BY_QUERY, "datetime_taken");
    else if(sort == DT_LIB_SORT_RATING)         g_snprintf (sq, 512, ORDER_BY_QUERY, "flags & 7 desc");
    else if(sort == DT_LIB_SORT_FILENAME)       g_snprintf (sq, 512, ORDER_BY_QUERY, "filename");
    else if(sort == DT_LIB_SORT_ID)             g_snprintf (sq, 512, ORDER_BY_QUERY, "id");
    else if(sort == DT_LIB_SORT_COLOR)          g_snprintf (sq, 512, ORDER_BY_QUERY, "color desc, filename");
  }
  
  /* store the new query */
  g_snprintf (query,MAX_QUERY_STRING_LENGTH,"%s %s%s", selq, sq, (collection->params.query_flags&COLLECTION_QUERY_USE_LIMIT)?" "LIMIT_QUERY:"");
  result = _dt_collection_store (collection,query);
  g_free (query); 
  return result;
}

void 
dt_collection_reset(const dt_collection_t *collection)
{
  dt_collection_params_t *params=(dt_collection_params_t *)&collection->params;

  /* setup defaults */
  params->query_flags = COLLECTION_QUERY_FULL;
  params->filter_flags = COLLECTION_FILTER_FILM_ID | COLLECTION_FILTER_ATLEAST_RATING;
  params->film_id = 1;
  params->rating = 1;

  /* check if stored query parameters exist */
  if (dt_conf_key_exists ("plugins/collection/filter_flags"))
  {
    /* apply stored query parameters from previous darktable session */
    params->film_id = dt_conf_get_int("plugins/collection/film_id");
    params->rating = dt_conf_get_int("plugins/collection/rating");
    params->query_flags = dt_conf_get_int("plugins/collection/query_flags");
    params->filter_flags= dt_conf_get_int("plugins/collection/filter_flags");
  }
  dt_collection_update (collection);
}

const gchar *
dt_collection_get_query (const dt_collection_t *collection)
{
  /* ensure there is a query string for collection */
  if(!collection->query) 
    dt_collection_update(collection);

  return collection->query;
}

uint32_t 
dt_collection_get_filter_flags(const dt_collection_t *collection)
{
  return  collection->params.filter_flags;
}

void 
dt_collection_set_filter_flags(const dt_collection_t *collection, uint32_t flags)
{
  dt_collection_params_t *params=(dt_collection_params_t *)&collection->params;
  params->filter_flags = flags;
}

uint32_t 
dt_collection_get_query_flags(const dt_collection_t *collection)
{
  return  collection->params.query_flags;
}

void 
dt_collection_set_query_flags(const dt_collection_t *collection, uint32_t flags)
{
  dt_collection_params_t *params=(dt_collection_params_t *)&collection->params;
  params->query_flags = flags;
}

void 
dt_collection_set_extended_where(const dt_collection_t *collection,gchar *extended_where)
{
  /* free extended where if alread exists */
  if (collection->where_ext)
    g_free (collection->where_ext);
  
  /* set new from parameter */
  ((dt_collection_t *)collection)->where_ext = g_strdup(extended_where);
}

void 
dt_collection_set_film_id (const dt_collection_t *collection, uint32_t film_id)
{
  dt_collection_params_t *params=(dt_collection_params_t *)&collection->params;
  params->film_id = film_id;
}

void 
dt_collection_set_rating (const dt_collection_t *collection, uint32_t rating)
{
  dt_collection_params_t *params=(dt_collection_params_t *)&collection->params;
  params->rating = rating;
}

static int 
_dt_collection_store (const dt_collection_t *collection, gchar *query) 
{
  if (collection->query && strcmp (collection->query,query) == 0) 
    return 0;
  
  /* store flags to gconf */
  if (!collection->clone)
  {
    dt_conf_set_int ("plugins/collection/query_flags",collection->params.query_flags);
    dt_conf_set_int ("plugins/collection/filter_flags",collection->params.filter_flags);
    dt_conf_set_int ("plugins/collection/film_id",collection->params.film_id);
    dt_conf_set_int ("plugins/collection/rating",collection->params.rating);
  }
  
  /* store query in context */
  if (collection->query)
    g_free (collection->query);
  
  ((dt_collection_t *)collection)->query = g_strdup(query);
  
  return 1;
}

uint32_t dt_collection_get_count(const dt_collection_t *collection) {
  sqlite3_stmt *stmt = NULL;
  uint32_t count=1;
  const gchar *query = dt_collection_get_query(collection);
  char countquery[2048]={0};
  snprintf(countquery, 2048, "select count(id) %s", query + 18);
  DT_DEBUG_SQLITE3_PREPARE_V2(darktable.db, countquery, -1, &stmt, NULL);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 1, 0);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 2, -1);
  if(sqlite3_step(stmt) == SQLITE_ROW)
    count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return count;
}


GList *dt_collection_get_selected (const dt_collection_t *collection)
{
  GList *list=NULL;
  dt_lib_sort_t sort = dt_conf_get_int ("ui_last/combo_sort");

  /* get collection order */
  char sq[512]={0};
  if ((collection->params.query_flags&COLLECTION_QUERY_USE_SORT))
  {
    if (sort == DT_LIB_SORT_DATETIME)           g_snprintf (sq, 512, ORDER_BY_QUERY, "datetime_taken");
    else if(sort == DT_LIB_SORT_RATING)         g_snprintf (sq, 512, ORDER_BY_QUERY, "flags & 7 desc");
    else if(sort == DT_LIB_SORT_FILENAME)       g_snprintf (sq, 512, ORDER_BY_QUERY, "filename");
    else if(sort == DT_LIB_SORT_ID)             g_snprintf (sq, 512, ORDER_BY_QUERY, "id");
    else if(sort == DT_LIB_SORT_COLOR)          g_snprintf (sq, 512, ORDER_BY_QUERY, "color desc,id");
  }
  
  
  sqlite3_stmt *stmt = NULL;
  char query[2048]={0};
  
  if (sort == DT_LIB_SORT_COLOR && (collection->params.query_flags & COLLECTION_QUERY_USE_SORT))
    g_snprintf (query,512,"select distinct a.imgid as id from (select imgid from selected_images) as a left outer join color_labels as b on a.imgid = b.imgid %s",sq);
  else
    g_snprintf(query,512, "select distinct id from images where id in (select imgid from selected_images) %s",sq);
  
  DT_DEBUG_SQLITE3_PREPARE_V2(darktable.db,query, -1, &stmt, NULL);

  while (sqlite3_step (stmt) == SQLITE_ROW)
  {
    long int imgid = sqlite3_column_int(stmt, 0);
    list = g_list_append (list, (gpointer)imgid);
  }

  return list;
}

static void
get_query_string(const int property, const gchar *escaped_text, char *query)
{
  switch(property)
  {
    case 0: // film roll
      snprintf(query, 1024, "(film_id in (select id from film_rolls where folder like '%%%s%%'))", escaped_text);
      break;

    case 5: // colorlabel
    {
      int color = 0;
      if     (strcmp(escaped_text,_("red")   )==0) color=0;
      else if(strcmp(escaped_text,_("yellow"))==0) color=1;
      else if(strcmp(escaped_text,_("green") )==0) color=2;
      else if(strcmp(escaped_text,_("blue")  )==0) color=3;
      else if(strcmp(escaped_text,_("purple"))==0) color=4;
      snprintf(query, 1024, "(id in (select imgid from color_labels where color=%d))", color);
    } break;
    
    case 4: // history
      snprintf(query, 1024, "(id %s in (select imgid from history where imgid=images.id)) ",(strcmp(escaped_text,_("altered"))==0)?"":"not");
      break;
      
    case 1: // camera
      snprintf(query, 1024, "(maker || ' ' || model like '%%%s%%')", escaped_text);
      break;
    case 2: // tag
      snprintf(query, 1024, "(id in (select imgid from tagged_images as a join "
                            "tags as b on a.tagid = b.id where name like '%%%s%%'))", escaped_text);
      break;

    // TODO: How to handle images without metadata? In the moment they are not shown.
    // TODO: Autogenerate this code?
    case 6: // title
      snprintf(query, 1024, "(id in (select id from meta_data where key = %d and value like '%%%s%%'))",
                            DT_METADATA_XMP_DC_TITLE, escaped_text);
      break;
    case 7: // description
        snprintf(query, 1024, "(id in (select id from meta_data where key = %d and value like '%%%s%%'))",
                              DT_METADATA_XMP_DC_DESCRIPTION, escaped_text);
        break;
    case 8: // creator
      snprintf(query, 1024, "(id in (select id from meta_data where key = %d and value like '%%%s%%'))",
                            DT_METADATA_XMP_DC_CREATOR, escaped_text);
      break;
    case 9: // publisher
      snprintf(query, 1024, "(id in (select id from meta_data where key = %d and value like '%%%s%%'))",
                            DT_METADATA_XMP_DC_PUBLISHER, escaped_text);
      break;
    case 10: // rights
      snprintf(query, 1024, "(id in (select id from meta_data where key = %d and value like '%%%s%%'))",
                            DT_METADATA_XMP_DC_RIGHTS, escaped_text);
      break;

    default: // case 3: // day
      snprintf(query, 1024, "(datetime_taken like '%%%s%%')", escaped_text);
      break;
  }
}

void
dt_collection_listener_register(void (*callback)(void *), void *data)
{
  dt_collection_listener_t *a = (dt_collection_listener_t *)malloc(sizeof(dt_collection_listener_t));
  a->callback = callback;
  a->data = data;
  darktable.collection_listeners = g_list_append(darktable.collection_listeners, a);
}

void
dt_collection_listener_unregister(void (*callback)(void *))
{
  GList *i = darktable.collection_listeners;
  while(i)
  {
    dt_collection_listener_t *a = (dt_collection_listener_t *)i->data;
    GList *ii = g_list_next(i);
    if(a->callback == callback)
    {
      free(a);
      darktable.collection_listeners = g_list_delete_link(darktable.collection_listeners, i);
    }
    i = ii;
  }
}

void
dt_collection_update_query()
{
  char query[1024], confname[200];
  char complete_query[4096];
  int pos = 0;

  const int num_rules = CLAMP(dt_conf_get_int("plugins/lighttable/collect/num_rules"), 1, 10);
  char *conj[] = {"and", "or", "and not"};
  complete_query[pos++] = '(';
  for(int i=0;i<num_rules;i++)
  {
    snprintf(confname, 200, "plugins/lighttable/collect/item%1d", i);
    const int property = dt_conf_get_int(confname);
    snprintf(confname, 200, "plugins/lighttable/collect/string%1d", i);
    gchar *text = dt_conf_get_string(confname);
    if(!text) break;
    snprintf(confname, 200, "plugins/lighttable/collect/mode%1d", i);
    const int mode = dt_conf_get_int(confname);
    gchar *escaped_text = dt_util_str_replace(text, "'", "''");

    get_query_string(property, escaped_text, query);

    if(i > 0) pos += sprintf(complete_query + pos, " %s %s", conj[mode], query);
    else pos += sprintf(complete_query + pos, "%s", query);
    
    g_free(escaped_text);
    g_free(text);
  }
  complete_query[pos++] = ')';
  complete_query[pos++] = '\0';

  // printf("complete query: `%s'\n", complete_query);
  
  /* set the extended where and the use of it in the query */
  dt_collection_set_extended_where (darktable.collection, complete_query);
  dt_collection_set_query_flags (darktable.collection, (dt_collection_get_query_flags (darktable.collection) | COLLECTION_QUERY_USE_WHERE_EXT));
  
  /* remove film id from default filter */
  dt_collection_set_filter_flags (darktable.collection, (dt_collection_get_filter_flags (darktable.collection) & ~COLLECTION_FILTER_FILM_ID));
  
  /* update query and at last the visual */
  dt_collection_update (darktable.collection);
  
  dt_control_queue_draw_all();

  // TODO: update list of recent queries
  // TODO: remove from selected images where not in this query.

  // notify our listeners:
  GList *i = darktable.collection_listeners;
  while(i)
  {
    dt_collection_listener_t *a = (dt_collection_listener_t *)i->data;
    a->callback(a->data);
    i = g_list_next(i);
  }
}


