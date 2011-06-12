/** 
 * @file llpluginclassbasic.h
 * @brief LLPluginClassBasic handles interaction with a plugin which knows about the "basic" message class.
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

#ifndef LL_LLPLUGINCLASSBASIC_H
#define LL_LLPLUGINCLASSBASIC_H

#include "llerror.h"					// Needed for LOG_CLASS
#include "stdtypes.h"					// Needed for F64
#include "llpluginprocessparent.h"
#include "llpluginclassmediaowner.h"
#include "llpluginmessage.h"
#include <string>
#include <queue>

class LLPluginClassBasic : public LLPluginProcessParentOwner
{
	LOG_CLASS(LLPluginClassBasic);

public:
	LLPluginClassBasic(void);
	virtual ~LLPluginClassBasic();

	// Local initialization, called when creating a plugin process. Return true if successful.
	bool init(std::string const& launcher_filename, std::string const& plugin_filename, bool debug);

	// Undoes everything init did. Called when destroying a plugin process.
	void reset(void);

	void idle(void);

	// Send message to the plugin, either queueing or sending directly.
	void sendMessage(LLPluginMessage const& message);

	// "Loading" means uninitialized or any state prior to fully running (processing commands).
	bool isPluginLoading(void) const { return mPlugin ? mPlugin->isLoading() : false; }

	// "Running" means the steady state -- i.e. processing messages.
	bool isPluginRunning(void) const { return mPlugin ? mPlugin->isRunning() : false; }
	
	// "Exited" means any regular or error state after "Running" (plugin may have crashed or exited normally).
	bool isPluginExited(void) const { return mPlugin ? mPlugin->isDone() : false; }

	std::string getPluginVersion() const { return mPlugin ? mPlugin->getPluginVersion() : std::string(""); }

	bool getDisableTimeout() const { return mPlugin ? mPlugin->getDisableTimeout() : false; }

	void setDisableTimeout(bool disable) { if (mPlugin) mPlugin->setDisableTimeout(disable); }

	enum EPriority
	{
		PRIORITY_SLEEP,		// Sleep 1 second every message.
		PRIORITY_LOW,		// Sleep 1/25 second.
		PRIORITY_NORMAL,	// Sleep 1/50 second.
		PRIORITY_HIGH		// Sleep 1/100 second.
	};

	static char const* priorityToString(EPriority priority);
	void setPriority(EPriority priority);

protected:
	EPriority mPriority;
	LLPluginProcessParent* mPlugin;

private:
	F64 mSleepTime;
	std::queue<LLPluginMessage> mSendQueue;		// Used to queue messages while the plugin initializes.

protected:
	// Called as last function when calling 'init()'.
	virtual bool init_impl(void) { return true; }

	// Called as last function when calling 'reset()'.
	virtual void reset_impl(void) { }

	// Called from idle() before flushing messages to the plugin.
	virtual void idle_impl(void) { }

	// Called from setPriority.
	virtual void priorityChanged(EPriority priority) { }

	// Inherited from LLPluginProcessParentOwner.
	/*virtual*/ void receivePluginMessage(LLPluginMessage const&);

	// Inherited from LLPluginProcessParentOwner.
	/*virtual*/ void receivedShutdown() { mPlugin->exitState(); }

	//--------------------------------------
	// Debug use only
	//
private:
	bool mDeleteOK;

public:
	void setDeleteOK(bool flag) { mDeleteOK = flag; }
};

#endif // LL_LLPLUGINCLASSBASIC_H
