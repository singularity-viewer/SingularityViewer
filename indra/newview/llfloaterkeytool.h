// <edit>
#ifndef LL_LLFLOATERKEYTOOL_H
#define LL_LLFLOATERKEYTOOL_H

#include "llfloater.h"
#include "llkeytool.h"

class LLFloaterKeyTool
: public LLFloater
{
public:
	typedef enum
	{
		YES,
		NO,
		MAYBE
	} isness;
	typedef struct
	{
		LLFloaterKeyTool* floater;
		LLKeyTool::LLKeyType key_type;
		LLAssetType::EType asset_type;
	} clickData;
	LLFloaterKeyTool(LLUUID id);
	static void show(LLUUID id);
	BOOL postBuild(void);
	void close(bool app_quitting);
	void showType(LLKeyTool::LLKeyType key_type, LLAssetType::EType asset_type, isness result);
	static void keyToolCallback(LLUUID id, LLKeyTool::LLKeyType key_type, LLAssetType::EType asset_type, BOOL is, void* user_data);
	static void onClickType(void* user_data);
	static void setChromeAgain(LLUICtrl* ctrl, void* user_data);
	LLUUID mKey;
	LLKeyTool* mKeyTool;
	S32 mListBottom;
	static std::list<LLFloaterKeyTool*> sInstances;
private:
	virtual ~LLFloaterKeyTool();
};

#endif
// </edit>
