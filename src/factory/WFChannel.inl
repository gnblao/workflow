/*
  Copyright (c) 2021 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Li Yingxin (liyingxin@sogou-inc.com)
          Xie Han (xiehan@sogou-inc.com)
*/


class WFChannelClient : public WFChannel
{
protected:
	virtual WFConnection *get_connection() const
	{
		CommConnection *conn;

		if (this->target)
		{
			conn = this->CommSession::get_connection();
			if (conn)
				return (WFConnection *)conn;
		}

		errno = ENOTCONN;
		return NULL;
	}
    
	virtual CommMessageOut *message_out()
	{
		/* By using prepare function, users can modify request after
		 * the connection is established. */
		if (this->prepare)
			this->prepare(this);

		return WFChannel::message_out();
	}

    virtual int first_timeout() {return this->receive_timeout();}
public:
	void set_prepare(std::function<void (WFChannelClient*)> prep)
	{
		this->prepare = std::move(prep);
	}

protected:
	std::function<void (WFChannelClient *)> prepare;

public:
	WFChannelClient(CommSchedObject *object, CommScheduler *scheduler):
        WFChannel(object, scheduler)
	{
	}

protected:
	virtual ~WFChannelClient() {};
};

class WFChannelServer : public WFChannel
{
protected:
	/* CommSession::get_connection() is supposed to be called only in the
	 * implementations of it's virtual functions. As a server task, to call
	 * this function after process() and before callback() is very dangerous
	 * and should be blocked. */
	virtual WFConnection *get_connection() const
	{
        return (WFConnection *)this->CommSession::get_connection();
	}

protected:
	virtual void dispatch()
	{
        this->subtask_done();
	}

public:
	WFChannelServer(CommService *service, CommScheduler *scheduler):
		WFChannel(NULL, scheduler)
	{
	}
	
    virtual ~WFChannelServer() {};
};


/**********WFComplexChannelClient for sequentially establish and send**********/

class WFComplexChannelClient : public WFChannelClient
{
public:
	void set_uri(const ParsedURI& uri) { this->uri = uri; }
	const ParsedURI *get_uri() const { return &this->uri; }

protected:
	virtual void dispatch();
	virtual SubTask *done();
	//virtual void handle_terminated();
	virtual WFRouterTask *route();
	void router_callback(WFRouterTask *task);

protected:
	WFRouterTask *router_task;
	ParsedURI uri;
	WFNSPolicy *ns_policy;
	RouteManager::RouteResult route_result;

public:
	WFComplexChannelClient(CommSchedObject *object, CommScheduler *scheduler) :
		WFChannelClient(object, scheduler)
	{
		this->state = WFT_STATE_UNDEFINED;
		this->error = 0;
	}

protected:
	virtual ~WFComplexChannelClient() { }
};


