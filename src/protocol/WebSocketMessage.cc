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

#include <cstddef>
#include <cstring>
#include <endian.h>
#include <iostream>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include "WebSocketMessage.h"
#include "websocket_parser.h"
#include "mysql_byteorder.h"

namespace protocol
{
WebSocketFrame::WebSocketFrame(WebSocketFrame&& msg) :
	ProtocolMessage(std::move(msg))
{
	this->parser = msg.parser;
	msg.parser = NULL;
}

WebSocketFrame& WebSocketFrame::operator = (WebSocketFrame&& msg)
{
	if (&msg != this)
	{
		*(ProtocolMessage *)this = std::move(msg);
		if (this->parser)
		{
			websocket_parser_deinit(this->parser);
			delete this->parser;
		}

		this->parser = msg.parser;
		msg.parser = NULL;
	}

	return *this;
}

int WebSocketFrame::append(const void *buf, size_t *size)
{
	int ret = websocket_parser_append_message(buf, size, this->parser);
    
    /*
    std::cout << "--------------------append ret:" << ret  
        << " nreceived:"<< this->parser->nreceived 
        << " nleft:"<< this->parser->nleft 
        << "\n";
    */
    
    if (this->parser->payload_length > this->size_limit)
	{
		this->parser->status_code = WSStatusCodeTooLarge;
		return 1; // don`t need websocket_parser_parse()
	}

	if (ret == 1)
		websocket_parser_parse(this->parser);

	return ret;
}

int WebSocketFrame::encode(struct iovec vectors[], int max)
{
	unsigned char *p = this->parser->header_buf;
	int cnt = 0;

    *p = (this->parser->fin << 7) | this->parser->opcode;
	p++;

	if (this->parser->payload_length < 126)
	{
        *p = this->parser->payload_length;
        p++;
	}
	else if (this->parser->payload_length < 65536)
	{
		*p = 126;
		p++;

		int2store(p, htons(this->parser->payload_length));
		p += 2;
	}
	else
	{
		*p = 127;
		p++;
		
        int8store(p, htobe64(this->parser->payload_length));
		p += 8;
	}

	vectors[cnt].iov_base = this->parser->header_buf;
	vectors[cnt].iov_len = p - this->parser->header_buf;
	cnt++;

	p = this->parser->header_buf + 1;
	*p = *p | (this->parser->mask << 7);

	if (this->parser->mask)
	{
		vectors[cnt].iov_base = this->parser->masking_key;
		vectors[cnt].iov_len = WS_MASKING_KEY_LENGTH;
		cnt++;
	}

	if (this->parser->payload_length)
	{
		websocket_parser_mask_data(this->parser);
		vectors[cnt].iov_base = this->parser->payload_data;
		vectors[cnt].iov_len = this->parser->payload_length;
		cnt++;
	}

	return cnt;
}

bool WebSocketFrame::set_opcode(int opcode)
{
	if (opcode < WebSocketFrameContinuation || opcode > WebSocketFramePong)
		return false;

	this->parser->opcode = opcode;
	return true;
}

int WebSocketFrame::get_opcode() const
{
	return this->parser->opcode;
}

void WebSocketFrame::set_masking_key(uint32_t masking_key)
{
	this->parser->mask = 1;
	memcpy(this->parser->masking_key, &masking_key, WS_MASKING_KEY_LENGTH);
}

uint32_t WebSocketFrame::get_masking_key() const
{
	if (!this->parser->mask)
		return atoi((char *)this->parser->masking_key);

	return 0;
}

bool WebSocketFrame::set_frame(const char *buf, size_t size,
                               enum ws_opcode opcode /* = WS_OPCODE_BINARY */,
                               bool fin /* = true */) {
    bool ret = true;

	this->parser->opcode = opcode;
	this->parser->fin = fin ? 1 :0;

	if (this->parser->payload_length && this->parser->payload_data)
	{
		free(this->parser->payload_data);
	}

	this->parser->payload_data = (char *)malloc(size);
    if (!this->parser->payload_data)
        return false;

	memcpy(this->parser->payload_data, buf, size);
	this->parser->payload_length = size;

	return ret;
}

int WebSocketFrame::get_status_code() {
    return this->parser->status_code;
}

bool WebSocketFrame::set_status_code_data(short status_code, const char *data) {
    return this->set_status_code_data(status_code, data, strlen(data));
}

bool WebSocketFrame::set_status_code_data(short status_code, const char *data, size_t size)
{
	unsigned char *p;

	if (this->parser->opcode != WebSocketFrameConnectionClose)
        return false;

	if  (this->parser->payload_length && this->parser->payload_data)
	{
		free(this->parser->payload_data);
	}

	this->parser->status_code = status_code;
	
	p = (unsigned char *)malloc(size + 2);
	if(!p)
        return false;

    this->parser->payload_length = size + 2;
	this->parser->payload_data = p;
    int2store(p, status_code);
    p += 2;

	memcpy(p, data, size);

	return true;
}

} // end namespace protocol

