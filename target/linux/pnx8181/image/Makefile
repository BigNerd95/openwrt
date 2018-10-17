#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

define Build/fullimage

	# Kernel:
	#   Data Address: 0x20008000, Entry Point: 0x20008000, OS: Linux, 
	#   CPU: ARM, image type: OS Kernel Image, compression type: none, 
	#   image name: "SP2Xcybertan_rom_bin"
	mkimage -A arm -O linux -C none -T kernel -a 0x20008000  \
		-e 0x20008000	-n "cybertan_rom_bin" -c "SP2X" \
		-d $(IMAGE_KERNEL) $(IMAGE_KERNEL).new

	# Filesystem:
	#   Data Address: 0x0, Entry Point: 0x0, OS: Linux, 
	#   CPU: ARM, image type: Filesystem Image, 
	#   compression type: none, image name: "SP2Xcybertan_rom_bin"
	mkimage -A arm -O linux -C none -T filesystem -a 0x00  \
		-e 0x00	-n "cybertan_rom_bin" -c "SP2X" \
		-d $(IMAGE_ROOTFS) $(IMAGE_ROOTFS).new

	cat $(IMAGE_KERNEL).new $(IMAGE_ROOTFS).new > $@.tmp

	# SOSTITUISCI CON paytonImageEditor
	#mkimage -A mips -O linux -T multi -a 0x00 -C none \
	#	-e 0x00 -n 'OpenWrt fullimage' \
	#	-d $@.tmp $@
	cp $@.tmp $@

	rm $(IMAGE_KERNEL).new
	rm $(IMAGE_ROOTFS).new
	rm $@.tmp
endef

$(eval $(call BuildImage))

