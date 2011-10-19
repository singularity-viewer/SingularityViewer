/** 
 *
 * Copyright (c) 2010, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU General Public License, version 2.0, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the GPL can be found in doc/GPL-license.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/gpl-2.0.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#include "llinventorymodel.h"
#include "llmemory.h"
#include "llviewerinventory.h"

// ============================================================================

class LLCOFMgr : public LLSingleton<LLCOFMgr>
{
	/*
	 * Helper functions
	 */
public:
	void                checkCOF();
	void                fetchCOF();
	static const LLUUID getCOF() { return gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT); }
	static void         getDescendentsOfAssetType(const LLUUID& idCat, LLInventoryModel::item_array_t& items, 
	                                              LLAssetType::EType typeAsset, bool fFollowFolderLinks);
	bool                isLinkInCOF(const LLUUID& idItem);
protected:
	void addCOFItemLink(const LLUUID& idItem, LLPointer<LLInventoryCallback> cb = NULL);
	void addCOFItemLink(const LLInventoryItem* pItem, LLPointer<LLInventoryCallback> cb = NULL);
	void removeCOFItemLinks(const LLUUID& idItem);

	/*
	 * Attachment functions
	 */
public:
	void addAttachment(const LLUUID& idItem);
	void removeAttachment(const LLUUID& idItem);
	void synchAttachments();
protected:
	void onLinkAttachmentComplete(const LLUUID& idItem);
	void linkPendingAttachments();
	void setLinkAttachments(bool fEnable);
	void updateAttachments();

	/*
	 * Wearable functions
	 */
public:
	void addWearable(const LLUUID& idItem);
	void removeWearable(const LLUUID& idItem);
	void synchWearables();
protected:
	void onLinkWearableComplete(const LLUUID& idItem);

	/*
	 * Base outfit folder functions
	 */
public:
	void addBOFLink(const LLUUID& idFolder, LLPointer<LLInventoryCallback> cb = NULL);
	void purgeBOFLink();

	/*
	 * Member variables
	 */
protected:
	bool       m_fLinkAttachments;
	uuid_vec_t m_PendingAttachLinks;
	uuid_vec_t m_PendingWearableLinks;

protected:
	LLCOFMgr() : m_fLinkAttachments(false) {}
private:
	friend class LLCOFFetcher;
	friend class LLCOFLinkTargetFetcher;
	friend class LLDeferredAddLinkTargetFetcher;
	friend class LLLinkAttachmentCallback;
	friend class LLLinkWearableCallback;
	friend class LLSingleton<LLCOFMgr>;
};

// ============================================================================
