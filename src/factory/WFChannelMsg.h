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

template <typename protocolMsg> class WFChannelMsg : public ChannelMsg
{
public:
	void set_callback(std::function<void(WFChannelMsg<protocolMsg> *)> cb)
	{
		this->callback = std::move(cb);
	}

	protocolMsg *get_msg() { return static_cast<protocolMsg *>(this->ChannelMsg::get_msg()); }
	protocolMsg *pick_msg()
	{
		return static_cast<protocolMsg *>(this->ChannelMsg::pick_msg());
	}

private:
	std::function<void(WFChannelMsg<protocolMsg> *)> callback;
	std::function<void(WFChannelMsg<protocolMsg> *)> process;

protected:
	virtual SubTask *done()
	{
		SeriesWork *series = series_of(this);

		if (this->callback)
			this->callback(this);

		if (this->inner_callback)
			this->inner_callback(this);

		delete this;
		return series->pop();
	}

	virtual void dispatch()
	{
		if (this->get_state() > WFC_MSG_STATE_SUCCEED)
		{
			if (this->process)
				this->process(this);

			if (this->inner_process)
				this->inner_process(this);

			// this->channel_eat_msg();
		}

		this->subtask_done();
	}

	virtual void handle(int state, int error)
	{
		if (state == WFT_STATE_SUCCESS || state == WFT_STATE_TOREPLY)
		{
			this->start();
		}
		else
		{
			std::cout << "bug: WFChannelMsg<protocolMsg> handle state must is "
				     "WFT_STATE_SUCCESS/WFT_STATE_TOREPLY, other is Bug!!!"
				  << std::endl;
			delete this;
		}
	}

public:
	WFChannelMsg(WFChannel *channel,
		     std::function<void(WFChannelMsg<protocolMsg> *)> proc = nullptr)
		: WFChannelMsg<protocolMsg>(channel, new protocolMsg, std::move(proc))
	{
	}

	WFChannelMsg(WFChannel *channel, protocolMsg *msg,
		     std::function<void(WFChannelMsg<protocolMsg> *)> proc = nullptr)
		: ChannelMsg(channel, msg), process()
	{
	}

	virtual ~WFChannelMsg() { }
};

#endif // _FACTORY_WFCHANNELMSG_H_
