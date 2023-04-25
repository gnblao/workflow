/*************************************************************************
    > File Name: WFTemplateChannel.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年04月18日 星期二 22时25分32秒
 ************************************************************************/


#ifndef _SRC_FACTORY_WFTEMPLATECHANNEL_H_
#define _SRC_FACTORY_WFTEMPLATECHANNEL_H_
#include "Communicator.h"
#include "EndpointParams.h"
#include "ProtocolMessage.h"
#include "WFChannel.h"
#include "WFChannelMsg.h"
#include "Workflow.h"
#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>

/**********TemlateChannel impl**********/
//using ChannelMsg = WFChannelMsg<protocolMsg>;

template<typename protocolMsg, typename ChannelMsg=WFChannelMsg<protocolMsg>>
class WFTemplateChannel {
private:
    static_assert(std::is_base_of<protocol::ProtocolMessage, protocolMsg>::value,
                  "protocol::ProtocolMessage must is base of protocolMsg");
public:
    int send_task_msg(MsgTask *task, int flag = WFC_MSG_STATE_OUT, protocolMsg *in = nullptr) {
        task->set_state(flag);
        
        if (in)
            series_of(dynamic_cast<MsgTask *>(in->session))->push_back(task);
        else
            task->start();
        
        return 0;
    }
    
    virtual int send(void *buf, size_t size) {
        int ret = 0;
        auto *task = this->thread_safe_new_msg(
                [](WFChannel* ch) {return new ChannelMsg(ch);});

        if (!task)
            return -1;

        auto *msg = task->get_msg();
        ret = msg->append_fill(buf, size);
        
        if (ret >= 0)
            this->send_task_msg(static_cast<MsgTask*>(task));
        else
            delete task;
        
        return ret;
    }
    
    virtual bool open() {
        return this->channel->is_open();
    }

protected:
    virtual MsgSession* thread_safe_new_msg(std::function<MsgSession*(WFChannel*)> fn) {
        // Atomic this->ref protects new(ChannelMsg) successfully
        // in the active sending scenario
        {
            std::lock_guard<std::recursive_mutex> lck(this->channel->write_mutex);
            if (!this->open())
                return nullptr;

            if (this->channel->incref() <= 0) {
                //std::cout << "This shouldn't happen, and if it does it's a bug!!!!" << std::endl;
                this->channel->decref(1);
                return nullptr;
            }
        }
    
        // now is safe new
        //auto task = new ChannelMsg(this->channel);
        auto task = fn(this->channel);
        this->channel->decref();
        
        return task;
    }


public:
    explicit WFTemplateChannel<protocolMsg, ChannelMsg>(WFChannel *channel) {
        assert(channel);
        this->channel = channel;
        this->process_msg_fn = nullptr;
    }

    virtual ~WFTemplateChannel(){}

protected:
    std::function<int (WFChannel *, protocolMsg*)> process_msg_fn;

public:
    void set_process_msg_fn(std::function<int (WFChannel *, protocolMsg*)> fn) {
        this->process_msg_fn = fn;
    }

private:
    WFChannel *channel;
};

template<typename protocolMsg, typename ChannelMsg=WFChannelMsg<protocolMsg>>
class WFTemplateChannelClient : public WFChannelClient, public WFTemplateChannel<protocolMsg, ChannelMsg> {
public:
    virtual int process_msg(MSG *message) { 
        if (this->process_msg_fn)
            return this->process_msg_fn(this, (protocolMsg*)message);
        
        return 0;
    }

    virtual MsgSession *new_msg_session() {
        MsgSession *session = new ChannelMsg(this);
        
        return session;
    }
    
    void set_frist_msg_fn(std::function<ChannelMsg* (WFChannel *)> fn) {
        this->frist_msg_fn = fn;
    }

private:
    // new ChannelMsg is safe in there
    std::function<ChannelMsg* (WFChannel*)> frist_msg_fn;

protected:
    int send_frist_msg() {
        if (this->frist_msg_fn) {
            ChannelMsg *task = this->frist_msg_fn(this);
            if (!task)
                return -1;

            return this->send_task_msg(task, WFC_MSG_STATE_OUT_LIST);
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
    explicit WFTemplateChannelClient(channel_callback_t cb = nullptr)
        : WFChannelClient(0, std::move(cb)), WFTemplateChannel<protocolMsg>(this), frist_msg_fn(nullptr) {
        
        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);
        
        //connect frist send data only once
        this->set_prepare_once(
            std::bind(&WFTemplateChannelClient::send_frist_msg, this));
    }

    virtual ~WFTemplateChannelClient(){}
};


template<typename protocolMsg, typename ChannelMsg=WFChannelMsg<protocolMsg>>
class WFTemplateChannelServer : public WFChannelServer, public WFTemplateChannel<protocolMsg, ChannelMsg> {
public:
    virtual int process_msg(MSG *message) { 
        if (this->process_msg_fn)
            return this->process_msg_fn(this, (protocolMsg*)message);
        
        return 0;
    }

    virtual MsgSession *new_msg_session() {
        MsgSession *session = new ChannelMsg(this);
        
        return session;
    }
  
public:
    explicit WFTemplateChannelServer(CommService *service, CommScheduler *scheduler)
        : WFChannelServer(scheduler, service), WFTemplateChannel<protocolMsg, ChannelMsg>(this) {
    
        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);
    }

    virtual ~WFTemplateChannelServer(){}
};
#endif //_SRC_FACTORY_WFTEMPLATECHANNEL_H_
