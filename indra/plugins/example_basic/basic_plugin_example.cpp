/**
 * @file basic_plugin_example.cpp
 * @brief Example Plugin for a basic plugin.
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

class BasicPluginExample : public BasicPluginBase
{
	public:
		BasicPluginExample(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance);
		~BasicPluginExample();

		/*virtual*/ void receiveMessage(char const* message_string);

	private:
		bool init();
};

BasicPluginExample::BasicPluginExample(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance) :
	BasicPluginBase(send_message_function, plugin_instance)
{
}

BasicPluginExample::~BasicPluginExample()
{
}

void BasicPluginExample::receiveMessage(char const* message_string)
{
	LLPluginMessage message_in;

	if (message_in.parse(message_string) >= 0)
	{
		std::string message_class = message_in.getClass();
		std::string message_name = message_in.getName();

		if (message_class == LLPLUGIN_MESSAGE_CLASS_BASE)
		{
			if (message_name == "init")
			{
				LLPluginMessage message("base", "init_response");
				LLSD versions = LLSD::emptyMap();
				versions[LLPLUGIN_MESSAGE_CLASS_BASE] = LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
				versions[LLPLUGIN_MESSAGE_CLASS_BASIC] = LLPLUGIN_MESSAGE_CLASS_BASIC_VERSION;
				message.setValueLLSD("versions", versions);

				std::string plugin_version = "Basic Plugin Example, version 1.0.0.0";
				message.setValue("plugin_version", plugin_version);
				sendMessage(message);
			}
			else if (message_name == "idle")
			{
				// This whole message should not have existed imho -- Aleric
			}
			else
			{
				std::cerr << "BasicPluginExample::receiveMessage: unknown base message: " << message_name << std::endl;
			}
		}
		else if (message_class == LLPLUGIN_MESSAGE_CLASS_BASIC)
		{
			if (message_name == "poke")
			{
				LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_BASIC, "pokeback");
				sendMessage(message);
			}
		}
		else
		{
			std::cerr << "BasicPluginExample::receiveMessage: unknown message class: " << message_class << std::endl;
		}
	}
}

bool BasicPluginExample::init(void)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_BASIC, "name_text");
	message.setValue("name", "Basic Plugin Example");
	sendMessage(message);

	return true;
}

int create_plugin(LLPluginInstance::sendMessageFunction send_message_function,
				  LLPluginInstance* plugin_instance,
				  BasicPluginBase** plugin_object)
{
	*plugin_object = new BasicPluginExample(send_message_function, plugin_instance);
	return 0;
}

