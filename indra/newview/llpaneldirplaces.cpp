/** 
 * @file llpaneldirplaces.cpp
 * @brief "Places" panel in the Find directory (not popular places, just places)
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

#include "llviewerprecompiledheaders.h"

#include "llpaneldirplaces.h"

// linden library includes
#include "llfontgl.h"
#include "message.h"
#include "lldir.h"
#include "llparcel.h"
#include "llregionflags.h"
#include "llqueryflags.h"

// viewer project includes
#include "llagent.h"
#include "llagentaccess.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterdirectory.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llviewerwindow.h"
#include "llpaneldirbrowser.h"
#include "lltextbox.h"
#include "lluiconstants.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "llworldmap.h"

LLPanelDirPlaces::LLPanelDirPlaces(const std::string& name, LLFloaterDirectory* floater)
	:	LLPanelDirBrowser(name, floater)
{
	// MBW -- HACK!!!
	// This looks like some sort of compiler bug.  We have a mysterious crash during initialization on powerpc boxes.
	// The crash seems unrelated to this code, but the commit that introduced it was narrowed down to this file.
	// Adding llinfos calls to both the constructor and destructor here makes the crash go away, even though they don't get called before the point of the crash.
	// This is wrong on many levels and scares the hell out of me.
	llinfos << "called" << llendl;
	mMinSearchChars = 3;
}

BOOL LLPanelDirPlaces::postBuild()
{
	LLPanelDirBrowser::postBuild();

	childSetKeystrokeCallback("name", &LLPanelDirBrowser::onKeystrokeName, this);

	childSetAction("Search", &LLPanelDirBrowser::onClickSearchCore, this);
	childDisable("Search");

	mCurrentSortColumn = "dwell";
	mCurrentSortAscending = FALSE;
	
	if (gAgent.getAgentAccess().isInTransition())
	{
		// during the AO transition, this combo has an Adult item.
		// Post-transition, it goes away. We can remove this conditional
		// after the transition and just use the "else" clause.
		childSetVisible("Category_Adult", true);
		childSetEnabled("Category_Adult", true);
	}
	else
	{
		// this is the code that should be preserved post-transition
		childSetVisible("Category", true);
		childSetEnabled("Category", true);
	}
	
	// Don't prepopulate the places list, as it hurts the database as of 2006-12-04. JC
	// initialQuery();

	return TRUE;
}

LLPanelDirPlaces::~LLPanelDirPlaces()
{
	// MBW -- HACK!!!
	// This looks like some sort of compiler bug.  We have a mysterious crash during initialization on powerpc boxes.
	// The crash seems unrelated to this code, but the commit that introduced it was narrowed down to this file.
	// Adding llinfos calls to both the constructor and destructor here makes the crash go away, even though they don't get called before the point of the crash.
	// This is wrong on many levels and scares the hell out of me.
	llinfos << "called" << llendl;
	// Children all cleaned up by default view destructor.
}


// virtual
void LLPanelDirPlaces::draw()
{
	updateMaturityCheckbox();

	LLPanelDirBrowser::draw();
}

// virtual
void LLPanelDirPlaces::performQuery()
{
	std::string place_name = childGetValue("name").asString();
	if (place_name.length() < mMinSearchChars)
	{
		return;
	}

    // "hi " is three chars but not a long-enough search
	std::string query_string = place_name;
	LLStringUtil::trim( query_string );
	bool query_was_filtered = (query_string != place_name);

	// possible we threw away all the short words in the query so check length
	if ( query_string.length() < mMinSearchChars )
	{
		LLNotificationsUtil::add("SeachFilteredOnShortWordsEmpty");
		return;
	};

	// if we filtered something out, display a popup
	if ( query_was_filtered )
	{
		LLSD args;
		args["FINALQUERY"] = query_string;
		LLNotificationsUtil::add("SeachFilteredOnShortWords", args);
	};

	std::string catstring = childGetValue("Category").asString();
	if (gAgent.getAgentAccess().isInTransition())
	{
		// during the AO transition, this combo has an Adult item.
		// Post-transition, it goes away. We can remove this conditional
		// after the transition and just use the "else" clause.
		catstring = childGetValue("Category_Adult").asString();
	}
	else
	{
		// this is the code that should be preserved post-transition
		catstring = childGetValue("Category").asString();
	}
	
	
	// Because LLParcel::C_ANY is -1, must do special check
	S32 category = 0;
	if (catstring == "any")
	{
		category = LLParcel::C_ANY;
	}
	else
	{
		category = LLParcel::getCategoryFromString(catstring);
	}

	U32 flags = 0x0;
	bool adult_enabled = gAgent.canAccessAdult();
	bool mature_enabled = gAgent.canAccessMature();

	if (gSavedSettings.getBOOL("ShowPGSims") ||
	    (!adult_enabled && !mature_enabled)) // if they can't have either of the others checked, force this one true 
	{
		flags |= DFQ_INC_PG;
	}

	if( gSavedSettings.getBOOL("ShowMatureSims") && mature_enabled)
	{
		flags |= DFQ_INC_MATURE;
	}

	if( gSavedSettings.getBOOL("ShowAdultSims") && adult_enabled)
	{
		flags |= DFQ_INC_ADULT;
	}
	
	// Pack old query flag in case we are talking to an old server
	if ( ((flags & DFQ_INC_PG) == DFQ_INC_PG) && !((flags & DFQ_INC_MATURE) == DFQ_INC_MATURE) )
	{
		flags |= DFQ_PG_PARCELS_ONLY;
	}
 
	if (0x0 == flags)
	{
		LLNotificationsUtil::add("NoContentToSearch");
		return; 
	}
	
	queryCore(query_string, category, flags);
}

void LLPanelDirPlaces::initialQuery()
{
	// All Linden locations in PG/Mature sims, any name.
	U32 flags = DFQ_INC_PG | DFQ_INC_MATURE;
	queryCore(LLStringUtil::null, LLParcel::C_LINDEN, flags);
}

void LLPanelDirPlaces::queryCore(const std::string& name, 
								 S32 category, 
								 U32 flags)
{
	setupNewSearch();

// JC: Sorting by dwell severely impacts the performance of the query.
// Instead of sorting on the dataserver, we sort locally once the results
// are received.
// IW: Re-enabled dwell sort based on new 3-character minimum description
// Hopefully we'll move to next-gen Find before this becomes a big problem

	flags |= DFQ_DWELL_SORT;

	LLMessageSystem* msg = gMessageSystem;

	msg->newMessage("DirPlacesQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->nextBlock("QueryData");
	msg->addUUID("QueryID", getSearchID());
	msg->addString("QueryText", name);
	msg->addU32("QueryFlags", flags);
	msg->addS8("Category", (S8)category);
	// No longer support queries by region name, too many regions
	// for combobox, no easy way to do autocomplete. JC
	msg->addString("SimName", "");
	msg->addS32Fast(_PREHASH_QueryStart,mSearchStart);
	gAgent.sendReliableMessage();
}



