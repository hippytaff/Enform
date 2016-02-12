#include "e.h"

/* local subsystem functions */

/* local subsystem globals */

/* externally accessible functions */
#define ENFORM_VERSION "0.0.2.2.2"
EAPI E_About *
e_about_new(E_Container *con)
{
   E_Obj_Dialog *od;
   char buf[16384];
   FILE *f;
   Eina_Strbuf *tbuf;

   od = e_obj_dialog_new(con, _("About Enform"), "E", "_about");
   if (!od) return NULL;
   e_obj_dialog_obj_theme_set(od, "base/theme/about", "e/widgets/about/main");
   e_obj_dialog_obj_part_text_set(od, "e.text.label", _("Close"));
   e_obj_dialog_obj_part_text_set(od, "e.text.title", _("Enform"));
   e_obj_dialog_obj_part_text_set(od, "e.text.version", ENFORM_VERSION);
   snprintf
     (buf, sizeof(buf), "%s%s",
     _(
       "<title>Copyright &copy; 2016. The Enform Developers.</>"
       "<br><br>"
       "<title>Copyright &copy; 2000-2012. The Enlightenment Developers </>"
       "<br><br>"
       "<title>The \"enlightenment\" is over!<br>"
       "presenting to you:"
       "<br><br><hilight>Enform</><br><br><title>the Window Manager</><br>"
       "<br>"
       "Thanking all the E developers, themers and artists"
       " over the years - current, past and forgotten!"
      ),
       "</>"
     );
   e_obj_dialog_obj_part_text_set(od, "e.textblock.about", buf);

   e_prefix_data_concat_static(buf, "AUTHORS");
   f = fopen(buf, "r");
   if (f)
     {
        tbuf = eina_strbuf_new();
        eina_strbuf_append(tbuf, _("<title>The Team</><br><br>"));
        while (fgets(buf, sizeof(buf), f))
          {
             int len;

             len = strlen(buf);
             if (len > 0)
               {
                  if (buf[len - 1] == '\n')
                    {
                       buf[len - 1] = 0;
                       len--;
                    }
                  if (len > 0)
                    {
                       char *p;

                       do
                         {
                            p = strchr(buf, '<');
                            if (p) *p = 0;
                         }
                       while (p);
                       do
                         {
                            p = strchr(buf, '>');
                            if (p) *p = 0;
                         }
                       while (p);
                       eina_strbuf_append_printf(tbuf, "%s<br>", buf);
                    }
               }
          }
        fclose(f);
        if (tbuf)
          {
             e_obj_dialog_obj_part_text_set(od, "e.textblock.authors",
                                            eina_strbuf_string_get(tbuf));
             eina_strbuf_free(tbuf);
          }
     }
   return (E_About *)od;
}

EAPI void
e_about_show(E_About *about)
{
   e_obj_dialog_show((E_Obj_Dialog *)about);
   e_obj_dialog_icon_set((E_Obj_Dialog *)about, "help-about");
}

