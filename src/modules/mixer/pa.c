#include <Ecore.h>
#include <Ecore_Con.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "pa.h"

static int pulse_init_count = 0;
int pa_log_dom = -1;
int PULSE_EVENT_CONNECTED = -1;
int PULSE_EVENT_DISCONNECTED = -1;
int PULSE_EVENT_CHANGE = -1;
Eina_Hash *pulse_sinks = NULL;
Eina_Hash *pulse_sources = NULL;

void
pulse_fake_free(void *d __UNUSED__, void *d2 __UNUSED__)
{}

static void
proplist_init(Pulse_Tag *tag)
{
   Eina_File *file;
   size_t size;
   const char *str;
   int argc;
   char **argv;
   char pid[32];
   tag->props = eina_hash_string_superfast_new((void*)eina_stringshare_del);
/*
    "L\0\0\0\tL\0\0\0\1"
    "P"
    "tapplication.process.id\0"
    "L\0\0\0\5x\0\0\0\0052742\0"
    "tapplication.process.user\0"
    "L\0\0\0\5x\0\0\0\5root\0"
    "tapplication.process.host\0"
    "L\0\0\0\fx\0\0\0\fdarc.ath.cx\0"
    "tapplication.process.binary\0"
    "L\0\0\0\6x\0\0\0\6pactl\0"
    "tapplication.name\0"
    "L\0\0\0\6x\0\0\0\6pactl\0"
    "tapplication.language\0"
    "L\0\0\0\vx\0\0\0\ven_US.utf8\0"
    "twindow.x11.display\0"
    "L\0\0\0\5x\0\0\0\5:0.0\0"
    "tapplication.process.machine_id\0"
    "L\0\0\0!x\0\0\0!16ebb73850fbfdfff2b0079400000030\0"
    "N"
*/
   tag->dsize += PA_TAG_SIZE_PROPLIST;
   snprintf(pid, sizeof(pid), "%"PRIu32, getpid());
   eina_hash_add(tag->props, "application.process.id", eina_stringshare_add(pid));
   tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.id") + strlen(pid) + 2 + PA_TAG_SIZE_U32;
   
   str = getenv("USER");
   eina_hash_add(tag->props, "application.process.user", eina_stringshare_add(str));
   tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.user") + strlen(str) + 2 + PA_TAG_SIZE_U32;
   
   file = eina_file_open("/etc/hostname", EINA_FALSE);
   if (file)
     {
        size = eina_file_size_get(file);
        str = (const char*)eina_file_map_all(file, EINA_FILE_POPULATE);
        eina_hash_add(tag->props, "application.process.host", eina_stringshare_add_length(str, size - 1));
        tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.host") + size + 1 + PA_TAG_SIZE_U32;
        eina_file_map_free(file, (void*)str);
        eina_file_close(file);
     }
   else
     {
        eina_hash_add(tag->props, "application.process.host", eina_stringshare_add(""));
        tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.host") + 2 + PA_TAG_SIZE_U32;
     }
   
   ecore_app_args_get(&argc, &argv);
   str = strrchr(argv[0], '/');
   str = (str) ? str + 1 : argv[0];
   eina_hash_add(tag->props, "application.process.binary", eina_stringshare_add(str));
   tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.binary") + strlen(str) + 2 + PA_TAG_SIZE_U32;
   eina_hash_add(tag->props, "application.name", eina_stringshare_add(str));
   tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.name") + strlen(str) + 2 + PA_TAG_SIZE_U32;
   
   str = getenv("LANG");
   if (str)
     {
        eina_hash_add(tag->props, "application.language", eina_stringshare_add(str));
        tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.language") + strlen(str) + 2 + PA_TAG_SIZE_U32;
     }
   
   str = getenv("DISPLAY");
   if (str)
     {
        eina_hash_add(tag->props, "window.x11.display", eina_stringshare_add(str));
        tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("window.x11.display") + strlen(str) + 2 + PA_TAG_SIZE_U32;
     }

   file = eina_file_open(PA_MACHINE_ID, EINA_FALSE);
   if (file)
     {
        size = eina_file_size_get(file);
        str = (const char*)eina_file_map_all(file, EINA_FILE_POPULATE);
        eina_hash_add(tag->props, "application.process.machine_id", eina_stringshare_add_length(str, size - 1));
        tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.machine_id") + size + 1 + PA_TAG_SIZE_U32;
        eina_file_map_free(file, (void*)str);
        eina_file_close(file);
        return;
     }
   {
      char buf[256];

      errno = 0;
      gethostname(buf, sizeof(buf));
      if (!errno)
        {
           eina_hash_add(tag->props, "application.process.machine_id", eina_stringshare_add(buf));
           tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.machine_id") + strlen(buf) + 2 + PA_TAG_SIZE_U32;
           return;
        }
      snprintf(buf, sizeof(buf), "%08lx", (unsigned long)gethostid());
      eina_hash_add(tag->props, "application.process.machine_id", eina_stringshare_add(buf));
      tag->dsize += PA_TAG_SIZE_ARBITRARY + sizeof("application.process.machine_id") + strlen(buf) + 2 + PA_TAG_SIZE_U32;
   }
}

static void
cookie_file(uint8_t *cookie)
{
   char buf[4096];
   Eina_File *file;
   size_t size;
   void *cookie_data;

   snprintf(buf, sizeof(buf), "%s/.pulse-cookie", getenv("HOME"));   
   file = eina_file_open(buf, EINA_FALSE);
   size = eina_file_size_get(file);
   cookie_data = eina_file_map_all(file, EINA_FILE_WILLNEED);
   memcpy(cookie, cookie_data, size);
   eina_file_map_free(file, cookie_data);
   eina_file_close(file);
}

static Pulse_Tag *
login_setup(Pulse *conn)
{
   Pulse_Tag *tag;
   uint32_t x;
   uint8_t cookie[PA_NATIVE_COOKIE_LENGTH];
   
   tag = calloc(1, sizeof(Pulse_Tag));
   tag->dsize = 4 * PA_TAG_SIZE_U32 + sizeof(cookie);
   tag->data = malloc(tag->dsize);
   tag_simple_init(conn, tag, PA_COMMAND_AUTH, PA_TAG_U32);
   DBG("%zu bytes", tag->dsize);
   
   if (!getuid())
     x = PA_PROTOCOL_VERSION;
   else
     x = PA_PROTOCOL_VERSION | 0x80000000U;
   tag_uint32(tag, x);
   DBG("%zu bytes", tag->dsize);
   
   cookie_file(cookie);
   tag_arbitrary(tag, cookie, sizeof(cookie));
   DBG("%zu bytes", tag->dsize);

   tag_finish(tag);
   
   return tag;
}

static Eina_Bool
pulse_recv(Pulse *conn, Ecore_Fd_Handler *fdh, Pulse_Tag **ret_tag)
{
   Pulse_Tag *tag;
   uint32_t x;

   if (ret_tag) *ret_tag = NULL;
   tag = eina_list_data_get(conn->iq);
   if (!tag)
     {
        tag = calloc(1, sizeof(Pulse_Tag));
        conn->iq = eina_list_append(conn->iq, tag);
     }
   if (!tag->auth)
     {
        msg_recv_creds(conn, tag);
        if (!tag->auth) return EINA_FALSE;
     }
   if (!tag->data)
     {
        tag->dsize = ntohl(tag->header[PA_PSTREAM_DESCRIPTOR_LENGTH]);
        if (!tag->dsize)
          {
             ERR("Kicked!");
             pulse_disconnect(conn);
             return EINA_FALSE;
          }
        tag->data = malloc(tag->dsize);
     }
   if (tag->pos < tag->dsize)
     {
        if (!msg_recv(conn, tag))
          return EINA_FALSE;
     }
   untag_uint32(tag, &x);
   EINA_SAFETY_ON_TRUE_GOTO((x != PA_COMMAND_REPLY) && (x != PA_COMMAND_SUBSCRIBE_EVENT), error);
   tag->command = x;
   if (x == PA_COMMAND_REPLY)
     untag_uint32(tag, &tag->tag_count);
   else
     tag->size += PA_TAG_SIZE_U32;
   if (conn->state != PA_STATE_CONNECTED)
     {
        ecore_main_fd_handler_active_set(fdh, ECORE_FD_WRITE);
        pulse_tag_free(tag);
     }
   else if (ret_tag)
     *ret_tag = tag;
   return EINA_TRUE;
error:
   ERR("Received error command %"PRIu32"!", x);
   pulse_tag_free(tag);
   return EINA_FALSE;
}

static void
login_finish(Pulse *conn, Ecore_Fd_Handler *fdh)
{
   Pulse_Tag *tag;

   tag = calloc(1, sizeof(Pulse_Tag));
   tag->dsize = 2 * PA_TAG_SIZE_U32; /* tag_simple_init */
   proplist_init(tag);
   DBG("prep %zu bytes", tag->dsize);
   tag->data = malloc(tag->dsize);
   tag_simple_init(conn, tag, PA_COMMAND_SET_CLIENT_NAME, PA_TAG_U32);
   tag_proplist(tag);
   tag_finish(tag);
   msg_send_creds(conn, tag);
   conn->state++;
   if (msg_send(conn, tag))
     ecore_main_fd_handler_active_set(fdh, ECORE_FD_READ);
   else
     conn->oq = eina_list_append(conn->oq, tag);
}

static Eina_Bool
fdh_func(Pulse *conn, Ecore_Fd_Handler *fdh)
{
   Pulse_Tag *rprev, *wprev;
   int pa_read, pa_write;

   if (conn->watching) pa_read = ECORE_FD_READ;
   else
     pa_read = !!ecore_main_fd_handler_active_get(fdh, ECORE_FD_READ) * ECORE_FD_READ;
   pa_write = !!ecore_main_fd_handler_active_get(fdh, ECORE_FD_WRITE) * ECORE_FD_WRITE;
   rprev = eina_list_data_get(conn->iq);
   wprev = eina_list_data_get(conn->oq);
   
   switch (conn->state)
     {
      case PA_STATE_INIT:
        if (!wprev)
          {
             wprev = login_setup(conn);
             conn->oq = eina_list_append(conn->oq, wprev);
          }

        if (!wprev->auth)
          msg_sendmsg_creds(conn, wprev);
          

        if (wprev->auth && msg_send(conn, wprev))
          {
             conn->state++;
             ecore_main_fd_handler_active_set(fdh, ECORE_FD_READ);
          }
        break;
      case PA_STATE_AUTH:
        if (pulse_recv(conn, fdh, NULL))
          login_finish(conn, fdh);
        break;
      case PA_STATE_MOREAUTH:
        if (pa_write)
          {
             if (msg_send(conn, wprev))
               ecore_main_fd_handler_active_set(fdh, ECORE_FD_READ);
             break;
          }
        if (pulse_recv(conn, fdh, NULL))
          {
             conn->state++;
             INF("Login complete!");
             ecore_main_fd_handler_active_set(fdh, 0);
             ecore_event_add(PULSE_EVENT_CONNECTED, conn, pulse_fake_free, NULL);
          }
        break;
      case PA_STATE_CONNECTED:
        if (pa_write)
          {
             if (wprev)
               {
                  DBG("write");
                  if (!wprev->auth)
                    msg_send_creds(conn, wprev);
                  if (wprev->auth)
                    {
                       if (msg_send(conn, wprev))
                         ecore_main_fd_handler_active_set(conn->fdh, ECORE_FD_READ | (!!conn->oq * ECORE_FD_WRITE));
                    }
               }
             else
               ecore_main_fd_handler_active_set(conn->fdh, ECORE_FD_READ);
          }
        if (pa_read)
          {
             DBG("read");
             if ((!rprev) || (!rprev->auth) || (rprev->pos < rprev->dsize))
               {
                  Pulse_Tag *tag;
                  PA_Commands command;
                  if (!pulse_recv(conn, fdh, &tag)) break;
                       
                  command = (uintptr_t)eina_hash_find(conn->tag_handlers, &tag->tag_count);
                  eina_hash_del_by_key(conn->tag_handlers, &tag->tag_count);
                  deserialize_tag(conn, command, tag);
                  if (!eina_list_count(conn->oq))
                    ecore_main_fd_handler_active_set(conn->fdh, pa_write | conn->watching * ECORE_FD_READ);
                  pulse_tag_free(tag);
               }
          }
      default:
        break;
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
con(Pulse *conn, int type __UNUSED__, Ecore_Con_Event_Server_Add *ev)
{
   int on = 1;
   int fd;
   int flags;

   if (conn != ecore_con_server_data_get(ev->server)) return ECORE_CALLBACK_PASS_ON;
   INF("connected to %s", ecore_con_server_name_get(ev->server));

   fd = ecore_con_server_fd_get(ev->server);
   if (fd == -1)
     {
        pulse_disconnect(conn);
        return ECORE_CALLBACK_RENEW;
     }
   conn->fd = dup(fd);
#ifdef SO_PASSCRED
   setsockopt(conn->fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
#endif
   setsockopt(conn->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
   fcntl(conn->fd, F_SETFL, O_NONBLOCK);

   flags = fcntl(conn->fd, F_GETFD);
   flags |= FD_CLOEXEC;
   fcntl(conn->fd, F_SETFD, flags);

   conn->fdh = ecore_main_fd_handler_add(conn->fd, ECORE_FD_WRITE, (Ecore_Fd_Cb)fdh_func, conn, NULL, NULL);
   ecore_con_server_del(conn->svr);
   conn->svr = NULL;
   return ECORE_CALLBACK_RENEW;
}

uint32_t
pulse_cards_get(Pulse *conn)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type = PA_COMMAND_GET_CARD_INFO_LIST;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   tag->dsize = 2 * PA_TAG_SIZE_U32;
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return tag->tag_count;
}

void
pulse_cb_set(Pulse *conn, uint32_t tagnum, Pulse_Cb cb)
{
   EINA_SAFETY_ON_NULL_RETURN(conn);

   if (cb) eina_hash_set(conn->tag_cbs, &tagnum, cb);
   else eina_hash_del_by_key(conn->tag_cbs, &tagnum);
}

uint32_t
pulse_type_get(Pulse *conn, uint32_t idx, Eina_Bool source)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type = source ? PA_COMMAND_GET_SOURCE_INFO : PA_COMMAND_GET_SINK_INFO;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   tag->dsize = 3 * PA_TAG_SIZE_U32 + PA_TAG_SIZE_STRING_NULL;
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_uint32(tag, idx);
   tag_string(tag, NULL);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return tag->tag_count;
}

uint32_t
pulse_types_get(Pulse *conn, Eina_Bool source)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type = source ? PA_COMMAND_GET_SOURCE_INFO_LIST : PA_COMMAND_GET_SINK_INFO_LIST;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   tag->dsize = 2 * PA_TAG_SIZE_U32;
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return tag->tag_count;
}

uint32_t
pulse_type_mute_set(Pulse *conn, uint32_t sink_num, Eina_Bool mute, Eina_Bool source)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type = source ? PA_COMMAND_SET_SOURCE_MUTE : PA_COMMAND_SET_SINK_MUTE;
   Eina_Hash *h;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   tag->dsize = 3 * PA_TAG_SIZE_U32 + PA_TAG_SIZE_STRING_NULL + PA_TAG_SIZE_BOOLEAN;
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_uint32(tag, sink_num);
   tag_string(tag, NULL);
   tag_bool(tag, !!mute);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   h = (source) ? pulse_sources : pulse_sinks;
   if (h)
     {
        Pulse_Sink *sink;

        sink = eina_hash_find(h, &sink_num);
        if (sink) sink->mute = !!mute;
     }
   return tag->tag_count;
}

uint32_t
pulse_type_volume_set(Pulse *conn, uint32_t sink_num, uint8_t channels, double vol, Eina_Bool source)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type = source ? PA_COMMAND_SET_SOURCE_MUTE : PA_COMMAND_SET_SINK_VOLUME;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   tag->dsize = 3 * PA_TAG_SIZE_U32 + PA_TAG_SIZE_STRING_NULL + PA_TAG_SIZE_CVOLUME + channels * sizeof(uint32_t);
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_uint32(tag, sink_num);
   tag_string(tag, NULL);
   tag_volume(tag, channels, vol);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return tag->tag_count;
}

uint32_t
pulse_server_info_get(Pulse *conn)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type = PA_COMMAND_GET_SERVER_INFO;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   tag->dsize = 2 * PA_TAG_SIZE_U32;
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return tag->tag_count;
}

uint32_t
pulse_sink_channel_volume_set(Pulse *conn, Pulse_Sink *sink, uint32_t id, double vol)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(id >= sink->channel_map.channels, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   type = sink->source ? PA_COMMAND_SET_SOURCE_VOLUME : PA_COMMAND_SET_SINK_VOLUME;
   tag->dsize = 3 * PA_TAG_SIZE_U32 + PA_TAG_SIZE_STRING_NULL + PA_TAG_SIZE_CVOLUME + sink->channel_map.channels * sizeof(uint32_t);
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   if (vol <= 0.0) sink->volume.values[id] = PA_VOLUME_MUTED;
   else sink->volume.values[id] = (vol * PA_VOLUME_NORM) / 100;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_uint32(tag, sink->index);
   tag_string(tag, NULL);
   tag_cvol(tag, &sink->volume);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return tag->tag_count;
}

uint32_t
pulse_sink_port_set(Pulse *conn, Pulse_Sink *sink, const char *port)
{
   Pulse_Tag *tag;
   int pa_read;
   uint32_t type;
   Eina_List *l;
   const char *p;
   Eina_Bool match = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);
   EINA_SAFETY_ON_NULL_RETURN_VAL(port, 0);
   EINA_LIST_FOREACH(sink->ports, l, p)
     {
        match = !strcmp(p, port);
        if (match) break;
     }
   EINA_SAFETY_ON_TRUE_RETURN_VAL(!match, 0);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, 0);
   type = sink->source ? PA_COMMAND_SET_SOURCE_PORT : PA_COMMAND_SET_SINK_PORT;
   tag->dsize = PA_TAG_SIZE_U32 + 2 * PA_TAG_SIZE_STRING + strlen(sink->name) + strlen(port);
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_uint32(tag, sink->index);
   tag_string(tag, sink->name);
   tag_string(tag, port);
   tag_finish(tag);
   pa_read = !!ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ) * ECORE_FD_READ;
   ecore_main_fd_handler_active_set(conn->fdh, pa_read | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return tag->tag_count;
}

Eina_Bool
pulse_sinks_watch(Pulse *conn)
{
   Pulse_Tag *tag;
   uint32_t type = PA_COMMAND_SUBSCRIBE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, EINA_FALSE);
   tag = calloc(1, sizeof(Pulse_Tag));
   EINA_SAFETY_ON_NULL_RETURN_VAL(tag, EINA_FALSE);
   tag->dsize = 3 * PA_TAG_SIZE_U32;
   tag->data = malloc(tag->dsize);
   tag->tag_count = conn->tag_count;
   tag_simple_init(conn, tag, type, PA_TAG_U32);
   tag_uint32(tag, PA_SUBSCRIPTION_MASK_ALL);
   tag_finish(tag);
   ecore_main_fd_handler_active_set(conn->fdh, ECORE_FD_READ | ECORE_FD_WRITE);
   conn->oq = eina_list_append(conn->oq, tag);
   eina_hash_add(conn->tag_handlers, &tag->tag_count, (uintptr_t*)((uintptr_t)type));
   return EINA_TRUE;
}

int
pulse_init(void)
{
   if (pulse_init_count++) return pulse_init_count;
   
   eina_init();
   ecore_init();
   ecore_con_init();
   pa_log_dom = eina_log_domain_register("pulse", EINA_COLOR_HIGH EINA_COLOR_BLUE);

   PULSE_EVENT_CONNECTED = ecore_event_type_new();
   PULSE_EVENT_DISCONNECTED = ecore_event_type_new();
   PULSE_EVENT_CHANGE = ecore_event_type_new();

   return pulse_init_count;
}

void
pulse_shutdown(void)
{
   if ((!pulse_init_count) || (!--pulse_init_count)) return;

   if (pulse_sinks) eina_hash_free(pulse_sinks);
   if (pulse_sources) eina_hash_free(pulse_sources);
   pulse_sinks = pulse_sources = NULL;
   eina_log_domain_unregister(pa_log_dom);
   ecore_con_shutdown();
   ecore_shutdown();
   eina_shutdown();
}

Pulse *
pulse_new(void)
{
   Pulse *conn;
   Eina_Iterator *it;
   const char *prev = NULL, *buf = NULL;
   time_t t = 0;
   char *home, h[4096];
   const Eina_File_Direct_Info *info;

   conn = calloc(1, sizeof(Pulse));
   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, NULL);
   home = getenv("PULSE_RUNTIME_PATH");

   if (!home)
     {
        home = getenv("HOME");
        snprintf(h, sizeof(h), "%s/.pulse", home);
        home = h;
     }

   it = eina_file_direct_ls(home);
   EINA_ITERATOR_FOREACH(it, info)
     {
        const char *rt = NULL;

        rt = strrchr(info->path + info->name_start, '-');
        if (rt)
          {
             if (!strcmp(rt + 1, "runtime"))
               {
                  struct stat st;
                  buf = eina_stringshare_printf("%s/native", info->path);
                  if (stat(buf, &st))
                    {
                       eina_stringshare_del(buf);
                       buf = NULL;
                       continue;
                    }
                  if (!t)
                    {
                       t = st.st_atime;
                       prev = buf;
                       buf = NULL;
                       continue;
                    }
                  if (t > st.st_atime)
                    {
                       eina_stringshare_del(buf);
                       buf = NULL;
                       continue;
                    }
                  eina_stringshare_del(prev);
                  prev = buf;
                  t = st.st_atime;
                  buf = NULL;
               }
          }
     }
   eina_iterator_free(it);
   if (!prev)
     {
        struct stat st;
        buf = eina_stringshare_add(STATEDIR "/run/pulse/native");
        if (stat(buf, &st))
          {
             INF("could not locate local socket '%s'!", buf);
             free(conn);
             return NULL;
          }
        conn->socket = buf;
     }
   else conn->socket = prev;
   conn->con = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_ADD, (Ecore_Event_Handler_Cb)con, conn);
   conn->tag_handlers = eina_hash_int32_new(NULL);
   conn->tag_cbs = eina_hash_int32_new(NULL);
   return conn;
}

void
pulse_free(Pulse *conn)
{
   Pulse_Tag *tag;
   if (!conn) return;
   if (conn->fdh) ecore_main_fd_handler_del(conn->fdh);
   else if (conn->svr) ecore_con_server_del(conn->svr);
   if (conn->con) ecore_event_handler_del(conn->con);
   eina_stringshare_del(conn->socket);
   EINA_LIST_FREE(conn->oq, tag)
     pulse_tag_free(tag);
   EINA_LIST_FREE(conn->iq, tag)
     pulse_tag_free(tag);
   eina_hash_free(conn->tag_handlers);
   eina_hash_free(conn->tag_cbs);
   free(conn);
}

Eina_Bool
pulse_connect(Pulse *conn)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, EINA_FALSE);
   conn->svr = ecore_con_server_connect(ECORE_CON_LOCAL_SYSTEM, conn->socket, -1, conn);
   return !!conn->svr;
}

void
pulse_disconnect(Pulse *conn)
{
   Eina_Bool event = EINA_FALSE;
   EINA_SAFETY_ON_NULL_RETURN(conn);

   conn->state = PA_STATE_INIT;
   if (conn->fdh)
     {
        ecore_main_fd_handler_del(conn->fdh);
        conn->fdh = NULL;
        close(conn->fd);
        conn->fd = -1;
        event = EINA_TRUE;
     }
   else if (conn->svr)
     {
        ecore_con_server_del(conn->svr);
        conn->svr = NULL;
        event = EINA_TRUE;
     }
   if (event)
     ecore_event_add(PULSE_EVENT_DISCONNECTED, conn, pulse_fake_free, NULL);
}

void
pulse_server_info_free(Pulse_Server_Info *ev)
{
   if (!ev) return;

   eina_stringshare_del(ev->name);
   eina_stringshare_del(ev->version);
   eina_stringshare_del(ev->username);
   eina_stringshare_del(ev->hostname);
   eina_stringshare_del(ev->default_sink);
   eina_stringshare_del(ev->default_source);
   free(ev);
}
