#
# Copyright (C) 2011 The Android Open-Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Target information
# Processor
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_SMP := true
ARCH_ARM_HAVE_ARMV7A := true
TARGET_ARCH_VARIANT := armv7-a-neon
ARCH_ARM_HAVE_TLS_REGISTER := true

TARGET_GLOBAL_CFLAGS += -mtune=cortex-a9 -mcpu=cortex-a9 -mfpu=neon 
TARGET_GLOBAL_CPPFLAGS += -mtune=cortex-a9 -mcpu=cortex-a9 -mfpu=neon 
TARGET_arm_CFLAGS := -O2 -fomit-frame-pointer -fstrict-aliasing -funswitch-loops fmodulo-sched -fmodulo-sched-allow-regmoves
TARGET_thumb_CFLAGS := -mthumb -Os -fomit-frame-pointer -fstrict-aliasing

# Kernel
TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := false
TARGET_NO_RADIOIMAGE := true
TARGET_NO_RECOVERY := false
BOARD_USES_GENERIC_AUDIO := false
USE_CAMERA_STUB := true
BOARD_HAVE_BLUETOOTH := false
TARGET_NO_RADIOIMAGE := true
BOARD_HAVE_FAKE_GPS := true

BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_PAGESIZE := 4096
BOARD_KERNEL_CMDLINE := mem=512M console=ttyO2,115200n8 vram=16M omapfb.vram=0:8M def_disp=lcd2
TARGET_BOARD_PLATFORM := omap4
TARGET_BOOTLOADER_BOARD_NAME := otter
TARGET_BOARD_INFO_FILE := device/amazon/otter/board-info.txt

# Kernel Build
#TARGET_PREBUILT_KERNEL := device/amazon/otter/kernel
#TARGET_KERNEL_SOURCE := kernel/amazon/otter
#TARGET_KERNEL_CONFIG := otter_android_defconfig
TARGET_KERNEL_CONFIG := river_defconfig

WLAN_MODULES:
	make clean -C hardware/ti/wlan/mac80211/compat_wl12xx
	make -j4 -C hardware/ti/wlan/mac80211/compat_wl12xx KERNEL_DIR=$(KERNEL_OUT) KLIB=$(KERNEL_OUT) KLIB_BUILD=$(KERNEL_OUT) ARCH=arm CROSS_COMPILE="arm-eabi-"
	mv hardware/ti/wlan/mac80211/compat_wl12xx/compat/compat.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/net/mac80211/mac80211.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/net/wireless/cfg80211.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/drivers/net/wireless/wl12xx/wl12xx.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/drivers/net/wireless/wl12xx/wl12xx_spi.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/drivers/net/wireless/wl12xx/wl12xx_sdio.ko $(KERNEL_MODULES_OUT)

SGX_MODULES:
	make clean -C kernel/amazon/otter/external/sgx/src/eurasia_km/eurasiacon/build/linux2/omap4430_android
	cp $(TARGET_KERNEL_SOURCE)/drivers/video/omap2/omapfb/omapfb.h $(KERNEL_OUT)/drivers/video/omap2/omapfb/omapfb.h
	make -j4 -C kernel/amazon/otter/external/sgx/src/eurasia_km/eurasiacon/build/linux2/omap4430_android ARCH=arm KERNEL_CROSS_COMPILE=arm-eabi- CROSS_COMPILE=arm-eabi- KERNELSRC=$(KERNEL_OUT)/../../../../../../kernel/amazon/otter KERNELDIR=$(KERNEL_OUT) TARGET_PRODUCT="blaze_tablet" BUILD=release TARGET_SGX=540 PLATFORM_VERSION=4.0
	mkdir -p $(TARGET_ROOT_OUT)/modules
	mv $(KERNEL_OUT)/../../target/kbuild/pvrsrvkm_sgx540_120.ko $(TARGET_ROOT_OUT)/modules/

#	mv $(KERNEL_OUT)/../../target/kbuild/omaplfb_sgx540_120.ko $(TARGET_ROOT_OUT)/modules/

# KERNEL_OUT
TARGET_KERNEL_MODULES := WLAN_MODULES SGX_MODULES

# Filesystem
TARGET_USERIMAGES_USE_EXT4 := true
BOARD_BOOTIMAGE_PARTITION_SIZE := 10485760
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 16777216
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 536870912
BOARD_USERDATAIMAGE_PARTITION_SIZE := 1192230912
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_VOLD_MAX_PARTITIONS := 32
BOARD_VOLD_EMMC_SHARES_DEV_MAJOR := true
TARGET_USE_CUSTOM_LUN_FILE_PATH := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun/file"
TARGET_RECOVERY_PRE_COMMAND := "idme postmode 1;"

BOARD_HAS_SDCARD_INTERNAL := true
BOARD_SDCARD_DEVICE_PRIMARY := /dev/block/platform/omap/omap_hsmmc.1/by-name/media
BOARD_SDCARD_DEVICE_INTERNAL := /dev/block/platform/omap/omap_hsmmc.1/by-name/media

#TARGET_PROVIDES_INIT_RC := true

# Connectivity - Wi-Fi
BOARD_WPA_SUPPLICANT_DRIVER      := NL80211
WPA_SUPPLICANT_VERSION           := VER_0_8_X
PRODUCT_WIRELESS_TOOLS           := true
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_wl12xx
BOARD_WLAN_DEVICE                := wl12xx_mac80211
BOARD_SOFTAP_DEVICE              := wl12xx_mac80211
WIFI_DRIVER_MODULE_PATH          := "/system/lib/modules/wl12xx_sdio.ko"
WIFI_DRIVER_MODULE_NAME          := "wl12xx_sdio"
WIFI_FIRMWARE_LOADER             := ""
COMMON_GLOBAL_CFLAGS 		 += -DUSES_TI_MAC80211

# Graphics
BOARD_EGL_CFG := device/amazon/otter/egl.cfg
USE_OPENGL_RENDERER := true

# OMAP
OMAP_ENHANCEMENT := true
ifdef OMAP_ENHANCEMENT
COMMON_GLOBAL_CFLAGS += -DOMAP_ENHANCEMENT -DTARGET_OMAP4
endif

# Misc.
BOARD_NEEDS_CUTILS_LOG := true
#BOARD_USES_SECURE_SERVICES := true

# CodeAurora Optimizations: msm8960: Improve performance of memmove, bcopy, and memmove_words
TARGET_USE_KRAIT_BIONIC_OPTIMIZATION := true
ifdef TARGET_USE_KRAIT_BIONIC_OPTIMIZATION
#TARGET_USE_KRAIT_PLD_SET := true
#TARGET_KRAIT_BIONIC_PLDOFFS := 10
#TARGET_KRAIT_BIONIC_PLDTHRESH := 10
#TARGET_KRAIT_BIONIC_BBTHRESH := 64
#TARGET_KRAIT_BIONIC_PLDSIZE := 64
endif

ADDITIONAL_DEFAULT_PROPERTIES += \
    ro.secure=0 \
    ro.allow.mock.location=0 \
    ro.debuggable=1 \
    persist.sys.usb.config=mass_storage,adb \

# Bootanimation
TARGET_BOOTANIMATION_PRELOAD := true
TARGET_BOOTANIMATION_TEXTURE_CACHE := true

# OTA Packaging
TARGET_PROVIDES_RELEASETOOLS              := true
TARGET_CUSTOM_RELEASETOOL                 := device/amazon/otter/releasetools/squisher
TARGET_RELEASETOOL_OTA_FROM_TARGET_SCRIPT := device/amazon/otter/releasetools/otter_ota_from_target_files
TARGET_RELEASETOOL_IMG_FROM_TARGET_SCRIPT := device/amazon/otter/releasetools/otter_img_from_target_files

