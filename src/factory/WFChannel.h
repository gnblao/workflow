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
#include "WFTask.h"
#include "WFTaskFactory.h"
#include "ProtocolMessage.h"

enum {
        WFC_MSG_STATE_ERROR = -1,
        WFC_MSG_STATE_UNDEFINED,
        WFC_MSG_STATE_IN,
        WFC_MSG_STATE_OUT,
        WFC_MSG_STATE_OUT_LIST,
};

class MsgSession :public CommSession {
public:
    virtual protocol::ProtocolMessage *get_msg() = 0;
    virtual int get_state() const = 0;
    virtual void set_state(int state) = 0;

    MsgSession(): CommSession() {}
    virtual ~MsgSession(){};
};

class WFChannel {
protected:
    using MSG = protocol::ProtocolMessage;
public:
    using BaseTask = WFNetworkTask<MSG, MSG>;
public:
    virtual MsgSession *new_msg_session() = 0;

    virtual void incref() = 0;
    virtual void decref() = 0;
    
    virtual long long get_msg_seq() = 0; /*for in msg only*/
    virtual long long get_req_seq() = 0; /*for req only*/
    
    virtual int fanout_msg_in(MSG *in, long long seq) = 0;
    virtual int fanout_msg_out(MSG *out, long long seq) = 0;
    virtual int msg_out(MSG *out) = 0;
    virtual int msg_out_list(MSG *out) = 0;
    
    virtual bool is_server() = 0;
    virtual bool is_open() = 0;
    virtual int shutdown() = 0;
};

template<typename ChannelBase = WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>>
class WFChannelImpl : public ChannelBase, public WFChannel
{
private:
    static_assert(std::is_base_of<BaseTask, ChannelBase>::value, 
            "WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>> must is base of ChannelBase");

protected:
	using task_callback_t = std::function<void (BaseTask *)>;
	
    virtual CommMessageIn *message_in()
    {
        MsgSession *session;
        
        if (!this->is_open())
            return nullptr;

        CommMessageIn *msg = this->get_message_in();
        if (msg)
            return msg;

        session = this->new_msg_session();
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
        CommMessageOut *msg;
        
        //if (!this->is_open())
        //    return nullptr;

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
    
    std::atomic_bool stop_flag{false};

public:
    //virtual MsgSession *new_msg_session() {return nullptr;};
    long long get_msg_seq() {return this->msg_seq;}
    long long get_req_seq() {return this->req_seq;}
	
    virtual int shutdown() {
        this->stop_flag.exchange(true);
        this->get_scheduler()->channel_shutdown(this);
        return 0;
    }
    
    virtual bool is_open() {return !this->stop_flag;};

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
    
    virtual int fanout_msg_in(MSG *in, long long seq)
	{
        int ret;
        CommSession *cur_session = in->session;

        if (!this->is_open())
            return -1;
        
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

    virtual int fanout_msg_out(MSG *out, long long seq)
    {
        int ret;
        if (!this->is_open())
            return -1;

        std::lock_guard<std::mutex> lck(this->out_mutex);
        assert(this->out_list_seq <= seq);
        
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

            ret = this->msg_out(msg);
            if (ret < 0) {
                delete msg;
                break;
            }
        }
        
        return 0;
    }
    
    virtual int msg_out(MSG *out, int flag)
    {
        int ret;
        
        if (!this->is_open())
            return -1;

        {
            std::lock_guard<std::mutex> lck(this->write_mutex);
            this->write_list.push_back(out);
        }
        
        if (flag == WFC_MSG_STATE_OUT_LIST)
            return 0;

        ret = this->get_scheduler()->channel_send_one(this);
        if (ret < 0) { 
            this->shutdown();
        }

        return 0;
    }

    virtual int msg_out(MSG *out) {
        return this->msg_out(out, WFC_MSG_STATE_OUT);
    }
    virtual int msg_out_list(MSG *out) {
        return this->msg_out(out, WFC_MSG_STATE_OUT_LIST);
    }
protected:
	/*for client*/
	explicit WFChannelImpl(int retry_max, task_callback_t&& cb):
	    ChannelBase(retry_max, std::move(cb))
	{
        this->msg_seq = 0;
        this->req_seq = 0;
        this->ref = 1;
        this->stop_flag = false;
	}
	
    /*for server*/
	explicit WFChannelImpl(CommScheduler *scheduler, task_callback_t&& cb):
	    ChannelBase(nullptr, scheduler, std::move(cb))
	{
        this->msg_seq = 0;
        this->req_seq = 0;
        this->ref = 1;
        this->stop_flag = false;
	}

protected:
    void delete_this(void *t) {
        this->stop_flag = true;
        
        CommMessageIn *in = this->get_message_in();
        if (in) {
            if (in->session) {
                delete in->session;
                in->session = nullptr;
            }
            //delete in;
        }
        
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
        
        //CommMessageIn *in = this->get_message_in();
        //if (in) {
        //    if (in->session) {
        //        delete in->session;
        //        in->session = nullptr;
        //    }
        //    //delete in;
        //    *(this->get_in()) = nullptr;
        //}
        
        CommMessageOut *out = this->get_message_out();
        if (out) {
            delete out;
            *(this->get_out()) = nullptr;
        }
    }

};

#include "WFChannel.inl"

#endif

