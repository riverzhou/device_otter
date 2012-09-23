#
# Copyright (C) 2012 The Android Open-Source Project
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

# This file includes all definitions that apply to ALL otter devices, and
# are also specific to otter devices
#
# Everything in this directory will become public


##############################################

# Hardware HALs
PRODUCT_PACKAGES += \
    lights.otter \
    audio.primary.otter\
    audio_policy.default \
    hwcomposer.default \
    power.otter \

PRODUCT_PACKAGES += \
    libinvensense_mpl \
    libaudioutils \
    libion \

# Wifi
PRODUCT_PACKAGES += \
    ti_wfd_libs \
    lib_driver_cmd_wl12xx \
    dhcpcd.conf \
    hostapd.conf \
    wpa_supplicant.conf \
    calibrator \
    crda \
    regulatory.bin \

# Sound
PRODUCT_PACKAGES += \
    tinyplay \
    tinymix \
    tinycap \

# Misc
PRODUCT_PACKAGES += \
    librs_jni \
    com.android.future.usb.accessory \
    libjni_pinyinime \
    make_ext4fs \
    setup_fs \
    calibrator \
    iontest \
    busybox \
    su \
    strace \

# Apps
PRODUCT_PACKAGES += \
    FileManager \
    Superuser \

##############################################

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:system/etc/permissions/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \

#---------------------------------------

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/prebuilt/bin/fix-mac.sh:/system/bin/fix-mac.sh \
    $(LOCAL_PATH)/prebuilt/bin/temperature_log.sh:/system/bin/temperature_log.sh \
    $(LOCAL_PATH)/prebuilt/bin/battery_log.sh:/system/bin/battery_log.sh \
    $(LOCAL_PATH)/prebuilt/bin/klog.sh:/system/bin/klog.sh \

#PRODUCT_COPY_FILES += \
#    $(LOCAL_PATH)/prebuilt/etc/init.d/01sysctl:system/etc/init.d/01sysctl \
#    $(LOCAL_PATH)/prebuilt/etc/init.d/02oom:system/etc/init.d/02oom \
#    $(LOCAL_PATH)/prebuilt/etc/init.d/03mount:system/etc/init.d/03mount \
#    $(LOCAL_PATH)/prebuilt/etc/sysctl.conf:system/etc/sysctl.conf \
#    $(LOCAL_PATH)/prebuilt/etc/init.d/01fixmac:system/etc/init.d/01fixmac \

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/prebuilt/etc/audio_policy.conf:/system/etc/audio_policy.conf \
    $(LOCAL_PATH)/prebuilt/etc/gps.conf:/system/etc/gps.conf \
    $(LOCAL_PATH)/prebuilt/etc/vold.fstab:/system/etc/vold.fstab \
    $(LOCAL_PATH)/prebuilt/etc/wifi/TQS_S_2.6.ini:system/etc/wifi/TQS_S_2.6.ini \

# key mapping and touchscreen files
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/prebuilt/usr/idc/ilitek_i2c.idc:/system/usr/idc/ilitek_i2c.idc \
    $(LOCAL_PATH)/prebuilt/usr/idc/twl6030_pwrbutton.idc:/system/usr/idc/twl6030_pwrbutton.idc \
    $(LOCAL_PATH)/prebuilt/usr/keylayout/twl6030_pwrbutton.kl:/system/usr/keylayout/twl6030_pwrbutton.kl \

#---------------------------------------
# Create dir /system/lib/modules/
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/kernel:/system/lib/modules/README \

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/vendor/aosp/media/bootanimation.zip:/system/media/bootanimation.zip \

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/vendor/ti/etc/media_codecs.xml:/system/etc/media_codecs.xml \

# misc
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/vendor/amazon/lib/hw/sensors.otter.so:/system/lib/hw/sensors.otter.so \
    $(LOCAL_PATH)/vendor/amazon/lib/libidme.so:/system/lib/libidme.so \
    $(LOCAL_PATH)/vendor/amazon/bin/idme:/system/bin/idme \

# Graphics
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/hw/gralloc.omap4430.so:/system/vendor/lib/hw/gralloc.omap4430.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/egl/libEGL_POWERVR_SGX540_120.so:/system/vendor/lib/egl/libEGL_POWERVR_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/egl/libGLESv1_CM_POWERVR_SGX540_120.so:/system/vendor/lib/egl/libGLESv1_CM_POWERVR_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/egl/libGLESv2_POWERVR_SGX540_120.so:/system/vendor/lib/egl/libGLESv2_POWERVR_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libglslcompiler_SGX540_120.so:/system/vendor/lib/libglslcompiler_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libIMGegl_SGX540_120.so:/system/vendor/lib/libIMGegl_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libpvr2d_SGX540_120.so:/system/vendor/lib/libpvr2d_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libpvrANDROID_WSEGL_SGX540_120.so:/system/vendor/lib/libpvrANDROID_WSEGL_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libPVRScopeServices_SGX540_120.so:/system/vendor/lib/libPVRScopeServices_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libsrv_init_SGX540_120.so:/system/vendor/lib/libsrv_init_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libsrv_um_SGX540_120.so:/system/vendor/lib/libsrv_um_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/lib/libusc_SGX540_120.so:/system/vendor/lib/libusc_SGX540_120.so \
    $(LOCAL_PATH)/vendor/imgtec/vendor/bin/pvrsrvinit:/system/vendor/bin/pvrsrvinit \
    $(LOCAL_PATH)/vendor/imgtec/vendor/bin/pvrsrvctl_SGX540_120:/system/vendor/bin/pvrsrvctl_SGX540_120 \
    $(LOCAL_PATH)/vendor/imgtec/etc/powervr.ini:/system/vendor/etc/powervr.ini \

# TI OMX
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libdomx.so:/system/vendor/lib/libdomx.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libmm_osal.so:/system/vendor/lib/libmm_osal.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libOMX.TI.DUCATI1.MISC.SAMPLE.so:/system/vendor/lib/libOMX.TI.DUCATI1.MISC.SAMPLE.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libOMX.TI.DUCATI1.VIDEO.CAMERA.so:/system/vendor/lib/libOMX.TI.DUCATI1.VIDEO.CAMERA.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libOMX.TI.DUCATI1.VIDEO.DECODER.secure.so:/system/vendor/lib/libOMX.TI.DUCATI1.VIDEO.DECODER.secure.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libOMX.TI.DUCATI1.VIDEO.DECODER.so:/system/vendor/lib/libOMX.TI.DUCATI1.VIDEO.DECODER.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libOMX.TI.DUCATI1.VIDEO.H264E.so:/system/vendor/lib/libOMX.TI.DUCATI1.VIDEO.H264E.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libOMX.TI.DUCATI1.VIDEO.MPEG4E.so:/system/vendor/lib/libOMX.TI.DUCATI1.VIDEO.MPEG4E.so \
    $(LOCAL_PATH)/vendor/ti/vendor/lib/libOMX_Core.so:/system/vendor/lib/libOMX_Core.so \

# wifi
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/vendor/ti/etc/firmware/ducati-m3.512MB.bin:/system/etc/firmware/ducati-m3.512MB.bin \
    $(LOCAL_PATH)/vendor/ti/etc/firmware/ti-connectivity/wl127x-fw-4-mr.bin:system/etc/firmware/ti-connectivity/wl127x-fw-4-mr.bin \
    $(LOCAL_PATH)/vendor/ti/etc/firmware/ti-connectivity/wl127x-fw-4-plt.bin:system/etc/firmware/ti-connectivity/wl127x-fw-4-plt.bin \
    $(LOCAL_PATH)/vendor/ti/etc/firmware/ti-connectivity/wl127x-fw-4-sr.bin:system/etc/firmware/ti-connectivity/wl127x-fw-4-sr.bin \
    $(LOCAL_PATH)/vendor/ti/etc/firmware/ti-connectivity/wl1271-nvs_127x.bin:system/etc/firmware/ti-connectivity/wl1271-nvs_127x.bin \
    $(LOCAL_PATH)/vendor/ti/etc/firmware/ti-connectivity/wl1271-nvs.bin:system/etc/firmware/ti-connectivity/wl1271-nvs.bin \

# drm (only for jb)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/vendor/google/vendor/lib/drm/libdrmwvmplugin.so:system/vendor/lib/drm/libdrmwvmplugin.so \
    $(LOCAL_PATH)/vendor/google/vendor/lib/libWVStreamControlAPI_L1.so:system/vendor/lib/libWVStreamControlAPI_L1.so \
    $(LOCAL_PATH)/vendor/google/vendor/lib/libwvdrm_L1.so:system/vendor/lib/libwvdrm_L1.so \
    $(LOCAL_PATH)/vendor/google/vendor/lib/libwvm.so:system/vendor/lib/libwvm.so \

##############################################

PRODUCT_PROPERTY_OVERRIDES := \
    dalvik.vm.stack-trace-file=/data/tmp/traces.txt \
    ro.opengles.version=131072 \
    ro.sf.hwrotation=270 \
    ro.sf.lcd_density=160 \
    ro.com.google.locationfeatures=1 \
    ro.com.google.networklocation=1 \
    ro.cwm.forbid_format = /bootloader,/dfs,/backup,/splash \
    ro.kernel.android.bootanim=1 \
    ro.crypto.state=unencrypted \
    wifi.interface=wlan0 \
    wifi.supplicant_scan_interval=120 \
    wifi.supplicant_scan_interval=180 \
    persist.sys.usb.config=mass_storage,adb \
    persist.sys.purgeable_assets=1 \
    persist.sys.root_access=3 \
    persist.service.adb.enable=1 \
    windowsmgr.max_events_per_sec=200 \
    pm.sleep_mode=1 \
    com.ti.omap_enhancement=true \
    omap.enhancement=true \
    persist.lab126.chargeprotect=1 \
    dalvik.vm.heapstartsize=5m \
    dalvik.vm.heapgrowthlimit=48m \
    dalvik.vm.heapsize=128m \

##############################################

PRODUCT_AAPT_CONFIG 		:= large mdpi
PRODUCT_AAPT_PREF_CONFIG 	:= large
PRODUCT_CHARACTERISTICS 	:= tablet,nosdcard
DEVICE_PACKAGE_OVERLAYS 	:= device/amazon/otter/overlay/aosp
PRODUCT_TAGS 			+= dalvik.gc.type-precise

