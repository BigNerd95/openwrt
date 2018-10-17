# PNX8181 subarchitecture

obj-y += arch.o irq.o irq-ext.o irq-devices.o time.o devices.o clock.o debug.o sctu.o sctu1.o sctu2.o
obj-$(CONFIG_GENERIC_GPIO) += gpio.o gpio-devices.o
obj-$(CONFIG_SPI_PNX8181) += spi-devices.o
obj-$(CONFIG_I2C) += i2c.o

obj-$(CONFIG_ARCH_PNX8181_LOADSIM) += loadsim.o loadsim-fiq.o
CFLAGS_loadsim.o := -O1
