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
#include <alsa/asoundlib.h>
#include "bas_util.h"
#include "bas_hfp.h"

void bas_hfp_close_audio_handle(bas_instance_t *inst)
{
    BAS_FUNC_ENTER();

    if (NULL == inst)
        return;

    if (NULL != inst->hfp_record_handle)
    {
        snd_pcm_unlink(inst->hfp_record_handle);
        snd_pcm_close(inst->hfp_record_handle);
        inst->hfp_record_handle = NULL;
    }

    if (NULL != inst->hfp_play_handle)
    {
        snd_pcm_close(inst->hfp_play_handle);
        inst->hfp_play_handle = NULL;
    }

    BAS_FUNC_LEAVE();
}

unsigned char bas_hfp_start_audio(bas_instance_t *inst, unsigned int sample_rate)
{
    int err;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t chunk_size = 0;
    char *dummy_data;
    int card_id, device_id;
    char bt_to_iacc_device[16];
    char iacc_to_bt_device[16];
    unsigned char result = FALSE;

    BAS_FUNC_ENTER();

    do
    {
        if (inst->hfp_audio_started)
        {
            result = TRUE;
            break;
        }

        err = bas_get_voicecall_device_id("Voicecall-bt-to-iacc",
                                          SND_PCM_STREAM_PLAYBACK, &card_id, &device_id);

        if (err != 0)
        {
            printf("Get Voicecall-bt-to-iacc device id failed\n");
            break;
        }

        sprintf(bt_to_iacc_device, "hw:%d,%d", card_id, device_id);
        err = bas_get_voicecall_device_id("Voicecall-iacc-to-bt",
                                          SND_PCM_STREAM_CAPTURE, &card_id, &device_id);

        if (err != 0)
        {
            printf("Get Voicecall-iacc-to-bt device id failed\n");
            break;
        }

        sprintf(iacc_to_bt_device, "hw:%d,%d", card_id, device_id);
        printf("bt_to_iacc_device: %s\n", bt_to_iacc_device);
        err = snd_pcm_open(&inst->hfp_play_handle, bt_to_iacc_device,
                           SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

        if (err < 0)
        {
            printf("audio open error: %s\n", snd_strerror(err));
            break;
        }

        printf("iacc_to_bt_device: %s\n", iacc_to_bt_device);
        err = snd_pcm_open(&inst->hfp_record_handle, iacc_to_bt_device,
                           SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);

        if (err < 0)
        {
            printf("audio open error: %s\n", snd_strerror(err));
            snd_pcm_close(inst->hfp_play_handle);
            break;
        }

        printf("Sample rate: %d\n", sample_rate);
        err = snd_pcm_set_params(inst->hfp_play_handle, SND_PCM_FORMAT_S16_LE,
                                 SND_PCM_ACCESS_RW_INTERLEAVED,
                                 1, sample_rate, 1, 256 * 1000000 / sample_rate);

        if (err < 0)
        {
            printf("Set parameters failed for %s.", snd_strerror(err));
            break;
        }

        err = snd_pcm_set_params(inst->hfp_record_handle, SND_PCM_FORMAT_S16_LE,
                                 SND_PCM_ACCESS_RW_INTERLEAVED,
                                 inst->mic, sample_rate, 1, 256 * 1000000 / sample_rate);

        if (err < 0)
        {
            printf("Set parameters failed for %s.", snd_strerror(err));
            break;
        }

        snd_pcm_link(inst->hfp_record_handle, inst->hfp_play_handle);
        snd_pcm_get_params(inst->hfp_play_handle, &buffer_size, &chunk_size);
        printf("buffer_size = %d, chunk_size = %d\n", (int)buffer_size, (int)chunk_size);
        dummy_data = malloc(buffer_size * 8);

        snd_pcm_prepare(inst->hfp_play_handle);
        /* Write a piece date to fill the buffer, the playback device can be start. */
        snd_pcm_writei(inst->hfp_play_handle, dummy_data, buffer_size);
        snd_pcm_start(inst->hfp_record_handle);
        free(dummy_data);
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
