/*
  Copyright (c) 2016, 2019-2020 The Linux Foundation. All rights reserved.

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
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "bas_util.h"
#include "bas_hfp.h"

#define SND_HFP_DOWNLINK_PLAYBACK_DEV_NAME      "MultiMedia21"
#define SND_HFP_DOWNLINK_CAPTURE_DEV_NAME       "MultiMedia21"
#define SND_HFP_UPLINK_PLAYBACK_DEV_NAME        "MultiMedia6"
#define SND_HFP_UPLINK_CAPTURE_DEV_NAME         "MultiMedia6"

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

const char *stream_name[stream_link_max][stream_connect_max] =
{
    { SND_HFP_DOWNLINK_PLAYBACK_DEV_NAME,  SND_HFP_DOWNLINK_CAPTURE_DEV_NAME },
    { SND_HFP_UPLINK_PLAYBACK_DEV_NAME,    SND_HFP_UPLINK_CAPTURE_DEV_NAME }
};

static void init_amixer(unsigned int rate)
{
    BAS_FUNC_ENTER();

    if (rate == 16000)
    {
        system("amixer -c 0 cset iface=MIXER,name='PRIM_AUX_PCM_RX SampleRate' KHZ_16");
        system("amixer -c 0 cset iface=MIXER,name='PRIM_AUX_PCM_TX SampleRate' KHZ_16");
    }
    else if (rate == 8000)
    {
        system("amixer -c 0 cset iface=MIXER,name='PRIM_AUX_PCM_RX SampleRate' KHZ_8");
        system("amixer -c 0 cset iface=MIXER,name='PRIM_AUX_PCM_TX SampleRate' KHZ_8");
    }
    else
        printf("Unsupported rate for HF audio: %d\n", rate);

    system("amixer -c 0 cset iface=MIXER,name='AUX_PCM_RX Audio Mixer MultiMedia6' 1");
    system("amixer -c 0 cset iface=MIXER,name='MultiMedia6 Mixer TERT_TDM_TX_0' 1");
    system("amixer -c 0 cset iface=MIXER,name='MultiMedia21 Mixer AUX_PCM_UL_TX' 1");
    system("amixer -c 0 cset iface=MIXER,name='TERT_TDM_RX_2 Audio Mixer MultiMedia21' 1");

    BAS_FUNC_LEAVE();
}

static snd_pcm_t *create_stream(bas_instance_t *inst,
                                stream_link_dir_t dir,
                                stream_connect_t ep,
                                sco_type_t sco_type,
                                unsigned int rate,
                                unsigned char channel)
{
    int err;
    int card_id = 0, device_id = 0;
    char device[16];
    snd_pcm_t *handle = NULL;
    snd_pcm_hw_params_t *hwparams;

    snd_pcm_hw_params_alloca(&hwparams);

    BAS_FUNC_ENTER();

    if (inst == NULL
        || dir >= stream_link_max || ep >= stream_connect_max || sco_type >= sco_max )
        return NULL;

    do
    {
        err = bas_get_voicecall_device_id(stream_name[dir][ep],
                                          ep == stream_playback ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE,
                                          &card_id, &device_id);

        if (err != 0)
        {
            printf("Get device %s id failed\n", stream_name[dir][ep]);
            break;
        }

        sprintf(device, "hw:%d,%d", card_id, device_id);

        printf("hfp dir[%d] ep[%d] device id: %s\n", dir, ep, device);
        err = snd_pcm_open(&handle, device,
                           ep == stream_playback ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE,
                           SND_PCM_NONBLOCK);

        if (err < 0)
        {
            printf("audio open error: %s\n", snd_strerror(err));
            break;
        }

        if ((err = snd_pcm_hw_params_any(handle, hwparams)) < 0)
        {
            printf("snd_pcm_hw_params_any() failed on playback PCM\n");
        }

        if ((err = snd_pcm_hw_params_set_rate(handle, hwparams, rate, 0)) < 0)
        {
            printf("snd_pcm_hw_params_set_rate() failed on playback PCM\n");
        }

        if ((err = snd_pcm_hw_params_set_access(handle, hwparams,  SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        {
            printf("snd_pcm_hw_params_set_access() failed on playback PCM\n");
        }

        if ((err = snd_pcm_hw_params_set_format(handle, hwparams, SND_PCM_FORMAT_S16_LE)) < 0)
        {
            printf("snd_pcm_hw_params_set_format() failed on playback PCM\n");
        }

        if ((err = snd_pcm_hw_params_set_channels(handle, hwparams, channel)) < 0)
        {
            printf("snd_pcm_hw_params_set_channels() failed on playback PCM\n");
        }

        if ((err = snd_pcm_hw_params(handle, hwparams)) < 0)
        {
            printf("snd_pcm_hw_params() failed on playback PCM\n");
        }
    }
    while(0);

    BAS_FUNC_LEAVE();
    return handle;
}

void bas_hfp_close_audio_handle(bas_instance_t *inst)
{
    BAS_FUNC_ENTER();

    if (NULL == inst)
        return;

    if (NULL != inst->hfp_downlink_record_handle)
    {
        snd_pcm_close(inst->hfp_downlink_record_handle);
        inst->hfp_downlink_record_handle = NULL;
    }

    if (NULL != inst->hfp_uplink_record_handle)
    {
        snd_pcm_close(inst->hfp_uplink_record_handle);
        inst->hfp_uplink_record_handle = NULL;
    }

    if (NULL != inst->hfp_downlink_play_handle)
    {
        snd_pcm_close(inst->hfp_downlink_play_handle);
        inst->hfp_downlink_play_handle = NULL;
    }

    if (NULL != inst->hfp_uplink_play_handle)
    {
        snd_pcm_close(inst->hfp_uplink_play_handle);
        inst->hfp_uplink_play_handle = NULL;
    }

    BAS_FUNC_LEAVE();
}

unsigned char bas_hfp_start_audio(bas_instance_t *inst, unsigned int sample_rate)
{
#if 0
    int err;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t chunk_size = 0;
    char *dummy_data;
#endif
    unsigned char result = FALSE;
    int sco_type = 0;

    BAS_FUNC_ENTER();

    do
    {
        if (inst->hfp_audio_started)
        {
            result = TRUE;
            break;
        }

        printf("Sample rate: %d\n", sample_rate);

        init_amixer(sample_rate);

        if (sample_rate == 8000)
            sco_type = sco_nb;
        else if (sample_rate == 16000)
            sco_type = sco_wb;
        else
            printf("Unsupported sample rate: %d\r\n", sample_rate);

        /* 1): connect stream uplink playback */
        inst->hfp_uplink_play_handle = create_stream(inst, stream_uplink, stream_playback, sco_type, sample_rate, 1);
        if (inst->hfp_uplink_play_handle == NULL)
            break;

        /* 2) : connect stream uplink capture */
        inst->hfp_uplink_record_handle = create_stream(inst, stream_uplink, stream_record, sco_type, 48000, 1);
        if (inst->hfp_uplink_record_handle == NULL)
            break;

        /* 3):  connect stream downlink playback */
        inst->hfp_downlink_play_handle = create_stream(inst, stream_downlink, stream_playback, sco_type, 48000, 1);
        if (inst->hfp_downlink_play_handle == NULL)
            break;

        /* 4): connect stream downlink capture */
        inst->hfp_downlink_record_handle = create_stream(inst, stream_downlink, stream_record, sco_type, sample_rate, 1);
        if (inst->hfp_downlink_record_handle == NULL)
            break;

        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("All stream create finish, BT audio Start!!")));

        snd_pcm_prepare(inst->hfp_downlink_play_handle);
        snd_pcm_prepare(inst->hfp_uplink_play_handle);
        snd_pcm_prepare(inst->hfp_downlink_record_handle);
        snd_pcm_prepare(inst->hfp_uplink_record_handle);
        snd_pcm_start(inst->hfp_downlink_record_handle);
        snd_pcm_start(inst->hfp_uplink_record_handle);
        snd_pcm_start(inst->hfp_downlink_play_handle);
        snd_pcm_start(inst->hfp_uplink_play_handle);

        inst->hfp_audio_started = TRUE;
        result = TRUE;
    }
    while (0);

    if (!result)
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
