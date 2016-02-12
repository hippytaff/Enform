#include "e.h"
#include "e_mod_main.h"

/* actual module specifics */
static E_Module *conf_module = NULL;

/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
     "Settings - Menu Settings"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   e_configure_registry_category_add("menus", 60, _("Menus"), NULL, "preferences-menus");
   e_configure_registry_item_add("menus/menu_settings", 30, _("Menu Settings"), NULL, "preferences-menu-settings", e_int_config_menus);
   conf_module = m;
   e_module_delayed_set(m, 1);
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   E_Config_Dialog *cfd;
   while ((cfd = e_config_dialog_get("E", "menus/menu_settings"))) e_object_del(E_OBJECT(cfd));
   e_configure_registry_item_del("menus/menu_settings");
   e_configure_registry_category_del("menus");
   conf_module = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return 1;
}
