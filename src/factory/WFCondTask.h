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
*/

#ifndef _WFCONDTASK_H_
#define _WFCONDTASK_H_

#include <bits/types/struct_timespec.h>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <utility>
#include "list.h"
#include "WFTask.h"
#include "WFCondition.h"

class WFCondWaitTask : public WFMailboxTask
{
protected:
	virtual SubTask *done();

private:
	struct list_head list;
    class WFCondition *cond;

private:
	void *msg;

public:
	explicit WFCondWaitTask(WFCondition *cond, wait_callback_t&& cb) :
		WFMailboxTask(&this->msg, 1, std::move(cb))
	{ 
        this->cond = cond;
        cond->add_waittask(this);
    }

	virtual ~WFCondWaitTask() { }

	friend class __WFWaitTimerTask;
	friend class WFCondition;
	friend class WFCondTaskFactory;
	friend class __ConditionMap;
};

class WFTimedWaitTask;

class __WFWaitTimerTask : public WFTimerTask
{
public:
    void clear_wait_task() // must called within this mutex
    {
        this->wait_task = NULL;
    }

protected:
    virtual int duration(struct timespec *value)
    {
        *value = this->timeout;
        return 0;
    }

    virtual SubTask *done();

protected:
    struct timespec timeout;

private:
    WFTimedWaitTask *wait_task;

public:
    __WFWaitTimerTask(WFTimedWaitTask *wait_task,
            const struct timespec *timeout,
            CommScheduler *scheduler) :
        WFTimerTask(scheduler, nullptr)
    {
        this->timeout = *timeout;
        this->wait_task = wait_task;
    }

    virtual ~__WFWaitTimerTask() {}
};

class WFTimedWaitTask : public WFCondWaitTask
{
public:
	void set_timer(__WFWaitTimerTask *timer) { this->timer = timer; }
	virtual void count();
	virtual void clear_timer();

protected:
	virtual void dispatch();
	SubTask* done();

private:
    __WFWaitTimerTask *timer;

public:
	explicit WFTimedWaitTask(WFCondition *cond, wait_callback_t&& cb, const timespec *timeout) :
		WFCondWaitTask(cond, std::move(cb))
	{
		this->timer = new __WFWaitTimerTask(this, timeout, WFGlobal::get_scheduler());
	}

    explicit WFTimedWaitTask(WFCondition *cond, wait_callback_t&& cb, int millisecond) :
		WFCondWaitTask(cond, std::move(cb))
	{
       struct timespec time;

       time.tv_sec = millisecond/1000;
       time.tv_nsec = (millisecond%1000)*1000*1000;

       WFTimedWaitTask(cond, std::move(cb), &time);
	}


	virtual ~WFTimedWaitTask();
};



#endif

