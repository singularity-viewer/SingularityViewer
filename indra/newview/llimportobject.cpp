
// <edit>
/** 
 * @file llimportobject.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "llapr.h"

#include "roles_constants.h"
#include "llimportobject.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llselectmgr.h"
#include "llviewerobject.h"
#include "llagent.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llfloater.h"
#include "lllineeditor.h"
#include "llinventorymodel.h"
#include "llinventorydefines.h"
#include "lluictrlfactory.h"
#include "llscrolllistctrl.h"
#include "llviewercontrol.h"
#include "llfloaterimport.h"
#include "llassetuploadresponders.h"
#include "lleconomy.h"
#include "llfloaterperms.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerobjectlist.h"
#include "llsdutil_math.h"
#include "llimagej2c.h"
// static vars
bool LLXmlImport::sImportInProgress = false;
bool LLXmlImport::sImportHasAttachments = false;
eImportObjectState LLXmlImport::sState = IMPORT_INIT;
LLUUID LLXmlImport::sExpectedUpdate;
LLUUID LLXmlImport::sFolderID;
LLViewerObject* LLXmlImport::sSupplyParams;
int LLXmlImport::sPrimsNeeded;
std::vector<LLImportObject*> LLXmlImport::sPrims;
std::map<std::string, U8> LLXmlImport::sId2attachpt;
std::map<U8, bool> LLXmlImport::sPt2watch;
std::map<U8, LLVector3> LLXmlImport::sPt2attachpos;
std::map<U8, LLQuaternion> LLXmlImport::sPt2attachrot;
std::map<U32, std::vector<LLViewerObject*> > LLXmlImport::sLinkSets;
std::map<U8, std::string> LLXmlImport::sDescriptions;
int LLXmlImport::sPrimIndex = 0;
int LLXmlImport::sAttachmentsDone = 0;
std::map<std::string, U32> LLXmlImport::sId2localid;
std::map<U32, LLVector3> LLXmlImport::sRootpositions;
std::map<U32, LLQuaternion> LLXmlImport::sRootrotations;
LLXmlImportOptions* LLXmlImport::sXmlImportOptions;
std::map<LLUUID,LLUUID> LLXmlImport::sTextureReplace;
int LLXmlImport::sTotalAssets = 0;
int LLXmlImport::sUploadedAssets = 0;
// timer for watching link
class LLLinkTimer : public LLEventTimer
{
public: 
	LLLinkTimer(std::vector<LLUUID> roots) : LLEventTimer(0.05f)
	{
		mOptions = LLXmlImport::sXmlImportOptions;
		mRoots = roots;
	}
	virtual BOOL tick()
	{
		// Import for this timer has been cancled :<
		if(!LLXmlImport::sImportInProgress || (LLXmlImport::sXmlImportOptions && mOptions && mOptions != LLXmlImport::sXmlImportOptions)) return TRUE;
		// LET US CONTINUE
		std::vector<LLUUID> unlinked_roots;
		for(std::vector<LLUUID>::iterator itr = mRoots.begin();itr != mRoots.end();itr++)
		{
			LLViewerObject* object = gObjectList.findObject((*itr));
			if(object)
			{
				if(object->numChildren() == 0)
				{
					llinfos << "WAIT " << (*itr).asString() << llendl;
					unlinked_roots.push_back((*itr));

				}
			}
		}
		if(unlinked_roots.size() > 0)
		{
			mRoots = unlinked_roots;
			return FALSE;
		}
		else
		{
			LLXmlImport::finish_link();
			return TRUE;
		}
	}
	
protected:
	LLXmlImportOptions* mOptions;
	std::vector<LLUUID> mRoots;
};
bool	operator==(const LLXmlImportOptions &a, const LLXmlImportOptions &b)
{
	return (a.mID == b.mID);
}

bool	operator!=(const LLXmlImportOptions &a, const LLXmlImportOptions &b)
{
	return (a.mID != b.mID);
}
LLXmlImportOptions::LLXmlImportOptions(LLXmlImportOptions* options)
{
	mName = options->mName;
	mRootObjects = options->mRootObjects;
	mChildObjects = options->mChildObjects;
	mWearables = options->mWearables;
	mSupplier = options->mSupplier;
	mKeepPosition = options->mKeepPosition;
	mAssetDir = options->mAssetDir;
	mID = options->mID;
}
LLXmlImportOptions::LLXmlImportOptions(std::string filename)
:	mSupplier(NULL),
	mKeepPosition(FALSE)
{
	mName = gDirUtilp->getBaseFileName(filename, true);
	llifstream in(filename);
	if(!in.is_open())
	{
		llwarns << "Couldn't open file..." << llendl;
		return;
	}
	LLSD llsd;
	if(LLSDSerialize::fromXML(llsd, in) < 1)
	{
		llwarns << "Messed up data?" << llendl;
		return;
	}
	mAssetDir = filename.substr(0,filename.find_last_of(".")) + "_assets";
	init(llsd);
}
LLXmlImportOptions::LLXmlImportOptions(LLSD llsd)
:	mName("stuff"),
	mSupplier(NULL),
	mKeepPosition(FALSE)
{
	init(llsd);
}
LLXmlImportOptions::~LLXmlImportOptions()
{
	clear();
}
void LLXmlImportOptions::clear()
{
	//for_each(mRootObjects.begin(), mRootObjects.end(), DeletePointer());
	mRootObjects.clear();
	//for_each(mChildObjects.begin(), mChildObjects.end(), DeletePointer());
	mChildObjects.clear();
	//for_each(mWearables.begin(), mWearables.end(), DeletePointer());
	mWearables.clear();
	//for_each(mAssets.begin(), mAssets.end(), DeletePointer());
	mAssets.clear();
}
void LLXmlImportOptions::init(LLSD llsd)
{
	clear();
	// Separate objects and wearables
	std::vector<LLImportObject*> unsorted_objects;
	LLSD::map_iterator map_end = llsd.endMap();
	for(LLSD::map_iterator map_iter = llsd.beginMap() ; map_iter != map_end; ++map_iter)
	{
		std::string key((*map_iter).first);
		LLSD item = (*map_iter).second;
		if(item.has("type"))
		{
			if(item["type"].asString() == "wearable")
				mWearables.push_back(new LLImportWearable(item));
			else
				unsorted_objects.push_back(new LLImportObject(key, item));
		}
		else // assumed to be a prim
			unsorted_objects.push_back(new LLImportObject(key, item));
	}
	// Separate roots from children
	int total_objects = (int)unsorted_objects.size();
	for(int i = 0; i < total_objects; i++)
	{
		if(unsorted_objects[i]->mParentId == "")
			mRootObjects.push_back(unsorted_objects[i]);
		else
			mChildObjects.push_back(unsorted_objects[i]);
	}

	mID.generate();
	
}
LLImportAssetData::LLImportAssetData(std::string infilename,LLUUID inassetid,LLAssetType::EType intype)
{
	LLTransactionID _tid;
	_tid.generate();
	assetid = _tid.makeAssetID(gAgent.getSecureSessionID());
	tid = _tid;
	oldassetid = inassetid;
	type = intype;
	inv_type = LLInventoryType::defaultForAssetType(type);
	name = inassetid.asString(); //use the original asset id as the name
	description = "";
	filename = infilename;
}
//We dont need this yet, but its useful to have
/*class LLImportTransferCallback : public LLInventoryCallback
{
public:
        LLImportTransferCallback(LLImportAssetData* data)
        {
                mData = data;
        }
        void fire(const LLUUID &inv_item)
        {
		//add to the inventory inject array and inject after the prim has been made.
        }
private:
        LLImportAssetData* data;
};
*/
class LLImportInventoryResponder : public LLAssetUploadResponder
{
public:
        LLImportInventoryResponder(const LLSD& post_data,
                                                                const LLUUID& vfile_id,
                                                                LLAssetType::EType asset_type, LLImportAssetData* data) : LLAssetUploadResponder(post_data, vfile_id, asset_type)
        {
                mData = data;
        }

        LLImportInventoryResponder(const LLSD& post_data, const std::string& file_name,
                                                                                           LLAssetType::EType asset_type) : LLAssetUploadResponder(post_data, file_name, asset_type)
        {

        }
        virtual void uploadComplete(const LLSD& content)
        {
		llinfos << "Adding " << content["new_inventory_item"].asUUID() << " " << content["new_asset"].asUUID() << " to inventory." << llendl;
		//LLPointer<LLInventoryCallback> cb = new LLImportTransferCallback(mData);
		
                if(mPostData["folder_id"].asUUID().notNull())
		{
			LLPermissions perm;
			U32 everyone_perms = PERM_NONE;
			U32 group_perms = PERM_NONE;
			U32 next_owner_perms = PERM_ALL;
			perm.init(gAgent.getID(), gAgent.getID(), LLUUID::null, LLUUID::null);
			if(content.has("new_next_owner_mask"))
			{
				// This is a new sim that provides creation perms so use them.
				// Do not assume we got the perms we asked for in mPostData 
				// since the sim may not have granted them all.
				everyone_perms = content["new_everyone_mask"].asInteger();
				group_perms = content["new_group_mask"].asInteger();
				next_owner_perms = content["new_next_owner_mask"].asInteger();
			}
			else 
			{
				// This old sim doesn't provide creation perms so use old assumption-based perms.
				if(mPostData["inventory_type"].asString() != "snapshot")
				{
					next_owner_perms = PERM_MOVE | PERM_TRANSFER;
				}
			}
			perm.initMasks(PERM_ALL, PERM_ALL, everyone_perms, group_perms, next_owner_perms);
			S32 creation_date_now = time_corrected();
			LLPointer<LLViewerInventoryItem> item = new LLViewerInventoryItem(content["new_inventory_item"].asUUID(),
												mPostData["folder_id"].asUUID(),
												perm,
												content["new_asset"].asUUID(),
												mData->type,
												mData->inv_type,
												mPostData["name"].asString(),
												mPostData["description"].asString(),
												LLSaleInfo::DEFAULT,
												LLInventoryItemFlags::II_FLAGS_NONE,
												creation_date_now);
			gInventory.updateItem(item);
			gInventory.notifyObservers();
			LLXmlImport::sTextureReplace[mData->oldassetid] = content["new_asset"].asUUID();
		}
		LLXmlImport::sUploadedAssets++;
		if(!LLXmlImport::sImportInProgress) return;
		LLFloaterImportProgress::update();
		if(LLXmlImport::sUploadedAssets < LLXmlImport::sTotalAssets)
		{
			LLImportAssetData* data = LLXmlImport::sXmlImportOptions->mAssets[LLXmlImport::sUploadedAssets];
		        data->folderid = mData->folderid;
			std::string url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
			if(!url.empty())
			{
				LLPointer<LLImageJ2C> integrity_test = new LLImageJ2C;
				if( !integrity_test->loadAndValidate( data->filename ) )
				{
					llinfos << "Image: " << data->filename << " is corrupt." << llendl;
				}
				S32 file_size;
				LLAPRFile infile ;
				infile.open(data->filename, LL_APR_RB, LLAPRFile::short_lived, &file_size);
				if (infile.getFileHandle())
				{
					LLVFile file(gVFS, data->assetid, data->type, LLVFile::WRITE);
					file.setMaxSize(file_size);
					const S32 buf_size = 65536;
					U8 copy_buf[buf_size];
					while ((file_size = infile.read(copy_buf, buf_size)))
					{
						file.write(copy_buf, file_size);
					}
					
					LLSD body;
					body["folder_id"] = data->folderid;
					body["asset_type"] = LLAssetType::lookup(data->type);
					body["inventory_type"] = LLInventoryType::lookup(data->inv_type);
					body["name"] = data->name;
					body["description"] = data->description;
					body["next_owner_mask"] = LLSD::Integer(U32_MAX);
					body["group_mask"] = LLSD::Integer(U32_MAX);
					body["everyone_mask"] = LLSD::Integer(U32_MAX);
					body["expected_upload_cost"] = LLSD::Integer(LLGlobalEconomy::Singleton::getInstance()->getPriceUpload());
					LLHTTPClient::post(url, body, new LLImportInventoryResponder(body, data->assetid, data->type,data));
				}
			}
			else
				llinfos << "NewFileAgentInventory does not exist!!!!" << llendl;
		}
		else
			LLXmlImport::finish_init();
		
                
        }
private:
        LLImportAssetData* mData;
};

std::string terse_F32_string( F32 f )
{
	std::string r = llformat( "%.2f", f );
	// "1.20"  -> "1.2"
	// "24.00" -> "24."
	S32 len = r.length();
	while( len > 0 && '0' == r[len - 1] )
	{
		r.erase(len-1, 1);
		len--;
	}
	if( '.' == r[len - 1] )
	{
		// "24." -> "24"
		r.erase(len-1, 1);
	}
	else if( ('-' == r[0]) && ('0' == r[1]) )
	{
		// "-0.59" -> "-.59"
		r.erase(1, 1);
	}
	else if( '0' == r[0] )
	{
		// "0.59" -> ".59"
		r.erase(0, 1);
	}
	return r;
}

LLImportWearable::LLImportWearable(LLSD sd)
{
	mOrginalLLSD = sd;
	mName = sd["name"].asString();
	mType = sd["wearabletype"].asInteger();

	LLSD params = sd["params"];
	LLSD textures = sd["textures"];

	mData = "LLWearable version 22\n" + 
			mName + "\n\n" + 
			"\tpermissions 0\n" + 
			"\t{\n" + 
			"\t\tbase_mask\t7fffffff\n" + 
			"\t\towner_mask\t7fffffff\n" + 
			"\t\tgroup_mask\t00000000\n" + 
			"\t\teveryone_mask\t00000000\n" + 
			"\t\tnext_owner_mask\t00082000\n" + 
			"\t\tcreator_id\t00000000-0000-0000-0000-000000000000\n" + 
			"\t\towner_id\t" + gAgent.getID().asString() + "\n" + 
			"\t\tlast_owner_id\t" + gAgent.getID().asString() + "\n" + 
			"\t\tgroup_id\t00000000-0000-0000-0000-000000000000\n" + 
			"\t}\n" + 
			"\tsale_info\t0\n" + 
			"\t{\n" + 
			"\t\tsale_type\tnot\n" + 
			"\t\tsale_price\t10\n" + 
			"\t}\n" + 
			"type " + llformat("%d", mType) + "\n";

	mData += llformat("parameters %d\n", params.size());
	LLSD::map_iterator map_iter = params.beginMap();
	LLSD::map_iterator map_end = params.endMap();
	for( ; map_iter != map_end; ++map_iter)
	{
		mData += (*map_iter).first + " " + terse_F32_string((*map_iter).second.asReal()) + "\n";
	}

	mData += llformat("textures %d\n", textures.size());
	map_iter = textures.beginMap();
	map_end = textures.endMap();
	for( ; map_iter != map_end; ++map_iter)
	{
		mTextures.push_back((*map_iter).second);
		mData += (*map_iter).first + " " + (*map_iter).second.asString() + "\n";
	}
}
void LLImportWearable::replaceTextures(std::map<LLUUID,LLUUID> textures_replace)
{
	LLSD sd = mOrginalLLSD;
	mName = sd["name"].asString();
	mType = sd["wearabletype"].asInteger();

	LLSD params = sd["params"];
	LLSD textures = sd["textures"];

	mData = "LLWearable version 22\n" + 
			mName + "\n\n" + 
			"\tpermissions 0\n" + 
			"\t{\n" + 
			"\t\tbase_mask\t7fffffff\n" + 
			"\t\towner_mask\t7fffffff\n" + 
			"\t\tgroup_mask\t00000000\n" + 
			"\t\teveryone_mask\t00000000\n" + 
			"\t\tnext_owner_mask\t00082000\n" + 
			"\t\tcreator_id\t00000000-0000-0000-0000-000000000000\n" + 
			"\t\towner_id\t" + gAgent.getID().asString() + "\n" + 
			"\t\tlast_owner_id\t" + gAgent.getID().asString() + "\n" + 
			"\t\tgroup_id\t00000000-0000-0000-0000-000000000000\n" + 
			"\t}\n" + 
			"\tsale_info\t0\n" + 
			"\t{\n" + 
			"\t\tsale_type\tnot\n" + 
			"\t\tsale_price\t10\n" + 
			"\t}\n" + 
			"type " + llformat("%d", mType) + "\n";

	mData += llformat("parameters %d\n", params.size());
	LLSD::map_iterator map_iter = params.beginMap();
	LLSD::map_iterator map_end = params.endMap();
	for( ; map_iter != map_end; ++map_iter)
	{
		mData += (*map_iter).first + " " + terse_F32_string((*map_iter).second.asReal()) + "\n";
	}

	mData += llformat("textures %d\n", textures.size());
	map_iter = textures.beginMap();
	map_end = textures.endMap();
	for( ; map_iter != map_end; ++map_iter)
	{
		LLUUID asset_id = (*map_iter).second.asUUID();
		std::map<LLUUID,LLUUID>::iterator iter = textures_replace.find(asset_id);
		if(iter != textures_replace.end()) asset_id = (*iter).second;
		mData += (*map_iter).first + " " + asset_id.asString() + "\n";
	}
}
//LLImportObject::LLImportObject(std::string id, std::string parentId)
//	:	LLViewerObject(LLUUID::null, 9, NULL, TRUE),
//		mId(id),
//		mParentId(parentId),
//		mPrimName("Object")
//{
//	importIsAttachment = false;
//}
LLImportObject::LLImportObject(std::string id, LLSD prim)
	:	LLViewerObject(LLUUID::null, 9, NULL, TRUE)
{
	importIsAttachment = false;
	mId = id;
	mParentId = "";
	mPrimName = "Object";
	if(prim.has("parent"))
	{
		mParentId = prim["parent"].asString();
	}
	// Stuff for attach
	if(prim.has("attach"))
	{
		importIsAttachment = true;
		importAttachPoint = (U8)prim["attach"].asInteger();
		importAttachPos = ll_vector3_from_sd(prim["position"]);
		importAttachRot = ll_quaternion_from_sd(prim["rotation"]);
	}
	// Transforms
	setPosition(ll_vector3_from_sd(prim["position"]), FALSE);
	setScale(ll_vector3_from_sd(prim["scale"]), FALSE);
	setRotation(ll_quaternion_from_sd(prim["rotation"]), FALSE);
	// Flags
	setFlags(FLAGS_CAST_SHADOWS, prim["shadows"].asInteger());
	setFlags(FLAGS_PHANTOM, prim["phantom"].asInteger());
	setFlags(FLAGS_USE_PHYSICS, prim["physical"].asInteger());
	// Volume params
	LLVolumeParams volume_params;
	volume_params.fromLLSD(prim["volume"]);
	
	setVolume(volume_params, 0, false);
	// Extra params
	if(prim.has("flexible"))
	{
		LLFlexibleObjectData* wat = new LLFlexibleObjectData();
		wat->fromLLSD(prim["flex"]);
		LLFlexibleObjectData flex = *wat;
		setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, flex, true);
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, TRUE, true);
	}
	if(prim.has("light"))
	{
		LLLightParams* wat = new LLLightParams();
		wat->fromLLSD(prim["light"]);
		LLLightParams light = *wat;
		setParameterEntry(LLNetworkData::PARAMS_LIGHT, light, true);
		setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, TRUE, true);
	}
	if(prim.has("sculpt"))
	{
		LLSculptParams *wat = new LLSculptParams();
		wat->fromLLSD(prim["sculpt"]);
		LLSculptParams sculpt = *wat;
		setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt, true);
		setParameterEntryInUse(LLNetworkData::PARAMS_SCULPT, TRUE, true);
	}
	// Textures
	LLSD textures = prim["textures"];
	LLSD::array_iterator array_iter = textures.beginArray();
	LLSD::array_iterator array_end = textures.endArray();
	int i = 0;
	for( ; array_iter != array_end; ++array_iter)
	{
		LLTextureEntry* wat = new LLTextureEntry();
		wat->fromLLSD(*array_iter);
		LLTextureEntry te = *wat;
		delete wat; //clean up yo memory
		mTextures.push_back(te.getID());
		setTE(i, te);
		i++;
	}
	mTextures.unique();
	if(prim.has("name"))
	{
		mPrimName = prim["name"].asString();
	}
	if(prim.has("description"))
	{
		mPrimDescription = prim["description"].asString();
	}
}
		
void LLXmlImport::rez_supply()
{
	if(sImportInProgress && sXmlImportOptions && (sPrimsNeeded > 0))
	{
		sPrimsNeeded--;
		//group
		LLUUID group_id = gAgent.getGroupID();
		LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (gSavedSettings.getBOOL("AscentAlwaysRezInGroup"))
		{
			if (gAgent.isInGroup(parcel->getGroupID()))
			{
				group_id = parcel->getGroupID();
			}
			else if (gAgent.isInGroup(parcel->getOwnerID()))
			{
				group_id = parcel->getOwnerID();
			}
		}
		else if (gAgent.hasPowerInGroup(parcel->getGroupID(), GP_LAND_ALLOW_CREATE) && !parcel->getIsGroupOwned())
		{
			group_id = parcel->getGroupID();
		}
		// Need moar prims
		if(sXmlImportOptions->mSupplier == NULL)
		{
			gMessageSystem->newMessageFast(_PREHASH_ObjectAdd);
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			gMessageSystem->addUUIDFast(_PREHASH_GroupID, group_id);
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU8Fast(_PREHASH_PCode, 9);
			gMessageSystem->addU8Fast(_PREHASH_Material, LL_MCODE_WOOD);
			gMessageSystem->addU32Fast(_PREHASH_AddFlags, FLAGS_CREATE_SELECTED);
			gMessageSystem->addU8Fast(_PREHASH_PathCurve, 16);
			gMessageSystem->addU8Fast(_PREHASH_ProfileCurve, 1);
			gMessageSystem->addU16Fast(_PREHASH_PathBegin, 0);
			gMessageSystem->addU16Fast(_PREHASH_PathEnd, 0);
			gMessageSystem->addU8Fast(_PREHASH_PathScaleX, 100);
			gMessageSystem->addU8Fast(_PREHASH_PathScaleY, 100);
			gMessageSystem->addU8Fast(_PREHASH_PathShearX, 0);
			gMessageSystem->addU8Fast(_PREHASH_PathShearY, 0);
			gMessageSystem->addS8Fast(_PREHASH_PathTwist, 0);
			gMessageSystem->addS8Fast(_PREHASH_PathTwistBegin, 0);
			gMessageSystem->addS8Fast(_PREHASH_PathRadiusOffset, 0);
			gMessageSystem->addS8Fast(_PREHASH_PathTaperX, 0);
			gMessageSystem->addS8Fast(_PREHASH_PathTaperY, 0);
			gMessageSystem->addU8Fast(_PREHASH_PathRevolutions, 0);
			gMessageSystem->addS8Fast(_PREHASH_PathSkew, 0);
			gMessageSystem->addU16Fast(_PREHASH_ProfileBegin, 0);
			gMessageSystem->addU16Fast(_PREHASH_ProfileEnd, 0);
			gMessageSystem->addU16Fast(_PREHASH_ProfileHollow, 0);
			gMessageSystem->addU8Fast(_PREHASH_BypassRaycast, 1);
			
			LLVector3 rezpos = gAgent.getPositionAgent() + LLVector3(0.0f, 0.0f, 2.0f);
			
			gMessageSystem->addVector3Fast(_PREHASH_RayStart, rezpos);
			gMessageSystem->addVector3Fast(_PREHASH_RayEnd, rezpos);
			gMessageSystem->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null);
			gMessageSystem->addU8Fast(_PREHASH_RayEndIsIntersection, 0);
			gMessageSystem->addVector3Fast(_PREHASH_Scale, sSupplyParams->getScale());
			gMessageSystem->addQuatFast(_PREHASH_Rotation, LLQuaternion::DEFAULT);
			gMessageSystem->addU8Fast(_PREHASH_State, 0);
			gMessageSystem->sendReliable(gAgent.getRegionHost());
		}
		else // have supplier
		{
			gMessageSystem->newMessageFast(_PREHASH_ObjectDuplicate);
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			gMessageSystem->addUUIDFast(_PREHASH_GroupID, group_id);
			gMessageSystem->nextBlockFast(_PREHASH_SharedData);
			
			LLVector3 rezpos = gAgent.getPositionAgent() + LLVector3(0.0f, 0.0f, 2.0f);
			rezpos -= sSupplyParams->getPositionRegion();
			
			gMessageSystem->addVector3Fast(_PREHASH_Offset, rezpos);
			gMessageSystem->addU32Fast(_PREHASH_DuplicateFlags, FLAGS_CREATE_SELECTED);
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, sXmlImportOptions->mSupplier->getLocalID());
			gMessageSystem->sendReliable(gAgent.getRegionHost());
		}
		LLFloaterImportProgress::update();
	}
}


// static
void LLXmlImport::import(LLXmlImportOptions* import_options)
{
	sXmlImportOptions = import_options;
	if(sXmlImportOptions->mSupplier == NULL)
	{
		LLViewerObject* cube = new LLViewerObject(LLUUID::null, 9, NULL, TRUE);
		cube->setScale(LLVector3(.31337f, .31337f, .31337f), FALSE);
		sSupplyParams = cube;
	}
	else sSupplyParams = sXmlImportOptions->mSupplier;

	if(!(sXmlImportOptions->mKeepPosition) && sXmlImportOptions->mRootObjects.size())
	{ // Reposition all roots so that the first root is somewhere near the avatar
		// Find the root closest to the ground
		int num_roots = (int)sXmlImportOptions->mRootObjects.size();
		int lowest_root = 0;
		F32 lowest_z(65536.f);
		for(int i = 0; i < num_roots; i++)
		{
			F32 z = sXmlImportOptions->mRootObjects[i]->getPosition().mV[2];
			if(z < lowest_z)
			{
				lowest_root = i;
				lowest_z = z;
			}
		}
		// Move all roots
		LLVector3 old_pos = sXmlImportOptions->mRootObjects[lowest_root]->getPosition();
		LLVector3 new_pos = gAgent.getPositionAgent() + (gAgent.getAtAxis() * 2.0f);
		LLVector3 difference = new_pos - old_pos;
		for(int i = 0; i < num_roots; i++)
		{
			sXmlImportOptions->mRootObjects[i]->setPosition(sXmlImportOptions->mRootObjects[i]->getPosition() + difference, FALSE);
		}
	}

	sState = IMPORT_INIT;
	// Make the actual importable list
	sPrims.clear();
	// Clear these attachment-related maps
	sPt2watch.clear();
	sId2attachpt.clear();
	sPt2attachpos.clear();
	sPt2attachrot.clear();
	sDescriptions.clear();
	// Go ahead and add roots first
	std::vector<LLImportObject*>::iterator root_iter = sXmlImportOptions->mRootObjects.begin();
	std::vector<LLImportObject*>::iterator root_end = sXmlImportOptions->mRootObjects.end();
	for( ; root_iter != root_end; ++root_iter)
	{
		sPrims.push_back(*root_iter);
		// Remember some attachment info
		if((*root_iter)->importIsAttachment)
		{
			sId2attachpt[(*root_iter)->mId] = (*root_iter)->importAttachPoint;
			sPt2watch[(*root_iter)->importAttachPoint] = true;
			sPt2attachpos[(*root_iter)->importAttachPoint] = (*root_iter)->importAttachPos;
			sPt2attachrot[(*root_iter)->importAttachPoint] = (*root_iter)->importAttachRot;
			sDescriptions[(*root_iter)->importAttachPoint] = (*root_iter)->mPrimDescription;
		}
	}
	// Then add children, nearest first
	std::vector<LLImportObject*> children(sXmlImportOptions->mChildObjects);
	for(root_iter = sXmlImportOptions->mRootObjects.begin() ; root_iter != root_end; ++root_iter)
	{
		while(children.size() > 0)
		{
			std::string rootid = (*root_iter)->mId;
			F32 lowest_mag = 65536.0f;
			std::vector<LLImportObject*>::iterator lowest_child_iter = children.begin();
			LLImportObject* lowest_child = (*lowest_child_iter);
			
			std::vector<LLImportObject*>::iterator child_end = children.end();
			for(std::vector<LLImportObject*>::iterator child_iter = children.begin() ; child_iter != child_end; ++child_iter)
			{
				if((*child_iter)->mParentId == rootid)
				{
					F32 mag = (*child_iter)->getPosition().magVec();
					if(mag < lowest_mag)
					{
						lowest_child_iter = child_iter;
						lowest_child = (*lowest_child_iter);
						lowest_mag = mag;
					}
				}
			}
			sPrims.push_back(lowest_child);
			children.erase(lowest_child_iter);
		}
	}
	
	sImportInProgress = true;
	sImportHasAttachments = (sId2attachpt.size() > 0);
	sPrimsNeeded = (int)sPrims.size();
	sTotalAssets = sXmlImportOptions->mAssets.size();
	sPrimIndex = 0;
	sUploadedAssets = 0;
	sId2localid.clear();
	sRootpositions.clear();
	sRootrotations.clear();
	sLinkSets.clear();

	LLFloaterImportProgress::show();
	LLFloaterImportProgress::update();

	// Create folder
	if((sXmlImportOptions->mWearables.size() > 0) || (sId2attachpt.size() > 0) || (sTotalAssets > 0))
	{
		sFolderID = gInventory.createNewCategory( gInventory.getRootFolderID(), LLFolderType::FT_NONE, sXmlImportOptions->mName);
	}
	if(sXmlImportOptions->mReplaceTexture && sTotalAssets > 0 && !sXmlImportOptions->mAssetDir.empty())
	{
		LLUUID folder_id = gInventory.createNewCategory( sFolderID, LLFolderType::FT_NONE, "Textures");
		//starting up the texture uploading
		LLImportAssetData* data = sXmlImportOptions->mAssets[0];
                data->folderid = folder_id;
		sTextureReplace[data->oldassetid] = data->assetid;
		std::string url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
		if(!url.empty())
		{
			LLPointer<LLImageJ2C> integrity_test = new LLImageJ2C;
			if( !integrity_test->loadAndValidate( data->filename ) )
			{
				llinfos << "Image: " << data->filename << " is corrupt." << llendl;
			}
			S32 file_size;
			LLAPRFile infile ;
			infile.open(data->filename, LL_APR_RB, LLAPRFile::short_lived, &file_size);
			if (infile.getFileHandle())
			{
				LLVFile file(gVFS, data->assetid, data->type, LLVFile::WRITE);
				file.setMaxSize(file_size);
				const S32 buf_size = 65536;
				U8 copy_buf[buf_size];
				while ((file_size = infile.read(copy_buf, buf_size)))
				{
					file.write(copy_buf, file_size);
				}

				LLSD body;
				body["folder_id"] = folder_id;
				body["asset_type"] = LLAssetType::lookup(data->type);
				body["inventory_type"] = LLInventoryType::lookup(data->inv_type);
				body["name"] = data->name;
				body["description"] = data->description;
				body["next_owner_mask"] = LLSD::Integer(U32_MAX);
				body["group_mask"] = LLSD::Integer(U32_MAX);
				body["everyone_mask"] = LLSD::Integer(U32_MAX);
				body["expected_upload_cost"] = LLSD::Integer(LLGlobalEconomy::Singleton::getInstance()->getPriceUpload());
				LLHTTPClient::post(url, body, new LLImportInventoryResponder(body, data->assetid, data->type,data));
			}
		}
		else
		{
			//maybe do legacy upload here?????
			llinfos << "NewFileAgentInventory does not exist!!!!" << llendl;
			LLXmlImport::finish_init();
		}
	}
	else
		LLXmlImport::finish_init();
}

void LLXmlImport::finish_init()
{
	// Go ahead and upload wearables
	int num_wearables = sXmlImportOptions->mWearables.size();
	for(int i = 0; i < num_wearables; i++)
	{
		sXmlImportOptions->mWearables[i]->replaceTextures(sTextureReplace); //hack for importing weable textures
		LLAssetType::EType at = LLAssetType::AT_CLOTHING;
		if(sXmlImportOptions->mWearables[i]->mType < 4) at = LLAssetType::AT_BODYPART;
		LLUUID tid;
		tid.generate();
		// Create asset
		gMessageSystem->newMessageFast(_PREHASH_AssetUploadRequest);
		gMessageSystem->nextBlockFast(_PREHASH_AssetBlock);
		gMessageSystem->addUUIDFast(_PREHASH_TransactionID, tid);
		gMessageSystem->addS8Fast(_PREHASH_Type, (S8)at);
		gMessageSystem->addBOOLFast(_PREHASH_Tempfile, FALSE);
		gMessageSystem->addBOOLFast(_PREHASH_StoreLocal, FALSE);
		gMessageSystem->addStringFast(_PREHASH_AssetData, sXmlImportOptions->mWearables[i]->mData.c_str());
		gMessageSystem->sendReliable(gAgent.getRegionHost());
		// Create item
		gMessageSystem->newMessageFast(_PREHASH_CreateInventoryItem);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_InventoryBlock);
		gMessageSystem->addU32Fast(_PREHASH_CallbackID, 0);
		gMessageSystem->addUUIDFast(_PREHASH_FolderID, sFolderID);
		gMessageSystem->addUUIDFast(_PREHASH_TransactionID, tid);
		gMessageSystem->addU32Fast(_PREHASH_NextOwnerMask, 532480);
		gMessageSystem->addS8Fast(_PREHASH_Type, at);
		gMessageSystem->addS8Fast(_PREHASH_InvType, 18);
		gMessageSystem->addS8Fast(_PREHASH_WearableType, sXmlImportOptions->mWearables[i]->mType);
		gMessageSystem->addStringFast(_PREHASH_Name, sXmlImportOptions->mWearables[i]->mName.c_str());
		gMessageSystem->addStringFast(_PREHASH_Description, "");
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
	// Go ahead and upload asset data
	rez_supply();
}
// static
void LLXmlImport::onNewPrim(LLViewerObject* object)
{
	if(sState != IMPORT_INIT)
	{
		llwarns << "called onNewPrim without getting back to IMPORT_INIT" << llendl;
		return;
	}
	
	if(sPrimIndex >= (int)sPrims.size())
	{
		if(sAttachmentsDone >= (int)sPt2attachpos.size())
		{
			// "stop calling me"
			sImportInProgress = false;
			return;
		}
	}
	
	sExpectedUpdate = object->getID();
	LLSelectMgr::getInstance()->selectObjectAndFamily(object);

	LLImportObject* from = sPrims[sPrimIndex];
	
	// Flags
	// trying this first in case it helps when supply is physical...
	U32 flags = from->mFlags;
	flags = flags & (~FLAGS_USE_PHYSICS);
	object->setFlags(flags, TRUE);
	object->setFlags(~flags, FALSE); // Can I improve this lol?
	if(from->mParentId == "")
	{
		// this will be a root
		sId2localid[from->mId] = object->getLocalID();
		sRootpositions[object->getLocalID()] = from->getPosition();
		sRootrotations[object->getLocalID()] = from->getRotation();
		sLinkSets[object->getLocalID()].push_back(object);
	}
	else
	{
		//make positions and rotations offset from the root prim.
		U32 parentlocalid = sId2localid[from->mParentId];
		from->setPosition((from->getPosition() * sRootrotations[parentlocalid]) + sRootpositions[parentlocalid]);
		from->setRotation(from->getRotation() * sRootrotations[parentlocalid]);
		sLinkSets[parentlocalid].push_back(object); //this is here so we dont get 1 prim objects into the linkset queue
		
	}
	// Transforms
	object->setScale(from->getScale(), FALSE);
	object->setRotation(from->getRotation(), FALSE);
	object->setPosition(from->getPosition(), FALSE);
	
	//using this because sendMultipleUpdate breaks rotations?
	object->sendRotationUpdate();
	LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_SCALE | UPD_POSITION);

	LLSelectMgr::getInstance()->deselectAll();
}
void LLXmlImport::onUpdatePrim(LLViewerObject* object)
{
	LLImportObject* from = sPrims[sPrimIndex];
	LLSelectMgr::getInstance()->selectObjectAndFamily(object);
	sState = (eImportObjectState) ((int)sState + 1);
	// Huge state machine
	switch(sState)
	{
	case IMPORT_VOLUME:
		{
			// Volume params
			LLVolumeParams params = from->getVolume()->getParams();
			object->setVolume(params, 0, false);
			object->sendShapeUpdate();
			break;
		}
	case IMPORT_EXTRA:
		{
			// Extra params
			if(from->isFlexible())
			{
				LLFlexibleObjectData flex = *((LLFlexibleObjectData*)from->getParameterEntry(LLNetworkData::PARAMS_FLEXIBLE));
				object->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, flex, true);
				object->setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, TRUE, true);
				object->parameterChanged(LLNetworkData::PARAMS_FLEXIBLE, true);
			}
			else
			{
				// send param not in use in case the supply prim has it
				object->setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, FALSE, true);
				object->parameterChanged(LLNetworkData::PARAMS_FLEXIBLE, true);
			}
			if (from->getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT))
			{
				LLLightParams light = *((LLLightParams*)from->getParameterEntry(LLNetworkData::PARAMS_LIGHT));
				object->setParameterEntry(LLNetworkData::PARAMS_LIGHT, light, true);
				object->setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, TRUE, true);
				object->parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
			}
			else
			{
				// send param not in use in case the supply prim has it
				object->setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, FALSE, true);
				object->parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
			}
			if (from->getParameterEntryInUse(LLNetworkData::PARAMS_SCULPT))
			{
				LLSculptParams sculpt = *((LLSculptParams*)from->getParameterEntry(LLNetworkData::PARAMS_SCULPT));
				if(sXmlImportOptions->mReplaceTexture && sTextureReplace.find(sculpt.getSculptTexture()) != sTextureReplace.end())
					sculpt.setSculptTexture(sTextureReplace[sculpt.getSculptTexture()]);
				object->setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt, true);
				object->setParameterEntryInUse(LLNetworkData::PARAMS_SCULPT, TRUE, true);
				object->parameterChanged(LLNetworkData::PARAMS_SCULPT, true);
			}
			else
			{
				// send param not in use in case the supply prim has it
				object->setParameterEntryInUse(LLNetworkData::PARAMS_SCULPT, FALSE, true);
				object->parameterChanged(LLNetworkData::PARAMS_SCULPT, true);
			}
			break;
		}
	case IMPORT_TEXTURE:
		{
			// Textures
			U8 te_count = from->getNumTEs();
			for (U8 i = 0; i < te_count; i++)
			{
				const LLTextureEntry* wat = from->getTE(i);
				LLTextureEntry te = *wat;
				if(sXmlImportOptions->mReplaceTexture && sTextureReplace.find(te.getID()) != sTextureReplace.end())
					te.setID(sTextureReplace[te.getID()]);
				object->setTE(i, te);
			}
			object->sendTEUpdate();	
			break;
		}
	case IMPORT_FINISH:
		{
			// Name
			std::string name = from->mPrimName;
			if(name.empty())
				name = "Object";
			LLSelectMgr::getInstance()->selectionSetObjectName(from->mPrimName);
			
			//Description
			if(from->importIsAttachment) //special description tracker
			{
				LLSelectMgr::getInstance()->selectionSetObjectDescription(from->mId);
			}
			else
			{	
				std::string desc = from->mPrimDescription;
				if(desc.empty())
					desc = "(No Description)";
				LLSelectMgr::getInstance()->selectionSetObjectDescription(desc);
			}
			sExpectedUpdate = LLUUID::null;
			sPrimIndex++;
			/////// finished block /////////
			if(sPrimIndex >= (int)sPrims.size())
			{
				// Link time
				for(std::map<U32, std::vector<LLViewerObject*> >::iterator itr = sLinkSets.begin();itr != sLinkSets.end();++itr)
				{
					std::vector<LLViewerObject*> linkset = (*itr).second;
					LLSelectMgr::getInstance()->deselectAll();
					LLSelectMgr::getInstance()->selectObjectAndFamily(linkset, true);
					LLSelectMgr::getInstance()->sendLink(); 
				}

				if(sId2attachpt.size() == 0)
				{
					sImportInProgress = false;
					std::string msg = "Imported " + sXmlImportOptions->mName;
					LLChat chat(msg);
					LLFloaterChat::addChat(chat);
					LLFloaterImportProgress::update();
				}
				else
				{
					// Take attachables into inventory
					std::string msg = "Wait a few moments for the attachments to link and attach...";
					LLChat chat(msg);
					LLFloaterChat::addChat(chat);
					sAttachmentsDone = 0;
					if(sLinkSets.size() > 0)
					{
						U32 ip = gAgent.getRegionHost().getAddress();
						U32 port = gAgent.getRegionHost().getPort();
						std::vector<LLUUID> roots;
						roots.resize(sLinkSets.size());
						for(std::map<U32, std::vector<LLViewerObject*> >::iterator itr = sLinkSets.begin();itr != sLinkSets.end();++itr)
						{
							LLUUID id = LLUUID::null;
							LLViewerObjectList::getUUIDFromLocal(id,itr->first,ip,port);
							if(id.notNull())
							{
								roots.push_back(id);
							}
						}
						new LLLinkTimer(roots);
					}
					else
					{
						finish_link();
					}
				}
			}
			else
			{
				LLFloaterImportProgress::update();
				rez_supply();
				sState = IMPORT_INIT;
			}
			break;
		}
	default:
		{
			llwarns << "got default state in state machine, resetting to IMPORT_INIT" << llendl;
			sState = IMPORT_INIT;
		}
	}
	LLSelectMgr::getInstance()->deselectAll();
}
void LLXmlImport::finish_link()
{
	std::map<std::string, U8>::iterator at_iter = sId2attachpt.begin();
	std::map<std::string, U8>::iterator at_end = sId2attachpt.end();
	for( ; at_iter != at_end; ++at_iter)
	{
		LLUUID tid;
		tid.generate();
		U32 at_localid = sId2localid[(*at_iter).first];
		gMessageSystem->newMessageFast(_PREHASH_DeRezObject);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_AgentBlock);
		gMessageSystem->addUUIDFast(_PREHASH_GroupID, LLUUID::null);
		gMessageSystem->addU8Fast(_PREHASH_Destination, DRD_TAKE_INTO_AGENT_INVENTORY);
		gMessageSystem->addUUIDFast(_PREHASH_DestinationID, sFolderID);
		gMessageSystem->addUUIDFast(_PREHASH_TransactionID, tid);
		gMessageSystem->addU8Fast(_PREHASH_PacketCount, 1);
		gMessageSystem->addU8Fast(_PREHASH_PacketNumber, 0);
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, at_localid);
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
}
// static
void LLXmlImport::onNewItem(LLViewerInventoryItem* item)
{
	U8 attachpt = sId2attachpt[item->getDescription()];
	if(attachpt)
	{
		// clear description, part 1
		item->setDescription(sDescriptions[attachpt]);
		item->updateServer(FALSE);

		// Attach it
		gMessageSystem->newMessageFast(_PREHASH_RezSingleAttachmentFromInv);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addUUIDFast(_PREHASH_ItemID, item->getUUID());
		gMessageSystem->addUUIDFast(_PREHASH_OwnerID, gAgent.getID());
		gMessageSystem->addU8Fast(_PREHASH_AttachmentPt, attachpt);
		gMessageSystem->addU32Fast(_PREHASH_ItemFlags, 0);
		gMessageSystem->addU32Fast(_PREHASH_GroupMask, 0);
		gMessageSystem->addU32Fast(_PREHASH_EveryoneMask, 0);
		gMessageSystem->addU32Fast(_PREHASH_NextOwnerMask, 0);
		gMessageSystem->addStringFast(_PREHASH_Name, item->getName());
		gMessageSystem->addStringFast(_PREHASH_Description, "");
		gMessageSystem->sendReliable(gAgent.getRegionHost());
	}
}

// static
void LLXmlImport::onNewAttachment(LLViewerObject* object)
{
	if(sPt2attachpos.size() == 0) return;

	U8 attachpt = (U8)object->getAttachmentPoint();
	if(sPt2watch[attachpt])
	{
		LLSelectMgr::getInstance()->selectObjectAndFamily(object);

		// clear description, part 2
		std::string desc = sDescriptions[attachpt];
		if(desc.empty())
			desc = "(No Description)";
		LLSelectMgr::getInstance()->selectionSetObjectDescription(desc);

		// position and rotation
		object->setRotation(sPt2attachrot[attachpt], FALSE);
		object->setPosition(sPt2attachpos[attachpt], FALSE);
		LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_POSITION | UPD_ROTATION);
		
		LLSelectMgr::getInstance()->deselectAll();
		// Done?
		sAttachmentsDone++;
		if(sAttachmentsDone >= (int)sPt2attachpos.size())
		{
			sImportInProgress = false;
			std::string msg = "Imported " + sXmlImportOptions->mName;
			LLChat chat(msg);
			LLFloaterChat::addChat(chat);
			LLFloaterImportProgress::update();
			return;
		}
	}
}



// static
void LLXmlImport::Cancel(void* user_data)
{
	sImportInProgress = false;
	LLFloaterImportProgress::sInstance->close(false);
}

// </edit>

