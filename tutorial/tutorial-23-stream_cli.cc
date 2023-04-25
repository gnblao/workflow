
#include "workflow/EndpointParams.h"
#include "workflow/StreamMessage.h"
#include "workflow/WFFacilities.h"
#include "workflow/WFGlobal.h"
#include "workflow/WFStreamClient.h"

#include "unistd.h"
#include <cstring>
#include <ctime>
#include <iostream>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using ChannelMsg = WFChannelMsg<protocol::StreamMessage>;
ChannelMsg * frist_msg_fn(WFChannel *channel) {
    size_t len;
    std::string s={"gdhjahgjgasjhfdhasfghasfhsaasgas"};
    
    auto task = new ChannelMsg(channel);
    if (task) {
        auto msg = task->get_msg();

        len = s.length();
        msg->append_fill((void *)s.c_str(), len);

        return task;
    }

    return nullptr;
}


int process_msg(WFChannel *ch, protocol::StreamMessage *in) {
    std::cout << std::string((char *)in->get_parser()->data,
                             in->get_parser()->size)
              << std::endl;

    //auto *channel = static_cast<StreamChannelClient*>(ch);
    //channel->send(buf, size);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {

        fprintf(stderr,
                "USAGE: %s <url>\n"
                " url format: ws://host:ip\n"
                "             wss://host:ip\n",
                argv[0]);
        return 0;
    }

    WFStreamClient client(argv[1]);
    client.set_process_fn(process_msg);
    client.set_frist_msg_fn(frist_msg_fn);

    std::string s125 =
        "111111111111111111111111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111111111";
    std::string s126 =
        "111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111";
    std::string s127 =
        "111111111111111111111111111111111111111111111111111111111111111111111"
        "1111111111111111111111111111111111111111111111111111111111";
    std::string s200 =
        "111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111111111111111";

    std::string s5000;
    for (int i = 0; i < 4000; i++)
        s5000 += s200;
    std::string s;
    while (1) {
        usleep(50000);
        std::cout << "please enter your context:";
        std::cin >> s;
        // std::cout <<s << std::endl;

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
        if (!client.send(s.c_str(), s.length()))
            break;
    }

    // sleep(10);
    return 0;
}
