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
#include "llvoavatar.h"
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

class LLPrefsAscentVanImpl : public LLPanel
{
public:
	LLPrefsAscentVanImpl();
	/*virtual*/ ~LLPrefsAscentVanImpl() { };

	virtual void refresh();

	void apply();
	void cancel();

private:
	static void onCommitCheckBox(LLUICtrl* ctrl, void* user_data);
	static void onCommitColor(LLUICtrl* ctrl, void* user_data);
	static void onManualClientUpdate(void* data);
	void refreshValues();
	//General
	BOOL mUseAccountSettings;
	BOOL mShowTPScreen;
	BOOL mPlayTPSound;
	BOOL mShowLogScreens;
	//Colors
	BOOL mShowSelfClientTag;
	BOOL mShowSelfClientTagColor;
	BOOL mCustomTagOn;
	std::string mCustomTagLabel;
	LLColor4 mCustomTagColor;
	LLColor4 mEffectColor;
	LLColor4 mFriendColor;
	LLColor4 mLindenColor;
	LLColor4 mMutedColor;
	LLColor4 mEMColor;
	U32 mSelectedClient;
};


LLPrefsAscentVanImpl::LLPrefsAscentVanImpl()
 : LLPanel(std::string("Ascent"))
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_ascent_vanity.xml");
	childSetCommitCallback("use_account_settings_check", onCommitCheckBox, this);
	childSetCommitCallback("customize_own_tag_check", onCommitCheckBox, this);
	childSetCommitCallback("show_friend_tag_check", onCommitCheckBox, this);
	childSetCommitCallback("use_status_check", onCommitCheckBox, this);

	childSetCommitCallback("custom_tag_color_swatch", onCommitColor, this);
	childSetCommitCallback("effect_color_swatch", onCommitColor, this);

	childSetCommitCallback("X Modifier", LLPrefsAscentVan::onCommitUpdateAvatarOffsets);
	childSetCommitCallback("Y Modifier", LLPrefsAscentVan::onCommitUpdateAvatarOffsets);
	childSetCommitCallback("Z Modifier", LLPrefsAscentVan::onCommitUpdateAvatarOffsets);
	
	childSetAction("update_clientdefs", onManualClientUpdate, this);
	//load client tags
	if(!LLVOAvatar::sClientResolutionList.has("isComplete"))
		LLVOAvatar::loadClientTags();
	refresh();
	
}

void LLPrefsAscentVan::onCommitUpdateAvatarOffsets(LLUICtrl* ctrl, void* userdata)
{
	gAgent.sendAgentSetAppearance();
	//llinfos << llformat("%d,%d,%d",gSavedSettings.getF32("EmeraldAvatarXModifier"),gSavedSettings.getF32("EmeraldAvatarYModifier"),gSavedSettings.getF32("EmeraldAvatarZModifier")) << llendl;
}

void LLPrefsAscentVanImpl::onCommitColor(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsAscentVanImpl* self = (LLPrefsAscentVanImpl*)user_data;

	llinfos << "Control named " << ctrl->getControlName() << " aka " << ctrl->getName() << llendl;

	if (ctrl->getName() == "custom_tag_color_swatch")
	{
		
		llinfos << "Recreating color message for tag update." << llendl;
		gSavedSettings.setString("AscentCustomTagLabel",		self->childGetValue("custom_tag_label_box"));
		gSavedSettings.setColor4("AscentCustomTagColor",		self->childGetValue("custom_tag_color_swatch"));
		gAgent.sendAgentSetAppearance();
		gAgent.resetClientTag();
	}
}

void LLPrefsAscentVanImpl::onManualClientUpdate(void* data)
{
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
void LLPrefsAscentVanImpl::onCommitCheckBox(LLUICtrl* ctrl, void* user_data)
{
	
	LLPrefsAscentVanImpl* self = (LLPrefsAscentVanImpl*)user_data;

	llinfos << "Control named " << ctrl->getControlName() << llendl;
	if (ctrl->getControlName() == "AscentStoreSettingsPerAccount")
	{
		self->refresh();
	}

	if ((ctrl->getName() == "show_friend_tag_check")||(ctrl->getName() == "use_status_check"))
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

	BOOL showCustomOptions;
	showCustomOptions = gSavedSettings.getBOOL("AscentUseCustomTag");
	self->childSetValue("customize_own_tag_check", showCustomOptions);
	self->childSetEnabled("custom_tag_label_text", showCustomOptions);
	self->childSetEnabled("custom_tag_label_box", showCustomOptions);
	self->childSetEnabled("custom_tag_color_text", showCustomOptions);
	self->childSetEnabled("custom_tag_color_swatch", showCustomOptions);
	if (!gAgent.getID().isNull())
		gAgent.resetClientTag();
}

void LLPrefsAscentVanImpl::refreshValues()
{
	//General
	mUseAccountSettings			= gSavedSettings.getBOOL("AscentStoreSettingsPerAccount");
	mShowTPScreen				= !gSavedSettings.getBOOL("AscentDisableTeleportScreens");
	mPlayTPSound				= gSavedSettings.getBOOL("OptionPlayTpSound");
	mShowLogScreens				= !gSavedSettings.getBOOL("AscentDisableLogoutScreens");
	
	//Colors
	mShowSelfClientTag			= gSavedSettings.getBOOL("AscentShowSelfTag");
	mShowSelfClientTagColor		= gSavedSettings.getBOOL("AscentShowSelfTagColor");	
	mCustomTagOn				= gSavedSettings.getBOOL("AscentUseCustomTag");

	mSelectedClient			= gSavedSettings.getU32("AscentReportClientIndex");
	mEffectColor			= gSavedSettings.getColor4("EffectColor");	
	
	childSetEnabled("custom_tag_label_text", mCustomTagOn);
	childSetEnabled("custom_tag_label_box", mCustomTagOn);
	childSetEnabled("custom_tag_color_text", mCustomTagOn);
	childSetEnabled("custom_tag_color_swatch", mCustomTagOn);

	mCustomTagLabel			= gSavedSettings.getString("AscentCustomTagLabel");
	mCustomTagColor			= gSavedSettings.getColor4("AscentCustomTagColor");
	mFriendColor			= gSavedSettings.getColor4("AscentFriendColor");
	mLindenColor			= gSavedSettings.getColor4("AscentLindenColor");
	mMutedColor				= gSavedSettings.getColor4("AscentMutedColor");
	mEMColor				= gSavedSettings.getColor4("AscentEstateOwnerColor");
	//mCustomColor			= gSavedSettings.getColor4("MoyMiniMapCustomColor");
}

void LLPrefsAscentVanImpl::refresh()
{
	refreshValues();
	//General --------------------------------------------------------------------------------
	childSetValue("use_account_settings_check", mUseAccountSettings);
	childSetValue("disable_tp_screen_check",	mShowTPScreen);
	childSetValue("tp_sound_check",				mPlayTPSound);
	childSetValue("disable_logout_screen_check", mShowLogScreens);

	//Colors ---------------------------------------------------------------------------------
	LLComboBox* combo = getChild<LLComboBox>("tag_spoofing_combobox");
	if(LLVOAvatar::sClientResolutionList.has("isComplete"))
	{
		combo->setColor(LLColor4::black);
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
		item->getColumn(0)->setColor(LLColor4::white);
	}
	combo->setCurrentByIndex(mSelectedClient);

	childSetValue("show_self_tag_check", mShowSelfClientTag);
	childSetValue("show_self_tag_color_check", mShowSelfClientTagColor);
	childSetValue("customize_own_tag_check", mCustomTagOn);
	childSetValue("custom_tag_label_box", mCustomTagLabel);
	
	getChild<LLColorSwatchCtrl>("effect_color_swatch")->set(mEffectColor);
	getChild<LLColorSwatchCtrl>("custom_tag_color_swatch")->set(mCustomTagColor);
	getChild<LLColorSwatchCtrl>("friend_color_swatch")->set(mFriendColor);
	getChild<LLColorSwatchCtrl>("linden_color_swatch")->set(mLindenColor);
	getChild<LLColorSwatchCtrl>("muted_color_swatch")->set(mMutedColor);
	getChild<LLColorSwatchCtrl>("em_color_swatch")->set(mEMColor);
	//getChild<LLColorSwatchCtrl>("custom_color_swatch")->set(mCustomColor);
	gSavedSettings.setColor4("EffectColor", LLColor4::white);
	gSavedSettings.setColor4("EffectColor", mEffectColor);
	
	gSavedSettings.setColor4("AscentFriendColor", LLColor4::white);
	gSavedSettings.setColor4("AscentFriendColor", mFriendColor);

	gSavedSettings.setColor4("AscentLindenColor", LLColor4::white);
	gSavedSettings.setColor4("AscentLindenColor", mLindenColor);

	gSavedSettings.setColor4("AscentMutedColor", LLColor4::white);
	gSavedSettings.setColor4("AscentMutedColor", mMutedColor);

	gSavedSettings.setColor4("AscentEstateOwnerColor", LLColor4::white);
	gSavedSettings.setColor4("AscentEstateOwnerColor", mEMColor);

	//gSavedSettings.setColor4("MoyMiniMapCustomColor", LLColor4::white);
	//gSavedSettings.setColor4("MoyMiniMapCustomColor", mCustomColor);
	gAgent.resetClientTag();
}

void LLPrefsAscentVanImpl::cancel()
{
	//General --------------------------------------------------------------------------------
	childSetValue("use_account_settings_check", mUseAccountSettings);
	childSetValue("disable_tp_screen_check",	mShowTPScreen);
	childSetValue("tp_sound_check",				mPlayTPSound);
	childSetValue("disable_logout_screen_check", mShowLogScreens);

	gSavedSettings.setColor4("EffectColor", LLColor4::white);
	gSavedSettings.setColor4("EffectColor", mEffectColor);
	gSavedSettings.setColor4("AscentFriendColor", LLColor4::yellow);
	gSavedSettings.setColor4("AscentFriendColor", mFriendColor);
	gSavedSettings.setColor4("AscentLindenColor", LLColor4::yellow);
	gSavedSettings.setColor4("AscentLindenColor", mLindenColor);
	gSavedSettings.setColor4("AscentMutedColor", LLColor4::yellow);
	gSavedSettings.setColor4("AscentMutedColor", mMutedColor);
	gSavedSettings.setColor4("AscentEstateOwnerColor", LLColor4::yellow);
	gSavedSettings.setColor4("AscentEstateOwnerColor", mEMColor);
	//gSavedSettings.setColor4("MoyMiniMapCustomColor", LLColor4::yellow);
	//gSavedSettings.setColor4("MoyMiniMapCustomColor", mCustomColor);
}

void LLPrefsAscentVanImpl::apply()
{
	std::string client_uuid;
	U32 client_index;
	//General -----------------------------------------------------------------------------
	gSavedSettings.setBOOL("AscentDisableTeleportScreens",	!childGetValue("disable_tp_screen_check"));
	gSavedSettings.setBOOL("OptionPlayTpSound",				childGetValue("tp_sound_check"));
	gSavedSettings.setBOOL("AscentDisableLogoutScreens",	!childGetValue("disable_logout_screen_check"));

	//Colors ------------------------------------------------------------------------------
	LLComboBox* combo = getChild<LLComboBox>("tag_spoofing_combobox");
	if (combo)
	{
		client_index = combo->getCurrentIndex();
		//Don't rebake if it's not neccesary.
		if (client_index != mSelectedClient)
		{
			client_uuid = combo->getSelectedValue().asString();
			gSavedSettings.setString("AscentReportClientUUID",  client_uuid);
			gSavedSettings.setU32("AscentReportClientIndex",  client_index);
			LLVOAvatar* avatar = gAgent.getAvatarObject();
			if (!avatar) return;

			gAgent.sendAgentSetAppearance();
		}
	}
	gSavedSettings.setBOOL("AscentShowSelfTag",			childGetValue("show_self_tag_check"));
	gSavedSettings.setBOOL("AscentShowSelfTagColor",	childGetValue("show_self_tag_color_check"));

	gSavedSettings.setColor4("EffectColor",				childGetValue("effect_color_swatch"));
	gSavedSettings.setColor4("AscentFriendColor",			childGetValue("friend_color_swatch"));
	gSavedSettings.setColor4("AscentLindenColor",			childGetValue("linden_color_swatch"));
	gSavedSettings.setColor4("AscentMutedColor",			childGetValue("muted_color_swatch"));
	gSavedSettings.setColor4("AscentEstateOwnerColor",		childGetValue("em_color_swatch"));
	gSavedSettings.setColor4("MoyMiniMapCustomColor",		childGetValue("custom_color_swatch"));
	gSavedSettings.setBOOL("AscentUseCustomTag",			childGetValue("customize_own_tag_check"));
	gSavedSettings.setString("AscentCustomTagLabel",		childGetValue("custom_tag_label_box"));
	gSavedSettings.setColor4("AscentCustomTagColor",		childGetValue("custom_tag_color_swatch"));
	
	refreshValues();
}

//---------------------------------------------------------------------------

LLPrefsAscentVan::LLPrefsAscentVan()
:	impl(* new LLPrefsAscentVanImpl())
{
}

LLPrefsAscentVan::~LLPrefsAscentVan()
{
	delete &impl;
}

void LLPrefsAscentVan::apply()
{
	impl.apply();
}

void LLPrefsAscentVan::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsAscentVan::getPanel()
{
	return &impl;
}
