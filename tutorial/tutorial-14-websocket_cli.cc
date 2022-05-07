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

#include "workflow/EndpointParams.h"
#include "workflow/WFGlobal.h"
#include "workflow/WFFacilities.h"
#include "workflow/WebSocketChannelImpl.h"
#include "workflow/WebSocketMessage.h"

#include <cstring>
#include <ctime>
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

	auto client = new WebSocketChannelClient(nullptr, nullptr);
	//client->set_uri(uri);
    client->init(uri);
    client->set_keep_alive(-1);
    
    client->start();

    std::string s125 = "11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    std::string s126 = "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    std::string s127 = "1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    std::string s200 = "11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
   
    std::string s5000;
    for (int i=0; i<4000; i++)
        s5000 += s200;
    std::string s;
    while (1) {
        usleep(50000);
        std::cout << "please enter your context:";
        std::cin >> s;
        //std::cout <<s << std::endl;
        
        if (!s.compare("exit"))
            break;
        
        if (!s.compare("125"))
            s = s125;
        if (!s.compare("126"))
            s = s126;
        if (!s.compare("127"))
            s = s127;
        if (!s.compare("200"))
            s = s200;
        if (!s.compare("5000"))
            s = s5000;
        client->send_text(s.c_str(), s.length());
    }

    client->channel_close();
    sleep(10);
	return 0;
}
