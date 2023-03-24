

#include <mutex>
#include <time.h>
#include <string>
#include <functional>
#include "list.h"
#include "rbtree.h"
#include "WFTask.h"
#include "WFCondTask.h"
#include "WFTaskFactory.h"
#include "WFCondTaskFactory.h"
#include "WFCondition.h"
#include "WFGlobal.h"

SubTask *WFCondWaitTask::done()
{
	SeriesWork *series = series_of(this);

	WFTimerTask *switch_task = WFTaskFactory::create_timer_task(0,
		[this](WFTimerTask *task) {
			if (this->callback)
				this->callback(this);
			delete this;
	});
	series->push_front(switch_task);

	return series->pop();
}

SubTask* __WFWaitTimerTask::done()
{
	WFTimedWaitTask *waiter = NULL;

	if (this->wait_task)
	{
		this->wait_task->state = WFT_STATE_SYS_ERROR;
		this->wait_task->error = ETIMEDOUT;
		waiter = this->wait_task;
		waiter->set_timer(NULL);
	}

	if (waiter)
		waiter->count();
	delete this;
	return NULL;
}

void WFTimedWaitTask::clear_timer()
{
	if(this->timer)
        this->timer->clear_wait_task();
	this->timer = NULL;
}

void WFTimedWaitTask::count()
{
    if (this->flag.exchange(true))
	{
		if (this->state == WFT_STATE_UNDEFINED)
			this->state = WFT_STATE_SUCCESS;
		this->subtask_done();
	}
}

SubTask* WFTimedWaitTask::done()
{
    // timeout
    if (this->state == WFT_STATE_SYS_ERROR)
        this->cond->del_waittask(this);
    else    
        clear_timer();

    return this->WFCondWaitTask::done();
}

void WFTimedWaitTask::dispatch()
{
	if (this->timer)
		timer->dispatch();

	//this->WFMailboxTask::count();
}

WFTimedWaitTask::~WFTimedWaitTask()
{
	if (this->state != WFT_STATE_SUCCESS)
		delete this->timer;
}


