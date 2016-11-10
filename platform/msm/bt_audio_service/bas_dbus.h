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

#ifndef _BAS_DBUS_H
#define _BAS_DBUS_H

#include <dbus/dbus.h>

#define BT_AUDIO_SERVICE_DBUS_NAME              "com.csr.btAudioService"
#define BT_AUDIO_SERVICE_DBUS_OBJ_PATH          "/com/csr/btAudioService"
#define BT_AUDIO_SERVICE_DBUS_INTERFACE         "com.csr.btAudioService.interface"
#define BT_AUDIO_SERVICE_METHOD_START_AUDIO     "StartBtAudio"
#define BT_AUDIO_SERVICE_HFP_AUDIO              "handsfree"
#define BT_AUDIO_SERVICE_A2DP_AUDIO             "a2dp"

#define BT_AUDIO_SERVICE_DBUS_ERROR             "BT.AUDIO.SERVICE.DBUS.ERROR"
#define BT_AUDIO_SERVICE_DBUS_ERROR_MESSAGE     "Failed"

#endif
