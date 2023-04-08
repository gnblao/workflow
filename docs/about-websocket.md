# websocket协议  
这是基于channel实现的websocket协议
（ws的特性实现不完善，有想法的小伙伴，可以玩玩～～）


git：
https://github.com/gnblao/workflow/tree/channel

## 关于channel
  [关于channel](https://github.com/gnblao/workflow/blob/channel/docs/about-channel.md)

## 编译和安装
[编译和安装](https://github.com/sogou/workflow#readme)

## 现有websocket的两个dome  
### server：
* /tutorial/tutorial-22-ws_echo_server.cc  
~~~cpp
static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo) { wait_group.done(); }

void process_text(WebSocketChannel *ws, protocol::WebSocketFrame *in) {
    std::cout << "-----data len:" << in->get_parser()->payload_length << std::endl;

    ws->send_frame(
        (char *)in->get_parser()->payload_data, 
        in->get_parser()->payload_length,
        in->get_parser()->payload_length,
        WebSocketFrameText);
	
    //ws->send_text(buf, len);
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

    WFWebSocketServer server;
    server.set_ping_interval(20*1000);
    server.set_process_text_fn(process_text);

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
* /tutorial/tutorial-14-websocket_cli.cc  
~~~cpp
void process_text(WebSocketChannel *ws, protocol::WebSocketFrame *in) {
    std::cout << std::string((char *)in->get_parser()->payload_data,
                             in->get_parser()->payload_length)
              << std::endl;
    
    //ws->send_text(buf, len);
    //ws->send_frame(buf, len, freamsize, WebSocketFrameText)
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
    
    std::string s;
    while (1) {
        usleep(50000);
        std::cout << "please enter your context:";
		        std::cin >> s;
        //std::cout <<s << std::endl;
        
        if (!s.compare("exit"))
            break;
        
        if (!client.send_text(s.c_str(), s.length()))
            break;
    }

    return 0;
}
~~~

## ws
srv：
./ws_echo_server 5679

cli：
./websocket_cli ws://127.0.0.1:5679

## wss
srv：
./ws_echo_server 5679 server.crt server.key
（注：openssl req -new -x509 -keyout server.key -out server.crt -config openssl.cnf）
cli：
./websocket_cli wss://127.0.0.1:5679


欢迎提一些issues和🧱，和有趣的想法～～～
