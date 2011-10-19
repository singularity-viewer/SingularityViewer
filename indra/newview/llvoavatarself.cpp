/** 
 * @file llvoavatar.cpp
 * @brief Implementation of LLVOAvatar class which is a derivation fo LLViewerObject
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
#include "llviewerprecompiledheaders.h"

#include "llvoavatarself.h"
#include "llvoavatar.h"

#include "pipeline.h"

#include "llagent.h" //  Get state values from here
#include "llagentcamera.h"
#include "llagentwearables.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llnotificationsutil.h"
#include "llselectmgr.h"
#include "lltoolgrab.h"	// for needsRenderBeam
#include "lltoolmgr.h" // for needsRenderBeam
#include "lltoolmorph.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerstats.h"
#include "llviewerregion.h"
#include "llmeshrepository.h"
#include "llvovolume.h"
LLVOAvatarSelf *gAgentAvatarp = NULL;
BOOL isAgentAvatarValid()
{
	return (gAgentAvatarp &&
			(gAgentAvatarp->getRegion() != NULL) &&
			(!gAgentAvatarp->isDead()));
}

/*********************************************************************************
 **                                                                             **
 ** Begin LLVOAvatarSelf Constructor routines
 **
 **/

LLVOAvatarSelf::LLVOAvatarSelf(const LLUUID& id,
							   const LLPCode pcode,
							   LLViewerRegion* regionp) :
	LLVOAvatar(id, pcode, regionp),
	mScreenp(NULL),
	mLastRegionHandle(0),
	mRegionCrossingCount(0)
{
	gAgentWearables.setAvatarObject(this);
	gAgentCamera.setAvatarObject(this);

	mMotionController.mIsSelf = TRUE;

	lldebugs << "Marking avatar as self " << id << llendl;
}

void LLVOAvatarSelf::initInstance()
{
	BOOL status = TRUE;
	// creates hud joint(mScreen) among other things
	status &= loadAvatarSelf();

	// adds attachment points to mScreen among other things
	LLVOAvatar::initInstance();

	llinfos << "Self avatar object created. Starting timer." << llendl;
	/*mDebugSelfLoadTimer.reset();
	// clear all times to -1 for debugging
	for (U32 i =0; i < LLVOAvatarDefines::TEX_NUM_INDICES; ++i)
	{
		for (U32 j = 0; j <= MAX_DISCARD_LEVEL; ++j)
		{
			mDebugTextureLoadTimes[i][j] = -1.0f;
		}
	}

	for (U32 i =0; i < LLVOAvatarDefines::BAKED_NUM_INDICES; ++i)
	{
		mDebugBakedTextureTimes[i][0] = -1.0f;
		mDebugBakedTextureTimes[i][1] = -1.0f;
	}*/

	status &= buildMenus();
	if (!status)
	{
		llerrs << "Unable to load user's avatar" << llendl;
		return;
	}
}

// virtual
void LLVOAvatarSelf::markDead()
{
	mBeam = NULL;
	LLVOAvatar::markDead();
}

/*virtual*/ BOOL LLVOAvatarSelf::loadAvatar()
{
	BOOL success = LLVOAvatar::loadAvatar();
	return success;
}


BOOL LLVOAvatarSelf::loadAvatarSelf()
{
	BOOL success = TRUE;
	// avatar_skeleton.xml
	if (!buildSkeletonSelf(sAvatarSkeletonInfo))
	{
		llwarns << "avatar file: buildSkeleton() failed" << llendl;
		return FALSE;
	}
	// TODO: make loadLayersets() called only by self.
	//success &= loadLayersets();

	return success;
}

BOOL LLVOAvatarSelf::buildSkeletonSelf(const LLVOAvatarSkeletonInfo *info)
{
	LLMemType mt(LLMemType::MTYPE_AVATAR);

	// add special-purpose "screen" joint
	mScreenp = new LLViewerJoint("mScreen", NULL);
	// for now, put screen at origin, as it is only used during special
	// HUD rendering mode
	F32 aspect = LLViewerCamera::getInstance()->getAspect();
	LLVector3 scale(1.f, aspect, 1.f);
	mScreenp->setScale(scale);
	mScreenp->setWorldPosition(LLVector3::zero);
	// need to update screen agressively when sidebar opens/closes, for example
	mScreenp->mUpdateXform = TRUE;
	return TRUE;
}

BOOL LLVOAvatarSelf::buildMenus()
{
	//-------------------------------------------------------------------------
	// build the attach and detach menus
	//-------------------------------------------------------------------------
	if(gNoRender)
		return TRUE;
	// *TODO: Translate
	gAttachBodyPartPieMenus[0] = NULL;
	gAttachBodyPartPieMenus[1] = new LLPieMenu(std::string("Right Arm >"));
	gAttachBodyPartPieMenus[2] = new LLPieMenu(std::string("Head >"));
	gAttachBodyPartPieMenus[3] = new LLPieMenu(std::string("Left Arm >"));
	gAttachBodyPartPieMenus[4] = NULL;
	gAttachBodyPartPieMenus[5] = new LLPieMenu(std::string("Left Leg >"));
	gAttachBodyPartPieMenus[6] = new LLPieMenu(std::string("Torso >"));
	gAttachBodyPartPieMenus[7] = new LLPieMenu(std::string("Right Leg >"));

	gDetachBodyPartPieMenus[0] = NULL;
	gDetachBodyPartPieMenus[1] = new LLPieMenu(std::string("Right Arm >"));
	gDetachBodyPartPieMenus[2] = new LLPieMenu(std::string("Head >"));
	gDetachBodyPartPieMenus[3] = new LLPieMenu(std::string("Left Arm >"));
	gDetachBodyPartPieMenus[4] = NULL;
	gDetachBodyPartPieMenus[5] = new LLPieMenu(std::string("Left Leg >"));
	gDetachBodyPartPieMenus[6] = new LLPieMenu(std::string("Torso >"));
	gDetachBodyPartPieMenus[7] = new LLPieMenu(std::string("Right Leg >"));

	for (S32 i = 0; i < 8; i++)
	{
		if (gAttachBodyPartPieMenus[i])
		{
			gAttachPieMenu->appendPieMenu( gAttachBodyPartPieMenus[i] );
		}
		else
		{
			BOOL attachment_found = FALSE;
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
				 iter != mAttachmentPoints.end();
				 ++iter)
			{
				LLViewerJointAttachment* attachment = iter->second;
				if (attachment->getGroup() == i)
				{
					LLMenuItemCallGL* item;
// [RLVa:KB] - Checked: 2009-07-06 (RLVa-1.0.0c)
					// We need the userdata param to disable options in this pie menu later on (Left Hand / Right Hand option)
					item = new LLMenuItemCallGL(attachment->getName(), 
												NULL, 
												object_selected_and_point_valid, attachment);
// [/RLVa:KB]
//						item = new LLMenuItemCallGL(attachment->getName(), 
//													NULL, 
//													object_selected_and_point_valid);
					item->addListener(gMenuHolder->getListenerByName("Object.AttachToAvatar"), "on_click", iter->first);
					
					gAttachPieMenu->append(item);

					attachment_found = TRUE;
					break;

				}
			}







			if (!attachment_found)
			{
				gAttachPieMenu->appendSeparator();
			}
		}

		if (gDetachBodyPartPieMenus[i])
		{
			gDetachPieMenu->appendPieMenu( gDetachBodyPartPieMenus[i] );
		}
		else
		{
			BOOL attachment_found = FALSE;
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
				 iter != mAttachmentPoints.end();
				 ++iter)
			{
				LLViewerJointAttachment* attachment = iter->second;
				if (attachment->getGroup() == i)
				{
					gDetachPieMenu->append(new LLMenuItemCallGL(attachment->getName(),
						&handle_detach_from_avatar, object_attached, attachment));

					attachment_found = TRUE;
					break;
				}
			}

			if (!attachment_found)
			{
				gDetachPieMenu->appendSeparator();
			}
		}
	}

	// add screen attachments
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin();
		 iter != mAttachmentPoints.end();
		 ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment->getGroup() == 8)
		{
			LLMenuItemCallGL* item;
// [RLVa:KB] - Checked: 2009-07-06 (RLVa-1.0.0c)
			// We need the userdata param to disable options in this pie menu later on
			item = new LLMenuItemCallGL(attachment->getName(), 
										NULL, 
										object_selected_and_point_valid, attachment);
// [/RLVa:KB]
//				item = new LLMenuItemCallGL(attachment->getName(), 
//											NULL, 
//											object_selected_and_point_valid);
			item->addListener(gMenuHolder->getListenerByName("Object.AttachToAvatar"), "on_click", iter->first);
			gAttachScreenPieMenu->append(item);
			gDetachScreenPieMenu->append(new LLMenuItemCallGL(attachment->getName(), 
						&handle_detach_from_avatar, object_attached, attachment));
		}
	}



	for (S32 pass = 0; pass < 2; pass++)
	{
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
			 iter != mAttachmentPoints.end();
			 ++iter)
		{
			LLViewerJointAttachment* attachment = iter->second;
			if (attachment->getIsHUDAttachment() != (pass == 1))
			{
				continue;
			}
			// RELEASE-RLVa: random comment because we want know if LL ever changes this to not include "attachment" as userdata
			LLMenuItemCallGL* item = new LLMenuItemCallGL(attachment->getName(), 
														  NULL, &object_selected_and_point_valid,
														  &attach_label, attachment);
			item->addListener(gMenuHolder->getListenerByName("Object.AttachToAvatar"), "on_click", iter->first);
			gAttachSubMenu->append(item);

			gDetachSubMenu->append(new LLMenuItemCallGL(attachment->getName(), 
				&handle_detach_from_avatar, object_attached, &detach_label, attachment));
			
		}
		if (pass == 0)
		{
			// put separator between non-hud and hud attachments
			gAttachSubMenu->appendSeparator();
			gDetachSubMenu->appendSeparator();
		}
	}

	for (S32 group = 0; group < 8; group++)
	{
		// skip over groups that don't have sub menus
		if (!gAttachBodyPartPieMenus[group] || !gDetachBodyPartPieMenus[group])
		{
			continue;
		}

		std::multimap<S32, S32> attachment_pie_menu_map;

		// gather up all attachment points assigned to this group, and throw into map sorted by pie slice number
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
			 iter != mAttachmentPoints.end();
			 ++iter)
		{
			LLViewerJointAttachment* attachment = iter->second;
			if(attachment->getGroup() == group)
			{
				// use multimap to provide a partial order off of the pie slice key
				S32 pie_index = attachment->getPieSlice();
				attachment_pie_menu_map.insert(std::make_pair(pie_index, iter->first));
			}
		}

		// add in requested order to pie menu, inserting separators as necessary
		S32 cur_pie_slice = 0;
		for (std::multimap<S32, S32>::iterator attach_it = attachment_pie_menu_map.begin();
			 attach_it != attachment_pie_menu_map.end(); ++attach_it)
		{
			S32 requested_pie_slice = attach_it->first;
			S32 attach_index = attach_it->second;
			while (cur_pie_slice < requested_pie_slice)
			{
				gAttachBodyPartPieMenus[group]->appendSeparator();
				gDetachBodyPartPieMenus[group]->appendSeparator();
				cur_pie_slice++;
			}

			LLViewerJointAttachment* attachment = get_if_there(mAttachmentPoints, attach_index, (LLViewerJointAttachment*)NULL);
			if (attachment)
			{
// [RLVa:KB] - Checked: 2009-07-06 (RLVa-1.0.0c)
				// We need the userdata param to disable options in this pie menu later on
				LLMenuItemCallGL* item = new LLMenuItemCallGL(attachment->getName(), 
															  NULL, object_selected_and_point_valid, attachment);
// [/RLVa:KB]
//					LLMenuItemCallGL* item = new LLMenuItemCallGL(attachment->getName(), 
//																  NULL, object_selected_and_point_valid);
				gAttachBodyPartPieMenus[group]->append(item);
				item->addListener(gMenuHolder->getListenerByName("Object.AttachToAvatar"), "on_click", attach_index);
				gDetachBodyPartPieMenus[group]->append(new LLMenuItemCallGL(attachment->getName(), 
																			&handle_detach_from_avatar,
																			object_attached, attachment));
				cur_pie_slice++;
			}
		}
	}
	return TRUE;
}

void LLVOAvatarSelf::cleanup()
{
	markDead();
 	delete mScreenp;
 	mScreenp = NULL;
	mRegionp = NULL;
}

LLVOAvatarSelf::~LLVOAvatarSelf()
{
	cleanup();
}
BOOL LLVOAvatarSelf::updateCharacter(LLAgent &agent)
{
	LLMemType mt(LLMemType::MTYPE_AVATAR);

	// update screen joint size
	if (mScreenp)
	{
		F32 aspect = LLViewerCamera::getInstance()->getAspect();
		LLVector3 scale(1.f, aspect, 1.f);
		mScreenp->setScale(scale);
		mScreenp->updateWorldMatrixChildren();
		resetHUDAttachments();
	}
	
	return LLVOAvatar::updateCharacter(agent);
}

// virtual
BOOL LLVOAvatarSelf::idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time)
{
	if (!isAgentAvatarValid())
	{
		return TRUE;
	}
	if(!gNoRender)
	{
		//Emerald performs some force-bakes stuff here. Added it in because we noticed slow responses with client tag ident. -HgB
		for(U8 i=0;i<getNumTEs();++i)
		{
			LLViewerTexture* te = getTEImage(i);
			te->forceActive();
		}
	}
	LLVOAvatar::idleUpdate(agent,world,time);
	if(!gNoRender)
		idleUpdateTractorBeam();
	return TRUE;	
}

// virtual
LLJoint *LLVOAvatarSelf::getJoint(const std::string &name)
{
	if (mScreenp)
	{
		LLJoint* jointp = mScreenp->findJoint(name);
		if (jointp) return jointp;
	}
	return LLVOAvatar::getJoint(name);
}
//virtual
void LLVOAvatarSelf::resetJointPositions( void )
{
	return LLVOAvatar::resetJointPositions();
}
// virtual
void LLVOAvatarSelf::requestStopMotion(LLMotion* motion)
{
	// Only agent avatars should handle the stop motion notifications.

	// Notify agent that motion has stopped
	gAgent.requestStopMotion(motion);
}

// virtual
void LLVOAvatarSelf::stopMotionFromSource(const LLUUID& source_id)
{
	for(AnimSourceIterator motion_it = mAnimationSources.find(source_id); motion_it != mAnimationSources.end();)
	{
		gAgent.sendAnimationRequest( motion_it->second, ANIM_REQUEST_STOP );
		mAnimationSources.erase(motion_it++);
	}

	LLViewerObject* object = gObjectList.findObject(source_id);
	if (object)
	{
		object->mFlags &= ~FLAGS_ANIM_SOURCE;
	}
}
//virtual
void LLVOAvatarSelf::removeMissingBakedTextures()
{
	BOOL removed = FALSE;
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		const S32 te = mBakedTextureDatas[i].mTextureIndex;
		const LLViewerTexture* tex = getTEImage(te);

		// Replace with default if we can't find the asset, assuming the
		// default is actually valid (which it should be unless something
		// is seriously wrong).
		if (!tex || tex->isMissingAsset())
		{
			LLViewerTexture *imagep = LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
			if (imagep)
			{
				setTEImage(te, imagep);
				removed = TRUE;
			}
		}
	}

	if (removed)
	{
		for(U32 i = 0; i < mBakedTextureDatas.size(); i++)
		{
			invalidateComposite(mBakedTextureDatas[i].mTexLayerSet, FALSE);
		}
		updateMeshTextures();
		requestLayerSetUploads();
	}
}

//virtual
void LLVOAvatarSelf::updateRegion(LLViewerRegion *regionp)
{
	// Save the global position
	LLVector3d global_pos_from_old_region = getPositionGlobal();

	// Change the region
	setRegion(regionp);

	if (regionp)
	{	// Set correct region-relative position from global coordinates
		setPositionGlobal(global_pos_from_old_region);

		// Diagnostic info
		//LLVector3d pos_from_new_region = getPositionGlobal();
		//llinfos << "pos_from_old_region is " << global_pos_from_old_region
		//	<< " while pos_from_new_region is " << pos_from_new_region
		//	<< llendl;
	}

	if (!regionp || (regionp->getHandle() != mLastRegionHandle))
	{
		if (mLastRegionHandle != 0)
		{
			++mRegionCrossingCount;
			F64 delta = (F64)mRegionCrossingTimer.getElapsedTimeF32();
			F64 avg = (mRegionCrossingCount == 1) ? 0 : LLViewerStats::getInstance()->getStat(LLViewerStats::ST_CROSSING_AVG);
			F64 delta_avg = (delta + avg*(mRegionCrossingCount-1)) / mRegionCrossingCount;
			LLViewerStats::getInstance()->setStat(LLViewerStats::ST_CROSSING_AVG, delta_avg);

			F64 max = (mRegionCrossingCount == 1) ? 0 : LLViewerStats::getInstance()->getStat(LLViewerStats::ST_CROSSING_MAX);
			max = llmax(delta, max);
			LLViewerStats::getInstance()->setStat(LLViewerStats::ST_CROSSING_MAX, max);

			// Diagnostics
			llinfos << "Region crossing took " << (F32)(delta * 1000.0) << " ms " << llendl;
		}
		if (regionp)
		{
			mLastRegionHandle = regionp->getHandle();
		}
	}
	mRegionCrossingTimer.reset();
	LLViewerObject::updateRegion(regionp);
}

//--------------------------------------------------------------------
// draw tractor beam when editing objects
//--------------------------------------------------------------------
//virtual
void LLVOAvatarSelf::idleUpdateTractorBeam()
{


	if(gSavedSettings.getBOOL("DisablePointAtAndBeam"))
	{
		return;
	}
	// </edit>

	const LLPickInfo& pick = gViewerWindow->getLastPick();

	// No beam for media textures
	// TODO: this will change for Media on a Prim
	if(pick.getObject() && pick.mObjectFace >= 0)
	{
		const LLTextureEntry* tep = pick.getObject()->getTE(pick.mObjectFace);
		if (tep && LLViewerMedia::textureHasMedia(tep->getID()))
		{
			return;
		}
	}

	// This is only done for yourself (maybe it should be in the agent?)
	if (!needsRenderBeam() || !mIsBuilt)
	{
		mBeam = NULL;
	}
	else if (!mBeam || mBeam->isDead())
	{
		// VEFFECT: Tractor Beam
		mBeam = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM);
		mBeam->setColor(LLColor4U(gAgent.getEffectColor()));
		mBeam->setSourceObject(this);
		mBeamTimer.reset();
	}

	if (!mBeam.isNull())
	{
		LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();

		if (gAgentCamera.mPointAt.notNull())
		{
			// get point from pointat effect
			mBeam->setPositionGlobal(gAgentCamera.mPointAt->getPointAtPosGlobal());
			mBeam->triggerLocal();
		}
		else if (selection->getFirstRootObject() && 
				selection->getSelectType() != SELECT_TYPE_HUD)
		{
			LLViewerObject* objectp = selection->getFirstRootObject();
			mBeam->setTargetObject(objectp);
		}
		else
		{
			mBeam->setTargetObject(NULL);
			LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();
			if (tool->isEditing())
			{
				if (tool->getEditingObject())
				{
					mBeam->setTargetObject(tool->getEditingObject());
				}
				else
				{
					mBeam->setPositionGlobal(tool->getEditingPointGlobal());
				}
			}
			else
			{
				
				mBeam->setPositionGlobal(pick.mPosGlobal);
			}

		}
		if (mBeamTimer.getElapsedTimeF32() > 0.25f)
		{
			mBeam->setColor(LLColor4U(gAgent.getEffectColor()));
			mBeam->setNeedsSendToSim(TRUE);
			mBeamTimer.reset();
		}
	}
}
//-----------------------------------------------------------------------------
// restoreMeshData()
//-----------------------------------------------------------------------------
// virtual
void LLVOAvatarSelf::restoreMeshData()
{
	LLMemType mt(LLMemType::MTYPE_AVATAR);
	
	//llinfos << "Restoring" << llendl;
	mMeshValid = TRUE;
	updateJointLODs();
	updateAttachmentVisibility(gAgentCamera.getCameraMode());

	// force mesh update as LOD might not have changed to trigger this
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY, TRUE);
}
//------------------------------------------------------------------------
// needsRenderBeam()
//------------------------------------------------------------------------
BOOL LLVOAvatarSelf::needsRenderBeam()
{
	if (gNoRender)
	{
		return FALSE;
	}
	LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();

	BOOL is_touching_or_grabbing = (tool == LLToolGrab::getInstance() && LLToolGrab::getInstance()->isEditing());
	if (LLToolGrab::getInstance()->getEditingObject() && 
		LLToolGrab::getInstance()->getEditingObject()->isAttachment())
	{
		// don't render selection beam on hud objects
		is_touching_or_grabbing = FALSE;
	}
	return is_touching_or_grabbing || (mState & AGENT_STATE_EDITING && LLSelectMgr::getInstance()->shouldShowSelection());
}