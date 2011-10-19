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

#ifndef RLV_VIEWER2_H
#define RLV_VIEWER2_H

#include "llcallbacklist.h"
#include "llinventorymodel.h"

#include "boost/function.hpp"

// ============================================================================
// From llinventoryobserver.h

class LLInventoryFetchItemsObserver : public LLInventoryObserver
{
public:
	LLInventoryFetchItemsObserver(const LLUUID& item_id = LLUUID::null); 
	LLInventoryFetchItemsObserver(const uuid_vec_t& item_ids); 

	BOOL isFinished() const;

	virtual void startFetch();
	virtual void changed(U32 mask);
	virtual void done() {};
protected:
	uuid_vec_t mComplete;
	uuid_vec_t mIncomplete;
	uuid_vec_t mIDs;
private:
	LLTimer mFetchingPeriod;

	/**
	 * Period of waiting a notification when requested items get added into inventory.
	 */
	static const F32 FETCH_TIMER_EXPIRY;
};

// ============================================================================
// From llinventoryfunctions.cpp

void change_item_parent(LLInventoryModel* model, LLViewerInventoryItem* item, const LLUUID& new_parent_id, BOOL restamp);
void change_category_parent(LLInventoryModel* model, LLViewerInventoryCategory* cat, const LLUUID& new_parent_id, BOOL restamp);

// ============================================================================

#endif // RLV_VIEWER2_H
