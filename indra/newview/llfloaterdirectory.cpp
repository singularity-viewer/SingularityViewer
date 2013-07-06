/** 
 * @file llfloaterdirectory.cpp
 * @brief The "Find" floater.  Should be llfloaterfind.
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

#include "llfloaterdirectory.h"

#include "llfloatersearch.h" // For callback to open web search
#include "llpaneldirfind.h"
#include "llpaneldirevents.h"
#include "llpaneldirland.h"
#include "llpaneldirpeople.h"
#include "llpaneldirpopular.h"
#include "llpaneldirgroups.h"
#include "llpaneldirplaces.h"
#include "llpaneldirclassified.h"

#include "llagent.h"
#include "llpanelavatar.h"
#include "llpanelevent.h"
#include "llpanelclassified.h"
#include "llpanelgroup.h"
#include "llpanelplace.h"
#include "lluictrlfactory.h"

#include "hippogridmanager.h"
#include "llenvmanager.h"
#include "llnotificationsutil.h"
#include "llviewerregion.h"

const char* market_panel = "market_panel";

class LLPanelDirMarket : public LLPanelDirFind
{
public:
	LLPanelDirMarket(const std::string& name, LLFloaterDirectory* floater)
	: LLPanelDirFind(name, floater, "market_browser")
	{}

	/*virtual*/ void search(const std::string& url)
	{
		if (url.empty()) navigateToDefaultPage();
	}

	/*virtual*/ void navigateToDefaultPage()
	{
		if (mWebBrowser && !mMarketplaceURL.empty()) mWebBrowser->navigateTo(mMarketplaceURL);
	}

	/*virtual*/ BOOL postBuild()
	{
		if (gHippoGridManager->getConnectedGrid()->isSecondLife())
		{
			mMarketplaceURL = getString("default_search_page");
		}
		else
		{
			getChild<LLUICtrl>("reset_btn")->setCommitCallback(boost::bind(&LLPanelDirMarket::navigateToDefaultPage, this));
		}
		return LLPanelDirFind::postBuild();
	}

	void handleRegionChange(LLTabContainer* container)
	{
		if (LLViewerRegion* region = gAgent.getRegion())
		{
			if (region->getFeaturesReceived())
			{
				setMarketplaceURL(container);
			}
			else
			{
				region->setFeaturesReceivedCallback(boost::bind(&LLPanelDirMarket::setMarketplaceURL, this, container));
			}
		}
	}

	void setMarketplaceURL(LLTabContainer* container)
	{
		if (LLViewerRegion* region = gAgent.getRegion())
		{
			LLSD info;
			region->getSimulatorFeatures(info);
			if (info.has("MarketplaceURL"))
			{
				std::string url = info["MarketplaceURL"].asString();
				if (mMarketplaceURL == url) return;

				if (mMarketplaceURL.empty())
				{
					container->addTabPanel(this, getLabel());
					mMarketplaceURL = url;
					navigateToDefaultPage();
				}
				else
				{
					LLNotificationsUtil::add("MarketplaceURLChanged", LLSD(), LLSD(),
							boost::bind(&LLPanelDirMarket::onConfirmChangeMarketplaceURL, this, boost::bind(LLNotificationsUtil::getSelectedOption, _1, _2), url));
				}
			}
			else if (!mMarketplaceURL.empty())
			{
				if (gFloaterView->getParentFloater(this)->getVisible()) // Notify the user that they're no longer on the region with the marketplace when search is open
				{
					LLNotificationsUtil::add("MarketplaceURLGone");
				}
				else // Search is not in use, remove the marketplace
				{
					mMarketplaceURL = "";
					container->removeTabPanel(this);
				}
			}
		}
	}

	void onConfirmChangeMarketplaceURL(const S32 option, const std::string& url)
	{
		if (option == 1) return; //no
		else //yes
		{
			mMarketplaceURL = url;
			if (option == 2) navigateToDefaultPage();
		}
	}

	static void* create(void* data)
	{
		return new LLPanelDirMarket(market_panel, static_cast<LLFloaterDirectory*>(data));
	}

private:
	std::string mMarketplaceURL;
};

static void* createWebPanel(void* data)
{
	struct LLPanelDirWeb : public LLPanelDirFind
	{
		LLPanelDirWeb(LLFloaterDirectory* f) : LLPanelDirFind("web_panel", f, "web_search_browser") {}
		/*virtual*/ void search(const std::string& url) {}
		/*virtual*/ void navigateToDefaultPage()
		{
			if (mWebBrowser) mWebBrowser->navigateTo(getString("default_search_page"));
		}
	};
	return new LLPanelDirWeb(static_cast<LLFloaterDirectory*>(data));
}

LLFloaterDirectory* LLFloaterDirectory::sInstance = NULL;
//static
S32 LLFloaterDirectory::sOldSearchCount = 0; // debug
S32 LLFloaterDirectory::sNewSearchCount = 0; // debug


LLFloaterDirectory::LLFloaterDirectory(const std::string& name)
:	LLFloater(name, std::string("FloaterFindRect2"), LLStringUtil::null),
	mMinimizing(false)
{
	sInstance = this;

	mFindAllPanel = NULL;
	mClassifiedPanel = NULL;
	mEventsPanel = NULL;
	mLandPanel = NULL;

	mPanelAvatarp = NULL;
	mPanelEventp = NULL;
	mPanelGroupp = NULL;
	mPanelGroupHolderp = NULL;
	mPanelPlacep = NULL;
	mPanelPlaceSmallp = NULL;
	mPanelClassifiedp = NULL;
	
	// Build the floater with our tab panel classes
	
	bool enableWebSearch = gHippoGridManager->getConnectedGrid()->isSecondLife() ||
						   !gHippoGridManager->getConnectedGrid()->getSearchUrl().empty();
	bool enableClassicAllSearch = !gHippoGridManager->getConnectedGrid()->isSecondLife();

	LLCallbackMap::map_t factory_map;
	factory_map["classified_panel"] = LLCallbackMap(createClassified, this);
	factory_map["events_panel"] = LLCallbackMap(createEvents, this);
	factory_map["places_panel"] = LLCallbackMap(createPlaces, this);
	factory_map["land_sales_panel"] = LLCallbackMap(createLand, this);
	factory_map["people_panel"] = LLCallbackMap(createPeople, this);
	factory_map["groups_panel"] = LLCallbackMap(createGroups, this);
	factory_map[market_panel] = LLCallbackMap(LLPanelDirMarket::create, this);
	if (enableWebSearch)
	{
		// web search and showcase only for SecondLife
		factory_map["find_all_panel"] = LLCallbackMap(createFindAll, this);
		factory_map["showcase_panel"] = LLCallbackMap(createShowcase, this);		
		if (!enableClassicAllSearch) factory_map["web_panel"] = LLCallbackMap(createWebPanel, this);
	}

	if (enableClassicAllSearch)
	{
		factory_map["find_all_old_panel"] = LLCallbackMap(createFindAllOld, this);
	}

	factory_map["classified_details_panel"] = LLCallbackMap(createClassifiedDetail, this);
	factory_map["event_details_panel"] = LLCallbackMap(createEventDetail, this);
	factory_map["group_details_panel"] = LLCallbackMap(createGroupDetail, this);
	factory_map["group_details_panel_holder"] = LLCallbackMap(createGroupDetailHolder, this);
	factory_map["place_details_panel"] = LLCallbackMap(createPlaceDetail, this);
	factory_map["place_details_small_panel"] = LLCallbackMap(createPlaceDetailSmall, this);

	factory_map["Panel Avatar"] = LLCallbackMap(createPanelAvatar, this);
	
	if (enableWebSearch)
	{
		mCommitCallbackRegistrar.add("Search.WebFloater", boost::bind(&LLFloaterSearch::open, boost::bind(LLFloaterSearch::getInstance)));
		if (enableClassicAllSearch)
			LLUICtrlFactory::getInstance()->buildFloater(this, "floater_directory3.xml", &factory_map);
		else
			LLUICtrlFactory::getInstance()->buildFloater(this, "floater_directory.xml", &factory_map);
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_directory2.xml", &factory_map);
	}
	moveResizeHandlesToFront();

	if(mPanelAvatarp)
	{
		mPanelAvatarp->selectTab(0);
	}
	
	LLTabContainer* container = getChild<LLTabContainer>("Directory Tabs");
	if (enableClassicAllSearch)
	{
		LLPanelDirMarket* marketp = static_cast<LLPanelDirMarket*>(container->getPanelByName(market_panel));
		container->removeTabPanel(marketp); // Until we get a MarketPlace URL, tab is removed.
		marketp->handleRegionChange(container);
		LLEnvManagerNew::instance().setRegionChangeCallback(boost::bind(&LLPanelDirMarket::handleRegionChange, marketp, container));
	}
	container->setCommitCallback(boost::bind(&LLFloaterDirectory::onTabChanged,_2));
}

LLFloaterDirectory::~LLFloaterDirectory()
{
	sInstance = NULL;

	//Note: this function is defined in the class LLFloater.
	//however, it causes crash if this line is postponed to ~LLFloater(). 
	//because it uses some pointers deleted below. That is, those pointers are used again after deleting.
	setMinimized( FALSE );

	delete mPanelAvatarp; mPanelAvatarp = NULL;
	delete mPanelEventp; mPanelEventp = NULL;
	delete mPanelGroupp; mPanelGroupp = NULL;
	delete mPanelGroupHolderp; mPanelGroupHolderp = NULL;
	delete mPanelPlacep; mPanelPlacep = NULL;
	delete mPanelPlaceSmallp; mPanelPlaceSmallp = NULL;
	delete mPanelClassifiedp; mPanelClassifiedp = NULL;
	gSavedSettings.setBOOL("ShowDirectory", FALSE);
}

// static
void *LLFloaterDirectory::createFindAll(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mFindAllPanel = LLPanelDirFindAllInterface::create(self);
	return self->mFindAllPanel;
}

// static
void* LLFloaterDirectory::createClassified(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mClassifiedPanel = new LLPanelDirClassified("classified_panel", self);
	return self->mClassifiedPanel;
}

// static
void* LLFloaterDirectory::createEvents(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mEventsPanel = new LLPanelDirEvents("events_panel", self);
	return self->mEventsPanel;
}

// static
void* LLFloaterDirectory::createShowcase(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	return new LLPanelDirPopular("showcase_panel", self);
}

// static
void* LLFloaterDirectory::createPlaces(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	return new LLPanelDirPlaces("places_panel", self);
}

// static
void* LLFloaterDirectory::createLand(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mLandPanel = new LLPanelDirLand("land_panel", self);
	return self->mLandPanel;
}


// static
void* LLFloaterDirectory::createPeople(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	return new LLPanelDirPeople("people_panel", self);
}

// static
void* LLFloaterDirectory::createGroups(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	return new LLPanelDirGroups("groups_panel", self);
}

// static
void *LLFloaterDirectory::createFindAllOld(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	return new LLPanelDirFindAllOld("find_all_old_panel", self);
}

// static
void* LLFloaterDirectory::createClassifiedDetail(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mPanelClassifiedp = new LLPanelClassified(true, false);
	self->mPanelClassifiedp->setVisible(FALSE);
	return self->mPanelClassifiedp;
}

//----------------------------------------------------------------------------

void*	LLFloaterDirectory::createPanelAvatar(void*	data)
{
	LLFloaterDirectory* self = (LLFloaterDirectory*)data;
	self->mPanelAvatarp = new LLPanelAvatar("Avatar", LLRect(), 
		FALSE); // disallow editing in search context (SL-48632)
	self->mPanelAvatarp->setVisible(FALSE);
	return self->mPanelAvatarp;

}


//----------------------------------------------------------------------------

// static
void* LLFloaterDirectory::createEventDetail(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mPanelEventp = new LLPanelEvent();
	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelEventp, "panel_event.xml");
	self->mPanelEventp->setVisible(FALSE);
	return self->mPanelEventp;
}

// static
void* LLFloaterDirectory::createGroupDetail(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mPanelGroupp = new LLPanelGroup(gAgent.getGroupID());
	self->mPanelGroupp->setAllowEdit(false); // Singu Note: This setting actually just tells the panel whether or not it is in search
	self->mPanelGroupp->setVisible(FALSE);
	return self->mPanelGroupp;
}

// static
void* LLFloaterDirectory::createGroupDetailHolder(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mPanelGroupHolderp = new LLPanel(std::string("PanelGroupHolder"));
	self->mPanelGroupHolderp->setVisible(FALSE);
	return self->mPanelGroupHolderp;
}

// static
void* LLFloaterDirectory::createPlaceDetail(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mPanelPlacep = new LLPanelPlace();
	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelPlacep, "panel_place.xml");
	self->mPanelPlacep->setVisible(FALSE);
	return self->mPanelPlacep;
}

// static
void* LLFloaterDirectory::createPlaceDetailSmall(void* userdata)
{
	LLFloaterDirectory *self = (LLFloaterDirectory*)userdata;
	self->mPanelPlaceSmallp = new LLPanelPlace();
	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelPlaceSmallp, "panel_place_small.xml");
	self->mPanelPlaceSmallp->setVisible(FALSE);
	return self->mPanelPlaceSmallp;
}

// static
void LLFloaterDirectory::requestClassifieds()
{
	if (sInstance && sInstance->mClassifiedPanel)
	{
		sInstance->mClassifiedPanel->performQuery();
	}
}

void LLFloaterDirectory::showFindAll(const std::string& search_text)
{
	showPanel("find_all_panel");
	LLPanelDirFindAllInterface::search(sInstance->mFindAllPanel, search_text);
}


void LLFloaterDirectory::showClassified(const LLUUID& classified_id)
{
	showPanel("classified_panel");

	if (sInstance->mClassifiedPanel)
	{
		sInstance->mClassifiedPanel->selectByUUID(classified_id);
	}
}

// static
void LLFloaterDirectory::showClassified(const std::string& search_text)
{
	performQueryOn("classified_panel", search_text);
}

// static
void LLFloaterDirectory::showEvents(S32 event_id)
{
	showPanel("events_panel"); 

	if (sInstance->mEventsPanel)
	{
		// HACK: force query for today's events
		sInstance->mEventsPanel->setDay(0);
		sInstance->mEventsPanel->performQuery();
		if (event_id != 0)
		{
			sInstance->mEventsPanel->selectEventByID(event_id);	
		}
	}
}

// static
void LLFloaterDirectory::showEvents(const std::string& search_text)
{
	performQueryOn("events_panel", search_text);
}

// static
void LLFloaterDirectory::showLandForSale(const LLUUID& parcel_id)
{
	showPanel("land_sales_panel");

	if (sInstance->mLandPanel)
	{
		sInstance->mLandPanel->selectByUUID(parcel_id);
	}
}

// static
void LLFloaterDirectory::showDestinations()
{
	showPanel("showcase_panel");
}

// static
void LLFloaterDirectory::showGroups(const std::string& search_text)
{
	performQueryOn("groups_panel", search_text);
}

// static
void LLFloaterDirectory::showPeople(const std::string& search_text)
{
	performQueryOn("people_panel", search_text);
}

// static
void LLFloaterDirectory::showPlaces(const std::string& search_text)
{
	performQueryOn("places_panel", search_text);
}

//static
void LLFloaterDirectory::performQueryOn(const std::string& name, const std::string& search_text)
{
	showPanel(name);
	if (search_text.empty()) return; // We're done here.
	LLPanelDirBrowser* panel = sInstance->getChild<LLPanelDirBrowser>(name);
	panel->getChild<LLUICtrl>("name")->setValue(search_text);
	panel->performQuery();
}

// static
void LLFloaterDirectory::refreshGroup(const LLUUID& group_id)
{
	if (sInstance && sInstance->mPanelGroupp
		&& (sInstance->mPanelGroupp->getID() == group_id) )
	{
		sInstance->mPanelGroupp->refreshData();
	}
}

void LLFloaterDirectory::focusCurrentPanel()
{
	LLTabContainer* tabs = getChild<LLTabContainer>("Directory Tabs");
	if (!tabs) return;

	LLPanel* panel = tabs->getCurrentPanel();
	if (!panel) return;

	panel->setFocus(TRUE);
}

// static
void LLFloaterDirectory::showPanel(const std::string& tabname)
{
	// This function gets called when web browser clicks are processed,
	// so we don't delete the existing panel, which would delete the 
	// web browser instance currently handling the click. JC
	if (!sInstance)
	{
		sInstance =  new LLFloaterDirectory("directory");
	}
	sInstance->open();	/*Flawfinder: ignore*/
	sInstance->childShowTab("Directory Tabs", tabname);
	sInstance->focusCurrentPanel();
}

// static
void LLFloaterDirectory::toggleFind(void*)
{
	if (!sInstance)
	{
		std::string panel = gSavedSettings.getString("LastFindPanel");
		bool hasWebSearch = gHippoGridManager->getConnectedGrid()->isSecondLife() ||
							!gHippoGridManager->getConnectedGrid()->getSearchUrl().empty();
		if (hasWebSearch && (panel == "find_all_panel" || panel == "showcase_panel"))
		{
			panel = "find_all_old_panel";
		}
		showPanel(panel);

		// HACK: force query for today's events
		if (sInstance->mEventsPanel)
		{
			sInstance->mEventsPanel->setDay(0);
			//sInstance->mEventsPanel->performQuery(); // Temporary change to help DB - Sabin
		}
		return;
	}

	BOOL panel_visible = sInstance->getVisible();
	if (!panel_visible)
	{
		sInstance->open(); /*Flawfinder: ignore*/
		sInstance->focusCurrentPanel();
	}
	else
	{
		sInstance->close();
	}
}

// static
void LLFloaterDirectory::toggleEvents(void*)
{
	if (!sInstance)
	{
		showEvents(0);
		return;
	}

	BOOL panel_visible = sInstance->getVisible();
	if (!panel_visible)
	{
		sInstance->open();	/*Flawfinder: ignore*/
		sInstance->showEvents(0);
	}
	else
	{
		LLPanel *current_panel = sInstance->childGetVisibleTab("Directory Tabs");
		if (current_panel == sInstance->mEventsPanel)
		{
			sInstance->close();
		}
		else
		{
			sInstance->showEvents(0);
		}
	}
}

// static
void LLFloaterDirectory::hide(void*)
{
	if (sInstance)
	{
		sInstance->close();
	}
}

// virtual
void LLFloaterDirectory::setVisible(BOOL visible)
{
	gSavedSettings.setBOOL("ShowDirectory", visible);
	LLFloater::setVisible(visible);
}

void LLFloaterDirectory::onClose(bool app_quitting)
{
	setVisible(FALSE);
}

// static
void LLFloaterDirectory::onTabChanged( const LLSD& param )
{
	gSavedSettings.setString("LastFindPanel", param.asString());
}

void LLFloaterDirectory::hideAllDetailPanels()
{
	if (mPanelAvatarp) mPanelAvatarp->setVisible(FALSE);
	if (mPanelEventp) mPanelEventp->setVisible(FALSE);
	if (mPanelGroupp) mPanelGroupp->setVisible(FALSE);
	if (mPanelGroupHolderp) mPanelGroupHolderp->setVisible(FALSE);
	if (mPanelPlacep) mPanelPlacep->setVisible(FALSE);
	if (mPanelPlaceSmallp) mPanelPlaceSmallp->setVisible(FALSE);
	if (mPanelClassifiedp) mPanelClassifiedp->setVisible(FALSE);
}
