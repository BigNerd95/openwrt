/*
 *  drivers/cordless/dev-voice.c - voice character device
 *
 *  This driver registers one character device for each available voice channel.
 *  It allows only one reader and one writer at the same time.
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

/* TODO:
 *  - if minor numbers dont start at zero, this code will fail
 *  - read/write several rtp packets at once
 *  - merge the two receiver threads (rtp & rtcp)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/cdev.h>
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

#include <cordless/voice.h>
#include "dev-voice.h"
#include "cfifo.h"
#include "cmsg-voice.h"
#include "cmsg.h"
#include "coma.h"

MODULE_AUTHOR("DSPG Technologies GmbH");
MODULE_LICENSE("GPL");

#define SUBSYSTEM_VOICE       4

#define FIFO_SIZE             1024

#define VOICE_CODEC_G711_ALAW 8
#define VOICE_CODEC_G722      9

#define DEFAULT_CODEC         VOICE_CODEC_G722
#define DEFAULT_DURATION      20

#define RTCP_PT_SR            200
#define RTCP_PT_RR            201

static char voice_name[] = "coma-voice";

dev_t voice_dev;
static struct cdev voice_cdev;

static struct cfifo *voice_c2l;
static struct cfifo *voice_l2c;
int voice_reg;

const unsigned int voice_numlines = CONFIG_CORDLESS_NUM_VOICELINES;

static DECLARE_COMPLETION(voice_reply_compl);
static DEFINE_MUTEX(voice_mutex);

static union cmsg_voice_params last_cmsg_voice_params;


enum voice_chan_status {
	CHANNEL_INVALID = 0,
	CHANNEL_INITIALIZED,
	CHANNEL_STARTED,
};

struct voice_kmode_rtcp {
	struct socket *sock;
	struct sockaddr_in remote_addr;
	struct task_struct *receiver;
	struct completion receiver_exit;
	struct mutex dec_fifo_mutex;
};

/**
 * struct voice_kmode - all information for kernelmode of one audio channel
 *
 * @sock:        socket to be used for rtp session
 * @remote_addr: address of peer
 * @receiver:    receiver kthread (receive rtp data from peer)
 * @sender:      sender kthread (send rtp data to peer)
 *
 */
struct voice_kmode {
	struct socket *sock;
	struct sockaddr_in remote_addr;
	struct task_struct *receiver;
	struct task_struct *sender;
	struct completion receiver_exit;
	struct voice_kmode_rtcp *rtcp;
};

/**
 * struct voice_chan - all the info needed to run one audio channel
 *
 * @enc_fifo:  pointer to cfifo used for the encoder
 * @dec_fifo:  pointer to cfifo used for the decoder
 * @reader:    pointer to the reading process
 * @writer:    pointer to the writing process
 *
 */
struct voice_chan {
	struct cfifo *enc_fifo;
	struct cfifo *dec_fifo;
	struct file *reader;
	struct file *writer;
	wait_queue_head_t enc_wq;
	enum voice_chan_status status;
	int id;
	struct voice_codec codec;
	struct mutex codec_mutex;
	int flushed;
	struct voice_kmode *kmode;
	/* dtmf handling */
	struct voice_dtmf_event *dtmf_event;
	unsigned int signal_event;
	struct mutex dtmf_event_mutex;
	struct mutex signal_event_mutex;
	wait_queue_head_t dtmf_wq;
	wait_queue_head_t signal_wq;
};

static struct voice_chan voice_channels[CONFIG_CORDLESS_NUM_VOICELINES];

static int 
voice_registered(void)
{
	return (voice_reg != 0);
}

static int
voice_setup(void)
{
	voice_reg = 1;
	return 0;
}

static void
voice_remove(void)
{
	voice_reg = 0;
}



static void
voice_generic_reply(union cmsg_voice_params *params)
{
	last_cmsg_voice_params = *params;
	complete(&voice_reply_compl);
}

/*
 * voice_chan specific function
 */

static void
voice_chan_reset(struct voice_chan *chan)
{
	cfifo_free(chan->enc_fifo);
	cfifo_free(chan->dec_fifo);
	memset(chan, 0, sizeof(*chan));
}

/*
 * coma handling
 */

void
voice_poll_fifos(void)
{
	int i;
	struct voice_chan *chan;

	for (i = 0; i < voice_numlines; i++) {
		chan = &voice_channels[i];

		if (chan->status == CHANNEL_INVALID)
			continue;

		if (!cfifo_empty(chan->enc_fifo))
			wake_up(&chan->enc_wq);
	}
}

void
voice_receive_signal(int id)
{
	struct voice_chan *chan = &voice_channels[id];

	mutex_lock(&chan->signal_event_mutex);
	chan->signal_event=1;

	mutex_unlock(&chan->signal_event_mutex);
	wake_up(&chan->signal_wq);
}


int
voice_process_message(struct cmsg *cmsg)
{
	int ret = 0;
	union cmsg_voice_params *params =
	                      (union cmsg_voice_params *)cmsg->params;

	switch(cmsg->type) {
	case CMSG_VOICE_REPLY_GET_CHAN:
	case CMSG_VOICE_REPLY_SET_CHAN_FIFOS:
	case CMSG_VOICE_REPLY_START_CHAN:
	case CMSG_VOICE_REPLY_STOP_CHAN:
	case CMSG_VOICE_REPLY_FREE_CHAN:
	case CMSG_VOICE_REPLY_START_RTCP:
	case CMSG_VOICE_REPLY_STOP_RTCP:
	case CMSG_VOICE_REPLY_SEND_DTMF:
		voice_generic_reply(params);
		break;
#if 0
	case CMSG_REPLY_REPORT_RTCP:
		rtcp_buf = cmsg->payload;
		rtcp_len = cmsg->payload_size;
		coma_generic_reply(params);
		break;
#endif
	case CMSG_VOICE_RECEIVE_DTMF:
		voice_receive_dtmf(params->receive_dtmf.id,
		                   params->receive_dtmf.status,
		                   params->receive_dtmf.event,
		                   params->receive_dtmf.volume,
		                   params->receive_dtmf.duration);
		break;
	case CMSG_VOICE_REMOTE_START:
		voice_receive_signal(params->receive_dtmf.id);

		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int
coma_create_voice_message(enum cmsg_voice_types type,
                        union cmsg_voice_params *params,
                        void *payload, unsigned int payload_size)
{
	return coma_create_message(SUBSYSTEM_VOICE, (int)type,
	                           (void *)params, sizeof(*params),
	                           payload, payload_size);
}

static int
voice_request_get_chan(int id)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_get_chan.id = id;
	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_GET_CHAN, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_get_chan(struct voice_chan *chan)
{
	int ret;

	mutex_lock(&voice_mutex);

	ret = voice_request_get_chan(chan->id);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_get_chan.result;
	mutex_unlock(&voice_mutex);

	return ret;

}

static int
voice_request_set_chan_fifos(int id, struct cfifo *enc, struct cfifo *dec)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_set_chan_fifos.id = id;
	params.request_set_chan_fifos.enc = enc;
	params.request_set_chan_fifos.dec = dec;

	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_SET_CHAN_FIFOS, &params,
	                          NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_set_fifos(struct voice_chan *chan)
{
	int ret;

	mutex_lock(&voice_mutex);

	ret = voice_request_set_chan_fifos(chan->id, chan->enc_fifo, chan->dec_fifo);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_set_chan_fifos.result;
	mutex_unlock(&voice_mutex);

	return ret;

}

static int
voice_request_start_chan(int id, struct voice_codec *codec)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_start_chan.id = id;
	params.request_start_chan.rx_codec = codec->rx_codec;
	params.request_start_chan.rx_codec_event = codec->rx_codec_event;
	params.request_start_chan.tx_codec = codec->tx_codec;
	params.request_start_chan.tx_codec_event = codec->tx_codec_event;
	params.request_start_chan.duration = codec->duration;
	params.request_start_chan.opts = codec->opts;
	params.request_start_chan.cng.level_rx=codec->cng.level_rx;
	params.request_start_chan.cng.level_tx=codec->cng.level_tx;
	params.request_start_chan.cng.max_sid_update=codec->cng.max_sid_update;
	params.request_start_chan.cng.mode_rx=codec->cng.mode_rx;
	params.request_start_chan.cng.mode_tx=codec->cng.mode_tx;
	params.request_start_chan.cng.vad_detect_level=codec->cng.vad_detect_level;
	params.request_start_chan.cng.vad_hangover=codec->cng.vad_hangover;

	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_START_CHAN, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_start_chan(struct voice_chan *chan)
{
	int ret;

	mutex_lock(&voice_mutex);

	ret = voice_request_start_chan(chan->id, &chan->codec);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_start_chan.result;
	mutex_unlock(&voice_mutex);

	return ret;
}

static int
voice_request_stop_chan(int id)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_stop_chan.id = id;

	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_STOP_CHAN, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_stop_chan(struct voice_chan *chan)
{
	int ret;

	mutex_lock(&voice_mutex);

	ret = voice_request_stop_chan(chan->id);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_stop_chan.result;
	mutex_unlock(&voice_mutex);

	return ret;
}

static int
voice_request_start_rtcp(int id,int rtcp_interval)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_start_rtcp.id = id;
	params.request_start_rtcp.rtcp_interval=rtcp_interval;
 
	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_START_RTCP, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_start_rtcp(struct voice_chan *chan,int rtcp_interval)
{
	int ret;

	mutex_lock(&voice_mutex);

	ret = voice_request_start_rtcp(chan->id,rtcp_interval);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_start_rtcp.result;
	mutex_unlock(&voice_mutex);

	return ret;
}

static int
voice_request_stop_rtcp(int id)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_stop_rtcp.id = id;

	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_STOP_RTCP, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_stop_rtcp(struct voice_chan *chan)
{
	int ret;

	mutex_lock(&voice_mutex);

	ret = voice_request_stop_rtcp(chan->id);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_stop_rtcp.result;
	mutex_unlock(&voice_mutex);

	return ret;
}

static int
voice_request_report_rtcp(int id)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_report_rtcp.id = id;

	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_REPORT_RTCP, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_report_rtcp(struct voice_chan *chan, unsigned char *buf, int len)
{
#if 0
	int ret;

	mutex_lock(&voice_mutex);

	rtcp_buf = NULL;
	rtcp_len = 0;
	ret = voice_request_report_rtcp(chan->id);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_report_rtcp.result;
	if ((rtcp_buf != NULL) && (rtcp_len > 0)) {
		if (rtcp_len > len)
			rtcp_len = len;
		memcpy(buf, rtcp_buf, rtcp_len);
	}
	mutex_unlock(&voice_mutex);


	return ret;
#endif
	return 0;
} 

static int
voice_request_free_chan(int id)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_free_chan.id = id;

	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_FREE_CHAN, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}

static int
voice_free_chan(struct voice_chan *chan)
{
	int ret;

	mutex_lock(&voice_mutex);

	ret = voice_request_free_chan(chan->id);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_free_chan.result;
	mutex_unlock(&voice_mutex);

	return ret;
}

static int
voice_chan_init(struct voice_chan *chan)
{
	int ret = -ENOMEM;

	chan->enc_fifo = cfifo_alloc(FIFO_SIZE);
	if (IS_ERR(chan->enc_fifo))
		goto err;

	chan->dec_fifo = cfifo_alloc(FIFO_SIZE);
	if (IS_ERR(chan->dec_fifo))
		goto err_free_enc;

	init_waitqueue_head(&chan->enc_wq);

	chan->codec.rx_codec = DEFAULT_CODEC;
	chan->codec.rx_codec_event = 0xff;
	chan->codec.tx_codec = DEFAULT_CODEC;
	chan->codec.tx_codec_event = 0xff;
	chan->codec.duration = DEFAULT_DURATION;
	chan->codec.opts = 0;
	mutex_init(&chan->codec_mutex);

	mutex_init(&chan->dtmf_event_mutex);
	init_waitqueue_head(&chan->dtmf_wq);

	mutex_init(&chan->signal_event_mutex);
	init_waitqueue_head(&chan->signal_wq);


	ret = voice_get_chan(chan);
	if (ret != 0)
		goto err_chan_reset;

	ret = voice_set_fifos(chan);
	if (ret != 0)
		goto err_chan_free;

	chan->status = CHANNEL_INITIALIZED;

	return 0;

err_chan_free:
	voice_free_chan(chan);
err_chan_reset:
	cfifo_free(chan->dec_fifo);
err_free_enc:
	cfifo_free(chan->enc_fifo);
err:
	return ret;
}

/*
 * kernelmode
 */

static int
voice_kmode_thread_recv(void *data)
{
	struct voice_chan *chan = (struct voice_chan *)data;
	struct voice_kmode *kmode = chan->kmode;
	struct socket *sock = kmode->sock;
	char buf[1024];
	char *dec_buf;
	struct msghdr msg;
	struct iovec iov;
	int recvd;
	int ret;

	memset(&msg, 0, sizeof(msg));
	memset(&iov, 0, sizeof(iov));

	msg.msg_name = &kmode->remote_addr;
	msg.msg_namelen = sizeof(kmode->remote_addr);

	/* daemonize() disabled all signals, allow SIGTERM */
	allow_signal(SIGTERM);

	while(!signal_pending(current) && !kthread_should_stop()) {
		//fd_set_bits fds;
		//int max_fds = 1;
		//s64 timeout = -1; /* infinite */

		/* The data of the IOVs is overriden by the kernel, reset them */
		iov.iov_base = buf;
		iov.iov_len  = sizeof(buf);

		/* fds.in = sock;
		zero_fd_set(max_fds, fds.res_in);
		ret = do_select(max_fds, &fds, &timeout); */

		/* kernel_recvmsg does most of the setup stuff for us */
		recvd = kernel_recvmsg(sock, &msg, (struct kvec *)&iov, 1,
				sizeof(buf), msg.msg_flags);

		/* Some sanity checks */
		if(recvd < 0 || kthread_should_stop() || signal_pending(current))
			break;

		if (unlikely(kmode->rtcp))
			mutex_lock(&kmode->rtcp->dec_fifo_mutex);

		ret = cfifo_request(chan->dec_fifo, (void **)&dec_buf, recvd);
		if (ret <= 0)
			return -ENOMEM;

		memcpy(dec_buf, buf, recvd);
		cfifo_commit(chan->dec_fifo, recvd);

		if (unlikely(kmode->rtcp))
			mutex_unlock(&kmode->rtcp->dec_fifo_mutex);

		coma_signal();
		set_current_state(TASK_INTERRUPTIBLE);
	}

	complete_and_exit(&kmode->receiver_exit, 0);
}

static int
voice_kmode_thread_recv_rtcp(void *data)
{
	struct voice_chan *chan = (struct voice_chan *)data;
	struct voice_kmode_rtcp *rtcp = chan->kmode->rtcp;
	struct socket *sock = chan->kmode->rtcp->sock;
	char buf[1024];
	char *dec_buf;
	struct msghdr msg;
	struct iovec iov;
	int recvd;
	int ret;

	memset(&msg, 0, sizeof(msg));
	memset(&iov, 0, sizeof(iov));

	msg.msg_name = &rtcp->remote_addr;
	msg.msg_namelen = sizeof(rtcp->remote_addr);

	/* daemonize() disabled all signals, allow SIGTERM */
	allow_signal(SIGTERM);

	while(!signal_pending(current) && !kthread_should_stop()) {
		/* data of the IOVs is overriden by the kernel, reset them */
		iov.iov_base = buf;
		iov.iov_len  = sizeof(buf);

		/* kernel_recvmsg does most of the setup stuff for us */
		recvd = kernel_recvmsg(sock, &msg, (struct kvec *)&iov, 1,
				sizeof(buf), msg.msg_flags);

		/* some sanity checks */
		if(recvd < 0 || kthread_should_stop() || signal_pending(current))
			break;

		mutex_lock(&rtcp->dec_fifo_mutex);

		ret = cfifo_request(chan->dec_fifo, (void **)&dec_buf, recvd);
		if (ret <= 0)
			return -ENOMEM;

		memcpy(dec_buf, buf, recvd);
		cfifo_commit(chan->dec_fifo, recvd);

		mutex_unlock(&rtcp->dec_fifo_mutex);

		coma_signal();
		set_current_state(TASK_INTERRUPTIBLE);
	}

	complete_and_exit(&rtcp->receiver_exit, 0);
}

static int
voice_kmode_thread_send(void *data)
{
	struct voice_chan *chan = (struct voice_chan *)data;
	struct voice_kmode *kmode = chan->kmode;
	struct socket *sock = kmode->sock;
	unsigned char *enc_buf;
	struct msghdr msg;
	struct iovec iov;
	int to_send, sent;
	int sent_rtcp = 0;

	memset(&msg, 0, sizeof(msg));
	memset(&iov, 0, sizeof(iov));

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_name = &kmode->remote_addr;
	msg.msg_namelen = sizeof(kmode->remote_addr);

	if (!chan->flushed) {
		cfifo_reset(chan->enc_fifo);
		chan->flushed = 1;
	}

	while (!kthread_should_stop()) {
		wait_event_interruptible(chan->enc_wq,
			((to_send = cfifo_get(chan->enc_fifo,
			                      (void **)&enc_buf)) > 0) ||
			kthread_should_stop());

		if (kthread_should_stop())
			break;

		/*
		 * choose socket and destination address depending on wether
		 * we have a RTCP or RTP message
		 */
		if (unlikely((enc_buf[1] == RTCP_PT_SR) ||
		              enc_buf[1] == RTCP_PT_RR)) {
			sock = kmode->rtcp->sock;
			msg.msg_name = &kmode->rtcp->remote_addr;
			msg.msg_namelen = sizeof(kmode->rtcp->remote_addr);
			sent_rtcp = 1;
		} else if (unlikely(sent_rtcp)) {
			sock = kmode->sock;
			msg.msg_name = &kmode->remote_addr;
			msg.msg_namelen = sizeof(kmode->remote_addr);
			sent_rtcp = 0;
		}
			
		/* send data */
		iov.iov_base = enc_buf;
		iov.iov_len = to_send;

		sent = sock_sendmsg(sock, &msg, to_send);
		if (sent < 0)
			printk(KERN_ERR "%s(%d): sock_sendmsg() failed with"
			       "error %d\n", __FUNCTION__, chan->id, sent);
		else if (sent != to_send)
			printk(KERN_ERR "%s(%d): failed to send %d bytes\n",
			       __FUNCTION__, chan->id, to_send - sent);
		
		cfifo_processed(chan->enc_fifo);
	}

	return 0;
}

static void
voice_kmode_stop_receiver(struct task_struct *task, struct completion *exit)
{
	int err;

	/*	
	 * The receiver maybe blocked in sock_recvmsg()
	 * we will send SIGTERM to the kthread, to avoid 
	 * crashes we must hold the big kernel lock
	 */
	lock_kernel();
	err = kill_proc(task->pid, SIGTERM, 1);
	unlock_kernel();

	if(err)
		printk("%s(): Can't stop receiver thread!\n", __FUNCTION__);

	wait_for_completion(exit);
}

static int
voice_kmode_start_rtcp(struct voice_chan *chan,
                           struct voice_kernelmode_rtcp *km_rtcp)
{
	struct voice_kmode_rtcp *rtcp;
	int ret;

	/* do not proceed if kernelmode is not active */
	if (!chan->kmode)
		return -1;

	rtcp = kmalloc(sizeof(*rtcp), GFP_KERNEL);
	if (!rtcp)
		return -ENOMEM;

	/* set up rtcp structure */
	rtcp->remote_addr = km_rtcp->remote_addr;
	rtcp->sock = sockfd_lookup(km_rtcp->sock_fd, &ret);
	init_completion(&rtcp->receiver_exit);
	mutex_init(&rtcp->dec_fifo_mutex);

	if (!rtcp->sock)
		goto err_free_rtcp;

	chan->kmode->rtcp = rtcp;

	/* create receiver thread */
	rtcp->receiver = kthread_create(voice_kmode_thread_recv_rtcp, chan,
	                                "kvoice_recv%d_rtcp", chan->id);
	if (IS_ERR(rtcp->receiver)) {
		ret = PTR_ERR(rtcp->receiver);
		goto err_free_rtcp;
	}

	/* start thread */
	wake_up_process(rtcp->receiver);

	/* notify cordless */
	voice_start_rtcp(chan,km_rtcp->rtcp_interval);
	
	return 0;

err_free_rtcp:
	chan->kmode->rtcp = NULL;
	kfree(rtcp);

	return ret;
}

static void
voice_kmode_stop_rtcp(struct voice_chan *chan)
{
	struct voice_kmode_rtcp *rtcp = chan->kmode->rtcp;

	/* notify cordless */
	voice_stop_rtcp(chan);

	/* stop the rest */
	voice_kmode_stop_receiver(rtcp->receiver, &rtcp->receiver_exit);

	chan->kmode->rtcp = NULL;
	kfree(rtcp);
}

static int
voice_kmode_start(struct voice_chan *chan, struct voice_kernelmode *km)
{
	struct voice_kmode *kmode;
	int ret;

	/* check if kernelmode is already active */
	if (chan->kmode)
		return -1;

	kmode = kmalloc(sizeof(*kmode), GFP_KERNEL);
	if (!kmode)
		return -ENOMEM;

	/* set up kmode structure */
	kmode->remote_addr = km->remote_addr;
	kmode->sock = sockfd_lookup(km->sock_fd, &ret);
	kmode->rtcp = NULL;
	init_completion(&kmode->receiver_exit);

	if (!kmode->sock)
		goto err_free_kmode;

	chan->kmode = kmode;

	/* create receiver thread */
	kmode->receiver = kthread_create(voice_kmode_thread_recv, chan,
	                                 "kvoice_recv%d", chan->id);
	if (IS_ERR(kmode->receiver)) {
		ret = PTR_ERR(kmode->receiver);
		goto err_free_kmode;
	}

	/* create sender thread */
	kmode->sender = kthread_create(voice_kmode_thread_send, chan,
	                               "kvoice_send%d", chan->id);
	if (IS_ERR(kmode->sender)) {
		ret = PTR_ERR(kmode->sender);
		goto err_stop_recv;
	}

	/* start the threads */
	wake_up_process(kmode->receiver);
	wake_up_process(kmode->sender);

	return 0;

err_stop_recv:
	voice_kmode_stop_receiver(kmode->receiver, &kmode->receiver_exit);

err_free_kmode:
	chan->kmode = NULL;
	kfree(kmode);

	return ret;
}

static void
voice_kmode_stop(struct voice_chan *chan)
{
	struct voice_kmode *kmode = chan->kmode;

	/* make sure kmode is active */
	if (!chan->kmode)
		return;

	if (kmode->rtcp)
		voice_kmode_stop_rtcp(chan);

	/*
	 * Stop the threads.
	 *
	 * As the receiver thread is heavily blocked by sock_recvmsg, we have
	 * to kill it differently and harder.
	 */
	kthread_stop(kmode->sender);
	voice_kmode_stop_receiver(kmode->receiver, &kmode->receiver_exit);
	
	sockfd_put(kmode->sock);

	kfree(kmode);
	chan->kmode = NULL;
}

/*
 * dtmf handling
 */

void
voice_receive_dtmf(int id, int status, int event, int volume, int duration)
{
	struct voice_chan *chan = &voice_channels[id];
	struct voice_dtmf_event *dtmf_event;

	mutex_lock(&chan->dtmf_event_mutex);

	/*
	 * incase there already is an unhandled dtmf event, overwrite it with
	 * the new one
	 */
	if (chan->dtmf_event != NULL) {
		dtmf_event = chan->dtmf_event;
	} else {
		/* if not, create one */
		dtmf_event = kmalloc(sizeof(*dtmf_event), GFP_KERNEL);
		if (!dtmf_event)
			return;
	}

	dtmf_event->status = status;
	dtmf_event->event = event;
	dtmf_event->volume = volume;
	dtmf_event->duration = duration;

	chan->dtmf_event = dtmf_event;

	mutex_unlock(&chan->dtmf_event_mutex);
	wake_up(&chan->dtmf_wq);
}


static int
voice_request_send_dtmf(int id, int status, char event, char volume,
                             int duration,int EvtDuration,int MaxEvtDuration)
{
	int ret;
	union cmsg_voice_params params;

	if (!voice_registered())
		return -EFAULT;

	params.request_send_dtmf.id = id;
	params.request_send_dtmf.status = status;
	params.request_send_dtmf.event = event;
	params.request_send_dtmf.volume = volume;
	params.request_send_dtmf.duration = duration;
	params.request_send_dtmf.EvtDuration =EvtDuration;
	params.request_send_dtmf.MaxEvtDuration=MaxEvtDuration;

	ret = coma_create_voice_message(CMSG_VOICE_REQUEST_SEND_DTMF, &params, NULL, 0);
	if (ret == 0)
		coma_signal();

	return ret;
}


static int
voice_send_dtmf(struct voice_chan *chan, struct voice_dtmf_event *new)
{
	int ret;
	mutex_lock(&voice_mutex);

	ret = voice_request_send_dtmf(chan->id, new->status, new->event, new->volume, new->duration, new->EvtDuration, new->MaxEvtDuration);
	if (ret < 0) {
		mutex_unlock(&voice_mutex);
		return -EFAULT;
	}

	wait_for_completion(&voice_reply_compl);

	ret = last_cmsg_voice_params.reply_send_dtmf.result;
	mutex_unlock(&voice_mutex);

	return ret;
}

/*
 * character device functions
 */

static int
voice_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
            unsigned long arg)
{
	int size;
	int ret = 0;

	void __user *argp = (void __user *)arg;
	unsigned int minor = iminor(inode);
	struct voice_chan *chan = &voice_channels[minor];

	if (_IOC_TYPE(cmd) != VOICE_IOC_MAGIC)
		return -EINVAL;

	if (_IOC_NR(cmd) > VOICE_IOC_MAXNR)
		return -EINVAL;

	size = _IOC_SIZE(cmd);

	if (_IOC_DIR(cmd) & _IOC_READ)
		if (!access_ok(VERIFY_WRITE, argp, size))
			return -EFAULT;
			
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		if (!access_ok(VERIFY_READ, argp, size))
			return -EFAULT;

	switch(cmd) {
	case VOICE_IOCSETCODEC:
	{
		struct voice_codec new;
			
		if (copy_from_user(&new, argp, sizeof(new)))
			return -EFAULT;
		
		/* 
		 * TODO: switch codec in stack, this will also do
		 *       the sanity checking for us (result in reply)
		 */
			
		/* update our local info if codec switch went ok */
		if (chan->status == CHANNEL_INVALID)
			return -EFAULT;

		if (chan->status == CHANNEL_STARTED)
			voice_stop_chan(chan);

		mutex_lock(&chan->codec_mutex);
		chan->codec = new;
		mutex_unlock(&chan->codec_mutex);

		ret = voice_start_chan(chan);
		if (ret < 0)
			return -EFAULT;

		chan->status = CHANNEL_STARTED;
		return 0;
	}

	case VOICE_IOCSENDDTMF:
	{
		struct voice_dtmf_event dtmf;

		if (copy_from_user(&dtmf, argp, sizeof(dtmf)))
			return -EFAULT;

		ret = voice_send_dtmf(chan, &dtmf);
		if (ret != 0)
			return -EFAULT;
		
		return 0;
	}
	
	case VOICE_IOCGETDTMF:
	{
		if (wait_event_interruptible(chan->dtmf_wq,
				(chan->dtmf_event != NULL))) {
			return -ERESTARTSYS;
		}

		mutex_lock(&chan->dtmf_event_mutex);
		if (copy_to_user(argp, chan->dtmf_event, 
		                 sizeof(*chan->dtmf_event))) {
			mutex_unlock(&chan->dtmf_event_mutex);
			return -EFAULT;
		}
		kfree(chan->dtmf_event);
		chan->dtmf_event = NULL;
		mutex_unlock(&chan->dtmf_event_mutex);
	
		return 0;
	}

	case VOICE_IOCGETSIGNAL:
	{
		if (wait_event_interruptible(chan->signal_wq,chan->signal_event >0 )) {
			printk("voice get signal error handler\n");
			return -ERESTARTSYS;
		}
		chan->signal_event = 0;
		return 0;
	}


	case VOICE_IOCFLUSH:
		cfifo_reset(chan->enc_fifo);
		return 0;
	
	case VOICE_IOCKERNELMODE:
	{
		struct voice_kernelmode km;

		/*
		 * if argp is set, start kernelmode, if its NULL,
		 * stop kernelmode
		 */
		if (argp) {
			if (copy_from_user(&km, argp, sizeof(km)))
				return -EFAULT;

			ret = voice_kmode_start(chan, &km);
		} else {
			voice_kmode_stop(chan);
		}
			
		return ret;
	}

	case VOICE_IOCRTCP:
	{
		struct voice_kernelmode_rtcp km_rtcp;
       
		/* if argp is set, start rtcp, if its NULL, stop rtcp */
		if (argp) {
			if (copy_from_user(&km_rtcp, argp, sizeof(km_rtcp)))
				return -EFAULT;
       	ret = voice_kmode_start_rtcp(chan, &km_rtcp);
		} else {
			voice_kmode_stop_rtcp(chan);
		}
	
		return 0;
	}

	default:  /* redundant, as cmd was checked against MAXNR */
		return -EINVAL;
	}
}

static int
voice_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	unsigned int flags = file->f_flags & O_ACCMODE;
	unsigned int minor = iminor(inode);
	struct voice_chan *chan = &voice_channels[minor];

	if (!voice_registered())
		return -1;

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

	if (chan->status == CHANNEL_INVALID) {
		chan->id = minor;
		ret = voice_chan_init(chan);
	}

	return ret;
}

static int
voice_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	unsigned int minor = iminor(inode);
	struct voice_chan *chan = &voice_channels[minor];

	if (!voice_registered())
		return -1;

	if (chan->reader == file)
		chan->reader = NULL;
	
	if (chan->writer == file)
		chan->writer = NULL;

	if (!chan->reader && !chan->writer) {
		if (chan->kmode)
			voice_kmode_stop(chan);

		voice_stop_chan(chan);
		voice_free_chan(chan);
		voice_chan_reset(chan);
	}

	return ret;
}


static ssize_t
voice_read(struct file *file, char __user *buf, size_t count_want,
           loff_t *f_pos)
{
	struct voice_chan *chan = (struct voice_chan *)file->private_data;
	unsigned char *enc_buf;
	size_t not_copied;
#if 0
	ssize_t count_remain = count_want;
#endif
	ssize_t to_copy;
	ssize_t ret;

	if (!voice_registered())
		return -1;

	if (chan->kmode)
		return -EBUSY;

	if (!chan->flushed) {
		cfifo_reset(chan->enc_fifo);
		chan->flushed = 1;
	}

	if (wait_event_interruptible(chan->enc_wq,
			((ret = cfifo_get(chan->enc_fifo, (void **)&enc_buf))
			   > 0)))
		return -ERESTARTSYS;

#if 0
	/*
	 * Read data until the next data chunk does not fit into the requested
	 * buffer anymore.
	 */
	while (ret > 0 && ret <= count_want) {
		to_copy = ret;
		not_copied = copy_to_user(buf, enc_buf, to_copy);
		cfifo_processed(enc_fifo);

		count_remain -= ret;
		if (unlikely(not_copied)) {
			count_remain += not_copied;
			
			/*
			 * We don't exit the loop here because the uncopied data
			 * will be lost anyway. Let's just happily continue :-)
			 */
		}
		if (count_remain <= 0)
			break;

		ret = cfifo_get(enc_fifo, (void *)&enc_buf);
	}
	
	return count_want - count_remain;
#endif

	/* copy one rtp packet at a time */
	to_copy = ret;
	not_copied = copy_to_user(buf, enc_buf, to_copy);
	cfifo_processed(chan->enc_fifo);

	return ret - not_copied;
}

static ssize_t
voice_write(struct file *file, const char __user *buf, size_t count,
            loff_t *f_pos)
{
	int ret = 0;
	struct voice_chan *chan = (struct voice_chan *)file->private_data;
	char *fifo_buf;

	if (!voice_registered())
		return -1;

	if (chan->kmode)
		return -EBUSY;

	ret = cfifo_request(chan->dec_fifo, (void **)&fifo_buf, count);
	if (ret <= 0)
		return -ENOMEM;

	ret = copy_from_user(fifo_buf, buf, count);
	if (ret != 0)
		return -1;

	cfifo_commit(chan->dec_fifo, count);
	coma_signal();

	return count;
}

static unsigned int
voice_poll(struct file *file, poll_table *event_list)
{
	struct voice_chan *chan = (struct voice_chan *)file->private_data;
	unsigned int mask = 0;

	/* we always allow writing */
	mask |= POLLOUT | POLLWRNORM;

	if(!cfifo_empty(chan->enc_fifo))
		mask |= POLLIN | POLLRDNORM;

	poll_wait(file, &chan->enc_wq, event_list);
	return mask;
}

static struct file_operations voice_fops = {
	.owner   = THIS_MODULE,
	.ioctl   = voice_ioctl,
	.open    = voice_open,
	.release = voice_release,
	.read    = voice_read,
	.write   = voice_write,
	.poll    = voice_poll,
};

void
voice_release_all(void)
{
	int i;
	struct voice_chan *chan;

	if (voice_registered()) {
		for (i = 0; i < CONFIG_CORDLESS_NUM_VOICELINES; i++) {
			chan = &voice_channels[i];

			chan->reader = NULL;
			chan->writer = NULL;

			if (chan->kmode)
				voice_kmode_stop(chan);

			voice_stop_chan(chan);
			voice_free_chan(chan);
			voice_chan_reset(chan);
		}
	}
}

int __init
voice_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&voice_dev, 0, voice_numlines, voice_name);
	if (ret)
		return ret;
	
	cdev_init(&voice_cdev, &voice_fops);
	
	voice_cdev.owner = THIS_MODULE;
	voice_cdev.ops   = &voice_fops;
	
	ret = cdev_add(&voice_cdev, voice_dev, voice_numlines);
	if (ret) {
		printk(KERN_ERR "%s: Error %d adding character device",
		       voice_name, ret);
		goto err_unreg_chrdev_reg;
	}

	voice_l2c = cfifo_alloc(256);
	if (IS_ERR(voice_l2c)) {
		return -ENOMEM;
	}

	voice_c2l = cfifo_alloc(256);
	if (IS_ERR(voice_c2l)) {
		return -ENOMEM;
	}

	ret = coma_register(SUBSYSTEM_VOICE, voice_l2c, voice_c2l,
	                    voice_process_message, voice_setup, voice_remove, voice_poll_fifos);
	if (ret < 0) {
		printk(KERN_ERR "%s: Registration failed: %d\n", voice_name,
		       ret);
		goto err_cfifo_free2;
	}

	printk(KERN_INFO "%s: character device initialized (major=%d)\n",
	       voice_name, MAJOR(voice_dev));

	return 0;

err_cfifo_free2:
	cfifo_free(voice_l2c);
	cfifo_free(voice_c2l);

err_unreg_chrdev_reg:
	unregister_chrdev_region(voice_dev, voice_numlines);


	return ret;
}

void __exit
voice_exit(void)
{
	cdev_del(&voice_cdev);
	unregister_chrdev_region(voice_dev, voice_numlines);
	voice_release_all();
}
module_init(voice_init);
module_exit(voice_exit);

