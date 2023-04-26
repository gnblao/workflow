/*************************************************************************
    > File Name: WFChannelMsg.h
    > Author: gnblao
    > Mail: gnblao
    > Created Time: 2022年04月17日 星期日 14时01分32秒
 ************************************************************************/

#ifndef _FACTORY_WFCHANNELMSG_H_
#define _FACTORY_WFCHANNELMSG_H_

#include <cassert>
#include <functional>
#include <iostream>

#include "Communicator.h"
#include "ProtocolMessage.h"
#include "SubTask.h"
#include "WFChannel.h"
#include "WFTask.h"
#include "Workflow.h"

class MsgTask : public SubTask, public MsgSession {
using MSG = protocol::ProtocolMessage;
public:
    void start() {
        assert(!series_of(this));
        Workflow::start_series_work(this, nullptr);
    }

    void dismiss() {
        assert(!series_of(this));
        delete this;
    }

public:
    explicit MsgTask(WFChannel *channel, MSG *msg) : state(WFC_MSG_STATE_OUT), error(0) {
        assert(channel);
        assert(msg);
        this->msg = msg;
    
        if(channel->incref() > 0) {
            this->channel = channel;
        } else { 
            std::cout << "The context of the new object (WFChannelMsg<XXX>) is incorrect !!!! "
                      << "please consider using the safe_new_msg_task function" << std::endl;
            this->channel = nullptr;
        }
    }
    
    virtual ~MsgTask() {
        if (this->channel)
            this->channel->decref();
        
        if (this->msg)
            delete this->msg;
    }

    WFChannel *get_channel() const { return this->channel; }
    //void set_channel(WFChannel *channel) { this->channel = channel; }

    virtual int get_state() const { return this->state; }
    virtual void set_state(int state) { this->state = state; }

protected:
    virtual MSG *get_msg() { return this->msg; }
    virtual MSG *pick_msg() {
        MSG *m = this->msg;

        this->msg = nullptr;

        return m;
    }

protected:
    int state;
    int error;

private:
    MSG *msg;

protected:
    WFChannel *channel;
};

template <typename MSG_ENTRY> 
class WFChannelMsg : public MsgTask {
public:
    void set_callback(std::function<void(WFChannelMsg<MSG_ENTRY> *)> cb) {
        this->callback = std::move(cb);
    }
    
    MSG_ENTRY* get_msg() { return static_cast<MSG_ENTRY*>(this->::MsgTask::get_msg());}
    MSG_ENTRY* pick_msg() { return static_cast<MSG_ENTRY*>(this->::MsgTask::pick_msg());}

protected:
    void set_inner_callback(std::function<void(WFChannelMsg<MSG_ENTRY> *)> cb) {
        this->inner_callback = std::move(cb);
    }
private:
    std::function<void(WFChannelMsg<MSG_ENTRY> *)> inner_callback;
    std::function<void(WFChannelMsg<MSG_ENTRY> *)> process;
    std::function<void(WFChannelMsg<MSG_ENTRY> *)> callback;

private:
    virtual void eat_msg() {
        int ret = -1;
        int state;
        auto channel = this->get_channel();
        MSG_ENTRY *msg = this->pick_msg();

        state = this->get_state();
        switch (state) {
        case WFC_MSG_STATE_IN:
            ret = channel->fanout_msg_in(msg);
            break;
        case WFC_MSG_STATE_OUT_LIST:
            ret = channel->msg_out_list(msg);
            break;
        case WFC_MSG_STATE_OUT:
            ret = channel->msg_out(msg);
            break;
        default:
            //ret = 0;
            break;
        }

        if (ret < 0) {
            this->set_state(WFC_MSG_STATE_ERROR);
            delete msg;
        } else {
            this->set_state(WFC_MSG_STATE_SUCCEED);
        }
    }

protected:
    virtual SubTask *done() {
        SeriesWork *series = series_of(this);
        
        if (this->get_state() == WFC_MSG_STATE_DONE) {
            delete this;
        } else {
            if (this->callback)
                this->callback(this);

            if (this->inner_callback)
                this->inner_callback(this);
            
            if (this->get_state() == WFC_MSG_STATE_DELAYED) {
                this->set_state(WFC_MSG_STATE_DONE);
            } else
                delete this;
        }
        
        return series->pop();
    }

    virtual void dispatch() {
        if (this->get_state() > WFC_MSG_STATE_SUCCEED) {
            if (this->process)
                this->process(this);

            this->eat_msg();
        }
            
        if (this->get_state() == WFC_MSG_STATE_SUCCEED) {
            this->set_state(WFC_MSG_STATE_DELAYED);

            //series_of(this)->push_front(this);
            series_of(this)->set_last_task(this);
        }

        this->subtask_done();
    }

    virtual void handle(int state, int error) {
        if (state == WFT_STATE_SUCCESS || state == WFT_STATE_TOREPLY) {
            //this->start();
            auto pool = this->channel->get_resource_pool();
            if (pool) {
                auto cond = pool->get(this);
                if (cond) {
                    this->set_inner_callback([pool](WFChannelMsg<MSG_ENTRY> *){ pool->post(nullptr);});
                    cond->start();
                }
            }
        } else {
            std::cout << "bug: WFChannelMsg<MSG_ENTRY> handle state must is "
                         "WFT_STATE_SUCCESS/WFT_STATE_TOREPLY, other is Bug!!!"
                      << std::endl;
        }
    }

public:
    WFChannelMsg(WFChannel *channel, std::function<void(WFChannelMsg<MSG_ENTRY> *)> proc = nullptr)
        : WFChannelMsg<MSG_ENTRY>(channel, new MSG_ENTRY, std::move(proc)) {}

    WFChannelMsg(WFChannel *channel, MSG_ENTRY *msg,
                 std::function<void(WFChannelMsg<MSG_ENTRY> *)> proc = nullptr)
        : MsgTask(channel, msg), process(std::move(proc)) {}

    virtual ~WFChannelMsg() {}
};

#endif // _FACTORY_WFCHANNELMSG_H_
