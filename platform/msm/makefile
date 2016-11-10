.PHONY:all bin lib
all: bt_audio_service

SYNERGY_ROOT_DIR := $(shell pwd)
SYNERGY_OPENSOURCE_DIR := $(SYNERGY_ROOT_DIR)/synergy-opensource
PLATFORM_DIR := $(SYNERGY_OPENSOURCE_DIR)/platform/msm
MSM_BAS_DIR := $(PLATFORM_DIR)/bt_audio_service

export MSM_BAS_DIR
bt_audio_service:
	make -C $(MSM_BAS_DIR) all
