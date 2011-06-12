/** 
 * @file plugin_host.cpp
 * @brief Testbed for llplugin which directly loads a plugin dynamic library.
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

#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llerrorcontrol.h"

class MessageReceiver: public LLPluginInstanceMessageListener
{
	// Inherited from LLPluginInstanceMessageListener
	/* virtual */ void receivePluginMessage(const std::string &message)
	{
		LL_INFOS("plugin_host") << "message received from plugin: " << message << LL_ENDL;
	}
	
};

int main(int argc, char **argv)
{
	ll_init_apr();

	// Set up llerror logging 
	{
		LLError::initForApplication(".");
		LLError::setDefaultLevel(LLError::LEVEL_DEBUG);
	}

	if(argc < 2)
	{
		LL_ERRS("plugin_host") << "usage: " << argv[0] << " plugin_filename" << LL_ENDL;
		exit(1);
	}
		
	std::string name = argv[1];
	
	MessageReceiver receiver;
	LLPluginInstance *plugin = new LLPluginInstance(&receiver);
	if(plugin->load(name) == 0)
	{
		LLPluginMessage message;
		message.setMessage("base", "init");
		message.setValue("foo", "1");
		message.setValue("bar", "2");
		plugin->sendMessage(message.generate());
		
		message.setMessage("base", "idle");
		message.setValue("baz", "3");
		plugin->sendMessage(message.generate());

		message.setMessage("base", "shutdown");
		plugin->sendMessage(message.generate());

		plugin->idle();
	}

	delete plugin;

	ll_cleanup_apr();	
}

