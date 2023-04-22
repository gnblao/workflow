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

#include "Communicator.h"
#include "ProtocolMessage.h"
#include "WFChannel.h"
#include "Workflow.h"

template <typename MSG> 
class WFChannelMsgBase : public SubTask, public MsgSession {
    static_assert(std::is_base_of<protocol::ProtocolMessage, MSG>::value,
                  "ProtocolMessage must is base of MSG");

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
    explicit WFChannelMsgBase(WFChannel *channel)
        : state(WFC_MSG_STATE_OUT), error(0), msg(new MSG) {
        assert(channel);
        this->channel = channel;
        this->channel->incref();
    }

    explicit WFChannelMsgBase(WFChannel *channel, MSG *msg) : state(WFC_MSG_STATE_OUT), error(0) {
        assert(channel);
        assert(msg);
        this->channel = channel;
        this->msg = msg;
        this->channel->incref();
    }

    virtual ~WFChannelMsgBase() {
        this->channel->decref();
        delete this->msg;
    }

    WFChannel *get_channel() const { return this->channel; }
    void set_channel(WFChannel *channel) { this->channel = channel; }

    virtual int get_state() const { return this->state; }
    virtual void set_state(int state) { this->state = state; }

    virtual MSG *get_msg() { return this->msg; }

protected:
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

private:
    virtual CommMessageOut *message_out() final {
        errno = ENOSYS;
        return NULL;
    }

    virtual CommMessageIn *message_in()  final {
        errno = ENOSYS;
        return NULL;
    }
};

template <typename MSG> class WFChannelMsg : public WFChannelMsgBase<MSG> {
public:
    std::function<void(WFChannelMsg<MSG> *)> process;
    std::function<void(WFChannelMsg<MSG> *)> callback;

    void set_callback(std::function<void(WFChannelMsg<MSG> *)> cb) {
        this->callback = std::move(cb);
    }

private:
    std::function<void(WFChannelMsg<MSG> *)> inner_callback;

    void set_inner_callback(std::function<void(WFChannelMsg<MSG> *)> cb) {
        this->inner_callback = std::move(cb);
    }
private:
    virtual void eat_msg() {
        int ret = -1;
        int state;
        auto channel = this->get_channel();
        MSG *msg = this->pick_msg();

        state = this->get_state();
        if (state == WFC_MSG_STATE_IN)
            ret = channel->fanout_msg_in(msg, msg->get_seq());
        else if (state == WFC_MSG_STATE_OUT_LIST)
            ret = channel->msg_out_list(msg);
        else
            ret = channel->msg_out(msg);

        if (ret < 0) {
            this->set_state(WFC_MSG_STATE_ERROR);
            delete msg;
        }
    }

protected:
    virtual SubTask *done() {
        SeriesWork *series = series_of(this);

        if (this->callback)
            this->callback(this);
        
        if (this->inner_callback)
            this->inner_callback(this);

        delete this;
        return series->pop();
    }

    virtual void dispatch() {
        if (this->process)
            this->process(this);

        this->eat_msg();
        this->subtask_done();
    }

    virtual void handle(int state, int error) {
        if (state == WFT_STATE_SUCCESS || state == WFT_STATE_TOREPLY) {
            //this->start();
            auto pool = this->channel->get_resource_pool();
            if (pool) {
                auto cond = pool->get(this);
                if (cond) {
                    this->set_inner_callback([pool](WFChannelMsg<MSG> *){ pool->post(nullptr);});
                    cond->start();
                }
            }
        } else {
            std::cout << "bug: WFChannelMsg<MSG> handle state must is "
                         "WFT_STATE_SUCCESS/WFT_STATE_TOREPLY other is Bug!!!"
                      << std::endl;
        }
    }

public:
    WFChannelMsg(WFChannel *channel, std::function<void(WFChannelMsg<MSG> *)> proc = nullptr)
        : WFChannelMsgBase<MSG>(channel), process(std::move(proc)) {}

    WFChannelMsg(WFChannel *channel, MSG *msg,
                 std::function<void(WFChannelMsg<MSG> *)> proc = nullptr)
        : WFChannelMsgBase<MSG>(channel, msg), process(std::move(proc)) {}

    virtual ~WFChannelMsg() {}
};

#endif // _FACTORY_WFCHANNELMSG_H_
