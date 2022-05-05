/*************************************************************************
    > File Name: WFChannelMsg.h
    > Author: gnblao 
    > Mail: gnblao 
    > Created Time: 2022年04月17日 星期日 14时01分32秒
 ************************************************************************/

#ifndef _WFCHANNEL_H_
#define _WFCHANNEL_H_

#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <mutex>
#include <type_traits>
#include <queue>
#include <list>
#include <utility>

#include "CommScheduler.h"
#include "Communicator.h"
#include "WFConnection.h"
#include "Workflow.h"
#include "WFTask.h"
#include "WFTaskFactory.h"
#include "CommRequest.h"
#include "ProtocolMessage.h"
#include "WFGlobal.h"

enum {
        WFC_MSG_STATE_ERROR = -1,
        WFC_MSG_STATE_UNDEFINED,
        WFC_MSG_STATE_IN,
        WFC_MSG_STATE_OUT,
        WFC_MSG_STATE_OUT_LIST,
};

class WFChannelMsgSession :public CommSession {
public:
    virtual protocol::ProtocolMessage *get_msg() =0;
    virtual int get_state() {return WFC_MSG_STATE_UNDEFINED;};
    virtual void set_state(int state){};

    WFChannelMsgSession(): CommSession() {}
    virtual ~WFChannelMsgSession(){};
};

class WFChannel {
protected:
    using MSG = protocol::ProtocolMessage;
public:
    virtual WFChannelMsgSession *new_channel_msg_session() = 0;
    virtual void incref() = 0;
    virtual void decref() = 0;
    virtual int channel_close() = 0;
    virtual long long get_channel_msg_seq() = 0;
    virtual int channel_fanout_msg_in(MSG *in, long long seq) = 0;
    virtual int channel_fanout_msg_out(MSG *out, long long seq) = 0;
    virtual int channel_msg_out(MSG *out, int flag = WFC_MSG_STATE_OUT) = 0;
    virtual bool is_server() = 0;
    std::atomic_bool stop_flag{false};
};

template<typename ChannelBase = WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>>
class WFChannelImpl : public ChannelBase, public WFChannel
{
static_assert(std::is_base_of<WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>, ChannelBase>::value, 
        "WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>> must is base of ChannelBase");
protected:
    using ChannelBaseTask = WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>;
	using task_callback_t = std::function<void (ChannelBaseTask *)>;
	
    virtual CommMessageIn *message_in()
    {
        if (this->stop_flag)
            return nullptr;

        CommMessageIn *msg = this->get_message_in();
        WFChannelMsgSession *session;

        if (msg)
            return msg;

        session = this->new_channel_msg_session();
        if (!session) {
            return nullptr;
        }

        msg = session->get_msg();
        session->set_state(WFC_MSG_STATE_IN);

        if (!msg) {
            return nullptr;
        }

        msg->seq = this->msg_seq++;
        msg->session = session;
        return msg;
    }

    virtual CommMessageOut *message_out()
    {
        //if (this->stop_flag)
        //    return nullptr;

        CommMessageOut *msg;
        std::lock_guard<std::mutex> lck(this->write_mutex);
        if (this->write_list.size()) {
            msg = this->write_list.front();
            this->write_list.pop_front();
            return msg;
        }

        return nullptr;
    }


private:
    std::atomic<long long> msg_seq;
    std::atomic<long long> req_seq;
    std::atomic<long> ref;

public:
    virtual WFChannelMsgSession *new_channel_msg_session() {return nullptr;};
    long long get_channel_msg_seq() {return this->msg_seq;}
	
    virtual int channel_close() {
        this->stop_flag.exchange(true);
        this->get_scheduler()->channel_shutdown(this);
        return 0;
    }

    virtual void incref() {
        this->ref++;
    }
    virtual void decref() {
        if (--this->ref == 0)
            delete this;
    }
    
    virtual bool is_server() {return false;}
    virtual bool is_channel() {return true;}

private:
    using __MSG = std::pair<long long, MSG *>; 
    class cmp {
    public:
        bool operator() (__MSG a, __MSG b) {
            return a.first > b.first;        
        }
    };
    
    std::mutex in_mutex;
    std::priority_queue<__MSG, std::vector<__MSG>, cmp> fanout_heap_in;
    std::list<MSG*> in_list;
    long long in_list_seq = 0;

    std::mutex out_mutex;
    std::priority_queue<__MSG, std::vector<__MSG>, cmp> fanout_heap_out;
    std::list<MSG*> out_list;
    long long out_list_seq = 0;
    
    std::mutex write_mutex;
    std::list<MSG*> write_list;
    
public:
    virtual int process_msg(MSG *msg) {
        return 0;
    }
    
    virtual int channel_fanout_msg_in(MSG *in, long long seq)
	{
        int ret;
        CommSession *cur_session = in->session;

        if (this->stop_flag)
            return -1;
        
        //std::cout << __func__ << " ---seq:" << seq << std::endl;
        
        std::lock_guard<std::mutex> lck(this->in_mutex);
        assert(this->in_list_seq <= seq);

        fanout_heap_in.emplace(std::make_pair(seq, in));
        while (fanout_heap_in.top().first == this->in_list_seq) {
            auto x =  fanout_heap_in.top();
            fanout_heap_in.pop();
            this->in_list_seq ++;

            if (x.second)
                in_list.push_back(x.second);
        }
       
        while (in_list.size()) {
            MSG *msg = in_list.front();
            in_list.pop_front();
            
            msg->session = cur_session;
            ret = process_msg(msg);
            delete msg;
            if (ret < 0)
                break;
        }

        return 0;
    }

    virtual int channel_fanout_msg_out(MSG *out, long long seq)
    {
        int ret;
        if (this->stop_flag)
            return -1;

        std::lock_guard<std::mutex> lck(this->out_mutex);
        assert(this->out_list_seq <= seq);
        
        //std::cout << __func__ << " seq :"  << seq << std::endl;        
        
        fanout_heap_out.emplace(std::make_pair(seq, out));
        while (fanout_heap_out.top().first == this->out_list_seq) {
            auto x =  fanout_heap_out.top();
            fanout_heap_out.pop();
            
            this->out_list_seq ++;
            if (x.second)
                out_list.push_back(x.second);
        }
       
        while (out_list.size()) {
            MSG *msg = out_list.front();
            out_list.pop_front();

            ret = this->channel_msg_out(msg);
            if (ret < 0) {
                delete msg;
                break;
            }
        }
        
        return 0;
    }
    
    virtual int channel_msg_out(MSG *out, int flag = WFC_MSG_STATE_OUT)
    {
        int ret;
        
        if (this->stop_flag)
            return -1;

        {
            std::lock_guard<std::mutex> lck(this->write_mutex);
            this->write_list.push_back(out);
        }
        
        if (flag == WFC_MSG_STATE_OUT_LIST)
            return 0;

        ret = this->get_scheduler()->channel_send_one(this);
        if (ret < 0) { 
            this->channel_close();
        }

        return 0;
    }

protected:
	/*for client*/
	explicit WFChannelImpl(int retry_max, task_callback_t&& cb):
	    ChannelBase(retry_max, std::move(cb))
	{
        this->msg_seq = 0;
        this->req_seq = 0;
        this->ref = 1;
	}
	
    /*for server*/
	explicit WFChannelImpl(CommScheduler *scheduler, task_callback_t&& cb):
	    ChannelBase(nullptr, scheduler, std::move(cb))
	{
        this->msg_seq = 0;
        this->req_seq = 0;
        this->ref = 1;
	}

protected:
    void delete_this(void *t) {
        //CommMessageIn *in = this->get_message_in();
        //if (in) {
        //    if (in->session) {
        //        delete in->session;
        //        in->session = nullptr;
        //    }
        //    //delete in;
        //}
        
        this->stop_flag = true;
        this->decref();
    }

    virtual ~WFChannelImpl()
    { 
        while (!fanout_heap_in.empty()) {
            auto _msg = fanout_heap_in.top();
            fanout_heap_in.pop();

            delete _msg.second;
        }

        for (auto x : in_list) {
            in_list.remove(x);
            delete x;
        }

        while (!fanout_heap_out.empty()) {
            auto _msg = fanout_heap_out.top();
            fanout_heap_out.pop();

            delete _msg.second;
        }

        for (auto x : out_list) {
            out_list.remove(x);
            delete x;
        }

        for (auto x : write_list) {
            write_list.remove(x);
            delete x;
        }
        
        CommMessageIn *in = this->get_message_in();
        if (in) {
            if (in->session) {
                delete in->session;
                in->session = nullptr;
            }
            //delete in;
            *(this->get_in()) = nullptr;
        }
        
        CommMessageOut *out = this->get_message_out();
        if (out) {
            delete out;
            *(this->get_out()) = nullptr;
        }
    }

};

#include "WFChannel.inl"

#endif

