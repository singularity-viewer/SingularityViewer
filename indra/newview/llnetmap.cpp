/** 
 * @file llnetmap.cpp
 * @author James Cook
 * @brief Display of surrounding regions, objects, and agents. View contained by LLFloaterMap.
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

#include "llnetmap.h"

#include "indra_constants.h"
#include "llui.h"
#include "llmath.h"		// clampf()
#include "llfocusmgr.h"
#include "llrender.h"

#include "llfloateravatarlist.h"

#include "llagent.h"
#include "llavatarnamecache.h"
#include "llcallingcard.h"
#include "llcolorscheme.h"
#include "llviewercontrol.h"
#include "llfloateravatarinfo.h"
#include "llfloaterworldmap.h"
#include "llframetimer.h"
#include "lltracker.h"
#include "llmenugl.h"
#include "llsurface.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "lluuid.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llworld.h"
#include "llworldmapview.h"		// shared draw code
#include "llappviewer.h"				// Only for constants!

#include "llglheaders.h"

//<edit>
#include "llmutelist.h"
//</edit>

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

const F32 MAP_SCALE_MIN = 32;
const F32 MAP_SCALE_MID = 256;
const F32 MAP_SCALE_MAX = 4096;
const F32 MAP_SCALE_INCREMENT = 16;
const F32 MAP_SCALE_ZOOM_FACTOR = 1.1f;		// Zoom in factor per click of the scroll wheel (10%)
const F32 MAP_MINOR_DIR_THRESHOLD = 0.08f;
const F32 MIN_DOT_RADIUS = 3.5f;
const F32 DOT_SCALE = 0.75f;
const F32 MIN_PICK_SCALE = 2.f;
const S32 MOUSE_DRAG_SLOP = 2;				// How far the mouse needs to move before we think it's a drag

LLNetMap::LLNetMap(const std::string& name) :
	LLPanel(name),
	mScale(128.f),
	mObjectMapTPM(1.f),
	mObjectMapPixels(255.f),
	mTargetPanX( 0.f ),
	mTargetPanY( 0.f ),
	mCurPanX( 0.f ),
	mCurPanY( 0.f ),
	mUpdateNow( FALSE )
{
	mScale = gSavedSettings.getF32("MiniMapScale");
	mPixelsPerMeter = mScale / LLWorld::getInstance()->getRegionWidthInMeters();
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);

	mObjectImageCenterGlobal = gAgent.getCameraPositionGlobal();
	
	// Register event listeners for popup menu
	(new LLScaleMap())->registerListener(this, "MiniMap.ZoomLevel");
	(new LLCenterMap())->registerListener(this, "MiniMap.Center");
	(new LLCheckCenterMap())->registerListener(this, "MiniMap.CheckCenter");
	(new LLRotateMap())->registerListener(this, "MiniMap.Rotate");
	(new LLCheckRotateMap())->registerListener(this, "MiniMap.CheckRotate");
	(new LLStopTracking())->registerListener(this, "MiniMap.StopTracking");
	(new LLEnableTracking())->registerListener(this, "MiniMap.EnableTracking");
	(new LLShowAgentProfile())->registerListener(this, "MiniMap.ShowProfile");
	(new LLEnableProfile())->registerListener(this, "MiniMap.EnableProfile");
	(new LLCamFollow())->registerListener(this, "MiniMap.CamFollow"); //moymod - add cam follow crap thingie
	(new mmsetred())->registerListener(this, "MiniMap.setred");
	(new mmsetgreen())->registerListener(this, "MiniMap.setgreen");
	(new mmsetblue())->registerListener(this, "MiniMap.setblue");
	(new mmsetyellow())->registerListener(this, "MiniMap.setyellow");
	(new mmsetcustom())->registerListener(this, "MiniMap.setcustom");

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_mini_map.xml");

	updateMinorDirections();

	LLMenuGL* menu = LLUICtrlFactory::getInstance()->buildMenu("menu_mini_map.xml", this);
	if (!menu)
	{
		menu = new LLMenuGL(LLStringUtil::null);
	}
	menu->setVisible(FALSE);
	mPopupMenuHandle = menu->getHandle();
}

LLNetMap::~LLNetMap()
{
}

void LLNetMap::setScale( F32 scale )
{
	mScale = scale;
	if (mScale == 0.f)
	{
		mScale = 0.1f;
	}
	gSavedSettings.setF32("MiniMapScale", mScale);

	if (mObjectImagep.notNull())
	{
		F32 width = (F32)(getRect().getWidth());
		F32 height = (F32)(getRect().getHeight());
		F32 diameter = sqrt(width * width + height * height);
		F32 region_widths = diameter / mScale;
		F32 meters = region_widths * LLWorld::getInstance()->getRegionWidthInMeters();
		F32 num_pixels = (F32)mObjectImagep->getWidth();
		mObjectMapTPM = num_pixels / meters;
		mObjectMapPixels = diameter;
	}

	mPixelsPerMeter = mScale / LLWorld::getInstance()->getRegionWidthInMeters();
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);

	mUpdateNow = TRUE;
}

void LLNetMap::translatePan( F32 delta_x, F32 delta_y )
{
	mTargetPanX += delta_x;
	mTargetPanY += delta_y;
}


///////////////////////////////////////////////////////////////////////////////////
LLColor4 mm_mapcols[1024];
LLUUID mm_mapkeys[1024];
U32 mm_netmapnum;

void LLNetMap::mm_setcolor(LLUUID key,LLColor4 col){
	if(mm_netmapnum>1023){
		llinfos << "Minimap color buffer filled, relog or something to clear it" << llendl;
		return;
	}
	mm_mapcols[mm_netmapnum]=col;
	mm_mapkeys[mm_netmapnum]=key;
	mm_netmapnum+=1;
}
void LLNetMap::draw()
{
 	static LLFrameTimer map_timer;

	if (mObjectImagep.isNull())
	{
		createObjectImage();
	}

	if (gSavedSettings.getS32( "MiniMapCenter") != MAP_CENTER_NONE)
	{
		mCurPanX = lerp(mCurPanX, mTargetPanX, LLCriticalDamp::getInterpolant(0.1f));
		mCurPanY = lerp(mCurPanY, mTargetPanY, LLCriticalDamp::getInterpolant(0.1f));
	}

	F32 rotation = 0;

	// Prepare a scissor region
	{
		LLGLEnable scissor(GL_SCISSOR_TEST);

		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			LLLocalClipRect clip(getLocalRect());

			glMatrixMode(GL_MODELVIEW);

			// Draw background rectangle
			if(isBackgroundVisible())
			{
				gGL.color4fv(isBackgroundOpaque() ? getBackgroundColor().mV : getTransparentColor().mV);
				gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0);
			}
		}

		// region 0,0 is in the middle
		S32 center_sw_left = getRect().getWidth() / 2 + llfloor(mCurPanX);
		S32 center_sw_bottom = getRect().getHeight() / 2 + llfloor(mCurPanY);

		gGL.pushMatrix();

		gGL.translatef( (F32) center_sw_left, (F32) center_sw_bottom, 0.f);
		
		BOOL rotate_map = gSavedSettings.getBOOL( "MiniMapRotate" );
		if (rotate_map)
		{
			// rotate subsequent draws to agent rotation
			rotation = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
			glRotatef( rotation * RAD_TO_DEG, 0.f, 0.f, 1.f);
		}

		// figure out where agent is
		S32 region_width = llround(LLWorld::getInstance()->getRegionWidthInMeters());
		LLColor4 this_region_color = gColors.getColor( "NetMapThisRegion" );
		LLColor4 live_region_color = gColors.getColor( "NetMapLiveRegion" );
		LLColor4 dead_region_color = gColors.getColor( "NetMapDeadRegion" );

		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
			 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* regionp = *iter;
			// Find x and y position relative to camera's center.
			LLVector3 origin_agent = regionp->getOriginAgent();
			LLVector3 rel_region_pos = origin_agent - gAgent.getCameraPositionAgent();
			F32 relative_x = (rel_region_pos.mV[0] / region_width) * mScale;
			F32 relative_y = (rel_region_pos.mV[1] / region_width) * mScale;

			// background region rectangle
			F32 bottom =	relative_y;
			F32 left =		relative_x;
			F32 top =		bottom + mScale ;
			F32 right =		left + mScale ;

			gGL.color4fv(regionp == gAgent.getRegion() ? this_region_color.mV : live_region_color.mV);
			if (!regionp->isAlive())
			{
				gGL.color4fv(dead_region_color.mV);
			}


			// Draw using texture.
			gGL.getTexUnit(0)->bind(regionp->getLand().getSTexture());
			gGL.begin(LLRender::QUADS);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(left, top);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(left, bottom);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(right, bottom);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(right, top);
			gGL.end();

			// Draw water
			gGL.setAlphaRejectSettings(LLRender::CF_GREATER, ABOVE_WATERLINE_ALPHA / 255.f);
			{
				if (regionp->getLand().getWaterTexture())
				{
					gGL.getTexUnit(0)->bind(regionp->getLand().getWaterTexture());
					gGL.begin(LLRender::QUADS);
						gGL.texCoord2f(0.f, 1.f);
						gGL.vertex2f(left, top);
						gGL.texCoord2f(0.f, 0.f);
						gGL.vertex2f(left, bottom);
						gGL.texCoord2f(1.f, 0.f);
						gGL.vertex2f(right, bottom);
						gGL.texCoord2f(1.f, 1.f);
						gGL.vertex2f(right, top);
					gGL.end();
				}
			}
			gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
		}

		// Redraw object layer periodically
		if (mUpdateNow || (map_timer.getElapsedTimeF32() > 0.5f))
		{
			mUpdateNow = FALSE;

			// Locate the centre of the object layer, accounting for panning
			LLVector3 new_center = globalPosToView(gAgent.getCameraPositionGlobal(), rotate_map);	
			new_center.mV[0] -= mCurPanX;
			new_center.mV[1] -= mCurPanY;
			new_center.mV[2] = 0.f;
			mObjectImageCenterGlobal = viewPosToGlobal(llround(new_center.mV[0]), llround(new_center.mV[1]), rotate_map);

			// Create the base texture.
			U8 *default_texture = mObjectRawImagep->getData();
			memset( default_texture, 0, mObjectImagep->getWidth() * mObjectImagep->getHeight() * mObjectImagep->getComponents() );

			// Draw buildings
			gObjectList.renderObjectsForMap(*this);

			mObjectImagep->setSubImage(mObjectRawImagep, 0, 0, mObjectImagep->getWidth(), mObjectImagep->getHeight());
			
			map_timer.reset();
		}

		LLVector3 map_center_agent = gAgent.getPosAgentFromGlobal(mObjectImageCenterGlobal);
		map_center_agent -= gAgent.getCameraPositionAgent();
		map_center_agent.mV[VX] *= mScale/region_width;
		map_center_agent.mV[VY] *= mScale/region_width;

		gGL.getTexUnit(0)->bind(mObjectImagep);
		F32 image_half_width = 0.5f*mObjectMapPixels;
		F32 image_half_height = 0.5f*mObjectMapPixels;

		gGL.begin(LLRender::QUADS);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, image_half_height + map_center_agent.mV[VY]);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(image_half_width + map_center_agent.mV[VX], map_center_agent.mV[VY] - image_half_height);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2f(image_half_width + map_center_agent.mV[VX], image_half_height + map_center_agent.mV[VY]);
		gGL.end();

		gGL.popMatrix();

		LLVector3d pos_global;
		LLVector3 pos_map;

		// Mouse pointer in local coordinates
		S32 local_mouse_x;
		S32 local_mouse_y;
		LLUI::getCursorPositionLocal(this, &local_mouse_x, &local_mouse_y);
		mClosestAgentToCursor.setNull();
		F32 closest_dist = F32_MAX;
		F32 min_pick_dist = mDotRadius * MIN_PICK_SCALE; 

		// Draw avatars
//		LLColor4 mapcolor = gAvatarMapColor;

		static const LLCachedControl<LLColor4>	standard_color("MapAvatar",LLColor4(0.f,1.f,0.f,1.f),gColors);
		static const LLCachedControl<LLColor4>	friend_color_stored("AscentFriendColor",LLColor4(1.f,1.f,0.f,1.f));
		static const LLCachedControl<LLColor4>	em_color("AscentEstateOwnerColor",LLColor4(1.f,0.6f,1.f,1.f));
		static const LLCachedControl<LLColor4>	linden_color("AscentLindenColor",LLColor4(0.f,0.f,1.f,1.f));
		static const LLCachedControl<LLColor4>	muted_color("AscentMutedColor",LLColor4(0.7f,0.7f,0.7f,1.f));

		// Draw avatars
// [RLVa:KB] - Version: 1.23.4 | Alternate: Snowglobe-1.2.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
		LLColor4 friend_color = (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? friend_color_stored : standard_color;
// [/RLVa:KB]
		std::vector<LLUUID> avatar_ids;
		std::vector<LLVector3d> positions;
		LLWorld::getInstance()->getAvatars(&avatar_ids, &positions);
		U32 a;
		for(U32 i=0; i<avatar_ids.size(); i++)
		{
			LLColor4 avColor = standard_color;
			// TODO: it'd be very cool to draw these in sorted order from lowest Z to highest.
			// just be careful to sort the avatar IDs along with the positions. -MG
			pos_map = globalPosToView(positions[i], rotate_map);
			if (positions[i].mdV[VZ] == 0.f)
			{
				pos_map.mV[VZ] = 16000.f;
			}
			std::string avName;

			gCacheName->getFullName(avatar_ids[i], avName);
			if(LLMuteList::getInstance()->isMuted(avatar_ids[i]))
			{
				avColor = muted_color;
			}

			LLViewerRegion* avatar_region = LLWorld::getInstance()->getRegionFromPosGlobal(positions[i]);
			LLUUID estate_owner = avatar_region? avatar_region->getOwner() : LLUUID::null;

			//Lindens are always more Linden than your friend, make that take precedence
			if(LLMuteList::getInstance()->isLinden(avName))
			{
				avColor = linden_color;
			}
			//check if they are an estate owner at their current position
			else if(estate_owner.notNull() && avatar_ids[i] == estate_owner)
			{
				avColor = em_color;
			}
			//without these dots, SL would suck.
			else if(is_agent_friend(avatar_ids[i]))
			{
				avColor = friend_color;
			}
			else 
			{
				// MOYMOD Minimap custom av colors.
				for(a=0;a<mm_netmapnum;a+=1)
				{
					if(avatar_ids[i]==mm_mapkeys[a])
					{
						avColor = mm_mapcols[a];
					}
				}
			}

			LLWorldMapView::drawAvatar(
				pos_map.mV[VX], pos_map.mV[VY], 
				avColor,
				pos_map.mV[VZ],
				mDotRadius);

			F32	dist_to_cursor = dist_vec(LLVector2(pos_map.mV[VX], pos_map.mV[VY]), LLVector2(local_mouse_x,local_mouse_y));
			if(dist_to_cursor < min_pick_dist && dist_to_cursor < closest_dist)
			{
				closest_dist = dist_to_cursor;
				mClosestAgentToCursor = avatar_ids[i];
				mClosestAgentPosition = positions[i];
			}
		}

		// Draw dot for autopilot target
		if (gAgent.getAutoPilot())
		{
			drawTracking(gAgent.getAutoPilotTargetGlobal(), rotate_map, gTrackColor);
		}
		else
		{
			LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
			if (  LLTracker::TRACKING_AVATAR == tracking_status )
			{
				drawTracking(LLAvatarTracker::instance().getGlobalPos(), rotate_map, gTrackColor);
			} 
			else if ( LLTracker::TRACKING_LANDMARK == tracking_status 
					|| LLTracker::TRACKING_LOCATION == tracking_status )
			{
				drawTracking(LLTracker::getTrackedPositionGlobal(), rotate_map, gTrackColor);
			}
		}

		// Draw dot for self avatar position
		pos_global = gAgent.getPositionGlobal();
		pos_map = globalPosToView(pos_global, rotate_map);
		LLUIImagePtr you = LLWorldMapView::sAvatarYouLargeImage;
		S32 dot_width = llround(mDotRadius * 2.f);
		you->draw(
			llround(pos_map.mV[VX] - mDotRadius),
			llround(pos_map.mV[VY] - mDotRadius),
			dot_width,
			dot_width);

		// Draw frustum
		F32 meters_to_pixels = mScale/ LLWorld::getInstance()->getRegionWidthInMeters();

		F32 horiz_fov = LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect();
		F32 far_clip_meters = LLViewerCamera::getInstance()->getFar();
		F32 far_clip_pixels = far_clip_meters * meters_to_pixels;

		F32 half_width_meters = far_clip_meters * tan( horiz_fov / 2 );
		F32 half_width_pixels = half_width_meters * meters_to_pixels;
		
		F32 ctr_x = (F32)center_sw_left;
		F32 ctr_y = (F32)center_sw_bottom;


		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		if (rotate_map)
		{
			gGL.color4fv(gColors.getColor("NetMapFrustum").mV);

			gGL.begin( LLRender::TRIANGLES  );
				gGL.vertex2f( ctr_x, ctr_y );
				gGL.vertex2f( ctr_x - half_width_pixels, ctr_y + far_clip_pixels );
				gGL.vertex2f( ctr_x + half_width_pixels, ctr_y + far_clip_pixels );
			gGL.end();
		}
		else
		{
			gGL.color4fv(gColors.getColor("NetMapFrustumRotating").mV);
			
			// If we don't rotate the map, we have to rotate the frustum.
			gGL.pushMatrix();
				gGL.translatef( ctr_x, ctr_y, 0 );
				glRotatef( atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] ) * RAD_TO_DEG, 0.f, 0.f, -1.f);
				gGL.begin( LLRender::TRIANGLES  );
					gGL.vertex2f( 0, 0 );
					gGL.vertex2f( -half_width_pixels, far_clip_pixels );
					gGL.vertex2f(  half_width_pixels, far_clip_pixels );
				gGL.end();
			gGL.popMatrix();
		}
	}
	
	// Rotation of 0 means that North is up
	setDirectionPos( getChild<LLTextBox>("e_label"), rotation);
	setDirectionPos( getChild<LLTextBox>("n_label"), rotation + F_PI_BY_TWO);
	setDirectionPos( getChild<LLTextBox>("w_label"), rotation + F_PI);
	setDirectionPos( getChild<LLTextBox>("s_label"), rotation + F_PI + F_PI_BY_TWO);

	setDirectionPos( getChild<LLTextBox>("ne_label"), rotation + F_PI_BY_TWO / 2);
	setDirectionPos( getChild<LLTextBox>("nw_label"), rotation + F_PI_BY_TWO + F_PI_BY_TWO / 2);
	setDirectionPos( getChild<LLTextBox>("sw_label"), rotation + F_PI + F_PI_BY_TWO / 2);
	setDirectionPos( getChild<LLTextBox>("se_label"), rotation + F_PI + F_PI_BY_TWO + F_PI_BY_TWO / 2);

	LLView::draw();
}

void LLNetMap::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);
	createObjectImage();
	updateMinorDirections();
}

LLVector3 LLNetMap::globalPosToView(const LLVector3d& global_pos, BOOL rotated)
{
	LLVector3d relative_pos_global = global_pos - gAgent.getCameraPositionGlobal();
	LLVector3 pos_local;
	pos_local.setVec(relative_pos_global);  // convert to floats from doubles

	pos_local.mV[VX] *= mPixelsPerMeter;
	pos_local.mV[VY] *= mPixelsPerMeter;
	// leave Z component in meters

	if( rotated )
	{
		F32 radians = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

	pos_local.mV[VX] += getRect().getWidth() / 2 + mCurPanX;
	pos_local.mV[VY] += getRect().getHeight() / 2 + mCurPanY;

	return pos_local;
}

void LLNetMap::drawTracking(const LLVector3d& pos_global, BOOL rotated,
							const LLColor4& color, BOOL draw_arrow )
{
	LLVector3 pos_local = globalPosToView( pos_global, rotated );
	if( (pos_local.mV[VX] < 0) ||
		(pos_local.mV[VY] < 0) ||
		(pos_local.mV[VX] >= getRect().getWidth()) ||
		(pos_local.mV[VY] >= getRect().getHeight()) )
	{
		if (draw_arrow)
		{
			S32 x = llround( pos_local.mV[VX] );
			S32 y = llround( pos_local.mV[VY] );
			LLWorldMapView::drawTrackingCircle( getRect(), x, y, color, 1, 10 );
			LLWorldMapView::drawTrackingArrow( getRect(), x, y, color );
		}
	}
	else
	{
		LLWorldMapView::drawTrackingDot(pos_local.mV[VX], 
										pos_local.mV[VY], 
										color,
										pos_local.mV[VZ]);
	}
}

LLVector3d LLNetMap::viewPosToGlobal( S32 x, S32 y, BOOL rotated )
{
	x -= llround(getRect().getWidth() / 2 + mCurPanX);
	y -= llround(getRect().getHeight() / 2 + mCurPanY);

	LLVector3 pos_local( (F32)x, (F32)y, 0.f );

	F32 radians = - atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );

	if( rotated )
	{
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

	pos_local *= ( LLWorld::getInstance()->getRegionWidthInMeters() / mScale );
	
	LLVector3d pos_global;
	pos_global.setVec( pos_local );
	pos_global += gAgent.getCameraPositionGlobal();

	return pos_global;
}

BOOL LLNetMap::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	// note that clicks are reversed from what you'd think: i.e. > 0  means zoom out, < 0 means zoom in
	F32 scale = mScale;
	
	scale *= pow(MAP_SCALE_ZOOM_FACTOR, -clicks);
	setScale(llclamp(scale, MAP_SCALE_MIN, MAP_SCALE_MAX));

	return TRUE;
}

BOOL LLNetMap::handleToolTip( S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen )
{
	BOOL handled = FALSE;
	if (gDisconnected)
	{
		return FALSE;
	}
	LLViewerRegion*	region = LLWorld::getInstance()->getRegionFromPosGlobal(viewPosToGlobal(x, y, gSavedSettings.getBOOL( "MiniMapRotate" )));
	if( region )
	{
		msg.assign("");
		std::string fullname;
		if(mClosestAgentToCursor.notNull() && gCacheName->getFullName(mClosestAgentToCursor, fullname))
		{
			//msg.append(fullname);
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
            // [Ansariel: Display name support]
			// msg.append( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? fullname : RlvStrings::getAnonym(fullname) );
            if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
            {
                msg.append(RlvStrings::getAnonym(fullname));
            }
            else
            {
#ifdef LL_RRINTERFACE_H //MK
    			if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
    			{
	    			fullname = gAgent.mRRInterface.getDummyName(fullname);
		    	}
			    else
			    {
#endif //mk
				    if (LLAvatarNameCache::useDisplayNames())
    				{
	    				LLAvatarName avatar_name;
		    			if (LLAvatarNameCache::get(mClosestAgentToCursor, &avatar_name))
			    		{
							static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
    						if (phoenix_name_system == 2 || (phoenix_name_system == 1 && avatar_name.mIsDisplayNameDefault))
					    	{
						    	fullname = avatar_name.mDisplayName;
    						}
	    					else
		    				{
			    				fullname = avatar_name.getCompleteName(true);
				    		}
					    }
    				}
#ifdef LL_RRINTERFACE_H //MK
			    }
#endif //mk
                msg.append(fullname);
            }
            // [/Ansariel: Display name support]
// [/RLVa:KB]
			msg.append("\n");

			LLVector3d mypos = gAgent.getPositionGlobal();
			LLVector3d position = mClosestAgentPosition;

			if ( LLFloaterAvatarList::getInstance() )
			{
				LLAvatarListEntry *ent = LLFloaterAvatarList::getInstance()->getAvatarEntry(mClosestAgentToCursor);
				if ( NULL != ent )
				{
					//position = LLFloaterAvatarList::AvatarPosition(mClosestAgentToCursor);
					position = ent->getPosition();
				}
			}
			LLVector3d delta = position - mypos;
			F32 distance = (F32)delta.magVec();


			//llinfos << distance << " - " << position << llendl;

			msg.append( llformat("\n(Distance: %.02fm)\n\n",distance) );
		}
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a) | Modified: RLVa-0.2.0b
		msg.append( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) ? region->getName() : RlvStrings::getString(RLV_STRING_HIDDEN) );
// [/RLVa:KB]
		//msg.append( region->getName() );

#ifndef LL_RELEASE_FOR_DOWNLOAD
		std::string buffer;
		msg.append("\n");
		buffer = region->getHost().getHostName();
		msg.append(buffer);
		msg.append("\n");
		buffer = region->getHost().getString();
		msg.append(buffer);
#endif
		msg.append("\n");
		msg.append(getToolTip());

		S32 SLOP = 4;
		localPointToScreen( 
			x - SLOP, y - SLOP, 
			&(sticky_rect_screen->mLeft), &(sticky_rect_screen->mBottom) );
		sticky_rect_screen->mRight = sticky_rect_screen->mLeft + 2 * SLOP;
		sticky_rect_screen->mTop = sticky_rect_screen->mBottom + 2 * SLOP;
		handled = TRUE;
	}
	if(!handled)
	{
		return LLPanel::handleToolTip(x, y, msg, sticky_rect_screen);
	}
	return handled;
}


void LLNetMap::setDirectionPos( LLTextBox* text_box, F32 rotation )
{
	// Rotation is in radians.
	// Rotation of 0 means x = 1, y = 0 on the unit circle.

	F32 half_height = (F32)( (getRect().getHeight() - text_box->getRect().getHeight()) / 2);
	F32 half_width  = (F32)( (getRect().getWidth() - text_box->getRect().getWidth()) / 2);
	F32 radius = llmin( half_height, half_width );

	// Inset by a little to account for position display.
	radius -= 8.f;

	text_box->setOrigin(llround(half_width  + radius * cos( rotation )),
						llround(half_height + radius * sin( rotation )));
}

void LLNetMap::updateMinorDirections()
{
	if (getChild<LLTextBox>("ne_label", TRUE, FALSE) == NULL)
	{
		return;
	}

	// Hide minor directions if they cover too much of the map
	bool show_minors = getChild<LLTextBox>("ne_label")->getRect().getHeight() < MAP_MINOR_DIR_THRESHOLD *
			llmin(getRect().getWidth(), getRect().getHeight());

	getChild<LLTextBox>("ne_label")->setVisible(show_minors);
	getChild<LLTextBox>("nw_label")->setVisible(show_minors);
	getChild<LLTextBox>("sw_label")->setVisible(show_minors);
	getChild<LLTextBox>("se_label")->setVisible(show_minors);
}

void LLNetMap::renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius_meters )
{
	LLVector3 local_pos;
	local_pos.setVec( pos - mObjectImageCenterGlobal );

	// DEV-17370 - megaprims of size > 4096 cause lag.  (go figger.)
	const F32 MAX_RADIUS = 256.0f;
	F32 radius_clamped = llmin(radius_meters, MAX_RADIUS);
	
	S32 diameter_pixels = llround(2 * radius_clamped * mObjectMapTPM);
	renderPoint( local_pos, color, diameter_pixels );
}


void LLNetMap::renderPoint(const LLVector3 &pos_local, const LLColor4U &color, 
						   S32 diameter, S32 relative_height)
{
	if (diameter <= 0)
	{
		return;
	}

	const S32 image_width = (S32)mObjectImagep->getWidth();
	const S32 image_height = (S32)mObjectImagep->getHeight();

	S32 x_offset = llround(pos_local.mV[VX] * mObjectMapTPM + image_width / 2);
	S32 y_offset = llround(pos_local.mV[VY] * mObjectMapTPM + image_height / 2);

	if ((x_offset < 0) || (x_offset >= image_width))
	{
		return;
	}
	if ((y_offset < 0) || (y_offset >= image_height))
	{
		return;
	}

	U8 *datap = mObjectRawImagep->getData();

	S32 neg_radius = diameter / 2;
	S32 pos_radius = diameter - neg_radius;
	S32 x, y;

	if (relative_height > 0)
	{
		// ...point above agent
		S32 px, py;

		// vertical line
		px = x_offset;
		for (y = -neg_radius; y < pos_radius; y++)
		{
			py = y_offset + y;
			if ((py < 0) || (py >= image_height))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.mAll;
		}

		// top line
		py = y_offset + pos_radius - 1;
		for (x = -neg_radius; x < pos_radius; x++)
		{
			px = x_offset + x;
			if ((px < 0) || (px >= image_width))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.mAll;
		}
	}
	else
	{
		// ...point level with agent
		for (x = -neg_radius; x < pos_radius; x++)
		{
			S32 p_x = x_offset + x;
			if ((p_x < 0) || (p_x >= image_width))
			{
				continue;
			}

			for (y = -neg_radius; y < pos_radius; y++)
			{
				S32 p_y = y_offset + y;
				if ((p_y < 0) || (p_y >= image_height))
				{
					continue;
				}
				S32 offset = p_x + p_y * image_width;
				((U32*)datap)[offset] = color.mAll;
			}
		}
	}
}

void LLNetMap::createObjectImage()
{
	// Find the size of the side of a square that surrounds the circle that surrounds getRect().
	// ... which is, the diagonal of the rect.
	F32 width = getRect().getWidth();
	F32 height = getRect().getHeight();
	S32 square_size = llround( sqrt(width*width + height*height) );

	// Find the least power of two >= the minimum size.
	const S32 MIN_SIZE = 64;
	const S32 MAX_SIZE = 256;
	S32 img_size = MIN_SIZE;
	while( (img_size*2 < square_size ) && (img_size < MAX_SIZE) )
	{
		img_size <<= 1;
	}

	if( mObjectImagep.isNull() ||
		(mObjectImagep->getWidth() != img_size) ||
		(mObjectImagep->getHeight() != img_size) )
	{
		mObjectRawImagep = new LLImageRaw(img_size, img_size, 4);
		U8* data = mObjectRawImagep->getData();
		memset( data, 0, img_size * img_size * 4 );
		mObjectImagep = LLViewerTextureManager::getLocalTexture( mObjectRawImagep.get(), FALSE);
	}
	setScale(mScale);
	mUpdateNow = TRUE;
}

BOOL LLNetMap::handleMouseDown( S32 x, S32 y, MASK mask )
{
	if (!(mask & MASK_SHIFT)) return FALSE;

	// Start panning
	gFocusMgr.setMouseCapture(this);

	mMouseDownPanX = llround(mCurPanX);
	mMouseDownPanY = llround(mCurPanY);
	mMouseDownX = x;
	mMouseDownY = y;
	return TRUE;
}

BOOL LLNetMap::handleMouseUp( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning)
		{
			// restore mouse cursor
			S32 local_x, local_y;
			local_x = mMouseDownX + llfloor(mCurPanX - mMouseDownPanX);
			local_y = mMouseDownY + llfloor(mCurPanY - mMouseDownPanY);
			LLRect clip_rect = getRect();
			clip_rect.stretch(-8);
			clip_rect.clipPointToRect(mMouseDownX, mMouseDownY, local_x, local_y);
			LLUI::setCursorPositionLocal(this, local_x, local_y);

			// finish the pan
			mPanning = FALSE;

			mMouseDownX = 0;
			mMouseDownY = 0;

			// auto centre
			mTargetPanX = 0;
			mTargetPanY = 0;
		}
		gViewerWindow->showCursor();
		gFocusMgr.setMouseCapture(NULL);
		return TRUE;
	}
	return FALSE;
}

// static
BOOL LLNetMap::outsideSlop( S32 x, S32 y, S32 start_x, S32 start_y, S32 slop )
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return (dx <= -slop || slop <= dx || dy <= -slop || slop <= dy);
}

BOOL LLNetMap::handleHover( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning || outsideSlop(x, y, mMouseDownX, mMouseDownY, MOUSE_DRAG_SLOP))
		{
			if (!mPanning)
			{
				// just started panning, so hide cursor
				mPanning = TRUE;
				gViewerWindow->hideCursor();
			}

			F32 delta_x = (F32)(gViewerWindow->getCurrentMouseDX());
			F32 delta_y = (F32)(gViewerWindow->getCurrentMouseDY());

			// Set pan to value at start of drag + offset
			mCurPanX += delta_x;
			mCurPanY += delta_y;
			mTargetPanX = mCurPanX;
			mTargetPanY = mCurPanY;

			gViewerWindow->moveCursorToCenter();
		}

		// Doesn't really matter, cursor should be hidden
		gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
	}
	else
	{
		if (mask & MASK_SHIFT)
		{
			// If shift is held, change the cursor to hint that the map can be dragged
			gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
		}
		else 
		{
			gViewerWindow->setCursor( UI_CURSOR_CROSS );
		}
	}
	
	return TRUE;
}

BOOL LLNetMap::handleDoubleClick( S32 x, S32 y, MASK mask )
{
	LLVector3d pos_global = viewPosToGlobal(x, y, gSavedSettings.getBOOL( "MiniMapRotate" ));
	BOOL new_target = FALSE;
	if (!LLTracker::isTracking(NULL))
	{
		gFloaterWorldMap->trackLocation(pos_global);
		new_target = TRUE;
	}

	if (gSavedSettings.getBOOL("DoubleClickTeleport"))
	{
		gAgent.teleportViaLocationLookAt(pos_global);
	}
	else 
	{
		LLFloaterWorldMap::show(NULL, new_target);
	}
	return TRUE;
}

BOOL LLNetMap::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	mClosestAgentAtLastRightClick = mClosestAgentToCursor;
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	if (menu)
	{
		menu->buildDrawLabels();
		menu->updateParent(LLMenuGL::sMenuContainer);
		LLMenuGL::showPopup(this, menu, x, y);
	}
	return TRUE;
}


// static
bool LLNetMap::LLScaleMap::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;

	S32 level = userdata.asInteger();

	switch(level)
	{
	case 0:
		self->setScale(MAP_SCALE_MIN);
		break;
	case 1:
		self->setScale(MAP_SCALE_MID);
		break;
	case 2:
		self->setScale(MAP_SCALE_MAX);
		break;
	default:
		break;
	}

	return true;
}

//moymod - minimap color shit
bool LLNetMap::mmsetred::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	//if(self->mClosestAgentAtLastRightClick){
		mm_setcolor(self->mClosestAgentAtLastRightClick,LLColor4(1.0,0.0,0.0,1.0));
	//}
	return true;
}
bool LLNetMap::mmsetgreen::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	//if(self->mClosestAgentAtLastRightClick){
		mm_setcolor(self->mClosestAgentAtLastRightClick,LLColor4(0.0,1.0,0.0,1.0));
	//}
	return true;
}
bool LLNetMap::mmsetblue::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	//if(self->mClosestAgentAtLastRightClick){
		mm_setcolor(self->mClosestAgentAtLastRightClick,LLColor4(0.0,0.0,1.0,1.0));
	//}
	return true;
}
bool LLNetMap::mmsetyellow::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	//if(self->mClosestAgentAtLastRightClick){
		mm_setcolor(self->mClosestAgentAtLastRightClick,LLColor4(1.0,1.0,0.0,1.0));
	//}
	return true;
}
bool LLNetMap::mmsetcustom::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	//if(self->mClosestAgentAtLastRightClick){
		mm_setcolor(self->mClosestAgentAtLastRightClick,gSavedSettings.getColor4("MoyMiniMapCustomColor"));
	//}
	return true;
}
bool LLNetMap::LLCenterMap::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	EMiniMapCenter center = (EMiniMapCenter)userdata.asInteger();

	if (gSavedSettings.getS32("MiniMapCenter") == center)
	{
		gSavedSettings.setS32("MiniMapCenter", MAP_CENTER_NONE);
	}
	else
	{
		gSavedSettings.setS32("MiniMapCenter", userdata.asInteger());
	}

	return true;
}

bool LLNetMap::LLCheckCenterMap::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	EMiniMapCenter center = (EMiniMapCenter)userdata["data"].asInteger();
	BOOL enabled = (gSavedSettings.getS32("MiniMapCenter") == center);

	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLNetMap::LLRotateMap::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	BOOL rotate = gSavedSettings.getBOOL("MiniMapRotate");
	gSavedSettings.setBOOL("MiniMapRotate", !rotate);

	return true;
}

bool LLNetMap::LLCheckRotateMap::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	BOOL enabled = gSavedSettings.getBOOL("MiniMapRotate");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLNetMap::LLStopTracking::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLTracker::stopTracking(NULL);
	return true;
}

bool LLNetMap::LLEnableTracking::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	self->findControl(userdata["control"].asString())->setValue(LLTracker::isTracking(NULL));
	return true;
}


bool LLNetMap::LLCamFollow::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
	LLFloaterAvatarList::lookAtAvatar(self->mClosestAgentAtLastRightClick);
	return true;
}


bool LLNetMap::LLShowAgentProfile::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
	if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		LLFloaterAvatarInfo::show(self->mClosestAgentAtLastRightClick);
	}
// [/RLVa:KB]
	//LLFloaterAvatarInfo::show(self->mClosestAgentAtLastRightClick);
	return true;
}

bool LLNetMap::LLEnableProfile::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLNetMap *self = mPtr;
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
	self->findControl(userdata["control"].asString())->setValue(
		(self->isAgentUnderCursor()) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) );
// [/RLVa:KB]
	//self->findControl(userdata["control"].asString())->setValue(self->isAgentUnderCursor());
	return true;
}
