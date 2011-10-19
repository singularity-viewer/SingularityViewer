/** 
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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
#include "llagent.h"
#include "llviewerinventory.h"
#include "rlvviewer2.h"

// ============================================================================
// From llinventoryobserver.cpp

void fetch_items_from_llsd(const LLSD& items_llsd);

const F32 LLInventoryFetchItemsObserver::FETCH_TIMER_EXPIRY = 60.0f;

LLInventoryFetchItemsObserver::LLInventoryFetchItemsObserver(const LLUUID& item_id)
{
	mIDs.clear();
	if (item_id != LLUUID::null)
	{
		mIDs.push_back(item_id);
	}
}

LLInventoryFetchItemsObserver::LLInventoryFetchItemsObserver(const uuid_vec_t& item_ids)
	: mIDs(item_ids)
{
}

BOOL LLInventoryFetchItemsObserver::isFinished() const
{
	return mIncomplete.empty();
}

void LLInventoryFetchItemsObserver::changed(U32 mask)
{
	lldebugs << this << " remaining incomplete " << mIncomplete.size()
			 << " complete " << mComplete.size()
			 << " wait period " << mFetchingPeriod.getRemainingTimeF32()
			 << llendl;

	// scan through the incomplete items and move or erase them as
	// appropriate.
	if (!mIncomplete.empty())
	{
		// Have we exceeded max wait time?
		bool timeout_expired = mFetchingPeriod.hasExpired();

		for (uuid_vec_t::iterator it = mIncomplete.begin(); it < mIncomplete.end(); )
		{
			const LLUUID& item_id = (*it);
			LLViewerInventoryItem* item = gInventory.getItem(item_id);
			if (item && item->isComplete())
			{
				mComplete.push_back(item_id);
				it = mIncomplete.erase(it);
			}
			else
			{
				if (timeout_expired)
				{
					// Just concede that this item hasn't arrived in reasonable time and continue on.
					llwarns << "Fetcher timed out when fetching inventory item UUID: " << item_id << LL_ENDL;
					it = mIncomplete.erase(it);
				}
				else
				{
					// Keep trying.
					++it;
				}
			}
		}

	}

	if (mIncomplete.empty())
	{
		lldebugs << this << " done at remaining incomplete "
				 << mIncomplete.size() << " complete " << mComplete.size() << llendl;
		done();
	}
	//llinfos << "LLInventoryFetchItemsObserver::changed() mComplete size " << mComplete.size() << llendl;
	//llinfos << "LLInventoryFetchItemsObserver::changed() mIncomplete size " << mIncomplete.size() << llendl;
}

void LLInventoryFetchItemsObserver::startFetch()
{
	LLUUID owner_id;
	LLSD items_llsd;
	for (uuid_vec_t::const_iterator it = mIDs.begin(); it < mIDs.end(); ++it)
	{
		LLViewerInventoryItem* item = gInventory.getItem(*it);
		if (item)
		{
			if (item->isComplete())
			{
				// It's complete, so put it on the complete container.
				mComplete.push_back(*it);
				continue;
			}
			else
			{
				owner_id = item->getPermissions().getOwner();
			}
		}
		else
		{
			// assume it's agent inventory.
			owner_id = gAgent.getID();
		}

		// Ignore categories since they're not items.  We
		// could also just add this to mComplete but not sure what the
		// side-effects would be, so ignoring to be safe.
		LLViewerInventoryCategory* cat = gInventory.getCategory(*it);
		if (cat)
		{
			continue;
		}

		// It's incomplete, so put it on the incomplete container, and
		// pack this on the message.
		mIncomplete.push_back(*it);
		
		// Prepare the data to fetch
		LLSD item_entry;
		item_entry["owner_id"] = owner_id;
		item_entry["item_id"] = (*it);
		items_llsd.append(item_entry);
	}

	mFetchingPeriod.reset();
	mFetchingPeriod.setTimerExpirySec(FETCH_TIMER_EXPIRY);

	fetch_items_from_llsd(items_llsd);
}

// ============================================================================
// From llinventoryfunctions.cpp

void change_item_parent(LLInventoryModel* model, LLViewerInventoryItem* item, const LLUUID& new_parent_id, BOOL restamp)
{
	if (item->getParentUUID() != new_parent_id)
	{
		LLInventoryModel::update_list_t update;
		LLInventoryModel::LLCategoryUpdate old_folder(item->getParentUUID(),-1);
		update.push_back(old_folder);
		LLInventoryModel::LLCategoryUpdate new_folder(new_parent_id, 1);
		update.push_back(new_folder);
		gInventory.accountForUpdate(update);

		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setParent(new_parent_id);
		new_item->updateParentOnServer(restamp);
		model->updateItem(new_item);
		model->notifyObservers();
	}
}

void change_category_parent(LLInventoryModel* model, LLViewerInventoryCategory* cat, const LLUUID& new_parent_id, BOOL restamp)
{
	if (!model || !cat)
	{
		return;
	}

	// Can't move a folder into a child of itself.
	if (model->isObjectDescendentOf(new_parent_id, cat->getUUID()))
	{
		return;
	}

	LLInventoryModel::update_list_t update;
	LLInventoryModel::LLCategoryUpdate old_folder(cat->getParentUUID(), -1);
	update.push_back(old_folder);
	LLInventoryModel::LLCategoryUpdate new_folder(new_parent_id, 1);
	update.push_back(new_folder);
	model->accountForUpdate(update);

	LLPointer<LLViewerInventoryCategory> new_cat = new LLViewerInventoryCategory(cat);
	new_cat->setParent(new_parent_id);
	new_cat->updateParentOnServer(restamp);
	model->updateCategory(new_cat);
	model->notifyObservers();
}

// ============================================================================
