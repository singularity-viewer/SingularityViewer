/** 
 * @file llmapresponders.cpp
 * @brief Processes responses received for map requests.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2010, Linden Research, Inc.
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

#include "llmapresponders.h"

#include "llfloaterworldmap.h"
#include "llviewertexturelist.h"
#include "llworldmap.h"
#include "llagent.h"

//virtual 
void LLMapLayerResponder::result(const LLSD& result)
{
	llinfos << "LLMapLayerResponder::result from capabilities" << llendl;

	S32 agent_flags = result["AgentData"]["Flags"];
	
	if (agent_flags != LLWorldMap::getInstance()->mCurrentMap)
	{
		llwarns << "Invalid or out of date map image type returned!" << llendl;
		return;
	}

	LLUUID image_id;
	//U32 left, right, top, bottom;

	LLWorldMap::getInstance()->mMapLayers[agent_flags].clear();

	bool use_web_map_tiles = LLWorldMap::useWebMapTiles();

	LLSD::array_const_iterator iter;
	BOOL adjust = FALSE;
	for(iter = result["LayerData"].beginArray(); iter != result["LayerData"].endArray(); ++iter)
	{
		const LLSD& layer_data = *iter;
		
		LLWorldMapLayer new_layer;
		new_layer.LayerDefined = TRUE;
		
		new_layer.LayerExtents.mLeft = layer_data["Left"];
		new_layer.LayerExtents.mRight = layer_data["Right"];
		new_layer.LayerExtents.mBottom = layer_data["Bottom"];
		new_layer.LayerExtents.mTop = layer_data["Top"];

		new_layer.LayerImageID = layer_data["ImageID"];
//		if (use_web_map_tiles)
//		{
//			new_layer.LayerImage = LLWorldMap::loadObjectsTile((U32)new_layer.LayerExtents.mLeft, (U32)new_layer.LayerExtents.mBottom); // no good... Maybe using of level 2 and higher web maps ?
//		}
//		else
//		{
			new_layer.LayerImage = LLViewerTextureManager::getFetchedTexture(new_layer.LayerImageID);
//		}
		gGL.getTexUnit(0)->bind(new_layer.LayerImage.get());
		new_layer.LayerImage->setAddressMode(LLTexUnit::TAM_CLAMP);

		F32 x_meters = F32(new_layer.LayerExtents.mLeft*REGION_WIDTH_UNITS);
		F32 y_meters = F32(new_layer.LayerExtents.mBottom*REGION_WIDTH_UNITS);
		adjust = LLWorldMap::getInstance()->extendAABB(U32(x_meters), U32(y_meters), 
							   U32(x_meters+REGION_WIDTH_UNITS*new_layer.LayerExtents.getWidth()),
							   U32(y_meters+REGION_WIDTH_UNITS*new_layer.LayerExtents.getHeight())) || adjust;

		LLWorldMap::getInstance()->mMapLayers[agent_flags].push_back(new_layer);
	}

	LLWorldMap::getInstance()->mMapLoaded[agent_flags] = TRUE;
	if(adjust) gFloaterWorldMap->adjustZoomSliderBounds();
	
	/*
	U32 agent_flags;
	msg->getU32Fast(_PREHASH_AgentData, _PREHASH_Flags, agent_flags);

	if (agent_flags < 0 || agent_flags >= MAP_SIM_IMAGE_TYPES)
	{
		llwarns << "Invalid map image type returned! " << agent_flags << llendl;
		return;
	}
	*/

	bool found_null_sim = false;

	adjust = FALSE;
	if (result.has("MapBlocks"))
	{
		const LLSD& map_blocks = result["MapBlocks"];
		for (iter = map_blocks.beginArray(); iter != map_blocks.endArray(); ++iter)
		{
			const LLSD& map_block = *iter;
		
			S32 x_regions = map_block["X"];
			S32 y_regions = map_block["Y"];
			S32 x_size = map_block["SizeX"];
			S32 y_size = map_block["SizeY"];
			std::string name = map_block["Name"];
			S32 access = map_block["Access"];
			S32 region_flags = map_block["RegionFlags"];
			S32 water_height = map_block["WaterHeight"];
			LLUUID image_id = map_block["MapImageID"];

			U32 x_meters = x_regions * REGION_WIDTH_UNITS;
			U32 y_meters = y_regions * REGION_WIDTH_UNITS;

			if (access == 255)
			{
				// This region doesn't exist
				if (LLWorldMap::getInstance()->mIsTrackingUnknownLocation &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[0] >= x_meters &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[0] < x_meters + 256 &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[1] >= y_meters &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[1] < y_meters + 256)
				{
					// We were tracking this location, but it doesn't exist
					LLWorldMap::getInstance()->mInvalidLocation = TRUE;
				}

				found_null_sim = true;
			}
			else
			{
				adjust = LLWorldMap::getInstance()->extendAABB(x_meters, 
											   y_meters, 
											   x_meters+REGION_WIDTH_UNITS,
											   y_meters+REGION_WIDTH_UNITS) || adjust;
				U64 handle = to_region_handle(x_meters, y_meters);

				// 			llinfos << "Map sim " << name << " image layer " << agent_flags << " ID " << image_id.getString() << llendl;
			
				LLSimInfo* siminfo = LLWorldMap::getInstance()->simInfoFromHandle(handle);
				if (siminfo == NULL)
				{
					siminfo = LLWorldMap::getInstance()->createSimInfoFromHandle(handle);
				}

				siminfo->setName( name );
				siminfo->setAccess( access );		/*Flawfinder: ignore*/
				siminfo->setRegionFlags( region_flags );
				siminfo->setWaterHeight( (F32) water_height );
				siminfo->setMapImageID( image_id, agent_flags );
				siminfo->setSize((U16)x_size, (U16)y_size);
				if (use_web_map_tiles)
				{
					siminfo->mCurrentImage = LLWorldMap::loadObjectsTile((U32)x_regions, (U32)y_regions);
				}
				else
				{
					siminfo->mCurrentImage = LLViewerTextureManager::getFetchedTexture(siminfo->mMapImageID[LLWorldMap::getInstance()->mCurrentMap]);
				}
				siminfo->mCurrentImage->setAddressMode(LLTexUnit::TAM_CLAMP);
				gGL.getTexUnit(0)->bind(siminfo->mCurrentImage.get());
			
				if (siminfo->mMapImageID[2].notNull())
				{
					siminfo->mOverlayImage = LLViewerTextureManager::getFetchedTexture(siminfo->mMapImageID[2]);
				}
				else
				{
					siminfo->mOverlayImage = NULL;
				}

				if (LLWorldMap::getInstance()->mIsTrackingUnknownLocation &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[0] >= x_meters &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[0] < x_meters + 256 &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[1] >= y_meters &&
					LLWorldMap::getInstance()->mUnknownLocation.mdV[1] < y_meters + 256)
				{
					if (siminfo->isDown())
					{
						// We were tracking this location, but it doesn't exist
						LLWorldMap::getInstance()->mInvalidLocation = true;
					}
					else
					{
						// We were tracking this location, and it does exist
						bool is_tracking_dbl = LLWorldMap::getInstance()->mIsTrackingDoubleClick == TRUE;
						gFloaterWorldMap->trackLocation(LLWorldMap::getInstance()->mUnknownLocation);
						if (is_tracking_dbl)
						{
							LLVector3d pos_global = LLTracker::getTrackedPositionGlobal();
							gAgent.teleportViaLocation( pos_global );
						}
					}
				}
			}
		}
	}
	if(adjust) gFloaterWorldMap->adjustZoomSliderBounds();
	gFloaterWorldMap->updateSims(found_null_sim);
}
