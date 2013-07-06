/**
 * @file aicurltimer.cpp
 *
 * Copyright (c) 2013, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   29/06/2013
 *   - Initial version, written by Aleric Inglewood @ SL
 */

// An AICurlTimer object provides a callback API for timer events
// by and from the curl thread. It is NOT threadsafe. Everything
// has to be called from the curl thread.
//
// Typical usage:
//
// AICurlTimer timer;
// ...
// // In curl thread (after successful construction is guaranteed).
// timer.create(5.5, boost::bind(&the_callback, <optional params>));	// Call the_callback(<optional params>) in 5.5 seconds.
//
// The callback function is always called by the curl thread.
//
// If timer.cancel() is called before the timer expires, then the callback
// function isn't called. Calling cancel() multiple times is not a problem.
// Note: It is not allowed to call cancel() is from the callback function
// (and rather pointless).
//
// The timer object can be reused (by calling create() again), but
// only after either the callback function was called, or after cancel()
// returned. Most notably, you can call create() again from inside the
// callback function to "restart" the timer.
//

#include "aicurltimer.h"
#include "lltimer.h"

static U64 const NEVER = ((U64)1) << 60;	// The year 36,560,871.

//static
F64 const AICurlTimer::sClockWidth_1ms = 1000.0 / calc_clock_frequency();       // Time between two clock ticks, in 1ms units.
U64 AICurlTimer::sTime_1ms;														// Time in 1ms units, set once per select() entry.
U64 AICurlTimer::sNextExpiration;
AICurlTimer::timer_list_type AICurlTimer::sTimerList;

void AICurlTimer::create(deltams_type expiration, signal_type::slot_type const& slot)
{
	AIRunningCurlTimer new_timer(expiration, this);
	llassert(mHandle.mRunningTimer == sTimerList.end());	// Create may only be called when the timer isn't already running.
	mHandle.init(sTimerList.insert(new_timer), slot);
	sNextExpiration = sTimerList.begin()->expiration();
}

void AICurlTimer::cancel(void)
{
    if (mHandle.mRunningTimer != sTimerList.end())
	{
		sTimerList.erase(mHandle.mRunningTimer);
		mHandle.mRunningTimer = sTimerList.end();
		sNextExpiration = sTimerList.empty() ? NEVER : sTimerList.begin()->expiration();
	}
}

void AICurlTimer::handleExpiration(void)
{
	for(;;)
	{
		if (sTimerList.empty())
		{
			// No running timers left.
			sNextExpiration = NEVER;
			break;
		}
		timer_list_type::iterator running_timer = sTimerList.begin();
		sNextExpiration = running_timer->expiration();
		if (sNextExpiration > sTime_1ms)
		{
			// Didn't expire yet.
			break;
		}

		Handle& handle(running_timer->getTimer()->mHandle);
		llassert_always(running_timer == handle.mRunningTimer);
		handle.mRunningTimer = sTimerList.end();
		running_timer->do_callback();		// May not throw exceptions.

		// Erase the timer from the running list.
		sTimerList.erase(running_timer);
	}
}

