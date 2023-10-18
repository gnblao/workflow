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

#ifndef _MPOLLER_H_
#define _MPOLLER_H_

#include <stddef.h>

#include "poller.h"

typedef struct __mpoller mpoller_t;

#ifdef __cplusplus
extern "C"
{
#endif

mpoller_t *mpoller_create(const struct poller_params *params, size_t nthreads);
int mpoller_start(mpoller_t *mpoller);
void mpoller_stop(mpoller_t *mpoller);
void mpoller_destroy(mpoller_t *mpoller);

#ifdef __cplusplus
}
#endif

struct __mpoller
{
	void **nodes_buf;
	unsigned int nthreads;
	poller_t *poller[1];
};

static inline int mpoller_add(const struct poller_data *data, int timeout, mpoller_t *mpoller)
{
	int index = (unsigned int)data->fd % mpoller->nthreads;
	return poller_add(data, timeout, mpoller->poller[index]);
}

static inline int mpoller_del(int fd, mpoller_t *mpoller)
{
	int index = (unsigned int)fd % mpoller->nthreads;
	return poller_del(fd, mpoller->poller[index]);
}

static inline int mpoller_mod(const struct poller_data *data, int timeout, mpoller_t *mpoller)
{
	int index = (unsigned int)data->fd % mpoller->nthreads;
	return poller_mod(data, timeout, mpoller->poller[index]);
}

static inline int mpoller_set_timeout(int fd, int timeout, mpoller_t *mpoller)
{
	int index = (unsigned int)fd % mpoller->nthreads;
	return poller_set_timeout(fd, timeout, mpoller->poller[index]);
}

static inline int mpoller_add_timer(const struct timespec *value, void *context, void **timer,
				    int *index, unsigned long long *timerid, mpoller_t *mpoller)
{
	static volatile unsigned int n = 0;
	poller_timerid_t tid;

	tid.id_key = n++;
	tid.poller_id = tid.id_key % mpoller->nthreads;

	int ret = poller_add_timer(value, context, timer, &tid, mpoller->poller[tid.poller_id]);
	if (ret)
		return ret;

	if (index)
		*index = tid.poller_id;

	if (timerid)
		*timerid = tid.u64;

	return ret;
}

//static inline int mpoller_add_timer(const struct timespec *value, void *context,
//									void **timer, int *index,
//									mpoller_t *mpoller)
//{
//	static unsigned int n = 0;
//	*index = n++ % mpoller->nthreads;
//	return poller_add_timer(value, context, timer, NULL, mpoller->poller[*index]);
//}

static inline int mpoller_del_timer_byid(unsigned long long timerid, mpoller_t *mpoller)
{
	poller_timerid_t tid;
	tid.u64 = timerid;

	if (tid.bitmap_id > 0 && tid.poller_id < mpoller->nthreads)
	{
		return poller_del_timer_byid(tid, mpoller->poller[tid.poller_id]);
	}

	return -1;
}

static inline int mpoller_del_timer(void *timer, int index, mpoller_t *mpoller)
{
	return poller_del_timer(timer, mpoller->poller[index]);
}

#endif
