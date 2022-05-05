/*************************************************************************
    > File Name: WFChannelImpl.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2022年05月05日 星期四 11时37分35秒
 ************************************************************************/


#include "ProtocolMessage.h"
#include "WFChannel.h"
#include "WFTask.h"
#include <functional>
#include <utility>

using WFChannelClientBase = WFChannelImpl<WFComplexClientTask<protocol::ProtocolMessage, protocol::ProtocolMessage>>;
using WFChannelServerBase = WFChannelImpl<WFNetworkTask<protocol::ProtocolMessage, protocol::ProtocolMessage>>;

class WFChannelClient : public WFChannelClientBase
{
protected:
	virtual CommMessageOut *message_out()
	{
		/* By using prepare function, users can modify request after
		 * the connection is established. */
		if (this->prepare)
			this->prepare(this);

		return WFChannelClientBase::message_out();
	}

    virtual int first_timeout() {return this->receive_timeout();}

private:
    std::function<void (WFChannel *)> prepare;

public:
	void set_prepare(std::function<void (WFChannel*)> prep)
	{
		this->prepare = std::move(prep);
	}
    
public:
	WFChannelClient(int retry_max, task_callback_t&& cb): 
        WFChannelClientBase(retry_max, std::move(cb)) {}

protected:
	virtual ~WFChannelClient() {};
};

class WFChannelServer : public WFChannelServerBase
{
protected:
	virtual void dispatch()
	{
        this->subtask_done();
	}

    virtual void handle(int state, int error) {
        this->start();
    }
    
    virtual SubTask *done()
	{
        SeriesWork *series = series_of(this);
		if (this->callback)
			this->callback(this);

        this->delete_this(this);
		return series->pop();
	}
    

    virtual WFConnection *get_connection() const
	{
        return (WFConnection *)this->CommSession::get_connection();
	}

public:
    virtual bool is_server() {return true;}
	
    explicit WFChannelServer(CommScheduler *scheduler, CommService *service=nullptr, task_callback_t &&cb = nullptr):
        WFChannelServerBase(scheduler, std::move(cb)) {}

    virtual ~WFChannelServer() {};
};

