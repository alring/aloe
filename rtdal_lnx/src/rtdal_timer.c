/* 
 * Copyright (c) 2012, Ismael Gomez-Miguelez <ismael.gomez@tsc.upc.edu>.
 * This file is part of ALOE++ (http://flexnets.upc.edu/)
 * 
 * ALOE++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ALOE++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with ALOE++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <error.h>

#ifdef EN_TIMERFD
#include <sys/timerfd.h>
#endif

#include "defs.h"
#include "rtdal_timer.h"
#include "rtdal.h"
#include "futex.h"

extern int timeslot;

/***
 *
 * Timer api based on:
 *
 * http://www.embedded-linux.co.uk/tutorial/periodic_threads
 *
 * @todo: compare performance with posix timer
 */
#ifdef EN_TIMERFD
void timer_close(rtdal_timer_t *info) {
	close(info->timer_fd);
}

int timer_setup (rtdal_timer_t *info)
{
	long int ns;
	__time_t sec;
	int fd;

	if (info->period<=0) {
		aerror_msg("Invalid period %d\n", (int) info->period);
		return -1;
	}

	/* Create the timer */
	fd = timerfd_create (CLOCK_REALTIME, 0);
	if (fd == -1) {
		perror("timerfd_create");
		return -1;
	}

	info->wakeups_missed = 0;
	info->timer_fd = fd;

	/* Make the timer periodic */
	sec = info->period/1000000000;
	ns = (info->period - (sec * 1000000));
	info->itval.it_interval.tv_sec = sec;
	info->itval.it_interval.tv_nsec = ns;
	info->itval.it_value.tv_sec = sec;
	info->itval.it_value.tv_nsec = ns;

	return 0;
}

inline int timer_start(rtdal_timer_t *info) {
	int ret;

	ret = timerfd_settime (info->timer_fd, 0, &info->itval, NULL);
	if (ret == -1) {
		perror("timerfd_settime");
		return -1;
	}
	return 0;
}

inline int timer_wait_period (rtdal_timer_t *info)
{
	unsigned long long missed;
	int ret;

	/* Wait for the next timer event. If we have missed any the
	   number is written to "missed" */
	ret = read (info->timer_fd, &missed, sizeof (missed));
	if (ret == -1)
	{
		perror ("read timer");
		return ret;
	}

	/* "missed" should always be >= 1, but just to be sure, check it is not 0 anyway */
	if (missed > 0)
		info->wakeups_missed += (missed - 1);

	return 0;
}
#endif
void* timer_run_thread(void* x) {
	rtdal_timer_t *obj = (rtdal_timer_t*) x;

	if (DEBUG_rtdal) {
		rtdal_task_print_sched();
	}

	switch(obj->mode) {
	case XENOMAI:
#ifdef __XENO__
		return xenomai_timer_run_thread(obj);
#else
		aerror("Not compiled with xenomai support\n");
		return NULL;
#endif
	case NANOSLEEP:
		return nanoclock_timer_run_thread(obj);
#ifdef EN_TIMERFD
	case TIMERFD:
		return timerfd_timer_run_thread(obj);
#endif
	default:
		aerror_msg("Unknown timer mode %d\n", obj->mode);
		return NULL;
	}
}
#ifdef EN_TIMERFD

void* timerfd_timer_run_thread(rtdal_timer_t* obj) {
	int s;

	assert(obj->period_function);

	if (timer_setup(obj)) {
		aerror("Setting up timer");
		goto timer_destroy;
	}

	if (obj->wait_futex) {
		futex_wait(obj->wait_futex);
	}
	if (timer_start(obj)) {
		aerror("timer_start");
		goto timer_destroy;
	}

	obj->stop = 0;
	while(!obj->stop) {
		if (timer_wait_period(obj)) {
			aerror("waiting for timer");
			goto timer_destroy;
		}
		obj->period_function(obj->arg, NULL);
	}

	s = 0;

timer_destroy:
	timer_close(obj);
	pthread_exit(&s);
	return NULL;
}
#endif
inline static void timespec_add_us(struct timespec *t, long int us) {
	t->tv_nsec += us;
	if (t->tv_nsec >= 1000000000) {
		t->tv_nsec = t->tv_nsec - 1000000000;
		t->tv_sec += 1;
	}
}
void timespec_sub_us(struct timespec *t, long int us) {
	t->tv_nsec -= us;
	if (t->tv_nsec < 0) {
		t->tv_nsec = t->tv_nsec + 1000000000;
		t->tv_sec -= 1;
	}
}

void* nanoclock_timer_run_thread(rtdal_timer_t* obj) {
	int s;
	int n;
	assert(obj->period_function);

	obj->stop = 0;
	if (obj->wait_futex) {
		futex_wait(obj->wait_futex);
		obj->next.tv_sec+=TIMER_FUTEX_GUARD_SEC;
		obj->next.tv_nsec=0;
	} else {
		clock_gettime(CLOCK_REALTIME, &obj->next);
	}
	n=0;
	while(!obj->stop) {
		timespec_add_us(&obj->next, obj->period);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME,
				&obj->next, NULL);
		
		n++;
		if (n>=obj->multiple) {
			obj->period_function(obj->arg, &obj->next);
			n=0;
		}
	}

	s = 0;
	pthread_exit(&s);
	return NULL;
}

#ifdef __XENO__
void *xenomai_timer_run_thread(rtdal_timer_t *obj) {
	struct timespec period,start;
	unsigned long overruns;
	int s;
	int n;
	unsigned int tscnt=0;
	struct timespec test;
		
	rtdal_task_print_sched();
	
	period.tv_sec=0;
	period.tv_nsec=obj->period;
	obj->stop = 0;
	if (obj->wait_futex) {
		futex_wait(obj->wait_futex);
		obj->next.tv_sec+=3;
		obj->next.tv_nsec=0;
	} else {
		clock_gettime(CLOCK_REALTIME, &obj->next);
		obj->next.tv_sec++;
	}
	s=pthread_make_periodic_np(pthread_self(), &obj->next, &period);
	if (s) {
		printf("error %d\n",s);
		return NULL;
	}
	printf("go!\n");
	n=0;
	while(!obj->stop) {
		overruns=0;
		if ((s=pthread_wait_np(&overruns))) {
			if (s!=110)
				printf("error2 %d\n",s);
		}
		if (overruns>0) {
			printf("[%d] %d overrun\n",tscnt, overruns);
		}
		n++;
		if (n>=obj->multiple) {
			obj->period_function(obj->arg, &obj->next);
			n=0;
		}
	}
	s=0;
	pthread_exit(&s);
	return NULL;
}
#endif
