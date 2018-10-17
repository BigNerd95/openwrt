/*
 *  drivers/cordless/cmsg-voice.h - cordless messages / voice (RTP)
 *
 *  Cordless messages are used to exchange commands and data between Linux and
 *  cordless over cfifos. They consist of a type specifier, an optional
 *  parameter block and optional additional payload.
 *
 *  Copyright (C) 2009 DSPG Technologies GmbH
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

#ifndef CMSG_VOICE_H
#define CMSG_VOICE_H

#include "cmsg.h"

enum cmsg_voice_types {
	CMSG_VOICE_REQUEST_GET_CHAN = 0,
	CMSG_VOICE_REPLY_GET_CHAN,
	CMSG_VOICE_REQUEST_SET_CHAN_FIFOS,
	CMSG_VOICE_REPLY_SET_CHAN_FIFOS,
	CMSG_VOICE_REQUEST_START_CHAN,
	CMSG_VOICE_REPLY_START_CHAN,
	CMSG_VOICE_REQUEST_STOP_CHAN,
	CMSG_VOICE_REPLY_STOP_CHAN,
	CMSG_VOICE_REQUEST_FREE_CHAN,
	CMSG_VOICE_REPLY_FREE_CHAN,
	CMSG_VOICE_REQUEST_SEND_DTMF,
	CMSG_VOICE_REPLY_SEND_DTMF,
	CMSG_VOICE_RECEIVE_DTMF,
	CMSG_VOICE_REQUEST_START_RTCP,
	CMSG_VOICE_REPLY_START_RTCP,
	CMSG_VOICE_REQUEST_STOP_RTCP,
	CMSG_VOICE_REPLY_STOP_RTCP,
	CMSG_VOICE_REQUEST_REPORT_RTCP,
	CMSG_VOICE_REPLY_REPORT_RTCP,
	CMSG_VOICE_REMOTE_START,

};

struct rtp_cng_opts {
	char level_rx;
	int  mode_rx;
	char level_tx;
	int  mode_tx;
	int  max_sid_update;
	int  vad_detect_level;
	int  vad_hangover;
};

union cmsg_voice_params {

	/* CMSG_REQUEST_GET_CHAN */
	struct  request_get_chan {
		int id;
	} request_get_chan;

	/* CMSG_REPLY_GET_CHAN */
	struct  reply_get_chan {
		int result;
	} reply_get_chan;

	/* CMSG_REQUEST_SET_CHAN_FIFOS */
	struct  request_set_chan_fifos {
		int id;
		struct cfifo *enc;
		struct cfifo *dec;
	} request_set_chan_fifos;

	/* CMSG_REPLY_SET_CHAN_FIFOS */
	struct  reply_set_chan_fifos {
		int result;
	} reply_set_chan_fifos;

	/* CMSG_REQUEST_START_CHAN */
	struct  request_start_chan {
		int id;
		char rx_codec;
		char rx_codec_event;
		char tx_codec;
		char tx_codec_event;
		int duration;
		int opts;
		struct rtp_cng_opts cng;
	} request_start_chan;

	/* CMSG_REPLY_START_CHAN */
	struct  reply_start_chan {
		int result;
	} reply_start_chan;

	/* CMSG_REQUEST_STOP_CHAN */
	struct  request_stop_chan {
		int id;
	} request_stop_chan;

	/* CMSG_REPLY_STOP_CHAN */
	struct  reply_stop_chan {
		int result;
	} reply_stop_chan;

	/* CMSG_REQUEST_FREE_CHAN */
	struct  request_free_chan {
		int id;
	} request_free_chan;

	/* CMSG_REPLY_FREE_CHAN */
	struct  reply_free_chan {
		int result;
	} reply_free_chan;

	/* CMSG_REQUEST_SEND_DTMF */
	struct request_send_dtmf {
		int id;
		int status;
		int event;
		int volume;
		int duration;
		int EvtDuration;
		int MaxEvtDuration;
	} request_send_dtmf;

	/* CMSG_REPLY_SEND_DTMF */
	struct reply_send_dtmf {
		int result;
	} reply_send_dtmf;

	/* CMSG_RECEIVE_DTMF */
	struct receive_dtmf {
		int id;
		int status;
		int event;
		int volume;
		int duration;
	} receive_dtmf;
	
	/* CMSG_REQUEST_START_RTCP */
	struct request_start_rtcp {
		int id;
		int rtcp_interval;
	} request_start_rtcp;

	/* CMSG_REPLY_START_RTCP */
	struct reply_start_rtcp {
		int result;
	} reply_start_rtcp;

	/* CMSG_REQUEST_STOP_RTCP */
	struct request_stop_rtcp {
		int id;
	} request_stop_rtcp;

	/* CMSG_REPLY_STOP_RTCP */
	struct reply_stop_rtcp {
		int result;
	} reply_stop_rtcp;

	/* CMSG_REQUEST_REPORT_RTCP */
	struct request_report_rtcp {
		int id;
	} request_report_rtcp;

	/* CMSG_REPLY_REPORT_RTCP */
	struct reply_report_rtcp {
		int result;
	} reply_report_rtcp;
};

#endif /* CMSG_VOICE_H */
