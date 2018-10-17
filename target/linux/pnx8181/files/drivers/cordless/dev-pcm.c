/*
 *  drivers/cordless/dev-pcm.c - PCM character device
 *
 *  This driver registers one character device for each available PCM channel.
 *  It allows only one reader and one writer at the same time.
 *
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

/* TODO:
 *  - if minor numbers dont start at zero, this code will fail
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/uio.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/net.h>
#include <linux/file.h>
#include <linux/completion.h>
#include <asm/uaccess.h>

#include "cmsg-pcm.h"
#include "cfifo.h"
#include "coma.h"

MODULE_AUTHOR("DSP Group Inc.");
MODULE_LICENSE("GPL");

#define SUBSYSTEM_PCM	2

#define FIFO_SIZE 2048

static char pcm_name[] = "coma-pcm";

static dev_t pcm_dev;
static struct cdev pcm_cdev;

static int pcm_initialized = 0;

static struct cfifo *pcm_l2c;
static struct cfifo *pcm_c2l;

static const unsigned int pcm_numlines = CONFIG_CORDLESS_NUM_PCMLINES;

static DECLARE_COMPLETION(pcm_reply_compl);
static DEFINE_MUTEX(pcm_mutex);

static union cmsg_pcm_params last_cmsg_pcm_params;

enum pcm_chan_status {
	CHANNEL_FREE = 0,
	CHANNEL_ALLOCATED,
	CHANNEL_STARTED,
};

/**
 * struct pcm_chan - all the info needed to run one PCM channel
 *
 * @pcm_fifo:  pointer to cfifo used for writing PCM data to cordless
 * @reader:    pointer to the reading process
 * @writer:    pointer to the writing process
 *
 */
struct pcm_chan {
	struct cfifo *pcm_fifo;
	struct file *reader;
	struct file *writer;
	wait_queue_head_t enc_wq;
	enum pcm_chan_status status;
	int flushed;
	int id;
};

static struct pcm_chan pcm_channels[CONFIG_CORDLESS_NUM_PCMLINES];

/*
 * pcm_chan specific function
 */

static int
pcm_registered(void)
{
	return (pcm_initialized != 0);
}

static int
pcm_setup(void)
{
	pcm_initialized = 1;

	return 0;
}

static void
pcm_remove(void)
{
	pcm_initialized = 0;
}

static void
pcm_generic_reply(union cmsg_pcm_params *params)
{
	last_cmsg_pcm_params = *params;
	complete(&pcm_reply_compl);
}

static int
pcm_process_message(struct cmsg *cmsg)
{
	int ret = 0;
	union cmsg_pcm_params *params =
	                      (union cmsg_pcm_params *)cmsg->params;

	switch(cmsg->type) {
	case CMSG_PCM_REPLY_START_CHAN:
	case CMSG_PCM_REPLY_STOP_CHAN:
		pcm_generic_reply(params);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int
coma_create_pcm_message(enum cmsg_pcm_types type,
                        union cmsg_pcm_params *params,
                        void *payload, unsigned int payload_size)
{
	return coma_create_message(SUBSYSTEM_PCM, (int)type,
	                           (void *)params, sizeof(*params),
	                           payload, payload_size);
}

static int
pcm_chan_init(struct pcm_chan *chan)
{
	int ret = -ENOMEM;

	chan->pcm_fifo = cfifo_alloc(FIFO_SIZE);
	if (IS_ERR(chan->pcm_fifo))
		return ret;

	init_waitqueue_head(&chan->enc_wq);

	chan->status = CHANNEL_ALLOCATED;

	return 0;
}

static void
pcm_chan_reset(struct pcm_chan *chan)
{
	if (chan->pcm_fifo)
		cfifo_free(chan->pcm_fifo);
	/* set the status to CHANNEL_FREE */
	memset(chan, 0, sizeof(*chan));
}

/*
 * coma handling
 */

static int
pcm_request_start_chan(int id, struct cfifo *fifo)
{
	int ret;
	union cmsg_pcm_params params;

	params.request_start_chan.id = id;
	params.request_start_chan.fifo = fifo; 
	ret = coma_create_pcm_message(CMSG_PCM_REQUEST_START_CHAN, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
pcm_start_chan(struct pcm_chan *chan)
{
	int ret;

	if (!pcm_registered())
		return -EFAULT;

	mutex_lock(&pcm_mutex);

	ret = pcm_request_start_chan(chan->id, chan->pcm_fifo);
	if (ret < 0) {
		mutex_unlock(&pcm_mutex);
		return -EFAULT;
	}

	wait_for_completion(&pcm_reply_compl);

	ret = last_cmsg_pcm_params.reply_start_chan.result;
	mutex_unlock(&pcm_mutex);

	return ret;
}

static int
pcm_request_stop_chan(int id)
{
	int ret;
	union cmsg_pcm_params params;

	params.request_stop_chan.id = id;

	ret = coma_create_pcm_message(CMSG_PCM_REQUEST_STOP_CHAN, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
pcm_stop_chan(struct pcm_chan *chan)
{
	int ret;

	if (!pcm_registered())
		return -EFAULT;

	mutex_lock(&pcm_mutex);

	ret = pcm_request_stop_chan(chan->id);
	if (ret < 0) {
		mutex_unlock(&pcm_mutex);
		return -EFAULT;
	}

	wait_for_completion(&pcm_reply_compl);

	chan->status = CHANNEL_ALLOCATED;

	ret = last_cmsg_pcm_params.reply_stop_chan.result;
	mutex_unlock(&pcm_mutex);

	return ret;
}

/*
 * character device functions
 */

static int
pcm_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
          unsigned long arg)
{
	int size;

	void __user *argp = (void __user *)arg;
	/* unsigned int minor = iminor(inode);
	struct pcm_chan *chan = &pcm_channels[minor]; */

	if (!pcm_registered())
		return -EFAULT;

	/* if (_IOC_TYPE(cmd) != PCM_IOC_MAGIC)
		return -EINVAL;

	if (_IOC_NR(cmd) > PCM_IOC_MAXNR)
		return -EINVAL; */

	size = _IOC_SIZE(cmd);

	if (_IOC_DIR(cmd) & _IOC_READ)
		if (!access_ok(VERIFY_WRITE, argp, size))
			return -EFAULT;

	if (_IOC_DIR(cmd) & _IOC_WRITE)
		if (!access_ok(VERIFY_READ, argp, size))
			return -EFAULT;

	switch(cmd) {
		/* currently no ioctls defined */
		return 0;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -EINVAL;
	}
}

static int
pcm_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	unsigned int flags = file->f_flags;
	unsigned int minor = iminor(inode);
	struct pcm_chan *chan = &pcm_channels[minor];

	if (!pcm_registered())
		return -EFAULT;

	/* only allow one reader */
	if (flags == O_RDONLY || flags == O_RDWR) {
		if (chan->reader)
			return -EBUSY;
		else {
			chan->reader = file;
			file->private_data = chan;
		}
	}

	/* only allow one writer */
	if (flags == O_WRONLY || flags == O_RDWR) {
		if (chan->writer)
			return -EBUSY;
		else {
			chan->writer = file;
			file->private_data = chan;
		}
	}

	if (chan->status == CHANNEL_FREE)
		pcm_chan_init(chan);

	/* start chan if necessary */
	if (chan->status == CHANNEL_ALLOCATED) {
		chan->id = minor;

		cfifo_reset(chan->pcm_fifo);

		ret = pcm_start_chan(chan);
		if (ret < 0)
			goto err_chan_free;

		chan->status = CHANNEL_STARTED;

		ret = 0;
	}

	goto out;

err_chan_free:
	pcm_stop_chan(chan);
	pcm_chan_reset(chan);
out:
	return ret;
}

static int
pcm_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	unsigned int minor = iminor(inode);
	struct pcm_chan *chan = &pcm_channels[minor];

	if (!pcm_registered())
		return -EFAULT;

	if (chan->reader == file)
		chan->reader = NULL;
	
	if (chan->writer == file)
		chan->writer = NULL;

	if (!chan->reader && !chan->writer) {
		if (chan->status == CHANNEL_STARTED)
			pcm_stop_chan(chan);
		if (chan->status != CHANNEL_FREE)
			pcm_chan_reset(chan);
	}

	return ret;
}

static ssize_t
pcm_write(struct file *file, const char __user *buf, size_t count,
          loff_t *f_pos)
{
	int ret = 0;
	struct pcm_chan *chan = (struct pcm_chan *)file->private_data;
	char *fifo_buf;

	if (!pcm_registered())
		return -EFAULT;

	do {
		ret = cfifo_request(chan->pcm_fifo, (void **)&fifo_buf, count);
		if (ret == 0)
			msleep(10);
		else
			break;
	} while (ret == 0);
	if (ret < 0)
		return -EFAULT;

	ret = copy_from_user(fifo_buf, buf, count);
	if (ret != 0)
		return -1;

	cfifo_commit(chan->pcm_fifo, count);
	coma_signal();

	return count;
}

static struct file_operations pcm_fops = {
	.owner   = THIS_MODULE,
	.ioctl   = pcm_ioctl,
	.open    = pcm_open,
	.release = pcm_release,
	.write   = pcm_write,
};

void
pcm_release_all(void)
{
	int i;
	struct pcm_chan *chan;

	if (!pcm_registered())
		return;

	for (i = 0; i < CONFIG_CORDLESS_NUM_PCMLINES; i++) {
		chan = &pcm_channels[i];

		chan->reader = NULL;
		chan->writer = NULL;

		if (chan->status == CHANNEL_STARTED)
			pcm_stop_chan(chan);
		if (chan->status != CHANNEL_FREE)
			pcm_chan_reset(chan);
	}
}

static int __init
pcm_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&pcm_dev, 0, pcm_numlines, pcm_name);
	if (ret)
		return ret;

	cdev_init(&pcm_cdev, &pcm_fops);

	pcm_cdev.owner = THIS_MODULE;
	pcm_cdev.ops   = &pcm_fops;

	ret = cdev_add(&pcm_cdev, pcm_dev, pcm_numlines);
	if (ret) {
		printk(KERN_ERR "%s: Error %d adding character device",
		       pcm_name, ret);
		goto err_unreg_chrdev_reg;
	}

	pcm_l2c = cfifo_alloc(256);
	if (IS_ERR(pcm_l2c)) {
		ret = -ENOMEM;
		goto err_cdev_del;
	}

	pcm_c2l = cfifo_alloc(256);
	if (IS_ERR(pcm_c2l)) {
		ret = -ENOMEM;
		goto err_cfifo_free1;
	}

	ret = coma_register(SUBSYSTEM_PCM, pcm_l2c, pcm_c2l,
	                    pcm_process_message, pcm_setup, pcm_remove, NULL);
	if (ret < 0) {
		printk(KERN_ERR "%s: Registration failed: %d\n", pcm_name,
		       ret);
		goto err_cfifo_free2;
	}

	printk(KERN_INFO "%s: character device initialized (major=%d)\n",
	       pcm_name, MAJOR(pcm_dev));

	return 0;

err_cfifo_free2:
	cfifo_free(pcm_c2l);
err_cfifo_free1:
	cfifo_free(pcm_l2c);
err_cdev_del:
	cdev_del(&pcm_cdev);
err_unreg_chrdev_reg:
	unregister_chrdev_region(pcm_dev, pcm_numlines);
	return ret;
}

static void __exit
pcm_exit(void)
{
	pcm_release_all();
	coma_deregister(SUBSYSTEM_PCM);
	cfifo_free(pcm_l2c);
	cfifo_free(pcm_c2l);
	cdev_del(&pcm_cdev);
	unregister_chrdev_region(pcm_dev, pcm_numlines);
}

module_init(pcm_init);
module_exit(pcm_exit);

