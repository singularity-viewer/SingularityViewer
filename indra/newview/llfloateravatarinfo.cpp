/** 
 * @file llfloateravatarinfo.cpp
 * @brief LLFloaterAvatarInfo class implementation
 * Avatar information as shown in a floating window from right-click
 * Profile.  Used for editing your own avatar info.  Just a wrapper
 * for LLPanelAvatar, shared with the Find directory.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llfloateravatarinfo.h"
#include "llavatarnamecache.h"

// viewer project includes
#include "llagentdata.h"
#include "llcommandhandler.h"
#include "llpanelavatar.h"
#include "lluictrlfactory.h"
#include "llweb.h"

// linden library includes
#include "llinventory.h"
#include "lluuid.h"
#include "message.h"


const char FLOATER_TITLE[] = "Profile";
const LLRect FAI_RECT(0, 530, 420, 0);

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

class LLAgentHandler : public LLCommandHandler
{
public:
	void verbCallback(const std::string& verb, LLUUID agent_id, const LLAvatarName& avatar_name)
	{
		if (verb == "im")
		{
			gIMMgr->setFloaterOpen(TRUE);
			gIMMgr->addSession( avatar_name.getCompleteName(), IM_NOTHING_SPECIAL, agent_id);
			return;
		}

		if (verb == "requestfriend")
		{
			LLPanelFriends::requestFriendshipDialog( agent_id, avatar_name.getCompleteName() );
			return;
		}

		if (verb == "mute")
		{
			LLFloaterMute::getInstance()->open();
			LLMute mute(agent_id, avatar_name.getCompleteName(), LLMute::AGENT);
			LLMuteList::getInstance()->add(mute);
			return;
		}

		if (verb == "unmute")
		{
			LLMute mute(agent_id, avatar_name.getCompleteName(), LLMute::AGENT);
			LLMuteList::getInstance()->remove(mute);
			return;
		}
	}

	// requires trusted browser to trigger
	LLAgentHandler() : LLCommandHandler("agent", true) { }

	bool handle(const LLSD& params, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (params.size() < 2) return false;
		LLUUID agent_id;
		if (!agent_id.set(params[0], FALSE))
		{
			return false;
		}

		{
			LLFloaterAvatarInfo::show(agent_id);
			return true;
		}

		if (verb == "pay")
		{
			handle_pay_by_id(agent_id);
			return true;
		}

		if (verb == "offerteleport")
		{
			handle_lure(agent_id);
			return true;
		}

		if ((verb == "im") || (verb == "requestfriend") || (verb == "unmute"))
		{
			LLAvatarNameCache::get(agent_id,   boost::bind(&LLAgentHandler::verbCallback, this, verb, _1, _2));
			return true;
		}
		
		if (verb == "mute")
		{
			if (LLMuteList::getInstance()->isMuted(agent_id))
			{
				LLFloaterMute::getInstance()->open();
				LLFloaterMute::getInstance()->selectMute(agent_id);
			}
			else
			{
				LLAvatarNameCache::get(agent_id, boost::bind(&LLAgentHandler::verbCallback, this, verb, _1, _2));
			}
			return true;
		}


		return false;
	}
};
LLAgentHandler gAgentHandler;

//-----------------------------------------------------------------------------
// Member functions
//-----------------------------------------------------------------------------

//----------------------------------------------------------------------------

void*	LLFloaterAvatarInfo::createPanelAvatar(void*	data)
{
	LLFloaterAvatarInfo* self = (LLFloaterAvatarInfo*)data;
	self->mPanelAvatarp = new LLPanelAvatar("PanelAv", LLRect(), TRUE); // allow edit self
	return self->mPanelAvatarp;
}

//----------------------------------------------------------------------------


BOOL	LLFloaterAvatarInfo::postBuild()
{
	return TRUE;
}

LLFloaterAvatarInfo::LLFloaterAvatarInfo(const std::string& name, const LLRect &rect, const LLUUID &avatar_id)
:	LLPreview(name, rect, FLOATER_TITLE, LLUUID::null, LLUUID::null), LLInstanceTracker<LLFloaterAvatarInfo, LLUUID>(avatar_id),
	mAvatarID( avatar_id ),
	mSuggestedOnlineStatus(ONLINE_STATUS_NO)
{
	setAutoFocus(TRUE);

	LLCallbackMap::map_t factory_map;

	factory_map["Panel Avatar"] = LLCallbackMap(createPanelAvatar, this);
	
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_profile.xml", &factory_map);

	if(mPanelAvatarp)
	{
		mPanelAvatarp->selectTab(0);
	}

	//gCacheName->get(avatar_id, FALSE, callbackLoadAvatarName);
	LLAvatarNameCache::get(avatar_id, boost::bind(&LLFloaterAvatarInfo::callbackLoadAvatarName, this, _1, _2));
}

// virtual
LLFloaterAvatarInfo::~LLFloaterAvatarInfo()
{
}

void LLFloaterAvatarInfo::resetGroupList()
{
	// only get these updates asynchronously via the group floater, which works on the agent only
	if (mAvatarID != gAgentID)
	{
		return;
	}

	mPanelAvatarp->resetGroupList();
}

// static
LLFloaterAvatarInfo* LLFloaterAvatarInfo::show(const LLUUID &avatar_id)
{
	if (avatar_id.isNull())
	{
		return NULL;
	}

	LLFloaterAvatarInfo *floater = LLFloaterAvatarInfo::getInstance(avatar_id);
	if(!floater)
	{
		floater =  new LLFloaterAvatarInfo("avatarinfo", FAI_RECT, avatar_id );
		floater->center();
	}

	// ...bring that window to front
	floater->open();	/*Flawfinder: ignore*/
	return floater;
}

// Open profile to a certain tab.
// static
void LLFloaterAvatarInfo::showFromObject(const LLUUID& avatar_id,std::string tab_name)
{
	LLFloaterAvatarInfo *floater = show(avatar_id);
	if (floater)
	{
		floater->mPanelAvatarp->setAvatarID(avatar_id, LLStringUtil::null, ONLINE_STATUS_NO);
		floater->mPanelAvatarp->selectTabByName(tab_name);
	}
}

// static
void LLFloaterAvatarInfo::showFromDirectory(const LLUUID &avatar_id)
{
	LLFloaterAvatarInfo *floater = show(avatar_id);
	if (floater)
	{
		floater->mPanelAvatarp->setAvatarID(avatar_id, LLStringUtil::null, ONLINE_STATUS_NO);
	}
}


// static
void LLFloaterAvatarInfo::showFromFriend(const LLUUID& agent_id, BOOL online)
{
	LLFloaterAvatarInfo *floater = show(agent_id);
	if (floater)
	{
		floater->mSuggestedOnlineStatus = online ? ONLINE_STATUS_YES : ONLINE_STATUS_NO;
	}
}


// static
void LLFloaterAvatarInfo::showFromProfile(const LLUUID &avatar_id, LLRect rect)
{
	if (avatar_id.isNull())
	{
		return;
	}

	LLFloaterAvatarInfo *floater = LLFloaterAvatarInfo::getInstance(avatar_id);
	if(!floater)
	{
		floater =  new LLFloaterAvatarInfo("avatarinfo", FAI_RECT, avatar_id);
		floater->translate(rect.mLeft - floater->getRect().mLeft + 16,
						   rect.mTop - floater->getRect().mTop - 16);
		floater->mPanelAvatarp->setAvatarID(avatar_id, LLStringUtil::null, ONLINE_STATUS_NO);
	}
	floater->open();
}

void LLFloaterAvatarInfo::showProfileCallback(S32 option, void *userdata)
{
	if (option == 0)
	{
		showFromObject(gAgentID);
	}
}

void LLFloaterAvatarInfo::callbackLoadAvatarName(const LLUUID& id, const LLAvatarName& av_name)
{
	// Build a new title including the avatar name.
	std::ostringstream title;
	//title << first << " " << last << " - " << floater->getTitle();
	title << av_name.getCompleteName()<< " - " << getTitle();
	setTitle(title.str());
}

//// virtual
void LLFloaterAvatarInfo::draw()
{
	// skip LLPreview::draw()
	LLFloater::draw();
}

// virtual
BOOL LLFloaterAvatarInfo::canClose()
{
	return mPanelAvatarp && mPanelAvatarp->canClose();
}

void LLFloaterAvatarInfo::loadAsset()
{
	if (mPanelAvatarp) {
		mPanelAvatarp->setAvatarID(mAvatarID, LLStringUtil::null, mSuggestedOnlineStatus);
		mAssetStatus = PREVIEW_ASSET_LOADING;
	}
}

LLPreview::EAssetStatus LLFloaterAvatarInfo::getAssetStatus()
{
	if (mPanelAvatarp && mPanelAvatarp->haveData())
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	return mAssetStatus;
}
