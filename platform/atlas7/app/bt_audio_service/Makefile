.PHONY:all clean

ifeq ($(ARCH), ARM)
CFLAGS += `$(HOST_DIR)/usr/bin/pkg-config --cflags dbus-1 alsa`
LIBS += `$(HOST_DIR)/usr/bin/pkg-config --libs alsa`
else
CFLAGS += `pkg-config --cflags dbus-1 alsa`
LIBS += `pkg-config --libs alsa dbus-1`
endif

INC += -I. \

SRC := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(srcs))
TARGET := bt_audio_service
LIBS += -lpthread -ldbus-1

all: clean $(TARGET)

bas_main.o: bas_main.c
	$(CC) -c $(CFLAGS) $(INC)$< -g

bas_a2dp.o: bas_a2dp.c
	$(CC) -c $(CFLAGS) $(INC)$< -g

bas_util.o: bas_util.c
	$(CC) -c $(CFLAGS) $(INC) $< -g

bas_hfp.o: bas_hfp.c
	$(CC) -c $(CFLAGS) $(INC) $< -g

bas_dbus.o: bas_dbus.c
	$(CC) -c $(CFLAGS) $(INC) $< -g

bt_audio_service: bas_main.o bas_hfp.o bas_util.o bas_a2dp.o bas_dbus.o
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm *.o -f
	rm bt_audio_service -f
