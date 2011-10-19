/** 
 * @file llpreviewsound.cpp
 * @brief LLPreviewSound class implementation
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

#include "llaudioengine.h"
#include "llagent.h"          // gAgent
#include "llbutton.h"
#include "llinventory.h"
#include "llinventoryview.h"
#include "lllineeditor.h"
#include "llpreviewsound.h"
#include "llresmgr.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"  // send_guid_sound_trigger
#include "lluictrlfactory.h"
// <edit>
#include "llvoavatarself.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llviewerwindow.h" // for alert
#include "statemachine/aifilepicker.h"
// for ambient play:
#include "llviewerregion.h"
// </edit>

extern LLAudioEngine* gAudiop;
extern LLAgent gAgent;

const F32 SOUND_GAIN = 1.0f;

LLPreviewSound::LLPreviewSound(const std::string& name, const LLRect& rect, const std::string& title, const LLUUID& item_uuid, const LLUUID& object_uuid)	:
	LLPreview( name, rect, title, item_uuid, object_uuid)
{
	
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_preview_sound.xml");

	childSetAction("Sound play btn",&LLPreviewSound::playSound,this);
	childSetAction("Sound audition btn",&LLPreviewSound::auditionSound,this);
	// <edit>
	childSetAction("Sound copy uuid btn", &LLPreviewSound::copyUUID, this);
	childSetAction("Play ambient btn", &LLPreviewSound::playAmbient, this);
	// </edit>

	LLButton* button = getChild<LLButton>("Sound play btn");
	button->setSoundFlags(LLView::SILENT);
	
	button = getChild<LLButton>("Sound audition btn");
	button->setSoundFlags(LLView::SILENT);

	const LLInventoryItem* item = getItem();
	
	childSetCommitCallback("desc", LLPreview::onText, this);
	childSetText("desc", item->getDescription());
	childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);	
	
	// preload the sound
	if(item && gAudiop)
	{
		gAudiop->preloadSound(item->getAssetUUID());
		// <edit>
		// that thing above doesn't actually start a sound transfer, so I will do it
		//LLAudioSource *asp = new LLAudioSource(gAgent.getID(), gAgent.getID(), F32(1.0f), LLAudioEngine::AUDIO_TYPE_UI);
		LLAudioSource *asp = gAgentAvatarp->getAudioSource(gAgent.getID());
		LLAudioData *datap = gAudiop->getAudioData(item->getAssetUUID());
		asp->addAudioData(datap, FALSE);
		// </edit>
	}
	
	setTitle(title);

	if (!getHost())
	{
		LLRect curRect = getRect();
		translate(rect.mLeft - curRect.mLeft, rect.mTop - curRect.mTop);
	}

}

// static
void LLPreviewSound::playSound( void *userdata )
{
	LLPreviewSound* self = (LLPreviewSound*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item && gAudiop)
	{
		send_sound_trigger(item->getAssetUUID(), SOUND_GAIN);
	}
}

// static
void LLPreviewSound::auditionSound( void *userdata )
{
	LLPreviewSound* self = (LLPreviewSound*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item && gAudiop)
	{
		LLVector3d lpos_global = gAgent.getPositionGlobal();
		gAudiop->triggerSound(item->getAssetUUID(), gAgent.getID(), SOUND_GAIN, LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
	}
}

// <edit>
void LLPreviewSound::playAmbient( void* userdata )
{
	LLPreviewSound* self = (LLPreviewSound*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item && gAudiop)
	{
		int gain = 0.01f;
		for(int i = 0; i < 2; i++)
		{
			gMessageSystem->newMessageFast(_PREHASH_SoundTrigger);
			gMessageSystem->nextBlockFast(_PREHASH_SoundData);
			gMessageSystem->addUUIDFast(_PREHASH_SoundID, LLUUID(item->getAssetUUID()));
			gMessageSystem->addUUIDFast(_PREHASH_OwnerID, LLUUID::null);
			gMessageSystem->addUUIDFast(_PREHASH_ObjectID, LLUUID::null);
			gMessageSystem->addUUIDFast(_PREHASH_ParentID, LLUUID::null);
			gMessageSystem->addU64Fast(_PREHASH_Handle, gAgent.getRegion()->getHandle());
			LLVector3d	pos = -from_region_handle(gAgent.getRegion()->getHandle());
			gMessageSystem->addVector3Fast(_PREHASH_Position, (LLVector3)pos);
			gMessageSystem->addF32Fast(_PREHASH_Gain, gain);

			gMessageSystem->sendReliable(gAgent.getRegionHost());

			gain = 1.0f;
		}
	}
}
// <edit>
/*
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
void LLPreviewSound::makeCopy( void *userdata )
{
	LLPreviewSound* self = (LLPreviewSound*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item && gAudiop)
	{
		// Find out if asset data is ready
		// I might be able to get rid of this
		if(!gAssetStorage->hasLocalAsset(item->getAssetUUID(), LLAssetType::AT_SOUND))
		{
			LLChat chat("Sound isn't downloaded yet, please try again in a moment.");
			LLFloaterChat::addChat(chat);
			return;
		}

		gAssetStorage->getAssetData(item->getAssetUUID(), LLAssetType::AT_SOUND, LLPreviewSound::gotAssetForCopy, self, TRUE);
	}
}

// static
void LLPreviewSound::gotAssetForCopy(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLPreviewSound* self = (LLPreviewSound*) user_data;
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

	LLVFile ofile(gVFS, asset_id, LLAssetType::AT_SOUND, LLVFile::APPEND);

	ofile.setMaxSize(size);
	ofile.write((U8*)buffer, size);

	// Upload that asset to the database
	LLSaveInfo* info = new LLSaveInfo(self->mItemUUID, self->mObjectUUID, "sound", tid);
	gAssetStorage->storeAssetData(tid, LLAssetType::AT_SOUND, onSaveCopyComplete, info, FALSE);
}

// static
void LLPreviewSound::onSaveCopyComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status)
{
	LLSaveInfo* info = (LLSaveInfo*)user_data;

	if (status == 0)
	{
		std::string item_name = "New Sound";
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
		gMessageSystem->addS8Fast(_PREHASH_Type, 1);
		gMessageSystem->addS8Fast(_PREHASH_InvType, 1);
		gMessageSystem->addU8Fast(_PREHASH_WearableType, 0);
		gMessageSystem->addStringFast(_PREHASH_Name, item_name);
		gMessageSystem->addStringFast(_PREHASH_Description, item_desc);
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
	else
	{
		llwarns << "Problem saving sound: " << status << llendl;
		LLStringUtil::format_map_t args;
		args["[REASON]"] = std::string(LLAssetStorage::getErrorString(status));
		gViewerWindow->alertXml("CannotUploadReason",args);
	}
}
*/
// static
void LLPreviewSound::copyUUID( void *userdata )
{
	LLPreviewSound* self = (LLPreviewSound*) userdata;
	const LLInventoryItem *item = self->getItem();

	if(item )
	{
		gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(item->getAssetUUID().asString()));
	}
}
// </edit>

// <edit>
// virtual
BOOL LLPreviewSound::canSaveAs() const
{
	return TRUE;
}

// virtual
void LLPreviewSound::saveAs()
{
	const LLInventoryItem *item = getItem();

	if(item)
	{
		gAssetStorage->getAssetData(item->getAssetUUID(), LLAssetType::AT_SOUND, LLPreviewSound::gotAssetForSave, this, TRUE);
	}
}

// static
void LLPreviewSound::gotAssetForSave(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLPreviewSound* self = (LLPreviewSound*) user_data;
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
	filepicker->open(LLDir::getScrubbedFileName(self->getItem()->getName()) + ".ogg", FFSAVE_OGG);
	filepicker->run(boost::bind(&LLPreviewSound::gotAssetForSave_continued, buffer, size, filepicker));
}

// static
void LLPreviewSound::gotAssetForSave_continued(char* buffer, S32 size, AIFilePicker* filepicker)
{
	if (filepicker->hasFilename())
	{
		std::string filename = filepicker->getFilename();
		std::ofstream export_file(filename.c_str(), std::ofstream::binary);
		export_file.write(buffer, size);
		export_file.close();
	}
	delete [] buffer;
}

// virtual
LLUUID LLPreviewSound::getItemID()
{
	const LLViewerInventoryItem* item = getItem();
	if(item)
	{
		return item->getUUID();
	}
	return LLUUID::null;
}
// </edit>
