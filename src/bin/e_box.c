#include "e.h"

typedef struct _E_Smart_Data E_Smart_Data;
typedef struct _E_Box_Item   E_Box_Item;

struct _E_Smart_Data
{
   Evas_Coord    x, y, w, h;
   Evas_Object  *obj;
   Evas_Object  *clip;
   int           frozen;
   unsigned char changed : 1;
   unsigned char horizontal : 1;
   unsigned char homogenous : 1;
   E_Box_Item  *items;
   unsigned int item_count;
   struct
   {
      Evas_Coord w, h;
   } min, max;
   struct
   {
      double x, y;
   } align;
};

struct _E_Box_Item
{
   EINA_INLIST;
   E_Smart_Data *sd;
   unsigned char fill_w : 1;
   unsigned char fill_h : 1;
   unsigned char expand_w : 1;
   unsigned char expand_h : 1;
   struct
   {
      Evas_Coord w, h;
   } min, max;
   struct
   {
      double x, y;
   } align;
   int x, y, w, h;
   Evas_Object  *obj;
};

/* local subsystem functions */
static void        _e_box_unpack_internal(E_Smart_Data *sd, E_Box_Item *bi);
static E_Box_Item *_e_box_smart_adopt(E_Smart_Data *sd, Evas_Object *obj);
static void        _e_box_smart_disown(E_Box_Item *bi);
static void        _e_box_smart_item_del_hook(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void        _e_box_smart_reconfigure(E_Smart_Data *sd);
static void        _e_box_smart_extents_calculate(E_Smart_Data *sd);

static void        _e_box_smart_init(void);
static void        _e_box_smart_add(Evas_Object *obj);
static void        _e_box_smart_del(Evas_Object *obj);
static void        _e_box_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static void        _e_box_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h);
static void        _e_box_smart_show(Evas_Object *obj);
static void        _e_box_smart_hide(Evas_Object *obj);
static void        _e_box_smart_color_set(Evas_Object *obj, int r, int g, int b, int a);
static void        _e_box_smart_clip_set(Evas_Object *obj, Evas_Object *clip);
static void        _e_box_smart_clip_unset(Evas_Object *obj);

/* local subsystem globals */
static Evas_Smart *_e_smart = NULL;

static inline Evas_Object *
_e_box_item_object_get(E_Box_Item *bi)
{
   if (!bi) return NULL;
   return bi->obj;
}

static inline Evas_Object *
_e_box_item_nth_get(E_Smart_Data *sd, unsigned int n)
{
   unsigned int x;
   E_Box_Item *bi;

   if (!sd->items) return NULL;
   if (n > sd->item_count / 2)
     {
        x = sd->item_count - 1;
        EINA_INLIST_REVERSE_FOREACH(EINA_INLIST_GET(sd->items), bi)
          {
             if (n == x) return bi->obj;
             x--;
          }
        return NULL;
     }
   x = 0;
   EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
     {
        if (n == x) return bi->obj;
        x++;
     }
   return NULL;
}

/* externally accessible functions */
EAPI Evas_Object *
e_box_add(Evas *evas)
{
   _e_box_smart_init();
   return evas_object_smart_add(evas, _e_smart);
}

EAPI int
e_box_freeze(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   sd->frozen++;
   return sd->frozen;
}

EAPI int
e_box_thaw(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   sd->frozen--;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
   return sd->frozen;
}

EAPI void
e_box_orientation_set(Evas_Object *obj, int horizontal)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR();
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->horizontal == horizontal) return;
   sd->horizontal = horizontal;
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
}

EAPI int
e_box_orientation_get(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   return sd->horizontal;
}

EAPI void
e_box_homogenous_set(Evas_Object *obj, int homogenous)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR();
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->homogenous == homogenous) return;
   sd->homogenous = homogenous;
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
}

EAPI int
e_box_pack_start(Evas_Object *obj, Evas_Object *child)
{
   E_Smart_Data *sd;
   E_Box_Item *bi;
   Eina_Inlist *l;

   if (!child) return 0;
   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   bi = _e_box_smart_adopt(sd, child);
   l = EINA_INLIST_GET(sd->items);
   l = eina_inlist_prepend(l, EINA_INLIST_GET(bi));
   sd->items = EINA_INLIST_CONTAINER_GET(l, E_Box_Item);
   sd->item_count++;
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
   return 0;
}

EAPI int
e_box_pack_end(Evas_Object *obj, Evas_Object *child)
{
   E_Smart_Data *sd;
   E_Box_Item *bi;
   Eina_Inlist *l;

   if (!child) return 0;
   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   bi = _e_box_smart_adopt(sd, child);
   l = EINA_INLIST_GET(sd->items);
   l = eina_inlist_append(l, EINA_INLIST_GET(bi));
   sd->items = EINA_INLIST_CONTAINER_GET(l, E_Box_Item);
   sd->item_count++;
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
   return sd->item_count - 1;
}

EAPI int
e_box_pack_before(Evas_Object *obj, Evas_Object *child, Evas_Object *before)
{
   E_Smart_Data *sd;
   E_Box_Item *bi, *bi2;
   int i = 0;
   Eina_Inlist *l;

   if (!child) return 0;
   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   bi2 = evas_object_data_get(before, "e_box_data");
   if (!bi2) return 0;
   bi = _e_box_smart_adopt(sd, child);
   l = EINA_INLIST_GET(sd->items);
   l = eina_inlist_prepend_relative(l, EINA_INLIST_GET(bi), EINA_INLIST_GET(bi2));
   sd->items = EINA_INLIST_CONTAINER_GET(l, E_Box_Item);
   sd->item_count++;
   
   for (l = EINA_INLIST_GET(bi)->prev; l; l = l->prev)
     i++;
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
   return i;
}

EAPI int
e_box_pack_after(Evas_Object *obj, Evas_Object *child, Evas_Object *after)
{
   E_Smart_Data *sd;
   E_Box_Item *bi, *bi2;
   int i = 0;
   Eina_Inlist *l;

   if (!child) return 0;
   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   bi2 = evas_object_data_get(after, "e_box_data");
   if (!bi2) return 0;
   bi = _e_box_smart_adopt(sd, child);
   l = EINA_INLIST_GET(sd->items);
   l = eina_inlist_append_relative(l, EINA_INLIST_GET(bi), EINA_INLIST_GET(bi2));
   sd->items = EINA_INLIST_CONTAINER_GET(l, E_Box_Item);
   sd->item_count++;
   for (l = EINA_INLIST_GET(bi)->prev; l; l = l->prev)
     i++;
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
   return i;
}

EAPI int
e_box_pack_count_get(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(0);
   if (!sd) return 0;
   return sd->item_count;
}

EAPI Evas_Object *
e_box_pack_object_nth(Evas_Object *obj, int n)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(NULL);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return NULL;
   return _e_box_item_nth_get(sd, n);
}

EAPI Evas_Object *
e_box_pack_object_first(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(NULL);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return NULL;
   return sd->items ? sd->items->obj : NULL;
}

EAPI Evas_Object *
e_box_pack_object_last(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERR(NULL);
   sd = evas_object_smart_data_get(obj);
   if ((!sd) || (!sd->items)) return NULL;
   return EINA_INLIST_CONTAINER_GET(EINA_INLIST_GET(sd->items)->last, E_Box_Item)->obj;
}

EAPI void
e_box_pack_options_set(Evas_Object *obj, int fill_w, int fill_h, int expand_w, int expand_h, double align_x, double align_y, Evas_Coord min_w, Evas_Coord min_h, Evas_Coord max_w, Evas_Coord max_h)
{
   E_Box_Item *bi;

   bi = evas_object_data_get(obj, "e_box_data");
   if (!bi) return;
   bi->fill_w = fill_w;
   bi->fill_h = fill_h;
   bi->expand_w = expand_w;
   bi->expand_h = expand_h;
   bi->align.x = align_x;
   bi->align.y = align_y;
   bi->min.w = min_w;
   bi->min.h = min_h;
   bi->max.w = max_w;
   bi->max.h = max_h;
   bi->sd->changed = 1;
   if (bi->sd->frozen <= 0) _e_box_smart_reconfigure(bi->sd);
}

EAPI void
e_box_unpack(Evas_Object *obj)
{
   E_Box_Item *bi;
   E_Smart_Data *sd;

   if (!obj) return;
   bi = evas_object_data_get(obj, "e_box_data");
   if (!bi) return;
   sd = bi->sd;
   if (!sd) return;
   _e_box_unpack_internal(sd, bi);
}

EAPI void
e_box_size_min_get(Evas_Object *obj, Evas_Coord *minw, Evas_Coord *minh)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR();
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->changed) _e_box_smart_extents_calculate(sd);
   if (minw) *minw = sd->min.w;
   if (minh) *minh = sd->min.h;
}

EAPI void
e_box_size_max_get(Evas_Object *obj, Evas_Coord *maxw, Evas_Coord *maxh)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR();
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->changed) _e_box_smart_extents_calculate(sd);
   if (maxw) *maxw = sd->max.w;
   if (maxh) *maxh = sd->max.h;
}

EAPI void
e_box_align_get(Evas_Object *obj, double *ax, double *ay)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR();
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (ax) *ax = sd->align.x;
   if (ay) *ay = sd->align.y;
}

EAPI void
e_box_align_set(Evas_Object *obj, double ax, double ay)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR();
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if ((sd->align.x == ax) && (sd->align.y == ay)) return;
   sd->align.x = ax;
   sd->align.y = ay;
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
}

/*
 * Returns the number of pixels that are hidden on the left/top side.
 */
EAPI void
e_box_align_pixel_offset_get(Evas_Object *obj, int *x, int *y)
{
   E_Smart_Data *sd;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR();
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (x) *x = (sd->min.w - sd->w) * (1.0 - sd->align.x);
   if (y) *y = (sd->min.h - sd->h) * (1.0 - sd->align.y);
}

EAPI Evas_Object *
e_box_item_at_xy_get(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   E_Smart_Data *sd;
   E_Box_Item *bi;

   if (evas_object_smart_smart_get(obj) != _e_smart) SMARTERRNR() NULL;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return NULL;
   if (!E_INSIDE(x, y, sd->x, sd->y, sd->w, sd->h)) return NULL;
   if (sd->horizontal)
     {
        if (x < sd->w / 2)
          {
             EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
               {
                  if (E_INSIDE(x, y, bi->x, bi->y, bi->w, bi->h)) return bi->obj;
               }
             return NULL;
          }
        EINA_INLIST_REVERSE_FOREACH(EINA_INLIST_GET(sd->items), bi)
          {
             if (E_INSIDE(x, y, bi->x, bi->y, bi->w, bi->h)) return bi->obj;
          }
        return NULL;
     }
   if (y < sd->h / 2)
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
          {
             if (E_INSIDE(x, y, bi->x, bi->y, bi->w, bi->h)) return bi->obj;
          }
        return NULL;
     }
   EINA_INLIST_REVERSE_FOREACH(EINA_INLIST_GET(sd->items), bi)
     {
        if (E_INSIDE(x, y, bi->x, bi->y, bi->w, bi->h)) return bi->obj;
     }
   return NULL;
}

EAPI Eina_Bool
e_box_item_size_get(Evas_Object *obj, int *w, int *h)
{
   E_Box_Item *bi;

   bi = evas_object_data_get(obj, "e_box_data");
   EINA_SAFETY_ON_NULL_RETURN_VAL(bi, EINA_FALSE);
   if (w) *w = bi->w;
   if (h) *h = bi->h;
   return EINA_TRUE;
}

/* local subsystem functions */
static void
_e_box_unpack_internal(E_Smart_Data *sd, E_Box_Item *bi)
{
   Eina_Inlist *l;

   l = EINA_INLIST_GET(sd->items);
   l = eina_inlist_remove(l, EINA_INLIST_GET(bi));
   sd->items = EINA_INLIST_CONTAINER_GET(l, E_Box_Item);
   sd->item_count--;
   _e_box_smart_disown(bi);
   sd->changed = 1;
   if (sd->frozen <= 0) _e_box_smart_reconfigure(sd);
}

static E_Box_Item *
_e_box_smart_adopt(E_Smart_Data *sd, Evas_Object *obj)
{
   E_Box_Item *bi;

   bi = calloc(1, sizeof(E_Box_Item));
   if (!bi) return NULL;
   bi->sd = sd;
   bi->obj = obj;
   /* defaults */
   bi->fill_w = 0;
   bi->fill_h = 0;
   bi->expand_w = 0;
   bi->expand_h = 0;
   bi->align.x = 0.5;
   bi->align.y = 0.5;
   bi->min.w = 0;
   bi->min.h = 0;
   bi->max.w = 0;
   bi->max.h = 0;
   evas_object_clip_set(obj, sd->clip);
   evas_object_smart_member_add(obj, bi->sd->obj);
   evas_object_data_set(obj, "e_box_data", bi);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FREE,
                                  _e_box_smart_item_del_hook, NULL);
   if ((!evas_object_visible_get(sd->clip)) &&
       (evas_object_visible_get(sd->obj)))
     evas_object_show(sd->clip);
   return bi;
}

static void
_e_box_smart_disown(E_Box_Item *bi)
{
   if (!bi) return;
   if (!bi->sd->items)
     {
        if (evas_object_visible_get(bi->sd->clip))
          evas_object_hide(bi->sd->clip);
     }
   evas_object_event_callback_del(bi->obj,
                                  EVAS_CALLBACK_FREE,
                                  _e_box_smart_item_del_hook);
   evas_object_smart_member_del(bi->obj);
   evas_object_clip_unset(bi->obj);
   evas_object_data_del(bi->obj, "e_box_data");
   free(bi);
}

static void
_e_box_smart_item_del_hook(void *data __UNUSED__, Evas *e __UNUSED__, Evas_Object *obj, void *event_info __UNUSED__)
{
   e_box_unpack(obj);
}

static void
_e_box_smart_reconfigure(E_Smart_Data *sd)
{
   Evas_Coord x, y, w, h, xx, yy;
   E_Box_Item *bi;
   int minw, minh, wdif, hdif;
   int count, expand;

   if (!sd->changed) return;

   x = sd->x;
   y = sd->y;
   w = sd->w;
   h = sd->h;

   _e_box_smart_extents_calculate(sd);
   minw = sd->min.w;
   minh = sd->min.h;
   count = sd->item_count;
   expand = 0;
   if (w < minw)
     {
        x = x + ((w - minw) * (1.0 - sd->align.x));
        w = minw;
     }
   if (h < minh)
     {
        y = y + ((h - minh) * (1.0 - sd->align.y));
        h = minh;
     }
   EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
     {
        if (sd->horizontal)
          {
             if (bi->expand_w) expand++;
          }
        else
          {
             if (bi->expand_h) expand++;
          }
     }
   if (expand == 0)
     {
        if (sd->horizontal)
          {
             x += (double)(w - minw) * sd->align.x;
             w = minw;
          }
        else
          {
             y += (double)(h - minh) * sd->align.y;
             h = minh;
          }
     }
   wdif = w - minw;
   hdif = h - minh;
   xx = x;
   yy = y;
   EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
     {
        if (sd->horizontal)
          {
             if (sd->homogenous)
               {
                  Evas_Coord ww, hh, ow, oh;

                  ww = (w / (Evas_Coord)count);
                  hh = h;
                  ow = bi->min.w;
                  if (bi->fill_w) ow = ww;
                  if ((bi->max.w >= 0) && (bi->max.w < ow))
                    ow = bi->max.w;
                  oh = bi->min.h;
                  if (bi->fill_h) oh = hh;
                  if ((bi->max.h >= 0) && (bi->max.h < oh))
                    oh = bi->max.h;
                  bi->x = xx + (Evas_Coord)(((double)(ww - ow)) * bi->align.x);
                  bi->y = yy + (Evas_Coord)(((double)(hh - oh)) * bi->align.y);
                  evas_object_move(bi->obj, bi->x, bi->y);
                  evas_object_resize(bi->obj, bi->w = ow, bi->h = oh);
                  xx += ww;
               }
             else
               {
                  Evas_Coord ww, hh, ow, oh;

                  ww = bi->min.w;
                  if ((expand > 0) && (bi->expand_w))
                    {
                       if (expand == 1) ow = wdif;
                       else ow = (w - minw) / expand;
                       wdif -= ow;
                       ww += ow;
                    }
                  hh = h;
                  ow = bi->min.w;
                  if (bi->fill_w) ow = ww;
                  if ((bi->max.w >= 0) && (bi->max.w < ow)) ow = bi->max.w;
                  oh = bi->min.h;
                  if (bi->fill_h) oh = hh;
                  if ((bi->max.h >= 0) && (bi->max.h < oh)) oh = bi->max.h;
                  bi->x = xx + (Evas_Coord)(((double)(ww - ow)) * bi->align.x);
                  bi->y = yy + (Evas_Coord)(((double)(hh - oh)) * bi->align.y);
                  evas_object_move(bi->obj, bi->x, bi->y);
                  evas_object_resize(bi->obj, bi->w = ow, bi->h = oh);
                  xx += ww;
               }
          }
        else
          {
             if (sd->homogenous)
               {
                  Evas_Coord ww, hh, ow, oh;

                  ww = w;
                  hh = (h / (Evas_Coord)count);
                  ow = bi->min.w;
                  if (bi->fill_w) ow = ww;
                  if ((bi->max.w >= 0) && (bi->max.w < ow)) ow = bi->max.w;
                  oh = bi->min.h;
                  if (bi->fill_h) oh = hh;
                  if ((bi->max.h >= 0) && (bi->max.h < oh)) oh = bi->max.h;
                  bi->x = xx + (Evas_Coord)(((double)(ww - ow)) * bi->align.x);
                  bi->y = yy + (Evas_Coord)(((double)(hh - oh)) * bi->align.y);
                  evas_object_move(bi->obj, bi->x, bi->y);
                  evas_object_resize(bi->obj, bi->w = ow, bi->h = oh);
                  yy += hh;
               }
             else
               {
                  Evas_Coord ww, hh, ow, oh;

                  ww = w;
                  hh = bi->min.h;
                  if ((expand > 0) && (bi->expand_h))
                    {
                       if (expand == 1) oh = hdif;
                       else oh = (h - minh) / expand;
                       hdif -= oh;
                       hh += oh;
                    }
                  ow = bi->min.w;
                  if (bi->fill_w) ow = ww;
                  if ((bi->max.w >= 0) && (bi->max.w < ow)) ow = bi->max.w;
                  oh = bi->min.h;
                  if (bi->fill_h) oh = hh;
                  if ((bi->max.h >= 0) && (bi->max.h < oh)) oh = bi->max.h;
                  bi->x = xx + (Evas_Coord)(((double)(ww - ow)) * bi->align.x);
                  bi->y = yy + (Evas_Coord)(((double)(hh - oh)) * bi->align.y);
                  evas_object_move(bi->obj, bi->x, bi->y);
                  evas_object_resize(bi->obj, bi->w = ow, bi->h = oh);
                  yy += hh;
               }
          }
     }
   sd->changed = 0;
}

static void
_e_box_smart_extents_calculate(E_Smart_Data *sd)
{
   E_Box_Item *bi;
   int minw, minh;

   /* FIXME: need to calc max */
   sd->max.w = -1; /* max < 0 == unlimited */
   sd->max.h = -1;

   minw = 0;
   minh = 0;
   if (sd->homogenous)
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
          {
             if (minh < bi->min.h) minh = bi->min.h;
             if (minw < bi->min.w) minw = bi->min.w;
          }
        if (sd->horizontal)
          minw *= sd->item_count;
        else
          minh *= sd->item_count;
     }
   else
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
          {
             if (sd->horizontal)
               {
                  if (minh < bi->min.h) minh = bi->min.h;
                  minw += bi->min.w;
               }
             else
               {
                  if (minw < bi->min.w) minw = bi->min.w;
                  minh += bi->min.h;
               }
          }
     }
   sd->min.w = minw;
   sd->min.h = minh;
}

static void
_e_box_smart_init(void)
{
   if (_e_smart) return;
   {
      static const Evas_Smart_Class sc =
      {
         "e_box",
         EVAS_SMART_CLASS_VERSION,
         _e_box_smart_add,
         _e_box_smart_del,
         _e_box_smart_move,
         _e_box_smart_resize,
         _e_box_smart_show,
         _e_box_smart_hide,
         _e_box_smart_color_set,
         _e_box_smart_clip_set,
         _e_box_smart_clip_unset,
         NULL,
         NULL,
         NULL,
         NULL,
         NULL,
         NULL,
         NULL
      };
      _e_smart = evas_smart_class_new(&sc);
   }
}

static void
_e_box_smart_add(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = calloc(1, sizeof(E_Smart_Data));
   if (!sd) return;
   sd->obj = obj;
   sd->x = 0;
   sd->y = 0;
   sd->w = 0;
   sd->h = 0;
   sd->align.x = 0.5;
   sd->align.y = 0.5;
   sd->clip = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(sd->clip, obj);
   evas_object_move(sd->clip, -100004, -100004);
   evas_object_resize(sd->clip, 200008, 200008);
   evas_object_color_set(sd->clip, 255, 255, 255, 255);
   evas_object_smart_data_set(obj, sd);
}

static void
_e_box_smart_del(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   /* FIXME: this gets into an infinite loop when changin basic->advanced on
    * ibar config dialog
    */
   e_box_freeze(obj);
   while (sd->items)
     e_box_unpack(sd->items->obj);
   e_box_thaw(obj);
   evas_object_del(sd->clip);
   free(sd);

   evas_object_smart_data_set(obj, NULL);
}

static void
_e_box_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   E_Smart_Data *sd;
   E_Box_Item *bi;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if ((x == sd->x) && (y == sd->y)) return;
   {
      Evas_Coord dx, dy;

      dx = x - sd->x;
      dy = y - sd->y;
      EINA_INLIST_FOREACH(EINA_INLIST_GET(sd->items), bi)
        {
           bi->x += dx;
           bi->y += dy;
           evas_object_move(bi->obj, bi->x, bi->y);
        }
   }
   sd->x = x;
   sd->y = y;
}

static void
_e_box_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if ((w == sd->w) && (h == sd->h)) return;
   sd->w = w;
   sd->h = h;
   sd->changed = 1;
   _e_box_smart_reconfigure(sd);
}

static void
_e_box_smart_show(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->items) evas_object_show(sd->clip);
}

static void
_e_box_smart_hide(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_hide(sd->clip);
}

static void
_e_box_smart_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_color_set(sd->clip, r, g, b, a);
}

static void
_e_box_smart_clip_set(Evas_Object *obj, Evas_Object *clip)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_clip_set(sd->clip, clip);
}

static void
_e_box_smart_clip_unset(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_clip_unset(sd->clip);
}

