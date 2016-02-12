#include "e.h"
#include "e_mod_main.h"

/* actual module specifics */
static E_Module *conf_module = NULL;

/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
     "Settings - Performance"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   e_configure_registry_category_add("advanced", 80, _("Advanced"), NULL, "preferences-advanced");
   e_configure_registry_item_add("advanced/performance", 20, _("Performance"), NULL, "preferences-system-performance", e_int_config_performance);   
   e_configure_registry_item_add("advanced/engine", 50, _("Engine"), NULL, "preferences-engine", e_int_config_engine);
   e_configure_registry_item_add("advanced/powermanagement", 50, _("Power Management"), NULL, "preferences-system-power-management", e_int_config_powermanagement);
   conf_module = m;
   e_module_delayed_set(m, 1);
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   E_Config_Dialog *cfd;
   while ((cfd = e_config_dialog_get("E", "advanced/engine"))) e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "advanced/performance"))) e_object_del(E_OBJECT(cfd));
   e_configure_registry_item_del("advanced/engine");
   e_configure_registry_item_del("advanced/performance");
   e_configure_registry_item_del("advanced/powermanagement");
   e_configure_registry_category_del("advanced");
   conf_module = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return 1;
}
