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

template <typename protocolMsg> 
class WFChannelMsg : public ChannelMsg {
public:
    void set_callback(std::function<void(WFChannelMsg<protocolMsg> *)> cb) {
        this->callback = std::move(cb);
    }
    
    protocolMsg* get_msg() { return static_cast<protocolMsg*>(this->ChannelMsg::get_msg());}

private:
    std::function<void(WFChannelMsg<protocolMsg> *)> callback;
    //std::function<void(WFChannelMsg<protocolMsg> *)> process;

    protocolMsg* pick_msg() { return static_cast<protocolMsg*>(this->ChannelMsg::pick_msg());}
private:
    virtual void channel_eat_msg() {
        int ret = -1;
        int state;
        auto channel = this->get_channel();
        protocolMsg *msg = this->pick_msg();

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
        
        if (this->get_state() == WFC_MSG_STATE_SUCCEED) {
            this->set_state(WFC_MSG_STATE_DELAYED);
            
            //series_of(this)->set_last_task(this);
            series_of(this)->push_back(this);
        } else {
            if (this->callback)
                this->callback(this);
            
            if (this->inner_callback)
                this->inner_callback(static_cast<ChannelMsg*>(this));
            
            delete this;
        }
        
        return series->pop();
    }

    virtual void dispatch() {
        if (this->get_state() > WFC_MSG_STATE_SUCCEED) {
            if (this->process)
                this->process(this);

            this->channel_eat_msg();
        }

        this->subtask_done();
    }

    virtual void handle(int state, int error) {
        if (state == WFT_STATE_SUCCESS || state == WFT_STATE_TOREPLY) {
            this->start();
        } else {
            std::cout << "bug: WFChannelMsg<protocolMsg> handle state must is "
                         "WFT_STATE_SUCCESS/WFT_STATE_TOREPLY, other is Bug!!!"
                      << std::endl;
            delete this;
        }
    }

public:
    WFChannelMsg(WFChannel *channel, std::function<void(ChannelMsg *)> proc = nullptr)
        : WFChannelMsg<protocolMsg>(channel, new protocolMsg, std::move(proc)) {}

    WFChannelMsg(WFChannel *channel, protocolMsg *msg,
                 std::function<void(ChannelMsg *)> proc = nullptr)
        : ChannelMsg(channel, msg, std::move(proc)) {}

    virtual ~WFChannelMsg() {}
};

#endif // _FACTORY_WFCHANNELMSG_H_
