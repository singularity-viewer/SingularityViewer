/** 
 * @file llagentwearables.h
 * @brief LLAgentWearables class header file
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 */

#ifndef LL_LLAGENTWEARABLES_H
#define LL_LLAGENTWEARABLES_H

// libraries
#include "llmemory.h"
#include "llui.h"
#include "lluuid.h"
#include "llinventory.h"

// newview
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llvoavatardefines.h"

class LLInventoryItem;
class LLVOAvatar;
class LLWearable;
class LLInitialWearablesFetch;
class LLViewerObject;
class LLTexLayerTemplate;

typedef std::vector<LLViewerObject*> llvo_vec_t;

class LLAgentWearables : public LLInitClass<LLAgentWearables>
{
	//--------------------------------------------------------------------
	// Constructors / destructors / Initializers
	//--------------------------------------------------------------------
public:
	friend class LLInitialWearablesFetch;

	LLAgentWearables();
	virtual ~LLAgentWearables();
	void 			setAvatarObject(LLVOAvatar *avatar);
	void			createStandardWearables(BOOL female); 
	void			cleanup();
	//void			dump();

	// LLInitClass interface
	static void initClass();
protected:
	void			createStandardWearablesDone(S32 index);
	void			createStandardWearablesAllDone();

	//--------------------------------------------------------------------
	// Queries
	//--------------------------------------------------------------------
public:
	BOOL			isWearingItem(const LLUUID& item_id) const;
	BOOL			isWearableModifiable(EWearableType type) const;
	BOOL			isWearableModifiable(const LLUUID& item_id) const;

	BOOL			isWearableCopyable(EWearableType type) const;
	BOOL			areWearablesLoaded() const { return mWearablesLoaded; };
	//void			updateWearablesLoaded();
	//void			checkWearablesLoaded() const;
	//bool			canMoveWearable(const LLUUID& item_id, bool closer_to_body);
	
	// Note: False for shape, skin, eyes, and hair, unless you have MORE than 1.
	//bool			canWearableBeRemoved(const LLWearable* wearable) const;

	//void			animateAllWearableParams(F32 delta, BOOL upload_bake);

	BOOL			needsReplacement(EWearableType wearableType, S32 remove);
	U32				getWearablePermMask(EWearableType type) const;
	
	//--------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------
public:
	const LLUUID&		getWearableItemID(EWearableType type ) const;
	//const LLUUID		getWearableAssetID(LLWearableType::EType type, U32 index /*= 0*/) const;
	const LLWearable*	getWearableFromItemID(const LLUUID& item_id) const;
	LLWearable*			getWearableFromItemID(const LLUUID& item_id);
	//LLWearable*	getWearableFromAssetID(const LLUUID& asset_id);
	LLInventoryItem* 	getWearableInventoryItem(EWearableType type);
	static BOOL		selfHasWearable( void* userdata );			// userdata is EWearableType
	LLWearable*			getWearable( const EWearableType type );
	const LLWearable*	getWearable( const EWearableType type ) const;
	//const LLWearable* 	getWearable(const LLWearableType::EType type, U32 index /*= 0*/) const;
	//LLWearable*		getTopWearable(const LLWearableType::EType type);
	//LLWearable*		getBottomWearable(const LLWearableType::EType type);
	//U32				getWearableCount(const LLWearableType::EType type) const;
	//U32				getWearableCount(const U32 tex_index) const;


	static EWearableType getTEWearableType( S32 te );
	static LLUUID	getDefaultTEImageID( S32 te );
	
	void			copyWearableToInventory( EWearableType type );

	static const U32 MAX_CLOTHING_PER_TYPE = 5; 


	//--------------------------------------------------------------------
	// Setters
	//--------------------------------------------------------------------

private:
	// Low-level data structure setter - public access is via setWearableItem, etc.
	//void 			setWearable(const LLWearableType::EType type, U32 index, LLWearable *wearable);
	//U32 			pushWearable(const LLWearableType::EType type, LLWearable *wearable);
	//void			wearableUpdated(LLWearable *wearable);
	//void 			popWearable(LLWearable *wearable);
	//void			popWearable(const LLWearableType::EType type, U32 index);
	
public:
	void			setWearableItem(LLInventoryItem* new_item, LLWearable* wearable);
	void			setWearableOutfit(const LLInventoryItem::item_array_t& items, const LLDynamicArray< LLWearable* >& wearables, BOOL remove);
	void			setWearableName(const LLUUID& item_id, const std::string& new_name);
	//void			addLocalTextureObject(const LLWearableType::EType wearable_type, const LLVOAvatarDefines::ETextureIndex texture_type, U32 wearable_index);
	//U32			getWearableIndex(LLWearable *wearable);

protected:
	void			setWearableFinal( LLInventoryItem* new_item, LLWearable* new_wearable );
	static bool		onSetWearableDialog(const LLSD& notification, const LLSD& response, LLWearable* wearable);

	void			addWearableToAgentInventory(LLPointer<LLInventoryCallback> cb,
												LLWearable* wearable, 
												const LLUUID& category_id = LLUUID::null,
												BOOL notify = TRUE);
						
	/**
	 * @brief Only public because of addWearableToAgentInventoryCallback.
	 *
	 * NOTE: Do not call this method unless you are the inventory callback.
	 * NOTE: This can suffer from race conditions when working on the
	 * same values for index.
	 * @param index The index in mWearableEntry.
	 * @param item_id The inventory item id of the new wearable to wear.
	 * @param wearable The actual wearable data.
	 */
	void addWearabletoAgentInventoryDone(
		S32 index,
		const LLUUID& item_id,
		LLWearable* wearable);

	void			recoverMissingWearable(EWearableType type);
	void			recoverMissingWearableDone();

	//--------------------------------------------------------------------
	// Editing/moving wearables
	//--------------------------------------------------------------------

public:
	//static void		createWearable(LLWearableType::EType type, bool wear = false, const LLUUID& parent_id = LLUUID::null);
	//static void		editWearable(const LLUUID& item_id);
	//bool			moveWearable(const LLViewerInventoryItem* item, bool closer_to_body);

	//void			requestEditingWearable(const LLUUID& item_id);
	//void			editWearableIfRequested(const LLUUID& item_id);

private:
	//LLUUID			mItemToEdit;

	//--------------------------------------------------------------------
	// Removing wearables
	//--------------------------------------------------------------------
public:
	void			removeWearable( EWearableType type );
private:
	void			removeWearableFinal( EWearableType type );
protected:
	static bool		onRemoveWearableDialog(const LLSD& notification, const LLSD& response);
	static void		userRemoveAllClothesStep2(BOOL proceed, void* userdata ); // userdata is NULL
	
	//--------------------------------------------------------------------
	// Server Communication
	//--------------------------------------------------------------------
public:
	// Processes the initial wearables update message (if necessary, since the outfit folder makes it redundant)
	static void		processAgentInitialWearablesUpdate(LLMessageSystem* mesgsys, void** user_data);
	//LLUUID			computeBakedTextureHash(LLVOAvatarDefines::EBakedTextureIndex baked_index,
	//										BOOL generate_valid_hash = TRUE);

protected:
	void			sendAgentWearablesUpdate();
	void			sendAgentWearablesRequest();
	void			queryWearableCache();
	//void 			updateServer();
	static void		onInitialWearableAssetArrived(LLWearable* wearable, void* userdata);

	//--------------------------------------------------------------------
	// Outfits
	//--------------------------------------------------------------------
public:
	
	// Should only be called if we *know* we've never done so before, since users may
	// not want the Library outfits to stay in their quick outfit selector and can delete them.
	//void			populateMyOutfitsFolder();
	void			makeNewOutfit(
						const std::string& new_folder_name,
						const LLDynamicArray<S32>& wearables_to_include,
						const LLDynamicArray<S32>& attachments_to_include,
						BOOL rename_clothing);
private:

	void			makeNewOutfitDone(S32 index);

	//--------------------------------------------------------------------
	// Save Wearables
	//--------------------------------------------------------------------
public:	
	void			saveWearableAs( EWearableType type, const std::string& new_name, BOOL save_in_lost_and_found );
	void			saveWearable( EWearableType type, BOOL send_update = TRUE );

	void			saveAllWearables();
	void			revertWearable( EWearableType type );
	void			revertAllWearables();

	//--------------------------------------------------------------------
	// Static UI hooks
	//--------------------------------------------------------------------
public:
	static void		userRemoveWearable( void* userdata );	// userdata is EWearableType
	static void		userRemoveAllClothes( void* userdata );	// userdata is NULL
//	static void 	userUpdateAttachments(LLInventoryModel::item_array_t& obj_item_array);
// [SL:KB] - Patch: Appearance-SyncAttach | Checked: 2010-09-22 (Catznip-2.2.0a) | Added: Catznip-2.2.0a
	// Not the best way to go about this but other attempts changed far too much LL code to be a viable solution
	static void 	userUpdateAttachments(LLInventoryModel::item_array_t& obj_item_array, bool fAttachOnly = false);
// [/SL:KB]
	static void		userRemoveAllAttachments( void* userdata);	// userdata is NULLy);
	static void		userRemoveMultipleAttachments(llvo_vec_t& llvo_array);
	static void		userRemoveAllAttachments();
	static void		userAttachMultipleAttachments(LLInventoryModel::item_array_t& obj_item_array);

	BOOL			itemUpdatePending(const LLUUID& item_id) const;
	U32				itemUpdatePendingCount() const;

	//--------------------------------------------------------------------
	// Signals
	//--------------------------------------------------------------------
public:
	/*typedef boost::function<void()>			loading_started_callback_t;
	typedef boost::signals2::signal<void()>	loading_started_signal_t;
	boost::signals2::connection				addLoadingStartedCallback(loading_started_callback_t cb);

	typedef boost::function<void()>			loaded_callback_t;
	typedef boost::signals2::signal<void()>	loaded_signal_t;
	boost::signals2::connection				addLoadedCallback(loaded_callback_t cb);

	void									notifyLoadingStarted();
	void									notifyLoadingFinished();*/

private:

	//--------------------------------------------------------------------
	// Member variables
	//--------------------------------------------------------------------
	struct LLWearableEntry
	{
		LLWearableEntry() : mItemID( LLUUID::null ), mWearable( NULL ) {}

		LLUUID		mItemID;	// ID of the inventory item in the agent's inventory.
		LLWearable*	mWearable;
	};
	LLWearableEntry mWearableEntry[ WT_COUNT ];
	BOOL			mWearablesLoaded;
	
	//--------------------------------------------------------------------------------
	// Support classes
	//--------------------------------------------------------------------------------
private:
	class createStandardWearablesAllDoneCallback : public LLRefCount
	{
	protected:
		~createStandardWearablesAllDoneCallback();
	};
	class sendAgentWearablesUpdateCallback : public LLRefCount
	{
	protected:
		~sendAgentWearablesUpdateCallback();
	};

	class addWearableToAgentInventoryCallback : public LLInventoryCallback
	{
	public:
		enum {
			CALL_NONE = 0,
			CALL_UPDATE = 1,
			CALL_RECOVERDONE = 2,
			CALL_CREATESTANDARDDONE = 4,
			CALL_MAKENEWOUTFITDONE = 8
		} EType;

		/**
		 * @brief Construct a callback for dealing with the wearables.
		 *
		 * Would like to pass the agent in here, but we can't safely
		 * count on it being around later.  Just use gAgent directly.
		 * @param cb callback to execute on completion (??? unused ???)
		 * @param index Index for the wearable in the agent
		 * @param wearable The wearable data.
		 * @param todo Bitmask of actions to take on completion.
		 */
		addWearableToAgentInventoryCallback(
			LLPointer<LLRefCount> cb,
			S32 index,
			LLWearable* wearable,
			U32 todo = CALL_NONE);
		virtual void fire(const LLUUID& inv_item);

	private:
		S32 mIndex;
		LLWearable* mWearable;
		U32 mTodo;
		LLPointer<LLRefCount> mCB;
	};

}; // LLAgentWearables

extern LLAgentWearables gAgentWearables;

//--------------------------------------------------------------------
// Types
//--------------------------------------------------------------------	

#endif // LL_AGENTWEARABLES_H
