/*************************************************************************
    > File Name: WFStreamServer.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年04月20日 星期四 16时59分34秒
 ************************************************************************/

#ifndef _SRC_SERVER_WFSTREAMSERVER_H_
#define _SRC_SERVER_WFSTREAMSERVER_H_

#include "WFServer.h"
#include "WFGlobal.h"
#include "ProtocolMessage.h"
#include "StreamMessage.h"
#include "WFTemplateChannel.h"

using StreamChannelServer = WFTemplateChannelServer<protocol::StreamMessage>;

static constexpr struct WFServerParams STREAM_SERVER_PARAMS_DEFAULT =
{
	.max_connections		=	2000,
	.peer_response_timeout	=	10 * 1000,
	.receive_timeout		=	-1,
	.keep_alive_timeout		=	60 * 1000,
	.request_size_limit		=	(size_t)-1,
	.ssl_accept_timeout		=	10 * 1000,
};

class WFStreamServer : public WFServer<protocol::ProtocolMessage,
							  protocol::ProtocolMessage> 
{
using MSG = protocol::ProtocolMessage;
public:
    inline CommSession *new_session(long long seq, CommConnection *conn)
    {
        StreamChannelServer *channel = new StreamChannelServer(this, WFGlobal::get_scheduler());

        channel->set_keep_alive(this->params.keep_alive_timeout);
        channel->set_receive_timeout(this->params.receive_timeout);
        channel->set_send_timeout(-1);
        channel->get_req()->set_size_limit(this->params.request_size_limit);
   
        channel->set_process_msg_fn(this->process_msg_fn);

        return channel;
    }
    
    WFStreamServer(int ping_interval = 10*1000) : 
        WFServer<MSG, MSG>(&STREAM_SERVER_PARAMS_DEFAULT, nullptr)
    {
        this->ping_interval = ping_interval;
    }

public: 
    void set_process_msg_fn(std::function<int(WFChannel*, protocol::StreamMessage *in)> fn) {
        this->process_msg_fn = fn; 
    }

    void set_ping_interval(int millisecond) {this->ping_interval = millisecond;}
protected:
    virtual bool is_channel() {return true;}

private:
    int ping_interval;          // millisecond
    
    std::function<int(WFChannel*, protocol::StreamMessage *in)> process_msg_fn;
};



#endif  // _SRC_SERVER_WFSTREAMSERVER_H_
