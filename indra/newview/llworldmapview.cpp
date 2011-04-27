/** 
 * @file llworldmapview.cpp
 * @brief LLWorldMapView class implementation
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

#include "llworldmapview.h"

#include "indra_constants.h"
#include "llui.h"
#include "llmath.h"		// clampf()
#include "llregionhandle.h"
#include "lleventflags.h"
#include "llrender.h"

#include "llagent.h"
#include "llcallingcard.h"
#include "llcolorscheme.h"
#include "llviewercontrol.h"
#include "llcylinder.h"
#include "llfloaterdirectory.h"
#include "llfloatermap.h"
#include "llfloaterworldmap.h"
#include "llfocusmgr.h"
#include "lltextbox.h"
#include "lltextureview.h"
#include "lltracker.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworldmap.h"
#include "llworldmipmap.h"
#include "lltexturefetch.h"
#include "llappviewer.h"				// Only for constants!
#include "lltrans.h"

#include "llglheaders.h"

#include "hippogridmanager.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

// Basically a C++ implementation of the OCEAN_COLOR defined in mapstitcher.py 
// Please ensure consistency between those 2 files (TODO: would be better to get that color from an asset source...)
// # Constants
// OCEAN_COLOR = "#1D475F"
const F32 OCEAN_RED   = (F32)(0x1D)/255.f;
const F32 OCEAN_GREEN = (F32)(0x47)/255.f;
const F32 OCEAN_BLUE  = (F32)(0x5F)/255.f;

const F32 GODLY_TELEPORT_HEIGHT = 200.f;
const S32 SCROLL_HINT_WIDTH = 65;
const F32 BIG_DOT_RADIUS = 5.f;
BOOL LLWorldMapView::sHandledLastClick = FALSE;

LLUIImagePtr LLWorldMapView::sAvatarSmallImage = NULL;
LLUIImagePtr LLWorldMapView::sAvatarYouImage = NULL;
LLUIImagePtr LLWorldMapView::sAvatarYouLargeImage = NULL;
LLUIImagePtr LLWorldMapView::sAvatarLevelImage = NULL;
LLUIImagePtr LLWorldMapView::sAvatarAboveImage = NULL;
LLUIImagePtr LLWorldMapView::sAvatarBelowImage = NULL;

LLUIImagePtr LLWorldMapView::sTelehubImage = NULL;
LLUIImagePtr LLWorldMapView::sInfohubImage = NULL;
LLUIImagePtr LLWorldMapView::sHomeImage = NULL;
LLUIImagePtr LLWorldMapView::sEventImage = NULL;
LLUIImagePtr LLWorldMapView::sEventMatureImage = NULL;
LLUIImagePtr LLWorldMapView::sEventAdultImage = NULL;

LLUIImagePtr LLWorldMapView::sTrackCircleImage = NULL;
LLUIImagePtr LLWorldMapView::sTrackArrowImage = NULL;

LLUIImagePtr LLWorldMapView::sClassifiedsImage = NULL;
LLUIImagePtr LLWorldMapView::sForSaleImage = NULL;
LLUIImagePtr LLWorldMapView::sForSaleAdultImage = NULL;

F32 LLWorldMapView::sThresholdA = 48.f;
F32 LLWorldMapView::sThresholdB = 96.f;
F32 LLWorldMapView::sPanX = 0.f;
F32 LLWorldMapView::sPanY = 0.f;
F32 LLWorldMapView::sTargetPanX = 0.f;
F32 LLWorldMapView::sTargetPanY = 0.f;
S32 LLWorldMapView::sTrackingArrowX = 0;
S32 LLWorldMapView::sTrackingArrowY = 0;
F32 LLWorldMapView::sPixelsPerMeter = 1.f;
bool LLWorldMapView::sVisibleTilesLoaded = false;
F32 LLWorldMapView::sMapScale = 128.f;
F32 CONE_SIZE = 0.6f;

std::map<std::string,std::string> LLWorldMapView::sStringsMap;

#define SIM_NULL_MAP_SCALE 4 // width in pixels, where we start drawing "null" sims
#define SIM_MAP_AGENT_SCALE 8 // width in pixels, where we start drawing agents
#define SIM_MAP_SCALE 4 // width in pixels, where we start drawing sim tiles

// Updates for agent locations.
#define AGENTS_UPDATE_TIME 60.0 // in seconds



void LLWorldMapView::initClass()
{
	sAvatarSmallImage = 	LLUI::getUIImage("map_avatar_8.tga");
	sAvatarYouImage =		LLUI::getUIImage("map_avatar_16.tga");
	sAvatarYouLargeImage =	LLUI::getUIImage("map_avatar_you_32.tga");
	sAvatarLevelImage = 	LLUI::getUIImage("map_avatar_32.tga");
	sAvatarAboveImage = 	LLUI::getUIImage("map_avatar_above_32.tga");
	sAvatarBelowImage = 	LLUI::getUIImage("map_avatar_below_32.tga");

	sHomeImage =			LLUI::getUIImage("map_home.tga");
	sTelehubImage = 		LLUI::getUIImage("map_telehub.tga");
	sInfohubImage = 		LLUI::getUIImage("map_infohub.tga");
	sEventImage =			LLUI::getUIImage("map_event.tga");
	sEventMatureImage =		LLUI::getUIImage("map_event_mature.tga");
	// To Do: update the image resource for adult events.
	sEventAdultImage =		LLUI::getUIImage("map_event_adult.tga");

	sTrackCircleImage =		LLUI::getUIImage("map_track_16.tga");
	sTrackArrowImage =		LLUI::getUIImage("direction_arrow.tga");
	sClassifiedsImage =		LLUI::getUIImage("icon_top_pick.tga");
	sForSaleImage =			LLUI::getUIImage("icon_for_sale.tga");
	// To Do: update the image resource for adult lands on sale.
	sForSaleAdultImage =    LLUI::getUIImage("icon_for_sale_adult.tga");
	
	sStringsMap["loading"] = LLTrans::getString("texture_loading");
	sStringsMap["offline"] = LLTrans::getString("worldmap_offline");
}

// static
void LLWorldMapView::cleanupClass()
{
	sAvatarSmallImage = NULL;
	sAvatarYouImage = NULL;
	sAvatarYouLargeImage = NULL;
	sAvatarLevelImage = NULL;
	sAvatarAboveImage = NULL;
	sAvatarBelowImage = NULL;

	sTelehubImage = NULL;
	sInfohubImage = NULL;
	sHomeImage = NULL;
	sEventImage = NULL;
	sEventMatureImage = NULL;
	sEventAdultImage = NULL;

	sTrackCircleImage = NULL;
	sTrackArrowImage = NULL;
	sClassifiedsImage = NULL;
	sForSaleImage = NULL;
	sForSaleAdultImage = NULL;
}

LLWorldMapView::LLWorldMapView(const std::string& name, const LLRect& rect )
:	LLPanel(name, rect, BORDER_NO), 
	mBackgroundColor( LLColor4( OCEAN_RED, OCEAN_GREEN, OCEAN_BLUE, 1.f ) ),
	mItemPicked(FALSE),
	mPanning( FALSE ),
	mMouseDownPanX( 0 ),
	mMouseDownPanY( 0 ),
	mMouseDownX( 0 ),
	mMouseDownY( 0 ),
	mSelectIDStart(0)
{
	sPixelsPerMeter = sMapScale / REGION_WIDTH_METERS;
	clearLastClick();

	const S32 DIR_WIDTH = 10;
	const S32 DIR_HEIGHT = 10;
	LLRect major_dir_rect(  0, DIR_HEIGHT, DIR_WIDTH, 0 );

	mTextBoxNorth = new LLTextBox( std::string("N"), major_dir_rect );
	addChild( mTextBoxNorth );

	LLColor4 minor_color( 1.f, 1.f, 1.f, .7f );
	
	mTextBoxEast =	new LLTextBox( std::string("E"), major_dir_rect );
	mTextBoxEast->setColor( minor_color );
	addChild( mTextBoxEast );
	
	major_dir_rect.mRight += 1 ;
	mTextBoxWest =	new LLTextBox( std::string("W"), major_dir_rect );
	mTextBoxWest->setColor( minor_color );
	addChild( mTextBoxWest );
	major_dir_rect.mRight -= 1 ;

	mTextBoxSouth = new LLTextBox( std::string("S"), major_dir_rect );
	mTextBoxSouth->setColor( minor_color );
	addChild( mTextBoxSouth );

	LLRect minor_dir_rect(  0, DIR_HEIGHT, DIR_WIDTH * 2, 0 );

	mTextBoxSouthEast =	new LLTextBox( std::string("SE"), minor_dir_rect );
	mTextBoxSouthEast->setColor( minor_color );
	addChild( mTextBoxSouthEast );
	
	mTextBoxNorthEast = new LLTextBox( std::string("NE"), minor_dir_rect );
	mTextBoxNorthEast->setColor( minor_color );
	addChild( mTextBoxNorthEast );
	
	mTextBoxSouthWest =	new LLTextBox( std::string("SW"), minor_dir_rect );
	mTextBoxSouthWest->setColor( minor_color );
	addChild( mTextBoxSouthWest );

	mTextBoxNorthWest = new LLTextBox( std::string("NW"), minor_dir_rect );
	mTextBoxNorthWest->setColor( minor_color );
	addChild( mTextBoxNorthWest );
}


LLWorldMapView::~LLWorldMapView()
{
	cleanupTextures();
}


// static
void LLWorldMapView::cleanupTextures()
{
}


// static
void LLWorldMapView::setScale( F32 scale )
{
	if (scale != sMapScale)
	{
		F32 old_scale = sMapScale;

		sMapScale = scale;
		if (sMapScale == 0.f)
		{
			sMapScale = 0.1f;
		}

		F32 ratio = (scale / old_scale);
		sPanX *= ratio;
		sPanY *= ratio;
		sTargetPanX = sPanX;
		sTargetPanY = sPanY;

		sPixelsPerMeter = sMapScale / REGION_WIDTH_METERS;
		sVisibleTilesLoaded = false;		
	}
}


// static
void LLWorldMapView::translatePan( S32 delta_x, S32 delta_y )
{
	sPanX += delta_x;
	sPanY += delta_y;
	sTargetPanX = sPanX;
	sTargetPanY = sPanY;
	sVisibleTilesLoaded = false;	
}


// static
void LLWorldMapView::setPan( S32 x, S32 y, BOOL snap )
{
	sTargetPanX = (F32)x;
	sTargetPanY = (F32)y;
	if (snap)
	{
		sPanX = sTargetPanX;
		sPanY = sTargetPanY;
	}
	sVisibleTilesLoaded = false;
}


///////////////////////////////////////////////////////////////////////////////////
// HELPERS

BOOL is_agent_in_region(LLViewerRegion* region, LLSimInfo* info)
{
	return ((region && info) && (info->mName == region->getName()));
}


///////////////////////////////////////////////////////////////////////////////////

void LLWorldMapView::draw()
{
	LLTextureView::clearDebugImages();

	F64 current_time = LLTimer::getElapsedSeconds();

	mVisibleRegions.clear();
	
	// animate pan if necessary
	sPanX = lerp(sPanX, sTargetPanX, LLCriticalDamp::getInterpolant(0.1f));
	sPanY = lerp(sPanY, sTargetPanY, LLCriticalDamp::getInterpolant(0.1f));

	const S32 width = getRect().getWidth();
	const S32 height = getRect().getHeight();
	LLVector3d camera_global = gAgent.getCameraPositionGlobal();

	LLLocalClipRect clip(getLocalRect());
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	
		glMatrixMode(GL_MODELVIEW);

		// Clear the background alpha to 0
		gGL.flush();
		gGL.setColorMask(false, true);
		gGL.setAlphaRejectSettings(LLRender::CF_GREATER_EQUAL, 0.f);
		gGL.setSceneBlendType(LLRender::BT_REPLACE);
		gGL.color4f(0.0f, 0.0f, 0.0f, 0.0f);
		gl_rect_2d(0, height, width, 0);
	}

	gGL.flush();
	gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
	gGL.setColorMask(true, true);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	if(gHippoGridManager->getConnectedGrid()->isSecondLife())
		drawMipmap(width, height);
	else
		drawTiles(width, height);
	
	LLFontGL* font = LLFontGL::getFontSansSerifSmall();
	const F32 half_width = F32(width) / 2.0f;
	const F32 half_height = F32(height) / 2.0f;	

	// Draw the region name in the lower left corner	
	for (LLWorldMap::sim_info_map_t::iterator it = LLWorldMap::getInstance()->mSimInfoMap.begin();
		 it != LLWorldMap::getInstance()->mSimInfoMap.end(); ++it)
	{
		U64 handle = it->first;
		LLSimInfo* info = (*it).second;		
		
		LLVector3d origin_global = from_region_handle(handle);

		// Find x and y position relative to camera's center.
		LLVector3d rel_region_pos = origin_global - camera_global;
		F32 relative_x = (rel_region_pos.mdV[0] / REGION_WIDTH_METERS) * sMapScale;
		F32 relative_y = (rel_region_pos.mdV[1] / REGION_WIDTH_METERS) * sMapScale;
		
		F32 bottom =	sPanY + half_height + relative_y;
		F32 left =		sPanX + half_width + relative_x;
		F32 top =		bottom + sMapScale ;
		F32 right =		left + sMapScale ;
		
		// disregard regions that are outside the rectangle
		if (top < 0.f ||
			bottom > height ||
			right < 0.f ||
			left > width )
		{
			continue;			
		}
		
		info->mShowAgentLocations = (sMapScale >= SIM_MAP_AGENT_SCALE);
		mVisibleRegions.push_back(handle);		
		// See if the agents need updating
		if (current_time - info->mAgentsUpdateTime > AGENTS_UPDATE_TIME)
		{
			LLWorldMap::getInstance()->sendItemRequest(MAP_ITEM_AGENT_LOCATIONS, info->mHandle);
			info->mAgentsUpdateTime = current_time;
		}		
		
		std::string mesg;
		if (sMapScale < sThresholdA)
		{
		}
		else if (sMapScale < sThresholdB)
		{
			//	mesg = llformat( info->mAgents);
		}
		else
		{
			//mesg = llformat("%d / %s (%s)",
			//			info->mAgents,
			//			info->mName.c_str(),
			//			LLViewerRegion::accessToShortString(info->mAccess).c_str() );
			// [RLVa:KB] - Alternate: Snowglobe-1.2.4 | Checked: 2009-07-04 (RLVa-1.0.0a)
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
			{
				mesg = RlvStrings::getString(RLV_STRING_HIDDEN);
			}
			// [/RLVa:KB]
			else if (info->mAccess == SIM_ACCESS_DOWN)
			{
				mesg = llformat( "%s (%s)", info->mName.c_str(), sStringsMap["offline"].c_str());
			}
			else
			{
				mesg = info->mName;
				U8 access = info->mAccess;
				switch(access)
				{
				case SIM_ACCESS_MIN:
					mesg += " (Min)";
					break;
				case SIM_ACCESS_PG:
					mesg += " (PG)";
					break;
				case SIM_ACCESS_MATURE:
					mesg += " (Mature)";
					break;
				case SIM_ACCESS_ADULT:
					mesg += " (Adult)";
					break;
				default:
					mesg += llformat(" (Access=%d)",access);
					break;
				}
			}
		}
		
		if (!mesg.empty())
		{
			font->renderUTF8(
					mesg, 0,
					llfloor(left + 3), 
					llfloor(bottom + 2),
					LLColor4::white,
					LLFontGL::LEFT,
					LLFontGL::BASELINE,
					LLFontGL::DROP_SHADOW);
			
			// If map texture is still loading,
			// display "Loading" placeholder text.
			/*if ((simimage != NULL) &&
				simimage->getDiscardLevel() != 1 &&
				simimage->getDiscardLevel() != 0)
			{
				font->renderUTF8(
						sStringsMap["loading"], 0,
						llfloor(left + 18), 
						llfloor(top - 25),
						LLColor4::white,
						LLFontGL::LEFT,
						LLFontGL::BASELINE,
						LLFontGL::DROP_SHADOW);			
			}*/
		}

	}
	
	
	// Draw background rectangle
	LLGLSUIDefault gls_ui;
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.setAlphaRejectSettings(LLRender::CF_GREATER_EQUAL, 0.f);
		gGL.blendFunc(LLRender::BF_ONE_MINUS_DEST_ALPHA, LLRender::BF_DEST_ALPHA);
		gGL.color4fv( mBackgroundColor.mV );
		gl_rect_2d(0, height, width, 0);
	}
	
	gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	// Infohubs
	if (gSavedSettings.getBOOL("MapShowInfohubs"))   //(sMapScale >= sThresholdB)
	{
		drawGenericItems(LLWorldMap::getInstance()->mInfohubs, sInfohubImage);
	}

	// Telehubs
	if (gSavedSettings.getBOOL("MapShowTelehubs"))   //(sMapScale >= sThresholdB)
	{
		drawGenericItems(LLWorldMap::getInstance()->mTelehubs, sTelehubImage);
	}

	// Home Sweet Home
	LLVector3d home;
	if (gAgent.getHomePosGlobal(&home))
	{
		drawImage(home, sHomeImage);
	}

	if (gSavedSettings.getBOOL("MapShowLandForSale"))
	{
		drawGenericItems(LLWorldMap::getInstance()->mLandForSale, sForSaleImage);
		// for 1.23, we're showing normal land and adult land in the same UI; you don't
		// get a choice about which ones you want. If you're currently asking for adult
		// content and land you'll get the adult land.
		if (gAgent.canAccessAdult())
		{
			drawGenericItems(LLWorldMap::getInstance()->mLandForSaleAdult, sForSaleAdultImage);
		}
	}
	
	if (gSavedSettings.getBOOL("MapShowPGEvents") ||
		gSavedSettings.getBOOL("MapShowMatureEvents") ||
		gSavedSettings.getBOOL("MapShowAdultEvents") )
	{
		drawEvents();
	}

	// Now draw your avatar after all that other stuff.
	LLVector3d pos_global = gAgent.getPositionGlobal();
	drawImage(pos_global, sAvatarYouImage);

	LLVector3 pos_map = globalPosToView(pos_global);
	if (!pointInView(llround(pos_map.mV[VX]), llround(pos_map.mV[VY])))
	{
		drawTracking(pos_global, 
			lerp(LLColor4::yellow, LLColor4::orange, 0.4f), 
			TRUE, 
			"You are here", 
			"", 
			llround(LLFontGL::getFontSansSerifSmall()->getLineHeight())); // offset vertically by one line, to avoid overlap with target tracking
	}

	// Show your viewing angle
	drawFrustum();

	// Draw icons for the avatars in each region.
	// Drawn after your avatar so you can see nearby people.
	if (gSavedSettings.getBOOL("MapShowPeople"))
	{
		drawAgents();
	}

	// Always draw tracking information
	LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
	if ( LLTracker::TRACKING_AVATAR == tracking_status )
	{
		drawTracking( LLAvatarTracker::instance().getGlobalPos(), gTrackColor, TRUE, LLTracker::getLabel(), "" );
	} 
	else if ( LLTracker::TRACKING_LANDMARK == tracking_status 
			 || LLTracker::TRACKING_LOCATION == tracking_status )
	{
		// While fetching landmarks, will have 0,0,0 location for a while,
		// so don't draw. JC
		LLVector3d pos_global = LLTracker::getTrackedPositionGlobal();
		if (!pos_global.isExactlyZero())
		{
			drawTracking( pos_global, gTrackColor, TRUE, LLTracker::getLabel(), LLTracker::getToolTip() );
		}
	}
	else if (LLWorldMap::getInstance()->mIsTrackingUnknownLocation)
	{
		if (LLWorldMap::getInstance()->mInvalidLocation)
		{
			// We know this location to be invalid
			LLColor4 loading_color(0.0, 0.5, 1.0, 1.0);
			drawTracking( LLWorldMap::getInstance()->mUnknownLocation, loading_color, TRUE, "Invalid Location", "");
		}
		else
		{
			double value = fmod(current_time, 2);
			value = 0.5 + 0.5*cos(value * 3.14159f);
			LLColor4 loading_color(0.0, F32(value/2), F32(value), 1.0);
			drawTracking( LLWorldMap::getInstance()->mUnknownLocation, loading_color, TRUE, "Loading...", "");
		}
	}
	// #endif used to be here
	
	// turn off the scissor
	LLGLDisable no_scissor(GL_SCISSOR_TEST);
	
	updateDirections();

	LLView::draw();

	updateVisibleBlocks();
} // end draw()


//virtual
void LLWorldMapView::setVisible(BOOL visible)
{
	LLPanel::setVisible(visible);
	if (!visible)
	{
		for (S32 map = 0; map < MAP_SIM_IMAGE_TYPES; map++)
	{
			for (U32 layer_idx=0; layer_idx<LLWorldMap::getInstance()->mMapLayers[map].size(); ++layer_idx)
		{
				if (LLWorldMap::getInstance()->mMapLayers[map][layer_idx].LayerDefined)
		{
					LLWorldMapLayer *layer = &LLWorldMap::getInstance()->mMapLayers[map][layer_idx];
					layer->LayerImage->setBoostLevel(0);
		}
	}
				}
		for (LLWorldMap::sim_info_map_t::iterator it = LLWorldMap::getInstance()->mSimInfoMap.begin();
			 it != LLWorldMap::getInstance()->mSimInfoMap.end(); ++it)
		{
			LLSimInfo* info = (*it).second;
			if (info->mCurrentImage.notNull())
			{
				info->mCurrentImage->setBoostLevel(0);
			}
			if (info->mOverlayImage.notNull())
			{
				info->mOverlayImage->setBoostLevel(0);
			}
		}
	}
}

void LLWorldMapView::drawTiles(S32 width, S32 height) {
	const F32 half_width = F32(width) / 2.0f;
	const F32 half_height = F32(height) / 2.0f;
	F32 layer_alpha = 1.f;
	LLVector3d camera_global = gAgent.getCameraPositionGlobal();
	
	// Draw one image per layer
	for (U32 layer_idx=0; layer_idx<LLWorldMap::getInstance()->mMapLayers[LLWorldMap::getInstance()->mCurrentMap].size(); ++layer_idx)
	{
		if (!LLWorldMap::getInstance()->mMapLayers[LLWorldMap::getInstance()->mCurrentMap][layer_idx].LayerDefined)
		{
			continue;
		}
		LLWorldMapLayer *layer = &LLWorldMap::getInstance()->mMapLayers[LLWorldMap::getInstance()->mCurrentMap][layer_idx];
		LLViewerTexture *current_image = layer->LayerImage;

		if (current_image->isMissingAsset())
		{
			continue; // better to draw nothing than the missing asset image
		}
		
		LLVector3d origin_global((F64)layer->LayerExtents.mLeft * REGION_WIDTH_METERS, (F64)layer->LayerExtents.mBottom * REGION_WIDTH_METERS, 0.f);

		// Find x and y position relative to camera's center.
		LLVector3d rel_region_pos = origin_global - camera_global;
		F32 relative_x = (rel_region_pos.mdV[0] / REGION_WIDTH_METERS) * sMapScale;
		F32 relative_y = (rel_region_pos.mdV[1] / REGION_WIDTH_METERS) * sMapScale;

		F32 pix_width = sMapScale*(layer->LayerExtents.getWidth() + 1);
		F32 pix_height = sMapScale*(layer->LayerExtents.getHeight() + 1);

		// When the view isn't panned, 0,0 = center of rectangle
		F32 bottom =	sPanY + half_height + relative_y;
		F32 left =		sPanX + half_width + relative_x;
		F32 top =		bottom + pix_height;
		F32 right =		left + pix_width;
		F32 pixel_area = pix_width*pix_height;
		// discard layers that are outside the rectangle
		// and discard small layers
		if (top < 0.f ||
			bottom > height ||
			right < 0.f ||
			left > width ||
			(pixel_area < 4*4))
		{
			current_image->setBoostLevel(0);
			continue;
		}
		
		current_image->setBoostLevel(LLViewerTexture::BOOST_MAP_VISIBLE);
		current_image->setKnownDrawSize(llround(pix_width * LLUI::sGLScaleFactor.mV[VX]), llround(pix_height * LLUI::sGLScaleFactor.mV[VY]));
		
		if (!current_image->hasGLTexture())
		{
			continue; // better to draw nothing than the default image
		}

// 		LLTextureView::addDebugImage(current_image);
		
		// Draw using the texture.  If we don't clamp we get artifact at
		// the edge.
		gGL.getTexUnit(0)->bind(current_image);

		// Draw map image into RGB
		//gGL.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gGL.flush();
		gGL.setColorMask(true, false);
		gGL.color4f(1.f, 1.f, 1.f, layer_alpha);

		gGL.begin(LLRender::QUADS);
			gGL.texCoord2f(0.0f, 1.0f);
			gGL.vertex3f(left, top, -1.0f);
			gGL.texCoord2f(0.0f, 0.0f);
			gGL.vertex3f(left, bottom, -1.0f);
			gGL.texCoord2f(1.0f, 0.0f);
			gGL.vertex3f(right, bottom, -1.0f);
			gGL.texCoord2f(1.0f, 1.0f);
			gGL.vertex3f(right, top, -1.0f);
		gGL.end();

		// draw an alpha of 1 where the sims are visible
	gGL.flush();
		gGL.setColorMask(false, true);
		gGL.color4f(1.f, 1.f, 1.f, 1.f);

		gGL.begin(LLRender::QUADS);
			gGL.texCoord2f(0.0f, 1.0f);
			gGL.vertex2f(left, top);
			gGL.texCoord2f(0.0f, 0.0f);
			gGL.vertex2f(left, bottom);
			gGL.texCoord2f(1.0f, 0.0f);
			gGL.vertex2f(right, bottom);
			gGL.texCoord2f(1.0f, 1.0f);
			gGL.vertex2f(right, top);
		gGL.end();
	}

	gGL.flush();
	gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
	gGL.setColorMask(true, true);

	F32 sim_alpha = 1.f;

	// Draw one image per region, centered on the camera position.
	const S32 MAX_SIMULTANEOUS_TEX = 100;
	const S32 MAX_REQUEST_PER_TICK = 5;
	const S32 MIN_REQUEST_PER_TICK = 1;
	S32 textures_requested_this_tick = 0;

	bool use_web_map_tiles = LLWorldMap::useWebMapTiles();

	for (LLWorldMap::sim_info_map_t::iterator it = LLWorldMap::getInstance()->mSimInfoMap.begin();
		 it != LLWorldMap::getInstance()->mSimInfoMap.end(); ++it)
	{
		U64 handle = (*it).first;
		LLSimInfo* info = (*it).second;

		LLViewerTexture* simimage = info->mCurrentImage;
		LLViewerTexture* overlayimage = info->mOverlayImage;

		if (sMapScale < SIM_MAP_SCALE)
		{
			if (simimage != NULL) simimage->setBoostLevel(0);
			if (overlayimage != NULL) overlayimage->setBoostLevel(0);
			continue;
		}

		LLVector3d origin_global = from_region_handle(handle);
		LLVector3d camera_global = gAgent.getCameraPositionGlobal();

		// Find x and y position relative to camera's center.
		LLVector3d rel_region_pos = origin_global - camera_global;
		F32 relative_x = (rel_region_pos.mdV[0] / REGION_WIDTH_METERS) * sMapScale;
		F32 relative_y = (rel_region_pos.mdV[1] / REGION_WIDTH_METERS) * sMapScale;

		// When the view isn't panned, 0,0 = center of rectangle
		F32 bottom =	sPanY + half_height + relative_y;
		F32 left =		sPanX + half_width + relative_x;
		F32 top =		bottom + sMapScale ;
		F32 right =		left + sMapScale ;

		// Switch to world map texture (if available for this region) if either:
		// 1. Tiles are zoomed out small enough, or
		// 2. Sim's texture has not been loaded yet
		F32 map_scale_cutoff = SIM_MAP_SCALE;
		if ((info->mRegionFlags & REGION_FLAGS_NULL_LAYER) > 0)
		{
			map_scale_cutoff = SIM_NULL_MAP_SCALE;
		}

		bool sim_visible =
			(sMapScale >= map_scale_cutoff) &&
			(simimage != NULL) &&
			(simimage->hasGLTexture());

		if (sim_visible)
		{
			// Fade in
			if (info->mAlpha < 0.0f)
				info->mAlpha = 1.f; // don't fade initially
			else
				info->mAlpha = lerp(info->mAlpha, 1.f, LLCriticalDamp::getInterpolant(0.15f));
		}
		else
		{
			// Fade out
			if (info->mAlpha < 0.0f)
				info->mAlpha = 0.f; // don't fade initially
			else
				info->mAlpha = lerp(info->mAlpha, 0.f, LLCriticalDamp::getInterpolant(0.15f));
		}

		// discard regions that are outside the rectangle
		// and discard small regions
		if (top < 0.f ||
			bottom > height ||
			right < 0.f ||
			left > width )
		{
			if (simimage != NULL) simimage->setBoostLevel(0);
			if (overlayimage != NULL) overlayimage->setBoostLevel(0);
			continue;
		}

		if (info->mCurrentImage.isNull())
		{
			if ((textures_requested_this_tick < MIN_REQUEST_PER_TICK) ||
				((LLAppViewer::getTextureFetch()->getNumRequests() < MAX_SIMULTANEOUS_TEX) &&
				 (textures_requested_this_tick < MAX_REQUEST_PER_TICK)))
			{
				textures_requested_this_tick++;
				if (use_web_map_tiles)
				{
					LLVector3d region_pos = info->getGlobalOrigin();
					info->mCurrentImage = LLWorldMap::loadObjectsTile((U32)(region_pos.mdV[VX] / REGION_WIDTH_UNITS), (U32)(region_pos.mdV[VY] / REGION_WIDTH_UNITS));
				}
				else
				{
					info->mCurrentImage = LLViewerTextureManager::getFetchedTexture(info->mMapImageID[LLWorldMap::getInstance()->mCurrentMap], MIPMAP_TRUE);
				}
                info->mCurrentImage->setAddressMode(LLTexUnit::TAM_CLAMP);
				simimage = info->mCurrentImage;
				gGL.getTexUnit(0)->bind(simimage);
			}
		}
		if (info->mOverlayImage.isNull() && info->mMapImageID[2].notNull())
		{
			if ((textures_requested_this_tick < MIN_REQUEST_PER_TICK) ||
				((LLAppViewer::getTextureFetch()->getNumRequests() < MAX_SIMULTANEOUS_TEX) &&
				 (textures_requested_this_tick < MAX_REQUEST_PER_TICK)))
			{
				textures_requested_this_tick++;
				info->mOverlayImage = LLViewerTextureManager::getFetchedTexture(info->mMapImageID[2], MIPMAP_TRUE);
				info->mOverlayImage->setAddressMode(LLTexUnit::TAM_CLAMP);
				overlayimage = info->mOverlayImage;
				gGL.getTexUnit(0)->bind(overlayimage);
			}
		}

		// Bias the priority escalation for images nearer
		LLVector3d center_global = origin_global;
		center_global.mdV[VX] += 128.0;
		center_global.mdV[VY] += 128.0;

		S32 draw_size = llround(sMapScale);
		if (simimage != NULL)
		{
			simimage->setBoostLevel(LLViewerTexture::BOOST_MAP);
			simimage->setKnownDrawSize(llround(draw_size * LLUI::sGLScaleFactor.mV[VX]), llround(draw_size * LLUI::sGLScaleFactor.mV[VY]));
		}

		if (overlayimage != NULL)
		{
			overlayimage->setBoostLevel(LLViewerTexture::BOOST_MAP);
			overlayimage->setKnownDrawSize(llround(draw_size * LLUI::sGLScaleFactor.mV[VX]), llround(draw_size * LLUI::sGLScaleFactor.mV[VY]));
		}
			
// 		LLTextureView::addDebugImage(simimage);

		if (sim_visible && info->mAlpha > 0.001f)
		{
			// Draw using the texture.  If we don't clamp we get artifact at
			// the edge.
			LLGLSUIDefault gls_ui;
			if (simimage != NULL)
				gGL.getTexUnit(0)->bind(simimage);

			gGL.setSceneBlendType(LLRender::BT_ALPHA);
			F32 alpha = sim_alpha * info->mAlpha;
			gGL.color4f(1.f, 1.0f, 1.0f, alpha);

			gGL.begin(LLRender::QUADS);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex3f(left, top, 0.f);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex3f(left, bottom, 0.f);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex3f(right, bottom, 0.f);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex3f(right, top, 0.f);
			gGL.end();

			if (gSavedSettings.getBOOL("MapShowLandForSale") && overlayimage && overlayimage->hasGLTexture())
			{
				gGL.getTexUnit(0)->bind(overlayimage);
				gGL.color4f(1.f, 1.f, 1.f, alpha);
				gGL.begin(LLRender::QUADS);
					gGL.texCoord2f(0.f, 1.f);
					gGL.vertex3f(left, top, -0.5f);
					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex3f(left, bottom, -0.5f);
					gGL.texCoord2f(1.f, 0.f);
					gGL.vertex3f(right, bottom, -0.5f);
					gGL.texCoord2f(1.f, 1.f);
					gGL.vertex3f(right, top, -0.5f);
				gGL.end();
			}
		
			if ((info->mRegionFlags & REGION_FLAGS_NULL_LAYER) == 0)
			{
				// draw an alpha of 1 where the sims are visible (except NULL sims)
				gGL.flush();
				gGL.setSceneBlendType(LLRender::BT_REPLACE);
				gGL.setColorMask(false, true);
				gGL.color4f(1.f, 1.f, 1.f, 1.f);

				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				gGL.begin(LLRender::QUADS);
					gGL.vertex2f(left, top);
					gGL.vertex2f(left, bottom);
					gGL.vertex2f(right, bottom);
					gGL.vertex2f(right, top);
				gGL.end();

				gGL.flush();
				gGL.setColorMask(true, true);
			}
		}

		if (info->mAccess == SIM_ACCESS_DOWN)
		{
			// Draw a transparent red square over down sims
			gGL.blendFunc(LLRender::BF_DEST_ALPHA, LLRender::BF_SOURCE_ALPHA);
			gGL.color4f(0.2f, 0.0f, 0.0f, 0.4f);

			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gGL.begin(LLRender::QUADS);
				gGL.vertex2f(left, top);
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(right, bottom);
				gGL.vertex2f(right, top);
			gGL.end();
		}

		// As part of the AO project, we no longer want to draw access indicators;
		// it's too complicated to get all the rules straight and will only 
		// cause confusion.
		/**********************
		 // If this is mature, and you are not, draw a line across it
		if (info->mAccess != SIM_ACCESS_DOWN
			&& info->mAccess > SIM_ACCESS_PG
			&& gAgent.isTeen())
		{
			gGL.blendFunc(LLRender::BF_DEST_ALPHA, LLRender::BF_ZERO);
			
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gGL.color3f(1.f, 0.f, 0.f);
			gGL.begin(LLRender::LINES);
				gGL.vertex2f(left, top);
				gGL.vertex2f(right, bottom);
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(right, top);
			gGL.end();
		}
		 **********************/
	}
}

void LLWorldMapView::drawGenericItems(const LLWorldMap::item_info_list_t& items, LLUIImagePtr image)
{
	LLWorldMap::item_info_list_t::const_iterator e;
	for (e = items.begin(); e != items.end(); ++e)
	{
		drawGenericItem(*e, image);
	}
}

void LLWorldMapView::drawGenericItem(const LLItemInfo& item, LLUIImagePtr image)
{
	drawImage(item.mPosGlobal, image);
}


void LLWorldMapView::drawImage(const LLVector3d& global_pos, LLUIImagePtr image, const LLColor4& color)
{
	LLVector3 pos_map = globalPosToView( global_pos );
	image->draw(llround(pos_map.mV[VX] - image->getWidth() /2.f),
				llround(pos_map.mV[VY] - image->getHeight()/2.f),
				color);
}

void LLWorldMapView::drawImageStack(const LLVector3d& global_pos, LLUIImagePtr image, U32 count, F32 offset, const LLColor4& color)
{
	LLVector3 pos_map = globalPosToView( global_pos );
	for(U32 i=0; i<count; i++)
	{
		image->draw(llround(pos_map.mV[VX] - image->getWidth() /2.f),
					llround(pos_map.mV[VY] - image->getHeight()/2.f + i*offset),
					color);
	}
}


void LLWorldMapView::drawAgents()
{	
	if(sMapScale < SIM_MAP_AGENT_SCALE)
		return;
		
	F32 agents_scale = (sMapScale * 0.9f) / 256.f;

	LLColor4 avatar_color = gColors.getColor( "MapAvatar" );
	//	LLColor4 friend_color = gColors.getColor( "MapFriend" );

	for (handle_list_t::iterator iter = mVisibleRegions.begin(); iter != mVisibleRegions.end(); ++iter)
	{
		U64 handle = *iter;
		LLSimInfo* siminfo = LLWorldMap::getInstance()->simInfoFromHandle(handle);
		if (siminfo && (siminfo->mAccess == SIM_ACCESS_DOWN))
		{
			continue;
		}
		LLWorldMap::agent_list_map_t::iterator counts_iter = LLWorldMap::getInstance()->mAgentLocationsMap.find(handle);
		if (siminfo && siminfo->mShowAgentLocations && counts_iter != LLWorldMap::getInstance()->mAgentLocationsMap.end())
		{
			// Show Individual agents (or little stacks where real agents are)
			LLWorldMap::item_info_list_t& agentcounts = counts_iter->second;
			S32 sim_agent_count = 0;
			for (LLWorldMap::item_info_list_t::iterator iter = agentcounts.begin();
				 iter != agentcounts.end(); ++iter)
			{
				const LLItemInfo& info = *iter;
				S32 agent_count = info.mExtra;
				sim_agent_count += info.mExtra;
				// Here's how we'd choose the color if info.mID were available but it's not being sent:
				//LLColor4 color = (agent_count == 1 && is_agent_friend(info.mID)) ? friend_color : avatar_color;
				drawImageStack(info.mPosGlobal, sAvatarSmallImage, agent_count, 3.f, avatar_color);
		}
			LLWorldMap::getInstance()->mNumAgents[handle] = sim_agent_count; // override mNumAgents for this sim
		}
		else
		{
			// Show agent 'stack' at center of sim
			S32 num_agents = LLWorldMap::getInstance()->mNumAgents[handle];
			if (num_agents > 0)
			{
				LLVector3d region_center = from_region_handle(handle);
				region_center[VX] += REGION_WIDTH_METERS / 2;
				region_center[VY] += REGION_WIDTH_METERS / 2;
				// Reduce the stack size as you zoom out - always display at lease one agent where there is one or more
				S32 agent_count = (S32)(((num_agents-1) * agents_scale + (num_agents-1) * 0.1f)+.1f) + 1;
				drawImageStack(region_center, sAvatarSmallImage, agent_count, 3.f, avatar_color);
			}
			}
		}
}


void LLWorldMapView::drawEvents()
{
	bool mature_enabled = gAgent.canAccessMature();
	bool adult_enabled = gAgent.canAccessAdult();

	BOOL show_pg = gSavedSettings.getBOOL("MapShowPGEvents");
	BOOL show_mature = mature_enabled && gSavedSettings.getBOOL("MapShowMatureEvents");
	BOOL show_adult = adult_enabled && gSavedSettings.getBOOL("MapShowAdultEvents");

    // First the non-selected events
    LLWorldMap::item_info_list_t::const_iterator e;
	if (show_pg)
	{
		for (e = LLWorldMap::getInstance()->mPGEvents.begin(); e != LLWorldMap::getInstance()->mPGEvents.end(); ++e)
		{
			if (!e->mSelected)
		{
				drawGenericItem(*e, sEventImage);   
			}
		}
		}
		if (show_mature)
		{
        for (e = LLWorldMap::getInstance()->mMatureEvents.begin(); e != LLWorldMap::getInstance()->mMatureEvents.end(); ++e)
        {
            if (!e->mSelected)
            {
                drawGenericItem(*e, sEventMatureImage);       
            }
        }
		}
		if (show_adult)
		{
        for (e = LLWorldMap::getInstance()->mAdultEvents.begin(); e != LLWorldMap::getInstance()->mAdultEvents.end(); ++e)
        {
            if (!e->mSelected)
            {
                drawGenericItem(*e, sEventAdultImage);       
		}
	}
    }
    // Then the selected events
	if (show_pg)
	{
		for (e = LLWorldMap::getInstance()->mPGEvents.begin(); e != LLWorldMap::getInstance()->mPGEvents.end(); ++e)
		{
			if (e->mSelected)
			{
				drawGenericItem(*e, sEventImage);
		}
		}
	}
    if (show_mature)
		{
        for (e = LLWorldMap::getInstance()->mMatureEvents.begin(); e != LLWorldMap::getInstance()->mMatureEvents.end(); ++e)
        {
            if (e->mSelected)
            {
                drawGenericItem(*e, sEventMatureImage);       
            }
        }
    }
	if (show_adult)
    {
        for (e = LLWorldMap::getInstance()->mAdultEvents.begin(); e != LLWorldMap::getInstance()->mAdultEvents.end(); ++e)
        {
            if (e->mSelected)
            {
                drawGenericItem(*e, sEventAdultImage);       
            }
		}
	}
}


void LLWorldMapView::drawFrustum()
{
	// Draw frustum
	F32 meters_to_pixels = sMapScale/ REGION_WIDTH_METERS;

	F32 horiz_fov = LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect();
	F32 far_clip_meters = LLViewerCamera::getInstance()->getFar();
	F32 far_clip_pixels = far_clip_meters * meters_to_pixels;

	F32 half_width_meters = far_clip_meters * tan( horiz_fov / 2 );
	F32 half_width_pixels = half_width_meters * meters_to_pixels;
	
	F32 ctr_x = getRect().getWidth() * 0.5f + sPanX;
	F32 ctr_y = getRect().getHeight() * 0.5f + sPanY;

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	// Since we don't rotate the map, we have to rotate the frustum.
	gGL.pushMatrix();
		gGL.translatef( ctr_x, ctr_y, 0 );
		glRotatef( atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] ) * RAD_TO_DEG, 0.f, 0.f, -1.f);

		// Draw triangle with more alpha in far pixels to make it 
		// fade out in distance.
		gGL.begin( LLRender::TRIANGLES  );
			gGL.color4f(1.f, 1.f, 1.f, 0.25f);
			gGL.vertex2f( 0, 0 );

			gGL.color4f(1.f, 1.f, 1.f, 0.02f);
			gGL.vertex2f( -half_width_pixels, far_clip_pixels );

			gGL.color4f(1.f, 1.f, 1.f, 0.02f);
			gGL.vertex2f(  half_width_pixels, far_clip_pixels );
		gGL.end();
	gGL.popMatrix();
}

void LLWorldMapView::drawMipmap(S32 width, S32 height)
{
	// Compute the level of the mipmap to use for the current scale level
	S32 level = LLWorldMipmap::scaleToLevel(sMapScale);
	// Set the tile boost level so that unused tiles get to 0
	LLWorldMap::getInstance()->equalizeBoostLevels();

	// Render whatever we already have loaded if we haven't the current level
	// complete and use it as a background (scaled up or scaled down)
	if (!sVisibleTilesLoaded)
	{
		// Note: the (load = false) parameter avoids missing tiles to be fetched (i.e. we render what we have, no more)
		// Check all the lower res levels and render them in reverse order (worse to best)
		// We need to traverse all the levels as the user can zoom in very fast
		for (S32 l = LLWorldMipmap::MAP_LEVELS; l > level; l--)
		{
			drawMipmapLevel(width, height, l, false);
		}
		// Skip the current level, as we'll do it anyway here under...

		// Just go one level down in res as it can really get too much stuff 
		// when zooming out and too small to see anyway...
		if (level > 1)
		{
			drawMipmapLevel(width, height, level - 1, false);
		}
	}
	else
	{
		//LL_INFOS("World Map") << "Render complete, don't draw background..." << LL_ENDL;
	}

	// Render the current level
	sVisibleTilesLoaded = drawMipmapLevel(width, height, level);

	return;
}

// Return true if all the tiles required to render that level have been fetched or are truly missing
bool LLWorldMapView::drawMipmapLevel(S32 width, S32 height, S32 level, bool load)
{
	// Check input level
	llassert (level > 0);
	if (level <= 0)
		return false;

	// Count tiles hit and completed
	S32 completed_tiles = 0;
	S32 total_tiles = 0;

	// Size in meters (global) of each tile of that level
	S32 tile_width = LLWorldMipmap::MAP_TILE_SIZE * (1 << (level - 1));
	// Dimension of the screen in meter at that scale
	LLVector3d pos_SW = viewPosToGlobal(0, 0);
	LLVector3d pos_NE = viewPosToGlobal(width, height);
	// Add external band of tiles on the outskirt so to hit the partially displayed tiles right and top
	pos_NE[VX] += tile_width;
	pos_NE[VY] += tile_width;

	// Iterate through the tiles on screen: we just need to ask for one tile every tile_width meters
	U32 grid_x, grid_y;
	for (F64 index_y = pos_SW[VY]; index_y < pos_NE[VY]; index_y += tile_width)
	{
		for (F64 index_x = pos_SW[VX]; index_x < pos_NE[VX]; index_x += tile_width)
		{
			// Compute the world coordinates of the current point
			LLVector3d pos_global(index_x, index_y, pos_SW[VZ]);
			// Convert to the mipmap level coordinates for that point (i.e. which tile to we hit)
			LLWorldMipmap::globalToMipmap(pos_global[VX], pos_global[VY], level, &grid_x, &grid_y);
			// Get the tile. Note: NULL means that the image does not exist (so it's considered "complete" as far as fetching is concerned)
			LLPointer<LLViewerFetchedTexture> simimage = LLWorldMap::getInstance()->getObjectsTile(grid_x, grid_y, level, load);
			if (simimage)
			{
				// Check the texture state
				if (simimage->hasGLTexture())
				{
					// Increment the number of completly fetched tiles
					completed_tiles++;

					// Convert those coordinates (SW corner of the mipmap tile) into world (meters) coordinates
					pos_global[VX] = grid_x * REGION_WIDTH_METERS;
					pos_global[VY] = grid_y * REGION_WIDTH_METERS;
					// Now to screen coordinates for SW corner of that tile
					LLVector3 pos_screen = globalPosToView (pos_global);
					F32 left   = pos_screen[VX];
					F32 bottom = pos_screen[VY];
					// Compute the NE corner coordinates of the tile now
					pos_global[VX] += tile_width;
					pos_global[VY] += tile_width;
					pos_screen = globalPosToView (pos_global);
					F32 right  = pos_screen[VX];
					F32 top    = pos_screen[VY];

					// Draw the tile
					LLGLSUIDefault gls_ui;
					gGL.getTexUnit(0)->bind(simimage.get());
					simimage->setAddressMode(LLTexUnit::TAM_CLAMP);

					gGL.setSceneBlendType(LLRender::BT_ALPHA);
					gGL.color4f(1.f, 1.0f, 1.0f, 1.0f);

					gGL.begin(LLRender::QUADS);
						gGL.texCoord2f(0.f, 1.f);
						gGL.vertex3f(left, top, 0.f);
						gGL.texCoord2f(0.f, 0.f);
						gGL.vertex3f(left, bottom, 0.f);
						gGL.texCoord2f(1.f, 0.f);
						gGL.vertex3f(right, bottom, 0.f);
						gGL.texCoord2f(1.f, 1.f);
						gGL.vertex3f(right, top, 0.f);
					gGL.end();
#if DEBUG_DRAW_TILE
					drawTileOutline(level, top, left, bottom, right);
#endif // DEBUG_DRAW_TILE
				}
				//else
				//{
				//	Waiting for a tile -> the level is not complete
				//	LL_INFOS("World Map") << "Unfetched tile. level = " << level << LL_ENDL;
				//}
			}
			else
			{
				// Unexistent tiles are counted as "completed"
				completed_tiles++;
			}
			// Increment the number of tiles in that level / screen
			total_tiles++;
		}
	}
	return (completed_tiles == total_tiles);
}

LLVector3 LLWorldMapView::globalPosToView( const LLVector3d& global_pos )
{
	LLVector3d relative_pos_global = global_pos - gAgent.getCameraPositionGlobal();
	LLVector3 pos_local;
	pos_local.setVec(relative_pos_global);  // convert to floats from doubles

	pos_local.mV[VX] *= sPixelsPerMeter;
	pos_local.mV[VY] *= sPixelsPerMeter;
	// leave Z component in meters


	pos_local.mV[VX] += getRect().getWidth() / 2 + sPanX;
	pos_local.mV[VY] += getRect().getHeight() / 2 + sPanY;

	return pos_local;
}


void LLWorldMapView::drawTracking(const LLVector3d& pos_global, const LLColor4& color, BOOL draw_arrow,
								  const std::string& label, const std::string& tooltip, S32 vert_offset )
{
	LLVector3 pos_local = globalPosToView( pos_global );
	S32 x = llround( pos_local.mV[VX] );
	S32 y = llround( pos_local.mV[VY] );
	LLFontGL* font = LLFontGL::getFontSansSerifSmall();
	S32 text_x = x;
	S32 text_y = (S32)(y - sTrackCircleImage->getHeight()/2 - font->getLineHeight());

	BOOL is_in_window = true;

	if(    x < 0 
		|| y < 0 
		|| x >= getRect().getWidth() 
		|| y >= getRect().getHeight() )
	{
		if (draw_arrow)
		{
			drawTrackingCircle( getRect(), x, y, color, 3, 15 );
			drawTrackingArrow( getRect(), x, y, color );
			text_x = sTrackingArrowX;
			text_y = sTrackingArrowY;
		}
		is_in_window = false;
	}
	else if (LLTracker::getTrackingStatus() == LLTracker::TRACKING_LOCATION &&
		LLTracker::getTrackedLocationType() != LLTracker::LOCATION_NOTHING)
	{
		drawTrackingCircle( getRect(), x, y, color, 3, 15 );
	}
	else
	{
		drawImage(pos_global, sTrackCircleImage, color);
	}

	// clamp text position to on-screen
	const S32 TEXT_PADDING = DEFAULT_TRACKING_ARROW_SIZE + 2;
	S32 half_text_width = llfloor(font->getWidthF32(label) * 0.5f);
	text_x = llclamp(text_x, half_text_width + TEXT_PADDING, getRect().getWidth() - half_text_width - TEXT_PADDING);
	text_y = llclamp(text_y + vert_offset, TEXT_PADDING + vert_offset, getRect().getHeight() - llround(font->getLineHeight()) - TEXT_PADDING - vert_offset);

	//if (label != "")
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a) | Added: RLVa-1.0.0a
	if ( (label != "") && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) )
// [/RLVa:KB]
	{
		font->renderUTF8(
			label, 0,
			text_x, 
			text_y,
			LLColor4::white, LLFontGL::HCENTER,
			LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);

		if (tooltip != "")
		{
			text_y -= (S32)font->getLineHeight();

			font->renderUTF8(
				tooltip, 0,
				text_x, 
				text_y,
				LLColor4::white, LLFontGL::HCENTER,
				LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);
		}
	}
}

// If you change this, then you need to change LLTracker::getTrackedPositionGlobal() as well
LLVector3d LLWorldMapView::viewPosToGlobal( S32 x, S32 y )
{
	x -= llfloor((getRect().getWidth() / 2 + sPanX));
	y -= llfloor((getRect().getHeight() / 2 + sPanY));

	LLVector3 pos_local( (F32)x, (F32)y, 0.f );

	pos_local *= ( REGION_WIDTH_METERS / sMapScale );
	
	LLVector3d pos_global;
	pos_global.setVec( pos_local );
	pos_global += gAgent.getCameraPositionGlobal();
	if(gAgent.isGodlike())
	{
		pos_global.mdV[VZ] = GODLY_TELEPORT_HEIGHT; // Godly height should always be 200.
	}
	else
	{
		pos_global.mdV[VZ] = gAgent.getPositionAgent().mV[VZ]; // Want agent's height, not camera's
	}

	return pos_global;
}


BOOL LLWorldMapView::handleToolTip( S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen )
{
	LLVector3d pos_global = viewPosToGlobal(x, y);

	LLSimInfo* info = LLWorldMap::getInstance()->simInfoFromPosGlobal(pos_global);
	if (info)
	{
		LLViewerRegion *region = gAgent.getRegion();

//		std::string message = llformat("%s (%s)", info->getName().c_str(), info->getAccessString().c_str());
// [RLVa:KB] - Alternate: Snowglobe-1.2.4 | Checked: 2009-07-04 (RLVa-1.0.0a)
		std::string message = llformat("%s (%s)", 
			(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) ? info->mName.c_str() : RlvStrings::getString(RLV_STRING_HIDDEN).c_str(), 
				 LLViewerRegion::accessToString(info->mAccess).c_str());
// [/RLVa:KB]

		if (info->mAccess != SIM_ACCESS_DOWN)
		{
			S32 agent_count = LLWorldMap::getInstance()->mNumAgents[info->mHandle];			
			if (region && region->getHandle() == info->mHandle)
			{
				++agent_count; // Bump by 1 if we're here
			}

			// We may not have an agent count when the map is really
			// zoomed out, so don't display anything about the count. JC
			if (agent_count > 0)
			{
				message += llformat("\n%d ", agent_count);

				if (agent_count == 1)
				{
					message += "avatar";
				}
				else
				{
					message += "avatars";
				}
			}
		}
		msg.assign( message );

		// Optionally show region flags
		std::string region_flags = LLViewerRegion::regionFlagsToString(info->mRegionFlags);

		if (!region_flags.empty())
		{
			msg += '\n';
			msg += region_flags;
		}
					
		S32 SLOP = 4;
		localPointToScreen( 
			x - SLOP, y - SLOP, 
			&(sticky_rect_screen->mLeft), &(sticky_rect_screen->mBottom) );
		sticky_rect_screen->mRight = sticky_rect_screen->mLeft + 2 * SLOP;
		sticky_rect_screen->mTop = sticky_rect_screen->mBottom + 2 * SLOP;
	}
	return TRUE;
}

// Pass relative Z of 0 to draw at same level.
// static
static void drawDot(F32 x_pixels, F32 y_pixels,
			 const LLColor4& color,
			 F32 relative_z,
			 F32 dot_radius,
			 LLUIImagePtr dot_image)
{
	const F32 HEIGHT_THRESHOLD = 7.f;

	if(-HEIGHT_THRESHOLD <= relative_z && relative_z <= HEIGHT_THRESHOLD)
	{
		dot_image->draw(llround(x_pixels) - dot_image->getWidth()/2,
						llround(y_pixels) - dot_image->getHeight()/2, 
						color);
	}
	else
	{
		// Draw V indicator for above or below
		// *TODO: Replace this vector drawing with icons
		
		F32 left =		x_pixels - dot_radius;
		F32 right =		x_pixels + dot_radius;
		F32 center = (left + right) * 0.5f;
		F32 top =		y_pixels + dot_radius;
		F32 bottom =	y_pixels - dot_radius;

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv( color.mV );
		LLUI::setLineWidth(3.0f);
		F32 point = relative_z > HEIGHT_THRESHOLD ? top : bottom; // Y pos of the point of the V
		F32 back = relative_z > HEIGHT_THRESHOLD ? bottom : top; // Y pos of the ends of the V
		gGL.begin( LLRender::LINES );
			gGL.vertex2f(left, back);
			gGL.vertex2f(center, point);
			gGL.vertex2f(center, point);
			gGL.vertex2f(right, back);
		gGL.end();
		LLUI::setLineWidth(1.0f);
	}
}

// Pass relative Z of 0 to draw at same level.
// static
void LLWorldMapView::drawAvatar(F32 x_pixels, 
								F32 y_pixels,
								LLColor4 color,
								F32 relative_z,
								F32 dot_radius)
{
	const F32 HEIGHT_THRESHOLD = 7.f;
	LLUIImagePtr dot_image = sAvatarLevelImage;
	
	if (relative_z == 16000.f) // Unknown altitude (0m or > 1020m)
	{
		//F32 mag = color.length();
		//color=color*0.5f + LLColor4(mag, mag, mag)*0.25f;
		dot_image = sAvatarSmallImage;
	}
	else if (relative_z < -HEIGHT_THRESHOLD) 
	{
		dot_image = sAvatarBelowImage; 
	}
	else if (relative_z > HEIGHT_THRESHOLD) 
	{ 
		dot_image = sAvatarAboveImage;
	}

	S32 dot_width = llround(dot_radius * 2.f);
	dot_image->draw(llround(x_pixels - dot_radius),
		            llround(y_pixels - dot_radius),
		            dot_width,
		            dot_width,
		            color);
}

// Pass relative Z of 0 to draw at same level.
// static
void LLWorldMapView::drawTrackingDot( F32 x_pixels, 
									  F32 y_pixels,
									  const LLColor4& color,
									  F32 relative_z,
									  F32 dot_radius)
{
	drawDot(x_pixels, y_pixels, color, relative_z, dot_radius, sTrackCircleImage);
}


// Pass relative Z of 0 to draw at same level.
// static
void LLWorldMapView::drawIconName(F32 x_pixels, 
								  F32 y_pixels,
								  const LLColor4& color,
								  const std::string& first_line,
								  const std::string& second_line)
{
	const S32 VERT_PAD = 8;
	S32 text_x = llround(x_pixels);
	S32 text_y = llround(y_pixels
						 - BIG_DOT_RADIUS
						 - VERT_PAD);

	// render text
	LLFontGL::getFontSansSerif()->renderUTF8(first_line, 0,
		text_x,
		text_y,
		color,
		LLFontGL::HCENTER,
		LLFontGL::TOP,
		LLFontGL::DROP_SHADOW);

	text_y -= llround(LLFontGL::getFontSansSerif()->getLineHeight());

	// render text
	LLFontGL::getFontSansSerif()->renderUTF8(second_line, 0,
		text_x,
		text_y,
		color,
		LLFontGL::HCENTER,
		LLFontGL::TOP,
		LLFontGL::DROP_SHADOW);
}


//static 
void LLWorldMapView::drawTrackingCircle( const LLRect& rect, S32 x, S32 y, const LLColor4& color, S32 min_thickness, S32 overlap )
{
	F32 start_theta = 0.f;
	F32 end_theta = F_TWO_PI;
	F32 x_delta = 0.f;
	F32 y_delta = 0.f;

	if (x < 0)
	{
		x_delta = 0.f - (F32)x;
		start_theta = F_PI + F_PI_BY_TWO;
		end_theta = F_TWO_PI + F_PI_BY_TWO;
	}
	else if (x > rect.getWidth())
	{
		x_delta = (F32)(x - rect.getWidth());
		start_theta = F_PI_BY_TWO;
		end_theta = F_PI + F_PI_BY_TWO;
	}

	if (y < 0)
	{
		y_delta = 0.f - (F32)y;
		if (x < 0)
		{
			start_theta = 0.f;
			end_theta = F_PI_BY_TWO;
		}
		else if (x > rect.getWidth())
		{
			start_theta = F_PI_BY_TWO;
			end_theta = F_PI;
		}
		else
		{
			start_theta = 0.f;
			end_theta = F_PI;
		}
	}
	else if (y > rect.getHeight())
	{
		y_delta = (F32)(y - rect.getHeight());
		if (x < 0)
		{
			start_theta = F_PI + F_PI_BY_TWO;
			end_theta = F_TWO_PI;
		}
		else if (x > rect.getWidth())
		{
			start_theta = F_PI;
			end_theta = F_PI + F_PI_BY_TWO;
		}
		else
		{
			start_theta = F_PI;
			end_theta = F_TWO_PI;
		}
	}

	F32 distance = sqrtf(x_delta * x_delta + y_delta * y_delta);

	distance = llmax(0.1f, distance);

	F32 outer_radius = distance + (1.f + (9.f * sqrtf(x_delta * y_delta) / distance)) * (F32)overlap;
	F32 inner_radius = outer_radius - (F32)min_thickness;
	
	F32 angle_adjust_x = asin(x_delta / outer_radius);
	F32 angle_adjust_y = asin(y_delta / outer_radius);

	if (angle_adjust_x)
	{
		if (angle_adjust_y)
		{
			F32 angle_adjust = llmin(angle_adjust_x, angle_adjust_y);
			start_theta += angle_adjust;
			end_theta -= angle_adjust;
		}
		else
		{
			start_theta += angle_adjust_x;
			end_theta -= angle_adjust_x;
		}
	}
	else if (angle_adjust_y)
	{
		start_theta += angle_adjust_y;
		end_theta -= angle_adjust_y;
	}

	glMatrixMode(GL_MODELVIEW);
	gGL.pushMatrix();
	gGL.translatef((F32)x, (F32)y, 0.f);
	gl_washer_segment_2d(inner_radius, outer_radius, start_theta, end_theta, 40, color, color);
	gGL.popMatrix();

}

// static
void LLWorldMapView::drawTrackingArrow(const LLRect& rect, S32 x, S32 y, 
									   const LLColor4& color,
									   S32 arrow_size)
{
	F32 x_center = (F32)rect.getWidth() / 2.f;
	F32 y_center = (F32)rect.getHeight() / 2.f;

	F32 x_clamped = (F32)llclamp( x, 0, rect.getWidth() - arrow_size );
	F32 y_clamped = (F32)llclamp( y, 0, rect.getHeight() - arrow_size );

	F32 slope = (F32)(y - y_center) / (F32)(x - x_center);
	F32 window_ratio = (F32)rect.getHeight() / (F32)rect.getWidth();

	if (llabs(slope) > window_ratio && y_clamped != (F32)y)
	{
		// clamp by y 
		x_clamped = (y_clamped - y_center) / slope + x_center;
		// adjust for arrow size
		x_clamped  = llclamp(x_clamped , 0.f, (F32)(rect.getWidth() - arrow_size) );
	}
	else if (x_clamped != (F32)x)
	{
		// clamp by x
		y_clamped = (x_clamped - x_center) * slope + y_center;
		// adjust for arrow size 
		y_clamped = llclamp( y_clamped, 0.f, (F32)(rect.getHeight() - arrow_size) );
	}

	// *FIX: deal with non-square window properly.
	// I do not understand what this comment means -- is it actually
	// broken or is it correctly dealing with non-square
	// windows. Phoenix 2007-01-03.
	S32 half_arrow_size = (S32) (0.5f * arrow_size);

	F32 angle = atan2( y + half_arrow_size - y_center, x + half_arrow_size - x_center);

	sTrackingArrowX = llfloor(x_clamped);
	sTrackingArrowY = llfloor(y_clamped);

	gl_draw_scaled_rotated_image(
		sTrackingArrowX,
		sTrackingArrowY,
		arrow_size, arrow_size, 
		RAD_TO_DEG * angle, 
		sTrackArrowImage->getImage(), 
		color);
}

void LLWorldMapView::setDirectionPos( LLTextBox* text_box, F32 rotation )
{
	// Rotation is in radians.
	// Rotation of 0 means x = 1, y = 0 on the unit circle.


	F32 map_half_height = getRect().getHeight() * 0.5f;
	F32 map_half_width = getRect().getWidth() * 0.5f;
	F32 text_half_height = text_box->getRect().getHeight() * 0.5f;
	F32 text_half_width = text_box->getRect().getWidth() * 0.5f;
	F32 radius = llmin( map_half_height - text_half_height, map_half_width - text_half_width );

	text_box->setOrigin( 
		llround(map_half_width - text_half_width + radius * cos( rotation )),
		llround(map_half_height - text_half_height + radius * sin( rotation )) );
}


void LLWorldMapView::updateDirections()
{
	S32 width = getRect().getWidth();
	S32 height = getRect().getHeight();

	S32 text_height = mTextBoxNorth->getRect().getHeight();
	S32 text_width = mTextBoxNorth->getRect().getWidth();

	const S32 PAD = 2;
	S32 top = height - text_height - PAD;
	S32 left = PAD*2;
	S32 bottom = PAD;
	S32 right = width - text_width - PAD;
	S32 center_x = width/2 - text_width/2;
	S32 center_y = height/2 - text_height/2;

	mTextBoxNorth->setOrigin( center_x, top );
	mTextBoxEast->setOrigin( right, center_y );
	mTextBoxSouth->setOrigin( center_x, bottom );
	mTextBoxWest->setOrigin( left, center_y );

	// These have wider text boxes
	text_width = mTextBoxNorthWest->getRect().getWidth();
	right = width - text_width - PAD;

	mTextBoxNorthWest->setOrigin(left, top);
	mTextBoxNorthEast->setOrigin(right, top);
	mTextBoxSouthWest->setOrigin(left, bottom);
	mTextBoxSouthEast->setOrigin(right, bottom);

// 	S32 hint_width = mTextBoxScrollHint->getRect().getWidth();
// 	mTextBoxScrollHint->setOrigin( width - hint_width - text_width - 2 * PAD, 
// 			PAD * 2 + text_height );
}


void LLWorldMapView::reshape( S32 width, S32 height, BOOL called_from_parent )
{
	LLView::reshape( width, height, called_from_parent );
}

bool LLWorldMapView::checkItemHit(S32 x, S32 y, LLItemInfo& item, LLUUID* id, bool track)
{
	LLVector3 pos_view = globalPosToView(item.mPosGlobal);
	S32 item_x = llround(pos_view.mV[VX]);
	S32 item_y = llround(pos_view.mV[VY]);

	if (x < item_x - BIG_DOT_RADIUS) return false;
	if (x > item_x + BIG_DOT_RADIUS) return false;
	if (y < item_y - BIG_DOT_RADIUS) return false;
	if (y > item_y + BIG_DOT_RADIUS) return false;

	LLSimInfo* sim_info = LLWorldMap::getInstance()->simInfoFromHandle(item.mRegionHandle);
	if (sim_info)
	{
		if (track)
		{
			gFloaterWorldMap->trackLocation(item.mPosGlobal);
		}
	}

	if (track)
	{
		gFloaterWorldMap->trackGenericItem(item);
	}

	item.mSelected = TRUE;
	*id = item.mID;

	return true;
}

// Handle a click, which might be on a dot
void LLWorldMapView::handleClick(S32 x, S32 y, MASK mask,
								 S32* hit_type,
								 LLUUID* id)
{
	LLVector3d pos_global = viewPosToGlobal(x, y);

	// *HACK: Adjust Z values automatically for liaisons & gods so
	// we swoop down when they click on the map. Sadly, the P2P
	// branch does not pay attention to this value; however, the
	// Distributed Messaging branch honors it.
	if(gAgent.isGodlike())
	{
		pos_global.mdV[VZ] = 200.0;
	}

	*hit_type = 0; // hit nothing

	LLWorldMap::getInstance()->mIsTrackingUnknownLocation = FALSE;
	LLWorldMap::getInstance()->mIsTrackingDoubleClick = FALSE;
	LLWorldMap::getInstance()->mIsTrackingCommit = FALSE;

	LLWorldMap::item_info_list_t::iterator it;

	// clear old selected stuff
	for (it = LLWorldMap::getInstance()->mPGEvents.begin(); it != LLWorldMap::getInstance()->mPGEvents.end(); ++it)
		{
		(*it).mSelected = FALSE;
	}
	for (it = LLWorldMap::getInstance()->mMatureEvents.begin(); it != LLWorldMap::getInstance()->mMatureEvents.end(); ++it)
			{
		(*it).mSelected = FALSE;
	}
	for (it = LLWorldMap::getInstance()->mAdultEvents.begin(); it != LLWorldMap::getInstance()->mAdultEvents.end(); ++it)
				{
		(*it).mSelected = FALSE;
				}
	for (it = LLWorldMap::getInstance()->mLandForSale.begin(); it != LLWorldMap::getInstance()->mLandForSale.end(); ++it)
				{
		(*it).mSelected = FALSE;
	}

	// Select event you clicked on
	if (gSavedSettings.getBOOL("MapShowPGEvents"))
					{
		for (it = LLWorldMap::getInstance()->mPGEvents.begin(); it != LLWorldMap::getInstance()->mPGEvents.end(); ++it)
		{
			LLItemInfo& event = *it;

						if (checkItemHit(x, y, event, id, false))
						{
							*hit_type = MAP_ITEM_PG_EVENT;
							mItemPicked = TRUE;
							gFloaterWorldMap->trackEvent(event);
							return;
						}
					}
				}
	if (gSavedSettings.getBOOL("MapShowMatureEvents"))
				{
		for (it = LLWorldMap::getInstance()->mMatureEvents.begin(); it != LLWorldMap::getInstance()->mMatureEvents.end(); ++it)
					{
			LLItemInfo& event = *it;

						if (checkItemHit(x, y, event, id, false))
						{
							*hit_type = MAP_ITEM_MATURE_EVENT;
							mItemPicked = TRUE;
							gFloaterWorldMap->trackEvent(event);
							return;
						}
					}
				}
	if (gSavedSettings.getBOOL("MapShowAdultEvents"))
				{
		for (it = LLWorldMap::getInstance()->mAdultEvents.begin(); it != LLWorldMap::getInstance()->mAdultEvents.end(); ++it)
					{
			LLItemInfo& event = *it;

						if (checkItemHit(x, y, event, id, false))
						{
							*hit_type = MAP_ITEM_ADULT_EVENT;
							mItemPicked = TRUE;
							gFloaterWorldMap->trackEvent(event);
							return;
						}
					}
				}

				if (gSavedSettings.getBOOL("MapShowLandForSale"))
				{
		for (it = LLWorldMap::getInstance()->mLandForSale.begin(); it != LLWorldMap::getInstance()->mLandForSale.end(); ++it)
					{
			LLItemInfo& land = *it;

			if (checkItemHit(x, y, land, id, true))
						{
							*hit_type = MAP_ITEM_LAND_FOR_SALE;
							mItemPicked = TRUE;
							return;
						}
					}
		
		for (it = LLWorldMap::getInstance()->mLandForSaleAdult.begin(); it != LLWorldMap::getInstance()->mLandForSaleAdult.end(); ++it)
						{
			LLItemInfo& land = *it;

			if (checkItemHit(x, y, land, id, true))
							{
								*hit_type = MAP_ITEM_LAND_FOR_SALE_ADULT;
								mItemPicked = TRUE;
								return;
							}
		}
	}
	// If we get here, we haven't clicked on an icon

	gFloaterWorldMap->trackLocation(pos_global);
	mItemPicked = FALSE;

	*id = LLUUID::null;
	return;
}


BOOL outside_slop(S32 x, S32 y, S32 start_x, S32 start_y)
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return (dx <= -2 || 2 <= dx || dy <= -2 || 2 <= dy);
}

BOOL LLWorldMapView::handleMouseDown( S32 x, S32 y, MASK mask )
{
	gFocusMgr.setMouseCapture( this );

	mMouseDownPanX = llround(sPanX);
	mMouseDownPanY = llround(sPanY);
	mMouseDownX = x;
	mMouseDownY = y;
	sHandledLastClick = TRUE;
	return TRUE;
}

BOOL LLWorldMapView::handleMouseUp( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning)
		{
			// restore mouse cursor
			S32 local_x, local_y;
			local_x = mMouseDownX + llfloor(sPanX - mMouseDownPanX);
			local_y = mMouseDownY + llfloor(sPanY - mMouseDownPanY);
			LLRect clip_rect = getRect();
			clip_rect.stretch(-8);
			clip_rect.clipPointToRect(mMouseDownX, mMouseDownY, local_x, local_y);
			LLUI::setCursorPositionLocal(this, local_x, local_y);

			// finish the pan
			mPanning = FALSE;
			
			mMouseDownX = 0;
			mMouseDownY = 0;
		}
		else
		{
			// ignore whether we hit an event or not
			S32 hit_type;
			LLUUID id;
			handleClick(x, y, mask, &hit_type, &id);
		}
		gViewerWindow->showCursor();
		gFocusMgr.setMouseCapture( NULL );
		return TRUE;
	}
	return FALSE;
}

U32 LLWorldMapView::updateBlock(S32 block_x, S32 block_y)
{
	U32 blocks_requested = 0;
	S32 offset = block_x | (block_y * MAP_BLOCK_RES);
	if (!LLWorldMap::getInstance()->mMapBlockLoaded[LLWorldMap::getInstance()->mCurrentMap][offset])
	{
// 		llinfos << "Loading Block (" << block_x << "," << block_y << ")" << llendl;
		LLWorldMap::getInstance()->sendMapBlockRequest(block_x << 3, block_y << 3, (block_x << 3) + 7, (block_y << 3) + 7);
		LLWorldMap::getInstance()->mMapBlockLoaded[LLWorldMap::getInstance()->mCurrentMap][offset] = TRUE;
		blocks_requested++;
	}
	return blocks_requested;
}

U32 LLWorldMapView::updateVisibleBlocks()
{
	if (sMapScale < SIM_MAP_SCALE)
	{
		// We don't care what is loaded if we're zoomed out
		return 0;
	}

	LLVector3d camera_global = gAgent.getCameraPositionGlobal();
	
	F32 pixels_per_region = sMapScale;
	const S32 width = getRect().getWidth();
	const S32 height = getRect().getHeight();
	// Convert pan to sim coordinates
	S32 world_center_x_lo = S32(((-sPanX - width/2) / pixels_per_region) + (camera_global.mdV[0] / REGION_WIDTH_METERS));
	S32 world_center_x_hi = S32(((-sPanX + width/2) / pixels_per_region) + (camera_global.mdV[0] / REGION_WIDTH_METERS));
	S32 world_center_y_lo = S32(((-sPanY - height/2) / pixels_per_region) + (camera_global.mdV[1] / REGION_WIDTH_METERS));
	S32 world_center_y_hi = S32(((-sPanY + height/2)/ pixels_per_region) + (camera_global.mdV[1] / REGION_WIDTH_METERS));

	// Find the corresponding 8x8 block
	S32 world_block_x_lo = world_center_x_lo >> 3;
	S32 world_block_x_hi = world_center_x_hi >> 3;
	S32 world_block_y_lo = world_center_y_lo >> 3;
	S32 world_block_y_hi = world_center_y_hi >> 3;

	U32 blocks_requested = 0;
	const U32 max_blocks_requested = 9;

	for (S32 block_x = llmax(world_block_x_lo, 0); block_x <= llmin(world_block_x_hi, MAP_BLOCK_RES-1); ++block_x)
	{
		for (S32 block_y = llmax(world_block_y_lo, 0); block_y <= llmin(world_block_y_hi, MAP_BLOCK_RES-1); ++block_y)
		{
			blocks_requested += updateBlock(block_x, block_y);
			if (blocks_requested >= max_blocks_requested)
				return blocks_requested;
		}
	}
	return blocks_requested;
}

BOOL LLWorldMapView::handleHover( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning || outside_slop(x, y, mMouseDownX, mMouseDownY))
		{
			// just started panning, so hide cursor
			if (!mPanning)
			{
				mPanning = TRUE;
				gViewerWindow->hideCursor();
			}

			F32 delta_x = (F32)(gViewerWindow->getCurrentMouseDX());
			F32 delta_y = (F32)(gViewerWindow->getCurrentMouseDY());

			// Set pan to value at start of drag + offset
			sPanX += delta_x;
			sPanY += delta_y;
			sTargetPanX = sPanX;
			sTargetPanY = sPanY;

			gViewerWindow->moveCursorToCenter();
		}

		// doesn't matter, cursor should be hidden
		gViewerWindow->setCursor(UI_CURSOR_CROSS );
		return TRUE;
	}
	else
	{
		// While we're waiting for data from the tracker, we're busy. JC
		LLVector3d pos_global = LLTracker::getTrackedPositionGlobal();
		if (LLTracker::isTracking(NULL)
			&& pos_global.isExactlyZero())
		{
			gViewerWindow->setCursor( UI_CURSOR_WAIT );
		}
		else
		{
			gViewerWindow->setCursor( UI_CURSOR_CROSS );
		}
		lldebugst(LLERR_USER_INPUT) << "hover handled by LLWorldMapView" << llendl;		
		return TRUE;
	}
}


BOOL LLWorldMapView::handleDoubleClick( S32 x, S32 y, MASK mask )
{
	if( sHandledLastClick )
	{
		S32 hit_type;
		LLUUID id;
		handleClick(x, y, mask, &hit_type, &id);

		switch (hit_type)
		{
		case MAP_ITEM_PG_EVENT:
		case MAP_ITEM_MATURE_EVENT:
		case MAP_ITEM_ADULT_EVENT:
			{
				gFloaterWorldMap->close();
				// This is an ungainly hack
				std::string uuid_str;
				S32 event_id;
				id.toString(uuid_str);
				uuid_str = uuid_str.substr(28);
				sscanf(uuid_str.c_str(), "%X", &event_id);
				LLFloaterDirectory::showEvents(event_id);
				break;
			}
		case MAP_ITEM_LAND_FOR_SALE:
		case MAP_ITEM_LAND_FOR_SALE_ADULT:
			{
				gFloaterWorldMap->close();
				LLFloaterDirectory::showLandForSale(id);
				break;
			}
		case MAP_ITEM_CLASSIFIED:
			{
				gFloaterWorldMap->close();
				LLFloaterDirectory::showClassified(id);
				break;
			}
		default:
			{
				if (LLWorldMap::getInstance()->mIsTrackingUnknownLocation)
				{
					LLWorldMap::getInstance()->mIsTrackingDoubleClick = TRUE;
				}
				else
				{
					// Teleport if we got a valid location
					LLVector3d pos_global = viewPosToGlobal(x,y);
					LLSimInfo* sim_info = LLWorldMap::getInstance()->simInfoFromPosGlobal(pos_global);
					if (sim_info && sim_info->mAccess != SIM_ACCESS_DOWN)
					{
						gAgent.teleportViaLocation( pos_global );
					}
				}
			}
		};

		return TRUE;
	}
	return FALSE;
}
