/** 
 * @file demo_plugin.cpp
 * @brief Test plugin to be loaded by the llplugin testbed.
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

#ifdef WIN32
#include <windows.h>
#endif

#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"

// TODO: Make sure that the only symbol exported from this library is LLPluginInitEntryPoint

class DemoPlugin
{
public:

	static int init(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data);

private:
	DemoPlugin(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data);
	~DemoPlugin();

	static void staticReceiveMessage(const char *message_string, void **user_data);
	void receiveMessage(const char *message_string);
	void sendMessage(const LLPluginMessage &message);

	LLPluginInstance::sendMessageFunction mHostSendFunction;
	void *mHostUserData;
	bool mDeleteMe;
	
	int mSharedSegmentFillValue;
	void *mSharedSegmentBase;
	size_t mSharedSegmentSize;
};

int DemoPlugin::init(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data)
{
	DemoPlugin *self = new DemoPlugin(host_send_func, host_user_data);
	*plugin_send_func = staticReceiveMessage;
	*plugin_user_data = (void*)self;

	return 0;
}

DemoPlugin::DemoPlugin(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data)
{
	std::cerr << "DemoPlugin constructor" << std::endl;

	mHostSendFunction = host_send_func;
	mHostUserData = host_user_data;
	mDeleteMe = false;
	mSharedSegmentBase = NULL;
	mSharedSegmentSize = 0;
	mSharedSegmentFillValue = 0;
}

DemoPlugin::~DemoPlugin()
{
	std::cerr << "DemoPlugin destructor" << std::endl;
}

void DemoPlugin::staticReceiveMessage(const char *message_string, void **user_data)
{
	DemoPlugin *self = (DemoPlugin*)*user_data;
	
	if(self != NULL)
	{
		self->receiveMessage(message_string);
	
		// If the plugin has processed the delete message, delete it.
		if(self->mDeleteMe)
		{
			delete self;
			*user_data = NULL;
		}
	}
}

void DemoPlugin::receiveMessage(const char *message_string)
{
//	std::cerr << "DemoPlugin::receiveMessage: received message: \"" << message_string << "\"" << std::endl;
	LLPluginMessage message_in;
	
	if(message_in.parse(message_string) >= 0)
	{
		std::string message_class = message_in.getClass();
		std::string message_name = message_in.getName();
		if(message_class == "base")
		{
			if(message_name == "init")
			{
				LLPluginMessage message("base", "init_response");
				LLSD versions = LLSD::emptyMap();
				versions[LLPLUGIN_MESSAGE_CLASS_BASE] = LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
				versions[LLPLUGIN_MESSAGE_CLASS_MEDIA] = LLPLUGIN_MESSAGE_CLASS_MEDIA_VERSION;
				message.setValueLLSD("versions", versions);
				sendMessage(message);
			}
			else if(message_name == "idle")
			{
				// no response is necessary here.
//				std::cerr << "DemoPlugin::receiveMessage: idle processing" << std::endl;
				if(mSharedSegmentBase != NULL)
				{
					// Fill the shared memory segment
					memset(mSharedSegmentBase, mSharedSegmentFillValue, mSharedSegmentSize);
					// and increment the fill value
					mSharedSegmentFillValue++;
				}
			}
			else if(message_name == "shutdown")
			{
				sendMessage(LLPluginMessage("base", "shutdown_response"));
				
				mDeleteMe = true;
			}
			else if(message_name == "shm_added")
			{
				// Normally, we would check the name and match it up with something from another message.
				// For this test, just fill any segment that comes in.
				mSharedSegmentSize = (size_t)message_in.getValueS32("size");
				mSharedSegmentBase = (void*)message_in.getValueU32("address");
				
				std::cerr << "DemoPlugin::receiveMessage: shared memory added, name: " << message_in.getValue("name") 
					<< ", size: " << mSharedSegmentSize 
					<< ", address: " << mSharedSegmentBase 
					<< std::endl;

				memset(mSharedSegmentBase, mSharedSegmentFillValue, mSharedSegmentSize);
			
			}
			else if(message_name == "shm_remove")
			{
				std::cerr << "DemoPlugin::receiveMessage: shared memory remove" << std::endl;
				
				// Normally, we would check the name and match it up with something from another message.
				// For this test, just stop filling the only segment we track.

				mSharedSegmentBase = NULL;

				// Send the response so it can be cleaned up.
				LLPluginMessage message("base", "shm_remove_response");
				message.setValue("name", message_in.getValue("name"));
				sendMessage(message);
			}
			else
			{
				std::cerr << "DemoPlugin::receiveMessage: unknown base message: " << message_name << std::endl;
			}
		}
		else
		{
			std::cerr << "DemoPlugin::receiveMessage: unknown message class: " << message_class << std::endl;
		}

	}
}

void DemoPlugin::sendMessage(const LLPluginMessage &message)
{
	std::string output = message.generate();
	mHostSendFunction(output.c_str(), &mHostUserData);
}


extern "C"
{
#ifdef WIN32
	__declspec(dllexport)
#endif
		int LLPluginInitEntryPoint(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data);
}

int 
#ifdef WIN32
	__declspec(dllexport)
#endif
	LLPluginInitEntryPoint(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data)
{
	return DemoPlugin::init(host_send_func, host_user_data, plugin_send_func, plugin_user_data);
}

#ifdef WIN32
int WINAPI DllEntryPoint( HINSTANCE hInstance, unsigned long reason, void* params )
{
	return 1;
}
#endif
