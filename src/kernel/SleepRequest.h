/*
  Copyright (c) 2019 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Xie Han (xiehan@sogou-inc.com)
*/

#ifndef _SLEEPREQUEST_H_
#define _SLEEPREQUEST_H_

#include <atomic>
#include <errno.h>
#include "SubTask.h"
#include "Communicator.h"
#include "CommScheduler.h"

class SleepRequest : public SubTask, public SleepSession
{
public:
    SleepRequest(CommScheduler *scheduler)
    {
        this->scheduler = scheduler;
        this->disrupted = false;
    }

public:
    virtual void dispatch()
    {
        if (this->scheduler->sleep(this) < 0)
            this->handle(SS_STATE_ERROR, errno);
    }

    virtual void unsleep()
    {
        if (!this->disrupted.exchange(true)) {
            this->scheduler->unsleep(this);

            this->state = SS_STATE_DISRUPTED;
            this->error = 0;
            this->subtask_done();
        }
    }

protected:
    int state;
    int error;

private:
    std::atomic<bool> disrupted;

protected:
    CommScheduler *scheduler;

protected:
    virtual void handle(int state, int error)
    {
        if (!this->disrupted.exchange(true)) {
            this->state = state;
            this->error = error;
            this->subtask_done();
        }
    }
};

#endif

