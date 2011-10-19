/** 
 *
 * Copyright (c) 2009-2010, Kitty Barnett
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

#include "llviewerprecompiledheaders.h"

#include "llagent.h"
#include "llcachename.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

#include "rlvfloaterbehaviour.h"
#include "rlvhandler.h"

// ============================================================================
// Helper functions
//

// Checked: 2010-03-11 (RLVa-1.1.3b) | Modified: RLVa-1.2.0g
std::string rlvGetItemNameFromObjID(const LLUUID& idObj, bool fIncludeAttachPt = true)
{
	const LLViewerObject* pObj = gObjectList.findObject(idObj);
	const LLViewerObject* pObjRoot = (pObj) ? pObj->getRootEdit() : NULL;
	const LLViewerInventoryItem* pItem = ((pObjRoot) && (pObjRoot->isAttachment())) ? gInventory.getItem(pObjRoot->getAttachmentItemID()) : NULL;

	std::string strItemName = (pItem) ? pItem->getName() : idObj.asString();
	if ( (!fIncludeAttachPt) || (!pObj) || (!pObj->isAttachment()) || (!gAgentAvatarp) )
		return strItemName;

	const LLViewerJointAttachment* pAttachPt = 
		get_if_there(gAgentAvatarp->mAttachmentPoints, RlvAttachPtLookup::getAttachPointIndex(pObjRoot), (LLViewerJointAttachment*)NULL);
	std::string strAttachPtName = (pAttachPt) ? pAttachPt->getName() : std::string("Unknown");
	return llformat("%s (%s, %s)", strItemName.c_str(), strAttachPtName.c_str(), (pObj == pObjRoot) ? "root" : "child");
}

// ============================================================================

RlvFloaterBehaviour::RlvFloaterBehaviour(const LLSD& key) 
	: LLFloater(std::string("rlvBehaviours"))
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_rlv_behaviour.xml");
}


void RlvFloaterBehaviour::show(void*)
{
	RlvFloaterBehaviour::showInstance();
}

// Checked: 2010-04-18 (RLVa-1.1.3b) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviour::refreshAll()
{
	LLCtrlListInterface* pBhvrList = childGetListInterface("behaviour_list");
	if (!pBhvrList)
		return;
	pBhvrList->operateOnAll(LLCtrlListInterface::OP_DELETE);

	if (!gAgentAvatarp)
		return;

	//
	// Set-up a row we can just reuse
	//
	LLSD sdRow;
	LLSD& sdColumns = sdRow["columns"];
	sdColumns[0]["column"] = "behaviour";   sdColumns[0]["type"] = "text";
	sdColumns[1]["column"] = "name"; sdColumns[1]["type"] = "text";

	//
	// List behaviours
	//
	const RlvHandler::rlv_object_map_t* pRlvObjects = gRlvHandler.getObjectMap();
	for (RlvHandler::rlv_object_map_t::const_iterator itObj = pRlvObjects->begin(), endObj = pRlvObjects->end(); itObj != endObj; ++itObj)
	{
		sdColumns[1]["value"] = rlvGetItemNameFromObjID(itObj->first);

		const rlv_command_list_t* pCommands = itObj->second.getCommandList();
		for (rlv_command_list_t::const_iterator itCmd = pCommands->begin(), endCmd = pCommands->end(); itCmd != endCmd; ++itCmd)
		{
			std::string strBhvr = itCmd->asString();
			
			LLUUID idOption(itCmd->getOption());
			if (idOption.notNull())
			{
				std::string strLookup;
				if ( (gCacheName->getFullName(idOption, strLookup)) || (gCacheName->getGroupName(idOption, strLookup)) )
				{
					if (strLookup.find("???") == std::string::npos)
						strBhvr.assign(itCmd->getBehaviour()).append(":").append(strLookup);
				}
				else if (m_PendingLookup.end() == m_PendingLookup.find(idOption))
				{
					m_PendingLookup[idOption] = gCacheName->get(idOption, false, boost::bind(&RlvFloaterBehaviour::onAvatarNameLookup,this,_1));
				}
			}

			sdColumns[0]["value"] = strBhvr;

			pBhvrList->addElement(sdRow, ADD_BOTTOM);
		}
	}
}

// ============================================================================
/*
 * LLFloater overrides
 */

BOOL RlvFloaterBehaviour::canClose()
{
	return !LLApp::isExiting();
}

void RlvFloaterBehaviour::onOpen()
{
	gRlvHandler.addBehaviourObserver(this);

	refreshAll();
}

void RlvFloaterBehaviour::onClose(bool fQuitting)
{
	LLFloater::setVisible(FALSE);

	gRlvHandler.removeBehaviourObserver(this);
		
	for (std::map<LLUUID, boost::signals2::connection>::const_iterator itLookup = m_PendingLookup.begin(); itLookup != m_PendingLookup.end(); ++itLookup)
	{
		itLookup->second.disconnect();
	}
	m_PendingLookup.clear();
}

BOOL RlvFloaterBehaviour::postBuild()
{
	return TRUE;
}

// ============================================================================
/*
 * RlvBehaviourObserver overrides
 */

void RlvFloaterBehaviour::changed(const RlvCommand& /*rlvCmd*/, bool /*fInternal*/)
{
	refreshAll();
}

// ============================================================================

void RlvFloaterBehaviour::onAvatarNameLookup(const LLUUID& uuid)
{
	std::map<LLUUID, boost::signals2::connection>::iterator itLookup = m_PendingLookup.find(uuid);
	if (itLookup != m_PendingLookup.end())
	{
		itLookup->second.disconnect();
		m_PendingLookup.erase(itLookup);
	}

	refreshAll();
}

// ============================================================================
