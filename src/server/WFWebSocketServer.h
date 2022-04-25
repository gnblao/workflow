/*************************************************************************
    > File Name: WFWebSocketServer.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2022年04月23日 星期六 11时58分03秒
 ************************************************************************/

#ifndef _SRC_SERVER_WFWEBSOCKETSERVER_H_
#define _SRC_SERVER_WFWEBSOCKETSERVER_H_

#include <utility>
#include "HttpMessage.h"
#include "ProtocolMessage.h"
#include "WFServer.h"
#include "WFGlobal.h"
#include "WFTaskFactory.h"
#include "WebSocketChannelImpl.h"

static constexpr struct WFServerParams WS_SERVER_PARAMS_DEFAULT =
{
	.max_connections		=	2000,
	.peer_response_timeout	=	10 * 1000,
	.receive_timeout		=	-1,
	.keep_alive_timeout		=	60 * 1000,
	.request_size_limit		=	(size_t)-1,
	.ssl_accept_timeout		=	10 * 1000,
};



class WFWebSocketServer :public WFServer<protocol::ProtocolMessage,
							  protocol::ProtocolMessage> 
{
using MSG = protocol::ProtocolMessage;
public:
    inline CommSession *new_session(long long seq, CommConnection *conn)
    {
        WebSocketChannelServer *channel = new WebSocketChannelServer(this, WFGlobal::get_scheduler());

        channel->set_keep_alive(this->params.keep_alive_timeout);
        channel->set_receive_timeout(this->params.receive_timeout);
        channel->get_req()->set_size_limit(this->params.request_size_limit);
        channel->set_sec_protocol("");
        channel->set_auto_gen_mkey(true);
        channel->set_ping_interval(10*1000*1000);
        
        return channel;
    }
    
    virtual bool is_channel() {return true;}

    WFWebSocketServer() : 
        WFServer<MSG, MSG>(&WS_SERVER_PARAMS_DEFAULT, nullptr)
    {
    }


};

#endif  // _SRC_SERVER_WFWEBSOCKETSERVER_H_
