/**
 * @file hgfloatertexteditor.h
 * @brief Asset Text Editor floater made by Hazim Gazov (based on hex editor floater by Day Oh)
 * @author Hazim Gazov
 * 
 * $LicenseInfo:firstyear=2010&license=WTFPLV2$
 *  
 */


#ifndef HG_HGFLOATERTEXT_H
#define HG_HGFLOATERTEXT_H

#include "llfloater.h"
#include "lltexteditor.h"
#include "llinventory.h"
#include "llviewertexture.h"

class HGFloaterTextEditor
: public LLFloater
{
public:
	HGFloaterTextEditor(LLInventoryItem* item);
	static void show(LLUUID item_id);
	BOOL postBuild(void);
	void close(bool app_quitting);
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
	static void onClickSave(void* user_data);
	static void onClickUpload(void* user_data);
	static void onSaveComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status);
	LLInventoryItem* mItem;
	LLTextEditor* mEditor;
	static std::list<HGFloaterTextEditor*> sInstances;
private:
	virtual ~HGFloaterTextEditor();
protected:
	static S32 sUploadAmount;
};

#endif
