// <edit>
/** 
 * @file llfloaterimport.cpp
 */

#include "llviewerprecompiledheaders.h"
#include "llimportobject.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llviewerobject.h"
#include "llagent.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llfloater.h"
#include "lllineeditor.h"
#include "llinventorymodel.h"
#include "lluictrlfactory.h"
#include "llscrolllistctrl.h"
#include "llfloaterimport.h"
#include "llimportobject.h"

LLFloaterImportProgress* LLFloaterImportProgress::sInstance;

LLFloaterXmlImportOptions::LLFloaterXmlImportOptions(LLXmlImportOptions* default_options)
:	mDefaultOptions(default_options)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_import_options.xml");
}

BOOL LLFloaterXmlImportOptions::postBuild()
{
	center();
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("import_list");
	// Add all wearables to list and keep an id:ptr map
	std::vector<LLImportWearable*>::iterator wearable_end = mDefaultOptions->mWearables.end();
	for(std::vector<LLImportWearable*>::iterator iter = mDefaultOptions->mWearables.begin();
		iter != wearable_end; ++iter)
	{
		LLImportWearable* wearablep = (*iter);
		LLUUID id; id.generate();
		mImportWearableMap[id] = wearablep;
		LLSD element;
		element["id"] = id;
		LLSD& check_column = element["columns"][LIST_CHECKED];
		check_column["column"] = "checked";
		check_column["type"] = "checkbox";
		check_column["value"] = true;
		LLSD& type_column = element["columns"][LIST_TYPE];
		type_column["column"] = "type";
		type_column["type"] = "icon";
		type_column["value"] = "inv_item_" + LLWearable::typeToTypeName((EWearableType)(wearablep->mType)) + ".tga";
		LLSD& name_column = element["columns"][LIST_NAME];
		name_column["column"] = "name";
		name_column["value"] = wearablep->mName;
		list->addElement(element, ADD_BOTTOM);
	}
	// Add all root objects to list and keep an id:ptr map
	std::vector<LLImportObject*>::iterator object_end = mDefaultOptions->mRootObjects.end();
	for(std::vector<LLImportObject*>::iterator iter = mDefaultOptions->mRootObjects.begin();
		iter != object_end; ++iter)
	{
		LLImportObject* objectp = (*iter);
		LLUUID id; id.generate();
		mImportObjectMap[id] = objectp;
		LLSD element;
		element["id"] = id;
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
		name_column["value"] = objectp->mPrimName;
		list->addElement(element, ADD_BOTTOM);
	}
	// Callbacks
	childSetAction("select_all_btn", onClickSelectAll, this);
	childSetAction("select_objects_btn", onClickSelectObjects, this);
	childSetAction("select_wearables_btn", onClickSelectWearables, this);
	childSetAction("ok_btn", onClickOK, this);
	childSetAction("cancel_btn", onClickCancel, this);
	return TRUE;
}

void LLFloaterXmlImportOptions::onClickSelectAll(void* user_data)
{
	LLFloaterXmlImportOptions* floaterp = (LLFloaterXmlImportOptions*)user_data;
	LLScrollListCtrl* list = floaterp->getChild<LLScrollListCtrl>("import_list");
	std::vector<LLScrollListItem*> items = list->getAllData();
	std::vector<LLScrollListItem*>::iterator item_iter = items.begin();
	std::vector<LLScrollListItem*>::iterator items_end = items.end();
	bool new_value = !((*item_iter)->getColumn(LIST_CHECKED)->getValue());
	for( ; item_iter != items_end; ++item_iter)
	{
		(*item_iter)->getColumn(LIST_CHECKED)->setValue(new_value);
	}
}

void LLFloaterXmlImportOptions::onClickSelectObjects(void* user_data)
{
	LLFloaterXmlImportOptions* floaterp = (LLFloaterXmlImportOptions*)user_data;
	LLScrollListCtrl* list = floaterp->getChild<LLScrollListCtrl>("import_list");
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
			(*item_iter)->getColumn(LIST_CHECKED)->setValue(new_value);
	}
}

void LLFloaterXmlImportOptions::onClickSelectWearables(void* user_data)
{
	LLFloaterXmlImportOptions* floaterp = (LLFloaterXmlImportOptions*)user_data;
	LLScrollListCtrl* list = floaterp->getChild<LLScrollListCtrl>("import_list");
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
			(*item_iter)->getColumn(LIST_CHECKED)->setValue(new_value);
	}
}

void LLFloaterXmlImportOptions::onClickOK(void* user_data)
{
	LLFloaterXmlImportOptions* floaterp = (LLFloaterXmlImportOptions*)user_data;
	LLXmlImportOptions* opt = new LLXmlImportOptions(floaterp->mDefaultOptions);
	opt->clear();
	opt->mReplaceTexture = floaterp->childGetValue("upload_textures");
	LLScrollListCtrl* list = floaterp->getChild<LLScrollListCtrl>("import_list");
	std::vector<LLScrollListItem*> items = list->getAllData();
	std::vector<LLScrollListItem*>::iterator item_end = items.end();
	std::vector<LLImportObject*>::iterator child_end = floaterp->mDefaultOptions->mChildObjects.end();
	std::list<LLUUID> textures;
	for(std::vector<LLScrollListItem*>::iterator iter = items.begin(); iter != item_end; ++iter)
	{
		if((*iter)->getColumn(LIST_CHECKED)->getValue())
		{ // checked
			LLUUID id = (*iter)->getUUID();
			if(floaterp->mImportWearableMap.find(id) != floaterp->mImportWearableMap.end())
			{
				opt->mWearables.push_back(floaterp->mImportWearableMap[id]);
				if(opt->mReplaceTexture)
					std::copy(floaterp->mImportWearableMap[id]->mTextures.begin(),floaterp->mImportWearableMap[id]->mTextures.end(),back_inserter(textures));
			}
			else // object
			{
				LLImportObject* objectp = floaterp->mImportObjectMap[id];

				if(opt->mReplaceTexture)
					std::copy(objectp->mTextures.begin(),objectp->mTextures.end(),back_inserter(textures));

				opt->mRootObjects.push_back(objectp);
				// Add child objects
				for(std::vector<LLImportObject*>::iterator child_iter = floaterp->mDefaultOptions->mChildObjects.begin();
					child_iter != child_end; ++child_iter)
				{
					if((*child_iter)->mParentId == objectp->mId)
					{
						if(opt->mReplaceTexture)
							std::copy((*child_iter)->mTextures.begin(),(*child_iter)->mTextures.end(),back_inserter(textures));
						opt->mChildObjects.push_back((*child_iter));
					}
				}
			}
		}
	}
	if(opt->mReplaceTexture)
	{
		textures.unique();
		if(!opt->mAssetDir.empty() && LLFile::isdir(opt->mAssetDir))
		{
			opt->mAssetDir.append(gDirUtilp->getDirDelimiter()); //lets add the Delimiter now
			for(std::list<LLUUID>::iterator itr = textures.begin(); itr != textures.end(); ++itr)
			{
				std::string filename = opt->mAssetDir + (*itr).asString() + ".j2c";
				//llinfos << "Looking texture at " << filename << llendl;
				if(LLFile::isfile(filename))
				{
					//llinfos << "Found texture at " << filename << llendl;
					LLImportAssetData* data = new LLImportAssetData(filename,(*itr),LLAssetType::AT_TEXTURE);

					opt->mAssets.push_back(data);
				}
			}
		}
	}
	opt->mKeepPosition = floaterp->childGetValue("keep_position_check");
	LLXmlImport::import(opt);
	floaterp->close();
}

void LLFloaterXmlImportOptions::onClickCancel(void* user_data)
{
	LLFloaterXmlImportOptions* floaterp = (LLFloaterXmlImportOptions*)user_data;
	floaterp->close();
}

LLFloaterImportProgress::LLFloaterImportProgress() 
:	LLFloater("ImportProgress", LLRect(0, 130, 400, 0), "Import progress")
{
	LLLineEditor* line;
	LLViewBorder* border;

	line = new LLLineEditor(
		std::string("Uploaded"),
		LLRect(4, 105, 396, 85),
		std::string("Uploaded Assets"));
	line->setEnabled(FALSE);
	addChild(line);

	border = new LLViewBorder(
		"UploadedBorder",
		LLRect(4, 104, 395, 85));
	addChild(border);

	line = new LLLineEditor(
		std::string("Created"),
		LLRect(4, 80, 396, 60),
		std::string("Created prims"));
	line->setEnabled(FALSE);
	addChild(line);

	border = new LLViewBorder(
		"CreatedBorder",
		LLRect(4, 79, 395, 60));
	addChild(border);

	line = new LLLineEditor(
		std::string("Edited"),
		LLRect(4, 55, 396, 35),
		std::string("Edited prims"));
	line->setEnabled(FALSE);
	addChild(line);

	border = new LLViewBorder(
		"EditedBorder",
		LLRect(4, 54, 395, 35));
	addChild(border);

	LLButton* button = new LLButton(
		"CancelButton",
		LLRect(300, 28, 394, 8));
	button->setLabel(std::string("Cancel"));
	button->setEnabled(TRUE);
	addChild(button);
	childSetAction("CancelButton", LLXmlImport::Cancel, this);

	sInstance = this;
}

LLFloaterImportProgress::~LLFloaterImportProgress()
{
	sInstance = NULL;
}

void LLFloaterImportProgress::close(bool app_quitting)
{
	LLXmlImport::sImportInProgress = false;
	LLFloater::close(app_quitting);
}

// static
void LLFloaterImportProgress::show()
{
	if(!sInstance)
		sInstance = new LLFloaterImportProgress();
	sInstance->open();
	sInstance->center();
}

// static
void LLFloaterImportProgress::update()
{
	if(sInstance)
	{
		LLFloaterImportProgress* floater = sInstance;

		int upload_goal = (int)LLXmlImport::sTotalAssets;
		int upload_done = LLXmlImport::sUploadedAssets;
		F32 upload_width = F32(390.f / F32(upload_goal));
		upload_width *= F32(upload_done);
		bool upload_finished = upload_done >= upload_goal;

		int create_goal = (int)LLXmlImport::sPrims.size();
		int create_done = create_goal - LLXmlImport::sPrimsNeeded;
		F32 create_width = F32(390.f / F32(create_goal));
		create_width *= F32(create_done);
		bool create_finished = create_done >= create_goal;

		int edit_goal = create_goal;
		int edit_done = LLXmlImport::sPrimIndex;
		F32 edit_width = F32(390.f / F32(edit_goal));
		edit_width *= edit_done;
		bool edit_finished = edit_done >= edit_goal;

		int attach_goal = (int)LLXmlImport::sId2attachpt.size();
		int attach_done = LLXmlImport::sAttachmentsDone;
		F32 attach_width = F32(390.f / F32(attach_goal));
		attach_width *= F32(attach_done);
		bool attach_finished = attach_done >= attach_goal;

		bool all_finished = upload_finished && create_finished && edit_finished && attach_finished;

		std::string title;
		title.assign(all_finished ? "Imported " : "Importing ");
		title.append(LLXmlImport::sXmlImportOptions->mName);
		if(!all_finished) title.append("...");
		floater->setTitle(title);

		std::string uploaded_text = llformat("Uploaded %d/%d assets", S32(upload_done), S32(upload_goal));
		std::string created_text = llformat("Created %d/%d prims", S32(create_done), S32(create_goal));
		std::string edited_text = llformat("Finished %d/%d prims", edit_done, edit_goal);

		LLLineEditor* text = floater->getChild<LLLineEditor>("Uploaded");
		text->setText(uploaded_text);

		text = floater->getChild<LLLineEditor>("Created");
		text->setText(created_text);

		text = floater->getChild<LLLineEditor>("Edited");
		text->setText(edited_text);

		LLViewBorder* border = floater->getChild<LLViewBorder>("UploadedBorder");
		border->setRect(LLRect(4, 104, 4 + upload_width, 85));

		border = floater->getChild<LLViewBorder>("CreatedBorder");
		border->setRect(LLRect(4, 79, 4 + create_width, 60));

		border = floater->getChild<LLViewBorder>("EditedBorder");
		border->setRect(LLRect(4, 54, 4 + edit_width, 35));

		LLButton* button = floater->getChild<LLButton>("CancelButton");
		button->setEnabled(!all_finished);
	}
}

// </edit>
