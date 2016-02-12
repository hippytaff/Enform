#include "pa.h"

static Pulse_Server_Info *
deserialize_server_info(Pulse *conn, Pulse_Tag *tag)
{
   Pulse_Server_Info *ev;
   pa_sample_spec spec;

   ev = calloc(1, sizeof(Pulse_Server_Info));
   ev->conn = conn;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, NULL);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &ev->name), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &ev->version), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &ev->username), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &ev->hostname), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_sample(tag, &spec), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &ev->default_sink), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &ev->default_source), error);
   
   return ev;
error:
   pulse_server_info_free(ev);
   return NULL;
}

static void
deserialize_sinks_watcher(Pulse *conn, Pulse_Tag *tag)
{
   pa_subscription_event_type_t e;
   uint32_t idx;

   EINA_SAFETY_ON_FALSE_RETURN(untag_uint32(tag, &e));
   EINA_SAFETY_ON_FALSE_RETURN(untag_uint32(tag, &idx));

   if (e & PA_SUBSCRIPTION_EVENT_CHANGE)
     {
        Pulse_Sink *sink;

        sink = eina_hash_find(pulse_sinks, &idx);
        if (sink)
          {
             if (pulse_sink_get(conn, idx))
               sink->update = EINA_TRUE;
          }
        else
          {
             sink = eina_hash_find(pulse_sources, &idx);
             if (!sink) return;
             if (pulse_source_get(conn, idx))
               sink->update = EINA_TRUE;
          }
     }
}

static Pulse_Sink *
deserialize_sink(Pulse *conn __UNUSED__, Pulse_Tag *tag, Eina_Bool source)
{
   Pulse_Sink *sink = NULL;
   Eina_Bool mute, exist;
   pa_sample_spec spec;
   uint32_t owner_module, monitor_source, flags, base_volume, state, n_volume_steps, card, n_ports;
   uint64_t latency, configured_latency;
   const char *monitor_source_name, *driver;
   Eina_Hash *props = NULL;
   unsigned int x;

   monitor_source_name = driver = NULL;
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &x), error);
   if (source && pulse_sources)
     sink = eina_hash_find(pulse_sources, &x);
   else if ((!source) && pulse_sinks)
     sink = eina_hash_find(pulse_sinks, &x);
   exist = !!sink;
   if (!sink) sink = calloc(1, sizeof(Pulse_Sink));
   sink->index = x;
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &sink->name), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &sink->description), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_sample(tag, &spec), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_channel_map(tag, &sink->channel_map), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &owner_module), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_cvol(tag, &sink->volume), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_bool(tag, &mute), error);
   sink->mute = !!mute;
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &monitor_source), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &monitor_source_name), error);
   eina_stringshare_del(monitor_source_name);
   EINA_SAFETY_ON_FALSE_GOTO(untag_usec(tag, &latency), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &driver), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &flags), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_proplist(tag, &props), error);
   eina_hash_free(props);
   EINA_SAFETY_ON_FALSE_GOTO(untag_usec(tag, &configured_latency), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &base_volume), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &state), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &n_volume_steps), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &card), error);
   EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &n_ports), error);

   for (x = 0; x < n_ports; x++)
     {
        Pulse_Sink_Port_Info *pi;

        pi = calloc(1, sizeof(Pulse_Sink_Port_Info));
        EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &pi->name), error);
        EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &pi->description), error);
        EINA_SAFETY_ON_FALSE_GOTO(untag_uint32(tag, &pi->priority), error);
        sink->ports = eina_list_append(sink->ports, pi);
     }
   EINA_SAFETY_ON_FALSE_GOTO(untag_string(tag, &sink->active_port), error);
   if (exist)
     ecore_event_add(PULSE_EVENT_CHANGE, sink, pulse_fake_free, NULL);
   else
     {
        if (source && (!pulse_sources))
          pulse_sources = eina_hash_int32_new((Eina_Free_Cb)pulse_sink_free);
        else if ((!source) && (!pulse_sinks))
          pulse_sinks = eina_hash_int32_new((Eina_Free_Cb)pulse_sink_free);
        eina_hash_add(source ? pulse_sources : pulse_sinks, (uintptr_t*)&sink->index, sink);
     }
   return sink;
error:
   pulse_sink_free(sink);
   eina_hash_free(props);
   return NULL;
}

Eina_Bool
deserialize_tag(Pulse *conn, PA_Commands command, Pulse_Tag *tag)
{
   Pulse_Cb cb;
   void *ev = (!command) ? NULL : PULSE_SUCCESS;

   cb = eina_hash_find(conn->tag_cbs, &tag->tag_count);
   if (command == PA_COMMAND_SUBSCRIBE)
     conn->watching = EINA_TRUE;
   switch (command)
     {
      case PA_COMMAND_GET_SERVER_INFO:
        if (!cb) return EINA_TRUE;
        ev = NULL;
        ev = deserialize_server_info(conn, tag);
        break;
      case PA_COMMAND_GET_SINK_INFO_LIST:
      case PA_COMMAND_GET_SOURCE_INFO_LIST:
        if (!cb) return EINA_TRUE;
        ev = NULL;
        while (tag->size < tag->dsize - PA_TAG_SIZE_STRING_NULL)
          {
             Pulse_Sink *sink;

             sink = deserialize_sink(conn, tag, (command == PA_COMMAND_GET_SOURCE_INFO_LIST));
             if (!sink)
               {
                  EINA_LIST_FREE(ev, sink)
                    pulse_sink_free(sink);
                  break;
               }
             if (cb) ev = eina_list_append(ev, sink);
          }
        break;
      case PA_COMMAND_GET_SINK_INFO:
      case PA_COMMAND_GET_SOURCE_INFO:
        if ((!cb) && (!conn->watching)) return EINA_TRUE;
        ev = deserialize_sink(conn, tag, (command == PA_COMMAND_GET_SOURCE_INFO));
        break;
      case 0:
        deserialize_sinks_watcher(conn, tag);
        return EINA_TRUE;
      default:
        break;
     }
   if (!cb) return EINA_TRUE;
   eina_hash_del_by_key(conn->tag_cbs, &tag->tag_count);
   cb(conn, tag->tag_count, ev);

   return EINA_TRUE;
}
