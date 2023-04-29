/*************************************************************************
    > File Name: WebSocketChannelImpl.h
    > Author: gnblao
    > Mail: gnblao
    > Created Time: 2022年04月10日 星期日 17时16分19秒
 ************************************************************************/

#ifndef _FACTORY_WEBSOCKETCHANNELIMPL_H_
#define _FACTORY_WEBSOCKETCHANNELIMPL_H_

#include "HttpMessage.h"
#include "HttpUtil.h"
#include "ProtocolMessage.h"
#include "WFGlobal.h"
#include "WebSocketMessage.h"
#include "WFChannel.h"
#include "WFChannelMsg.h"
#include "Workflow.h"
#include <algorithm>
#include <atomic>
#include <ctime>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <type_traits>
#include <utility>

#define WS_HTTP_SEC_KEY_K "Sec-WebSocket-Key"
#define WS_HTTP_SEC_KEY_V "dGhlIHNhbXBsZSBub25jZQ=="
#define WS_HTTP_SEC_PROTOCOL_K "Sec-WebSocket-Protocol"
#define WS_HTTP_SEC_VERSION_K "Sec-WebSocket-Version"
#define WS_GUID_RFC4122 "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/**********WebSocket task impl**********/

using WSHearderReq = WFChannelMsg<protocol::HttpRequest>;
using WSHearderRsp = WFChannelMsg<protocol::HttpResponse>;

using protocolMsg = protocol::WebSocketFrame;
using WSFrame = WFChannelMsg<protocolMsg>;
using WebSocketChannelClientBase = WFChannelClient<protocolMsg>;
using WebSocketChannelServerBase = WFChannelServer<protocolMsg>;

class WebSocketChannel {
public:
    void set_sec_protocol(const std::string &protocol) { this->sec_protocol = protocol; }
    void set_sec_version(const std::string &version) { this->sec_version = version; }

    void set_process_binary_fn(std::function<void(WebSocketChannel*, protocolMsg *in)> fn) {
        this->process_binary_fn = fn; 
    }
    void set_process_text_fn(std::function<void(WebSocketChannel*, protocolMsg *in)> fn) {
        this->process_text_fn = fn; 
    }
    
    const std::string get_sec_protocol() const { return this->sec_protocol; }
    const std::string get_sec_version() const { return this->sec_version; }

    void set_ping_interval(int millisecond) { this->ping_interval = millisecond; }
    void set_auto_gen_mkey(bool b) { this->auto_gen_mkey = b; }

    unsigned int gen_masking_key() {
        if (this->auto_gen_mkey == false)
            return 0;

        return this->gen();
    }
   
    // client
    //virtual int send_header_req(WFChannel *) { return 0; }
    virtual int send_header_req() { return 0; }
    virtual int process_header_rsp(protocol::HttpResponse *message) { return 0; }

    virtual int send_ping();
    // virtual int send_pong(); /* It's not possible to actively send pong */
    virtual int send_close(short status_code);
    virtual int send_text(const char *data, size_t size);
    virtual int send_binary(const char *data, size_t size);
    
    virtual int send_frame(const char *data, int size, int frame_size, 
            enum ws_opcode opcode, std::function<void()> cb = nullptr);
    virtual int __send_frame(const char *data, int size, enum ws_opcode opcode, bool fin, 
            std::function<void(WFChannelMsg<protocolMsg> *)> cb = nullptr, protocolMsg *in = nullptr);

    virtual int process_ping(protocolMsg *msg);
    virtual int process_pong(protocolMsg *msg) { return 0; }
    virtual int process_close(protocolMsg *msg);

    virtual int process_text(protocolMsg *msg) { 
        if (this->process_text_fn)
            this->process_text_fn(this, msg);
        return 0; 
    }
    virtual int process_binary(protocolMsg *msg) { 
        if (this->process_binary_fn)
            this->process_binary_fn(this, msg);
        return 0; 
    }

    // server
    virtual int process_header_req(protocol::HttpRequest *msg) { return 0; }

    void create_ping_timer() {
        std::unique_lock<std::recursive_mutex> lck(this->channel->write_mutex);
        if (!open())
            return;

        if (this->channel->incref() > 0) {
            lck.unlock();
            if (this->ping_interval > 0) {
                auto timer = WFTaskFactory::create_timer_task(
                        this->ping_interval * 1000,
                        std::bind(&WebSocketChannel::timer_callback, this,
                            std::placeholders::_1),
                        [this] (unsigned long long id) { this->ping_timerid=id;}
                        );

                this->channel->set_delete_cb(
                        std::bind(&WebSocketChannel::delete_callback, this));

                timer->start();
            } else {
                this->channel->decref();
            }
        } else {
            //std::cout << "This shouldn't happen, and if it does it's a bug!!!!"
            //          << std::endl;
            //this->channel->decref(1);
        }
    }

    virtual bool open() {
        if (this->handshake_status != WS_HANDSHAKE_OPEN)
            return false;

        return this->channel->is_open();
    }

 protected:
    void update_lasttime() { clock_gettime(CLOCK_MONOTONIC, &this->last_time); }
private:
    void delete_callback() {
        if (this->ping_timerid) {
            WFGlobal::get_scheduler()->unsleep(this->ping_timerid);
        }
    }
    
    void timer_callback(WFTimerTask *timer) {
        this->ping_timerid = 0;
        this->channel->set_delete_cb(nullptr);
        
        if (timer->get_state() == SS_STATE_COMPLETE) {
            if (this->channel->is_open() && this->ping_interval < update_interval())
                this->send_ping();

            if (this->ping_interval > 0 && this->channel->is_open())
                this->create_ping_timer();
        }
        
        this->channel->decref();
    }

    int update_interval() {
        struct timespec cur_time;
        int time_used;

        clock_gettime(CLOCK_MONOTONIC, &cur_time);

        time_used = 1000 * (cur_time.tv_sec - this->last_time.tv_sec) +
                    (cur_time.tv_nsec - this->last_time.tv_nsec) / 1000000;

        return time_used;
    }

public:
    explicit WebSocketChannel(WFChannel *channel) {
        assert(channel);
        this->channel = channel;
        this->handshake_status = WS_HANDSHAKE_UNDEFINED;
    }

    virtual ~WebSocketChannel() {}

protected:
    struct timespec last_time;
    int ping_interval = 5*1000; /*millisecond*/
    size_t size_limit;
    bool auto_gen_mkey; // random Masking-Key
    std::mt19937 gen;
    std::string sec_protocol{"chat"}; // Sec-WebSocket-Protocol
    std::string sec_version{"13"};    // Sec-WebSocket-Version

    /*uplevel api*/
    std::function<void(WebSocketChannel*, protocolMsg *in)> process_text_fn;
    std::function<void(WebSocketChannel*, protocolMsg *in)> process_binary_fn;
    
    enum {
        WS_HANDSHAKE_UNDEFINED = -1,
        WS_HANDSHAKE_OPEN,
        WS_HANDSHAKE_CLOSING,
        WS_HANDSHAKE_CLOSED,
    };
    int handshake_status;

private:
    WFChannel *channel;
    unsigned long long ping_timerid;
};

class WebSocketChannelClient : public WebSocketChannelClientBase, public WebSocketChannel {
public:
    virtual int send_header_req();

    virtual int process_header_rsp(protocol::HttpResponse *message) { return 0;};
    virtual int process_msg(MSG *message) {
        if (message->get_seq() > 0) {
            protocolMsg *msg = (protocolMsg *)message;

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
            this->process_header_rsp((protocol::HttpResponse *)message);
            this->handshake_status = WS_HANDSHAKE_OPEN;
            if (this->ping_interval > 0)
                this->create_ping_timer();
        }

        this->update_lasttime();
        return 0;
    }

    const ParsedURI *get_uri() const { return &this->uri_; }

public:
    virtual MsgSession *new_msg_session() {
        MsgSession *session = nullptr;
        if (this->get_msg_seq() == 0)
            // session = new WSHearderRsp(this, nullptr);
            session = new WSHearderRsp(this);
        else {
            // session = new WSFrame(this, nullptr);
            session = new WSFrame(this);
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
    WebSocketChannelClient(channel_callback_t cb = nullptr)
        : WebSocketChannelClientBase(0,std::move(cb)), WebSocketChannel(this) {
        this->auto_gen_mkey = true;

        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);

        this->set_prepare_once(
            std::bind(&WebSocketChannelClient::send_header_req, this));
    }

    virtual ~WebSocketChannelClient(){}
};

class WebSocketChannelServer : public WebSocketChannelServerBase, public WebSocketChannel {
public:
    virtual int process_header_req(protocol::HttpRequest *req);

    virtual int process_msg(MSG *message) {
        if (message->get_seq() > 0) {
            protocolMsg *msg = (protocolMsg *)message;

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
            this->process_header_req((protocol::HttpRequest *)message);
            this->handshake_status = WS_HANDSHAKE_OPEN;
            if (this->ping_interval > 0)
                this->create_ping_timer();
        }

        this->update_lasttime();
        return 0;
    }

public:
    virtual MsgSession *new_msg_session() {
        MsgSession *session = nullptr;
        if (this->get_msg_seq() == 0)
            // session = new WSHearderReq(this, nullptr);
            session = new WSHearderReq(this);
        else {
            // session = new WSFrame(this, nullptr);
            session = new WSFrame(this);
        }

        return session;
    }

public:
    explicit WebSocketChannelServer(CommService *service, CommScheduler *scheduler)
        : WebSocketChannelServerBase(scheduler, service), WebSocketChannel(this) {
        this->auto_gen_mkey = auto_gen_mkey;

        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);

        this->ping_interval = 5 * 1000;
    }

    virtual ~WebSocketChannelServer(){}
};

#endif // __FACTORY_WEBSOCKETCHANNELIMPL_H_
