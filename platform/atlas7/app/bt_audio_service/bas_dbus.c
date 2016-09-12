/* 
  Copyright (c) 2016, The Linux Foundation. All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials provided
        with the distribution.
      * Neither the name of The Linux Foundation nor the names of its
        contributors may be used to endorse or promote products derived
        from this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "bas_dbus.h"
#include "bas_main.h"

typedef enum
{
    DBUS_FLAG_DEPRECATED   = (1 << 0),
    DBUS_FLAG_EXPERIMENTAL = (1 << 1),
} DBusFlags;

typedef struct
{
    const char *name;
    const char *signature;
} DBusArgInfo;

typedef DBusMessage *(* DBusMethodFunction) (DBusConnection *connection,
                                             DBusMessage *message, void *user_data);
typedef struct
{
    const char          *name;
    DBusMethodFunction  function;
    DBusFlags           flags;
    const DBusArgInfo   *in_args;
    const DBusArgInfo   *out_args;
} DBusMethodTable;

#define DBUS_ARGS(args...) (const DBusArgInfo[]) { args, {NULL,NULL} }

#define CONNECT_RETRY_INTERVAL                      1
#define CSR_SFL_HF_VOICECALLMANAGER_INTERFACE       "com.csr.synergy.voicecallmanager"
#define AVRCP_MEDIA_TRANSPORT_DBUS_INTERFACE        "com.csr.synergy.avrcp.MediaTransport"

static DBusMessage *bas_start_audio(DBusConnection *connection,
                                    DBusMessage *message,
                                    void *user_data);
static DBusHandlerResult bas_message_handler(DBusConnection *connection,
                                             DBusMessage *message, void *user_data);

static const DBusMethodTable bas_methods[] =
{
    {
        BT_AUDIO_SERVICE_METHOD_START_AUDIO,
        bas_start_audio,
        0,
        NULL,
        DBUS_ARGS({ "profile", "s" }, { "value", "b" })
    },

    {
        NULL,
        NULL,
        0,
        NULL,
        NULL
    }
};

static const DBusObjectPathVTable object_vtable =
{
    .message_function = bas_message_handler,
};

static DBusMessage *bas_start_audio(DBusConnection *connection,
                                    DBusMessage *message,
                                    void *user_data)
{
    char *profile;
    dbus_bool_t start;
    unsigned int sample_rate;
    DBusError error;
    unsigned char result = FALSE;
    DBusMessage *reply;
    bas_instance_t *inst = (bas_instance_t *)user_data;

    BAS_FUNC_ENTER();

    do
    {
        dbus_error_init(&error);

        if (NULL == inst)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("Error user data ")));
            break;

        }

        if (dbus_message_get_args(message, &error,
                                  DBUS_TYPE_STRING, &profile,
                                  DBUS_TYPE_BOOLEAN, &start,
                                  DBUS_TYPE_UINT32, &sample_rate,
                                  DBUS_TYPE_INVALID) == FALSE)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("Get parameter error: '%s' "), error.message));
            dbus_error_free(&error);
            break;
        }

        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Profile:%s, Start:%s, sample rate:%d"), 
                        profile, (0 == start) ? "FALSE" : "TRUE", sample_rate));
        
        if (strcmp(profile, BT_AUDIO_SERVICE_HFP_AUDIO) == 0)
        {

            if (start)
            {
                if (bas_hfp_start_audio(inst, sample_rate))
                {
                    result = TRUE;
                    break;
                }
            }
            else
            {
                if (bas_hfp_stop_audio(inst))
                {
                    result = TRUE;
                    break;
                }
            }
        }
        /*Obsolete. A2dp audio is not handled by bt audio service for 8311 and QCA chip*/
        else if (strcmp(profile, BT_AUDIO_SERVICE_A2DP_AUDIO) == 0)
        {
            if (start)
            {
                if (bas_a2dp_start_audio(inst))
                {
                    result = TRUE;
                    break;
                }
            }
            else
            {
                if (bas_a2dp_stop_audio(inst))
                {
                    result = TRUE;
                    break;
                }
            }

            break;
        }
        else
            break;

    }
    while (0);

    if (result)
    {
        reply = dbus_message_new_method_return(message);
    }
    else
    {
        reply = dbus_message_new_error(message, BT_AUDIO_SERVICE_DBUS_ERROR, BT_AUDIO_SERVICE_DBUS_ERROR_MESSAGE);

    }

    if (!reply)
    {
        IFDBG(DebugOut(DEBUG_ERROR, TEXT("Create reply message fail")));
        return NULL;
    }

    BAS_FUNC_LEAVE();
    return reply;
}

static DBusHandlerResult base_process_message(DBusConnection *connection,
                                              DBusMessage *message, const DBusMethodTable *method,
                                              void *user_data)
{
    DBusMessage *reply;

    BAS_FUNC_ENTER();
    IFDBG(DebugOut(DEBUG_INIT, TEXT("conn: %p, msg: %p, method: %p, data: %p"),
                   connection, message, method, user_data));

    reply = method->function(connection, message, user_data);

    if (NULL != reply)
    {
        dbus_connection_send(connection, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    else
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    BAS_FUNC_LEAVE();
}

static DBusHandlerResult bas_message_handler(DBusConnection *connection,
                                             DBusMessage *message, void *user_data)
{
    const DBusMethodTable *method;
    const char *interface;
    DBusHandlerResult dbusResult;

    BAS_FUNC_ENTER();
    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("conn: %p, msg: %p, data: %p"), connection, message, user_data));

    interface = dbus_message_get_interface(message);

    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Get interface: '%s' "), interface));


    for (method = &bas_methods[0]; method &&
         method->name && method->function; method++)
    {
        if (dbus_message_is_method_call(message, interface,
                                        method->name) == FALSE)
        {
            continue;
        }

        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Method call %s found"), method->name));
        /*
        if (check_message_signature(method->in_args, message) == FALSE)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("Checking signature failed of method call '%s' "), method->name));
            continue;
        }
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Checking signature passed")));
        */
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("process_message")));

        dbusResult = base_process_message(connection, message, method,
                                          user_data);
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("DBus result: %d"), dbusResult));

        return dbusResult;
    }

    IFDBG(DebugOut(DEBUG_WARN, TEXT("Cannot find the method")));
    BAS_FUNC_LEAVE();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult bas_event_filter_handler(DBusConnection *conn, DBusMessage *msg, void *data)
{
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_is_signal(msg, CSR_SFL_HF_VOICECALLMANAGER_INTERFACE, "PropertyChanged")
        || dbus_message_is_signal(msg, AVRCP_MEDIA_TRANSPORT_DBUS_INTERFACE, "PropertyChanged"))
    {
        DBusMessageIter iter, subiter;
        const char *name;

        dbus_message_iter_init(msg, &iter);
        dbus_message_iter_get_basic(&iter, &name);

        if (strcmp("speakergain_value", name) == 0)
        {
            unsigned char volume;

            /* Handle hfp sco volume adjust, range : 0-15 */
            dbus_message_iter_next(&iter);
            dbus_message_iter_recurse(&iter, &subiter);
            dbus_message_iter_get_basic(&subiter, &volume);

            IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Set HFP Vol [%d]"), volume));

            if ((volume > 15) || bas_set_mixer_volume("Voicecall Stream Vol", volume * 100 / 15) < 0)
            {
                IFDBG(DebugOut(DEBUG_ERROR, TEXT("Fail to Set HFP Vol [%d]"), volume));
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        }
        else if (strcmp("Volume", name) == 0)
        {
            unsigned short volume;

            /* Handle avrcp absolute volume adjust, range : 0-127 */
            dbus_message_iter_next(&iter);
            dbus_message_iter_recurse(&iter, &subiter);
            dbus_message_iter_get_basic(&subiter, &volume);

            IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Set AVRCP Absolute Vol [%d]"), volume));

            if ((volume > 127) || bas_set_mixer_volume("Music Stream Vol", volume * 100 / 127) < 0)
            {
                IFDBG(DebugOut(DEBUG_ERROR, TEXT("Fail to Set AVRCP Absolute Vol [%d]"), volume));
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* Server thread that will receive message from the other side
 *
 * This thread will receive messages from the other side and forward
 * them to the scheduler by the message handler function registered in
 * event dispatch function
 */
static void *bas_dbus_thread(void *thread_param)
{
    DBusError err;
    bas_instance_t *inst = (bas_instance_t *)thread_param;

    BAS_FUNC_ENTER();
    dbus_error_init(&err);

    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Flush dbus")));

    dbus_connection_flush(inst->dbus_con);

    if (dbus_error_is_set(&err))
    {
        IFDBG(DebugOut(DEBUG_ERROR, TEXT("Flush error (%s)"), err.message));
        return NULL;
    }

    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("dbus_connection_read_write_dispatch")));

    /* loop listening for signals & methods being emmitted */
    while (inst->run && dbus_connection_read_write_dispatch(inst->dbus_con, -1));

    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Loop over")));
    BAS_FUNC_LEAVE();
    return NULL;
}

/*This function should be used as a common one*/
static void connectDbusInfinitely(DBusBusType type, DBusConnection **conn, unsigned int time)
{
    DBusError err;
    DBusConnection *connection;

    BAS_FUNC_ENTER();
    dbus_error_init(&err);

    /*connect dbus continuouly until success*/
    do
    {
        connection = dbus_bus_get(type, &err);
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("DBus connection id: %p"), connection));

        if (dbus_error_is_set(&err))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("Connection fail, err: '%s' "), err.message));
            dbus_error_free(&err);
            sleep(time);
            IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Retrying dbus connection")));

        }

    }
    while (NULL == connection);

    *conn = connection;
    BAS_FUNC_LEAVE();
}

/* Initialize dbus for bt audio service */
unsigned char bas_dbus_init(bas_instance_t *inst)
{
    int res;
    DBusError err;
    unsigned char result = FALSE;

    BAS_FUNC_ENTER();

    do
    {
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Starting audio dbus service...")));
        /* initialise the errors */
        dbus_error_init(&err);
        dbus_threads_init_default();
        connectDbusInfinitely(DBUS_BUS_SYSTEM, &inst->dbus_con, CONNECT_RETRY_INTERVAL);
        dbus_connection_set_exit_on_disconnect(inst->dbus_con, TRUE);
        dbus_bus_add_match(inst->dbus_con, "type='signal', sender='com.csr.synergy'", &err);

        if (dbus_error_is_set(&err))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("dbus_bus_add_match failed!")));
            dbus_error_free(&err);
            break;
        }

        res = dbus_bus_request_name(inst->dbus_con, BT_AUDIO_SERVICE_DBUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("dbus_bus_request_name '%s'"), BT_AUDIO_SERVICE_DBUS_NAME));

        if (dbus_error_is_set(&err))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("dbus_bus_request_name fail, err: '%s' "), err.message));
            dbus_error_free(&err);
            break;
        }

        if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != res)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("DBUS not primary owner")));
            break;
        }

        inst->run = TRUE;

        if (!dbus_connection_register_object_path(inst->dbus_con,
                                                  BT_AUDIO_SERVICE_DBUS_OBJ_PATH,
                                                  &object_vtable, inst))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("Can't register object path '%s' "), BT_AUDIO_SERVICE_DBUS_OBJ_PATH));
            break;
        }

        if (!dbus_connection_add_filter(inst->dbus_con, bas_event_filter_handler, NULL, NULL))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("Add filter error")));
            break;
        }

        res = pthread_create(&inst->tid, NULL, bas_dbus_thread, inst);

        if (res != 0)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("Failed to create rx-thread")));
            break;
        }

        result = TRUE;
    }
    while (0);

    BAS_FUNC_LEAVE();
    return result;
}
