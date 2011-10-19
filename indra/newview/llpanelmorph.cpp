/** 
 * @file llpanelmorph.cpp
 * @brief LLPanelMorph class implementation
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

// file include
#include "llviewerprecompiledheaders.h"

/*
#include "llpanelmorph.h"

// library includes
#include "llerror.h"
#include "llrect.h"
#include "llstring.h"
#include "llfontgl.h"

// project includes
#include "llui.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "lltextbox.h"
#include "llbutton.h"
//#include "lldraghandle.h"

#include "llviewerwindow.h"
#include "llviewerobject.h"
#include "llresmgr.h"

#include "llcontrol.h"

#include "llagent.h"
#include "llvoavatar.h"
#include "lltoolmgr.h"
#include "lltoolmorph.h"
#include "llquantize.h"

//
// Constants
//
const S32 VPAD = 2;
const S32 HPAD = 4;

const S32 TITLE_WIDTH = 100;
const S32 TITLE_HEIGHT = 16;

const S32 CLOSE_BOX_SIZE = 16;

const S32 LABEL_HOFF = 8;
const S32 LABEL_WIDTH = 140;
const S32 SPIN_WIDTH = 250;

// minimum and maximum morph weight values (a UI/network constraint, not inherent to morphs)
const F32 MORPH_WEIGHT_MIN = (-1.f);
const F32 MORPH_WEIGHT_MAX = (1.f);
// this is the magic number that morph values are quantized to for network transmission purposes
// the 200 value is a result of encoding the weights as a string (and avoiding NULL terminators
// and avoiding \n (the between-NV-pair delimiter).  Map -1 to 1 to 50-250.  HACK.  JC
const S32 MORPH_WEIGHT_OFFSET = (50);
const S32 MORPH_WEIGHT_QUANTUM = ((MORPH_WEIGHT_MAX - MORPH_WEIGHT_MIN) / MAXSTRINGVAL);

//#define MORPH_PANEL_SHOW_SPINNERS

//
// Globals
//
LLPanelMorph *gPanelMorph = NULL;

//
// Methods
//

//static
//-----------------------------------------------------------------------------
// LLPanelMorph()
//-----------------------------------------------------------------------------
LLPanelMorph::LLPanelMorph(const std::string& name, const LLRect& rect)
:	LLPanel( name, rect )
{
	mParamSpinners = NULL;
	mSexRadioGroup = NULL;
	mRect = rect;
	mMesh = NULL;
	setBackgroundVisible( TRUE );
}

//-----------------------------------------------------------------------------
// ~LLPanelMorph()
//-----------------------------------------------------------------------------
LLPanelMorph::~LLPanelMorph()
{
#ifdef MORPH_PANEL_SHOW_SPINNERS
	for (S32 i = 0; i < mNumParamSpinners; i++)
	{
		removeChild(mParamSpinners[i]);
		delete mParamSpinners[i];
	}
	delete [] mParamSpinners;
#endif
}

// static
//-----------------------------------------------------------------------------
// onCommitSexChange()
//-----------------------------------------------------------------------------
void LLPanelMorph::onCommitSexChange( void *data)
{
//	LLRadioGroup *group = (LLRadioGroup *)data;

//	LLVOAvatar *avatarp = gAgentAvatarp;
//	
//	LLNameValue* avatarSexNV = avatarp->getNVPair("AvatarSex");
//	if (!avatarSexNV)
//	{
//		avatarp->addNVPair( "AvatarSex U32 READ_WRITE SIM_SPACE_VIEWER 0");
//		avatarSexNV = avatarp->getNVPair("AvatarSex");
//	}
//
//	avatarSexNV->setU32(gSavedSettings.getU32("AvatarSex"));
//
//	avatarp->sendNVPairToSimulator("AvatarSex");
//
//	// preemptively update sex based on local value
//	avatarp->updateSex();
//
//	LLVisualParamHint::requestHintUpdates();

}


// static
//-----------------------------------------------------------------------------
// onCommitMorphChange()
//-----------------------------------------------------------------------------
void LLPanelMorph::onCommitMorphChange( LLUICtrl* ctrl, void* userdata )
{
//#ifdef MORPH_PANEL_SHOW_SPINNERS
//	LLVOAvatar *avatarp = gAgentAvatarp;
//
//	LLPanelMorph* self = (LLPanelMorph*) userdata;
//
//	S32 i = 0;
//	for(LLPolyMorphTarget* morphTargetp = self->mMesh->getFirstVisualParam();
//	morphTargetp;
//	morphTargetp = self->mMesh->getNextVisualParam())
//	{
//		for(S32 j = 0; j < self->mNumParamSpinners; j++)
//		{
//			if (!strcmp(self->mParamSpinners[j]->getName(), morphTargetp->getName()))
//			{
//				avatarp->mHeadMesh0.getMesh()->setDistortionWeight(morphTargetp, self->mParamSpinners[j]->get());
//			}
//		}
//		i++;
//	}
//	avatarp->updateDistortions();
//	avatarp->sendVisualParams();
//#endif
}

//-----------------------------------------------------------------------------
// updateSpinners()
//-----------------------------------------------------------------------------
void LLPanelMorph::updateSpinners(LLPolyMesh *mesh)
{
	if (mesh != mMesh) return;
#ifdef MORPH_PANEL_SHOW_SPINNERS

	for(LLViewerVisualParam *param = (LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
		param;
		param = gAgentAvatarp->getNextVisualParam())
	{
		if (param->getID() == -1 ||
			param->getGroup() != LLVisualParam::VISUAL_PARAM_GROUP_TWEAKABLE) continue;

		F32 paramWeight = gAgentAvatarp->getVisualParamWeight(morphTargetp);

		for (S32 i = 0; i < mNumParamSpinners; i++)
		{
			if (!strcmp(mParamSpinners[i]->getName() , morphTargetp->getName()))
			{
				mParamSpinners[i]->set(paramWeight);
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// createSpinners()
//-----------------------------------------------------------------------------
void LLPanelMorph::createSpinners(LLPolyMesh *mesh)
{
	const LLFontGL* font = LLResMgr::getInstance()->getRes( LLFONT_SANSSERIF_SMALL );
	S32 cur_y;
	S32 new_height;
	S32 control_focus = -1;

	if (mSexRadioGroup)
	{
		removeChild(mSexRadioGroup);
		delete mSexRadioGroup;
	}

	if (mParamSpinners)
	{
		for (S32 i = 0; i < mNumParamSpinners; i++)
		{
			if (mParamSpinners[i]->hasFocus())
			{
				control_focus = i;
				mParamSpinners[i]->setFocus(FALSE);
			}
		}
		for (i = 0; i < mNumParamSpinners; i++)
		{
			removeChild(mParamSpinners[i]);
			delete mParamSpinners[i];
		}
		delete [] mParamSpinners;
		mParamSpinners = NULL;
	}
	
#ifdef MORPH_PANEL_SHOW_SPINNERS
	S32 numSpinners = 0;

	for(LLViewerVisualParam *param = (LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
		param;
		param = gAgentAvatarp->getNextVisualParam())
	{
		if(param->getID() != -1 &&
			param->getGroup() == LLVisualParam::VISUAL_PARAM_GROUP_TWEAKABLE)
		{
			numSpinners++;
		}
	}

	if (numSpinners != 0)
	{
		mParamSpinners = new LLSpinCtrl*[numSpinners];
	}

	// set focus back to control #n
	if (control_focus >= 0 && control_focus < numSpinners)
	{
		mParamSpinners[control_focus]->setFocus(TRUE);
	}
	new_height = numSpinners * (SPINCTRL_HEIGHT + VPAD) + (VPAD * 3) + CHECKBOXCTRL_HEIGHT + VPAD + VPAD;

#else
	new_height = (VPAD * 3) + CHECKBOXCTRL_HEIGHT + VPAD + VPAD;
#endif

	LLRect panelRect = getRect();
	reshape(panelRect.getWidth(), new_height, TRUE);

	cur_y = new_height - VPAD - VPAD;

	LLRect radio_rect;
	radio_rect.setLeftTopAndSize(HPAD + LABEL_HOFF, 
		cur_y, 
		getRect().getWidth() - HPAD - HPAD - LABEL_HOFF, 
		CHECKBOXCTRL_HEIGHT + VPAD);
	mSexRadioGroup = new LLRadioGroup("sex radio", 
		radio_rect, 
		"AvatarSex", 
		&LLPanelMorph::onCommitSexChange, 
		(void *)this);
	addChild(mSexRadioGroup);
	
	radio_rect.setLeftTopAndSize(HPAD + LABEL_HOFF, 
		0, 
		(getRect().getWidth() - HPAD - HPAD - LABEL_HOFF) / 2.f, 
		0);
	mSexRadioGroup->addRadioButton("Female", "Female", radio_rect, font);

	radio_rect.setLeftTopAndSize(((getRect().getWidth() - HPAD - HPAD - LABEL_HOFF) / 2.f) + HPAD + LABEL_HOFF, 
		0, 
		(getRect().getWidth() - HPAD - HPAD - LABEL_HOFF) / 2.f, 
		0);
	mSexRadioGroup->addRadioButton("Male", "Male", radio_rect, font);

	cur_y -= CHECKBOXCTRL_HEIGHT + VPAD + VPAD;

#ifdef MORPH_PANEL_SHOW_SPINNERS
	S32 i = 0;

	for(LLViewerVisualParam *param = (LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
		param;
		param = gAgentAvatarp->getNextVisualParam())
	{
		if (param->getID() == -1 ||
			param->getGroup() != LLVisualParam::VISUAL_PARAM_GROUP_TWEAKABLE) continue;
		LLRect spinnerRect;
		spinnerRect.setLeftTopAndSize(HPAD + LABEL_HOFF, 
			cur_y, 
			getRect().getWidth() - HPAD - HPAD - LABEL_HOFF, 
			cur_y - SPINCTRL_HEIGHT);
		mParamSpinners[i] = new LLSpinCtrl(param->getName(),
			spinnerRect,
			font,
			param->getName(),
			&LLPanelMorph::onCommitMorphChange, 
			this,
			param->getWeight(),
			param->getMinWeight(), 
			param->getMaxWeight(), 
			MORPH_WEIGHT_QUANTUM * 2, 
			NULL, 
			LABEL_WIDTH );

		mParamSpinners[i]->setPrecision(2);
		mParamSpinners[i]->setFollowsTop();
		mParamSpinners[i]->setFollowsLeft();
		addChild(mParamSpinners[i]); 
		cur_y -= VPAD + SPINCTRL_HEIGHT;
		i++;
	}

	mNumParamSpinners = numSpinners;
#endif
	mMesh = mesh;
}
*/
