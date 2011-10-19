/** 
 * @file llhoverview.cpp
 * @brief LLHoverView class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

// self include
#include "llhoverview.h"

// Library includes
#include "llfontgl.h"
#include "message.h"
#include "llgl.h"
#include "llrender.h"
#include "llfontgl.h"
#include "llparcel.h"
#include "lldbstrings.h"
#include "llclickaction.h"

// Viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llavatarnamecache.h"
#include "llcachename.h"
#include "llviewercontrol.h"
#include "lldrawable.h"
#include "llpermissions.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "lltrans.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolselectland.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llglheaders.h"
#include "llviewertexturelist.h"
//#include "lltoolobjpicker.h"
#include "llhudmanager.h"	// HACK for creating flex obj's

#include "llhudmanager.h" // For testing effects
#include "llhudeffect.h"

#include "hippogridmanager.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

//
// Constants
//
const char* DEFAULT_DESC = "(No Description)";
const F32 DELAY_BEFORE_SHOW_TIP = 0.35f;

//
// Local globals
//

LLHoverView *gHoverView = NULL;

//
// Static member functions
//
BOOL LLHoverView::sShowHoverTips = TRUE;

//
// Member functions
//

LLHoverView::LLHoverView(const std::string& name, const LLRect& rect)
:	LLView(name, rect, FALSE)
{
	mDoneHoverPick = FALSE;
	mStartHoverPickTimer = FALSE;
	mHoverActive = FALSE;
	mUseHover = TRUE;
	mTyping = FALSE;
	mHoverOffset.clearVec();
}

LLHoverView::~LLHoverView()
{
}

void LLHoverView::updateHover(LLTool* current_tool)
{
	BOOL picking_tool = (	current_tool == LLToolPie::getInstance() 
							|| current_tool == LLToolSelectLand::getInstance() );

	mUseHover = !gAgentCamera.cameraMouselook() 
				&& picking_tool 
				&& !mTyping;
	if (mUseHover)
	{
		if ((gViewerWindow->getMouseVelocityStat()->getPrev(0) < 0.01f)
			&& (LLViewerCamera::getInstance()->getAngularVelocityStat()->getPrev(0) < 0.01f)
			&& (LLViewerCamera::getInstance()->getVelocityStat()->getPrev(0) < 0.01f))
		{
			if (!mStartHoverPickTimer)
			{
				mStartHoverTimer.reset();
				mStartHoverPickTimer = TRUE;
				//  Clear the existing text so that we do not briefly show the wrong data.
				mText.clear();
			}

			if (mDoneHoverPick)
			{
				// Just update the hover data
				updateText();
			}
			else if (mStartHoverTimer.getElapsedTimeF32() > DELAY_BEFORE_SHOW_TIP)
			{
				gViewerWindow->pickAsync(gViewerWindow->getCurrentMouseX(),
													 gViewerWindow->getCurrentMouseY(), 0, pickCallback );
			}
		}
		else
		{
			cancelHover();
		}
	}

}

void LLHoverView::pickCallback(const LLPickInfo& pick_info)
{
	gHoverView->mLastPickInfo = pick_info;
	LLViewerObject* hit_obj = pick_info.getObject();

	if (hit_obj)
	{
		gHoverView->setHoverActive(TRUE);
		LLSelectMgr::getInstance()->setHoverObject(hit_obj, pick_info.mObjectFace);
		gHoverView->mLastHoverObject = hit_obj;
		gHoverView->mHoverOffset = pick_info.mObjectOffset;
	}
	else
	{
		gHoverView->mLastHoverObject = NULL;
	}

	// We didn't hit an object, but we did hit land.
	if (!hit_obj && pick_info.mPosGlobal != LLVector3d::zero)
	{
		gHoverView->setHoverActive(TRUE);
		gHoverView->mHoverLandGlobal = pick_info.mPosGlobal;
		LLViewerParcelMgr::getInstance()->setHoverParcel( gHoverView->mHoverLandGlobal );
	}
	else
	{
		gHoverView->mHoverLandGlobal = LLVector3d::zero;
	}

	gHoverView->mDoneHoverPick = TRUE;
}

void LLHoverView::setTyping(BOOL b)
{
	mTyping = b;
}


void LLHoverView::cancelHover()
{
	mStartHoverTimer.reset();
	mDoneHoverPick = FALSE;
	mStartHoverPickTimer = FALSE;

	LLSelectMgr::getInstance()->setHoverObject(NULL);
	// Can't do this, some code relies on hover object still being
	// set after the hover is cancelled!  Dammit.  JC
	// mLastHoverObject = NULL;

	setHoverActive(FALSE);
}

void LLHoverView::resetLastHoverObject()
{
	mLastHoverObject = NULL;
}

void LLHoverView::updateText()
{
	LLViewerObject* hit_object = getLastHoverObject();
	std::string line;

	mText.clear();
	if ( hit_object )
	{
		if ( hit_object->isHUDAttachment() )
		{
			// no hover tips for HUD elements, since they can obscure
			// what the HUD is displaying
			return;
		}

		if ( hit_object->isAttachment() )
		{
			// get root of attachment then parent, which is avatar
			LLViewerObject* root_edit = hit_object->getRootEdit();
			if (!root_edit)
			{
				// Strange parenting issue, don't show any text
				return;
			}
			hit_object = (LLViewerObject*)root_edit->getParent();
			if (!hit_object)
			{
				// another strange parenting issue, bail out
				return;
			}
		}

		line.clear();
		if (hit_object->isAvatar())
		{
			LLNameValue* title = hit_object->getNVPair("Title");
			LLNameValue* firstname = hit_object->getNVPair("FirstName");
			LLNameValue* lastname =  hit_object->getNVPair("LastName");
			if (firstname && lastname)
			{
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
				{
					line = RlvStrings::getAnonym(line.append(firstname->getString()).append(1, ' ').append(lastname->getString()));
				}
				else
				{
// [/RLVa:KB]

                    // [Ansariel: Display name support]
    				std::string complete_name = firstname->getString();
    				complete_name += " ";
    				complete_name += lastname->getString();

    				if (LLAvatarNameCache::useDisplayNames())
    				{
    					LLAvatarName avatar_name;
    					if (LLAvatarNameCache::get(hit_object->getID(), &avatar_name))
    					{
							static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
    						if (phoenix_name_system == 2 || (phoenix_name_system == 1 && avatar_name.mIsDisplayNameDefault))
    						{
    							complete_name = avatar_name.mDisplayName;
    						}
    						else
    						{
    							complete_name = avatar_name.getCompleteName();
    						}
    					}
    				}
					// [/Ansariel: Display name support]

					if (title)
					{
						line.append(title->getString());
						line.append(1, ' ');
					}
					
					// [Ansariel: Display name support]
					line += complete_name;
					// [/Ansariel: Display name support]
					
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
				}
// [/RLVa:KB]
			}
			else
			{
				line.append(LLTrans::getString("TooltipPerson"));
			}
			mText.push_back(line);
		}
		else
		{
			//
			//  We have hit a regular object (not an avatar or attachment)
			// 

			//
			//  Default prefs will suppress display unless the object is interactive
			//
			BOOL suppressObjectHoverDisplay = !gSavedSettings.getBOOL("ShowAllObjectHoverTip");			
			
			LLSelectNode *nodep = LLSelectMgr::getInstance()->getHoverNode();;
			if (nodep)
			{
				line.clear();
				if (nodep->mName.empty())
				{
					line.append(LLTrans::getString("TooltipNoName"));
				}
				else
				{
					line.append( nodep->mName );
				}
				mText.push_back(line);

				if (!nodep->mDescription.empty()
					&& nodep->mDescription != DEFAULT_DESC)
				{
					mText.push_back( nodep->mDescription );
				}

				// Line: "Owner: James Linden"
				line.clear();
				line.append(LLTrans::getString("TooltipOwner") + " ");

				if (nodep->mValid)
				{
					LLUUID owner;
					std::string name;
					if (!nodep->mPermissions->isGroupOwned())
					{
						// [Ansariel: Display name support]
						LLAvatarName avatar_name;
						// [/Ansariel: Display name support]
						owner = nodep->mPermissions->getOwner();
						if (LLUUID::null == owner)
						{
							line.append(LLTrans::getString("TooltipPublic"));
						}
						// [Ansariel: Display name support]
						//else if(gCacheName->getFullName(owner, name))
						else if (LLAvatarNameCache::get(owner, &avatar_name))
						{
							static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
							switch (phoenix_name_system)
							{
								case 0 : name = avatar_name.getCompleteName(); break;
								case 1 : name = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
								case 2 : name = avatar_name.mDisplayName; break;
								default : name = avatar_name.getCompleteName(); break;
							}
						// [/Ansariel: Display name support]
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
							if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
							{
								name = RlvStrings::getAnonym(name);
							}
// [/RLVa:KB]

							line.append(name);
						}
						else
						{
							line.append(LLTrans::getString("RetrievingData"));
						}
					}
					else
					{
						std::string name;
						owner = nodep->mPermissions->getGroup();
						if (gCacheName->getGroupName(owner, name))
						{
							line.append(name);
							line.append(LLTrans::getString("TooltipIsGroup"));
						}
						else
						{
							line.append(LLTrans::getString("RetrievingData"));
						}
					}
				}
				else
				{
					line.append(LLTrans::getString("RetrievingData"));
				}
				mText.push_back(line);

				// Build a line describing any special properties of this object.
				LLViewerObject *object = hit_object;
				LLViewerObject *parent = (LLViewerObject *)object->getParent();

				if (object &&
					(object->usePhysics() ||
					 object->flagScripted() || 
					 object->flagHandleTouch() || (parent && parent->flagHandleTouch()) ||
					 object->flagTakesMoney() || (parent && parent->flagTakesMoney()) ||
					 object->flagAllowInventoryAdd() ||
					 object->flagTemporary() ||
					 object->flagPhantom()) )
				{
					line.clear();
					if (object->flagScripted())
					{
						
						line.append(LLTrans::getString("TooltipFlagScript") + " ");
					}

					if (object->usePhysics())
					{
						line.append(LLTrans::getString("TooltipFlagPhysics") + " ");
					}

					if (object->flagHandleTouch() || (parent && parent->flagHandleTouch()) )
					{
						line.append(LLTrans::getString("TooltipFlagTouch") + " ");
						suppressObjectHoverDisplay = FALSE;		//  Show tip
					}

					if (object->flagTakesMoney() || (parent && parent->flagTakesMoney()) )
					{
						line.append(gHippoGridManager->getConnectedGrid()->getCurrencySymbol() + " ");
						suppressObjectHoverDisplay = FALSE;		//  Show tip
					}

					if (object->flagAllowInventoryAdd())
					{
						line.append(LLTrans::getString("TooltipFlagDropInventory") + " ");
						suppressObjectHoverDisplay = FALSE;		//  Show tip
					}

					if (object->flagPhantom())
					{
						line.append(LLTrans::getString("TooltipFlagPhantom") + " ");
					}

					if (object->flagTemporary())
					{
						line.append(LLTrans::getString("TooltipFlagTemporary") + " ");
					}

					if (object->usePhysics() || 
						object->flagHandleTouch() ||
						(parent && parent->flagHandleTouch()) )
					{
						line.append(LLTrans::getString("TooltipFlagRightClickMenu") + " ");
					}
					mText.push_back(line);
				}

				// Free to copy / For Sale: L$
				line.clear();
				if (nodep->mValid)
				{
					BOOL for_copy = nodep->mPermissions->getMaskEveryone() & PERM_COPY && object->permCopy();
					BOOL for_sale = nodep->mSaleInfo.isForSale() &&
									nodep->mPermissions->getMaskOwner() & PERM_TRANSFER &&
									(nodep->mPermissions->getMaskOwner() & PERM_COPY ||
									 nodep->mSaleInfo.getSaleType() != LLSaleInfo::FS_COPY);
					if (for_copy)
					{
						line.append(LLTrans::getString("TooltipFreeToCopy"));
						suppressObjectHoverDisplay = FALSE;		//  Show tip
					}
					else if (for_sale)
					{
						LLStringUtil::format_map_t args;
						args["[CURRENCY]"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
						args["[AMOUNT]"] = llformat("%d", nodep->mSaleInfo.getSalePrice());
						line.append(LLTrans::getString("TooltipForSaleL$", args));
						suppressObjectHoverDisplay = FALSE;		//  Show tip
					}
					else
					{
						// Nothing if not for sale
						// line.append("Not for sale");
					}
				}
				else
				{
					LLStringUtil::format_map_t args;
					args["[MESSAGE]"] = LLTrans::getString("RetrievingData");
					line.append(LLTrans::getString("TooltipForSaleMsg", args));
				}
				mText.push_back(line);
			}
			line.clear();
			S32 prim_count = LLSelectMgr::getInstance()->getHoverObjects()->getObjectCount();
			line.append(llformat("Prims: %d", prim_count));
			mText.push_back(line);

			line.clear();
			line.append("Position: ");

			LLViewerRegion *region = gAgent.getRegion();
			LLVector3 position = region->getPosRegionFromGlobal(hit_object->getPositionGlobal());//regionp->getOriginAgent();
			LLVector3 mypos = region->getPosRegionFromGlobal(gAgent.getPositionGlobal());
			

			LLVector3 delta = position - mypos;
			F32 distance = (F32)delta.magVec();

			line.append(llformat("<%.02f,%.02f,%.02f>",position.mV[0],position.mV[1],position.mV[2]));
			mText.push_back(line);
			line.clear();
			line.append(llformat("Distance: %.02fm",distance));
			mText.push_back(line);
			
			//  If the hover tip shouldn't be shown, delete all the object text
			if (suppressObjectHoverDisplay)
			{
				mText.clear();
			}
		}
	}
	else if ( mHoverLandGlobal != LLVector3d::zero )
	{
		// 
		//  Do not show hover for land unless prefs are set to allow it.
		// 
		
		if (!gSavedSettings.getBOOL("ShowLandHoverTip")) return; 

		// Didn't hit an object, but since we have a land point we
		// must be hovering over land.

		LLParcel* hover_parcel = LLViewerParcelMgr::getInstance()->getHoverParcel();
		LLUUID owner;
		S32 width = 0;
		S32 height = 0;

		if ( hover_parcel )
		{
			owner = hover_parcel->getOwnerID();
			width = S32(LLViewerParcelMgr::getInstance()->getHoverParcelWidth());
			height = S32(LLViewerParcelMgr::getInstance()->getHoverParcelHeight());
		}

		// Line: "Land"
		line.clear();
		line.append(LLTrans::getString("TooltipLand"));
		if (hover_parcel)
		{
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a) | Added: RLVa-0.2.0b
			line.append( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) 
				? hover_parcel->getName() : RlvStrings::getString(RLV_STRING_HIDDEN_PARCEL) );
// [/RLVa:KB]
			//line.append(hover_parcel->getName());
		}
		mText.push_back(line);

		// Line: "Owner: James Linden"
		line.clear();
		line.append(LLTrans::getString("TooltipOwner") + " ");

		if ( hover_parcel )
		{
			std::string name;
			if (LLUUID::null == owner)
			{
				line.append(LLTrans::getString("TooltipPublic"));
			}
			else if (hover_parcel->getIsGroupOwned())
			{
				if (gCacheName->getGroupName(owner, name))
				{
					line.append(name);
					line.append(LLTrans::getString("TooltipIsGroup"));
				}
				else
				{
					line.append(LLTrans::getString("RetrievingData"));
				}
			}
			else if(gCacheName->getFullName(owner, name))
			{
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0b
				line.append( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? name : RlvStrings::getAnonym(name));
// [/RLVa:KB]
				//line.append(name);
			}
			else
			{
				line.append(LLTrans::getString("RetrievingData"));
			}
		}
		else
		{
			line.append(LLTrans::getString("RetrievingData"));
		}
		mText.push_back(line);

		// Line: "no fly, not safe, no build"

		// Don't display properties for your land.  This is just
		// confusing, because you can do anything on your own land.
		if ( hover_parcel && owner != gAgent.getID() )
		{
			S32 words = 0;
			
			line.clear();
			// JC - Keep this in the same order as the checkboxes
			// on the land info panel
			if ( !hover_parcel->getAllowModify() )
			{
				if ( hover_parcel->getAllowGroupModify() )
				{
					line.append(LLTrans::getString("TooltipFlagGroupBuild"));
				}
				else
				{
					line.append(LLTrans::getString("TooltipFlagNoBuild"));
				}
				words++;
			}

			if ( !hover_parcel->getAllowTerraform() )
			{
				if (words) line.append(", ");
				line.append(LLTrans::getString("TooltipFlagNoEdit"));
				words++;
			}

			if ( hover_parcel->getAllowDamage() )
			{
				if (words) line.append(", ");
				line.append(LLTrans::getString("TooltipFlagNotSafe"));
				words++;
			}

			// Maybe we should reflect the estate's block fly bit here as well?  DK 12/1/04
			if ( !hover_parcel->getAllowFly() )
			{
				if (words) line.append(", ");
				line.append(LLTrans::getString("TooltipFlagNoFly"));
				words++;
			}

			if ( !hover_parcel->getAllowOtherScripts() )
			{
				if (words) line.append(", ");
				if ( hover_parcel->getAllowGroupScripts() )
				{
					line.append(LLTrans::getString("TooltipFlagGroupScripts"));
				}
				else
				{
					line.append(LLTrans::getString("TooltipFlagNoScripts"));
				}
				
				words++;
			}

			if (words) 
			{
				mText.push_back(line);
			}
		}

		// Line: "Size: 1x4"
		// Only show for non-public land
		/*
		if ( hover_parcel && LLUUID::null != owner)
		{
			line = llformat("Size: %dx%d", width, height );
			mText.push_back(line);
		}
		*/
		if (hover_parcel && hover_parcel->getParcelFlag(PF_FOR_SALE))
		{
			LLStringUtil::format_map_t args;
			args["[CURRENCY]"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
			args["[AMOUNT]"] = llformat("%d", hover_parcel->getSalePrice());
			line = LLTrans::getString("TooltipForSaleL$", args);
			mText.push_back(line);
		}
	}
}


void LLHoverView::draw()
{
	if ( !isHovering() )
	{
		return;
	}

	// To toggle off hover tips, you have to just suppress the draw.
	// The picking is still needed to do cursor changes over physical
	// and scripted objects.  JC
//	if (!sShowHoverTips) 
// [RLVa:KB] - Checked: 2010-01-02 (RLVa-1.1.0l) | Modified: RLVa-1.1.0l
#ifdef RLV_EXTENSION_CMD_INTERACT
	if ( (!sShowHoverTips) || (gRlvHandler.hasBehaviour(RLV_BHVR_INTERACT)) )
#else
	if (!sShowHoverTips) 
#endif // RLV_EXTENSION_CMD_INTERACT
// [/RLVa:KB]
	{
		return;
	}

	const F32 MAX_HOVER_DISPLAY_SECS = 5.f;
	if (mHoverTimer.getElapsedTimeF32() > MAX_HOVER_DISPLAY_SECS)
	{
		return;
	}

	const F32 MAX_ALPHA = 0.9f;
	//const F32 STEADY_ALPHA = 0.3f;
	F32 alpha;
	if (mHoverActive)
	{
		alpha = 1.f;

		if (isHoveringObject())
		{
			// look at object
			LLViewerObject *hover_object = getLastHoverObject();
			if (hover_object->isAvatar())
			{
				gAgentCamera.setLookAt(LOOKAT_TARGET_HOVER, getLastHoverObject(), LLVector3::zero);
			}
			else
			{
				gAgentCamera.setLookAt(LOOKAT_TARGET_HOVER, getLastHoverObject(), mHoverOffset);
			}
		}
	}
	else
	{
		alpha = llmax(0.f, MAX_ALPHA - mHoverTimer.getElapsedTimeF32()*2.f);
	}

	// Bail out if no text to display
	if (mText.empty())
	{
		return;
	}

	// Don't draw if no alpha
	if (alpha <= 0.f)
	{
		return;
	}

	LLUIImagePtr box_imagep = LLUI::getUIImage("rounded_square.tga");
	LLUIImagePtr shadow_imagep = LLUI::getUIImage("rounded_square_soft.tga");

	const LLFontGL* fontp = LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF_SMALL);

	// Render text.
	LLColor4 text_color = gColors.getColor("ToolTipTextColor");
	// LLColor4 border_color = gColors.getColor("ToolTipBorderColor");
	LLColor4 bg_color = gColors.getColor("ToolTipBgColor");
	LLColor4 shadow_color = gColors.getColor("ColorDropShadow");

	// Could decrease the alpha here. JC
	//text_color.mV[VALPHA] = alpha;
	//border_color.mV[VALPHA] = alpha;
	//bg_color.mV[VALPHA] = alpha;

	S32 max_width = 0;
	S32 num_lines = mText.size();
	for (text_list_t::iterator iter = mText.begin(); iter != mText.end(); ++iter)
	{
		max_width = llmax(max_width, (S32)fontp->getWidth(*iter));
	}

	S32 left	= mHoverPos.mX + 10;
	S32 top		= mHoverPos.mY - 16;
	S32 right	= mHoverPos.mX + max_width + 30;
	S32 bottom	= mHoverPos.mY - 24 - llfloor(num_lines*fontp->getLineHeight());

	// Push down if there's a one-click icon
	if (mHoverActive
		&& isHoveringObject()
		&& mLastHoverObject->getClickAction() != CLICK_ACTION_NONE)
	{
		const S32 CLICK_OFFSET = 10;
		top -= CLICK_OFFSET;
		bottom -= CLICK_OFFSET;
	}

	// Make sure the rect is completely visible
	LLRect old_rect = getRect();
	setRect( LLRect(left, top, right, bottom ) );
	translateIntoRect( gViewerWindow->getVirtualWindowRect(), FALSE );
	left = getRect().mLeft;
	top = getRect().mTop;
	right = getRect().mRight;
	bottom = getRect().mBottom;
	setRect(old_rect);

	LLGLSUIDefault gls_ui;

	shadow_color.mV[VALPHA] = 0.7f * alpha;
	S32 shadow_offset = gSavedSettings.getS32("DropShadowTooltip");
	shadow_imagep->draw(LLRect(left + shadow_offset, top - shadow_offset, right + shadow_offset, bottom - shadow_offset), shadow_color);

	bg_color.mV[VALPHA] = alpha;
	box_imagep->draw(LLRect(left, top, right, bottom), bg_color);

	S32 cur_offset = top - 4;
	for (text_list_t::iterator iter = mText.begin(); iter != mText.end(); ++iter)
	{
		fontp->renderUTF8(*iter, 0, left + 10, cur_offset, text_color, LLFontGL::LEFT, LLFontGL::TOP);
		cur_offset -= llfloor(fontp->getLineHeight());
	}
}

void LLHoverView::setHoverActive(const BOOL active)
{
	if (active != mHoverActive)
	{
		mHoverTimer.reset();
	}

	mHoverActive = active;

	if (active)
	{
		mHoverPos = gViewerWindow->getCurrentMouse();
	}
}


BOOL LLHoverView::isHoveringLand() const
{
	return !mHoverLandGlobal.isExactlyZero();
}


BOOL LLHoverView::isHoveringObject() const
{
	return !mLastHoverObject.isNull() && !mLastHoverObject->isDead();
}


LLViewerObject* LLHoverView::getLastHoverObject() const
{
	if (!mLastHoverObject.isNull() && !mLastHoverObject->isDead())
	{
		return mLastHoverObject;
	}
	else
	{
		return NULL;
	}
}

// EOF
