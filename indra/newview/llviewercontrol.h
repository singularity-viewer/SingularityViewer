/** 
 * @file llviewercontrol.h
 * @brief references to viewer-specific control files
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERCONTROL_H
#define LL_LLVIEWERCONTROL_H

#include <map>
#include "llcontrol.h"
#include "aithreadsafe.h"

class LLUICtrl;

// Enabled this definition to compile a 'hacked' viewer that
// allows a hacked godmode to be toggled on and off.
#define TOGGLE_HACKED_GODLIKE_VIEWER 
#ifdef TOGGLE_HACKED_GODLIKE_VIEWER
extern BOOL gHackGodmode;
#endif

// These functions found in llcontroldef.cpp *TODO: clean this up!
//setting variables are declared in this function
void settings_setup_listeners();

typedef std::map<std::string, LLControlGroup*> settings_map_type;
extern AIThreadSafe<settings_map_type> gSettings;

// for the graphics settings
void create_graphics_group(LLControlGroup& group);

// saved at end of session
// gSavedSettings and gSavedPerAccountSettings moved to llcontrol.h

// Read-only
extern LLControlGroup gColors;

// Saved at end of session
extern LLControlGroup gCrashSettings;

// Set after settings loaded
extern std::string gLastRunVersion;
extern std::string gCurrentVersion;


bool handleCloudSettingsChanged(const LLSD& newvalue);

//A template would be a little awkward to use here.. so.. a preprocessor macro. Alas. onCommitControlSetting(gSavedSettings) etc.
void onCommitControlSetting_gSavedSettings(LLUICtrl* ctrl, void* name);
void onCommitControlSetting_gSavedPerAccountSettings(LLUICtrl* ctrl, void* name);
#define onCommitControlSetting(controlgroup) onCommitControlSetting_##controlgroup

#endif // LL_LLVIEWERCONTROL_H
