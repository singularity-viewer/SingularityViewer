/**
 * @file llvoavatar.h
 * @brief Declaration of LLVOAvatar class which is a derivation fo
 * LLViewerObject
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

#ifndef LL_LLVOAVATAR_H
#define LL_LLVOAVATAR_H

#include <map>
#include <deque>
#include <string>
#include <vector>

#include "imageids.h"			// IMG_INVISIBLE
#include "llchat.h"
#include "lldrawpoolalpha.h"
#include "llviewerobject.h"
#include "llcharacter.h"
#include "llviewerjointmesh.h"
#include "llviewerjointattachment.h"
#include "llrendertarget.h"
#include "llwearable.h"
#include "llvoavatardefines.h"
#include "emeraldboobutils.h"
#include "llavatarname.h"


extern const LLUUID ANIM_AGENT_BODY_NOISE;
extern const LLUUID ANIM_AGENT_BREATHE_ROT;
extern const LLUUID ANIM_AGENT_PHYSICS_MOTION;
extern const LLUUID ANIM_AGENT_EDITING;
extern const LLUUID ANIM_AGENT_EYE;
extern const LLUUID ANIM_AGENT_FLY_ADJUST;
extern const LLUUID ANIM_AGENT_HAND_MOTION;
extern const LLUUID ANIM_AGENT_HEAD_ROT;
extern const LLUUID ANIM_AGENT_PELVIS_FIX;
extern const LLUUID ANIM_AGENT_TARGET;
extern const LLUUID ANIM_AGENT_WALK_ADJUST;

class LLTexLayerSet;
class LLVoiceVisualizer;
class LLHUDText;
class LLHUDEffectSpiral;
class LLTexGlobalColor;
class LLTexGlobalColorInfo;
class LLTexLayerSetInfo;
class LLDriverParamInfo;
class LLVOAvatarBoneInfo;
class LLVOAvatarSkeletonInfo;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLVOAvatar
// 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLVOAvatar :
	public LLViewerObject,
	public LLCharacter
{
protected:
	struct LLVOAvatarXmlInfo;

/********************************************************************************
 **                                                                            **
 **                    INITIALIZATION
 **/

public:
	LLVOAvatar(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp);
	/*virtual*/ void markDead();
	static void			initClass(); // Initialize data that's only init'd once per class.
	static void			cleanupClass();	// Cleanup data that's only init'd once per class.
protected:
	virtual				~LLVOAvatar();
	BOOL				loadSkeletonNode();
	BOOL				loadMeshNodes();

/**                    Initialization
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    INHERITED
 **/

	//--------------------------------------------------------------------
	// LLViewerObject interface and related
	//--------------------------------------------------------------------
public:
	virtual void			updateGL();
	virtual U32    	 	 	processUpdateMessage(LLMessageSystem *mesgsys,
													 void **user_data,
													 U32 block_num,
													 const EObjectUpdateType update_type,
													 LLDataPacker *dp);
	/*virtual*/ BOOL idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time);
	virtual BOOL   	 	 	updateLOD();
	BOOL  	 	 	 	 	updateJointLODs();
	/*virtual*/ BOOL 		isActive() const; // Whether this object needs to do an idleUpdate.
	/*virtual*/ void 		updateTextures();
	/*virtual*/ S32 		setTETexture(const U8 te, const LLUUID& uuid); // If setting a baked texture, need to request it from a non-local sim.
	/*virtual*/ void 		onShift(const LLVector3& shift_vector);
	virtual U32    	 	 	getPartitionType() const;
	virtual const  	 	 	LLVector3 getRenderPosition() const;
	virtual void   	 	 	updateDrawable(BOOL force_damped);
	/*virtual*/ LLDrawable* createDrawable(LLPipeline *pipeline);
	/*virtual*/ BOOL 		updateGeometry(LLDrawable *drawable);
	/*virtual*/ void 		setPixelAreaAndAngle(LLAgent &agent);
	virtual void   	 	 	updateRegion(LLViewerRegion *regionp);
	void 					updateSpatialExtents(LLVector3& newMin, LLVector3 &newMax);
	void 					getSpatialExtents(LLVector3& newMin, LLVector3& newMax);	
	/*virtual*/ BOOL lineSegmentIntersect(const LLVector3& start, const LLVector3& end,
												 S32 face = -1,                    // which face to check, -1 = ALL_SIDES
												 BOOL pick_transparent = FALSE,
												 S32* face_hit = NULL,             // which face was hit
												 LLVector3* intersection = NULL,   // return the intersection point
												 LLVector2* tex_coord = NULL,      // return the texture coordinates of the intersection point
												 LLVector3* normal = NULL,         // return the surface normal at the intersection point
												 LLVector3* bi_normal = NULL);     // return the surface bi-normal at the intersection point

	//--------------------------------------------------------------------
	// LLCharacter interface and related
	//--------------------------------------------------------------------
public:
	virtual LLVector3    	getCharacterPosition();
	virtual LLQuaternion 	getCharacterRotation();
	virtual LLVector3    	getCharacterVelocity();
	virtual LLVector3    	getCharacterAngularVelocity();
	virtual LLJoint*		getCharacterJoint(U32 num);
	virtual BOOL			allocateCharacterJoints(U32 num);

	LLUUID 					remapMotionID(const LLUUID& id);
	virtual BOOL			startMotion(const LLUUID& id, F32 time_offset = 0.f);
	virtual BOOL			stopMotion(const LLUUID& id, BOOL stop_immediate = FALSE);
	virtual void			stopMotionFromSource(const LLUUID& source_id);
	virtual void			requestStopMotion(LLMotion* motion);
	LLMotion*				findMotion(const LLUUID& id) const;
	void					startDefaultMotions();

	virtual LLJoint*		getJoint(const std::string &name);
	virtual LLJoint*     	getRootJoint() { return &mRoot; }

	virtual const char*		getAnimationPrefix() { return "avatar"; }
	virtual const LLUUID&   getID();
	virtual LLVector3		getVolumePos(S32 joint_index, LLVector3& volume_offset);
	virtual LLJoint*		findCollisionVolume(U32 volume_id);
	virtual S32				getCollisionVolumeID(std::string &name);
	virtual void			addDebugText(const std::string& text);
	virtual F32          	getTimeDilation();
	virtual void			getGround(const LLVector3 &inPos, LLVector3 &outPos, LLVector3 &outNorm);
	virtual F32				getPixelArea() const;
	virtual LLPolyMesh*		getHeadMesh();
	virtual LLPolyMesh*		getUpperBodyMesh();
	virtual LLVector3d		getPosGlobalFromAgent(const LLVector3 &position);
	virtual LLVector3		getPosAgentFromGlobal(const LLVector3d &position);
	virtual void			updateVisualParams();


/**                    Inherited
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    STATE
 **/

public:
 	bool			isSelf() const { return mIsSelf; } // True if this avatar is for this viewer's agent
	bool			isBuilt() const { return mIsBuilt; }
private:
	BOOL			mSupportsAlphaLayers; // For backwards compatibility, TRUE for 1.23+ clients

	//--------------------------------------------------------------------
	// Updates
	//--------------------------------------------------------------------
public:
	BOOL updateCharacter(LLAgent &agent);
	void 			idleUpdateVoiceVisualizer(bool voice_enabled);
	void 			idleUpdateMisc(bool detailed_update);
	void idleUpdateAppearanceAnimation();
	void 			idleUpdateLipSync(bool voice_enabled);
	void 			idleUpdateLoadingEffect();
	void 			idleUpdateWindEffect();
	void 			idleUpdateNameTag(const LLVector3& root_pos_last);
	void			clearNameTag();
	static void		invalidateNameTag(const LLUUID& agent_id);
	// force all name tags to rebuild, useful when display names turned on/off
	static void		invalidateNameTags();
	void 			idleUpdateRenderCost();
	void 			idleUpdateBelowWater();
	void 			idleUpdateTractorBeam();	//1.23
	void 			idleUpdateBoobEffect();	//Emerald
	
	void updateAttachmentVisibility(U32 camera_mode);	//Agent only

	LLFrameTimer 	mIdleTimer;
	std::string		getIdleTime();
	
	//--------------------------------------------------------------------
	// Static preferences (controlled by user settings/menus)
	//--------------------------------------------------------------------
public:
	static S32		sRenderName;
	static bool		sRenderGroupTitles;
	static U32		sMaxVisible; //(affected by control "RenderAvatarMaxVisible")
	static F32		sRenderDistance; //distance at which avatars will render.
	static BOOL		sShowAnimationDebug; // show animation debug info
	static BOOL		sUseImpostors; //use impostors for far away avatars
	static BOOL		sShowFootPlane;	// show foot collision plane reported by server
	static BOOL		sVisibleInFirstPerson;
	static S32		sNumLODChangesThisFrame;
	static S32		sNumVisibleChatBubbles;
	static BOOL		sDebugInvisible;
	static BOOL		sShowAttachmentPoints;
	static F32		sLODFactor; // user-settable LOD factor
	static F32		sPhysicsLODFactor; // user-settable physics LOD factor
	static BOOL		sJointDebug; // output total number of joints being touched for each avatar
	static BOOL		sDebugAvatarRotation;

	//--------------------------------------------------------------------
	// Region state
	//--------------------------------------------------------------------
public:
	LLHost			getObjectHost() const;

	//--------------------------------------------------------------------
	// Loading state
	//--------------------------------------------------------------------
public:
	BOOL			isFullyLoaded() const;
	//BOOL			isReallyFullyLoaded();
	BOOL			updateIsFullyLoaded();
protected:
	bool 			sendAvatarTexturesRequest();
	void			updateRuthTimer(bool loading);
private:
	BOOL			mFullyLoaded;
	BOOL			mPreviousFullyLoaded;
	BOOL			mFullyLoadedInitialized;
	S32				mFullyLoadedFrameCounter;
	LLFrameTimer	mFullyLoadedTimer;
	LLFrameTimer	mRuthTimer;
	
/**                    State
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    SKELETON
 **/

public:
	void				updateHeadOffset();
	F32					getPelvisToFoot() const { return mPelvisToFoot; }

	LLVector3			mHeadOffset; // current head position
	LLViewerJoint		mRoot;
protected:
	static BOOL			parseSkeletonFile(const std::string& filename);
	void				buildCharacter();
	BOOL 				loadAvatar();

	BOOL				setupBone(const LLVOAvatarBoneInfo* info, LLViewerJoint* parent, S32 &current_volume_num, S32 &current_joint_num);
	BOOL				buildSkeleton(const LLVOAvatarSkeletonInfo *info);
private:
	BOOL				mIsBuilt; // state of deferred character building
	S32					mNumJoints;
	LLViewerJoint*		mSkeleton;
	
	//--------------------------------------------------------------------
	// Pelvis height adjustment members.
	//--------------------------------------------------------------------
public:
	LLVector3			mBodySize;
	S32					mLastSkeletonSerialNum;
private:
	F32					mPelvisToFoot;

	//--------------------------------------------------------------------
	// Cached pointers to well known joints
	//--------------------------------------------------------------------
public:
	LLViewerJoint* 		mPelvisp;
	LLViewerJoint* 		mTorsop;
	LLViewerJoint* 		mChestp;
	LLViewerJoint* 		mNeckp;
	LLViewerJoint* 		mHeadp;
	LLViewerJoint* 		mSkullp;
	LLViewerJoint* 		mEyeLeftp;
	LLViewerJoint* 		mEyeRightp;
	LLViewerJoint* 		mHipLeftp;
	LLViewerJoint* 		mHipRightp;
	LLViewerJoint* 		mKneeLeftp;
	LLViewerJoint* 		mKneeRightp;
	LLViewerJoint* 		mAnkleLeftp;
	LLViewerJoint* 		mAnkleRightp;
	LLViewerJoint* 		mFootLeftp;
	LLViewerJoint* 		mFootRightp;
	LLViewerJoint* 		mWristLeftp;
	LLViewerJoint* 		mWristRightp;

	//--------------------------------------------------------------------
	// XML parse tree
	//--------------------------------------------------------------------
private:
	static LLXmlTree 	sXMLTree; // avatar config file
	static LLXmlTree 	sSkeletonXMLTree; // avatar skeleton file

/**                    Skeleton
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    RENDERING
 **/

public:
	// Graphical stuff for objects - maybe broken out into render class later?
	U32 renderFootShadows();
	U32 		renderImpostor(LLColor4U color = LLColor4U(255,255,255,255), S32 diffuse_channel = 0);
	U32 		renderRigid();
	U32 		renderSkinned(EAvatarRenderPass pass);
	U32 		renderTransparent(BOOL first_pass);
	void 		renderCollisionVolumes();
	static void	deleteCachedImages(bool clearAll=true);
	static void	destroyGL();
	static void	restoreGL();
	BOOL 		mIsDummy; // for special views
	S32			mSpecialRenderMode; // special lighting
private:
	bool		shouldAlphaMask();

	BOOL 		mNeedsSkin; // avatar has been animated and verts have not been updated
	S32	 		mUpdatePeriod;
	S32  		mNumInitFaces; //number of faces generated when creating the avatar drawable, does not inculde splitted faces due to long vertex buffer.

	//--------------------------------------------------------------------
	// Visibility
	//--------------------------------------------------------------------
protected:
	void 		updateVisibility();
private:
	U32	 		mVisibilityRank;
	BOOL 		mVisible;
	
	//--------------------------------------------------------------------
	// Shadowing
	//--------------------------------------------------------------------
public:
	void 		updateShadowFaces();
	LLDrawable*	mShadow;
private:
	LLFace* 	mShadow0Facep;
	LLFace* 	mShadow1Facep;
	LLPointer<LLViewerTexture> mShadowImagep;

	//--------------------------------------------------------------------
	// Impostors
	//--------------------------------------------------------------------
public:
	BOOL 		isImpostor() const;
	BOOL 	    needsImpostorUpdate() const;
	const LLVector3& getImpostorOffset() const;
	const LLVector2& getImpostorDim() const;
	void 		getImpostorValues(LLVector3* extents, LLVector3& angle, F32& distance) const;
	void 		cacheImpostorValues();
	void 		setImpostorDim(const LLVector2& dim);
	static void	resetImpostors();
	static void updateImpostors();
	LLRenderTarget mImpostor;
	BOOL		mNeedsImpostorUpdate;
private:
	LLVector3	mImpostorOffset;
	LLVector2	mImpostorDim;
	BOOL		mNeedsAnimUpdate;
	LLVector3	mImpostorExtents[2];
	LLVector3	mImpostorAngle;
	F32			mImpostorDistance;
	F32			mImpostorPixelArea;
	LLVector3	mLastAnimExtents[2];  

	//--------------------------------------------------------------------
	// Wind rippling in clothes
	//--------------------------------------------------------------------
public:
	LLVector4	mWindVec;
	F32			mRipplePhase;
	BOOL		mBelowWater;
private:
	F32			mWindFreq;
	LLFrameTimer mRippleTimer;
	F32			mRippleTimeLast;
	LLVector3	mRippleAccel;
	LLVector3	mLastVel;

	//--------------------------------------------------------------------
	// Culling
	//--------------------------------------------------------------------
public:
	static void	cullAvatarsByPixelArea();
	BOOL		isCulled() const { return mCulled; }
private:
	BOOL		mCulled;

	//--------------------------------------------------------------------
	// Freeze counter
	//--------------------------------------------------------------------
public:
	static void updateFreezeCounter(S32 counter = 0);
private:
	static S32  sFreezeCounter;

	//--------------------------------------------------------------------
	// Internal functions
	//--------------------------------------------------------------------
protected:
	BOOL needsRenderBeam();
/**                    Rendering
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    TEXTURES
 **/

	//--------------------------------------------------------------------
	// Loading status
	//--------------------------------------------------------------------
public:
 	BOOL            isTextureDefined(U8 te) const;
	BOOL			isTextureVisible(U8 te) const;

protected:
	BOOL			isFullyBaked();
	static BOOL		areAllNearbyInstancesBaked(S32& grey_avatars);

	//--------------------------------------------------------------------
	// Baked textures
	//--------------------------------------------------------------------
public:
protected:
	static void		onBakedTextureMasksLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
	static void		onInitialBakedTextureLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
	static void		onBakedTextureLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
	void			removeMissingBakedTextures();
	void			useBakedTexture(const LLUUID& id);

	struct BakedTextureData
	{
		LLUUID								mLastTextureIndex;
		LLTexLayerSet* 						mTexLayerSet; // Only exists for self
		bool								mIsLoaded;
		bool								mIsUsed;
		LLVOAvatarDefines::ETextureIndex 	mTextureIndex;
		U32									mMaskTexName;
		// Stores pointers to the joint meshes that this baked texture deals with
		std::vector< LLViewerJointMesh * > 	mMeshes;  // std::vector<LLViewerJointMesh> mJoints[i]->mMeshParts
	};
	typedef std::vector<BakedTextureData> 	bakedtexturedata_vec_t;
	bakedtexturedata_vec_t 					mBakedTextureDatas;
	//--------------------------------------------------------------------
	// Local Textures
	//--------------------------------------------------------------------
protected:
	void			setLocalTexture(LLVOAvatarDefines::ETextureIndex i, LLViewerFetchedTexture* tex, BOOL baked_version_exits);
	void			addLocalTextureStats(LLVOAvatarDefines::ETextureIndex i, LLViewerTexture* imagep, F32 texel_area_ratio, BOOL rendered, BOOL covered_by_baked);
	//--------------------------------------------------------------------
	// Layers
	//--------------------------------------------------------------------
protected:
	void			deleteLayerSetCaches(bool clearAll = true);
	void			addBakedTextureStats(LLViewerFetchedTexture* imagep, F32 pixel_area, F32 texel_area_ratio, S32 boost_level);

	//--------------------------------------------------------------------
	// Composites
	//--------------------------------------------------------------------
public:
	void			invalidateComposite( LLTexLayerSet* layerset, BOOL set_by_user );
	void			invalidateAll();

	//--------------------------------------------------------------------
	// Static texture/mesh/baked dictionary
	//--------------------------------------------------------------------
public:
	static BOOL 	isIndexLocalTexture(LLVOAvatarDefines::ETextureIndex i);
	static BOOL 	isIndexBakedTexture(LLVOAvatarDefines::ETextureIndex i);
private:
	static const LLVOAvatarDefines::LLVOAvatarDictionary *getDictionary() { return sAvatarDictionary; }
	static LLVOAvatarDefines::LLVOAvatarDictionary* sAvatarDictionary;
	static LLVOAvatarSkeletonInfo* 					sAvatarSkeletonInfo;
	static LLVOAvatarXmlInfo* 						sAvatarXmlInfo;

	//--------------------------------------------------------------------
	// Messaging
	//--------------------------------------------------------------------
public:
	void 			onFirstTEMessageReceived();
private:
	BOOL			mFirstTEMessageReceived;
	BOOL			mFirstAppearanceMessageReceived;


//Most this stuff is Agent only

	//--------------------------------------------------------------------
	// Textures and Layers
	//--------------------------------------------------------------------
protected:
	void			requestLayerSetUpdate(LLVOAvatarDefines::ETextureIndex i);


	LLTexLayerSet*	getLayerSet(LLVOAvatarDefines::ETextureIndex index) const;
	S32				getLocalDiscardLevel(LLVOAvatarDefines::ETextureIndex index);

	//--------------------------------------------------------------------
	// Other public functions
	//--------------------------------------------------------------------
public:
	static void		dumpTotalLocalTextureByteCount();
protected:
	void			getLocalTextureByteCount( S32* gl_byte_count );

public:
	void			dumpLocalTextures();
	const LLUUID&	grabLocalTexture(LLVOAvatarDefines::ETextureIndex index);
	BOOL			canGrabLocalTexture(LLVOAvatarDefines::ETextureIndex index);

	void			setCompositeUpdatesEnabled(BOOL b);

	void setNameFromChat(const std::string &text) { mNameFromChatOverride = mNameFromChatChanged = true; mNameFromChatText = text; }
	void clearNameFromChat() { mNameFromChatOverride = false; mNameFromChatChanged = true; mNameFromChatText = ""; }

public:
	

	//--------------------------------------------------------------------
	// texture compositing (used only by the LLTexLayer series of classes)
	//--------------------------------------------------------------------
public:
	BOOL			isLocalTextureDataAvailable( const LLTexLayerSet* layerset );
	BOOL			isLocalTextureDataFinal( const LLTexLayerSet* layerset );
	LLVOAvatarDefines::ETextureIndex	getBakedTE( LLTexLayerSet* layerset );
	void			updateComposites();
	//BOOL			getLocalTextureRaw( LLVOAvatarDefines::ETextureIndex index, LLImageRaw* image_raw_pp );
	BOOL			getLocalTextureGL( LLVOAvatarDefines::ETextureIndex index, LLViewerTexture** image_gl_pp );
	const LLUUID&	getLocalTextureID( LLVOAvatarDefines::ETextureIndex index );
	LLGLuint		getScratchTexName( LLGLenum format, U32* texture_bytes );
	BOOL			bindScratchTexture( LLGLenum format );
	void			forceBakeAllTextures(bool slam_for_debug = false);
	static void		processRebakeAvatarTextures(LLMessageSystem* msg, void**);
	void			setNewBakedTexture( LLVOAvatarDefines::ETextureIndex i, const LLUUID& uuid );
	void			setCachedBakedTexture( LLVOAvatarDefines::ETextureIndex i, const LLUUID& uuid );
	void			releaseUnnecessaryTextures();
	void			requestLayerSetUploads();
	bool			hasPendingBakedUploads();
	static void		onLocalTextureLoaded( BOOL succcess, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata );
	static enum EWearableType	getTEWearableType(LLVOAvatarDefines::ETextureIndex te );
	static LLUUID			getDefaultTEImageID(LLVOAvatarDefines::ETextureIndex te );
	static void		onChangeSelfInvisible(BOOL newvalue);
	void			setInvisible(BOOL newvalue);

	void			wearableUpdated(EWearableType type, BOOL upload_result = TRUE);

	//--------------------------------------------------------------------
	// texture compositing
	//--------------------------------------------------------------------
public:
	void			setLocTexTE( U8 te, LLViewerTexture* image, BOOL set_by_user );
	void			setupComposites();

/**                    Textures
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    MESHES
 **/

public:
	void 			updateMeshTextures();
	void 			updateSexDependentLayerSets(BOOL set_by_user);
	void 			dirtyMesh(); // Dirty the avatar mesh
	void 			updateMeshData();
protected:
	void 			releaseMeshData();
	void 			restoreMeshData();
private:
	void 			dirtyMesh(S32 priority); // Dirty the avatar mesh, with priority
	S32 			mDirtyMesh; // 0 -- not dirty, 1 -- morphed, 2 -- LOD
	BOOL			mMeshTexturesDirty;

	typedef std::multimap<std::string, LLPolyMesh*> polymesh_map_t;
	polymesh_map_t 									mMeshes;
	std::vector<LLViewerJoint *> 					mMeshLOD;

	//--------------------------------------------------------------------
	// Destroy invisible mesh
	//--------------------------------------------------------------------
protected:
	BOOL			mMeshValid;
	LLFrameTimer	mMeshInvisibleTime;

/**                    Meshes
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    APPEARANCE
 **/

public:
	void 			processAvatarAppearance(LLMessageSystem* mesgsys);
	void 			hideSkirt();
	void			startAppearanceAnimation(BOOL set_by_user, BOOL play_sound);
	LLPolyMesh*		getMesh(LLPolyMeshSharedData* shared_data);
	
	//--------------------------------------------------------------------
	// Appearance morphing
	//--------------------------------------------------------------------
public:
	BOOL			getIsAppearanceAnimating() const { return mAppearanceAnimating; }
private:
	BOOL			mAppearanceAnimating;
	LLFrameTimer	mAppearanceMorphTimer;
	F32				mLastAppearanceBlendTime;
	BOOL			mAppearanceAnimSetByUser;	//1.23

public:
	typedef std::map<S32, std::string> lod_mesh_map_t;
	typedef std::map<std::string, lod_mesh_map_t> mesh_info_t;

	static void getMeshInfo(mesh_info_t* mesh_info);

	//--------------------------------------------------------------------
	// Clothing colors (convenience functions to access visual parameters)
	//--------------------------------------------------------------------
public:
	void			setClothesColor( LLVOAvatarDefines::ETextureIndex te, const LLColor4& new_color, BOOL set_by_user );
	LLColor4		getClothesColor(LLVOAvatarDefines::ETextureIndex te);
	static BOOL		teToColorParams( LLVOAvatarDefines::ETextureIndex te, const char* param_name[3] );

	//--------------------------------------------------------------------
	// Global colors
	//--------------------------------------------------------------------
public:
	LLColor4		getGlobalColor(const std::string& color_name ) const;
	void			onGlobalColorChanged(const LLTexGlobalColor* global_color, BOOL set_by_user );
private:
	LLTexGlobalColor* mTexSkinColor;
	LLTexGlobalColor* mTexHairColor;
	LLTexGlobalColor* mTexEyeColor;

	//--------------------------------------------------------------------
	// Visibility
	//--------------------------------------------------------------------
public:
	BOOL			isVisible() const;
	void			setVisibilityRank(U32 rank);
	U32				getVisibilityRank()  const { return mVisibilityRank; } // unused
	static S32 		sNumVisibleAvatars; // Number of instances of this class
	static LLColor4 getDummyColor();
	
	//--------------------------------------------------------------------
	// Customize
	//--------------------------------------------------------------------
public:
	static void		onCustomizeStart();
	static void		onCustomizeEnd();
/**                    Appearance
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    WEARABLES
 **/

public:
	BOOL			isWearingWearableType( EWearableType type ) const;
	
	//--------------------------------------------------------------------
	// Attachments
	//--------------------------------------------------------------------
public:
	void 				clampAttachmentPositions();
	BOOL attachObject(LLViewerObject *viewer_object);
	BOOL detachObject(LLViewerObject *viewer_object);
	static LLVOAvatar* findAvatarFromAttachment( LLViewerObject* obj );
protected:
// [RLVa:KB] - Checked: 2009-12-18 (RLVa-1.1.0i) | Added: RLVa-1.1.0i
	LLViewerJointAttachment* getTargetAttachmentPoint(const LLViewerObject* viewer_object) const;
// [/RLVa:KB]
	void 				lazyAttach();

	//--------------------------------------------------------------------
	// Map of attachment points, by ID
	//--------------------------------------------------------------------
public:
	S32 				getAttachmentCount(); // Warning: order(N) not order(1) // currently used only by -self
	typedef std::map<S32, LLViewerJointAttachment*> attachment_map_t;
	attachment_map_t 								mAttachmentPoints;
	std::vector<LLPointer<LLViewerObject> > 		mPendingAttachment;	
	//--------------------------------------------------------------------
	// HUD functions
	//--------------------------------------------------------------------
public:
	BOOL 				hasHUDAttachment() const;
	LLBBox 				getHUDBBox() const;
	void 				rebuildHUD();
	void 				resetHUDAttachments();
	BOOL				canAttachMoreObjects() const;
protected:
	U32					getNumAttachments() const; // O(N), not O(1)

	//--------------------------------------------------------------------
	// Old/nonstandard/Agent-only functions
	//--------------------------------------------------------------------
public:
	static BOOL		detachAttachmentIntoInventory(const LLUUID& item_id);
	BOOL 			isWearingAttachment( const LLUUID& inv_item_id );
	// <edit> testzone attachpt
	BOOL 			isWearingUnsupportedAttachment( const LLUUID& inv_item_id );
	// </edit>
	LLViewerObject* getWornAttachment( const LLUUID& inv_item_id );
// [RLVa:KB] - Checked: 2010-03-14 (RLVa-1.2.0a) | Added: RLVa-1.1.0i
	LLViewerJointAttachment* getWornAttachmentPoint(const LLUUID& inv_item_id) const;
// [/RLVa:KB]
	const std::string getAttachedPointName(const LLUUID& inv_item_id);

	// <edit>
	std::map<S32, LLUUID> mUnsupportedAttachmentPoints;
	// </edit>
	
/**                    Wearables
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    ACTIONS
 **/

	//--------------------------------------------------------------------
	// Animations
	//--------------------------------------------------------------------
public:
	BOOL 			isAnyAnimationSignaled(const LLUUID *anim_array, const S32 num_anims) const;
	void 			processAnimationStateChanges();
protected:
	BOOL 			processSingleAnimationStateChange(const LLUUID &anim_id, BOOL start);
	void 			resetAnimations();
private:
	LLTimer			mAnimTimer;
	F32				mTimeLast;	

	//--------------------------------------------------------------------
	// Animation state data
	//--------------------------------------------------------------------
public:
	typedef std::map<LLUUID, S32>::iterator AnimIterator;
	std::map<LLUUID, S32> 					mSignaledAnimations; // requested state of Animation name/value
	std::map<LLUUID, S32> 					mPlayingAnimations; // current state of Animation name/value

	typedef std::multimap<LLUUID, LLUUID> 	AnimationSourceMap;
	typedef AnimationSourceMap::iterator 	AnimSourceIterator;
	AnimationSourceMap 						mAnimationSources; // object ids that triggered anim ids

	//--------------------------------------------------------------------
	// Chat
	//--------------------------------------------------------------------
public:
	void			addChat(const LLChat& chat);
	void	   		clearChat();
	void			startTyping() { mTyping = TRUE; mTypingTimer.reset(); mIdleTimer.reset();}
	void			stopTyping() { mTyping = FALSE; }
private:
	BOOL			mVisibleChat;

	//--------------------------------------------------------------------
	// Lip synch morphs
	//--------------------------------------------------------------------
private:
	bool 		   	mLipSyncActive; // we're morphing for lip sync
	LLVisualParam* 	mOohMorph; // cached pointers morphs for lip sync
	LLVisualParam* 	mAahMorph; // cached pointers morphs for lip sync

	//--------------------------------------------------------------------
	// Flight
	//--------------------------------------------------------------------
public:
	BOOL			mInAir;
	LLFrameTimer	mTimeInAir;

/**                    Actions
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    PHYSICS
 **/

private:
	F32 		mSpeedAccum; // measures speed (for diagnostics mostly).
	BOOL 		mTurning; // controls hysteresis on avatar rotation
	F32			mSpeed; // misc. animation repeated state

	//--------------------------------------------------------------------
	// Collision volumes
	//--------------------------------------------------------------------
public:
  	S32			mNumCollisionVolumes;
	LLViewerJointCollisionVolume* mCollisionVolumes;
protected:
	BOOL		allocateCollisionVolumes(U32 num);

	//--------------------------------------------------------------------
	// Dimensions
	//--------------------------------------------------------------------
public:
	void 		resolveHeightGlobal(const LLVector3d &inPos, LLVector3d &outPos, LLVector3 &outNorm);
	void 		resolveHeightAgent(const LLVector3 &inPos, LLVector3 &outPos, LLVector3 &outNorm);
	void 		resolveRayCollisionAgent(const LLVector3d start_pt, const LLVector3d end_pt, LLVector3d &out_pos, LLVector3 &out_norm);
	void 		slamPosition(); // Slam position to transmitted position (for teleport);
protected:
	void 		computeBodySize();

	//--------------------------------------------------------------------
	// Material being stepped on
	//--------------------------------------------------------------------
private:
	BOOL		mStepOnLand;
	U8			mStepMaterial;
	LLVector3	mStepObjectVelocity;

	//--------------------------------------------------------------------
	// Emerald legacy boob bounce
	//--------------------------------------------------------------------
public:
	F32				getActualBoobGrav() const { return mLocalBoobConfig.actualBoobGrav; }
	void			setActualBoobGrav(F32 grav)
	{
		mLocalBoobConfig.actualBoobGrav = grav;
		if(!mFirstSetActualBoobGravRan)
		{
			mBoobState.boobGrav = grav;
			mFirstSetActualBoobGravRan = true;
		}
	}
	static EmeraldGlobalBoobConfig sBoobConfig;
private:
	bool			mFirstSetActualBoobGravRan;
	LLFrameTimer	mBoobBounceTimer;
	EmeraldAvatarLocalBoobConfig mLocalBoobConfig;
	EmeraldBoobState mBoobState;
	
public:
	bool mSupportsPhysics; //Client supports v2 wearable physics. Disable emerald physics.
	
/**                    Physics
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    HIERARCHY
 **/

public:
	virtual BOOL 	setParent(LLViewerObject* parent);
	virtual void 	addChild(LLViewerObject *childp);
	virtual void 	removeChild(LLViewerObject *childp);

	//--------------------------------------------------------------------
	// Sitting
	//--------------------------------------------------------------------
public:
	BOOL			isSitting() const {return mIsSitting;}
	void 			sitOnObject(LLViewerObject *sit_object);
	void 			getOffObject();
private:
	// set this property only with LLVOAvatar::sitDown method
	BOOL 			mIsSitting;

/**                    Hierarchy
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    NAME
 **/

public:
	std::string		getFullname() const; // Returns "FirstName LastName"
protected:
	static void		getAnimLabels(LLDynamicArray<std::string>* labels);
	static void		getAnimNames(LLDynamicArray<std::string>* names);	
private:
	LLWString		mNameString;
	std::string  	mSubNameString;
	std::string  	mTitle;
	bool	  		mNameAway;
	bool	  		mNameBusy;
	bool	  		mNameMute;
	bool      		mNameAppearance;
	bool			mRenderTag;
	bool      		mRenderGroupTitles;
	std::string      mRenderedName;
	std::string      mClientName;
	std::string      mIdleString;
	S32		  mUsedNameSystem;

	//--------------------------------------------------------------------
	// Display the name (then optionally fade it out)
	//--------------------------------------------------------------------
public:
	LLFrameTimer	mChatTimer;
	LLPointer<LLHUDText>		mNameText;
private:
private:
	LLFrameTimer	mTimeVisible;
	std::deque<LLChat> mChats;
	BOOL			mTyping;
	LLFrameTimer	mTypingTimer;
	static void on_avatar_name_response(const LLUUID& agent_id, const LLAvatarName& av_name, void *userdata);

	//--------------------------------------------------------------------
	// Client tagging
	//--------------------------------------------------------------------
public:
	// <edit>
	void getClientInfo(std::string& clientTag, LLColor4& tagColor, BOOL useComment=FALSE);
	std::string extraMetadata;
	// </edit>
	
	static bool 	updateClientTags();
	static bool 	loadClientTags();
	std::string 	mClientTag; //Zwagoth's new client identification system. -HgB
	LLColor4 		mClientColor; //Zwagoth's new client identification system. -HgB

	bool mNameFromChatOverride;
	bool mNameFromChatChanged;
	std::string mNameFromChatText;
	std::string mNameFromAttachment;

/**                    Name
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    SOUNDS
 **/

	//--------------------------------------------------------------------
	// Voice visualizer
	//--------------------------------------------------------------------
public:
	// Responsible for detecting the user's voice signal (and when the
	// user speaks, it puts a voice symbol over the avatar's head) and gesticulations
	LLPointer<LLVoiceVisualizer>  mVoiceVisualizer;
	int					mCurrentGesticulationLevel;

	//--------------------------------------------------------------------
	// Step sound
	//--------------------------------------------------------------------
protected:
	const LLUUID& 		getStepSound() const;
private:
	// Global table of sound ids per material, and the ground
	const static LLUUID	sStepSounds[LL_MCODE_END];
	const static LLUUID	sStepSoundOnLand;
protected:
	static LLSD sClientResolutionList;
	friend class LLPrefsAscentVanImpl;

	bool isUnknownClient();
	static void resolveClient(LLColor4& avatar_name_color, std::string& client, LLVOAvatar* avatar);
	friend class LLFloaterAvatarList;
	//--------------------------------------------------------------------
	// Foot step state (for generating sounds)
	//--------------------------------------------------------------------
public:
	void 				setFootPlane(const LLVector4 &plane) { mFootPlane = plane; }
	LLVector4			mFootPlane;
private:
	BOOL				mWasOnGroundLeft;
	BOOL				mWasOnGroundRight;

/**                    Sounds
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    DIAGNOSTICS
 **/

	//--------------------------------------------------------------------
	// General
	//--------------------------------------------------------------------
public:
	static void			dumpArchetypeXML(void*);
	static void			dumpScratchTextureByteCount(); //Agent only
	static void			dumpBakedStatus();
	const std::string 	getBakedStatusForPrintout() const;
	void				dumpAvatarTEs(const std::string& context) const;

	static F32 			sUnbakedTime; // Total seconds with >=1 unbaked avatars
	static F32 			sUnbakedUpdateTime; // Last time stats were updated (to prevent multiple updates per frame) 
	static F32 			sGreyTime; // Total seconds with >=1 grey avatars	
	static F32 			sGreyUpdateTime; // Last time stats were updated (to prevent multiple updates per frame) 
protected:
	S32					getUnbakedPixelAreaRank();
	BOOL				mHasGrey;
private:
	LLUUID				mSavedTE[ LLVOAvatarDefines::TEX_NUM_INDICES ];
	BOOL				mHasBakedHair;
	F32					mMinPixelArea;
	F32					mMaxPixelArea;
	F32					mAdjustedPixelArea;
	std::string  		mDebugText;

/**                    Diagnostics
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    SUPPORT CLASSES
 **/

protected: // Shared with LLVOAvatarSelf

	struct LLVOAvatarXmlInfo
	{
		LLVOAvatarXmlInfo();
		~LLVOAvatarXmlInfo();

		BOOL 	parseXmlSkeletonNode(LLXmlTreeNode* root);
		BOOL 	parseXmlMeshNodes(LLXmlTreeNode* root);
		BOOL 	parseXmlColorNodes(LLXmlTreeNode* root);
		BOOL 	parseXmlLayerNodes(LLXmlTreeNode* root);
		BOOL 	parseXmlDriverNodes(LLXmlTreeNode* root);

		struct LLVOAvatarMeshInfo
		{
			typedef std::pair<LLPolyMorphTargetInfo*,BOOL> morph_info_pair_t;
			typedef std::vector<morph_info_pair_t> morph_info_list_t;

			LLVOAvatarMeshInfo() : mLOD(0), mMinPixelArea(.1f) {}
			~LLVOAvatarMeshInfo()
			{
				morph_info_list_t::iterator iter;
				for (iter = mPolyMorphTargetInfoList.begin(); iter != mPolyMorphTargetInfoList.end(); iter++)
				{
					delete iter->first;
				}
				mPolyMorphTargetInfoList.clear();
			}

			std::string mType;
			S32			mLOD;
			std::string	mMeshFileName;
			std::string	mReferenceMeshName;
			F32			mMinPixelArea;
			morph_info_list_t mPolyMorphTargetInfoList;
		};
		typedef std::vector<LLVOAvatarMeshInfo*> mesh_info_list_t;
		mesh_info_list_t mMeshInfoList;

		typedef std::vector<LLPolySkeletalDistortionInfo*> skeletal_distortion_info_list_t;
		skeletal_distortion_info_list_t mSkeletalDistortionInfoList;
	
		struct LLVOAvatarAttachmentInfo
		{
			LLVOAvatarAttachmentInfo()
				: mGroup(-1), mAttachmentID(-1), mPieMenuSlice(-1), mVisibleFirstPerson(FALSE),
				  mIsHUDAttachment(FALSE), mHasPosition(FALSE), mHasRotation(FALSE) {}
			std::string mName;
			std::string mJointName;
			LLVector3 mPosition;
			LLVector3 mRotationEuler;
			S32 mGroup;
			S32 mAttachmentID;
			S32 mPieMenuSlice;
			BOOL mVisibleFirstPerson;
			BOOL mIsHUDAttachment;
			BOOL mHasPosition;
			BOOL mHasRotation;
		};
		typedef std::vector<LLVOAvatarAttachmentInfo*> attachment_info_list_t;
		attachment_info_list_t mAttachmentInfoList;
	
		LLTexGlobalColorInfo *mTexSkinColorInfo;
		LLTexGlobalColorInfo *mTexHairColorInfo;
		LLTexGlobalColorInfo *mTexEyeColorInfo;

		typedef std::vector<LLTexLayerSetInfo*> layer_info_list_t;
		layer_info_list_t mLayerInfoList;

		typedef std::vector<LLDriverParamInfo*> driver_info_list_t;
		driver_info_list_t mDriverInfoList;
	};

public: //Public until pulled out of LLTexLayer
	struct LLMaskedMorph
	{
		LLMaskedMorph(LLPolyMorphTarget *morph_target, BOOL invert) :
			mMorphTarget(morph_target), 
			mInvert(invert)
		{
			morph_target->addPendingMorphMask();
		}
	
		LLPolyMorphTarget	*mMorphTarget;
		BOOL				mInvert;
	};

/**                    Support classes
 **                                                                            **
 *******************************************************************************/

// <edit>

public:
	//bool mNametagSaysIdle;
	//bool mIdleForever;
	//LLFrameTimer mIdleTimer;
	//U32 mIdleMinutes;
	LLUUID mFocusObject;
	LLVector3d mFocusVector;
	//void resetIdleTime();
// </edit>

private:
	//-----------------------------------------------------------------------------------------------
	// Per-avatar information about texture data.
	// To-do: Move this to private implementation class
	//-----------------------------------------------------------------------------------------------

	struct LocalTextureData
	{
		LocalTextureData() : mIsBakedReady(false), mDiscard(MAX_DISCARD_LEVEL+1), mImage(NULL)
		{}
		LLPointer<LLViewerFetchedTexture> mImage;
		bool mIsBakedReady;
		S32 mDiscard;
	};
	typedef std::map<LLVOAvatarDefines::ETextureIndex, LocalTextureData> localtexture_map_t;
	localtexture_map_t mLocalTextureData;


	//--------------------------------------------------------------------
	// Private member variables.
	//--------------------------------------------------------------------
	BOOL mIsSelf; // True if this avatar is for this viewer's agent

	LLViewerJoint *mScreenp; // special purpose joint for HUD attachments

	// Scratch textures used for compositing
	static LLMap< LLGLenum, LLGLuint*> sScratchTexNames;
	static LLMap< LLGLenum, F32*> sScratchTexLastBindTime;
	static S32 sScratchTexBytes;
	
	static LLSD sClientResolutionList;

	bool isUnknownClient();
	static void resolveClient(LLColor4& avatar_name_color, std::string& client, LLVOAvatar* avatar);
	friend class LLFloaterAvatarList;

	LLPointer<LLHUDEffectSpiral> mBeam;
	LLFrameTimer mBeamTimer;
	
	U64		  mLastRegionHandle;
	LLFrameTimer mRegionCrossingTimer;
	S32		  mRegionCrossingCount;
	
	static bool		sDoProperArc;
};

//-----------------------------------------------------------------------------------------------
// Inlines
//-----------------------------------------------------------------------------------------------
inline BOOL LLVOAvatar::isTextureDefined(U8 te) const
{
	return (getTEImage(te)->getID() != IMG_DEFAULT_AVATAR && getTEImage(te)->getID() != IMG_DEFAULT);
}

inline BOOL LLVOAvatar::isTextureVisible(U8 te) const
{
	return ((isTextureDefined(te) || mIsSelf)
			&& (getTEImage(te)->getID() != IMG_INVISIBLE 
				|| LLDrawPoolAlpha::sShowDebugAlpha));
}

#endif // LL_VO_AVATAR_H
