/*************************************************************************
    > File Name: WebSocketChannelImpl.h
    > Author: gnblao 
    > Mail: gnblao
    > Created Time: 2022年04月10日 星期日 17时16分19秒
 ************************************************************************/

#ifndef _FACTORY_WEBSOCKETCHANNELIMPL_H_
#define _FACTORY_WEBSOCKETCHANNELIMPL_H_

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <random>
#include <functional>
#include <iostream>
#include <type_traits>
#include "Communicator.h"
#include "ProtocolMessage.h"
#include "SubTask.h"
#include "WFTask.h"
#include "WFTaskFactory.h"
#include "WFCondition.h"
#include "WFCondTaskFactory.h"
#include "WFNameService.h"
#include "RouteManager.h"
#include "WFGlobal.h"
#include "EndpointParams.h"
#include "HttpUtil.h"
#include "HttpMessage.h"
#include "WFChannel.h"
#include "WFChannelMsg.h"
#include "WebSocketMessage.h"
#include "Workflow.h"
#include "websocket_parser.h"

#define WS_HTTP_SEC_KEY_K		"Sec-WebSocket-Key"
#define WS_HTTP_SEC_KEY_V		"dGhlIHNhbXBsZSBub25jZQ=="
#define WS_HTTP_SEC_PROTOCOL_K	"Sec-WebSocket-Protocol"
#define WS_HTTP_SEC_VERSION_K	"Sec-WebSocket-Version"
#define WS_GUID_RFC4122 "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/**********WebSocket task impl**********/

using WSHearderReq  = WFChannelMsg<protocol::HttpRequest>;
using WSHearderRsp  = WFChannelMsg<protocol::HttpResponse>;
using WSFrame = WFChannelMsg<protocol::WebSocketFrame>;

using websocket_process_t = std::function<void (WSFrame *)>;
using websocket_callback_t = std::function<void (WSFrame *)>;

class WebSocketTools {
public:
	void set_sec_protocol(const std::string &protocol) { this->sec_protocol = protocol;}
	void set_sec_version(const std::string &version) { this->sec_version = version;}

	const std::string get_sec_protocol() const { return this->sec_protocol;}
	const std::string get_sec_version() const { return this->sec_version;}
    
    void set_ping_interval(int microsecond) {this->ping_interval = microsecond;}
    void set_auto_gen_mkey(bool b) {this->auto_gen_mkey = b;}

	uint32_t gen_masking_key()
	{
			return 0;
		if (this->auto_gen_mkey == false)
			return 0;

		return this->gen();
	}

    //client
    virtual int send_header_req(WFChannel*) {return 0;} 
    virtual int process_header_rsp(protocol::HttpResponse *message) {return 0;}
    
    virtual int send_ping();
    virtual int send_pong();
    virtual int send_close(int status_code, std::string str) ;
    virtual int send_text(const char *data, size_t size) ;
    virtual int send_binary(const char *data, size_t size) ;

    virtual int process_ping(protocol::WebSocketFrame *msg);
    virtual int process_pong(protocol::WebSocketFrame *msg) {return 0;}
    virtual int process_close(protocol::WebSocketFrame *msg);
    virtual int process_text(protocol::WebSocketFrame *msg)  {return 0;}
    virtual int process_binary(protocol::WebSocketFrame *msg)  {return 0;}
    
    //server
    virtual int process_header_req(protocol::HttpRequest *msg) {return 0;}

    void timer_callback(WFTimerTask *timer)  {
        if (!this->channel->stop_flag)
            this->send_ping();

        if (this->ping_interval > 0 && !this->channel->stop_flag)
            this->create_ping_timer();
    
        this->channel->decref();
    }

    void create_ping_timer() {
        auto timer = WFTaskFactory::create_timer_task(this->ping_interval, 
                std::bind(&WebSocketTools::timer_callback, this, std::placeholders::_1));
        
        this->channel->incref();
        timer->start();
    }

public:
    WebSocketTools(WFChannel *channel) {
        assert(channel);
        this->channel = channel;
        this->handshake_status = WS_HANDSHAKE_UNDEFINED;
    }
    
    virtual ~WebSocketTools() {}

protected:
	size_t size_limit;
    int ping_interval;
	bool auto_gen_mkey; // random Masking-Key
	std::random_device rd;
	std::mt19937 gen;
	std::string sec_protocol{"chat"}; // Sec-WebSocket-Protocol
	std::string sec_version{"13"}; // Sec-WebSocket-Version
    
    enum {
        WS_HANDSHAKE_UNDEFINED,
        WS_HANDSHAKE_OPEN,
        WS_HANDSHAKE_CLOSING,
        WS_HANDSHAKE_CLOSED,
    };
    int handshake_status;
private:
    WFChannel *channel;
};

class WebSocketChannelClient : public WFChannelClient , public WebSocketTools
{
public:
    virtual int send_header_req(WFChannel*); 
    
    virtual int process_header_rsp(protocol::HttpResponse *message) {return 0;}
    virtual int process_text(protocol::WebSocketFrame *msg);
    virtual int process_binary(protocol::WebSocketFrame *msg)  {return 0;}

    virtual int process_msg(MSG *message) 
    {
        if (message->get_seq() > 0) {
            protocol::WebSocketFrame *msg = (protocol::WebSocketFrame*) message;

            switch (msg->get_opcode()) {
            case WebSocketFramePing:
                this->process_ping(msg);
                break;
            case WebSocketFramePong:
                this->process_pong(msg);
                break;
            case WebSocketFrameText:
                this->process_text(msg);
                break;
            case WebSocketFrameBinary:
                this->process_binary(msg);
                break;
            case WebSocketFrameConnectionClose:
                this->process_close(msg);
                break;
            default:
                break;
            }
        } else {
            this->process_header_rsp((protocol::HttpResponse*)message);
            this->handshake_status = WS_HANDSHAKE_OPEN;
        }

        return 0;
    }

	const ParsedURI *get_uri() const { return &this->uri_; }

public:
    websocket_process_t websocket_process;

public:
    virtual WFChannelMsgSession *new_channel_msg_session() {
        WFChannelMsgSession *session = nullptr;
        if (this->get_channel_msg_seq() == 0)
            session = new WSHearderRsp(this, nullptr);
        else {
            session = new WSFrame(this, nullptr);
            ((protocol::WebSocketFrame *)session->get_msg())->set_server();
        }

        return session;
    }

protected:
    virtual bool init_success() {
        bool is_ssl = false;

        if (uri_.scheme && strcasecmp(uri_.scheme, "wss") == 0)
            is_ssl = true;
        
        this->set_transport_type(is_ssl ? TT_TCP_SSL : TT_TCP);
        return true;

    }


public:
	WebSocketChannelClient(CommSchedObject *object,
							CommScheduler *scheduler) :
		//WFComplexChannelClient(object, scheduler),
		WFChannelClient(0, nullptr),
        WebSocketTools(this)
	{
		this->auto_gen_mkey = true;
        
        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);

        this->set_prepare(std::bind(&WebSocketChannelClient::send_header_req, 
                    this, std::placeholders::_1));
    }
};


class WebSocketChannelServer : public WFChannelServer ,public WebSocketTools
{
public:
    virtual int process_header_req(protocol::HttpRequest *req); 
    virtual int process_text(protocol::WebSocketFrame *msg);
    virtual int process_binary(protocol::WebSocketFrame *msg)  {return 0;}
    
    virtual int process_msg(MSG *message) 
    {
        if (message->get_seq() > 0) {
            protocol::WebSocketFrame *msg = (protocol::WebSocketFrame*) message;

            switch (msg->get_opcode()) {
            case WebSocketFramePing:
                this->process_ping(msg);
                break;
            case WebSocketFrameText:
                this->process_text(msg);
                break;
            case WebSocketFrameBinary:
                this->process_binary(msg);
                break;
            case WebSocketFramePong:
                this->process_pong(msg);
                break;
            case WebSocketFrameConnectionClose:
                this->process_close(msg);
                break;
            default:
                break;
            }
        } else {
            this->process_header_req((protocol::HttpRequest*)message);
            this->create_ping_timer();
        }

        return 0;
    }
    
public:
    websocket_process_t websocket_process;

public:
    virtual WFChannelMsgSession *new_channel_msg_session() {
        WFChannelMsgSession *session = nullptr;
        if (this->get_channel_msg_seq() == 0)
            session = new WSHearderReq(this, nullptr);
        else {
            session = new WSFrame(this, nullptr);
            if (this->is_server())
                ((protocol::WebSocketFrame *)session->get_msg())->set_server();
        }

        return session;
    }

public:
	WebSocketChannelServer(CommService *service,
							CommScheduler *scheduler):
	    WFChannelServer(scheduler, service),
        WebSocketTools(this)
	{
		this->auto_gen_mkey = auto_gen_mkey;
        
        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);
        
        this->ping_interval = 5 * 1000 *1000;
    }
};



#endif  // __FACTORY_WEBSOCKETCHANNELIMPL_H_
