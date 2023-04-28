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
#include "workflow/WebSocketMessage.h"
#include "workflow/WFWebSocketClient.h"

#include <cstring>
#include <ctime>
#include <iostream>
#include <ostream>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "unistd.h"


void process_text(WebSocketChannel *ws, protocol::WebSocketFrame *in) {
    std::cout << std::string((char *)in->get_parser()->payload_data,
                             in->get_parser()->payload_length)
              << std::endl;
}


int main(int argc, char *argv[])
{
	if (argc != 2)
    {
    	
        fprintf(stderr, "USAGE: %s <url>\n"
                        " url format: ws://host:ip\n"
                        "             wss://host:ip\n", argv[0]);
        return 0;
    }
	
    
    WFWebSocketClient client(argv[1]);
    client.set_auto_gen_mkey(false);
    client.set_process_text_fn(process_text);
    
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
        std::cout << "please enter your context:" ;
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
        if (!client.send_text(s.c_str(), s.length()))
            break;
    }

    //sleep(10);
	return 0;
}
