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
#include "WFChannel.h"

CommMessageIn *WFChannel::message_in() 
{
    if (this->stop_flag)
        return nullptr;

    CommMessageIn *msg = this->get_message_in();
    WFChannelMsgSession *session;

    if (msg)
        return msg;

    session = this->new_channel_msg_session();
    if (session) {
        msg = session->get_msg();
        session->set_state(WFC_MSG_STATE_IN);
    }

    if (msg) {
        msg->seq = this->msg_seq++;
        msg->session = session;
    }

    return msg;
}

CommMessageOut *WFChannel::message_out()
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


void WFComplexChannelClient::dispatch()
{
	if (this->object)
		return this->WFChannel::dispatch();

	if (this->state == WFT_STATE_UNDEFINED)
	{
		this->router_task = this->route();
		series_of(this)->push_front(this);
		series_of(this)->push_front(this->router_task);
	}

	this->subtask_done();
}

SubTask *WFComplexChannelClient::done()
{
	SeriesWork *series = series_of(this);

    if (this->state == WFT_STATE_SYS_ERROR) {
        this->ns_policy->failed(&this->route_result, NULL, this->target);
    } else
    {
        this->ns_policy->success(&this->route_result, NULL, this->target);
    }

	if (this->router_task)
	{
		this->router_task = NULL;
		return series->pop();
	}

	if (this->callback)
		this->callback(this);

    delete this;
	return series->pop();
}

WFRouterTask *WFComplexChannelClient::route()
{
	auto&& cb = std::bind(&WFComplexChannelClient::router_callback,
						  this, std::placeholders::_1);
	struct WFNSParams params = {
		.type			=	TT_TCP,
		.uri			=	this->uri,
		.info			=	"",
		.fixed_addr		=	true,
		.retry_times	=	0,
		.tracing		=	NULL,
	};

	WFNameService *ns = WFGlobal::get_name_service();
	this->ns_policy = ns->get_policy(this->uri.host ? this->uri.host : "");
	return this->ns_policy->create_router_task(&params, cb);
}

void WFComplexChannelClient::router_callback(WFRouterTask *task)
{
	if (task->get_state() == WFT_STATE_SUCCESS)
	{
		this->route_result = std::move(*task->get_result());
		this->set_request_object(this->route_result.request_object);
	}
	else
	{
		this->state = task->get_state();
		this->error = task->get_error();
	}
}

