// <edit>
#ifndef LL_LLKEYTOOL_H
#define LL_LLKEYTOOL_H

#include "llcommon.h"
#include "lluuid.h"
#include "message.h"

class LLKeyTool
{
public:
	typedef enum
	{ // Remember to add to aWhat and openKey
		KT_AGENT,
		KT_TASK,
		KT_GROUP,
		KT_REGION,
		KT_PARCEL,
		KT_ITEM,
		KT_ASSET,
		KT_COUNT
	} LLKeyType;
	LLKeyTool(LLUUID key, void (*callback)(LLUUID, LLKeyType, LLAssetType::EType, BOOL, void*), void* user_data);
	~LLKeyTool();
	static std::list<LLKeyTool*> mKeyTools;
	static std::string aWhat(LLKeyType key_type, LLAssetType::EType asset_type = LLAssetType::AT_NONE);
	static void openKey(LLUUID id, LLKeyType key_type, LLAssetType::EType = LLAssetType::AT_NONE);
	static void callback(LLUUID id, LLKeyType key_type, LLAssetType::EType asset_type, BOOL is);
	static void onCacheName(const LLUUID& id, const std::string& name, bool is_group);
	static void onObjectPropertiesFamily(LLMessageSystem *msg, void **user_data);
	static void gotGroupProfile(LLUUID id);
	static void onParcelInfoReply(LLMessageSystem *msg, void **user_data);
	static void onTransferInfo(LLMessageSystem *msg, void **user_data);
	static void onImageData(LLMessageSystem *msg, void **user_data);
	static void onImageNotInDatabase(LLMessageSystem* msg, void **user_data);
	void (*mCallback)(LLUUID, LLKeyType, LLAssetType::EType, BOOL, void*);
	void* mUserData;
	void tryAgent();
	void tryTask();
	void tryGroup();
	void tryRegion();
	void tryParcel();
	void tryItem();
	void tryAsset(LLAssetType::EType asset_type);
	LLUUID mKey;
	std::map<LLKeyType, BOOL> mKeyTypesDone;
	std::map<LLAssetType::EType, BOOL> mAssetTypesDone;
};

#endif
// </edit>
