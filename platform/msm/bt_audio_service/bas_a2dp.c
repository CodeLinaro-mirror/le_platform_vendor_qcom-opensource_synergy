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
#include <signal.h>
#include <pulse/pulseaudio.h>
#include "bas_util.h"
#include "bas_a2dp.h"

void bas_a2dp_close_audio_handle(bas_instance_t *inst)
{
    BAS_FUNC_ENTER();

    if (NULL == inst)
    {
        printf("inst is NULL");
        return;
    }

    if (NULL != inst->a2dp_play_handle)
    {
        inst->a2dp_play_handle = NULL;
    }

    BAS_FUNC_LEAVE();
}

unsigned char bas_a2dp_start_audio(bas_instance_t *inst)
{  
    unsigned char result = FALSE;
    BAS_FUNC_ENTER();    
    BAS_FUNC_LEAVE();
    return result;
}

unsigned char bas_a2dp_stop_audio(bas_instance_t *inst)
{
    unsigned char result = TRUE;
    BAS_FUNC_ENTER();
    bas_a2dp_close_audio_handle(inst);
    inst->a2dp_audio_started = FALSE;
    BAS_FUNC_LEAVE();
    return result;
}
