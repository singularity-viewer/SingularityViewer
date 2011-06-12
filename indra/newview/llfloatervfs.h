// <edit>
#ifndef LL_LLFLOATERVFS_H
#define LL_LLFLOATERVFS_H
#include "llfloater.h"
#include "llassettype.h"

class AIFilePicker;

class LLFloaterVFS : LLFloater
{
typedef struct
{
	std::string mFilename;
	std::string mName;
	LLUUID mID;
	LLAssetType::EType mType;
} entry;
public:
	LLFloaterVFS();
	~LLFloaterVFS();
	static void show();
	BOOL postBuild();
	void refresh();
	void add(entry file);
	void clear();
	void reloadAll();
	void setEditID(LLUUID edit_id);
	entry getEditEntry();
	void commitEdit();
	void reloadEntry(entry file);
	void removeEntry();
	static void onClickAdd(void* user_data);
	static void onClickAdd_continued(void* user_data, AIFilePicker* filepicker);
	static void onClickClear(void* user_data);
	static void onClickReloadAll(void* user_data);
	static void onCommitFileList(LLUICtrl* ctrl, void* user_data);
	static void onCommitEdit(LLUICtrl* ctrl, void* user_data);
	static void onClickCopyUUID(void* user_data);
	static void onClickItem(void* user_data);
	static void onClickReload(void* user_data);
	static void onClickRemove(void* user_data);
private:
	static LLFloaterVFS* sInstance;
	static std::list<entry> mFiles;
	LLUUID mEditID;
	void setMassEnabled(bool enabled);
	void setEditEnabled(bool enabled);
};
#endif
// </edit>
