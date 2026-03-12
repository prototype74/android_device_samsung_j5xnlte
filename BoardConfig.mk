#
# Copyright 2013 The Android Open Source Project
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

DEVICE_PATH := device/samsung/j5xnlte

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := MSM8916
TARGET_NO_BOOTLOADER := true
BOARD_VENDOR := samsung

# Board
TARGET_BOARD_PLATFORM := msm8916
TARGET_BOARD_PLATFORM_GPU := qcom-adreno306
TARGET_BOARD_SUFFIX := _64
TARGET_USES_64_BIT_BINDER := true

# Arch
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a53
TARGET_CPU_SMP := true

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv7-a-neon
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a53

# Kernel
BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_CMDLINE := console=null androidboot.hardware=qcom msm_rtb.filter=0x3F ehci-hcd.park=3 androidboot.bootdevice=7824900.sdhci
BOARD_KERNEL_PAGESIZE := 2048
BOARD_KERNEL_IMAGE_NAME := Image.gz
BOARD_KERNEL_SEPARATED_DT := true
BOARD_CUSTOM_BOOTIMG_MK :=  $(DEVICE_PATH)/mkbootimg.mk
BOARD_MKBOOTIMG_ARGS := --ramdisk_offset 0x02000000
BOARD_MKBOOTIMG_ARGS += --tags_offset 0x01e00000
BOARD_MKBOOTIMG_ARGS += --pagesize $(BOARD_KERNEL_PAGESIZE)
KERNEL_TOOLCHAIN := $(PWD)/prebuilts/gcc/$(HOST_OS)-x86/aarch64/aarch64-linux-android-4.9/bin
KERNEL_TOOLCHAIN_PREFIX := aarch64-linux-android-
TARGET_KERNEL_SOURCE := kernel/samsung/msm8916
TARGET_KERNEL_CONFIG := msm8916_sec_defconfig
TARGET_KERNEL_VARIANT_CONFIG := msm8916_sec_j5xlte_eur_defconfig
TARGET_KERNEL_SELINUX_CONFIG := selinux_defconfig
TARGET_KERNEL_SELINUX_LOG_CONFIG := selinux_log_defconfig

# Filesystems
BOARD_SYSTEMIMAGE_PARTITION_TYPE := ext4
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := ext4
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_SUPPRESS_SECURE_ERASE := true
BOARD_HAS_LARGE_FILESYSTEM := true

# Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 13631488
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 15728640
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 3145728000
BOARD_CACHEIMAGE_PARTITION_SIZE := 209715200
BOARD_USERDATAIMAGE_PARTITION_SIZE := 12138274816
BOARD_FLASH_BLOCK_SIZE := 131072

# System props
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/system.prop

# QCOM configs
BOARD_USES_QCOM_HARDWARE := true
BOARD_USES_QC_TIME_SERVICES := true

# Recovery
BOARD_NEEDS_LZMA_MINIGZIP := true
LZMA_RAMDISK_TARGETS := recovery

# TWRP basic configs
TW_DEVICE_VERSION := 0_SDC2
RECOVERY_SDCARD_ON_DATA := true
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
TARGET_RECOVERY_QCOM_RTC_FIX := true
TW_INPUT_BLACKLIST := "hbtp_vm"
TW_THEME := portrait_hdpi
TW_HAS_DOWNLOAD_MODE := true
TW_MTP_DEVICE := "/dev/mtp_usb"
TW_NO_EXFAT_FUSE := true
TW_NO_LEGACY_PROPS := true
TW_NO_REBOOT_BOOTLOADER := true
TARGET_USE_CUSTOM_LUN_FILE_PATH := "/sys/devices/platform/msm_hsusb/gadget/lun%d/file"
TW_CUSTOM_CPU_TEMP_PATH := "/sys/class/thermal/thermal_zone0/temp"
TW_USE_TOOLBOX := true
TW_EXCLUDE_BASH := true
TW_EXCLUDE_PYTHON := true

# TWRP crypto support
TW_INCLUDE_CRYPTO := true

# TWRP Brightness
TW_BRIGHTNESS_PATH := "/sys/class/leds/lcd-backlight/brightness"
TW_MAX_BRIGHTNESS := 255
TW_DEFAULT_BRIGHTNESS := 128

# Init device model
TARGET_INIT_VENDOR_LIB := libinit_j5xnlte
TARGET_LIBINIT_DEFINES_FILE := $(DEVICE_PATH)/init/init_j5xnlte.cpp
