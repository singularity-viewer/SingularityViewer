/** 
 * @file llmemberlistener.h
 * @brief Listener class which registers itself with its parent view
 *
 * <code>
 * Example usage:
 * 
 * (in header)
 * 
 * class T {
 *    class LLDoTest : public LLMemberListener<LLInventoryView>
 *    {
 *        bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata);
 *    };
 *    LLDoTest mDoTest;
 * }
 * 
 * (in cpp)
 * 
 * T::T() {
 *   mDoTest.registerListener(this, "T.Test");
 * }
 * 
 * T::LLDoTest::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
 * {
 *   T *self = mPtr;
 *   ...
 * }
 * </code>
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLMEMBERLISTENER_H
#define LL_LLMEMBERLISTENER_H

#include "llevent.h"

template <class T>
class LLMemberListener : public LLSimpleListener
{
public:
	LLMemberListener() : mPtr(NULL), mRegisteredName() { }

	void registerListener(T *pointer, const std::string& register_name)
	{
		mPtr = pointer;
		mRegisteredName = register_name;
		pointer->registerEventListener(register_name, this);
	}

	// This is what you have to override to handle this event
	virtual bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) = 0;

protected:
	T *mPtr; // The object that this listener manipulates
	std::string mRegisteredName;
};


#endif // LL_LLMEMBERLISTENER_H
