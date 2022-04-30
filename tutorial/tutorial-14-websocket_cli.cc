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

#include "workflow/WFGlobal.h"
#include "workflow/WFFacilities.h"
#include "workflow/WebSocketChannelImpl.h"
#include "workflow/WebSocketMessage.h"

#include <cstring>
#include <iostream>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "unistd.h"

using namespace protocol;

void process(WSFrame *task)
{
	const char *data;
	size_t size;

	if (task->get_msg()->get_opcode() == WebSocketFrameText)
	{
		task->get_msg()->get_data(&data, &size);
		fprintf(stderr, "get text message: [%.*s]\n", (int)size, data);
	}
	else
	{
		fprintf(stderr, "opcode=%d\n", task->get_msg()->get_opcode());
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE: %s <url>\n	url format: ws://host:ip\n", argv[0]);
		return 0;
	}
    
    ParsedURI uri;
	if (URIParser::parse(argv[1], uri) < 0)
		return -1;

	auto client = new WebSocketChannelClient(nullptr, WFGlobal::get_scheduler());
	client->set_uri(uri);
    client->set_keep_alive(-1);

    client->start();

    std::string s;
    while (1) {
        std::cout << std::endl;
        std::cout << "please enter your context:";
        std::cin >> s;
    
        client->send_text(s.c_str(), s.length());
    }
	return 0;
}
