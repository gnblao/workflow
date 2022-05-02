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
#include "CommRequest.h"
#include "ProtocolMessage.h"
#include "WFGlobal.h"

enum {
        WFC_MSG_STATE_UNDEFINED = -1,
        WFC_MSG_STATE_IN,
        WFC_MSG_STATE_OUT,
        WFC_MSG_STATE_OUT_LIST,
        WFC_MSG_STATE_ERROR,
};

class WFChannelMsgSession :public CommSession {
public:
    virtual protocol::ProtocolMessage *get_msg() =0;
    virtual int get_state() {return WFC_MSG_STATE_UNDEFINED;};
    virtual void set_state(int state){};

    WFChannelMsgSession(): CommSession() {}
    virtual ~WFChannelMsgSession(){};
};

using ChannelBase = WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>;
class WFChannel : public ChannelBase
{
protected:
    using MSG = protocol::ProtocolMessage;

    virtual WFConnection *get_connection() const { return nullptr;};
	virtual CommMessageIn *message_in();
    virtual CommMessageOut *message_out();

private:
    std::atomic<long long> msg_seq;
    std::atomic<long long> req_seq;

    std::atomic<long> ref;

public:
    std::atomic_bool stop_flag{false};
	
    virtual WFChannelMsgSession *new_channel_msg_session() {return nullptr;};
    long long get_channel_msg_seq() {return this->msg_seq;}
    virtual bool is_channel() {return true;}
	
    virtual int channel_close() {
        this->stop_flag.exchange(true);
        this->get_scheduler()->channel_shutdown(this);
        return 0;
    }

    void incref() {
        this->ref++;
    }
    void decref() {
        if (--this->ref == 0)
            delete this;
    }
    
    virtual bool is_server() {return false;}
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

public:
	WFChannel(CommSchedObject *object, CommScheduler *scheduler) :
	    ChannelBase(object, scheduler, nullptr)
	{
        this->msg_seq = 0;
        this->req_seq = 0;
        this->ref = 1;
	}

private:
    void __clear() {
        CommMessageIn *in = this->get_message_in();
        if (in) {
            if (in->session) {
                delete in->session;
                in->session = nullptr;
            }
            //delete in;
        }
    }

protected:
    virtual SubTask *done()
	{
        this->stop_flag = true;
        this->__clear();

        SeriesWork *series = series_of(this);
		if (this->callback)
			this->callback(this);

        this->decref();
		//delete this;
		return series->pop();
	}
    
    virtual ~WFChannel()
    { 
        while (!fanout_heap_in.empty()) {
            auto _msg = fanout_heap_in.top();
            fanout_heap_in.pop();

            delete _msg.second;
        }

        for (auto x : in_list) {
            delete x;
        }

        while (!fanout_heap_out.empty()) {
            auto _msg = fanout_heap_out.top();
            fanout_heap_out.pop();

            delete _msg.second;
        }

        for (auto x : out_list) {
            delete x;
        }

        for (auto x : write_list) {
            delete x;
        }

        CommMessageOut *out = this->get_message_out();
        delete out;
    }

};

#include "WFChannel.inl"

#endif

