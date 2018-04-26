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

#include <signal.h>
#include "bas_main.h"
#include "bas_hfp.h"
#include "bas_a2dp.h"

extern unsigned char bas_dbus_init(bas_instance_t *inst);

static bas_instance_t *bas_inst;
static void signal_handler(int signum)
{
    BAS_FUNC_ENTER();

    switch (signum)
    {
        case SIGPIPE:
        {
            IFDBG(DebugOut(DEBUG_MESSAGE, TEXT("Ignore SIGPIPE")));
            break;
        }

        case SIGHUP:
        {
            IFDBG(DebugOut(DEBUG_MESSAGE, TEXT("SIGHUP")));
            break;
        }

        case SIGINT:
        {
            IFDBG(DebugOut(DEBUG_MESSAGE, TEXT("SIGINT")));
            break;
        }

        case SIGTERM:
        {
            IFDBG(DebugOut(DEBUG_MESSAGE, TEXT("SIGTERM")));
            break;
        }

        case SIGSEGV:
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("SIGSEGV got. Segment fault")));
            break;
        }

        case SIGABRT:
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("SIGABRT got. Process abort")));
            break;
        }

        default:
        {
            IFDBG(DebugOut(DEBUG_WARN, TEXT("Unknown signal: %d"), signum));
            break;
        }
    }

    bas_hfp_close_audio_handle(bas_inst);
    bas_a2dp_close_audio_handle(bas_inst);
    IFDBG(DebugOut(DEBUG_ERROR, TEXT("Quit Bt Audio Service")));
    BAS_FUNC_LEAVE();
    exit(0);
}

static unsigned char bas_signal_init()
{
    struct sigaction actNew, actOld;

    BAS_FUNC_ENTER();
    sigemptyset (&actNew.sa_mask);
    actNew.sa_handler   = signal_handler;
    actNew.sa_flags     = 0;

    /* SIGPIPE */
    sigaction (SIGPIPE, NULL, &actOld);

    if (actOld.sa_handler != SIG_IGN)
    {
        sigaction (SIGPIPE, &actNew, NULL);
    }

    /* SIGHUP */
    sigaction (SIGHUP, NULL, &actOld);

    if (actOld.sa_handler != SIG_IGN)
    {
        sigaction (SIGHUP, &actNew, NULL);
    }

    /* SIGINT */
    sigaction (SIGINT, NULL, &actOld);

    if (actOld.sa_handler != SIG_IGN)
    {
        sigaction (SIGINT, &actNew, NULL);
    }

    /* SIGTERM */
    sigaction (SIGTERM, NULL, &actOld);

    if (actOld.sa_handler != SIG_IGN)
    {
        sigaction (SIGTERM, &actNew, NULL);
    }

    /* SIGSEGV */
    sigaction (SIGSEGV, NULL, &actOld);

    if (actOld.sa_handler != SIG_IGN)
    {
        sigaction (SIGSEGV, &actNew, NULL);
    }

    BAS_FUNC_LEAVE();
    return TRUE;
}

static unsigned char bas_instance_init(bas_instance_t **inst)
{
    BAS_FUNC_ENTER();
    bas_inst = (bas_instance_t *)malloc(sizeof(bas_instance_t));

    if (NULL == bas_inst)
    {
        IFDBG(DebugOut(DEBUG_ERROR, TEXT("Malloc failed")));
        return FALSE;
    }
    memset(bas_inst, 0, sizeof(bas_instance_t));

    bas_inst->a2dp_audio_started = FALSE;
    bas_inst->a2dp_play_handle   = NULL;
    bas_inst->dbus_con           = NULL;
    bas_inst->hfp_audio_started  = FALSE;
    bas_inst->hfp_play_handle    = NULL;
    bas_inst->hfp_record_handle  = NULL;
    bas_inst->mic                = 1;
    bas_inst->run                = FALSE;
    bas_inst->tid                = 0;

    /* for PulseAudio Control */
    bas_inst->mainloop           = NULL;
    bas_inst->api                = NULL;
    bas_inst->ctx                = NULL;
    bas_inst->downlink_p_stream  = NULL;
    bas_inst->downlink_c_stream  = NULL;
    bas_inst->uplink_p_stream    = NULL;
    bas_inst->uplink_c_stream    = NULL;
    
    *inst = bas_inst;
    BAS_FUNC_LEAVE();
    return TRUE;
}

static void bas_instance_deinit()
{
    BAS_FUNC_ENTER();

    if (NULL != bas_inst)
    {
        free(bas_inst);
    }

    bas_inst = NULL;
    BAS_FUNC_LEAVE();
}

static void bas_deinit()
{
    //    bas_dbus_deinit();
    //    bas_signal_deinit();
    BAS_FUNC_ENTER();
    bas_instance_deinit();
    BAS_FUNC_LEAVE();
}

static void bas_set_mic(bas_instance_t *inst, unsigned char mic)
{
    BAS_FUNC_ENTER();
    IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("mic number:%d"), mic));

    if (mic != 1 && mic != 2)
    {
        inst->mic = 1;
    }
    else
    {
        inst->mic = mic;
    }

    BAS_FUNC_LEAVE();
}

int main(int argc, char **argv)
{
    unsigned int mic = 1;
    bas_instance_t *inst;

    BAS_FUNC_ENTER();
    //    IFDBG(DebugInitialize(DEFAULT_OUTPUT_MODE, DEFAULT_OUTPUT_MASK, NULL));
    IFDBG(DebugOut(DEBUG_MESSAGE, TEXT("\nbt_audio_service build time:%s, %s"), __DATE__, __TIME__));

    if (!bas_instance_init(&inst))
        return -1;

    if (argc == 2)
    {
        sscanf(argv[1], "%d", &mic);
        bas_set_mic(inst, mic);
    }

    if (!bas_dbus_init(inst))
        return -1;

    if (!bas_signal_init())
        return -1;

    while (1)
    {
        sleep(1000);
    }

    BAS_FUNC_LEAVE();
    return 0;
}

