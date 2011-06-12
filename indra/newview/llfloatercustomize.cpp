/** 
 * @file llfloatercustomize.cpp
 * @brief The customize avatar floater, triggered by "Appearance..."
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

#include "llimagejpeg.h"
#include "llfloatercustomize.h"
#include "llfontgl.h"
#include "llbutton.h"
#include "lliconctrl.h"
#include "llresmgr.h"
#include "llmorphview.h"
#include "llfloatertools.h"
#include "llagent.h"
#include "lltoolmorph.h"
#include "llvoavatar.h"
#include "llradiogroup.h"
#include "lltoolmgr.h"
#include "llviewermenu.h"
#include "llscrollcontainer.h"
#include "llscrollingpanellist.h"
#include "llsliderctrl.h"
#include "lltabcontainervertical.h"
#include "llviewerwindow.h"
#include "llinventorymodel.h"
#include "llinventoryview.h"
#include "lltextbox.h"
#include "lllineeditor.h"
#include "llviewertexturelist.h"
#include "llfocusmgr.h"
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llappearance.h"
#include "imageids.h"
#include "llmodaldialog.h"
#include "llassetstorage.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "llwearablelist.h"
#include "llviewerinventory.h"
#include "lldbstrings.h"
#include "llcolorswatch.h"
#include "llglheaders.h"
#include "llui.h"
#include "llviewermessage.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"

#include "statemachine/aifilepicker.h"
#include "hippogridmanager.h"

using namespace LLVOAvatarDefines;

//*TODO:translate : The ui xml for this really needs to be integrated with the appearance paramaters

// Globals
LLFloaterCustomize* gFloaterCustomize = NULL;

const F32 PARAM_STEP_TIME_THRESHOLD = 0.25f;

/////////////////////////////////////////////////////////////////////
// LLFloaterCustomizeObserver

class LLFloaterCustomizeObserver : public LLInventoryObserver
{
public:
	LLFloaterCustomizeObserver(LLFloaterCustomize* fc) : mFC(fc) {}
	virtual ~LLFloaterCustomizeObserver() {}
	virtual void changed(U32 mask) { mFC->updateScrollingPanelUI(); }
protected:
	LLFloaterCustomize* mFC;
};

////////////////////////////////////////////////////////////////////////////

// Local Constants 

class LLWearableSaveAsDialog : public LLModalDialog
{
private:
	std::string	mItemName;
	void		(*mCommitCallback)(LLWearableSaveAsDialog*,void*);
	void*		mCallbackUserData;

public:
	LLWearableSaveAsDialog( const std::string& desc, void(*commit_cb)(LLWearableSaveAsDialog*,void*), void* userdata )
		: LLModalDialog( LLStringUtil::null, 240, 100 ),
		  mCommitCallback( commit_cb ),
		  mCallbackUserData( userdata )
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_wearable_save_as.xml");
		
		childSetAction("Save", LLWearableSaveAsDialog::onSave, this );
		childSetAction("Cancel", LLWearableSaveAsDialog::onCancel, this );

		childSetTextArg("name ed", "[DESC]", desc);
	}

	virtual void startModal()
	{
		LLModalDialog::startModal();
		LLLineEditor* edit = getChild<LLLineEditor>("name ed");
		if (!edit) return;
		edit->setFocus(TRUE);
		edit->selectAll();
	}

	const std::string& getItemName() { return mItemName; }

	static void onSave( void* userdata )
	{
		LLWearableSaveAsDialog* self = (LLWearableSaveAsDialog*) userdata;
		self->mItemName = self->childGetValue("name ed").asString();
		LLStringUtil::trim(self->mItemName);
		if( !self->mItemName.empty() )
		{
			if( self->mCommitCallback )
			{
				self->mCommitCallback( self, self->mCallbackUserData );
			}
			self->close(); // destroys this object
		}
	}

	static void onCancel( void* userdata )
	{
		LLWearableSaveAsDialog* self = (LLWearableSaveAsDialog*) userdata;
		self->close(); // destroys this object
	}
};

////////////////////////////////////////////////////////////////////////////

BOOL edit_wearable_for_teens(EWearableType type)
{
	switch(type)
	{
	case WT_UNDERSHIRT:
	case WT_UNDERPANTS:
		return FALSE;
	default:
		return TRUE;
	}
}

class LLMakeOutfitDialog : public LLModalDialog
{
private:
	std::string	mFolderName;
	void		(*mCommitCallback)(LLMakeOutfitDialog*,void*);
	void*		mCallbackUserData;
	std::vector<std::pair<std::string,S32> > mCheckBoxList;
	
public:
	LLMakeOutfitDialog( void(*commit_cb)(LLMakeOutfitDialog*,void*), void* userdata )
		: LLModalDialog(LLStringUtil::null,515, 510, TRUE ),
		  mCommitCallback( commit_cb ),
		  mCallbackUserData( userdata )
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_new_outfit_dialog.xml");
		
		// Build list of check boxes
		for( S32 i = 0; i < WT_COUNT; i++ )
		{
			std::string name = std::string("checkbox_") + LLWearable::typeToTypeLabel( (EWearableType)i );
			mCheckBoxList.push_back(std::make_pair(name,i));
			// Hide teen items
			if (gAgent.isTeen() &&
				!edit_wearable_for_teens((EWearableType)i))
			{
				// hide wearable checkboxes that don't apply to this account
				std::string name = std::string("checkbox_") + LLWearable::typeToTypeLabel( (EWearableType)i );
				childSetVisible(name, FALSE);
			}
		}

		// NOTE: .xml needs to be updated if attachments are added or their names are changed!
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		if( avatar )
		{
			for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
				 iter != avatar->mAttachmentPoints.end(); )
			{
				LLVOAvatar::attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachment = curiter->second;
				S32	attachment_pt = curiter->first;	
				BOOL object_attached = ( attachment->getNumObjects() > 0 );
				std::string name = std::string("checkbox_") + attachment->getName();
				mCheckBoxList.push_back(std::make_pair(name,attachment_pt));
				childSetEnabled(name, object_attached);
			}
		}

		if(!gHippoGridManager->getConnectedGrid()->supportsInvLinks()) {
			childSetEnabled("checkbox_use_links", FALSE);
			childSetValue("checkbox_use_links", FALSE);
			childSetEnabled("checkbox_use_outfits", FALSE);
			childSetValue("checkbox_use_outfits", FALSE);
		}
		
		childSetAction("Save", onSave, this ); 
		childSetAction("Cancel", onCancel, this ); 
		childSetAction("Check All", onCheckAll, this );
		childSetAction("Uncheck All", onUncheckAll, this );

		LLCheckBoxCtrl* pOutfitFoldersCtrl = getChild<LLCheckBoxCtrl>("checkbox_use_outfits");
		pOutfitFoldersCtrl->setCommitCallback(&LLMakeOutfitDialog::onOutfitFoldersToggle);
		pOutfitFoldersCtrl->setCallbackUserData(this);
	}

	BOOL getRenameClothing()
	{
		return childGetValue("rename").asBoolean();

	}
	virtual void draw()
	{
		BOOL one_or_more_items_selected = FALSE;
		for( S32 i = 0; i < (S32)mCheckBoxList.size(); i++ )
		{
			if( childGetValue(mCheckBoxList[i].first).asBoolean() )
			{
				one_or_more_items_selected = TRUE;
				break;
			}
		}

		childSetEnabled("Save", one_or_more_items_selected );
		
		LLModalDialog::draw();
	}

	const std::string& getFolderName() { return mFolderName; }

	void setWearableToInclude( S32 wearable, S32 enabled, S32 selected )
	{
		EWearableType wtType = (EWearableType)wearable;
		if ( ( (0 <= wtType) && (wtType < WT_COUNT) ) && 
			 ( (LLAssetType::AT_BODYPART != LLWearable::typeToAssetType(wtType)) || (!gSavedSettings.getBOOL("UseOutfitFolders")) ) )
		{
			std::string name = std::string("checkbox_") + LLWearable::typeToTypeLabel(wtType);
			childSetEnabled(name, enabled);
			childSetValue(name, selected);
		}
	}

	void getIncludedItems( LLDynamicArray<S32> &wearables_to_include, LLDynamicArray<S32> &attachments_to_include )
	{
		for( S32 i = 0; i < (S32)mCheckBoxList.size(); i++)
		{
			std::string name = mCheckBoxList[i].first;
			BOOL checked = childGetValue(name).asBoolean();
			if (i < WT_COUNT )
			{
				if( checked )
				{
					wearables_to_include.put(i);
				}
			}
			else
			{
				if( checked )
				{
					S32 attachment_pt = mCheckBoxList[i].second;
					attachments_to_include.put( attachment_pt );
				}
			}
		}
	}

	static void onSave( void* userdata )
	{
		LLMakeOutfitDialog* self = (LLMakeOutfitDialog*) userdata;
		self->mFolderName = self->childGetValue("name ed").asString();
		LLStringUtil::trim(self->mFolderName);
		if( !self->mFolderName.empty() )
		{
			if( self->mCommitCallback )
			{
				self->mCommitCallback( self, self->mCallbackUserData );
			}
			self->close(); // destroys this object
		}
	}

	static void onCheckAll( void* userdata )
	{
		LLMakeOutfitDialog* self = (LLMakeOutfitDialog*) userdata;
		for( S32 i = 0; i < (S32)(self->mCheckBoxList.size()); i++)
		{
			std::string name = self->mCheckBoxList[i].first;
			if(self->childIsEnabled(name))self->childSetValue(name,TRUE);
		}
	}

	static void onUncheckAll( void* userdata )
	{
		LLMakeOutfitDialog* self = (LLMakeOutfitDialog*) userdata;
		for( S32 i = 0; i < (S32)(self->mCheckBoxList.size()); i++)
		{
			std::string name = self->mCheckBoxList[i].first;
			if(self->childIsEnabled(name))self->childSetValue(name,FALSE);
		}
	}

	static void onCancel( void* userdata )
	{
		LLMakeOutfitDialog* self = (LLMakeOutfitDialog*) userdata;
		self->close(); // destroys this object
	}

	BOOL postBuild()
	{
		refresh();
		return TRUE;
	}

	void refresh()
	{
		BOOL fUseOutfits = gSavedSettings.getBOOL("UseOutfitFolders");

		for (S32 idxType = 0; idxType < WT_COUNT; idxType++ )
		{
			EWearableType wtType = (EWearableType)idxType;
			if (LLAssetType::AT_BODYPART != LLWearable::typeToAssetType(wtType))
				continue;
			LLCheckBoxCtrl* pCheckCtrl = getChild<LLCheckBoxCtrl>(std::string("checkbox_") + LLWearable::typeToTypeLabel(wtType));
			if (!pCheckCtrl)
				continue;

			pCheckCtrl->setEnabled(!fUseOutfits);
			if (fUseOutfits)
				pCheckCtrl->setValue(TRUE);
		}
		childSetEnabled("checkbox_use_links", !fUseOutfits);
	}

	static void onOutfitFoldersToggle(LLUICtrl*, void* pParam)
	{
		LLMakeOutfitDialog* pSelf = (LLMakeOutfitDialog*)pParam;
		if (pSelf)
			pSelf->refresh();
	}
};

/////////////////////////////////////////////////////////////////////
// LLPanelEditWearable

enum ESubpart {
	SUBPART_SHAPE_HEAD = 1, // avoid 0
	SUBPART_SHAPE_EYES,
	SUBPART_SHAPE_EARS,
	SUBPART_SHAPE_NOSE,
	SUBPART_SHAPE_MOUTH,
	SUBPART_SHAPE_CHIN,
	SUBPART_SHAPE_TORSO,
	SUBPART_SHAPE_LEGS,
	SUBPART_SHAPE_WHOLE,
	SUBPART_SHAPE_DETAIL,
	SUBPART_SKIN_COLOR,
	SUBPART_SKIN_FACEDETAIL,
	SUBPART_SKIN_MAKEUP,
	SUBPART_SKIN_BODYDETAIL,
	SUBPART_HAIR_COLOR,
	SUBPART_HAIR_STYLE,
	SUBPART_HAIR_EYEBROWS,
	SUBPART_HAIR_FACIAL,
	SUBPART_EYES,
	SUBPART_SHIRT,
	SUBPART_PANTS,
	SUBPART_SHOES,
	SUBPART_SOCKS,
	SUBPART_JACKET,
	SUBPART_GLOVES,
	SUBPART_UNDERSHIRT,
	SUBPART_UNDERPANTS,
	SUBPART_SKIRT,
	SUBPART_ALPHA,
	SUBPART_TATTOO,
	SUBPART_PHYSICS_BREASTS_UPDOWN,
    SUBPART_PHYSICS_BREASTS_INOUT,
    SUBPART_PHYSICS_BREASTS_LEFTRIGHT,
    SUBPART_PHYSICS_BELLY_UPDOWN,
    SUBPART_PHYSICS_BUTT_UPDOWN,
    SUBPART_PHYSICS_BUTT_LEFTRIGHT,
    SUBPART_PHYSICS_ADVANCED
 };

struct LLSubpart
{
	LLSubpart() : mSex( SEX_BOTH ), mVisualHint(true) {}

	std::string			mButtonName;
	std::string			mTargetJoint;
	std::string			mEditGroup;
	LLVector3d			mTargetOffset;
	LLVector3d			mCameraOffset;
	ESex				mSex;

	bool				mVisualHint;
};

////////////////////////////////////////////////////////////////////////////

class LLPanelEditWearable : public LLPanel
{
public:
	LLPanelEditWearable( EWearableType type );
	virtual ~LLPanelEditWearable();

	virtual BOOL 		postBuild();
	virtual void		draw();
	virtual BOOL		isDirty() const;	// LLUICtrl
	
	void				addSubpart(const std::string& name, ESubpart id, LLSubpart* part );
	void				addTextureDropTarget( ETextureIndex te, const std::string& name, const LLUUID& default_image_id, BOOL allow_no_texture );
	void				addInvisibilityCheckbox(ETextureIndex te, const std::string& name);
	void				addColorSwatch( ETextureIndex te, const std::string& name );

	const std::string&	getLabel()	{ return LLWearable::typeToTypeLabel( mType ); }
	EWearableType		getType()	{ return mType; }

	LLSubpart*			getCurrentSubpart() { return mSubpartList[mCurrentSubpart]; }
	ESubpart			getDefaultSubpart();
	void				setSubpart( ESubpart subpart );
	void				switchToDefaultSubpart();

	void 				setWearable(LLWearable* wearable, U32 perm_mask, BOOL is_complete);

	void 				setUIPermissions(U32 perm_mask, BOOL is_complete);

	void				hideTextureControls();
	bool				textureIsInvisible(ETextureIndex te);
	void				initPreviousTextureList();
	void				initPreviousTextureListEntry(ETextureIndex te);
	
	virtual void		setVisible( BOOL visible );

	// Callbacks
	static void			onBtnSubpart( void* userdata );
	static void			onBtnTakeOff( void* userdata );
	static void			onBtnSave( void* userdata );

	static void			onBtnSaveAs( void* userdata );
	static void			onSaveAsCommit( LLWearableSaveAsDialog* save_as_dialog, void* userdata );

	static void			onBtnRevert( void* userdata );
	static void			onBtnTakeOffDialog( S32 option, void* userdata );
	static void			onBtnCreateNew( void* userdata );
	static void			onTextureCommit( LLUICtrl* ctrl, void* userdata );
	static void			onInvisibilityCommit( LLUICtrl* ctrl, void* userdata );
	static void			onColorCommit( LLUICtrl* ctrl, void* userdata );
	static void			onCommitSexChange( LLUICtrl*, void* userdata );
	static bool			onSelectAutoWearOption(const LLSD& notification, const LLSD& response);



private:
	EWearableType		mType;
	BOOL				mCanTakeOff;
	std::map<std::string, S32> mTextureList;
	std::map<std::string, S32> mInvisibilityList;
	std::map<std::string, S32> mColorList;
	std::map<ESubpart, LLSubpart*> mSubpartList;
	std::map<S32, LLUUID> mPreviousTextureList;
	ESubpart			mCurrentSubpart;
};

////////////////////////////////////////////////////////////////////////////

LLPanelEditWearable::LLPanelEditWearable( EWearableType type )
	: LLPanel( LLWearable::typeToTypeLabel( type ) ),
	  mType( type )
{
}

BOOL LLPanelEditWearable::postBuild()
{
	LLAssetType::EType asset_type = LLWearable::typeToAssetType( mType );
	/*std::string icon_name = (asset_type == LLAssetType::AT_CLOTHING ?
										 "inv_item_clothing.tga" :
										 "inv_item_skin.tga" );*/
	std::string icon_name = get_item_icon_name(asset_type,LLInventoryType::IT_WEARABLE,mType,FALSE);

	childSetValue("icon", icon_name);

	childSetAction("Create New", LLPanelEditWearable::onBtnCreateNew, this );

	// If PG, can't take off underclothing or shirt
	mCanTakeOff =
		LLWearable::typeToAssetType( mType ) == LLAssetType::AT_CLOTHING &&
		!( gAgent.isTeen() && (mType == WT_UNDERSHIRT || mType == WT_UNDERPANTS) );
	childSetVisible("Take Off", mCanTakeOff);
	childSetAction("Take Off", LLPanelEditWearable::onBtnTakeOff, this );

	childSetAction("Save",  &LLPanelEditWearable::onBtnSave, (void*)this );

	childSetAction("Save As", &LLPanelEditWearable::onBtnSaveAs, (void*)this );

	childSetAction("Revert", &LLPanelEditWearable::onBtnRevert, (void*)this );

	return TRUE;
}

LLPanelEditWearable::~LLPanelEditWearable()
{
	std::for_each(mSubpartList.begin(), mSubpartList.end(), DeletePairedPointer());

	// Clear colorswatch commit callbacks that point to this object.
	for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
	{
		childSetCommitCallback(iter->first, NULL, NULL);		
	}
}

void LLPanelEditWearable::addSubpart( const std::string& name, ESubpart id, LLSubpart* part )
{
	if (!name.empty())
	{
		childSetAction(name, &LLPanelEditWearable::onBtnSubpart, (void*)id);
		part->mButtonName = name;
	}
	mSubpartList[id] = part;
	
}

// static
void LLPanelEditWearable::onBtnSubpart(void* userdata)
{
	LLFloaterCustomize* floater_customize = gFloaterCustomize;
	if (!floater_customize) return;
	LLPanelEditWearable* self = floater_customize->getCurrentWearablePanel();
	if (!self) return;
	ESubpart subpart = (ESubpart) (intptr_t)userdata;
	self->setSubpart( subpart );
}

void LLPanelEditWearable::setSubpart( ESubpart subpart )
{
	mCurrentSubpart = subpart;

	for (std::map<ESubpart, LLSubpart*>::iterator iter = mSubpartList.begin();
		 iter != mSubpartList.end(); ++iter)
	{
		LLButton* btn = getChild<LLButton>(iter->second->mButtonName);
		if (btn)
		{
			btn->setToggleState( subpart == iter->first );
		}
	}

	LLSubpart* part = get_if_there(mSubpartList, (ESubpart)subpart, (LLSubpart*)NULL);
	if( part )
	{
		// Update the thumbnails we display
		LLFloaterCustomize::param_map sorted_params;
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		ESex avatar_sex = avatar->getSex();
		LLViewerInventoryItem* item;
		item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(mType);
		U32 perm_mask = 0x0;
		BOOL is_complete = FALSE;
		bool can_export = false;
		bool can_import = false;
		if(item)
		{
			perm_mask = item->getPermissions().getMaskOwner();
			is_complete = item->isComplete();
			
			if (subpart <= 18) // body parts only
			{
				can_import = true;

				if (is_complete && 
					gAgent.getID() == item->getPermissions().getOwner() &&
					gAgent.getID() == item->getPermissions().getCreator() &&
					(PERM_ITEM_UNRESTRICTED &
					perm_mask) == PERM_ITEM_UNRESTRICTED)
				{
					can_export = true;
				}
			}
		}
		setUIPermissions(perm_mask, is_complete);
		BOOL editable = ((perm_mask & PERM_MODIFY) && is_complete) ? TRUE : FALSE;
		
		for(LLViewerVisualParam* param = (LLViewerVisualParam *)avatar->getFirstVisualParam(); 
			param; 
			param = (LLViewerVisualParam *)avatar->getNextVisualParam())
		{
			if (param->getID() == -1
				|| !param->isTweakable() 
				|| param->getEditGroup() != part->mEditGroup 
				|| !(param->getSex() & avatar_sex))
			{
				continue;
			}

			// negative getDisplayOrder() to make lowest order the highest priority
			LLFloaterCustomize::param_map::value_type vt(-param->getDisplayOrder(), LLFloaterCustomize::editable_param(editable, param));
			llassert( sorted_params.find(-param->getDisplayOrder()) == sorted_params.end() );  // Check for duplicates
			sorted_params.insert(vt);
		}
		gFloaterCustomize->generateVisualParamHints(NULL, sorted_params, part->mVisualHint);
		gFloaterCustomize->updateScrollingPanelUI();
		gFloaterCustomize->childSetEnabled("Export", can_export);
		gFloaterCustomize->childSetEnabled("Import", can_import);

		// Update the camera
		gMorphView->setCameraTargetJoint( gAgent.getAvatarObject()->getJoint( part->mTargetJoint ) );
		gMorphView->setCameraTargetOffset( part->mTargetOffset );
		gMorphView->setCameraOffset( part->mCameraOffset );
		gMorphView->setCameraDistToDefault();
		if (gSavedSettings.getBOOL("AppearanceCameraMovement"))
		{
			gMorphView->updateCamera();
		}
	}
}

// static
void LLPanelEditWearable::onBtnTakeOff( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	
	LLWearable* wearable = gAgent.getWearable( self->mType );
	if( !wearable )
	{
		return;
	}

	gAgent.removeWearable( self->mType );
}

// static
void LLPanelEditWearable::onBtnSave( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	gAgent.saveWearable( self->mType );
}

// static
void LLPanelEditWearable::onBtnSaveAs( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLWearable* wearable = gAgent.getWearable( self->getType() );
	if( wearable )
	{
		LLWearableSaveAsDialog* save_as_dialog = new LLWearableSaveAsDialog( wearable->getName(), onSaveAsCommit, self );
		save_as_dialog->startModal();
		// LLWearableSaveAsDialog deletes itself.
	}
}

// static
void LLPanelEditWearable::onSaveAsCommit( LLWearableSaveAsDialog* save_as_dialog, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( avatar )
	{
		gAgent.saveWearableAs( self->getType(), save_as_dialog->getItemName(), FALSE );
	}
}


// static
void LLPanelEditWearable::onBtnRevert( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	gAgent.revertWearable( self->mType );
}

// static
void LLPanelEditWearable::onBtnCreateNew( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLSD payload;
	payload["wearable_type"] = (S32)self->getType();
	LLNotifications::instance().add("AutoWearNewClothing", LLSD(), payload, &onSelectAutoWearOption);
}

bool LLPanelEditWearable::onSelectAutoWearOption(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if(avatar)
	{
		// Create a new wearable in the default folder for the wearable's asset type.
		LLWearable* wearable = gWearableList.createNewWearable( (EWearableType)notification["payload"]["wearable_type"].asInteger() );
		LLAssetType::EType asset_type = wearable->getAssetType();

		LLUUID folder_id;
		// regular UI, items get created in normal folder
		folder_id = gInventory.findCategoryUUIDForType(asset_type);

		// Only auto wear the new item if the AutoWearNewClothing checkbox is selected.
		LLPointer<LLInventoryCallback> cb = option == 0 ? 
			new WearOnAvatarCallback : NULL;
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
			folder_id, wearable->getTransactionID(), wearable->getName(), wearable->getDescription(),
			asset_type, LLInventoryType::IT_WEARABLE, wearable->getType(),
			wearable->getPermissions().getMaskNextOwner(), cb);
	}
	return false;
}

bool LLPanelEditWearable::textureIsInvisible(ETextureIndex te)
{
	if (gAgent.getWearable(mType))
	{
		LLVOAvatar *avatar = gAgent.getAvatarObject();
		if (avatar)
		{
			const LLTextureEntry* current_te = avatar->getTE(te);
			return (current_te && current_te->getID() == IMG_INVISIBLE);
		}
	}
	return false;
}

void LLPanelEditWearable::addInvisibilityCheckbox(ETextureIndex te, const std::string& name)
{
	childSetCommitCallback(name, LLPanelEditWearable::onInvisibilityCommit, this);

	mInvisibilityList[name] = te;
}

// static
void LLPanelEditWearable::onInvisibilityCommit(LLUICtrl* ctrl, void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLCheckBoxCtrl* checkbox_ctrl = (LLCheckBoxCtrl*) ctrl;
	LLVOAvatar *avatar = gAgent.getAvatarObject();
	if (!avatar)
	{
		return;
	}

	ETextureIndex te = (ETextureIndex)(self->mInvisibilityList[ctrl->getName()]);

	bool new_invis_state = checkbox_ctrl->get();
	if (new_invis_state)
	{
		LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture(IMG_INVISIBLE);
		const LLTextureEntry* current_te = avatar->getTE(te);
		if (current_te)
		{
			self->mPreviousTextureList[(S32)te] = current_te->getID();
		}
		avatar->setLocTexTE(te, image, TRUE);
		avatar->wearableUpdated(self->mType, FALSE);
	}
	else
	{
		// Try to restore previous texture, if any.
		LLUUID prev_id = self->mPreviousTextureList[(S32)te];
		if (prev_id.isNull() || (prev_id == IMG_INVISIBLE))
		{
			prev_id = LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID"));
		}
		if (prev_id.notNull())
		{
			LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture(prev_id);
			avatar->setLocTexTE(te, image, TRUE);
			avatar->wearableUpdated(self->mType, FALSE);
		}
		
	}
}

void LLPanelEditWearable::addColorSwatch( ETextureIndex te, const std::string& name )
{
	childSetCommitCallback(name, LLPanelEditWearable::onColorCommit, this);
	mColorList[name] = te;
}

// static
void LLPanelEditWearable::onColorCommit( LLUICtrl* ctrl, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLColorSwatchCtrl* color_ctrl = (LLColorSwatchCtrl*) ctrl;

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( self && color_ctrl && avatar )
	{
		std::map<std::string, S32>::const_iterator cl_itr = self->mColorList.find(ctrl->getName());
		if(cl_itr != self->mColorList.end())
		{
			ETextureIndex te = (ETextureIndex)cl_itr->second;

			LLColor4 old_color = avatar->getClothesColor( te );
			const LLColor4& new_color = color_ctrl->get();
			if( old_color != new_color )
			{
				// Set the new version
				avatar->setClothesColor( te, new_color, TRUE );

				LLVisualParamHint::requestHintUpdates();
				avatar->wearableUpdated(self->mType, FALSE);
			}
		}
	}
}

void LLPanelEditWearable::initPreviousTextureList()
{
	initPreviousTextureListEntry(TEX_LOWER_ALPHA);
	initPreviousTextureListEntry(TEX_UPPER_ALPHA);
	initPreviousTextureListEntry(TEX_HEAD_ALPHA);
	initPreviousTextureListEntry(TEX_EYES_ALPHA);
	initPreviousTextureListEntry(TEX_LOWER_ALPHA);
}

void LLPanelEditWearable::initPreviousTextureListEntry(ETextureIndex te)
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar)
	{
		return;
	}
	const LLTextureEntry* current_te = avatar->getTE(te);
	if (current_te)
	{
		mPreviousTextureList[te] = current_te->getID();
	}
}

void LLPanelEditWearable::addTextureDropTarget( ETextureIndex te, const std::string& name,
												const LLUUID& default_image_id, BOOL allow_no_texture )
{
	childSetCommitCallback(name, LLPanelEditWearable::onTextureCommit, this);
	LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>(name);
	if (texture_ctrl)
	{
		texture_ctrl->setDefaultImageAssetID(default_image_id);
		texture_ctrl->setAllowNoTexture( allow_no_texture );
		// Don't allow (no copy) or (no transfer) textures to be selected.
		texture_ctrl->setImmediateFilterPermMask(PERM_NONE);//PERM_COPY | PERM_TRANSFER);
		texture_ctrl->setNonImmediateFilterPermMask(PERM_NONE);//PERM_COPY | PERM_TRANSFER);
	}
	mTextureList[name] = te;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (avatar)
	{
		LLWearable* wearable = gAgent.getWearable(mType);
		if (wearable && mType == WT_ALPHA)
		{
			const LLTextureEntry* current_te = avatar->getTE(te);
			if (current_te)
			{
				mPreviousTextureList[te] = current_te->getID();
			}
		}
	}
}

// static
void LLPanelEditWearable::onTextureCommit( LLUICtrl* ctrl, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLTextureCtrl* texture_ctrl = (LLTextureCtrl*) ctrl;

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( avatar )
	{
		ETextureIndex te = (ETextureIndex)(self->mTextureList[ctrl->getName()]);

		// Set the new version
		LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture( texture_ctrl->getImageAssetID() );
		if (image->getID().isNull())
		{
			image = LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
		}
		self->mTextureList[ctrl->getName()] = te;
		if (gAgent.getWearable(self->mType))
		{
			avatar->setLocTexTE(te, image, TRUE);
			avatar->wearableUpdated(self->mType, FALSE);
		}
		if (self->mType == WT_ALPHA && image->getID() != IMG_INVISIBLE)
		{
			self->mPreviousTextureList[te] = image->getID();
		}
	}
}


ESubpart LLPanelEditWearable::getDefaultSubpart()
{
	switch( mType )
	{
		case WT_SHAPE:		return SUBPART_SHAPE_WHOLE;
		case WT_SKIN:		return SUBPART_SKIN_COLOR;
		case WT_HAIR:		return SUBPART_HAIR_COLOR;
		case WT_EYES:		return SUBPART_EYES;
		case WT_SHIRT:		return SUBPART_SHIRT;
		case WT_PANTS:		return SUBPART_PANTS;
		case WT_SHOES:		return SUBPART_SHOES;
		case WT_SOCKS:		return SUBPART_SOCKS;
		case WT_JACKET:		return SUBPART_JACKET;
		case WT_GLOVES:		return SUBPART_GLOVES;
		case WT_UNDERSHIRT:	return SUBPART_UNDERSHIRT;
		case WT_UNDERPANTS:	return SUBPART_UNDERPANTS;
		case WT_SKIRT:		return SUBPART_SKIRT;
		case WT_ALPHA:		return SUBPART_ALPHA;
		case WT_TATTOO:		return SUBPART_TATTOO;
		case WT_PHYSICS:	return SUBPART_PHYSICS_BELLY_UPDOWN;

		default:	llassert(0);		return SUBPART_SHAPE_WHOLE;
	}
}


void LLPanelEditWearable::draw()
{
	if( gFloaterCustomize->isMinimized() )
	{
		return;
	}

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( !avatar )
	{
		return;
	}

	LLWearable* wearable = gAgent.getWearable( mType );
	BOOL has_wearable = (wearable != NULL );
	BOOL is_dirty = isDirty();
	BOOL is_modifiable = FALSE;
	BOOL is_copyable = FALSE;
	BOOL is_complete = FALSE;
	LLViewerInventoryItem* item;
	item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(mType);
	if(item)
	{
		const LLPermissions& perm = item->getPermissions();
		is_modifiable = perm.allowModifyBy(gAgent.getID(), gAgent.getGroupID());
		is_copyable = perm.allowCopyBy(gAgent.getID(), gAgent.getGroupID());
		is_complete = item->isComplete();
	}

	childSetEnabled("Save", is_modifiable && is_complete && has_wearable && is_dirty);
	childSetEnabled("Save As", is_copyable && is_complete && has_wearable);
	childSetEnabled("Revert", has_wearable && is_dirty );
	childSetEnabled("Take Off",  has_wearable );
	childSetVisible("Take Off", mCanTakeOff && has_wearable  );
	childSetVisible("Create New", !has_wearable );

	childSetVisible("not worn instructions",  !has_wearable );
	childSetVisible("no modify instructions",  has_wearable && !is_modifiable);

	for (std::map<ESubpart, LLSubpart*>::iterator iter = mSubpartList.begin();
		 iter != mSubpartList.end(); ++iter)
	{
		childSetVisible(iter->second->mButtonName,has_wearable);
		if( has_wearable && is_complete && is_modifiable )
		{
			childSetEnabled(iter->second->mButtonName, iter->second->mSex & avatar->getSex() );
		}
		else
		{
			childSetEnabled(iter->second->mButtonName, FALSE );
		}
	}

	childSetVisible("square", !is_modifiable);

	childSetVisible("title", FALSE);
	childSetVisible("title_no_modify", FALSE);
	childSetVisible("title_not_worn", FALSE);
	childSetVisible("title_loading", FALSE);

	childSetVisible("path", FALSE);

	LLTextBox *av_height = getChild<LLTextBox>("avheight",FALSE,FALSE);
	if(av_height) //Only display this if the element exists
	{
		// Display the shape's nominal height.
		//
		// The value for avsize is the best available estimate from
		// measuring against prims.
		float avsize = avatar->mBodySize.mV[VZ] + .195;
		int inches = (int)(avsize / .0254f);
		int feet = inches / 12;
		inches %= 12;

		std::ostringstream avheight(std::ostringstream::trunc);
		avheight << std::fixed << std::setprecision(2) << avsize << " m ("
			<< feet << "' " << inches << "\")";
		av_height->setVisible(TRUE);
		av_height->setTextArg("[AVHEIGHT]",avheight.str());		
	}
	
	if(has_wearable && !is_modifiable)
	{
		// *TODO:Translate
		childSetVisible("title_no_modify", TRUE);
		childSetTextArg("title_no_modify", "[DESC]", std::string(LLWearable::typeToTypeLabel( mType )));
		
		hideTextureControls();
	}
	else if(has_wearable && !is_complete)
	{
		// *TODO:Translate
		childSetVisible("title_loading", TRUE);
		childSetTextArg("title_loading", "[DESC]", std::string(LLWearable::typeToTypeLabel( mType )));
			
		std::string path;
		const LLUUID& item_id = gAgent.getWearableItem( wearable->getType() );
		gInventory.appendPath(item_id, path);
		childSetVisible("path", TRUE);
		childSetTextArg("path", "[PATH]", path);

		hideTextureControls();
	}
	else if(has_wearable && is_modifiable)
	{
		childSetVisible("title", TRUE);
		childSetTextArg("title", "[DESC]", wearable->getName() );

		std::string path;
		const LLUUID& item_id = gAgent.getWearableItem( wearable->getType() );
		gInventory.appendPath(item_id, path);
		childSetVisible("path", TRUE);
		childSetTextArg("path", "[PATH]", path);

		for( std::map<std::string, S32>::iterator iter = mTextureList.begin();
			 iter != mTextureList.end(); ++iter )
		{
			std::string name = iter->first;
			LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>(name);
			S32 te_index = iter->second;
			childSetVisible(name, is_copyable && is_modifiable && is_complete );
			if (texture_ctrl)
			{
				const LLTextureEntry* te = avatar->getTE(te_index);

				LLUUID new_id;
				
				if( te && (te->getID() != IMG_DEFAULT_AVATAR) )
				{
					new_id = te->getID();
				}
				else
				{
					new_id = LLUUID::null;
				}

				LLUUID old_id = texture_ctrl->getImageAssetID();

				if (old_id != new_id)
				{
					// texture has changed, close the floater to avoid DEV-22461
					texture_ctrl->closeFloater();
				}
				
				texture_ctrl->setImageAssetID(new_id);
			}
		}

		for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
		{
			std::string name = iter->first;
			S32 te_index = iter->second;
			childSetVisible(name, is_modifiable && is_complete );
			childSetEnabled(name, is_modifiable && is_complete );
			LLColorSwatchCtrl* ctrl = getChild<LLColorSwatchCtrl>(name);
			if (ctrl)
			{
				ctrl->set(avatar->getClothesColor( (ETextureIndex)te_index ) );
			}
		}

		for (std::map<std::string, S32>::iterator iter = mInvisibilityList.begin();
			 iter != mInvisibilityList.end(); ++iter)
		{
			std::string name = iter->first;
			ETextureIndex te = (ETextureIndex)iter->second;
			childSetVisible(name, is_copyable && is_modifiable && is_complete);
			childSetEnabled(name, is_copyable && is_modifiable && is_complete);
			LLCheckBoxCtrl* ctrl = getChild<LLCheckBoxCtrl>(name);
			if (ctrl)
			{
				ctrl->set(textureIsInvisible(te));
			}
		}

		for (std::map<std::string, S32>::iterator iter = mInvisibilityList.begin();
			 iter != mInvisibilityList.end(); ++iter)
		{
			std::string name = iter->first;
			ETextureIndex te = (ETextureIndex)iter->second;
			childSetVisible(name, is_copyable && is_modifiable && is_complete);
			childSetEnabled(name, is_copyable && is_modifiable && is_complete);
			LLCheckBoxCtrl* ctrl = getChild<LLCheckBoxCtrl>(name);
			if (ctrl)
			{
				ctrl->set(textureIsInvisible(te));
			}
		}
	}
	else
	{
		// *TODO:Translate
		childSetVisible("title_not_worn", TRUE);
		childSetTextArg("title_not_worn", "[DESC]", std::string(LLWearable::typeToTypeLabel( mType )));

		hideTextureControls();
	}
	
	childSetVisible("icon", has_wearable && is_modifiable);

	LLPanel::draw();
}

void LLPanelEditWearable::hideTextureControls()
{
	for (std::map<std::string, S32>::iterator iter = mTextureList.begin();
			 iter != mTextureList.end(); ++iter)
	{
		childSetVisible(iter->first, FALSE);
	}
	for (std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter)
	{
		childSetVisible(iter->first, FALSE);
	}
	for (std::map<std::string, S32>::iterator iter = mInvisibilityList.begin();
		 iter != mInvisibilityList.end(); ++iter)
	{
		childSetVisible(iter->first, FALSE);
	}
}

void LLPanelEditWearable::setWearable(LLWearable* wearable, U32 perm_mask, BOOL is_complete)
{
	if( wearable )
	{
		setUIPermissions(perm_mask, is_complete);
		if (mType == WT_ALPHA)
		{
			initPreviousTextureList();
		}
	}
}

void LLPanelEditWearable::switchToDefaultSubpart()
{
	setSubpart( getDefaultSubpart() );
}

void LLPanelEditWearable::setVisible(BOOL visible)
{
	LLPanel::setVisible( visible );
	if( !visible )
	{
		for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
		{
			// this forces any open color pickers to cancel their selection
			childSetEnabled(iter->first, FALSE );
		}
	}
}

BOOL LLPanelEditWearable::isDirty() const
{
	LLWearable* wearable = gAgent.getWearable( mType );
	if( !wearable )
	{
		return FALSE;
	}

	if( wearable->isDirty() )
	{
		return TRUE;
	}

	return FALSE;
}

// static
void LLPanelEditWearable::onCommitSexChange( LLUICtrl*, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar)
	{
		return;
	}

	if( !gAgent.isWearableModifiable(self->mType))
	{
		return;
	}

	ESex new_sex = gSavedSettings.getU32("AvatarSex") ? SEX_MALE : SEX_FEMALE;

	LLViewerVisualParam* param = (LLViewerVisualParam*)avatar->getVisualParam( "male" );
	if( !param )
	{
		return;
	}

	param->setWeight( (new_sex == SEX_MALE), TRUE );

	avatar->updateSexDependentLayerSets( TRUE );

	avatar->updateVisualParams();

	gFloaterCustomize->clearScrollingPanelList();

	// Assumes that we're in the "Shape" Panel.
	self->setSubpart( SUBPART_SHAPE_WHOLE );
}

void LLPanelEditWearable::setUIPermissions(U32 perm_mask, BOOL is_complete)
{
	BOOL is_copyable = (perm_mask & PERM_COPY) ? TRUE : FALSE;
	BOOL is_modifiable = (perm_mask & PERM_MODIFY) ? TRUE : FALSE;

	childSetEnabled("Save", is_modifiable && is_complete);
	childSetEnabled("Save As", is_copyable && is_complete);
	childSetEnabled("sex radio", is_modifiable && is_complete);
	for( std::map<std::string, S32>::iterator iter = mTextureList.begin();
		 iter != mTextureList.end(); ++iter )
	{
		childSetVisible(iter->first, is_copyable && is_modifiable && is_complete );
	}
	for( std::map<std::string, S32>::iterator iter = mColorList.begin();
		 iter != mColorList.end(); ++iter )
	{
		childSetVisible(iter->first, is_modifiable && is_complete );
	}
	for (std::map<std::string, S32>::iterator iter = mInvisibilityList.begin();
		 iter != mInvisibilityList.end(); ++iter)
	{
		childSetVisible(iter->first, is_copyable && is_modifiable && is_complete);
	}
	for (std::map<std::string, S32>::iterator iter = mInvisibilityList.begin();
		 iter != mInvisibilityList.end(); ++iter)
	{
		childSetVisible(iter->first, is_copyable && is_modifiable && is_complete);
	}
}

/////////////////////////////////////////////////////////////////////
// LLScrollingPanelParam

class LLScrollingPanelParam : public LLScrollingPanel
{
public:
	LLScrollingPanelParam( const std::string& name, LLViewerJointMesh* mesh, LLViewerVisualParam* param, BOOL allow_modify, bool bVisualHint );
	virtual ~LLScrollingPanelParam();

	virtual void		draw();
	virtual void		setVisible( BOOL visible );
	virtual void		updatePanel(BOOL allow_modify);

	static void			onSliderMouseDown(LLUICtrl* ctrl, void* userdata);
	static void			onSliderMoved(LLUICtrl* ctrl, void* userdata);
	static void			onSliderMouseUp(LLUICtrl* ctrl, void* userdata);

	static void			onHintMinMouseDown(void* userdata);
	static void			onHintMinHeldDown(void* userdata);
	static void			onHintMaxMouseDown(void* userdata);
	static void			onHintMaxHeldDown(void* userdata);
	static void			onHintMinMouseUp(void* userdata);
	static void			onHintMaxMouseUp(void* userdata);

	void				onHintMouseDown( LLVisualParamHint* hint );
	void				onHintHeldDown( LLVisualParamHint* hint );

	F32					weightToPercent( F32 weight );
	F32					percentToWeight( F32 percent );

public:
	LLViewerVisualParam* mParam;
	LLPointer<LLVisualParamHint>	mHintMin;
	LLPointer<LLVisualParamHint>	mHintMax;
	LLButton*						mLess;
	LLButton*						mMore;

	static S32 			sUpdateDelayFrames;
	
protected:
	LLTimer				mMouseDownTimer;	// timer for how long mouse has been held down on a hint.
	F32					mLastHeldTime;

	BOOL mAllowModify;
};

//static
S32 LLScrollingPanelParam::sUpdateDelayFrames = 0;

const S32 BTN_BORDER = 2;
const S32 PARAM_HINT_WIDTH = 128;
const S32 PARAM_HINT_HEIGHT = 128;
const S32 PARAM_HINT_LABEL_HEIGHT = 16;
const S32 PARAM_PANEL_WIDTH = 2 * (3* BTN_BORDER + PARAM_HINT_WIDTH +  LLPANEL_BORDER_WIDTH);
const S32 PARAM_PANEL_HEIGHT = 2 * BTN_BORDER + PARAM_HINT_HEIGHT + PARAM_HINT_LABEL_HEIGHT + 4 * LLPANEL_BORDER_WIDTH; 

LLScrollingPanelParam::LLScrollingPanelParam( const std::string& name,
											  LLViewerJointMesh* mesh, LLViewerVisualParam* param, BOOL allow_modify, bool bVisualHint )
	: LLScrollingPanel( name, LLRect( 0, PARAM_PANEL_HEIGHT, PARAM_PANEL_WIDTH, 0 ) ),
	  mParam(param),
	  mAllowModify(allow_modify),
	  mLess(NULL),
	  mMore(NULL)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_scrolling_param.xml");
	
	//Set up the slider
	LLSliderCtrl *slider = getChild<LLSliderCtrl>("param slider");
	slider->setValue(weightToPercent(param->getWeight()));
	slider->setLabelArg("[DESC]", param->getDisplayName());
	slider->setEnabled(mAllowModify);
	slider->setCommitCallback(LLScrollingPanelParam::onSliderMoved);
	slider->setCallbackUserData(this);

	if(bVisualHint)
	{
		S32 pos_x = 2 * LLPANEL_BORDER_WIDTH;
		S32 pos_y = 3 * LLPANEL_BORDER_WIDTH + SLIDERCTRL_HEIGHT;
		F32 min_weight = param->getMinWeight();
		F32 max_weight = param->getMaxWeight();

		mHintMin = new LLVisualParamHint( pos_x, pos_y, PARAM_HINT_WIDTH, PARAM_HINT_HEIGHT, mesh, param,  min_weight);
		pos_x += PARAM_HINT_WIDTH + 3 * BTN_BORDER;
		mHintMax = new LLVisualParamHint( pos_x, pos_y, PARAM_HINT_WIDTH, PARAM_HINT_HEIGHT, mesh, param, max_weight );

		mHintMin->setAllowsUpdates( FALSE );
		mHintMax->setAllowsUpdates( FALSE );

		// *TODO::translate
		std::string min_name = param->getMinDisplayName();
		std::string max_name = param->getMaxDisplayName();
		childSetValue("min param text", min_name);
		childSetValue("max param text", max_name);
		mLess = getChild<LLButton>("less");
		mLess->setMouseDownCallback( LLScrollingPanelParam::onHintMinMouseDown );
		mLess->setMouseUpCallback( LLScrollingPanelParam::onHintMinMouseUp );
		mLess->setHeldDownCallback( LLScrollingPanelParam::onHintMinHeldDown );
		mLess->setHeldDownDelay( PARAM_STEP_TIME_THRESHOLD );

		mMore = getChild<LLButton>("more");
		mMore->setMouseDownCallback( LLScrollingPanelParam::onHintMaxMouseDown );
		mMore->setMouseUpCallback( LLScrollingPanelParam::onHintMaxMouseUp );
		mMore->setHeldDownCallback( LLScrollingPanelParam::onHintMaxHeldDown );
		mMore->setHeldDownDelay( PARAM_STEP_TIME_THRESHOLD );
	}
	else
	{
		//Kill everything that isn't the slider...
		child_list_t to_remove;
		child_list_t::const_iterator it;
		for (it = getChildList()->begin(); it != getChildList()->end(); it++)
		{
			if ((*it) != slider && (*it)->getName() != "panel border")
			{
				to_remove.push_back(*it);
			}
		}
		for (it = to_remove.begin(); it != to_remove.end(); it++)
		{
			removeChild(*it, TRUE);
		}
		slider->translate(0,PARAM_HINT_HEIGHT);
		reshape(getRect().getWidth(),getRect().getHeight()-PARAM_HINT_HEIGHT);
	}

	setVisible(FALSE);
	setBorderVisible( FALSE );
}

LLScrollingPanelParam::~LLScrollingPanelParam()
{
	mHintMin = NULL;
	mHintMax = NULL;
}

void LLScrollingPanelParam::updatePanel(BOOL allow_modify)
{
	childSetValue("param slider", weightToPercent( mParam->getWeight() ) );
	if(mHintMin)
		mHintMin->requestUpdate( sUpdateDelayFrames++ );
	if(mHintMax)
		mHintMax->requestUpdate( sUpdateDelayFrames++ );

	mAllowModify = allow_modify;
	childSetEnabled("param slider", mAllowModify);

	if(mLess)
		mLess->setEnabled(mAllowModify);
	if(mMore)
		mMore->setEnabled(mAllowModify);
}

void LLScrollingPanelParam::setVisible( BOOL visible )
{
	if( getVisible() != visible )
	{
		LLPanel::setVisible( visible );
		if(mHintMin)
			mHintMin->setAllowsUpdates( visible );
		if(mHintMax)
			mHintMax->setAllowsUpdates( visible );

		if( visible )
		{
			if(mHintMin)
				mHintMin->setUpdateDelayFrames( sUpdateDelayFrames++ );
			if(mHintMax)
				mHintMax->setUpdateDelayFrames( sUpdateDelayFrames++ );
		}
	}
}

void LLScrollingPanelParam::draw()
{
	if( gFloaterCustomize->isMinimized() )
	{
		return;
	}
	
	if(mLess)
		mLess->setVisible(mHintMin ? mHintMin->getVisible() : false);
	if(mMore)
		mMore->setVisible(mHintMax ? mHintMax->getVisible() : false);

	// Draw all the children except for the labels
	childSetVisible( "min param text", FALSE );
	childSetVisible( "max param text", FALSE );
	LLPanel::draw();

	// Draw the hints over the "less" and "more" buttons.
	if(mHintMin)
	{
		glPushMatrix();
		{
			const LLRect& r = mHintMin->getRect();
			F32 left = (F32)(r.mLeft + BTN_BORDER);
			F32 bot  = (F32)(r.mBottom + BTN_BORDER);
			glTranslatef(left, bot, 0.f);
			mHintMin->draw();
		}
		glPopMatrix();
	}

	if(mHintMax)
	{
		glPushMatrix();
		{
			const LLRect& r = mHintMax->getRect();
			F32 left = (F32)(r.mLeft + BTN_BORDER);
			F32 bot  = (F32)(r.mBottom + BTN_BORDER);
			glTranslatef(left, bot, 0.f);
			mHintMax->draw();
		}
		glPopMatrix();
	}


	// Draw labels on top of the buttons
	childSetVisible( "min param text", TRUE );
	drawChild(getChild<LLView>("min param text"), BTN_BORDER, BTN_BORDER);

	childSetVisible( "max param text", TRUE );
	drawChild(getChild<LLView>("max param text"), BTN_BORDER, BTN_BORDER);
}

void updateAvatarHeightDisplay()
{
       if (gFloaterCustomize)
        {
               LLVOAvatar* avatar = gAgent.getAvatarObject();
               F32 avatar_size = (avatar->mBodySize.mV[VZ]) + (F32)0.17; //mBodySize is actually quite a bit off.
               gFloaterCustomize->getChild<LLTextBox>("HeightTextM")->setValue(llformat("%.2f", avatar_size) + "m");
               F32 feet = avatar_size / 0.3048;
               F32 inches = (feet - (F32)((U32)feet)) * 12.0;
               gFloaterCustomize->getChild<LLTextBox>("HeightTextI")->setValue(llformat("%d'%d\"", (U32)feet, (U32)inches));
        }
 }

// static
void LLScrollingPanelParam::onSliderMoved(LLUICtrl* ctrl, void* userdata)
{
	LLSliderCtrl* slider = (LLSliderCtrl*) ctrl;
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	LLViewerVisualParam* param = self->mParam;

	F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( param );
	F32 new_weight = self->percentToWeight( (F32)slider->getValue().asReal() );
	if (current_weight != new_weight )
	{
		updateAvatarHeightDisplay();
		gAgent.getAvatarObject()->setVisualParamWeight( param, new_weight, FALSE);
		gAgent.getAvatarObject()->updateVisualParams();
	}
}

// static
void LLScrollingPanelParam::onSliderMouseDown(LLUICtrl* ctrl, void* userdata)
{
}

// static
void LLScrollingPanelParam::onSliderMouseUp(LLUICtrl* ctrl, void* userdata)
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;

	LLVisualParamHint::requestHintUpdates( self->mHintMin, self->mHintMax );
}

// static
void LLScrollingPanelParam::onHintMinMouseDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintMouseDown( self->mHintMin );
}

// static
void LLScrollingPanelParam::onHintMaxMouseDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintMouseDown( self->mHintMax );
}


void LLScrollingPanelParam::onHintMouseDown( LLVisualParamHint* hint )
{
	// morph towards this result
	F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );

	// if we have maxed out on this morph, we shouldn't be able to click it
	if( hint->getVisualParamWeight() != current_weight )
	{
		mMouseDownTimer.reset();
		mLastHeldTime = 0.f;
	}
}

// static
void LLScrollingPanelParam::onHintMinHeldDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintHeldDown( self->mHintMin );
}

// static
void LLScrollingPanelParam::onHintMaxHeldDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintHeldDown( self->mHintMax );
}
	
void LLScrollingPanelParam::onHintHeldDown( LLVisualParamHint* hint )
{
	F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );

	if (current_weight != hint->getVisualParamWeight() )
	{
		const F32 FULL_BLEND_TIME = 2.f;
		F32 elapsed_time = mMouseDownTimer.getElapsedTimeF32() - mLastHeldTime;
		mLastHeldTime += elapsed_time;

		F32 new_weight;
		if (current_weight > hint->getVisualParamWeight() )
		{
			new_weight = current_weight - (elapsed_time / FULL_BLEND_TIME);
		}
		else
		{
			new_weight = current_weight + (elapsed_time / FULL_BLEND_TIME);
		}

		// Make sure we're not taking the slider out of bounds
		// (this is where some simple UI limits are stored)
		F32 new_percent = weightToPercent(new_weight);
		LLSliderCtrl* slider = getChild<LLSliderCtrl>("param slider");
		if (slider)
		{
			if (slider->getMinValue() < new_percent
				&& new_percent < slider->getMaxValue())
			{
				gAgent.getAvatarObject()->setVisualParamWeight( hint->getVisualParam(), new_weight, TRUE);
				gAgent.getAvatarObject()->updateVisualParams();

				slider->setValue( weightToPercent( new_weight ) );
			}
		}
	}
}

// static
void LLScrollingPanelParam::onHintMinMouseUp( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;

	F32 elapsed_time = self->mMouseDownTimer.getElapsedTimeF32();

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (avatar)
	{
		LLVisualParamHint* hint = self->mHintMin;

		if (elapsed_time < PARAM_STEP_TIME_THRESHOLD)
		{
			// step in direction
			F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );
			F32 range = self->mHintMax->getVisualParamWeight() - self->mHintMin->getVisualParamWeight();
			// step a fraction in the negative directiona
			F32 new_weight = current_weight - (range / 10.f);
			F32 new_percent = self->weightToPercent(new_weight);
			LLSliderCtrl* slider = self->getChild<LLSliderCtrl>("param slider");
			if (slider)
			{
				if (slider->getMinValue() < new_percent
					&& new_percent < slider->getMaxValue())
				{
					avatar->setVisualParamWeight(hint->getVisualParam(), new_weight, TRUE);
					slider->setValue( self->weightToPercent( new_weight ) );
				}
			}
		}
	}

	LLVisualParamHint::requestHintUpdates( self->mHintMin, self->mHintMax );
}

void LLScrollingPanelParam::onHintMaxMouseUp( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;

	F32 elapsed_time = self->mMouseDownTimer.getElapsedTimeF32();

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (avatar)
	{
		LLVisualParamHint* hint = self->mHintMax;

		if (elapsed_time < PARAM_STEP_TIME_THRESHOLD)
		{
			// step in direction
			F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );
			F32 range = self->mHintMax->getVisualParamWeight() - self->mHintMin->getVisualParamWeight();
			// step a fraction in the negative direction
			F32 new_weight = current_weight + (range / 10.f);
			F32 new_percent = self->weightToPercent(new_weight);
			LLSliderCtrl* slider = self->getChild<LLSliderCtrl>("param slider");
			if (slider)
			{
				if (slider->getMinValue() < new_percent
					&& new_percent < slider->getMaxValue())
				{
					avatar->setVisualParamWeight(hint->getVisualParam(), new_weight, TRUE);
					slider->setValue( self->weightToPercent( new_weight ) );
				}
			}
		}
	}

	LLVisualParamHint::requestHintUpdates( self->mHintMin, self->mHintMax );
}


F32 LLScrollingPanelParam::weightToPercent( F32 weight )
{
	LLViewerVisualParam* param = mParam;
	return (weight - param->getMinWeight()) /  (param->getMaxWeight() - param->getMinWeight()) * 100.f;
}

F32 LLScrollingPanelParam::percentToWeight( F32 percent )
{
	LLViewerVisualParam* param = mParam;
	return percent / 100.f * (param->getMaxWeight() - param->getMinWeight()) + param->getMinWeight();
}

const std::string& LLFloaterCustomize::getEditGroup()
{
	return getCurrentWearablePanel()->getCurrentSubpart()->mEditGroup;
}


/////////////////////////////////////////////////////////////////////
// LLFloaterCustomize

// statics
EWearableType	LLFloaterCustomize::sCurrentWearableType = WT_INVALID;

struct WearablePanelData
{
	WearablePanelData(LLFloaterCustomize* floater, EWearableType type)
		: mFloater(floater), mType(type) {}
	LLFloaterCustomize* mFloater;
	EWearableType mType;
};

LLFloaterCustomize::LLFloaterCustomize()
:	LLFloater(std::string("customize")),
	mScrollingPanelList( NULL ),
	mInventoryObserver(NULL),
	mNextStepAfterSaveCallback( NULL ),
	mNextStepAfterSaveUserdata( NULL )
{
	memset(&mWearablePanelList[0],0,sizeof(char*)*WT_COUNT); //Initialize to 0

	gSavedSettings.setU32("AvatarSex", (gAgent.getAvatarObject()->getSex() == SEX_MALE) );

	mResetParams = new LLVisualParamReset();
	
	// create the observer which will watch for matching incoming inventory
	mInventoryObserver = new LLFloaterCustomizeObserver(this);
	gInventory.addObserver(mInventoryObserver);

	LLCallbackMap::map_t factory_map;
	factory_map["Shape"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SHAPE) ) );
	factory_map["Skin"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SKIN) ) );
	factory_map["Hair"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_HAIR) ) );
	factory_map["Eyes"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_EYES) ) );
	factory_map["Shirt"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SHIRT) ) );
	factory_map["Pants"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_PANTS) ) );
	factory_map["Shoes"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SHOES) ) );
	factory_map["Socks"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SOCKS) ) );
	factory_map["Jacket"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_JACKET) ) );
	factory_map["Gloves"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_GLOVES) ) );
	factory_map["Undershirt"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_UNDERSHIRT) ) );
	factory_map["Underpants"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_UNDERPANTS) ) );
	factory_map["Skirt"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SKIRT) ) );
	factory_map["Alpha"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_ALPHA)));
	factory_map["Tattoo"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_TATTOO)));
	factory_map["Physics"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_PHYSICS)));

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_customize.xml", &factory_map);
}

BOOL LLFloaterCustomize::postBuild()
{
	childSetAction("Make Outfit", LLFloaterCustomize::onBtnMakeOutfit, (void*)this);
	childSetAction("Ok", LLFloaterCustomize::onBtnOk, (void*)this);
	childSetAction("Cancel", LLFloater::onClickClose, (void*)this);

    // reX
	childSetAction("Import", LLFloaterCustomize::onBtnImport, (void*)this);
	childSetAction("Export", LLFloaterCustomize::onBtnExport, (void*)this);
	
	// Wearable panels
	initWearablePanels();

	// Tab container
	childSetTabChangeCallback("customize tab container", "Shape", onTabChanged, (void*)WT_SHAPE, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Skin", onTabChanged, (void*)WT_SKIN, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Hair", onTabChanged, (void*)WT_HAIR, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Eyes", onTabChanged, (void*)WT_EYES, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Shirt", onTabChanged, (void*)WT_SHIRT, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Pants", onTabChanged, (void*)WT_PANTS, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Shoes", onTabChanged, (void*)WT_SHOES, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Socks", onTabChanged, (void*)WT_SOCKS, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Jacket", onTabChanged, (void*)WT_JACKET, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Gloves", onTabChanged, (void*)WT_GLOVES, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Undershirt", onTabChanged, (void*)WT_UNDERSHIRT, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Underpants", onTabChanged, (void*)WT_UNDERPANTS, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Skirt", onTabChanged, (void*)WT_SKIRT, onTabPrecommit );
	childSetTabChangeCallback("customize tab container", "Alpha", onTabChanged, (void*)WT_ALPHA, onTabPrecommit);
	childSetTabChangeCallback("customize tab container", "Tattoo", onTabChanged, (void*)WT_TATTOO, onTabPrecommit);
	childSetTabChangeCallback("customize tab container", "Physics", onTabChanged, (void*)WT_PHYSICS, onTabPrecommit);

	// Remove underwear panels for teens
	if (gAgent.isTeen())
	{
		LLTabContainer* tab_container = getChild<LLTabContainer>("customize tab container");
		if (tab_container)
		{
			LLPanel* panel;
			panel = tab_container->getPanelByName("Undershirt");
			if (panel) tab_container->removeTabPanel(panel);
			panel = tab_container->getPanelByName("Underpants");
			if (panel) tab_container->removeTabPanel(panel);
		}
	}
	
	// Scrolling Panel
	initScrollingPanelList();
	
	return TRUE;
}

void LLFloaterCustomize::open()
{
	LLFloater::open();
	// childShowTab depends on gFloaterCustomize being defined and therefore must be called after the constructor. - Nyx
	childShowTab("customize tab container", "Shape", true);
	setCurrentWearableType(WT_SHAPE);
}

////////////////////////////////////////////////////////////////////////////

// static
void LLFloaterCustomize::setCurrentWearableType( EWearableType type )
{
	if( LLFloaterCustomize::sCurrentWearableType != type )
	{
		LLFloaterCustomize::sCurrentWearableType = type; 

		S32 type_int = (S32)type;
		if( gFloaterCustomize
			&& gFloaterCustomize->mWearablePanelList[type_int])
		{
			std::string panelname = gFloaterCustomize->mWearablePanelList[type_int]->getName();
			gFloaterCustomize->childShowTab("customize tab container", panelname);
			gFloaterCustomize->switchToDefaultSubpart();
		}
	}
}

// reX: new function
void LLFloaterCustomize::onBtnImport( void* userdata )
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(FFLOAD_XML);
	filepicker->run(boost::bind(&LLFloaterCustomize::onBtnImport_continued, filepicker));
}

void LLFloaterCustomize::onBtnImport_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
	{
		// User canceled import.
		return;
	}

	const std::string filename = filepicker->getFilename();

	FILE* fp = LLFile::fopen(filename, "rb");

	//char text_buffer[2048];		/* Flawfinder: ignore */
	S32 c;
	S32 typ;
	S32 count;
	S32 param_id=0;
	F32 param_weight=0;
	S32 fields_read;

	for( S32 i=0; i < WT_COUNT; i++ )
	{
		fields_read = fscanf( fp, "type %d\n", &typ);
		if( fields_read != 1 )
		{
			llwarns << "Bad asset type: early end of file" << llendl;
			return;
		}

		fields_read = fscanf( fp, "parameters %d\n", &count);
		if( fields_read != 1 )
		{
			llwarns << "Bad parameters : early end of file" << llendl;
			return;
		}
		for(c=0;c<count;c++)
		{
			fields_read = fscanf( fp, "%d %f\n", &param_id, &param_weight );
			if( fields_read != 2 )
			{
				llwarns << "Bad parameters list: early end of file" << llendl;
				return;
			}
			gAgent.getAvatarObject()->setVisualParamWeight( param_id, param_weight, TRUE);
			gAgent.getAvatarObject()->updateVisualParams();
		}
	}

	fclose(fp);
	return;
}

// reX: new function
void LLFloaterCustomize::onBtnExport( void* userdata )
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open("", FFSAVE_XML);
	filepicker->run(boost::bind(&LLFloaterCustomize::onBtnExport_continued, filepicker));
}

void LLFloaterCustomize::onBtnExport_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
	{
		// User canceled export.
		return;
	}

	LLViewerInventoryItem* item;
	BOOL is_modifiable;

	const std::string filename = filepicker->getFilename();

	FILE* fp = LLFile::fopen(filename, "wb");

	for( S32 i=0; i < WT_COUNT; i++ )
	{
		is_modifiable = FALSE;
		LLWearable* old_wearable = gAgent.getWearable((EWearableType)i);
		if( old_wearable )
		{
			item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem((EWearableType)i);
			if(item)
			{
				const LLPermissions& perm = item->getPermissions();
				is_modifiable = perm.allowModifyBy(gAgent.getID(), gAgent.getGroupID());
			}
		}
		if (is_modifiable)
		{
			old_wearable->FileExportParams(fp);
		}
		if (!is_modifiable)
		{
			fprintf( fp, "type %d\n",i);
			fprintf( fp, "parameters 0\n");
		}
	}	

	for( S32 i=0; i < WT_COUNT; i++ )
	{
		is_modifiable = FALSE;
		LLWearable* old_wearable = gAgent.getWearable((EWearableType)i);
		if( old_wearable )
		{
			item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem((EWearableType)i);
			if(item)
			{
				const LLPermissions& perm = item->getPermissions();
				is_modifiable = perm.allowModifyBy(gAgent.getID(), gAgent.getGroupID());
			}
		}
		if (is_modifiable)
		{
			old_wearable->FileExportTextures(fp);
		}
		if (!is_modifiable)
		{
			fprintf( fp, "type %d\n",i);
			fprintf( fp, "textures 0\n");
		}
	}	

	fclose(fp);
}

// static
void LLFloaterCustomize::onBtnOk( void* userdata )
{
	LLFloaterCustomize* floater = (LLFloaterCustomize*) userdata;
	gAgent.saveAllWearables();

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if ( avatar )
	{
		avatar->invalidateAll();
		
		avatar->requestLayerSetUploads();

		gAgent.sendAgentSetAppearance();
	}

	gFloaterView->sendChildToBack(floater);
	handle_reset_view();  // Calls askToSaveIfDirty
}

// static
void LLFloaterCustomize::onBtnMakeOutfit( void* userdata )
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if(avatar)
	{
		LLMakeOutfitDialog* dialog = new LLMakeOutfitDialog( onMakeOutfitCommit, NULL );
		// LLMakeOutfitDialog deletes itself.

		for( S32 i = 0; i < WT_COUNT; i++ )
		{
			BOOL enabled = (gAgent.getWearable( (EWearableType) i ) != NULL);
			BOOL selected = (enabled && (WT_SHIRT <= i) && (i < WT_COUNT)); // only select clothing by default
			if (gAgent.isTeen()
				&& !edit_wearable_for_teens((EWearableType)i))
			{
				dialog->setWearableToInclude( i, FALSE, FALSE );
			}
			else
			{
				dialog->setWearableToInclude( i, enabled, selected );
			}
		}
		dialog->startModal();
	}
}

// static
void LLFloaterCustomize::onMakeOutfitCommit( LLMakeOutfitDialog* dialog, void* userdata )
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if(avatar)
	{
		LLDynamicArray<S32> wearables_to_include;
		LLDynamicArray<S32> attachments_to_include;  // attachment points

		dialog->getIncludedItems( wearables_to_include, attachments_to_include );

		gAgent.makeNewOutfit( dialog->getFolderName(), wearables_to_include, attachments_to_include, dialog->getRenameClothing() );
	}
}

////////////////////////////////////////////////////////////////////////////

// static
void* LLFloaterCustomize::createWearablePanel(void* userdata)
{
	WearablePanelData* data = (WearablePanelData*)userdata;
	EWearableType type = data->mType;
	LLPanelEditWearable* panel;
	if ((gAgent.isTeen() && !edit_wearable_for_teens(data->mType) ))
	{
		panel = NULL;
	}
	else
	{
		panel = new LLPanelEditWearable( type );
	}
	data->mFloater->mWearablePanelList[type] = panel;
	delete data;
	return panel;
}

void LLFloaterCustomize::initWearablePanels()
{
	LLSubpart* part;
	
	/////////////////////////////////////////
	// Shape
	LLPanelEditWearable* panel = mWearablePanelList[ WT_SHAPE ];

	// body
	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "shape_body";
	part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
	part->mCameraOffset.setVec(-2.5f, 0.5f, 0.8f);
	panel->addSubpart( "Body", SUBPART_SHAPE_WHOLE, part );

	// head supparts
	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_head";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Head", SUBPART_SHAPE_HEAD, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_eyes";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Eyes", SUBPART_SHAPE_EYES, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_ears";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Ears", SUBPART_SHAPE_EARS, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_nose";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Nose", SUBPART_SHAPE_NOSE, part );


	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_mouth";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Mouth", SUBPART_SHAPE_MOUTH, part );


	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_chin";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Chin", SUBPART_SHAPE_CHIN, part );

	// torso
	part = new LLSubpart();
	part->mTargetJoint = "mTorso";
	part->mEditGroup = "shape_torso";
	part->mTargetOffset.setVec(0.f, 0.f, 0.3f);
	part->mCameraOffset.setVec(-1.f, 0.15f, 0.3f);
	panel->addSubpart( "Torso", SUBPART_SHAPE_TORSO, part );

	// legs
	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "shape_legs";
	part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
	part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
	panel->addSubpart( "Legs", SUBPART_SHAPE_LEGS, part );

	panel->childSetCommitCallback("sex radio", LLPanelEditWearable::onCommitSexChange, panel);

	/////////////////////////////////////////
	// Skin
	panel = mWearablePanelList[ WT_SKIN ];

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "skin_color";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Skin Color", SUBPART_SKIN_COLOR, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "skin_facedetail";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Face Detail", SUBPART_SKIN_FACEDETAIL, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "skin_makeup";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Makeup", SUBPART_SKIN_MAKEUP, part );

	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "skin_bodydetail";
	part->mTargetOffset.setVec(0.f, 0.f, -0.2f);
	part->mCameraOffset.setVec(-2.5f, 0.5f, 0.5f);
	panel->addSubpart( "Body Detail", SUBPART_SKIN_BODYDETAIL, part );

	panel->addTextureDropTarget( TEX_HEAD_BODYPAINT,  "Head Tattoos", 	LLUUID::null, TRUE );
	panel->addTextureDropTarget( TEX_UPPER_BODYPAINT, "Upper Tattoos", 	LLUUID::null, TRUE );
	panel->addTextureDropTarget( TEX_LOWER_BODYPAINT, "Lower Tattoos", 	LLUUID::null, TRUE );

	/////////////////////////////////////////
	// Hair
	panel = mWearablePanelList[ WT_HAIR ];

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_color";
	part->mTargetOffset.setVec(0.f, 0.f, 0.10f);
	part->mCameraOffset.setVec(-0.4f, 0.05f, 0.10f);
	panel->addSubpart( "Color", SUBPART_HAIR_COLOR, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_style";
	part->mTargetOffset.setVec(0.f, 0.f, 0.10f);
	part->mCameraOffset.setVec(-0.4f, 0.05f, 0.10f);
	panel->addSubpart( "Style", SUBPART_HAIR_STYLE, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_eyebrows";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Eyebrows", SUBPART_HAIR_EYEBROWS, part );

	part = new LLSubpart();
	part->mSex = SEX_MALE;
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_facial";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Facial", SUBPART_HAIR_FACIAL, part );

	panel->addTextureDropTarget(TEX_HAIR, "Texture",
								LLUUID( gSavedSettings.getString( "UIImgDefaultHairUUID" ) ),
								FALSE );

	/////////////////////////////////////////
	// Eyes
	panel = mWearablePanelList[ WT_EYES ];

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "eyes";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( LLStringUtil::null, SUBPART_EYES, part );

	panel->addTextureDropTarget(TEX_EYES_IRIS, "Iris",
								LLUUID( gSavedSettings.getString( "UIImgDefaultEyesUUID" ) ),
								FALSE );

	/////////////////////////////////////////
	// Shirt
	panel = mWearablePanelList[ WT_SHIRT ];

	part = new LLSubpart();
	part->mTargetJoint = "mTorso";
	part->mEditGroup = "shirt";
	part->mTargetOffset.setVec(0.f, 0.f, 0.3f);
	part->mCameraOffset.setVec(-1.f, 0.15f, 0.3f);
	panel->addSubpart( LLStringUtil::null, SUBPART_SHIRT, part );

	panel->addTextureDropTarget( TEX_UPPER_SHIRT, "Fabric",
								 LLUUID( gSavedSettings.getString( "UIImgDefaultShirtUUID" ) ),
								 FALSE );

	panel->addColorSwatch( TEX_UPPER_SHIRT, "Color/Tint" );


	/////////////////////////////////////////
	// Pants
	panel = mWearablePanelList[ WT_PANTS ];

	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "pants";
	part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
	part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
	panel->addSubpart( LLStringUtil::null, SUBPART_PANTS, part );

	panel->addTextureDropTarget(TEX_LOWER_PANTS, "Fabric",
								LLUUID( gSavedSettings.getString( "UIImgDefaultPantsUUID" ) ),
								FALSE );

	panel->addColorSwatch( TEX_LOWER_PANTS, "Color/Tint" );


	/////////////////////////////////////////
	// Shoes
	panel = mWearablePanelList[ WT_SHOES ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "shoes";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_SHOES, part );

		panel->addTextureDropTarget( TEX_LOWER_SHOES, "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultShoesUUID" ) ),
									 FALSE );

		panel->addColorSwatch( TEX_LOWER_SHOES, "Color/Tint" );
	}


	/////////////////////////////////////////
	// Socks
	panel = mWearablePanelList[ WT_SOCKS ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "socks";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_SOCKS, part );

		panel->addTextureDropTarget( TEX_LOWER_SOCKS, "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultSocksUUID" ) ),
									 FALSE );

		panel->addColorSwatch( TEX_LOWER_SOCKS, "Color/Tint" );
	}

	/////////////////////////////////////////
	// Jacket
	panel = mWearablePanelList[ WT_JACKET ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "jacket";
		part->mTargetOffset.setVec(0.f, 0.f, 0.f);
		part->mCameraOffset.setVec(-2.f, 0.1f, 0.3f);
		panel->addSubpart( LLStringUtil::null, SUBPART_JACKET, part );

		panel->addTextureDropTarget( TEX_UPPER_JACKET, "Upper Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ),
									 FALSE );
		panel->addTextureDropTarget( TEX_LOWER_JACKET, "Lower Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ),
									 FALSE );

		panel->addColorSwatch( TEX_UPPER_JACKET, "Color/Tint" );
	}

	/////////////////////////////////////////
	// Skirt
	panel = mWearablePanelList[ WT_SKIRT ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "skirt";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_SKIRT, part );

		panel->addTextureDropTarget( TEX_SKIRT,  "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultSkirtUUID" ) ),
									 FALSE );

		panel->addColorSwatch( TEX_SKIRT, "Color/Tint" );
	}


	/////////////////////////////////////////
	// Gloves
	panel = mWearablePanelList[ WT_GLOVES ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "gloves";
		part->mTargetOffset.setVec(0.f, 0.f, 0.f);
		part->mCameraOffset.setVec(-1.f, 0.15f, 0.f);
		panel->addSubpart( LLStringUtil::null, SUBPART_GLOVES, part );

		panel->addTextureDropTarget( TEX_UPPER_GLOVES,  "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultGlovesUUID" ) ),
									 FALSE );

		panel->addColorSwatch( TEX_UPPER_GLOVES, "Color/Tint" );
	}


	/////////////////////////////////////////
	// Undershirt
	panel = mWearablePanelList[ WT_UNDERSHIRT ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "undershirt";
		part->mTargetOffset.setVec(0.f, 0.f, 0.3f);
		part->mCameraOffset.setVec(-1.f, 0.15f, 0.3f);
		panel->addSubpart( LLStringUtil::null, SUBPART_UNDERSHIRT, part );

		panel->addTextureDropTarget( TEX_UPPER_UNDERSHIRT,  "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ),
									 FALSE );

		panel->addColorSwatch( TEX_UPPER_UNDERSHIRT, "Color/Tint" );
	}

	/////////////////////////////////////////
	// Underpants
	panel = mWearablePanelList[ WT_UNDERPANTS ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "underpants";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_UNDERPANTS, part );

		panel->addTextureDropTarget( TEX_LOWER_UNDERPANTS, "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ),
									 FALSE );

		panel->addColorSwatch( TEX_LOWER_UNDERPANTS, "Color/Tint" );
	}

	/////////////////////////////////////////
	// Alpha
	panel = mWearablePanelList[WT_ALPHA];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "alpha";
		part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
		part->mCameraOffset.setVec(-2.5f, 0.5f, 0.8f);
		panel->addSubpart(LLStringUtil::null, SUBPART_ALPHA, part);

		panel->addTextureDropTarget(TEX_LOWER_ALPHA, "Lower Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									TRUE);
		panel->addTextureDropTarget(TEX_UPPER_ALPHA, "Upper Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									TRUE);
		panel->addTextureDropTarget(TEX_HEAD_ALPHA, "Head Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									TRUE);
		panel->addTextureDropTarget(TEX_EYES_ALPHA, "Eye Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									TRUE);
		panel->addTextureDropTarget(TEX_HAIR_ALPHA, "Hair Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									TRUE);

		panel->addInvisibilityCheckbox(TEX_LOWER_ALPHA, "lower alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_UPPER_ALPHA, "upper alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_HEAD_ALPHA, "head alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_EYES_ALPHA, "eye alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_HAIR_ALPHA, "hair alpha texture invisible");
	}

	/////////////////////////////////////////
	// Tattoo
	panel = mWearablePanelList[WT_TATTOO];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "tattoo";
		part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
		part->mCameraOffset.setVec(-2.5f, 0.5f, 0.8f);
		panel->addSubpart(LLStringUtil::null, SUBPART_TATTOO, part);

		panel->addTextureDropTarget(TEX_LOWER_TATTOO, "Lower Tattoo",
									LLUUID::null,
									TRUE);
		panel->addTextureDropTarget(TEX_UPPER_TATTOO, "Upper Tattoo",
									LLUUID::null,
									TRUE);
		panel->addTextureDropTarget(TEX_HEAD_TATTOO, "Head Tattoo",
									LLUUID::null,
									TRUE);
		panel->addColorSwatch(TEX_LOWER_TATTOO, "Color/Tint"); //-ASC-TTRFE
	}

	/////////////////////////////////////////
	// Physics

	panel = mWearablePanelList[WT_PHYSICS];

	if(panel)
	{
		part = new LLSubpart();
		part->mSex = SEX_FEMALE;
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "physics_breasts_updown";
		part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
		part->mCameraOffset.setVec(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Breast Bounce", SUBPART_PHYSICS_BREASTS_UPDOWN, part);

		part = new LLSubpart();
		part->mSex = SEX_FEMALE;
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "physics_breasts_inout";
		part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
		part->mCameraOffset.setVec(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Breast Cleavage", SUBPART_PHYSICS_BREASTS_INOUT, part);

		part = new LLSubpart();
		part->mSex = SEX_FEMALE;
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "physics_breasts_leftright";
		part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
		part->mCameraOffset.setVec(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Breast Sway", SUBPART_PHYSICS_BREASTS_LEFTRIGHT, part);

		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "physics_belly_updown";
		part->mTargetOffset.setVec(0.f, 0.f, -.05f);
		part->mCameraOffset.setVec(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Belly Bounce", SUBPART_PHYSICS_BELLY_UPDOWN, part);

		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "physics_butt_updown";
		part->mTargetOffset.setVec(0.f, 0.f, -0.1f);
		part->mCameraOffset.setVec(0.3f, 0.8f, -0.1f);
		part->mVisualHint = false;
		panel->addSubpart("Butt Bounce", SUBPART_PHYSICS_BUTT_UPDOWN, part);

		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "physics_butt_leftright";
		part->mTargetOffset.setVec(0.f, 0.f, -0.1f);
		part->mCameraOffset.setVec(0.3f, 0.8f, -0.1f);
		part->mVisualHint = false;
		panel->addSubpart("Butt Sway", SUBPART_PHYSICS_BUTT_LEFTRIGHT, part);

		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "physics_advanced";
		part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
		part->mCameraOffset.setVec(-2.5f, 0.5f, 0.8f);
		part->mVisualHint = false;
		panel->addSubpart("Advanced Parameters", SUBPART_PHYSICS_ADVANCED, part);

	}
}

////////////////////////////////////////////////////////////////////////////

LLFloaterCustomize::~LLFloaterCustomize()
{
	llinfos << "Destroying LLFloaterCustomize" << llendl;
	mResetParams = NULL;
	gInventory.removeObserver(mInventoryObserver);
	delete mInventoryObserver;
}

void LLFloaterCustomize::switchToDefaultSubpart()
{
	getCurrentWearablePanel()->switchToDefaultSubpart();
}

void LLFloaterCustomize::draw()
{
	if( isMinimized() )
	{
		LLFloater::draw();
		return;
	}

	// only do this if we are in the customize avatar mode
	// and not transitioning into or out of it
	// *TODO: This is a sort of expensive call, which only needs
	// to be called when the tabs change or an inventory item
	// arrives. Figure out some way to avoid this if possible.
	updateInventoryUI();
	
	updateAvatarHeightDisplay();

	LLScrollingPanelParam::sUpdateDelayFrames = 0;
	
	LLFloater::draw();
}

BOOL LLFloaterCustomize::isDirty() const
{
	for(S32 i = 0; i < WT_COUNT; i++)
	{
		if( mWearablePanelList[i]
			&& mWearablePanelList[i]->isDirty() )
		{
			return TRUE;
		}
	}
	return FALSE;
}

// static
void LLFloaterCustomize::onTabPrecommit( void* userdata, bool from_click )
{
	EWearableType type = (EWearableType)(intptr_t) userdata;
	if (type != WT_INVALID && gFloaterCustomize && gFloaterCustomize->getCurrentWearableType() != type)
	{
		gFloaterCustomize->askToSaveIfDirty(onCommitChangeTab, userdata);
	}
	else
	{
		onCommitChangeTab(TRUE, NULL);
	}
}


// static
void LLFloaterCustomize::onTabChanged( void* userdata, bool from_click )
{
	EWearableType wearable_type = (EWearableType) (intptr_t)userdata;
	if (wearable_type != WT_INVALID)
	{
		LLFloaterCustomize::setCurrentWearableType(wearable_type);
	}
}

void LLFloaterCustomize::onClose(bool app_quitting)
{
	// since this window is potentially staying open, push to back to let next window take focus
	gFloaterView->sendChildToBack(this);
	handle_reset_view();  // Calls askToSaveIfDirty
}

// static
void LLFloaterCustomize::onCommitChangeTab(BOOL proceed, void* userdata)
{
	if (!proceed || !gFloaterCustomize)
	{
		return;
	}

	LLTabContainer* tab_container = gFloaterCustomize->getChild<LLTabContainer>("customize tab container");
	if (tab_container)
	{
		tab_container->setTab(-1);
	}
}



////////////////////////////////////////////////////////////////////////////

const S32 LOWER_BTN_HEIGHT = 18 + 8;

const S32 FLOATER_CUSTOMIZE_BUTTON_WIDTH = 82;
const S32 FLOATER_CUSTOMIZE_BOTTOM_PAD = 30;
const S32 LINE_HEIGHT = 16;
const S32 HEADER_PAD = 8;
const S32 HEADER_HEIGHT = 3 * (LINE_HEIGHT + LLFLOATER_VPAD) + (2 * LLPANEL_BORDER_WIDTH) + HEADER_PAD; 

void LLFloaterCustomize::initScrollingPanelList()
{
	LLScrollableContainerView* scroll_container =
		getChild<LLScrollableContainerView>("panel_container");
	// LLScrollingPanelList's do not import correctly 
// 	mScrollingPanelList = LLUICtrlFactory::getScrollingPanelList(this, "panel_list");
	mScrollingPanelList = new LLScrollingPanelList(std::string("panel_list"), LLRect());
	if (scroll_container)
	{
		scroll_container->setScrolledView(mScrollingPanelList);
		scroll_container->addChild(mScrollingPanelList);
	}
}

void LLFloaterCustomize::clearScrollingPanelList()
{
	if( mScrollingPanelList )
	{
		mScrollingPanelList->clearPanels();
	}
}

void LLFloaterCustomize::generateVisualParamHints(LLViewerJointMesh* joint_mesh, LLFloaterCustomize::param_map& params, bool bVisualHint)
{
	// sorted_params is sorted according to magnitude of effect from
	// least to greatest.  Adding to the front of the child list
	// reverses that order.
	if( mScrollingPanelList )
	{
		mScrollingPanelList->clearPanels();
		param_map::iterator end = params.end();
		for(param_map::iterator it = params.begin(); it != end; ++it)
		{
			mScrollingPanelList->addPanel( new LLScrollingPanelParam( "LLScrollingPanelParam", joint_mesh, (*it).second.second, (*it).second.first, bVisualHint) );
		}
	}
}

void LLFloaterCustomize::setWearable(EWearableType type, LLWearable* wearable, U32 perm_mask, BOOL is_complete)
{
	llassert( type < WT_COUNT );
	gSavedSettings.setU32("AvatarSex", (gAgent.getAvatarObject()->getSex() == SEX_MALE) );
	
	LLPanelEditWearable* panel = mWearablePanelList[ type ];
	if( panel )
	{
		panel->setWearable(wearable, perm_mask, is_complete);
		updateScrollingPanelList((perm_mask & PERM_MODIFY) ? is_complete : FALSE);
	}
}

void LLFloaterCustomize::updateScrollingPanelList(BOOL allow_modify)
{
	if( mScrollingPanelList )
	{
		LLScrollingPanelParam::sUpdateDelayFrames = 0;
		mScrollingPanelList->updatePanels(allow_modify );
	}
}


void LLFloaterCustomize::askToSaveIfDirty( void(*next_step_callback)(BOOL proceed, void* userdata), void* userdata )
{
	if( isDirty())
	{
		// Ask if user wants to save, then continue to next step afterwards
		mNextStepAfterSaveCallback = next_step_callback;
		mNextStepAfterSaveUserdata = userdata;

		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotifications::instance().add("SaveClothingBodyChanges", LLSD(), LLSD(),
			boost::bind(&LLFloaterCustomize::onSaveDialog, this, _1, _2));
		return;
	}

	// Try to move to the next step
	if( next_step_callback )
	{
		next_step_callback( TRUE, userdata );
	}
}


bool LLFloaterCustomize::onSaveDialog(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	BOOL proceed = FALSE;
	EWearableType cur = getCurrentWearableType();

	switch( option )
	{
	case 0:  // "Save"
		gAgent.saveWearable( cur );
		proceed = TRUE;
		break;

	case 1:  // "Don't Save"
		{
			gAgent.revertWearable( cur );
			proceed = TRUE;
		}
		break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}

	if( mNextStepAfterSaveCallback )
	{
		mNextStepAfterSaveCallback( proceed, mNextStepAfterSaveUserdata );
	}
	return false;
}

// fetch observer
class LLCurrentlyWorn : public LLInventoryFetchObserver
{
public:
	LLCurrentlyWorn() {}
	~LLCurrentlyWorn() {}
	virtual void done() { /* no operation necessary */}
};

void LLFloaterCustomize::fetchInventory()
{
	// Fetch currently worn items
	LLInventoryFetchObserver::item_ref_t ids;
	LLUUID item_id;
	for(S32 type = (S32)WT_SHAPE; type < (S32)WT_COUNT; ++type)
	{
		item_id = gAgent.getWearableItem((EWearableType)type);
		if(item_id.notNull())
		{
			ids.push_back(item_id);
		}
	}

	// Fire & forget. The mInventoryObserver will catch inventory
	// updates and correct the UI as necessary.
	LLCurrentlyWorn worn;
	worn.fetchItems(ids);
}

void LLFloaterCustomize::updateInventoryUI()
{
	BOOL all_complete = TRUE;
	BOOL is_complete = FALSE;
	U32 perm_mask = 0x0;
	LLPanelEditWearable* panel;
	LLViewerInventoryItem* item;
	for(S32 i = 0; i < WT_COUNT; ++i)
	{
		item = NULL;
		panel = mWearablePanelList[i];
		if(panel)
		{
			item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(panel->getType());
		}
		if(item)
		{
			is_complete = item->isComplete();
			if(!is_complete)
			{
				all_complete = FALSE;
			}
			perm_mask = item->getPermissions().getMaskOwner();
		}
		else
		{
			is_complete = false;
			perm_mask = 0x0;
		}
		if(i == sCurrentWearableType)
		{
			if(panel)
			{
				panel->setUIPermissions(perm_mask, is_complete);
			}
			BOOL is_vis = panel && item && is_complete && (perm_mask & PERM_MODIFY);
			childSetVisible("panel_container", is_vis);
		}
	}

	childSetEnabled("Make Outfit", all_complete);
}

void LLFloaterCustomize::updateScrollingPanelUI()
{
	LLPanelEditWearable* panel = mWearablePanelList[sCurrentWearableType];
	if(panel)
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(panel->getType());
		if(item)
		{
			U32 perm_mask = item->getPermissions().getMaskOwner();
			BOOL is_complete = item->isComplete();
			updateScrollingPanelList((perm_mask & PERM_MODIFY) ? is_complete : FALSE);
		}
	}
}

