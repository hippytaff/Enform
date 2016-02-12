#include "e_mod_main.h"

extern const char _e_mixer_Name[];

struct _E_Config_Dialog_Data
{
   int         lock_sliders;
   int         show_locked;
   int         keybindings_popup;
   int         card_num;
   int         channel;
   const char *card;
   const char *channel_name;
   Eina_List  *cards;
   Eina_List  *cards_names;
   Eina_List  *channels_names;
   struct mixer_config_ui
   {
      Evas_Object *table;
      struct mixer_config_ui_general
      {
         Evas_Object *frame;
         Evas_Object *lock_sliders;
         Evas_Object *show_locked;
         Evas_Object *keybindings_popup;
      } general;
      struct mixer_config_ui_cards
      {
         Evas_Object   *frame;
         E_Radio_Group *radio;
      } cards;
      struct mixer_config_ui_channels
      {
         Evas_Object   *frame;
         Evas_Object   *scroll;
         Evas_Object   *list;
         E_Radio_Group *radio;
         Eina_List     *radios;
      } channels;
   } ui;
   E_Mixer_Gadget_Config *conf;
};

static void
_mixer_fill_cards_info(E_Config_Dialog_Data *cfdata)
{
   const char *card;
   const char *name;
   Eina_List *l;
   int i = 0;

   cfdata->card_num = -1;
   cfdata->cards = e_mod_mixer_cards_get();
   cfdata->cards_names = NULL;
   EINA_LIST_FOREACH(cfdata->cards, l, card)
     {
        name = e_mod_mixer_card_name_get(card);
        if ((cfdata->card_num < 0) && card && cfdata->card &&
            (strcmp(card, cfdata->card) == 0))
          cfdata->card_num = i;

        cfdata->cards_names = eina_list_append(cfdata->cards_names, name);

        i++;
     }

   if (cfdata->card_num < 0)
     {
        card = eina_list_nth(cfdata->cards, 0);
        if (card)
          {
             cfdata->card_num = 0;
             eina_stringshare_del(cfdata->card);
             cfdata->card = eina_stringshare_ref(card);
          }
     }
}

static void
_mixer_fill_channels_info(E_Config_Dialog_Data *cfdata)
{
   E_Mixer_System *sys;
   const char *channel;
   Eina_List *l;
   int i = 0;

   sys = e_mod_mixer_new(cfdata->card);
   if (!sys)
     return;

   cfdata->channel = 0;
   cfdata->channel_name = eina_stringshare_add(cfdata->conf->channel_name);
   cfdata->channels_names = e_mod_mixer_channels_names_get(sys);
   EINA_LIST_FOREACH(cfdata->channels_names, l, channel)
     {
        if (channel && cfdata->channel_name &&
            (channel == cfdata->channel_name ||
             strcmp(channel, cfdata->channel_name) == 0))
          {
             cfdata->channel = i;
             break;
          }

        i++;
     }
   e_mod_mixer_del(sys);
}

static void *
_create_data(E_Config_Dialog *dialog)
{
   E_Config_Dialog_Data *cfdata;
   E_Mixer_Gadget_Config *conf;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   if (!cfdata)
     return NULL;

   conf = dialog->data;
   cfdata->conf = conf;
   cfdata->lock_sliders = conf->lock_sliders;
   cfdata->show_locked = conf->show_locked;
   cfdata->keybindings_popup = conf->keybindings_popup;
   cfdata->card = eina_stringshare_add(conf->card);
   _mixer_fill_cards_info(cfdata);
   _mixer_fill_channels_info(cfdata);

   return cfdata;
}

static void
_free_data(E_Config_Dialog *dialog, E_Config_Dialog_Data *cfdata)
{
   E_Mixer_Gadget_Config *conf = dialog->data;
   const char *card;

   if (conf)
     conf->dialog = NULL;

   if (!cfdata)
     return;

   EINA_LIST_FREE(cfdata->cards_names, card)
     eina_stringshare_del(card);

   if (cfdata->channels_names)
     e_mod_mixer_channels_free(cfdata->channels_names);
   if (cfdata->cards)
     e_mod_mixer_cards_free(cfdata->cards);

   eina_stringshare_del(cfdata->card);
   eina_stringshare_del(cfdata->channel_name);

   eina_list_free(cfdata->ui.channels.radios);

   E_FREE(cfdata);
}

static int
_basic_apply(E_Config_Dialog *dialog, E_Config_Dialog_Data *cfdata)
{
   E_Mixer_Gadget_Config *conf = dialog->data;
   const char *card, *channel;

   conf->lock_sliders = cfdata->lock_sliders;
   conf->show_locked = cfdata->show_locked;
   conf->keybindings_popup = cfdata->keybindings_popup;
   conf->using_default = EINA_FALSE;

   card = eina_list_nth(cfdata->cards, cfdata->card_num);
   if (card)
     {
        eina_stringshare_del(conf->card);
        conf->card = eina_stringshare_ref(card);
     }

   channel = eina_list_nth(cfdata->channels_names, cfdata->channel);
   if (channel)
     {
        eina_stringshare_del(conf->channel_name);
        conf->channel_name = eina_stringshare_ref(channel);
     }

   e_mixer_update(conf->instance);
   return 1;
}

static void
_lock_change(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   E_Config_Dialog_Data *cfdata = data;
   e_widget_disabled_set(cfdata->ui.general.show_locked, !cfdata->lock_sliders);
}

static void
_basic_create_general(Evas *evas, E_Config_Dialog_Data *cfdata)
{
   struct mixer_config_ui_general *ui = &cfdata->ui.general;

   ui->frame = e_widget_framelist_add(evas, _("General Settings"), 0);

   ui->lock_sliders = e_widget_check_add(
       evas, _("Lock Sliders"), &cfdata->lock_sliders);
   evas_object_smart_callback_add(
     ui->lock_sliders, "changed", _lock_change, cfdata);
   e_widget_framelist_object_append(ui->frame, ui->lock_sliders);

   ui->show_locked = e_widget_check_add(
       evas, _("Show both sliders when locked"), &cfdata->show_locked);
   e_widget_disabled_set(ui->show_locked, !cfdata->lock_sliders);
   e_widget_framelist_object_append(ui->frame, ui->show_locked);

   ui->keybindings_popup = e_widget_check_add(
       evas, _("Show Popup on volume change via keybindings"), &cfdata->keybindings_popup);
   e_widget_framelist_object_append(ui->frame, ui->keybindings_popup);
}

static void
_clear_channels(E_Config_Dialog_Data *cfdata)
{
   Evas_Object *o;

   EINA_LIST_FREE(cfdata->ui.channels.radios, o)
     evas_object_del(o);
}

static void
_fill_channels(Evas *evas, E_Config_Dialog_Data *cfdata)
{
   struct mixer_config_ui_channels *ui = &cfdata->ui.channels;
   Evas_Object *selected;
   Evas_Coord mw, mh;
   const char *name;
   Eina_List *l;
   int i = 0;

   ui->radio = e_widget_radio_group_new(&cfdata->channel);
   EINA_LIST_FOREACH(cfdata->channels_names, l, name)
     {
        Evas_Object *ow;

        if (!name) continue;

        ow = e_widget_radio_add(evas, name, i, ui->radio);
        ui->radios = eina_list_append(ui->radios, ow);
        e_widget_list_object_append(ui->list, ow, 1, 1, 0.0);

        ++i;
     }

   e_widget_size_min_get(ui->list, &mw, &mh);
   evas_object_resize(ui->list, mw, mh);

   selected = eina_list_nth(ui->radios, cfdata->channel);
   if (selected)
     {
        Evas_Coord x, y, w, h, lx, ly;
        evas_object_geometry_get(selected, &x, &y, &w, &h);
        evas_object_geometry_get(ui->list, &lx, &ly, NULL, NULL);
        x -= lx;
        y -= ly - 10;
        h += 20;
        e_widget_scrollframe_child_region_show(ui->scroll, x, y, w, h);
     }
}

static void
_channel_scroll_set_min_size(struct mixer_config_ui_channels *ui)
{
   Evas_Coord w, h;
   int len = eina_list_count(ui->radios);
   if (len < 1)
     return;

   e_widget_size_min_get(ui->list, &w, &h);
   h = 4 * h / len;
   e_widget_size_min_set(ui->scroll, w, h);
}

static void
_basic_create_channels(Evas *evas, E_Config_Dialog_Data *cfdata)
{
   struct mixer_config_ui_channels *ui = &cfdata->ui.channels;

   ui->list = e_widget_list_add(evas, 1, 0);
   ui->scroll = e_widget_scrollframe_simple_add(evas, ui->list);
   ui->frame = e_widget_framelist_add(evas, _("Channels"), 0);

   _fill_channels(evas, cfdata);

   _channel_scroll_set_min_size(ui);
   e_widget_framelist_object_append(ui->frame, ui->scroll);
}

static void
_card_change(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   E_Config_Dialog_Data *cfdata = data;
   Evas *evas;
   char *card;

   eina_stringshare_del(cfdata->card);

   e_mod_mixer_channels_free(cfdata->channels_names);

   eina_stringshare_del(cfdata->channel_name);

   card = eina_list_nth(cfdata->cards, cfdata->card_num);
   cfdata->card = eina_stringshare_add(card);
   _mixer_fill_channels_info(cfdata);

   evas = evas_object_evas_get(obj);
   _clear_channels(cfdata);
   _fill_channels(evas, cfdata);
}

static void
_basic_create_cards(Evas *evas, E_Config_Dialog_Data *cfdata)
{
   struct mixer_config_ui_cards *ui = &cfdata->ui.cards;
   const char *card;
   Eina_List *l;
   int i = 0;

   ui->frame = e_widget_framelist_add(evas, _("Sound Cards"), 0);
   ui->radio = e_widget_radio_group_new(&cfdata->card_num);
   EINA_LIST_FOREACH(cfdata->cards_names, l, card)
     {
        Evas_Object *ow;

        if (!card) continue;

        ow = e_widget_radio_add(evas, card, i, ui->radio);
        e_widget_framelist_object_append(ui->frame, ow);
        evas_object_smart_callback_add(ow, "changed", _card_change, cfdata);

        ++i;
     }
}

static Evas_Object *
_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   if (!cfdata)
     return NULL;

   e_dialog_resizable_set(cfd->dia, 1);
   cfdata->ui.table = e_widget_table_add(evas, 0);
   _basic_create_general(evas, cfdata);
   _basic_create_cards(evas, cfdata);
   _basic_create_channels(evas, cfdata);

   e_widget_table_object_append(cfdata->ui.table, cfdata->ui.general.frame,
                                0, 0, 1, 1, 1, 1, 1, 0);
   e_widget_table_object_append(cfdata->ui.table, cfdata->ui.cards.frame,
                                0, 1, 1, 1, 1, 1, 1, 0);
   e_widget_table_object_append(cfdata->ui.table, cfdata->ui.channels.frame,
                                0, 2, 1, 1, 1, 1, 1, 1);

   return cfdata->ui.table;
}

void
e_mixer_config_pulse_toggle(void)
{
}

E_Config_Dialog *
e_mixer_config_dialog_new(E_Container *con, E_Mixer_Gadget_Config *conf)
{
   E_Config_Dialog *dialog;
   E_Config_Dialog_View *view;

   if (e_config_dialog_find(_e_mixer_Name, "e_mixer_config_dialog_new"))
     return NULL;

   view = E_NEW(E_Config_Dialog_View, 1);
   if (!view)
     return NULL;

   view->create_cfdata = _create_data;
   view->free_cfdata = _free_data;
   view->basic.create_widgets = _basic_create;
   view->basic.apply_cfdata = _basic_apply;

   dialog = e_config_dialog_new(con, _("Mixer Settings"),
                                _e_mixer_Name, "e_mixer_config_dialog_new",
                                e_mixer_theme_path(), 0, view, conf);

   return dialog;
}

