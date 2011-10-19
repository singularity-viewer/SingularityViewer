/**
 * @file hgfloatertexteditor.cpp
 * @brief Asset Text Editor floater made by Hazim Gazov (based on hex editor floater by Day Oh)
 * @author Hazim Gazov
 * 
 * $LicenseInfo:firstyear=2010&license=WTFPLV2$
 *  
 */

// <edit>

#include "llviewerprecompiledheaders.h"

#include "hgfloatertexteditor.h"
#include "lluictrlfactory.h"
#include "llinventorybackup.h" // for downloading
#include "llviewercontrol.h" // gSavedSettings
#include "llviewerwindow.h" // alertXML
#include "llagent.h" // gAgent getID
#include "llviewermenufile.h"
#include "llviewerregion.h" // getCapability
#include "llassetuploadresponders.h" // LLUpdateAgentInventoryResponder
#include "llinventorymodel.h" // gInventory.updateItem
#include "llappviewer.h" // System Folders
#include "llfloaterperms.h" //get default perms
#include "lllocalinventory.h"
#include "llnotificationsutil.h"

std::list<HGFloaterTextEditor*> HGFloaterTextEditor::sInstances;
S32 HGFloaterTextEditor::sUploadAmount = 10;

HGFloaterTextEditor::HGFloaterTextEditor(LLInventoryItem* item)
:	LLFloater()
{
	sInstances.push_back(this);
	mItem = item;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_asset_text_editor.xml");
}

// static
void HGFloaterTextEditor::show(LLUUID item_id)
{
	LLInventoryItem* item = (LLInventoryItem*)gInventory.getItem(item_id);
	if(item)
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("FloaterAssetTextEditorRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		HGFloaterTextEditor* floaterp = new HGFloaterTextEditor(item);
		floaterp->setRect(rect);
		gFloaterView->adjustToFitScreen(floaterp, FALSE);
	}
}

HGFloaterTextEditor::~HGFloaterTextEditor()
{
	sInstances.remove(this);
}

void HGFloaterTextEditor::close(bool app_quitting)
{
	LLFloater::close(app_quitting);
}

BOOL HGFloaterTextEditor::postBuild(void)
{
	LLTextEditor* editor = getChild<LLTextEditor>("text_editor");
	mEditor = editor;

	childSetEnabled("upload_btn", false);
	childSetLabelArg("upload_btn", "[UPLOAD]", std::string("Upload"));
	childSetAction("upload_btn", onClickUpload, this);
	childSetEnabled("save_btn", false);
	childSetAction("save_btn", onClickSave, this);

	if(mItem)
	{
		std::string title = "Text editor: " + mItem->getName();
		const char* asset_type_name = LLAssetType::lookup(mItem->getType());
		if(asset_type_name)
		{
			title.append(" (" + std::string(asset_type_name) + ")");
		}
		setTitle(title);
	}
#if OPENSIM_RULES!=1
	if(mItem->getCreatorUUID() == gAgentID)
	{
#endif /* OPENSIM_RULES!=1 */
		// Load the asset
		editor->setVisible(FALSE);
		childSetText("status_text", std::string("Loading..."));
		LLInventoryBackup::download(mItem, this, imageCallback, assetCallback);
#if OPENSIM_RULES!=1
	} else {
		this->close(false);
	}
#endif /* OPENSIM_RULES!=1 */

	return TRUE;
}

// static
void HGFloaterTextEditor::imageCallback(BOOL success, 
					LLViewerFetchedTexture *src_vi,
					LLImageRaw* src, 
					LLImageRaw* aux_src, 
					S32 discard_level,
					BOOL final,
					void* userdata)
{
	if(final)
	{
		LLInventoryBackup::callbackdata* data = static_cast<LLInventoryBackup::callbackdata*>(userdata);
		HGFloaterTextEditor* floater = (HGFloaterTextEditor*)(data->floater);
		if(!floater) return;
		if(std::find(sInstances.begin(), sInstances.end(), floater) == sInstances.end()) return; // no more crash
		//LLInventoryItem* item = data->item;

		if(!success)
		{
			floater->childSetText("status_text", std::string("Unable to download asset."));
			return;
		}

		U8* src_data = src->getData();
		S32 size = src->getDataSize();
		std::string new_data;
		for(S32 i = 0; i < size; i++)
			new_data += (char)src_data[i];

		delete[] src_data;

		floater->mEditor->setValue(new_data);
		floater->mEditor->setVisible(TRUE);
		floater->childSetText("status_text", std::string("Note: Image data shown isn't the actual asset data, yet"));

		floater->childSetEnabled("save_btn", false);
		floater->childSetEnabled("upload_btn", true);
		floater->childSetLabelArg("upload_btn", "[UPLOAD]", std::string("Upload (L$10)"));
	}
	else
	{
		src_vi->setBoostLevel(LLViewerTexture::BOOST_UI);
	}
}

// static
void HGFloaterTextEditor::assetCallback(LLVFS *vfs,
				   const LLUUID& asset_uuid,
				   LLAssetType::EType type,
				   void* user_data, S32 status, LLExtStat ext_status)
{
	LLInventoryBackup::callbackdata* data = static_cast<LLInventoryBackup::callbackdata*>(user_data);
	HGFloaterTextEditor* floater = (HGFloaterTextEditor*)(data->floater);
	if(!floater) return;
	if(std::find(sInstances.begin(), sInstances.end(), floater) == sInstances.end()) return; // no more crash
	LLInventoryItem* item = data->item;

	if(status != 0 && item->getType() != LLAssetType::AT_NOTECARD)
	{
		floater->childSetText("status_text", std::string("Unable to download asset."));
		return;
	}

	// Todo: this doesn't work for static vfs shit
	LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
	S32 size = file.getSize();

	std::string new_data("");
	if(size > 0)
	{
		char* buffer = new char[size + 1];
	if (buffer == NULL)
	{
		llerrs << "Memory Allocation Failed" << llendl;
		return;
	}

		file.read((U8*)buffer, size);
		buffer[size - 1] = 0;

		new_data = std::string(buffer);
		delete[] buffer;
	}


	floater->mEditor->setText(LLStringExplicit(new_data));
	floater->mEditor->setVisible(TRUE);
	floater->childSetText("status_text", llformat("File Size: %d", size));

	floater->childSetEnabled("upload_btn", true);
	floater->childSetEnabled("save_btn", false);
	if(item->getPermissions().allowModifyBy(gAgent.getID()))
	{
		switch(item->getType())
		{
		case LLAssetType::AT_TEXTURE:
		case LLAssetType::AT_ANIMATION:
		case LLAssetType::AT_SOUND:
			floater->childSetLabelArg("upload_btn", "[UPLOAD]", std::string("Upload (L$10)"));
			break;
		case LLAssetType::AT_LANDMARK:
		case LLAssetType::AT_CALLINGCARD:
			floater->childSetEnabled("upload_btn", false);
			floater->childSetEnabled("save_btn", false);
			break;
		default:
			floater->childSetEnabled("save_btn", true);
			break;
		}
	}
	else
	{
		switch(item->getType())
		{
		case LLAssetType::AT_TEXTURE:
		case LLAssetType::AT_ANIMATION:
		case LLAssetType::AT_SOUND:
			floater->childSetLabelArg("upload_btn", "[UPLOAD]", std::string("Upload (L$10)"));
			break;
		default:
			break;
		}
	}

	// Never enable save if it's a pretend item
	if(gInventory.isObjectDescendentOf(item->getUUID(), gSystemFolderRoot))
	{
		floater->childSetEnabled("save_btn", false);
	}
}

// static
void HGFloaterTextEditor::onClickUpload(void* user_data)
{
	HGFloaterTextEditor* floater = (HGFloaterTextEditor*)user_data;
	LLInventoryItem* item = floater->mItem;

	LLTransactionID transaction_id;
	transaction_id.generate();
	LLUUID fake_asset_id = transaction_id.makeAssetID(gAgent.getSecureSessionID());

	const char* value = floater->mEditor->getText().c_str();
	int size = strlen(value);
	U8* buffer = new U8[size];
	for(int i = 0; i < size; i++)
		buffer[i] = (U8)value[i];
	
	delete[] value;

	LLVFile file(gVFS, fake_asset_id, item->getType(), LLVFile::APPEND);
	file.setMaxSize(size);
	if (!file.write(buffer, size))
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "Couldn't write data to file";
		LLNotificationsUtil::add("ErrorMessage", args);
		return;
	}
	
	LLAssetStorage::LLStoreAssetCallback callback = NULL;
	void *fake_user_data = NULL;

	if(item->getType() != LLAssetType::AT_GESTURE && item->getType() != LLAssetType::AT_LSL_TEXT
		&& item->getType() != LLAssetType::AT_NOTECARD)
	{
		upload_new_resource(transaction_id, 
			item->getType(), 
			item->getName(), 
			item->getDescription(), 
			0, 
			LLFolderType::assetTypeToFolderType(item->getType()), 
			item->getInventoryType(), 
			LLFloaterPerms::getNextOwnerPerms(), LLFloaterPerms::getGroupPerms(), LLFloaterPerms::getEveryonePerms(),
			item->getName(),  
			callback, 
			sUploadAmount,
			fake_user_data);
	}
	else // gestures and scripts, create an item first
	{ // AND notecards
		//if(item->getType() == LLAssetType::AT_NOTECARD) gDontOpenNextNotecard = true;
		create_inventory_item(	gAgent.getID(),
									gAgent.getSessionID(),
									item->getParentUUID(), //gInventory.findCategoryUUIDForType(item->getType()),
									LLTransactionID::tnull,
									item->getName(),
									fake_asset_id.asString(),
									item->getType(),
									item->getInventoryType(),
									(LLWearableType::EType)item->getFlags(),
									PERM_ITEM_UNRESTRICTED,
									new NewResourceItemCallback);
	}
}

struct LLSaveInfo
{
	LLSaveInfo(HGFloaterTextEditor* floater, LLTransactionID transaction_id)
		: mFloater(floater), mTransactionID(transaction_id)
	{
	}

	HGFloaterTextEditor* mFloater;
	LLTransactionID mTransactionID;
};

// static
void HGFloaterTextEditor::onClickSave(void* user_data)
{
	HGFloaterTextEditor* floater = (HGFloaterTextEditor*)user_data;
	LLInventoryItem* item = floater->mItem;

	LLTransactionID transaction_id;
	transaction_id.generate();
	LLUUID fake_asset_id = transaction_id.makeAssetID(gAgent.getSecureSessionID());

	const char* value = floater->mEditor->getText().c_str();
	int size = strlen(value);
	U8* buffer = new U8[size];
	for(int i = 0; i < size; i++)
		buffer[i] = (U8)value[i];

	LLVFile file(gVFS, fake_asset_id, item->getType(), LLVFile::APPEND);
	file.setMaxSize(size);
	if (!file.write(buffer, size))
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "Couldn't write data to file";
		LLNotificationsUtil::add("ErrorMessage", args);
		return;
	}

	bool caps = false;
	std::string url;
	LLSD body;
	body["item_id"] = item->getUUID();

	switch(item->getType())
	{
	case LLAssetType::AT_GESTURE:
		url = gAgent.getRegion()->getCapability("UpdateGestureAgentInventory");
		caps = true;
		break;
	case LLAssetType::AT_LSL_TEXT:
		url = gAgent.getRegion()->getCapability("UpdateScriptAgent");
		body["target"] = "mono";
		caps = true;
		break;
	case LLAssetType::AT_NOTECARD:
		url = gAgent.getRegion()->getCapability("UpdateNotecardAgentInventory");
		caps = true;
		break;
	default: // wearables & notecards, Oct 12 2009
		// ONLY WEARABLES, Oct 15 2009
		floater->childSetText("status_text", std::string("Saving..."));
		LLSaveInfo* info = new LLSaveInfo(floater, transaction_id);
		gAssetStorage->storeAssetData(transaction_id, item->getType(), onSaveComplete, info);
		caps = false;
		break;
	}

	if(caps)
	{
		LLHTTPClient::post(url, body,
					new LLUpdateAgentInventoryResponder(body, fake_asset_id, item->getType()));
	}
}

void HGFloaterTextEditor::onSaveComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status)
{
	LLSaveInfo* info = (LLSaveInfo*)user_data;
	HGFloaterTextEditor* floater = info->mFloater;
	if(std::find(sInstances.begin(), sInstances.end(), floater) == sInstances.end()) return; // no more crash
	LLInventoryItem* item = floater->mItem;

	floater->childSetText("status_text", std::string(""));

	if(item && (status == 0))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setDescription(item->getDescription());
		new_item->setTransactionID(info->mTransactionID);
		new_item->setAssetUUID(asset_uuid);
		new_item->updateServer(FALSE);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
	}
	else
	{
		LLSD args;
		args["ERROR_MESSAGE"] = llformat("Upload failed with status %d, also %d", status, ext_status);
		LLNotificationsUtil::add("ErrorMessage", args);
	}
}

// </edit>
