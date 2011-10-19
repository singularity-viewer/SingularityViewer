/** 
 * @file llpanelinventory.h
 * @brief LLPanelInventory class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

//*****************************************************************************
//
// This class represents the panel used to view and control a
// particular task's inventory.
//
//*****************************************************************************

#ifndef LL_LLPANELINVENTORY_H
#define LL_LLPANELINVENTORY_H

#include "llinventory.h"
#include "lluuid.h"
#include "llmap.h"
#include "llviewerobject.h"
#include "llvoinventorylistener.h"
#include "llpanel.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPanelInventory
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLScrollableContainerView;
class LLFolderView;
class LLFolderViewFolder;
class LLViewerObject;
class LLFolderViewEventListener;
//class LLVOInventoryListener;

// Utility function to hide all entries except those in the list
class LLMenuGL;
void hide_context_entries(LLMenuGL& menu, 
		const std::vector<std::string> &entries_to_show, 
		const std::vector<std::string> &disabled_entries);

class LLPanelInventory : public LLPanel, public LLVOInventoryListener
{
protected:
	LLScrollableContainerView* mScroller;
	LLFolderView* mFolders;
	
	LLUUID mTaskUUID;
	BOOL mHaveInventory;
	BOOL mIsInventoryEmpty;
	BOOL mInventoryNeedsUpdate;

protected:
	void reset();
	/*virtual*/ void inventoryChanged(LLViewerObject* object,
								 LLInventoryObject::object_list_t* inventory,
								 S32 serial_num,
								 void* user_data);
	void updateInventory();
	void createFolderViews(LLInventoryObject* inventory_root, LLInventoryObject::object_list_t& contents);
	void createViewsForCategory(LLInventoryObject::object_list_t* inventory,
								LLInventoryObject* parent,
								LLFolderViewFolder* folder);

	void clearContents();

public:
	LLPanelInventory(const std::string& name, const LLRect& rect);
	virtual ~LLPanelInventory();

	void refresh();
	const LLUUID& getTaskUUID() { return mTaskUUID;}
	void removeSelectedItem();
	void startRenamingSelectedItem();

	LLFolderView* getRootFolder() const { return mFolders; }

	virtual void draw();
	virtual void deleteAllChildren();
	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void *cargo_data, EAcceptance *accept, std::string& tooltip_msg);
	
	static void idle(void* user_data);
};

void init_object_inventory_panel_actions(LLPanelInventory *panel);

#endif // LL_LLPANELINVENTORY_H
