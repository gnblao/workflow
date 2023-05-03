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
#include "Workflow.h"

enum
{
	WFC_MSG_STATE_ERROR = -1,
	WFC_MSG_STATE_SUCCEED = 0,
	WFC_MSG_STATE_IN = 1,
	WFC_MSG_STATE_OUT = 2,
	WFC_MSG_STATE_OUT_LIST = 3,
	WFC_MSG_STATE_OUT_WRITE_LIST = 4, // only for client first msg
};

class MsgSession : public CommSession
{
public:
	virtual protocol::ProtocolMessage *get_msg() = 0;
	virtual int get_state() const = 0;
	virtual void set_state(int state) = 0;

	MsgSession() : CommSession() { }
	virtual ~MsgSession(){};

private:
	virtual CommMessageOut *message_out() final
	{
		errno = ENOSYS;
		return NULL;
	}

	virtual CommMessageIn *message_in() final
	{
		errno = ENOSYS;
		return NULL;
	}
};

class ChannelMsg;

class WFChannel
{
protected:
	using MSG = protocol::ProtocolMessage;

public:
	using Channel = WFNetworkTask<MSG, MSG>;

public:
	virtual MsgSession *new_msg_session() = 0;

	virtual bool is_open(int is_deep = 0) = 0;
	virtual int shutdown() = 0;

	virtual int incref() = 0;
	virtual void decref() = 0;

	virtual int send_channel_msg(ChannelMsg *task) = 0;
	virtual int recv_channel_msg(ChannelMsg *task) = 0;

	virtual bool is_server() = 0;
	virtual void set_delete_cb(std::function<void()>) = 0;
	virtual int send(void *buf, size_t size) = 0;

protected:
	// synchronization for channel and CommConnEntry
	std::recursive_mutex write_mutex;

	std::atomic<long long> msg_in_seq;
	std::atomic<long long> msg_out_seq;

public:
	long long get_msg_in_seq() { return this->msg_in_seq; }
	long long get_msg_out_seq() { return this->msg_out_seq; }

	template <typename CMsgEntry = ChannelMsg>
	CMsgEntry *safe_new_channel_msg(int state = WFC_MSG_STATE_OUT)
	{
		// Atomic this->ref to protect new(CMsgEntry) ctx
		// in the active sending scenario
		{
			std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
			if (!this->is_open())
				return nullptr;

			if (this->incref() <= 0)
			{
				return nullptr;
			}
		}

		// now is safe new
		auto task = new CMsgEntry(this);
		this->decref();

		if (!task)
			return nullptr;

		if (state == WFC_MSG_STATE_IN)
			task->set_seq(this->msg_in_seq++);
		else
			task->set_seq(this->msg_out_seq++);

		task->set_state(state);
		return task;
	}
};

class ChannelMsg : public SubTask, public MsgSession
{
private:
	using MSG = protocol::ProtocolMessage;

public:
	void start()
	{
		if (!this->channel)
		{
			delete this;
			return;
		}

		switch (this->state)
		{
		case WFC_MSG_STATE_IN:
			this->channel->recv_channel_msg(this);
			break;
		case WFC_MSG_STATE_OUT:
		case WFC_MSG_STATE_OUT_LIST:
		case WFC_MSG_STATE_OUT_WRITE_LIST:
			this->channel->send_channel_msg(this);
			break;
		default:
			delete this;
			break;
		}
	}

public:
	explicit ChannelMsg(WFChannel *channel, MSG *msg)
		: inner_callback(nullptr), state(WFC_MSG_STATE_OUT), error(0)
	{
		assert(channel);
		assert(msg);
		this->msg = msg;

		if (channel->incref() > 0)
		{
			this->channel = channel;
		}
		else
		{
			this->channel = nullptr;
			std::cout << "!!! channel has been shut down !!!! "
				  << "The context of the new object (WFChannelMsg<XXX>) is risky "
				     "!!!! "
				  << "please using the safe_new_channel_msg function to new a "
				     "object on the channel"
				  << std::endl;
		}
	}

	virtual ~ChannelMsg()
	{
		if (this->channel)
			this->channel->decref();

		if (this->msg)
			delete this->msg;
	}

	WFChannel *get_channel() const { return this->channel; }

	virtual int get_state() const { return this->state; }
	virtual void set_state(int state) { this->state = state; }

public:
	virtual MSG *get_msg() { return this->msg; }
	virtual MSG *pick_msg()
	{
		MSG *m = this->msg;
		this->msg = nullptr;
		return m;
	}

public:
	void set_inner_callback(std::function<void(ChannelMsg *)> cb)
	{
		this->inner_callback = std::move(cb);
	}

	void set_inner_process(std::function<void(ChannelMsg *)> cb) { this->inner_process = cb; }

protected:
	std::function<void(ChannelMsg *)> inner_callback;
	std::function<void(ChannelMsg *)> inner_process;

protected:
	int state;
	int error;

private:
	MSG *msg;

protected:
	WFChannel *channel;
};

template <typename ChannelEntry = WFChannel::Channel>
class WFChannelImpl : public ChannelEntry, public WFChannel
{
private:
	static_assert(
		std::is_base_of<Channel, ChannelEntry>::value,
		"WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>> must is "
		"base of ChannelEntry");

protected:
	using channel_callback_t = std::function<void(Channel *)>;

	virtual CommMessageIn *message_in()
	{
		MsgSession *session;

		CommMessageIn *msg = this->get_message_in();
		if (msg)
			return msg;

		session = this->new_msg_session();
		if (!session)
			return nullptr;

		msg = session->get_msg();
		if (!msg)
			return nullptr;

		msg->session = session;
		return msg;
	}

	virtual CommMessageOut *message_out()
	{
		CommMessageOut *msg = nullptr;

		std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
		if (!this->is_open())
			return msg;

		if (this->write_list.size())
		{
			msg = this->write_list.front();
			this->write_list.pop_front();
		}

		return msg;
	}

private:
	std::atomic<int> ref;
	std::atomic<bool> stop_flag{false};

public:
	virtual int shutdown()
	{
		std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
		if (this->stop_flag)
			return -1;

		this->get_scheduler()->channel_shutdown(this);
		return 0;
	}

	virtual bool is_open(int is_deep = 0)
	{
		CommConnection *conn;

		// for performance
		if (this->stop_flag)
			return false;

		if (!is_deep)
			return true;

		std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
		if (this->stop_flag)
			return false;

		conn = this->get_connection();
		if (!conn || !conn->entry)
			return false;

		// switch (conn->entry->state)
		//{
		// case CONN_STATE_ESTABLISHED:
		// case CONN_STATE_CONNECTING:
		//     return true;
		// default:
		//     return false;
		// }
		// return false;

		return true;
	};

	virtual int incref()
	{
		int value = this->ref;

		while (value > 0 && !this->ref.compare_exchange_strong(value, value + 1))
		{
		}

		return value;
	}

	virtual void decref()
	{
		if (this->ref.fetch_sub(1) == 1)
			delete this;
	}

	virtual bool is_server() { return false; }

	virtual bool is_channel() { return true; }

public:
	virtual int process_msg(MSG *msg) { return 0; }

public:
	virtual int recv_channel_msg(ChannelMsg *task)
	{
		auto *pool = this->get_in_resource_pool();
		if (pool)
		{
			auto *cond = pool->get(task);
			if (cond)
			{
				task->set_inner_process(
					std::bind(&WFChannelImpl<ChannelEntry>::channel_eat_msg,
						  this, std::placeholders::_1));
				task->set_inner_callback([pool](ChannelMsg *)
							 { pool->post(nullptr); });

				cond->start();
			}
		}

		return 0;
	}

	virtual int send_channel_msg(ChannelMsg *task)
	{
		auto *pool = this->get_out_resource_pool();
		if (pool)
		{
			auto *cond = pool->get(task);
			if (cond)
			{
				task->set_inner_process(
					std::bind(&WFChannelImpl<ChannelEntry>::channel_eat_msg,
						  this, std::placeholders::_1));
				task->set_inner_callback([pool](ChannelMsg *)
							 { pool->post(nullptr); });
				cond->start();
			}
		}

		return 0;
	}

protected:
	/*for client*/
	explicit WFChannelImpl(int retry_max, channel_callback_t &&cb)
		: ChannelEntry(retry_max, std::move(cb)), in_msg_pool(1), out_msg_pool(1)
	{
		this->msg_in_seq = 0;
		this->msg_out_seq = 0;
		this->ref = 1;
		this->stop_flag = false;

		this->delete_callback = nullptr;
	}

	/*for server*/
	explicit WFChannelImpl(CommScheduler *scheduler, channel_callback_t &&cb)
		: ChannelEntry(nullptr, scheduler, std::move(cb)), in_msg_pool(1), out_msg_pool(1)
	{
		this->msg_in_seq = 0;
		this->msg_out_seq = 0;
		this->ref = 1;
		this->stop_flag = false;

		this->delete_callback = nullptr;
	}

private:
	using __MSG = std::pair<long long, MSG *>;
	class cmp
	{
	public:
		bool operator()(__MSG a, __MSG b) { return a.first > b.first; }
	};

	using __MSG_HEAP = std::priority_queue<__MSG, std::vector<__MSG>, cmp>;

private:
	WFResourcePool in_msg_pool;
	//    std::mutex       in_mutex;
	__MSG_HEAP fanout_heap_in;
	std::list<MSG *> heap_in_list;
	long long heap_in_seq = 0;

	WFResourcePool out_msg_pool;
	//    std::mutex       out_mutex;
	__MSG_HEAP fanout_heap_out;
	std::list<MSG *> heap_out_list;
	long long heap_out_seq = 0;

	// std::recursive_mutex       write_mutex;
	std::list<MSG *> write_list;

private:
	WFResourcePool *get_in_resource_pool() { return &this->in_msg_pool; }

	WFResourcePool *get_out_resource_pool() { return &this->out_msg_pool; }

	int fanout_msg_in(MSG *in)
	{
		int ret;
		long long seq = in->get_seq();
		CommSession *cur_session = in->session;

		// std::lock_guard<std::mutex> lck(this->in_mutex);
		assert(this->heap_in_seq <= seq);

		fanout_heap_in.emplace(std::make_pair(seq, in));
		while (fanout_heap_in.top().first == this->heap_in_seq)
		{
			auto x = fanout_heap_in.top();
			fanout_heap_in.pop();
			this->heap_in_seq++;

			if (x.second)
				heap_in_list.push_back(x.second);
		}

		while (heap_in_list.size())
		{
			MSG *msg = heap_in_list.front();

			msg->session = cur_session;
			ret = process_msg(msg);
			if (ret < 0)
				break;

			heap_in_list.pop_front();
		}

		return 0;
	}

	int fanout_msg_out(MSG *out, int state = WFC_MSG_STATE_OUT)
	{
		int ret;
		long long seq = out->get_seq();
		// std::lock_guard<std::mutex> lck(this->out_mutex);

		if (state == WFC_MSG_STATE_OUT_LIST)
		{
			heap_out_list.push_back(out);
			out = nullptr;
		}

		if (state == WFC_MSG_STATE_OUT_WRITE_LIST)
		{
			this->msg_out(out, state);
			out = nullptr;
		}

		assert(this->heap_out_seq <= seq);
		fanout_heap_out.emplace(std::make_pair(seq, out));
		while (fanout_heap_out.top().first == this->heap_out_seq)
		{
			auto x = fanout_heap_out.top();
			fanout_heap_out.pop();

			this->heap_out_seq++;
			if (x.second)
				heap_out_list.push_back(x.second);
		}

		while (heap_out_list.size())
		{
			MSG *msg = heap_out_list.front();

			ret = this->msg_out(msg);
			if (ret < 0)
			{
				break;
			}

			heap_out_list.pop_front();
		}

		return 0;
	}

	int msg_out(MSG *out, int flag = WFC_MSG_STATE_OUT)
	{
		int ret;

		std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
		this->write_list.push_back(out);

		if (flag == WFC_MSG_STATE_OUT_WRITE_LIST)
			return 0;

		if (!this->is_open(1))
		{
			return 0;
		}

		ret = this->get_scheduler()->channel_send_one(this);
		if (ret < 0)
		{
			this->shutdown();
		}

		return 0;
	}

private:
	virtual void channel_eat_msg(ChannelMsg *task)
	{
		int ret = -1;
		int state;
		MSG *msg = task->pick_msg();
		msg->set_seq(task->get_seq());

		state = task->get_state();
		switch (state)
		{
		case WFC_MSG_STATE_IN:
			ret = this->fanout_msg_in(msg);
			break;
		case WFC_MSG_STATE_OUT:
		case WFC_MSG_STATE_OUT_LIST:
		case WFC_MSG_STATE_OUT_WRITE_LIST:
			ret = this->fanout_msg_out(msg, state);
			// ret = this->msg_out(msg, state);
			break;
		default:
			break;
		}

		if (ret < 0)
		{
			task->set_state(WFC_MSG_STATE_ERROR);
			delete msg;
		}
		else
		{
			task->set_state(WFC_MSG_STATE_SUCCEED);
		}
	}

private:
	std::function<void()> delete_callback;

public:
	virtual void set_delete_cb(std::function<void()> bc)
	{
		this->delete_callback = std::move(bc);
	}

protected:
	virtual WFConnection *get_connection() const
	{
		return (WFConnection *)this->CommSession::get_connection();
	}

	virtual void delete_this(void *t)
	{
		std::lock_guard<std::recursive_mutex> lck(this->write_mutex);
		if (this->stop_flag.exchange(true))
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

		if (this->delete_callback)
			this->delete_callback();

		this->decref();
	}

	virtual ~WFChannelImpl()
	{
		while (!fanout_heap_in.empty())
		{
			auto _msg = fanout_heap_in.top();
			fanout_heap_in.pop();

			delete _msg.second;
		}

		for (auto x : heap_in_list)
		{
			heap_in_list.remove(x);
			delete x;
		}

		while (!fanout_heap_out.empty())
		{
			auto _msg = fanout_heap_out.top();
			fanout_heap_out.pop();

			delete _msg.second;
		}

		for (auto x : heap_out_list)
		{
			heap_out_list.remove(x);
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

