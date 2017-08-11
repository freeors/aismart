/* $Id: thread.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

#include "global.hpp"

#include <vector>

#include "thread.hpp"

namespace rtc {
void worker_thread::DoWork() { worker_.DoWork(); }
void worker_thread::OnWorkStart() { worker_.OnWorkStart(); }
void worker_thread::OnWorkDone() { worker_.OnWorkDone(); }
}

namespace threading {

mutex::mutex() : m_(SDL_CreateMutex())
{}

mutex::~mutex()
{
	SDL_DestroyMutex(m_);
}

lock::lock(mutex& m) : m_(m)
{
	SDL_mutexP(m_.m_);
}

lock::~lock()
{
	SDL_mutexV(m_.m_);
}

condition::condition() : cond_(SDL_CreateCond())
{}

condition::~condition()
{
	SDL_DestroyCond(cond_);
}

bool condition::wait(const mutex& m)
{
	return SDL_CondWait(cond_,m.m_) == 0;
}

condition::WAIT_TIMEOUT_RESULT condition::wait_timeout(const mutex& m, unsigned int timeout)
{
	const int res = SDL_CondWaitTimeout(cond_,m.m_,timeout);
	switch(res) {
		case 0: return WAIT_OK;
		case SDL_MUTEX_TIMEDOUT: return WAIT_TIMED_OUT;
		default:
			// SDL_CondWaitTimeout: $SDL_GetError()
			return WAIT_ERROR;
	}
}

bool condition::notify_one()
{
	if(SDL_CondSignal(cond_) < 0) {
		// SDL_CondSignal: $SDL_GetError()
		return false;
	}

	return true;
}

bool condition::notify_all()
{
	if(SDL_CondBroadcast(cond_) < 0) {
		// SDL_CondBroadcast: $SDL_GetError()
		return false;
	}
	return true;
}

}
