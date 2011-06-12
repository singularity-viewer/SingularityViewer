/* Copyright (c) 2009
 *
 * Modular Systems All rights reserved.
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
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
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
 */

#include "llviewerprecompiledheaders.h"

#include "llchat.h"
#include "llfloaterchat.h"
#include "scriptcounter.h"
#include "llselectmgr.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llviewerregion.h"
#include "llwindow.h"
#include "lltransfersourceasset.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llviewerobject.h"
#include <sstream>

ScriptCounter* ScriptCounter::sInstance;
U32 ScriptCounter::invqueries;
U32 ScriptCounter::status;
U32 ScriptCounter::scriptcount;
LLUUID ScriptCounter::reqObjectID;
LLDynamicArray<LLUUID> ScriptCounter::delUUIDS;
bool ScriptCounter::doDelete;
std::set<std::string> ScriptCounter::objIDS;
int ScriptCounter::objectCount;
LLViewerObject* ScriptCounter::foo;
void cmdline_printchat(std::string chat);
std::stringstream ScriptCounter::sstr;
int ScriptCounter::countingDone;

ScriptCounter::ScriptCounter()
{
	llassert_always(sInstance == NULL);
	sInstance = this;

}

ScriptCounter::~ScriptCounter()
{
	sInstance = NULL;
}
void ScriptCounter::init()
{
	if(!sInstance)
		sInstance = new ScriptCounter();
	status = IDLE;
}

LLVOAvatar* find_avatar_from_object( LLViewerObject* object );

LLVOAvatar* find_avatar_from_object( const LLUUID& object_id );

void ScriptCounter::showResult(std::string output)
{
	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	chat.mText = output;
	LLFloaterChat::addChat(chat);
	//sstr << scriptcount;
	//cmdline_printchat(sstr.str());
	init();
}

void ScriptCounter::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID,object_id );
	std::string name;
	std::string user_msg;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);
	if(reqObjectID.notNull())
	if(object_id == reqObjectID)
	{
		if(doDelete)
		{
			user_msg = llformat("Deleted %u scripts from object %s.", scriptcount, name.c_str());
		}
		else
		{
			user_msg = llformat("Counted %u scripts in object %s.", scriptcount, name.c_str());
		}
		reqObjectID.setNull();
		if(countingDone)
		{
			showResult(user_msg);
		}
	}
}

void ScriptCounter::processObjectProperties(LLMessageSystem* msg, void** user_data)
{
	std::string user_msg;
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID,object_id );
	std::string name;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);
	if(reqObjectID.notNull())
	if(object_id == reqObjectID)
	{
		if(doDelete)
		{
			user_msg = llformat("Deleted %u scripts from object %s.", scriptcount, name.c_str());
		}
		else
		{
			user_msg = llformat("Counted %u scripts in object %s.", scriptcount, name.c_str());
		}
		reqObjectID.setNull();
		if(countingDone)
		{
			showResult(user_msg);
		}
	}
}

void ScriptCounter::serializeSelection(bool delScript)
{
	LLDynamicArray<LLViewerObject*> objectArray;
	foo=LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
	sstr.str("");
	doDelete=false;
	scriptcount=0;
	objIDS.clear();
	delUUIDS.clear();
	objectCount=0;
	countingDone=false;
	reqObjectID.setNull();
	if(foo)
	{
		if(foo->isAvatar())
		{
			LLVOAvatar* av=find_avatar_from_object(foo);
			if(av)
			{
				for (LLVOAvatar::attachment_map_t::iterator iter = av->mAttachmentPoints.begin();
					iter != av->mAttachmentPoints.end();
					++iter)
				{
					LLViewerJointAttachment* attachment = iter->second;
					if (!attachment->getValid())
						continue ;
					for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itObj = attachment->mAttachedObjects.begin();
							itObj != attachment->mAttachedObjects.end(); ++itObj)
					{
						LLViewerObject* object = *itObj;
						if(object)
						{
							objectArray.put(object);
							objectCount++;
						}
					}
				}
			}
		}
		else
		{
			for (LLObjectSelection::valid_root_iterator iter = LLSelectMgr::getInstance()->getSelection()->valid_root_begin();
					 iter != LLSelectMgr::getInstance()->getSelection()->valid_root_end(); iter++)
			{
				LLSelectNode* selectNode = *iter;
				LLViewerObject* object = selectNode->getObject();
				if(object)
				{
					objectArray.put(object);
					objectCount++;
				}
			}
			doDelete=delScript;
		}
		F32 throttle = gSavedSettings.getF32("OutBandwidth");
		if((throttle == 0.f) || (throttle > 128000.f))
		{
			gMessageSystem->mPacketRing.setOutBandwidth(128000);
			gMessageSystem->mPacketRing.setUseOutThrottle(TRUE);
		}
		showResult(llformat("Counting scripts, please wait..."));
		if((objectCount == 1) && !(foo->isAvatar()))
		{
			LLViewerObject *reqObject=((LLViewerObject*)foo->getRoot());
			if(reqObject->isAvatar())
			{
				for (LLObjectSelection::iterator iter = LLSelectMgr::getInstance()->getSelection()->begin();
					 iter != LLSelectMgr::getInstance()->getSelection()->end(); iter++ )
				{			
					LLSelectNode *nodep = *iter;
					LLViewerObject* objectp = nodep->getObject();
					if (objectp->isRootEdit())
					{
						reqObjectID=objectp->getID();
						LLMessageSystem* msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_ObjectSelect);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
						msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
						msg->nextBlockFast(_PREHASH_ObjectData);
						msg->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
						msg->sendReliable(gAgent.getRegionHost());
						break;
					}
				}
			}
			else
			{
				reqObjectID=reqObject->getID();
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_RequestFlags, 0 );
				msg->addUUIDFast(_PREHASH_ObjectID, reqObjectID);
				gAgent.sendReliableMessage();
			}
		}
		serialize(objectArray);
	}
}

void ScriptCounter::serialize(LLDynamicArray<LLViewerObject*> objects)
{
	init();
	status = COUNTING;
	for(LLDynamicArray<LLViewerObject*>::iterator itr = objects.begin(); itr != objects.end(); ++itr)
	{
		LLViewerObject* object = *itr;
		if (object)
			subserialize(object);
	}
	if(invqueries == 0)
		init();
}

void ScriptCounter::subserialize(LLViewerObject* linkset)
{
	LLViewerObject* object = linkset;
	LLDynamicArray<LLViewerObject*> count_objects;
	count_objects.put(object);
	LLViewerObject::child_list_t child_list = object->getChildren();
	for (LLViewerObject::child_list_t::iterator i = child_list.begin(); i != child_list.end(); ++i)
	{
		LLViewerObject* child = *i;
		if(!child->isAvatar())
			count_objects.put(child);
	}
	S32 object_index = 0;
	while ((object_index < count_objects.count()))
	{
		object = count_objects.get(object_index++);
		LLUUID id = object->getID();
		objIDS.insert(id.asString());
		llinfos << "Counting scripts in prim " << object->getID().asString() << llendl;
		object->registerInventoryListener(sInstance,NULL);
		object->dirtyInventory();
		object->requestInventory();
		invqueries += 1;
	}
}

void ScriptCounter::completechk()
{
	std::string user_msg;
	llinfos << "Completechk called." << llendl;
	if(invqueries == 0)
	{
		llinfos << "InvQueries = 0..." << llendl;
		if(reqObjectID.isNull())
		{
			llinfos << "reqObjectId is null..." << llendl;
			if(foo->isAvatar())
			{
				int valid=1;
				LLVOAvatar *av=find_avatar_from_object(foo);
				LLNameValue *firstname;
				LLNameValue *lastname;
				if(!av)
				  valid=0;
				else
				{
				  firstname = av->getNVPair("FirstName");
				  lastname = av->getNVPair("LastName");
				  if(!firstname || !lastname)
					valid=0;
				  if(valid)
				  {
					  user_msg = llformat("Counted %u scripts in %u attachments on %s %s.", scriptcount, objectCount, firstname->getString()  , lastname->getString());
					  //sstr << "Counted scripts from " << << " attachments on " << firstname->getString() << " " << lastname->getString() << ": ";
				  }
				}
				if(!valid)
				{
					user_msg = llformat("Counted %u scripts in %u attachments on selected avatar.", scriptcount, objectCount);
					//sstr << "Counted scripts from " << objectCount << " attachments on avatar: ";
				}
			}
			else
			{
				if(doDelete)
				{
					user_msg = llformat("Deleted %u scripts in %u objects.", scriptcount, objectCount);
					//sstr << "Deleted scripts in " << objectCount << " objects: ";
				}
				else
				{
					user_msg = llformat("Counted %u scripts in %u objects.", scriptcount, objectCount);
					//sstr << "Counted scripts in " << objectCount << " objects: ";
				}
			}
			F32 throttle = gSavedSettings.getF32("OutBandwidth");
			if(throttle != 0.f)
			{
				gMessageSystem->mPacketRing.setOutBandwidth(throttle);
				gMessageSystem->mPacketRing.setUseOutThrottle(TRUE);
			}
			else
			{
				gMessageSystem->mPacketRing.setOutBandwidth(0.0);
				gMessageSystem->mPacketRing.setUseOutThrottle(FALSE);
			}
			llinfos << "Sending readout to chat..." << llendl;
			showResult(user_msg);
		}
		else
			countingDone=true;
	}
}

void ScriptCounter::inventoryChanged(LLViewerObject* obj,
								 LLInventoryObject::object_list_t* inv,
								 S32 serial_num,
								 void* user_data)
{
	llinfos << "InventoryChanged called..." << llendl;
	if(status == IDLE)
	{
		obj->removeInventoryListener(sInstance);
		return;
	}
	if(objIDS.find(obj->getID().asString()) != objIDS.end())
	{
		if(inv)
		{
			LLInventoryObject::object_list_t::const_iterator it = inv->begin();
			LLInventoryObject::object_list_t::const_iterator end = inv->end();
			for( ;	it != end;	++it)
			{
				LLInventoryObject* asset = (*it);
				if(asset)
				{
					if(asset->getType() == LLAssetType::AT_LSL_TEXT)
					{
						scriptcount+=1;
						if(doDelete==true)
							delUUIDS.push_back(asset->getUUID());
					}
				}
			}
			if(doDelete==true)
			{
				while (delUUIDS.count() > 0)
				{
					const LLUUID toDelete=delUUIDS[0];
					delUUIDS.remove(0);
					if(toDelete.notNull())
						obj->removeInventory(toDelete);
				}
			}
		}
		llinfos << "Attempting completechk..." << llendl;
		invqueries -= 1;
		objIDS.erase(obj->getID().asString());
		reqObjectID.setNull();
		obj->removeInventoryListener(sInstance);
		completechk();
	}
}
