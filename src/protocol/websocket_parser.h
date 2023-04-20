/*
  Copyright (c) 2021 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Li Yingxin (liyingxin@sogou-inc.com)
*/

#ifndef _WEBSOCKET_PARSER_H_
#define _WEBSOCKET_PARSER_H_

// Minimum length of a WebSocket frame header.
#define WS_BASIC_HEADER_LENGTH  2
// Maximum length of a WebSocket header
#define WS_MAX_HEADER_LENGTH  14
#define WS_MASKING_KEY_LENGTH  4

#include <stddef.h>
#include <stdint.h>

enum ws_opcode
{
	WebSocketFrameContinuation		= 0,
	WebSocketFrameText				= 1,
	WebSocketFrameBinary			= 2,
	WebSocketFrameConnectionClose	= 8,
	WebSocketFramePing				= 9,
	WebSocketFramePong				= 10,
};

enum
{
	WSStatusCodeUndefined			= 0,

	WSStatusCodeNormal				= 1000,
	WSStatusCodeGoingAway			= 1001,
	WSStatusCodeProtocolError		= 1002,
	WSStatusCodeUnsupported			= 1003,
	WSStatusCodeReserved			= 1004, // reserved
	WSStatusCodeNoStatus			= 1005, // reserved
	WSStatusCodeAbnomal				= 1006, // reserved
	WSStatusCodeUnsupportedData		= 1007,
	WSStatusCodePolicyViolation		= 1008,
	WSStatusCodeTooLarge			= 1009,
	WSStatusCodeMissExtention		= 1010,
	WSStatusCodeInternalError		= 1011,
//	WSStatusCodeServiceRestart		= 1012,
//	WSStatusCodeTryAgainLater		= 1013,
	WSStatusCodeTLSHandshake		= 1015, // reserved

	WSStatusCodeProtocolMax			= 2999,

	WSStatusCodeIANAMin				= 3000,
	WSStatusCodeIANAMax				= 3999,

	WSStatusCodeUserMin				= 4000,
	WSStatusCodeUserMax				= 4999,
};

enum 
{
    WS_HEADER_DOING = 0,
    WS_HEADER_DONE,
    WS_FRAME_DOING,
    WS_FRAME_DONE,
};

typedef struct __websocket_parser
{
	char fin;
	char mask;
	int opcode;
	unsigned char masking_key[WS_MASKING_KEY_LENGTH];
	unsigned char header_buf[WS_MAX_HEADER_LENGTH];
	int masking_key_offset;
	int status_code;
	int status;
	unsigned long long nreceived;
	unsigned long long nleft;
	unsigned long long payload_length;
	void *payload_data;
} websocket_parser_t;

#ifdef __cplusplus
extern "C"
{
#endif

void websocket_parser_init(websocket_parser_t *parser);
void websocket_parser_deinit(websocket_parser_t *parser);
int websocket_parser_append_message(const void *buf, size_t *n,
									websocket_parser_t *parser);

int websocket_parser_parse(websocket_parser_t *parser);

void websocket_parser_mask_data(websocket_parser_t *parser);

unsigned char *utf8_check(unsigned char *s, size_t len);

#ifdef __cplusplus
}
#endif

#endif

