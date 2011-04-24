// <edit>
/** 
 * @file llfloaterimport.h
 */

#ifndef LL_LLFLOATERIMPORT_H
#define LL_LLFLOATERIMPORT_H
 
#include "llviewerobject.h"
#include "llfloater.h"
#include "llimportobject.h"


class LLFloaterImportProgress
: public LLFloater
{
public:
	void close(bool app_quitting);
	static LLFloaterImportProgress* sInstance;
	static void show();
	static void update();
private:
	LLFloaterImportProgress();
	virtual ~LLFloaterImportProgress();
};


class LLFloaterXmlImportOptions : public LLFloater
{
public:
	LLFloaterXmlImportOptions(LLXmlImportOptions* default_options);
	BOOL postBuild();
	LLXmlImportOptions* mDefaultOptions;
	std::map<LLUUID, LLImportWearable*> mImportWearableMap;
	std::map<LLUUID, LLImportObject*> mImportObjectMap;
private:
	enum LIST_COLUMN_ORDER
	{
		LIST_CHECKED,
		LIST_TYPE,
		LIST_NAME,
	};
	static void onClickSelectAll(void* user_data);
	static void onClickSelectObjects(void* user_data);
	static void onClickSelectWearables(void* user_data);
	static void onClickOK(void* user_data);
	static void onClickCancel(void* user_data);
};

#endif
// </edit>
