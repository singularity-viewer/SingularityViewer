/**
 * @file llviewerpluginmanager.h
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

#ifndef LLVIEWERPLUGINMANAGER_H
#define LLVIEWERPLUGINMANAGER_H

#include <string>
#include "llmemory.h"
#include "llerror.h"
#include "lldir.h"
#include "llfile.h"
#include "llviewercontrol.h"
#include "llpluginclassbasic.h"

class LLViewerPluginManager : public LLRefCount
{
	LOG_CLASS(LLViewerPluginManager);

public:
	// Construct an uninitialized LLViewerPluginManager object.
	LLViewerPluginManager(void) : mPluginBase(NULL) { }

	// Create a PLUGIN_TYPE (must be derived from LLPluginClassBasic).
	// This uses PLUGIN_TYPE::launcher_name() and PLUGIN_TYPE::plugin_basename().
	// If successful, returns the created LLPluginClassBasic, NULL otherwise.
	template<class PLUGIN_TYPE, typename T>
	  LLPluginClassBasic* createPlugin(T* user_data);

	// Delete the plugin.
	void destroyPlugin();

	// Handle plugin messages.
	virtual void update();

	// Return pointer to plugin.
	LLPluginClassBasic* getPlugin(void) const { return mPluginBase; }

private:
	// Called from createPlugin.
	void send_plugin_failure_warning(std::string const& plugin_basename);

protected:
	LLPluginClassBasic* mPluginBase;		//!< Pointer to the base class of the underlaying plugin.
};

template<class PLUGIN_TYPE, typename T>
LLPluginClassBasic* LLViewerPluginManager::createPlugin(T* user_data)
{
	// Always delete the old plugin first.
	destroyPlugin();
	  
	std::string plugin_name = gDirUtilp->getLLPluginFilename(PLUGIN_TYPE::plugin_basename());

	// See if the plugin executable exists.
	llstat s;
	if (LLFile::stat(PLUGIN_TYPE::launcher_name(), &s))
	{
		LL_WARNS("Plugin") << "Couldn't find launcher at " << PLUGIN_TYPE::launcher_name() << LL_ENDL;
	}
	else if (LLFile::stat(plugin_name, &s))
	{
		LL_WARNS("Plugin") << "Couldn't find plugin at " << plugin_name << LL_ENDL;
	}
	else
	{
		LLPluginClassBasic* plugin = new PLUGIN_TYPE(user_data);
		if (plugin->init(PLUGIN_TYPE::launcher_name(), plugin_name, gSavedSettings.getBOOL("PluginAttachDebuggerToPlugins")))
		{
			mPluginBase = plugin;
		}
		else
		{
			LL_WARNS("Plugin") << "Failed to init plugin. Destroying." << LL_ENDL;
			delete plugin;
		}
	}

	if (mPluginBase)
		return mPluginBase;

	send_plugin_failure_warning(PLUGIN_TYPE::plugin_basename());
	return NULL;
}

#endif	// LLVIEWERPLUGINMANAGER_H
