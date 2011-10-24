// <edit>
#include "llviewerprecompiledheaders.h"
#include "llkeytool.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llcachename.h"
#include "llgroupmgr.h"
#include "lllandmark.h"
#include "llviewerobjectlist.h"
#include "lllocalinventory.h" // used by openKey
#include "llfloateravatarinfo.h" // used by openKey
#include "llfloatergroupinfo.h" // used by openKey
#include "llfloaterparcel.h" // used by openKey
#include "llchat.h" // used by openKey (region id...)
#include "llfloaterchat.h" // used by openKey (region id...)
#include "llfloaterworldmap.h" // used by openKey
#include "llselectmgr.h" // used by openKey
#include "llfloatertools.h" // used by openKey
#include "lltoolmgr.h" // used by openKey
#include "lltoolcomp.h" // used by openKey

std::list<LLKeyTool*> LLKeyTool::mKeyTools;

LLKeyTool::LLKeyTool(LLUUID key, void (*callback)(LLUUID, LLKeyType, LLAssetType::EType, BOOL, void*), void* user_data)
{
	mKey = key;
	mCallback = callback;
	//lets add our handlers to the message system.
	if(mKeyTools.empty())
	{
		gMessageSystem->addHandlerFuncFast(_PREHASH_ObjectPropertiesFamily, &onObjectPropertiesFamily);
		gMessageSystem->addHandlerFuncFast(_PREHASH_ParcelInfoReply, &onParcelInfoReply);
		gMessageSystem->addHandlerFuncFast(_PREHASH_ImageData, &onImageData);
		gMessageSystem->addHandlerFuncFast(_PREHASH_ImageNotInDatabase, &onImageNotInDatabase);
		gMessageSystem->addHandlerFuncFast(_PREHASH_TransferInfo, &onTransferInfo);
	}
	mKeyTools.push_back(this);
	mUserData = user_data;

	tryAgent();
	tryTask();
	tryGroup();
	tryRegion();
	tryParcel();
	tryItem();

	// try assets
	// it seems these come back in reverse order, so order them in reverse...
	std::list<LLAssetType::EType> ordered_asset_types;
	ordered_asset_types.push_back(LLAssetType::AT_BODYPART);
	ordered_asset_types.push_back(LLAssetType::AT_CLOTHING);
	ordered_asset_types.push_back(LLAssetType::AT_GESTURE);
	//ordered_asset_types.push_back(LLAssetType::AT_NOTECARD); // does not work anymore without an item
	ordered_asset_types.push_back(LLAssetType::AT_LANDMARK);
	ordered_asset_types.push_back(LLAssetType::AT_ANIMATION);
	ordered_asset_types.push_back(LLAssetType::AT_SOUND);
	ordered_asset_types.push_back(LLAssetType::AT_TEXTURE);
	for(std::list<LLAssetType::EType>::iterator iter = ordered_asset_types.begin();
		iter != ordered_asset_types.end();
		++iter)
	{
		tryAsset(*iter);
	}
}

LLKeyTool::~LLKeyTool()
{
	// Remove from map
	mKeyTools.remove(this);

	//if empty remove all callbacks
	if(mKeyTools.empty())
	{
		gMessageSystem->delHandlerFuncFast(_PREHASH_ObjectPropertiesFamily, &onObjectPropertiesFamily);
		gMessageSystem->delHandlerFuncFast(_PREHASH_ParcelInfoReply, &onParcelInfoReply);
		gMessageSystem->delHandlerFuncFast(_PREHASH_ImageData, &onImageData);
		gMessageSystem->delHandlerFuncFast(_PREHASH_ImageNotInDatabase, &onImageNotInDatabase);
		gMessageSystem->delHandlerFuncFast(_PREHASH_TransferInfo, &onTransferInfo);
	}
}

// static
std::string LLKeyTool::aWhat(LLKeyTool::LLKeyType key_type, LLAssetType::EType asset_type)
{
	std::string name = "Missingno.";
	std::string type = "Missingno.";
	switch(key_type)
	{
	case KT_AGENT:
		name = "agent";
		break;
	case KT_TASK:
		name = "task";
		break;
	case KT_GROUP:
		name = "group";
		break;
	case KT_REGION:
		name = "region";
		break;
	case KT_PARCEL:
		name = "parcel";
		break;
	case KT_ITEM:
		name = "item";
		break;
	case KT_ASSET:
		type = ll_safe_string(LLAssetType::lookupHumanReadable(asset_type));
		if (!type.empty())
		{
			name = type + " asset";
		}
		break;
	default:
		break;
	}
	return name;
}
static void region_track_callback(const U64& region_handle)
{
	if(region_handle != 0)
	{
		LLVector3d pos_global = from_region_handle(region_handle);
		pos_global += LLVector3d(128.0f, 128.0f, 0.0f);
		gFloaterWorldMap->trackLocation(pos_global);
		LLFloaterWorldMap::show(NULL, TRUE);
	}
}
// static
void LLKeyTool::openKey(LLUUID id, LLKeyType key_type, LLAssetType::EType asset_type)
{
	if(key_type == LLKeyTool::KT_ASSET)
	{
		LLLocalInventory::addItem(id.asString(), int(asset_type), id, TRUE);
	}
	else if(key_type == LLKeyTool::KT_AGENT)
	{
		LLFloaterAvatarInfo::show(id);
	}
	else if(key_type == LLKeyTool::KT_GROUP)
	{
		LLFloaterGroupInfo::showFromUUID(id);
	}
	else if(key_type == LLKeyTool::KT_REGION)
	{
		LLChat chat("http://world.secondlife.com/region/" + id.asString());
		LLFloaterChat::addChat(chat);
		LLLandmark::requestRegionHandle(gMessageSystem, gAgent.getRegionHost(),
			id, boost::bind(&region_track_callback, _2));
	}
	else if(key_type == LLKeyTool::KT_PARCEL)
	{
		LLFloaterParcelInfo::show(id);
	}
	else if(key_type == LLKeyTool::KT_ITEM)
	{
		LLLocalInventory::open(id);
	}
	else if(key_type == LLKeyTool::KT_TASK)
	{
		LLViewerObject* object = gObjectList.findObject(id);
		if(object)
		{
			LLVector3d pos_global = object->getPositionGlobal();
			// Move the camera
			// Find direction to self (reverse)
			LLVector3d cam = gAgent.getPositionGlobal() - pos_global;
			cam.normalize();
			// Go 4 meters back and 3 meters up
			cam *= 4.0f;
			cam += pos_global;
			cam += LLVector3d(0.f, 0.f, 3.0f);

			gAgentCamera.setFocusOnAvatar(FALSE, FALSE);
			gAgentCamera.setCameraPosAndFocusGlobal(cam, pos_global, id);
			gAgentCamera.setCameraAnimating(FALSE);

			if(!object->isAvatar())
			{
				gFloaterTools->open();		/* Flawfinder: ignore */
				LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
				gFloaterTools->setEditTool( LLToolCompTranslate::getInstance() );
				LLSelectMgr::getInstance()->selectObjectAndFamily(object, FALSE);
			}

		}
		else
		{
			// Todo: ObjectPropertiesFamily display
		}
	}
	else
	{
		llwarns << "Unhandled key type " << key_type << llendl;
	}
}

// static
void LLKeyTool::callback(LLUUID id, LLKeyType key_type, LLAssetType::EType asset_type, BOOL is)
{
	std::list<LLKeyTool*>::iterator kt_iter = mKeyTools.begin();
	std::list<LLKeyTool*>::iterator kt_end = mKeyTools.end();
	for( ; kt_iter != kt_end; ++kt_iter)
	{
		if((*kt_iter)->mKey == id)
		{
			LLKeyTool* tool = (*kt_iter);
			BOOL call = FALSE;

			if(key_type != KT_ASSET)
			{
				if(tool->mKeyTypesDone.find(key_type) == tool->mKeyTypesDone.end())
				{
					tool->mKeyTypesDone[key_type] = TRUE;
					call = TRUE;
				}
			}
			else // asset type
			{
				if(tool->mAssetTypesDone.find(asset_type) == tool->mAssetTypesDone.end())
				{
					tool->mAssetTypesDone[asset_type] = TRUE;
					call = TRUE;
				}
			}
			
			if(call)
			{
				tool->mKeyTypesDone[key_type] = is;
				llinfos << tool->mKey << (is ? " is " : " is not ") << aWhat(key_type, asset_type) << llendl;
				if(tool->mCallback)
				{
					tool->mCallback(id, key_type, asset_type, is, tool->mUserData);
				}
			}
		}
	}
}

void LLKeyTool::tryAgent()
{
	gCacheName->get(mKey, FALSE, onCacheName);
}

void LLKeyTool::onCacheName(const LLUUID& id, const std::string& name, bool is_group)
{
	//yes someone could technicaly spoof this but who cares.
	if((!is_group) && (name != LLCacheName::sCacheName["nobody"] &&
					   name != "(???) (???)"))
		callback(id, LLKeyTool::KT_AGENT, LLAssetType::AT_NONE, TRUE);
	else
		callback(id, LLKeyTool::KT_AGENT, LLAssetType::AT_NONE, FALSE);
}

void LLKeyTool::tryTask()
{
	if(gObjectList.findObject(mKey))
	{
		callback(mKey, KT_TASK, LLAssetType::AT_NONE, TRUE);
	}
	else
	{
		gMessageSystem->newMessage(_PREHASH_RequestObjectPropertiesFamily);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_RequestFlags, 0);
		gMessageSystem->addUUIDFast(_PREHASH_ObjectID, mKey);
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
}

// static
void LLKeyTool::onObjectPropertiesFamily(LLMessageSystem *msg, void **user_data)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, id);

	callback(id, KT_TASK, LLAssetType::AT_NONE, TRUE);
}

void LLKeyTool::tryGroup()
{
	LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mKey);
}

// static
void LLKeyTool::gotGroupProfile(LLUUID id)
{
	callback(id, LLKeyTool::KT_GROUP, LLAssetType::AT_NONE, TRUE);
}

static void  keytool_region_handle_callback(const LLUUID& region_id, const U64& region_handle)
{
	if(region_handle == 0) LLKeyTool::callback(region_id, LLKeyTool::KT_REGION, LLAssetType::AT_NONE, FALSE);
	else LLKeyTool::callback(region_id, LLKeyTool::KT_REGION, LLAssetType::AT_NONE, TRUE);
}

void LLKeyTool::tryRegion()
{
	LLLandmark::requestRegionHandle(gMessageSystem, gAgent.getRegionHost(),
		mKey, boost::bind(&keytool_region_handle_callback,_1,_2));
}

void LLKeyTool::tryParcel()
{
	gMessageSystem->newMessage(_PREHASH_ParcelInfoRequest);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_Data);
	gMessageSystem->addUUIDFast(_PREHASH_ParcelID, mKey);
	gMessageSystem->sendReliable(gAgent.getRegionHost());
}

void LLKeyTool::onParcelInfoReply(LLMessageSystem *msg, void **user_data)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_ParcelID, id);
	
	callback(id, KT_PARCEL, LLAssetType::AT_NONE, TRUE);
}

void LLKeyTool::tryItem()
{
	if(gInventory.getItem(mKey))
		callback(mKey, KT_ITEM, LLAssetType::AT_NONE, TRUE);
	else
		callback(mKey, KT_ITEM, LLAssetType::AT_NONE, FALSE);
}

void LLKeyTool::tryAsset(LLAssetType::EType asset_type)
{
	if(asset_type == LLAssetType::AT_TEXTURE)
	{
		gMessageSystem->newMessageFast(_PREHASH_RequestImage);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_RequestImage);
		gMessageSystem->addUUIDFast(_PREHASH_Image, mKey);
		gMessageSystem->addS8Fast(_PREHASH_DiscardLevel, 0);
		gMessageSystem->addF32Fast(_PREHASH_DownloadPriority, 1015000);
		gMessageSystem->addU32Fast(_PREHASH_Packet, 0);
		gMessageSystem->addU8Fast(_PREHASH_Type, 0); // TYPE_NORMAL
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
	else
	{		
		S32 type = (S32)(asset_type);
		LLUUID transfer_id;
		transfer_id.generate();
		U8 params[20];
		LLDataPackerBinaryBuffer dpb(params, 20);
		dpb.packUUID(mKey, "AssetID");
		dpb.packS32(type, "AssetType");

		gMessageSystem->newMessageFast(_PREHASH_TransferRequest);
		gMessageSystem->nextBlockFast(_PREHASH_TransferInfo);
		gMessageSystem->addUUIDFast(_PREHASH_TransferID, transfer_id);
		gMessageSystem->addS32Fast(_PREHASH_ChannelType, 2); // LLTCT_ASSET (e_transfer_channel_type)
		gMessageSystem->addS32Fast(_PREHASH_SourceType, 2); // LLTST_ASSET (e_transfer_source_type)
		gMessageSystem->addF32Fast(_PREHASH_Priority, 100.0f);
		gMessageSystem->addBinaryDataFast(_PREHASH_Params, params, 20);
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
}

// static
void LLKeyTool::onTransferInfo(LLMessageSystem *msg, void **user_data)
{
	S32 params_size = msg->getSize(_PREHASH_TransferInfo, _PREHASH_Params);
	if(params_size < 1) return;
	U8 tmp[1024];
	msg->getBinaryDataFast(_PREHASH_TransferInfo, _PREHASH_Params, tmp, params_size);
	LLDataPackerBinaryBuffer dpb(tmp, 1024);
	LLUUID asset_id;
	dpb.unpackUUID(asset_id, "AssetID");
	S32 asset_type;
	dpb.unpackS32(asset_type, "AssetType");
	S32 status;
	msg->getS32Fast(_PREHASH_TransferInfo, _PREHASH_Status, status, 0);

	callback(asset_id, KT_ASSET, (LLAssetType::EType)asset_type, BOOL(status == 0)); // LLTS_OK (e_status_codes)
}

// static
void LLKeyTool::onImageData(LLMessageSystem *msg, void **user_data)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_ImageID, _PREHASH_ID, id, 0);
	callback(id, KT_ASSET, LLAssetType::AT_TEXTURE, TRUE);
}

// static
void LLKeyTool::onImageNotInDatabase(LLMessageSystem* msg, void **user_data)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_ImageID, _PREHASH_ID, id, 0);
	callback(id, KT_ASSET, LLAssetType::AT_TEXTURE, FALSE);
}

// </edit>