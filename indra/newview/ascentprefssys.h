/** 
 * @file hbprefsinert.h
 * @brief  Ascent Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Henri Beauchamp.
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

#ifndef ASCENTPREFSSYS_H
#define ASCENTPREFSSYS_H


#include "llpanel.h"


class LLPrefsAscentSys : public LLPanel
{
public:
	LLPrefsAscentSys();
	~LLPrefsAscentSys();

	void apply();
	void cancel();
    void refresh();

	LLPanel* getPanel();

protected:
	static void onCommitCheckBox(LLUICtrl* ctrl, void* user_data);
    static void onSpellAdd(void* data);
    static void onSpellRemove(void* data);
    static void onSpellGetMore(void* data);
    static void onSpellEditCustom(void* data);
    static void onSpellBaseComboBoxCommit(LLUICtrl* ctrl, void* userdata);
	void refreshValues();
	//General -----------------------------------------------------------------------------
	BOOL mDoubleClickTeleport;
		BOOL mResetCameraAfterTP;
		BOOL mOffsetTPByUserHeight;
	BOOL mPreviewAnimInWorld;
	BOOL mSaveScriptsAsMono;
	BOOL mAlwaysRezInGroup;
	//Disable Teleport Progress
	//Disable Logout progress
	//always show Build
	BOOL mAlwaysShowFly;
	//Disable camera minimum distance
	BOOL mPowerUser;
	BOOL mUseSystemFolder;
	BOOL mUploadToSystem;
	//Chat/IM -----------------------------------------------------------------------------
	BOOL mHideNotificationsInChat;
	BOOL mPlayTypingSound;
	BOOL mHideTypingNotification;
	BOOL mEnableMUPose;
	BOOL mEnableOOCAutoClose;
	U32 mLinksForChattingObjects;
	U32 mTimeFormat;
	U32 mDateFormat;
	BOOL mSecondsInChatAndIMs;
	//Performance -------------------------------------------------------------------------
	BOOL mFetchInventoryOnLogin;
	BOOL mEnableLLWind;
	BOOL mEnableClouds;
	BOOL mEnableClassicClouds;
	BOOL mSpeedRez;
	U32 mSpeedRezInterval;
	//Command Line ------------------------------------------------------------------------
	//Privacy -----------------------------------------------------------------------------
	BOOL mBroadcastViewerEffects;
	BOOL mDisablePointAtAndBeam;
	BOOL mPrivateLookAt;
	BOOL mShowLookAt;
	BOOL mRevokePermsOnStandUp;
	BOOL mDisableClickSit;
	//Text Options ------------------------------------------------------------------------
    BOOL mSpellDisplay;
};

#endif