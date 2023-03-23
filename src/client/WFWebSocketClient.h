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
        URIParser::parse(uri, this->uri_);
        this->client_->init(this->uri_);
        this->client_->set_keep_alive(-1);
        this->client_->start();
    }

    virtual ~WFWebSocketClient()
    {
        if (this->client_)
            this->client_->shutdown();

        if (this->wg_)
        {
            this->wg_->wait();
            delete this->wg_;
        }
    }

    bool send_text(const char *data, size_t size)
    {
        if (!this->open())
            return false;

        return !this->client_->send_text(data, size);
    }

    void set_auto_gen_mkey(bool b) {
        this->client_->set_auto_gen_mkey(b);
    }
    
    void set_ping_interval(int millisecond) {
        this->client_->set_ping_interval(millisecond);
    }
    
    void set_process_binary_fn(std::function<void(WebSocketChannel*, protocol::WebSocketFrame *in)> fn) {
        this->client_->set_process_binary_fn(fn); 
    }
    
    void set_process_text_fn(std::function<void(WebSocketChannel*, protocol::WebSocketFrame *in)> fn) {
        this->client_->set_process_text_fn(fn); 
    }

protected:
    void channel_done_callback(WFChannel::BaseTask *)
    {
        std::unique_lock<std::mutex> lck(this->mutex_);
        this->client_ = nullptr;
        this->wg_->done();

        auto wg   = this->wg_;
        this->wg_ = nullptr;
        delete wg;
    }

    bool open()
    {
        if (!this->client_ && !this->retry_)
            return false;

        if (!this->client_)
            this->reset();

        std::unique_lock<std::mutex> lck(this->mutex_);
        if (this->client_)
            return this->client_->open();

        return false;
    }

private:
    bool reset()
    {
        std::unique_lock<std::mutex> lck(this->mutex_);
        this->client_ = new WebSocketChannelClient(
            std::bind(&WFWebSocketClient::channel_done_callback, this, std::placeholders::_1));
        this->wg_ = new WFFacilities::WaitGroup(1);
        this->client_->init(this->uri_);
        this->client_->set_keep_alive(-1);
        this->client_->start();
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
