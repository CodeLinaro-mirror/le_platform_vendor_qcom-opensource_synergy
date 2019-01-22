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
#include <pulse/pulseaudio.h>
#include "bas_main.h"


int bas_set_mixer_volume(bas_instance_t *inst, int volume)
{
    int result = -1; 
    int idx;
    pa_operation *operation = NULL;
    pa_cvolume vol;

    vol.channels = 1;
    vol.values[0] = volume;

    BAS_FUNC_ENTER(); 
    
    if (inst == NULL || inst->mainloop == NULL || inst->downlink_p_stream == NULL)
        return result;

    pa_threaded_mainloop_lock (inst->mainloop);

    do
    {    
        if ((idx = pa_stream_get_index (inst->downlink_p_stream)) == PA_INVALID_INDEX)
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_stream_get_index failed")));
            break;
        }        
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("pa_stream_get_index:[%d]"), idx));
                
        if (!(operation = pa_context_set_sink_input_volume (inst->ctx, idx, &vol, NULL, NULL)))
        {
            IFDBG(DebugOut(DEBUG_ERROR, TEXT("pa_context_set_sink_input_volume failed")));
            break;
        } 
        
        IFDBG(DebugOut(DEBUG_OUTPUT, TEXT("pa_context_set_sink_input_volume:[%d] success !!! "), volume));
        result = 0;

        if (operation)
            pa_operation_unref (operation);    

    }
    while(0);  
    
    pa_threaded_mainloop_unlock (inst->mainloop);   

    BAS_FUNC_LEAVE();
    
    return result;

}

