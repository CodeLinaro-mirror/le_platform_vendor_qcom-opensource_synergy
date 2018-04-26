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

#ifndef _BAS_MAIN_H
#define _BAS_MAIN_H

#include <pthread.h>
#include <sys/types.h>
#include <dbus/dbus.h>
#include <asoundlib.h>
#include <pulse/pulseaudio.h>

typedef struct
{
    pthread_t               tid;
    DBusConnection         *dbus_con;
    unsigned char           run;
    unsigned char           hfp_audio_started;
    unsigned char           a2dp_audio_started;
    snd_pcm_t              *hfp_play_handle;
    snd_pcm_t              *hfp_record_handle;  
    snd_pcm_t              *a2dp_play_handle;
    unsigned char           mic;

    /* for PulseAudio control */
    pa_threaded_mainloop    *mainloop;
    pa_mainloop_api         *api;
    pa_context              *ctx;
    pa_stream               *downlink_p_stream;
    pa_stream               *downlink_c_stream;
    pa_stream               *uplink_p_stream;
    pa_stream               *uplink_c_stream;

} bas_instance_t;

#define DEBUG_INIT             1
#define DEBUG_OUTPUT           1
#define DEBUG_MESSAGE          1
#define DEBUG_WARN             1
#define DEBUG_ERROR            1

#define IFDBG(x) x
#define TEXT(str) str
#define DebugOut(x, fmt, ...)	{ \
                                    if (x) \
                             	    { \
                             	        printf("%s:", __FUNCTION__); \
                             	        printf(fmt, ## __VA_ARGS__); \
                             	        printf("\n"); \
                             	    } \
                             	}

#define FUNC_ENTER_STRING       TEXT("+++")
#define FUNC_LEAVE_STRING       TEXT("---")
#define BAS_FUNC_ENTER()        IFDBG(DebugOut(DEBUG_OUTPUT, FUNC_ENTER_STRING))
#define BAS_FUNC_LEAVE()        IFDBG(DebugOut(DEBUG_OUTPUT, FUNC_LEAVE_STRING))

#endif
