/*
 *  drivers/cordless/dua.c - cordless DUA / netlink interface to user space
 *
 *  Using netlink, a socket interface to Linux user space is implemented that
 *  allows sending DUA messages between user space and cordless.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/netlink.h>
#include <net/sock.h>

#include "coma.h"
#include "cmsg-netlink.h"

MODULE_AUTHOR("DSP Group Inc.");
MODULE_LICENSE("GPL");

//#define DEBUG
#define DUA_CFIFO_SIZE	10240
#define SUBSYSTEM_DUA	3

static char dua_name[] = "coma-dua";

static struct sock *sock_dua;
static __u32 nl_dua_pid;
static struct cfifo *dua_l2c;
static struct cfifo *dua_c2l;

static int dua_initialized = 0;

static int
dua_setup(void)
{
	dua_initialized = 1;

	return 0;
}

static void
dua_remove(void)
{
	dua_initialized = 0;
}

static int
coma_create_dua_message(enum cmsg_netlink_types type,
                            void *payload, unsigned int payload_size)
{
	return coma_create_message(SUBSYSTEM_DUA, (int)type,
	                           NULL, 0, payload, payload_size);
}

static int
dua_to_stack(unsigned char *buf, unsigned int size)
{
	int ret;

	if (! dua_initialized)
		return -EFAULT;

	ret = coma_create_dua_message(CMSG_NETLINK_DUA, buf, size);
	if (ret == 0)
		coma_signal();

	return ret;
}

static void
cordless_nl_send_dua(void *data, unsigned int size)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	void *payload;

#ifdef DEBUG
	printk("%s(size = %d): sending to pid = %d\n", __FUNCTION__, size,
	       nl_dua_pid);
#endif
	
	/* unknown userspace app, do not send to ourself */
	if (!nl_dua_pid)
		return;

	skb = alloc_skb(NLMSG_SPACE(size), GFP_KERNEL);
	if (!skb) {
		printk(KERN_ERR "%s: alloc_skb(size = %d) returned NULL\n",
		       dua_name, NLMSG_SPACE(size));
		return;
	}

	nlh = NLMSG_PUT(skb, 0, 0x123, NLMSG_DONE, size);

	/* data */
	payload = NLMSG_DATA(nlh);
	memcpy(payload, data, size);

	/* send */
	netlink_unicast(sock_dua, skb, nl_dua_pid, MSG_DONTWAIT);

	return;

nlmsg_failure:
	kfree_skb(skb);
}

static int
dua_process_message(struct cmsg *cmsg)
{
	int ret = 0;

	switch(cmsg->type) {
	case CMSG_NETLINK_DUA:
		cordless_nl_send_dua(cmsg->payload, cmsg->payload_size);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static void
cordless_nl_process_dua_message(struct nlmsghdr *nlh)
{
	int size = nlh->nlmsg_len - NLMSG_HDRLEN;
	char *data = NLMSG_DATA(nlh);

#ifdef DEBUG
	printk("%s: called %s from pid = %d\n", dua_name, __FUNCTION__,
	       nl_dua_pid);
	printk("%s: got message with len = %d\n", dua_name, size);
#endif

	dua_to_stack(data, size);
}

static void
cordless_nl_input_dua(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;

#ifdef DEBUG
	printk("%s: called %s()\n", dua_name, __FUNCTION__);
#endif

	if (! dua_initialized)
		return;

	nlh = nlmsg_hdr(skb);
	if (!NLMSG_OK(nlh, skb->len)) {
		printk(KERN_ERR "%s: received corrupt netlink message\n",
		       dua_name);
		return;
	}

	/* update address of the netlink application */
	nl_dua_pid = nlh->nlmsg_pid;

	/* process message from the application */
	cordless_nl_process_dua_message(nlh);
}

static int __init
cordless_dua_init(void)
{
	int ret;

	sock_dua = netlink_kernel_create(&init_net, NETLINK_DUA, 0,
	                          cordless_nl_input_dua, NULL, THIS_MODULE);
	if (!sock_dua){
		/* FIXME there's no netlink_kernel_destroy() */
		return -EFAULT;
	}

	dua_l2c = cfifo_alloc(DUA_CFIFO_SIZE);
	if (IS_ERR(dua_l2c))
		return -ENOMEM;

	dua_c2l = cfifo_alloc(DUA_CFIFO_SIZE);
	if (IS_ERR(dua_c2l)) {
		ret = -ENOMEM;
		goto err_free_cfifo1;
	}

	ret = coma_register(SUBSYSTEM_DUA, dua_l2c, dua_c2l,
	                    dua_process_message, dua_setup,
	                    dua_remove, 0);
	if (ret < 0) {
		ret = -EFAULT;
		goto err_free_cfifo2;
	}

	printk(KERN_INFO "%s: netlink interface registered\n", dua_name);

	return 0;

err_free_cfifo2:
	cfifo_free(dua_c2l);
err_free_cfifo1:
	cfifo_free(dua_l2c);

	return ret;
}
module_init(cordless_dua_init);

static void __exit
cordless_dua_exit(void)
{
	cfifo_free(dua_l2c);
	cfifo_free(dua_c2l);
	sock_release(sock_dua->sk_socket);
}
module_exit(cordless_dua_exit);

