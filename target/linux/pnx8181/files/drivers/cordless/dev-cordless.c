/*
 *  drivers/cordless/dev-cordless.c - the cordless character device driver
 *
 *  This driver registers the character device for the communication between
 *  Linux user space and cordless.
 *
 *  Copyright (C) 2007 NXP Semiconductors
 *  Copyright (C) 2008, 2009 DSPG Technologies GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/kfifo.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/arch/platform.h>
#include <asm/mach/time.h>

#include "dev-cordless.h"
#include "cmsg.h"
#include "coma.h"

#include <asm/arch/hardware.h>
#include <asm/arch/pnx8181-regs.h>
#include <asm/arch/irqs.h>
#include <asm/mach/map.h>

MODULE_AUTHOR("DSPG Technologies GmbH");
MODULE_LICENSE("GPL");

static char cordless_name[] = "cordless";

static dev_t cordless_dev;
static struct cdev cordless_cdev;

static DECLARE_WAIT_QUEUE_HEAD(log_wait_queue);
static struct kfifo *log_messages;
static spinlock_t log_messages_lock;

static int cordless_loaded = 0;
static int config_len = 0;

volatile void __iomem *scram     = (void __iomem *)PNX8181_SCRAM_VA_MEM;
volatile void __iomem *scram_ext = (void __iomem *)PNX8181_SCRAM_EXT_VA;

static struct cordless_config *dc;

struct mutex config_event_mutex;
wait_queue_head_t config_wq;
int config_request = 0;
unsigned char *configuration = NULL;
struct file* p_eeprom_update_filp = 0;

extern unsigned char firetux_get_boardrevision(void);

static int
cordless_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int
cordless_release(struct inode *inode, struct file *filp)
{
	if(p_eeprom_update_filp == filp ){
		p_eeprom_update_filp = 0;
	}
	return 0;
}

void cordless_signal_cordless(void)
{
	trigger_irq(COMA_CORDLESS_IID);
}
EXPORT_SYMBOL(cordless_signal_cordless);

static void
cordless_device_release(struct device *dev)
{
}

struct platform_device cordless_device = {
	.name	= "cordless",
	.id	= 0,
	.dev	= {
		.release = cordless_device_release,
	}
};

int
coma_probe(struct platform_device *dev)
{
	dc = dev->dev.platform_data;
	if (dc == NULL) {
		printk(KERN_ERR "%s: no configuration set!\n", cordless_name);
		return -ENODEV;
	}

	return coma_setup(dc->fifo_l2c, dc->fifo_c2l);
}

int
coma_remove(struct platform_device *dev)
{
	coma_release();

	return 0;
}

struct platform_driver coma_driver = {
	.probe = coma_probe,
	.remove = coma_remove,
	.driver = {
		.name = "cordless",
	},
};

typedef void (*coma_init_func)(void);

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02 /* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

/*
 * Check the gzip header and return its length
 */
static int check_gzip_header(char *in)
{
	unsigned char flags;
	unsigned char magic[2]; /* magic header */
	unsigned char method;
	int inptr = 0;

	if (!in)
		return -1;

	magic[0] = in[inptr++];
	magic[1] = in[inptr++];
	method   = in[inptr++];

	if (magic[0] != 037 ||
	    ((magic[1] != 0213) && (magic[1] != 0236))) {
		printk("bad gzip magic numbers");
		return -1;
	}

	/* We only support method #8, DEFLATED */
	if (method != 8)  {
		printk("internal error, invalid method");
		return -1;
	}

	flags  = (unsigned char)in[inptr++];
	if ((flags & ENCRYPTED) != 0) {
		printk("Input is encrypted");
		return -1;
	}
	if ((flags & CONTINUATION) != 0) {
		printk("Multi part input");
		return -1;
	}
	if ((flags & RESERVED) != 0) {
		printk("Input has invalid flags");
		return -1;
	}
	inptr += 4; /* skip timestamp */

	inptr++;    /* ignore extra flags for the moment */
	inptr++;    /* ignore OS type for the moment */

	if ((flags & EXTRA_FIELD) != 0) {
		unsigned len = (unsigned)in[inptr++];
		len |= ((unsigned)in[inptr++])<<8;
		inptr += len;
	}

	/* Get original file name if it was truncated */
	if ((flags & ORIG_NAME) != 0) {
		/* Discard the old name */
		while (in[inptr++] != 0) /* null */ ;
	}

	/* Discard file comment if any */
	if ((flags & COMMENT) != 0) {
		while (in[inptr++] != 0) /* null */ ;
	}

	return inptr;
}

/*
 * Return the original length of the compressed file
 */
static int get_gzip_len(char *in, unsigned long size)
{
	int orig_len;

	if (!in || (size < 4))
		return -1;

	orig_len  = (unsigned long)in[size-4];
	orig_len |= (unsigned long)in[size-3] << 8;
	orig_len |= (unsigned long)in[size-2] << 16;
	orig_len |= (unsigned long)in[size-1] << 24;

	return orig_len;
}
extern unsigned int PNX8181_SCRAM_EXT;
void __init create_mapping(struct map_desc *md);

static int
cordless_domain_init(unsigned long fifo_size, void __user *cordless_user_img,
                     unsigned long cordless_img_size, int options)
{
	void *cordless_image = NULL;
	struct cordless_img_hdr *img_hdr;
	struct cordless_img_hdr *img_ext_hdr;
	int ret = 0;
	unsigned long expected_version;
	unsigned long max_image_size;

	if ((fifo_size == 0) || (fifo_size > CORDLESS_MAX_FIFO_SIZE) ||
	    (cordless_img_size == 0) ||
	    (cordless_img_size > CORDLESS_SCRAM_AVAILABLE + 0x100000)) {
		/* FIXME */
		printk(KERN_ERR "%s: invalid parameters: "
		                "fifo_size=%lu, cordless_img_size=%lu\n",
		                cordless_name, fifo_size, cordless_img_size);
		ret = -EINVAL;
		goto out;
	}
	cordless_image = vmalloc(cordless_img_size);
	if (cordless_image == NULL) {
		printk(KERN_ERR "%s: no memory for cordless image\n",
		       cordless_name);
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(cordless_image, (void __user *)cordless_user_img,
	                   cordless_img_size)) {
		printk(KERN_ERR "%s: cannot copy cordless image "
	                        "from user space\n", cordless_name);
		ret = -EFAULT;
		goto out_image;
	}

	/* check image header */
	img_hdr = (struct cordless_img_hdr *)cordless_image;
	if (img_hdr->magic != CORDLESS_MAGIC) {
		int ret, header_len, orig_len;
		void *uncompress_buf;

		/* no cordless magic found - let's try zlib */

		header_len = check_gzip_header(cordless_image);
		orig_len = get_gzip_len(cordless_image, cordless_img_size);

		/* sanity checks */
		if ((header_len < 0) || (orig_len <= 0) ||
		    (orig_len > (CORDLESS_SCRAM_AVAILABLE + 0x100000))) {
			printk(KERN_ERR "%s: invalid image magic\n",
			                cordless_name);
			ret = -EFAULT;
			goto out_image;
		}

		uncompress_buf = vmalloc(orig_len);
		if (uncompress_buf == NULL) {
			printk(KERN_ERR "%s: no memory for decompressing the cordless image\n",
			                cordless_name);
			ret = -ENOMEM;
			goto out_image;
		}

		ret = zlib_inflate_blob(uncompress_buf, orig_len,
		                        cordless_image + header_len,
		                        cordless_img_size - header_len);
		if (ret > 0) {
			vfree(cordless_image);
			cordless_image = vmalloc(ret);
			if (cordless_image == NULL) {
				printk(KERN_ERR "%s: no memory for cordless image\n",
				                cordless_name);
				ret = -ENOMEM;
				goto out;
			}
			memcpy(cordless_image, uncompress_buf, ret);
		}
		vfree(uncompress_buf);
	}

	img_hdr = (struct cordless_img_hdr *)cordless_image;
	if (img_hdr->magic != CORDLESS_MAGIC) {
		printk(KERN_ERR "%s: invalid image magic\n", cordless_name);
		ret = -EFAULT;
		goto out_image;
	}

	expected_version = CORDLESS_IMG_VERSION;
	if (img_hdr->img_version != expected_version) {
		printk(KERN_ERR "%s: image version mismatch (got: v%lu, "
		                "kernel expected: v%lu)\n", cordless_name,
		                img_hdr->img_version, expected_version);
		ret = -EFAULT;
		goto out_image;
	}
	printk(KERN_INFO "%s: loading %s\n", cordless_name,
	       img_hdr->info);

	max_image_size =  CORDLESS_SCRAM_AVAILABLE;
	max_image_size -= img_hdr->reserved_space;
	max_image_size -= (  img_hdr->dest_addr & 0x00FFFFFF) -
	                  (PNX8181_SCRAM_VA_MEM & 0x00FFFFFF);
	if (img_hdr->total_img_size > max_image_size) {
		printk(KERN_ERR "%s: image too big! sz=0x%08lX, max=0x%08lX\n",
		       cordless_name, img_hdr->total_img_size, max_image_size);
		ret = -EFAULT;
		goto out_image;
	}

	/* in case there's a 2nd image header in the file, its the
	 * SCRAM extension in SDRAM which is mapped to PNX8181_SCRAM_EXT_VA */
	if (cordless_img_size > img_hdr->hdr_size + img_hdr->img_size) {
		img_ext_hdr = (struct cordless_img_hdr *)
		(cordless_image + img_hdr->hdr_size + img_hdr->img_size);
		if (img_ext_hdr->magic == CORDLESS_MAGIC) {
			/* printk(KERN_INFO "%s: found 2nd image header, mapping"
			      " %lu (0x%lx) bytes to 0x%8.8x\n", cordless_name,
			                  (long unsigned)img_ext_hdr->img_size,
			                  (long unsigned)img_ext_hdr->img_size,
			                  PNX8181_SCRAM_EXT_VA); */
			memset((void *)scram_ext, 0, PNX8181_SCRAM_EXT_SIZE);
			memcpy((void __iomem *)scram_ext,
			       (void*)img_ext_hdr +
			       img_ext_hdr->hdr_size,
			       img_ext_hdr->img_size);
		}
	}

	memset(&cordless_device, 0, sizeof(struct platform_device));
	cordless_device.id = 0;
	cordless_device.dev.release = cordless_device_release;
	cordless_device.name = "cordless";

	memset(&coma_driver, 0, sizeof(struct platform_driver));
	coma_driver.probe = coma_probe;
	coma_driver.remove = coma_remove;
	coma_driver.driver.name = "cordless";

	dc = (struct cordless_config *)kmalloc(sizeof(struct cordless_config) +
	     img_hdr->hdr_size, GFP_KERNEL);
	if (dc == NULL) {
		printk(KERN_ERR "%s: no memory for config\n", cordless_name);
		ret = -ENOMEM;
		goto out_image;
	}
	dc->fifo_c2l = cfifo_alloc(fifo_size);
	dc->fifo_l2c = cfifo_alloc(fifo_size);
	if (IS_ERR(dc->fifo_c2l) || IS_ERR(dc->fifo_l2c)) {
		printk(KERN_ERR "%s: can't initialize fifo with size %lu\n",
		                cordless_name, fifo_size);
		ret = -ENOMEM;
		goto out_config;
	}

	dc->end_of_memory = (PNX8181_SCRAM_VA_MEM & 0xF0FFFFFF) + 0x02000000 + CORDLESS_SCRAM_AVAILABLE;
	dc->firetux_board_revision = (unsigned long)firetux_get_boardrevision();
	dc->img_hdr = *img_hdr;

	/*
	 * zero the physical ram and invalidate all mappings
	 * FIXME: do this the correct way
	 */
	memset((void *)scram, 0, CORDLESS_SCRAM_AVAILABLE);
	memset((void *)scram + 0x01000000, 0, CORDLESS_SCRAM_AVAILABLE);
	memset((void *)scram + 0x02000000, 0, CORDLESS_SCRAM_AVAILABLE);
	memset((void *)scram + 0x03000000, 0, CORDLESS_SCRAM_AVAILABLE);

	*(struct cordless_config **)scram = dc;
	memcpy((void __iomem *)img_hdr->dest_addr,
	       cordless_image + img_hdr->hdr_size, img_hdr->img_size);

	cordless_device.dev.platform_data = dc;

	/* create platform device  */
	ret = platform_device_register(&cordless_device);
	if (ret < 0) {
		goto out_fifos;
	}

	ret = platform_driver_register(&coma_driver);
	if (ret) {
		printk(KERN_ERR "%s: cannot register driver\n", cordless_name);
		goto out_device;
	}

	/* initialize the very basics of cordless */

	/* install FIQ handler */
	set_fiq_handler((void *)dc->img_hdr.fiq_handler_start,
	                dc->img_hdr.fiq_handler_len);

	/* setup stacks for the modes used by cordless (UND and FIQ) */
	__asm__ __volatile__ (
		"msr	cpsr_c, %2\n\t"
		"mov	sp, %0\n\t"
		"msr	cpsr_c, %3\n\t"
		"mov	sp, %1\n\t"
		"msr	cpsr_c, %4"
		:
		: "r" (dc->end_of_memory),
		  "r" (dc->end_of_memory - dc->img_hdr.reserved_space),
		  "I" (PSR_F_BIT | PSR_I_BIT | FIQ_MODE),
		  "I" (PSR_F_BIT | PSR_I_BIT | UND_MODE),
		  "I" (PSR_F_BIT | PSR_I_BIT | SVC_MODE)
		: "r14");

	/* configure interrupt controller for FIQs */
	__raw_writel(((unsigned long)scram & ~(0x7ff)), PNX8181_INTC + 0x104);
	/* register FIQ handler in cordless domain */
	__raw_writel((1<<28) | (1<<27) | (1<<26) | (1<<16) | (1<<8) | 1,
	             PNX8181_INTC + 0x400 + (COMA_CORDLESS_IID * 4));
	__raw_writel(dc->img_hdr.init_func_addr,
	             (((unsigned long)scram & ~(0x7ff)) +
	             (COMA_CORDLESS_IID * 8)));

	/* enable FIQs */
	local_fiq_enable();

	goto out_image;

out_device:
	platform_device_unregister(&cordless_device);
out_fifos:
	if (!(IS_ERR(dc->fifo_c2l)))
		cfifo_free(dc->fifo_c2l);
	if (!(IS_ERR(dc->fifo_l2c)))
		cfifo_free(dc->fifo_l2c);
out_config:
	kfree(dc);
out_image:
	vfree(cordless_image);
out:
	return ret;
}


static int
cordless_domain_deinit(void)
{
	memset((void *)scram, 0, CORDLESS_SCRAM_AVAILABLE);
	memset((void *)scram_ext, 0, PNX8181_SCRAM_EXT_SIZE);

	platform_driver_unregister(&coma_driver);
	platform_device_unregister(&cordless_device);
	cfifo_free(dc->fifo_c2l);
	cfifo_free(dc->fifo_l2c);
	kfree(dc);

	return 0;
}

int
cordless_print(unsigned char *text, unsigned int len)
{
	int ret;

	ret = kfifo_put(log_messages, text, len);
	wake_up(&log_wait_queue);

	return 0;
}
EXPORT_SYMBOL(cordless_print);

void
cordless_configuration_write(int pos, unsigned char *buf, unsigned int len)
{
	struct config_write_reply reply;
	reply.bytes_written_successfully = 0;
	reply.result = -1;
	if(p_eeprom_update_filp != 0){
	mutex_lock(&config_event_mutex);

	if ((len > config_len) || (pos < 0) || (pos+len > config_len)) {
		/* all above should not occur at all*/
		mutex_unlock(&config_event_mutex);
		printk(KERN_ERR "%s: invalid configuration write request (%d "
		       "bytes at %d)\n", cordless_name, len, pos);
		reply.result = -3;
		coma_reply_configuration_write(reply.bytes_written_successfully, reply.result);
		return;
	}

	memcpy(configuration + pos, buf, len);

	config_request = 1;

	mutex_unlock(&config_event_mutex);

	wake_up(&config_wq);
	}
	else{
		printk("\n!!!There is no process waiting for write to file!!!\n");
		reply.result = -2;
		coma_reply_configuration_write(reply.bytes_written_successfully, reply.result);
		return;
	}
}
EXPORT_SYMBOL(cordless_configuration_write);

/*
 * file operations
 */

static ssize_t
cordless_read(struct file *filp, char __user *buf, size_t count_want,
              loff_t *f_pos)
{
	unsigned char *data;
	size_t not_copied;
	size_t to_copy;

	if (wait_event_interruptible(log_wait_queue,
	                             kfifo_len(log_messages) > 0))
		return -ERESTART;
	
	to_copy = min(kfifo_len(log_messages), count_want);

	data = kmalloc(to_copy, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	to_copy = kfifo_get(log_messages, data, to_copy);
	if (to_copy < 0) {
		kfree(data);
		return -EIO;
	}

	not_copied = copy_to_user(buf, data, to_copy);
	kfree(data);

	return to_copy - not_copied;
}

extern unsigned char timing_stats[66 * 8];

static int
cordless_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
               unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned int size;
	int ret = 0;

	if (_IOC_TYPE(cmd) != CORDLESS_IOC_MAGIC)
		return -EINVAL;

	if (_IOC_NR(cmd) > CORDLESS_IOC_MAXNR)
		return -EINVAL;

	size = _IOC_SIZE(cmd);

	if (_IOC_DIR(cmd) & _IOC_READ)
		if (!access_ok(VERIFY_WRITE, argp, size))
			return -EFAULT;

	if (_IOC_DIR(cmd) & _IOC_WRITE)
		if (!access_ok(VERIFY_READ, argp, size))
			return -EFAULT;

	switch(cmd) {
	case CORDLESS_IOCINIT:
		{
			struct cordless_init di;

			if (cordless_loaded) {
				printk(KERN_ERR "%s: already loaded\n",
				       cordless_name);
				return -EFAULT;
			}

			if (copy_from_user(&di, argp, sizeof(di)))
				return -EFAULT;

			if (di.config_len > 0) {
				configuration = vmalloc(di.config_len);
				if (configuration == NULL)
					return -EFAULT;

				if (copy_from_user(configuration,
				                   di.configuration,
				                   di.config_len)) {
					vfree(configuration);
					return -EFAULT;
				}

				config_len = di.config_len;
			}

			ret = cordless_domain_init(di.fifo_size, //+di.config_len,
			                           di.cordless_img,
			                           di.cordless_img_size,
			                           di.options);
			if (ret == 0)
				ret = coma_init(di.options, configuration,
				                config_len);

			if (ret != 0) {
				printk(KERN_ERR "%s: init failed\n",
				       cordless_name);
				ret = -EFAULT;
			} else {
				printk(KERN_INFO "%s: init successful\n",
				       cordless_name);
				cordless_loaded = 1;
			}
		}
		return ret;

	case CORDLESS_IOCDEINIT:
		ret = coma_deinit();
		cordless_loaded = 0;
		if (ret == 0)
			ret = cordless_domain_deinit();
		if (ret != 0) {
			printk(KERN_ERR "%s: deinit failed\n", cordless_name);
			ret = -EFAULT;
		} else {
			printk(KERN_INFO "%s: deinit\n", cordless_name);
		}
		return ret;

	case CORDLESS_IOCTIMING:
		{
			struct timing_request *timingreq;
			timingreq = (struct timing_request __user *)arg;
			ret = coma_timing(timingreq->options);
			put_user(ret, &timingreq->timestamp);
			if (copy_to_user(&timingreq->interrupts, timing_stats,
			    66 * 8)) {
				return -EFAULT;
			}
			ret = 0;
		}
		return ret;

	case CORDLESS_IOCCONFIG:
		{
			p_eeprom_update_filp =filp;

			if (wait_event_interruptible(config_wq,
					(config_request != 0))) {
				return -ERESTART;
			}

			mutex_lock(&config_event_mutex);
			if (copy_to_user(argp, configuration, config_len)) {
				mutex_unlock(&config_event_mutex);
				return -EFAULT;
			}
			config_request = 0;
			mutex_unlock(&config_event_mutex);
		}
		return 0;
  	case CORDLESS_IOCMENU:
		ret=coma_menu();
		return ret;
		break;
	case CORDLESS_IOCCONFIG_REPLY:
		{
			struct config_write_reply reply;
			if (copy_from_user(&reply, argp, sizeof(reply)))
				return -EFAULT;
			ret = coma_reply_configuration_write(reply.bytes_written_successfully, reply.result);
			
		}
		return ret;
		
	case CORDLESS_IOCCONFIG_SET_STATUS:
		{
			int status;
			if (copy_from_user(&status, argp, sizeof(status)))
				return -EFAULT;
			//eeprom_update_active = status;
			p_eeprom_update_filp= (struct file*)status;
			ret = 0;
		}
		return ret;
	default:  /* redundant, as cmd was checked against MAXNR */
		return -EINVAL;
	}
}

static struct file_operations cordless_fops = {
	.owner =     THIS_MODULE,
	.ioctl =     cordless_ioctl,
	.read =      cordless_read,
	.open =      cordless_open,
	.release =   cordless_release,
};

/*
 * module operations
 */

static int __init
cordless_init(void)
{
	int ret=0;

        if (CORDLESS_MAJOR) {
		cordless_dev = MKDEV(CORDLESS_MAJOR, 0);
		ret = register_chrdev_region(cordless_dev, 1, cordless_name);
	} else
		ret = alloc_chrdev_region(&cordless_dev, 0, 1, cordless_name);

	if (ret)
		return ret;

	printk(KERN_DEBUG "%s: SCRAM mapped to 0x%x\n", cordless_name,
	       (unsigned int)scram);

	cdev_init(&cordless_cdev, &cordless_fops);

	cordless_cdev.owner = THIS_MODULE;
	cordless_cdev.ops   = &cordless_fops;
	
	ret = cdev_add(&cordless_cdev, cordless_dev, 1);
	if (ret) {
		printk(KERN_ERR "%s: Error %d adding character device",
		       cordless_name, ret);
		goto err_unreg_chrdev_reg;
	}

	printk(KERN_INFO "%s: character device initialized (major=%d)\n",
	       cordless_name, MAJOR(cordless_dev));

	spin_lock_init(&log_messages_lock);
	log_messages = kfifo_alloc(4096, GFP_KERNEL, &log_messages_lock);

	mutex_init(&config_event_mutex);
	init_waitqueue_head(&config_wq);
	config_request = 0;

	return 0;

err_unreg_chrdev_reg:
	unregister_chrdev_region(cordless_dev, 1);

	return ret;
}

static void __exit
cordless_exit(void)
{
	cdev_del(&cordless_cdev);
	unregister_chrdev_region(cordless_dev, 1);
}

module_init(cordless_init);
module_exit(cordless_exit);
