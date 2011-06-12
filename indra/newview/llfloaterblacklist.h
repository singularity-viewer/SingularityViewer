// <edit>
#ifndef LL_LLFLOATERBLACKLIST_H
#define LL_LLFLOATERBLACKLIST_H
#include "llfloater.h"

class AIFilePicker;

class LLFloaterBlacklist : LLFloater
{
public:
	LLFloaterBlacklist();
	~LLFloaterBlacklist();
	static void show();
	BOOL postBuild();
	void refresh();
	static LLFloaterBlacklist* getInstance() { return sInstance; };
	
	
	/*This is the function to call to add anything to the blacklist, 
	key is the asset ID
	LLSD data is as follows: LLSD[entry_type] = LLAssetType::Etype, 
							 LLSD[entry_name] = std::string, 
							 //LLSD[entry_date] = (automatically generated std::string)

	The LLSD object can hold other data without interfering with the function of this list
	as long as you keep the above format.
	*/
	static void addEntry(LLUUID key, LLSD data);




	static std::map<LLUUID,LLSD> blacklist_entries;
	static std::vector<LLUUID> blacklist_textures;
		
	static void loadFromSave();


protected:
	LLUUID mSelectID;
private:
	static LLFloaterBlacklist* sInstance;
	static void updateBlacklists();
	static void saveToDisk();
	static void onClickAdd(void* user_data);
	static void onClickClear(void* user_data);
	static void onClickSave(void* user_data);
	static void onClickSave_continued(AIFilePicker* filepicker);
	static void onClickLoad(void* user_data);
	static void onClickLoad_continued(AIFilePicker* filepicker);
	static void onClickCopyUUID(void* user_data);
	static void onClickRemove(void* user_data);
	
};
#endif
// </edit>
