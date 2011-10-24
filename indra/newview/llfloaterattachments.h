// <edit>
#ifndef LL_LLFLOATERATTACHMENTS_H
#define LL_LLFLOATERATTACHMENTS_H

#include "llfloater.h"
#include "llchat.h"
#include "llselectmgr.h"
#include "llvoavatar.h"
#include "llvoavatardefines.h"
#include <set>

//maximum number of hud prims to look for, too many and you won't get all of the killobject messages, it seems
#define MAX_HUD_PRIM_NUM 250

class LLHUDAttachment
{
public:
	LLHUDAttachment(std::string name, std::string description, LLUUID owner_id, LLUUID object_id, LLUUID from_task_id, std::vector<LLUUID> textures, U32 request_id = 0, S32 inv_serial = 0);

	std::string mName;
	std::string mDescription;
	LLUUID mOwnerID;
	LLUUID mObjectID;
	LLUUID mFromTaskID;
	std::vector<LLUUID> mTextures;
	U32 mRequestID;
	S32 mInvSerial;
};

class LLFloaterAttachments
: public LLFloater
{
public:
	LLFloaterAttachments();
	BOOL postBuild(void);
	void addAvatarStuff(LLVOAvatar* avatarp);
	void updateNamesProgress();
	void receivePrimName(LLViewerObject* object, std::string name);
	void receiveHUDPrimInfo(LLHUDAttachment* hud_attachment);
	void receiveHUDPrimRoot(LLHUDAttachment* hud_attachment);
	void receiveKillObject(U32 local_id);
	void requestAttachmentProps(LLViewerObject* childp, LLVOAvatar* avatarp);
	void connectFullAndLocalIDs();
	void selectAgentHudPrims(LLViewerObject* avatar);

	static void onCommitAttachmentList(LLUICtrl* ctrl, void* user_data);

	static void onClickInventory(void* user_data);
	static void onClickTextures(void* user_data);
	static void onClickViewChildren(void* user_data);

	void addAttachmentToList(LLUUID objectid, std::string name, std::string desc);

	void refreshButtons();

	static U32 sCurrentRequestID;

	bool mHandleKillObject;
	bool mViewingChildren;
	bool mIDsConnected;

	//A MILLION STL CONTAINERS EVERYWHERE
	LLUUID mAvatarID;

	std::map<U32, LLUUID> mRootRequests;

	std::map<LLUUID, U32> mFull2LocalID;
	std::map<LLUUID, LLHUDAttachment*> mHUDAttachmentPrims;
	std::multimap<LLUUID, LLUUID> mHUDAttachmentHierarchy;

	U32 mReceivedProps;

	std::list<U32> mPendingRequests;
	std::list<LLUUID> mHUDPrims;

	static std::map<LLUUID, S16> sInventoryRequests;

	static void dumpTaskInvFile(const std::string& title, const std::string& filename);

	static std::vector<LLFloaterAttachments*> instances; // for callback-type use

	static void dispatchHUDObjectProperties(LLHUDAttachment* hud_attachment);

	static void dispatchKillObject(LLMessageSystem* msg, void** user_data);
	static void processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data);
	
private:
	virtual ~LLFloaterAttachments();

	enum LIST_COLUMN_ORDER
	{
		LIST_TYPE,
		LIST_NAME,
		LIST_DESC
	};
	
	LLObjectSelectionHandle mSelection;
};

#endif
// </edit>
