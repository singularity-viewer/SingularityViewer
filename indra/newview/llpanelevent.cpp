/** 
 * @file llpanelevent.cpp
 * @brief Display for events in the finder
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llpanelevent.h"
#include "message.h"
#include "llui.h"

#include "llagent.h"
#include "llviewerwindow.h"
#include "llbutton.h"
#include "llcachename.h"
#include "lleventflags.h"
#include "lleventnotifier.h"
#include "llfloater.h"
#include "llfloaterworldmap.h"
#include "llinventorymodel.h"
#include "llnotificationsutil.h"
#include "llsecondlifeurls.h"
#include "lltextbox.h"
#include "llviewertexteditor.h"
#include "lluiconstants.h"
#include "llviewercontrol.h"
#include "llweb.h"
#include "llworldmap.h"
#include "lluictrlfactory.h"

//static
std::list<LLPanelEvent*> LLPanelEvent::sAllPanels;

LLPanelEvent::LLPanelEvent() : LLPanel(std::string("Event Panel"))
{
	sAllPanels.push_back(this);
}


LLPanelEvent::~LLPanelEvent()
{
	sAllPanels.remove(this);
}


BOOL LLPanelEvent::postBuild()
{
	mTBName = getChild<LLTextBox>("event_name");

	mTBCategory = getChild<LLTextBox>("event_category");
	
	mTBDate = getChild<LLTextBox>("event_date");

	mTBDuration = getChild<LLTextBox>("event_duration");

	mTBDesc = getChild<LLTextEditor>("event_desc");
	mTBDesc->setWordWrap(TRUE);
	mTBDesc->setEnabled(FALSE);

	mTBRunBy = getChild<LLTextBox>("event_runby");
	mTBLocation = getChild<LLTextBox>("event_location");
	mTBCover = getChild<LLTextBox>("event_cover");

	mTeleportBtn = getChild<LLButton>( "teleport_btn");
	mTeleportBtn->setClickedCallback(onClickTeleport);
	mTeleportBtn->setCallbackUserData(this);

	mMapBtn = getChild<LLButton>( "map_btn");
	mMapBtn->setClickedCallback(onClickMap);
	mMapBtn->setCallbackUserData(this);

	mNotifyBtn = getChild<LLButton>( "notify_btn");
	mNotifyBtn->setClickedCallback(onClickNotify);
	mNotifyBtn->setCallbackUserData(this);

	mCreateEventBtn = getChild<LLButton>( "create_event_btn");
	mCreateEventBtn->setClickedCallback(onClickCreateEvent);
	mCreateEventBtn->setCallbackUserData(this);

	return TRUE;
}

void LLPanelEvent::setEventID(const U32 event_id)
{
	mEventID = event_id;
	// Should reset all of the panel state here
	resetInfo();

	if (event_id != 0)
	{
		gEventNotifier.add(event_id);
	}
}

//static 
void LLPanelEvent::processEventInfoReply(LLMessageSystem *msg, void **)
{
	// extract the agent id
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );

	U32 event_id;
	msg->getU32("EventData", "EventID", event_id);

	// look up all panels which have this avatar
	for (panel_list_t::iterator iter = sAllPanels.begin(); iter != sAllPanels.end(); ++iter)
	{
		LLPanelEvent* self = *iter;
		// Skip updating panels which aren't for this event
		if (self->mEventID != event_id)
		{
			continue;
		}
		self->mEventInfo.unpack(msg);
		self->mTBName->setText(self->mEventInfo.mName);
		self->mTBCategory->setText(self->mEventInfo.mCategoryStr);
		self->mTBDate->setText(self->mEventInfo.mTimeStr);
		self->mTBDesc->setText(self->mEventInfo.mDesc);

		self->mTBDuration->setText(llformat("%d:%.2d", self->mEventInfo.mDuration / 60, self->mEventInfo.mDuration % 60));

		if (!self->mEventInfo.mHasCover)
		{
			self->mTBCover->setText(self->getString("none"));
		}
		else
		{
			self->mTBCover->setText(llformat("%d", self->mEventInfo.mCover));
		}

		F32 global_x = (F32)self->mEventInfo.mPosGlobal.mdV[VX];
		F32 global_y = (F32)self->mEventInfo.mPosGlobal.mdV[VY];

		S32 region_x = llround(global_x) % REGION_WIDTH_UNITS;
		S32 region_y = llround(global_y) % REGION_WIDTH_UNITS;
		S32 region_z = llround((F32)self->mEventInfo.mPosGlobal.mdV[VZ]);
		
		std::string desc = self->mEventInfo.mSimName + llformat(" (%d, %d, %d)", region_x, region_y, region_z);
		self->mTBLocation->setText(desc);

		if (self->mEventInfo.mEventFlags & EVENT_FLAG_MATURE)
		{
			self->childSetVisible("event_mature_yes", TRUE);
			self->childSetVisible("event_mature_no", FALSE);
		}
		else
		{
			self->childSetVisible("event_mature_yes", FALSE);
			self->childSetVisible("event_mature_no", TRUE);
		}

		if (self->mEventInfo.mUnixTime < time_corrected())
		{
			self->mNotifyBtn->setEnabled(FALSE);
		}
		else
		{
			self->mNotifyBtn->setEnabled(TRUE);
		}
		
		if (gEventNotifier.hasNotification(self->mEventInfo.mID))
		{
			self->mNotifyBtn->setLabel(self->getString("dont_notify"));
		}
		else
		{
			self->mNotifyBtn->setLabel(self->getString("notify"));
		}
	}
}


void LLPanelEvent::draw()
{
	std::string name;
	gCacheName->getFullName(mEventInfo.mRunByID, name);

	mTBRunBy->setText(name);

	LLPanel::draw();
}

void LLPanelEvent::resetInfo()
{
	// Clear all of the text fields.
}

// static
void LLPanelEvent::onClickTeleport(void* data)
{
	LLPanelEvent* self = (LLPanelEvent*)data;

	if (!self->mEventInfo.mPosGlobal.isExactlyZero())
	{
		gAgent.teleportViaLocation(self->mEventInfo.mPosGlobal);
		gFloaterWorldMap->trackLocation(self->mEventInfo.mPosGlobal);
	}
}


// static
void LLPanelEvent::onClickMap(void* data)
{
	LLPanelEvent* self = (LLPanelEvent*)data;

	if (!self->mEventInfo.mPosGlobal.isExactlyZero())
	{
		gFloaterWorldMap->trackLocation(self->mEventInfo.mPosGlobal);
		LLFloaterWorldMap::show(NULL, TRUE);
	}
}


// static
/*
void LLPanelEvent::onClickLandmark(void* data)
{
	LLPanelEvent* self = (LLPanelEvent*)data;
	//create_landmark(self->mTBName->getText(), "", self->mEventInfo.mPosGlobal);
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("CreateLandmarkForEvent");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_EventData);
	msg->addU32Fast(_PREHASH_EventID, self->mEventID);
	msg->nextBlockFast(_PREHASH_InventoryBlock);
	LLUUID folder_id;
	folder_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
	msg->addUUIDFast(_PREHASH_FolderID, folder_id);
	msg->addStringFast(_PREHASH_Name, self->mTBName->getText());
	gAgent.sendReliableMessage();
}
*/

// static
void LLPanelEvent::onClickCreateEvent(void* data)
{
	LLNotificationsUtil::add("PromptGoToEventsPage", LLSD(), LLSD(), callbackCreateEventWebPage); 
}

// static
void LLPanelEvent::onClickNotify(void *data)
{
	LLPanelEvent* self = (LLPanelEvent*)data;

	if (!gEventNotifier.hasNotification(self->mEventID))
	{
		gEventNotifier.add(self->mEventInfo);
		self->mNotifyBtn->setLabel(self->getString("dont_notify"));
	}
	else
	{
		gEventNotifier.remove(self->mEventInfo.mID);
		self->mNotifyBtn->setLabel(self->getString("notify"));
	}
}

// static
bool LLPanelEvent::callbackCreateEventWebPage(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		llinfos << "Loading events page " << EVENTS_URL << llendl;

		LLWeb::loadURL(EVENTS_URL);
	}
	return false;
}
