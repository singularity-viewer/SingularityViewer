/** 
 * @file llfasttimer.cpp
 * @brief Implementation of the fast timer.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llfasttimer.h"

#include "llmemory.h"
#include "llprocessor.h"


#if LL_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "lltimer.h"
#elif LL_LINUX || LL_SOLARIS
#include <sys/time.h>
#include <sched.h>
#include "lltimer.h"
#elif LL_DARWIN
#include <sys/time.h>
#include "lltimer.h"	// get_clock_count()
#else 
#error "architecture not supported"
#endif

//////////////////////////////////////////////////////////////////////////////
// statics


LLFastTimer::EFastTimerType LLFastTimer::sCurType = LLFastTimer::FTM_OTHER;
int LLFastTimer::sCurDepth = 0;
U64 LLFastTimer::sStart[LLFastTimer::FTM_MAX_DEPTH];
U64 LLFastTimer::sCounter[LLFastTimer::FTM_NUM_TYPES];
U64 LLFastTimer::sCountHistory[LLFastTimer::FTM_HISTORY_NUM][LLFastTimer::FTM_NUM_TYPES];
U64 LLFastTimer::sCountAverage[LLFastTimer::FTM_NUM_TYPES];
U64 LLFastTimer::sCalls[LLFastTimer::FTM_NUM_TYPES];
U64 LLFastTimer::sCallHistory[LLFastTimer::FTM_HISTORY_NUM][LLFastTimer::FTM_NUM_TYPES];
U64 LLFastTimer::sCallAverage[LLFastTimer::FTM_NUM_TYPES];
S32 LLFastTimer::sCurFrameIndex = -1;
S32 LLFastTimer::sLastFrameIndex = -1;
int LLFastTimer::sPauseHistory = 0;
int LLFastTimer::sResetHistory = 0;

#define USE_RDTSC 0

#if LL_LINUX || LL_SOLARIS
U64 LLFastTimer::sClockResolution = 1000000000; // 1e9, Nanosecond resolution
#else 
U64 LLFastTimer::sClockResolution = 1000000; // 1e6, Microsecond resolution
#endif


//static
#if (LL_DARWIN || LL_LINUX || LL_SOLARIS) && !(defined(__i386__) || defined(__amd64__))
U64 LLFastTimer::countsPerSecond() // counts per second for the *32-bit* timer
{
	return sClockResolution >> 8;
}
#else // windows or x86-mac or x86-linux or x86-solaris
U64 LLFastTimer::countsPerSecond() // counts per second for the *32-bit* timer
{
#if USE_RDTSC || !LL_WINDOWS
	//getCPUFrequency returns MHz and sCPUClockFrequency wants to be in Hz
	static U64 sCPUClockFrequency = U64(LLProcessorInfo().getCPUFrequency()*1000000.0);

	// we drop the low-order byte in our timers, so report a lower frequency
#else
	// If we're not using RDTSC, each fasttimer tick is just a performance counter tick.
	// Not redefining the clock frequency itself (in llprocessor.cpp/calculate_cpu_frequency())
	// since that would change displayed MHz stats for CPUs
	static bool firstcall = true;
	static U64 sCPUClockFrequency;
	if (firstcall)
	{
		QueryPerformanceFrequency((LARGE_INTEGER*)&sCPUClockFrequency);
		firstcall = false;
	}
#endif
	return sCPUClockFrequency >> 8;
}
#endif

void LLFastTimer::reset()
{
	countsPerSecond(); // good place to calculate clock frequency
	
	if (sCurDepth != 0)
	{
		llerrs << "LLFastTimer::Reset() when sCurDepth != 0" << llendl;
	}
	if (sPauseHistory)
	{
		sResetHistory = 1;
	}
	else if (sResetHistory)
	{
		sCurFrameIndex = -1;
		sResetHistory = 0;
	}
	else if (sCurFrameIndex >= 0)
	{
		int hidx = sCurFrameIndex % FTM_HISTORY_NUM;
		for (S32 i=0; i<FTM_NUM_TYPES; i++)
		{
			sCountHistory[hidx][i] = sCounter[i];
			sCountAverage[i] = (sCountAverage[i]*sCurFrameIndex + sCounter[i]) / (sCurFrameIndex+1);
			sCallHistory[hidx][i] = sCalls[i];
			sCallAverage[i] = (sCallAverage[i]*sCurFrameIndex + sCalls[i]) / (sCurFrameIndex+1);
		}
		sLastFrameIndex = sCurFrameIndex;
	}
	else
	{
		for (S32 i=0; i<FTM_NUM_TYPES; i++)
		{
			sCountAverage[i] = 0;
			sCallAverage[i] = 0;
		}
	}
	
	sCurFrameIndex++;
	
	for (S32 i=0; i<FTM_NUM_TYPES; i++)
	{
		sCounter[i] = 0;
		sCalls[i] = 0;
	}
	sCurDepth = 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// Important note: These implementations must be FAST!
//


#if LL_WINDOWS
//
// Windows implementation of CPU clock
//

//
// NOTE: put back in when we aren't using platform sdk anymore
//
// because MS has different signatures for these functions in winnt.h
// need to rename them to avoid conflicts
//#define _interlockedbittestandset _renamed_interlockedbittestandset
//#define _interlockedbittestandreset _renamed_interlockedbittestandreset
//#include <intrin.h>
//#undef _interlockedbittestandset
//#undef _interlockedbittestandreset

//inline U32 LLFastTimer::getCPUClockCount32()
//{
//	U64 time_stamp = __rdtsc();
//	return (U32)(time_stamp >> 8);
//}
//
//// return full timer value, *not* shifted by 8 bits
//inline U64 LLFastTimer::getCPUClockCount64()
//{
//	return __rdtsc();
//}

// shift off lower 8 bits for lower resolution but longer term timing
// on 1Ghz machine, a 32-bit word will hold ~1000 seconds of timing
#if USE_RDTSC
U32 LLFastTimer::getCPUClockCount32()
{
	U32 ret_val;
	__asm
	{
        _emit   0x0f
        _emit   0x31
		shr eax,8
		shl edx,24
		or eax, edx
		mov dword ptr [ret_val], eax
	}
    return ret_val;
}

// return full timer value, *not* shifted by 8 bits
U64 LLFastTimer::getCPUClockCount64()
{
	U64 ret_val;
	__asm
	{
        _emit   0x0f
        _emit   0x31
		mov eax,eax
		mov edx,edx
		mov dword ptr [ret_val+4], edx
		mov dword ptr [ret_val], eax
	}
    return ret_val;
}

std::string LLFastTimer::sClockType = "rdtsc";

#else
//LL_COMMON_API U64 get_clock_count(); // in lltimer.cpp
// These use QueryPerformanceCounter, which is arguably fine and also works on amd architectures.
U32 LLFastTimer::getCPUClockCount32()
{
	return (U32)(get_clock_count()>>8);
}

U64 LLFastTimer::getCPUClockCount64()
{
	return get_clock_count();
}

std::string LLFastTimer::sClockType = "QueryPerformanceCounter";
#endif

#endif


#if (LL_LINUX || LL_SOLARIS) && !(defined(__i386__) || defined(__amd64__))
//
// Linux and Solaris implementation of CPU clock - non-x86.
// This is accurate but SLOW!  Only use out of desperation.
//
// Try to use the MONOTONIC clock if available, this is a constant time counter
// with nanosecond resolution (but not necessarily accuracy) and attempts are
// made to synchronize this value between cores at kernel start. It should not
// be affected by CPU frequency. If not available use the REALTIME clock, but
// this may be affected by NTP adjustments or other user activity affecting
// the system time.
U64 LLFastTimer::getCPUClockCount64()
{
	struct timespec tp;
	
#ifdef CLOCK_MONOTONIC // MONOTONIC supported at build-time?
	if (-1 == clock_gettime(CLOCK_MONOTONIC,&tp)) // if MONOTONIC isn't supported at runtime then ouch, try REALTIME
#endif
		clock_gettime(CLOCK_REALTIME,&tp);

	return (tp.tv_sec*LLFastTimer::sClockResolution)+tp.tv_nsec;        
}

U32 LLFastTimer::getCPUClockCount32()
{
	return (U32)(LLFastTimer::getCPUClockCount64() >> 8);
}

std::string LLFastTimer::sClockType = "clock_gettime";

#endif // (LL_LINUX || LL_SOLARIS) && !(defined(__i386__) || defined(__amd64__))


#if (LL_LINUX || LL_SOLARIS || LL_DARWIN) && (defined(__i386__) || defined(__amd64__))
//
// Mac+Linux+Solaris FAST x86 implementation of CPU clock
U32 LLFastTimer::getCPUClockCount32()
{
	U64 x;
	__asm__ volatile (".byte 0x0f, 0x31": "=A"(x));
	return (U32)(x >> 8);
}

U64 LLFastTimer::getCPUClockCount64()
{
	U64 x;
	__asm__ volatile (".byte 0x0f, 0x31": "=A"(x));
	return x;
}

std::string LLFastTimer::sClockType = "rdtsc";
#endif