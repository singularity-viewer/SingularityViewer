/** 
 * @file llworldmap.h
 * @brief Underlying data storage for the map of the entire world.
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

#ifndef LL_LLWORLDMAP_H
#define LL_LLWORLDMAP_H

#include <map>
#include <string>
#include <vector>

#include "v3math.h"
#include "v3dmath.h"
#include "llframetimer.h"
#include "llmapimagetype.h"
#include "llworldmipmap.h"
#include "lluuid.h"
#include "llmemory.h"
#include "llviewerregion.h"
#include "llviewertexture.h"
#include "lleventinfo.h"
#include "v3color.h"

class LLMessageSystem;


class LLItemInfo
{
public:
	LLItemInfo(F32 global_x, F32 global_y, const std::string& name, LLUUID id, S32 extra = 0, S32 extra2 = 0);

	// Setters
	void setTooltip(const std::string& tooltip) { mToolTip = tooltip; }
	void setElevation(F64 z) { mPosGlobal.mdV[VZ] = z; }
	// Accessors
	const LLVector3d& getGlobalPosition() const { return mPosGlobal; } 
	const std::string& getName() const { return mName; }
	const std::string& getToolTip() const { return mToolTip; }
	const LLUUID& getUUID() const { return mID; }
	U64 getRegionHandle() const { return mRegionHandle; }
	bool isName(const std::string& name) const { return (mName == name); }		// True if name same as item's name
private:
	std::string mName;
	std::string mToolTip;
	LLVector3d	mPosGlobal;
	LLUUID		mID;
	U64			mRegionHandle;
public: //public for now.. non-standard.
	BOOL		mSelected;
	S32			mExtra;
	S32			mExtra2;
};

#define MAP_SIM_IMAGE_TYPES 3
// 0 - Prim
// 1 - Terrain Only
// 2 - Overlay: Land For Sale

class LLSimInfo
{
public:
	LLSimInfo(U64 handle);

	// Convert local region coordinates into world coordinates
	LLVector3d getGlobalPos(const LLVector3& local_pos) const;
	// Get the world coordinates of the SW corner of that region
	LLVector3d getGlobalOrigin() const;
	LLVector3 getLocalPos(LLVector3d global_pos) const;

	void clearImage();					// Clears the reference to the Land for sale image for that region
	void dropImagePriority();			// Drops the boost level of the Land for sale image for that region
	void updateAgentCount(F64 time);	// Send an item request for agent count on that region if time's up
	// Setters
	void setName(std::string& name) { mName = name; }
	void setAccess (U8 accesscode) { mAccess = accesscode; }
	void setRegionFlags (U32 region_flags) { mRegionFlags = region_flags; }
	void setWaterHeight (F32 water_height) { mWaterHeight = water_height; }
	void setAlpha(F32 alpha) { mAlpha = alpha; }
	
	void setMapImageID (const LLUUID& id, const U8 &layer) { mMapImageID[layer] = id; }

	// Accessors
	std::string getName() const { return mName; }
	const std::string getFlagsString() const { return LLViewerRegion::regionFlagsToString(mRegionFlags); }
	const U32 getRegionFlags() const { return mRegionFlags; }
	const std::string getAccessString() const { return LLViewerRegion::accessToString((U8)mAccess); }
	const U8 getAccess() const { return mAccess; }
	
	const F32 getWaterHeight() const { return mWaterHeight; }
	const F32 getAlpha() const { return mAlpha; }
	const U64 getHandle() const { return mHandle; }
	bool isName(const std::string& name) const;
	bool isDown() { return (mAccess == SIM_ACCESS_DOWN); }
	bool isPG() { return (mAccess <= SIM_ACCESS_PG); }
	bool isAdult() { return (mAccess == SIM_ACCESS_ADULT); }
private:
	U64 mHandle;
	std::string mName;

	F64 mAgentsUpdateTime;		// Time stamp giving the last time the agents information was requested for that region
	bool mFirstAgentRequest;	// Init agent request flag
	U8 mAccess;
	U32 mRegionFlags;
	F32 mWaterHeight;

	F32 mAlpha;

public:
	BOOL mShowAgentLocations;	// are agents visible?

	// Image ID for the current overlay mode.
	LLUUID mMapImageID[MAP_SIM_IMAGE_TYPES];

	// Hold a reference to the currently displayed image.
	LLPointer<LLViewerFetchedTexture> mCurrentImage;
	LLPointer<LLViewerFetchedTexture> mOverlayImage;
};

#define MAP_BLOCK_RES 256

struct LLWorldMapLayer
{
	BOOL LayerDefined;
	LLPointer<LLViewerTexture> LayerImage;
	LLUUID LayerImageID;
	LLRect LayerExtents;

	LLWorldMapLayer() : LayerDefined(FALSE) { }
};


class LLWorldMap : public LLSingleton<LLWorldMap>
{
	friend class LLMapLayerResponder;
public:
	typedef void(*url_callback_t)(U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport);

	LLWorldMap();
	~LLWorldMap();

	// clears the list
	void reset();

	// clear the visible items
	void eraseItems();

	// Removes references to cached images
	void clearImageRefs();

	// Clears the flags indicating that we've received sim infos
	// Causes a re-request of the sim info without erasing extisting info
	void clearSimFlags();
	
	// Drops the priority of the images being fetched	
	void dropImagePriorities();

	// Region Map access
	typedef std::map<U64, LLSimInfo*> sim_info_map_t;
	const LLWorldMap::sim_info_map_t& getRegionMap() const { return mSimInfoMap; }
	// Returns simulator information, or NULL if out of range
	LLSimInfo* simInfoFromHandle(const U64 handle);

	// Returns simulator information, or NULL if out of range
	LLSimInfo* simInfoFromPosGlobal(const LLVector3d& pos_global);

	// Returns simulator information for named sim, or NULL if non-existent
	LLSimInfo* simInfoFromName(const std::string& sim_name);

	// Gets simulator name for a global position, returns true if it was found
	bool simNameFromPosGlobal(const LLVector3d& pos_global, std::string& outSimName );

	// Sets the current layer
	void setCurrentLayer(S32 layer, bool request_layer = false);

	void sendMapLayerRequest();
	void sendMapBlockRequest(U16 min_x, U16 min_y, U16 max_x, U16 max_y, bool return_nonexistent = false);
	void sendNamedRegionRequest(std::string region_name);
	void sendNamedRegionRequest(std::string region_name, 
		url_callback_t callback,
		const std::string& callback_url,
		bool teleport);
	void sendHandleRegionRequest(U64 region_handle, 
		url_callback_t callback,
		const std::string& callback_url,
		bool teleport);
	void sendItemRequest(U32 type, U64 handle = 0);

	static void processMapLayerReply(LLMessageSystem*, void**);
	static void processMapBlockReply(LLMessageSystem*, void**);
	static void processMapItemReply(LLMessageSystem*, void**);

	static void gotMapServerURL(bool flag) { sGotMapURL = flag; }
	static bool useWebMapTiles();
	static LLPointer<LLViewerFetchedTexture> loadObjectsTile(U32 grid_x, U32 grid_y);

	void dump();

	// Extend the bounding box of the list of simulators. Returns true
	// if the extents changed.
	BOOL extendAABB(U32 x_min, U32 y_min, U32 x_max, U32 y_max);

	// build coverage maps for telehub region visualization
	void updateTelehubCoverage();
	BOOL coveredByTelehub(LLSimInfo* infop);

	// Bounds of the world, in meters
	U32 getWorldWidth() const;
	U32 getWorldHeight() const;
	
	// World Mipmap delegation: currently used when drawing the mipmap
	void	equalizeBoostLevels();
	LLPointer<LLViewerFetchedTexture> getObjectsTile(U32 grid_x, U32 grid_y, S32 level, bool load = true) { return mWorldMipmap.getObjectsTile(grid_x, grid_y, level, load); }

private:
	bool clearItems(bool force = false);	// Clears the item lists
	
	// Create a region record corresponding to the handle, insert it in the region map and returns a pointer
	LLSimInfo* createSimInfoFromHandle(const U64 handle);

	// Map from region-handle to simulator info
	sim_info_map_t mSimInfoMap;

public:

	BOOL			mIsTrackingUnknownLocation, mInvalidLocation, mIsTrackingDoubleClick, mIsTrackingCommit;
	LLVector3d		mUnknownLocation;

	bool mRequestLandForSale;

	typedef std::vector<LLItemInfo> item_info_list_t;
	item_info_list_t mTelehubs;
	item_info_list_t mInfohubs;
	item_info_list_t mPGEvents;
	item_info_list_t mMatureEvents;
	item_info_list_t mAdultEvents;
	item_info_list_t mLandForSale;
	item_info_list_t mLandForSaleAdult;

	std::map<U64,S32> mNumAgents;

	typedef std::map<U64, item_info_list_t> agent_list_map_t;
	agent_list_map_t mAgentLocationsMap;
	
	std::vector<LLWorldMapLayer>	mMapLayers[MAP_SIM_IMAGE_TYPES];
	BOOL							mMapLoaded[MAP_SIM_IMAGE_TYPES];
	BOOL *							mMapBlockLoaded[MAP_SIM_IMAGE_TYPES];
	S32								mCurrentMap;

	// AABB of the list of simulators
	U32		mMinX;
	U32		mMaxX;
	U32		mMinY;
	U32		mMaxY;

	U8*		mNeighborMap;
	U8*		mTelehubCoverageMap;
	S32		mNeighborMapWidth;
	S32		mNeighborMapHeight;

private:
	
	LLWorldMipmap	mWorldMipmap;	
	
	LLTimer	mRequestTimer;

	// search for named region for url processing
	std::string mSLURLRegionName;
	U64 mSLURLRegionHandle;
	std::string mSLURL;
	url_callback_t mSLURLCallback;
	bool mSLURLTeleport;

	static bool sGotMapURL;
};

#endif
