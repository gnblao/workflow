/*************************************************************************
    > File Name: WFWebSocketServer.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2022年04月23日 星期六 11时58分03秒
 ************************************************************************/

#ifndef _SRC_SERVER_WFWEBSOCKETSERVER_H_
#define _SRC_SERVER_WFWEBSOCKETSERVER_H_

#include "WFServer.h"
#include "WFGlobal.h"
#include "ProtocolMessage.h"
#include "WebSocketChannelImpl.h"
#include "WebSocketMessage.h"

static constexpr struct WFServerParams WS_SERVER_PARAMS_DEFAULT =
{
	.max_connections		=	2000,
	.peer_response_timeout	=	-1,
	.receive_timeout		=	-1,
	.keep_alive_timeout		=	60 * 1000,
	.request_size_limit		=	(size_t)-1,
	.ssl_accept_timeout		=	10 * 1000,
};

class WFWebSocketServer : public WFServer<protocol::ProtocolMessage,
							  protocol::ProtocolMessage> 
{
using MSG = protocol::ProtocolMessage;
using protocolMsg = protocol::WebSocketFrame;

public:
    inline CommSession *new_session(long long seq, CommConnection *conn)
    {
        WebSocketChannelServer *channel = new WebSocketChannelServer(this, WFGlobal::get_scheduler());

        channel->set_keep_alive(this->params.keep_alive_timeout);
        channel->set_receive_timeout(this->params.receive_timeout);
        channel->set_send_timeout(this->params.peer_response_timeout);
        
        channel->get_req()->set_size_limit(this->params.request_size_limit);

        channel->set_sec_version(this->sec_version);
        channel->set_sec_protocol(this->sec_protocol);
        channel->set_auto_gen_mkey(this->auto_gen_mkey);
        channel->set_ping_interval(this->ping_interval);
        
        channel->set_process_text_fn(this->process_text_fn);
        channel->set_process_binary_fn(this->process_binary_fn);

        return channel;
    }
    
    WFWebSocketServer(int ping_interval=10*1000) : 
        WFServer<MSG, MSG>(&WS_SERVER_PARAMS_DEFAULT, nullptr)
    {
        this->ping_interval = ping_interval;
        this->auto_gen_mkey = true;
//        this->sec_version = "13";
//        this->sec_protocol = "chat";
    }

public: 
    void set_keep_alive_timeout(int millisecond) {
        this->params.keep_alive_timeout = millisecond;
    }
    void set_receive_timeout(int millisecond) {
        this->params.receive_timeout = millisecond;
    }
    void set_send_timeout(int millisecond) {
        this->params.peer_response_timeout = millisecond;
    }

public: 
    void set_auto_gen_mkey(bool b) {
        this->auto_gen_mkey = b;
    }
 
    void set_ping_interval(int millisecond) {
        this->ping_interval = millisecond;
    }

	void set_sec_protocol(const std::string &protocol) { 
        this->sec_protocol = protocol;
    }
	
    void set_sec_version(const std::string &version) {
        this->sec_version = version;
    }
    
    void set_process_binary_fn(std::function<void(WebSocketChannel*, protocolMsg *in)> fn) {
        this->process_binary_fn = fn; 
    }
    void set_process_text_fn(std::function<void(WebSocketChannel*, protocolMsg *in)> fn) {
        this->process_text_fn = fn; 
    }

protected:
    virtual bool is_channel() {return true;}

private:
    int ping_interval;          // millisecond
	bool auto_gen_mkey;         // random Masking-Key
	std::string sec_protocol;   // Sec-WebSocket-Protocol
	std::string sec_version;    // Sec-WebSocket-Version
    
    std::function<void(WebSocketChannel*, protocolMsg *in)> process_text_fn;
    std::function<void(WebSocketChannel*, protocolMsg *in)> process_binary_fn;
};

#endif  // _SRC_SERVER_WFWEBSOCKETSERVER_H_
