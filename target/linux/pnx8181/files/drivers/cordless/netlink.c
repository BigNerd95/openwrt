/*
 *  drivers/cordless/netlink.c - generic cordless netlink interface to user
 *                               space
 *
 *  Using netlink, a socket interface to Linux user space is implemented that
 *  allows sending coma messages between user space and cordless.
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

MODULE_AUTHOR("DSP Group Inc.");
MODULE_LICENSE("GPL");

//#define DEBUG
#ifndef NETLINK_CFIFO_SIZE
#define NETLINK_CFIFO_SIZE	10000
#endif

/*
 * The following definitions are expected in the file that includes this file:
 * SERVICE_NAME     - name of service, e.g., "coma-xxx"
 * SERVICE_ID       - id of service
 * NETLINK_ID         - Linux kernel netlink id (see "include/linux/netlink.h")
 * NETLINK_CFIFO_SIZE - size of cfifo (optional, default: 10000)
 */

static struct sock *netlink_sock;
static __u32 netlink_pid;
static struct cfifo *netlink_l2c;
static struct cfifo *netlink_c2l;

static int netlink_initialized = 0;

static int
netlink_setup(void)
{
	netlink_initialized = 1;

	return 0;
}

static void
netlink_remove(void)
{
	netlink_initialized = 0;
}

static int
coma_create_netlink_message(void *payload, unsigned int payload_size)
{
	return coma_create_message(SERVICE_ID, SERVICE_ID,
	                           NULL, 0, payload, payload_size);
}

static int
netlink_to_stack(unsigned char *buf, unsigned int size)
{
	int ret;

	if (! netlink_initialized)
		return -EFAULT;

	ret = coma_create_netlink_message(buf, size);
	if (ret == 0)
		coma_signal();

	return ret;
}

static void
netlink_send(void *data, unsigned int size)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	void *payload;

#ifdef DEBUG
	printk("%s(size = %d): sending to pid = %d\n", __FUNCTION__, size,
	       netlink_pid);
#endif
	
	/* unknown userspace app, do not send to ourself */
	if (!netlink_pid)
		return;

	//skb = alloc_skb(NLMSG_SPACE(size), GFP_KERNEL);
	skb = alloc_skb(NLMSG_SPACE(size), GFP_ATOMIC);
	if (!skb) {
		printk(KERN_ERR "%s: alloc_skb(size = %d) returned NULL\n",
		       SERVICE_NAME, NLMSG_SPACE(size));
		return;
	}

	nlh = NLMSG_PUT(skb, 0, 0x123, NLMSG_DONE, size);

	/* data */
	payload = NLMSG_DATA(nlh);
	memcpy(payload, data, size);

	/* send */
	netlink_unicast(netlink_sock, skb, netlink_pid, MSG_DONTWAIT);

	return;

nlmsg_failure:
	kfree_skb(skb);
}

static int
netlink_process_message(struct cmsg *cmsg)
{
	int ret = 0;

	switch(cmsg->type) {
	case SERVICE_ID:
		netlink_send(cmsg->payload, cmsg->payload_size);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static void
netlink_receive_message(struct nlmsghdr *nlh)
{
	int size = nlh->nlmsg_len - NLMSG_HDRLEN;
	char *data = NLMSG_DATA(nlh);

#ifdef DEBUG
	printk("%s: called %s from pid = %d\n", SERVICE_NAME, __FUNCTION__,
	       netlink_pid);
	printk("%s: got message with len = %d\n", SERVICE_NAME, size);
#endif

	netlink_to_stack(data, size);
}

static void
netlink_input(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;

#ifdef DEBUG
	printk("%s: called %s()\n", SERVICE_NAME, __FUNCTION__);
#endif

	if (!netlink_initialized)
		return;

	nlh = nlmsg_hdr(skb);
	if (!NLMSG_OK(nlh, skb->len)) {
		printk(KERN_ERR "%s: received corrupt netlink message\n",
		       SERVICE_NAME);
		return;
	}

	/* update address of the netlink application */
	netlink_pid = nlh->nlmsg_pid;

	/* process message from the application */
	netlink_receive_message(nlh);
}

static int __init
netlink_init(void)
{
	int ret;

	netlink_sock = netlink_kernel_create(&init_net, NETLINK_ID, 0,
	                                     netlink_input, NULL,
	                                     THIS_MODULE);
	if (!netlink_sock) {
		/* FIXME there's no netlink_kernel_destroy() */
		return -EFAULT;
	}

	netlink_l2c = cfifo_alloc(NETLINK_CFIFO_SIZE);
	if (IS_ERR(netlink_l2c))
		return -ENOMEM;

	netlink_c2l = cfifo_alloc(NETLINK_CFIFO_SIZE);
	if (IS_ERR(netlink_c2l)) {
		ret = -ENOMEM;
		goto err_free_cfifo1;
	}

	ret = coma_register(SERVICE_ID, netlink_l2c, netlink_c2l,
	                    netlink_process_message, netlink_setup,
	                    netlink_remove, 0);
	if (ret < 0) {
		ret = -EFAULT;
		goto err_free_cfifo2;
	}

	printk(KERN_INFO "%s: netlink interface registered\n", SERVICE_NAME);

	return 0;

err_free_cfifo2:
	cfifo_free(netlink_c2l);
err_free_cfifo1:
	cfifo_free(netlink_l2c);

	return ret;
}
module_init(netlink_init);

static void __exit
netlink_exit(void)
{
	cfifo_free(netlink_l2c);
	cfifo_free(netlink_c2l);
	sock_release(netlink_sock->sk_socket);
}
module_exit(netlink_exit);

