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

#include <mutex>
#include "list.h"
#include "WFTask.h"
#include "WFCondTask.h"
#include "WFCondition.h"

void WFCondition::signal(void *msg)
{
	WFCondWaitTask *task = NULL;

	this->mutex.lock();
	if (!list_empty(&this->wait_list))
	{
		task = list_entry(this->wait_list.next, WFCondWaitTask, list);
		list_del(&task->list);
	}

	this->mutex.unlock();
	if (task)
		task->send(msg);
}

void WFCondition::broadcast(void *msg)
{
	WFCondWaitTask *task;
	struct list_head *pos, *tmp;
	LIST_HEAD(tmp_list);

	this->mutex.lock();
	if (!list_empty(&this->wait_list))
	{
		list_for_each_safe(pos, tmp, &this->wait_list)
		{
			list_move_tail(pos, &tmp_list);
			task = list_entry(pos, WFCondWaitTask, list);
		}
	}

	this->mutex.unlock();
	while (!list_empty(&tmp_list))
	{
		task = list_entry(tmp_list.next, WFCondWaitTask, list);
		list_del(&task->list);
		task->send(msg);
	}
}

bool WFCondition::add_waittask(WFCondWaitTask *task) 
{
    this->mutex.lock();
    list_add_tail(&task->list, &this->wait_list);
    this->ref++;
    this->mutex.unlock();
    return true;
}

bool WFCondition::del_waittask(WFCondWaitTask *task)
{
    assert(task->cond == this);

    this->mutex.lock();
    list_del(&task->list);
    this->ref--;
    this->mutex.unlock();

    return true;
}

WFCondition::~WFCondition()
{
	if (this->ref > 0)
        this->broadcast(NULL);
}

