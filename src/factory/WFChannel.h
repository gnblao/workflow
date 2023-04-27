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
#include <list>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

#include "CommScheduler.h"
#include "Communicator.h"
#include "ProtocolMessage.h"
#include "SubTask.h"
#include "WFResourcePool.h"
#include "WFTask.h"
#include "WFTaskFactory.h"

enum
{
    WFC_MSG_STATE_DONE = -3,
    WFC_MSG_STATE_DELAYED = -2,
    WFC_MSG_STATE_ERROR = -1,
    WFC_MSG_STATE_SUCCEED = 0,
    WFC_MSG_STATE_IN = 1,
    WFC_MSG_STATE_OUT = 2,
    WFC_MSG_STATE_OUT_LIST = 3,
};

class MsgSession : public CommSession
{
public:
    virtual protocol::ProtocolMessage *get_msg()            = 0;
    virtual int                        get_state() const    = 0;
    virtual void                       set_state(int state) = 0;

    MsgSession() : CommSession()
    {
    }
    virtual ~MsgSession(){};

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

class WFChannel
{
protected:
    using MSG = protocol::ProtocolMessage;
    
public:
    using Channel = WFNetworkTask<MSG, MSG>;

public:
    // new ChannelMsg is safe in the new_msg_session function
    virtual MsgSession *new_msg_session() = 0;
    
    // Active call for send 
    std::recursive_mutex       write_mutex;
    // virtual MsgSession *thread_safe_new_msg_session() = 0;

    virtual int incref() = 0;
    virtual void decref(int skip_delete=0) = 0;

    virtual long long get_msg_seq() = 0; /*for in msg only*/
    virtual long long get_req_seq() = 0; /*for req only*/

    virtual int fanout_msg_in(MSG *in)   = 0;
    virtual int fanout_msg_out(MSG *out) = 0;
    virtual int msg_out(MSG *out)                       = 0;
    virtual int msg_out_list(MSG *out)                  = 0;

    virtual bool is_server() = 0;
    virtual bool is_open()   = 0;
    virtual int  shutdown()  = 0;
    virtual WFResourcePool* get_resource_pool()  = 0;

    virtual void set_termination_cb(std::function<void()>)  = 0;
};

template <typename ChannelEntry = WFChannel::Channel>
class WFChannelImpl : public ChannelEntry, public WFChannel
{
private:
    static_assert(std::is_base_of<Channel, ChannelEntry>::value,
                  "WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>> must is "
                  "base of ChannelEntry");
protected:
    using channel_callback_t = std::function<void(Channel*)>;

    virtual CommMessageIn *message_in()
    {
        MsgSession *session;

        //if (!this->is_open())
        //    return nullptr;

        CommMessageIn *msg = this->get_message_in();
        if (msg)
            return msg;

        session = this->new_msg_session();
        if (!session)
            return nullptr;
        
        session->set_state(WFC_MSG_STATE_IN);
        session->set_seq(this->msg_seq++);

        msg = session->get_msg();
        if (!msg)
            return nullptr;
        
        msg->seq     = session->get_seq();
        msg->session = session;
        return msg;
    }

    virtual CommMessageOut *message_out()
    {
        CommMessageOut *msg = nullptr;

        //std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
        if (!this->write_mutex.try_lock())
            return msg;

        if (!this->is_open())
            goto unlock_out;     
        
        if (this->write_list.size())
        {
            msg = this->write_list.front();
            this->write_list.pop_front();
        }

unlock_out:
        this->write_mutex.unlock();
        return msg;
    }

private:
    std::atomic<long long> msg_seq;
    std::atomic<long long> req_seq;
    std::atomic<int>       ref;

    std::atomic<bool> stop_flag{false};
    
public:
    // virtual MsgSession *new_msg_session() {return nullptr;};

    long long get_msg_seq()
    {
        return this->msg_seq;
    }

    long long get_req_seq()
    {
        return this->req_seq;
    }

    virtual int shutdown()
    {
        std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
        if (this->stop_flag) 
            return -1;
        
        this->get_scheduler()->channel_shutdown(this);
        return 0;
    }

    virtual bool is_open()
    {
        CommConnection *conn;
        
        // for performance
        if  (this->stop_flag)
            return false;;
        
        std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
        if (this->stop_flag) 
            return false;
        
        conn = this->get_connection();
        if (!conn || !conn->entry)
            return false;
        
        switch (conn->entry->state) {
        case CONN_STATE_ESTABLISHED:
        case CONN_STATE_CONNECTING:
            return true;
        default:
            return false;
        }
        
        return false;
    };

    virtual int incref()
    {
        int value = this->ref;

        while (value > 0 && !this->ref.compare_exchange_strong(value, value+1)) {
        }
        
        return value;
        //return this->ref.fetch_add(1);
    }
    
    virtual void decref(int skip_delete=0)
    {
        if (this->ref.fetch_sub(1) == 1 && !skip_delete)
            delete this;
    }

    virtual bool is_server()
    {
        return false;
    }
    virtual bool is_channel()
    {
        return true;
    }

private:
    using __MSG = std::pair<long long, MSG *>;
    class cmp
    {
    public:
        bool operator()(__MSG a, __MSG b)
        {
            return a.first > b.first;
        }
    };

    using __MSG_HEAP = std::priority_queue<__MSG, std::vector<__MSG>, cmp>;

    std::mutex       in_mutex;
    __MSG_HEAP       fanout_heap_in;
    std::list<MSG *> in_list;
    long long        in_list_seq = 0;
    WFResourcePool   in_msg_pool;

    std::mutex       out_mutex;
    __MSG_HEAP       fanout_heap_out;
    std::list<MSG *> out_list;
    long long        out_list_seq = 0;

    //std::recursive_mutex       write_mutex;
    std::list<MSG *> write_list;
   
public:
    virtual int process_msg(MSG *msg)
    {
        return 0;
    }
    
    virtual WFResourcePool* get_resource_pool() {
        return &this->in_msg_pool;
    }
    
    virtual int fanout_msg_in(MSG *in)
    {
        int ret;
        long long seq = in->get_seq();
        CommSession *cur_session = in->session;

        //if (!this->is_open())
        //    return -1;

        std::lock_guard<std::mutex> lck(this->in_mutex);
        assert(this->in_list_seq <= seq);

        fanout_heap_in.emplace(std::make_pair(seq, in));
        while (fanout_heap_in.top().first == this->in_list_seq)
        {
            auto x = fanout_heap_in.top();
            fanout_heap_in.pop();
            this->in_list_seq++;

            if (x.second)
                in_list.push_back(x.second);
        }

        while (in_list.size())
        {
            MSG *msg = in_list.front();
            in_list.pop_front();

            msg->session = cur_session;
            ret          = process_msg(msg);
            delete msg;
            if (ret < 0)
                break;
        }

        return 0;
    }

    virtual int fanout_msg_out(MSG *out)
    {
        int ret;
        long long seq = out->get_seq();
        std::lock_guard<std::mutex> lck(this->out_mutex);
        if (!this->is_open())
            return -1;

        assert(this->out_list_seq <= seq);
        fanout_heap_out.emplace(std::make_pair(seq, out));
        while (fanout_heap_out.top().first == this->out_list_seq)
        {
            auto x = fanout_heap_out.top();
            fanout_heap_out.pop();

            this->out_list_seq++;
            if (x.second)
                out_list.push_back(x.second);
        }

        while (out_list.size())
        {
            MSG *msg = out_list.front();
            out_list.pop_front();

            ret = this->msg_out(msg);
            if (ret < 0)
            {
                delete msg;
                break;
            }
        }

        return 0;
    }

    virtual int msg_out(MSG *out, int flag)
    {
        int ret;

        std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
        this->write_list.push_back(out);

        if (flag == WFC_MSG_STATE_OUT_LIST)
            return 0;

        if (!this->is_open()) {
            return 0;
        }
        
        ret = this->get_scheduler()->channel_send_one(this);
        if (ret < 0)
        {
            this->shutdown();
        }

        return 0;
    }

    virtual int msg_out(MSG *out)
    {
        return this->msg_out(out, WFC_MSG_STATE_OUT);
    }
    virtual int msg_out_list(MSG *out)
    {
        return this->msg_out(out, WFC_MSG_STATE_OUT_LIST);
    }

protected:
    /*for client*/
    explicit WFChannelImpl(int retry_max, channel_callback_t &&cb)
        : ChannelEntry(retry_max, std::move(cb)), in_msg_pool(1)
    {
        this->msg_seq   = 0;
        this->req_seq   = 0;
        this->ref       = 1;
        this->stop_flag = false;
        
        this->termination_cb = nullptr;
    }

    /*for server*/
    explicit WFChannelImpl(CommScheduler *scheduler, channel_callback_t &&cb)
        : ChannelEntry(nullptr, scheduler, std::move(cb)), in_msg_pool(1)
    {
        this->msg_seq   = 0;
        this->req_seq   = 0;
        this->ref       = 1;
        this->stop_flag = false;
        
        this->termination_cb = nullptr;
    }

private:
    std::function<void()> termination_cb;

public: 
    virtual void set_termination_cb(std::function<void()> bc) {
        this->termination_cb = std::move(bc);
    } 

protected:
    virtual void delete_this(void *t)
    {
        std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
        if(this->stop_flag.exchange(true))
            return;
        
        CommConnection **conn = this->get_conn();
        *conn = nullptr;

        CommMessageIn *in = this->get_message_in();
        if (in)
        {
            if (in->session)
            {
                delete in->session;
                in->session = nullptr;
            }
            // delete in;
            *(this->get_in()) = nullptr;
        }
       
        if (this->termination_cb)
            this->termination_cb();

        this->decref();
    }

    virtual WFConnection *get_connection() const
    {
        return (WFConnection *)this->CommSession::get_connection();
    }

    virtual ~WFChannelImpl()
    {
        while (!fanout_heap_in.empty())
        {
            auto _msg = fanout_heap_in.top();
            fanout_heap_in.pop();

            delete _msg.second;
        }

        for (auto x : in_list)
        {
            in_list.remove(x);
            delete x;
        }

        while (!fanout_heap_out.empty())
        {
            auto _msg = fanout_heap_out.top();
            fanout_heap_out.pop();

            delete _msg.second;
        }

        for (auto x : out_list)
        {
            out_list.remove(x);
            delete x;
        }

        for (auto x : write_list)
        {
            write_list.remove(x);
            delete x;
        }

        /* move to  delete_this
        CommMessageIn *in = this->get_message_in();
        if (in) {
            if (in->session) {
                delete in->session;
                in->session = nullptr;
            }
            // delete in;
            *(this->get_in()) = nullptr;
        }
        */

        CommMessageOut *out = this->get_message_out();
        if (out)
        {
            delete out;
            *(this->get_out()) = nullptr;
        }
    }
};

#include "WFChannel.inl"

#endif

