// <edit>
#ifndef LL_LLINVENTORYBACKUP_H
#define LL_LLINVENTORYBACKUP_H

#if LL_WINDOWS
#define OS_SEP "\\"
#else
#define OS_SEP "/"
#endif


#include "llviewerinventory.h"
#include "llfolderview.h"
#include "statemachine/aifilepicker.h"
#include "llviewertexture.h"
#include "llfloater.h"

class AIDirPicker;

class LLInventoryBackupOrder
{
public:
	LLInventoryBackupOrder();

	std::string mPath;
	std::vector<LLInventoryCategory*> mCats;
	std::vector<LLInventoryItem*> mItems;

	bool mDownloadTextures;
	bool mDownloadSounds;
	bool mDownloadCallingCards;
	bool mDownloadLandmarks;
	bool mDownloadScripts;
	bool mDownloadWearables;
	bool mDownloadObjects;
	bool mDownloadNotecards;
	bool mDownloadAnimations;
	bool mDownloadGestures;
	//bool mDownloadOthers;
};

class LLFloaterInventoryBackupSettings
: public LLFloater
{
public:
	LLFloaterInventoryBackupSettings(LLInventoryBackupOrder* order);
	BOOL postBuild(void);
	static void onClickNext(void* userdata);
	static void onClickNext_continued(void* userdata, AIDirPicker* dirpicker);

	LLInventoryBackupOrder* mOrder;
	virtual ~LLFloaterInventoryBackupSettings();
};

class LLFloaterInventoryBackup
: public LLFloater
{
public:
	LLFloaterInventoryBackup(std::string path, std::vector<LLInventoryCategory*> cats, std::vector<LLInventoryItem*> items);
	BOOL postBuild(void);

	std::string mPath;
	std::vector<LLInventoryCategory*> mCats;
	std::vector<LLInventoryItem*> mItems;
	std::vector<LLInventoryItem*>::iterator mItemIter;
	int mBusy;

	static std::list<LLFloaterInventoryBackup*> sInstances;

private:
	virtual ~LLFloaterInventoryBackup();
	void setStatus(LLUUID itemid, std::string status);
	void finishItem(LLUUID itemid, std::string status);
	void advance();
	static void imageCallback(BOOL success, 
					LLViewerFetchedTexture *src_vi,
					LLImageRaw* src, 
					LLImageRaw* aux_src, 
					S32 discard_level,
					BOOL final,
					void* userdata);
	static void assetCallback(LLVFS *vfs,
				   const LLUUID& asset_uuid,
				   LLAssetType::EType type,
				   void* user_data, S32 status, LLExtStat ext_status);

	int mItemsTotal;
	int mItemsCompleted;

	enum LIST_COLUMN_ORDER
	{
		LIST_TYPE,
		LIST_NAME,
		LIST_STATUS
	};

};

class AIFilePicker;

class LLInventoryBackup
{
public:
	static ESaveFilter getSaveFilter(LLInventoryItem* item);
	static std::string getExtension(LLInventoryItem* item);
	static std::string getUniqueFilename(std::string filename, std::string extension);
	static std::string getUniqueDirname(std::string dirname);
	static bool itemIsFolder(LLInventoryItem* item);
	static void save(LLFolderView* folder);
	static void download(LLInventoryItem* item, LLFloater* floater, loaded_callback_func onImage, LLGetAssetCallback onAsset);
	static std::string getPath(LLInventoryCategory* cat, std::vector<LLInventoryCategory*> cats);

	struct callbackdata
	{
		LLFloater* floater;
		LLInventoryItem* item;
	};

private:
	static void imageCallback(BOOL success, 
					LLViewerFetchedTexture *src_vi,
					LLImageRaw* src, 
					LLImageRaw* aux_src, 
					S32 discard_level,
					BOOL final,
					void* userdata);
	static void imageCallback_continued(LLImageRaw* src, AIFilePicker* filepicker);
	static void assetCallback(LLVFS *vfs,
				   const LLUUID& asset_uuid,
				   LLAssetType::EType type,
				   void* user_data, S32 status, LLExtStat ext_status);
	static void assetCallback_continued(char* buffer, S32 size, AIFilePicker* filepicker);
	static void climb(LLInventoryCategory* cat,
							  std::vector<LLInventoryCategory*>& cats,
							  std::vector<LLInventoryItem*>& items);
};



#endif
// </edit>
