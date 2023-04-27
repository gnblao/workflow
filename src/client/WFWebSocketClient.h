/*************************************************************************
    > File Name: WFWebsocketClient.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2022年05月07日 星期六 16时15分11秒
 ************************************************************************/
#include "WFChannel.h"
#include "WFFacilities.h"
#include "WebSocketChannelImpl.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#ifndef _SRC_CLIENT_WFWEBSOCKETCLIENT_H_
#define _SRC_CLIENT_WFWEBSOCKETCLIENT_H_

class WFWebSocketClient
{
public:
    explicit WFWebSocketClient(std::string uri)
        : retry_(false), wg_(new WFFacilities::WaitGroup(1)),
          client_(new WebSocketChannelClient(
              std::bind(&WFWebSocketClient::channel_done_callback, this, std::placeholders::_1)))
    {
        assert(client_);
        URIParser::parse(uri, this->uri_);
        this->client_->init(this->uri_);
        this->client_->set_keep_alive(-1);
        this->client_->set_receive_timeout(-1);
        this->client_->set_send_timeout(-1);
        this->client_->start();
    }

    virtual ~WFWebSocketClient()
    {
        {
            std::unique_lock<std::mutex> lck(this->mutex_);
            if (this->client_)
                this->client_->shutdown();
        }
        
        if (this->wg_)
        {
            this->wg_->wait();
            delete this->wg_;
        }

        if (this->client_)
            delete this->client_;
    }

    int send_text(const char *data, size_t size)
    {
        std::unique_lock<std::mutex> lck(this->mutex_);
        if(this->client_)
            return this->client_->send_text(data, size);
        return false;
    }

    void set_auto_gen_mkey(bool b) {
        std::unique_lock<std::mutex> lck(this->mutex_);
        if(this->client_)
            this->client_->set_auto_gen_mkey(b);
    }
    
    void set_ping_interval(int millisecond) {
        std::unique_lock<std::mutex> lck(this->mutex_);
        if(this->client_)
            this->client_->set_ping_interval(millisecond);
    }
    
    void set_process_binary_fn(std::function<void(WebSocketChannel*, protocol::WebSocketFrame *in)> fn) {
        std::unique_lock<std::mutex> lck(this->mutex_);
        if(this->client_)
            this->client_->set_process_binary_fn(fn); 
    }
    
    void set_process_text_fn(std::function<void(WebSocketChannel*, protocol::WebSocketFrame *in)> fn) {
        std::unique_lock<std::mutex> lck(this->mutex_);
        if(this->client_)
            this->client_->set_process_text_fn(fn); 
    }

protected:
    void channel_done_callback(WFChannel::Channel*)
    {
        std::unique_lock<std::mutex> lck(this->mutex_);
        this->client_ = nullptr;
        this->wg_->done();
    }

    bool open()
    {
        //if (!this->client_ && !this->retry_)
        //    return false;

        //if (!this->client_)
        //    this->reset();

        std::unique_lock<std::mutex> lck(this->mutex_);
        if (this->client_)
            return this->client_->open();

        return false;
    }

private:
    bool reset()
    {
        (void)this->retry_;
        //std::unique_lock<std::mutex> lck(this->mutex_);
        //this->client_ = new WebSocketChannelClient(
        //    std::bind(&WFWebSocketClient::channel_done_callback, this, std::placeholders::_1));
        //this->wg_ = new WFFacilities::WaitGroup(1);
        //this->client_->init(this->uri_);
        //this->client_->set_keep_alive(-1);
        //this->client_->start();
        return true;
    }

private:
    bool                     retry_;
    ParsedURI                uri_;
    WFFacilities::WaitGroup *wg_;
    WebSocketChannelClient  *client_;
    std::mutex               mutex_;
};

#endif // _SRC_CLIENT_WFWEBSOCKETCLIENT_H_
