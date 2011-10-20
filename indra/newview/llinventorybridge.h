/** 
 * @file llinventorybridge.h
 * @brief Implementation of the Inventory-Folder-View-Bridge classes.
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

#ifndef LL_LLINVENTORYBRIDGE_H
#define LL_LLINVENTORYBRIDGE_H

#include "llcallingcard.h"
#include "llfloaterproperties.h"
#include "llfolderview.h"
#include "llinventorymodel.h"
//#include "llinventoryview.h"
#include "llviewercontrol.h"
#include "llwearable.h"

class LLInventoryPanel;
class LLInventoryModel;
class LLMenuGL;
class LLCallingCardObserver;
class LLViewerJointAttachment;

typedef std::vector<std::string> menuentry_vec_t;

struct LLAttachmentRezAction
{
	LLUUID	mItemID;
	S32		mAttachPt;
};

// [RLVa:KB] - Checked: 2009-12-18 (RLVa-1.1.0i) | Added: RLVa-1.1.0i
// Moved from llinventorybridge.cpp because we need it in RlvForceWearLegacy
struct LLFoundData
{
	LLFoundData(const LLUUID& item_id,
				const LLUUID& asset_id,
				const std::string& name,
				LLAssetType::EType asset_type) :
		mItemID(item_id),
		mAssetID(asset_id),
		mName(name),
		mAssetType(asset_type),
		mWearable( NULL ) {}
	
	LLUUID mItemID;
	LLUUID mAssetID;
	std::string mName;
	LLAssetType::EType mAssetType;
	LLWearable* mWearable;
};

struct LLWearableHoldingPattern
{
	LLWearableHoldingPattern(BOOL fAddToOutfit) : mResolved(0), mAddToOutfit(fAddToOutfit) {}
	~LLWearableHoldingPattern()
	{
		for_each(mFoundList.begin(), mFoundList.end(), DeletePointer());
		mFoundList.clear();
	}
	typedef std::list<LLFoundData*> found_list_t;
	found_list_t mFoundList;
	S32 mResolved;
	BOOL mAddToOutfit;
};
// [/RLVa:KB]

//helper functions
class LLShowProps 
{
public:

	static void showProperties(const LLUUID& uuid)
	{
		if(!LLFloaterProperties::show(uuid, LLUUID::null))
		{
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PropertiesRect");
			rect.translate( left - rect.mLeft, top - rect.mTop );
			LLFloaterProperties* floater;
			floater = new LLFloaterProperties("item properties",
											rect,
											"Inventory Item Properties",
											uuid,
											LLUUID::null);
			// keep onscreen
			gFloaterView->adjustToFitScreen(floater, FALSE);
		}
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanelObserver
//
// Bridge to support knowing when the inventory has changed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryPanelObserver : public LLInventoryObserver
{
public:
	LLInventoryPanelObserver(LLInventoryPanel* ip) : mIP(ip) {}
	virtual ~LLInventoryPanelObserver() {}
	virtual void changed(U32 mask);
protected:
	LLInventoryPanel* mIP;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvFVBridge (& it's derived classes)
//
// Short for Inventory-Folder-View-Bridge. This is an
// implementation class to be able to view inventory items.
//
// You'll want to call LLInvItemFVELister::createBridge() to actually create
// an instance of this class. This helps encapsulate the
// funcationality a bit. (except for folders, you can create those
// manually...)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInvFVBridge : public LLFolderViewEventListener
{
public:
	// This method is a convenience function which creates the correct
	// type of bridge based on some basic information
	static LLInvFVBridge* createBridge(LLAssetType::EType asset_type,
									   LLAssetType::EType actual_asset_type,
									   LLInventoryType::EType inv_type,
									   LLInventoryPanel* inventory,
									   const LLUUID& uuid,
									   U32 flags = 0x00);
	virtual ~LLInvFVBridge() {}


	//--------------------------------------------------------------------
	// LLInvFVBridge functionality
	//--------------------------------------------------------------------
	virtual const LLUUID& getUUID() const { return mUUID; }
	virtual void clearDisplayName() {}
	virtual const std::string& getPrefix() { return LLStringUtil::null; }
	virtual void restoreItem() {}
	virtual void restoreToWorld() {}

	//--------------------------------------------------------------------
	// Inherited LLFolderViewEventListener functions
	//--------------------------------------------------------------------
	virtual const std::string& getName() const;
	virtual const std::string& getDisplayName() const;
	virtual PermissionMask getPermissionMask() const;
	virtual LLFolderType::EType getPreferredType() const;
	virtual time_t getCreationDate() const;
	virtual LLFontGL::StyleFlags getLabelStyle() const { return LLFontGL::NORMAL; }
	virtual std::string getLabelSuffix() const { return LLStringUtil::null; }
	virtual void openItem() {}
	virtual void previewItem() {openItem();}
	virtual void showProperties();
	virtual BOOL isItemRenameable() const { return TRUE; }
	//virtual BOOL renameItem(const std::string& new_name) {}
	virtual BOOL isItemRemovable();
	virtual BOOL isItemMovable();
	virtual BOOL isItemInTrash() const;
	virtual BOOL isLink() const;
	//virtual BOOL removeItem() = 0;
	virtual void removeBatch(LLDynamicArray<LLFolderViewEventListener*>& batch);
	virtual void move(LLFolderViewEventListener* new_parent_bridge) {}
	virtual BOOL isItemCopyable() const { return FALSE; }
	virtual BOOL copyToClipboard() const { return FALSE; }
	virtual BOOL cutToClipboard() const { return FALSE; }
	virtual BOOL isClipboardPasteable() const;
	virtual BOOL isClipboardPasteableAsLink() const;
	virtual void pasteFromClipboard() {}
	virtual void pasteLinkFromClipboard() {}
	void getClipboardEntries(bool show_asset_id, menuentry_vec_t &items, 
							 menuentry_vec_t &disabled_items, U32 flags);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual BOOL startDrag(EDragAndDropType* type, LLUUID* id) const;
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data) { return FALSE; }
	virtual LLInventoryType::EType getInventoryType() const { return mInvType; }
	virtual LLWearableType::EType getWearableType() const { return LLWearableType::WT_NONE; }

protected:
	virtual void addTrashContextMenuOptions(menuentry_vec_t &items,
											menuentry_vec_t &disabled_items);
	virtual void addDeleteContextMenuOptions(menuentry_vec_t &items,
											 menuentry_vec_t &disabled_items);
	virtual void addOpenRightClickMenuOption(menuentry_vec_t &items);
protected:
	LLInvFVBridge(LLInventoryPanel* inventory, /*LLFolderView* root,*/ const LLUUID& uuid);

	LLInventoryObject* getInventoryObject() const;
	LLInventoryModel* getInventoryModel() const;
	
	BOOL isLinkedObjectInTrash() const; // Is this obj or its baseobj in the trash?
	BOOL isLinkedObjectMissing() const; // Is this a linked obj whose baseobj is not in inventory?
	// return true if the item is in agent inventory. if false, it
	// must be lost or in the inventory library.
	BOOL isAgentInventory() const; // false if lost or in the inventory library
	BOOL isCOFFolder() const; // true if COF or descendent of
	virtual BOOL isItemPermissive() const;
	static void changeItemParent(LLInventoryModel* model,
								 LLViewerInventoryItem* item,
								 const LLUUID& new_parent,
								 BOOL restamp);
	static void changeCategoryParent(LLInventoryModel* model,
									 LLViewerInventoryCategory* item,
									 const LLUUID& new_parent,
									 BOOL restamp);
	void removeBatchNoCheck(LLDynamicArray<LLFolderViewEventListener*>& batch);

protected:
	LLInventoryPanel* mInventoryPanel;
	LLUUID mUUID;	// item id
	LLInventoryType::EType mInvType;
	BOOL mIsLink;
};

class AIFilePicker;

class LLItemBridge : public LLInvFVBridge
{
public:
	LLItemBridge(LLInventoryPanel* inventory, const LLUUID& uuid) :
		LLInvFVBridge(inventory, uuid) {}

	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);

	virtual void selectItem();
	virtual void restoreItem();
	virtual void restoreToWorld();
	virtual void gotoItem(LLFolderView *folder);
	virtual LLUIImagePtr getIcon() const;
	virtual const std::string& getDisplayName() const;
	virtual std::string getLabelSuffix() const;
	virtual LLFontGL::StyleFlags getLabelStyle() const;
	virtual PermissionMask getPermissionMask() const;
	virtual time_t getCreationDate() const;
	virtual BOOL isItemRenameable() const;
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL removeItem();
	virtual BOOL isItemCopyable() const;
	virtual BOOL copyToClipboard() const;
	virtual BOOL hasChildren() const { return FALSE; }
	virtual BOOL isUpToDate() const { return TRUE; }

    static void showFloaterImagePreview(LLInventoryItem* item, AIFilePicker* filepicker);

	/*virtual*/ void clearDisplayName() { mDisplayName.clear(); }

	LLViewerInventoryItem* getItem() const;

protected:
	virtual BOOL isItemPermissive() const;
	static void buildDisplayName(LLInventoryItem* item, std::string& name);
	mutable std::string mDisplayName;
};


class LLFolderBridge : public LLInvFVBridge
{
public:
	LLFolderBridge(LLInventoryPanel* inventory, 
				   //LLFolderView* root,
				   const LLUUID& uuid) :
		LLInvFVBridge(inventory, /*root,*/ uuid),
		mCallingCards(FALSE),
		mWearables(FALSE)
	{}
	BOOL dragItemIntoFolder(LLInventoryItem* inv_item, BOOL drop);
	BOOL dragCategoryIntoFolder(LLInventoryCategory* inv_category, BOOL drop);
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void openItem();
	virtual BOOL isItemRenameable() const;
	virtual void selectItem();
	virtual void restoreItem();

	virtual LLFolderType::EType getPreferredType() const;
	virtual LLUIImagePtr getIcon() const;
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL removeItem();
	virtual void pasteFromClipboard();
	virtual void pasteLinkFromClipboard();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual BOOL hasChildren() const;
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);

	virtual BOOL isItemRemovable();
	virtual BOOL isItemMovable();
	virtual BOOL isUpToDate() const;
	virtual BOOL isClipboardPasteable() const;
	virtual BOOL isClipboardPasteableAsLink() const;

	static void createWearable(LLFolderBridge* bridge, LLWearableType::EType type);
	static void createWearable(LLUUID parent_folder_id, LLWearableType::EType type);

	LLViewerInventoryCategory* getCategory() const;

protected:
	// menu callbacks
	static void pasteClipboard(void* user_data);
	static void createNewCategory(void* user_data);

	static void createNewShirt(void* user_data);
	static void createNewPants(void* user_data);
	static void createNewShoes(void* user_data);
	static void createNewSocks(void* user_data);
	static void createNewJacket(void* user_data);
	static void createNewSkirt(void* user_data);
	static void createNewGloves(void* user_data);
	static void createNewUndershirt(void* user_data);
	static void createNewUnderpants(void* user_data);
	static void createNewAlpha(void* user_data);
	static void createNewTattoo(void* user_data);
	static void createNewShape(void* user_data);
	static void createNewSkin(void* user_data);
	static void createNewHair(void* user_data);
	static void createNewEyes(void* user_data);
	static void createNewPhysics(void* user_data);

	BOOL checkFolderForContentsOfType(LLInventoryModel* model, LLInventoryCollectFunctor& typeToCheck);

	void modifyOutfit(BOOL append, BOOL replace = FALSE);
	menuentry_vec_t getMenuItems() { return mItems; } // returns a copy of current menu items
public:
	static LLFolderBridge* sSelf;
	static void staticFolderOptionsMenu();
	void folderOptionsMenu();
private:
	BOOL			mCallingCards;
	BOOL			mWearables;
	LLMenuGL*		mMenu;
	menuentry_vec_t		mItems;
	menuentry_vec_t		mDisabledItems;
};


class LLTextureBridge : public LLItemBridge
{
public:
	virtual const std::string& getPrefix() { static std::string ret("Texture: ");return ret; }
	LLTextureBridge(LLInventoryPanel* inventory, 
					//LLFolderView* root,
					const LLUUID& uuid, 
					LLInventoryType::EType type) :
		LLItemBridge(inventory, /*root,*/ uuid)
	{
		mInvType = type;
	}
	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
};

class LLSoundBridge : public LLItemBridge
{
public:
 	LLSoundBridge(LLInventoryPanel* inventory, 
				  //LLFolderView* root,
				  const LLUUID& uuid) :
		LLItemBridge(inventory, /*root,*/ uuid) {}
	virtual void openItem();
	virtual void previewItem();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	static void openSoundPreview(void*);
};

class LLLandmarkBridge : public LLItemBridge
{
public:
	//Need access to prefix statically for LLViewerTextEditor::openEmbeddedLandmark
	static const std::string& prefix() { static std::string ret("Landmark: ");return ret; }
	virtual const std::string& getPrefix() { return prefix(); }
 	LLLandmarkBridge(LLInventoryPanel* inventory, 
					 //LLFolderView* root,
					 const LLUUID& uuid, 
					 U32 flags = 0x00);
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
protected:
	BOOL mVisited;
};

class LLCallingCardBridge : public LLItemBridge
{
public:
	virtual const std::string& getPrefix() { static std::string ret("Calling Card: ");return ret; }
	LLCallingCardBridge(LLInventoryPanel* inventory, 
						//LLFolderView* root,
						const LLUUID& uuid );
	~LLCallingCardBridge();
	virtual std::string getLabelSuffix() const;
	//virtual const std::string& getDisplayName() const;
	virtual LLUIImagePtr getIcon() const;
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void openItem();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	//virtual void renameItem(const std::string& new_name);
	//virtual BOOL removeItem();
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);
	void refreshFolderViewItem();
protected:
	LLCallingCardObserver* mObserver;
};


class LLNotecardBridge : public LLItemBridge
{
public:
	virtual const std::string& getPrefix() { static std::string ret("Notecard: ");return ret; }
	LLNotecardBridge(LLInventoryPanel* inventory, 
					 //LLFolderView* root,
					 const LLUUID& uuid) :
		LLItemBridge(inventory, /*root,*/ uuid) {}
	virtual void openItem();
};

class LLGestureBridge : public LLItemBridge
{
public:
	virtual const std::string& getPrefix() { static std::string ret("Gesture: ");return ret; }
	LLGestureBridge(LLInventoryPanel* inventory, 
					//LLFolderView* root,
					const LLUUID& uuid) :
		LLItemBridge(inventory, /*root,*/ uuid) {}	
	// Only suffix for gesture items, not task items, because only
	// gestures in your inventory can be active.
	virtual LLFontGL::StyleFlags getLabelStyle() const;
	virtual std::string getLabelSuffix() const;
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void openItem();
	virtual BOOL removeItem();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
};


class LLAnimationBridge : public LLItemBridge
{
public:
	virtual const std::string& getPrefix() { static std::string ret("Animation: ");return ret; }
	LLAnimationBridge(LLInventoryPanel* inventory, 
					  //LLFolderView* root, 
					  const LLUUID& uuid) :
		LLItemBridge(inventory, /*root,*/ uuid) {}
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual void openItem();
};


class LLObjectBridge : public LLItemBridge
{
public:
	virtual const std::string& getPrefix() { static std::string ret("Object: ");return ret; }
	LLObjectBridge(LLInventoryPanel* inventory, 
				   //LLFolderView* root, 
				   const LLUUID& uuid, 
				   LLInventoryType::EType type, 
				   U32 flags);
	virtual LLUIImagePtr	getIcon() const;
	virtual void			performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void			openItem();
	virtual std::string getLabelSuffix() const;
	virtual void			buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual BOOL			isItemRemovable();
	virtual BOOL renameItem(const std::string& new_name);
	LLInventoryObject* getObject() const;
protected:
	static LLUUID	sContextMenuItemID;  // Only valid while the context menu is open.
	U32 mAttachPt;
	BOOL mIsMultiObject;
};

class LLLSLTextBridge : public LLItemBridge
{
public:
	virtual const std::string& getPrefix() { static std::string ret("Script: ");return ret; }
	LLLSLTextBridge(LLInventoryPanel* inventory, 
					//LLFolderView* root, 
					const LLUUID& uuid ) :
		LLItemBridge(inventory, /*root,*/ uuid) {}
	virtual void openItem();
};


class LLWearableBridge : public LLItemBridge
{
public:
	LLWearableBridge(LLInventoryPanel* inventory, 
					 /*LLFolderView* root,*/
					 const LLUUID& uuid, 
					 LLAssetType::EType asset_type, 
					 LLInventoryType::EType inv_type, 
					 LLWearableType::EType wearable_type);
	virtual LLUIImagePtr getIcon() const;
	virtual void	performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void	openItem();
	virtual void	buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual std::string getLabelSuffix() const;
	virtual BOOL	isItemRemovable();
	virtual BOOL renameItem(const std::string& new_name);
	virtual LLWearableType::EType getWearableType() const { return mWearableType; }

	static void		onWearOnAvatar( void* userdata );	// Access to wearOnAvatar() from menu
	static BOOL		canWearOnAvatar( void* userdata );
	static void		onWearOnAvatarArrived( LLWearable* wearable, void* userdata );
	void			wearOnAvatar();

	static BOOL		canEditOnAvatar( void* userdata );	// Access to editOnAvatar() from menu
	static void		onEditOnAvatar( void* userdata );
	void			editOnAvatar();

	static BOOL		canRemoveFromAvatar( void* userdata );
	static void		onRemoveFromAvatar( void* userdata );
	static void		onRemoveFromAvatarArrived( LLWearable* wearable, void* userdata );
protected:
	LLAssetType::EType mAssetType;
	LLWearableType::EType  mWearableType;
};

class LLLinkItemBridge : public LLItemBridge
{
public:
	LLLinkItemBridge(LLInventoryPanel* inventory, 
					 //LLFolderView* root,
					 const LLUUID& uuid) :
		LLItemBridge(inventory, /*root,*/ uuid) {}
	virtual const std::string& getPrefix() { return sPrefix; }
	virtual LLUIImagePtr getIcon() const;
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
protected:
	static std::string sPrefix;
};

class LLLinkFolderBridge : public LLItemBridge
{
public:
	LLLinkFolderBridge(LLInventoryPanel* inventory, 
					   //LLFolderView* root,
					   const LLUUID& uuid) :
		LLItemBridge(inventory, /*root,*/ uuid) {}
	virtual const std::string& getPrefix() { return sPrefix; }
	virtual LLUIImagePtr getIcon() const;
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model, std::string action);
	virtual void gotoItem(LLFolderView *folder);
protected:
	const LLUUID &getFolderID() const;
	static std::string sPrefix;
};

class LLMeshBridge : public LLItemBridge
{
    friend class LLInvFVBridge;
public:
    virtual LLUIImagePtr getIcon() const;
    virtual void openItem();
    virtual void previewItem();
    virtual void buildContextMenu(LLMenuGL& menu, U32 flags);

protected:
    LLMeshBridge(LLInventoryPanel* inventory, 
             const LLUUID& uuid) :
                       LLItemBridge(inventory, uuid) {}
};

void rez_attachment(LLViewerInventoryItem* item, 
					LLViewerJointAttachment* attachment,
					bool replace = false);

// Move items from an in-world object's "Contents" folder to a specified
// folder in agent inventory.
BOOL move_inv_category_world_to_agent(const LLUUID& object_id, 
									  const LLUUID& category_id,
									  BOOL drop,
									  void (*callback)(S32, void*) = NULL,
									  void* user_data = NULL);
#endif // LL_LLINVENTORYBRIDGE_H
