/**
 * @file llpanelinventory.cpp
 * @brief LLPanelInventory class implementation
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
// Implementation of the panel inventory - used to view and control a
// task's inventory.
//
//*****************************************************************************

#include "llviewerprecompiledheaders.h"

#include <sstream> // for std::ostringstream
#include <utility> // for std::pair<>

#include "stdenums.h"
#include "llpanelinventory.h"

#include "message.h"
#include "lldarray.h"
#include "llfontgl.h"
#include "llassetstorage.h"
#include "llinventory.h"
#include "llinventorybridge.h"
#include "llinventorydefines.h"
#include "llinventoryicon.h"
#include "llnotificationsutil.h"

#include "llagent.h"
#include "llcallbacklist.h"
#include "llfocusmgr.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterproperties.h"
#include "llfolderview.h"
#include "llgl.h"
#include "llinventorymodel.h"
#include "llinventoryicon.h"
#include "llinventorybridge.h"
#include "llmenugl.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "roles_constants.h"
#include "llscrollcontainer.h"
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llwearable.h"
#include "llviewerassettype.h"
// [RLVa:KB] - Checked: 2010-03-27 (RLVa-1.2.0b)
#include "rlvhandler.h"
// [/RLVa:KB]

#include "hippogridmanager.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------


///----------------------------------------------------------------------------
/// Class LLTaskInvFVBridge
///----------------------------------------------------------------------------

class LLTaskInvFVBridge : public LLFolderViewEventListener
{
protected:
	LLUUID mUUID;
	std::string mName;
	mutable std::string mDisplayName;
	LLPanelInventory* mPanel;
	U32 mFlags;

	LLInventoryItem* findItem() const;

public:
	LLTaskInvFVBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name,
		U32 flags=0);
	virtual ~LLTaskInvFVBridge( void ) {}

	virtual LLFontGL::StyleFlags getLabelStyle() const { return LLFontGL::NORMAL; }
	virtual std::string getLabelSuffix() const { return LLStringUtil::null; }

	static LLTaskInvFVBridge* createObjectBridge(LLPanelInventory* panel,
												 LLInventoryObject* object);
	void showProperties();
	void buyItem();
	S32 getPrice();
	static bool commitBuyItem(const LLSD& notification, const LLSD& response);

	// LLFolderViewEventListener functionality
	virtual const std::string& getName() const;
	virtual const std::string& getDisplayName() const;
	virtual PermissionMask getPermissionMask() const { return PERM_NONE; }
	virtual const LLUUID& getUUID() const { return mUUID; }
	virtual time_t getCreationDate() const;
	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual void previewItem();
	virtual void selectItem() {}
	virtual BOOL isItemRenameable() const;
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL isItemMovable();
	virtual BOOL isItemRemovable();
	virtual BOOL removeItem();
	virtual void removeBatch(LLDynamicArray<LLFolderViewEventListener*>& batch);
	virtual void move(LLFolderViewEventListener* parent_listener);
	virtual BOOL isItemCopyable() const;
	virtual BOOL copyToClipboard() const;
	virtual BOOL cutToClipboard() const;
	virtual BOOL isClipboardPasteable() const;
	virtual void pasteFromClipboard();
	virtual void pasteLinkFromClipboard();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual BOOL isUpToDate() const { return TRUE; }
	virtual BOOL hasChildren() const { return FALSE; }
	virtual LLInventoryType::EType getInventoryType() const { return LLInventoryType::IT_NONE; }
	// LLDragAndDropBridge functionality
	virtual BOOL startDrag(EDragAndDropType* type, LLUUID* id) const;
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);
//	virtual void dropped();

};

LLTaskInvFVBridge::LLTaskInvFVBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name,
	U32 flags):
	mUUID(uuid),
	mName(name),
	mPanel(panel),
	mFlags(flags)
{

}

LLInventoryItem* LLTaskInvFVBridge::findItem() const
{
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(object)
	{
		return (LLInventoryItem*)(object->getInventoryObject(mUUID));
	}
	return NULL;
}

void LLTaskInvFVBridge::showProperties()
{
	if(!LLFloaterProperties::show(mUUID, mPanel->getTaskUUID()))
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PropertiesRect");
		rect.translate( left - rect.mLeft, top - rect.mTop );
		LLFloaterProperties* floater = new LLFloaterProperties("obj item properties",
													   rect,
													   "Object Inventory Item Properties",
													   mUUID,
													   mPanel->getTaskUUID());
		floater->open();		/*Flawfinder: ignore*/
	}
}

struct LLBuyInvItemData
{
	LLUUID mTaskID;
	LLUUID mItemID;
	LLAssetType::EType mType;

	LLBuyInvItemData(const LLUUID& task,
					 const LLUUID& item,
					 LLAssetType::EType type) :
		mTaskID(task), mItemID(item), mType(type)
	{}
};

void LLTaskInvFVBridge::buyItem()
{
	llinfos << "LLTaskInvFVBridge::buyItem()" << llendl;
	LLInventoryItem* item = findItem();
	if(!item || !item->getSaleInfo().isForSale()) return;
	LLBuyInvItemData* inv = new LLBuyInvItemData(mPanel->getTaskUUID(),
												 mUUID,
												 item->getType());

	const LLSaleInfo& sale_info = item->getSaleInfo();
	const LLPermissions& perm = item->getPermissions();
	const std::string owner_name; // no owner name currently... FIXME?

	LLViewerObject* obj;
	if( ( obj = gObjectList.findObject( mPanel->getTaskUUID() ) ) && obj->isAttachment() )
	{
		LLNotificationsUtil::add("Cannot_Purchase_an_Attachment");
		llinfos << "Attempt to purchase an attachment" << llendl;
		delete inv;
	}
	else
	{
        LLSD args;
        args["PRICE"] = llformat("%d",sale_info.getSalePrice());
        args["CURRENCY"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
        args["OWNER"] = owner_name;
        if (sale_info.getSaleType() != LLSaleInfo::FS_CONTENTS)
        {
        	U32 next_owner_mask = perm.getMaskNextOwner();
        	args["MODIFYPERM"] = LLNotifications::instance().getGlobalString((next_owner_mask & PERM_MODIFY) ? "PermYes" : "PermNo");
        	args["COPYPERM"] = LLNotifications::instance().getGlobalString((next_owner_mask & PERM_COPY) ? "PermYes" : "PermNo");
        	args["RESELLPERM"] = LLNotifications::instance().getGlobalString((next_owner_mask & PERM_TRANSFER) ? "PermYes" : "PermNo");
        }

		std::string alertdesc;
       	switch(sale_info.getSaleType())
       	{
       	  case LLSaleInfo::FS_ORIGINAL:
       		alertdesc = owner_name.empty() ? "BuyOriginalNoOwner" : "BuyOriginal";
       		break;
       	  case LLSaleInfo::FS_CONTENTS:
       		alertdesc = owner_name.empty() ? "BuyContentsNoOwner" : "BuyContents";
       		break;
		  case LLSaleInfo::FS_COPY:
       	  default:
       		alertdesc = owner_name.empty() ? "BuyCopyNoOwner" : "BuyCopy";
       		break;
       	}

		LLSD payload;
		payload["task_id"] = inv->mTaskID;
		payload["item_id"] = inv->mItemID;
		payload["type"] = inv->mType;
		LLNotifications::instance().add(alertdesc, args, payload, LLTaskInvFVBridge::commitBuyItem);
	}
}

S32 LLTaskInvFVBridge::getPrice()
{
	LLInventoryItem* item = findItem();
	if(item)
	{
		return item->getSaleInfo().getSalePrice();
	}
	else
	{
		return -1;
	}
}

// static
bool LLTaskInvFVBridge::commitBuyItem(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if(0 == option)
	{
		LLViewerObject* object = gObjectList.findObject(notification["payload"]["task_id"].asUUID());
		if(!object || !object->getRegion()) return false;


		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_BuyObjectInventory);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_Data);
		msg->addUUIDFast(_PREHASH_ObjectID, notification["payload"]["task_id"].asUUID());
		msg->addUUIDFast(_PREHASH_ItemID, notification["payload"]["item_id"].asUUID());
		msg->addUUIDFast(_PREHASH_FolderID,
			gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType((LLAssetType::EType)notification["payload"]["type"].asInteger())));
		msg->sendReliable(object->getRegion()->getHost());
	}
	return false;
}

const std::string& LLTaskInvFVBridge::getName() const
{
	return mName;
}

const std::string& LLTaskInvFVBridge::getDisplayName() const
{
	LLInventoryItem* item = findItem();
	if(item)
	{
		mDisplayName.assign(item->getName());

		const LLPermissions& perm(item->getPermissions());
		BOOL copy = gAgent.allowOperation(PERM_COPY, perm, GP_OBJECT_MANIPULATE);
		BOOL mod  = gAgent.allowOperation(PERM_MODIFY, perm, GP_OBJECT_MANIPULATE);
		BOOL xfer = gAgent.allowOperation(PERM_TRANSFER, perm, GP_OBJECT_MANIPULATE);

		if(!copy)
		{
			mDisplayName.append(" (no copy)");
		}
		if(!mod)
		{
			mDisplayName.append(" (no modify)");
		}
		if(!xfer)
		{
			mDisplayName.append(" (no transfer)");
		}
	}

	return mDisplayName;
}

// BUG: No creation dates for task inventory
time_t LLTaskInvFVBridge::getCreationDate() const
{
	return 0;
}

LLUIImagePtr LLTaskInvFVBridge::getIcon() const
{
	BOOL item_is_multi = FALSE;
	if ( mFlags & LLInventoryItemFlags::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS )
	{
		item_is_multi = TRUE;
	}

	return LLInventoryIcon::getIcon(LLAssetType::AT_OBJECT, LLInventoryType::IT_OBJECT, 0, item_is_multi );
}

void LLTaskInvFVBridge::openItem()
{
	// no-op.
	lldebugs << "LLTaskInvFVBridge::openItem()" << llendl;
}

void LLTaskInvFVBridge::previewItem()
{
	openItem();
}

BOOL LLTaskInvFVBridge::isItemRenameable() const
{
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
	{
		return FALSE;
	}
// [/RLVa:KB]

	if(gAgent.isGodlike()) return TRUE;
//	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(object)
	{
		LLInventoryItem* item;
		item = (LLInventoryItem*)(object->getInventoryObject(mUUID));
		if(item && gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
										 GP_OBJECT_MANIPULATE, GOD_LIKE))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLTaskInvFVBridge::renameItem(const std::string& new_name)
{
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
	{
		return FALSE;
	}
// [/RLVa:KB]

//	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(object)
	{
		LLViewerInventoryItem* item = NULL;
		item = (LLViewerInventoryItem*)object->getInventoryObject(mUUID);
		if(item && (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
										GP_OBJECT_MANIPULATE, GOD_LIKE)))
		{
			LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
			new_item->rename(new_name);
			object->updateInventory(
				new_item,
				TASK_INVENTORY_ITEM_KEY,
				false);
		}
	}
	return TRUE;
}

BOOL LLTaskInvFVBridge::isItemMovable()
{
	//LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	//if(object && (object->permModify() || gAgent.isGodlike()))
	//{
	//	return TRUE;
	//}
	//return FALSE;
// [RLVa:KB] - Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
	if (rlv_handler_t::isEnabled())
	{
		const LLViewerObject* pObj = gObjectList.findObject(mPanel->getTaskUUID());
		if (pObj)
		{
			if (gRlvAttachmentLocks.isLockedAttachment(pObj->getRootEdit()))
			{
				return FALSE;
			}
			else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) )
			{
				LLVOAvatar* pAvatar = gAgentAvatarp;
				if ( (pAvatar) && (pAvatar->isSitting()) && (pAvatar->getRoot() == pObj->getRootEdit()) )
					return FALSE;
			}
		}
	}
// [/RLVa:KB]
	return TRUE;
}

BOOL LLTaskInvFVBridge::isItemRemovable()
{
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
// [RLVa:KB] - Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
	if ( (object) && (rlv_handler_t::isEnabled()) )
	{
		const LLViewerObject* pObjRoot = object->getRootEdit();
		if (gRlvAttachmentLocks.isLockedAttachment(pObjRoot))
		{
			return FALSE;
		}
		else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) )
		{
			LLVOAvatar* pAvatar = gAgentAvatarp;
			if ( (pAvatar) && (pAvatar->isSitting()) && (pAvatar->getRoot() == pObjRoot) )
				return FALSE;
		}
	}
// [/RLVa:KB]

	if(object
	   && (object->permModify() || object->permYouOwner()))
	{
		return TRUE;
	}
	return FALSE;
}

// helper for remove
typedef std::pair<LLUUID, std::list<LLUUID> > two_uuids_list_t;
typedef std::pair<LLPanelInventory*, two_uuids_list_t> remove_data_t;

bool remove_task_inventory_callback(const LLSD& notification, const LLSD& response, LLPanelInventory* panel)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLViewerObject* object = gObjectList.findObject(notification["payload"]["task_id"].asUUID());
	if(option == 0 && object)
	{
		// yes
		LLSD::array_const_iterator list_end = notification["payload"]["inventory_ids"].endArray();
		for (LLSD::array_const_iterator list_it = notification["payload"]["inventory_ids"].beginArray();
			list_it != list_end; 
			++list_it)
		{
			object->removeInventory(list_it->asUUID());
		}

		// refresh the UI.
		panel->refresh();
	}
	return false;
}

BOOL LLTaskInvFVBridge::removeItem()
{
	if(isItemRemovable() && mPanel)
	{
		LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
		if(object)
		{
			if(object->permModify())
			{
				// just do it.
				object->removeInventory(mUUID);
				return TRUE;
			}
			else
			{
				remove_data_t* data = new remove_data_t;
				data->first = mPanel;
				data->second.first = mPanel->getTaskUUID();
				data->second.second.push_back(mUUID);
				LLSD payload;
				payload["task_id"] = mPanel->getTaskUUID();
				payload["inventory_ids"].append(mUUID);
				LLNotificationsUtil::add("RemoveItemWarn", LLSD(), payload, boost::bind(&remove_task_inventory_callback, _1, _2, mPanel));
				return FALSE;
			}
		}
	}
	return FALSE;
}

void LLTaskInvFVBridge::removeBatch(LLDynamicArray<LLFolderViewEventListener*>& batch)
{
	if (!mPanel)
	{
		return;
	}

	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if (!object)
	{
		return;
	}

	if (!object->permModify())
	{
		LLSD payload;
		payload["task_id"] = mPanel->getTaskUUID();
		for (S32 i = 0; i < (S32)batch.size(); i++)
		{
			LLTaskInvFVBridge* itemp = (LLTaskInvFVBridge*)batch[i];
			payload["inventory_ids"].append(itemp->getUUID());
		}
		LLNotificationsUtil::add("RemoveItemWarn", LLSD(), payload, boost::bind(&remove_task_inventory_callback, _1, _2, mPanel));
		
	}
	else
	{
		for (S32 i = 0; i < (S32)batch.size(); i++)
		{
			LLTaskInvFVBridge* itemp = (LLTaskInvFVBridge*)batch[i];

			if(itemp->isItemRemovable())
			{
				// just do it.
				object->removeInventory(itemp->getUUID());
			}
		}
	}
}

void LLTaskInvFVBridge::move(LLFolderViewEventListener* parent_listener)
{
}

BOOL LLTaskInvFVBridge::isItemCopyable() const
{
	LLInventoryItem* item = findItem();
	if(!item) return FALSE;
	return gAgent.allowOperation(PERM_COPY, item->getPermissions(),
								GP_OBJECT_MANIPULATE);
}

BOOL LLTaskInvFVBridge::copyToClipboard() const
{
	return FALSE;
}

BOOL LLTaskInvFVBridge::cutToClipboard() const
{
	return FALSE;
}

BOOL LLTaskInvFVBridge::isClipboardPasteable() const
{
	return FALSE;
}

void LLTaskInvFVBridge::pasteFromClipboard()
{
}

void LLTaskInvFVBridge::pasteLinkFromClipboard()
{
}

BOOL LLTaskInvFVBridge::startDrag(EDragAndDropType* type, LLUUID* id) const
{
	//llinfos << "LLTaskInvFVBridge::startDrag()" << llendl;
	if(mPanel)
	{
		LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
		if(object)
		{
			LLInventoryItem* inv = NULL;
			if((inv = (LLInventoryItem*)object->getInventoryObject(mUUID)))
			{
				const LLPermissions& perm = inv->getPermissions();
				bool can_copy = gAgent.allowOperation(PERM_COPY, perm,
														GP_OBJECT_MANIPULATE);
// [RLVa:KB] - Checked: 2009-10-10 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
				// Kind of redundant due to the note below, but in case that ever gets fixed
				if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
				{
					return FALSE;
				}
// [/RLVa:KB]
				if (object->isAttachment() && !can_copy)
				{
                    //RN: no copy contents of attachments cannot be dragged out
                    // due to a race condition and possible exploit where
                    // attached objects do not update their inventory items
                    // when their contents are manipulated
                    return FALSE;
				}
				if((can_copy && perm.allowTransferTo(gAgent.getID()))
				   || object->permYouOwner())
//				   || gAgent.isGodlike())

				{
					*type = LLViewerAssetType::lookupDragAndDropType(inv->getType());

					*id = inv->getUUID();
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

BOOL LLTaskInvFVBridge::dragOrDrop(MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data)
{
	//llinfos << "LLTaskInvFVBridge::dragOrDrop()" << llendl;
	return FALSE;
}

//void LLTaskInvFVBridge::dropped()
//{
//	llwarns << "LLTaskInvFVBridge::dropped() - not implemented" << llendl;
//}

// virtual
void LLTaskInvFVBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if (action == "task_buy")
	{
		// Check the price of the item.
		S32 price = getPrice();
		if (-1 == price)
		{
			llwarns << "label_buy_task_bridged_item: Invalid price" << llendl;
		}
		else
		{
			if (price > 0 && price > gStatusBar->getBalance())
			{
				LLFloaterBuyCurrency::buyCurrency("This costs", price);
			}
			else
			{
				buyItem();
			}
		}
	}
	else if (action == "task_open")
	{
		openItem();
	}
	else if (action == "task_properties")
	{
		showProperties();
	}
}

void LLTaskInvFVBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryItem* item = findItem();
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	
	if (!item)
	{
		hide_context_entries(menu, items, disabled_items);
		return;
	}

	 // *TODO: Translate
	if(gAgent.allowOperation(PERM_OWNER, item->getPermissions(),
							 GP_OBJECT_MANIPULATE)
	   && item->getSaleInfo().isForSale())
	{
		items.push_back(std::string("Task Buy"));

		std::string label("Buy");
		// Check the price of the item.
		S32 price = getPrice();
		if (-1 == price)
		{
			llwarns << "label_buy_task_bridged_item: Invalid price" << llendl;
		}
		else
		{
			std::ostringstream info;
			info << "Buy for " << gHippoGridManager->getConnectedGrid()->getCurrencySymbol() << price;
			label.assign(info.str());
		}

		const LLView::child_list_t *list = menu.getChildList();
		LLView::child_list_t::const_iterator itor;
		for (itor = list->begin(); itor != list->end(); ++itor)
		{
			std::string name = (*itor)->getName();
			LLMenuItemCallGL* menu_itemp = dynamic_cast<LLMenuItemCallGL*>(*itor);
			if (name == "Task Buy" && menu_itemp)
			{
				menu_itemp->setLabel(label);
			}
		}
	}
	else
	{
		items.push_back(std::string("Task Open"));
		if (!isItemCopyable())
		{
			disabled_items.push_back(std::string("Task Open"));
		}
// [RLVa:KB] - Checked: 2010-03-01 (RLVa-1.2.0b) | Modified: RLVa-1.1.0a
		else if (rlv_handler_t::isEnabled())
		{
			LLViewerObject* pAttachObj = gObjectList.findObject(mPanel->getTaskUUID());
			bool fLocked = (pAttachObj) ? gRlvAttachmentLocks.isLockedAttachment(pAttachObj->getRootEdit()) : false;
			if ( ((LLAssetType::AT_NOTECARD == item->getType()) && ((gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) || (fLocked))) || 
				 ((LLAssetType::AT_LSL_TEXT == item->getType()) && ((gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) || (fLocked))) ||
				 ((LLAssetType::AT_TEXTURE == item->getType()) && (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE))) )
			{
				disabled_items.push_back(std::string("Task Open"));
			}
		}
// [/RLVa:KB]
	}
	items.push_back(std::string("Task Properties"));
//	if(isItemRenameable())
//	{
//		items.push_back(std::string("Task Rename"));
//	}
//	if(isItemRemovable())
//	{
//		items.push_back(std::string("Task Remove"));
//	}
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Added: RLVa-1.2.1f
	items.push_back(std::string("Task Rename"));
	items.push_back(std::string("Task Remove"));
	if (!isItemRenameable())
	{
		disabled_items.push_back(std::string("Task Rename"));
	}
	if (!isItemRemovable())
	{
		disabled_items.push_back(std::string("Task Remove"));
	}
// [/RLVa:KB]

	hide_context_entries(menu, items, disabled_items);
}


///----------------------------------------------------------------------------
/// Class LLTaskFolderBridge
///----------------------------------------------------------------------------

class LLTaskCategoryBridge : public LLTaskInvFVBridge
{
public:
	LLTaskCategoryBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	virtual const std::string& getDisplayName() const { return getName(); }
	virtual BOOL isItemRenameable() const;
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL isItemRemovable();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual BOOL hasChildren() const;
	virtual BOOL startDrag(EDragAndDropType* type, LLUUID* id) const;
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);
};

LLTaskCategoryBridge::LLTaskCategoryBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskCategoryBridge::getIcon() const
{
	return LLUI::getUIImage("inv_folder_plain_closed.tga");
}

BOOL LLTaskCategoryBridge::isItemRenameable() const
{
	return FALSE;
}

BOOL LLTaskCategoryBridge::renameItem(const std::string& new_name)
{
	return FALSE;
}

BOOL LLTaskCategoryBridge::isItemRemovable()
{
	return FALSE;
}

void LLTaskCategoryBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	items.push_back(std::string("Task Open"));  // *TODO: Translate
	hide_context_entries(menu, items, disabled_items);
}

BOOL LLTaskCategoryBridge::hasChildren() const
{
	// return TRUE if we have or do know know if we have children.
	// *FIX: For now, return FALSE - we will know for sure soon enough.
	return FALSE;
}

BOOL LLTaskCategoryBridge::startDrag(EDragAndDropType* type, LLUUID* id) const
{
	//llinfos << "LLTaskInvFVBridge::startDrag()" << llendl;
	if(mPanel)
	{
		LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
		if(object)
		{
			const LLInventoryObject* cat = object->getInventoryObject(mUUID);
			if ( (cat) && (move_inv_category_world_to_agent(mUUID, LLUUID::null, FALSE)) )
			{
				*type = LLViewerAssetType::lookupDragAndDropType(cat->getType());
				*id = mUUID;
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL LLTaskCategoryBridge::dragOrDrop(MASK mask, BOOL drop,
									  EDragAndDropType cargo_type,
									  void* cargo_data)
{
	//llinfos << "LLTaskCategoryBridge::dragOrDrop()" << llendl;
	BOOL accept = FALSE;
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(object)
	{
		switch(cargo_type)
		{
		case DAD_CATEGORY:
			accept = LLToolDragAndDrop::getInstance()->dadUpdateInventoryCategory(object,drop);
			break;
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_LANDMARK:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_CLOTHING:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
		// <edit>
		case DAD_CALLINGCARD:
		// </edit>
			// *HACK: In order to resolve SL-22177, we need to block
			// drags from notecards and objects onto other
			// objects. uncomment the simpler version when we have
			// that right.
			accept = LLToolDragAndDrop::isInventoryDropAcceptable(object, (LLViewerInventoryItem*)cargo_data);
			// <edit> testzone
			//if(LLToolDragAndDrop::isInventoryDropAcceptable(
			//	   object, (LLViewerInventoryItem*)cargo_data)
			if(object->permModify()
			// </edit>
			   && (LLToolDragAndDrop::SOURCE_WORLD != LLToolDragAndDrop::getInstance()->getSource())
			   && (LLToolDragAndDrop::SOURCE_NOTECARD != LLToolDragAndDrop::getInstance()->getSource()))
			{
				accept = TRUE;
			}
			if(accept && drop)
			{
				LLToolDragAndDrop::dropInventory(object,
												 (LLViewerInventoryItem*)cargo_data,
												 LLToolDragAndDrop::getInstance()->getSource(),
												 LLToolDragAndDrop::getInstance()->getSourceID());
			}
			break;
		case DAD_SCRIPT:
			// *HACK: In order to resolve SL-22177, we need to block
			// drags from notecards and objects onto other
			// objects. uncomment the simpler version when we have
			// that right.
			//accept = LLToolDragAndDrop::isInventoryDropAcceptable(object, (LLViewerInventoryItem*)cargo_data);
			if(LLToolDragAndDrop::isInventoryDropAcceptable(
				   object, (LLViewerInventoryItem*)cargo_data)
			   && (LLToolDragAndDrop::SOURCE_WORLD != LLToolDragAndDrop::getInstance()->getSource())
			   && (LLToolDragAndDrop::SOURCE_NOTECARD != LLToolDragAndDrop::getInstance()->getSource()))
			{
				accept = TRUE;
			}
			if(accept && drop)
			{
				LLViewerInventoryItem* item = (LLViewerInventoryItem*)cargo_data;
				// rez in the script active by default, rez in
				// inactive if the control key is being held down.
				BOOL active = ((mask & MASK_CONTROL) == 0);
				LLToolDragAndDrop::dropScript(object, item, active,
											  LLToolDragAndDrop::getInstance()->getSource(),
											  LLToolDragAndDrop::getInstance()->getSourceID());
			}
			break;
		// <edit>
		//case DAD_CALLINGCARD:
		// </edit>
		default:
			break;
		}
	}
	return accept;
}

///----------------------------------------------------------------------------
/// Class LLTaskTextureBridge
///----------------------------------------------------------------------------

class LLTaskTextureBridge : public LLTaskInvFVBridge
{
public:
	LLTaskTextureBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name,
		LLInventoryType::EType it);

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
protected:
	LLInventoryType::EType mInventoryType;
};

LLTaskTextureBridge::LLTaskTextureBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name,
	LLInventoryType::EType it) :
	LLTaskInvFVBridge(panel, uuid, name),
	mInventoryType(it)
{
}

LLUIImagePtr LLTaskTextureBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_TEXTURE, mInventoryType, 0, FALSE);
}

void LLTaskTextureBridge::openItem()
{
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
	if (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE))
	{
		RlvNotifications::notifyBlockedViewTexture();
		return;
	}
// [/RLVa:KB]

	llinfos << "LLTaskTextureBridge::openItem()" << llendl;
	if(!LLPreview::show(mUUID))
	{
		// There isn't one, so make a new preview
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
		rect.translate( left - rect.mLeft, top - rect.mTop );
		LLPreviewTexture* preview = new LLPreviewTexture("preview task texture",
												 rect,
												 getName(),
												 mUUID,
												 mPanel->getTaskUUID());
		preview->setFocus(TRUE);
	}
}


///----------------------------------------------------------------------------
/// Class LLTaskSoundBridge
///----------------------------------------------------------------------------

class LLTaskSoundBridge : public LLTaskInvFVBridge
{
public:
	LLTaskSoundBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	static void openSoundPreview(void* data);
};

LLTaskSoundBridge::LLTaskSoundBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskSoundBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SOUND, LLInventoryType::IT_SOUND, 0, FALSE);
}

void LLTaskSoundBridge::openItem()
{
	openSoundPreview((void*)this);
}

void LLTaskSoundBridge::openSoundPreview(void* data)
{
	LLTaskSoundBridge* self = (LLTaskSoundBridge*)data;
	if(!self) return;
	if(!LLPreview::show(self->mUUID))
	{
		// There isn't one, so make a new preview
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewSoundRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewSound* floaterp = 	new LLPreviewSound("preview task sound",
							   rect,
							   self->getName(),
							   self->mUUID,
							   self->mPanel->getTaskUUID());
		floaterp->open();	/*Flawfinder: ignore*/
	}
}

// virtual
void LLTaskSoundBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if (action == "task_play")
	{
		LLInventoryItem* item = findItem();
		if(item)
		{
			send_sound_trigger(item->getAssetUUID(), 1.0);
		}
	}
	LLTaskInvFVBridge::performAction(folder, model, action);
}

void LLTaskSoundBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryItem* item = findItem();
	if(!item) return;
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	// *TODO: Translate
	if(item->getPermissions().getOwner() != gAgent.getID()
	   && item->getSaleInfo().isForSale())
	{
		items.push_back(std::string("Task Buy"));

		std::string label("Buy");
		// Check the price of the item.
		S32 price = getPrice();
		if (-1 == price)
		{
			llwarns << "label_buy_task_bridged_item: Invalid price" << llendl;
		}
		else
		{
			std::ostringstream info;
			info << "Buy for " << gHippoGridManager->getConnectedGrid()->getCurrencySymbol() << price;
			label.assign(info.str());
		}

		const LLView::child_list_t *list = menu.getChildList();
		LLView::child_list_t::const_iterator itor;
		for (itor = list->begin(); itor != list->end(); ++itor)
		{
			std::string name = (*itor)->getName();
			LLMenuItemCallGL* menu_itemp = dynamic_cast<LLMenuItemCallGL*>(*itor);
			if (name == "Task Buy" && menu_itemp)
			{
				menu_itemp->setLabel(label);
			}
		}
	}
	else
	{
		items.push_back(std::string("Task Open")); 
		if (!isItemCopyable())
		{
			disabled_items.push_back(std::string("Task Open"));
		}
	}
	items.push_back(std::string("Task Properties"));
	if(isItemRenameable())
	{
		items.push_back(std::string("Task Rename"));
	}
	if(isItemRemovable())
	{
		items.push_back(std::string("Task Remove"));
	}

	items.push_back(std::string("Task Play"));
	/*menu.appendSeparator();
	menu.append(new LLMenuItemCallGL("Play",
									 &LLTaskSoundBridge::playSound,
									 NULL,
									 (void*)this));*/

	hide_context_entries(menu, items, disabled_items);
}

///----------------------------------------------------------------------------
/// Class LLTaskLandmarkBridge
///----------------------------------------------------------------------------

class LLTaskLandmarkBridge : public LLTaskInvFVBridge
{
public:
	LLTaskLandmarkBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
};

LLTaskLandmarkBridge::LLTaskLandmarkBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskLandmarkBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_LANDMARK, LLInventoryType::IT_LANDMARK, 0, FALSE);
}


///----------------------------------------------------------------------------
/// Class LLTaskCallingCardBridge
///----------------------------------------------------------------------------

class LLTaskCallingCardBridge : public LLTaskInvFVBridge
{
public:
	LLTaskCallingCardBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	virtual BOOL isItemRenameable() const;
	virtual BOOL renameItem(const std::string& new_name);
};

LLTaskCallingCardBridge::LLTaskCallingCardBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskCallingCardBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_CALLINGCARD, LLInventoryType::IT_CALLINGCARD, 0, FALSE);
}

BOOL LLTaskCallingCardBridge::isItemRenameable() const
{
	return FALSE;
}

BOOL LLTaskCallingCardBridge::renameItem(const std::string& new_name)
{
	return FALSE;
}


///----------------------------------------------------------------------------
/// Class LLTaskScriptBridge
///----------------------------------------------------------------------------

class LLTaskScriptBridge : public LLTaskInvFVBridge
{
public:
	LLTaskScriptBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	//static BOOL enableIfCopyable( void* userdata );
};

LLTaskScriptBridge::LLTaskScriptBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskScriptBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SCRIPT, LLInventoryType::IT_LSL, 0, FALSE);
}


class LLTaskLSLBridge : public LLTaskScriptBridge
{
public:
	LLTaskLSLBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual void openItem();
	virtual BOOL removeItem();
	//virtual void buildContextMenu(LLMenuGL& menu);

	//static void copyToInventory(void* userdata);
};

LLTaskLSLBridge::LLTaskLSLBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskScriptBridge(panel, uuid, name)
{
}

void LLTaskLSLBridge::openItem()
{
	llinfos << "LLTaskLSLBridge::openItem() " << mUUID << llendl;
	if(LLLiveLSLEditor::show(mUUID, mPanel->getTaskUUID()))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}

// [RLVa:KB] - Checked: 2010-03-27 (RLVa-1.1.3b) | Modified: RLVa-1.1.0a
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
	{
		RlvNotifications::notifyBlockedViewScript();
		return;
	}
// [/RLVa:KB]

	if(object->permModify() || gAgent.isGodlike())
	{
		std::string title("Script: ");
		LLInventoryItem* item = findItem();
		if (item)
		{
			title.append(item->getName());
		}

		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewScriptRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLLiveLSLEditor* editor;
		editor = new LLLiveLSLEditor("lsl ed",
									   rect,
									   title,
									   mPanel->getTaskUUID(),
									   mUUID);
		LLMultiFloater* previous_host = LLFloater::getFloaterHost();
		LLFloater::setFloaterHost(NULL);
		editor->open();	/*Flawfinder: ignore*/
		LLFloater::setFloaterHost(previous_host);

		// keep onscreen
		gFloaterView->adjustToFitScreen(editor, FALSE);
	}
}

BOOL LLTaskLSLBridge::removeItem()
{
	LLLiveLSLEditor::hide(mUUID, mPanel->getTaskUUID());
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskObjectBridge
///----------------------------------------------------------------------------

class LLTaskObjectBridge : public LLTaskInvFVBridge
{
public:
	LLTaskObjectBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
};

LLTaskObjectBridge::LLTaskObjectBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskObjectBridge::getIcon() const
{
	BOOL item_is_multi = FALSE;
	if ( mFlags & LLInventoryItemFlags::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS )
	{
		item_is_multi = TRUE;
	}

	return LLInventoryIcon::getIcon(LLAssetType::AT_OBJECT, LLInventoryType::IT_OBJECT, 0, item_is_multi);
}

///----------------------------------------------------------------------------
/// Class LLTaskNotecardBridge
///----------------------------------------------------------------------------

class LLTaskNotecardBridge : public LLTaskInvFVBridge
{
public:
	LLTaskNotecardBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual BOOL removeItem();
};

LLTaskNotecardBridge::LLTaskNotecardBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskNotecardBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_NOTECARD, LLInventoryType::IT_NOTECARD, 0, FALSE);
}

void LLTaskNotecardBridge::openItem()
{
	if(LLPreview::show(mUUID))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.3b) | Modified: RLVa-1.1.0a
	if ( (rlv_handler_t::isEnabled()) && 
		 ((gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) || (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit()))) )
	{
		RlvNotifications::notifyBlockedViewNote();
		return;
	}
// [/RLVa:KB]
	if(object->permModify() || gAgent.isGodlike())
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewScriptRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewNotecard* preview;
		preview = new LLPreviewNotecard("live notecard editor",
										 rect,
										 getName(),
										 mUUID,
										 mPanel->getTaskUUID());
		preview->setFocus(TRUE);  // if you're opening a notecard from an object's inventory, it takes focus

		// keep onscreen
		gFloaterView->adjustToFitScreen(preview, FALSE);
	}
}

BOOL LLTaskNotecardBridge::removeItem()
{
	LLPreview::hide(mUUID);
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskGestureBridge
///----------------------------------------------------------------------------

class LLTaskGestureBridge : public LLTaskInvFVBridge
{
public:
	LLTaskGestureBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual BOOL removeItem();
};

LLTaskGestureBridge::LLTaskGestureBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskGestureBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_GESTURE, LLInventoryType::IT_GESTURE, 0, FALSE);
}

void LLTaskGestureBridge::openItem()
{
	if(LLPreview::show(mUUID))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}

	// TODO: save rectangle
	std::string title = getName();
	LLUUID item_id = mUUID;
	LLUUID object_id = mPanel->getTaskUUID();
	LLPreviewGesture* preview = LLPreviewGesture::show(title, item_id, object_id);

	// keep onscreen
	gFloaterView->adjustToFitScreen(preview, FALSE);
}

BOOL LLTaskGestureBridge::removeItem()
{
	// Don't need to deactivate gesture because gestures inside objects
	// can never be active.
	LLPreview::hide(mUUID);
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskAnimationBridge
///----------------------------------------------------------------------------

class LLTaskAnimationBridge : public LLTaskInvFVBridge
{
public:
	LLTaskAnimationBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual BOOL removeItem();
};

LLTaskAnimationBridge::LLTaskAnimationBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskAnimationBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_ANIMATION, LLInventoryType::IT_ANIMATION, 0, FALSE);
}

void LLTaskAnimationBridge::openItem()
{
	if(LLPreview::show(mUUID))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}

	// TODO: what permissions allow looking at animation?
	if(object->permModify() || gAgent.isGodlike())
	{
		// TODO: save rectangle
		std::string title = getName();
		LLUUID item_id = mUUID;
		LLUUID object_id = mPanel->getTaskUUID();

		if(!LLPreview::show(mUUID))
		{
			// There isn't one, so make a new preview
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewAnimRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);

			LLPreviewAnim* preview = new LLPreviewAnim("preview anim",
									rect,
								   getName(),
								   mUUID,
								   0,
								   mPanel->getTaskUUID());
			preview->setFocus(TRUE);  // take focus if you're looking at one of these

			// Force to be entirely onscreen.
			gFloaterView->adjustToFitScreen(preview, FALSE);
		}
	}
}

BOOL LLTaskAnimationBridge::removeItem()
{
	LLPreview::hide(mUUID);
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskWearableBridge
///----------------------------------------------------------------------------

class LLTaskWearableBridge : public LLTaskInvFVBridge
{
public:
	LLTaskWearableBridge(
		LLPanelInventory* panel,
		const LLUUID& uuid,
		const std::string& name,
		LLAssetType::EType asset_type,
		U32 flags);

	virtual LLUIImagePtr getIcon() const;

protected:
	LLAssetType::EType		mAssetType;
};

LLTaskWearableBridge::LLTaskWearableBridge(
	LLPanelInventory* panel,
	const LLUUID& uuid,
	const std::string& name,
	LLAssetType::EType asset_type,
	U32 flags) :
	LLTaskInvFVBridge(panel, uuid, name, flags),
	mAssetType( asset_type )
{
}

LLUIImagePtr LLTaskWearableBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(mAssetType, LLInventoryType::IT_WEARABLE, mFlags, FALSE );
}


///----------------------------------------------------------------------------
/// LLTaskInvFVBridge impl
//----------------------------------------------------------------------------

LLTaskInvFVBridge* LLTaskInvFVBridge::createObjectBridge(LLPanelInventory* panel,
														 LLInventoryObject* object)
{
	LLTaskInvFVBridge* new_bridge = NULL;
	LLAssetType::EType type = object->getType();
	LLInventoryItem* item = NULL;
	switch(type)
	{
	case LLAssetType::AT_TEXTURE:
		item = (LLInventoryItem*)object;
		new_bridge = new LLTaskTextureBridge(panel,
											 object->getUUID(),
											 object->getName(),
											 item->getInventoryType());
		break;
	case LLAssetType::AT_SOUND:
		new_bridge = new LLTaskSoundBridge(panel,
										   object->getUUID(),
										   object->getName());
		break;
	case LLAssetType::AT_LANDMARK:
		new_bridge = new LLTaskLandmarkBridge(panel,
											  object->getUUID(),
											  object->getName());
		break;
	case LLAssetType::AT_CALLINGCARD:
		new_bridge = new LLTaskCallingCardBridge(panel,
												 object->getUUID(),
												 object->getName());
		break;
	case LLAssetType::AT_SCRIPT:
		// OLD SCRIPTS DEPRECATED - JC
		llwarns << "Old script" << llendl;
		//new_bridge = new LLTaskOldScriptBridge(panel,
		//									   object->getUUID(),
		//									   object->getName());
		break;
	case LLAssetType::AT_OBJECT:
		new_bridge = new LLTaskObjectBridge(panel,
											object->getUUID(),
											object->getName());
		break;
	case LLAssetType::AT_NOTECARD:
		new_bridge = new LLTaskNotecardBridge(panel,
											  object->getUUID(),
											  object->getName());
		break;
	case LLAssetType::AT_ANIMATION:
		new_bridge = new LLTaskAnimationBridge(panel,
											  object->getUUID(),
											  object->getName());
		break;
	case LLAssetType::AT_GESTURE:
		new_bridge = new LLTaskGestureBridge(panel,
											  object->getUUID(),
											  object->getName());
		break;
	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
		item = (LLInventoryItem*)object;
		new_bridge = new LLTaskWearableBridge(panel,
											  object->getUUID(),
											  object->getName(),
											  type,
											  item->getFlags());
		break;
	case LLAssetType::AT_CATEGORY:
		new_bridge = new LLTaskCategoryBridge(panel,
											  object->getUUID(),
											  object->getName());
		break;
	case LLAssetType::AT_LSL_TEXT:
		new_bridge = new LLTaskLSLBridge(panel,
										 object->getUUID(),
										 object->getName());
		break;
	default:
		llinfos << "Unhandled inventory type (llassetstorage.h): "
				<< (S32)type << llendl;
		break;
	}
	return new_bridge;
}


///----------------------------------------------------------------------------
/// Class LLPanelInventory
///----------------------------------------------------------------------------

// Default constructor
LLPanelInventory::LLPanelInventory(const std::string& name, const LLRect& rect) :
	LLPanel(name, rect),
	mScroller(NULL),
	mFolders(NULL),
	mHaveInventory(FALSE),
	mIsInventoryEmpty(TRUE),
	mInventoryNeedsUpdate(FALSE)
{
	reset();
	// Callbacks
	init_object_inventory_panel_actions(this);
	gIdleCallbacks.addFunction(idle, this);
}

// Destroys the object
LLPanelInventory::~LLPanelInventory()
{
	if (!gIdleCallbacks.deleteFunction(idle, this))
	{
		llwarns << "LLPanelInventory::~LLPanelInventory() failed to delete callback" << llendl;
	}
}


void LLPanelInventory::clearContents()
{
	mHaveInventory = FALSE;
	mIsInventoryEmpty = TRUE;
	if (LLToolDragAndDrop::getInstance() && LLToolDragAndDrop::getInstance()->getSource() == LLToolDragAndDrop::SOURCE_WORLD)
	{
		LLToolDragAndDrop::getInstance()->endDrag();
	}

	if( mScroller )
	{
		// removes mFolders
		removeChild( mScroller );
		mScroller->die();
		mScroller = NULL;
		mFolders = NULL;
	}
}


void LLPanelInventory::reset()
{
	clearContents();

	setBorderVisible(FALSE);

	LLRect dummy_rect(0, 1, 1, 0);
	mFolders = new LLFolderView(std::string("task inventory"), NULL, dummy_rect, getTaskUUID(), this);
	// this ensures that we never say "searching..." or "no items found"
	mFolders->getFilter()->setShowFolderState(LLInventoryFilter::SHOW_ALL_FOLDERS);

	LLRect scroller_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	mScroller = new LLScrollableContainerView(std::string("task inventory scroller"), scroller_rect, mFolders );
	mScroller->setFollowsAll();
	addChild(mScroller);

	mFolders->setScrollContainer( mScroller );
}

void LLPanelInventory::inventoryChanged(LLViewerObject* object,
										LLInventoryObject::object_list_t* inventory,
										S32 serial_num,
										void* data)
{
	if(!object) return;

	//llinfos << "invetnory arrived: \n"
	//		<< " panel UUID: " << panel->mTaskUUID << "\n"
	//		<< " task  UUID: " << object->mID << llendl;
	if(mTaskUUID == object->mID)
	{
		mInventoryNeedsUpdate = TRUE;
	}

	// refresh any properties floaters that are hanging around.
	if(inventory)
	{
		// We need to copy the ones that need refreshing onto a
		// temporary object because we cannot iterate through the
		// object inventory twice... A pox on stateful iteration!
		LLFloaterProperties* floater = NULL;
		LLDynamicArray<LLFloaterProperties*> refresh;

		LLInventoryObject::object_list_t::const_iterator it = inventory->begin();
		LLInventoryObject::object_list_t::const_iterator end = inventory->end();
		for( ;	it != end;	++it)
		{
			floater = LLFloaterProperties::find((*it)->getUUID(),
												object->getID());
			if(floater)
			{
				refresh.put(floater);
			}
		}
		S32 count = refresh.count();
		for(S32 i = 0; i < count; ++i)
		{
			refresh.get(i)->refresh();
		}
	}
}

void LLPanelInventory::updateInventory()
{
	//llinfos << "inventory arrived: \n"
	//		<< " panel UUID: " << panel->mTaskUUID << "\n"
	//		<< " task  UUID: " << object->mID << llendl;
	// We're still interested in this task's inventory.
	std::set<LLUUID> selected_items;
	BOOL inventory_has_focus = FALSE;
	if (mHaveInventory && mFolders->getNumSelectedDescendants())
	{
		mFolders->getSelectionList(selected_items);
		inventory_has_focus = gFocusMgr.childHasKeyboardFocus(mFolders);
	}

	reset();

	LLViewerObject* objectp = gObjectList.findObject(mTaskUUID);
	if (objectp)
	{
		LLInventoryObject* inventory_root = objectp->getInventoryRoot();
		LLInventoryObject::object_list_t contents;
		objectp->getInventoryContents(contents);
		if (inventory_root)
		{
			createFolderViews(inventory_root, contents);
			mHaveInventory = TRUE;
			mIsInventoryEmpty = FALSE;
			mFolders->setEnabled(TRUE);
		}
		else
		{
			// TODO: create an empty inventory
			mIsInventoryEmpty = TRUE;
			mHaveInventory = TRUE;
		}
	}
	else
	{
		// TODO: create an empty inventory
		mIsInventoryEmpty = TRUE;
		mHaveInventory = TRUE;
	}

	// restore previous selection
	std::set<LLUUID>::iterator selection_it;
	BOOL first_item = TRUE;
	for (selection_it = selected_items.begin(); selection_it != selected_items.end(); ++selection_it)
	{
		LLFolderViewItem* selected_item = mFolders->getItemByID(*selection_it);
		if (selected_item)
		{
			//HACK: "set" first item then "change" each other one to get keyboard focus right
			if (first_item)
			{
				mFolders->setSelection(selected_item, TRUE, inventory_has_focus);
				first_item = FALSE;
			}
			else
			{
				mFolders->changeSelection(selected_item, TRUE);
			}
		}
	}

	mFolders->arrangeFromRoot();
	mInventoryNeedsUpdate = FALSE;
}

// *FIX: This is currently a very expensive operation, because we have
// to iterate through the inventory one time for each category. This
// leads to an N^2 based on the category count. This could be greatly
// speeded with an efficient multimap implementation, but we don't
// have that in our current arsenal.
void LLPanelInventory::createFolderViews(LLInventoryObject* inventory_root, LLInventoryObject::object_list_t& contents)
{
	if (!inventory_root)
	{
		return;
	}
	// Create a visible root category.
	LLTaskInvFVBridge* bridge = NULL;
	bridge = LLTaskInvFVBridge::createObjectBridge(this, inventory_root);
	if(bridge)
	{
		LLFolderViewFolder* new_folder = NULL;
		new_folder = new LLFolderViewFolder(inventory_root->getName(),
											bridge->getIcon(),
											mFolders,
											bridge);
		new_folder->addToFolder(mFolders, mFolders);
		new_folder->toggleOpen();

		createViewsForCategory(&contents, inventory_root, new_folder);
	}
}

typedef std::pair<LLInventoryObject*, LLFolderViewFolder*> obj_folder_pair;

void LLPanelInventory::createViewsForCategory(LLInventoryObject::object_list_t* inventory, 
											  LLInventoryObject* parent,
											  LLFolderViewFolder* folder)
{
	// Find all in the first pass
	LLDynamicArray<obj_folder_pair*> child_categories;
	LLTaskInvFVBridge* bridge;
	LLFolderViewItem* view;

	LLInventoryObject::object_list_t::iterator it = inventory->begin();
	LLInventoryObject::object_list_t::iterator end = inventory->end();
	for( ; it != end; ++it)
	{
		LLInventoryObject* obj = *it;

		if(parent->getUUID() == obj->getParentUUID())
		{
			bridge = LLTaskInvFVBridge::createObjectBridge(this, obj);
			if(!bridge)
			{
				continue;
			}
			if(LLAssetType::AT_CATEGORY == obj->getType())
			{
				view = new LLFolderViewFolder(obj->getName(),
											  bridge->getIcon(),
											  mFolders,
											  bridge);
				child_categories.put(new obj_folder_pair(obj,
														 (LLFolderViewFolder*)view));
			}
			else
			{
				view = new LLFolderViewItem(obj->getName(),
											bridge->getIcon(),
											bridge->getCreationDate(),
											mFolders,
											bridge);
			}
			view->addToFolder(folder, mFolders);
		}
	}

	// now, for each category, do the second pass
	for(S32 i = 0; i < child_categories.count(); i++)
	{
		createViewsForCategory(inventory, child_categories[i]->first,
							   child_categories[i]->second );
		delete child_categories[i];
	}
}

void LLPanelInventory::refresh()
{
	//llinfos << "LLPanelInventory::refresh()" << llendl;
	BOOL has_inventory = FALSE;
	const BOOL non_root_ok = TRUE;
	LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode(NULL, non_root_ok);
	if(node)
	{
		LLViewerObject* object = node->getObject();
		if(object && ((LLSelectMgr::getInstance()->getSelection()->getRootObjectCount() == 1)
					  || (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1)))
		{
			// determine if we need to make a request. Start with a
			// default based on if we have inventory at all.
			BOOL make_request = !mHaveInventory;

			// If the task id is different than what we've stored,
			// then make the request.
			if(mTaskUUID != object->mID)
			{
				mTaskUUID = object->mID;
				make_request = TRUE;

				// This is a new object so pre-emptively clear the contents
				// Otherwise we show the old stuff until the update comes in
				clearContents();

				// Register for updates from this object,
				registerVOInventoryListener(object,NULL);
			}

			// Based on the node information, we may need to dirty the
			// object inventory and get it again.
			if(node->mValid)
			{
				if(node->mInventorySerial != object->getInventorySerial() || object->isInventoryDirty())
				{
					make_request = TRUE;
				}
			}

			// do the request if necessary.
			if(make_request)
			{
				requestVOInventory();
			}
			has_inventory = TRUE;
		}
	}
	if(!has_inventory)
	{
		mTaskUUID = LLUUID::null;
		removeVOInventoryListener();
		clearContents();
	}
	//llinfos << "LLPanelInventory::refresh() " << mTaskUUID << llendl;
}

void LLPanelInventory::removeSelectedItem()
{
	if(mFolders)
	{
		mFolders->removeSelectedItems();
	}
}

void LLPanelInventory::startRenamingSelectedItem()
{
	if(mFolders)
	{
		mFolders->startRenamingSelectedItem();
	}
}

void LLPanelInventory::draw()
{
	LLPanel::draw();

	if(mIsInventoryEmpty)
	{
		// *TODO: Translate
		if((LLUUID::null != mTaskUUID) && (!mHaveInventory))
		{
			LLFontGL::getFontSansSerif()->renderUTF8(std::string("Loading contents..."), 0,
										 (S32)(getRect().getWidth() * 0.5f),
										 10,
										 LLColor4( 1, 1, 1, 1 ),
										 LLFontGL::HCENTER,
										 LLFontGL::BOTTOM);
		}
		else if(mHaveInventory)
		{
			LLFontGL::getFontSansSerif()->renderUTF8(std::string("No contents"), 0,
										 (S32)(getRect().getWidth() * 0.5f),
										 10,
										 LLColor4( 1, 1, 1, 1 ),
										 LLFontGL::HCENTER,
										 LLFontGL::BOTTOM);
		}
	}
}

void LLPanelInventory::deleteAllChildren()
{
	mScroller = NULL;
	mFolders = NULL;
	LLView::deleteAllChildren();
}

BOOL LLPanelInventory::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void *cargo_data, EAcceptance *accept, std::string& tooltip_msg)
{
	if (mFolders && mHaveInventory)
	{
		LLFolderViewItem* folderp = mFolders->getNextFromChild(NULL);
		if (!folderp)
		{
			return FALSE;
		}
		// Try to pass on unmodified mouse coordinates
		S32 local_x = x - mFolders->getRect().mLeft;
		S32 local_y = y - mFolders->getRect().mBottom;

		if (mFolders->pointInView(local_x, local_y))
		{
			return mFolders->handleDragAndDrop(local_x, local_y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
		}
		else
		{
			//force mouse coordinates to be inside folder rectangle
			return mFolders->handleDragAndDrop(5, 1, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
		}
	}
	else
	{
		return FALSE;
	}
}

//static
void LLPanelInventory::idle(void* user_data)
{
	LLPanelInventory* self = (LLPanelInventory*)user_data;


	if (self->mInventoryNeedsUpdate)
	{
		self->updateInventory();
	}
}
