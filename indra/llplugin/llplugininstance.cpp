/** 
 * @file llplugininstance.cpp
 * @brief LLPluginInstance handles loading the dynamic library of a plugin and setting up its entry points for message passing.
 *
 * @cond
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2010, Linden Research, Inc.
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
#if LL_LINUX && defined(LL_STANDALONE)
#include <dlfcn.h>
#include <apr_portable.h>
#endif

#include "linden_common.h"

#include "llplugininstance.h"
#include "llaprpool.h"

/** Virtual destructor. */
LLPluginInstanceMessageListener::~LLPluginInstanceMessageListener()
{
}

/** 
 * TODO:DOC describe how it's used
 */
const char *LLPluginInstance::PLUGIN_INIT_FUNCTION_NAME = "LLPluginInitEntryPoint";

/** 
 * Constructor.
 *
 * @param[in] owner Plugin instance. TODO:DOC is this a good description of what "owner" is?
 */
LLPluginInstance::LLPluginInstance(LLPluginInstanceMessageListener *owner) :
	mDSOHandle(NULL),
	mPluginObject(NULL),
	mReceiveMessageFunction(NULL)
{
	mOwner = owner;
}

/** 
 * Destructor.
 */
LLPluginInstance::~LLPluginInstance()
{
	if(mDSOHandle != NULL)
	{
		apr_dso_unload(mDSOHandle);
		mDSOHandle = NULL;
	}
}

/** 
 * Dynamically loads the plugin and runs the plugin's init function.
 *
 * @param[in] plugin_file Name of plugin dll/dylib/so. TODO:DOC is this correct? see .h
 * @return 0 if successful, APR error code or error code from the plugin's init function on failure.
 */
int LLPluginInstance::load(std::string &plugin_file)
{
	pluginInitFunction init_function = NULL;
	
#if LL_LINUX && defined(LL_STANDALONE)
    void *dso_handle = dlopen(plugin_file.c_str(), RTLD_NOW | RTLD_GLOBAL);
    int result = (!dso_handle)?APR_EDSOOPEN:apr_os_dso_handle_put(&mDSOHandle,
            dso_handle, LLAPRRootPool::get()());
#else
	int result = apr_dso_load(&mDSOHandle,
					  plugin_file.c_str(),
					  LLAPRRootPool::get()());
#endif
	if(result != APR_SUCCESS)
	{
		char buf[1024];
		apr_dso_error(mDSOHandle, buf, sizeof(buf));

#if LL_LINUX && defined(LL_STANDALONE)
		LL_WARNS("Plugin") << "plugin load " << plugin_file << " failed with error " << result << " , additional info string: " << buf << LL_ENDL;
#else
		LL_WARNS("Plugin") << "apr_dso_load of " << plugin_file << " failed with error " << result << " , additional info string: " << buf << LL_ENDL;
#endif
	}
	
	if(result == APR_SUCCESS)
	{
		result = apr_dso_sym((apr_dso_handle_sym_t*)&init_function,
						 mDSOHandle,
						 PLUGIN_INIT_FUNCTION_NAME);

		if(result != APR_SUCCESS)
		{
			LL_WARNS("Plugin") << "apr_dso_sym failed with error " << result << LL_ENDL;
		}
	}
	
	if(result == APR_SUCCESS)
	{
		result = init_function(&LLPluginInstance::staticReceiveMessage, this, &mReceiveMessageFunction, &mPluginObject);

		if(result != 0)
		{
			LL_WARNS("Plugin") << "call to init function failed with error " << result << LL_ENDL;
		}
	}
	
	return (int)result;
}

// This is the SLPlugin process (the child process).
// This is not part of a DSO.
//
// This function is called from LLPluginProcessChild::receiveMessageRaw
// for messages received from the viewer that are not internal.
//
// It sends the message to the DSO by calling the registered 'received'
// function (for example, FilepickerPlugin::receiveMessage).
/** 
 * Sends a message to the plugin.
 *
 * @param[in] message Message
 */
void LLPluginInstance::sendMessage(const std::string &message)
{
	if(mReceiveMessageFunction)
	{
		LL_DEBUGS("Plugin") << "sending message to plugin: \"" << message << "\"" << LL_ENDL;
		mReceiveMessageFunction(message.c_str(), &mPluginObject);
	}
	else
	{
		LL_WARNS("Plugin") << "dropping message: \"" << message << "\"" << LL_ENDL;
	}
}

/**
 * Idle. TODO:DOC what's the purpose of this?
 *
 */
void LLPluginInstance::idle(void)
{
}

// static
void LLPluginInstance::staticReceiveMessage(char const* message_string, LLPluginInstance** self_ptr)
{
	// TODO: validate that the self argument is still a valid LLPluginInstance pointer
	// we could also use a key that's looked up in a map (instead of a direct pointer) for safety, but that's probably overkill
	(*self_ptr)->receiveMessage(message_string);
}

// This is the SLPlugin process.
// This is not part of a DSO.
//
// This function is called by a loaded DSO (through a function pointer, it
// is called from BasicPluginBase::sendMessage) for messages it wants to
// send to the viewer. It calls LLPluginProcessChild::receivePluginMessage.
/**
 * Plugin receives message from plugin loader shell.
 *
 * @param[in] message_string Message
 */
void LLPluginInstance::receiveMessage(const char *message_string)
{
	if(mOwner)
	{
		LL_DEBUGS("Plugin") << "processing incoming message: \"" << message_string << "\"" << LL_ENDL;		
		mOwner->receivePluginMessage(message_string);
	}
	else
	{
		LL_WARNS("Plugin") << "dropping incoming message: \"" << message_string << "\"" << LL_ENDL;		
	}	
}
