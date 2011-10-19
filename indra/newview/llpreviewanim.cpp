/** 
 * @file llpreviewanim.cpp
 * @brief LLPreviewAnim class implementation
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

#include "llpreviewanim.h"
#include "llbutton.h"
#include "llresmgr.h"
#include "llinventory.h"
#include "llinventoryview.h"
#include "llvoavatarself.h"
#include "llagent.h"          // gAgent
#include "llkeyframemotion.h"
#include "statemachine/aifilepicker.h"
#include "lllineeditor.h"
#include "lluictrlfactory.h"
#include "lluictrlfactory.h"
// <edit>
#include "llviewerwindow.h" // for alert
#include "llappviewer.h" // gStaticVFS
// </edit>

extern LLAgent gAgent;

LLPreviewAnim::LLPreviewAnim(const std::string& name, const LLRect& rect, const std::string& title, const LLUUID& item_uuid, const S32& activate, const LLUUID& object_uuid )	:
	LLPreview( name, rect, title, item_uuid, object_uuid)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_preview_animation.xml");

	childSetAction("Anim play btn",playAnim,this);
	childSetAction("Anim audition btn",auditionAnim,this);

	const LLInventoryItem* item = getItem();
	
	childSetCommitCallback("desc", LLPreview::onText, this);
	childSetText("desc", item->getDescription());
	childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);
	
	setTitle(title);

	if (!getHost())
	{
		LLRect curRect = getRect();
		translate(rect.mLeft - curRect.mLeft, rect.mTop - curRect.mTop);
	}

	// preload the animation
	if(item)
	{
		gAgentAvatarp->createMotion(item->getAssetUUID());
	}
	
	switch ( activate ) 
	{
		case 1:
		{
			playAnim( (void *) this );
			break;
		}
		case 2:
		{
			auditionAnim( (void *) this );
			break;
		}
		default:
		{
		//do nothing
		}
	}
}

// static
void LLPreviewAnim::endAnimCallback( void *userdata )
{
	LLHandle<LLFloater>* handlep = ((LLHandle<LLFloater>*)userdata);
	LLFloater* self = handlep->get();
	delete handlep; // done with the handle
	if (self)
	{
		self->childSetValue("Anim play btn", FALSE);
		self->childSetValue("Anim audition btn", FALSE);
	}
}

// static
void LLPreviewAnim::playAnim( void *userdata )
{
	LLPreviewAnim* self = (LLPreviewAnim*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item)
	{
		LLUUID itemID=item->getAssetUUID();

		LLButton* btn = self->getChild<LLButton>("Anim play btn");
		if (btn)
		{
			btn->toggleState();
		}
		
		if (self->childGetValue("Anim play btn").asBoolean() ) 
		{
			self->mPauseRequest = NULL;
			gAgent.sendAnimationRequest(itemID, ANIM_REQUEST_START);
			
			LLVOAvatar* avatar = gAgentAvatarp;
			LLMotion*   motion = avatar->findMotion(itemID);
			
			if (motion)
			{
				motion->setDeactivateCallback(&endAnimCallback, (void *)(new LLHandle<LLFloater>(self->getHandle())));
			}
		}
		else
		{
			gAgentAvatarp->stopMotion(itemID);
			gAgent.sendAnimationRequest(itemID, ANIM_REQUEST_STOP);
		}
	}
}

// static
void LLPreviewAnim::auditionAnim( void *userdata )
{
	LLPreviewAnim* self = (LLPreviewAnim*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item)
	{
		LLUUID itemID=item->getAssetUUID();

		LLButton* btn = self->getChild<LLButton>("Anim audition btn");
		if (btn)
		{
			btn->toggleState();
		}
		
		if (self->childGetValue("Anim audition btn").asBoolean() ) 
		{
			self->mPauseRequest = NULL;
			gAgentAvatarp->startMotion(item->getAssetUUID());
			
			LLVOAvatar* avatar = gAgentAvatarp;
			LLMotion*   motion = avatar->findMotion(itemID);
			
			if (motion)
			{
				motion->setDeactivateCallback(&endAnimCallback, (void *)(new LLHandle<LLFloater>(self->getHandle())));
			}
		}
		else
		{
			gAgentAvatarp->stopMotion(itemID);
			gAgent.sendAnimationRequest(itemID, ANIM_REQUEST_STOP);
		}
	}
}

// <edit>
// static
/*
void LLPreviewAnim::copyAnim(void *userdata)
{
	LLPreviewAnim* self = (LLPreviewAnim*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item)
	{
		// Some animations aren't hosted on the servers
		// I guess they're in this static vfs thing
		bool static_vfile = false;
		LLVFile* anim_file = new LLVFile(gStaticVFS, item->getAssetUUID(), LLAssetType::AT_ANIMATION);
		if (anim_file && anim_file->getSize())
		{
			//S32 anim_file_size = anim_file->getSize();
			//U8* anim_data = new U8[anim_file_size];
			//if(anim_file->read(anim_data, anim_file_size))
			//{
			//	static_vfile = true;
			//}
			static_vfile = true; // for method 2
			LLPreviewAnim::gotAssetForCopy(gStaticVFS, item->getAssetUUID(), LLAssetType::AT_ANIMATION, self, 0, 0);
		}
		delete anim_file;
		anim_file = NULL;
		
		if(!static_vfile)
		{
			// Get it from the servers
			gAssetStorage->getAssetData(item->getAssetUUID(), LLAssetType::AT_ANIMATION, LLPreviewAnim::gotAssetForCopy, self, TRUE);
		}
	}
}

struct LLSaveInfo
{
	LLSaveInfo(const LLUUID& item_id, const LLUUID& object_id, const std::string& desc,
				const LLTransactionID tid)
		: mItemUUID(item_id), mObjectUUID(object_id), mDesc(desc), mTransactionID(tid)
	{
	}

	LLUUID mItemUUID;
	LLUUID mObjectUUID;
	std::string mDesc;
	LLTransactionID mTransactionID;
};

// static
void LLPreviewAnim::gotAssetForCopy(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLPreviewAnim* self = (LLPreviewAnim*) user_data;
	//const LLInventoryItem *item = self->getItem();

	LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
	S32 size = file.getSize();

	char* buffer = new char[size];
	if (buffer == NULL)
	{
		llerrs << "Memory Allocation Failed" << llendl;
		return;
	}

	file.read((U8*)buffer, size);

	// Write it back out...

	LLTransactionID tid;
	LLAssetID asset_id;
	tid.generate();
	asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

	LLVFile ofile(gVFS, asset_id, LLAssetType::AT_ANIMATION, LLVFile::APPEND);

	ofile.setMaxSize(size);
	ofile.write((U8*)buffer, size);

	// Upload that asset to the database
	LLSaveInfo* info = new LLSaveInfo(self->mItemUUID, self->mObjectUUID, "animation", tid);
	gAssetStorage->storeAssetData(tid, LLAssetType::AT_ANIMATION, onSaveCopyComplete, info, FALSE);

	delete[] buffer;
	buffer = NULL;
}

// static
void LLPreviewAnim::onSaveCopyComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status)
{
	LLSaveInfo* info = (LLSaveInfo*)user_data;

	if (status == 0)
	{
		std::string item_name = "New Animation";
		std::string item_desc = "";
		// Saving into user inventory
		LLViewerInventoryItem* item;
		item = (LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
		if(item)
		{
			item_name = item->getName();
			item_desc = item->getDescription();
		}
		gMessageSystem->newMessageFast(_PREHASH_CreateInventoryItem);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_InventoryBlock);
		gMessageSystem->addU32Fast(_PREHASH_CallbackID, 0);
		gMessageSystem->addUUIDFast(_PREHASH_FolderID, LLUUID::null);
		gMessageSystem->addUUIDFast(_PREHASH_TransactionID, info->mTransactionID);
		gMessageSystem->addU32Fast(_PREHASH_NextOwnerMask, 2147483647);
		gMessageSystem->addS8Fast(_PREHASH_Type, LLAssetType::AT_ANIMATION);
		gMessageSystem->addS8Fast(_PREHASH_InvType, LLInventoryType::IT_ANIMATION);
		gMessageSystem->addU8Fast(_PREHASH_WearableType, 0);
		gMessageSystem->addStringFast(_PREHASH_Name, item_name);
		gMessageSystem->addStringFast(_PREHASH_Description, item_desc);
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
	else
	{
		llwarns << "Problem saving animation: " << status << llendl;
		LLStringUtil::format_map_t args;
		args["[REASON]"] = std::string(LLAssetStorage::getErrorString(status));
		gViewerWindow->alertXml("CannotUploadReason",args);
	}
}
*/
void LLPreviewAnim::copyAnimID(void *userdata)
{
	LLPreviewAnim* self = (LLPreviewAnim*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item)
	{
		gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(item->getAssetUUID().asString()));
	}
}
// </edit>

// <edit>
// virtual
BOOL LLPreviewAnim::canSaveAs() const
{
	return TRUE;
}

// virtual
void LLPreviewAnim::saveAs()
{
	const LLInventoryItem *item = getItem();

	if(item)
	{
		// Some animations aren't hosted on the servers
		// I guess they're in this static vfs thing
		bool static_vfile = false;
		LLVFile* anim_file = new LLVFile(gStaticVFS, item->getAssetUUID(), LLAssetType::AT_ANIMATION);
		if (anim_file && anim_file->getSize())
		{
			//S32 anim_file_size = anim_file->getSize();
			//U8* anim_data = new U8[anim_file_size];
			//if(anim_file->read(anim_data, anim_file_size))
			//{
			//	static_vfile = true;
			//}
			static_vfile = true; // for method 2
			LLPreviewAnim::gotAssetForSave(gStaticVFS, item->getAssetUUID(), LLAssetType::AT_ANIMATION, this, 0, 0);
		}
		delete anim_file;
		anim_file = NULL;

		if(!static_vfile)
		{
			gAssetStorage->getAssetData(item->getAssetUUID(), LLAssetType::AT_ANIMATION, LLPreviewAnim::gotAssetForSave, this, TRUE);
		}
	}
}

// static
void LLPreviewAnim::gotAssetForSave(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLPreviewAnim* self = (LLPreviewAnim*) user_data;
	//const LLInventoryItem *item = self->getItem();

	LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
	S32 size = file.getSize();

	char* buffer = new char[size];
	if (buffer == NULL)
	{
		llerrs << "Memory Allocation Failed" << llendl;
		return;
	}

	file.read((U8*)buffer, size);

	// Write it back out...

	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(LLDir::getScrubbedFileName(self->getItem()->getName()) + ".animatn", FFSAVE_ANIMATN);
	filepicker->run(boost::bind(&LLPreviewAnim::gotAssetForSave_continued, buffer, size, filepicker));
}

// static
void LLPreviewAnim::gotAssetForSave_continued(char* buffer, S32 size, AIFilePicker* filepicker)
{
	if (filepicker->hasFilename())
	{
		std::string filename = filepicker->getFilename();
		std::ofstream export_file(filename.c_str(), std::ofstream::binary);
		export_file.write(buffer, size);
		export_file.close();
	}
	delete[] buffer;
}

// virtual
LLUUID LLPreviewAnim::getItemID()
{
	const LLViewerInventoryItem* item = getItem();
	if(item)
	{
		return item->getUUID();
	}
	return LLUUID::null;
}
// </edit>

void LLPreviewAnim::onClose(bool app_quitting)
{
	const LLInventoryItem *item = getItem();

	if(item)
	{
		gAgentAvatarp->stopMotion(item->getAssetUUID());
		gAgent.sendAnimationRequest(item->getAssetUUID(), ANIM_REQUEST_STOP);
					
		LLVOAvatar* avatar = gAgentAvatarp;
		LLMotion*   motion = avatar->findMotion(item->getAssetUUID());
		
		if (motion)
		{
			// *TODO: minor memory leak here, user data is never deleted (Use real callbacks)
			motion->setDeactivateCallback(NULL, (void *)NULL);
		}
	}
	destroy();
}
