/** 
 * @file ascentprefssys.cpp
 * @Ascent Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Henri Beauchamp.
 * Rewritten in its entirety 2010 Hg Beeks. 
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

//File include
#include "ascentprefssys.h"
#include "llagent.h"
#include "llaudioengine.h" //For POWER USER affirmation.
#include "llchat.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llfloaterchat.h" //For POWER USER affirmation.
#include "llradiogroup.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"


LLPrefsAscentSys::LLPrefsAscentSys()
{
    LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_ascent_system.xml");

    childSetCommitCallback("speed_rez_check", onCommitCheckBox, this);
    childSetCommitCallback("double_click_teleport_check", onCommitCheckBox, this);
    childSetCommitCallback("system_folder_check", onCommitCheckBox, this);
    childSetCommitCallback("show_look_at_check", onCommitCheckBox, this);
    childSetCommitCallback("enable_clouds", onCommitCheckBox, this);
    childSetCommitCallback("power_user_check", onCommitCheckBox, this);
    childSetCommitCallback("power_user_confirm_check", onCommitCheckBox, this);

    childSetCommitCallback("chat_cmd_toggle", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLinePos", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineGround", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineHeight", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineTeleportHome", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineRezPlatform", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineCalc", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineClearChat", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineDrawDistance", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdTeleportToCam", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineKeyToName", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineOfferTp", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineMapTo", onCommitCmdLine, this);
    childSetCommitCallback("AscentCmdLineTP2", onCommitCmdLine, this);

    refreshValues();
    refresh();
}

LLPrefsAscentSys::~LLPrefsAscentSys()
{
}

//static
void LLPrefsAscentSys::onCommitCheckBox(LLUICtrl* ctrl, void* user_data)
{
    LLPrefsAscentSys* self = (LLPrefsAscentSys*)user_data;	
    
//    llinfos << "Change to " << ctrl->getControlName()  << " aka " << ctrl->getName() << llendl;
    
    if (ctrl->getName() == "speed_rez_check")
    {
        bool enabled = self->childGetValue("speed_rez_check").asBoolean();
        self->childSetEnabled("speed_rez_interval", enabled);
        self->childSetEnabled("speed_rez_seconds", enabled);
    }
    else if (ctrl->getName() == "double_click_teleport_check")
    {
        bool enabled = self->childGetValue("double_click_teleport_check").asBoolean();
        self->childSetEnabled("center_after_teleport_check", enabled);
        self->childSetEnabled("offset_teleport_check", enabled);
    }
    else if (ctrl->getName() == "system_folder_check")
    {
        bool enabled = self->childGetValue("system_folder_check").asBoolean();
        self->childSetEnabled("temp_in_system_check", enabled);
    }
    else if (ctrl->getName() == "enable_clouds")
    {
        bool enabled = self->childGetValue("enable_clouds").asBoolean();
        self->childSetEnabled("enable_classic_clouds", enabled);
    }
    else if (ctrl->getName() == "power_user_check")
    {
        bool enabled = self->childGetValue("power_user_check").asBoolean();
        self->childSetEnabled("power_user_confirm_check", enabled);
        self->childSetValue("power_user_confirm_check", false);
    }
    else if (ctrl->getName() == "power_user_confirm_check")
    {
        bool enabled = self->childGetValue("power_user_confirm_check").asBoolean();

        gSavedSettings.setBOOL("AscentPowerfulWizard", enabled);

        if (enabled)
        {
            LLVector3d lpos_global = gAgent.getPositionGlobal();
            gAudiop->triggerSound(LLUUID("58a38e89-44c6-c52b-deb8-9f1ddc527319"), gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
            LLChat chat;
            chat.mSourceType = CHAT_SOURCE_SYSTEM;
            chat.mText = llformat("You are bestowed with powers beyond mortal comprehension.\nUse your newfound abilities wisely.\nUnlocked:\n- Animation Priority up to 7 - Meant for animations that should override anything and everything at all times. DO NOT USE THIS FOR GENERAL ANIMATIONS.\n- Right click > Destroy objects - Permanently deletes an object immediately, when you don't feel like waiting for the server to respond.\n- Right Click > Explode objects - Turns an object physical, temporary, and delinks it.");
            LLFloaterChat::addChat(chat);
        }
    }
}

//static
void LLPrefsAscentSys::onCommitCmdLine(LLUICtrl* ctrl, void* user_data)
{
    LLPrefsAscentSys* self = (LLPrefsAscentSys*)user_data;

    if (ctrl->getName() == "chat_cmd_toggle")
    {
        bool enabled = self->childGetValue("chat_cmd_toggle").asBoolean();
        self->childSetEnabled("cmd_line_text_2",           enabled);
        self->childSetEnabled("cmd_line_text_3",           enabled);
        self->childSetEnabled("cmd_line_text_4",           enabled);
        self->childSetEnabled("cmd_line_text_5",           enabled);
        self->childSetEnabled("cmd_line_text_6",           enabled);
        self->childSetEnabled("cmd_line_text_7",           enabled);
        self->childSetEnabled("cmd_line_text_8",           enabled);
        self->childSetEnabled("cmd_line_text_9",           enabled);
        self->childSetEnabled("cmd_line_text_10",          enabled);
        self->childSetEnabled("cmd_line_text_11",          enabled);
        self->childSetEnabled("cmd_line_text_12",          enabled);
        self->childSetEnabled("cmd_line_text_13",          enabled);
        self->childSetEnabled("cmd_line_text_15",          enabled);
        self->childSetEnabled("AscentCmdLinePos",          enabled);
        self->childSetEnabled("AscentCmdLineGround",       enabled);
        self->childSetEnabled("AscentCmdLineHeight",       enabled);
        self->childSetEnabled("AscentCmdLineTeleportHome", enabled);
        self->childSetEnabled("AscentCmdLineRezPlatform",  enabled);
        self->childSetEnabled("AscentPlatformSize",        enabled);
        self->childSetEnabled("AscentCmdLineCalc",         enabled);
        self->childSetEnabled("AscentCmdLineClearChat",    enabled);
        self->childSetEnabled("AscentCmdLineDrawDistance", enabled);
        self->childSetEnabled("AscentCmdTeleportToCam",    enabled);
        self->childSetEnabled("AscentCmdLineKeyToName",    enabled);
        self->childSetEnabled("AscentCmdLineOfferTp",      enabled);
        self->childSetEnabled("AscentCmdLineMapTo",        enabled);
        self->childSetEnabled("map_to_keep_pos",           enabled);
        self->childSetEnabled("AscentCmdLineTP2",          enabled);
    }

    gSavedSettings.setString("AscentCmdLinePos",          self->childGetValue("AscentCmdLinePos"));
    gSavedSettings.setString("AscentCmdLineGround",       self->childGetValue("AscentCmdLineGround"));
    gSavedSettings.setString("AscentCmdLineHeight",       self->childGetValue("AscentCmdLineHeight"));
    gSavedSettings.setString("AscentCmdLineTeleportHome", self->childGetValue("AscentCmdLineTeleportHome"));
    gSavedSettings.setString("AscentCmdLineRezPlatform",  self->childGetValue("AscentCmdLineRezPlatform"));
    gSavedSettings.setString("AscentCmdLineCalc",         self->childGetValue("AscentCmdLineCalc"));
    gSavedSettings.setString("AscentCmdLineClearChat",    self->childGetValue("AscentCmdLineClearChat"));
    gSavedSettings.setString("AscentCmdLineDrawDistance", self->childGetValue("AscentCmdLineDrawDistance"));
    gSavedSettings.setString("AscentCmdTeleportToCam",    self->childGetValue("AscentCmdTeleportToCam"));
    gSavedSettings.setString("AscentCmdLineKeyToName",    self->childGetValue("AscentCmdLineKeyToName"));
    gSavedSettings.setString("AscentCmdLineOfferTp",      self->childGetValue("AscentCmdLineOfferTp"));
    gSavedSettings.setString("AscentCmdLineMapTo",        self->childGetValue("AscentCmdLineMapTo"));
    gSavedSettings.setString("AscentCmdLineTP2",          self->childGetValue("AscentCmdLineTP2"));
}

void LLPrefsAscentSys::refreshValues()
{
    //General -----------------------------------------------------------------------------
    mDoubleClickTeleport		= gSavedSettings.getBOOL("DoubleClickTeleport");
    mResetCameraAfterTP		= gSavedSettings.getBOOL("OptionRotateCamAfterLocalTP");
    mOffsetTPByUserHeight	= gSavedSettings.getBOOL("OptionOffsetTPByAgentHeight");
    mPreviewAnimInWorld			= gSavedSettings.getBOOL("PreviewAnimInWorld");
//  mSaveScriptsAsMono			= gSavedSettings.getBOOL("SaveScriptsAsMono");
    mAlwaysRezInGroup			= gSavedSettings.getBOOL("AscentAlwaysRezInGroup");
    mBuildAlwaysEnabled			= gSavedSettings.getBOOL("AscentBuildAlwaysEnabled");
    mAlwaysShowFly				= gSavedSettings.getBOOL("AscentFlyAlwaysEnabled");
    mDisableMinZoom				= gSavedSettings.getBOOL("AscentDisableMinZoomDist");
    mPowerUser					= gSavedSettings.getBOOL("AscentPowerfulWizard");
    mUseSystemFolder			= gSavedSettings.getBOOL("AscentUseSystemFolder");
    mUploadToSystem				= gSavedSettings.getBOOL("AscentSystemTemporary");
    mFetchInventoryOnLogin		= gSavedSettings.getBOOL("FetchInventoryOnLogin");
    mEnableLLWind				= gSavedSettings.getBOOL("WindEnabled");
    mEnableClouds				= gSavedSettings.getBOOL("CloudsEnabled");
    mEnableClassicClouds		= gSavedSettings.getBOOL("SkyUseClassicClouds");
    mSpeedRez					= gSavedSettings.getBOOL("SpeedRez");
    mSpeedRezInterval			= gSavedSettings.getU32("SpeedRezInterval");

    //Command Line ------------------------------------------------------------------------
    mCmdLine                    = gSavedSettings.getBOOL("AscentCmdLine");
    mCmdLinePos                 = gSavedSettings.getString("AscentCmdLinePos");
    mCmdLineGround              = gSavedSettings.getString("AscentCmdLineGround");
    mCmdLineHeight              = gSavedSettings.getString("AscentCmdLineHeight");
    mCmdLineTeleportHome        = gSavedSettings.getString("AscentCmdLineTeleportHome");
    mCmdLineRezPlatform         = gSavedSettings.getString("AscentCmdLineRezPlatform");
    mCmdPlatformSize            = gSavedSettings.getF32("AscentPlatformSize");
    mCmdLineCalc                = gSavedSettings.getString("AscentCmdLineCalc");
    mCmdLineClearChat           = gSavedSettings.getString("AscentCmdLineClearChat");
    mCmdLineDrawDistance        = gSavedSettings.getString("AscentCmdLineDrawDistance");
    mCmdTeleportToCam           = gSavedSettings.getString("AscentCmdTeleportToCam");
    mCmdLineKeyToName           = gSavedSettings.getString("AscentCmdLineKeyToName");
    mCmdLineOfferTp             = gSavedSettings.getString("AscentCmdLineOfferTp");
    mCmdLineMapTo               = gSavedSettings.getString("AscentCmdLineMapTo");
    mCmdMapToKeepPos            = gSavedSettings.getBOOL("AscentMapToKeepPos");
    mCmdLineTP2                 = gSavedSettings.getString("AscentCmdLineTP2");

    //Privacy -----------------------------------------------------------------------------
    mBroadcastViewerEffects		= gSavedSettings.getBOOL("BroadcastViewerEffects");
    mDisablePointAtAndBeam		= gSavedSettings.getBOOL("DisablePointAtAndBeam");
    mPrivateLookAt				= gSavedSettings.getBOOL("PrivateLookAt");
    mShowLookAt					= gSavedSettings.getBOOL("AscentShowLookAt");
    mRevokePermsOnStandUp		= gSavedSettings.getBOOL("RevokePermsOnStandUp");
    mDisableClickSit			= gSavedSettings.getBOOL("DisableClickSit");
    mDisplayScriptJumps			= gSavedSettings.getBOOL("AscentDisplayTotalScriptJumps");
    mNumScriptDiff              = gSavedSettings.getF32("Ascentnumscriptdiff");
}

void LLPrefsAscentSys::refresh()
{
    childSetEnabled("center_after_teleport_check",	mDoubleClickTeleport);
    childSetEnabled("offset_teleport_check",		mDoubleClickTeleport);
    childSetValue("power_user_check",				mPowerUser);
    childSetValue("power_user_confirm_check",		mPowerUser);
    childSetEnabled("temp_in_system_check",			mUseSystemFolder);
    childSetEnabled("speed_rez_interval",           mSpeedRez);
    childSetEnabled("speed_rez_seconds",            mSpeedRez);

    childSetEnabled("cmd_line_text_2",            mCmdLine);
    childSetEnabled("cmd_line_text_3",            mCmdLine);
    childSetEnabled("cmd_line_text_4",            mCmdLine);
    childSetEnabled("cmd_line_text_5",            mCmdLine);
    childSetEnabled("cmd_line_text_6",            mCmdLine);
    childSetEnabled("cmd_line_text_7",            mCmdLine);
    childSetEnabled("cmd_line_text_8",            mCmdLine);
    childSetEnabled("cmd_line_text_9",            mCmdLine);
    childSetEnabled("cmd_line_text_10",           mCmdLine);
    childSetEnabled("cmd_line_text_11",           mCmdLine);
    childSetEnabled("cmd_line_text_12",           mCmdLine);
    childSetEnabled("cmd_line_text_13",           mCmdLine);
    childSetEnabled("cmd_line_text_15",           mCmdLine);
    childSetEnabled("AscentCmdLinePos",           mCmdLine);
    childSetEnabled("AscentCmdLineGround",        mCmdLine);
    childSetEnabled("AscentCmdLineHeight",        mCmdLine);
    childSetEnabled("AscentCmdLineTeleportHome",  mCmdLine);
    childSetEnabled("AscentCmdLineRezPlatform",   mCmdLine);
    childSetEnabled("AscentPlatformSize",         mCmdLine);
    childSetEnabled("AscentCmdLineCalc",          mCmdLine);
    childSetEnabled("AscentCmdLineClearChat",     mCmdLine);
    childSetEnabled("AscentCmdLineDrawDistance",  mCmdLine);
    childSetEnabled("AscentCmdTeleportToCam",     mCmdLine);
    childSetEnabled("AscentCmdLineKeyToName",     mCmdLine);
    childSetEnabled("AscentCmdLineOfferTp",       mCmdLine);
    childSetEnabled("AscentCmdLineMapTo",         mCmdLine);
    childSetEnabled("map_to_keep_pos",            mCmdLine);
    childSetEnabled("AscentCmdLineTP2",           mCmdLine);

    childSetValue("AscentCmdLinePos",           mCmdLinePos);
    childSetValue("AscentCmdLineGround",        mCmdLineGround);
    childSetValue("AscentCmdLineHeight",        mCmdLineHeight);
    childSetValue("AscentCmdLineTeleportHome",  mCmdLineTeleportHome);
    childSetValue("AscentCmdLineRezPlatform",   mCmdLineRezPlatform);
    childSetValue("AscentCmdLineCalc",          mCmdLineCalc);
    childSetValue("AscentCmdLineClearChat",     mCmdLineClearChat);
    childSetValue("AscentCmdLineDrawDistance",  mCmdLineDrawDistance);
    childSetValue("AscentCmdTeleportToCam",     mCmdTeleportToCam);
    childSetValue("AscentCmdLineKeyToName",     mCmdLineKeyToName);
    childSetValue("AscentCmdLineOfferTp",       mCmdLineOfferTp);
    childSetValue("AscentCmdLineMapTo",         mCmdLineMapTo);
    childSetValue("AscentCmdLineTP2",           mCmdLineTP2);
}

void LLPrefsAscentSys::cancel()
{
    //General -----------------------------------------------------------------------------
    gSavedSettings.setBOOL("DoubleClickTeleport", mDoubleClickTeleport);
        gSavedSettings.setBOOL("OptionRotateCamAfterLocalTP", mResetCameraAfterTP);
        gSavedSettings.setBOOL("OptionOffsetTPByAgentHeight", mOffsetTPByUserHeight);
    gSavedSettings.setBOOL("PreviewAnimInWorld", mPreviewAnimInWorld);
//    gSavedSettings.setBOOL("SaveScriptsAsMono", mSaveScriptsAsMono);
    gSavedSettings.setBOOL("AscentAlwaysRezInGroup", mAlwaysRezInGroup);
    gSavedSettings.setBOOL("AscentBuildAlwaysEnabled", mBuildAlwaysEnabled);
    gSavedSettings.setBOOL("AscentFlyAlwaysEnabled", mAlwaysShowFly);
    gSavedSettings.setBOOL("AscentDisableMinZoomDist", mDisableMinZoom);
    gSavedSettings.setBOOL("AscentUseSystemFolder", mUseSystemFolder);
        gSavedSettings.setBOOL("AscentSystemTemporary", mUploadToSystem);
    gSavedSettings.setBOOL("FetchInventoryOnLogin", mFetchInventoryOnLogin);
    gSavedSettings.setBOOL("WindEnabled", mEnableLLWind);
    gSavedSettings.setBOOL("CloudsEnabled", mEnableClouds);
        gSavedSettings.setBOOL("SkyUseClassicClouds", mEnableClassicClouds);
    gSavedSettings.setBOOL("SpeedRez", mSpeedRez);
        gSavedSettings.setU32("SpeedRezInterval", mSpeedRezInterval);

    //Command Line ------------------------------------------------------------------------
    gSavedSettings.setBOOL("AscentCmdLine",                 mCmdLine);
    gSavedSettings.setString("AscentCmdLinePos",		    mCmdLinePos);
    gSavedSettings.setString("AscentCmdLineGround",		    mCmdLineGround);
    gSavedSettings.setString("AscentCmdLineHeight",		    mCmdLineHeight);
    gSavedSettings.setString("AscentCmdLineTeleportHome",	mCmdLineTeleportHome);
    gSavedSettings.setString("AscentCmdLineRezPlatform",	mCmdLineRezPlatform);
    gSavedSettings.setF32("AscentPlatformSize",             mCmdPlatformSize);
    gSavedSettings.setString("AscentCmdLineCalc",		    mCmdLineCalc);
    gSavedSettings.setString("AscentCmdLineClearChat",	    mCmdLineClearChat);
    gSavedSettings.setString("AscentCmdLineDrawDistance",	mCmdLineDrawDistance);
    gSavedSettings.setString("AscentCmdTeleportToCam",		mCmdTeleportToCam);
    gSavedSettings.setString("AscentCmdLineKeyToName",		mCmdLineKeyToName);
    gSavedSettings.setString("AscentCmdLineOfferTp",		mCmdLineOfferTp);
    gSavedSettings.setString("AscentCmdLineMapTo",			mCmdLineMapTo);
    gSavedSettings.setBOOL("AscentMapToKeepPos",            mCmdMapToKeepPos);
    gSavedSettings.setString("AscentCmdLineTP2",			mCmdLineTP2);

    //Privacy -----------------------------------------------------------------------------
    gSavedSettings.setBOOL("BroadcastViewerEffects",        mBroadcastViewerEffects);
    gSavedSettings.setBOOL("DisablePointAtAndBeam",         mDisablePointAtAndBeam);
    gSavedSettings.setBOOL("PrivateLookAt",                 mPrivateLookAt);
    gSavedSettings.setBOOL("AscentShowLookAt",              mShowLookAt);
    gSavedSettings.setBOOL("RevokePermsOnStandUp",          mRevokePermsOnStandUp);
    gSavedSettings.setBOOL("DisableClickSit",               mDisableClickSit);
    gSavedSettings.setBOOL("AscentDisplayTotalScriptJumps", mDisplayScriptJumps);
    gSavedSettings.setF32("Ascentnumscriptdiff",            mNumScriptDiff);
}

void LLPrefsAscentSys::apply()
{
    refreshValues();
    refresh();
}
