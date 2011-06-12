/**
 * @file llscopedvolatileaprpool.h
 * @brief Implementation of LLScopedVolatileAPRPool
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLSCOPEDVOLATILEAPRPOOL_H
#define LL_LLSCOPEDVOLATILEAPRPOOL_H

#include "llthread.h"

//! Scoped volatile memory pool.
//
// As the AIVolatileAPRPool should never keep allocations very
// long, it's most common use is for allocations with a lifetime
// equal to it's scope.
//
// This is a convenience class that makes just a little easier to type.
//
class LLScopedVolatileAPRPool
{
private:
	AIVolatileAPRPool& mPool;
	apr_pool_t* mScopedAPRpool;
public:
	LLScopedVolatileAPRPool() : mPool(AIThreadLocalData::tldata().mVolatileAPRPool), mScopedAPRpool(mPool.getVolatileAPRPool()) { }
	~LLScopedVolatileAPRPool() { mPool.clearVolatileAPRPool(); }
	// Only use this to pass the pointer to a libapr-1 function that requires it.
	operator apr_pool_t*() const { return mScopedAPRpool; }
};

#endif
