#!/bin/bash

case "$1" in
	onlyboot ) 	ONLYBOOT_BUILD=true ;; 	 	
	oldboot ) 	OLDBOOT_BUILD=true ;;   	
	oldkernel ) 	OLDKERNEL_BUILD=true ;;		
esac

CPU_NUMBER=`grep processor /proc/cpuinfo | wc -l`
let CPU_NUMBER+=1

PWD=`pwd`

TOP_DIR=${PWD}
TOOLS_DIR=${PWD}/tools/
TESTKEY_PEM=${PWD}/build/target/product/security/testkey.x509.pem
TESTKEY_PK8=${PWD}/build/target/product/security/testkey.pk8

PRODUCT_DIR=${PWD}/out/target/product/otter/
OTA_DIR=${PRODUCT_DIR}/OTA/
RAMDISK_DIR=${PRODUCT_DIR}/ramdisk/
BOOT_DIR=${PRODUCT_DIR}/boot/
KERNEL_DIR=${PRODUCT_DIR}/kernel/
ROOT_DIR=${PRODUCT_DIR}/root/

RAMDISK_SOURCE=${PWD}/device/amazon/otter/ramdisk/
DEVICE_SOURCE=${PWD}/device/amazon/otter/
KERNEL_SOURCE=${PWD}/kernel/amazon/otter/

if [  "$ONLYBOOT_BUILD" != "true" ] ; then 
METAINF_SOURCE=${PWD}/device/amazon/otter/OTA/full/META-INF/
else
METAINF_SOURCE=${PWD}/device/amazon/otter/OTA/boot/META-INF/
fi

KERNEL_BASE=0x80000000
KERNEL_PAGESIZE=4096
KERNEL_CMDLINE="mem=512M console=tty0 vram=16M omapfb.vram=0:8M"
#KERNEL_DEFCONFIG=otter_android_defconfig
KERNEL_DEFCONFIG=river_defconfig

GCC_PREFIX=${PWD}/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-

SGX_BASE=${TOP_DIR}/hardware/ti/sgx540/eurasia_km/eurasiacon/
SGX_SOURCE=${SGX_BASE}/build/linux2/omap4430_android
SGX_DIR=${SGX_BASE}/binary2_540_120_omap4430_android_release/target/

#-----------------------------------------------

rm    -rf ${OTA_DIR}
mkdir -p  ${OTA_DIR}/META-INF/
cp    -rf ${METAINF_SOURCE}/* ${OTA_DIR}/META-INF/

if [ "$OLDBOOT_BUILD" != "true" ] || [ ! -f ${PRODUCT_DIR}/boot.img ] ; then
	rm    -rf ${RAMDISK_DIR}
	mkdir -p  ${RAMDISK_DIR}
	cp    -rf ${RAMDISK_SOURCE}/* ${RAMDISK_DIR}
	mkdir -p  ${RAMDISK_DIR}/data
	mkdir -p  ${RAMDISK_DIR}/dev
	mkdir -p  ${RAMDISK_DIR}/modules
	mkdir -p  ${RAMDISK_DIR}/proc
	mkdir -p  ${RAMDISK_DIR}/sbin
	mkdir -p  ${RAMDISK_DIR}/sys
	mkdir -p  ${RAMDISK_DIR}/system
	cp    -f  ${ROOT_DIR}/init      ${RAMDISK_DIR}/init
	cp    -f  ${ROOT_DIR}/sbin/adbd ${RAMDISK_DIR}/sbin/adbd
	rm    -rf ${BOOT_DIR}
	mkdir -p  ${BOOT_DIR}

	if [ "$OLDKERNEL_BUILD" != "true" ] || [ ! -f ${KERNEL_DIR}/arch/arm/boot/zImage ] ; then
		cp -f ${DEVICE_SOURCE}/river_defconfig  ${KERNEL_SOURCE}/arch/arm/configs/river_defconfig

		rm    -rf ${KERNEL_DIR}
		mkdir -p  ${KERNEL_DIR}
		export ARCH=arm
		export CROSS_COMPILE=${GCC_PREFIX}
		export KERNEL_CROSS_COMPILE=${GCC_PREFIX}
		make  -C ${KERNEL_SOURCE} O=${KERNEL_DIR}  ${KERNEL_DEFCONFIG}
		make  -C ${KERNEL_SOURCE} O=${KERNEL_DIR}  -j ${CPU_NUMBER}

		export KERNELDIR=${KERNEL_DIR}
		export KERNELSRC=${KERNEL_SOURCE}
		cd ${SGX_SOURCE}
		make  clean 
		make  -j ${CPU_NUMBER} TARGET_PRODUCT="blaze_tablet" BUILD=release TARGET_SGX=540 PLATFORM_VERSION=4.0
        	mkdir -p ${RAMDISK_DIR}/modules
		cp ${SGX_DIR}/pvrsrvkm_sgx540_120.ko ${RAMDISK_DIR}/modules
		cd ${TOP_DIR}
	fi 

	${TOOLS_DIR}/mkbootfs ${RAMDISK_DIR} | gzip > ${BOOT_DIR}/ramdisk.gz
	${TOOLS_DIR}/mkbootimg --cmdline "${KERNEL_CMDLINE}" --kernel ${KERNEL_DIR}/arch/arm/boot/zImage --ramdisk ${BOOT_DIR}/ramdisk.gz -o ${PRODUCT_DIR}/boot.img --base ${KERNEL_BASE} --pagesize ${KERNEL_PAGESIZE}
fi 
cp ${PRODUCT_DIR}/boot.img ${OTA_DIR}/boot.img

if [ "$ONLYBOOT_BUILD" != "true" ] && [ "$OLDKERNEL_BUILD" != "true" ] ; then
	${TOOLS_DIR}/simg2img ${PRODUCT_DIR}/system.img ${OTA_DIR}/system.img
	OTA_FILE=${PRODUCT_DIR}/full_otter-ota
else
	OTA_FILE=${PRODUCT_DIR}/boot_otter-ota
fi

rm -f ${OTA_FILE}.zip
cd ${OTA_DIR}/
zip -ry ${OTA_FILE}.zip .
cd ${TOP_DIR}

echo java -Xmx2048m -jar ${TOOLS_DIR}/signapk.jar -w ${TESTKEY_PEM} ${TESTKEY_PK8} ${OTA_FILE}.zip ${OTA_FILE}-signed.zip
java -Xmx2048m -jar ${TOOLS_DIR}/signapk.jar -w ${TESTKEY_PEM} ${TESTKEY_PK8} ${OTA_FILE}.zip ${OTA_FILE}-signed.zip
echo  ${OTA_FILE}-signed.zip
ls -l ${OTA_FILE}-signed.zip

