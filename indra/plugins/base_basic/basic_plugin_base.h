/** 
 * @file basic_plugin_base.h
 * @brief Basic plugin base class for Basic API plugin system
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

#ifndef BASIC_PLUGIN_BASE_H
#define BASIC_PLUGIN_BASE_H

#include "linden_common.h"
#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"

#include <sstream>

class BasicPluginBase
{
public:
	BasicPluginBase(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance);
	//! Basic plugin destructor.
	virtual ~BasicPluginBase() { sPluginBase = NULL; }

	//! Handle received message from plugin loader shell.
	virtual void receiveMessage(char const* message_string) = 0;

	// This function is actually called and then calls the member function above.
	static void staticReceiveMessage(char const* message_string, BasicPluginBase** self_ptr);

	// Pointer to self used for logging (this object should be a singleton).
	static BasicPluginBase* sPluginBase;

	// Used for log messages. Use macros below.
	void sendLogMessage(std::string const& message, LLPluginMessage::LLPLUGIN_LOG_LEVEL level);

	// Flush all messages to the viewer.
	void flushMessages(void);

	// Flush all messages and then shoot down the whole process.
	void sendShutdownMessage(void);

protected:
	void sendMessage(LLPluginMessage const& message);

	//! Message data being sent to plugin loader shell by mSendMessageFunction.
	LLPluginInstance* mPluginInstance;

	//! Function to send message from plugin to plugin loader shell.
	LLPluginInstance::sendMessageFunction mSendMessageFunction;

	//! Flag to delete plugin instance (self).
	bool mDeleteMe;
};

/** The plugin <b>must</b> define this function to create its instance.
 * It should look something like this: 
 * @code
 * {  
 *    *plugin_object = new FooPluginBar(send_message_function, plugin_instance); 
 *    return 0; 
 * }  
 * @endcode
 */
int create_plugin(
	LLPluginInstance::sendMessageFunction send_message_function, 
	LLPluginInstance* plugin_instance, 
	BasicPluginBase** plugin_object);

#if LL_DEBUG
#define LL_DEBUG_PLUGIN_MESSAGES 1
#else
#define LL_DEBUG_PLUGIN_MESSAGES 0
#endif

/** Convenience macros for calling BasicPluginBase::sendLogMessage.
 * To log a message, use one of:
 * @code
 * PLS_DEBUGS << "Hello debug!" << PLS_ENDL;
 * PLS_INFOS << "Hello info!" << PLS_ENDL;
 * PLS_WARNS << "Hello warning!" << PLS_ENDL;
 * PLS_ERRS << "Hello error!" << PLS_ENDL;
 * @endcode
 */
#define PLS_LOG_MESSAGE(level, generate_code) \
	do { \
		if (generate_code && BasicPluginBase::sPluginBase) \
	    { \
			LLPluginMessage::LLPLUGIN_LOG_LEVEL _pls_log_msg_level = level;\
			std::ostringstream _pls_log_msg_stream; \
			_pls_log_msg_stream
#define PLS_ENDL \
			LLError::End(); \
			BasicPluginBase::sPluginBase->sendLogMessage(_pls_log_msg_stream.str(), _pls_log_msg_level); \
		} \
	} while(0)
// Only send plugin log messages of level info and lower when compiled with debugging.
#define PLS_DEBUGS PLS_LOG_MESSAGE(LLPluginMessage::LOG_LEVEL_DEBUG, LL_DEBUG_PLUGIN_MESSAGES)
#define PLS_INFOS PLS_LOG_MESSAGE(LLPluginMessage::LOG_LEVEL_INFO, LL_DEBUG_PLUGIN_MESSAGES)
#define PLS_WARNS PLS_LOG_MESSAGE(LLPluginMessage::LOG_LEVEL_WARN, 1)
#define PLS_ERRS PLS_LOG_MESSAGE(LLPluginMessage::LOG_LEVEL_ERR, 1)
#define PLS_FLUSH do { BasicPluginBase::sPluginBase->flushMessages(); } while(0)

#endif // BASIC_PLUGIN_BASE

