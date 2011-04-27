/** 
 * @file llpreviewtexture.cpp
 * @brief LLPreviewTexture class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llpreviewtexture.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llfilepicker.h"
#include "llimagetga.h"
#include "llinventoryview.h"
#include "llinventory.h"
#include "llresmgr.h"
#include "lltextbox.h"
#include "lltextureview.h"
#include "llui.h"
#include "llviewertexturelist.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "lllineeditor.h"
#include "llfloateravatarinfo.h"


const S32 PREVIEW_TEXTURE_MIN_WIDTH = 300;
const S32 PREVIEW_TEXTURE_MIN_HEIGHT = 120;

const F32 PREVIEW_TEXTURE_MAX_ASPECT = 200.f;
const F32 PREVIEW_TEXTURE_MIN_ASPECT = 0.005f;

const S32 CLIENT_RECT_VPAD = 4;

const F32 SECONDS_TO_SHOW_FILE_SAVED_MSG = 8.f;

LLPreviewTexture * LLPreviewTexture::sInstance;
LLPreviewTexture::LLPreviewTexture(const std::string& name,
								   const LLRect& rect,
								   const std::string& title,
								   const LLUUID& item_uuid,
								   const LLUUID& object_id,
								   BOOL show_keep_discard)
:	LLPreview(name, rect, title, item_uuid, object_id, TRUE, PREVIEW_TEXTURE_MIN_WIDTH, PREVIEW_TEXTURE_MIN_HEIGHT ),
	mLoadingFullImage( FALSE ),
	mShowKeepDiscard(show_keep_discard),
	mCopyToInv(FALSE),
	  mIsCopyable(FALSE),
	  mUpdateDimensions(TRUE),
	mLastHeight(0),
	mLastWidth(0),
	mAspectRatio(0.f),
	mImage(NULL),
	mImageOldBoostLevel(LLViewerTexture::BOOST_NONE)
{
	const LLInventoryItem *item = getItem();
	if(item)
	{
		mImageID = item->getAssetUUID();
		const LLPermissions& perm = item->getPermissions();
		U32 mask = PERM_NONE;
		if(perm.getOwner() == gAgent.getID())
		{
			mask = perm.getMaskBase();
		}
		else if(gAgent.isInGroup(perm.getGroup()))
		{
			mask = perm.getMaskGroup();
		}
		else
		{
			mask = perm.getMaskEveryone();
		}
		if((mask & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED)
		{
			mIsCopyable = TRUE;
		}
	}

	init();

	setTitle(title);

	if (!getHost())
	{
		LLRect curRect = getRect();
		translate(rect.mLeft - curRect.mLeft, rect.mTop - curRect.mTop);
	}
}

// Note: uses asset_id as a dummy item id.
LLPreviewTexture::LLPreviewTexture(
	const std::string& name,
	const LLRect& rect,
	const std::string& title,
	const LLUUID& asset_id,
	BOOL copy_to_inv,
	BOOL copyable)
	:
	LLPreview(
		name,
		rect,
		title,
		asset_id,
		LLUUID::null,
		TRUE,
		PREVIEW_TEXTURE_MIN_WIDTH,
		PREVIEW_TEXTURE_MIN_HEIGHT ),
	mImageID(asset_id),
	mLoadingFullImage( FALSE ),
	mShowKeepDiscard(FALSE),
	mCopyToInv(copy_to_inv),
	mIsCopyable(copyable),
	mLastHeight(0),
	mLastWidth(0),
	mAspectRatio(0.f),
	mAlphaMaskResult(0)
{

	init();

	setTitle(title);
	LLRect curRect = getRect();
	translate(curRect.mLeft - rect.mLeft, curRect.mTop - rect.mTop);
	
}


LLPreviewTexture::~LLPreviewTexture()
{
	LLLoadedCallbackEntry::cleanUpCallbackList(&mCallbackTextureList) ;

	if( mLoadingFullImage )
	{
		getWindow()->decBusyCount();
	}
	mImage->setBoostLevel(mImageOldBoostLevel);
	mImage = NULL;
	sInstance = NULL;
}


void LLPreviewTexture::init()
{
	sInstance = this;
	LLUICtrlFactory::getInstance()->buildFloater(sInstance,"floater_preview_texture.xml");
	
	childSetVisible("desc", !mCopyToInv);	// Hide description field for embedded textures
	childSetVisible("desc txt", !mCopyToInv);
	childSetVisible("Copy To Inventory", mCopyToInv);
	childSetVisible("Keep", mShowKeepDiscard);
	childSetVisible("Discard", mShowKeepDiscard);
	childSetAction("openprofile", onClickProfile, this);

	if (mCopyToInv) 
	{
		childSetAction("Copy To Inventory",LLPreview::onBtnCopyToInv,this);
	}

	else if (mShowKeepDiscard)
	{
		childSetAction("Keep",onKeepBtn,this);
		childSetAction("Discard",onDiscardBtn,this);
	}

	else 
	{
		// If the buttons are hidden move stuff down to use the space.
		
		LLRect keep_rect, old_rect, new_rect;
		S32 diff;
		
		childGetRect("Keep", keep_rect);
		childGetRect("combo_aspect_ratio", old_rect);
		
		diff = old_rect.mBottom - keep_rect.mBottom;
		
		new_rect.setOriginAndSize(old_rect.mLeft, old_rect.mBottom - diff,
								  old_rect.getWidth(), old_rect.getHeight());
		childSetRect("combo_aspect_ratio", new_rect);

		childGetRect("aspect_ratio", old_rect);
		new_rect.setOriginAndSize(old_rect.mLeft, old_rect.mBottom - diff,
								  old_rect.getWidth(), old_rect.getHeight());
		childSetRect("aspect_ratio", new_rect);

		childGetRect("dimensions", old_rect);
		new_rect.setOriginAndSize(old_rect.mLeft, old_rect.mBottom - diff,
								  old_rect.getWidth(), old_rect.getHeight());
		childSetRect("dimensions", new_rect);
	}


	if (!mCopyToInv) 
	{
		const LLInventoryItem* item = getItem();
		
		if (item)
		{
			mCreatorKey = item->getCreatorUUID();
			childSetCommitCallback("desc", LLPreview::onText, this);
			childSetText("desc", item->getDescription());
			childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);
			childSetText("uuid", getItemID().asString());
			childSetText("uploader", getItemCreatorName());
			childSetText("uploadtime", getItemCreationDate());
			childSetText("alphanote", std::string("Loading..."));
		}
	}
	
	childSetCommitCallback("combo_aspect_ratio", onAspectRatioCommit, this);
	LLComboBox* combo = getChild<LLComboBox>("combo_aspect_ratio");
	combo->setCurrentByIndex(0);
}

void LLPreviewTexture::callbackLoadAvatarName(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group, void* data)
{
	if (!sInstance) return;
	std::ostringstream fullname;
	fullname << first << " " << last;
	sInstance->childSetText("uploader", fullname.str());
}

void LLPreviewTexture::draw()
{
	if (mUpdateDimensions)
	{
		updateDimensions();
	}
	
	LLPreview::draw();

	if (!isMinimized())
	{
		LLGLSUIDefault gls_ui;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		
		const LLRect& border = mClientRect;
		LLRect interior = mClientRect;
		interior.stretch( -PREVIEW_BORDER_WIDTH );

		// ...border
		gl_rect_2d( border, LLColor4(0.f, 0.f, 0.f, 1.f));
		gl_rect_2d_checkerboard( interior );

		if ( mImage.notNull() )
		{
			// Draw the texture
			glColor3f( 1.f, 1.f, 1.f );
			gl_draw_scaled_image(interior.mLeft,
								interior.mBottom,
								interior.getWidth(),
								interior.getHeight(),
								mImage);

			if (mAlphaMaskResult != mImage->getIsAlphaMask())
			{
				if (!mImage->getIsAlphaMask())
				{
					childSetColor("alphanote", LLColor4::green);
					childSetText("alphanote", std::string("No Alpha"));
				}
				else
				{
					childSetColor("alphanote", LLColor4::red);
					childSetText("alphanote", std::string("Has Alpha"));
				}
				mAlphaMaskResult = mImage->getIsAlphaMask();
			}
			// Pump the texture priority
			F32 pixel_area = mLoadingFullImage ? (F32)MAX_IMAGE_AREA  : (F32)(interior.getWidth() * interior.getHeight() );
			mImage->addTextureStats( pixel_area );
			if(pixel_area > 0.f)
			{
				//boost the previewed image priority to the highest to make it to get loaded first.
				mImage->setAdditionalDecodePriority(1.0f) ;
			}
			// Don't bother decoding more than we can display, unless
			// we're loading the full image.
			if (!mLoadingFullImage)
			{
				S32 int_width = interior.getWidth();
				S32 int_height = interior.getHeight();
				mImage->setKnownDrawSize(int_width, int_height);
			}
			else
			{
				// Don't use this feature
				mImage->setKnownDrawSize(0, 0);
			}

			if( mLoadingFullImage )
			{
				// *TODO: Translate
				LLFontGL::getFontSansSerif()->renderUTF8(std::string("Receiving:"), 0,
					interior.mLeft + 4, 
					interior.mBottom + 4,
					LLColor4::white, LLFontGL::LEFT, LLFontGL::BOTTOM,
					LLFontGL::DROP_SHADOW);
				
				F32 data_progress = mImage->getDownloadProgress();
				
				// Draw the progress bar.
				const S32 BAR_HEIGHT = 12;
				const S32 BAR_LEFT_PAD = 80;
				S32 left = interior.mLeft + 4 + BAR_LEFT_PAD;
				S32 bar_width = getRect().getWidth() - left - RESIZE_HANDLE_WIDTH - 2;
				S32 top = interior.mBottom + 4 + BAR_HEIGHT;
				S32 right = left + bar_width;
				S32 bottom = top - BAR_HEIGHT;

				LLColor4 background_color(0.f, 0.f, 0.f, 0.75f);
				LLColor4 decoded_color(0.f, 1.f, 0.f, 1.0f);
				LLColor4 downloaded_color(0.f, 0.5f, 0.f, 1.0f);

				gl_rect_2d(left, top, right, bottom, background_color);

				if (data_progress > 0.0f)
				{
					// Downloaded bytes
					right = left + llfloor(data_progress * (F32)bar_width);
					if (right > left)
					{
						gl_rect_2d(left, top, right, bottom, downloaded_color);
					}
				}
			}
			else if(!mSavedFileTimer.hasExpired())
			{
				// *TODO: Translate
				LLFontGL::getFontSansSerif()->renderUTF8(std::string("File Saved"), 0,
					interior.mLeft + 4,
					interior.mBottom + 4,
					LLColor4::white, LLFontGL::LEFT, LLFontGL::BOTTOM,
					LLFontGL::DROP_SHADOW);
			}
		}
	} 
}


// virtual
BOOL LLPreviewTexture::canSaveAs() const
{
	return mIsCopyable && !mLoadingFullImage && mImage.notNull() && !mImage->isMissingAsset();
}


// virtual
void LLPreviewTexture::saveAs()
{
	if( mLoadingFullImage )
		return;

	LLFilePicker& file_picker = LLFilePicker::instance();
	const LLViewerInventoryItem* item = getItem() ;
	if( !file_picker.getSaveFile( LLFilePicker::FFSAVE_TGA, item ? LLDir::getScrubbedFileName(item->getName()) : LLStringUtil::null) )
	{
		// User canceled or we failed to acquire save file.
		return;
	}
	// remember the user-approved/edited file name.
	mSaveFileName = file_picker.getFirstFile();
	mLoadingFullImage = TRUE;
	getWindow()->incBusyCount();
	mImage->forceToSaveRawImage(0) ;//re-fetch the raw image if the old one is removed.
	mImage->setLoadedCallback( LLPreviewTexture::onFileLoadedForSave, 
								0, TRUE, FALSE, new LLUUID( mItemUUID ), &mCallbackTextureList );
}


// static
void LLPreviewTexture::onFileLoadedForSave(BOOL success, 
											LLViewerFetchedTexture *src_vi,
											LLImageRaw* src, 
											LLImageRaw* aux_src, 
											S32 discard_level,
											BOOL final,
											void* userdata)
{
	LLUUID* item_uuid = (LLUUID*) userdata;
	LLPreviewTexture* self = NULL;
	preview_map_t::iterator found_it = LLPreview::sInstances.find(*item_uuid);
	if(found_it != LLPreview::sInstances.end())
	{
		self = (LLPreviewTexture*) found_it->second;
	}

	if( final || !success )
	{
		delete item_uuid;

		if( self )
		{
			self->getWindow()->decBusyCount();
			self->mLoadingFullImage = FALSE;
		}
	}

	if( self && final && success )
	{
		LLPointer<LLImageTGA> image_tga = new LLImageTGA;
		if( !image_tga->encode( src ) )
		{
			LLSD args;
			args["FILE"] = self->mSaveFileName;
			LLNotifications::instance().add("CannotEncodeFile", args);
		}
		else if( !image_tga->save( self->mSaveFileName ) )
		{
			LLSD args;
			args["FILE"] = self->mSaveFileName;
			LLNotifications::instance().add("CannotWriteFile", args);
		}
		else
		{
			self->mSavedFileTimer.reset();
			self->mSavedFileTimer.setTimerExpirySec( SECONDS_TO_SHOW_FILE_SAVED_MSG );
		}

		self->mSaveFileName.clear();
	}

	if( self && !success )
	{
		LLNotifications::instance().add("CannotDownloadFile");
	}
}

LLUUID LLPreviewTexture::getItemID()
{
	const LLViewerInventoryItem* item = getItem();
	if(item)
	{
		U32 perms = item->getPermissions().getMaskOwner();
		if ((perms & PERM_TRANSFER) &&
			(perms & PERM_COPY))
		{
			return item->getUUID();
		}
	}
	return LLUUID::null;
}

std::string LLPreviewTexture::getItemCreationDate()
{
	const LLViewerInventoryItem* item = getItem();
	if(item)
	{
		std::string time;
		timeToFormattedString(item->getCreationDate(), gSavedSettings.getString("TimestampFormat"), time);
		return time;
	}
	return "Unknown";
}

std::string LLPreviewTexture::getItemCreatorName()
{
	const LLViewerInventoryItem* item = getItem();
	if(item)
	{
		std::string name;
		gCacheName->getFullName(item->getCreatorUUID(), name);
		mCreatorKey = item->getCreatorUUID();
		return name;
	}
	return "Unknown";
}


// It takes a while until we get height and width information.
// When we receive it, reshape the window accordingly.
void LLPreviewTexture::updateDimensions()
{
	if (!mImage) return;

	S32 image_height = llmax(1, mImage->getHeight(0));
	S32 image_width = llmax(1, mImage->getWidth(0));
	// Attempt to make the image 1:1 on screen.
	// If that fails, cut width by half.
	S32 client_width = image_width;
	S32 client_height = image_height;
	S32 horiz_pad = 2 * (LLPANEL_BORDER_WIDTH + PREVIEW_PAD) + PREVIEW_RESIZE_HANDLE_SIZE;
	S32 vert_pad = PREVIEW_HEADER_SIZE + 2 * CLIENT_RECT_VPAD + LLPANEL_BORDER_WIDTH;	
	S32 max_client_width = gViewerWindow->getWindowWidth() - horiz_pad;
	S32 max_client_height = gViewerWindow->getWindowHeight() - vert_pad;

	if (mAspectRatio > 0.f) client_height = llceil((F32)client_width / mAspectRatio);

	while ((client_width > max_client_width) ||
	       (client_height > max_client_height ))
	{
		client_width /= 2;
		client_height /= 2;
	}

	S32 view_width = client_width + horiz_pad;
	S32 view_height = client_height + vert_pad;
	
	// set text on dimensions display (should be moved out of here and into a callback of some sort)
	childSetTextArg("dimensions", "[WIDTH]", llformat("%d", mImage->getFullWidth()));
	childSetTextArg("dimensions", "[HEIGHT]", llformat("%d", mImage->getFullHeight()));
	
	// add space for dimensions and aspect ratio
	S32 info_height = 0;
	LLRect aspect_rect;
	childGetRect("combo_aspect_ratio", aspect_rect);
	S32 aspect_height = aspect_rect.getHeight();
	info_height += aspect_height + CLIENT_RECT_VPAD;
	view_height += info_height;
	
	S32 button_height = 0;
	if (mShowKeepDiscard || mCopyToInv) {  //mCopyToInvBtn

		// add space for buttons
		view_height += 	BTN_HEIGHT + CLIENT_RECT_VPAD;
		button_height = BTN_HEIGHT + PREVIEW_PAD;
	}

	view_width = llmax(view_width, getMinWidth());
	view_height = llmax(view_height, getMinHeight());
	
	if (client_height != mLastHeight || client_width != mLastWidth)
	{
		mLastWidth = client_width;
		mLastHeight = client_height;

		S32 old_top = getRect().mTop;
		S32 old_left = getRect().mLeft;
		if (getHost())
		{
			getHost()->growToFit(view_width, view_height);
		}
		else
		{
			reshape( view_width, view_height );
			S32 new_bottom = old_top - getRect().getHeight();
			setOrigin( old_left, new_bottom );
			// Try to keep whole view onscreen, don't allow partial offscreen.
			gFloaterView->adjustToFitScreen(this, FALSE);
		}
	}

	
	if (!mUserResized)
	{
		// clamp texture size to fit within actual size of floater after attempting resize
		client_width = llmin(client_width, getRect().getWidth() - horiz_pad);
		client_height = llmin(client_height, getRect().getHeight() - PREVIEW_HEADER_SIZE 
						- (2 * CLIENT_RECT_VPAD) - LLPANEL_BORDER_WIDTH - info_height);

		
	}
	else
	{
		client_width = getRect().getWidth() - horiz_pad;
		if (mAspectRatio > 0.f)
		{
			client_height = llround(client_width / mAspectRatio);
		}
		else
		{
			client_height = getRect().getHeight() - vert_pad;
		}
	}

	S32 max_height = getRect().getHeight() - PREVIEW_BORDER - button_height 
		- CLIENT_RECT_VPAD - info_height - CLIENT_RECT_VPAD - PREVIEW_HEADER_SIZE;

	if (mAspectRatio > 0.f)
	{
		max_height = llmax(max_height, 1);

		if (client_height > max_height)
		{
			client_height = max_height;
			client_width = llround(client_height * mAspectRatio);
		}
	}
	else
	{
		S32 max_width = getRect().getWidth() - horiz_pad;

		client_height = llclamp(client_height, 1, max_height);
		client_width = llclamp(client_width, 1, max_width);
	}
	
	LLRect window_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	window_rect.mTop -= (PREVIEW_HEADER_SIZE + CLIENT_RECT_VPAD);
	window_rect.mBottom += PREVIEW_BORDER + button_height + CLIENT_RECT_VPAD + info_height + CLIENT_RECT_VPAD;

	mClientRect.setLeftTopAndSize(window_rect.getCenterX() - (client_width / 2), window_rect.mTop, client_width, client_height);

	// Hide the aspect ratio label if the window is too narrow
	// Assumes the label should be to the right of the dimensions
	LLRect dim_rect, aspect_label_rect;
	childGetRect("aspect_ratio", aspect_label_rect);
	childGetRect("dimensions", dim_rect);
	childSetVisible("aspect_ratio", dim_rect.mRight < aspect_label_rect.mLeft);
}

// Return true if everything went fine, false if we somewhat modified the ratio as we bumped on border values
bool LLPreviewTexture::setAspectRatio(const F32 width, const F32 height)
{
	mUpdateDimensions = TRUE;

	// We don't allow negative width or height. Also, if height is positive but too small, we reset to default
	// A default 0.f value for mAspectRatio means "unconstrained" in the rest of the code
	if ((width <= 0.f) || (height <= F_APPROXIMATELY_ZERO))
	{
		mAspectRatio = 0.f;
		return false;
	}
	
	// Compute and store the ratio
	F32 ratio = width / height;
	mAspectRatio = llclamp(ratio, PREVIEW_TEXTURE_MIN_ASPECT, PREVIEW_TEXTURE_MAX_ASPECT);
	
	// Return false if we clamped the value, true otherwise
	return (ratio == mAspectRatio);
}

void LLPreviewTexture::onClickProfile(void* userdata)
{
	LLPreviewTexture* self = (LLPreviewTexture*) userdata;
	LLUUID key =  self->mCreatorKey;
	if (!key.isNull()) LLFloaterAvatarInfo::showFromDirectory(key);
}

void LLPreviewTexture::onAspectRatioCommit(LLUICtrl* ctrl, void* userdata)
{
	LLPreviewTexture* self = (LLPreviewTexture*) userdata;
	
	std::string ratio(ctrl->getValue().asString());
	std::string::size_type separator(ratio.find_first_of(":/\\"));
	
	if (std::string::npos == separator) {
		// If there's no separator assume we want an unconstrained ratio
		self->setAspectRatio(0.0f, 0.0f);
		return;
	}
	
	F32 width, height;
	std::istringstream numerator(ratio.substr(0, separator));
	std::istringstream denominator(ratio.substr(separator + 1));
	numerator >> width;
	denominator >> height;

	// TO DO: We could use the return value to decide to rebuild the width and height string here...
	self->setAspectRatio(width, height);
}

void LLPreviewTexture::loadAsset()
{
	mImage = LLViewerTextureManager::getFetchedTexture(mImageID, MIPMAP_TRUE, LLViewerTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
	mImageOldBoostLevel = mImage->getBoostLevel();
	mImage->setBoostLevel(LLViewerTexture::BOOST_PREVIEW);
	mImage->forceToSaveRawImage(0) ;
	mAssetStatus = PREVIEW_ASSET_LOADING;
	mUpdateDimensions = TRUE;
	updateDimensions();
}

LLPreview::EAssetStatus LLPreviewTexture::getAssetStatus()
{
	if (mImage.notNull() && (mImage->getFullWidth() * mImage->getFullHeight() > 0))
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	return mAssetStatus;
}
