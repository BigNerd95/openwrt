/*
 *  drivers/cordless/coma.c - the cordless manager
 *
 *  The cordless manager is responsible for maintaining the main cfifo between
 *  Linux and cordless which is used for loading and unloading cordless and for
 *  registration of other subsystems (e.g., debug output, VoIP/RTP, etc.)
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
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/fiq.h>
#include <asm/uaccess.h>
#include <asm/arch/platform.h>

MODULE_AUTHOR("DSPG Technologies GmbH");
MODULE_LICENSE("GPL");

#define SERVICE_COMA 0

#include "coma.h"
#include "cfifo.h"
#include "cmsg-coma.h"
#include "dev-cordless.h"

static int coma_stack_loaded = 0;
static int coma_stack_initialized = 0;

static DECLARE_COMPLETION(reply_compl);
static DEFINE_MUTEX(coma_mutex);

static union cmsg_coma_params last_cmsg_coma_params;

unsigned char timing_stats[66 * 8];

struct service_s {
	int id;
	int active;
	struct cfifo *l2c;
	struct cfifo *c2l;
	int (*process_message)(struct cmsg *cmsg);
	int (*setup)(void);
	void (*remove)(void);
	void (*poll)(void);
};

struct service_s services[NR_SERVICES];

/*
 * helper functions
 */

int
coma_create_message(int id, int type, void *params, unsigned int params_size,
                    unsigned char *payload, unsigned int payload_size)
{
	int ret;
	struct cmsg *cmsg;

	ret = cmsg_alloc_from_cfifo(&cmsg, services[id].l2c, params_size,
	                            payload_size);
	if (ret != 0)
		return ret;

	cmsg->type = type;

	if (params && params_size)
		memcpy(cmsg->params, params, params_size);

	if (payload && payload_size)
		memcpy(cmsg->payload, payload, payload_size);

	ret = cmsg_commit_to_cfifo(cmsg, services[id].l2c);

	return ret;
}
EXPORT_SYMBOL(coma_create_message);

static int
coma_create_coma_message(enum cmsg_coma_types type,
                         union cmsg_coma_params *params,
                         void *payload, unsigned int payload_size)
{
	return coma_create_message(SERVICE_COMA, (int)type,
	                           (void *)params, sizeof(*params),
	                           payload, payload_size);
}

/*
 * coma functions
 */

void
coma_signal(void)
{
	cordless_signal_cordless();
}
EXPORT_SYMBOL(coma_signal);

int
coma_loaded(void)
{
	return (coma_stack_loaded != 0);
}

int
coma_initialized(void)
{
	return (coma_stack_initialized != 0);
}
EXPORT_SYMBOL(coma_initialized);

static void
coma_generic_reply(union cmsg_coma_params *params)
{
	last_cmsg_coma_params = *params;
	complete(&reply_compl);
}

int coma_menu(void)
{
	int ret;

	if (!coma_initialized())
		return -EFAULT;

	ret = coma_create_coma_message(CMSG_COMA_MENU, NULL, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
coma_request_timing(int options)
{
	union cmsg_coma_params params;
	int ret;

	if (!coma_initialized())
		return -EFAULT;

	params.request_timing.options = options;
	ret = coma_create_coma_message(CMSG_COMA_REQUEST_TIMING, &params,
	                               NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

int
coma_timing(int options)
{
	int ret;
	mutex_lock(&coma_mutex);

	ret = coma_request_timing(options);
	if (ret < 0) {
		mutex_unlock(&coma_mutex);
		return -EFAULT;
	}

	wait_for_completion(&reply_compl);

	ret = last_cmsg_coma_params.reply_timing.timestamp;
	mutex_unlock(&coma_mutex);

	return ret;
}
EXPORT_SYMBOL(coma_timing);

/*
 * service (de)registration
 */

static int
coma_request_register(int id, struct cfifo *l2c, struct cfifo *c2l)
{
	union cmsg_coma_params params;
	int ret;

	if (!coma_loaded())
		return -EFAULT;

	mutex_lock(&coma_mutex);
	params.request_register.id = id;
	params.request_register.l2c = l2c;
	params.request_register.c2l = c2l;

	ret = coma_create_coma_message(CMSG_COMA_REQUEST_REGISTER,
	                               &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	wait_for_completion(&reply_compl);

	ret = last_cmsg_coma_params.reply_register.result;
	mutex_unlock(&coma_mutex);

	return ret;
}

int
coma_register(int id, struct cfifo *l2c, struct cfifo *c2l,
              int (*process_message)(struct cmsg *cmsg),
              int (*setup)(void), void (*remove)(void), void (*poll)(void))
{
	if (id >= NR_SERVICES)
		return -EINVAL;

	services[id].id = id;
	services[id].active = 0;
	services[id].l2c = l2c;
	services[id].c2l = c2l;
	services[id].process_message = process_message;
	services[id].setup = setup;
	services[id].remove = remove;
	services[id].poll = poll;

	if (coma_stack_initialized) {
		int ret = coma_request_register(id, l2c, c2l);
		if (ret == 0) { /* success */
			services[id].active = 1;
			if (setup)
				setup();
		} else
			return ret;
	}

	return 0; /* will register with cordless later */
}
EXPORT_SYMBOL(coma_register);

static int
coma_request_deregister(int id)
{
	union cmsg_coma_params params;
	int ret;

	if (!coma_loaded())
		return -EFAULT;

	mutex_lock(&coma_mutex);
	params.request_deregister.id = id;

	ret = coma_create_coma_message(CMSG_COMA_REQUEST_DEREGISTER,
	                               &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	wait_for_completion(&reply_compl);

	ret = last_cmsg_coma_params.reply_deregister.result;
	mutex_unlock(&coma_mutex);

	return ret;
}

int
coma_deregister(int id)
{
	int ret = 0;

	if (id >= NR_SERVICES)
		return -EINVAL;

	if (coma_stack_initialized)
		ret = coma_request_deregister(id);

	services[id].id = 0;
	services[id].active = 0;
	services[id].l2c = NULL;
	services[id].c2l = NULL;
	services[id].process_message = NULL;
	services[id].setup = NULL;
	services[id].remove = NULL;
	services[id].poll = NULL;

	return ret;
}
EXPORT_SYMBOL(coma_deregister);

/*
 * stack (de)initialization
 */

static int
coma_request_init(int options, unsigned char *configuration,
                  unsigned int config_len)
{
	union cmsg_coma_params params;
	int ret;

	if (!coma_loaded())
		return -EFAULT;

	params.request_init.options = options;

	ret = coma_create_coma_message(CMSG_COMA_REQUEST_INIT, &params,
	                               configuration, config_len);
	if (ret == 0)
		coma_signal();

	return ret;
}

int
coma_init(int options, unsigned char *configuration, unsigned int config_len)
{
	int i, ret;

	ret = coma_request_init(options, configuration, config_len);
	if (ret < 0)
		return -EFAULT;

	wait_for_completion(&reply_compl);

	coma_stack_initialized = 1;

	for (i = 1; i < NR_SERVICES; i++) {
		/* register all other subsystems */
		if (!services[i].active && services[i].id) {
			ret = coma_request_register(i, services[i].l2c,
			                               services[i].c2l);
			if (ret >= 0) { /* success */
				if (services[i].setup)
					ret = services[i].setup();
				if (ret >= 0)
					services[i].active = 1;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(coma_init);

int
coma_deinit(void)
{
	int i;
	int ret = 0;

	if (!coma_loaded())
		return -EFAULT;

	if (coma_stack_initialized) {
		for (i = 1 ; i < NR_SERVICES; i++)
			if (services[i].active)
				coma_request_deregister(i);

		ret = coma_create_coma_message(CMSG_COMA_DEINIT, NULL, NULL, 0);
		if (ret == 0)
			coma_signal();

		coma_stack_initialized = 0;
	}

	for (i = 1; i < NR_SERVICES; i++) {
		if (services[i].active && services[i].remove)
			services[i].remove();
		services[i].active = 0;
	}

	return ret;
}
EXPORT_SYMBOL(coma_deinit);

/*
 * message handling
 */

int
coma_reply_configuration_write(int bytes_written, int result)
{
	int ret;
	union cmsg_coma_params params;

	if (!coma_initialized())
		return -EFAULT;

	mutex_lock(&coma_mutex);
	params.reply_write_configuration.bytes_written = bytes_written;
	params.reply_write_configuration.result = result;

	ret = coma_create_coma_message(CMSG_REPLY_WRITE_CONFIGURATION, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	mutex_unlock(&coma_mutex);

	return ret;
}
EXPORT_SYMBOL(coma_reply_configuration_write);

static int
coma_process_message(struct cmsg *cmsg)
{
	int ret = 0;
	union cmsg_coma_params *params =
	                       (union cmsg_coma_params *)cmsg->params;

	switch(cmsg->type) {
	case CMSG_COMA_REPLY_INIT:
	case CMSG_COMA_REPLY_REGISTER:
	case CMSG_COMA_REPLY_DEREGISTER:
		coma_generic_reply(params);
		break;
	case CMSG_COMA_REPLY_TIMING:
		memcpy(timing_stats, cmsg->payload, 66 * 8);
		coma_generic_reply(params);
		break;
	case CMSG_COMA_WRITE_CONFIGURATION:
		cordless_configuration_write(params->write_configuration.pos,
		                             cmsg->payload,
		                             cmsg->payload_size);
		ret = 0;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

/*
 * interrupt handling
 */

static void
coma_poll(int i)
{
	struct cmsg *cmsg;
	int ret;

	if (!services[i].active || !services[i].c2l ||
	    !services[i].process_message)
		return;

	while ((cfifo_get(services[i].c2l, (void **)&cmsg)) > 0) {
		cmsg->params = cmsg + 1;
		cmsg->payload = (unsigned char *)(cmsg + 1) + cmsg->params_size;
		ret = services[i].process_message(cmsg);
		if (ret < 0)
			printk(KERN_ERR "coma: error processing cmsg "
			       "(type = %d, ret = %d, services%i)\n",
			       cmsg->type, ret, i);
	
		cfifo_processed(services[i].c2l);
	}

	if (services[i].poll)
		services[i].poll();
}

static void
coma_process_irq(unsigned long dummy)
{
	int i;

	for (i = 0; i < NR_SERVICES; i++)
		coma_poll(i);
}

DECLARE_TASKLET(coma_tasklet, coma_process_irq, 0);

static irqreturn_t
coma_irq_handler(int irq, void *dev_id)
{
	tasklet_schedule(&coma_tasklet);
	return IRQ_HANDLED;
}

/*
 * start-up and release
 */

int coma_setup(struct cfifo *l2c, struct cfifo *c2l)
{
	int ret;

	services[SERVICE_COMA].id = SERVICE_COMA;
	services[SERVICE_COMA].active = 1;
	services[SERVICE_COMA].l2c = l2c;
	services[SERVICE_COMA].c2l = c2l;
	services[SERVICE_COMA].setup = NULL;
	services[SERVICE_COMA].process_message = coma_process_message;
	services[SERVICE_COMA].poll = NULL;

	ret = request_irq(COMA_LINUX_IID, coma_irq_handler, 0, "coma", NULL);
	if (ret != 0) {
		printk(KERN_ERR "coma: cannot register irq.\n");
		return ret;
	}

	coma_stack_loaded = 1;

	return 0;
}
EXPORT_SYMBOL(coma_setup);

void coma_release(void)
{
	int i;

	coma_stack_loaded = 0;
	coma_stack_initialized = 0;

	for (i = 1 ; i < NR_SERVICES; i++)
		services[i].active = 0;

	free_irq(COMA_LINUX_IID, NULL);
}
EXPORT_SYMBOL(coma_release);
