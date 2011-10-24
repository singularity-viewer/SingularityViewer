/** 
 * @file ascentprefsvan.cpp
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
#include "ascentprefsvan.h"

//project includes
#include "llcolorswatch.h"
#include "llvoavatarself.h"
#include "llagent.h"
#include "llfloaterchat.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "v4color.h"
#include "lluictrlfactory.h"
#include "llcombobox.h"
#include "llwind.h"
#include "llviewernetwork.h"
#include "pipeline.h"
#include "llscrolllistctrl.h"

LLPrefsAscentVan::LLPrefsAscentVan()
{
    LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_ascent_vanity.xml");

    childSetCommitCallback("tag_spoofing_combobox", onCommitClientTag, this);

    childSetCommitCallback("show_my_tag_check", onCommitCheckBox, this);
    childSetCommitCallback("show_self_tag_check", onCommitCheckBox, this);
    childSetCommitCallback("show_self_tag_color_check", onCommitCheckBox, this);
    childSetCommitCallback("customize_own_tag_check", onCommitCheckBox, this);
    childSetCommitCallback("show_friend_tag_check", onCommitCheckBox, this);
    childSetCommitCallback("use_status_check", onCommitCheckBox, this);

    childSetCommitCallback("custom_tag_label_box", onCommitTextModified, this);

    childSetCommitCallback("X Modifier", onCommitUpdateAvatarOffsets);
    childSetCommitCallback("Y Modifier", onCommitUpdateAvatarOffsets);
    childSetCommitCallback("Z Modifier", onCommitUpdateAvatarOffsets);

    childSetAction("update_clientdefs", onManualClientUpdate, this);

    refreshValues();
    refresh();
}

LLPrefsAscentVan::~LLPrefsAscentVan()
{
}
//static
void LLPrefsAscentVan::onCommitClientTag(LLUICtrl* ctrl, void* userdata)
{
    std::string client_uuid;
    U32 client_index;

    LLPrefsAscentVan* self = (LLPrefsAscentVan*)userdata;
    LLComboBox* combo = (LLComboBox*)ctrl;

    if (combo)
    {
        client_index = combo->getCurrentIndex();
        if (client_index != self->mSelectedClient)
        {
            client_uuid = combo->getSelectedValue().asString();
            gSavedSettings.setString("AscentReportClientUUID",  client_uuid);
            gSavedSettings.setU32("AscentReportClientIndex",  client_index);

            LLVOAvatar* avatar = gAgentAvatarp;

            if (avatar)
            {
				gAgent.resetClientTag();
				gAgent.sendAgentSetAppearance();
            }
        }
    }
}

//static
void LLPrefsAscentVan::onCommitUpdateAvatarOffsets(LLUICtrl* ctrl, void* userdata)
{
    if (!gAgent.getID().isNull())
    {
        gAgent.sendAgentSetAppearance();
    }
}

//static
void LLPrefsAscentVan::onCommitTextModified(LLUICtrl* ctrl, void* userdata)
{
    LLPrefsAscentVan* self = (LLPrefsAscentVan*)userdata;

    if (ctrl->getName() == "custom_tag_label_box")
    {
        gSavedSettings.setString("AscentCustomTagLabel", self->childGetValue("custom_tag_label_box"));
    }
	
	gAgent.resetClientTag();
}

//static
void LLPrefsAscentVan::onManualClientUpdate(void* data)
{
	LLPrefsAscentVan* self = (LLPrefsAscentVan*)data;
	std::string client_uuid;
	U32 client_index;
	//Colors ------------------------------------------------------------------------------
	LLComboBox* combo = self->getChild<LLComboBox>("tag_spoofing_combobox");
	if (combo)
	{
		client_index = combo->getCurrentIndex();
		//Don't rebake if it's not neccesary.
		if (client_index != gSavedSettings.getU32("AscentReportClientIndex"))
		{
			client_uuid = combo->getSelectedValue().asString();
			gSavedSettings.setString("AscentReportClientUUID",  client_uuid);
			gSavedSettings.setU32("AscentReportClientIndex",  client_index);
			if (gAgentAvatarp)
			{
				gAgent.sendAgentSetAppearance();
			}
		}
	}

    LLChat chat;
    chat.mSourceType = CHAT_SOURCE_SYSTEM;
    chat.mText = llformat("Definitions already up-to-date.");
    if (LLVOAvatar::updateClientTags())
    {
        chat.mText = llformat("Client definitions updated.");
        LLVOAvatar::loadClientTags();
        for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
        iter != LLCharacter::sInstances.end(); ++iter)
        {
            LLVOAvatar* avatarp = (LLVOAvatar*) *iter;
            if(avatarp)
            {
                LLVector3 root_pos_last = avatarp->mRoot.getWorldPosition();
                avatarp->mClientTag = "";
            }
        }
    }
    LLFloaterChat::addChat(chat);
}

//static
void LLPrefsAscentVan::onCommitCheckBox(LLUICtrl* ctrl, void* user_data)
{
    LLPrefsAscentVan* self = (LLPrefsAscentVan*)user_data;

//	llinfos << "Control named " << ctrl->getControlName() << llendl;

    if (ctrl->getName() == "show_friend_tag_check")
    {
        for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
        iter != LLCharacter::sInstances.end(); ++iter)
        {
            LLVOAvatar* avatarp = (LLVOAvatar*) *iter;
            if(avatarp)
            {
                LLVector3 root_pos_last = avatarp->mRoot.getWorldPosition();
                avatarp->mClientTag = "";
            }
        }
    }
    else if (ctrl->getName() == "use_status_check")
    {
        for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
        iter != LLCharacter::sInstances.end(); ++iter)
        {
            LLVOAvatar* avatarp = (LLVOAvatar*) *iter;
            if(avatarp)
            {
                LLVector3 root_pos_last = avatarp->mRoot.getWorldPosition();
                avatarp->mClientTag = "";
            }
        }

        BOOL showCustomColors = gSavedSettings.getBOOL("AscentUseStatusColors");
        self->childSetEnabled("friends_color_textbox", showCustomColors);
        self->childSetEnabled("friend_color_swatch", showCustomColors);
        self->childSetEnabled("estate_owner_color_swatch", showCustomColors);
        self->childSetEnabled("linden_color_swatch", showCustomColors);
        self->childSetEnabled("muted_color_swatch", showCustomColors);
    }
    else if (ctrl->getName() == "customize_own_tag_check")
    {
        BOOL showCustomOptions = gSavedSettings.getBOOL("AscentUseCustomTag");
        self->childSetEnabled("custom_tag_label_text", showCustomOptions);
        self->childSetEnabled("custom_tag_label_box", showCustomOptions);
        self->childSetEnabled("custom_tag_color_text", showCustomOptions);
        self->childSetEnabled("custom_tag_color_swatch", showCustomOptions);
    }

    if (!gAgent.getID().isNull())
    {
        gAgent.sendAgentSetAppearance();
        gAgent.resetClientTag();
    }
}

// Store current settings for cancel
void LLPrefsAscentVan::refreshValues()
{
    //General --------------------------------------------------------------------------------
    mUseAccountSettings		= gSavedSettings.getBOOL("AscentStoreSettingsPerAccount");
    mShowTPScreen			= !gSavedSettings.getBOOL("AscentDisableTeleportScreens");
    mPlayTPSound			= gSavedSettings.getBOOL("OptionPlayTpSound");
    mShowLogScreens			= !gSavedSettings.getBOOL("AscentDisableLogoutScreens");

    //Tags\Colors ----------------------------------------------------------------------------
    mAscentUseTag           = gSavedSettings.getBOOL("AscentUseTag");
    mReportClientUUID       = gSavedSettings.getString("AscentReportClientUUID");
    mSelectedClient			= gSavedSettings.getU32("AscentReportClientIndex");
    mShowSelfClientTag		= gSavedSettings.getBOOL("AscentShowSelfTag");
    mShowSelfClientTagColor = gSavedSettings.getBOOL("AscentShowSelfTagColor");
    mShowFriendsTag         = gSavedSettings.getBOOL("AscentShowFriendsTag");
    mCustomTagOn			= gSavedSettings.getBOOL("AscentUseCustomTag");
    mCustomTagLabel			= gSavedSettings.getString("AscentCustomTagLabel");
    mCustomTagColor			= gSavedSettings.getColor4("AscentCustomTagColor");
    mShowOthersTag          = gSavedSettings.getBOOL("AscentShowOthersTag");
    mShowOthersTagColor     = gSavedSettings.getBOOL("AscentShowOthersTagColor");
    mShowIdleTime           = gSavedSettings.getBOOL("AscentShowIdleTime");
    mUseStatusColors        = gSavedSettings.getBOOL("AscentUseStatusColors");
    mUpdateTagsOnLoad       = gSavedSettings.getBOOL("AscentUpdateTagsOnLoad");
    mEffectColor			= gSavedSettings.getColor4("EffectColor");
    mFriendColor			= gSavedSettings.getColor4("AscentFriendColor");
    mEstateOwnerColor		= gSavedSettings.getColor4("AscentEstateOwnerColor");
    mLindenColor			= gSavedSettings.getColor4("AscentLindenColor");
    mMutedColor				= gSavedSettings.getColor4("AscentMutedColor");
    //mCustomColor			= gSavedSettings.getColor4("MoyMiniMapCustomColor");

    //Body Dynamics --------------------------------------------------------------------------
    mBreastPhysicsToggle    = gSavedSettings.getBOOL("EmeraldBreastPhysicsToggle");
    mBoobMass               = gSavedSettings.getF32("EmeraldBoobMass");
    mBoobHardness           = gSavedSettings.getF32("EmeraldBoobHardness");
    mBoobVelMax             = gSavedSettings.getF32("EmeraldBoobVelMax");
    mBoobFriction           = gSavedSettings.getF32("EmeraldBoobFriction");
    mBoobVelMin             = gSavedSettings.getF32("EmeraldBoobVelMin");

    mAvatarXModifier        = gSavedSettings.getF32("AscentAvatarXModifier");
    mAvatarYModifier        = gSavedSettings.getF32("AscentAvatarYModifier");
    mAvatarZModifier        = gSavedSettings.getF32("AscentAvatarZModifier");
}

// Update controls based on current settings
void LLPrefsAscentVan::refresh()
{
    //General --------------------------------------------------------------------------------
	LLComboBox* combo_fonts = getChild<LLComboBox>("fonts_selector");
	std::string fontxml = gSavedSettings.getString("FontSettingsFile");
	combo_fonts->selectByValue(fontxml);

    //Tags\Colors ----------------------------------------------------------------------------
    //Colors ---------------------------------------------------------------------------------
	LLComboBox* combo = getChild<LLComboBox>("tag_spoofing_combobox");
	if(LLVOAvatar::sClientResolutionList.has("isComplete"))
	{
		//combo->setColor(LLColor4::black);
		combo->clear();
		for(LLSD::map_iterator itr = LLVOAvatar::sClientResolutionList.beginMap(); itr != LLVOAvatar::sClientResolutionList.endMap(); itr++)
		{
			LLSD value = (*itr).second;
			if(value.has("name"))
			{
				std::string name = value.get("name");
				std::string uuid = (*itr).first;
				LLColor4 color = LLColor4(value.get("color"));
				if(value["multiple"].asReal() != 0)
				{
					color *= 1.0/(value["multiple"].asReal()+1.0f);
				}
				LLScrollListItem* item = combo->add(name,uuid);
				//bad practice
				item->getColumn(0)->setColor(color);
			}
		}
		//add Viewer 2.0
		LLScrollListItem* item = combo->add("Viewer 2.0",IMG_DEFAULT_AVATAR);
		//bad practice
		item->getColumn(0)->setColor(LLColor4::black);
	}
	combo->setCurrentByIndex(mSelectedClient);

    childSetEnabled("friends_color_textbox",     mUseStatusColors);
    childSetEnabled("friend_color_swatch",       mUseStatusColors);
    childSetEnabled("estate_owner_color_swatch", mUseStatusColors);
    childSetEnabled("linden_color_swatch",       mUseStatusColors);
    childSetEnabled("muted_color_swatch",        mUseStatusColors);

    childSetEnabled("custom_tag_label_text",   mCustomTagOn);
    childSetEnabled("custom_tag_label_box",    mCustomTagOn);
    childSetValue("custom_tag_label_box", gSavedSettings.getString("AscentCustomTagLabel"));
    childSetEnabled("custom_tag_color_text",   mCustomTagOn);
    childSetEnabled("custom_tag_color_swatch", mCustomTagOn);

    //Body Dynamics --------------------------------------------------------------------------
    childSetEnabled("EmeraldBoobMass",     mBreastPhysicsToggle);
    childSetEnabled("EmeraldBoobHardness", mBreastPhysicsToggle);
    childSetEnabled("EmeraldBoobVelMax",   mBreastPhysicsToggle);
    childSetEnabled("EmeraldBoobFriction", mBreastPhysicsToggle);
    childSetEnabled("EmeraldBoobVelMin",   mBreastPhysicsToggle);
}

// Reset settings to local copy
void LLPrefsAscentVan::cancel()
{
    //General --------------------------------------------------------------------------------
    gSavedSettings.setBOOL("AscentStoreSettingsPerAccount", mUseAccountSettings);
    gSavedSettings.setBOOL("AscentDisableTeleportScreens", !mShowTPScreen);
    gSavedSettings.setBOOL("OptionPlayTpSound",             mPlayTPSound);
    gSavedSettings.setBOOL("AscentDisableLogoutScreens",   !mShowLogScreens);

    //Tags\Colors ----------------------------------------------------------------------------
    gSavedSettings.setBOOL("AscentUseTag",               mAscentUseTag);
    gSavedSettings.setString("AscentReportClientUUID",   mReportClientUUID);
    gSavedSettings.setU32("AscentReportClientIndex",     mSelectedClient);
    gSavedSettings.setBOOL("AscentShowSelfTag",          mShowSelfClientTag);
    gSavedSettings.setBOOL("AscentShowSelfTagColor",     mShowSelfClientTagColor);
    gSavedSettings.setBOOL("AscentShowFriendsTag",       mShowFriendsTag);
    gSavedSettings.setBOOL("AscentUseCustomTag",         mCustomTagOn);
    gSavedSettings.setString("AscentCustomTagLabel",     mCustomTagLabel);
    gSavedSettings.setColor4("AscentCustomTagColor",     mCustomTagColor);
    gSavedSettings.setBOOL("AscentShowOthersTag",        mShowOthersTag);
    gSavedSettings.setBOOL("AscentShowOthersTagColor",   mShowOthersTagColor);
    gSavedSettings.setBOOL("AscentShowIdleTime",         mShowIdleTime);
    gSavedSettings.setBOOL("AscentUseStatusColors",      mUseStatusColors);
    gSavedSettings.setBOOL("AscentUpdateTagsOnLoad",     mUpdateTagsOnLoad);
    gSavedSettings.setColor4("EffectColor",              mEffectColor);
    gSavedSettings.setColor4("AscentFriendColor",        mFriendColor);
    gSavedSettings.setColor4("AscentEstateOwnerColor",   mEstateOwnerColor);
    gSavedSettings.setColor4("AscentLindenColor",        mLindenColor);
    gSavedSettings.setColor4("AscentMutedColor",         mMutedColor);
//    gSavedSettings.setColor4("MoyMiniMapCustomColor",    mCustomColor);

    //Body Dynamics --------------------------------------------------------------------------
    gSavedSettings.setBOOL("EmeraldBreastPhysicsToggle", mBreastPhysicsToggle);
    gSavedSettings.setF32("EmeraldBoobMass",             mBoobMass);
    gSavedSettings.setF32("EmeraldBoobHardness",         mBoobHardness);
    gSavedSettings.setF32("EmeraldBoobVelMax",           mBoobVelMax);
    gSavedSettings.setF32("EmeraldBoobFriction",         mBoobFriction);
    gSavedSettings.setF32("EmeraldBoobVelMin",           mBoobVelMin);

    gSavedSettings.setF32("AscentAvatarXModifier",       mAvatarXModifier);
    gSavedSettings.setF32("AscentAvatarYModifier",       mAvatarYModifier);
    gSavedSettings.setF32("AscentAvatarZModifier",       mAvatarZModifier);
}

// Update local copy so cancel has no effect
void LLPrefsAscentVan::apply()
{
    refreshValues();
    refresh();
	if (gAgentAvatarp) //check if we got inworld
	{
		gAgent.resetClientTag();
		gAgent.sendAgentSetAppearance();
	}
}
