// <edit>
 
#include "llviewerprecompiledheaders.h"
#include "llfloaterexport.h"
#include "lluictrlfactory.h"
#include "llsdutil.h"
#include "llsdserialize.h"
#include "llselectmgr.h"
#include "llscrolllistctrl.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llagent.h"
#include "llvoavatar.h"
#include "llvoavatardefines.h"
#include "llimportobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llwindow.h"
#include "llviewertexturelist.h"
#include "lltexturecache.h"
#include "llimage.h"
#include "llappviewer.h"
#include "llfloaterattachments.h"
#include "llworld.h"
#include "llviewerregion.h"
#include "llfile.h"

#include "llmath.h"
#include "llv4math.h"

#include "llfloatertextdump.h"

//Half of this file is copy/pasted from llfloaterexport and the stuff I tacked on sucks. I'm not unshitting it. You've been warned.
//but, it'll be patched soon, so who cares.

//the fullid -> localid mappings are probably wrong or will be missing altogether. oh well.

std::vector<LLFloaterAttachments*> LLFloaterAttachments::instances;
U32 LLFloaterAttachments::sCurrentRequestID = 1;
std::map<LLUUID, S16> LLFloaterAttachments::sInventoryRequests;

LLFloaterAttachments::LLFloaterAttachments()
:	LLFloater(),
	mReceivedProps(0),
	mViewingChildren(false),
	mIDsConnected(false)
{
	mSelection = LLSelectMgr::getInstance()->getSelection();
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_attachments.xml");

	if(LLFloaterAttachments::instances.empty())
	{
		gMessageSystem->addHandlerFuncFast(_PREHASH_ObjectPropertiesFamily, &processObjectPropertiesFamily);
		gMessageSystem->addHandlerFuncFast(_PREHASH_KillObject, &dispatchKillObject);
	}

	LLFloaterAttachments::instances.push_back(this);
}


LLFloaterAttachments::~LLFloaterAttachments()
{
	std::vector<LLFloaterAttachments*>::iterator pos = std::find(LLFloaterAttachments::instances.begin(), LLFloaterAttachments::instances.end(), this);
	if(pos != LLFloaterAttachments::instances.end())
	{
		LLFloaterAttachments::instances.erase(pos);
	}

	if(LLFloaterAttachments::instances.empty())
	{
		gMessageSystem->delHandlerFuncFast(_PREHASH_ObjectPropertiesFamily, &processObjectPropertiesFamily);
		gMessageSystem->delHandlerFuncFast(_PREHASH_KillObject, &dispatchKillObject);
	}
}

BOOL LLFloaterAttachments::postBuild(void)
{
	if(!mSelection) return TRUE;
	if(mSelection->getRootObjectCount() < 1) return TRUE;

	childSetCommitCallback("attachment_list", onCommitAttachmentList, this);

	childSetAction("inventory_btn", onClickInventory, this);
	childSetAction("textures_btn", onClickTextures, this);
	childSetAction("view_children_btn", onClickViewChildren, this);

	LLViewerObject* avatar = NULL;

	for (LLObjectSelection::valid_root_iterator iter = mSelection->valid_root_begin();
		 iter != mSelection->valid_root_end(); iter++)
	{
		LLSelectNode* nodep = *iter;
		LLViewerObject* objectp = nodep->getObject();

		LLViewerObject* parentp = objectp->getSubParent();
		if(parentp)
		{
			if(parentp->isAvatar())
			{
				// parent is an avatar
				avatar = parentp;
				break;
			}
		}

		if(objectp->isAvatar())
		{
			avatar = objectp;
			break;
		}

	}

	if(avatar)
	{
		std::string av_name;
		gCacheName->getFullName(avatar->getID(), av_name);

		if(!av_name.empty())
			setTitle(av_name + " HUDs");

		selectAgentHudPrims(avatar);
	}

	return TRUE;
}

void LLFloaterAttachments::onCommitAttachmentList(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterAttachments* floaterp = (LLFloaterAttachments*)user_data;
	floaterp->refreshButtons();
}

void LLFloaterAttachments::refreshButtons()
{
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("attachment_list");
	LLScrollListItem* scroll_itemp = scrollp->getFirstSelected();
	if(scroll_itemp)
	{
		LLUUID primid = scroll_itemp->getUUID();
		BOOL have_localid = mReceivedProps >= mPendingRequests.size() && mFull2LocalID.count(primid) > 0;

		getChild<LLButton>("inventory_btn")->setEnabled(have_localid);
		getChild<LLButton>("textures_btn")->setEnabled(have_localid);
		if(mViewingChildren)
			getChild<LLButton>("view_children_btn")->setEnabled(TRUE);
		else
			getChild<LLButton>("view_children_btn")->setEnabled(mHUDAttachmentHierarchy.count(primid) > 0);
	}
	else
	{
		getChild<LLButton>("inventory_btn")->setEnabled(FALSE);
		getChild<LLButton>("textures_btn")->setEnabled(FALSE);
		getChild<LLButton>("view_children_btn")->setEnabled(FALSE);
	}

	if(mViewingChildren) getChild<LLButton>("view_children_btn")->setEnabled(TRUE);
}

void LLFloaterAttachments::onClickInventory(void* user_data)
{
	LLFloaterAttachments* floaterp = (LLFloaterAttachments*)user_data;

	LLScrollListCtrl* scrollp = floaterp->getChild<LLScrollListCtrl>("attachment_list");
	LLScrollListItem* scroll_itemp = scrollp->getFirstSelected();
	if(scroll_itemp)
	{
		if(floaterp->mFull2LocalID.count(scroll_itemp->getUUID()) > 0)
		{
			sInventoryRequests[scroll_itemp->getUUID()] = floaterp->mHUDAttachmentPrims[scroll_itemp->getUUID()]->mInvSerial;

			gMessageSystem->newMessageFast(_PREHASH_RequestTaskInventory);
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			gMessageSystem->nextBlockFast(_PREHASH_InventoryData);
			gMessageSystem->addU32Fast(_PREHASH_LocalID, floaterp->mFull2LocalID[scroll_itemp->getUUID()]);
			gMessageSystem->sendReliable(gAgent.getRegionHost());
		}
		else
		{
			llinfos << "Couldn't find localid for " << scroll_itemp->getUUID().asString() << llendl;
		}
	}
}

void LLFloaterAttachments::onClickTextures(void* user_data)
{
	LLFloaterAttachments* floaterp = (LLFloaterAttachments*)user_data;

	LLScrollListCtrl* scrollp = floaterp->getChild<LLScrollListCtrl>("attachment_list");
	LLScrollListItem* scroll_itemp = scrollp->getFirstSelected();
	if(scroll_itemp)
	{
		std::vector<LLUUID> textures = floaterp->mHUDAttachmentPrims[scroll_itemp->getUUID()]->mTextures;

		std::set<LLUUID> seen_textures;

		std::vector<LLUUID>::iterator it_textures = textures.begin();
		std::vector<LLUUID>::iterator it_textures_end = textures.end();

		while(it_textures != it_textures_end)
		{
			if(seen_textures.count((*it_textures)) == 0)
			{
				seen_textures.insert((*it_textures));
				LLFloaterChat::addChat(LLChat((*it_textures).asString()));
			}
			it_textures++;
		}
	}
}

void LLFloaterAttachments::onClickViewChildren(void* user_data)
{
	LLFloaterAttachments* floaterp = (LLFloaterAttachments*)user_data;

	LLScrollListCtrl* scrollp = floaterp->getChild<LLScrollListCtrl>("attachment_list");
	LLScrollListItem* scroll_itemp = scrollp->getFirstSelected();
	if(scroll_itemp && !floaterp->mViewingChildren)
	{
		LLUUID primid = scroll_itemp->getUUID();
		if(floaterp->mHUDAttachmentHierarchy.count(primid) > 0)
		{
			scrollp->clearRows();

			std::multimap<LLUUID, LLUUID>::iterator child_begin = floaterp->mHUDAttachmentHierarchy.lower_bound(primid);
			std::multimap<LLUUID, LLUUID>::iterator child_end = floaterp->mHUDAttachmentHierarchy.upper_bound(primid);

			while(child_begin != child_end)
			{
				LLHUDAttachment* hud_prim = floaterp->mHUDAttachmentPrims[child_begin->second];
				floaterp->addAttachmentToList(hud_prim->mObjectID, hud_prim->mName, hud_prim->mDescription);
				child_begin++;
			}
			floaterp->getChild<LLButton>("view_children_btn")->setLabel(std::string("Parent..."));
			floaterp->mViewingChildren = true;
		}
	}
	else if(floaterp->mViewingChildren)
	{
		if(floaterp->mHUDAttachmentHierarchy.count(floaterp->mAvatarID) > 0)
		{
			scrollp->clearRows();

			std::multimap<LLUUID, LLUUID>::iterator parent_begin = floaterp->mHUDAttachmentHierarchy.lower_bound(floaterp->mAvatarID);
			std::multimap<LLUUID, LLUUID>::iterator parent_end = floaterp->mHUDAttachmentHierarchy.upper_bound(floaterp->mAvatarID);

			while(parent_begin != parent_end)
			{
				LLHUDAttachment* hud_prim = floaterp->mHUDAttachmentPrims[parent_begin->second];
				floaterp->addAttachmentToList(hud_prim->mObjectID, hud_prim->mName, hud_prim->mDescription);
				parent_begin++;
			}
			floaterp->getChild<LLButton>("view_children_btn")->setLabel(std::string("Children..."));
			floaterp->mViewingChildren = false;
		}
	}

	floaterp->refreshButtons();
}

void LLFloaterAttachments::selectAgentHudPrims(LLViewerObject* avatar)
{
	//try and find the prims for the HUD attachments

	mAvatarID = avatar->getID();

	U32 local_id = avatar->getLocalID() + 1;

	mHandleKillObject = true;


	gMessageSystem->newMessageFast(_PREHASH_ObjectSelect);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, local_id);
	int block_counter = 0;
	int i = 0;

	U32 ip = avatar->getRegion()->getHost().getAddress();
	U32 port = avatar->getRegion()->getHost().getPort();

	//try to select all of the possible hud prim localids
	while(i<MAX_HUD_PRIM_NUM)
	{
		++local_id;

		LLUUID fullid;
		gObjectList.getUUIDFromLocal(fullid, local_id, ip, port);
		if(fullid == LLUUID::null)
		{
			mPendingRequests.push_back(local_id);
			i++;

			block_counter++;
			if(block_counter >= 254)
			{
				// start a new message
				gMessageSystem->sendReliable(avatar->getRegion()->getHost());
				gMessageSystem->newMessageFast(_PREHASH_ObjectSelect);
				gMessageSystem->nextBlockFast(_PREHASH_AgentData);
				gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
			}
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, local_id);
		}
	}
	gMessageSystem->sendReliable(avatar->getRegion()->getHost());

	//deselect all of the hud prim localids
	local_id = avatar->getLocalID() + 1;
	i = 0;

	gMessageSystem->newMessageFast(_PREHASH_ObjectDeselect);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, local_id);
	block_counter = 0;
	while(i<MAX_HUD_PRIM_NUM)
	{
		local_id++;

		LLUUID fullid;
		gObjectList.getUUIDFromLocal(fullid, local_id, ip, port);
		if(fullid == LLUUID::null)
		{
			i++;

			block_counter++;
			if(block_counter >= 254)
			{
				// start a new message
				gMessageSystem->sendReliable(avatar->getRegion()->getHost());
				gMessageSystem->newMessageFast(_PREHASH_ObjectDeselect);
				gMessageSystem->nextBlockFast(_PREHASH_AgentData);
				gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
			}
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, local_id);
		}
	}
	gMessageSystem->sendReliable(avatar->getRegion()->getHost());
}

void LLFloaterAttachments::dispatchHUDObjectProperties(LLHUDAttachment* hud_attachment)
{
	std::vector<LLFloaterAttachments*>::iterator iter = LLFloaterAttachments::instances.begin();
	std::vector<LLFloaterAttachments*>::iterator end = LLFloaterAttachments::instances.end();
	for( ; iter != end; ++iter)
	{
		(*iter)->receiveHUDPrimInfo(hud_attachment);
	}
}

void LLFloaterAttachments::receiveHUDPrimInfo(LLHUDAttachment* hud_attachment)
{
	//check that this is for an avatar we're monitoring
	if( hud_attachment->mOwnerID == mAvatarID)
	{
		//check that we don't have this already and that it really is a hud object
		if(mHUDAttachmentPrims.count(hud_attachment->mObjectID) == 0 && !gObjectList.findObject(hud_attachment->mObjectID))
		{

			mHUDAttachmentPrims[hud_attachment->mObjectID] = hud_attachment;
			mHUDPrims.push_back(hud_attachment->mObjectID);

			mRootRequests[sCurrentRequestID] = hud_attachment->mObjectID;

			gMessageSystem->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);

			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());

			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_RequestFlags, sCurrentRequestID++);
			gMessageSystem->addUUID(_PREHASH_ObjectID, hud_attachment->mObjectID);

			gMessageSystem->sendReliable(gAgent.getRegionHost());
		}
	}
	if(++mReceivedProps >= mPendingRequests.size())
	{
		connectFullAndLocalIDs();
	}
	llinfos << "Prim: " << mReceivedProps << ":" << mPendingRequests.size() << llendl;
}

void LLFloaterAttachments::dispatchKillObject(LLMessageSystem* msg, void** user_data)
{
	std::vector<LLFloaterAttachments*>::iterator iter = LLFloaterAttachments::instances.begin();
	std::vector<LLFloaterAttachments*>::iterator end = LLFloaterAttachments::instances.end();
	for( ; iter != end; ++iter)
	{
		if((*iter)->mHandleKillObject)
		{
			S32	i;

			S32	num_objects = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);

			for (i = 0; i < num_objects; i++)
			{
				U32	local_id;
				msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);
				(*iter)->receiveKillObject(local_id);
			}
		}
	}
}

void LLFloaterAttachments::receiveKillObject(U32 local_id)
{
	std::list<U32>::iterator localid_iter = std::find(mPendingRequests.begin(), mPendingRequests.end(), local_id);
	if(localid_iter != mPendingRequests.end())
	{
		mPendingRequests.erase(localid_iter);

		llinfos << "Prim: " << mReceivedProps << ":" << mPendingRequests.size() << llendl;

		if(mReceivedProps >= mPendingRequests.size())
		{
			connectFullAndLocalIDs();
		}
	}
}

void LLFloaterAttachments::connectFullAndLocalIDs()
{
	if(mIDsConnected) return;

	mIDsConnected = true;
	mHandleKillObject = false;

	std::list<U32>::iterator request_iter = mPendingRequests.begin();
	std::list<LLUUID>::iterator hudprim_iter = mHUDPrims.begin();
	while(request_iter != mPendingRequests.end())
	{
		mFull2LocalID[(*hudprim_iter)] = (*request_iter);

		//llinfos << (*hudprim_iter).asString() << " : " << (*request_iter) << llendl;

		request_iter++;
		hudprim_iter++;
	}
}

void LLFloaterAttachments::receiveHUDPrimRoot(LLHUDAttachment* hud_attachment)
{
	//there will probably be a reason for this if later. or maybe not. who knows.

	LLHUDAttachment* child = mHUDAttachmentPrims[mRootRequests[hud_attachment->mRequestID]];
	//the object requested was a root prim
	if(hud_attachment->mObjectID == mAvatarID)
	{
		//llinfos << hud_attachment->mName << " : " << child->mName << llendl;

		mHUDAttachmentHierarchy.insert(std::pair<LLUUID,LLUUID>(hud_attachment->mObjectID, mRootRequests[hud_attachment->mRequestID]));

		LLScrollListCtrl* list = getChild<LLScrollListCtrl>("attachment_list");

		if(list->getItemIndex(child->mObjectID) == -1)
		{
			if(!mViewingChildren)
				addAttachmentToList(child->mObjectID, child->mName, child->mDescription);
		}
	}
	//the object requested was a child prim
	else if(mHUDAttachmentPrims.count(hud_attachment->mObjectID))
	{
		//llinfos << hud_attachment->mName << " : " << child->mName << llendl;

		mHUDAttachmentHierarchy.insert(std::pair<LLUUID,LLUUID>(hud_attachment->mObjectID, mRootRequests[hud_attachment->mRequestID]));
	}
}

void LLFloaterAttachments::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{
	U32 request_flags;
	LLUUID id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, id);

	LLUUID owner_id;
	LLUUID group_id;

	msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_RequestFlags,	request_flags );

	if(request_flags == 0x0)
	{
		return;
	}

	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID,		owner_id );
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID,		group_id );

	// unpack name & desc
	std::string name;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);

	std::string desc;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, desc);

	// Now look through all of the hovered nodes
	struct f : public LLSelectedNodeFunctor
	{
		LLUUID mID;
		f(const LLUUID& id) : mID(id) {}
		virtual bool apply(LLSelectNode* node)
		{
			return (node->getObject() && node->getObject()->mID == mID);
		}
	} func(id);
	LLSelectNode* node = LLSelectMgr::getInstance()->getHoverObjects()->getFirstNode(&func);

	if(!node)
	{
		std::vector<LLFloaterAttachments*>::iterator iter = LLFloaterAttachments::instances.begin();
		std::vector<LLFloaterAttachments*>::iterator end = LLFloaterAttachments::instances.end();
		for( ; iter != end; ++iter)
		{
			(*iter)->receiveHUDPrimRoot(new LLHUDAttachment(name, desc, owner_id, id, LLUUID::null, std::vector<LLUUID>(), request_flags));
		}
	}
}

void LLFloaterAttachments::addAttachmentToList(LLUUID objectid, std::string name, std::string desc)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("attachment_list");
	LLSD element;
	element["id"] = objectid;

	LLSD& type_column = element["columns"][LIST_TYPE];
	type_column["column"] = "type";
	type_column["type"] = "icon";
	type_column["value"] = "inv_item_object.tga";

	LLSD& name_column = element["columns"][LIST_NAME];
	name_column["column"] = "name";
	name_column["value"] = name;

	LLSD& desc_column = element["columns"][LIST_DESC];
	desc_column["column"] = "desc";
	desc_column["value"] = desc;
	list->addElement(element, ADD_BOTTOM);

	refreshButtons();
}

void LLFloaterAttachments::dumpTaskInvFile(const std::string& title, const std::string &filename)
{
	std::string filename_and_local_path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, filename);
	llifstream ifs(filename_and_local_path);
	if(ifs.good())
	{
		std::stringstream sstr;

		sstr << ifs.rdbuf();

		LLFloaterTextDump::show(title, sstr.str());

		ifs.close();
		LLFile::remove(filename_and_local_path);
	}
	else
	{
		llwarns << "unable to load task inventory: " << filename_and_local_path
				<< llendl;
	}
}

//this is dumb.
LLHUDAttachment::LLHUDAttachment(std::string name, std::string description, LLUUID owner_id, LLUUID object_id, LLUUID from_task_id, std::vector<LLUUID> textures, U32 request_id, S32 inv_serial)
	: mName(name), mDescription(description), mOwnerID(owner_id), mObjectID(object_id), mFromTaskID(from_task_id), mTextures(textures), mRequestID(request_id), mInvSerial(inv_serial)
{
}

// </edit>
