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
#endif
#include "lltimer.h"

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

U64 LLFastTimer::sClockResolution = calc_clock_frequency(50U);	// Resolution of get_clock_count()

U64 LLFastTimer::countsPerSecond() // counts per second for the *32-bit* timer
{
	return sClockResolution >> 8;
}

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

// shift off lower 8 bits for lower resolution but longer term timing
// on 1Ghz machine, a 32-bit word will hold ~1000 seconds of timing

//LL_COMMON_API U64 get_clock_count(); // in lltimer.cpp
// On windows these use QueryPerformanceCounter, which is arguably fine and also works on amd architectures.
U32 LLFastTimer::getCPUClockCount32()
{
	return get_clock_count() >> 8;
}

U64 LLFastTimer::getCPUClockCount64()
{
	return get_clock_count();
}
