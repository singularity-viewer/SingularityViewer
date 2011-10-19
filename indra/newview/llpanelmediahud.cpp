/** 
 * @file llpanelmsgs.cpp
 * @brief Message popup preferences panel
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

//LLPanelMediaHUD
#include "llagent.h"
#include "llagentcamera.h"
#include "llparcel.h"
#include "llpanel.h"
#include "llselectmgr.h"
#include "llrender.h"
#include "lldrawable.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "lluictrlfactory.h"
#include "llbutton.h"
#include "llface.h"
#include "llhudview.h"
#include "lliconctrl.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llpanelmediahud.h"
#include "llpluginclassmedia.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewermedia.h"
#include "llviewermediafocus.h"
#include "llvovolume.h"
#include "llweb.h"

glh::matrix4f glh_get_current_modelview();
glh::matrix4f glh_get_current_projection();

const F32 ZOOM_NEAR_PADDING		= 1.0f;
const F32 ZOOM_MEDIUM_PADDING	= 1.2f;
const F32 ZOOM_FAR_PADDING		= 1.5f;

//
// LLPanelMediaHUD
//

LLPanelMediaHUD::LLPanelMediaHUD(viewer_media_t media_impl)
 :	mMediaImpl(media_impl)
{
	mMediaFocus = false;
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_media_hud.xml");
	mMouseMoveTimer.reset();
	mFadeTimer.stop();
	mCurrentZoom = ZOOM_NONE;
	mScrollState = SCROLL_NONE;

	mPanelHandle.bind(this);
}
LLPanelMediaHUD::~LLPanelMediaHUD()
{
	mMediaImpl = NULL;
}

BOOL LLPanelMediaHUD::postBuild()
{
	LLButton* close_btn = getChild<LLButton>("close");
	close_btn->setClickedCallback(onClickClose, this);

	LLButton* back_btn = getChild<LLButton>("back");
	back_btn->setClickedCallback(onClickBack, this);

	LLButton* fwd_btn = getChild<LLButton>("fwd");
	fwd_btn->setClickedCallback(onClickForward, this);

	LLButton* home_btn = getChild<LLButton>("home");
	home_btn->setClickedCallback(onClickHome, this);

	LLButton* stop_btn = getChild<LLButton>("stop");
	stop_btn->setClickedCallback(onClickStop, this);

	LLButton* media_stop_btn = getChild<LLButton>("media_stop");
	media_stop_btn->setClickedCallback(onClickStop, this);

	LLButton* reload_btn = getChild<LLButton>("reload");
	reload_btn->setClickedCallback(onClickReload, this);

	LLButton* play_btn = getChild<LLButton>("play");
	play_btn->setClickedCallback(onClickPlay, this);

	LLButton* pause_btn = getChild<LLButton>("pause");
	pause_btn->setClickedCallback(onClickPause, this);

	LLButton* open_btn = getChild<LLButton>("new_window");
	open_btn->setClickedCallback(onClickOpen, this);

	LLButton* zoom_btn = getChild<LLButton>("zoom_frame");
	zoom_btn->setClickedCallback(onClickZoom, this);

	LLButton* open_btn_h = getChild<LLButton>("new_window_hover");
	open_btn_h->setClickedCallback(onClickOpen, this);

	LLButton* zoom_btn_h = getChild<LLButton>("zoom_frame_hover");
	zoom_btn_h->setClickedCallback(onClickZoom, this);

	LLButton* scroll_up_btn = getChild<LLButton>("scrollup");
	scroll_up_btn->setClickedCallback(onScrollUp, this);
	scroll_up_btn->setHeldDownCallback(onScrollUpHeld);
	scroll_up_btn->setMouseUpCallback(onScrollStop);
	LLButton* scroll_left_btn = getChild<LLButton>("scrollleft");
	scroll_left_btn->setClickedCallback(onScrollLeft, this);
	scroll_left_btn->setHeldDownCallback(onScrollLeftHeld);
	scroll_left_btn->setMouseUpCallback(onScrollStop);
	LLButton* scroll_right_btn = getChild<LLButton>("scrollright");
	scroll_right_btn->setClickedCallback(onScrollRight, this);
	scroll_right_btn->setHeldDownCallback(onScrollLeftHeld);
	scroll_right_btn->setMouseUpCallback(onScrollStop);
	LLButton* scroll_down_btn = getChild<LLButton>("scrolldown");
	scroll_down_btn->setClickedCallback(onScrollDown, this);
	scroll_down_btn->setHeldDownCallback(onScrollDownHeld);
	scroll_down_btn->setMouseUpCallback(onScrollStop);

	mMouseInactiveTime = gSavedSettings.getF32("MediaControlTimeout");
	mControlFadeTime = gSavedSettings.getF32("MediaControlFadeTime");

	mCurrentZoom = ZOOM_NONE;
	// clicks on HUD buttons do not remove keyboard focus from media
	setIsChrome(TRUE);
	return TRUE;
}

void LLPanelMediaHUD::updateShape()
{
	const S32 MIN_HUD_WIDTH=200;
	const S32 MIN_HUD_HEIGHT=120;

	LLPluginClassMedia* media_plugin = NULL;
	if(mMediaImpl.notNull() && mMediaImpl->hasMedia())
	{
		media_plugin = mMediaImpl->getMediaPlugin();
	}

	// Early out for no media plugin
	if(media_plugin == NULL)
	{
		setVisible(FALSE);
		return;
	}

	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	bool can_navigate = parcel->getMediaAllowNavigate();

	// LLObjectSelectionHandle selection = LLViewerMediaFocus::getInstance()->getSelection();

	LLSelectNode* nodep = mMediaFocus ? LLSelectMgr::getInstance()->getSelection()->getFirstNode() : LLSelectMgr::getInstance()->getHoverNode();
	if(! nodep)
	{
		return;
	}
	setVisible(FALSE);
	LLViewerObject* objectp = nodep->getObject();

	if (objectp)
	{

		// Set the state of the buttons
		LLButton* back_btn = getChild<LLButton>("back");
		LLButton* fwd_btn = getChild<LLButton>("fwd");
		LLButton* reload_btn = getChild<LLButton>("reload");
		LLButton* play_btn = getChild<LLButton>("play");
		LLButton* pause_btn = getChild<LLButton>("pause");
		LLButton* stop_btn = getChild<LLButton>("stop");
		LLButton* media_stop_btn = getChild<LLButton>("media_stop");
		LLButton* home_btn = getChild<LLButton>("home");
		LLButton* close_btn = getChild<LLButton>("close");
		LLButton* open_btn = getChild<LLButton>("new_window");
		LLPanel* media_focused_panel = getChild<LLPanel>("media_focused_controls");
		LLPanel* media_hover_panel = getChild<LLPanel>("media_hover_controls");
		back_btn->setVisible(true);
		fwd_btn->setVisible(true);
		reload_btn->setVisible(true);
		stop_btn->setVisible(false);
		home_btn->setVisible(true);
		close_btn->setVisible(true);
		open_btn->setVisible(true);


		if(mMediaFocus)
		{
			back_btn->setEnabled(mMediaImpl->canNavigateBack() && can_navigate);
			fwd_btn->setEnabled(mMediaImpl->canNavigateForward() && can_navigate);
			stop_btn->setEnabled(can_navigate);
			home_btn->setEnabled(can_navigate);
			LLPluginClassMediaOwner::EMediaStatus result = media_plugin->getStatus();

			if(media_plugin->pluginSupportsMediaTime())
			{
				reload_btn->setEnabled(FALSE);
				reload_btn->setVisible(FALSE);
				media_stop_btn->setVisible(TRUE);
				home_btn->setVisible(FALSE);
				back_btn->setEnabled(TRUE);
				fwd_btn->setEnabled(TRUE);
				switch(result)
				{
				case LLPluginClassMediaOwner::MEDIA_PLAYING:
					play_btn->setEnabled(FALSE);
					play_btn->setVisible(FALSE);
					pause_btn->setEnabled(TRUE);
					pause_btn->setVisible(TRUE);
					media_stop_btn->setEnabled(TRUE);
					break;
				case LLPluginClassMediaOwner::MEDIA_PAUSED:
				default:
					pause_btn->setEnabled(FALSE);
					pause_btn->setVisible(FALSE);
					play_btn->setEnabled(TRUE);
					play_btn->setVisible(TRUE);
					media_stop_btn->setEnabled(FALSE);
					break;
				}
			}
			else
			{
				play_btn->setVisible(FALSE);
				pause_btn->setVisible(FALSE);
				media_stop_btn->setVisible(FALSE);
				if(result == LLPluginClassMediaOwner::MEDIA_LOADING)
				{
					reload_btn->setEnabled(FALSE);
					reload_btn->setVisible(FALSE);
					stop_btn->setEnabled(TRUE);
					stop_btn->setVisible(TRUE);
				}
				else
				{
					reload_btn->setEnabled(TRUE);
					reload_btn->setVisible(TRUE);
					stop_btn->setEnabled(FALSE);
					stop_btn->setVisible(FALSE);
				}
			}
		}
		media_focused_panel->setVisible(mMediaFocus);
		media_hover_panel->setVisible(!mMediaFocus);

		if(media_plugin == NULL)
		// Handle Scrolling
		switch (mScrollState) 
		{
		case SCROLL_UP:
			media_plugin->scrollEvent(0, -1, MASK_NONE);
			break;
		case SCROLL_DOWN:
			media_plugin->scrollEvent(0, 1, MASK_NONE);
			break;
		case SCROLL_LEFT:
			mMediaImpl->handleKeyHere(KEY_LEFT, MASK_NONE);
			break;
		case SCROLL_RIGHT:
			mMediaImpl->handleKeyHere(KEY_RIGHT, MASK_NONE);
			break;
		case SCROLL_NONE:
		default:
			break;
		}
		LLBBox screen_bbox;
		setVisible(TRUE);
		glh::matrix4f mat = glh_get_current_projection()*glh_get_current_modelview();
		std::vector<LLVector3>::iterator vert_it;
		std::vector<LLVector3>::iterator vert_end;
		std::vector<LLVector3> vect_face;

		LLVolume* volume = objectp->getVolume();

		if (volume)
		{
			const LLVolumeFace& vf = volume->getVolumeFace(nodep->getLastSelectedTE());

			const LLVector3* ext = (LLVector3*)vf.mExtents->getF32ptr();

			LLVector3 center = (ext[0]+ext[1])*0.5f;
			LLVector3 size = (ext[1]-ext[0])*0.5f;
			LLVector3 vert[] =
			{
				center + size.scaledVec(LLVector3(1,1,1)),
				center + size.scaledVec(LLVector3(-1,1,1)),
				center + size.scaledVec(LLVector3(1,-1,1)),
				center + size.scaledVec(LLVector3(-1,-1,1)),
				center + size.scaledVec(LLVector3(1,1,-1)),
				center + size.scaledVec(LLVector3(-1,1,-1)),
				center + size.scaledVec(LLVector3(1,-1,-1)),
				center + size.scaledVec(LLVector3(-1,-1,-1)),
			};

			LLVOVolume* vo = (LLVOVolume*) objectp;

			for (U32 i = 0; i < 8; i++)
			{
				vect_face.push_back(vo->volumePositionToAgent(vert[i]));	
			}
		}
		vert_it = vect_face.begin();
		vert_end = vect_face.end();

		LLVector3 min = LLVector3(1,1,1);
		LLVector3 max = LLVector3(-1,-1,-1);
		for(; vert_it != vert_end; ++vert_it)
		{
			// project silhouette vertices into screen space
			glh::vec3f screen_vert = glh::vec3f(vert_it->mV); 
			mat.mult_matrix_vec(screen_vert);

			// add to screenspace bounding box
			update_min_max(min, max, LLVector3(screen_vert.v));
		}

		LLCoordGL screen_min;
		screen_min.mX = llround((F32)gViewerWindow->getWindowWidth() * (min.mV[VX] + 1.f) * 0.5f);
		screen_min.mY = llround((F32)gViewerWindow->getWindowHeight() * (min.mV[VY] + 1.f) * 0.5f);

		LLCoordGL screen_max;
		screen_max.mX = llround((F32)gViewerWindow->getWindowWidth() * (max.mV[VX] + 1.f) * 0.5f);
		screen_max.mY = llround((F32)gViewerWindow->getWindowHeight() * (max.mV[VY] + 1.f) * 0.5f);

		// grow panel so that screenspace bounding box fits inside "media_region" element of HUD
		LLRect media_hud_rect;
		getParent()->screenRectToLocal(LLRect(screen_min.mX, screen_max.mY, screen_max.mX, screen_min.mY), &media_hud_rect);
		LLView* media_region = getChild<LLView>("media_region");
		media_hud_rect.mLeft -= media_region->getRect().mLeft;
		media_hud_rect.mBottom -= media_region->getRect().mBottom;
		media_hud_rect.mTop += getRect().getHeight() - media_region->getRect().mTop;
		media_hud_rect.mRight += getRect().getWidth() - media_region->getRect().mRight;


		LLRect old_hud_rect = media_hud_rect;
		// keep all parts of HUD on-screen
		media_hud_rect.intersectWith(getParent()->getLocalRect());

		// If we had to clip the rect, don't display the border
		childSetVisible("bg_image", false);

		// clamp to minimum size, keeping centered
		media_hud_rect.setCenterAndSize(media_hud_rect.getCenterX(), media_hud_rect.getCenterY(),
			llmax(MIN_HUD_WIDTH, media_hud_rect.getWidth()), llmax(MIN_HUD_HEIGHT, media_hud_rect.getHeight()));

		setShape(media_hud_rect);

		// Test mouse position to see if the cursor is stationary
		LLCoordWindow cursor_pos_window;
		getWindow()->getCursorPosition(&cursor_pos_window);

		// If last pos is not equal to current pos, the mouse has moved
		// We need to reset the timer, and make sure the panel is visible
		if(cursor_pos_window.mX != mLastCursorPos.mX ||
			cursor_pos_window.mY != mLastCursorPos.mY ||
			mScrollState != SCROLL_NONE)
		{
			mMouseMoveTimer.start();
			mLastCursorPos = cursor_pos_window;
		}
		
		// Mouse has been stationary, but not for long enough to fade the UI
		if(mMouseMoveTimer.getElapsedTimeF32() < mMouseInactiveTime)
		{
			// If we have started fading, reset the alpha values
			if(mFadeTimer.getStarted())
			{
				F32 alpha = 1.0f;
				setAlpha(alpha);
				mFadeTimer.stop();
			}
		}
		// If we need to start fading the UI (and we have not already started)
		else if (!mFadeTimer.getStarted())
		{
			mFadeTimer.start();
		}
	}
}
/*virtual*/
void LLPanelMediaHUD::draw()
{
	if(mFadeTimer.getStarted())
	{
		if(mFadeTimer.getElapsedTimeF32() >= mControlFadeTime)
		{
			setVisible(FALSE);
		}
		else
		{
			F32 time = mFadeTimer.getElapsedTimeF32();
			F32 alpha = llmax(lerp(1.0, 0.0, time / mControlFadeTime), 0.0f);
			setAlpha(alpha);
		}
	}
	LLPanel::draw();
}
void LLPanelMediaHUD::setAlpha(F32 alpha)
{
	LLViewQuery query;

	LLView* query_view = mMediaFocus ? getChildView("media_focused_controls") : getChildView("media_hover_controls");
	child_list_t children = query(query_view);
	for (child_list_iter_t child_iter = children.begin();
		child_iter != children.end(); ++child_iter)
	{
		LLUICtrl* ctrl = dynamic_cast<LLUICtrl*>(*child_iter);
		if (ctrl)
			ctrl->setAlpha(alpha);
	}

	LLPanel::setAlpha(alpha);
}
BOOL LLPanelMediaHUD::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	return LLViewerMediaFocus::getInstance()->handleScrollWheel(x, y, clicks);
}
bool LLPanelMediaHUD::isMouseOver()
{
	if( ! getVisible() )
	{
		return false;
	}
	LLRect screen_rect;
	LLCoordWindow cursor_pos_window;
	getWindow()->getCursorPosition(&cursor_pos_window);

	localRectToScreen(getLocalRect(), &screen_rect);
	// screenPointToLocal(cursor_pos_gl.mX, cursor_pos_gl.mY, &local_mouse_x, &local_mouse_y);

	if(screen_rect.pointInRect(cursor_pos_window.mX, cursor_pos_window.mY))
	{
		return true;
	}
	return false;
}

//static
void LLPanelMediaHUD::onClickClose(void* user_data)
{
	LLViewerMediaFocus::getInstance()->setFocusFace(FALSE, NULL, 0, NULL);
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if(this_panel->mCurrentZoom != ZOOM_NONE)
	{
		// gAgentCamera.setFocusOnAvatar(TRUE, ANIMATE);
		this_panel->mCurrentZoom = ZOOM_NONE;
	}
	this_panel->setVisible(FALSE);

}

//static
void LLPanelMediaHUD::onClickBack(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if (this_panel->mMediaImpl.notNull() && this_panel->mMediaImpl->hasMedia())
	{
		if(this_panel->mMediaImpl->getMediaPlugin()->pluginSupportsMediaTime())
		{
			this_panel->mMediaImpl->getMediaPlugin()->start(-2.0);
		}
		else
		{
			this_panel->mMediaImpl->getMediaPlugin()->browse_back();
		}

	}
}
//static
void LLPanelMediaHUD::onClickForward(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if (this_panel->mMediaImpl.notNull() && this_panel->mMediaImpl->hasMedia())
	{
		if(this_panel->mMediaImpl->getMediaPlugin()->pluginSupportsMediaTime())
		{
			this_panel->mMediaImpl->getMediaPlugin()->start(2.0);
		}
		else
		{
			this_panel->mMediaImpl->getMediaPlugin()->browse_forward();
		}
	}
}
//static
void LLPanelMediaHUD::onClickHome(void* user_data)
{
	//LLViewerMedia::navigateHome();
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if(this_panel->mMediaImpl.notNull())
	{
		this_panel->mMediaImpl->navigateHome();
	}
}
//static
void LLPanelMediaHUD::onClickOpen(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if(this_panel->mMediaImpl.notNull())
	{
		LLWeb::loadURL(this_panel->mMediaImpl->getMediaURL());
	}	
}
//static
void LLPanelMediaHUD::onClickReload(void* user_data)
{
	//LLViewerMedia::navigateHome();
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
	if(objectp && this_panel->mMediaImpl.notNull())
	{
		this_panel->mMediaImpl->navigateTo(objectp->getMediaURL());
	}
}
//static
void LLPanelMediaHUD::onClickPlay(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if (this_panel->mMediaImpl.notNull() && this_panel->mMediaImpl->hasMedia())
	{
		this_panel->mMediaImpl->getMediaPlugin()->start();
	}
}
//static
void LLPanelMediaHUD::onClickPause(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if (this_panel->mMediaImpl.notNull() && this_panel->mMediaImpl->hasMedia())
	{
		this_panel->mMediaImpl->getMediaPlugin()->pause();
	}
}
//static 
void LLPanelMediaHUD::onClickStop(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if (this_panel->mMediaImpl.notNull() && this_panel->mMediaImpl->hasMedia())
	{
		if(this_panel->mMediaImpl->getMediaPlugin()->pluginSupportsMediaTime())
		{
			this_panel->mMediaImpl->getMediaPlugin()->stop();
		}
		else
		{
			this_panel->mMediaImpl->getMediaPlugin()->browse_stop();
		}
	}
}
//static 
void LLPanelMediaHUD::onClickZoom(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	this_panel->nextZoomLevel();
}
void LLPanelMediaHUD::nextZoomLevel()
{
	F32 zoom_padding = 0.0f;
	S32 last_zoom_level = (S32)mCurrentZoom;
	mCurrentZoom = (EZoomLevel)((last_zoom_level + 1) % (S32)ZOOM_END);

	switch (mCurrentZoom)
	{
	case ZOOM_NONE:
		{
			gAgentCamera.setFocusOnAvatar(TRUE, ANIMATE);
			break;
		}
	case ZOOM_MEDIUM:
		{
			zoom_padding = ZOOM_MEDIUM_PADDING;
			break;
		}
	default:
		{
			gAgentCamera.setFocusOnAvatar(TRUE, ANIMATE);
			break;
		}
	}

	if (zoom_padding > 0.0f)		
		LLViewerMediaFocus::getInstance()->setCameraZoom(zoom_padding);
}
void LLPanelMediaHUD::onScrollUp(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if(this_panel->mMediaImpl.notNull() && this_panel->mMediaImpl->hasMedia())
	{
		this_panel->mMediaImpl->getMediaPlugin()->scrollEvent(0, -1, MASK_NONE);
	}
}
void LLPanelMediaHUD::onScrollUpHeld(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	this_panel->mScrollState = SCROLL_UP;
}
void LLPanelMediaHUD::onScrollRight(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if(this_panel->mMediaImpl.notNull())
	{
		this_panel->mMediaImpl->handleKeyHere(KEY_RIGHT, MASK_NONE);
	}
}
void LLPanelMediaHUD::onScrollRightHeld(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	this_panel->mScrollState = SCROLL_RIGHT;
}

void LLPanelMediaHUD::onScrollLeft(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if(this_panel->mMediaImpl.notNull())
	{
		this_panel->mMediaImpl->handleKeyHere(KEY_LEFT, MASK_NONE);
	}
}
void LLPanelMediaHUD::onScrollLeftHeld(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	this_panel->mScrollState = SCROLL_LEFT;
}

void LLPanelMediaHUD::onScrollDown(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	if(this_panel->mMediaImpl.notNull() && this_panel->mMediaImpl->hasMedia())
	{
		this_panel->mMediaImpl->getMediaPlugin()->scrollEvent(0, 1, MASK_NONE);
	}
}
void LLPanelMediaHUD::onScrollDownHeld(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	this_panel->mScrollState = SCROLL_DOWN;
}

void LLPanelMediaHUD::onScrollStop(void* user_data)
{
	LLPanelMediaHUD* this_panel = static_cast<LLPanelMediaHUD*> (user_data);
	this_panel->mScrollState = SCROLL_NONE;
}
