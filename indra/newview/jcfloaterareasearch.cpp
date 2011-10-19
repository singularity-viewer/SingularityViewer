/* Copyright (c) 2009
 *
 * Modular Systems Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems Ltd nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS LTD AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified, debugged, optimized and improved by Henri Beauchamp Feb 2010.
 */

#include "llviewerprecompiledheaders.h"

#include "lluuid.h"
#include "lluictrlfactory.h"
#include "llscrolllistctrl.h"

#include "llagent.h"
#include "lltracker.h"
#include "llviewerobjectlist.h"
#include "llviewercontrol.h"
#include "jcfloaterareasearch.h"

JCFloaterAreaSearch* JCFloaterAreaSearch::sInstance = NULL;
LLViewerRegion* JCFloaterAreaSearch::sLastRegion = NULL;
S32 JCFloaterAreaSearch::sRequested = 0;
std::map<LLUUID, AObjectDetails> JCFloaterAreaSearch::sObjectDetails;
std::string JCFloaterAreaSearch::sSearchedName;
std::string JCFloaterAreaSearch::sSearchedDesc;
std::string JCFloaterAreaSearch::sSearchedOwner;
std::string JCFloaterAreaSearch::sSearchedGroup;

const std::string request_string = "JCFloaterAreaSearch::Requested_ø§µ";
const F32 min_refresh_interval = 0.25f;	// Minimum interval between list refreshes in seconds.

JCFloaterAreaSearch::JCFloaterAreaSearch() :
LLFloater(),
mCounterText(0),
mResultList(0)
{
	llassert_always(sInstance == NULL);
	sInstance = this;
	mLastUpdateTimer.reset();
}

JCFloaterAreaSearch::~JCFloaterAreaSearch()
{
	sInstance = NULL;
}

void JCFloaterAreaSearch::close(bool app)
{
	if (app)
	{
		LLFloater::close(app);
	}
	else
	{
		if (sInstance)
		{
			sInstance->setVisible(FALSE);
		}
	}
}

BOOL JCFloaterAreaSearch::postBuild()
{
	mResultList = getChild<LLScrollListCtrl>("result_list");
	mResultList->setCallbackUserData(this);
	mResultList->setDoubleClickCallback(onDoubleClick);
	mResultList->sortByColumn("Name", TRUE);

	mCounterText = getChild<LLTextBox>("counter");

	childSetAction("Refresh", search, this);
	childSetAction("Stop", cancel, this);

	childSetKeystrokeCallback("Name query chunk", onCommitLine, 0);
	childSetKeystrokeCallback("Description query chunk", onCommitLine, 0);
	childSetKeystrokeCallback("Owner query chunk", onCommitLine, 0);
	childSetKeystrokeCallback("Group query chunk", onCommitLine, 0);

	return TRUE;
}

// static
void JCFloaterAreaSearch::checkRegion()
{
	// Check if we changed region, and if we did, clear the object details cache.
	LLViewerRegion* region = gAgent.getRegion();
	if (region != sLastRegion)
	{
		sLastRegion = region;
		sRequested = 0;
		sObjectDetails.clear();
		if (sInstance)
		{
			sInstance->mResultList->deleteAllItems();
			sInstance->mCounterText->setText(std::string("Listed/Pending/Total"));
		}
	}
}

// static
void JCFloaterAreaSearch::toggle()
{
	if (sInstance)
	{
		if (sInstance->getVisible())
		{
			sInstance->setVisible(FALSE);
		}
		else
		{
			checkRegion();
			sInstance->setVisible(TRUE);
		}
	}
	else
	{
		sInstance = new JCFloaterAreaSearch();
		LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_area_search.xml");
	}
}

// static
void JCFloaterAreaSearch::onDoubleClick(void *userdata)
{
	JCFloaterAreaSearch *self = (JCFloaterAreaSearch*)userdata;
	LLScrollListItem *item = self->mResultList->getFirstSelected();
	if (!item) return;
	LLUUID object_id = item->getUUID();
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (objectp)
	{
		LLTracker::trackLocation(objectp->getPositionGlobal(), sObjectDetails[object_id].name, "", LLTracker::LOCATION_ITEM);
	}
}

// static
void JCFloaterAreaSearch::cancel(void* data)
{
	checkRegion();
	if (sInstance)
	{
		sInstance->close(TRUE);
	}
	sSearchedName = "";
	sSearchedDesc = "";
	sSearchedOwner = "";
	sSearchedGroup = "";
}

// static
void JCFloaterAreaSearch::search(void* data)
{
	//llinfos << "Clicked search" << llendl;
	checkRegion();
	results();
}

// static
void JCFloaterAreaSearch::onCommitLine(LLLineEditor* line, void* user_data)
{
	std::string name = line->getName();
	std::string text = line->getText();
	LLStringUtil::toLower(text);
	line->setText(text);
	if (name == "Name query chunk") sSearchedName = text;
	else if (name == "Description query chunk") sSearchedDesc = text;
	else if (name == "Owner query chunk") sSearchedOwner = text;
	else if (name == "Group query chunk") sSearchedGroup = text;
	//llinfos << "loaded " << name << " with "<< text << llendl;

	if (text.length() > 3)
	{
		checkRegion();
		results();
	}
}

// static
void JCFloaterAreaSearch::requestIfNeeded(LLViewerObject *objectp)
{
	LLUUID object_id = objectp->getID();
	if (sObjectDetails.count(object_id) == 0)
	{
		//llinfos << "not in list" << llendl;
		AObjectDetails* details = &sObjectDetails[object_id];
		details->name = request_string;
		details->desc = request_string;
		details->owner_id = LLUUID::null;
		details->group_id = LLUUID::null;

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_RequestFlags, 0 );
		msg->addUUIDFast(_PREHASH_ObjectID, object_id);
		gAgent.sendReliableMessage();
		//llinfos << "Sent data request for object " << object_id << llendl;
		sRequested++;
	}
}

// static
void JCFloaterAreaSearch::results()
{
	if (!sInstance) return;
	if (!(sInstance->getVisible())) return;
	if (sRequested > 0 && sInstance->mLastUpdateTimer.getElapsedTimeF32() < min_refresh_interval) return;
	//llinfos << "results()" << llendl;
	LLDynamicArray<LLUUID> selected = sInstance->mResultList->getSelectedIDs();
	S32 scrollpos = sInstance->mResultList->getScrollPos();
	sInstance->mResultList->deleteAllItems();
	S32 i;
	S32 total = gObjectList.getNumObjects();

	LLViewerRegion* our_region = gAgent.getRegion();
	for (i = 0; i < total; i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (objectp)
		{
			if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
				!objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
			{
				LLUUID object_id = objectp->getID();
				if (sObjectDetails.count(object_id) == 0)
				{
					//llinfos << "not all entries are \"\"" << llendl;
					requestIfNeeded(objectp);
				}
				else
				{
					//llinfos << "all entries are \"\" or we have data" << llendl;
					AObjectDetails* details = &sObjectDetails[object_id];
					std::string object_name = details->name;
					std::string object_desc = details->desc;
					std::string object_owner;
					std::string object_group;
					gCacheName->getFullName(details->owner_id, object_owner);
					gCacheName->getGroupName(details->group_id, object_group);
					if (object_name != request_string)
					{
						//llinfos << "both names are loaded or aren't needed" << llendl;
						std::string onU = object_owner;
						std::string cnU = object_group;
						LLStringUtil::toLower(object_name);
						LLStringUtil::toLower(object_desc);
						LLStringUtil::toLower(object_owner);
						LLStringUtil::toLower(object_group);
						if ((sSearchedName == "" || object_name.find(sSearchedName) != -1) &&
							(sSearchedDesc == "" || object_desc.find(sSearchedDesc) != -1) &&
							(sSearchedOwner == "" || object_owner.find(sSearchedOwner) != -1) &&
							(sSearchedGroup == "" || object_group.find(sSearchedGroup) != -1))
						{
							//llinfos << "pass" << llendl;
							LLSD element;
							element["id"] = object_id;
							element["columns"][LIST_OBJECT_NAME]["column"] = "Name";
							element["columns"][LIST_OBJECT_NAME]["type"] = "text";
							element["columns"][LIST_OBJECT_NAME]["value"] = details->name;	//item->getName();//ai->second//"avatar_icon";
							element["columns"][LIST_OBJECT_DESC]["column"] = "Description";
							element["columns"][LIST_OBJECT_DESC]["type"] = "text";
							element["columns"][LIST_OBJECT_DESC]["value"] = details->desc;	//ai->second;
							element["columns"][LIST_OBJECT_OWNER]["column"] = "Owner";
							element["columns"][LIST_OBJECT_OWNER]["type"] = "text";
							element["columns"][LIST_OBJECT_OWNER]["value"] = onU;			//ai->first;
							element["columns"][LIST_OBJECT_GROUP]["column"] = "Group";
							element["columns"][LIST_OBJECT_GROUP]["type"] = "text";
							element["columns"][LIST_OBJECT_GROUP]["value"] = cnU;			//ai->second;
							sInstance->mResultList->addElement(element, ADD_BOTTOM);
						}
					}
				}
			}
		}
	}

	sInstance->mResultList->sortItems();
	sInstance->mResultList->selectMultiple(selected);
	sInstance->mResultList->setScrollPos(scrollpos);
	sInstance->mCounterText->setText(llformat("%d listed/%d pending/%d total", sInstance->mResultList->getItemCount(), sRequested, sObjectDetails.size()));
	sInstance->mLastUpdateTimer.reset();
}

// static
void JCFloaterAreaSearch::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{
	checkRegion();

	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id);

	bool exists = (sObjectDetails.count(object_id) != 0);
	AObjectDetails* details = &sObjectDetails[object_id];
	if (!exists || details->name == request_string)
	{
		// We cache unknown objects (to avoid having to request them later)
		// and requested objects.
		if (exists && sRequested > 0) sRequested--;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, details->owner_id);
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, details->group_id);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, details->name);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, details->desc);
		gCacheName->get(details->owner_id, false, boost::bind(&JCFloaterAreaSearch::results));
		gCacheName->get(details->group_id, true, boost::bind(&JCFloaterAreaSearch::results));
		//llinfos << "Got info for " << (exists ? "requested" : "unknown") << " object " << object_id << llendl;
	}
}
