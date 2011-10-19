/**
 * @file dofloaterhex.h
 * @brief Hex Editor Floater made by Day
 * @author Day Oh
 * 
 * $LicenseInfo:firstyear=2009&license=WTFPLV2$
 *  
 */

// <edit>

#include "llviewerprecompiledheaders.h"

#include "dofloaterhex.h"
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
#include "llnotificationsutil.h"

std::list<DOFloaterHex*> DOFloaterHex::sInstances;
S32 DOFloaterHex::sUploadAmount = 10;

DOFloaterHex::DOFloaterHex(LLUUID item_id, BOOL vfs, LLAssetType::EType asset_type)
:	LLFloater()
{
	sInstances.push_back(this);

	mVFS = vfs;

	//we are editing an asset directly from the VFS
	if(vfs)
	{
		mAssetId = item_id;
		mAssetType = asset_type;

	//we are editing an inventory item
	} else {
		mItem = (LLInventoryItem*)gInventory.getItem(item_id);
		mAssetId = mItem->getAssetUUID();
		mAssetType = mItem->getType();
	}
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_hex.xml");
}

//this bit should be rewritten entirely
void DOFloaterHex::show(LLUUID item_id, BOOL vfs, LLAssetType::EType asset_type)
{
	if(!vfs)
	{
		LLInventoryItem* item = (LLInventoryItem*)gInventory.getItem(item_id);
		if(item)
		{
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("FloaterHexRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);

			DOFloaterHex* floaterp = new DOFloaterHex(item_id);
			floaterp->setRect(rect);

			gFloaterView->adjustToFitScreen(floaterp, FALSE);
		}
	} else if (item_id.notNull() && asset_type != LLAssetType::AT_NONE) {
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("FloaterHexRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		DOFloaterHex* floaterp = new DOFloaterHex(item_id, true, asset_type);
		floaterp->setRect(rect);

		llinfos << "Asset ID: " << item_id.asString() << llendl;

		gFloaterView->adjustToFitScreen(floaterp, FALSE);
	}
}

DOFloaterHex::~DOFloaterHex()
{
	sInstances.remove(this);
}

void DOFloaterHex::close(bool app_quitting)
{
	LLFloater::close(app_quitting);
}

BOOL DOFloaterHex::postBuild(void)
{
	DOHexEditor* editor = getChild<DOHexEditor>("hex");
	mEditor = editor;
#ifndef COLUMN_SPAN
	// Set number of columns
	U8 columns = U8(gSavedSettings.getU32("HexEditorColumns"));
	editor->setColumns(columns);
	// Reflect clamped U8ness in settings
	gSavedSettings.setU32("HexEditorColumns", U32(columns));
#endif
	handleSizing();

	childSetEnabled("upload_btn", false);
	childSetLabelArg("upload_btn", "[UPLOAD]", std::string("Upload"));
	childSetAction("upload_btn", onClickUpload, this);
	childSetEnabled("save_btn", false);
	childSetAction("save_btn", onClickSave, this);

	if(!mVFS && mItem)
	{
		std::string title = "Hex editor: " + mItem->getName();
		const char* asset_type_name = LLAssetType::lookup(mItem->getType());
		if(asset_type_name)
		{
			title.append(" (" + std::string(asset_type_name) + ")");
		}
		setTitle(title);
	}
	if(!mVFS)
	{
		// Load the asset
		editor->setVisible(FALSE);
		childSetText("status_text", std::string("Loading..."));
		LLInventoryBackup::download(mItem, this, imageCallback, assetCallback);
	}
	else if (mVFS) //the asset already exists in the VFS, we don't need to fetch it
		           //and we don't want to associate it with an item
	{
		setTitle(mAssetId.asString());
		readVFile();
	} else {
		this->close(false);
	}

	return TRUE;
}

// static
void DOFloaterHex::imageCallback(BOOL success, 
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
		DOFloaterHex* floater = (DOFloaterHex*)(data->floater);
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
		std::vector<U8> new_data;
		for(S32 i = 0; i < size; i++)
			new_data.push_back(src_data[i]);

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
void DOFloaterHex::assetCallback(LLVFS *vfs,
				   const LLUUID& asset_uuid,
				   LLAssetType::EType type,
				   void* user_data, S32 status, LLExtStat ext_status)
{

	LLInventoryBackup::callbackdata* data = static_cast<LLInventoryBackup::callbackdata*>(user_data);
	DOFloaterHex* floater = (DOFloaterHex*)(data->floater);
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

	char* buffer = new char[size];
	if (buffer == NULL)
	{
		llerrs << "Memory Allocation Failed" << llendl;
		return;
	}

	file.read((U8*)buffer, size);

	std::vector<U8> new_data;
	for(S32 i = 0; i < size; i++)
		new_data.push_back(buffer[i]);

	delete[] buffer;

	floater->mEditor->setValue(new_data);
	floater->mEditor->setVisible(TRUE);
	floater->childSetText("status_text", std::string(""));

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
void DOFloaterHex::onClickUpload(void* user_data)
{

	DOFloaterHex* floater = (DOFloaterHex*)user_data;
	LLInventoryItem* item = floater->mItem;

	LLTransactionID transaction_id;
	transaction_id.generate();
	LLUUID fake_asset_id = transaction_id.makeAssetID(gAgent.getSecureSessionID());

	std::vector<U8> value = floater->mEditor->getValue();
	int size = value.size();
	U8* buffer = new U8[size];
	for(int i = 0; i < size; i++)
		buffer[i] = value[i];
	value.clear();

	LLVFile file(gVFS, fake_asset_id, item->getType(), LLVFile::APPEND);
	file.setMaxSize(size);
	if (!file.write(buffer, size))
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "Couldn't write data to file";
		LLNotificationsUtil::add("ErrorMessage", args);
		return;
	}
	delete[] buffer;
	
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
	LLSaveInfo(DOFloaterHex* floater, LLTransactionID transaction_id)
		: mFloater(floater), mTransactionID(transaction_id)
	{
	}

	DOFloaterHex* mFloater;
	LLTransactionID mTransactionID;
};

// static
void DOFloaterHex::onClickSave(void* user_data)
{
	DOFloaterHex* floater = (DOFloaterHex*)user_data;
	LLInventoryItem* item = floater->mItem;

	LLTransactionID transaction_id;
	transaction_id.generate();
	LLUUID fake_asset_id = transaction_id.makeAssetID(gAgent.getSecureSessionID());

	std::vector<U8> value = floater->mEditor->getValue();
	int size = value.size();
	U8* buffer = new U8[size];
	for(int i = 0; i < size; i++)
		buffer[i] = value[i];
	value.clear();

	LLVFile file(gVFS, fake_asset_id, item->getType(), LLVFile::APPEND);
	file.setMaxSize(size);
	if (!file.write(buffer, size))
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "Couldn't write data to file";
		LLNotificationsUtil::add("ErrorMessage", args);
		return;
	}
	delete[] buffer;


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

void DOFloaterHex::onSaveComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status)
{
	LLSaveInfo* info = (LLSaveInfo*)user_data;
	DOFloaterHex* floater = info->mFloater;
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

void DOFloaterHex::onCommitColumnCount(LLUICtrl *control, void *user_data)
{
	if(control && user_data)
	{
		DOFloaterHex *instance = (DOFloaterHex *)user_data;
		U8 columns = llclamp<U8>((U8)llfloor(control->getValue().asReal()), 0x00, 0xFF);
		instance->mEditor->setColumns(columns);
		gSavedSettings.setU32("HexEditorColumns", (U32)instance->mEditor->getColumns());
		instance->handleSizing();
	}
}

void DOFloaterHex::handleSizing()
{
	// Reshape a little based on columns
	S32 min_width = S32(mEditor->getSuggestedWidth(MIN_COLS)) + 20;
	setResizeLimits(min_width, getMinHeight());
	if(getRect().getWidth() < min_width)
	{
		//LLRect rect = getRect();
		//rect.setOriginAndSize(rect.mLeft, rect.mBottom, min_width, rect.getHeight());
		//setRect(rect);

		reshape(min_width, getRect().getHeight(), FALSE);
		mEditor->reshape(mEditor->getRect().getWidth(), mEditor->getRect().getHeight(), TRUE);
	}
}

void DOFloaterHex::readVFile()
{
	if(!mVFS) return;
	// quick cut paste job
	// Todo: this doesn't work for static vfs shit
	LLVFile file(gVFS, mAssetId, mAssetType, LLVFile::READ);
	if(file.getVFSThread())
	{
		S32 size = file.getSize();

		char* buffer = new char[size];
		if (buffer == NULL)
		{
			llerrs << "Memory Allocation Failed" << llendl;
			return;
		}

		file.read((U8*)buffer, size);

		std::vector<U8> new_data;
		for(S32 i = 0; i < size; i++)
			new_data.push_back(buffer[i]);

		delete[] buffer;

		mEditor->setValue(new_data);
		mEditor->setVisible(TRUE);

	}

	childSetText("status_text", std::string("Editing VFile"));

	childSetEnabled("upload_btn", false);
	childSetEnabled("save_btn", false);
}

// </edit>
