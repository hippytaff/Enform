#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifdef __FreeBSD__
# include <sys/types.h>
# include <sys/sysctl.h>
#endif

#ifdef __OpenBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <errno.h>
#include <err.h>
#endif

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>
#include "e_mod_main.h"

static int sensor_type = SENSOR_TYPE_NONE;
static char *sensor_name = NULL;
static int poll_interval = 32;
static int cur_poll_interval = 32;

static char *sensor_path = NULL;
#if defined (__FreeBSD__) || defined (__OpenBSD__)
static int mib[5];
#endif

#ifdef __OpenBSD__
static int dev, numt;
static struct sensordev snsrdev;
static size_t sdlen = sizeof(snsrdev);
static struct sensor snsr;
static size_t slen = sizeof(snsr);
#endif

static Ecore_Poller *poller = NULL;
static int ptemp = 0;

static void      init(void);
static int       check(void);
static Eina_Bool poll_cb(void *data);

Eina_List *
temperature_get_bus_files(const char *bus)
{
   Eina_List *result;
   Eina_List *therms;
   char path[PATH_MAX];
   char busdir[PATH_MAX];
   char *name;

   result = NULL;

   snprintf(busdir, sizeof(busdir), "/sys/bus/%s/devices", bus);
   /* Look through all the devices for the given bus. */
   therms = ecore_file_ls(busdir);

   EINA_LIST_FREE(therms, name)
     {
        Eina_List *files;
        char *file;

        /* Search each device for temp*_input, these should be
         * temperature devices. */
        snprintf(path, sizeof(path), "%s/%s", busdir, name);
        files = ecore_file_ls(path);
        EINA_LIST_FREE(files, file)
          {
             if ((!strncmp("temp", file, 4)) &&
                 (!strcmp("_input", &file[strlen(file) - 6])))
               {
                  char *f;

                  snprintf(path, sizeof(path),
                           "%s/%s/%s", busdir, name, file);
                  f = strdup(path);
                  if (f) result = eina_list_append(result, f);
               }
             free(file);
          }
        free(name);
     }
   return result;
}

static void
init(void)
{
   Eina_List *therms;
   char path[PATH_MAX];
#ifdef __FreeBSD__
   size_t len;
#endif

   if ((!sensor_type) || ((!sensor_name) || (sensor_name[0] == 0)))
     {
        E_FREE(sensor_name);
        E_FREE(sensor_path);
#ifdef __FreeBSD__
        /* TODO: FreeBSD can also have more temperature sensors! */
        sensor_type = SENSOR_TYPE_FREEBSD;
        sensor_name = strdup("tz0");
#elif __OpenBSD__
        mib[0] = CTL_HW;
        mib[1] = HW_SENSORS;

        for (dev = 0;; dev++)
          {
             mib[2] = dev;
             if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1)
               {
                  if (errno == ENOENT) /* no further sensors */
                    break;
                  else
                    continue;
               }
             if (strcmp(snsrdev.xname, "cpu0") == 0)
               {
                  sensor_type = SENSOR_TYPE_OPENBSD;
                  sensor_name = strdup("cpu0");
                  break;
               }
             else if (strcmp(snsrdev.xname, "km0") == 0)
               {
                  sensor_type = SENSOR_TYPE_OPENBSD;
                  sensor_name = strdup("km0");
                  break;
               }
          }
#else
        therms = ecore_file_ls("/proc/acpi/thermal_zone");
        if (therms)
          {
             char *name;

             name = eina_list_data_get(therms);
             sensor_type = SENSOR_TYPE_LINUX_ACPI;
             sensor_name = strdup(name);

             eina_list_free(therms);
          }
        else
          {
             eina_list_free(therms);
             therms = ecore_file_ls("/sys/class/thermal");
             if (therms)
               {
                  char *name;
                  Eina_List *l;

                  EINA_LIST_FOREACH(therms, l, name)
                    {
                       if (!strncmp(name, "thermal", 7))
                         {
                            sensor_type = SENSOR_TYPE_LINUX_SYS;
                            sensor_name = strdup(name);
                            eina_list_free(therms);
                            therms = NULL;
                            break;
                         }
                    }
                  if (therms) eina_list_free(therms);
               }
             if (therms)
               {
                  if (ecore_file_exists("/proc/omnibook/temperature"))
                    {
                       sensor_type = SENSOR_TYPE_OMNIBOOK;
                       sensor_name = strdup("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/temperatures/sensor1_temperature"))
                    {
                       sensor_type = SENSOR_TYPE_LINUX_PBOOK;
                       sensor_name = strdup("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/temperatures/cpu_temperature"))
                    {
                       sensor_type = SENSOR_TYPE_LINUX_MACMINI;
                       sensor_name = strdup("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/platform/coretemp.0/temp1_input"))
                    {
                       sensor_type = SENSOR_TYPE_LINUX_INTELCORETEMP;
                       sensor_name = strdup("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/platform/thinkpad_hwmon/temp1_input"))
                    {
                       sensor_type = SENSOR_TYPE_LINUX_THINKPAD;
                       sensor_name = strdup("dummy");
                    }
                  else
                    {
                       // try the i2c bus
                       therms = temperature_get_bus_files("i2c");
                       if (therms)
                         {
                            char *name;

                            if ((name = eina_list_data_get(therms)))
                              {
                                 if (ecore_file_exists(name))
                                   {
                                      int len;

                                      snprintf(path, sizeof(path),
                                               "%s", ecore_file_file_get(name));
                                      len = strlen(path);
                                      if (len > 6) path[len - 6] = '\0';
                                      sensor_type = SENSOR_TYPE_LINUX_I2C;
                                      sensor_path = strdup(name);
                                      sensor_name = strdup(path);
                                      printf("sensor type = i2c\n"
                                             "sensor path = %s\n"
                                             "sensor name = %s\n",
                                             sensor_path, sensor_name);
                                   }
                              }
                            eina_list_free(therms);
                         }
                       if (!sensor_path)
                         {
                            // try the pci bus
                            therms = temperature_get_bus_files("pci");
                            if (therms)
                              {
                                 char *name;

                                 if ((name = eina_list_data_get(therms)))
                                   {
                                      if (ecore_file_exists(name))
                                        {
                                           int len;

                                           snprintf(path, sizeof(path),
                                                    "%s", ecore_file_file_get(name));
                                           len = strlen(path);
                                           if (len > 6) path[len - 6] = '\0';
                                           sensor_type = SENSOR_TYPE_LINUX_PCI;
                                           sensor_path = strdup(name);
                                           free(sensor_name);
                                           sensor_name = strdup(path);
                                           printf("sensor type = pci\n"
                                                  "sensor path = %s\n"
                                                  "sensor name = %s\n",
                                                  sensor_path, sensor_name);
                                        }
                                   }
                                 eina_list_free(therms);
                              }
                         }
                    }
               }
          }
#endif
     }
   if ((sensor_type) && (sensor_name) && (!sensor_path))
     {
        char *name;

        switch (sensor_type)
          {
           case SENSOR_TYPE_NONE:
             break;

           case SENSOR_TYPE_FREEBSD:
#ifdef __FreeBSD__
             snprintf(path, sizeof(path), "hw.acpi.thermal.%s.temperature",
                      sensor_name);
             sensor_path = strdup(path);
             len = 5;
             sysctlnametomib(sensor_path, mib, &len);
#endif
             break;

           case SENSOR_TYPE_OPENBSD:
#ifdef __OpenBSD__
             for (numt = 0; numt < snsrdev.maxnumt[SENSOR_TEMP]; numt++) {
                  mib[4] = numt;
                  slen = sizeof(snsr);
                  if (sysctl(mib, 5, &snsr, &slen, NULL, 0) == -1)
                    continue;
                  if (slen > 0 && (snsr.flags & SENSOR_FINVALID) == 0)
                    {
                       break;
                    }
               }
#endif

             break;

           case SENSOR_TYPE_OMNIBOOK:
             sensor_path = strdup("/proc/omnibook/temperature");
             break;

           case SENSOR_TYPE_LINUX_MACMINI:
             sensor_path = strdup("/sys/devices/temperatures/cpu_temperature");
             break;

           case SENSOR_TYPE_LINUX_PBOOK:
             sensor_path = strdup("/sys/devices/temperatures/sensor1_temperature");
             break;

           case SENSOR_TYPE_LINUX_INTELCORETEMP:
             sensor_path = strdup("/sys/devices/platform/coretemp.0/temp1_input");
             break;

           case SENSOR_TYPE_LINUX_THINKPAD:
             sensor_path = strdup("/sys/devices/platform/thinkpad_hwmon/temp1_input");
             break;

           case SENSOR_TYPE_LINUX_I2C:
             therms = ecore_file_ls("/sys/bus/i2c/devices");

             EINA_LIST_FREE(therms, name)
               {
                  snprintf(path, sizeof(path),
                           "/sys/bus/i2c/devices/%s/%s_input",
                           name, sensor_name);
                  if (ecore_file_exists(path))
                    {
                       sensor_path = strdup(path);
                       /* We really only care about the first
                        * one for the default. */
                       break;
                    }
                  free(name);
               }
             break;

           case SENSOR_TYPE_LINUX_PCI:
             therms = ecore_file_ls("/sys/bus/pci/devices");

             EINA_LIST_FREE(therms, name)
               {
                  snprintf(path, sizeof(path),
                           "/sys/bus/pci/devices/%s/%s_input",
                           name, sensor_name);
                  if (ecore_file_exists(path))
                    {
                       sensor_path = strdup(path);
                       /* We really only care about the first
                        * one for the default. */
                       break;
                    }
                  free(name);
               }
             break;

           case SENSOR_TYPE_LINUX_ACPI:
             snprintf(path, sizeof(path),
                      "/proc/acpi/thermal_zone/%s/temperature", sensor_name);
             sensor_path = strdup(path);
             break;

           case SENSOR_TYPE_LINUX_SYS:
             snprintf(path, sizeof(path),
                      "/sys/class/thermal/%s/temp", sensor_name);
             sensor_path = strdup(path);
             break;

           default:
             break;
          }
     }
}

static int
check(void)
{
   FILE *f = NULL;
   int ret = 0;
   int temp = 0;
   char buf[4096];
#ifdef __FreeBSD__
   size_t len;
   size_t ftemp = 0;
#endif

   /* TODO: Make standard parser. Seems to be two types of temperature string:
    * - Somename: <temp> C
    * - <temp>
    */
   switch (sensor_type)
     {
      case SENSOR_TYPE_NONE:
        /* TODO: Slow down poller? */
        break;

      case SENSOR_TYPE_FREEBSD:
#ifdef __FreeBSD__
        len = sizeof(temp);
        if (sysctl(mib, 5, &ftemp, &len, NULL, 0) != -1)
          {
             temp = (ftemp - 2732) / 10;
             ret = 1;
          }
        else
          goto error;
#endif
        break;

      case SENSOR_TYPE_OPENBSD:
#ifdef __OpenBSD__
        if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
          {
             temp = (snsr.value - 273150000) / 1000000.0;
             ret = 1;
          }
        else
          goto error;
#endif
        break;

      case SENSOR_TYPE_OMNIBOOK:
        f = fopen(sensor_path, "r");
        if (f)
          {
             char dummy[4096];

             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             if (sscanf(buf, "%s %s %i", dummy, dummy, &temp) == 3)
               ret = 1;
             else
               goto error;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_MACMINI:
      case SENSOR_TYPE_LINUX_PBOOK:
        f = fopen(sensor_path, "rb");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             if (sscanf(buf, "%i", &temp) == 1)
               ret = 1;
             else
               goto error;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_INTELCORETEMP:
      case SENSOR_TYPE_LINUX_I2C:
      case SENSOR_TYPE_LINUX_THINKPAD:
        f = fopen(sensor_path, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             /* actually read the temp */
             if (sscanf(buf, "%i", &temp) == 1)
               ret = 1;
             else
               goto error;
             /* Hack for temp */
             temp = temp / 1000;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_PCI:
        f = fopen(sensor_path, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             /* actually read the temp */
             if (sscanf(buf, "%i", &temp) == 1)
               ret = 1;
             else
               goto error;
             /* Hack for temp */
             temp = temp / 1000;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_ACPI:
        f = fopen(sensor_path, "r");
        if (f)
          {
             char *p, *q;

             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             p = strchr(buf, ':');
             if (p)
               {
                  p++;
                  while (*p == ' ')
                    p++;
                  q = strchr(p, ' ');
                  if (q) *q = 0;
                  temp = atoi(p);
                  ret = 1;
               }
             else
               goto error;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_SYS:
        f = fopen(sensor_path, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             temp = atoi(buf);
             temp /= 1000;
             ret = 1;
          }
        else
          goto error;
        break;

      default:
        break;
     }

   if (ret) return temp;

   return -999;
error:
   if (f) fclose(f);
   sensor_type = SENSOR_TYPE_NONE;
   E_FREE(sensor_name);
   E_FREE(sensor_path);
   return -999;
}

static Eina_Bool
poll_cb(void *data __UNUSED__)
{
   int t, pp;

   t = check();
   pp = cur_poll_interval;
   if (t != ptemp)
     {
        cur_poll_interval /= 2;
        if (cur_poll_interval < 4) cur_poll_interval = 4;
     }
   else
     {
        cur_poll_interval *= 2;
        if (cur_poll_interval > 256) cur_poll_interval = 256;
     }
   /* adapt polling based on if temp changes - every time it changes,
    * halve the time between polls, otherwise double it, between 4 and
    * 256 ticks */
   if (pp != cur_poll_interval)
     {
        if (poller) ecore_poller_del(poller);
        poller = ecore_poller_add(ECORE_POLLER_CORE, cur_poll_interval,
                                  poll_cb, NULL);
     }
   if (t != ptemp)
     {
        if (t == -999) printf("ERROR\n");
        else printf("%i\n", t);
        fflush(stdout);
     }
   ptemp = t;
   return ECORE_CALLBACK_RENEW;
}

int
main(int argc, char *argv[])
{
   if (argc != 4)
     {
        printf("ARGS INCORRECT!\n");
        return 0;
     }
   sensor_type = atoi(argv[1]);
   sensor_name = strdup(argv[2]);
   if (!strcmp(sensor_name, "-null-"))
     E_FREE(sensor_name);
   poll_interval = atoi(argv[3]);
   cur_poll_interval = poll_interval;

   ecore_init();

   init();

   if (poller) ecore_poller_del(poller);
   poller = ecore_poller_add(ECORE_POLLER_CORE, cur_poll_interval,
                             poll_cb, NULL);
   poll_cb(NULL);

   ecore_main_loop_begin();
   ecore_shutdown();

   return 0;
}

