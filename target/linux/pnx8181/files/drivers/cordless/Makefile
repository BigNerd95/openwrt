#
# Makefile for the kernel DECT adaptation layer
#
obj-$(CONFIG_CORDLESS) += cordless.o
cordless-objs := cfifo.o cfifo-io.o cmsg.o coma.o dev-cordless.o
obj-$(CONFIG_CORDLESS_DEBUG) += coma-debug.o
coma-debug-objs := debug.o
