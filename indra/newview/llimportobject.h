// <edit>
/** 
 * @file llimportobject.h
 */

#ifndef LL_LLIMPORTOBJECT_H
#define LL_LLIMPORTOBJECT_H

#include "llviewerobject.h"
#include "llfloater.h"
#include "llvoavatardefines.h"

typedef enum e_import_object_state
	{
		IMPORT_INIT,
		//IMPORT_TRANSFORM, Done when new prim is seen.
		IMPORT_VOLUME,
		IMPORT_EXTRA,
		IMPORT_TEXTURE,
		IMPORT_FINISH //moves on to next prim or links.
	} eImportObjectState;
class LLImportAssetData
{
public:
	LLImportAssetData(std::string infilename,LLUUID inassetid,LLAssetType::EType intype);
        U32 localid;
        LLAssetType::EType type;
        LLInventoryType::EType inv_type;
	EWearableType wear_type;
        LLTransactionID tid;
        LLUUID assetid;
	LLUUID folderid;
	LLUUID oldassetid;
        std::string name;
        std::string description;
        std::string filename;
};

class LLImportWearable
{
public:
	std::string mName;
	int mType;
	std::string mData;
	LLSD mOrginalLLSD;
	LLImportWearable(LLSD sd);
	std::list<LLUUID> mTextures;
	void replaceTextures(std::map<LLUUID,LLUUID> textures_replace);
};

class LLImportObject : public LLViewerObject
{
public:
	//LLImportObject(std::string id, std::string parentId);
	LLImportObject(std::string id, LLSD prim);
	std::string mId;
	std::string mParentId;
	std::string mPrimName;
	std::string mPrimDescription;
	bool importIsAttachment;
	U32 importAttachPoint;
	LLVector3 importAttachPos;
	LLQuaternion importAttachRot;
	std::list<LLUUID> mTextures;
};

class LLXmlImportOptions
{
	friend bool	operator==(const LLXmlImportOptions &a, const LLXmlImportOptions &b);
	friend bool	operator!=(const LLXmlImportOptions &a, const LLXmlImportOptions &b);
public:
	LLXmlImportOptions(LLXmlImportOptions* options);
	LLXmlImportOptions(std::string filename);
	LLXmlImportOptions(LLSD llsd);
	virtual ~LLXmlImportOptions();
	void clear();
	void init(LLSD llsd);
	std::string mName;
	//LLSD mLLSD;
	std::string mAssetDir;
	std::vector<LLImportAssetData*> mAssets;
	std::vector<LLImportObject*> mRootObjects;
	std::vector<LLImportObject*> mChildObjects;
	std::vector<LLImportWearable*> mWearables;
	BOOL mKeepPosition;
	LLViewerObject* mSupplier;
	BOOL mReplaceTexture;
protected:
	LLUUID mID;
};


class LLXmlImport
{
public:
	static void import(LLXmlImportOptions* import_options);
	static void onNewPrim(LLViewerObject* object);
	static void onUpdatePrim(LLViewerObject* object);
	static void onNewItem(LLViewerInventoryItem* item);
	static void onNewAttachment(LLViewerObject* object);
	static void Cancel(void* user_data);
	static void rez_supply();
	static void finish_init();
	static void finish_link();

	static bool sImportInProgress;
	static bool sImportHasAttachments;
	static eImportObjectState sState;
	static LLUUID sExpectedUpdate;
	static LLUUID sFolderID;
	static LLViewerObject* sSupplyParams;
	static int sPrimsNeeded;
	static std::vector<LLImportObject*> sPrims; // all prims being imported
	static std::map<U8, bool> sPt2watch; // attach points that need watching
	static std::map<std::string, U8> sId2attachpt; // attach points of all attachables
	static std::map<U8, LLVector3> sPt2attachpos; // positions of all attachables
	static std::map<U8, LLQuaternion> sPt2attachrot; // rotations of all attachables
	static std::map<U32, std::vector<LLViewerObject*> > sLinkSets;//Linksets to link.
	static std::map<U8, std::string> sDescriptions; //original descriptions for attachments
	static int sPrimIndex;
	static int sAttachmentsDone;
	static std::map<std::string, U32> sId2localid;
	static std::map<U32, LLVector3> sRootpositions;
	static std::map<U32, LLQuaternion> sRootrotations;
	static LLXmlImportOptions* sXmlImportOptions;

	static int sTotalAssets;
	static int sUploadedAssets;
	static std::map<LLUUID,LLUUID> sTextureReplace;
};

#endif
// </edit>

