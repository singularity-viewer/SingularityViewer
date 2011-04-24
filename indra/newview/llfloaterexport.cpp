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
#include "llfilepicker.h"
#include "llagent.h"
#include "llvoavatar.h"
#include "llvoavatardefines.h"
#include "llimportobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llwindow.h"
#include "llviewerimagelist.h"
#include "lltexturecache.h"
#include "llimage.h"
#include "llappviewer.h"
#include "llsdutil_math.h"
#include "llimagej2c.h"
std::vector<LLFloaterExport*> LLFloaterExport::instances;

class CacheReadResponder : public LLTextureCache::ReadResponder
{
public:
CacheReadResponder(const LLUUID& id, const std::string& filename)
: mID(id)
{
	mFormattedImage = new LLImageJ2C;
	setImage(mFormattedImage);
	mFilename = filename;
}
void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat, BOOL imagelocal)
{
	if(imageformat==IMG_CODEC_TGA && mFormattedImage->getCodec()==IMG_CODEC_J2C)
	{
		llwarns<<"Bleh its a tga not saving"<<llendl;
		mFormattedImage=NULL;
		mImageSize=0;
		return;
	}

	if (mFormattedImage.notNull())
	{
		llassert_always(mFormattedImage->getCodec() == imageformat);
		mFormattedImage->appendData(data, datasize);
	}
	else
	{
		mFormattedImage = LLImageFormatted::createFromType(imageformat);
		mFormattedImage->setData(data,datasize);
	}
	mImageSize = imagesize;
	mImageLocal = imagelocal;
}

virtual void completed(bool success)
{
	if(success && (mFormattedImage.notNull()) && mImageSize>0)
	{

		llinfos << "SUCCESS getting texture "<<mID<< llendl;

		llinfos << "Saving to "<< mFilename<<llendl;

		if(!mFormattedImage->save(mFilename))
		{
			llinfos << "FAIL saving texture "<<mID<< llendl;
		}

	}
	else
	{
		if(!success)
			llwarns << "FAIL NOT SUCCESSFUL getting texture "<<mID<< llendl;
		if(mFormattedImage.isNull())
			llwarns << "FAIL image is NULL "<<mID<< llendl;
	}
}
private:
	LLPointer<LLImageFormatted> mFormattedImage;
	LLUUID mID;
	std::string mFilename;
};


LLExportable::LLExportable(LLViewerObject* object, std::string name, std::map<U32,std::pair<std::string, std::string> >& primNameMap)
:	mObject(object),
	mType(EXPORTABLE_OBJECT),
	mPrimNameMap(&primNameMap)
{
}

LLExportable::LLExportable(LLVOAvatar* avatar, EWearableType type, std::map<U32,std::pair<std::string, std::string> >& primNameMap)
:	mAvatar(avatar),
	mType(EXPORTABLE_WEARABLE),
	mWearableType(type),
	mPrimNameMap(&primNameMap)
{
}

LLSD LLExportable::asLLSD()
{
	if(mType == EXPORTABLE_OBJECT)
	{
		std::list<LLViewerObject*> prims;

		prims.push_back(mObject);

		LLViewerObject::child_list_t child_list = mObject->getChildren();
		for (LLViewerObject::child_list_t::iterator i = child_list.begin(); i != child_list.end(); ++i)
		{
			LLViewerObject* child = *i;
			if(child->getPCode() < LL_PCODE_APP)
			{
				prims.push_back(child);
			}
		}

		LLSD llsd;

		std::list<LLViewerObject*>::iterator prim_iter = prims.begin();
		std::list<LLViewerObject*>::iterator prims_end = prims.end();
		for( ; prim_iter != prims_end; ++prim_iter)
		{
			LLViewerObject* object = (*prim_iter);

			LLSD prim_llsd;

			prim_llsd["type"] = "prim";

			if (!object->isRoot())
			{
				if(!object->getSubParent()->isAvatar())
				{
					// Parent id
					prim_llsd["parent"] = llformat("%d", object->getSubParent()->getLocalID());
				}
			}
			if(object->getSubParent())
			{
				if(object->getSubParent()->isAvatar())
				{
					// attachment-specific
					U8 state = object->getState();
					S32 attachpt = ((S32)((((U8)state & AGENT_ATTACH_MASK) >> 4) | (((U8)state & ~AGENT_ATTACH_MASK) << 4)));
					prim_llsd["attach"] = attachpt;
				}
			}

			// Transforms
			prim_llsd["position"] = object->getPosition().getValue();
			prim_llsd["scale"] = object->getScale().getValue();
			prim_llsd["rotation"] = ll_sd_from_quaternion(object->getRotation());

			// Flags
			prim_llsd["shadows"] = object->flagCastShadows();
			prim_llsd["phantom"] = object->flagPhantom();
			prim_llsd["physical"] = (BOOL)(object->mFlags & FLAGS_USE_PHYSICS);

			// Volume params
			LLVolumeParams params = object->getVolume()->getParams();
			prim_llsd["volume"] = params.asLLSD();

			// Extra params
			if (object->isFlexible())
			{
				// Flexible
				LLFlexibleObjectData* flex = (LLFlexibleObjectData*)object->getParameterEntry(LLNetworkData::PARAMS_FLEXIBLE);
				prim_llsd["flexible"] = flex->asLLSD();
			}
			if (object->getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT))
			{
				// Light
				LLLightParams* light = (LLLightParams*)object->getParameterEntry(LLNetworkData::PARAMS_LIGHT);
				prim_llsd["light"] = light->asLLSD();
			}
			if (object->getParameterEntryInUse(LLNetworkData::PARAMS_SCULPT))
			{
				// Sculpt
				LLSculptParams* sculpt = (LLSculptParams*)object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
				prim_llsd["sculpt"] = sculpt->asLLSD();
			}

			// Textures
			LLSD te_llsd;
			U8 te_count = object->getNumTEs();
			for (U8 i = 0; i < te_count; i++)
			{
				te_llsd.append(object->getTE(i)->asLLSD());
			}
			prim_llsd["textures"] = te_llsd;

			std::map<U32,std::pair<std::string, std::string> >::iterator pos = (*mPrimNameMap).find(object->getLocalID());
			if(pos != (*mPrimNameMap).end())
			{
				prim_llsd["name"] = (*mPrimNameMap)[object->getLocalID()].first;
				prim_llsd["description"] = (*mPrimNameMap)[object->getLocalID()].second;
			}

			llsd[llformat("%d", object->getLocalID())] = prim_llsd;
		}
		
		return llsd;
	}
	else if(mType == EXPORTABLE_WEARABLE)
	{
		LLSD llsd; // pointless map with single key/value

		LLSD item_sd; // map for wearable

		item_sd["type"] = "wearable";

		S32 type_s32 = (S32)mWearableType;
		std::string wearable_name = LLWearable::typeToTypeName( mWearableType );

		item_sd["name"] = mAvatar->getFullname() + " " + wearable_name;
		item_sd["wearabletype"] = type_s32;

		LLSD param_map;

		for( LLVisualParam* param = mAvatar->getFirstVisualParam(); param; param = mAvatar->getNextVisualParam() )
		{
			LLViewerVisualParam* viewer_param = (LLViewerVisualParam*)param;
			if( (viewer_param->getWearableType() == type_s32) && 
				(viewer_param->getGroup() == VISUAL_PARAM_GROUP_TWEAKABLE) )
			{
				param_map[llformat("%d", viewer_param->getID())] = viewer_param->getWeight();
			}
		}

		item_sd["params"] = param_map;

		LLSD textures_map;

		for( S32 te = 0; te < LLVOAvatarDefines::TEX_NUM_INDICES; te++ )
		{
			if( LLVOAvatar::getTEWearableType( (LLVOAvatarDefines::ETextureIndex)te ) == mWearableType )
			{
				LLViewerImage* te_image = mAvatar->getTEImage( te );
				if( te_image )
				{
					textures_map[llformat("%d", te)] = te_image->getID();
				}
			}
		}

		item_sd["textures"] = textures_map;

		// Generate a unique ID for it...
		LLUUID myid;
		myid.generate();

		llsd[myid.asString()] = item_sd;

		return llsd;
	}
	return LLSD();
}



LLFloaterExport::LLFloaterExport()
:	LLFloater()
{
	mSelection = LLSelectMgr::getInstance()->getSelection();
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_export.xml");
	LLFloaterExport::instances.push_back(this);
}


LLFloaterExport::~LLFloaterExport()
{
	std::vector<LLFloaterExport*>::iterator pos = std::find(LLFloaterExport::instances.begin(), LLFloaterExport::instances.end(), this);
	if(pos != LLFloaterExport::instances.end())
	{
		LLFloaterExport::instances.erase(pos);
	}
}

BOOL LLFloaterExport::postBuild(void)
{
	if(!mSelection) return TRUE;
	if(mSelection->getRootObjectCount() < 1) return TRUE;

	// New stuff: Populate prim name map

	for (LLObjectSelection::valid_iterator iter = mSelection->valid_begin();
		 iter != mSelection->valid_end(); iter++)
	{
		LLSelectNode* nodep = *iter;
		LLViewerObject* objectp = nodep->getObject();
		U32 localid = objectp->getLocalID();
		std::string name = nodep->mName;
		mPrimNameMap[localid] = std::pair<std::string, std::string>(name, nodep->mDescription);
	}

	// Older stuff

	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("export_list");

	std::map<LLViewerObject*, bool> avatars;

	for (LLObjectSelection::valid_root_iterator iter = mSelection->valid_root_begin();
		 iter != mSelection->valid_root_end(); iter++)
	{
		LLSelectNode* nodep = *iter;
		LLViewerObject* objectp = nodep->getObject();
		std::string objectp_id = llformat("%d", objectp->getLocalID());

		if(list->getItemIndex(objectp->getID()) == -1)
		{
			bool is_attachment = false;
			bool is_root = true;
			LLViewerObject* parentp = objectp->getSubParent();
			if(parentp)
			{
				if(!parentp->isAvatar())
				{
					// parent is a prim I guess
					is_root = false;
				}
				else
				{
					// parent is an avatar
					is_attachment = true;
					if(!avatars[parentp]) avatars[parentp] = true;
				}
			}

			bool is_prim = true;
			if(objectp->getPCode() >= LL_PCODE_APP)
			{
				is_prim = false;
			}

			bool is_avatar = objectp->isAvatar();

			
			if(is_root && is_prim)
			{
				LLSD element;
				element["id"] = objectp->getID();

				LLSD& check_column = element["columns"][LIST_CHECKED];
				check_column["column"] = "checked";
				check_column["type"] = "checkbox";
				check_column["value"] = true;

				LLSD& type_column = element["columns"][LIST_TYPE];
				type_column["column"] = "type";
				type_column["type"] = "icon";
				type_column["value"] = "inv_item_object.tga";

				LLSD& name_column = element["columns"][LIST_NAME];
				name_column["column"] = "name";
				if(is_attachment)
					name_column["value"] = nodep->mName + " (worn on " + utf8str_tolower(objectp->getAttachmentPointName()) + ")";
				else
					name_column["value"] = nodep->mName;

				LLSD& avatarid_column = element["columns"][LIST_AVATARID];
				avatarid_column["column"] = "avatarid";
				if(is_attachment)
					avatarid_column["value"] = parentp->getID();
				else
					avatarid_column["value"] = LLUUID::null;

				LLExportable* exportable = new LLExportable(objectp, nodep->mName, mPrimNameMap);
				mExportables[objectp->getID()] = exportable->asLLSD();

				list->addElement(element, ADD_BOTTOM);

				addToPrimList(objectp);
			}
			else if(is_avatar)
			{
				if(!avatars[objectp])
				{
					avatars[objectp] = true;
				}
			}
		}
	}
	std::map<LLViewerObject*, bool>::iterator avatar_iter = avatars.begin();
	std::map<LLViewerObject*, bool>::iterator avatars_end = avatars.end();
	for( ; avatar_iter != avatars_end; avatar_iter++)
	{
		LLViewerObject* avatar = (*avatar_iter).first;
		addAvatarStuff((LLVOAvatar*)avatar);
	}

	updateNamesProgress();

	childSetAction("select_all_btn", onClickSelectAll, this);
	childSetAction("select_objects_btn", onClickSelectObjects, this);
	childSetAction("select_wearables_btn", onClickSelectWearables, this);

	childSetAction("save_as_btn", onClickSaveAs, this);
	childSetAction("make_copy_btn", onClickMakeCopy, this);

	return TRUE;
}

void LLFloaterExport::addAvatarStuff(LLVOAvatar* avatarp)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("export_list");
	for( S32 type = WT_SHAPE; type < WT_COUNT; type++ )
	{
		// guess whether this wearable actually exists
		// by checking whether it has any textures that aren't default
		bool exists = false;
		if(type == WT_SHAPE)
		{
			exists = true;
		}
		else if (type == WT_ALPHA || type == WT_TATTOO) //alpha layers and tattos are unsupported for now
		{
			continue;
		}
		else
		{
			for( S32 te = 0; te < LLVOAvatarDefines::TEX_NUM_INDICES; te++ )
			{
				if( (S32)LLVOAvatar::getTEWearableType( (LLVOAvatarDefines::ETextureIndex)te ) == type )
				{
					LLViewerImage* te_image = avatarp->getTEImage( te );
					if( te_image->getID() != IMG_DEFAULT_AVATAR)
					{
						exists = true;
						break;
					}
				}
			}
		}

		if(exists)
		{
			std::string wearable_name = LLWearable::typeToTypeName( (EWearableType)type );
			std::string name = avatarp->getFullname() + " " + wearable_name;
			LLUUID myid;
			myid.generate();

			LLSD element;
			element["id"] = myid;

			LLSD& check_column = element["columns"][LIST_CHECKED];
			check_column["column"] = "checked";
			check_column["type"] = "checkbox";
			check_column["value"] = false;

			LLSD& type_column = element["columns"][LIST_TYPE];
			type_column["column"] = "type";
			type_column["type"] = "icon";
			type_column["value"] = "inv_item_" + wearable_name + ".tga";

			LLSD& name_column = element["columns"][LIST_NAME];
			name_column["column"] = "name";
			name_column["value"] = name;

			LLSD& avatarid_column = element["columns"][LIST_AVATARID];
			avatarid_column["column"] = "avatarid";
			avatarid_column["value"] = avatarp->getID();

			LLExportable* exportable = new LLExportable(avatarp, (EWearableType)type, mPrimNameMap);
			mExportables[myid] = exportable->asLLSD();

			list->addElement(element, ADD_BOTTOM);
		}
	}

	// Add attachments
	LLViewerObject::child_list_t child_list = avatarp->getChildren();
	for (LLViewerObject::child_list_t::iterator i = child_list.begin(); i != child_list.end(); ++i)
	{
		LLViewerObject* childp = *i;
		if(list->getItemIndex(childp->getID()) == -1)
		{
			LLSD element;
			element["id"] = childp->getID();

			LLSD& check_column = element["columns"][LIST_CHECKED];
			check_column["column"] = "checked";
			check_column["type"] = "checkbox";
			check_column["value"] = false;

			LLSD& type_column = element["columns"][LIST_TYPE];
			type_column["column"] = "type";
			type_column["type"] = "icon";
			type_column["value"] = "inv_item_object.tga";

			LLSD& name_column = element["columns"][LIST_NAME];
			name_column["column"] = "name";
			name_column["value"] = "Object (worn on " + utf8str_tolower(childp->getAttachmentPointName()) + ")";

			LLSD& avatarid_column = element["columns"][LIST_AVATARID];
			avatarid_column["column"] = "avatarid";
			avatarid_column["value"] = avatarp->getID();

			LLExportable* exportable = new LLExportable(childp, "Object", mPrimNameMap);
			mExportables[childp->getID()] = exportable->asLLSD();

			list->addElement(element, ADD_BOTTOM);

			addToPrimList(childp);
			//LLSelectMgr::getInstance()->selectObjectAndFamily(childp, false);
			//LLSelectMgr::getInstance()->deselectObjectAndFamily(childp, true, true);

			LLViewerObject::child_list_t select_list = childp->getChildren();
			LLViewerObject::child_list_t::iterator select_iter;
			int block_counter;

			gMessageSystem->newMessageFast(_PREHASH_ObjectSelect);
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, childp->getLocalID());
			block_counter = 0;
			for (select_iter = select_list.begin(); select_iter != select_list.end(); ++select_iter)
			{
				block_counter++;
				if(block_counter >= 254)
				{
					// start a new message
					gMessageSystem->sendReliable(childp->getRegion()->getHost());
					gMessageSystem->newMessageFast(_PREHASH_ObjectSelect);
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
				}
				gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
				gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, (*select_iter)->getLocalID());
			}
			gMessageSystem->sendReliable(childp->getRegion()->getHost());

			gMessageSystem->newMessageFast(_PREHASH_ObjectDeselect);
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, childp->getLocalID());
			block_counter = 0;
			for (select_iter = select_list.begin(); select_iter != select_list.end(); ++select_iter)
			{
				block_counter++;
				if(block_counter >= 254)
				{
					// start a new message
					gMessageSystem->sendReliable(childp->getRegion()->getHost());
					gMessageSystem->newMessageFast(_PREHASH_ObjectDeselect);
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					gMessageSystem->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
				}
				gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
				gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, (*select_iter)->getLocalID());
			}
			gMessageSystem->sendReliable(childp->getRegion()->getHost());
		}
	}
}

//static
void LLFloaterExport::onClickSelectAll(void* user_data)
{
	LLFloaterExport* floater = (LLFloaterExport*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("export_list");
	std::vector<LLScrollListItem*> items = list->getAllData();
	std::vector<LLScrollListItem*>::iterator item_iter = items.begin();
	std::vector<LLScrollListItem*>::iterator items_end = items.end();
	bool new_value = !((*item_iter)->getColumn(LIST_CHECKED)->getValue());
	for( ; item_iter != items_end; ++item_iter)
	{
		LLScrollListItem* item = (*item_iter);
		item->getColumn(LIST_CHECKED)->setValue(new_value);
	}
}

//static
void LLFloaterExport::onClickSelectObjects(void* user_data)
{
	LLFloaterExport* floater = (LLFloaterExport*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("export_list");
	std::vector<LLScrollListItem*> items = list->getAllData();
	std::vector<LLScrollListItem*>::iterator item_iter = items.begin();
	std::vector<LLScrollListItem*>::iterator items_end = items.end();
	bool new_value = false;
	for( ; item_iter != items_end; ++item_iter)
	{
		if(((*item_iter)->getColumn(LIST_TYPE)->getValue()).asString() == "inv_item_object.tga")
		{
			new_value = !((*item_iter)->getColumn(LIST_CHECKED)->getValue());
			break;
		}
	}
	for(item_iter = items.begin(); item_iter != items_end; ++item_iter)
	{
		if(((*item_iter)->getColumn(LIST_TYPE)->getValue()).asString() == "inv_item_object.tga")
		{
			LLScrollListItem* item = (*item_iter);
			item->getColumn(LIST_CHECKED)->setValue(new_value);
		}
	}
}

//static
void LLFloaterExport::onClickSelectWearables(void* user_data)
{
	LLFloaterExport* floater = (LLFloaterExport*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("export_list");
	std::vector<LLScrollListItem*> items = list->getAllData();
	std::vector<LLScrollListItem*>::iterator item_iter = items.begin();
	std::vector<LLScrollListItem*>::iterator items_end = items.end();
	bool new_value = false;
	for( ; item_iter != items_end; ++item_iter)
	{
		if(((*item_iter)->getColumn(LIST_TYPE)->getValue()).asString() != "inv_item_object.tga")
		{
			new_value = !((*item_iter)->getColumn(LIST_CHECKED)->getValue());
			break;
		}
	}
	for(item_iter = items.begin(); item_iter != items_end; ++item_iter)
	{
		if(((*item_iter)->getColumn(LIST_TYPE)->getValue()).asString() != "inv_item_object.tga")
		{
			LLScrollListItem* item = (*item_iter);
			item->getColumn(LIST_CHECKED)->setValue(new_value);
		}
	}
}

LLSD LLFloaterExport::getLLSD()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("export_list");
	std::vector<LLScrollListItem*> items = list->getAllData();
	LLSD sd;
	std::vector<LLScrollListItem*>::iterator item_iter = items.begin();
	std::vector<LLScrollListItem*>::iterator items_end = items.end();
	for( ; item_iter != items_end; ++item_iter)
	{
		LLScrollListItem* item = (*item_iter);
		if(item->getColumn(LIST_CHECKED)->getValue())
		{
			LLSD item_sd = mExportables[item->getUUID()];
			LLSD::map_iterator map_iter = item_sd.beginMap();
			LLSD::map_iterator map_end = item_sd.endMap();
			for( ; map_iter != map_end; ++map_iter)
			{
				std::string key((*map_iter).first);
				LLSD data = (*map_iter).second;
				// copy it...
				sd[key] = data;
			}
		}
	}
	return sd;
}

//static
void LLFloaterExport::onClickSaveAs(void* user_data)
{
	LLFloaterExport* floater = (LLFloaterExport*)user_data;
	LLSD sd = floater->getLLSD();

	if(sd.size())
	{
		std::string default_filename = "untitled";

		// count the number of selected items
		LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("export_list");
		std::vector<LLScrollListItem*> items = list->getAllData();
		int item_count = 0;
		LLUUID avatarid = (*(items.begin()))->getColumn(LIST_AVATARID)->getValue().asUUID();
		bool all_same_avatarid = true;
		std::vector<LLScrollListItem*>::iterator item_iter = items.begin();
		std::vector<LLScrollListItem*>::iterator items_end = items.end();
		for( ; item_iter != items_end; ++item_iter)
		{
			LLScrollListItem* item = (*item_iter);
			if(item->getColumn(LIST_CHECKED)->getValue())
			{
				item_count++;
				if(item->getColumn(LIST_AVATARID)->getValue().asUUID() != avatarid)
				{
					all_same_avatarid = false;
				}
			}
		}

		if(item_count == 1)
		{
			// Exporting one item?  Use its name for the filename.
			// But make sure it's a root!
			LLSD target = (*(sd.beginMap())).second;
			if(target.has("parent"))
			{
				std::string parentid = target["parent"].asString();
				target = sd[parentid];
			}
			if(target.has("name"))
			{
				if(target["name"].asString().length() > 0)
				{
					default_filename = target["name"].asString();
				}
			}
		}
		else
		{
			// Multiple items?
			// If they're all part of the same avatar, use the avatar's name as filename.
			if(all_same_avatarid)
			{
				std::string fullname;
				if(gCacheName->getFullName(avatarid, fullname))
				{
					default_filename = fullname;
				}
			}
		}

		LLFilePicker& file_picker = LLFilePicker::instance();
		if(file_picker.getSaveFile( LLFilePicker::FFSAVE_XML, LLDir::getScrubbedFileName(default_filename + ".xml")))
		{
			std::string file_name = file_picker.getFirstFile();
			std::string path = file_name.substr(0,file_name.find_last_of(".")) + "_assets";
			BOOL download_texture = floater->childGetValue("download_textures");
			if(download_texture)
			{
				if(!LLFile::isdir(path))
				{
					LLFile::mkdir(path);
				}else
				{
					U32 response = OSMessageBox("Directory "+path+" already exists, would you like to continue and override files?", "Directory Already Exists", OSMB_YESNO);
					if(response)
					{
						return;
					}
				}
				path.append(gDirUtilp->getDirDelimiter()); //lets add the Delimiter now
			}
			// set correct names within llsd and download textures
			LLSD::map_iterator map_iter = sd.beginMap();
			LLSD::map_iterator map_end = sd.endMap();
			std::list<LLUUID> textures;

			for( ; map_iter != map_end; ++map_iter)
			{
				std::istringstream keystr((*map_iter).first);
				U32 key;
				keystr >> key;
				LLSD item = (*map_iter).second;
				if(item["type"].asString() == "prim")
				{
					std::string name = floater->mPrimNameMap[key].first;
					item["name"] = name;
					item["description"] = floater->mPrimNameMap[key].second;
					// I don't understand C++ :(
					sd[(*map_iter).first] = item;

					if(download_texture)
					{
						//textures
						LLSD::array_iterator tex_iter = item["textures"].beginArray();
						for( ; tex_iter != item["textures"].endArray(); ++tex_iter)
						{
							textures.push_back((*tex_iter)["imageid"].asUUID());
						}
						if(item.has("sculpt"))
						{
							textures.push_back(item["sculpt"]["texture"].asUUID());
						}
					}
				}
				else if(download_texture && item["type"].asString() == "wearable")
				{
					LLSD::map_iterator tex_iter = item["textures"].beginMap();
					for( ; tex_iter != item["textures"].endMap(); ++tex_iter)
					{
						textures.push_back((*tex_iter).second.asUUID());
					}
				}
			}
			if(download_texture)
			{
				textures.unique();
				while(!textures.empty())
				{
					llinfos << "Requesting texture " << textures.front().asString() << llendl;
					LLViewerImage* img = gImageList.getImage(textures.front(), MIPMAP_TRUE, FALSE);
		            img->setBoostLevel(LLViewerImageBoostLevel::BOOST_MAX_LEVEL);

		            CacheReadResponder* responder = new CacheReadResponder(textures.front(), std::string(path + textures.front().asString() + ".j2c"));
					LLAppViewer::getTextureCache()->readFromCache(textures.front(),LLWorkerThread::PRIORITY_HIGH,0,999999,responder);
					textures.pop_front();
				}
			}

			llofstream export_file(file_name);
			LLSDSerialize::toPrettyXML(sd, export_file);
			export_file.close();

			std::string msg = "Saved ";
			msg.append(file_name);
			if(download_texture) msg.append(" (Content might take some time to download)");
			LLChat chat(msg);
			LLFloaterChat::addChat(chat);
		}
	}
	else
	{
		std::string msg = "No exportable items selected";
		LLChat chat(msg);
		LLFloaterChat::addChat(chat);
		return;
	}
	
	floater->close();
}

//static
void LLFloaterExport::onClickMakeCopy(void* user_data)
{
	LLFloaterExport* floater = (LLFloaterExport*)user_data;
	LLSD sd = floater->getLLSD();

	if(sd.size())
	{
		LLXmlImport::import(new LLXmlImportOptions(sd));
	}
	else
	{
		std::string msg = "No copyable items selected";
		LLChat chat(msg);
		LLFloaterChat::addChat(chat);
		return;
	}
	
	// I guess close the floater because only one import is allowed at once anyway
	floater->close();
}

void LLFloaterExport::addToPrimList(LLViewerObject* object)
{
	mPrimList.push_back(object->getLocalID());
	LLViewerObject::child_list_t child_list = object->getChildren();
	for (LLViewerObject::child_list_t::iterator i = child_list.begin(); i != child_list.end(); ++i)
	{
		LLViewerObject* child = *i;
		if(child->getPCode() < LL_PCODE_APP)
		{
			mPrimList.push_back(child->getLocalID());
		}
	}
}

void LLFloaterExport::updateNamesProgress()
{
	childSetText("names_progress_text", llformat("Names retrieved: %d of %d", mPrimNameMap.size(), mPrimList.size()));
}

void LLFloaterExport::receivePrimName(LLViewerObject* object, std::string name, std::string desc)
{
	LLUUID fullid = object->getID();
	U32 localid = object->getLocalID();
	if(std::find(mPrimList.begin(), mPrimList.end(), localid) != mPrimList.end())
	{
		mPrimNameMap[localid] = std::pair<std::string, std::string>(name, desc);
		LLScrollListCtrl* list = getChild<LLScrollListCtrl>("export_list");
		S32 item_index = list->getItemIndex(fullid);
		if(item_index != -1)
		{
			std::vector<LLScrollListItem*> items = list->getAllData();
			std::vector<LLScrollListItem*>::iterator iter = items.begin();
			std::vector<LLScrollListItem*>::iterator end = items.end();
			for( ; iter != end; ++iter)
			{
				if((*iter)->getUUID() == fullid)
				{
					(*iter)->getColumn(LIST_NAME)->setValue(name + " (worn on " + utf8str_tolower(object->getAttachmentPointName()) + ")");
					break;
				}
			}
		}
		updateNamesProgress();
	}
}

// static
void LLFloaterExport::receiveObjectProperties(LLUUID fullid, std::string name, std::string desc)
{
	LLViewerObject* object = gObjectList.findObject(fullid);
	std::vector<LLFloaterExport*>::iterator iter = LLFloaterExport::instances.begin();
	std::vector<LLFloaterExport*>::iterator end = LLFloaterExport::instances.end();
	for( ; iter != end; ++iter)
	{
		(*iter)->receivePrimName(object, name, desc);
	}
}

// </edit>
