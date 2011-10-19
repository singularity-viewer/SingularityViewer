/** 
 * @file llfloaterobjectiminfo.cpp
 * @brief A floater with information about an object that sent an IM.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterobjectiminfo.h"

#include "llagentdata.h"
#include "llcachename.h"
#include "llcommandhandler.h"
#include "llfloater.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroupinfo.h"
#include "llfloatermute.h"
#include "llmutelist.h"
#include "llsdutil.h"
#include "lluictrlfactory.h"
#include "llurldispatcher.h"
#include "llviewercontrol.h"

// [RLVa:KB] - Version: 1.23.4
#include "rlvhandler.h"
// [/RLVa:KB]

////////////////////////////////////////////////////////////////////////////
// LLFloaterObjectIMInfo
class LLFloaterObjectIMInfo : public LLFloater, public LLFloaterSingleton<LLFloaterObjectIMInfo>
{
public:
	LLFloaterObjectIMInfo(const LLSD& sd);
	virtual ~LLFloaterObjectIMInfo() { };

	BOOL postBuild(void);

	void update(const LLUUID& id, const std::string& name, const std::string& slurl, const LLUUID& owner, bool owner_is_group);

	// UI Handlers
	static void onClickMap(void* data);
	static void onClickOwner(void* data);
	static void onClickMute(void* data);

	void nameCallback(const LLUUID& id, const std::string& full_name, bool is_group);

private:
	LLUUID mObjectID;
	std::string mObjectName;
	std::string mSlurl;
	LLUUID mOwnerID;
	std::string mOwnerName;
	bool mOwnerIsGroup;
};

LLFloaterObjectIMInfo::LLFloaterObjectIMInfo(const LLSD& seed)
: mObjectID(), mObjectName(), mSlurl(), mOwnerID(), mOwnerName(), mOwnerIsGroup(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_object_im_info.xml");
	
	if (getRect().mLeft == 0 
		&& getRect().mBottom == 0)
	{
		center();
	}
}

BOOL LLFloaterObjectIMInfo::postBuild(void)
{
	childSetAction("Mute",onClickMute,this);
	childSetActionTextbox("OwnerName",onClickOwner, this);
	childSetActionTextbox("Slurl",onClickMap, this);

	return true;
}

void LLFloaterObjectIMInfo::update(const LLUUID& object_id, const std::string& name, const std::string& slurl, const LLUUID& owner_id, bool owner_is_group)
{
	// When talking to an old region we won't have a slurl.
	// The object id isn't really the object id either but we don't use it so who cares.
	//bool have_slurl = !slurl.empty();
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a) | Added: RLVa-0.2.0g
	bool have_slurl = (!slurl.empty()) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));
// [/RLVa:KB]
	childSetVisible("Unknown_Slurl",!have_slurl);
	childSetVisible("Slurl",have_slurl);

	childSetText("ObjectName",name);
	childSetText("Slurl",slurl);
	childSetText("OwnerName",std::string(""));

//	bool my_object = (owner_id == gAgentID);
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	bool my_object = (owner_id == gAgentID) || ((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(owner_id)));
// [/RLVa:KB]
	childSetEnabled("Mute",!my_object);
	
	mObjectID = object_id;
	mObjectName = name;
	mSlurl = slurl;
	mOwnerID = owner_id;
	mOwnerIsGroup = owner_is_group;

	if (gCacheName) gCacheName->get(owner_id,owner_is_group,boost::bind(&LLFloaterObjectIMInfo::nameCallback,this,_1,_2,_3));
}

//static 
void LLFloaterObjectIMInfo::onClickMap(void* data)
{
	LLFloaterObjectIMInfo* self = (LLFloaterObjectIMInfo*)data;

	std::ostringstream link;
	link << "secondlife://" << self->mSlurl;
	class LLMediaCtrl* web = NULL;
	LLURLDispatcher::dispatch(link.str(), web, true);
}

//static 
void LLFloaterObjectIMInfo::onClickOwner(void* data)
{
	LLFloaterObjectIMInfo* self = (LLFloaterObjectIMInfo*)data;
	if (self->mOwnerIsGroup)
	{
		LLFloaterGroupInfo::showFromUUID(self->mOwnerID);
	}
//	else
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	else if ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) || (!RlvUtil::isNearbyAgent(self->mOwnerID)) )
// [/RLVa:KB]
	{
		LLFloaterAvatarInfo::showFromObject(self->mOwnerID);
	}
}

//static 
void LLFloaterObjectIMInfo::onClickMute(void* data)
{
	LLFloaterObjectIMInfo* self = (LLFloaterObjectIMInfo*)data;

	LLMute::EType mute_type = (self->mOwnerIsGroup) ? LLMute::GROUP : LLMute::AGENT;
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	if ( (LLMute::GROUP != mute_type) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(self->mOwnerID)) )
	{
		return;
	}
// [/RLVa:KB]

	LLMute mute(self->mOwnerID, self->mOwnerName, mute_type);
	LLMuteList::getInstance()->add(mute);
	LLFloaterMute::showInstance();
	self->close();
}

//static 
void LLFloaterObjectIMInfo::nameCallback(const LLUUID& id, const std::string& full_name, bool is_group)
{
	mOwnerName = full_name;
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	if ( (!is_group) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(id)) )
	{
		mOwnerName = RlvStrings::getAnonym(mOwnerName);
	}
// [/RLVa:KB]

	childSetText("OwnerName", mOwnerName);
}

////////////////////////////////////////////////////////////////////////////
// LLObjectIMInfo
void LLObjectIMInfo::show(const LLUUID &object_id, const std::string &name, const std::string &location, const LLUUID &owner_id, bool owner_is_group)
{
	LLFloaterObjectIMInfo* im_info_floater = LLFloaterObjectIMInfo::showInstance();
	im_info_floater->update(object_id,name,location,owner_id,owner_is_group);
}

////////////////////////////////////////////////////////////////////////////
// LLObjectIMInfoHandler
class LLObjectIMInfoHandler : public LLCommandHandler
{
public:
	LLObjectIMInfoHandler() : LLCommandHandler("objectim", true) { }

	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web);
};

// Creating the object registers with the dispatcher.
LLObjectIMInfoHandler gObjectIMHandler;

// ex. secondlife:///app/objectim/9426adfc-9c17-8765-5f09-fdf19957d003?owner=a112d245-9095-4e9c-ace4-ffa31717f934&groupowned=true&slurl=ahern/123/123/123&name=Object
bool LLObjectIMInfoHandler::handle(const LLSD &tokens, const LLSD &query_map, LLMediaCtrl* web)
{
	LLUUID task_id = tokens[0].asUUID();
	std::string name = query_map["name"].asString();
	std::string slurl = query_map["slurl"].asString();
	LLUUID owner = query_map["owner"].asUUID();
	bool group_owned = query_map.has("groupowned");
	
	LLObjectIMInfo::show(task_id,name,slurl,owner,group_owned);

	return true;
}
