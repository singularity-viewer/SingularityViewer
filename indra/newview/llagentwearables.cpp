/** 
 * @file llagentwearables.cpp
 * @brief LLAgentWearables class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"
#include "llagentwearables.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llcallbacklist.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "llinventorybridge.h"
#include "llmd5.h"
#include "llnotifications.h"
#include "lltexlayer.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llwearable.h"
#include "llwearablelist.h"

#include "cofmgr.h"
#include "llfloatercustomize.h"

// [RLVa:KB] - Checked: 2010-09-27 (RLVa-1.1.3b)
#include "rlvhandler.h"
#include "rlvinventory.h"
#include "llattachmentsmgr.h"
// [/RLVa:KB]

#include "hippogridmanager.h"

#include <boost/scoped_ptr.hpp>

LLAgentWearables gAgentWearables;

using namespace LLVOAvatarDefines;
LLAgentWearables::LLAgentWearables() :
	mWearablesLoaded(FALSE)
{
}

LLAgentWearables::~LLAgentWearables()
{
	cleanup();
}

void LLAgentWearables::cleanup()
{
}

// static
void LLAgentWearables::initClass()
{
}

void LLAgentWearables::setAvatarObject(LLVOAvatar *avatar)
{ 
	if (avatar)
	{
		sendAgentWearablesRequest();
	}
}
// wearables
LLAgentWearables::createStandardWearablesAllDoneCallback::~createStandardWearablesAllDoneCallback()
{
	llinfos << "destructor - all done?" << llendl;
	gAgentWearables.createStandardWearablesAllDone();
}

LLAgentWearables::sendAgentWearablesUpdateCallback::~sendAgentWearablesUpdateCallback()
{
	gAgentWearables.sendAgentWearablesUpdate();
}

LLAgentWearables::addWearableToAgentInventoryCallback::addWearableToAgentInventoryCallback(
	LLPointer<LLRefCount> cb, S32 index, LLWearable* wearable, U32 todo) :
	mIndex(index),
	mWearable(wearable),
	mTodo(todo),
	mCB(cb)
{
	llinfos << "constructor" << llendl;
}

void LLAgentWearables::addWearableToAgentInventoryCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
		return;

	gAgentWearables.addWearabletoAgentInventoryDone(mIndex, inv_item, mWearable);

	if (mTodo & CALL_UPDATE)
	{
		gAgentWearables.sendAgentWearablesUpdate();
	}
	if (mTodo & CALL_RECOVERDONE)
	{
		gAgentWearables.recoverMissingWearableDone();
	}
	/*
	 * Do this for every one in the loop
	 */
	if (mTodo & CALL_CREATESTANDARDDONE)
	{
		gAgentWearables.createStandardWearablesDone(mIndex);
	}
	if (mTodo & CALL_MAKENEWOUTFITDONE)
	{
		gAgentWearables.makeNewOutfitDone(mIndex);
	}
}

void LLAgentWearables::addWearabletoAgentInventoryDone(
	S32 index,
	const LLUUID& item_id,
	LLWearable* wearable)
{
	if (item_id.isNull())
		return;

	LLUUID old_item_id = mWearableEntry[index].mItemID;
	mWearableEntry[index].mItemID = item_id;
	mWearableEntry[index].mWearable = wearable;
	if (old_item_id.notNull())
		gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
	gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);
	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if(item && wearable)
	{
		// We're changing the asset id, so we both need to set it
		// locally via setAssetUUID() and via setTransactionID() which
		// will be decoded on the server. JC
		item->setAssetUUID(wearable->getID());
		item->setTransactionID(wearable->getTransactionID());
		gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		item->updateServer(FALSE);
	}
	gInventory.notifyObservers();
}

void LLAgentWearables::sendAgentWearablesUpdate()
{
	// First make sure that we have inventory items for each wearable
	S32 i;
	for(i=0; i < WT_COUNT; ++i)
	{
		LLWearable* wearable = mWearableEntry[ i ].mWearable;
		if (wearable)
		{
			if( mWearableEntry[ i ].mItemID.isNull() )
			{
				LLPointer<LLInventoryCallback> cb =
					new addWearableToAgentInventoryCallback(
						LLPointer<LLRefCount>(NULL),
						i,
						wearable,
						addWearableToAgentInventoryCallback::CALL_NONE);
				addWearableToAgentInventory(cb, wearable);
			}
			else
			{
				gInventory.addChangedMask( LLInventoryObserver::LABEL,
						mWearableEntry[i].mItemID );
			}
		}
	}

	// Then make sure the inventory is in sync with the avatar.
	gInventory.notifyObservers();

	// This isn't the proper place to be doing this, but it's a good "catch-all"
	LLCOFMgr::instance().synchWearables();

	// Send the AgentIsNowWearing 
	gMessageSystem->newMessageFast(_PREHASH_AgentIsNowWearing);

	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	LL_DEBUGS("Wearables") << "sendAgentWearablesUpdate()" << LL_ENDL;
	for(i=0; i < WT_COUNT; ++i)
	{
		gMessageSystem->nextBlockFast(_PREHASH_WearableData);

		U8 type_u8 = (U8)i;
		gMessageSystem->addU8Fast(_PREHASH_WearableType, type_u8 );

		LLWearable* wearable = mWearableEntry[ i ].mWearable;
		if( wearable )
		{
			LL_DEBUGS("Wearables") << "Sending wearable " << wearable->getName() << " mItemID = " << mWearableEntry[ i ].mItemID << LL_ENDL; 
			LLUUID item_id = mWearableEntry[i].mItemID;
			const LLViewerInventoryItem *item = gInventory.getItem(item_id);
			if (item && item->getIsLinkType())
			{
				// Get the itemID that this item points to.  i.e. make sure
				// we are storing baseitems, not their links, in the database.
				item_id = item->getLinkedUUID();
			}
			gMessageSystem->addUUIDFast(_PREHASH_ItemID, item_id);
		}
		else
		{
			LL_DEBUGS("Wearables") << "Not wearing wearable type " << LLWearable::typeToTypeName((EWearableType)i) << LL_ENDL;
			gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID::null );
		}

		LL_DEBUGS("Wearables") << "       " << LLWearable::typeToTypeLabel((EWearableType)i) << " : " << (wearable ? wearable->getID() : LLUUID::null) << LL_ENDL;
	}
	gAgent.sendReliableMessage();
}

void LLAgentWearables::saveWearable( EWearableType type, BOOL send_update )
{
	LLWearable* old_wearable = mWearableEntry[(S32)type].mWearable;
	if( old_wearable && (old_wearable->isDirty() || old_wearable->isOldVersion()) )
	{
		LLWearable* new_wearable = gWearableList.createCopyFromAvatar( old_wearable );
		mWearableEntry[(S32)type].mWearable = new_wearable;

		LLInventoryItem* item = gInventory.getItem(mWearableEntry[(S32)type].mItemID);
		if( item )
		{
			// Update existing inventory item
			LLPointer<LLViewerInventoryItem> template_item =
				new LLViewerInventoryItem(item->getUUID(),
										  item->getParentUUID(),
										  item->getPermissions(),
										  new_wearable->getID(),
										  new_wearable->getAssetType(),
										  item->getInventoryType(),
										  item->getName(),
										  item->getDescription(),
										  item->getSaleInfo(),
										  item->getFlags(),
										  item->getCreationDate());
			template_item->setTransactionID(new_wearable->getTransactionID());
			template_item->updateServer(FALSE);
			gInventory.updateItem(template_item);
		}
		else
		{
			// Add a new inventory item (shouldn't ever happen here)
			U32 todo = addWearableToAgentInventoryCallback::CALL_NONE;
			if (send_update)
			{
				todo |= addWearableToAgentInventoryCallback::CALL_UPDATE;
			}
			LLPointer<LLInventoryCallback> cb =
				new addWearableToAgentInventoryCallback(
					LLPointer<LLRefCount>(NULL),
					(S32)type,
					new_wearable,
					todo);
			addWearableToAgentInventory(cb, new_wearable);
			return;
		}
		
		gAgent.getAvatarObject()->wearableUpdated( type );

		if( send_update )
		{
			sendAgentWearablesUpdate();
		}
	}
}

void LLAgentWearables::saveWearableAs(
	EWearableType type,
	const std::string& new_name,
	BOOL save_in_lost_and_found)
{
	if(!isWearableCopyable(type))
	{
		llwarns << "LLAgent::saveWearableAs() not copyable." << llendl;
		return;
	}
	LLWearable* old_wearable = getWearable(type);
	if(!old_wearable)
	{
		llwarns << "LLAgent::saveWearableAs() no old wearable." << llendl;
		return;
	}
	LLInventoryItem* item = gInventory.getItem(mWearableEntry[type].mItemID);
	if(!item)
	{
		llwarns << "LLAgent::saveWearableAs() no inventory item." << llendl;
		return;
	}
	std::string trunc_name(new_name);
	LLStringUtil::truncate(trunc_name, DB_INV_ITEM_NAME_STR_LEN);
	LLWearable* new_wearable = gWearableList.createCopyFromAvatar(
		old_wearable,
		trunc_name);
	LLPointer<LLInventoryCallback> cb =
		new addWearableToAgentInventoryCallback(
			LLPointer<LLRefCount>(NULL),
			type,
			new_wearable,
			addWearableToAgentInventoryCallback::CALL_UPDATE);
	LLUUID category_id;
	if (save_in_lost_and_found)
	{
		category_id = gInventory.findCategoryUUIDForType(
			LLAssetType::AT_LOST_AND_FOUND);
	}
	else
	{
		// put in same folder as original
		category_id = item->getParentUUID();
	}

	copy_inventory_item(
		gAgent.getID(),
		item->getPermissions().getOwner(),
		item->getUUID(),
		category_id,
		new_name,
		cb);

/*
	LLWearable* old_wearable = getWearable( type );
	if( old_wearable )
	{
		std::string old_name = old_wearable->getName();
		old_wearable->setName( new_name );
		LLWearable* new_wearable = gWearableList.createCopyFromAvatar( old_wearable );
		old_wearable->setName( old_name );
			
		LLUUID category_id;
		LLInventoryItem* item = gInventory.getItem( mWearableEntry[ type ].mItemID );
		if( item )
		{
			new_wearable->setPermissions(item->getPermissions());
			if (save_in_lost_and_found)
			{
				category_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LOST_AND_FOUND);
			}
			else
			{
				// put in same folder as original
				category_id = item->getParentUUID();
			}
			LLInventoryView* view = LLInventoryView::getActiveInventory();
			if(view)
			{
				view->getPanel()->setSelection(item->getUUID(), TAKE_FOCUS_NO);
			}
		}

		mWearableEntry[ type ].mWearable = new_wearable;
		LLPointer<LLInventoryCallback> cb =
			new addWearableToAgentInventoryCallback(
				LLPointer<LLRefCount>(NULL),
				type,
				addWearableToAgentInventoryCallback::CALL_UPDATE);
		addWearableToAgentInventory(cb, new_wearable, category_id);
	}
*/
}

void LLAgentWearables::revertWearable( EWearableType type )
{
	LLWearable* wearable = mWearableEntry[(S32)type].mWearable;
	if( wearable )
	{
		wearable->writeToAvatar( TRUE );
	}
	gAgent.sendAgentSetAppearance();
}

void LLAgentWearables::revertAllWearables()
{
	for( S32 i=0; i < WT_COUNT; i++ )
	{
		revertWearable( (EWearableType)i );
	}
}

void LLAgentWearables::saveAllWearables()
{
	//if(!gInventory.isLoaded())
	//{
	//	return;
	//}

	for( S32 i=0; i < WT_COUNT; i++ )
	{
		saveWearable( (EWearableType)i, FALSE );
	}
	sendAgentWearablesUpdate();
}

// Called when the user changes the name of a wearable inventory item that is currenlty being worn.
void LLAgentWearables::setWearableName( const LLUUID& item_id, const std::string& new_name )
{
	for( S32 i=0; i < WT_COUNT; i++ )
	{
		if( mWearableEntry[i].mItemID == item_id )
		{
			LLWearable* old_wearable = mWearableEntry[i].mWearable;
			llassert( old_wearable );

			std::string old_name = old_wearable->getName();
			old_wearable->setName( new_name );
			LLWearable* new_wearable = gWearableList.createCopy( old_wearable );
			LLInventoryItem* item = gInventory.getItem(item_id);
			if(item)
			{
				new_wearable->setPermissions(item->getPermissions());
			}
			old_wearable->setName( old_name );

			mWearableEntry[i].mWearable = new_wearable;
			sendAgentWearablesUpdate();
			break;
		}
	}
}


BOOL LLAgentWearables::isWearableModifiable(EWearableType type) const
{
	LLUUID item_id = getWearableItemID(type);
	return item_id.notNull() ? isWearableModifiable(item_id) : FALSE;
}

BOOL LLAgentWearables::isWearableModifiable(const LLUUID& item_id) const
{
	if(!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if(item && item->getPermissions().allowModifyBy(gAgent.getID(),
														gAgent.getGroupID()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLAgentWearables::isWearableCopyable(EWearableType type) const
{
	LLUUID item_id = getWearableItemID(type);
	if(!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if(item && item->getPermissions().allowCopyBy(gAgent.getID(),
													  gAgent.getGroupID()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

U32 LLAgentWearables::getWearablePermMask(EWearableType type) const
{
	LLUUID item_id = getWearableItemID(type);
	if(!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if(item)
		{
			return item->getPermissions().getMaskOwner();
		}
	}
	return PERM_NONE;
}

LLInventoryItem* LLAgentWearables::getWearableInventoryItem(EWearableType type)
{
	LLUUID item_id = getWearableItemID(type);
	LLInventoryItem* item = NULL;
	if(item_id.notNull())
	{
		 item = gInventory.getItem(item_id);
	}
	return item;
}

const LLWearable* LLAgentWearables::getWearableFromItemID( const LLUUID& item_id ) const
{
	for( S32 i=0; i < WT_COUNT; i++ )
	{
		if( mWearableEntry[i].mItemID == item_id )
		{
			return mWearableEntry[i].mWearable;
		}
	}
	return NULL;
}

LLWearable* LLAgentWearables::getWearableFromItemID( const LLUUID& item_id )
{
	for( S32 i=0; i < WT_COUNT; i++ )
	{
		if( mWearableEntry[i].mItemID == item_id )
		{
			return mWearableEntry[i].mWearable;
		}
	}
	return NULL;
}


void LLAgentWearables::sendAgentWearablesRequest()
{
	gMessageSystem->newMessageFast(_PREHASH_AgentWearablesRequest);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );
	gAgent.sendReliableMessage();
}

// Used to enable/disable menu items.
// static
BOOL LLAgentWearables::selfHasWearable( void* userdata )
{
	EWearableType type = (EWearableType)(intptr_t)userdata;
	return gAgentWearables.getWearable( type ) != NULL;
}
LLWearable* LLAgentWearables::getWearable(const EWearableType type)
{ 
	return (type < WT_COUNT) ? mWearableEntry[ type ].mWearable : NULL;
}
const LLWearable* LLAgentWearables::getWearable(const EWearableType type) const
{ 
	return (type < WT_COUNT) ? mWearableEntry[ type ].mWearable : NULL;
}

const LLUUID &LLAgentWearables::getWearableItemID(EWearableType type) const
{
	 return (type < WT_COUNT) ? mWearableEntry[ type ].mItemID : LLUUID::null;
}

BOOL LLAgentWearables::isWearingItem( const LLUUID& item_id ) const
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	return (getWearableFromItemID(base_item_id) != NULL);
}

// static
void LLAgentWearables::processAgentInitialWearablesUpdate( LLMessageSystem* mesgsys, void** user_data )
{
	// We should only receive this message a single time.  Ignore subsequent AgentWearablesUpdates
	// that may result from AgentWearablesRequest having been sent more than once. 
	static bool first = true;
	if (!first) return;
	first = false;

	LLUUID agent_id;
	gMessageSystem->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( avatar && (agent_id == avatar->getID()) )
	{
		gMessageSystem->getU32Fast(_PREHASH_AgentData, _PREHASH_SerialNum, gAgentQueryManager.mUpdateSerialNum );
		
		S32 num_wearables = gMessageSystem->getNumberOfBlocksFast(_PREHASH_WearableData);
		if( num_wearables < 4 )
		{
			// Transitional state.  Avatars should always have at least their body parts (hair, eyes, shape and skin).
			// The fact that they don't have any here (only a dummy is sent) implies that this account existed
			// before we had wearables, or that the database has gotten messed up.
			return;
		}

		//lldebugs << "processAgentInitialWearablesUpdate()" << llendl;
		// Add wearables
		LLUUID asset_id_array[ WT_COUNT ];
		S32 i;
		for( i=0; i < num_wearables; i++ )
		{
			U8 type_u8 = 0;
			gMessageSystem->getU8Fast(_PREHASH_WearableData, _PREHASH_WearableType, type_u8, i );
			if( type_u8 >= WT_COUNT )
			{
				continue;
			}
			EWearableType type = (EWearableType) type_u8;

			LLUUID item_id;
			gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_ItemID, item_id, i );

			LLUUID asset_id;
			gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_AssetID, asset_id, i );
			if( asset_id.isNull() )
			{
				LLWearable::removeFromAvatar( type, FALSE );
			}
			else
			{
				LLAssetType::EType asset_type = LLWearable::typeToAssetType( type );
				if( asset_type == LLAssetType::AT_NONE )
				{
					continue;
				}

				gAgentWearables.mWearableEntry[type].mItemID = item_id;
				asset_id_array[type] = asset_id;
			}

			LL_DEBUGS("Wearables") << "       " << LLWearable::typeToTypeLabel(type) << " " << asset_id << " item id " << gAgentWearables.mWearableEntry[type].mItemID.asString() << LL_ENDL;
		}

		LLCOFMgr::instance().fetchCOF();

		// now that we have the asset ids...request the wearable assets
		for( i = 0; i < WT_COUNT; i++ )
		{
			LL_DEBUGS("Wearables") << "      fetching " << asset_id_array[i] << LL_ENDL;
			if( !gAgentWearables.mWearableEntry[i].mItemID.isNull() )
			{
				gWearableList.getAsset( 
					asset_id_array[i],
					LLStringUtil::null,
					LLWearable::typeToAssetType( (EWearableType) i ), 
					LLAgentWearables::onInitialWearableAssetArrived, (void*)(intptr_t)i );
			}
		}

		// Not really sure where else to put this
		gIdleCallbacks.addFunction(&LLAttachmentsMgr::onIdle, NULL);
	}
}

// A single wearable that the avatar was wearing on start-up has arrived from the database.
// static
void LLAgentWearables::onInitialWearableAssetArrived( LLWearable* wearable, void* userdata )
{
	EWearableType type = (EWearableType)(intptr_t)userdata;

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( !avatar )
	{
		return;
	}

	if( wearable )
	{
		llassert( type == wearable->getType() );
		gAgentWearables.mWearableEntry[ type ].mWearable = wearable;

		// disable composites if initial textures are baked
		avatar->setupComposites();
		gAgentWearables.queryWearableCache();

		wearable->writeToAvatar( FALSE );
		avatar->setCompositeUpdatesEnabled(TRUE);
		gInventory.addChangedMask( LLInventoryObserver::LABEL, gAgentWearables.mWearableEntry[type].mItemID );
	}
	else
	{
		// Somehow the asset doesn't exist in the database.
		gAgentWearables.recoverMissingWearable( type );
	}

	gInventory.notifyObservers();

	// Have all the wearables that the avatar was wearing at log-in arrived?
	if( !gAgentWearables.mWearablesLoaded )
	{
		gAgentWearables.mWearablesLoaded = TRUE;
		for( S32 i = 0; i < WT_COUNT; i++ )
		{
			if( !gAgentWearables.mWearableEntry[i].mItemID.isNull() && !gAgentWearables.mWearableEntry[i].mWearable )
			{
				gAgentWearables.mWearablesLoaded = FALSE;
				break;
			}
		}
	}

	if( gAgentWearables.mWearablesLoaded )
	{
		// Make sure that the server's idea of the avatar's wearables actually match the wearables.
		gAgent.sendAgentSetAppearance();

		// Check to see if there are any baked textures that we hadn't uploaded before we logged off last time.
		// If there are any, schedule them to be uploaded as soon as the layer textures they depend on arrive.
		if( !gAgentCamera.cameraCustomizeAvatar() )
		{
			avatar->requestLayerSetUploads();
		}
	}
}

// Normally, all wearables referred to "AgentWearablesUpdate" will correspond to actual assets in the
// database.  If for some reason, we can't load one of those assets, we can try to reconstruct it so that
// the user isn't left without a shape, for example.  (We can do that only after the inventory has loaded.)
void LLAgentWearables::recoverMissingWearable( EWearableType type )
{
	// Try to recover by replacing missing wearable with a new one.
	LLNotifications::instance().add("ReplacedMissingWearable");
	lldebugs << "Wearable " << LLWearable::typeToTypeLabel( type ) << " could not be downloaded.  Replaced inventory item with default wearable." << llendl;
	LLWearable* new_wearable = gWearableList.createNewWearable(type);

	S32 type_s32 = (S32) type;
	mWearableEntry[type_s32].mWearable = new_wearable;
	new_wearable->writeToAvatar( TRUE );

	// Add a new one in the lost and found folder.
	// (We used to overwrite the "not found" one, but that could potentially
	// destory content.) JC
	LLUUID lost_and_found_id = 
		gInventory.findCategoryUUIDForType(LLAssetType::AT_LOST_AND_FOUND);
	LLPointer<LLInventoryCallback> cb =
		new addWearableToAgentInventoryCallback(
			LLPointer<LLRefCount>(NULL),
			type_s32,
			new_wearable,
			addWearableToAgentInventoryCallback::CALL_RECOVERDONE);
	addWearableToAgentInventory( cb, new_wearable, lost_and_found_id, TRUE);
}

void LLAgentWearables::recoverMissingWearableDone()
{
	// Have all the wearables that the avatar was wearing at log-in arrived or been fabricated?
	mWearablesLoaded = TRUE;
	for( S32 i = 0; i < WT_COUNT; i++ )
	{
		if( !mWearableEntry[i].mItemID.isNull() && !mWearableEntry[i].mWearable )
		{
			mWearablesLoaded = FALSE;
			break;
		}
	}

	if( mWearablesLoaded )
	{
		// Make sure that the server's idea of the avatar's wearables actually match the wearables.
		gAgent.sendAgentSetAppearance();
	}
	else
	{
		gInventory.addChangedMask( LLInventoryObserver::LABEL, LLUUID::null );
		gInventory.notifyObservers();
	}
}

void LLAgentWearables::createStandardWearables(BOOL female)
{
	llwarns << "Creating Standard " << (female ? "female" : "male" )
			<< " Wearables" << llendl;

	if (!isAgentAvatarValid()) return;

	gAgent.getAvatarObject()->setSex(female ? SEX_FEMALE : SEX_MALE);

	const BOOL create[WT_COUNT] = 
	{
		TRUE,  //WT_SHAPE
		TRUE,  //WT_SKIN
		TRUE,  //WT_HAIR
		TRUE,  //WT_EYES
		TRUE,  //WT_SHIRT
		TRUE,  //WT_PANTS
		TRUE,  //WT_SHOES
		TRUE,  //WT_SOCKS
		FALSE, //WT_JACKET
		FALSE, //WT_GLOVES
		TRUE,  //WT_UNDERSHIRT
		TRUE,  //WT_UNDERPANTS
		FALSE, //WT_SKIRT
		FALSE, //WT_ALPHA
		FALSE, //WT_TATTOO
		FALSE, //WT_PHYSICS
	};

	for( S32 i=0; i < WT_COUNT; i++ )
	{
		bool once = false;
		LLPointer<LLRefCount> donecb = NULL;
		if( create[i] )
		{
			if (!once)
			{
				once = true;
				donecb = new createStandardWearablesAllDoneCallback;
			}
			llassert( mWearableEntry[i].mWearable == NULL );
			LLWearable* wearable = gWearableList.createNewWearable((EWearableType)i);
			mWearableEntry[i].mWearable = wearable;
			// no need to update here...
			LLPointer<LLInventoryCallback> cb =
				new addWearableToAgentInventoryCallback(
					donecb,
					i,
					wearable,
					addWearableToAgentInventoryCallback::CALL_CREATESTANDARDDONE);
			addWearableToAgentInventory(cb, wearable, LLUUID::null, FALSE);
		}
	}
}
void LLAgentWearables::createStandardWearablesDone(S32 index)
{
	LLWearable* wearable = mWearableEntry[index].mWearable;

	if (wearable)
	{
		wearable->writeToAvatar(TRUE);
	}
}

void LLAgentWearables::createStandardWearablesAllDone()
{
	// ... because sendAgentWearablesUpdate will notify inventory
	// observers.
	mWearablesLoaded = TRUE; 
	sendAgentWearablesUpdate();
	gAgent.sendAgentSetAppearance();

	// Treat this as the first texture entry message, if none received yet
	gAgent.getAvatarObject()->onFirstTEMessageReceived();
}

void LLAgentWearables::makeNewOutfit( 
	const std::string& new_folder_name,
	const LLDynamicArray<S32>& wearables_to_include,
	const LLDynamicArray<S32>& attachments_to_include,
	BOOL rename_clothing)
{
	if (!gAgent.getAvatarObject())
	{
		return;
	}

	BOOL fUseLinks = !gSavedSettings.getBOOL("UseInventoryLinks") ||
					 !gHippoGridManager->getConnectedGrid()->supportsInvLinks();
	BOOL fUseOutfits = gSavedSettings.getBOOL("UseOutfitFolders") &&
					   gHippoGridManager->getConnectedGrid()->supportsInvLinks();

	LLAssetType::EType typeDest = (fUseOutfits) ? LLAssetType::AT_MY_OUTFITS : LLAssetType::AT_CLOTHING;
	LLAssetType::EType typeFolder = (fUseOutfits) ? LLAssetType::AT_OUTFIT : LLAssetType::AT_NONE;

	// First, make a folder for the outfit.
	LLUUID folder_id = gInventory.createNewCategory(gInventory.findCategoryUUIDForType(typeDest), typeFolder, new_folder_name);

	bool found_first_item = false;

	///////////////////
	// Wearables

	if( wearables_to_include.count() )
	{
		// Then, iterate though each of the wearables and save copies of them in the folder.
		S32 i;
		S32 count = wearables_to_include.count();
		LLPointer<LLRefCount> cbdone = NULL;
		for( i = 0; i < count; ++i )
		{
			S32 index = wearables_to_include[i];
			LLWearable* old_wearable = mWearableEntry[ index ].mWearable;
			if( old_wearable )
			{
				LLViewerInventoryItem* item = gInventory.getItem(mWearableEntry[index].mItemID);
				if (fUseOutfits)
				{
					std::string strOrdering = llformat("@%d", item->getWearableType() * 100);

					link_inventory_item(
						gAgent.getID(),
						item->getLinkedUUID(),
						folder_id,
						item->getName(),
						strOrdering,
						LLAssetType::AT_LINK,
						LLPointer<LLInventoryCallback>(NULL));
				}
				else
				{
					std::string new_name = item->getName();
					if (rename_clothing)
					{
						new_name = new_folder_name;
						new_name.append(" ");
						new_name.append(old_wearable->getTypeLabel());
						LLStringUtil::truncate(new_name, DB_INV_ITEM_NAME_STR_LEN);
					}

					if (fUseLinks || isWearableCopyable((EWearableType)index))
					{
						LLWearable* new_wearable = gWearableList.createCopy(old_wearable);
						if (rename_clothing)
						{
							new_wearable->setName(new_name);
						}

						S32 todo = addWearableToAgentInventoryCallback::CALL_NONE;
						if (!found_first_item)
						{
							found_first_item = true;
							/* set the focus to the first item */
							todo |= addWearableToAgentInventoryCallback::CALL_MAKENEWOUTFITDONE;
							/* send the agent wearables update when done */
							cbdone = new sendAgentWearablesUpdateCallback;
						}
						LLPointer<LLInventoryCallback> cb =
							new addWearableToAgentInventoryCallback(
								cbdone,
								index,
								new_wearable,
								todo);
						if (isWearableCopyable((EWearableType)index))
						{
							copy_inventory_item(
								gAgent.getID(),
								item->getPermissions().getOwner(),
								item->getLinkedUUID(),
								folder_id,
								new_name,
								cb);
						}
						else
						{
							move_inventory_item(
								gAgent.getID(),
								gAgent.getSessionID(),
								item->getLinkedUUID(),
								folder_id,
								new_name,
								cb);
						}
					}
					else
					{
						link_inventory_item(
							gAgent.getID(),
							item->getLinkedUUID(),
							folder_id,
							item->getName(),		// Apparently, links cannot have arbitrary names...
							item->getDescription(),
							LLAssetType::AT_LINK,
							LLPointer<LLInventoryCallback>(NULL));
					}
				}
			}
		}
		gInventory.notifyObservers();
	}


	///////////////////
	// Attachments

	if( attachments_to_include.count() )
	{
		for( S32 i = 0; i < attachments_to_include.count(); i++ )
		{
			S32 attachment_pt = attachments_to_include[i];
			LLViewerJointAttachment* attachment = get_if_there(gAgent.getAvatarObject()->mAttachmentPoints, attachment_pt, (LLViewerJointAttachment*)NULL );
			if(!attachment) continue;
			for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
				 attachment_iter != attachment->mAttachedObjects.end();
				 ++attachment_iter)
 			{
				LLViewerObject *attached_object = (*attachment_iter);
				if (!attached_object) continue;
				const LLUUID& item_id = attached_object->getAttachmentItemID();
				if (item_id.isNull()) continue;
				LLInventoryItem* item = gInventory.getItem(item_id);
				if (!item) continue;
				if (fUseOutfits)
				{
					link_inventory_item(
						gAgent.getID(),
						item->getLinkedUUID(),
						folder_id,
						item->getName(),
						item->getDescription(),
						LLAssetType::AT_LINK,
						LLPointer<LLInventoryCallback>(NULL));
				}
				else
				{
					if (fUseLinks || item->getPermissions().allowCopyBy(gAgent.getID()))
					{
						const LLUUID& old_folder_id = item->getParentUUID();

						move_inventory_item(
							gAgent.getID(),
							gAgent.getSessionID(),
							item->getLinkedUUID(),
							folder_id,
							item->getName(),
							LLPointer<LLInventoryCallback>(NULL));

						if (item->getPermissions().allowCopyBy(gAgent.getID()))
						{
							copy_inventory_item(
								gAgent.getID(),
								item->getPermissions().getOwner(),
								item->getLinkedUUID(),
								old_folder_id,
								item->getName(),
								LLPointer<LLInventoryCallback>(NULL));
						}
					}
					else
					{
						link_inventory_item(
							gAgent.getID(),
							item->getLinkedUUID(),
							folder_id,
							item->getName(),
							item->getDescription(),
							LLAssetType::AT_LINK,
							LLPointer<LLInventoryCallback>(NULL));
					}
				}
			}
		}
	} 
}

void LLAgentWearables::makeNewOutfitDone(S32 index)
{
	LLUUID first_item_id = mWearableEntry[index].mItemID;
	// Open the inventory and select the first item we added.
	if( first_item_id.notNull() )
	{
		LLInventoryView* view = LLInventoryView::getActiveInventory();
		if(view)
		{
			view->getPanel()->setSelection(first_item_id, TAKE_FOCUS_NO);
		}
	}
}


void LLAgentWearables::addWearableToAgentInventory(LLPointer<LLInventoryCallback> cb,
												   LLWearable* wearable,
												   const LLUUID& category_id,
												   BOOL notify)
{
	create_inventory_item(gAgent.getID(),
						  gAgent.getSessionID(),
						  category_id,
						  wearable->getTransactionID(),
						  wearable->getName(),
						  wearable->getDescription(),
						  wearable->getAssetType(),
						  LLInventoryType::IT_WEARABLE,
						  wearable->getType(),
						  wearable->getPermissions().getMaskNextOwner(),
						  cb);
}

void LLAgentWearables::removeWearable( EWearableType type )
{
	LLWearable* old_wearable = mWearableEntry[ type ].mWearable;

	if ( (gAgent.isTeen())
		 && (type == WT_UNDERSHIRT || type == WT_UNDERPANTS))
	{
		// Can't take off underclothing in simple UI mode or on PG accounts
		return;
	}

// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.1.3b)
	if ( (rlv_handler_t::isEnabled()) && (!gRlvWearableLocks.canRemove(type)) )
	{
		return;
	}
// [/RLVa:KB]

	if( old_wearable )
	{
		if( old_wearable->isDirty() )
		{
			LLSD payload;
			payload["wearable_type"] = (S32)type;
			// Bring up view-modal dialog: Save changes? Yes, No, Cancel
			LLNotifications::instance().add("WearableSave", LLSD(), payload, &LLAgentWearables::onRemoveWearableDialog);
			return;
		}
		else
		{
			removeWearableFinal( type );
		}
	}
}

// static 
bool LLAgentWearables::onRemoveWearableDialog(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	EWearableType type = (EWearableType)notification["payload"]["wearable_type"].asInteger();
	switch( option )
	{
	case 0:  // "Save"
		gAgentWearables.saveWearable( type );
		gAgentWearables.removeWearableFinal( type );
		break;

	case 1:  // "Don't Save"
		gAgentWearables.removeWearableFinal( type );
		break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}
	return false;
}

// Called by removeWearable() and onRemoveWearableDialog() to actually do the removal.
void LLAgentWearables::removeWearableFinal( EWearableType type )
{
	LLWearable* old_wearable = mWearableEntry[ type ].mWearable;

	gInventory.addChangedMask( LLInventoryObserver::LABEL, mWearableEntry[type].mItemID );

	mWearableEntry[ type ].mWearable = NULL;
	mWearableEntry[ type ].mItemID.setNull();

	queryWearableCache();

	if( old_wearable )
	{
		old_wearable->removeFromAvatar( TRUE );
	}

	// Update the server
	sendAgentWearablesUpdate(); 
	gAgent.sendAgentSetAppearance();
	gInventory.notifyObservers();
}

void LLAgentWearables::copyWearableToInventory( EWearableType type )
{
	LLWearable* wearable = mWearableEntry[ type ].mWearable;
	if( wearable )
	{
		// Save the old wearable if it has changed.
		if( wearable->isDirty() )
		{
			wearable = gWearableList.createCopyFromAvatar( wearable );
			mWearableEntry[ type ].mWearable = wearable;
		}

		// Make a new entry in the inventory.  (Put it in the same folder as the original item if possible.)
		LLUUID category_id;
		LLInventoryItem* item = gInventory.getItem( mWearableEntry[ type ].mItemID );
		if( item )
		{
			category_id = item->getParentUUID();
			wearable->setPermissions(item->getPermissions());
		}
		LLPointer<LLInventoryCallback> cb =
			new addWearableToAgentInventoryCallback(
				LLPointer<LLRefCount>(NULL),
				type,
				wearable);
		addWearableToAgentInventory(cb, wearable, category_id);
	}
}


// A little struct to let setWearable() communicate more than one value with onSetWearableDialog().
struct LLSetWearableData
{
	LLSetWearableData( const LLUUID& new_item_id, LLWearable* new_wearable ) :
		mNewItemID( new_item_id ), mNewWearable( new_wearable ) {}
	LLUUID				mNewItemID;
	LLWearable*			mNewWearable;
};

static bool isFirstPhysicsWearable(EWearableType type, LLInventoryItem *new_item, LLWearable *new_wearable)
{
	if (type == WT_PHYSICS && gSavedSettings.getWarning("FirstPhysicsWearable"))
	{
		class WearableDelayedCallback
		{
		public:
			static void setDelayedWearable( const LLSD& notification, const LLSD& response, LLUUID item_id, LLWearable *wearable )
			{
				if(LLNotification::getSelectedOption(notification, response) == 0) //User selected wear
				{
					gSavedSettings.setWarning("FirstPhysicsWearable",FALSE);
					LLInventoryItem *item = gInventory.getItem(item_id);
					if(item)
						gAgentWearables.setWearableItem(item,wearable); //re-enter.
				}
			}
		};
		LLNotifications::instance().add("FirstPhysicsWearable",LLSD(),LLSD(),boost::bind(WearableDelayedCallback::setDelayedWearable, _1, _2, new_item->getUUID(),new_wearable));
		return true;
	}
	return false;
}

BOOL LLAgentWearables::needsReplacement(EWearableType  wearableType, S32 remove)
{
	return TRUE;
	/*if (remove) return TRUE;
	
	return getWearable(wearableType) ? TRUE : FALSE;*/
}

// Assumes existing wearables are not dirty.
void LLAgentWearables::setWearableOutfit(const LLInventoryItem::item_array_t& items,
										 const LLDynamicArray< LLWearable* >& wearables,
	BOOL remove )
{
	lldebugs << "setWearableOutfit() start" << llendl;

	BOOL wearables_to_remove[WT_COUNT];
	wearables_to_remove[WT_SHAPE]		= FALSE;
	wearables_to_remove[WT_SKIN]		= FALSE;
	wearables_to_remove[WT_HAIR]		= FALSE;
	wearables_to_remove[WT_EYES]		= FALSE;
// [RLVa:KB] - Checked: 2009-07-06 (RLVa-1.1.3b) | Added: RLVa-0.2.2a
	wearables_to_remove[WT_SHIRT]		= remove && gRlvWearableLocks.canRemove(WT_SHIRT);
	wearables_to_remove[WT_PANTS]		= remove && gRlvWearableLocks.canRemove(WT_PANTS);
	wearables_to_remove[WT_SHOES]		= remove && gRlvWearableLocks.canRemove(WT_SHOES);
	wearables_to_remove[WT_SOCKS]		= remove && gRlvWearableLocks.canRemove(WT_SOCKS);
	wearables_to_remove[WT_JACKET]		= remove && gRlvWearableLocks.canRemove(WT_JACKET);
	wearables_to_remove[WT_GLOVES]		= remove && gRlvWearableLocks.canRemove(WT_GLOVES);
	wearables_to_remove[WT_UNDERSHIRT]	= (!gAgent.isTeen()) && remove && gRlvWearableLocks.canRemove(WT_UNDERSHIRT);
	wearables_to_remove[WT_UNDERPANTS]	= (!gAgent.isTeen()) && remove && gRlvWearableLocks.canRemove(WT_UNDERPANTS);
	wearables_to_remove[WT_SKIRT]		= remove && gRlvWearableLocks.canRemove(WT_SKIRT);
	wearables_to_remove[WT_ALPHA]		= remove && gRlvWearableLocks.canRemove(WT_ALPHA);
	wearables_to_remove[WT_TATTOO]		= remove && gRlvWearableLocks.canRemove(WT_TATTOO);
	wearables_to_remove[WT_PHYSICS]		= remove && gRlvWearableLocks.canRemove(WT_PHYSICS);
// [/RLVa:KB]

	S32 count = wearables.count();
	llassert( items.count() == count );

	S32 i;
	for( i = 0; i < count; i++ )
	{
		LLWearable* new_wearable = wearables[i];
		LLPointer<LLInventoryItem> new_item = items[i];

		llassert(new_wearable);
		EWearableType type = new_wearable->getType();
		wearables_to_remove[type] = FALSE;

		LLWearable* old_wearable = mWearableEntry[ type ].mWearable;
		if( old_wearable )
		{
			const LLUUID& old_item_id = mWearableEntry[ type ].mItemID;
			if( (old_wearable->getID() == new_wearable->getID()) &&
				(old_item_id == new_item->getUUID()) )
			{
				lldebugs << "No change to wearable asset and item: " << LLWearable::typeToTypeName( type ) << llendl;
				continue;
			}

			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);

			// Assumes existing wearables are not dirty.
			if( old_wearable->isDirty() )
			{
				llassert(0);
				continue;
			}
		}

		if (isFirstPhysicsWearable(type, new_item, new_wearable))
		{
			return;
		}

		mWearableEntry[ type ].mItemID = new_item->getUUID();
		mWearableEntry[ type ].mWearable = new_wearable;
	}

	std::vector<LLWearable*> wearables_being_removed;

	for( i = 0; i < WT_COUNT; i++ )
	{
		if( wearables_to_remove[i] )
		{
			wearables_being_removed.push_back(mWearableEntry[ i ].mWearable);
			mWearableEntry[ i ].mWearable = NULL;
			
			gInventory.addChangedMask(LLInventoryObserver::LABEL, mWearableEntry[ i ].mItemID);
			mWearableEntry[ i ].mItemID.setNull();
		}
	}

	gInventory.notifyObservers();

	queryWearableCache();

	std::vector<LLWearable*>::iterator wearable_iter;

	for( wearable_iter = wearables_being_removed.begin(); 
		wearable_iter != wearables_being_removed.end();
		++wearable_iter)
	{
		LLWearable* wearablep = *wearable_iter;
		if (wearablep)
		{
			wearablep->removeFromAvatar( TRUE );
		}
	}

	for( i = 0; i < count; i++ )
	{
		wearables[i]->writeToAvatar( TRUE );
	}

	// Start rendering & update the server
	mWearablesLoaded = TRUE; 
	sendAgentWearablesUpdate();
	gAgent.sendAgentSetAppearance();

	lldebugs << "setWearableOutfit() end" << llendl;
}


// User has picked "wear on avatar" from a menu.
void LLAgentWearables::setWearableItem( LLInventoryItem* new_item, LLWearable* new_wearable )
{
	EWearableType type = new_wearable->getType();

	LLWearable* old_wearable = mWearableEntry[ type ].mWearable;

// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.0.0d)
	// Block if: we can't wear on that layer; or we're already wearing something there we can't take off
	if ( (rlv_handler_t::isEnabled()) && (!gRlvWearableLocks.canWear(type)) )
	{
		return;
	}
// [/RLVa:KB]

	if (isFirstPhysicsWearable(type, new_item, new_wearable))
	{
		return;
	}

	if( old_wearable )
	{
		const LLUUID& old_item_id = mWearableEntry[ type ].mItemID;
		if( (old_wearable->getID() == new_wearable->getID()) &&
			(old_item_id == new_item->getUUID()) )
		{
			lldebugs << "No change to wearable asset and item: " << LLWearable::typeToTypeName( type ) << llendl;
			return;
		}

		if( old_wearable->isDirty() )
		{
			// Bring up modal dialog: Save changes? Yes, No, Cancel
			LLSD payload;
			payload["item_id"] = new_item->getUUID();
			LLNotifications::instance().add( "WearableSave", LLSD(), payload, boost::bind(LLAgentWearables::onSetWearableDialog, _1, _2, new_wearable));
			return;
		}
	}

	setWearableFinal( new_item, new_wearable );
}

// static 
bool LLAgentWearables::onSetWearableDialog( const LLSD& notification, const LLSD& response, LLWearable* wearable )
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLInventoryItem* new_item = gInventory.getItem( notification["payload"]["item_id"].asUUID());
	if( !new_item )
	{
		delete wearable;
		return false;
	}

	switch( option )
	{
	case 0:  // "Save"
		gAgentWearables.saveWearable( wearable->getType() );
		gAgentWearables.setWearableFinal( new_item, wearable );
		break;

	case 1:  // "Don't Save"
		gAgentWearables.setWearableFinal( new_item, wearable );
		break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}

	delete wearable;
	return false;
}

// Called from setWearable() and onSetWearableDialog() to actually set the wearable.
void LLAgentWearables::setWearableFinal( LLInventoryItem* new_item, LLWearable* new_wearable )
{
	const EWearableType type = new_wearable->getType();

	// Replace the old wearable with a new one.
	llassert( new_item->getAssetUUID() == new_wearable->getID() );
	LLUUID old_item_id = mWearableEntry[ type ].mItemID;
	mWearableEntry[ type ].mItemID = new_item->getUUID();
	mWearableEntry[ type ].mWearable = new_wearable;

	if (old_item_id.notNull())
	{
		gInventory.addChangedMask( LLInventoryObserver::LABEL, old_item_id );
		gInventory.notifyObservers();
	}

	//llinfos << "LLVOAvatar::setWearable()" << llendl;
	queryWearableCache();
	new_wearable->writeToAvatar( TRUE );

	// Update the server
	sendAgentWearablesUpdate();
	gAgent.sendAgentSetAppearance();
}

void LLAgentWearables::queryWearableCache()
{
	if (!mWearablesLoaded)
	{
		return;
	}

	// Look up affected baked textures.
	// If they exist:
	//		disallow updates for affected layersets (until dataserver responds with cache request.)
	//		If cache miss, turn updates back on and invalidate composite.
	//		If cache hit, modify baked texture entries.
	//
	// Cache requests contain list of hashes for each baked texture entry.
	// Response is list of valid baked texture assets. (same message)

	gMessageSystem->newMessageFast(_PREHASH_AgentCachedTexture);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->addS32Fast(_PREHASH_SerialNum, gAgentQueryManager.mWearablesCacheQueryID);

	S32 num_queries = 0;
	for (U8 baked_index = 0; baked_index < BAKED_NUM_INDICES; baked_index++ )
	{
		const LLVOAvatarDictionary::WearableDictionaryEntry *wearable_dict = LLVOAvatarDictionary::getInstance()->getWearable((EBakedTextureIndex)baked_index);
		LLUUID hash;
		for (U8 i=0; i < wearable_dict->mWearablesVec.size(); i++)
		{
			// EWearableType wearable_type = gBakedWearableMap[baked_index][wearable_num];
			const EWearableType wearable_type = wearable_dict->mWearablesVec[i];
			const LLWearable* wearable = getWearable(wearable_type);
			if (wearable)
			{
				hash ^= wearable->getID();
			}
		}
		if (hash.notNull())
		{
			hash ^= wearable_dict->mHashID;
			num_queries++;
			// *NOTE: make sure at least one request gets packed

			//llinfos << "Requesting texture for hash " << hash << " in baked texture slot " << baked_index << llendl;
			gMessageSystem->nextBlockFast(_PREHASH_WearableData);
			gMessageSystem->addUUIDFast(_PREHASH_ID, hash);
			gMessageSystem->addU8Fast(_PREHASH_TextureIndex, (U8)baked_index);
		}

		gAgentQueryManager.mActiveCacheQueries[ baked_index ] = gAgentQueryManager.mWearablesCacheQueryID;
	}
	//VWR-22113: gAgent.getRegion() can return null if invalid, seen here on logout
	if(gAgent.getRegion())
	{
		llinfos << "Requesting texture cache entry for " << num_queries << " baked textures" << llendl;
		gMessageSystem->sendReliable(gAgent.getRegion()->getHost());
		gAgentQueryManager.mNumPendingQueries++;
		gAgentQueryManager.mWearablesCacheQueryID++;
	}
}

// User has picked "remove from avatar" from a menu.
// static
void LLAgentWearables::userRemoveWearable( void* userdata )
{
	EWearableType type = (EWearableType)(intptr_t)userdata;
	
	if( !(type==WT_SHAPE || type==WT_SKIN || type==WT_HAIR || type==WT_EYES) ) //&&
		//!((!gAgent.isTeen()) && ( type==WT_UNDERPANTS || type==WT_UNDERSHIRT )) )
	{
		gAgentWearables.removeWearable( type );
	}
}

void LLAgentWearables::userRemoveAllClothes( void* userdata )
{
	// We have to do this up front to avoid having to deal with the case of multiple wearables being dirty.
	if (gAgentCamera.cameraCustomizeAvatar())
	{
		gFloaterCustomize->askToSaveIfDirty( LLAgentWearables::userRemoveAllClothesStep2, NULL );
	}
	else
	{
		userRemoveAllClothesStep2( TRUE, NULL );
	}
}

void LLAgentWearables::userRemoveAllClothesStep2( BOOL proceed, void* userdata )
{
	if( proceed )
	{
		gAgentWearables.removeWearable( WT_SHIRT );
		gAgentWearables.removeWearable( WT_PANTS );
		gAgentWearables.removeWearable( WT_SHOES );
		gAgentWearables.removeWearable( WT_SOCKS );
		gAgentWearables.removeWearable( WT_JACKET );
		gAgentWearables.removeWearable( WT_GLOVES );
		gAgentWearables.removeWearable( WT_UNDERSHIRT );
		gAgentWearables.removeWearable( WT_UNDERPANTS );
		gAgentWearables.removeWearable( WT_SKIRT );
		gAgentWearables.removeWearable( WT_ALPHA );
		gAgentWearables.removeWearable( WT_TATTOO );
		gAgentWearables.removeWearable( WT_PHYSICS );
	}
}

// Combines userRemoveAllAttachments() and userAttachMultipleAttachments() logic to
// get attachments into desired state with minimal number of adds/removes.
//void LLAgentWearables::userUpdateAttachments(LLInventoryModel::item_array_t& obj_item_array)
// [SL:KB] - Patch: Appearance-SyncAttach | Checked: 2010-09-22 (Catznip-2.2.0a) | Added: Catznip-2.2.0a
void LLAgentWearables::userUpdateAttachments(LLInventoryModel::item_array_t& obj_item_array, bool fAttachOnly)
// [/SL:KB]
{
	// Possible cases:
	// already wearing but not in request set -> take off.
	// already wearing and in request set -> leave alone.
	// not wearing and in request set -> put on.

	LLVOAvatar* pAvatar = gAgent.getAvatarObject();
	if (!pAvatar) return;

	std::set<LLUUID> requested_item_ids;
	std::set<LLUUID> current_item_ids;
	for (S32 i=0; i<obj_item_array.count(); i++)
		requested_item_ids.insert(obj_item_array[i].get()->getLinkedUUID());

	// Build up list of objects to be removed and items currently attached.
	llvo_vec_t objects_to_remove;
	for (LLVOAvatar::attachment_map_t::iterator iter = pAvatar->mAttachmentPoints.begin(); 
		 iter != pAvatar->mAttachmentPoints.end();)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
			 attachment_iter != attachment->mAttachedObjects.end();
			 ++attachment_iter)
		{
			LLViewerObject *objectp = (*attachment_iter);
			if (objectp)
			{
				LLUUID object_item_id = objectp->getAttachmentItemID();
				if (requested_item_ids.find(object_item_id) != requested_item_ids.end())
				{
					// Object currently worn, was requested.
					// Flag as currently worn so we won't have to add it again.
					current_item_ids.insert(object_item_id);
				}
				else
				{
					// object currently worn, not requested.
					objects_to_remove.push_back(objectp);
				}
			}
		}
	}

	LLInventoryModel::item_array_t items_to_add;
	for (LLInventoryModel::item_array_t::iterator it = obj_item_array.begin();
		 it != obj_item_array.end();
		 ++it)
	{
		LLUUID linked_id = (*it).get()->getLinkedUUID();
		if (current_item_ids.find(linked_id) != current_item_ids.end())
		{
			// Requested attachment is already worn.
		}
		else
		{
			// Requested attachment is not worn yet.
			items_to_add.push_back(*it);
		}
	}
	// S32 remove_count = objects_to_remove.size();
	// S32 add_count = items_to_add.size();
	// llinfos << "remove " << remove_count << " add " << add_count << llendl;

	// Remove everything in objects_to_remove
//	userRemoveMultipleAttachments(objects_to_remove);
// [SL:KB] - Patch: Appearance-SyncAttach | Checked: 2010-09-22 (Catznip-2.2.0a) | Added: Catznip-2.2.0a
	if (!fAttachOnly)
	{
		userRemoveMultipleAttachments(objects_to_remove);
	}
// [/SL:KB]

	// Add everything in items_to_add
	userAttachMultipleAttachments(items_to_add);
}

void LLAgentWearables::userRemoveMultipleAttachments(llvo_vec_t& objects_to_remove)
{
	if (!gAgent.getAvatarObject()) return;

// [RLVa:KB] - Checked: 2010-03-04 (RLVa-1.1.3b) | Modified: RLVa-1.2.0a
	// RELEASE-RLVa: [SL-2.0.0] Check our callers and verify that erasing elements from the passed vector won't break random things
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_REMOVE)) )
	{
		llvo_vec_t::iterator itObj = objects_to_remove.begin();
		while (itObj != objects_to_remove.end())
		{
			const LLViewerObject* pAttachObj = *itObj;
			if (gRlvAttachmentLocks.isLockedAttachment(pAttachObj))
			{
				itObj = objects_to_remove.erase(itObj);

				// Fall-back code: re-add the attachment if it got removed from COF somehow (compensates for possible bugs elsewhere)
				LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
				LLLinkedItemIDMatches f(pAttachObj->getAttachmentItemID());
				gInventory.collectDescendentsIf(LLCOFMgr::instance().getCOF(), folders, items, LLInventoryModel::EXCLUDE_TRASH, f);
				RLV_ASSERT( 0 != items.count() );
				if (0 == items.count())
					LLCOFMgr::instance().addAttachment(pAttachObj->getAttachmentItemID());
			}
			else
			{
				++itObj;
			}
		}
	}
// [/RLVa:KB]

	if (objects_to_remove.empty())
		return;

	gMessageSystem->newMessage("ObjectDetach");
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	
	for (llvo_vec_t::iterator it = objects_to_remove.begin();
		 it != objects_to_remove.end();
		 ++it)
	{
		LLViewerObject *objectp = *it;
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
	}
	gMessageSystem->sendReliable(gAgent.getRegionHost());
}

void LLAgentWearables::userRemoveAllAttachments( void* userdata )
{
	LLVOAvatar* avatarp = gAgent.getAvatarObject();
	if(!avatarp)
	{
		llwarns << "No avatar found." << llendl;
		return;
	}

	llvo_vec_t objects_to_remove;
	
	for (LLVOAvatar::attachment_map_t::iterator iter = avatarp->mAttachmentPoints.begin(); 
		 iter != avatarp->mAttachmentPoints.end();)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
			 attachment_iter != attachment->mAttachedObjects.end();
			 ++attachment_iter)
		{
			LLViewerObject *attached_object = (*attachment_iter);
//			if (attached_object)
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.1.3b) | Modified: RLVa-1.1.3b
			if ( (attached_object) && ((!rlv_handler_t::isEnabled()) || (!gRlvAttachmentLocks.isLockedAttachment(attached_object))) )
// [/RLVa:KB]
			{
				objects_to_remove.push_back(attached_object);
			}
		}
	}
	userRemoveMultipleAttachments(objects_to_remove);
}

void LLAgentWearables::userAttachMultipleAttachments(LLInventoryModel::item_array_t& obj_item_array)
{
// [RLVa:KB] - Checked: 2010-03-04 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
	// RELEASE-RLVa: [SL-2.0.0] Check our callers and verify that erasing elements from the passed vector won't break random things
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_ANY)) )
	{
		// Fall-back code: everything should really already have been pruned before we get this far
		for (S32 idxItem = obj_item_array.count() - 1; idxItem >= 0; idxItem--)
		{
			const LLInventoryItem* pItem = obj_item_array.get(idxItem).get();
			if (!gRlvAttachmentLocks.canAttach(pItem))
			{
				obj_item_array.remove(idxItem);
				RLV_ASSERT(false);
			}
		}
	}
// [/RLVa:KB]

	// Build a compound message to send all the objects that need to be rezzed.
	S32 obj_count = obj_item_array.count();

	// Limit number of packets to send
	const S32 MAX_PACKETS_TO_SEND = 10;
	const S32 OBJECTS_PER_PACKET = 4;
	const S32 MAX_OBJECTS_TO_SEND = MAX_PACKETS_TO_SEND * OBJECTS_PER_PACKET;
	if( obj_count > MAX_OBJECTS_TO_SEND )
	{
		obj_count = MAX_OBJECTS_TO_SEND;
	}
				
	// Create an id to keep the parts of the compound message together
	LLUUID compound_msg_id;
	compound_msg_id.generate();
	LLMessageSystem* msg = gMessageSystem;

	for(S32 i = 0; i < obj_count; ++i)
	{
		if( 0 == (i % OBJECTS_PER_PACKET) )
		{
			// Start a new message chunk
			msg->newMessageFast(_PREHASH_RezMultipleAttachmentsFromInv);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->nextBlockFast(_PREHASH_HeaderData);
			msg->addUUIDFast(_PREHASH_CompoundMsgID, compound_msg_id );
			msg->addU8Fast(_PREHASH_TotalObjects, obj_count );
			msg->addBOOLFast(_PREHASH_FirstDetachAll, false );
		}

		const LLInventoryItem* item = obj_item_array.get(i).get();
		bool replace = !gHippoGridManager->getConnectedGrid()->supportsInvLinks();
		msg->nextBlockFast(_PREHASH_ObjectData );
		msg->addUUIDFast(_PREHASH_ItemID, item->getLinkedUUID());
		msg->addUUIDFast(_PREHASH_OwnerID, item->getPermissions().getOwner());
		msg->addU8Fast(_PREHASH_AttachmentPt, replace? 0 : ATTACHMENT_ADD);	// Wear at the previous or default attachment point
		pack_permissions_slam(msg, item->getFlags(), item->getPermissions());
		msg->addStringFast(_PREHASH_Name, item->getName());
		msg->addStringFast(_PREHASH_Description, item->getDescription());

		if( (i+1 == obj_count) || ((OBJECTS_PER_PACKET-1) == (i % OBJECTS_PER_PACKET)) )
		{
			// End of message chunk
			msg->sendReliable( gAgent.getRegion()->getHost() );
		}
	}
}
