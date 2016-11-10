.PHONY:all clean

USE_MSM_PLATFORM := 1

ifeq ($(ARCH), ARM)
CFLAGS += `$(HOST_DIR)/usr/bin/pkg-config --cflags dbus-1 alsa`
LIBS += `$(HOST_DIR)/usr/bin/pkg-config --libs alsa`
else
CFLAGS += `pkg-config --cflags dbus-1 alsa`
LIBS += `pkg-config --libs alsa dbus-1`
endif

ifeq ($(USE_MSM_PLATFORM), 1)
CFLAGS += -DUSE_PULSE_AUDIO
LIBS += -lpulse -lpulse-simple
else
CFLAGS += -DUSE_ALSA 
endif

INC += -I$(MSM_BAS_DIR) \

SRC := $(MSM_BAS_DIR)/$(wildcard *.c)
#OBJS := $(patsubst %.c,%.o,$(srcs))
TARGET := bt_audio_service
LIBS += -lpthread -ldbus-1

all: clean $(TARGET)

bas_main.o: $(MSM_BAS_DIR)/bas_main.c
	$(CC) -c $(CFLAGS) $(INC)$< -g

bas_a2dp.o: $(MSM_BAS_DIR)/bas_a2dp.c
	$(CC) -c $(CFLAGS) $(INC)$< -g

bas_util.o: $(MSM_BAS_DIR)/bas_util.c
	$(CC) -c $(CFLAGS) $(INC) $< -g

bas_hfp.o: $(MSM_BAS_DIR)/bas_hfp.c
	$(CC) -c $(CFLAGS) $(INC) $< -g

bas_dbus.o: $(MSM_BAS_DIR)/bas_dbus.c
	$(CC) -c $(CFLAGS) $(INC) $< -g

bt_audio_service: bas_main.o bas_hfp.o bas_util.o bas_a2dp.o bas_dbus.o
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm *.o -f
	rm bt_audio_service -f
