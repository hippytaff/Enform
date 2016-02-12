#ifdef E_TYPEDEFS

typedef struct _E_Color_Class E_Color_Class;

#else
#ifndef E_COLOR_CLASSES_H
#define E_COLOR_CLASSES_H

struct _E_Color_Class
{
   const char	 *name; /* stringshared name */
   int		  r, g, b, a;
   int		  r2, g2, b2, a2;
   int		  r3, g3, b3, a3;
};

EINTERN int e_color_class_init(void);
EINTERN int e_color_class_shutdown(void);

EAPI void e_color_class_instance_set(E_Color_Class *cc,
				     int r, int g, int b, int a,
				     int r2, int b2, int g2, int a2,
				     int r3, int g3, int b3, int a3);
EAPI E_Color_Class *e_color_class_set_stringshared(const char *color_class,
						   int r, int g, int b, int a,
						   int r2, int b2, int g2, int a2,
						   int r3, int g3, int b3, int a3);
EAPI E_Color_Class *e_color_class_set(const char *color_class,
				      int r, int g, int b, int a,
				      int r2, int b2, int g2, int a2,
				      int r3, int g3, int b3, int a3);
EAPI E_Color_Class *e_color_class_find(const char *name);
EAPI E_Color_Class *e_color_class_find_stringshared(const char *name);


EAPI void e_color_class_instance_del(E_Color_Class *cc);
EAPI void e_color_class_del_stringshared(const char *name);
EAPI void e_color_class_del(const char *name);

EAPI Eina_List *e_color_class_list(void);

#endif
#endif
