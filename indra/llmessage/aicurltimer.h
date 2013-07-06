/**
 * @file aicurltimer.h
 * @brief Implementation of AICurlTimer.
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
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AICURLTIMER_H
#define AICURLTIMER_H

#include "llerror.h"			// llassert
#include "stdtypes.h"			// U64, F64
#include <boost/signals2.hpp>
#include <set>

class AICurlTimer
{
  protected:
	typedef boost::signals2::signal<void (void)> signal_type;
	typedef long deltams_type;

  private:
	// Use separate struct for this object because it is non-copyable.
	struct Signal {
		signal_type mSignal;
	};

	class AIRunningCurlTimer {
	  private:
		U64 mExpire;						// Time at which the timer expires, in miliseconds since the epoch (compared to LLCurlTimer::sTime_1ms).
		AICurlTimer* mTimer;				// The actual timer.
		// Can be mutable, since only the mExpire is used for ordering this object in the multiset AICurlTimer::sTimerList.
		mutable Signal* mCallback;			// Pointer to callback struct, or NULL when the object wasn't added to sTimerList yet.

	  public:
		AIRunningCurlTimer(deltams_type expiration, AICurlTimer* timer) : mExpire(AICurlTimer::sTime_1ms + expiration), mTimer(timer), mCallback(NULL) { }
		~AIRunningCurlTimer() { delete mCallback; }

		// This function is called after the final object was added to sTimerList (where it is initialized in-place).
		void init(signal_type::slot_type const& slot) const
			{
			  // We may only call init() once.
			  llassert(!mCallback);
			  mCallback = new Signal;
			  mCallback->mSignal.connect(slot);
			}

		// Order AICurlTimer::sTimerList so that the timer that expires first is up front.
		friend bool operator<(AIRunningCurlTimer const& ft1, AIRunningCurlTimer const& ft2) { return ft1.mExpire < ft2.mExpire; }

		void do_callback(void) const { mCallback->mSignal(); }
		U64 expiration(void) const { return mExpire; }
		AICurlTimer* getTimer(void) const { return mTimer; }

#if LL_DEBUG
		// May not copy this object after it was initialized.
		AIRunningCurlTimer(AIRunningCurlTimer const& running_curl_timer) :
			mExpire(running_curl_timer.mExpire), mTimer(running_curl_timer.mTimer), mCallback(running_curl_timer.mCallback)
			{ llassert(!mCallback); }
#endif
	};

	typedef std::multiset<AIRunningCurlTimer> timer_list_type;
	static timer_list_type sTimerList;			// List with all running timers.
	static U64 sNextExpiration;					// Cache of smallest value in sTimerList.

  public:
	static F64 const sClockWidth_1ms;			// Time between two clock ticks in 1 ms units.
	static U64 sTime_1ms;						// Time since the epoch in 1 ms units. Set by AICurlThread::run().

	static deltams_type nextExpiration(void) { return static_cast<deltams_type>(sNextExpiration - sTime_1ms); }
	static bool expiresBefore(deltams_type timeout_ms) { return sNextExpiration < sTime_1ms + timeout_ms; }

  private:
	class Handle {
	  public:
		timer_list_type::iterator mRunningTimer;	// Points to the running timer, or to sTimerList.end() when not running.
													// Access to this iterator is protected by the AICurlTimer::sMutex!

		// Constructor for a not-running timer.
		Handle(void) : mRunningTimer(sTimerList.end()) { }

		// Actual initialization used by AICurlTimer::create.
		void init(timer_list_type::iterator const& running_timer, signal_type::slot_type const& slot)
			{
			  mRunningTimer = running_timer;
			  mRunningTimer->init(slot);
			}

	  private:
		// No assignment operator.
	  	Handle& operator=(Handle const&) { return *this; }
	};

	Handle mHandle;

  public:
	// Construct an AICurlTimer that is not running.
	AICurlTimer(void) { }

	// Construction of a running AICurlTimer with expiration time expiration in miliseconds, and callback slot.
	AICurlTimer(deltams_type expiration, signal_type::slot_type const& slot) { create(expiration, slot); }

	// Destructing the AICurlTimer object terminates the running timer (if still running).
	// Note that cancel() must have returned BEFORE anything is destructed that would disallow the callback function to be called.
	// So, if the AICurlTimer is a member of an object whose callback function is called then cancel() has
	// to be called manually (or from the destructor of THAT object), before that object is destructed.
	// Cancel may be called multiple times.
	~AICurlTimer() { cancel(); }

	void create(deltams_type expiration, signal_type::slot_type const& slot);
	void cancel(void);

	bool isRunning(void) const { return mHandle.mRunningTimer != sTimerList.end(); }

  public:
	static void handleExpiration(void);
};

#endif

