# stream协议（tcp[tcp+ssl]原始数据流）
这是基于channel实现的stream协议

git：
https://github.com/gnblao/workflow/tree/channel

## 关于channel
  [关于channel](https://github.com/gnblao/workflow/blob/channel/docs/about-channel.md)

## 编译和安装
[编译和安装](https://github.com/sogou/workflow#readme)

## 现有stream的两个dome  
### stream：
* [/tutorial/tutorial-23-stream_srv.cc](/tutorial/tutorial-23-stream_srv.cc)
 
~~~cpp
int process_text(WFChannel *ch, protocol::StreamMessage *in) {
    std::cout << "-----data len:" << in->get_parser()->size << std::endl;

    auto *channel = static_cast<StreamChannelServer*>(ch);
    return channel->send(
        (char *)in->get_parser()->data, in->get_parser()->size);
}


int main(int argc, char *argv[]) {
    unsigned short port;
    char *cert_file;
    char *key_file;
    int ret;

    if (argc != 2 && argc != 4) {
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        fprintf(stderr, "ssl : %s <port> <cert_file> <key_file>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);

    signal(SIGINT, sig_handler);

    WFStreamServer server;
    server.set_ping_interval(20*1000);
    server.set_process_msg_fn(process_text);

    if (argc == 4) {
        cert_file = argv[2];
        key_file = argv[3];

        ret = server.start(port, cert_file, key_file);
    } else {
        ret = server.start(port);
    }

    if (ret == 0) {
        wait_group.wait();
        server.stop();
    } else {
        perror("Cannot start server");
        exit(1);
    }

    return 0;
}
~~~

### client：
* [/tutorial/tutorial-23-stream_cli.cc](/tutorial/tutorial-23-stream_cli.cc)
 
~~~cpp
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


    std::string s;
    while (1) {
        usleep(50000);
        std::cout << "please enter your context:";
        std::cin >> s;
        // std::cout <<s << std::endl;

        if (!s.compare("exit"))
            break;
			
        if (!client.send(s.c_str(), s.length()))
            break;
    }

    return 0;
}
~~~

## tcp
srv：
./stream_srv 5679

cli：
./stream_cli ws://127.0.0.1:5679

## tcp+ssl
srv：
./stream_srv 5679 server.crt server.key
（注：openssl req -new -x509 -keyout server.key -out server.crt -config openssl.cnf）

cli：
./stream_cli wss://127.0.0.1:5679


欢迎提一些issues和🧱，和有趣的想法～～～
