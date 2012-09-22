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

# Board Setting
TARGET_CPU_ABI 				:= armeabi-v7a
TARGET_CPU_ABI2 			:= armeabi
TARGET_CPU_SMP 				:= true
ARCH_ARM_HAVE_ARMV7A 			:= true
TARGET_ARCH_VARIANT 			:= armv7-a-neon
ARCH_ARM_HAVE_TLS_REGISTER 		:= true

TARGET_GLOBAL_CFLAGS 			+= -mtune=cortex-a9 -mcpu=cortex-a9 -mfpu=neon 
TARGET_GLOBAL_CPPFLAGS 			+= -mtune=cortex-a9 -mcpu=cortex-a9 -mfpu=neon 
TARGET_arm_CFLAGS 			:= -O2 -fomit-frame-pointer -fstrict-aliasing -funswitch-loops fmodulo-sched -fmodulo-sched-allow-regmoves
TARGET_thumb_CFLAGS 			:= -mthumb -Os -fomit-frame-pointer -fstrict-aliasing

TARGET_BOARD_PLATFORM 			:= omap4
TARGET_BOOTLOADER_BOARD_NAME 		:= otter
BOARD_USES_GENERIC_AUDIO 		:= false
BOARD_HAVE_BLUETOOTH 			:= false
BOARD_HAVE_FAKE_GPS 			:= true
USE_CAMERA_STUB 			:= true
#TARGET_PROVIDES_INIT_RC 		:= true

BOARD_EGL_CFG 				:= device/amazon/otter/egl.cfg
USE_OPENGL_RENDERER 			:= true

TARGET_NO_BOOTLOADER 			:= true
TARGET_NO_KERNEL 			:= true
TARGET_NO_RADIOIMAGE 			:= true
TARGET_NO_RECOVERY 			:= true
TARGET_NO_RADIOIMAGE 			:= true

# Filesystem
TARGET_USERIMAGES_USE_EXT4 		:= true
BOARD_BOOTIMAGE_PARTITION_SIZE 		:= 10485760
BOARD_RECOVERYIMAGE_PARTITION_SIZE 	:= 16777216
BOARD_SYSTEMIMAGE_PARTITION_SIZE 	:= 536870912
BOARD_USERDATAIMAGE_PARTITION_SIZE 	:= 1192230912
BOARD_FLASH_BLOCK_SIZE 			:= 4096
BOARD_VOLD_MAX_PARTITIONS 		:= 32
BOARD_VOLD_EMMC_SHARES_DEV_MAJOR 	:= true
TARGET_USE_CUSTOM_LUN_FILE_PATH 	:= "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun/file"
TARGET_RECOVERY_PRE_COMMAND 		:= "idme postmode 1;"

BOARD_HAS_SDCARD_INTERNAL 		:= true
BOARD_SDCARD_DEVICE_PRIMARY 		:= /dev/block/platform/omap/omap_hsmmc.1/by-name/media
BOARD_SDCARD_DEVICE_INTERNAL 		:= /dev/block/platform/omap/omap_hsmmc.1/by-name/media

# Connectivity - Wi-Fi
BOARD_WPA_SUPPLICANT_DRIVER      	:= NL80211
WPA_SUPPLICANT_VERSION           	:= VER_0_8_X
PRODUCT_WIRELESS_TOOLS           	:= true
BOARD_WPA_SUPPLICANT_PRIVATE_LIB 	:= lib_driver_cmd_wl12xx
BOARD_WLAN_DEVICE                	:= wl12xx_mac80211
BOARD_SOFTAP_DEVICE              	:= wl12xx_mac80211
WIFI_DRIVER_MODULE_PATH          	:= "/modules/wl12xx_sdio.ko"
WIFI_DRIVER_MODULE_NAME          	:= "wl12xx_sdio"
WIFI_FIRMWARE_LOADER             	:= ""
COMMON_GLOBAL_CFLAGS 		 	+= -DUSES_TI_MAC80211

# OMAP
#OMAP_ENHANCEMENT 			:= true
#ifdef OMAP_ENHANCEMENT
#    COMMON_GLOBAL_CFLAGS 		+= -DOMAP_ENHANCEMENT -DTARGET_OMAP4
#endif

# Misc.
BOARD_NEEDS_CUTILS_LOG 			:= true

ADDITIONAL_DEFAULT_PROPERTIES 		+= \
    ro.secure=0 \
    ro.allow.mock.location=0 \
    ro.debuggable=1 \
    persist.sys.usb.config=mass_storage,adb \

# Bootanimation
TARGET_BOOTANIMATION_PRELOAD 		:= true
TARGET_BOOTANIMATION_TEXTURE_CACHE 	:= true

