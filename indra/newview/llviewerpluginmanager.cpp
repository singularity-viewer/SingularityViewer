/**
 * @file llviewerpluginmanager.cpp
 * @brief Client interface to the plugin engine
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"
#include "llviewerpluginmanager.h"
#include "llnotificationsutil.h"

void LLViewerPluginManager::destroyPlugin()
{
	delete mPluginBase;
	mPluginBase = NULL;
}

void LLViewerPluginManager::update()
{
	if (!mPluginBase)
	{
		return;
	}

	mPluginBase->idle();

	if (mPluginBase->isPluginExited())
	{
		destroyPlugin();
		return;
	}
}

void LLViewerPluginManager::send_plugin_failure_warning(std::string const& plugin_basename)
{
	LL_WARNS("Plugin") << "plugin intialization failed for plugin: " << plugin_basename << LL_ENDL;
	LLSD args;
	args["MIME_TYPE"] = plugin_basename;	// FIXME: Use different notification.
	LLNotificationsUtil::add("NoPlugin", args);
}

