/**
 * @file llviewermediaeventemitter.cpp
 * @brief Implementation of LLViewerMediaEventEmitter
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"
#include "llviewermediaeventemitter.h"
#include "llviewermediaobserver.h"
#include <algorithm>

LLViewerMediaEventEmitter::~LLViewerMediaEventEmitter()
{
	observerListType::iterator iter = mObservers.begin();

	while (iter != mObservers.end())
	{
		LLViewerMediaObserver* self = *iter;
		++iter;
		remObserver(self);
	}
}

bool LLViewerMediaEventEmitter::addObserver(LLViewerMediaObserver* observer)
{
	if (!observer)
		return false;

	if (std::find(mObservers.begin(), mObservers.end(), observer) != mObservers.end())
		return false;

	mObservers.push_back(observer);
	observer->mEmitters.push_back(this);

	return true;
}

bool LLViewerMediaEventEmitter::remObserver(LLViewerMediaObserver* observer)
{
	if (!observer)
		return false;

	mObservers.remove(observer);
	observer->mEmitters.remove(this);

	return true;
}

void LLViewerMediaEventEmitter::emitEvent(LLPluginClassMedia* media, LLPluginClassMediaOwner::EMediaEvent event)
{
	observerListType::iterator iter = mObservers.begin();

	while (iter != mObservers.end())
	{
		LLViewerMediaObserver* self = *iter;
		++iter;
		self->handleMediaEvent(media, event);
	}
}
