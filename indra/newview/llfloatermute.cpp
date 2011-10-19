/** 
 * @file llfloatermute.cpp
 * @brief Container for mute list
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

#include "llviewerprecompiledheaders.h"

#include "llfloatermute.h"

#include "llfontgl.h"
#include "llrect.h"
#include "llerror.h"
#include "llstring.h"
#include "message.h"
#include "llnotificationsutil.h"

// project include
#include "llagent.h"
#include "llfloateravatarpicker.h"
#include "llbutton.h"
#include "lllineeditor.h"
#include "llmutelist.h"
#include "llresizehandle.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "lluictrlfactory.h"
#include "llfocusmgr.h"

//
// Constants
//
const std::string FLOATER_TITLE = "Muted Residents & Objects";
const F32 INSTANT_MSG_SIZE = 8.0f;
const LLColor4 INSTANT_MSG_COLOR(1, 1, 1, 1);
const LLColor4 MUTED_MSG_COLOR(0.5f, 0.5f, 0.5f, 1.f);

const S32 LINE = 16;
const S32 LEFT = 2;
const S32 VPAD = 4;
const S32 HPAD = 4;

//-----------------------------------------------------------------------------
// LLFloaterMuteObjectUI()
//-----------------------------------------------------------------------------
// Class for handling mute object by name floater.
class LLFloaterMuteObjectUI : public LLFloater
{
public:
	typedef void(*callback_t)(const std::string&, void*);

	static LLFloaterMuteObjectUI* show(callback_t callback,
					   void* userdata);
	virtual BOOL postBuild();

protected:
	LLFloaterMuteObjectUI();
	virtual ~LLFloaterMuteObjectUI();
	virtual BOOL handleKeyHere(KEY key, MASK mask);

private:
	// UI Callbacks
	static void onBtnOk(void *data);
	static void onBtnCancel(void *data);

	void (*mCallback)(const std::string& objectName, 
			  void* userdata);
	void* mCallbackUserData;

	static LLFloaterMuteObjectUI* sInstance;
};

LLFloaterMuteObjectUI* LLFloaterMuteObjectUI::sInstance = NULL;

LLFloaterMuteObjectUI::LLFloaterMuteObjectUI()
	: LLFloater(std::string("Mute object by name")),
	  mCallback(NULL),
	  mCallbackUserData(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_mute_object.xml", NULL);
}

// Destroys the object
LLFloaterMuteObjectUI::~LLFloaterMuteObjectUI()
{
	gFocusMgr.releaseFocusIfNeeded( this );
	sInstance = NULL;
}

LLFloaterMuteObjectUI* LLFloaterMuteObjectUI::show(callback_t callback,
						   void* userdata)
{
	const bool firstInstantiation = (sInstance == NULL);
	if (firstInstantiation)
	{
		sInstance = new LLFloaterMuteObjectUI;
	}
	sInstance->mCallback = callback;
	sInstance->mCallbackUserData = userdata;
  
	sInstance->open();
	if (firstInstantiation)
	{
		sInstance->center();
	}

	return sInstance;
}


BOOL LLFloaterMuteObjectUI::postBuild()
{
	childSetAction("OK", onBtnOk, this);
	childSetAction("Cancel", onBtnCancel, this);
	return TRUE;
}

void LLFloaterMuteObjectUI::onBtnOk(void* userdata)
{
	LLFloaterMuteObjectUI* self = (LLFloaterMuteObjectUI*)userdata;
	if (!self) return;

	if (self->mCallback)
	{
		const std::string& text = self->childGetValue("object_name").asString();
		self->mCallback(text,self->mCallbackUserData);
	}
	self->close();
}

void LLFloaterMuteObjectUI::onBtnCancel(void* userdata)
{
	LLFloaterMuteObjectUI* self = (LLFloaterMuteObjectUI*)userdata;
	if (!self) return;

	self->close();
}

BOOL LLFloaterMuteObjectUI::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		onBtnOk(this);
		return TRUE;
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		onBtnCancel(this);
		return TRUE;
	}

	return LLFloater::handleKeyHere(key, mask);
}

//
// Member Functions
//

//-----------------------------------------------------------------------------
// LLFloaterMute()
//-----------------------------------------------------------------------------
LLFloaterMute::LLFloaterMute(const LLSD& seed)
:	LLFloater(std::string("mute floater"), std::string("FloaterMuteRect3"), FLOATER_TITLE, 
			  RESIZE_YES, 220, 140, DRAG_ON_TOP, MINIMIZE_YES, CLOSE_YES)
{

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_mute.xml", NULL, FALSE);
}

// LLMuteListObserver callback interface implementation.
/* virtual */ void LLFloaterMute::onChange()
{
	refreshMuteList();
}

BOOL LLFloaterMute::postBuild()
{
	childSetCommitCallback("mutes", onSelectName, this);
	childSetAction("Mute resident...", onClickPick, this);
	childSetAction("Mute object by name...", onClickMuteByName, this);
	childSetAction("Unmute", onClickRemove, this);

	mMuteList = getChild<LLScrollListCtrl>("mutes");
	mMuteList->setCommitOnSelectionChange(TRUE);

	LLMuteList::getInstance()->addObserver(this);
	
	refreshMuteList();

	return TRUE;
}

//-----------------------------------------------------------------------------
// ~LLFloaterMute()
//-----------------------------------------------------------------------------
LLFloaterMute::~LLFloaterMute()
{
}

//-----------------------------------------------------------------------------
// refreshMuteList()
//-----------------------------------------------------------------------------
void LLFloaterMute::refreshMuteList()
{
	mMuteList->deleteAllItems();

	std::vector<LLMute> mutes = LLMuteList::getInstance()->getMutes();
	std::vector<LLMute>::iterator it;
	for (it = mutes.begin(); it != mutes.end(); ++it)
	{
		std::string display_name = it->getDisplayName();
		mMuteList->addStringUUIDItem(display_name, it->mID);
	}

	updateButtons();
}

void LLFloaterMute::selectMute(const LLUUID& mute_id)
{
	mMuteList->selectByID(mute_id);
	updateButtons();
}

//-----------------------------------------------------------------------------
// updateButtons()
//-----------------------------------------------------------------------------
void LLFloaterMute::updateButtons()
{
	if (mMuteList->getFirstSelected())
	{
		childSetEnabled("Unmute", TRUE);
	}
	else
	{
		childSetEnabled("Unmute", FALSE);
	}
}

//-----------------------------------------------------------------------------
// onSelectName()
//-----------------------------------------------------------------------------
void LLFloaterMute::onSelectName(LLUICtrl *caller, void *data)
{
	LLFloaterMute *floater = (LLFloaterMute*)data;

	floater->updateButtons();
}

//-----------------------------------------------------------------------------
// onClickRemove()
//-----------------------------------------------------------------------------
void LLFloaterMute::onClickRemove(void *data)
{
	LLFloaterMute* floater = (LLFloaterMute *)data;

	std::string name = floater->mMuteList->getSelectedItemLabel();
	LLUUID id = floater->mMuteList->getStringUUIDSelectedItem();
	LLMute mute(id);
	mute.setFromDisplayName(name);
	// now mute.mName has the suffix trimmed off
	
	S32 last_selected = floater->mMuteList->getFirstSelectedIndex();
	if (LLMuteList::getInstance()->remove(mute))
	{
		// Above removals may rebuild this dialog.
		
		if (last_selected == floater->mMuteList->getItemCount())
		{
			// we were on the last item, so select the last item again
			floater->mMuteList->selectNthItem(last_selected - 1);
		}
		else
		{
			// else select the item after the last item previously selected
			floater->mMuteList->selectNthItem(last_selected);
		}
	}
	floater->updateButtons();
}

//-----------------------------------------------------------------------------
// onClickPick()
//-----------------------------------------------------------------------------
void LLFloaterMute::onClickPick(void *data)
{
	LLFloaterMute* floaterp = (LLFloaterMute*)data;
	const BOOL allow_multiple = FALSE;
	const BOOL close_on_select = TRUE;
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(onPickUser, data, allow_multiple, close_on_select);
	floaterp->addDependentFloater(picker);
}

//-----------------------------------------------------------------------------
// onPickUser()
//-----------------------------------------------------------------------------
void LLFloaterMute::onPickUser(const std::vector<std::string>& names, const std::vector<LLUUID>& ids, void* user_data)
{
	LLFloaterMute* floaterp = (LLFloaterMute*)user_data;
	if (!floaterp) return;
	if (names.empty() || ids.empty()) return;

	LLMute mute(ids[0], names[0], LLMute::AGENT);
	LLMuteList::getInstance()->add(mute);
	floaterp->updateButtons();
}


void LLFloaterMute::onClickMuteByName(void* data)
{
	LLFloaterMuteObjectUI* picker = LLFloaterMuteObjectUI::show(callbackMuteByName,data);
	assert(picker);

	LLFloaterMute* floaterp = (LLFloaterMute*)data;
	floaterp->addDependentFloater(picker);
}

void LLFloaterMute::callbackMuteByName(const std::string& text, void* data)
{
	if (text.empty()) return;

	LLMute mute(LLUUID::null, text, LLMute::BY_NAME);
	BOOL success = LLMuteList::getInstance()->add(mute);
	if (!success)
	{
		LLNotificationsUtil::add("MuteByNameFailed");
	}
}
