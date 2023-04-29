/*************************************************************************
    > File Name: WFChannelImpl.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2022年05月05日 星期四 11时37分35秒
 ************************************************************************/

#include "WFChannelMsg.h"
#include <functional>
#include <memory>
using WFChannelClientBase =
    WFChannelImpl<WFComplexClientTask<protocol::ProtocolMessage, protocol::ProtocolMessage>>;
using WFChannelServerBase =
    WFChannelImpl<WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>>;

template<typename protocolMsg, typename CMsgEntry=WFChannelMsg<protocolMsg>>
class WFChannelClient : public WFChannelClientBase
{
protected:
    virtual CommMessageOut *message_out()
    {
        /* By using prepare function, users can modify request after
         * the connection is established. */
        if (this->prepare)
            this->prepare(this);

        return WFChannelClientBase::message_out();
    }

    virtual int first_timeout()
    {
        return this->receive_timeout();
    }

    void set_prepare_once(std::function<void()> fn)
    {
        this->WFChannelClientBase::set_prepare([fn, this](Channel*) {
            fn();
            this->WFChannelClientBase::set_prepare(nullptr);
        }); 
    }

public:
    virtual int process_msg(MSG *message) { 
        if (this->process_msg_fn)
            return this->process_msg_fn(this, (protocolMsg*)message);
        
        return 0;
    }

    virtual MsgSession *new_msg_session() {
        MsgSession *session = new CMsgEntry(this);
        
        return session;
    }
    
    void set_frist_msg_fn(std::function<ChannelMsg* (WFChannel *)> fn) {
        this->frist_msg_fn = fn;
    }

    void set_process_msg_fn(std::function<int (WFChannel *, protocolMsg*)> fn) {
        this->process_msg_fn = fn;
    }

    virtual int send(void *buf, size_t size) {
        int ret = 0;
        auto *task = static_cast<CMsgEntry*>(this->safe_new_channel_msg(
                [](WFChannel* ch) {return new CMsgEntry(ch);}));

        if (!task)
            return -1;

        auto *msg = task->get_msg();
        ret = msg->append_fill(buf, size);
        
        task->start();
        
        return ret;
    }

private:
    // new CMsgEntry is safe in there
    std::function<ChannelMsg* (WFChannel*)> frist_msg_fn;
    std::function<int (WFChannel *, protocolMsg*)> process_msg_fn;

protected:
    int send_frist_msg() {
        if (this->frist_msg_fn) {
            ChannelMsg *task = this->frist_msg_fn(this);
            if (!task)
                return -1;

            return this->send_channel_msg(task, WFC_MSG_STATE_OUT_LIST);
        }
        
        return 0;
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
    WFChannelClient(int retry_max, channel_callback_t &&cb)
        : WFChannelClientBase(retry_max, std::move(cb)) {
            
        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);
       
        this->frist_msg_fn = nullptr;
        //connect frist send data only once
        this->set_prepare_once(
            std::bind(&WFChannelClient::send_frist_msg, this));
    }

protected:
    virtual ~WFChannelClient(){};
};

template<typename protocolMsg, typename CMsgEntry=WFChannelMsg<protocolMsg>>
class WFChannelServer : public WFChannelServerBase
{
public:
    virtual int process_msg(MSG *message) { 
        if (this->process_msg_fn)
            return this->process_msg_fn(this, (protocolMsg*)message);
        
        return 0;
    }

    virtual MsgSession *new_msg_session() {
        MsgSession *session = new CMsgEntry(this);
        
        return session;
    }
 
    virtual int send(void *buf, size_t size) {
        int ret = 0;
        auto *task = static_cast<CMsgEntry*>(this->safe_new_channel_msg(
                [](WFChannel* ch) {return new CMsgEntry(ch);}));

        if (!task)
            return -1;

        auto *msg = task->get_msg();
        ret = msg->append_fill(buf, size);
        
        task->start();
        return ret;
    }
 
public:
    void set_process_msg_fn(std::function<int (WFChannel *, protocolMsg*)> fn) {
        this->process_msg_fn = fn;
    }

private:
    std::function<int (WFChannel *, protocolMsg*)> process_msg_fn;
 
protected:
    virtual void dispatch()
    {
        this->subtask_done();
    }

    virtual void handle(int state, int error)
    {
        this->start();
    }

    virtual SubTask *done()
    {
        SeriesWork *series = series_of(this);
        if (this->callback)
            this->callback(this);

        this->delete_this(static_cast<void *>(this));
        return series->pop();
    }

    virtual bool is_server()
    {
        return true;
    }

public:
    explicit WFChannelServer(CommScheduler *scheduler, CommService *service = nullptr,
                             channel_callback_t &&cb = nullptr)
        : WFChannelServerBase(scheduler, std::move(cb))
    {
        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);
    }

    virtual ~WFChannelServer(){};
};

