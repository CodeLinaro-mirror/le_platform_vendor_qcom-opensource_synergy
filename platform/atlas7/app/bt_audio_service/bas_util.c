/*
 *
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Based on vplay program by Michael Beck
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/* Modifications provided under 
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
#include <alsa/mixer.h>
#include "bas_main.h"

int bas_get_voicecall_device_id(char *device_name, snd_pcm_stream_t stream,
                                int *card_id, int *device_id)
{
    snd_ctl_t *handle;
    int card, err, dev, idx;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t *pcminfo;
    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcminfo);

    BAS_FUNC_ENTER();
    card = -1;

    if (snd_card_next(&card) < 0 || card < 0)
    {
        printf("no soundcards found...");
        return;
    }

    while (card >= 0)
    {
        char name[32];
        sprintf(name, "hw:%d", card);

        if ((err = snd_ctl_open(&handle, name, 0)) < 0)
        {
            printf("control open (%i): %s", card,
                   snd_strerror(err));
            goto next_card;
        }

        if ((err = snd_ctl_card_info(handle, info)) < 0)
        {
            printf("control hardware info (%i): %s", card,
                   snd_strerror(err));
            snd_ctl_close(handle);
            goto next_card;
        }

        dev = -1;

        while (1)
        {
            unsigned int count;

            if (snd_ctl_pcm_next_device(handle, &dev) < 0)
                printf("snd_ctl_pcm_next_device");

            if (dev < 0)
                break;

            snd_pcm_info_set_device(pcminfo, dev);
            snd_pcm_info_set_subdevice(pcminfo, 0);
            snd_pcm_info_set_stream(pcminfo, stream);

            if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0)
            {
                if (err != -ENOENT)
                    printf("control digital audio info (%i): %s", card, snd_strerror(err));

                continue;
            }

            if ((!strcmp(snd_ctl_card_info_get_id(info), "kasaudiocard"))
                && (!strncmp(snd_pcm_info_get_id(pcminfo), device_name,
                             strlen(device_name))))
            {
                *card_id = card;
                *device_id = dev;
                snd_ctl_close(handle);
                return 0;
            }
        }

        snd_ctl_close(handle);
    next_card:

        if (snd_card_next(&card) < 0)
        {
            printf("snd_card_next");
            break;
        }
    }

    BAS_FUNC_LEAVE();
    return -ENODEV;
}

int bas_set_mixer_volume(char *mixer_name, long volume)
{
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    int err;
    const char *card = "default";
    long min, max;

    BAS_FUNC_ENTER();

    printf("Set [%s] Volume to [%ld]\n", mixer_name, volume);

    snd_mixer_selem_id_alloca(&sid);

    /* sets mixer index and name */
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, mixer_name);


    if ((err = snd_mixer_open(&handle, 0)) < 0)
    {
        printf("Unable to open mixer: %s", snd_strerror(err));
        return -1;
    }

    if ((err = snd_mixer_attach(handle, card)) < 0)
    {
        printf("Unable to attach mixer to device %s: %s", card, snd_strerror(err));
        snd_mixer_close(handle);
        return -1;
    }

    if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0)
    {
        printf("Unable to register mixer elements: %s", snd_strerror(err));
        snd_mixer_close(handle);
        return -1;
    }

    if ((err = snd_mixer_load(handle)) < 0)
    {
        printf("Unable to load mixer elements: %s", snd_strerror(err));
        snd_mixer_close(handle);
        return -1;
    }

    elem = snd_mixer_find_selem(handle, sid);

    if (!elem)
    {
        printf("Unable to find mixer elements");
        snd_mixer_close(handle);
        return -1;
    }


    if ((err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max)) < 0)
    {
        printf("Unable to get playback volume range: %s", snd_strerror(err));
        snd_mixer_close(handle);
        return -1;
    }

    /* The original range is 0-96, however, volume value under 50 is hard to hear! */
    /* So make some adjustments: just using range 48 - 96 indeed, that is, supposing the min value is 48, not 0 */
    /*if ((err = snd_mixer_selem_set_playback_volume_all(elem, volume * (max - min) / 100)) < 0)*/
    if ((err = snd_mixer_selem_set_playback_volume_all(elem, (100 + volume) * (max - min) / 200)) < 0)
    {
        printf("Unable to set playback volume to [%lds]: %s", volume, snd_strerror(err));
        snd_mixer_close(handle);
        return -1;
    }

    snd_mixer_close(handle);

    BAS_FUNC_LEAVE();

    return 0;

}

