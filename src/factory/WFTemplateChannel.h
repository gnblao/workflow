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
#include <cstddef>
#include <functional>

/**********TemlateChannel impl**********/
//using ChannelMsg = WFChannelMsg<protocolMsg>;

template<typename protocolMsg, typename ChannelMsg=WFChannelMsg<protocolMsg>>
class WFTemplateChannel {
public:
    int send_task_msg(ChannelMsg *task, int flag = WFC_MSG_STATE_OUT, protocolMsg *in = nullptr) {
        task->set_state(flag);
        
        if (in)
            series_of(dynamic_cast<ChannelMsg *>(in->session))->push_back(task);
        else
            task->start();
        
        return 0;
    }
    
    virtual int send(void *buf, size_t size) {
        int ret = 0;
        //if (!this->open())
        //    return -1;

        auto *task = new ChannelMsg(this->channel);
        auto *msg = task->get_msg();
        ret = msg->append_fill((char *)buf, size);
        
        if (ret >= 0)
            return this->send_task_msg(task);
        
        delete task;
        return ret;
    }

    virtual bool open() {
        return this->channel->is_open();
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
    
    void set_frist_msg_fill_fn(std::function<int (protocolMsg*)> fn) {
        this->frist_msg_fill_fn = fn;
    }

private:
    std::function<int (protocolMsg*)> frist_msg_fill_fn;

protected:
    int send_frist_msg() {
        int ret;
        if (this->frist_msg_fill_fn) {
            auto *task = new ChannelMsg(this);
            auto *msg = task->get_msg(); 
            
            ret = this->frist_msg_fill_fn(msg);
            if (ret >= 0)
                return this->send_task_msg(task, WFC_MSG_STATE_OUT_LIST);
            
            delete task;
            return ret;
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
        : WFChannelClient(0, std::move(cb)), WFTemplateChannel<protocolMsg>(this), frist_msg_fill_fn(nullptr) {
        
        this->set_keep_alive(-1);
        this->set_receive_timeout(-1);
        this->set_send_timeout(-1);
        
        //connect frist send data
        this->set_prepare(
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

    }

    virtual ~WFTemplateChannelServer(){}
};
#endif //_SRC_FACTORY_WFTEMPLATECHANNEL_H_
