#ifdef E_TYPEDEFS

typedef enum _E_Container_Shape_Change
{
   E_CONTAINER_SHAPE_ADD,
   E_CONTAINER_SHAPE_DEL,
   E_CONTAINER_SHAPE_SHOW,
   E_CONTAINER_SHAPE_HIDE,
   E_CONTAINER_SHAPE_MOVE,
   E_CONTAINER_SHAPE_RESIZE,
   E_CONTAINER_SHAPE_RECTS
} E_Container_Shape_Change;

typedef enum _E_Layer
{
   E_LAYER_DESKTOP = 0,
   E_LAYER_BELOW = 50,
   E_LAYER_NORMAL = 100,
   E_LAYER_ABOVE = 150,
   E_LAYER_EDGE = 200,
   E_LAYER_FULLSCREEN = 250,
   E_LAYER_EDGE_FULLSCREEN = 300,
   E_LAYER_POPUP = 300,
   E_LAYER_TOP = 350,
   E_LAYER_DRAG = 400,
   E_LAYER_PRIO = 450
} E_Layer;

typedef struct _E_Container                E_Container;
typedef struct _E_Border_List              E_Border_List;
typedef struct _E_Container_Shape          E_Container_Shape;
typedef struct _E_Container_Shape_Callback E_Container_Shape_Callback;
typedef struct _E_Event_Container_Resize   E_Event_Container_Resize;

#else
#ifndef E_CONTAINER_H
#define E_CONTAINER_H

#define E_CONTAINER_TYPE (int) 0xE0b01003
#define E_CONTAINER_SHAPE_TYPE (int) 0xE0b01004

struct _E_Container
{
   E_Object             e_obj_inherit;

   Ecore_X_Window       win;
   int                  x, y, w, h;
   char                 visible : 1;
   E_Manager           *manager;

   unsigned int         num;
   const char	       *name;

   Ecore_Evas          *bg_ecore_evas;
   Evas                *bg_evas;
   Evas_Object         *bg_blank_object;
   Ecore_X_Window       bg_win;
   Ecore_X_Window       event_win;

   Eina_List           *shapes;
   Eina_List           *shape_change_cb;
   Eina_List           *zones;

   unsigned int clients;
   struct {
      Ecore_X_Window win;
      Eina_List *clients;
   } layers[12];

   Ecore_X_Window       scratch_win;
};

struct _E_Border_List
{
   E_Container *container;
   Eina_Array   client_array;
   int          pos;
};

struct _E_Container_Shape
{
   E_Object       e_obj_inherit;

   E_Container   *con;
   int            x, y, w, h;
   unsigned char  visible : 1;
   struct {
      int x, y, w, h;
   } solid_rect;
   Eina_List     *shape;
};

struct _E_Container_Shape_Callback
{
   void (*func) (void *data, E_Container_Shape *es, E_Container_Shape_Change ch);
   void *data;
};

struct _E_Event_Container_Resize
{
   E_Container *container;
};

EINTERN int          e_container_init(void);
EINTERN int          e_container_shutdown(void);

EAPI E_Container *e_container_new(E_Manager *man);
EAPI void         e_container_show(E_Container *con);
EAPI void         e_container_hide(E_Container *con);
EAPI E_Container *e_container_current_get(E_Manager *man);
EAPI E_Container *e_container_number_get(E_Manager *man, int num);
EAPI void         e_container_move(E_Container *con, int x, int y);
EAPI void         e_container_resize(E_Container *con, int w, int h);
EAPI void         e_container_move_resize(E_Container *con, int x, int y, int w, int h);
EAPI void         e_container_raise(E_Container *con);
EAPI void         e_container_lower(E_Container *con);

EAPI E_Border_List *e_container_border_list_first(E_Container *con);
EAPI E_Border_List *e_container_border_list_last(E_Container *con);
EAPI E_Border      *e_container_border_list_next(E_Border_List *list);
EAPI E_Border      *e_container_border_list_prev(E_Border_List *list);
EAPI void           e_container_border_list_free(E_Border_List *list);

EAPI E_Zone      *e_container_zone_at_point_get(E_Container *con, int x, int y);
EAPI E_Zone      *e_container_zone_number_get(E_Container *con, int num);
EAPI E_Zone      *e_container_zone_id_get(E_Container *con, int id);

EAPI E_Desk            *e_container_desk_window_profile_get(E_Container *con, const char *profile);

EAPI E_Container_Shape *e_container_shape_add(E_Container *con);
EAPI void               e_container_shape_show(E_Container_Shape *es);
EAPI void               e_container_shape_hide(E_Container_Shape *es);
EAPI void               e_container_shape_move(E_Container_Shape *es, int x, int y);
EAPI void               e_container_shape_resize(E_Container_Shape *es, int w, int h);
EAPI Eina_List         *e_container_shape_list_get(E_Container *con);
EAPI void               e_container_shape_geometry_get(E_Container_Shape *es, int *x, int *y, int *w, int *h);
EAPI E_Container       *e_container_shape_container_get(E_Container_Shape *es);
EAPI void               e_container_shape_change_callback_add(E_Container *con, void (*func) (void *data, E_Container_Shape *es, E_Container_Shape_Change ch), void *data);
EAPI void               e_container_shape_change_callback_del(E_Container *con, void (*func) (void *data, E_Container_Shape *es, E_Container_Shape_Change ch), void *data);
EAPI Eina_List         *e_container_shape_rects_get(E_Container_Shape *es);
EAPI void               e_container_shape_rects_set(E_Container_Shape *es, Ecore_X_Rectangle *rects, int num);
EAPI void               e_container_shape_solid_rect_set(E_Container_Shape *es, int x, int y, int w, int h);
EAPI void               e_container_shape_solid_rect_get(E_Container_Shape *es, int *x, int *y, int *w, int *h);

EAPI int                e_container_borders_count(E_Container *con);
EAPI void               e_container_border_add(E_Border *bd);
EAPI void               e_container_border_remove(E_Border *bd);
EAPI void               e_container_window_raise(E_Container *con, Ecore_X_Window win, E_Layer layer);
EAPI void               e_container_window_lower(E_Container *con, Ecore_X_Window win, E_Layer layer);
EAPI E_Border          *e_container_border_raise(E_Border *bd);
EAPI E_Border          *e_container_border_lower(E_Border *bd);
EAPI void               e_container_border_stack_above(E_Border *bd, E_Border *above);
EAPI void               e_container_border_stack_below(E_Border *bd, E_Border *below);

EAPI void               e_container_all_freeze(void);
EAPI void               e_container_all_thaw(void);

EAPI E_Container *e_container_evas_object_container_get(Evas_Object *obj);

extern EAPI int E_EVENT_CONTAINER_RESIZE;

#endif
#endif
