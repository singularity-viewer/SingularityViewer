/** 
 * @file plugin_process_launcher.cpp
 * @brief Testbed for llplugin which launches the plugin loader shell.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "llprocesslauncher.h"
#include "lltimer.h" // for ms_sleep()
#include "llpumpio.h"
#include "llapr.h"
#include "llerrorcontrol.h"
#include "llpluginprocessparent.h"

class PluginProcessLauncherMessageReceiver : public LLPluginProcessParentOwner
{
LOG_CLASS(PluginProcessLauncherMessageReceiver);

public:
	virtual ~PluginProcessLauncherMessageReceiver()
	{
	}
	
	/* virtual */ void receivePluginMessage(const LLPluginMessage &message)
	{
		LL_INFOS("plugin_process_launcher") << "received message: \n" << message.generate() << LL_ENDL;
	}
};

LLPumpIO* gServicePump;
LLPluginProcessParent *gPlugin;


#define SHARED_MEMORY_SIZE 0x10000
#define SHARED_MEMORY_NAME "testsegment"
enum State
{
	STATE_STARTUP,
	STATE_ADD_MEMORY,
	STATE_RUNNING,
	STATE_REMOVE_MEMORY,
	STATE_SHUTDOWN,
	STATE_CLEANUP,
	STATE_DONE
};

int main(int argc, char **argv)
{
	ll_init_apr();

	// Set up llerror logging 
	{
		LLError::initForApplication(".");
		LLError::setDefaultLevel(LLError::LEVEL_INFO);
	}
	
	std::string launcher_name;
	std::string plugin_name;

	if(argc >= 3)
	{
		launcher_name = argv[1];
		plugin_name = argv[2];
	}
	else
	{
#if LL_DARWIN
		// hardcoding the testbed arguments by default
		launcher_name = "plugin_process_host";
		plugin_name = "libdemo_plugin.dylib";
#elif LL_WINDOWS
		// hardcoding the testbed arguments by default
		launcher_name = "plugin_process_host.exe";
		plugin_name = "demo_plugin.dll";
#else
		LL_ERRS("plugin_process_launcher") << "usage: " << argv[0] << " launcher_filename plugin_filename" << LL_ENDL;
#endif
	}

	PluginProcessLauncherMessageReceiver receiver;
	
	gServicePump = new LLPumpIO(gAPRPoolp);
	gServicePump->prime(gAPRPoolp);

	gPlugin = new LLPluginProcessParent(gServicePump, &receiver);

	State state = STATE_STARTUP;
	while(state != STATE_DONE)
	{
		switch(state)
		{
			case STATE_STARTUP:
				LL_INFOS("plugin_process_launcher") << "startup" << LL_ENDL;
				gPlugin->init(launcher_name, plugin_name);
				state = STATE_ADD_MEMORY;
			break;
			
			case STATE_ADD_MEMORY:
				if(gPlugin->isRunning())
				{
					LL_INFOS("plugin_process_launcher") << "adding shared memory" << LL_ENDL;
					gPlugin->addSharedMemory(SHARED_MEMORY_SIZE);
					state = STATE_RUNNING;
				}
			break;
			
			case STATE_RUNNING:
			{
				volatile unsigned char *addr = (unsigned char*)gPlugin->getSharedMemoryAddress(SHARED_MEMORY_NAME);
				if(addr != NULL)
				{
					int val = (int)(addr[0]);
					if(val >= 16)
					{
						state = STATE_REMOVE_MEMORY;
					}
					else
					{
						LL_INFOS("plugin_process_launcher") << "running, value from shared memory is " << val << LL_ENDL;
					}
				}
			}
			break;
			
			case STATE_REMOVE_MEMORY:
				LL_INFOS("plugin_process_launcher") << "removing shared memory" << LL_ENDL;
				gPlugin->removeSharedMemory(SHARED_MEMORY_NAME);
				state = STATE_SHUTDOWN;
			break;
			
			case STATE_SHUTDOWN:
			{
				volatile unsigned char *addr = (unsigned char*)gPlugin->getSharedMemoryAddress(SHARED_MEMORY_NAME);
				if(addr == NULL)
				{
					LL_INFOS("plugin_process_launcher") << "sending shutdown request" << LL_ENDL;
					gPlugin->shutdownRequest();
					state = STATE_CLEANUP;
				}
			}
			break;
			
			case STATE_CLEANUP:
				if(gPlugin->isDone())
				{
					LL_INFOS("plugin_process_launcher") << "plugin is done" << LL_ENDL;
					state = STATE_DONE;
				}
			break;
			
			case STATE_DONE:
				// should never reach here -- the while() should exit first.
			break;
		}
		
		// Do this every time through the loop
		if(state != STATE_DONE)
		{
			gServicePump->pump();
			gServicePump->callback();
			gPlugin->idle();
			ms_sleep(100);
		}
	}

	delete gPlugin;
	
	ll_cleanup_apr();
	
}
