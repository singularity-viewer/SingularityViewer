/** 
 * @file basic_plugin_base.cpp
 * @brief Basic plugin base class for Basic API plugin system
 *
 * All plugins should be a subclass of BasicPluginBase. 
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
#include "basic_plugin_base.h"
#ifdef WIN32
#include <windows.h>
#endif


// TODO: Make sure that the only symbol exported from this library is LLPluginInitEntryPoint

// Used for logging.
BasicPluginBase* BasicPluginBase::sPluginBase;

////////////////////////////////////////////////////////////////////////////////
/// Basic plugin constructor.
///
/// @param[in] send_message_function Function for sending messages from plugin to plugin loader shell
/// @param[in] plugin_instance Message data for messages from plugin to plugin loader shell
BasicPluginBase::BasicPluginBase(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance) :
		mPluginInstance(plugin_instance), mSendMessageFunction(send_message_function), mDeleteMe(false)
{
	llassert(!sPluginBase);
	sPluginBase = this;
}

/**
 * Receive message from plugin loader shell.
 * 
 * @param[in] message_string Message string
 * @param[in] user_data Message data
 *
 */
void BasicPluginBase::staticReceiveMessage(char const* message_string, BasicPluginBase** self_ptr)
{
  	BasicPluginBase* self = *self_ptr;
	if(self != NULL)
	{
		self->receiveMessage(message_string);

		// If the plugin has processed the delete message, delete it.
		if(self->mDeleteMe)
		{
			delete self;
			*self_ptr = NULL;
		}
	}
}

// This is the SLPlugin process.
// This is the loaded DSO.
//
// Call this function to send 'message' to the viewer.
// Note: mSendMessageFunction points to LLPluginInstance::staticReceiveMessage, so indirectly this
//       just calls LLPluginInstance::receiveMessage (mPluginInstance->receiveMessage) where
//       mPluginInstance is the LLPluginInstance created in LLPluginProcessChild::idle during
//       state STATE_PLUGIN_LOADING. That function then immediately calls mOwner->receivePluginMessage
//       which is implemented as LLPluginProcessChild::receivePluginMessage, the same
//       LLPluginProcessChild object that created the LLPluginInstance.
/**
 * Send message to plugin loader shell.
 * 
 * @param[in] message Message data being sent to plugin loader shell
 *
 */
void BasicPluginBase::sendMessage(const LLPluginMessage &message)
{
	std::string output = message.generate();
	PLS_DEBUGS << "BasicPluginBase::sendMessage: Sending: " << output << PLS_ENDL;
	mSendMessageFunction(output.c_str(), &mPluginInstance);
}

/**
 * Send debug log message to plugin loader shell.
 *
 * @param[in] message Log message being sent to plugin loader shell
 * @param[in] level Log message level, enum of LLPluginMessage::LLPLUGIN_LOG_LEVEL
 *
 */
void BasicPluginBase::sendLogMessage(std::string const& message, LLPluginMessage::LLPLUGIN_LOG_LEVEL level)
{
	LLPluginMessage logmessage(LLPLUGIN_MESSAGE_CLASS_INTERNAL, "log_message");
	logmessage.setValue("message", message);
	logmessage.setValueS32("log_level",level);
	mSendMessageFunction(logmessage.generate().c_str(), &mPluginInstance);
	// Theoretically we should flush here (that's what PLS_ENDL means), but we don't because
	// that interfers with how the code would act without debug messages. Instead, the developer
	// is responsible to call flushMessages themselves at the appropriate places.
}

/**
 * Flush all messages to the viewer.
 *
 * This blocks if necessary, up till 10 seconds, before it gives up.
 */
void BasicPluginBase::flushMessages(void)
{
	LLPluginMessage logmessage(LLPLUGIN_MESSAGE_CLASS_INTERNAL, "flush");
	mSendMessageFunction(logmessage.generate().c_str(), &mPluginInstance);
}

/**
 * Send shutdown message to the plugin loader shell.
 *
 * This will cause the SLPlugin process that loaded this DSO to be terminated.
 */
void BasicPluginBase::sendShutdownMessage(void)
{
	// Do a double flush. First flush all messages so far, then send the shutdown message,
	// which also will try to flush itself before terminating the process.
	flushMessages();
	LLPluginMessage shutdownmessage(LLPLUGIN_MESSAGE_CLASS_INTERNAL, "shutdown");
	sendMessage(shutdownmessage);
}

#if LL_WINDOWS
# define LLSYMEXPORT __declspec(dllexport)
#elif LL_LINUX
# define LLSYMEXPORT __attribute__ ((visibility("default")))
#else
# define LLSYMEXPORT /**/
#endif

extern "C"
{
	LLSYMEXPORT int LLPluginInitEntryPoint(LLPluginInstance::sendMessageFunction send_message_function,
										   LLPluginInstance* plugin_instance,
										   LLPluginInstance::receiveMessageFunction* receive_message_function,
										   BasicPluginBase** plugin_object);
}

/**
 * Plugin initialization and entry point. Establishes communication channel for messages between plugin and plugin loader shell.  TODO:DOC - Please check!
 * 
 * @param[in] send_message_function Function for sending messages from plugin to viewer
 * @param[in] plugin_instance Message data for messages from plugin to plugin loader shell
 * @param[out] receive_message_function Function for receiving message from viewer to plugin
 * @param[out] plugin_object Pointer to plugin instance
 *
 * @return int, where 0=success
 *
 */
LLSYMEXPORT int
LLPluginInitEntryPoint(LLPluginInstance::sendMessageFunction send_message_function,
					   LLPluginInstance* plugin_instance,
					   LLPluginInstance::receiveMessageFunction* receive_message_function,
					   BasicPluginBase** plugin_object)
{
	*receive_message_function = BasicPluginBase::staticReceiveMessage;
	return create_plugin(send_message_function, plugin_instance, plugin_object);
}

#ifdef WIN32
int WINAPI DllEntryPoint( HINSTANCE hInstance, unsigned long reason, void* params )
{
	return 1;
}
#endif
