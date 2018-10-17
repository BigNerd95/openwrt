#
# Copyright (C) 2008-2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=pnx8181
BOARDNAME:=NXP PNX8181
FEATURES:=squashfs
CPU_TYPE:=arm926ej-s
SUBTARGETS:=generic

KERNEL_PATCHVER:=3.18
#KERNEL_PATCHVER:=2.6.26

include $(INCLUDE_DIR)/target.mk

#DEFAULT_PACKAGES += \
#	kmod-gpio-button-hotplug swconfig \
#	uboot-envtools

$(eval $(call BuildTarget))
