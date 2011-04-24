// <edit>
#ifndef LL_LLFLOATEREXPORT_H
#define LL_LLFLOATEREXPORT_H

#include "llfloater.h"
#include "llselectmgr.h"
#include "llvoavatar.h"
#include "llvoavatardefines.h"

//class LLExportObject
//{
//public:
//	LLExportObject(LLViewerObject* object);
//	//LLExportObject(LLViewerObject* object, std::string name);
//
//	LLSD asLLSD();
//
//	LLViewerObject* mObject;
//};
//
//class LLExportWearable
//{
//public:
//	LLExportWearable(LLVOAvatar* avatar, EWearableType type);
//
//	LLSD asLLSD();
//	std::string getIcon();
//
//	LLVOAvatar* mAvatar;
//	EWearableType mType;
//	std::string mName;
//};
//
//
//class LLExportBakedWearable // not used
//{
//public:
//	LLExportBakedWearable(LLVOAvatar* avatar, EWearableType type);
//
//	LLSD asLLSD();
//	std::string getIcon();
//
//	LLVOAvatar* mAvatar;
//	EWearableType mType;
//	std::string mName;
//};



class LLExportable
{
	enum EXPORTABLE_TYPE
	{
		EXPORTABLE_OBJECT,
		EXPORTABLE_WEARABLE
	};

public:
	LLExportable(LLViewerObject* object, std::string name, std::map<U32,std::pair<std::string, std::string> >& primNameMap);
	LLExportable(LLVOAvatar* avatar, EWearableType type, std::map<U32,std::pair<std::string, std::string> >& primNameMap);

	LLSD asLLSD();

	EXPORTABLE_TYPE mType;
	EWearableType mWearableType;
	LLViewerObject* mObject;
	LLVOAvatar* mAvatar;
	std::map<U32,std::pair<std::string, std::string> >* mPrimNameMap;
};


class LLFloaterExport
: public LLFloater
{
public:
	LLFloaterExport();
	BOOL postBuild(void);
	void addAvatarStuff(LLVOAvatar* avatarp);
	void updateNamesProgress();
	void receivePrimName(LLViewerObject* object, std::string name, std::string desc);

	LLSD getLLSD();

	std::vector<U32> mPrimList;
	std::map<U32, std::pair<std::string, std::string> > mPrimNameMap;

	static std::vector<LLFloaterExport*> instances; // for callback-type use

	static void receiveObjectProperties(LLUUID fullid, std::string name, std::string desc);

	static void onClickSelectAll(void* user_data);
	static void onClickSelectObjects(void* user_data);
	static void onClickSelectWearables(void* user_data);
	static void onClickSaveAs(void* user_data);
	static void onClickMakeCopy(void* user_data);
	
private:
	virtual ~LLFloaterExport();
	void addToPrimList(LLViewerObject* object);

	enum LIST_COLUMN_ORDER
	{
		LIST_CHECKED,
		LIST_TYPE,
		LIST_NAME,
		LIST_AVATARID
	};
	
	LLObjectSelectionHandle mSelection;
	std::map<LLUUID, LLSD> mExportables;
};

#endif
// </edit>
