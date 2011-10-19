/**
* @file floaterlocalassetbrowse.cpp
* @brief Local texture support
*
* $LicenseInfo:firstyear=2009&license=viewergpl$
*
* Copyright (c) 2010, author unknown
*
* Imprudence Viewer Source Code
* The source code in this file ("Source Code") is provided to you
* under the terms of the GNU General Public License, version 2.0
* ("GPL"). Terms of the GPL can be found in doc/GPL-license.txt in
* this distribution, or online at
* http://secondlifegrid.net/programs/open_source/licensing/gplv2
*
* There are special exceptions to the terms and conditions of the GPL as
* it is applied to this Source Code. View the full text of the exception
* in the file doc/FLOSS-exception.txt in this software distribution, or
* online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
*
* By copying, modifying or distributing this software, you acknowledge
* that you have read and understood your obligations described above,
* and agree to abide by those obligations.
*
* ALL SOURCE CODE IS PROVIDED "AS IS." THE AUTHOR MAKES NO
* WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
* COMPLETENESS OR PERFORMANCE.
* $/LicenseInfo$
*/

/*

tag: vaa emerald local_asset_browser

this feature is still a work in progress.

*/

/* basic headers */
#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"

/* own class header && upload floater header */
#include "floaterlocalassetbrowse.h"
//#include "floaterlocaluploader.h" <- in development.

/* image compression headers. */
#include "llimagebmp.h"
#include "llimagetga.h"
#include "llimagejpeg.h"
#include "llimagepng.h"

/* misc headers */
#include <time.h>
#include <ctime>
#include "llviewertexturelist.h"
#include "llviewerobjectlist.h"
#include "statemachine/aifilepicker.h"
#include "llviewermenufile.h"
#include "llfloaterimagepreview.h"
#include "llfile.h"

/* repeated in header */
#include "lltexturectrl.h"   
#include "llscrolllistctrl.h"
#include "llviewercontrol.h"

/* including to force rebakes when needed */
#include "llagent.h"
#include "llvoavatarself.h"

/* sculpt refresh */
#include "llvovolume.h"
#include "llface.h"


/*=======================================*/
/*     Instantiating manager class       */
/*    and formally declaring it's list   */
/*=======================================*/ 
LocalAssetBrowser* gLocalBrowser;
LocalAssetBrowserTimer* gLocalBrowserTimer;
std::vector<LocalBitmap*> LocalAssetBrowser::loaded_bitmaps;
bool    LocalAssetBrowser::mLayerUpdated;
bool    LocalAssetBrowser::mSculptUpdated;

/*=======================================*/
/*  LocalBitmap: unit class              */
/*=======================================*/ 
/*
	The basic unit class responsible for
	containing one loaded local texture.
*/

LocalBitmap::LocalBitmap(std::string fullpath)
{
	this->valid = false;
	if ( gDirUtilp->fileExists(fullpath) )
	{
		/* taking care of basic properties */
		this->id.generate();
		this->filename	    = fullpath;
		this->linkstatus    = LINK_OFF;
		this->keep_updating = false;
		this->shortname     = gDirUtilp->getBaseFileName(this->filename, true);
		this->bitmap_type   = TYPE_TEXTURE;
		this->sculpt_dirty  = false;
		this->volume_dirty  = false;
		this->valid         = false;

		/* taking care of extension type now to avoid switch madness */
		std::string temp_exten = gDirUtilp->getExtension(this->filename);

		if (temp_exten == "bmp") { this->extension = IMG_EXTEN_BMP; }
		else if (temp_exten == "tga") { this->extension = IMG_EXTEN_TGA; }
		else if (temp_exten == "jpg" || temp_exten == "jpeg") { this->extension = IMG_EXTEN_JPG; }
		else if (temp_exten == "png") { this->extension = IMG_EXTEN_PNG; }
		else { return; } // no valid extension.

		/* getting file's last modified */

		llstat temp_stat;
		LLFile::stat(this->filename, &temp_stat);
		std::time_t time = temp_stat.st_mtime;

		this->last_modified = asctime( localtime(&time) );

		/* checking if the bitmap is valid && decoding if it is */
		LLImageRaw* raw_image = new LLImageRaw();
		if ( this->decodeSelf(raw_image) )
		{
			/* creating a shell LLViewerTexture and fusing raw image into it */
			LLViewerFetchedTexture* viewer_image = new LLViewerFetchedTexture( "file://"+this->filename, this->id, LOCAL_USE_MIPMAPS );
			viewer_image->createGLTexture( LOCAL_DISCARD_LEVEL, raw_image );
			viewer_image->setCachedRawImage(-1,raw_image);

			/* making damn sure gTextureList will not delete it prematurely */
			viewer_image->ref(); 

			/* finalizing by adding LLViewerTexture instance into gTextureList */
			gTextureList.addImage(viewer_image);

			/* filename is valid, bitmap is decoded and valid, i can haz liftoff! */
			this->valid = true;
		}
	}
}

LocalBitmap::~LocalBitmap()
{
}

/* [maintenence functions] */
void LocalBitmap::updateSelf()
{
	if ( this->linkstatus == LINK_ON || this->linkstatus == LINK_UPDATING )
	{
		/* making sure file still exists */
		if ( !gDirUtilp->fileExists(this->filename) ) { this->linkstatus = LINK_BROKEN; return; }

		/* exists, let's check if it's lastmod has changed */
		llstat temp_stat;
		LLFile::stat(this->filename, &temp_stat);
		std::time_t temp_time = temp_stat.st_mtime;

		LLSD new_last_modified = asctime( localtime(&temp_time) );
		if ( this->last_modified.asString() == new_last_modified.asString() ) { return; }

		/* here we update the image */
		LLImageRaw* new_imgraw = new LLImageRaw();
		
		if ( !decodeSelf(new_imgraw) ) { this->linkstatus = LINK_UPDATING; return; }
		else { this->linkstatus = LINK_ON; }

		LLViewerFetchedTexture* image = gTextureList.findImage(this->id);
		
		if (!image->forSculpt()) 
		    { image->createGLTexture( LOCAL_DISCARD_LEVEL, new_imgraw ); }
		else
			{ image->setCachedRawImage(-1,new_imgraw); }

		/* finalizing by updating lastmod to current */
		this->last_modified = new_last_modified;

		/* setting unit property to reflect that it has been changed */
		switch (this->bitmap_type)
		{
			case TYPE_TEXTURE:
				  { break; }

			case TYPE_SCULPT:
				  {
					  /* sets a bool to run through all visible sculpts in one go, and update the ones necessary. */
					  this->sculpt_dirty = true;
					  this->volume_dirty = true;
					  gLocalBrowser->setSculptUpdated( true );
					  break;
				  }

			case TYPE_LAYER:
				  {
					  /* sets a bool to rebake layers after the iteration is done with */
					  gLocalBrowser->setLayerUpdated( true );
					  break;
				  }

			default:
				  { break; }

		}
	}

}

bool LocalBitmap::decodeSelf(LLImageRaw* rawimg)
{
	switch (this->extension)
	{
		case IMG_EXTEN_BMP:
			{
				LLPointer<LLImageBMP> bmp_image = new LLImageBMP;
				if ( !bmp_image->load(filename) ) { break; }
				if ( !bmp_image->decode(rawimg, 0.0f) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT );
				return true;
			}

		case IMG_EXTEN_TGA:
			{
				LLPointer<LLImageTGA> tga_image = new LLImageTGA;
				if ( !tga_image->load(filename) ) { break; }
				if ( !tga_image->decode(rawimg) ) { break; }

				if(	( tga_image->getComponents() != 3) &&
					( tga_image->getComponents() != 4) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT );
				return true;
			}

		case IMG_EXTEN_JPG:
			{
				LLPointer<LLImageJPEG> jpeg_image = new LLImageJPEG;
				if ( !jpeg_image->load(filename) ) { break; }
				if ( !jpeg_image->decode(rawimg, 0.0f) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT );
				return true;
			}

		case IMG_EXTEN_PNG:
			{
				LLPointer<LLImagePNG> png_image = new LLImagePNG;
				if ( !png_image->load(filename) ) { break; }
				if ( !png_image->decode(rawimg, 0.0f) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT );
				return true;
			}

		default:
			break;
	}
	return false;
}

void LocalBitmap::setUpdateBool()
{
	if ( this->linkstatus != LINK_BROKEN )
	{
		if ( !this->keep_updating )
		{
			this->linkstatus = LINK_ON;
			this->keep_updating = true;
		}
		else
		{
			this->linkstatus = LINK_OFF;
			this->keep_updating = false;
		}
	}
	else
	{
		this->keep_updating = false;
	}
}

void LocalBitmap::setType( S32 type )
{
	this->bitmap_type = type;
}

/* [information query functions] */
std::string LocalBitmap::getShortName()
{
	return this->shortname;
}

std::string LocalBitmap::getFileName()
{
	return this->filename;
}

LLUUID LocalBitmap::getID()
{
	return this->id;
}

LLSD LocalBitmap::getLastModified()
{
	return this->last_modified;
}

std::string LocalBitmap::getLinkStatus()
{
	switch(this->linkstatus)
	{
		case LINK_ON:
			return "On";

		case LINK_OFF:
			return "Off";

		case LINK_BROKEN:
			return "Broken";

		case LINK_UPDATING:
			return "Updating";

		default:
			return "Unknown";
	}
}

bool LocalBitmap::getUpdateBool()
{
	return this->keep_updating;
}

bool LocalBitmap::getIfValidBool()
{
	return this->valid;
}

LocalBitmap* LocalBitmap::getThis()
{
	return this;
}

S32 LocalBitmap::getType()
{
	return this->bitmap_type;
}

std::vector<LLFace*> LocalBitmap::getFaceUsesThis(LLDrawable* drawable)
{
	std::vector<LLFace*> matching_faces;

	for ( S32 face_iter = 0; face_iter <= drawable->getNumFaces(); face_iter++ )
	{
		LLFace* newface = drawable->getFace(face_iter);

		if ( this->id == newface->getTexture()->getID() )
		{ matching_faces.push_back(newface); }
	}

	return matching_faces;
}

std::vector<affected_object> LocalBitmap::getUsingObjects(bool seek_by_type, bool seek_textures, bool seek_sculptmaps)
{
	std::vector<affected_object> affected_vector;
	
	for( LLDynamicArrayPtr< LLPointer<LLViewerObject>, 256 >::iterator  iter = gObjectList.mObjects.begin();
		 iter != gObjectList.mObjects.end(); iter++ )
	{
		LLViewerObject* obj = *iter;

		if (obj->isDead())
		{
			continue;
		}

		affected_object shell;
		shell.object = obj;
		shell.local_sculptmap = false;
		bool obj_relevant = false;

		if ( obj && obj->mDrawable )
		{
			/* looking for textures */
			if ( seek_textures || ( seek_by_type && this->bitmap_type == TYPE_TEXTURE ) )
			{
				std::vector<LLFace*> affected_faces = this->getFaceUsesThis( obj->mDrawable );
				if ( !affected_faces.empty() )
				{
					shell.face_list = affected_faces;
					obj_relevant = true;
				}
			}

			/* looking for sculptmaps */
			if ( ( seek_sculptmaps || ( seek_by_type && this->bitmap_type == TYPE_SCULPT ) )
				   && obj->isSculpted() && obj->getVolume() 
				   && this->id == obj->getVolume()->getParams().getSculptID() 
			   )	
			{ 
				shell.local_sculptmap = true;
				obj_relevant = true;
			}
		}

		if (obj_relevant)
		{ affected_vector.push_back(shell); }
	}



	return affected_vector;
}

void LocalBitmap::getDebugInfo()
{
	/* debug function: dumps everything human readable into llinfos */
	llinfos << "===[local bitmap debug]==="               << "\n"
			<< "path: "          << this->filename        << "\n"
			<< "name: "          << this->shortname       << "\n"
			<< "extension: "     << this->extension       << "\n"
			<< "uuid: "          << this->id              << "\n"
			<< "last modified: " << this->last_modified   << "\n"
			<< "link status: "   << this->getLinkStatus() << "\n"
			<< "keep updated: "  << this->keep_updating   << "\n"
			<< "type: "          << this->bitmap_type     << "\n"
			<< "is valid: "      << this->valid           << "\n"
			<< "=========================="               << llendl;
	
}

/*=======================================*/
/*  LocalAssetBrowser: internal class  */
/*=======================================*/ 
/*
	Responsible for internal workings.
	Instantiated at the top of the source file.
	Sits in memory until the viewer is closed.
*/

LocalAssetBrowser::LocalAssetBrowser()
{
	this->mLayerUpdated = false;
	this->mSculptUpdated = false;
}

LocalAssetBrowser::~LocalAssetBrowser()
{

}

void LocalAssetBrowser::AddBitmap()
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(FFLOAD_IMAGE, "", "image", true);
	filepicker->run(boost::bind(&LocalAssetBrowser::AddBitmap_continued, filepicker));
}

void LocalAssetBrowser::AddBitmap_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
		return;

	bool change_happened = false;
	std::vector<std::string> const& filenames(filepicker->getFilenames());
	for(std::vector<std::string>::const_iterator filename = filenames.begin(); filename != filenames.end(); ++filename)
	{
		LocalBitmap* unit = new LocalBitmap(*filename);

		if	( unit->getIfValidBool() )
		{
			loaded_bitmaps.push_back( unit ); 
			change_happened = true;
		}
	}

	if ( change_happened )
	{ onChangeHappened(); }
}

void LocalAssetBrowser::DelBitmap( std::vector<LLScrollListItem*> delete_vector, S32 column )
{
	bool change_happened = false;
	for( std::vector<LLScrollListItem*>::iterator list_iter = delete_vector.begin();
		 list_iter != delete_vector.end(); list_iter++ )
	{
		LLScrollListItem* list_item = *list_iter; 
		if ( list_item )
		{
			LLUUID id = list_item->getColumn(column)->getValue().asUUID();
			for (local_list_iter iter = loaded_bitmaps.begin();
				 iter != loaded_bitmaps.end();)
			{
				LocalBitmap* unit = (*iter)->getThis();

				if ( unit->getID() == id )
				{	
					LLViewerFetchedTexture* image = gTextureList.findImage(id);
					gTextureList.deleteImage( image );
					image->unref();

					iter = loaded_bitmaps.erase(iter);
					delete unit;
					unit = NULL;

					change_happened = true;
				}
				else
				{ iter++; }
			}
		}
	}

	if ( change_happened )
	{ onChangeHappened(); }
}

void LocalAssetBrowser::onUpdateBool(LLUUID id)
{
	LocalBitmap* unit = GetBitmapUnit( id );
	if ( unit ) 
	{ 
		unit->setUpdateBool(); 
		PingTimer();
	}
}

void LocalAssetBrowser::onSetType(LLUUID id, S32 type)
{
	LocalBitmap* unit = GetBitmapUnit( id );
	if ( unit ) 
	{ unit->setType(type); }
}

LocalBitmap* LocalAssetBrowser::GetBitmapUnit(LLUUID id)
{
	local_list_iter iter = loaded_bitmaps.begin();
	for (; iter != loaded_bitmaps.end(); iter++)
	{ 
		if ( (*iter)->getID() == id ) 
		{
			return (*iter)->getThis();
		} 
	}
	
	return NULL;
}

bool LocalAssetBrowser::IsDoingUpdates()
{
	local_list_iter iter = loaded_bitmaps.begin();
	for (; iter != loaded_bitmaps.end(); iter++)
	{ 
		if ( (*iter)->getUpdateBool() ) 
		{ return true; } /* if at least one unit in the list needs updates - we need a timer. */
	}

	return false;
}


/* Reaction to a change in bitmaplist, this function finds a texture picker floater's appropriate scrolllist
   and passes this scrolllist's pointer to UpdateTextureCtrlList for processing.
   it also processes timer start/stops as needed */
void LocalAssetBrowser::onChangeHappened()
{
	/* own floater update */
	FloaterLocalAssetBrowser::UpdateBitmapScrollList();

	/* texturepicker related */
	const LLView::child_list_t* child_list = gFloaterView->getChildList();
	LLView::child_list_const_iter_t child_list_iter = child_list->begin();

	for (; child_list_iter != child_list->end(); child_list_iter++)
	{
		LLView* view = *child_list_iter;
		if ( view->getName() == LOCAL_TEXTURE_PICKER_NAME )
		{
			LLScrollListCtrl* ctrl = view->getChild<LLScrollListCtrl>
									( LOCAL_TEXTURE_PICKER_LIST_NAME, 
									  LOCAL_TEXTURE_PICKER_RECURSE, 
									  LOCAL_TEXTURE_PICKER_CREATEIFMISSING );

			if ( ctrl ) { UpdateTextureCtrlList(ctrl); }
		}
	}

	/* poking timer to see if it's still needed/still not needed */
	PingTimer();

}

void LocalAssetBrowser::PingTimer()
{
	if ( !loaded_bitmaps.empty() && IsDoingUpdates() )
	{
		if (!gLocalBrowserTimer) 
		{ gLocalBrowserTimer = new LocalAssetBrowserTimer(); }
		
		if ( !gLocalBrowserTimer->isRunning() )
		{ gLocalBrowserTimer->start(); }
	}

	else
	{
		if (gLocalBrowserTimer)
		{
			if ( gLocalBrowserTimer->isRunning() ) 
			{ gLocalBrowserTimer->stop(); } 
		}
	}
}

/* This function refills the texture picker floater's scrolllist with the updated contents of bitmaplist */
void LocalAssetBrowser::UpdateTextureCtrlList(LLScrollListCtrl* ctrl)
{
	if ( ctrl ) // checking again in case called externally for some silly reason.
	{
		ctrl->clearRows(); 
		if ( !loaded_bitmaps.empty() )
		{
			local_list_iter iter = loaded_bitmaps.begin();
			for ( ; iter != loaded_bitmaps.end(); iter++ )
			{
				LLSD element;
				element["columns"][0]["column"] = "unit_name";
				element["columns"][0]["type"] = "text";
				element["columns"][0]["value"] = (*iter)->shortname;

				element["columns"][1]["column"] = "unit_id_HIDDEN";
				element["columns"][1]["type"] = "text";
				element["columns"][1]["value"] = (*iter)->id;

				ctrl->addElement(element);
			}
		}
	}
}

void LocalAssetBrowser::PerformTimedActions(void)
{
	// perform checking if updates are needed && update if so.
	local_list_iter iter;
	for (iter = loaded_bitmaps.begin(); iter != loaded_bitmaps.end(); iter++)
	{ (*iter)->updateSelf(); }

	// one or more sculpts have been updated, refreshing them.
	if ( mSculptUpdated )
	{
		LocalAssetBrowser::local_list_iter iter;
		for(iter = loaded_bitmaps.begin(); iter != loaded_bitmaps.end(); iter++)
		{
			if ( (*iter)->sculpt_dirty )
			{
				PerformSculptUpdates( (*iter)->getThis() );
				(*iter)->sculpt_dirty = false;
			}
		}
		mSculptUpdated = false;
	}

	// one of the layer bitmaps has been updated, we need to rebake.
	if ( mLayerUpdated )
	{
	    LLVOAvatar* avatar = gAgentAvatarp;
	    if (avatar) { avatar->forceBakeAllTextures(SLAM_FOR_DEBUG); }
		
		mLayerUpdated = false;
	}
}

void LocalAssetBrowser::PerformSculptUpdates(LocalBitmap* unit)
{

	/* looking for sculptmap using objects only */
	std::vector<affected_object> object_list = unit->getUsingObjects(false, false, true);
	if (object_list.empty()) { return; }

	for( std::vector<affected_object>::iterator iter = object_list.begin();
		 iter != object_list.end(); iter++ )
	{
		affected_object aobj = *iter;
		if ( aobj.object )
		{
			if ( !aobj.local_sculptmap ) { continue; } // should never get here. only in case of misuse.
			
			// update code [begin]
			if ( unit->volume_dirty )
			{
				LLImageRaw* rawimage = gTextureList.findImage( unit->getID() )->getCachedRawImage();

				aobj.object->getVolume()->sculpt(rawimage->getWidth(), rawimage->getHeight(), 
												  rawimage->getComponents(), rawimage->getData(), 0);	
				unit->volume_dirty = false;
			}

				// tell affected drawable it's got updated
				aobj.object->mDrawable->getVOVolume()->setSculptChanged( true );
				aobj.object->mDrawable->getVOVolume()->markForUpdate( true );
			// update code [end]
		}
			
	}

}

/*==================================================*/
/*  FloaterLocalAssetBrowser : floater class      */
/*==================================================*/ 
/*
	Responsible for talking to the user.
	Instantiated by user request.
	Destroyed when the floater is closed.

*/

// Floater Globals
FloaterLocalAssetBrowser* FloaterLocalAssetBrowser::sLFInstance = NULL;

// widgets:
	LLButton* mAddBtn;
	LLButton* mDelBtn;
	LLButton* mMoreBtn;
	LLButton* mLessBtn;
	LLButton* mUploadBtn;

	
	LLScrollListCtrl* mBitmapList;
	LLTextureCtrl*    mTextureView;
	LLCheckBoxCtrl*   mUpdateChkBox;

	LLLineEditor* mPathTxt;
	LLLineEditor* mUUIDTxt;
	LLLineEditor* mNameTxt;

	LLTextBox* mLinkTxt;
	LLTextBox* mTimeTxt;
	LLComboBox* mTypeComboBox;

	LLTextBox* mCaptionPathTxt;
	LLTextBox* mCaptionUUIDTxt;
	LLTextBox* mCaptionLinkTxt;
	LLTextBox* mCaptionNameTxt;
	LLTextBox* mCaptionTimeTxt;

FloaterLocalAssetBrowser::FloaterLocalAssetBrowser()
:   LLFloater(std::string("local_bitmap_browser_floater"))
{
	// xui creation:
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_local_asset_browse.xml");
	
	// setting element/xui children:
	mAddBtn         = getChild<LLButton>("add_btn");
	mDelBtn         = getChild<LLButton>("del_btn");
	mMoreBtn        = getChild<LLButton>("more_btn");
	mLessBtn        = getChild<LLButton>("less_btn");
	mUploadBtn      = getChild<LLButton>("upload_btn");

	mBitmapList     = getChild<LLScrollListCtrl>("bitmap_list");
	mTextureView    = getChild<LLTextureCtrl>("texture_view");
	mUpdateChkBox   = getChild<LLCheckBoxCtrl>("keep_updating_checkbox");

	mPathTxt            = getChild<LLLineEditor>("path_text");
	mUUIDTxt            = getChild<LLLineEditor>("uuid_text");
	mNameTxt            = getChild<LLLineEditor>("name_text");

	mLinkTxt		    = getChild<LLTextBox>("link_text");
	mTimeTxt		    = getChild<LLTextBox>("time_text");
	mTypeComboBox       = getChild<LLComboBox>("type_combobox");

	mCaptionPathTxt     = getChild<LLTextBox>("path_caption_text");
	mCaptionUUIDTxt     = getChild<LLTextBox>("uuid_caption_text");
	mCaptionLinkTxt     = getChild<LLTextBox>("link_caption_text");
	mCaptionNameTxt     = getChild<LLTextBox>("name_caption_text");
	mCaptionTimeTxt	    = getChild<LLTextBox>("time_caption_text");

	// pre-disabling line editors, they're for view only and buttons that shouldn't be on on-spawn.
	mPathTxt->setEnabled( false );
	mUUIDTxt->setEnabled( false );
	mNameTxt->setEnabled( false );

	mDelBtn->setEnabled( false );
	mUploadBtn->setEnabled( false );

	// setting button callbacks:
	mAddBtn->setClickedCallback(         onClickAdd,         this);
	mDelBtn->setClickedCallback(         onClickDel,         this);
	mMoreBtn->setClickedCallback(        onClickMore,        this);
	mLessBtn->setClickedCallback(        onClickLess,        this);
	mUploadBtn->setClickedCallback(      onClickUpload,      this);
	
	// combo callback
	mTypeComboBox->setCommitCallback(onCommitTypeCombo);

	// scrolllist callbacks
	mBitmapList->setCommitCallback(onChooseBitmapList);

	// checkbox callbacks
	mUpdateChkBox->setCommitCallback(onClickUpdateChkbox);

}

void FloaterLocalAssetBrowser::show(void*)
{
    if (!sLFInstance)
	sLFInstance = new FloaterLocalAssetBrowser();
    sLFInstance->open();
	sLFInstance->UpdateBitmapScrollList();
}

FloaterLocalAssetBrowser::~FloaterLocalAssetBrowser()
{
    sLFInstance=NULL;
}

void FloaterLocalAssetBrowser::onClickAdd(void* userdata)
{	
	gLocalBrowser->AddBitmap();
}

void FloaterLocalAssetBrowser::onClickDel(void* userdata)
{
	gLocalBrowser->DelBitmap( sLFInstance->mBitmapList->getAllSelected() );
}

/* what stopped me from using a single button and simply changing it's label
   is the fact that i'd need to hardcode the button labels here, and that is griff. */
void FloaterLocalAssetBrowser::onClickMore(void* userdata)
{
	FloaterResize(true);
}

void FloaterLocalAssetBrowser::onClickLess(void* userdata)
{
	FloaterResize(false);
}

void FloaterLocalAssetBrowser::onClickUpload(void* userdata)
{
	std::string filename = gLocalBrowser->GetBitmapUnit( 
		(LLUUID)sLFInstance->mBitmapList->getSelectedItemLabel(BITMAPLIST_COL_ID) )->getFileName();

	if ( !filename.empty() )
	{
		LLFloaterImagePreview* floaterp = new LLFloaterImagePreview(filename);
		LLUICtrlFactory::getInstance()->buildFloater(floaterp, "floater_image_preview.xml");
	}
}

void FloaterLocalAssetBrowser::onChooseBitmapList(LLUICtrl* ctrl, void *userdata)
{
	bool button_status = sLFInstance->mBitmapList->isEmpty();
	sLFInstance->mDelBtn->setEnabled(!button_status);
	sLFInstance->mUploadBtn->setEnabled(!button_status);

	sLFInstance->UpdateRightSide();
}

void FloaterLocalAssetBrowser::onClickUpdateChkbox(LLUICtrl *ctrl, void *userdata)
{
	std::string temp_str = sLFInstance->mBitmapList->getSelectedItemLabel(BITMAPLIST_COL_ID);
	if ( !temp_str.empty() )
	{
		gLocalBrowser->onUpdateBool( (LLUUID)temp_str );
		sLFInstance->UpdateRightSide();
	}
}

void FloaterLocalAssetBrowser::onCommitTypeCombo(LLUICtrl* ctrl, void *userdata)
{
	std::string temp_str = sLFInstance->mBitmapList->getSelectedItemLabel(BITMAPLIST_COL_ID);

	if ( !temp_str.empty() )
	{
		S32 selection = sLFInstance->mTypeComboBox->getCurrentIndex();
		gLocalBrowser->onSetType( (LLUUID)temp_str, selection ); 

	}
}

void FloaterLocalAssetBrowser::FloaterResize(bool expand)
{
	sLFInstance->mMoreBtn->setVisible(!expand);
	sLFInstance->mLessBtn->setVisible(expand);
	sLFInstance->mTextureView->setVisible(expand);
	sLFInstance->mUpdateChkBox->setVisible(expand);
	sLFInstance->mCaptionPathTxt->setVisible(expand);
	sLFInstance->mCaptionUUIDTxt->setVisible(expand);
	sLFInstance->mCaptionLinkTxt->setVisible(expand);
	sLFInstance->mCaptionNameTxt->setVisible(expand);
	sLFInstance->mCaptionTimeTxt->setVisible(expand);
	sLFInstance->mTypeComboBox->setVisible(expand);

	sLFInstance->mTimeTxt->setVisible(expand);
	sLFInstance->mPathTxt->setVisible(expand);
	sLFInstance->mUUIDTxt->setVisible(expand);
	sLFInstance->mLinkTxt->setVisible(expand);
	sLFInstance->mNameTxt->setVisible(expand);

	if(expand)
	{ 
		sLFInstance->reshape(LF_FLOATER_EXPAND_WIDTH, LF_FLOATER_HEIGHT); 
		sLFInstance->setResizeLimits(LF_FLOATER_EXPAND_WIDTH, LF_FLOATER_HEIGHT);
		sLFInstance->UpdateRightSide();
	}
	else
	{ 
		sLFInstance->reshape(LF_FLOATER_CONTRACT_WIDTH, LF_FLOATER_HEIGHT);
		sLFInstance->setResizeLimits(LF_FLOATER_CONTRACT_WIDTH, LF_FLOATER_HEIGHT);
	}

}

void FloaterLocalAssetBrowser::UpdateBitmapScrollList()
{
	if ( !sLFInstance ) { return; }

	sLFInstance->mBitmapList->clearRows();
	if (!gLocalBrowser->loaded_bitmaps.empty())
	{
		
		LocalAssetBrowser::local_list_iter iter;
		for(iter = gLocalBrowser->loaded_bitmaps.begin(); iter != gLocalBrowser->loaded_bitmaps.end(); iter++)
		{
			LLSD element;
			element["columns"][BITMAPLIST_COL_NAME]["column"] = "bitmap_name";
			element["columns"][BITMAPLIST_COL_NAME]["type"]   = "text";
			element["columns"][BITMAPLIST_COL_NAME]["value"]  = (*iter)->getShortName();

			element["columns"][BITMAPLIST_COL_ID]["column"] = "bitmap_uuid";
			element["columns"][BITMAPLIST_COL_ID]["type"]   = "text";
			element["columns"][BITMAPLIST_COL_ID]["value"]  = (*iter)->getID();

			sLFInstance->mBitmapList->addElement(element);
		}

	}
	sLFInstance->UpdateRightSide();
}

void FloaterLocalAssetBrowser::UpdateRightSide()
{
	/*
	Since i'm not keeping a bool on if the floater is expanded or not, i'll
	just check if one of the widgets that shows when the floater is expanded is visible.

	Also obviously before updating - checking if something IS actually selected :o
	*/

	if ( !sLFInstance->mTextureView->getVisible() ) { return; }

	if ( !sLFInstance->mBitmapList->getAllSelected().empty() )
	{ 
		LocalBitmap* unit = gLocalBrowser->GetBitmapUnit( LLUUID(sLFInstance->mBitmapList->getSelectedItemLabel(BITMAPLIST_COL_ID)) );
		
		if ( unit )
		{
			sLFInstance->mTextureView->setImageAssetID( unit->getID() );
			sLFInstance->mUpdateChkBox->set( unit->getUpdateBool() );
			sLFInstance->mPathTxt->setText( unit->getFileName() );
			sLFInstance->mUUIDTxt->setText( unit->getID().asString() );
			sLFInstance->mNameTxt->setText( unit->getShortName() );
			sLFInstance->mTimeTxt->setText( unit->getLastModified().asString() );
			sLFInstance->mLinkTxt->setText( unit->getLinkStatus() );
			sLFInstance->mTypeComboBox->selectNthItem( unit->getType() );

			sLFInstance->mTextureView->setEnabled(true);
			sLFInstance->mUpdateChkBox->setEnabled(true);
			sLFInstance->mTypeComboBox->setEnabled(true);
		}
	}
	else
	{
		sLFInstance->mTextureView->setImageAssetID( NO_IMAGE );
		sLFInstance->mTextureView->setEnabled( false );
		sLFInstance->mUpdateChkBox->set( false );
		sLFInstance->mUpdateChkBox->setEnabled( false );

		sLFInstance->mTypeComboBox->selectFirstItem();
		sLFInstance->mTypeComboBox->setEnabled( false );
		
		sLFInstance->mPathTxt->setText( LLStringExplicit("None") );
		sLFInstance->mUUIDTxt->setText( LLStringExplicit("None") );
		sLFInstance->mNameTxt->setText( LLStringExplicit("None") );
		sLFInstance->mLinkTxt->setText( LLStringExplicit("None") );
		sLFInstance->mTimeTxt->setText( LLStringExplicit("None") );
	}
}


/*==================================================*/
/*     LocalAssetBrowserTimer: timer class          */
/*==================================================*/ 
/*
	A small, simple timer class inheriting from
	LLEventTimer, responsible for pinging the
	LocalAssetBrowser class to perform it's
	updates / checks / etc.

*/

LocalAssetBrowserTimer::LocalAssetBrowserTimer() : LLEventTimer( (F32)TIMER_HEARTBEAT )
{

}

LocalAssetBrowserTimer::~LocalAssetBrowserTimer()
{

}

BOOL LocalAssetBrowserTimer::tick()
{
	gLocalBrowser->PerformTimedActions();
	return FALSE;
}

void LocalAssetBrowserTimer::start()
{
	mEventTimer.start();
}

void LocalAssetBrowserTimer::stop()
{
	mEventTimer.stop();
}

bool LocalAssetBrowserTimer::isRunning()
{
	return mEventTimer.getStarted();
}

