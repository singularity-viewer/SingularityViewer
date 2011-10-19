// <edit>
#include "llviewerprecompiledheaders.h"

#include "llinventorybackup.h"
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "statemachine/aifilepicker.h"
#include "statemachine/aidirpicker.h"
#include "llviewertexturelist.h" // gTextureList
#include "llagent.h" // gAgent
#include "llviewerwindow.h" // gViewerWindow
#include "llfloater.h"
#include "lluictrlfactory.h"
#include "llscrolllistctrl.h"
#include "llimagetga.h"
#include "llnotificationsutil.h"

std::list<LLFloaterInventoryBackup*> LLFloaterInventoryBackup::sInstances;

LLInventoryBackupOrder::LLInventoryBackupOrder()
{
	// My personal defaults based on what is assumed to not work
	mDownloadTextures = true;
	mDownloadSounds = true;
	mDownloadCallingCards = false;
	mDownloadLandmarks = true;
	mDownloadScripts = true;
	mDownloadWearables = true;
	mDownloadObjects = false;
	mDownloadNotecards = true;
	mDownloadAnimations = true;
	mDownloadGestures = true;
	//mDownloadOthers = true;
}

LLFloaterInventoryBackupSettings::LLFloaterInventoryBackupSettings(LLInventoryBackupOrder* order)
:	LLFloater(),
	mOrder(order)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inventory_backup_settings.xml");
}

LLFloaterInventoryBackupSettings::~LLFloaterInventoryBackupSettings()
{
}

BOOL LLFloaterInventoryBackupSettings::postBuild(void)
{
	childSetValue("chk_textures", mOrder->mDownloadTextures);
	childSetValue("chk_sounds", mOrder->mDownloadSounds);
	childSetValue("chk_callingcards", mOrder->mDownloadCallingCards);
	childSetValue("chk_landmarks", mOrder->mDownloadLandmarks);
	childSetValue("chk_scripts", mOrder->mDownloadScripts);
	childSetValue("chk_wearables", mOrder->mDownloadWearables);
	childSetValue("chk_objects", mOrder->mDownloadObjects);
	childSetValue("chk_notecards", mOrder->mDownloadNotecards);
	childSetValue("chk_animations", mOrder->mDownloadAnimations);
	childSetValue("chk_gestures", mOrder->mDownloadGestures);
	//childSetValue("chk_others", mOrder->mDownloadOthers);

	childSetAction("next_btn", LLFloaterInventoryBackupSettings::onClickNext, this);

	return TRUE;
}

// static
void LLFloaterInventoryBackupSettings::onClickNext(void* userdata)
{
	LLFloaterInventoryBackupSettings* floater = (LLFloaterInventoryBackupSettings*)userdata;
	LLInventoryBackupOrder* order = floater->mOrder;

	// Apply changes to filters
	order->mDownloadAnimations = floater->childGetValue("chk_animations");
	order->mDownloadCallingCards = floater->childGetValue("chk_callingcards");
	order->mDownloadGestures = floater->childGetValue("chk_gestures");
	order->mDownloadLandmarks = floater->childGetValue("chk_landmarks");
	order->mDownloadNotecards = floater->childGetValue("chk_notecards");
	order->mDownloadObjects = floater->childGetValue("chk_objects");
	//order->mDownloadOthers = floater->childGetValue("chk_others");
	order->mDownloadScripts = floater->childGetValue("chk_scripts");
	order->mDownloadSounds = floater->childGetValue("chk_sounds");
	order->mDownloadTextures = floater->childGetValue("chk_textures");
	order->mDownloadWearables = floater->childGetValue("chk_wearables");

	// Make filters
	std::map<LLAssetType::EType, bool> type_remove;
	type_remove[LLAssetType::AT_ANIMATION] = !order->mDownloadAnimations;
	type_remove[LLAssetType::AT_BODYPART] = !order->mDownloadWearables;
	type_remove[LLAssetType::AT_CALLINGCARD] = !order->mDownloadCallingCards;
	type_remove[LLAssetType::AT_CLOTHING] = !order->mDownloadWearables;
	type_remove[LLAssetType::AT_GESTURE] = !order->mDownloadGestures;
	type_remove[LLAssetType::AT_IMAGE_JPEG] = !order->mDownloadTextures;
	type_remove[LLAssetType::AT_IMAGE_TGA] = !order->mDownloadTextures;
	type_remove[LLAssetType::AT_LANDMARK] = !order->mDownloadLandmarks;
	type_remove[LLAssetType::AT_LSL_TEXT] = !order->mDownloadScripts;
	type_remove[LLAssetType::AT_NOTECARD] = !order->mDownloadNotecards;
	type_remove[LLAssetType::AT_OBJECT] = !order->mDownloadObjects;
	type_remove[LLAssetType::AT_SCRIPT] = !order->mDownloadScripts;
	type_remove[LLAssetType::AT_SOUND] = !order->mDownloadSounds;
	type_remove[LLAssetType::AT_SOUND_WAV] = !order->mDownloadSounds;
	type_remove[LLAssetType::AT_TEXTURE] = !order->mDownloadTextures;
	type_remove[LLAssetType::AT_TEXTURE_TGA] = !order->mDownloadTextures;

	// Apply filters
	std::vector<LLInventoryItem*>::iterator item_iter = order->mItems.begin();
	for( ; item_iter != order->mItems.end(); )
	{
		if(type_remove[(*item_iter)->getType()])
			order->mItems.erase(item_iter);
		else
			++item_iter;
	}

	if(order->mItems.size() < 1)
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "No items passed the filter \\o/";
		LLNotificationsUtil::add("ErrorMessage", args);
		return;
	}

	// Get dir name
	AIDirPicker* dirpicker = new AIDirPicker("New Folder");
	dirpicker->run(boost::bind(&LLFloaterInventoryBackupSettings::onClickNext_continued, userdata, dirpicker));
}

// static
void LLFloaterInventoryBackupSettings::onClickNext_continued(void* userdata, AIDirPicker* dirpicker)
{
	LLFloaterInventoryBackupSettings* floater = (LLFloaterInventoryBackupSettings*)userdata;
	LLInventoryBackupOrder* order = floater->mOrder;

	if (!dirpicker->hasDirname())
	{
		floater->close();
		return;
	}
	std::string dirname = dirpicker->getDirname();

	// Make local directory tree
	LLFile::mkdir(dirname);
	std::vector<LLInventoryCategory*>::iterator _cat_iter = order->mCats.begin();
	std::vector<LLInventoryCategory*>::iterator _cat_end = order->mCats.end();
	for( ; _cat_iter != _cat_end; ++_cat_iter)
	{
		std::string path = dirname + OS_SEP + LLInventoryBackup::getPath(*_cat_iter, order->mCats);
		LLFile::mkdir(path);
	}

	// Go go backup floater
	LLFloaterInventoryBackup* backup_floater = new LLFloaterInventoryBackup(dirname, order->mCats, order->mItems);
	backup_floater->center();

	// Close myself
	floater->close();
}

// static
bool LLInventoryBackup::itemIsFolder(LLInventoryItem* item)
{
	return ((item->getInventoryType() == LLInventoryType::IT_CATEGORY)
		|| (item->getInventoryType() == LLInventoryType::IT_ROOT_CATEGORY));
}

// static
ESaveFilter LLInventoryBackup::getSaveFilter(LLInventoryItem* item)
{
	LLAssetType::EType type = item->getType();
	LLWearableType::EType wear = (LLWearableType::EType)(item->getFlags() & 0xFF);
	switch(type)
	{
	case LLAssetType::AT_TEXTURE:
		return FFSAVE_TGA;
	case LLAssetType::AT_SOUND:
		return FFSAVE_OGG;
	case LLAssetType::AT_SCRIPT:
	case LLAssetType::AT_LSL_TEXT:
		return FFSAVE_LSL;
	case LLAssetType::AT_ANIMATION:
		return FFSAVE_ANIMATN;
	case LLAssetType::AT_GESTURE:
		return FFSAVE_GESTURE;
	case LLAssetType::AT_NOTECARD:
		return FFSAVE_NOTECARD;
	case LLAssetType::AT_LANDMARK:
		return FFSAVE_LANDMARK;
	case LLAssetType::AT_BODYPART:
	case LLAssetType::AT_CLOTHING:
		switch(wear)
		{
		case LLWearableType::WT_EYES:
			return FFSAVE_EYES;
		case LLWearableType::WT_GLOVES:
			return FFSAVE_GLOVES;
		case LLWearableType::WT_HAIR:
			return FFSAVE_HAIR;
		case LLWearableType::WT_JACKET:
			return FFSAVE_JACKET;
		case LLWearableType::WT_PANTS:
			return FFSAVE_PANTS;
		case LLWearableType::WT_SHAPE:
			return FFSAVE_SHAPE;
		case LLWearableType::WT_SHIRT:
			return FFSAVE_SHIRT;
		case LLWearableType::WT_SHOES:
			return FFSAVE_SHOES;
		case LLWearableType::WT_SKIN:
			return FFSAVE_SKIN;
		case LLWearableType::WT_SKIRT:
			return FFSAVE_SKIRT;
		case LLWearableType::WT_SOCKS:
			return FFSAVE_SOCKS;
		case LLWearableType::WT_UNDERPANTS:
			return FFSAVE_UNDERPANTS;
		case LLWearableType::WT_UNDERSHIRT:
			return FFSAVE_UNDERSHIRT;
		case LLWearableType::WT_PHYSICS:
			return FFSAVE_PHYSICS;
		default:
			return FFSAVE_ALL;
		}
	default:
		return FFSAVE_ALL;
	}
}

// static
std::string LLInventoryBackup::getExtension(LLInventoryItem* item)
{
	LLAssetType::EType type = item->getType();
	LLWearableType::EType wear = (LLWearableType::EType)(item->getFlags() & 0xFF);
	std::string scratch;
	switch(type)
	{
	case LLAssetType::AT_TEXTURE:
		return ".tga";
	case LLAssetType::AT_SOUND:
		return ".ogg";
	case LLAssetType::AT_SCRIPT:
	case LLAssetType::AT_LSL_TEXT:
		return ".lsl";
	case LLAssetType::AT_ANIMATION:
		return ".animatn";
	case LLAssetType::AT_GESTURE:
		return ".gesture";
	case LLAssetType::AT_NOTECARD:
		return ".notecard";
	case LLAssetType::AT_LANDMARK:
		return ".landmark";
	case LLAssetType::AT_BODYPART:
	case LLAssetType::AT_CLOTHING:
		scratch = LLWearableType::getTypeName(wear);
		if(scratch == "invalid")
		{
			if(type == LLAssetType::AT_BODYPART)
				scratch = "bodypart";
			else
				scratch = "clothing";
		}
		return "." + scratch;
	default:
		return "";
	}
}

// static
std::string LLInventoryBackup::getUniqueFilename(std::string filename, std::string extension)
{
	if(LLFile::isfile( (filename + extension).c_str() ))
	{
		int i = 1;
		while(LLFile::isfile( (filename + llformat(" %d", i) + extension).c_str() ))
		{
			i++;
		}
		return filename + llformat(" %d", i) + extension;
	}
	return filename + extension;
}

// static
std::string LLInventoryBackup::getUniqueDirname(std::string dirname)
{
	if(LLFile::isdir(dirname.c_str()))
	{
		int i = 1;
		while(LLFile::isdir( (dirname + llformat(" %d", i)).c_str() ))
		{
			i++;
		}
		return dirname + llformat(" %d", i);
	}
	return dirname;
}


// static
void LLInventoryBackup::download(LLInventoryItem* item, LLFloater* floater, loaded_callback_func onImage, LLGetAssetCallback onAsset)
{
	LLInventoryBackup::callbackdata* userdata = new LLInventoryBackup::callbackdata();
	userdata->floater = floater;
	userdata->item = item;
	LLViewerFetchedTexture* imagep;
	
	switch(item->getType())
	{
	case LLAssetType::AT_TEXTURE:
		imagep = LLViewerTextureManager::getFetchedTexture(item->getAssetUUID(), MIPMAP_TRUE, LLViewerTexture::BOOST_UI);
		imagep->setLoadedCallback( onImage, 0, TRUE, FALSE, userdata, NULL ); // was setLoadedCallbackNoAuth
		break;
	case LLAssetType::AT_NOTECARD:
	case LLAssetType::AT_SCRIPT:
	case LLAssetType::AT_LSL_TEXT: // normal script download
	case LLAssetType::AT_LSL_BYTECODE:
		gAssetStorage->getInvItemAsset(LLHost::invalid,
										gAgent.getID(),
										gAgent.getSessionID(),
										item->getPermissions().getOwner(),
										LLUUID::null,
										item->getUUID(),
										item->getAssetUUID(),
										item->getType(),
										onAsset,
										userdata, // user_data
										TRUE);
		break;
	case LLAssetType::AT_SOUND:
	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
	case LLAssetType::AT_ANIMATION:
	case LLAssetType::AT_GESTURE:
	default:
		gAssetStorage->getAssetData(item->getAssetUUID(), item->getType(), onAsset, userdata, TRUE);
		break;
	}
}

// static
void LLInventoryBackup::imageCallback(BOOL success, 
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
		LLInventoryItem* item = data->item;

		if(!success)
		{
			LLSD args;
			args["ERROR_MESSAGE"] = "Download didn't work on " + item->getName() + ".";
			LLNotificationsUtil::add("ErrorMessage", args);
			return;
		}

		AIFilePicker* filepicker = AIFilePicker::create();
		filepicker->open(LLDir::getScrubbedFileName(item->getName()), getSaveFilter(item));
		filepicker->run(boost::bind(&LLInventoryBackup::imageCallback_continued, src, filepicker));
	}
	else
	{
		src_vi->setBoostLevel(LLViewerTexture::BOOST_UI);
	}
}

void LLInventoryBackup::imageCallback_continued(LLImageRaw* src, AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
		return;

	std::string filename = filepicker->getFilename();

	LLPointer<LLImageTGA> image_tga = new LLImageTGA;
	if( !image_tga->encode( src ) )
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "Couldn't encode file.";
		LLNotificationsUtil::add("ErrorMessage", args);
	}
	else if( !image_tga->save( filename ) )
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "Couldn't write file.";
		LLNotificationsUtil::add("ErrorMessage", args);
	}
}

// static
void LLInventoryBackup::assetCallback(LLVFS *vfs,
				   const LLUUID& asset_uuid,
				   LLAssetType::EType type,
				   void* user_data, S32 status, LLExtStat ext_status)
{
	LLInventoryBackup::callbackdata* data = static_cast<LLInventoryBackup::callbackdata*>(user_data);
	LLInventoryItem* item = data->item;

	if(status != 0)
	{
		LLSD args;
		args["ERROR_MESSAGE"] = "Download didn't work on " + item->getName() + ".";
		LLNotificationsUtil::add("ErrorMessage", args);
		return;
	}

	// Todo: this doesn't work for static vfs shit
	LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
	S32 size = file.getSize();

	char* buffer = new char[size];		// Deleted in assetCallback_continued.
	if (buffer == NULL)
	{
		llerrs << "Memory Allocation Failed" << llendl;
		return;
	}

	file.read((U8*)buffer, size);

	// Write it back out...

	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(LLDir::getScrubbedFileName(item->getName()), getSaveFilter(item));
	filepicker->run(boost::bind(&LLInventoryBackup::assetCallback_continued, buffer, size, filepicker));
}

// static
void LLInventoryBackup::assetCallback_continued(char* buffer, S32 size, AIFilePicker* filepicker)
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

// static
void LLInventoryBackup::climb(LLInventoryCategory* cat,
							  std::vector<LLInventoryCategory*>& cats,
							  std::vector<LLInventoryItem*>& items)
{
	LLInventoryModel* model = &gInventory;

	// Add this category
	cats.push_back(cat);

	LLInventoryModel::cat_array_t *direct_cats;
	LLInventoryModel::item_array_t *direct_items;
	model->getDirectDescendentsOf(cat->getUUID(), direct_cats, direct_items);

	// Add items
	LLInventoryModel::item_array_t::iterator item_iter = direct_items->begin();
	LLInventoryModel::item_array_t::iterator item_end = direct_items->end();
	for( ; item_iter != item_end; ++item_iter)
	{
		items.push_back(*item_iter);
	}

	// Do subcategories
	LLInventoryModel::cat_array_t::iterator cat_iter = direct_cats->begin();
	LLInventoryModel::cat_array_t::iterator cat_end = direct_cats->end();
	for( ; cat_iter != cat_end; ++cat_iter)
	{
		climb(*cat_iter, cats, items);
	}
}

// static
std::string LLInventoryBackup::getPath(LLInventoryCategory* cat, std::vector<LLInventoryCategory*> cats)
{
	LLInventoryModel* model = &gInventory;
	std::string path = LLDir::getScrubbedFileName(cat->getName());
	LLInventoryCategory* parent = model->getCategory(cat->getParentUUID());
	while(parent && (std::find(cats.begin(), cats.end(), parent) != cats.end()))
	{
		path = LLDir::getScrubbedFileName(parent->getName()) + OS_SEP + path;
		parent = model->getCategory(parent->getParentUUID());
	}
	return path;
}

// static
void LLInventoryBackup::save(LLFolderView* folder)
{
	LLInventoryModel* model = &gInventory;

	std::set<LLUUID> selected_items;
	folder->getSelectionList(selected_items);

	if(selected_items.size() < 1)
	{
		// No items selected?  Omg
		return;
	}
	else if(selected_items.size() == 1) 
	{
		// One item.  See if it's a folder
		LLUUID id = *(selected_items.begin());
		LLInventoryItem* item = model->getItem(id);
		if(item)
		{
			if(!itemIsFolder(item))
			{
				// Single item, save it now
				LLInventoryBackup::download((LLViewerInventoryItem*)item, NULL, imageCallback, assetCallback);
				return;
			}
		}
	}

	// We got here?  We need to save multiple items or at least make a folder

	std::vector<LLInventoryCategory*> cats;
	std::vector<LLInventoryItem*> items;

	// Make complete lists of child categories and items
	std::set<LLUUID>::iterator sel_iter = selected_items.begin();
	std::set<LLUUID>::iterator sel_end = selected_items.end();
	for( ; sel_iter != sel_end; ++sel_iter)
	{
		LLInventoryCategory* cat = model->getCategory(*sel_iter);
		if(cat)
		{
			climb(cat, cats, items);
		}
	}

	// And what about items inside a folder that wasn't selected?
	// I guess I will just add selected items, so long as they aren't already added
	for(sel_iter = selected_items.begin(); sel_iter != sel_end; ++sel_iter)
	{
		LLInventoryItem* item = model->getItem(*sel_iter);
		if(item)
		{
			if(std::find(items.begin(), items.end(), item) == items.end())
			{
				items.push_back(item);
				LLInventoryCategory* parent = model->getCategory(item->getParentUUID());
				if(std::find(cats.begin(), cats.end(), parent) == cats.end())
				{
					cats.push_back(parent);
				}
			}
		}
	}

	LLInventoryBackupOrder* order = new LLInventoryBackupOrder();
	order->mCats = cats;
	order->mItems = items;
	LLFloaterInventoryBackupSettings* floater = new LLFloaterInventoryBackupSettings(order);
	floater->center();
}



LLFloaterInventoryBackup::LLFloaterInventoryBackup(std::string path, std::vector<LLInventoryCategory*> cats, std::vector<LLInventoryItem*> items)
:	LLFloater(),
	mPath(path),
	mCats(cats),
	mItems(items),
	mBusy(0)
{
	mItemsTotal = mItems.size();
	mItemsCompleted = 0;

	LLFloaterInventoryBackup::sInstances.push_back(this);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inventory_backup.xml");
}


LLFloaterInventoryBackup::~LLFloaterInventoryBackup()
{
	LLFloaterInventoryBackup::sInstances.remove(this);
}

BOOL LLFloaterInventoryBackup::postBuild(void)
{
	// Make progress bar

	/*
	LLLineEditor* line = new LLLineEditor(
		std::string("progress_line"),
		LLRect(4, 80, 396, 60),
		std::string("Progress"));
	line->setEnabled(FALSE);
	addChild(line);

	LLViewBorder* border = new LLViewBorder(
		"progress_border",
		LLRect(4, 79, 395, 60));
	addChild(border);
	*/

	// Add all items to the list

	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("item_list");

	std::vector<LLInventoryItem*>::iterator item_iter = mItems.begin();
	std::vector<LLInventoryItem*>::iterator item_end = mItems.end();
	for( ; item_iter != item_end; ++item_iter)
	{
		LLSD element;
		element["id"] = (*item_iter)->getUUID();

		LLSD& type_column = element["columns"][LIST_TYPE];
		type_column["column"] = "type";
		type_column["type"] = "icon";
		type_column["value"] = "move_down_in.tga"; // FIXME

		LLSD& name_column = element["columns"][LIST_NAME];
		name_column["column"] = "name";
		name_column["value"] = (*item_iter)->getName();

		LLSD& status_column = element["columns"][LIST_STATUS];
		status_column["column"] = "status";
		status_column["value"] = "Pending";

		LLScrollListItem* scroll_itemp = list->addElement(element, ADD_BOTTOM);
		
		//hack to stop crashing
		LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(LIST_TYPE);
		icon->setClickCallback(NULL, NULL);
	}

	// Setup and go!
	mBusy = 1;
	mItemIter = mItems.begin();
	setStatus((*mItemIter)->getUUID(), "Downloading");
	LLInventoryBackup::download(*mItemIter, this, LLFloaterInventoryBackup::imageCallback, LLFloaterInventoryBackup::assetCallback);
	advance();

	return TRUE;
}

void LLFloaterInventoryBackup::advance()
{
	while((mItemIter != mItems.end()) && (mBusy < 4))
	{
		mBusy++;
		mItemIter++;
		if(mItemIter >= mItems.end()) break;
		setStatus((*mItemIter)->getUUID(), "Downloading");
		LLInventoryBackup::download(*mItemIter, this, LLFloaterInventoryBackup::imageCallback, LLFloaterInventoryBackup::assetCallback);
	}
}

void LLFloaterInventoryBackup::setStatus(LLUUID itemid, std::string status)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("item_list");
	std::vector<LLScrollListItem*> items = list->getAllData();
	std::vector<LLScrollListItem*>::iterator iter = items.begin();
	std::vector<LLScrollListItem*>::iterator end = items.end();
	for( ; iter != end; ++iter)
	{
		if((*iter)->getUUID() == itemid)
		{
			(*iter)->getColumn(LIST_STATUS)->setValue(status);
			break;
		}
	}
}

void LLFloaterInventoryBackup::finishItem(LLUUID itemid, std::string status)
{
	// Update big happy progress bar
	mItemsCompleted++;
	LLView* progress_background = getChildView("progress_background", TRUE, TRUE);
	LLRect rect = progress_background->getRect();
	float item_count = (float)mItemsTotal;
	float item_pos = (float)mItemsCompleted;
	float rect_width = (float)rect.getWidth();
	float incr = rect_width / item_count;
	incr *= item_pos;
	rect.mRight = rect.mLeft + (S32)incr;
	LLView* progress_foreground = getChildView("progress_foreground", TRUE, TRUE);
	progress_foreground->setRect(rect);

	if(mItemsCompleted >= mItemsTotal)
	{
		childSetText("progress_background", llformat("Completed %d items.", mItemsTotal));
		childSetVisible("progress_foreground", false);
	}

	// Update item status
	setStatus(itemid, status);

	// And advance
	mBusy--;
	advance();
}

// static
void LLFloaterInventoryBackup::imageCallback(BOOL success, 
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
		LLFloaterInventoryBackup* floater = (LLFloaterInventoryBackup*)(data->floater);
		LLInventoryItem* item = data->item;

		if(std::find(LLFloaterInventoryBackup::sInstances.begin(), LLFloaterInventoryBackup::sInstances.end(), floater) == LLFloaterInventoryBackup::sInstances.end())
		{
			return;
		}

		if(!success)
		{
			floater->finishItem(item->getUUID(), "Failed download");
			return;
		}

		std::string filename = floater->mPath + OS_SEP + LLInventoryBackup::getPath(gInventory.getCategory(item->getParentUUID()), floater->mCats) + OS_SEP + LLDir::getScrubbedFileName(item->getName());
		filename = LLInventoryBackup::getUniqueFilename(filename, LLInventoryBackup::getExtension(item));

		LLPointer<LLImageTGA> image_tga = new LLImageTGA;
		if( !image_tga->encode( src ) )
		{
			floater->finishItem(item->getUUID(), "Failed tga encode");
		}
		else if( !image_tga->save( filename ) )
		{
			floater->finishItem(item->getUUID(), "Failed save");
		}
		else
		{
			floater->finishItem(item->getUUID(), "Done");
		}
	}
	else
	{
		src_vi->setBoostLevel(LLViewerTexture::BOOST_UI);
	}
}

// static
void LLFloaterInventoryBackup::assetCallback(LLVFS *vfs,
				   const LLUUID& asset_uuid,
				   LLAssetType::EType type,
				   void* user_data, S32 status, LLExtStat ext_status)
{
	LLInventoryBackup::callbackdata* data = static_cast<LLInventoryBackup::callbackdata*>(user_data);
	LLFloaterInventoryBackup* floater = (LLFloaterInventoryBackup*)(data->floater);
	LLInventoryItem* item = data->item;

	if(std::find(LLFloaterInventoryBackup::sInstances.begin(), LLFloaterInventoryBackup::sInstances.end(), floater) == LLFloaterInventoryBackup::sInstances.end())
	{
		return;
	}

	if(status != 0)
	{
		floater->finishItem(item->getUUID(), "Failed download");
		return;
	}

	// Todo: this doesn't work for static vfs shit
	LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
	S32 size = file.getSize();

	char* buffer = new char[size];
	if (buffer == NULL)
	{
		//llerrs << "Memory Allocation Failed" << llendl;
		floater->finishItem(item->getUUID(), "Failed memory allocation");
		return;
	}

	file.read((U8*)buffer, size);

	// Write it back out...
	std::string filename = floater->mPath + OS_SEP + LLInventoryBackup::getPath(gInventory.getCategory(item->getParentUUID()), floater->mCats) + OS_SEP + LLDir::getScrubbedFileName(item->getName());
	filename = LLInventoryBackup::getUniqueFilename(filename, LLInventoryBackup::getExtension(item));

	std::ofstream export_file(filename.c_str(), std::ofstream::binary);
	export_file.write(buffer, size);
	export_file.close();

	floater->finishItem(item->getUUID(), "Done");
}
// </edit>
