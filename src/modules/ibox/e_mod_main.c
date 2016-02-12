#include "e.h"
#include "e_mod_main.h"

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);
static void             _gc_id_del(const E_Gadcon_Client_Class *client_class, const char *id);
/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION,
   "ibox",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, _gc_id_del,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_INSET
};

/* actual module specifics */

typedef struct _Instance  Instance;

typedef struct _IBox      IBox;
typedef struct _IBox_Icon IBox_Icon;

struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas_Object     *o_ibox;
   IBox            *ibox;
   E_Drop_Handler  *drop_handler;
   Config_Item     *ci;
   E_Gadcon_Orient  orient;
};

struct _IBox
{
   Instance    *inst;
   Evas_Object *o_box;
   Evas_Object *o_drop;
   Evas_Object *o_drop_over;
   Evas_Object *o_empty;
   IBox_Icon   *ic_drop_before;
   int          drop_before;
   Eina_List   *icons;
   E_Zone      *zone;
   Evas_Coord   dnd_x, dnd_y;
};

struct _IBox_Icon
{
   IBox        *ibox;
   Evas_Object *o_holder;
   Evas_Object *o_icon;
   Evas_Object *o_holder2;
   Evas_Object *o_icon2;
   E_Border    *border;
   struct
   {
      unsigned char start : 1;
      unsigned char dnd : 1;
      int           x, y;
      int           dx, dy;
   } drag;
};

static IBox        *_ibox_new(Evas *evas, E_Zone *zone);
static void         _ibox_free(IBox *b);
static void         _ibox_cb_empty_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_empty_handle(IBox *b);
static void         _ibox_fill(IBox *b);
static void         _ibox_empty(IBox *b);
static void         _ibox_orient_set(IBox *b, int horizontal);
static void         _ibox_resize_handle(IBox *b);
static void         _ibox_instance_drop_zone_recalc(Instance *inst);
static IBox_Icon   *_ibox_icon_find(IBox *b, E_Border *bd);
static IBox_Icon   *_ibox_icon_at_coord(IBox *b, Evas_Coord x, Evas_Coord y);
static IBox_Icon   *_ibox_icon_new(IBox *b, E_Border *bd);
static void         _ibox_icon_free(IBox_Icon *ic);
static void         _ibox_icon_fill(IBox_Icon *ic);
static void         _ibox_icon_fill_label(IBox_Icon *ic);
static void         _ibox_icon_empty(IBox_Icon *ic);
static void         _ibox_icon_signal_emit(IBox_Icon *ic, char *sig, char *src);
static Eina_List   *_ibox_zone_find(E_Zone *zone);
static void         _ibox_cb_obj_moveresize(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_menu_configuration(void *data, E_Menu *m, E_Menu_Item *mi);
static void         _ibox_cb_icon_mouse_in(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_icon_mouse_out(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_icon_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_icon_mouse_up(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_icon_mouse_move(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_icon_move(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_icon_resize(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibox_cb_drag_finished(E_Drag *drag, int dropped);
static void         _ibox_inst_cb_enter(void *data, const char *type, void *event_info);
static void         _ibox_inst_cb_move(void *data, const char *type, void *event_info);
static void         _ibox_inst_cb_leave(void *data, const char *type, void *event_info);
static void         _ibox_inst_cb_drop(void *data, const char *type, void *event_info);
static void         _ibox_drop_position_update(Instance *inst, Evas_Coord x, Evas_Coord y);
static void         _ibox_inst_cb_scroll(void *data);
static Eina_Bool    _ibox_cb_event_border_add(void *data, int type, void *event);
static Eina_Bool    _ibox_cb_event_border_remove(void *data, int type, void *event);
static Eina_Bool    _ibox_cb_event_border_iconify(void *data, int type, void *event);
static Eina_Bool    _ibox_cb_event_border_uniconify(void *data, int type, void *event);
static Eina_Bool    _ibox_cb_event_border_icon_change(void *data, int type, void *event);
static Eina_Bool    _ibox_cb_event_border_urgent_change(void *data, int type, void *event);
static Eina_Bool    _ibox_cb_event_border_zone_set(void *data, int type, void *event);
static Eina_Bool    _ibox_cb_event_desk_show(void *data, int type, void *event);
static Config_Item *_ibox_config_item_get(const char *id);

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;

Config *ibox_config = NULL;

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   IBox *b;
   Evas_Object *o;
   E_Gadcon_Client *gcc;
   Instance *inst;
   Evas_Coord x, y, w, h;
   const char *drop[] = { "enform/border" };
   Config_Item *ci;

   inst = E_NEW(Instance, 1);

   ci = _ibox_config_item_get(id);
   inst->ci = ci;

   b = _ibox_new(gc->evas, gc->zone);
   b->inst = inst;
   inst->ibox = b;
   _ibox_fill(b);
   o = b->o_box;
   gcc = e_gadcon_client_new(gc, name, id, style, o);
   gcc->data = inst;
   ci->gcc = gcc;

   inst->gcc = gcc;
   inst->o_ibox = o;
   inst->orient = E_GADCON_ORIENT_HORIZ;

   evas_object_geometry_get(o, &x, &y, &w, &h);
   inst->drop_handler =
     e_drop_handler_add(E_OBJECT(inst->gcc), inst,
                        _ibox_inst_cb_enter, _ibox_inst_cb_move,
                        _ibox_inst_cb_leave, _ibox_inst_cb_drop,
                        drop, 1, x, y, w, h);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE,
                                  _ibox_cb_obj_moveresize, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE,
                                  _ibox_cb_obj_moveresize, inst);
   ibox_config->instances = eina_list_append(ibox_config->instances, inst);
   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   inst = gcc->data;
   inst->ci->gcc = NULL;
   ibox_config->instances = eina_list_remove(ibox_config->instances, inst);
   e_drop_handler_del(inst->drop_handler);
   _ibox_free(inst->ibox);
   free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   Instance *inst;

   inst = gcc->data;
   if ((int)orient != -1) inst->orient = orient;

   switch (inst->orient)
     {
      case E_GADCON_ORIENT_FLOAT:
      case E_GADCON_ORIENT_HORIZ:
      case E_GADCON_ORIENT_TOP:
      case E_GADCON_ORIENT_BOTTOM:
      case E_GADCON_ORIENT_CORNER_TL:
      case E_GADCON_ORIENT_CORNER_TR:
      case E_GADCON_ORIENT_CORNER_BL:
      case E_GADCON_ORIENT_CORNER_BR:
        _ibox_orient_set(inst->ibox, 1);
        e_gadcon_client_aspect_set(gcc, eina_list_count(inst->ibox->icons) * 16, 16);
        break;

      case E_GADCON_ORIENT_VERT:
      case E_GADCON_ORIENT_LEFT:
      case E_GADCON_ORIENT_RIGHT:
      case E_GADCON_ORIENT_CORNER_LT:
      case E_GADCON_ORIENT_CORNER_RT:
      case E_GADCON_ORIENT_CORNER_LB:
      case E_GADCON_ORIENT_CORNER_RB:
        _ibox_orient_set(inst->ibox, 0);
        e_gadcon_client_aspect_set(gcc, 16, eina_list_count(inst->ibox->icons) * 16);
        break;

      default:
        break;
     }
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class __UNUSED__)
{
   return _("IBox");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class __UNUSED__, Evas *evas)
{
   Evas_Object *o;
   char buf[PATH_MAX];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-ibox.edj",
            e_module_dir_get(ibox_config->module));
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
   static char buf[4096];

   snprintf(buf, sizeof(buf), "%s.%d", client_class->name,
            eina_list_count(ibox_config->instances) + 1);
   return buf;
}

static void
_gc_id_del(const E_Gadcon_Client_Class *client_class __UNUSED__, const char *id __UNUSED__)
{
/* yes - don't do this. on shutdown gadgets are deleted and this means config
 * for them is deleted - that means empty config is saved. keep them around
 * as if u add a gadget back it can pick up its old config again
   Config_Item *ci;

   ci = _ibox_config_item_get(id);
   if (ci)
     {
        if (ci->id) eina_stringshare_del(ci->id);
        ibox_config->items = eina_list_remove(ibox_config->items, ci);
     }
 */
}

static IBox *
_ibox_new(Evas *evas, E_Zone *zone)
{
   IBox *b;

   b = E_NEW(IBox, 1);
   b->o_box = e_box_add(evas);
   e_box_homogenous_set(b->o_box, 1);
   e_box_orientation_set(b->o_box, 1);
   e_box_align_set(b->o_box, 0.5, 0.5);
   b->zone = zone;
   return b;
}

static void
_ibox_free(IBox *b)
{
   _ibox_empty(b);
   evas_object_del(b->o_box);
   if (b->o_drop) evas_object_del(b->o_drop);
   if (b->o_drop_over) evas_object_del(b->o_drop_over);
   if (b->o_empty) evas_object_del(b->o_empty);
   free(b);
}

static void
_ibox_cb_empty_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Down *ev;
   IBox *b;
   E_Menu *m;
   E_Menu_Item *mi;
   int cx, cy;

   ev = event_info;
   b = data;
   if (ev->button != 3) return;

   m = e_menu_new();
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Settings"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _ibox_cb_menu_configuration, b);

   m = e_gadcon_client_util_menu_items_append(b->inst->gcc, m, 0);

   e_gadcon_canvas_zone_geometry_get(b->inst->gcc->gadcon,
                                     &cx, &cy, NULL, NULL);
   e_menu_activate_mouse(m,
                         e_util_zone_current_get(e_manager_current_get()),
                         cx + ev->output.x, cy + ev->output.y, 1, 1,
                         E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
   evas_event_feed_mouse_up(b->inst->gcc->gadcon->evas, ev->button,
                            EVAS_BUTTON_NONE, ev->timestamp, NULL);
}

static void
_ibox_empty_handle(IBox *b)
{
   if (!b->icons)
     {
        if (!b->o_empty)
          {
             Evas_Coord w, h;

             b->o_empty = evas_object_rectangle_add(evas_object_evas_get(b->o_box));
             evas_object_event_callback_add(b->o_empty, EVAS_CALLBACK_MOUSE_DOWN, _ibox_cb_empty_mouse_down, b);
             evas_object_color_set(b->o_empty, 0, 0, 0, 0);
             evas_object_show(b->o_empty);
             e_box_pack_end(b->o_box, b->o_empty);
             evas_object_geometry_get(b->o_box, NULL, NULL, &w, &h);
             if (e_box_orientation_get(b->o_box))
               w = h;
             else
               h = w;
             e_box_pack_options_set(b->o_empty,
                                    1, 1, /* fill */
                                    1, 1, /* expand */
                                    0.5, 0.5, /* align */
                                    w, h, /* min */
                                    9999, 9999 /* max */
                                    );
          }
     }
   else if (b->o_empty)
     {
        evas_object_del(b->o_empty);
        b->o_empty = NULL;
     }
}

static void
_ibox_fill(IBox *b)
{
   IBox_Icon *ic;
   E_Border_List *bl;
   E_Border *bd;
   int ok;
   int mw, mh, h;

   bl = e_container_border_list_first(b->zone->container);
   while ((bd = e_container_border_list_next(bl)))
     {
        ok = 0;
        if ((b->inst->ci->show_zone == 0) && (bd->iconic))
          {
             ok = 1;
          }
        else if ((b->inst->ci->show_zone == 1) && (bd->iconic))
          {
             if (bd->sticky)
               {
                  ok = 1;
               }
             else if ((b->inst->ci->show_desk == 0) && (bd->zone == b->zone))
               {
                  ok = 1;
               }
             else if ((b->inst->ci->show_desk == 1) && (bd->zone == b->zone) &&
                      (bd->desk == e_desk_current_get(b->zone)))
               {
                  ok = 1;
               }
          }

        if (ok)
          {
             ic = _ibox_icon_new(b, bd);
             b->icons = eina_list_append(b->icons, ic);
             e_box_pack_end(b->o_box, ic->o_holder);
          }
     }
   e_container_border_list_free(bl);

   _ibox_empty_handle(b);
   _ibox_resize_handle(b);
   if (!b->inst->gcc) return;
   if (!b->inst->ci->expand_on_desktop) return;
   if (!e_gadcon_site_is_desktop(b->inst->gcc->gadcon->location->site)) return;
   e_box_size_min_get(b->o_box, &mw, &mh);
   evas_object_geometry_get(b->inst->gcc->o_frame, NULL, NULL, NULL, &h);
   evas_object_resize(b->inst->gcc->o_frame, MIN(mw, b->inst->gcc->gadcon->zone->w), MAX(h, mh));
}

static void
_ibox_empty(IBox *b)
{
   E_FREE_LIST(b->icons, _ibox_icon_free);
   _ibox_empty_handle(b);
}

static void
_ibox_orient_set(IBox *b, int horizontal)
{
   e_box_orientation_set(b->o_box, horizontal);
   e_box_align_set(b->o_box, 0.5, 0.5);
}

static void
_ibox_resize_handle(IBox *b)
{
   Eina_List *l;
   IBox_Icon *ic;
   int w, h;

   evas_object_geometry_get(b->o_box, NULL, NULL, &w, &h);
   if (e_box_orientation_get(b->o_box))
     w = h;
   else
     h = w;
   e_box_freeze(b->o_box);
   EINA_LIST_FOREACH(b->icons, l, ic)
     {
        e_box_pack_options_set(ic->o_holder,
                               1, 1, /* fill */
                               0, 0, /* expand */
                               0.5, 0.5, /* align */
                               w, h, /* min */
                               w, h /* max */
                               );
     }
   e_box_thaw(b->o_box);
}

static void
_ibox_instance_drop_zone_recalc(Instance *inst)
{
   Evas_Coord x, y, w, h;

   e_gadcon_client_viewport_geometry_get(inst->gcc, &x, &y, &w, &h);
   e_drop_handler_geometry_set(inst->drop_handler, x, y, w, h);
}

static IBox_Icon *
_ibox_icon_find(IBox *b, E_Border *bd)
{
   Eina_List *l;
   IBox_Icon *ic;

   EINA_LIST_FOREACH(b->icons, l, ic)
     {
        if (ic->border == bd) return ic;
     }
   return NULL;
}

static IBox_Icon *
_ibox_icon_at_coord(IBox *b, Evas_Coord x, Evas_Coord y)
{
   Eina_List *l;
   IBox_Icon *ic;

   EINA_LIST_FOREACH(b->icons, l, ic)
     {
        Evas_Coord dx, dy, dw, dh;

        evas_object_geometry_get(ic->o_holder, &dx, &dy, &dw, &dh);
        if (E_INSIDE(x, y, dx, dy, dw, dh)) return ic;
     }
   return NULL;
}

static IBox_Icon *
_ibox_icon_new(IBox *b, E_Border *bd)
{
   IBox_Icon *ic;

   ic = E_NEW(IBox_Icon, 1);
   e_object_ref(E_OBJECT(bd));
   ic->ibox = b;
   ic->border = bd;
   ic->o_holder = edje_object_add(evas_object_evas_get(b->o_box));
   e_theme_edje_object_set(ic->o_holder, "base/theme/modules/ibox",
                           "e/modules/ibox/icon");
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_IN, _ibox_cb_icon_mouse_in, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_OUT, _ibox_cb_icon_mouse_out, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_DOWN, _ibox_cb_icon_mouse_down, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_UP, _ibox_cb_icon_mouse_up, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_MOVE, _ibox_cb_icon_mouse_move, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOVE, _ibox_cb_icon_move, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_RESIZE, _ibox_cb_icon_resize, ic);
   evas_object_show(ic->o_holder);

   ic->o_holder2 = edje_object_add(evas_object_evas_get(b->o_box));
   e_theme_edje_object_set(ic->o_holder2, "base/theme/modules/ibox",
                           "e/modules/ibox/icon_overlay");
   evas_object_layer_set(ic->o_holder2, 9999);
   evas_object_pass_events_set(ic->o_holder2, 1);
   evas_object_show(ic->o_holder2);

   _ibox_icon_fill(ic);
   return ic;
}

static void
_ibox_icon_free(IBox_Icon *ic)
{
   if (ic->ibox->ic_drop_before == ic)
     ic->ibox->ic_drop_before = NULL;
   _ibox_icon_empty(ic);
   evas_object_del(ic->o_holder);
   evas_object_del(ic->o_holder2);
   e_object_unref(E_OBJECT(ic->border));
   free(ic);
}

static void
_ibox_icon_fill(IBox_Icon *ic)
{
   ic->o_icon = e_border_icon_add(ic->border, evas_object_evas_get(ic->ibox->o_box));
   edje_object_part_swallow(ic->o_holder, "e.swallow.content", ic->o_icon);
   evas_object_pass_events_set(ic->o_icon, 1);
   evas_object_show(ic->o_icon);
   ic->o_icon2 = e_border_icon_add(ic->border, evas_object_evas_get(ic->ibox->o_box));
   edje_object_part_swallow(ic->o_holder2, "e.swallow.content", ic->o_icon2);
   evas_object_pass_events_set(ic->o_icon2, 1);
   evas_object_show(ic->o_icon2);

   _ibox_icon_fill_label(ic);
}

static void
_ibox_icon_fill_label(IBox_Icon *ic)
{
   const char *label = NULL;

   switch (ic->ibox->inst->ci->icon_label)
     {
      case 0:
        label = ic->border->client.netwm.name;
        if (!label)
          label = ic->border->client.icccm.name;
        break;

      case 1:
        label = ic->border->client.icccm.title;
        break;

      case 2:
        label = ic->border->client.icccm.class;
        break;

      case 3:
        label = ic->border->client.netwm.icon_name;
        if (!label)
          label = ic->border->client.icccm.icon_name;
        break;

      case 4:
        label = e_border_name_get(ic->border);
        break;
     }

   if (!label) label = "?";
   edje_object_part_text_set(ic->o_holder2, "e.text.label", label);
}

static void
_ibox_icon_empty(IBox_Icon *ic)
{
   if (ic->o_icon) evas_object_del(ic->o_icon);
   if (ic->o_icon2) evas_object_del(ic->o_icon2);
   ic->o_icon = NULL;
   ic->o_icon2 = NULL;
}

static void
_ibox_icon_signal_emit(IBox_Icon *ic, char *sig, char *src)
{
   if (ic->o_holder)
     edje_object_signal_emit(ic->o_holder, sig, src);
   if (ic->o_icon)
     edje_object_signal_emit(ic->o_icon, sig, src);
   if (ic->o_holder2)
     edje_object_signal_emit(ic->o_holder2, sig, src);
   if (ic->o_icon2)
     edje_object_signal_emit(ic->o_icon2, sig, src);
}

static Eina_List *
_ibox_zone_find(E_Zone *zone)
{
   Eina_List *ibox = NULL;
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(ibox_config->instances, l, inst)
     {
        if (inst->ci->show_zone == 0)
          ibox = eina_list_append(ibox, inst->ibox);
        else if ((inst->ci->show_zone == 1) && (inst->ibox->zone == zone))
          ibox = eina_list_append(ibox, inst->ibox);
     }
   return ibox;
}

static void
_ibox_cb_obj_moveresize(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Instance *inst;

   inst = data;
   _ibox_resize_handle(inst->ibox);
   _ibox_instance_drop_zone_recalc(inst);
}

static void
_ibox_cb_icon_mouse_in(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   IBox_Icon *ic;

   ic = data;
   _ibox_icon_signal_emit(ic, "e,state,focused", "e");
   if (ic->ibox->inst->ci->show_label)
     {
        _ibox_icon_fill_label(ic);
        _ibox_icon_signal_emit(ic, "e,action,show,label", "e");
     }
}

static void
_ibox_cb_icon_mouse_out(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   IBox_Icon *ic;

   ic = data;
   _ibox_icon_signal_emit(ic, "e,state,unfocused", "e");
   if (ic->ibox->inst->ci->show_label)
     _ibox_icon_signal_emit(ic, "e,action,hide,label", "e");
}

static void
_ibox_cb_icon_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Down *ev;
   IBox_Icon *ic;

   ev = event_info;
   ic = data;
   if (ev->button == 1)
     {
        ic->drag.x = ev->output.x;
        ic->drag.y = ev->output.y;
        ic->drag.start = 1;
        ic->drag.dnd = 0;
     }
   else if (ev->button == 3)
     {
        E_Menu *m;
        E_Menu_Item *mi;
        int cx, cy;

        m = e_menu_new();

        /* FIXME: other icon options go here too */
        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _ibox_cb_menu_configuration, ic->ibox);

        m = e_gadcon_client_util_menu_items_append(ic->ibox->inst->gcc, m, 0);

        e_gadcon_canvas_zone_geometry_get(ic->ibox->inst->gcc->gadcon,
                                          &cx, &cy, NULL, NULL);
        e_menu_activate_mouse(m,
                              e_util_zone_current_get(e_manager_current_get()),
                              cx + ev->output.x, cy + ev->output.y, 1, 1,
                              E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
     }
}

static void
_ibox_cb_icon_mouse_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Up *ev;
   IBox_Icon *ic;

   ev = event_info;
   ic = data;
   if ((ev->button == 1) && (!ic->drag.dnd))
     {
        e_border_uniconify(ic->border);
        e_border_focus_set(ic->border, 1, 1);
     }
}

static void
_ibox_cb_icon_mouse_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Move *ev;
   IBox_Icon *ic;

   ev = event_info;
   ic = data;
   if (ic->drag.start)
     {
        int dx, dy;

        dx = ev->cur.output.x - ic->drag.x;
        dy = ev->cur.output.y - ic->drag.y;
        if (((dx * dx) + (dy * dy)) >
            (e_config->drag_resist * e_config->drag_resist))
          {
             E_Drag *d;
             Evas_Object *o;
             Evas_Coord x, y, w, h;
             const char *drag_types[] = { "enform/border" };
             E_Gadcon_Client *gcc;
             ic->drag.dnd = 1;
             ic->drag.start = 0;

             evas_object_geometry_get(ic->o_icon, &x, &y, &w, &h);
             d = e_drag_new(ic->ibox->inst->gcc->gadcon->zone->container,
                            x, y, drag_types, 1,
                            ic->border, -1, NULL, _ibox_cb_drag_finished);
             o = e_border_icon_add(ic->border, e_drag_evas_get(d));
             e_drag_object_set(d, o);

             e_drag_resize(d, w, h);
             e_drag_start(d, ic->drag.x, ic->drag.y);
             e_object_ref(E_OBJECT(ic->border));
             ic->ibox->icons = eina_list_remove(ic->ibox->icons, ic);
             _ibox_resize_handle(ic->ibox);
             gcc = ic->ibox->inst->gcc;
             _gc_orient(gcc, -1);
             _ibox_icon_free(ic);
          }
     }
}

static void
_ibox_cb_icon_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   IBox_Icon *ic;
   Evas_Coord x, y;

   ic = data;
   evas_object_geometry_get(ic->o_holder, &x, &y, NULL, NULL);
   evas_object_move(ic->o_holder2, x, y);
   evas_object_raise(ic->o_holder2);
}

static void
_ibox_cb_icon_resize(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   IBox_Icon *ic;
   Evas_Coord w, h;

   ic = data;
   evas_object_geometry_get(ic->o_holder, NULL, NULL, &w, &h);
   evas_object_resize(ic->o_holder2, w, h);
   evas_object_raise(ic->o_holder2);
}

static void
_ibox_cb_drag_finished(E_Drag *drag, int dropped)
{
   E_Border *bd;

   bd = drag->data;
   if (!dropped) e_border_uniconify(bd);
   e_object_unref(E_OBJECT(bd));
}

static void
_ibox_cb_drop_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   IBox *b;
   Evas_Coord x, y;

   b = data;
   evas_object_geometry_get(b->o_drop, &x, &y, NULL, NULL);
   evas_object_move(b->o_drop_over, x, y);
}

static void
_ibox_cb_drop_resize(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   IBox *b;
   Evas_Coord w, h;

   b = data;
   evas_object_geometry_get(b->o_drop, NULL, NULL, &w, &h);
   evas_object_resize(b->o_drop_over, w, h);
}

static void
_ibox_inst_cb_scroll(void *data)
{
   Instance *inst;

   /* Update the position of the dnd to handle for autoscrolling
    * gadgets. */
   inst = data;
   _ibox_drop_position_update(inst, inst->ibox->dnd_x, inst->ibox->dnd_y);
}

static void
_ibox_drop_position_update(Instance *inst, Evas_Coord x, Evas_Coord y)
{
   IBox_Icon *ic;

   inst->ibox->dnd_x = x;
   inst->ibox->dnd_y = y;

   if (inst->ibox->o_drop)
     e_box_unpack(inst->ibox->o_drop);
   ic = _ibox_icon_at_coord(inst->ibox, x, y);
   inst->ibox->ic_drop_before = ic;
   if (ic)
     {
        Evas_Coord ix, iy, iw, ih;
        int before = 0;

        evas_object_geometry_get(ic->o_holder, &ix, &iy, &iw, &ih);
        if (e_box_orientation_get(inst->ibox->o_box))
          {
             if (x < (ix + (iw / 2))) before = 1;
          }
        else
          {
             if (y < (iy + (ih / 2))) before = 1;
          }
        if (before)
          e_box_pack_before(inst->ibox->o_box, inst->ibox->o_drop, ic->o_holder);
        else
          e_box_pack_after(inst->ibox->o_box, inst->ibox->o_drop, ic->o_holder);
        inst->ibox->drop_before = before;
     }
   else e_box_pack_end(inst->ibox->o_box, inst->ibox->o_drop);
   e_box_pack_options_set(inst->ibox->o_drop,
                          1, 1, /* fill */
                          1, 1, /* expand */
                          0.5, 0.5, /* align */
                          1, 1, /* min */
                          -1, -1 /* max */
                          );
   _ibox_resize_handle(inst->ibox);
   _gc_orient(inst->gcc, -1);
}

static void
_ibox_inst_cb_enter(void *data, const char *type __UNUSED__, void *event_info)
{
   E_Event_Dnd_Enter *ev;
   Instance *inst;
   Evas_Object *o, *o2;

   ev = event_info;
   inst = data;
   o = edje_object_add(evas_object_evas_get(inst->ibox->o_box));
   inst->ibox->o_drop = o;
   o2 = edje_object_add(evas_object_evas_get(inst->ibox->o_box));
   inst->ibox->o_drop_over = o2;
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE, _ibox_cb_drop_move, inst->ibox);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE, _ibox_cb_drop_resize, inst->ibox);
   e_theme_edje_object_set(o, "base/theme/modules/ibox",
                           "e/modules/ibox/drop");
   e_theme_edje_object_set(o2, "base/theme/modules/ibox",
                           "e/modules/ibox/drop_overlay");
   evas_object_layer_set(o2, 19999);
   evas_object_show(o);
   evas_object_show(o2);
   _ibox_drop_position_update(inst, ev->x, ev->y);
   e_gadcon_client_autoscroll_cb_set(inst->gcc, _ibox_inst_cb_scroll, inst);
   e_gadcon_client_autoscroll_update(inst->gcc, ev->x, ev->y);
}

static void
_ibox_inst_cb_move(void *data, const char *type __UNUSED__, void *event_info)
{
   E_Event_Dnd_Move *ev;
   Instance *inst;

   ev = event_info;
   inst = data;
   _ibox_drop_position_update(inst, ev->x, ev->y);
   e_gadcon_client_autoscroll_update(inst->gcc, ev->x, ev->y);
}

static void
_ibox_inst_cb_leave(void *data, const char *type __UNUSED__, void *event_info __UNUSED__)
{
   Instance *inst;

   inst = data;
   inst->ibox->ic_drop_before = NULL;
   evas_object_del(inst->ibox->o_drop);
   inst->ibox->o_drop = NULL;
   evas_object_del(inst->ibox->o_drop_over);
   inst->ibox->o_drop_over = NULL;
   e_gadcon_client_autoscroll_cb_set(inst->gcc, NULL, NULL);
   _ibox_resize_handle(inst->ibox);
   _gc_orient(inst->gcc, -1);
}

static void
_ibox_inst_cb_drop(void *data, const char *type, void *event_info)
{
   E_Event_Dnd_Drop *ev;
   Instance *inst;
   E_Border *bd = NULL;
   IBox *b;
   IBox_Icon *ic, *ic2;
   Eina_List *l;

   ev = event_info;
   inst = data;
   if (!strcmp(type, "enform/border"))
     {
        bd = ev->data;
        if (!bd) return;
     }
   else return;

   if (!bd->iconic) e_border_iconify(bd);

   ic2 = inst->ibox->ic_drop_before;
   if (ic2)
     {
        /* Add new eapp before this icon */
        if (!inst->ibox->drop_before)
          {
             EINA_LIST_FOREACH(inst->ibox->icons, l, ic)
               {
                  if (ic == ic2)
                    {
                       ic2 = eina_list_data_get(l->next);
                       break;
                    }
               }
          }
        if (!ic2) goto atend;
        b = inst->ibox;
        if (_ibox_icon_find(b, bd)) return;
        ic = _ibox_icon_new(b, bd);
        if (!ic) return;
        b->icons = eina_list_prepend_relative(b->icons, ic, ic2);
        e_box_pack_before(b->o_box, ic->o_holder, ic2->o_holder);
     }
   else
     {
atend:
        b = inst->ibox;
        if (_ibox_icon_find(b, bd)) return;
        ic = _ibox_icon_new(b, bd);
        if (!ic) return;
        b->icons = eina_list_append(b->icons, ic);
        e_box_pack_end(b->o_box, ic->o_holder);
     }

   evas_object_del(inst->ibox->o_drop);
   inst->ibox->o_drop = NULL;
   evas_object_del(inst->ibox->o_drop_over);
   inst->ibox->o_drop_over = NULL;
   _ibox_empty_handle(b);
   e_gadcon_client_autoscroll_cb_set(inst->gcc, NULL, NULL);
   _ibox_resize_handle(inst->ibox);
   _gc_orient(inst->gcc, -1);
}

static Eina_Bool
_ibox_cb_event_border_add(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Border_Add *ev;
   IBox *b;
   IBox_Icon *ic;
   E_Desk *desk;

   ev = event;
   /* add if iconic */
   desk = e_desk_current_get(ev->border->zone);
   if (ev->border->iconic)
     {
        Eina_List *ibox;
        ibox = _ibox_zone_find(ev->border->zone);
        EINA_LIST_FREE(ibox, b)
          {
             if (_ibox_icon_find(b, ev->border)) continue;
             if ((b->inst->ci->show_desk) && (ev->border->desk != desk) && (!ev->border->sticky)) continue;
             ic = _ibox_icon_new(b, ev->border);
             if (!ic) continue;
             b->icons = eina_list_append(b->icons, ic);
             e_box_pack_end(b->o_box, ic->o_holder);
             _ibox_empty_handle(b);
             _ibox_resize_handle(b);
             _gc_orient(b->inst->gcc, -1);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ibox_cb_event_border_remove(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Border_Remove *ev;
   IBox *b;
   IBox_Icon *ic;
   Eina_List *ibox;

   ev = event;
   /* find icon and remove if there */
   ibox = _ibox_zone_find(ev->border->zone);
   EINA_LIST_FREE(ibox, b)
     {
        ic = _ibox_icon_find(b, ev->border);
        if (!ic) continue;
        _ibox_icon_free(ic);
        b->icons = eina_list_remove(b->icons, ic);
        _ibox_empty_handle(b);
        _ibox_resize_handle(b);
        _gc_orient(b->inst->gcc, -1);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ibox_cb_event_border_iconify(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Border_Iconify *ev;
   IBox *b;
   IBox_Icon *ic;
   Eina_List *ibox;
   E_Desk *desk;

   ev = event;
   /* add icon for ibox for right zone */
   /* do some sort of anim when iconifying */
   desk = e_desk_current_get(ev->border->zone);
   ibox = _ibox_zone_find(ev->border->zone);
   EINA_LIST_FREE(ibox, b)
     {
        int h, mw, mh;
        if (_ibox_icon_find(b, ev->border)) continue;
        if ((b->inst->ci->show_desk) && (ev->border->desk != desk) && (!ev->border->sticky)) continue;
        ic = _ibox_icon_new(b, ev->border);
        if (!ic) continue;
        b->icons = eina_list_append(b->icons, ic);
        e_box_pack_end(b->o_box, ic->o_holder);
        _ibox_empty_handle(b);
        _ibox_resize_handle(b);
        _gc_orient(b->inst->gcc, -1);
        if (!b->inst->ci->expand_on_desktop) continue;
        if (!e_gadcon_site_is_desktop(b->inst->gcc->gadcon->location->site)) continue;
        e_box_size_min_get(b->o_box, &mw, &mh);
        evas_object_geometry_get(b->inst->gcc->o_frame, NULL, NULL, NULL, &h);
        evas_object_resize(b->inst->gcc->o_frame, MIN(mw, b->inst->gcc->gadcon->zone->w), MAX(h, mh));
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ibox_cb_event_border_uniconify(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Border_Uniconify *ev;
   IBox *b;
   IBox_Icon *ic;
   Eina_List *ibox;

   ev = event;
   /* del icon for ibox for right zone */
   /* do some sort of anim when uniconifying */
   ibox = _ibox_zone_find(ev->border->zone);
   EINA_LIST_FREE(ibox, b)
     {
        int mw, mh, h;
        ic = _ibox_icon_find(b, ev->border);
        if (!ic) continue;
        _ibox_icon_free(ic);
        b->icons = eina_list_remove(b->icons, ic);
        _ibox_empty_handle(b);
        _ibox_resize_handle(b);
        _gc_orient(b->inst->gcc, -1);
        if (!b->inst->ci->expand_on_desktop) continue;
        if (!e_gadcon_site_is_desktop(b->inst->gcc->gadcon->location->site)) continue;
        e_box_size_min_get(b->o_box, &mw, &mh);
        evas_object_geometry_get(b->inst->gcc->o_frame, NULL, NULL, NULL, &h);
        evas_object_resize(b->inst->gcc->o_frame, MIN(mw, b->inst->gcc->gadcon->zone->w), MAX(h, mh));
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ibox_cb_event_border_icon_change(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Border_Icon_Change *ev;
   IBox *b;
   IBox_Icon *ic;
   Eina_List *ibox;

   ev = event;
   /* update icon */
   ibox = _ibox_zone_find(ev->border->zone);
   EINA_LIST_FREE(ibox, b)
     {
        ic = _ibox_icon_find(b, ev->border);
        if (!ic) continue;
        _ibox_icon_empty(ic);
        _ibox_icon_fill(ic);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ibox_cb_event_border_urgent_change(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Border_Urgent_Change *ev;
   IBox *b;
   IBox_Icon *ic;
   Eina_List *ibox;

   ev = event;
   /* update icon */
   ibox = _ibox_zone_find(ev->border->zone);
   EINA_LIST_FREE(ibox, b)
     {
        ic = _ibox_icon_find(b, ev->border);
        if (!ic) continue;
        if (ev->border->client.icccm.urgent)
          {
             e_gadcon_urgent_show(b->inst->gcc->gadcon);
             edje_object_signal_emit(ic->o_holder, "e,state,urgent", "e");
             edje_object_signal_emit(ic->o_holder2, "e,state,urgent", "e");
          }
        else
          {
             edje_object_signal_emit(ic->o_holder, "e,state,not_urgent", "e");
             edje_object_signal_emit(ic->o_holder2, "e,state,not_urgent", "e");
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ibox_cb_event_border_zone_set(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Border_Zone_Set *ev;

   ev = event;
   /* delete from current zone ibox, add to new one */
   if (ev->border->iconic)
     {
     }
   return 1;
}

static Eina_Bool
_ibox_cb_event_desk_show(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Desk_Show *ev;
   IBox *b;
   Eina_List *ibox;

   ev = event;
   /* delete all wins from ibox and add only for current desk */
   ibox = _ibox_zone_find(ev->desk->zone);
   EINA_LIST_FREE(ibox, b)
     {
        if (b->inst->ci->show_desk)
          {
             _ibox_empty(b);
             _ibox_fill(b);
             _ibox_resize_handle(b);
             _gc_orient(b->inst->gcc, -1);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Config_Item *
_ibox_config_item_get(const char *id)
{
   Config_Item *ci;

   GADCON_CLIENT_CONFIG_GET(Config_Item, ibox_config->items, _gadcon_class, id);

   ci = E_NEW(Config_Item, 1);
   ci->id = eina_stringshare_add(id);
   ci->show_label = 0;
   ci->show_zone = 1;
   ci->show_desk = 0;
   ci->icon_label = 0;
   ibox_config->items = eina_list_append(ibox_config->items, ci);
   return ci;
}

void
_ibox_config_update(Config_Item *ci)
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(ibox_config->instances, l, inst)
     {
        if (inst->ci != ci) continue;

        _ibox_empty(inst->ibox);
        _ibox_fill(inst->ibox);
        _ibox_resize_handle(inst->ibox);
        _gc_orient(inst->gcc, -1);
     }
}

static void
_ibox_cb_menu_configuration(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   IBox *b;
   int ok = 1;
   Eina_List *l;
   E_Config_Dialog *cfd;

   b = data;
   EINA_LIST_FOREACH(ibox_config->config_dialog, l, cfd)
     {
        if (cfd->data == b->inst->ci)
          {
             ok = 0;
             break;
          }
     }
   if (ok) _config_ibox_module(b->inst->ci);
}

/***************************************************************************/
/**/
/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "IBox"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   conf_item_edd = E_CONFIG_DD_NEW("IBox_Config_Item", Config_Item);
   #undef T
   #undef D
   #define T Config_Item
   #define D conf_item_edd
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, expand_on_desktop, INT);
   E_CONFIG_VAL(D, T, show_label, INT);
   E_CONFIG_VAL(D, T, show_zone, INT);
   E_CONFIG_VAL(D, T, show_desk, INT);
   E_CONFIG_VAL(D, T, icon_label, INT);

   conf_edd = E_CONFIG_DD_NEW("IBox_Config", Config);
   #undef T
   #undef D
   #define T Config
   #define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   ibox_config = e_config_domain_load("module.ibox", conf_edd);
   if (!ibox_config)
     {
        Config_Item *ci;

        ibox_config = E_NEW(Config, 1);

        ci = E_NEW(Config_Item, 1);
        ci->id = eina_stringshare_add("ibox.1");
        ci->show_label = 0;
        ci->show_zone = 1;
        ci->show_desk = 0;
        ci->icon_label = 0;
        ibox_config->items = eina_list_append(ibox_config->items, ci);
     }

   ibox_config->module = m;

   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_BORDER_ADD, _ibox_cb_event_border_add, NULL);
   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_BORDER_REMOVE, _ibox_cb_event_border_remove, NULL);
   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_BORDER_ICONIFY, _ibox_cb_event_border_iconify, NULL);
   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_BORDER_UNICONIFY, _ibox_cb_event_border_uniconify, NULL);
   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_BORDER_ICON_CHANGE, _ibox_cb_event_border_icon_change, NULL);
   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_BORDER_URGENT_CHANGE, _ibox_cb_event_border_urgent_change, NULL);
   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_BORDER_ZONE_SET, _ibox_cb_event_border_zone_set, NULL);
   E_LIST_HANDLER_APPEND(ibox_config->handlers, E_EVENT_DESK_SHOW, _ibox_cb_event_desk_show, NULL);

/* FIXME: add these later for things taskbar-like functionality
   ibox_config->handlers = eina_list_append
     (ibox_config->handlers, ecore_event_handler_add
      (E_EVENT_BORDER_DESK_SET, _ibox_cb_event_border_zone_set, NULL));
   ibox_config->handlers = eina_list_append
     (ibox_config->handlers, ecore_event_handler_add
      (E_EVENT_BORDER_SHOW, _ibox_cb_event_border_zone_set, NULL));
   ibox_config->handlers = eina_list_append
     (ibox_config->handlers, ecore_event_handler_add
      (E_EVENT_BORDER_HIDE, _ibox_cb_event_border_zone_set, NULL));
   ibox_config->handlers = eina_list_append
     (ibox_config->handlers, ecore_event_handler_add
      (E_EVENT_BORDER_STACK, _ibox_cb_event_border_zone_set, NULL));
   ibox_config->handlers = eina_list_append
     (ibox_config->handlers, ecore_event_handler_add
      (E_EVENT_BORDER_STICK, _ibox_cb_event_border_zone_set, NULL));
 */
   e_gadcon_provider_register(&_gadcon_class);
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   Config_Item *ci;
   e_gadcon_provider_unregister(&_gadcon_class);

   E_FREE_LIST(ibox_config->handlers, ecore_event_handler_del);

   while (ibox_config->config_dialog)
     /* there is no need to eves_list_remove_list. It is done implicitly in
      * dialog _free_data function
      */
     e_object_del(E_OBJECT(ibox_config->config_dialog->data));

   EINA_LIST_FREE(ibox_config->items, ci)
     {
        eina_stringshare_del(ci->id);
        free(ci);
     }

   E_FREE(ibox_config);
   E_CONFIG_DD_FREE(conf_item_edd);
   E_CONFIG_DD_FREE(conf_edd);
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   e_config_domain_save("module.ibox", conf_edd, ibox_config);
   return 1;
}

