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

#ifndef _WEBSOCKETMESSAGE_H_
#define _WEBSOCKETMESSAGE_H_

#include <cstddef>
#include <string.h>
#include <string>
#include <stdint.h>
#include "ProtocolMessage.h"
#include "websocket_parser.h"

namespace protocol
{

#define WS_HANDSHAKE_TIMEOUT    10 * 1000

class WebSocketFrame : public ProtocolMessage
{
public:
	bool set_opcode(int opcode);
	int get_opcode() const;

	void set_masking_key(uint32_t masking_key);
	uint32_t get_masking_key() const;

	void set_fin(bool fin) {this->parser->fin = fin;};
	bool get_fin() {return this->parser->fin;};
	
    bool set_frame(const char *data, size_t size, enum ws_opcode opcode, bool fin);

    int get_status_code();
    bool set_status_code_data(short status_code, const char *data);
    bool set_status_code_data(short status_code, const char *data, size_t size);

public:
	const websocket_parser_t *get_parser() { return this->parser; }

protected:
	virtual int encode(struct iovec vectors[], int max);
	virtual int append(const void *buf, size_t *size);

public:
	WebSocketFrame() : parser(new websocket_parser_t)
	{
		websocket_parser_init(this->parser);
	}

	virtual ~WebSocketFrame()
	{
		websocket_parser_deinit(this->parser);
		delete this->parser;
	}

	WebSocketFrame(const WebSocketFrame& msg) = delete;
	WebSocketFrame& operator = (const WebSocketFrame& msg) = delete;
	
    WebSocketFrame(WebSocketFrame&& msg);
	WebSocketFrame& operator = (WebSocketFrame&& msg);

private:
	websocket_parser_t *parser;
};

} // end namespace protocol

#endif

