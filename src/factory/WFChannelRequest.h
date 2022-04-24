/*************************************************************************
    > File Name: WFChannelRequest.h
    > Author: 
    > Mail: 
    > Created Time: 2022年04月17日 星期日 15时09分18秒
 ************************************************************************/

#ifndef _SRC_FACTORY_WFCHANNELREQUEST_H_
#define _SRC_FACTORY_WFCHANNELREQUEST_H_


#include <atomic>
#include <cassert>
#include <functional>
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
#include "WFChannel.h"

template<typename REQ, typename RSP>
class WFChannelRequest : public SubTask, public CommSession
{
static_assert(std::is_base_of<protocol::ProtocolMessage, REQ>::value, "ProtocolMessage must is base of REQ");
static_assert(std::is_base_of<protocol::ProtocolMessage, RSP>::value, "ProtocolMessage must is base of RSP");
public:
	void start()
	{
		assert(!series_of(this));
		Workflow::start_series_work(this, nullptr);
	}

	void dismiss()
	{
		assert(!series_of(this));
		delete this;
	}

public:
    WFChannelRequest(WFChannel *channel): 
        state(WFC_MSG_STATE_UNDEFINED), error(0)
    {
        this->channel = channel;
    }

    WFChannel *get_channel() const { return this->channel; }
	void set_channel(WFChannel *channel) { this->channel = channel; }

    long long get_state() const { return this->state; }
    void set_state(int state) { this->state = state; }
	
	virtual REQ *get_req()
	{
		return &this->req;
	}

	void set_callback(std::function<void (WFChannelRequest<REQ, RSP> *)> cb)
	{
		this->callback = std::move(cb);
	}

	enum {
        WFC_MSG_STATE_UNDEFINED,
        WFC_MSG_STATE_TOREPLY,
        WFC_MSG_STATE_TOUPPER,
    };

protected:
    int state;
	int error;

private:
	REQ req;
	RSP rsp;

public:
	void *user_data;

protected:
	WFChannel *channel;

	std::function<void (WFChannelRequest<REQ, RSP> *)> callback;

private:
	virtual CommMessageOut *message_out()
	{
		errno = ENOSYS;
		return NULL;
	}

	virtual CommMessageIn *message_in()
	{
		errno = ENOSYS;
		return NULL;
	}

protected:
	virtual void dispatch() {
        this->subtask_done();
    }

    virtual SubTask *done()
	{
		SeriesWork *series = series_of(this);

		if (this->callback)
			this->callback(this);
		
        delete this;
		return series->pop();
	}

	virtual void handle(int state, int error)
	{
		this->state = state;
		this->error = error;
		this->subtask_done();
	}
};


#endif  // SRC_FACTORY_WFCHANNELREQUEST_H_
