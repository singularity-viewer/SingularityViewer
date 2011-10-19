/** 
 * @file llfloatergroupinfo.cpp
 * @brief LLFloaterGroupInfo class implementation
 * Floater used both for display of group information and for
 * creating new groups.
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

#include "llviewerprecompiledheaders.h"

#include "llfloatergroupinfo.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloaterchatterbox.h"
#include "llpanelgroup.h"
#include "llviewermessage.h" // for inventory_offer_callback
//#include "llviewerwindow.h"
#include "llnotifications.h"

#include "llcachename.h"

const char FLOATER_TITLE[] = "Group Information";
const LLRect FGI_RECT(0, 530, 420, 0);

//
// Globals
//
std::map<LLUUID, LLFloaterGroupInfo*> LLFloaterGroupInfo::sInstances;

class LLGroupHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLGroupHandler() : LLCommandHandler("group", true) { }
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (tokens.size() < 1)
		{
			return false;
		}

		if (tokens[0].asString() == "create")
		{
			LLFloaterGroupInfo::showCreateGroup(NULL);
			return true;
		}

		if (tokens.size() < 2)
		{
			return false;
		}

		if (tokens[0].asString() == "list")
		{
			if (tokens[1].asString() == "show")
			{
				// CP_TODO: get the value we pass in via the XUI name 
				// of the tab instead of using a literal like this
				LLFloaterMyFriends::showInstance( 1 );

				return true;
			}
            return false;
		}

		LLUUID group_id;
		if (!group_id.set(tokens[0], FALSE))
		{
			return false;
		}

		if (tokens[1].asString() == "about")
		{
			LLFloaterGroupInfo::showFromUUID(group_id);
			return true;
		}
		return false;
	}
};
LLGroupHandler gGroupHandler;

//-----------------------------------------------------------------------------
// Implementation
//-----------------------------------------------------------------------------
LLFloaterGroupInfo::LLFloaterGroupInfo(const std::string& name, const LLRect &rect, const std::string& title, const LLUUID& group_id, const std::string& tab_name)
:	LLFloater(name, rect, title),
	mGroupID( group_id )
{
	// Construct the filename of the group panel xml definition file.
	mPanelGroupp = new LLPanelGroup("panel_group.xml",
									"PanelGroup",
									group_id,
									tab_name);
	addChild(mPanelGroupp);
}

// virtual
LLFloaterGroupInfo::~LLFloaterGroupInfo()
{
	sInstances.erase(mGroupID);
}

BOOL LLFloaterGroupInfo::canClose()
{
	// Ask the panel if it is ok to close.
	if ( mPanelGroupp )
	{
		return mPanelGroupp->canClose();
	}
	return TRUE;
}

void LLFloaterGroupInfo::selectTabByName(std::string tab_name)
{
	mPanelGroupp->selectTab(tab_name);
}

// static
void LLFloaterGroupInfo::showMyGroupInfo(void *)
{
	showFromUUID( gAgent.getGroupID() );
}

// static
void LLFloaterGroupInfo::showCreateGroup(void *)
{
	showFromUUID(LLUUID::null, "general_tab");
}

// static
void LLFloaterGroupInfo::closeGroup(const LLUUID& group_id)
{
	LLFloaterGroupInfo *fgi = get_if_there(sInstances, group_id, (LLFloaterGroupInfo*)NULL);
	if (fgi)
	{
		if (fgi->mPanelGroupp)
		{
			fgi->mPanelGroupp->close();
		}
	}
}

// static
void LLFloaterGroupInfo::closeCreateGroup()
{
	closeGroup(LLUUID::null);
}

// static 
void LLFloaterGroupInfo::refreshGroup(const LLUUID& group_id)
{
	LLFloaterGroupInfo *fgi = get_if_there(sInstances, group_id, (LLFloaterGroupInfo*)NULL);
	if (fgi)
	{
		if (fgi->mPanelGroupp)
		{
			fgi->mPanelGroupp->refreshData();
		}
	}
}

// static
void LLFloaterGroupInfo::callbackLoadGroupName(const LLUUID& id, const std::string& full_name, bool is_group)
{
	LLFloaterGroupInfo *fgi = get_if_there(sInstances, id, (LLFloaterGroupInfo*)NULL);

	if (fgi)
	{
		// Build a new title including the group name.
		std::ostringstream title;
		title << full_name << " - " << FLOATER_TITLE;
		fgi->setTitle(title.str());
	}
}

// static
void LLFloaterGroupInfo::showFromUUID(const LLUUID& group_id,
									  const std::string& tab_name)
{
	// If we don't have a floater for this group, create one.
	LLFloaterGroupInfo *fgi = get_if_there(sInstances, group_id, (LLFloaterGroupInfo*)NULL);
	if (!fgi)
	{
		fgi = new LLFloaterGroupInfo("groupinfo",
									 FGI_RECT,
									 FLOATER_TITLE,
									 group_id,
									 tab_name);

		sInstances[group_id] = fgi;

		if (group_id.notNull())
		{
			// Look up the group name.
			// The callback will insert it into the title.
			gCacheName->get(group_id, true, boost::bind(&callbackLoadGroupName, _1, _2, _3));
		}
	}

	fgi->selectTabByName(tab_name);
	
	fgi->center();
	fgi->open();	/*Flawfinder: ignore*/
}

// static 
void LLFloaterGroupInfo::showNotice(const std::string& subject,
									const std::string& message,
									const LLUUID& group_id,
									const bool& has_inventory,
									const std::string& inventory_name,
									LLOfferInfo* inventory_offer)
{
	llinfos << "LLFloaterGroupInfo::showNotice : " << subject << llendl;

	if (group_id.isNull())
	{
		// We need to clean up that inventory offer.
		if (inventory_offer)
		{
			inventory_offer->forceResponse(IOR_DECLINE);
		}
		return;
	}

	// If we don't have a floater for this group, drop this packet on the floor.
	LLFloaterGroupInfo *fgi = get_if_there(sInstances, group_id, (LLFloaterGroupInfo*)NULL);
	if (!fgi)
	{
		// We need to clean up that inventory offer.
		if (inventory_offer)
		{
			inventory_offer->forceResponse(IOR_DECLINE);
		}
		return;
	}
	
	fgi->mPanelGroupp->showNotice(subject,message,has_inventory,inventory_name,inventory_offer);
}
