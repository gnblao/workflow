/*************************************************************************
    > File Name: WFStreamClient.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年04月20日 星期四 15时13分33秒
 ************************************************************************/

#include "WFChannel.h"
#include "WFChannelMsg.h"
#include "StreamMessage.h"
#include "WFFacilities.h"

#ifndef _SRC_CLIENT_WFSTREAMCLIENT_H_
#define _SRC_CLIENT_WFSTREAMCLIENT_H_

class WFStreamClient
{
using MSG = protocol::ProtocolMessage;
using protocolMsg = protocol::StreamMessage;

using ChannelMsg = WFChannelMsg<protocolMsg>;
using StreamChannelClient = WFChannelClient<protocolMsg>;

public:
    explicit WFStreamClient(std::string uri)
        : wg_(new WFFacilities::WaitGroup(1)),
          client_(new StreamChannelClient(0, 
              std::bind(&WFStreamClient::channel_done_callback, this, std::placeholders::_1)))
    {
        URIParser::parse(uri, this->uri_);
        this->client_->init(this->uri_);
        this->client_->set_keep_alive(-1);
        this->client_->start();
    }

    virtual ~WFStreamClient()
    {
        if (this->client_)
            this->client_->shutdown();

        if (this->wg_)
        {
            this->wg_->wait();
            delete this->wg_;
        }
    }

    int send(const char *data, size_t size)
    {
        if (!this->open())
            return false;

        return this->client_->send((void*)data, size);
    }

    void set_process_fn(std::function<int(WFChannel *, protocolMsg *)> fn) {
        this->client_->set_process_msg_fn(fn); 
    }
    
    void set_frist_msg_fn(std::function<ChannelMsg* (WFChannel*)> fn) {
        this->client_->set_frist_msg_fn(fn); 
    }
 
protected:
    void channel_done_callback(WFChannel::Channel*)
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
        std::unique_lock<std::mutex> lck(this->mutex_);
        if (!this->client_)
            return false;

        if (this->client_)
            return this->client_->is_open();

        return false;
    }

private:
  //  bool reset()
  //  {
  //      std::unique_lock<std::mutex> lck(this->mutex_);
  //      this->client_ = new StreamChannelClient(
  //          std::bind(&WFStreamClient::channel_done_callback, this, std::placeholders::_1));
  //      this->wg_ = new WFFacilities::WaitGroup(1);
  //      this->client_->init(this->uri_);
  //      this->client_->set_keep_alive(-1);
  //      this->client_->start();
  //      return true;
  //  }

private:
    ParsedURI                uri_;
    WFFacilities::WaitGroup *wg_;
    StreamChannelClient  *client_;
    std::mutex               mutex_;
};


#endif  // _SRC_CLIENT_WFSTREAMCLIENT_H_
