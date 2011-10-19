/** 
 * @file llfloaterteleporthistory.cpp
 * @author Zi Ree
 * @brief LLFloaterTeleportHistory class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2008, Linden Research, Inc.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#include "linden_common.h"

#include <algorithm>

//MK
#include "llworld.h"
#include "lleventpoll.h"
#include "llagent.h"
//mk
#include "llappviewer.h"
#include "llfloaterteleporthistory.h"
#include "llfloaterworldmap.h"
#include "lltimer.h"
#include "lluictrlfactory.h"
#include "llurldispatcher.h"
#include "llurlsimstring.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llweb.h"
#include "llsdserialize.h"
// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

LLFloaterTeleportHistory::LLFloaterTeleportHistory(const LLSD& seed)
:	LLFloater(std::string("teleporthistory")),
	mPlacesList(NULL),
	mID(0)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_teleport_history.xml", NULL);
}

// virtual
LLFloaterTeleportHistory::~LLFloaterTeleportHistory()
{
}

// virtual
void LLFloaterTeleportHistory::onFocusReceived()
{
	// take care to enable or disable buttons depending on the selection in the places list
	if (mPlacesList->getFirstSelected())
	{
		setButtonsEnabled(TRUE);
	}
	else
	{
		setButtonsEnabled(FALSE);
	}
	LLFloater::onFocusReceived();
}

BOOL LLFloaterTeleportHistory::postBuild()
{
	// make sure the cached pointer to the scroll list is valid
	mPlacesList=getChild<LLScrollListCtrl>("places_list");
	if (!mPlacesList)
	{
		llwarns << "coud not get pointer to places list" << llendl;
		return FALSE;
	}

	// setup callbacks for the scroll list
	mPlacesList->setDoubleClickCallback(onTeleport);
	childSetCommitCallback("places_list", onPlacesSelected, this);
	childSetAction("teleport", onTeleport, this);
	childSetAction("show_on_map", onShowOnMap, this);
	childSetAction("copy_slurl", onCopySLURL, this);

	return TRUE;
}

void LLFloaterTeleportHistory::addPendingEntry(std::string regionName, S16 x, S16 y, S16 z)
{
// [RLVa:KB]
	if(gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		return;
// [/RLVa:KB]


	// Set pending entry timestamp
	U32 utc_time;
	utc_time = time_corrected();
	struct tm* internal_time;
	internal_time = utc_to_pacific_time(utc_time, gPacificDaylightTime);
	// check if we are in daylight savings time
	std::string timeZone = " PST";
	if (gPacificDaylightTime)
	{
		timeZone = " PDT";
	}
#ifdef LOCALIZED_TIME
	timeStructToFormattedString(internal_time, gSavedSettings.getString("LongTimeFormat"), mPendingTimeString);
	mPendingTimeString += timeZone;
#else
	mPendingTimeString = llformat("%02d:%02d:%02d", internal_time->tm_hour, internal_time->tm_min, internal_time->tm_sec) + timeZone;
#endif

	// Set pending region name
	mPendingRegionName = regionName;

	// Set pending position
	mPendingPosition = llformat("%d, %d, %d", x, y, z);

	// prepare simstring for later parsing
	mPendingSimString = regionName + llformat("/%d/%d/%d", x, y, z); 
	mPendingSimString = LLWeb::escapeURL(mPendingSimString);

	// Prepare the SLURL
	mPendingSLURL = LLURLDispatcher::buildSLURL(regionName, x, y, z);
}

void LLFloaterTeleportHistory::addEntry(std::string parcelName)
{
	if (mPendingRegionName.empty())
	{
		return;
	}

 	// only if the cached scroll list pointer is valid
	if (mPlacesList)
	{
		// build the list entry
		LLSD value;
		value["id"] = mID;
		value["columns"][LIST_PARCEL]["column"] = "parcel";
		value["columns"][LIST_PARCEL]["value"] = parcelName;
		value["columns"][LIST_REGION]["column"] = "region";
		value["columns"][LIST_REGION]["value"] = mPendingRegionName;
		value["columns"][LIST_POSITION]["column"] = "position";
		value["columns"][LIST_POSITION]["value"] = mPendingPosition;
		value["columns"][LIST_VISITED]["column"] = "visited";
		value["columns"][LIST_VISITED]["value"] = mPendingTimeString;

		// these columns are hidden and serve as data storage for simstring and SLURL
		value["columns"][LIST_SLURL]["column"] = "slurl";
		value["columns"][LIST_SLURL]["value"] = mPendingSLURL;
		value["columns"][LIST_SIMSTRING]["column"] = "simstring";
		value["columns"][LIST_SIMSTRING]["value"] = mPendingSimString;

		// add the new list entry on top of the list, deselect all and disable the buttons
		const S32 max_entries = gSavedSettings.getS32("TeleportHistoryMaxEntries");
		S32 num_entries = mPlacesList->getItemCount();
		while(num_entries >= max_entries)
		{
			mPlacesList->deleteItems(LLSD(mID - num_entries--));
		}

		mPlacesList->addElement(value, ADD_TOP);
		mPlacesList->deselectAllItems(TRUE);
		setButtonsEnabled(FALSE);
		mID++;
		saveFile("teleport_history.xml");	//Comment out to disable saving after every teleport.
	}
	else
	{
		llwarns << "pointer to places list is NULL" << llendl;
	}

	mPendingRegionName.clear();
}

void LLFloaterTeleportHistory::setButtonsEnabled(BOOL on)
{
	// enable or disable buttons
	childSetEnabled("teleport", on);
	childSetEnabled("show_on_map", on);
	childSetEnabled("copy_slurl", on);
}
//static
void LLFloaterTeleportHistory::loadFile(const std::string &file_name)
{
	LLFloaterTeleportHistory *pFloaterHistory = LLFloaterTeleportHistory::findInstance();
	if(!pFloaterHistory)
		return;

	LLScrollListCtrl* pScrollList = pFloaterHistory->mPlacesList;
	if(!pScrollList)
		return;

	std::string temp_str(gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + file_name);

	llifstream file(temp_str);

	if (file.is_open())
	{
		llinfos << "Loading "<< file_name << " file at " << temp_str << llendl;
		LLSD data;
		LLSDSerialize::fromXML(data, file);
		if (data.isUndefined())
		{
			llinfos << "file missing, ill-formed, "
				"or simply undefined; not reading the"
				" file" << llendl;
		}
		else
		{
			const S32 max_entries = gSavedSettings.getS32("TeleportHistoryMaxEntries");
			const S32 num_entries = llmin(max_entries,(const S32)data.size());
			pScrollList->clear();
			for(S32 i = 0; i < num_entries; i++) //Lower entry = newer
			{
				pScrollList->addElement(data[i], ADD_BOTTOM);
			}
			pScrollList->deselectAllItems(TRUE);
			pFloaterHistory->mID = pScrollList->getItemCount();
			pFloaterHistory->setButtonsEnabled(FALSE);
		}
	}
}

struct SortByAge{
  inline bool operator() (LLScrollListItem* const i,LLScrollListItem* const j) const {
	  return (i->getValue().asInteger()>j->getValue().asInteger());
  }
};

//static
void LLFloaterTeleportHistory::saveFile(const std::string &file_name)
{
	LLFloaterTeleportHistory *pFloaterHistory = LLFloaterTeleportHistory::findInstance();
	if(!pFloaterHistory)
		return;

	std::string temp_str = gDirUtilp->getLindenUserDir(true);
	if( temp_str.empty() )
	{
		llinfos << "Can't save teleport history - no user directory set yet." << llendl;
		return;
	}
	temp_str += gDirUtilp->getDirDelimiter() + file_name;
	llofstream export_file(temp_str);
	if (!export_file.good())
	{
		llwarns << "Unable to open " << file_name << " for output." << llendl;
		return;
	}
	llinfos << "Writing "<< file_name << " file at " << temp_str << llendl;

	LLSD elements;
	LLScrollListCtrl* pScrollList = pFloaterHistory->mPlacesList;
	if (pScrollList)
	{
		std::vector<LLScrollListItem*> data_list = pScrollList->getAllData();
		std::sort(data_list.begin(),data_list.end(),SortByAge());//Re-sort. Column sorting may have mucked the list up. Newer entries in front.
		for (std::vector<LLScrollListItem*>::iterator itr = data_list.begin(); itr != data_list.end(); ++itr)
		{
			//Pack into LLSD mimicing one passed to addElement
			LLSD data_entry;
			//id is actually reverse of the indexing of the element LLSD. Higher id = newer.
			data_entry["id"] = pScrollList->getItemCount() - elements.size() - 1;	
			for(S32 i = 0; i < (*itr)->getNumColumns(); ++i)	
			{
				data_entry["columns"][i]["column"]=pScrollList->getColumn(i)->mName;
				data_entry["columns"][i]["value"]=(*itr)->getColumn(i)->getValue();
			}
			elements.append(data_entry);
		}
	}
	LLSDSerialize::toXML(elements, export_file);
	export_file.close();
}

// virtual
void LLFloaterTeleportHistory::onClose(bool app_quitting)
{
	LLFloater::setVisible(FALSE);
}

// virtual
BOOL LLFloaterTeleportHistory::canClose()
{
	return !LLApp::isExiting();
}

// callbacks

// static
void LLFloaterTeleportHistory::onPlacesSelected(LLUICtrl* /* ctrl */, void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// on selection change check if we need to enable or disable buttons
	if (self->mPlacesList->getFirstSelected())
	{
		self->setButtonsEnabled(TRUE);
	}
	else
	{
		self->setButtonsEnabled(FALSE);
	}
}

// static
void LLFloaterTeleportHistory::onTeleport(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// build secondlife::/app link from simstring for instant teleport to destination
	std::string slapp = "secondlife:///app/teleport/" + self->mPlacesList->getFirstSelected()->getColumn(LIST_SIMSTRING)->getValue().asString();
	LLMediaCtrl* web = NULL;
	LLURLDispatcher::dispatch(slapp, web, TRUE);
}

// static
void LLFloaterTeleportHistory::onShowOnMap(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// get simstring from selected entry and parse it for its components
	std::string simString = self->mPlacesList->getFirstSelected()->getColumn(LIST_SIMSTRING)->getValue().asString();
	std::string region = "";
	S32 x = 128;
	S32 y = 128;
	S32 z = 20;

	LLURLSimString::parse(simString, &region, &x, &y, &z);

	// point world map at position
	gFloaterWorldMap->trackURL(region, x, y, z);
	LLFloaterWorldMap::show(NULL, TRUE);
}

// static
void LLFloaterTeleportHistory::onCopySLURL(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// get SLURL of the selected entry and copy it to the clipboard
	std::string SLURL = self->mPlacesList->getFirstSelected()->getColumn(LIST_SLURL)->getValue().asString();
	gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(SLURL));
}
