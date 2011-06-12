// <edit>
//A lot of bad things going on in here, needs a rewrite.
#include "llviewerprecompiledheaders.h"
#include "llfloatervfsexplorer.h"
#include "lluictrlfactory.h"
#include "llscrolllistctrl.h"
#include "llcheckboxctrl.h"
#include "llvfs.h"
#include "lllocalinventory.h"
#include "llviewerwindow.h"
#include "llassetconverter.h"
#include "dofloaterhex.h"

LLFloaterVFSExplorer* LLFloaterVFSExplorer::sInstance;
std::map<LLVFSFileSpecifier, LLVFSFileBlock*> LLFloaterVFSExplorer::sVFSFileMap;

LLFloaterVFSExplorer::LLFloaterVFSExplorer()
:	LLFloater(),
	mEditID(LLUUID::null)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_vfs_explorer.xml");
}
LLFloaterVFSExplorer::~LLFloaterVFSExplorer()
{
	sInstance = NULL;
}
// static
void LLFloaterVFSExplorer::show()
{
	if(sInstance)
		sInstance->open();
	else
	{
		sInstance = new LLFloaterVFSExplorer();
		sInstance->open();
	}
}
BOOL LLFloaterVFSExplorer::postBuild()
{
	childSetCommitCallback("file_list", onCommitFileList, this);
	childSetAction("remove_btn", onClickRemove, this);
	childSetAction("reload_all_btn", onClickReload, this);
	childSetAction("copy_uuid_btn", onClickCopyUUID, this);
	childSetAction("edit_data_btn", onClickEditData, this);
	childSetAction("item_btn", onClickItem, this);
	refresh();
	return TRUE;
}
void LLFloaterVFSExplorer::refresh()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("file_list");
	list->clearRows();
	sVFSFileMap = gVFS->getFileList();
	std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator end = sVFSFileMap.end();
	for(std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator iter = sVFSFileMap.begin(); iter != end; ++iter)
	{
		LLVFSFileSpecifier file_spec = iter->first;
		LLSD element;
		element["id"] = file_spec.mFileID;
		LLSD& name_column = element["columns"][0];
		name_column["column"] = "name";
		name_column["value"] = file_spec.mFileID.asString();
		LLSD& type_column = element["columns"][1];
		type_column["column"] = "type";
		type_column["value"] = std::string(LLAssetType::lookup(file_spec.mFileType));
		list->addElement(element, ADD_BOTTOM);
	}
	setEditID(mEditID);
}
void LLFloaterVFSExplorer::reloadAll()
{
	//get our magic from gvfs here
	//this should re-request all assets from the server if possible.
	refresh();
}
void LLFloaterVFSExplorer::setEditID(LLUUID edit_id)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("file_list");
	bool found_in_list = (list->getItemIndex(edit_id) != -1);
	bool found_in_files = false;
	LLVFSFileSpecifier file;
	std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator end = sVFSFileMap.end();
	for(std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator iter = sVFSFileMap.begin(); iter != end; ++iter)
	{
		if((*iter).first.mFileID == edit_id)
		{
			found_in_files = true;
			file = (*iter).first;
			break;
		}
	}
	if(found_in_files && found_in_list)
	{
		mEditID = edit_id;
		list->selectByID(edit_id);
		setEditEnabled(true);
		childSetText("name_edit", file.mFileID.asString());
		childSetText("id_edit", file.mFileID.asString());
		childSetValue("type_combo", std::string(LLAssetType::lookup(file.mFileType)));
	}
	else
	{
		mEditID = LLUUID::null;
		list->deselectAllItems(TRUE);
		setEditEnabled(false);
		childSetText("name_edit", std::string(""));
		childSetText("id_edit", std::string(""));
		childSetValue("type_combo", std::string("animatn"));
	}
}
LLVFSFileSpecifier LLFloaterVFSExplorer::getEditEntry()
{
	LLVFSFileSpecifier file;
	std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator end = sVFSFileMap.end();
	for(std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator iter = sVFSFileMap.begin(); iter != end; ++iter)
	{
		if((*iter).first.mFileID == mEditID)
			return (*iter).first;
	}
	file.mFileID = LLUUID::null;
	return file;
}

void LLFloaterVFSExplorer::removeEntry()
{
	std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator end = sVFSFileMap.end();
	for(std::map<LLVFSFileSpecifier, LLVFSFileBlock*>::iterator iter = sVFSFileMap.begin(); iter != end; ++iter)
	{
		if((*iter).first.mFileID == mEditID)
		{
			gVFS->removeFile((*iter).first.mFileID, (*iter).first.mFileType);
			sVFSFileMap.erase(iter);
			break;
		}
		else ++iter;
	}
	refresh();
}
void LLFloaterVFSExplorer::setEditEnabled(bool enabled)
{
	childSetEnabled("name_edit", false);
	childSetEnabled("id_edit", false);
	childSetEnabled("type_combo", false);
	childSetEnabled("edit_data_btn", enabled);
	childSetEnabled("remove_btn", enabled);
	childSetEnabled("copy_uuid_btn", enabled);
}
// static
void LLFloaterVFSExplorer::onCommitFileList(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterVFSExplorer* floaterp = (LLFloaterVFSExplorer*)user_data;
	LLScrollListCtrl* list = floaterp->getChild<LLScrollListCtrl>("file_list");
	LLUUID selected_id(LLUUID::null);
	if(list->getFirstSelected())
		selected_id = list->getFirstSelected()->getUUID();
	floaterp->setEditID(selected_id);
}
// static
void LLFloaterVFSExplorer::onClickCopyUUID(void* user_data)
{
	LLFloaterVFSExplorer* floaterp = (LLFloaterVFSExplorer*)user_data;
	LLVFSFileSpecifier file = floaterp->getEditEntry();
	gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(file.mFileID.asString()));
}
// static
void LLFloaterVFSExplorer::onClickRemove(void* user_data)
{
	LLFloaterVFSExplorer* floaterp = (LLFloaterVFSExplorer*)user_data;
	floaterp->removeEntry();
}
// static
void LLFloaterVFSExplorer::onClickReload(void* user_data)
{
	LLFloaterVFSExplorer* floaterp = (LLFloaterVFSExplorer*)user_data;
	floaterp->reloadAll();
}
// static
void LLFloaterVFSExplorer::onClickEditData(void* user_data)
{
	LLFloaterVFSExplorer* floaterp = (LLFloaterVFSExplorer*)user_data;
	LLVFSFileSpecifier file = floaterp->getEditEntry();
	DOFloaterHex::show(file.mFileID, true, file.mFileType);
}
// static
void LLFloaterVFSExplorer::onClickItem(void* user_data)
{
	LLFloaterVFSExplorer* floaterp = (LLFloaterVFSExplorer*)user_data;
	LLVFSFileSpecifier file = floaterp->getEditEntry();
	LLLocalInventory::addItem(file.mFileID.asString(),file.mFileType,file.mFileID,true);
}
// </edit>
