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

#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include "bas_util.h"
#include "bas_hfp.h"

typedef enum 
{
    stream_downlink,
    stream_uplink,

    stream_link_max  
    
} stream_link_dir_t;

typedef enum 
{
    sco_nb,     /* for narrow band : 8k */
    sco_wb,     /* for wide band : 16k */

    sco_max  
    
} sco_type_t;

typedef enum 
{
    stream_playback,
    stream_record,
    
    stream_connect_max  
    
} stream_connect_t;

/* Difference between 8K and 16k  ----> nb and wb */
const char *proplist_value[stream_link_max][sco_max] =
{
    { "hfp_downlink", "hfp_wb_downlink" },
    { "hfp_uplink",   "hfp_wb_uplink" }
};

const char *stream_name[stream_link_max][stream_connect_max] =
{
    { "hfp_downlink_playback",  "hfp_downlink_capture" },
    { "hfp_uplink_playback",    "hfp_uplink_capture" }
};

static void sample_context_state_cb (pa_context *context, void *userdata)
{
    pa_context_state_t state;
    pa_threaded_mainloop *mainloop = (pa_threaded_mainloop *) userdata;

    state = pa_context_get_state (context);

	IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Got new context state %d"), state));

    switch (state) 
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:      
            IFDBG(DebugOut(DEBUG_MESSAGE, TEXT("Signaling")));
            pa_threaded_mainloop_signal (mainloop, 0);
            break;

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
  }
}

static void pa_wait_for_context_ready (pa_context *context, void *userdata)
{
    pa_stream_state_t state;
    pa_threaded_mainloop *mainloop = (pa_threaded_mainloop *) userdata;
  
    for (;;) 
    {
        pa_context_state_t state;
  
        state = pa_context_get_state (context);
  
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Context state is now %d"), state));
  
        if (!PA_CONTEXT_IS_GOOD (state))
            return;
  
        if (state == PA_CONTEXT_READY)
            break;
  
        /* Wait until the context is ready */
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Waiting...")));
        pa_threaded_mainloop_wait(mainloop);
    }
}

static void sample_stream_state_cb (pa_stream *stream, void *userdata)
{
    pa_stream_state_t state;
    pa_threaded_mainloop *mainloop = (pa_threaded_mainloop *) userdata;

    state = pa_stream_get_state (stream);  
    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Got new stream state %d"), state));

    switch (state) 
    {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:            
            IFDBG(DebugOut(DEBUG_MESSAGE, TEXT("Signaling")));
            pa_threaded_mainloop_signal (mainloop, 0);
            break;

        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

static void pa_wait_for_stream_ready (pa_stream *stream, void *userdata)
{
    pa_stream_state_t state;
    pa_threaded_mainloop *mainloop = (pa_threaded_mainloop *) userdata;

    for (;;) 
    {
        state = pa_stream_get_state (stream);
        
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Stream state is now %d"), state));

        if (!PA_STREAM_IS_GOOD (state))
            return;

        if (state == PA_STREAM_READY)
            break;

        /* Wait until the stream is ready */
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Waiting...")));
        pa_threaded_mainloop_wait (mainloop);
  }
}

static unsigned char connect_server(bas_instance_t *inst)
{
    unsigned char result = FALSE;

    if (inst == NULL || inst->mainloop == NULL)
        return result;

    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Connecting to server ... ")));
    
    pa_threaded_mainloop_lock (inst->mainloop);

    do
    {   
        if (!(inst->api = pa_threaded_mainloop_get_api (inst->mainloop)))        
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_threaded_mainloop_get_api failed")));
            break;
        }
        
        if (!(inst->ctx = pa_context_new (inst->api , NULL)))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_context_new failed")));
            break;
        }

        /* register some essential callbacks */
        pa_context_set_state_callback (inst->ctx, sample_context_state_cb, inst->mainloop);

        if (pa_context_connect (inst->ctx, NULL,0,NULL) < 0)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_context_connect failed")));
            break;
        }

        pa_wait_for_context_ready(inst->ctx, inst->mainloop);

        result = TRUE;
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Connecting with server success")));
        
    }
    while(0);  
    
    pa_threaded_mainloop_unlock (inst->mainloop);   

    return result;
        
}


static pa_stream* create_stream(bas_instance_t *inst, 
                                    stream_link_dir_t dir, 
                                    stream_connect_t ep, 
                                    sco_type_t sco_type,
                                    unsigned int rate, 
                                    unsigned char channel)
{
    pa_stream *stream = NULL;
    pa_proplist *link_proplist = NULL;
    int latency = 20000; // start latency in micro seconds    
    pa_sample_spec ss;
    pa_buffer_attr bufattr;
    int ret;  

    if (inst == NULL || inst->mainloop == NULL 
        || dir >= stream_link_max || ep >= stream_connect_max || sco_type >= sco_max )
        return NULL;

    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("create stream, link_dir:[%d], stream_connect [%d]!!"), dir, ep));

    pa_threaded_mainloop_lock (inst->mainloop);

    do
    {
        ss.rate     = rate;
        ss.channels = channel;
        ss.format   = PA_SAMPLE_S16LE;

        bufattr.fragsize    = (uint32_t)-1;
        bufattr.maxlength   = pa_usec_to_bytes(latency,&ss);
        bufattr.minreq      = pa_usec_to_bytes(0,&ss);
        bufattr.prebuf      = (uint32_t)-1;
        bufattr.tlength     = pa_usec_to_bytes(latency,&ss);

        if (!(link_proplist = pa_proplist_new()))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_proplist_new failed")));
            break;        
        }
        if (pa_proplist_sets(link_proplist, PA_PROP_MEDIA_ROLE, proplist_value[dir][sco_type]) < 0)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_proplist_sets failed")));
            break;        
        } 
        
        if (!(stream = pa_stream_new_with_proplist(inst->ctx, stream_name[dir][ep],&ss, NULL,link_proplist)))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_stream_new_with_proplist failed")));
            break;        
        } 
        
        pa_stream_set_state_callback(stream, sample_stream_state_cb, inst->mainloop);
        
        /* Connect to Sink or Source */
        if (ep == stream_playback)
        {
            ret = pa_stream_connect_playback(stream, NULL, &bufattr,
                                            PA_STREAM_INTERPOLATE_TIMING
                                            |PA_STREAM_ADJUST_LATENCY
                                            |PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL);
        }
        else
        {
            ret = pa_stream_connect_record(stream, NULL, &bufattr,
                                    PA_STREAM_INTERPOLATE_TIMING
                                    |PA_STREAM_ADJUST_LATENCY
                                    |PA_STREAM_AUTO_TIMING_UPDATE);        
        }
        
        if (ret < 0) 
        {            
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_stream_connect failed !!!")));
            break;
        }

        pa_wait_for_stream_ready(stream, inst->mainloop); 
        
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("create stream success!!")));
    }
    while (0);

    /* free resource : proplist */
    if (link_proplist)
    {
        pa_proplist_free(link_proplist);
        link_proplist = NULL;
    }
    
    pa_threaded_mainloop_unlock (inst->mainloop);

    return stream;

}

static void pa_wait_for_context_disconnect (pa_context *context, void *userdata)
{
    pa_stream_state_t state;
    pa_threaded_mainloop *mainloop = (pa_threaded_mainloop *) userdata;
  
    for (;;) 
    {
        pa_context_state_t state;
  
        state = pa_context_get_state (context);
  
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Context state is now %d"), state));
  
        if (!PA_CONTEXT_IS_GOOD (state))
            return;
  
        if (state == PA_CONTEXT_TERMINATED)
            break;
  
        /* Wait until the context is TERMINATED */
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Waiting...")));
        pa_threaded_mainloop_wait (mainloop);
    }
}



static void pa_wait_for_stream_disconnect (pa_stream *stream, void *userdata)
{
    pa_stream_state_t state;
    pa_threaded_mainloop *mainloop = (pa_threaded_mainloop *) userdata;

    for (;;) 
    {
        state = pa_stream_get_state (stream);
        
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Stream state is now %d"), state));

        if (!PA_STREAM_IS_GOOD (state))
            return;

        if (state == PA_STREAM_TERMINATED)
            break;

        /* Wait until the stream is TERMINATED */
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("Waiting...")));
        pa_threaded_mainloop_wait (mainloop);
  }
}

static void disconnect_stream(bas_instance_t *inst, pa_stream **stream)
{
    if (inst == NULL || inst->mainloop == NULL || *stream == NULL)
        return; 

    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("disconnect_stream...")));
    
    pa_threaded_mainloop_lock (inst->mainloop);    
	pa_stream_disconnect(*stream);
    pa_wait_for_stream_disconnect(*stream, inst->mainloop);		
	pa_stream_unref(*stream);
    pa_threaded_mainloop_unlock (inst->mainloop);

    *stream == NULL;
}


void bas_hfp_close_audio_handle(bas_instance_t *inst)
{
    BAS_FUNC_ENTER();

     if (inst == NULL || inst->mainloop == NULL)
        return;

    /* Disconnect 4 Stream Lines */
    disconnect_stream(inst, &inst->downlink_p_stream);
    disconnect_stream(inst, &inst->downlink_c_stream);
    disconnect_stream(inst, &inst->uplink_p_stream);
    disconnect_stream(inst, &inst->uplink_c_stream);  

    /*Disconnect Server */
	pa_threaded_mainloop_lock (inst->mainloop);
	pa_context_disconnect(inst->ctx);
	pa_wait_for_context_disconnect(inst->ctx, inst->mainloop);
	pa_context_unref(inst->ctx);
	pa_threaded_mainloop_unlock (inst->mainloop);
    
	pa_threaded_mainloop_stop ( inst->mainloop);
	pa_threaded_mainloop_free ( inst->mainloop);
    inst->mainloop = NULL;
	
    BAS_FUNC_LEAVE();
}

unsigned char bas_hfp_start_audio(bas_instance_t *inst, unsigned int sample_rate)
{
    unsigned char result = FALSE; 
    int ret = 0; 
    sco_type_t sco_type = sco_nb;
   
    BAS_FUNC_ENTER();          

    do
    {
        /* create threaded mainloop */
        inst->mainloop = pa_threaded_mainloop_new ();
        if (pa_threaded_mainloop_start (inst->mainloop) < 0) 
        {
            pa_threaded_mainloop_free (inst->mainloop);
            inst->mainloop = NULL;
            break;
        }

        /* connect to server */
        if (!connect_server(inst))
            break; 

        if (sample_rate == 16000)
            sco_type = sco_wb;
            
        /* 1):  connect stream downlink playback */
        inst->downlink_p_stream = create_stream(inst, stream_downlink, stream_playback, sco_type, 48000, 2);
        if (inst->downlink_p_stream == NULL)
            break;    
        
        /* 2): connect stream downlink capture */
        /* TODO : 8K and 16K handle */
        inst->downlink_c_stream = create_stream(inst, stream_downlink, stream_record, sco_type, sample_rate, 1);
        if (inst->downlink_c_stream == NULL)
            break; 

        /*3): connect stream uplink playback */
        inst->uplink_p_stream = create_stream(inst, stream_uplink, stream_playback, sco_type, sample_rate, 1);
        if (inst->uplink_p_stream == NULL)
            break; 

        /*4) : connect stream uplink capture */
        inst->uplink_c_stream = create_stream(inst, stream_uplink, stream_record, sco_type, 48000, 2);
        if (inst->uplink_c_stream == NULL)
            break;       
        
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("All stream create finish, BT audio Start!!")));  
	
        result = TRUE;

    }while(0);

    if(!result)
		bas_hfp_close_audio_handle(inst);
    
    BAS_FUNC_LEAVE();  
    return result;
    
}


unsigned char bas_hfp_stop_audio(bas_instance_t *inst)
{
    BAS_FUNC_ENTER();
    bas_hfp_close_audio_handle(inst);
    inst->hfp_audio_started = FALSE;
    BAS_FUNC_LEAVE();
}
