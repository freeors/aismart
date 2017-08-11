/* $Id: thread.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef THREAD_HPP_INCLUDED
#define THREAD_HPP_INCLUDED

#include "SDL.h"
#include "SDL_thread.h"

#include <list>

#include <boost/scoped_ptr.hpp>

#include "rtc_base/signalthread.h"
#include "rtc_base/bind.h"

class tworker;

namespace rtc {

class worker_thread: public SignalThread
{
public:
	worker_thread(tworker& worker)
		: worker_(worker)
	{}

protected:
	void DoWork() override;
	void OnWorkStart() override;
	void OnWorkDone() override;

protected:
	tworker& worker_;
};

}

class tworker
{
public:
	virtual void DoWork() = 0;
	virtual void OnWorkStart() = 0;
	virtual void OnWorkDone() = 0;

protected:
	tworker()
		: main_(rtc::Thread::Current())
		, thread_(new rtc::worker_thread(*this))
	{}
	virtual ~tworker()
	{
		thread_->Destroy(true);
		thread_ = NULL;
	}

protected:
	rtc::Thread* main_;
	rtc::worker_thread* thread_;
};

// Threading primitives wrapper for SDL_Thread.
//
// This module defines primitives for wrapping C++ around SDL's threading
// interface
namespace threading
{

// Binary mutexes.
//
// Implements an interface to binary mutexes. This class only defines the
// mutex itself. Locking is handled through the friend class lock,
// and monitor interfacing through condition variables is handled through
// the friend class condition.
class mutex
{
public:
	mutex();
	~mutex();

	friend class lock;
	friend class condition;

public:
	SDL_mutex* const m_;

private:
	mutex(const mutex&);
	void operator=(const mutex&);
};

// Binary mutex locking.
//
// Implements a locking object for mutexes. The creation of a lock
// object on a mutex will lock the mutex as long as this object is
// not deleted.
class lock
{
public:
	// Create a lock object on the mutex given as a parameter to
	// the constructor. The lock will be held for the duration
	// of the object existence.
	// If the mutex is already locked, the constructor will
	// block until the mutex lock can be acquired.
	//
	// \param m the mutex on which we should try to lock.
	explicit lock(mutex& m);
	// Delete the lock object, thus releasing the lock acquired
	// on the mutex which the lock object was created with.
	~lock();
private:
	lock(const lock&);
	void operator=(const lock&);

	mutex& m_;
};

// Condition variable locking.
//
// Implements condition variables for mutexes. A condition variable
// allows you to free up a lock inside a critical section
// of the code and regain it later. Condition classes only make
// sense to do operations on, if one already acquired a mutex.
class condition
{
public:
	condition();
	~condition();

	// Wait on the condition. When the condition is met, you
	// have a lock on the mutex and can do work in the critical
	// section. When the condition is not met, wait blocks until
	// the condition is met and atomically frees up the lock on
	// the mutex. One will automatically regain the lock when the
	// thread unblocks.
	//
	// If wait returns false we have an error. In this case one cannot
	// assume that he has a lock on the mutex anymore.
	//
	// \param m the mutex you wish to free the lock for
	// \returns true: the wait was successful, false: an error occurred
	//
	// \pre You have already acquired a lock on mutex m
	//
	bool wait(const mutex& m);

	enum WAIT_TIMEOUT_RESULT { WAIT_OK, WAIT_TIMED_OUT, WAIT_ERROR };

	// wait on the condition with a timeout. Basically the same as the
	// wait() function, but if the lock is not acquired before the
	// timeout, the function returns with an error.
	//
	// \param m the mutex you wish free the lock for.
	// \param timeout the allowed timeout in milliseconds (ms)
	// \returns result based on whether condition was met, it timed out,
	// or there was an error
	WAIT_TIMEOUT_RESULT wait_timeout(const mutex& m, unsigned int timeout);
	// signal the condition and wake up one thread waiting on the
	// condition. If no thread is waiting, notify_one() is a no-op.
	// Does not unlock the mutex.
	bool notify_one();

	// signal all threads waiting on the condition and let them contend
	// for the lock. This is often used when varying resource amounts are
	// involved and you do not know how many processes might continue.
	// The function should be used with care, especially if many threads are
	// waiting on the condition variable.
	bool notify_all();

private:
	condition(const condition&);
	void operator=(const condition&);

	SDL_cond* const cond_;
};

}

#endif
