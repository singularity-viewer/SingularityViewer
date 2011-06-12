// <edit>
#ifndef LL_LLFLOATERVFSEXPLORER_H
#define LL_LLFLOATERVFSEXPLORER_H

#include "llfloater.h"
#include "llassettype.h"
#include "llvfs.h"

class LLFloaterVFSExplorer : LLFloater
{
typedef struct
{
	std::string mFilename;
	std::string mName;
	LLUUID mID;
	LLAssetType::EType mType;
} entry;
public:
	LLFloaterVFSExplorer();
	~LLFloaterVFSExplorer();
	static void show();
	BOOL postBuild();
	void refresh();
	void reloadAll();
	void removeEntry();
	void setEditID(LLUUID edit_id);
	LLVFSFileSpecifier getEditEntry();
	static void onCommitFileList(LLUICtrl* ctrl, void* user_data);
	static void onClickCopyUUID(void* user_data);
	static void onClickRemove(void* user_data);
	static void onClickReload(void* user_data);
	static void onClickEditData(void* user_data);
	static void onClickItem(void* user_data);
private:
	static LLFloaterVFSExplorer* sInstance;
	static std::map<LLVFSFileSpecifier, LLVFSFileBlock*> sVFSFileMap;
	LLUUID mEditID;
	void setEditEnabled(bool enabled);
};
#endif
// </edit>
