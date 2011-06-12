/** 
 * @file llpluginclassbasic.cpp
 * @brief LLPluginClassBasic handles a plugin which knows about the "basic" message class.
 *
 * @cond
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
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 * @endcond
 */

#include "linden_common.h"
#include "indra_constants.h"

#include "llpluginclassbasic.h"
#include "llpluginmessageclasses.h"

LLPluginClassBasic::LLPluginClassBasic(void) : mPlugin(NULL), mDeleteOK(true)
{
	// Note that this only initializes the base class, the derived class doesn't exist yet!
	// Derived classes must therefore call their own reset_impl() from their constructor.
	reset();
}

LLPluginClassBasic::~LLPluginClassBasic()
{
	llassert_always(mDeleteOK);
	delete mPlugin;
}

bool LLPluginClassBasic::init(std::string const& launcher_filename, std::string const& plugin_filename, bool debug)
{	
	LL_DEBUGS("Plugin") << "launcher: " << launcher_filename << LL_ENDL;
	LL_DEBUGS("Plugin") << "plugin: " << plugin_filename << LL_ENDL;
	
	mPlugin = new LLPluginProcessParent(this);
	mPlugin->setSleepTime(mSleepTime);
	
	mPlugin->init(launcher_filename, plugin_filename, debug);

	return init_impl();
}

void LLPluginClassBasic::reset()
{
	if (mPlugin != NULL)
	{
		delete mPlugin;
		mPlugin = NULL;
	}
	mSleepTime = 1.0f / 50.0f;
	mPriority = PRIORITY_NORMAL;
	reset_impl();
}

void LLPluginClassBasic::idle(void)
{
	if(mPlugin)
	{
		mPlugin->idle();
	}

	idle_impl();
	
	if(mPlugin && mPlugin->isRunning())
	{
		// Send queued messages
		while(!mSendQueue.empty())
		{
			LLPluginMessage message = mSendQueue.front();
			mSendQueue.pop();
			mPlugin->sendMessage(message);
		}
	}
}

char const* LLPluginClassBasic::priorityToString(EPriority priority)
{
	const char* result = "UNKNOWN";
	switch(priority)
	{
		case PRIORITY_SLEEP:		result = "sleep";		break;
		case PRIORITY_LOW:			result = "low";			break;
		case PRIORITY_NORMAL:		result = "normal";		break;
		case PRIORITY_HIGH:			result = "high";		break;
	}
	
	return result;
}

void LLPluginClassBasic::setPriority(EPriority priority)
{
	if (mPriority != priority)
	{
		mPriority = priority;

		LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_BASIC, "set_priority");
		
		std::string priority_string = priorityToString(priority);
		switch(priority)
		{
			case PRIORITY_SLEEP:
				mSleepTime = 1.0f;
				break;
			case PRIORITY_LOW:		
				mSleepTime = 1.0f / 25.0f;
				break;
			case PRIORITY_NORMAL:	
				mSleepTime = 1.0f / 50.0f;
				break;
			case PRIORITY_HIGH:		
				mSleepTime = 1.0f / 100.0f;
				break;
		}
		
		message.setValue("priority", priority_string);
		sendMessage(message);
		
		if(mPlugin)
		{
			mPlugin->setSleepTime(mSleepTime);
		}
		
		LL_DEBUGS("PluginPriority") << this << ": setting priority to " << priority_string << LL_ENDL;

		priorityChanged(mPriority);
	}
}

/* virtual */ 
void LLPluginClassBasic::receivePluginMessage(const LLPluginMessage &message)
{
	std::string message_class = message.getClass();

	if (message_class == LLPLUGIN_MESSAGE_CLASS_BASIC)
	{
		std::string message_name = message.getName();

		// This class hasn't defined any incoming messages yet.
//		if (message_name == "message_name")
//		{
//		}
//		else 
		{
			LL_WARNS("Plugin") << "Unknown " << message_class << " class message: " << message_name << LL_ENDL;
		}
	}
}

// This is the viewer process (the parent process)
//
// Call this function to send a message to a plugin.
// It calls LLPluginProcessParent::sendMessage.
void LLPluginClassBasic::sendMessage(LLPluginMessage const& message)
{
	if(mPlugin && mPlugin->isRunning())
	{
		mPlugin->sendMessage(message);
	}
	else
	{
		// The plugin isn't set up yet -- queue this message to be sent after initialization.
		mSendQueue.push(message);
	}
}
