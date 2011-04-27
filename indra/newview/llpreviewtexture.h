/** 
 * @file llpreviewtexture.h
 * @brief LLPreviewTexture class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLPREVIEWTEXTURE_H
#define LL_LLPREVIEWTEXTURE_H

#include "llpreview.h"
#include "llbutton.h"
#include "llframetimer.h"
#include "llviewertexture.h"

class LLComboBox;
class LLImageRaw;

class LLPreviewTexture : public LLPreview
{
public:
	LLPreviewTexture(
		const std::string& name,
		const LLRect& rect,
		const std::string& title,
		const LLUUID& item_uuid,
		const LLUUID& object_id,
		BOOL show_keep_discard = FALSE);
	LLPreviewTexture(
		const std::string& name,
		const LLRect& rect,
		const std::string& title,
		const LLUUID& asset_id,
		BOOL copy_to_inv = FALSE,
		BOOL copyable = TRUE);
	~LLPreviewTexture();

	virtual void		draw();

	virtual BOOL		canSaveAs() const;
	virtual void		saveAs();
	virtual LLUUID		getItemID();
	virtual std::string	getItemCreatorName();
	virtual std::string	getItemCreationDate();
	virtual void		loadAsset();
	virtual EAssetStatus	getAssetStatus();

	static void			saveToFile(void* userdata);
	static void			onFileLoadedForSave( 
							BOOL success,
							LLViewerFetchedTexture *src_vi,
							LLImageRaw* src, 
							LLImageRaw* aux_src,
							S32 discard_level, 
							BOOL final,
							void* userdata );
	static LLPreviewTexture* getInstance(){ return sInstance; }
	LLUUID mCreatorKey;

protected:
	void				init();
	bool				setAspectRatio(const F32 width, const F32 height);
	static void			onAspectRatioCommit(LLUICtrl*,void* userdata);

	virtual const char *getTitleName() const { return "Texture"; }
	
private:
	void				updateImageID(); // set what image is being uploaded.
	void				updateDimensions();
	LLUUID				mImageID;
	LLPointer<LLViewerFetchedTexture>		mImage;
	S32                 mImageOldBoostLevel;
	std::string			mSaveFileName;
	LLFrameTimer		mSavedFileTimer;
	BOOL				mLoadingFullImage;
	BOOL                mShowKeepDiscard;
	BOOL                mCopyToInv;
	
	static LLPreviewTexture* sInstance;
	static void			onClickProfile(void* userdata);
	static void callbackLoadAvatarName(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group, void* data);

	// This is stored off in a member variable, because the save-as
	// button and drag and drop functionality need to know.
	BOOL mIsCopyable;
	BOOL mUpdateDimensions;
	S32 mLastHeight;
	S32 mLastWidth;
	F32 mAspectRatio;	// 0 = Unconstrained
	S32 mAlphaMaskResult;

	LLLoadedCallbackEntry::source_callback_list_t mCallbackTextureList ; 
};
#endif  // LL_LLPREVIEWTEXTURE_H
