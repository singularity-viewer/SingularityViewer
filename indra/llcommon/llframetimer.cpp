/** 
 * @file llframetimer.cpp
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * Copyright (c) 2011, Aleric Inglewood.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "u64.h"

#include "llframetimer.h"

// Local constants.
static F64 const USEC_PER_SECOND = 1000000.0;
static F64 const USEC_TO_SEC_F64 = 0.000001;

// Static members
U64 const LLFrameTimer::sStartTotalTime = totalTime();				// Application start in microseconds since epoch.
U64 LLFrameTimer::sTotalTime = LLFrameTimer::sStartTotalTime;		// Current time in microseconds since epoch, updated at least once per frame.
F64 LLFrameTimer::sTotalSeconds =									// Current time in seconds since epoch, updated together with LLFrameTimer::sTotalTime.
        U64_to_F64(LLFrameTimer::sTotalTime) * USEC_TO_SEC_F64;
F64 LLFrameTimer::sFrameTime = 0.0;									// Current time in seconds since application start, updated together with LLFrameTimer::sTotalTime.
// Updated exactly once per frame:
S32 LLFrameTimer::sFrameCount = 0;									// Current frame number (number of frames since application start).
U64 LLFrameTimer::sPrevTotalTime = LLFrameTimer::sStartTotalTime;	// Previous (frame) time in microseconds since epoch, updated once per frame.
U64 LLFrameTimer::sFrameDeltaTime = 0;								// Microseconds between last two calls to LLFrameTimer::updateFrameTimeAndCount.

// static
void LLFrameTimer::updateFrameTime()
{
	sTotalTime = totalTime();
	sTotalSeconds = U64_to_F64(sTotalTime) * USEC_TO_SEC_F64;
	sFrameTime = U64_to_F64(sTotalTime - sStartTotalTime) * USEC_TO_SEC_F64;
} 

// static
void LLFrameTimer::updateFrameTimeAndCount()
{
	updateFrameTime();
	sFrameDeltaTime = sTotalTime - sPrevTotalTime;
	sPrevTotalTime = sTotalTime;
	++sFrameCount;
}

void LLFrameTimer::reset(F32 expiration)
{
	llassert(!mPaused);
	mStartTime = sFrameTime;
	mExpiry = sFrameTime + expiration;
}

void LLFrameTimer::start(F32 expiration)
{
	reset(expiration);
	mRunning = true;							// Start, if not already started.
}

void LLFrameTimer::stop()
{
	llassert(!mPaused);
	mRunning = false;
}

// Don't combine pause/unpause with start/stop
// Useage:
//  LLFrameTime foo; // starts automatically
//  foo.unpause(); // noop but safe
//  foo.pause(); // pauses timer
//  foo.unpause() // unpauses
//  F32 elapsed = foo.getElapsedTimeF32() // does not include time between pause() and unpause()
//  Note: elapsed would also be valid with no unpause() call (= time run until pause() called)
void LLFrameTimer::pause()
{
	if (!mPaused)
	{
		mStartTime = sFrameTime - mStartTime;	// Abuse mStartTime to store the elapsed time so far.
	}
	mPaused = true;
}

void LLFrameTimer::unpause()
{
	if (mPaused)
	{
		mStartTime = sFrameTime - mStartTime;	// Set mStartTime consistent with the elapsed time so far.
	}
	mPaused = false;
}

void LLFrameTimer::setTimerExpirySec(F32 expiration)
{
	llassert(!mPaused);
	mExpiry = mStartTime + expiration;
}

void LLFrameTimer::setExpiryAt(F64 seconds_since_epoch)
{
	llassert(!mPaused);
	mStartTime = sFrameTime;
	mExpiry = seconds_since_epoch - (USEC_TO_SEC_F64 * sStartTotalTime);
}

F64 LLFrameTimer::expiresAt() const
{
	F64 expires_at = U64_to_F64(sStartTotalTime) * USEC_TO_SEC_F64;
	expires_at += mExpiry;
	return expires_at;
}

bool LLFrameTimer::checkExpirationAndReset(F32 expiration)
{
	if (hasExpired())
	{
		reset(expiration);
		return true;
	}
	return false;
}

// static
F32 LLFrameTimer::getFrameDeltaTimeF32()
{
	return (F32)(U64_to_F64(sFrameDeltaTime) * USEC_TO_SEC_F64); 
}


//	static 
// Return seconds since the current frame started
F32  LLFrameTimer::getCurrentFrameTime()
{
	U64 frame_time = totalTime() - sTotalTime;
	return (F32)(U64_to_F64(frame_time) * USEC_TO_SEC_F64); 
}

// Glue code to avoid full class .h file #includes
F32  getCurrentFrameTime()
{
	return (F32)(LLFrameTimer::getCurrentFrameTime());
}
