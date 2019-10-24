/** 
 * @file llpanelavatar.cpp
 * @brief LLPanelAvatar and related class implementations
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llpanelavatar.h"

#include "llavatarconstants.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llclassifiedflags.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "llwindow.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "lldroptarget.h"
#include "llfloatergroupinfo.h"
#include "llfloatermute.h"
#include "llfloateravatarinfo.h"
#include "llgroupactions.h"
#include "lllineeditor.h"
#include "llnameeditor.h"
#include "llnotificationsutil.h"
#include "llpanelclassified.h"
#include "llpanelpick.h"
#include "llpreviewtexture.h"
#include "llpluginclassmedia.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llweb.h"

#include <iosfwd>
#include <boost/date_time.hpp>


#include "hippogridmanager.h" // Include Gridmanager for OpenSim Support in profiles, provides ability for the helpbutton to redirect to GRID Websites Help page

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

// Statics
std::list<LLPanelAvatar*> LLPanelAvatar::sAllPanels;
BOOL LLPanelAvatar::sAllowFirstLife = FALSE;

BOOL is_agent_mappable(const LLUUID& agent_id);


//-----------------------------------------------------------------------------
// LLPanelAvatarTab()
//-----------------------------------------------------------------------------
LLPanelAvatarTab::LLPanelAvatarTab(const std::string& name, const LLRect &rect, 
								   LLPanelAvatar* panel_avatar)
:	LLPanel(name, rect),
	mPanelAvatar(panel_avatar),
	mDataRequested(false)
{
	//Register with parent so it can relay agentid to tabs, since the id is set AFTER creation.
	panel_avatar->mAvatarPanelList.push_back(this);
}

void LLPanelAvatarTab::setAvatarID(const LLUUID& avatar_id)
{
	if (mAvatarID != avatar_id)
	{
		if (mAvatarID.notNull())
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
		mAvatarID = avatar_id;
		if (mAvatarID.notNull())
		{
			LLAvatarPropertiesProcessor::getInstance()->addObserver(mAvatarID, this);
			if (LLUICtrl* ctrl = findChild<LLUICtrl>("Mute"))
				ctrl->setValue(LLMuteList::instance().isMuted(mAvatarID));
		}
	}
	
}
// virtual
LLPanelAvatarTab::~LLPanelAvatarTab()
{
	if (mAvatarID.notNull())
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
}

// virtual
void LLPanelAvatarTab::draw()
{
	refresh();
	LLPanel::draw();
}

//-----------------------------------------------------------------------------
// LLPanelAvatarSecondLife()
//-----------------------------------------------------------------------------
LLPanelAvatarSecondLife::LLPanelAvatarSecondLife(const std::string& name, 
												 const LLRect &rect, 
												 LLPanelAvatar* panel_avatar ) 
:	LLPanelAvatarTab(name, rect, panel_avatar),
	mPartnerID()
{
	LLMuteList::instance().addObserver(this);
}

LLPanelAvatarSecondLife::~LLPanelAvatarSecondLife()
{
	LLMuteList::instance().removeObserver(this);
}

void LLPanelAvatarSecondLife::refresh()
{
}

//-----------------------------------------------------------------------------
// clearControls()
// Empty the data out of the controls, since we have to wait for new
// data off the network.
//-----------------------------------------------------------------------------
void LLPanelAvatarSecondLife::clearControls()
{
	getChild<LLTextureCtrl>("img")->setImageAssetID(LLUUID::null);
	childSetValue("about", LLStringUtil::null);
	childSetValue("born", LLStringUtil::null);
	childSetValue("acct", LLStringUtil::null);

	childSetValue("partner_edit", LLUUID::null);
	mPartnerID = LLUUID::null;

	getChild<LLScrollListCtrl>("groups")->deleteAllItems();
}

// virtual
void LLPanelAvatarSecondLife::processProperties(void* data, EAvatarProcessorType type)
{
	if(type == APT_PROPERTIES)
	{
		const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>( data );
		if (pAvatarData && (mAvatarID == pAvatarData->avatar_id) && (pAvatarData->avatar_id != LLUUID::null))
		{
			LLStringUtil::format_map_t args;
			args["[ACCTTYPE]"] = LLAvatarPropertiesProcessor::accountType(pAvatarData);
			args["[PAYMENTINFO]"] = LLAvatarPropertiesProcessor::paymentInfo(pAvatarData);
			args["[AGEVERIFICATION]"] = LLStringUtil::null;
			
			{
				const auto account_info = getString("CaptionTextAcctInfo", args);
				auto acct = getChild<LLUICtrl>("acct");
				acct->setValue(account_info);
				acct->setToolTip(account_info);
			}

			getChild<LLTextureCtrl>("img")->setImageAssetID(pAvatarData->image_id);

			// Show avatar age in days.
			{
				using namespace boost::gregorian;
				int year, month, day;
				const auto& born = pAvatarData->born_on;
				if (!born.empty() && sscanf(born.c_str(),"%d/%d/%d", &month, &day, &year) == 3 // Make sure input is valid
				&& month > 0 && month <= 12 && day > 0 && day <= 31 && year >= 1400) // Don't use numbers that gregorian will choke on
				{
					date birthday(year, month, day), today(day_clock::local_day());
					std::ostringstream born_on;
					const std::locale fmt(std::locale::classic(), new date_facet(gSavedSettings.getString("ShortDateFormat").data()));
					born_on.imbue(fmt);
					born_on << birthday << " (" << today - birthday << ')';
					childSetValue("born", born_on.str());
				}
				else childSetValue("born", born);
			}

			bool allow_publish = (pAvatarData->flags & AVATAR_ALLOW_PUBLISH);
			childSetValue("allow_publish", allow_publish);

			mPartnerID = pAvatarData->partner_id;
			getChildView("partner_edit")->setValue(mPartnerID);
		}
	}
	else if (type == APT_GROUPS)
	{
		const LLAvatarGroups* pAvatarGroups = static_cast<const LLAvatarGroups*>(data);
		if (pAvatarGroups && pAvatarGroups->avatar_id == mAvatarID && pAvatarGroups->avatar_id.notNull())
		{
			LLScrollListCtrl* group_list = getChild<LLScrollListCtrl>("groups");
			if (!pAvatarGroups->group_list.size())
			{
				group_list->setCommentText(getString("None"));
			}

			for(LLAvatarGroups::group_list_t::const_iterator it = pAvatarGroups->group_list.begin();
				it != pAvatarGroups->group_list.end(); ++it)
			{
				// Is this really necessary?  Remove existing entry if it exists.
				// TODO: clear the whole list when a request for data is made
				S32 index = group_list->getItemIndex(it->group_id);
				if (index >= 0)
					group_list->deleteSingleItem(index);

				LLScrollListItem::Params row;
				row.value(it->group_id);

				std::string font_style("NORMAL"); // Set normal color if not found or if group is visible in profile
				if (pAvatarGroups->avatar_id == pAvatarGroups->agent_id) // own avatar
					for (std::vector<LLGroupData>::iterator i = gAgent.mGroups.begin(); i != gAgent.mGroups.end(); ++i) // Search for this group in the agent's groups list
						if (i->mID == it->group_id)
						{
							if (i->mListInProfile)
								font_style = "BOLD";
							break;
						}

				if (it->group_id == gAgent.getGroupID())
					font_style.append("|ITALIC");
				row.columns.add(LLScrollListCell::Params().value(it->group_id.notNull() ? it->group_name : "").font("SANSSERIF_SMALL").font_style(font_style));
				group_list->addRow(row,ADD_SORTED);
			}
		}
	}
}

void LLPanelAvatarSecondLife::onChangeDetailed(const LLMute& mute)
{
	if (mute.mID != mAvatarID) return;
	getChild<LLUICtrl>("Mute")->setValue(LLMuteList::instance().hasMute(mute));
}

//-----------------------------------------------------------------------------
// enableControls()
//-----------------------------------------------------------------------------
void LLPanelAvatarSecondLife::enableControls(BOOL self)
{
	childSetEnabled("img", self);
	childSetEnabled("about", self);
	if (self) // We can't give inventory to self
	{
		if (LLDropTarget* drop_target = findChild<LLDropTarget>("drop_target_rect"))
			removeChild(drop_target);
		if (LLTextBox* text_box = findChild<LLTextBox>("Give item:"))
			removeChild(text_box);
	}
	childSetVisible("allow_publish", self);
	childSetEnabled("allow_publish", self);
	childSetVisible("?", self);
	childSetEnabled("?", self);
}

// virtual
void LLPanelAvatarFirstLife::processProperties(void* data, EAvatarProcessorType type)
{
	if (type == APT_PROPERTIES)
	{
		const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>(data);
		if (pAvatarData && (mAvatarID == pAvatarData->avatar_id) && (pAvatarData->avatar_id != LLUUID::null))
		{
			// Teens don't get these
			getChild<LLTextEditor>("about")->setText(pAvatarData->fl_about_text, false);
			getChild<LLTextureCtrl>("img")->setImageAssetID(pAvatarData->fl_image_id);
		}
	}
}

void LLPanelAvatarSecondLife::onDoubleClickGroup()
{
	if (LLScrollListItem* item = getChild<LLScrollListCtrl>("groups")->getFirstSelected())
		LLGroupActions::show(item->getUUID());
}

// static - Not anymore :P
bool LLPanelAvatarSecondLife::onClickPartnerHelpLoadURL(const LLSD& notification, const LLSD& response)
{
	if (!LLNotification::getSelectedOption(notification, response))
	{
		const auto& grid = *gHippoGridManager->getConnectedGrid();
		const std::string url = grid.isSecondLife() ? "http://secondlife.com/partner" : grid.getPartnerUrl();
		if (!url.empty()) LLWeb::loadURL(url);
	}
	return false;
}

//-----------------------------------------------------------------------------
// LLPanelAvatarFirstLife()
//-----------------------------------------------------------------------------
LLPanelAvatarFirstLife::LLPanelAvatarFirstLife(const std::string& name, 
											   const LLRect &rect, 
											   LLPanelAvatar* panel_avatar ) 
:	LLPanelAvatarTab(name, rect, panel_avatar)
{
}

void LLPanelAvatarFirstLife::enableControls(BOOL self)
{
	childSetEnabled("img", self);
	childSetEnabled("about", self);
}

//-----------------------------------------------------------------------------
// postBuild
//-----------------------------------------------------------------------------

void show_picture(const LLUUID& id, const std::string& name);
static std::string profile_picture_title(const std::string& str) { return "Profile Picture: " + str; }
static void show_partner_help() { LLNotificationsUtil::add("ClickPartnerHelpAvatar", LLSD(), LLSD(), boost::bind(LLPanelAvatarSecondLife::onClickPartnerHelpLoadURL, _1, _2)); }
void show_log_browser(const LLUUID& id)
{
	void show_log_browser(const std::string& name, const std::string& id);
	LLAvatarName av_name;
	LLAvatarNameCache::get(id, &av_name);
	show_log_browser(av_name.getLegacyName(), id.asString());
}
BOOL LLPanelAvatarSecondLife::postBuild()
{
	childSetEnabled("born", FALSE);
	childSetEnabled("partner_edit", FALSE);
	getChild<LLUICtrl>("partner_help")->setCommitCallback(boost::bind(show_partner_help));

	childSetAction("?", boost::bind(LLNotificationsUtil::add, "ClickPublishHelpAvatar"));
	LLPanelAvatar* pa = getPanelAvatar();
	enableControls(pa->getAvatarID() == gAgentID);

	childSetVisible("About:",LLPanelAvatar::sAllowFirstLife);
	childSetVisible("(500 chars)",LLPanelAvatar::sAllowFirstLife);
	childSetVisible("about",LLPanelAvatar::sAllowFirstLife);
	
	childSetVisible("allow_publish",LLPanelAvatar::sAllowFirstLife);
	childSetVisible("?",LLPanelAvatar::sAllowFirstLife);

	childSetVisible("online_yes",FALSE);

	getChild<LLUICtrl>("Find on Map")->setCommitCallback(boost::bind(LLAvatarActions::showOnMap, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
	getChild<LLUICtrl>("Instant Message...")->setCommitCallback(boost::bind(LLAvatarActions::startIM, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
	getChild<LLUICtrl>("GroupInvite_Button")->setCommitCallback(boost::bind(static_cast<void(*)(const LLUUID&)>(LLAvatarActions::inviteToGroup), boost::bind(&LLPanelAvatar::getAvatarID, pa)));

	getChild<LLUICtrl>("Add Friend...")->setCommitCallback(boost::bind(LLAvatarActions::requestFriendshipDialog, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
	getChild<LLUICtrl>("Log")->setCommitCallback(boost::bind(show_log_browser, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
	getChild<LLUICtrl>("Pay...")->setCommitCallback(boost::bind(LLAvatarActions::pay, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("Mute"))
	{
		ctrl->setCommitCallback(boost::bind(LLAvatarActions::toggleBlock, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
		ctrl->setValue(LLMuteList::instance().isMuted(mAvatarID));
	}

	getChild<LLUICtrl>("Offer Teleport...")->setCommitCallback(boost::bind(static_cast<void(*)(const LLUUID&)>(LLAvatarActions::offerTeleport), boost::bind(&LLPanelAvatar::getAvatarID, pa)));

	getChild<LLScrollListCtrl>("groups")->setDoubleClickCallback(boost::bind(&LLPanelAvatarSecondLife::onDoubleClickGroup,this));

	LLTextureCtrl* ctrl = getChild<LLTextureCtrl>("img");
	ctrl->setFallbackImageName("default_profile_picture.j2c");
	auto show_pic = [&]
	{
		show_picture(getChild<LLTextureCtrl>("img")->getImageAssetID(), profile_picture_title(getChild<LLLineEditor>("dnname")->getText()));
	};
	auto show_pic_if_not_self = [=] { if (!ctrl->canChange()) show_pic(); };

	ctrl->setMouseUpCallback(std::bind(show_pic_if_not_self));
	getChild<LLUICtrl>("bigimg")->setCommitCallback(std::bind(show_pic));

	return TRUE;
}

BOOL LLPanelAvatarFirstLife::postBuild()
{
	enableControls(getPanelAvatar()->getAvatarID() == gAgentID);

	LLTextureCtrl* ctrl = getChild<LLTextureCtrl>("img");
	ctrl->setFallbackImageName("default_profile_picture.j2c");
	auto show_pic = [&]
	{
		show_picture(getChild<LLTextureCtrl>("img")->getImageAssetID(), "First Life Picture");
	};
	auto show_pic_if_not_self = [=] { if (!ctrl->canChange()) show_pic(); };

	ctrl->setMouseUpCallback(std::bind(show_pic_if_not_self));
	getChild<LLUICtrl>("flbigimg")->setCommitCallback(std::bind(show_pic));
	return TRUE;
}

BOOL LLPanelAvatarNotes::postBuild()
{
	LLTextEditor* te(getChild<LLTextEditor>("notes edit"));
	te->setCommitCallback(boost::bind(&LLPanelAvatar::sendAvatarNotesUpdate, getPanelAvatar()));
	te->setCommitOnFocusLost(true);
	return TRUE;
}

BOOL LLPanelAvatarWeb::postBuild()
{
	LLLineEditor* url_edit = getChild<LLLineEditor>("url_edit");
	LLUICtrl* loadctrl = getChild<LLUICtrl>("load");

	url_edit->setKeystrokeCallback(boost::bind(&LLView::setEnabled, loadctrl, boost::bind(&std::string::length, boost::bind(&LLLineEditor::getText, _1))));
	url_edit->setCommitCallback(boost::bind(&LLPanelAvatarWeb::load, this, boost::bind(&LLSD::asString, _2)));

	loadctrl->setCommitCallback(boost::bind(&LLPanelAvatarWeb::onCommitLoad, this, _2));

	getChild<LLUICtrl>("web_profile_help")->setCommitCallback(boost::bind(LLNotificationsUtil::add, "ClickWebProfileHelpAvatar"));

	mWebBrowser = getChild<LLMediaCtrl>("profile_html");
	mWebBrowser->addObserver(this);

	return TRUE;
}

// virtual
void LLPanelAvatarWeb::processProperties(void* data, EAvatarProcessorType type)
{
	if(type == APT_PROPERTIES)
	{
		const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>( data );
		if (pAvatarData && (mAvatarID == pAvatarData->avatar_id) && (pAvatarData->avatar_id.notNull()))
		{
			setWebURL(pAvatarData->profile_url);
		}
	}
}

BOOL LLPanelAvatarClassified::postBuild()
{
	getChild<LLUICtrl>("New...")->setCommitCallback(boost::bind(&LLPanelAvatarClassified::onClickNew, this));
	getChild<LLUICtrl>("Delete...")->setCommitCallback(boost::bind(&LLPanelAvatarClassified::onClickDelete, this));
	// *HACK: Don't allow making new classifieds from inside the directory.
	// The logic for save/don't save when closing is too hairy, and the
	// directory is conceptually read-only. JC
	for (LLView* view = this; !mInDirectory && view; view = view->getParent())
		if (view->getName() == "directory")
			mInDirectory = true;
	return TRUE;
}

BOOL LLPanelAvatarPicks::postBuild()
{
	getChild<LLUICtrl>("New...")->setCommitCallback(boost::bind(&LLPanelAvatarPicks::onClickNew, this));
	getChild<LLUICtrl>("Delete...")->setCommitCallback(boost::bind(&LLPanelAvatarPicks::onClickDelete, this));
	
	//For pick import and export - RK
	getChild<LLUICtrl>("Import...")->setCommitCallback(boost::bind(&LLPanelAvatarPicks::onClickImport, this));
	getChild<LLUICtrl>("Export...")->setCommitCallback(boost::bind(&LLPanelAvatarPicks::onClickExport, this));
	return TRUE;
}

BOOL LLPanelAvatarAdvanced::postBuild()
{
	for(size_t ii = 0; ii < LL_ARRAY_SIZE(mWantToCheck); ++ii)
		mWantToCheck[ii] = NULL;
	for(size_t ii = 0; ii < LL_ARRAY_SIZE(mSkillsCheck); ++ii)
		mSkillsCheck[ii] = NULL;
	mWantToCount = (8>LL_ARRAY_SIZE(mWantToCheck))?LL_ARRAY_SIZE(mWantToCheck):8;
	for(S32 tt=0; tt < mWantToCount; ++tt)
	{	
		std::string ctlname = llformat("chk%d", tt);
		mWantToCheck[tt] = getChild<LLCheckBoxCtrl>(ctlname);
	}	
	mSkillsCount = (6>LL_ARRAY_SIZE(mSkillsCheck))?LL_ARRAY_SIZE(mSkillsCheck):6;

	for(S32 tt=0; tt < mSkillsCount; ++tt)
	{
		//Find the Skills checkboxes and save off thier controls
		std::string ctlname = llformat("schk%d",tt);
		mSkillsCheck[tt] = getChild<LLCheckBoxCtrl>(ctlname);
	}

	mWantToEdit = getChild<LLLineEditor>("want_to_edit");
	mSkillsEdit = getChild<LLLineEditor>("skills_edit");
	childSetVisible("skills_edit",LLPanelAvatar::sAllowFirstLife);
	childSetVisible("want_to_edit",LLPanelAvatar::sAllowFirstLife);

	return TRUE;
}

// virtual
void LLPanelAvatarAdvanced::processProperties(void* data, EAvatarProcessorType type)
{
	if(type == APT_INTERESTS)
	{
		const LLAvatarInterestsInfo* i_info = static_cast<LLAvatarInterestsInfo*>(data);
		if(i_info && i_info->avatar_id == mAvatarID && i_info->avatar_id.notNull())
		{
			setWantSkills(i_info->want_to_mask,i_info->want_to_text,i_info->skills_mask,i_info->skills_text,i_info->languages_text);
		}
	}
}

//-----------------------------------------------------------------------------
// LLPanelAvatarWeb
//-----------------------------------------------------------------------------
LLPanelAvatarWeb::LLPanelAvatarWeb(const std::string& name, const LLRect& rect, 
								   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar),
	mWebBrowser(NULL)
{
}

LLPanelAvatarWeb::~LLPanelAvatarWeb()
{
	// stop observing browser events
	if  ( mWebBrowser )
	{
		mWebBrowser->remObserver( this );
	}
}

void LLPanelAvatarWeb::refresh()
{
	if (!mNavigateTo.empty())
	{
		LL_INFOS() << "Loading " << mNavigateTo << LL_ENDL;
		mWebBrowser->navigateTo(mNavigateTo);
		mNavigateTo = "";
	}
}


void LLPanelAvatarWeb::enableControls(BOOL self)
{	
	childSetEnabled("url_edit",self);
	childSetVisible("status_text",!self && !mHome.empty());
	childSetText("status_text", LLStringUtil::null);
}

void LLPanelAvatarWeb::setWebURL(std::string url)
{
	bool changed_url = (mHome != url);

	mHome = url;
	
	childSetText("url_edit", mHome);
	childSetEnabled("load", mHome.length() > 0);

	if (!mHome.empty() && gSavedSettings.getBOOL("AutoLoadWebProfiles"))
	{
		if (changed_url)
		{
			load(mHome);
		}
		childSetVisible("status_text", getPanelAvatar()->getAvatarID() != gAgentID);
	}
	else
	{
		childSetVisible("profile_html", false);
		childSetVisible("status_text", false);
	}
}

void LLPanelAvatarWeb::load(const std::string& url)
{
	bool have_url = (!url.empty());

	childSetVisible("profile_html", have_url);
	childSetVisible("status_text", have_url);
	childSetText("status_text", LLStringUtil::null);

	if (have_url)
	{
		mNavigateTo = url;
	}
}

void LLPanelAvatarWeb::onCommitLoad(const LLSD& value)
{
	const std::string& valstr(value.asString());
	if (valstr.empty()) // load url string into browser panel
	{
		load(childGetText("url_edit"));
	}
	else if (valstr == "open") // open in user's external browser
	{
		const std::string& urlstr(childGetText("url_edit"));
		if (!urlstr.empty()) LLWeb::loadURLExternal(urlstr);
	}
	else if (valstr == "home") // reload profile owner's home page
	{
		if (!mHome.empty()) load(mHome);
	}
}



void LLPanelAvatarWeb::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
			childSetText("status_text", self->getStatusText());
		break;

		case MEDIA_EVENT_LOCATION_CHANGED:
			childSetText("url_edit", self->getLocation());
		break;

		default:
			// Having a default case makes the compiler happy.
		break;
	}
}

//-----------------------------------------------------------------------------
// LLPanelAvatarAdvanced
//-----------------------------------------------------------------------------
LLPanelAvatarAdvanced::LLPanelAvatarAdvanced(const std::string& name, 
											 const LLRect& rect, 
											 LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar),
	mWantToCount(0),
	mSkillsCount(0),
	mWantToEdit( NULL ),
	mSkillsEdit( NULL )
{
}

void LLPanelAvatarAdvanced::enableControls(BOOL self)
{
	for(S32 t(0); t < mWantToCount; ++t)
		if (mWantToCheck[t])
			mWantToCheck[t]->setEnabled(self);
	for(S32 t(0); t < mSkillsCount; ++t)
		if (mSkillsCheck[t])
			mSkillsCheck[t]->setEnabled(self);
	if (mWantToEdit)
		mWantToEdit->setEnabled(self);
	if (mSkillsEdit)
		mSkillsEdit->setEnabled(self);
	childSetEnabled("languages_edit", self);
}

void LLPanelAvatarAdvanced::setWantSkills(U32 want_to_mask, const std::string& want_to_text,
										  U32 skills_mask, const std::string& skills_text,
										  const std::string& languages_text)
{
	for(S32 i = 0; i < mWantToCount; ++i)
	{
		mWantToCheck[i]->set(want_to_mask & 1<<i);
	}
	for(S32 i = 0; i < mSkillsCount; ++i)
	{
		mSkillsCheck[i]->set(skills_mask & 1<<i);
	}
	if (mWantToEdit && mSkillsEdit)
	{
		mWantToEdit->setText(want_to_text);
		mSkillsEdit->setText(skills_text);
	}

	childSetText("languages_edit",languages_text);
}

void LLPanelAvatarAdvanced::getWantSkills(U32* want_to_mask, std::string& want_to_text,
										  U32* skills_mask, std::string& skills_text,
										  std::string& languages_text)
{
	if (want_to_mask)
	{
		*want_to_mask = 0;
		for(S32 t = 0; t < mWantToCount; ++t)
			if (mWantToCheck[t]->get())
				*want_to_mask |= 1<<t;
	}
	if (skills_mask)
	{
		*skills_mask = 0;
		for(S32 t = 0; t < mSkillsCount; ++t)
			if(mSkillsCheck[t]->get())
				*skills_mask |= 1<<t;
	}
	if (mWantToEdit)
		want_to_text = mWantToEdit->getText();

	if (mSkillsEdit)
		skills_text = mSkillsEdit->getText();

	languages_text = childGetText("languages_edit");
}	

//-----------------------------------------------------------------------------
// LLPanelAvatarNotes()
//-----------------------------------------------------------------------------
LLPanelAvatarNotes::LLPanelAvatarNotes(const std::string& name, const LLRect& rect, LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar)
{
}

void LLPanelAvatarNotes::refresh()
{
	if (!isDataRequested())
	{
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarNotesRequest(mAvatarID);
		setDataRequested(true);
	}
}

void LLPanelAvatarNotes::clearControls()
{
	LLView* view(getChildView("notes edit"));
	view->setValue(getString("Loading"));
	view->setEnabled(false);
}


//-----------------------------------------------------------------------------
// LLPanelAvatarClassified()
//-----------------------------------------------------------------------------
LLPanelAvatarClassified::LLPanelAvatarClassified(const std::string& name, const LLRect& rect,
									   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar)
, mInDirectory(false)
{
}

void LLPanelAvatarClassified::refresh()
{
	if (!isDataRequested())
	{
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarClassifiedsRequest(mAvatarID);
		setDataRequested(true);
	}
}


BOOL LLPanelAvatarClassified::canClose()
{
	LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");
	for (S32 i = 0; i < tabs->getTabCount(); i++)
	{
		LLPanelClassifiedInfo* panel = (LLPanelClassifiedInfo*)tabs->getPanelByIndex(i);
		if (!panel->canClose())
		{
			return FALSE;
		}
	}
	return TRUE;
}

BOOL LLPanelAvatarClassified::titleIsValid()
{
	if (LLTabContainer* tabs = getChild<LLTabContainer>("classified tab"))
		if (LLPanelClassifiedInfo* panel = (LLPanelClassifiedInfo*)tabs->getCurrentPanel())
			if (!panel->titleIsValid())
				return FALSE;
	return TRUE;
}

void LLPanelAvatarClassified::apply()
{
	LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");
	for (S32 i = 0; i < tabs->getTabCount(); i++)
	{
		LLPanelClassifiedInfo* panel = (LLPanelClassifiedInfo*)tabs->getPanelByIndex(i);
		panel->apply();
	}
}


void LLPanelAvatarClassified::deleteClassifiedPanels()
{
	getChild<LLTabContainer>("classified tab")->deleteAllTabs();
	childSetVisible("New...", false);
	childSetVisible("Delete...", false);
	childSetVisible("loading_text", true);
}

// virtual
void LLPanelAvatarClassified::processProperties(void* data, EAvatarProcessorType type)
{
	if (type == APT_CLASSIFIEDS)
	{
		LLAvatarClassifieds* c_info = static_cast<LLAvatarClassifieds*>(data);
		if (c_info && mAvatarID == c_info->target_id)
		{
			LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");

			for(LLAvatarClassifieds::classifieds_list_t::iterator it = c_info->classifieds_list.begin();
				it != c_info->classifieds_list.end(); ++it)
			{
				LLPanelClassifiedInfo* panel_classified = new LLPanelClassifiedInfo(false, false);

				panel_classified->setClassifiedID(it->classified_id);

				// This will request data from the server when the pick is first drawn.
				panel_classified->markForServerRequest();

				// The button should automatically truncate long names for us
				tabs->addTabPanel(panel_classified, it->name);
			}

			// Make sure somebody is highlighted.  This works even if there
			// are no tabs in the container.
			tabs->selectFirstTab();

			bool self = gAgentID == mAvatarID;
			S32 tab_count = tabs->getTabCount();
			bool allow_new = tab_count < MAX_CLASSIFIEDS
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
				&& !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
// [/RLVa:KB]
			LLView* view(getChildView("New..."));
			view->setEnabled(self && !mInDirectory && allow_new);
			view->setVisible(!mInDirectory);
			view = getChildView("Delete...");
			view->setEnabled(self && !mInDirectory && tab_count);
			view->setVisible(!mInDirectory);
			view = getChildView("loading_text");
			view->setVisible(false);
		}
	}
}

// Create a new classified panel.  It will automatically handle generating
// its own id when it's time to save.
// static
void LLPanelAvatarClassified::onClickNew()
{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) return;
// [/RLVa:KB]
	LLNotificationsUtil::add("AddClassified", LLSD(), LLSD(), boost::bind(&LLPanelAvatarClassified::callbackNew, this, _1, _2));
		
}

bool LLPanelAvatarClassified::callbackNew(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
		return false;
	LLPanelClassifiedInfo* panel_classified = new LLPanelClassifiedInfo(false, false);
	panel_classified->initNewClassified();
	LLTabContainer*	tabs = getChild<LLTabContainer>("classified tab");
	tabs->addTabPanel(panel_classified, panel_classified->getClassifiedName());
	tabs->selectLastTab();
	bool allow_new = tabs->getTabCount() < MAX_CLASSIFIEDS
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
		&& !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
// [/RLVa:KB]
	childSetEnabled("New...", allow_new);
	childSetEnabled("Delete...", true);
	return true;
}


// static
void LLPanelAvatarClassified::onClickDelete()
{
	LLTabContainer*	tabs = getChild<LLTabContainer>("classified tab");
	LLPanelClassifiedInfo* panel_classified = (LLPanelClassifiedInfo*)tabs->getCurrentPanel();
	if (!panel_classified) return;

	LLSD args;
	args["NAME"] = panel_classified->getClassifiedName();
	LLNotificationsUtil::add("DeleteClassified", args, LLSD(), boost::bind(&LLPanelAvatarClassified::callbackDelete, this, _1, _2));
}


bool LLPanelAvatarClassified::callbackDelete(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
		return false;
	LLTabContainer*	tabs = getChild<LLTabContainer>("classified tab");
	LLPanelClassifiedInfo* panel_classified = (LLPanelClassifiedInfo*)tabs->getCurrentPanel();

	if (!panel_classified) return false;

	LLAvatarPropertiesProcessor::getInstance()->sendClassifiedDelete(panel_classified->getClassifiedID());
	tabs->removeTabPanel(panel_classified);
	delete panel_classified;
	panel_classified = NULL;
	childSetEnabled("New...", !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));
	childSetEnabled("Delete...", tabs->getTabCount());
	return true;
}


//-----------------------------------------------------------------------------
// LLPanelAvatarPicks()
//-----------------------------------------------------------------------------
LLPanelAvatarPicks::LLPanelAvatarPicks(const std::string& name, 
									   const LLRect& rect,
									   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar)
{
}


void LLPanelAvatarPicks::refresh()
{
	if (!isDataRequested())
	{
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarPicksRequest(mAvatarID);
		setDataRequested(true);
	}
}


void LLPanelAvatarPicks::deletePickPanels()
{
	getChild<LLTabContainer>("picks tab")->deleteAllTabs();

	childSetVisible("New...", false);
	childSetVisible("Delete...", false);
	childSetVisible("loading_text", true);

	//For pick import and export - RK
	childSetVisible("Export...", false);
	childSetVisible("Import...", false);

}

// virtual
void LLPanelAvatarPicks::processProperties(void* data, EAvatarProcessorType type)
{
	if (type == APT_PICKS)
	{
		LLAvatarPicks* picks = static_cast<LLAvatarPicks*>(data);

		//llassert_always(picks->target_id != gAgent.getID());
		//llassert_always(mAvatarID != gAgent.getID());

		if (picks && mAvatarID == picks->target_id)
		{
			LLTabContainer* tabs =  getChild<LLTabContainer>("picks tab");

			// Clear out all the old panels.  We'll replace them with the correct
			// number of new panels.
			deletePickPanels();

			bool self(gAgentID == mAvatarID);
			for (LLAvatarPicks::picks_list_t::iterator it = picks->picks_list.begin();
				it != picks->picks_list.end(); ++it)
			{
				LLPanelPick* panel_pick = new LLPanelPick();
				panel_pick->setPickID(it->first, mAvatarID);

				// This will request data from the server when the pick is first
				// drawn.
				panel_pick->markForServerRequest();

				// The button should automatically truncate long names for us
				LL_INFOS() << "Adding tab for " << mAvatarID << " " << (self ? "Self" : "Other") << ": '" << it->second << "'" << LL_ENDL;
				tabs->addTabPanel(panel_pick, it->second);
			}

			// Make sure somebody is highlighted.  This works even if there
			// are no tabs in the container.
			tabs->selectFirstTab();
			bool edit(getPanelAvatar()->isEditable());
			S32 tab_count = tabs->getTabCount();
			LLView* view = getChildView("New...");
			view->setEnabled(self && tab_count < MAX_AVATAR_PICKS
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
				&& !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));
// [/RLVa:KB]
			view->setVisible(self && edit);
			view = getChildView("Delete...");
			view->setEnabled(self && tab_count);
			view->setVisible(self && edit);

			//For pick import/export - RK
			view = getChildView("Import...");
			view->setVisible(self && edit);
			view->setEnabled(tab_count < MAX_AVATAR_PICKS);
			view = getChildView("Export...");
			view->setEnabled(self && tab_count);
			view->setVisible(self);

			childSetVisible("loading_text", false);
		}
	}
}

// Create a new pick panel.  It will automatically handle generating
// its own id when it's time to save.
void LLPanelAvatarPicks::onClickNew()
{
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		return;
// [/RLVa:KB]
	LLPanelPick* panel_pick = new LLPanelPick;
	LLTabContainer* tabs =  getChild<LLTabContainer>("picks tab");

	panel_pick->initNewPick();
	tabs->addTabPanel(panel_pick, panel_pick->getPickName());
	tabs->selectLastTab();
	S32 tab_count = tabs->getTabCount();
	getChildView("New...")->setEnabled(tab_count < MAX_AVATAR_PICKS
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
		&& !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));
// [/RLVa:KB]
	getChildView("Delete...")->setEnabled(true);
	getChildView("Import...")->setEnabled(tab_count < MAX_AVATAR_PICKS);
}

//Pick import and export - RK
void LLPanelAvatarPicks::onClickImport()
{
	mPanelPick = new LLPanelPick;
	mPanelPick->importNewPick(&LLPanelAvatarPicks::onClickImport_continued, this);
}

// static
void LLPanelAvatarPicks::onClickImport_continued(void* data, bool import)
{
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	LLTabContainer* tabs = self->getChild<LLTabContainer>("picks tab");
	if (import && self->mPanelPick)
	{
		tabs->addTabPanel(self->mPanelPick, self->mPanelPick->getPickName());
		tabs->selectLastTab();
		self->childSetEnabled("New...", !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));
		self->childSetEnabled("Delete...", false);
		self->childSetEnabled("Import...", tabs->getTabCount() < MAX_AVATAR_PICKS);
	}
}

void LLPanelAvatarPicks::onClickExport()
{
	LLPanelPick* panel_pick = (LLPanelPick*)getChild<LLTabContainer>("picks tab")->getCurrentPanel();
	if (!panel_pick) return;

	panel_pick->exportPick();
}

void LLPanelAvatarPicks::onClickDelete()
{
	LLPanelPick* panel_pick = (LLPanelPick*)getChild<LLTabContainer>("picks tab")->getCurrentPanel();
	if (!panel_pick) return;

	LLSD args;
	args["PICK"] = panel_pick->getPickName();

	LLNotificationsUtil::add("DeleteAvatarPick", args, LLSD(),
									boost::bind(&LLPanelAvatarPicks::callbackDelete, this, _1, _2));
}

bool LLPanelAvatarPicks::callbackDelete(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
		return false;
	LLTabContainer* tabs = getChild<LLTabContainer>("picks tab");
	LLPanelPick* panel_pick = (LLPanelPick*)tabs->getCurrentPanel();
	if (!panel_pick) return false;

	LLMessageSystem* msg = gMessageSystem;

	// If the viewer has a hacked god-mode, then this call will fail.
	if (gAgent.isGodlike())
	{
		msg->newMessage("PickGodDelete");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgentID);
		msg->addUUID("SessionID", gAgentSessionID);
		msg->nextBlock("Data");
		msg->addUUID("PickID", panel_pick->getPickID());
		// *HACK: We need to send the pick's creator id to accomplish
		// the delete, and we don't use the query id for anything. JC
		msg->addUUID( "QueryID", panel_pick->getPickCreatorID() );
		gAgent.sendReliableMessage();
	}
	else
	{
		LLAvatarPropertiesProcessor::getInstance()->sendPickDelete(panel_pick->getPickID());
	}

	tabs->removeTabPanel(panel_pick);
	delete panel_pick;
	panel_pick = NULL;
	childSetEnabled("New...", !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));
	childSetEnabled("Delete...", tabs->getTabCount());
	childSetEnabled("Import...", true);
	return true;
}


//-----------------------------------------------------------------------------
// LLPanelAvatar
//-----------------------------------------------------------------------------
LLPanelAvatar::LLPanelAvatar(
	const std::string& name,
	const LLRect &rect,
	BOOL allow_edit)
	:
	LLPanel(name, rect, FALSE),
	mPanelSecondLife(NULL),
	mPanelAdvanced(NULL),
	mPanelClassified(NULL),
	mPanelPicks(NULL),
	mPanelNotes(NULL),
	mPanelFirstLife(NULL),
	mPanelWeb(NULL),
	mAvatarID(LLUUID::null),	// mAvatarID is set with setAvatarID()
	mHaveProperties(FALSE),
	mHaveStatistics(FALSE),
	mHaveNotes(false),
	mLastNotes(),
	mAllowEdit(allow_edit)
{
	sAllPanels.push_back(this);

	LLCallbackMap::map_t factory_map;

	factory_map["2nd Life"] = LLCallbackMap(createPanelAvatarSecondLife, this);
	factory_map["WebProfile"] = LLCallbackMap(createPanelAvatarWeb, this);
	factory_map["Interests"] = LLCallbackMap(createPanelAvatarInterests, this);
	factory_map["Picks"] = LLCallbackMap(createPanelAvatarPicks, this);
	factory_map["Classified"] = LLCallbackMap(createPanelAvatarClassified, this);
	factory_map["1st Life"] = LLCallbackMap(createPanelAvatarFirstLife, this);
	factory_map["My Notes"] = LLCallbackMap(createPanelAvatarNotes, this);
	
	mCommitCallbackRegistrar.add("Profile.Web", boost::bind(LLAvatarActions::showProfile, boost::bind(&LLPanelAvatar::getAvatarID, this), true));
	mCommitCallbackRegistrar.add("Profile.TeleportRequest", boost::bind(LLAvatarActions::teleportRequest, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_avatar.xml", &factory_map);

	selectTab(0);
}

BOOL LLPanelAvatar::postBuild()
{
	mTab = getChild<LLTabContainer>("tab");
	LLUICtrl* ctrl = getChild<LLUICtrl>("Kick");
	ctrl->setCommitCallback(boost::bind(LLAvatarActions::kick, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	ctrl->setVisible(false);
	ctrl->setEnabled(false);
	ctrl = getChild<LLUICtrl>("Freeze");
	ctrl->setCommitCallback(boost::bind(LLAvatarActions::freeze, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	ctrl->setVisible(false);
	ctrl->setEnabled(false);
	ctrl = getChild<LLUICtrl>("Unfreeze");
	ctrl->setCommitCallback(boost::bind(LLAvatarActions::unfreeze, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	ctrl->setVisible(false);
	ctrl->setEnabled(false);
	ctrl = getChild<LLUICtrl>("csr_btn");
	ctrl->setCommitCallback(boost::bind(LLAvatarActions::csr, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	ctrl->setVisible(false);
	ctrl->setEnabled(false);
	getChild<LLUICtrl>("OK")->setCommitCallback(boost::bind(&LLPanelAvatar::onClickOK, this));
	getChild<LLUICtrl>("Cancel")->setCommitCallback(boost::bind(&LLPanelAvatar::onClickCancel, this));
	getChild<LLUICtrl>("copy_flyout")->setCommitCallback(boost::bind(&LLPanelAvatar::onClickCopy, this, _2));
	getChildView("web_profile")->setVisible(!gSavedSettings.getString("WebProfileURL").empty());

	if (mTab && !sAllowFirstLife)
	{
		LLPanel* panel = mTab->getPanelByName("1st Life");
		if (panel) mTab->removeTabPanel(panel);

		panel = mTab->getPanelByName("WebProfile");
		if (panel) mTab->removeTabPanel(panel);
	}

	//This text never changes. We simply toggle visibility.
	ctrl = getChild<LLUICtrl>("online_yes");
	ctrl->setVisible(false);
	ctrl->setColor(LLColor4::green);

	return TRUE;
}


LLPanelAvatar::~LLPanelAvatar()
{
	LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
	sAllPanels.remove(this);
	mCacheConnection.disconnect();
}


BOOL LLPanelAvatar::canClose()
{
	return !mPanelClassified || mPanelClassified->canClose();
}

void LLPanelAvatar::setOnlineStatus(EOnlineStatus online_status)
{
	// Online status NO could be because they are hidden
	// If they are a friend, we may know the truth!
	if ((ONLINE_STATUS_YES != online_status)
		&& mIsFriend
		&& LLAvatarTracker::instance().isBuddyOnline(mAvatarID))
	{
		online_status = ONLINE_STATUS_YES;
	}

	if(mPanelSecondLife)
		mPanelSecondLife->childSetVisible("online_yes", online_status == ONLINE_STATUS_YES);

	LLView* offer_tp(getChildView("Offer Teleport..."));
	LLView* map_stalk(getChildView("Find on Map"));
	// Since setOnlineStatus gets called after setAvatarID
	// need to make sure that "Offer Teleport" doesn't get set
	// to TRUE again for yourself
	if (mAvatarID != gAgentID)
	{
		offer_tp->setVisible(true);
		map_stalk->setVisible(true);

		bool prelude(gAgent.inPrelude());
		bool godlike(gAgent.isGodlike());
		offer_tp->setEnabled(!prelude /*(&& online_status == ONLINE_STATUS_YES)*/);
		offer_tp->setToolTip(godlike ? getString("TeleportGod") : prelude ? getString("TeleportPrelude") : getString("TeleportNormal"));
		// Note: we don't always know online status, so always allow gods to try to track
		map_stalk->setEnabled(godlike || is_agent_mappable(mAvatarID));
		map_stalk->setToolTip(!mIsFriend ? getString("ShowOnMapNonFriend") : (ONLINE_STATUS_YES != online_status) ? getString("ShowOnMapFriendOffline") : getString("ShowOnMapFriendOnline"));
	}
}

void LLPanelAvatar::setAvatarID(const LLUUID &avatar_id)
{
	auto dnname = getChild<LLNameEditor>("dnname");
	if (avatar_id != mAvatarID)
	{
		if (mAvatarID.notNull())
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
		mAvatarID = avatar_id;
		dnname->setNameID(avatar_id, false);
	}

	if (avatar_id.isNull()) return;

	LLAvatarPropertiesProcessor::getInstance()->addObserver(mAvatarID, this);

	// Determine if we have their calling card.
	mIsFriend = LLAvatarActions::isFriend(mAvatarID);

	// setOnlineStatus uses mIsFriend
	setOnlineStatus(ONLINE_STATUS_NO);

	bool own_avatar(mAvatarID == gAgentID);

	for(std::list<LLPanelAvatarTab*>::iterator it=mAvatarPanelList.begin();it!=mAvatarPanelList.end();++it)
	{
		(*it)->setAvatarID(avatar_id);
	}

	if (mPanelSecondLife) mPanelSecondLife->enableControls(own_avatar && mAllowEdit);
	if (mPanelWeb) mPanelWeb->enableControls(own_avatar && mAllowEdit);
	if (mPanelAdvanced) mPanelAdvanced->enableControls(own_avatar && mAllowEdit);
	// Teens don't have this.
	if (mPanelFirstLife) mPanelFirstLife->enableControls(own_avatar && mAllowEdit);

	if (LLDropTarget* drop_target = findChild<LLDropTarget>("drop_target_rect"))
		drop_target->setEntityID(mAvatarID);

	dnname->setShowCompleteName(gSavedSettings.getBOOL("SinguCompleteNameProfiles"));

	if (auto key_edit = getChildView("avatar_key"))
		key_edit->setValue(mAvatarID.asString());

	// While we're waiting for data off the network, clear out the  old data.
	if (mPanelSecondLife)
		mPanelSecondLife->clearControls();
	if (mPanelPicks)
		mPanelPicks->deletePickPanels();
	if (mPanelPicks)
		mPanelPicks->setDataRequested(false);
	if (mPanelClassified)
		mPanelClassified->deleteClassifiedPanels();
	if (mPanelClassified)
		mPanelClassified->setDataRequested(false);
	if (mPanelNotes)
		mPanelNotes->clearControls();
	if (mPanelNotes)
		mPanelNotes->setDataRequested(false);
	mHaveNotes = false;
	mLastNotes.clear();

	// Request just the first two pages of data.  The picks,
	// classifieds, and notes will be requested when that panel
	// is made visible. JC
	sendAvatarPropertiesRequest();

	LLView* view(getChildView("OK"));
	view->setVisible(own_avatar && mAllowEdit);
	view->setEnabled(false); // OK button disabled until properties data arrives
	view = getChildView("Cancel");
	view->setVisible(own_avatar && mAllowEdit);
	view->setEnabled(own_avatar && mAllowEdit);
	view = getChildView("Instant Message...");
	view->setVisible(!own_avatar);
	view->setEnabled(false);
	view = getChildView("GroupInvite_Button");
	view->setVisible(!own_avatar);
	view->setEnabled(false);
	view = getChildView("Mute");
	view->setVisible(!own_avatar);
	view->setEnabled(false);
	if (own_avatar)
	{
		view = getChildView("Offer Teleport...");
		view->setVisible(false);
		view->setEnabled(false);
		view = getChildView("Find on Map");
		view->setVisible(false);
		view->setEnabled(false);
	}
	view = getChildView("Add Friend...");
	view->setVisible(!own_avatar);
	view->setEnabled(!own_avatar && !mIsFriend);
	view = getChildView("Pay...");
	view->setVisible(!own_avatar);
	view->setEnabled(false);
	getChildView("Log")->setVisible(!own_avatar);

	getChild<LLNameEditor>("avatar_key")->setText(avatar_id.asString());

	bool is_god = gAgent.isGodlike();
	view = getChildView("Kick");
	view->setVisible(is_god);
	view->setEnabled(is_god);
	view = getChildView("Freeze");
	view->setVisible(is_god);
	view->setEnabled(is_god);
	view = getChildView("Unfreeze");
	view->setVisible(is_god);
	view->setEnabled(is_god);
	view = getChildView("csr_btn");
	view->setVisible(is_god);
	view->setEnabled(is_god);
}


void LLPanelAvatar::resetGroupList()
{
	// only get these updates asynchronously via the group floater, which works on the agent only
	if (mAvatarID != gAgentID) return;
		
	if (mPanelSecondLife)
	{
		LLScrollListCtrl* group_list = mPanelSecondLife->getChild<LLScrollListCtrl>("groups");
		if (group_list)
		{
			const LLUUID selected_id	= group_list->getSelectedValue();
			const S32	selected_idx	= group_list->getFirstSelectedIndex();
			const S32	scroll_pos		= group_list->getScrollPos();

			group_list->deleteAllItems();
			
			S32 count = gAgent.mGroups.size();
			for(S32 i = 0; i < count; ++i)
			{
				LLGroupData group_data = gAgent.mGroups[i];


				const LLUUID& id(group_data.mID);
				LLScrollListItem::Params row;
				row.value(id);
				std::string font_style = group_data.mListInProfile ? "BOLD" : "NORMAL";
				if (id == gAgent.getGroupID())
					font_style.append("|ITALIC");
				/* Show group title?  DUMMY_POWER for Don Grep
					(group_data.mOfficer ? "Officer of " : "Member of ") + group_data.mName;
				*/
				row.columns.add(LLScrollListCell::Params().value(group_data.mName).font("SANSSERIF_SMALL").font_style(font_style).width(0));
				group_list->addRow(row, ADD_SORTED);
			}
			if (selected_id.notNull())
				group_list->selectByValue(selected_id);
			if (selected_idx != group_list->getFirstSelectedIndex()) //if index changed then our stored pos is pointless.
				group_list->scrollToShowSelected();
			else
				group_list->setScrollPos(scroll_pos);
		}
	}
}

void LLPanelAvatar::onClickCopy(const LLSD& val)
{
	if (val.isUndefined())
	{
		LL_INFOS() << "Copy agent id: " << mAvatarID << LL_ENDL;
		gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(mAvatarID.asString()));
	}
	else
	{
		void copy_profile_uri(const LLUUID& id, bool group = false);
		copy_profile_uri(mAvatarID);
	}
}

void LLPanelAvatar::onClickOK()
{
	// JC: Only save the data if we actually got the original
	// properties.  Otherwise we might save blanks into
	// the database.
	if (mHaveProperties)
	{
		sendAvatarPropertiesUpdate();

		if (mTab->getCurrentPanel() != mPanelClassified || mPanelClassified->titleIsValid())
		{
			mPanelClassified->apply();

			if (LLFloaterAvatarInfo* infop = LLFloaterAvatarInfo::getInstance(mAvatarID))
				infop->close();
		}
	}
}

void LLPanelAvatar::onClickCancel()
{
	if (LLFloaterAvatarInfo* infop = LLFloaterAvatarInfo::getInstance(mAvatarID))
		infop->close();
	else // We're in the Search directory and are cancelling an edit to our own profile, so reset.
		sendAvatarPropertiesRequest();
}


void LLPanelAvatar::sendAvatarPropertiesRequest()
{
	LL_DEBUGS() << "LLPanelAvatar::sendAvatarPropertiesRequest()" << LL_ENDL; 

	LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(mAvatarID);
}

void LLPanelAvatar::sendAvatarNotesUpdate()
{
	std::string notes = mPanelNotes->childGetValue("notes edit").asString();

	if (!mHaveNotes && (notes.empty() || notes == getString("Loading")) || // no notes from server and no user updates
		notes == mLastNotes) // Avatar notes unchanged
		return;

	auto& inst(LLAvatarPropertiesProcessor::instance());
	inst.sendNotes(mAvatarID, notes);
	inst.sendAvatarNotesRequest(mAvatarID); // Rerequest notes to update anyone that might be listening, also to be sure we match the server.
}

// virtual
void LLPanelAvatar::processProperties(void* data, EAvatarProcessorType type)
{
	if (type == APT_PROPERTIES)
	{
		const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>( data );
		if (pAvatarData && (mAvatarID == pAvatarData->avatar_id) && (pAvatarData->avatar_id.notNull()))
		{
			childSetEnabled("Instant Message...",TRUE);
			childSetEnabled("GroupInvite_Button",TRUE);
			childSetEnabled("Pay...",TRUE);
			childSetEnabled("Mute",TRUE);

			mHaveProperties = TRUE;
			enableOKIfReady();

			/*tm t;
			if (sscanf(born_on.c_str(), "%u/%u/%u", &t.tm_mon, &t.tm_mday, &t.tm_year) == 3 && t.tm_year > 1900)
			{
				t.tm_year -= 1900;
				t.tm_mon--;
				t.tm_hour = t.tm_min = t.tm_sec = 0;
				timeStructToFormattedString(&t, gSavedSettings.getString("ShortDateFormat"), born_on);
			}*/
			setOnlineStatus(pAvatarData->flags & AVATAR_ONLINE ? ONLINE_STATUS_YES : ONLINE_STATUS_NO);
			getChild<LLTextEditor>("about")->setText(pAvatarData->about_text, false);
		}
	}
	else if (type == APT_NOTES)
	{
		const LLAvatarNotes* pAvatarNotes = static_cast<const LLAvatarNotes*>( data );
		if (pAvatarNotes && (mAvatarID == pAvatarNotes->target_id) && (pAvatarNotes->target_id != LLUUID::null))
		{
			if (!mHaveNotes) // Only update the UI if we don't already have the notes, we could be editing them now!
			{
				auto notes = getChildView("notes edit");
				notes->setEnabled(true);
				notes->setValue(pAvatarNotes->notes);
				mHaveNotes = true;
			}
			mLastNotes = pAvatarNotes->notes;
		}
	}
}

// Don't enable the OK button until you actually have the data.
// Otherwise you will write blanks back into the database.
void LLPanelAvatar::enableOKIfReady()
{
	LLView* OK(getChildView("OK"));
	OK->setEnabled(mHaveProperties && OK->getVisible());
}

void LLPanelAvatar::sendAvatarPropertiesUpdate()
{
	LL_INFOS() << "Sending avatarinfo update" << LL_ENDL;
	LLAvatarData avatar_data;
	avatar_data.image_id = mPanelSecondLife->getChild<LLTextureCtrl>("img")->getImageAssetID();
	avatar_data.fl_image_id = mPanelFirstLife ? mPanelFirstLife->getChild<LLTextureCtrl>("img")->getImageAssetID() : LLUUID::null;
	avatar_data.about_text = mPanelSecondLife->childGetValue("about").asString();
	avatar_data.fl_about_text = mPanelFirstLife ? mPanelFirstLife->childGetValue("about").asString() : LLStringUtil::null;
	avatar_data.allow_publish = sAllowFirstLife && childGetValue("allow_publish");
	avatar_data.profile_url = mPanelWeb->childGetText("url_edit");
	LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesUpdate(&avatar_data);

	LLAvatarInterestsInfo interests_data;
	interests_data.want_to_mask = 0x0;
	interests_data.skills_mask = 0x0;
	mPanelAdvanced->getWantSkills(&interests_data.want_to_mask, interests_data.want_to_text, &interests_data.skills_mask, interests_data.skills_text, interests_data.languages_text);

	LLAvatarPropertiesProcessor::getInstance()->sendAvatarInterestsUpdate(&interests_data);
}

void LLPanelAvatar::selectTab(S32 tabnum)
{
	if (mTab) mTab->selectTab(tabnum);
}

void LLPanelAvatar::selectTabByName(std::string tab_name)
{
	if (!mTab) return;
	if (tab_name.empty())
		mTab->selectFirstTab();
	else
		mTab->selectTabByName(tab_name);
}

void* LLPanelAvatar::createPanelAvatarSecondLife(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelSecondLife = new LLPanelAvatarSecondLife("2nd Life", LLRect(), self);
	return self->mPanelSecondLife;
}

void* LLPanelAvatar::createPanelAvatarWeb(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelWeb = new LLPanelAvatarWeb("Web",LLRect(),self);
	return self->mPanelWeb;
}

void* LLPanelAvatar::createPanelAvatarInterests(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelAdvanced = new LLPanelAvatarAdvanced("Interests", LLRect(), self);
	return self->mPanelAdvanced;
}


void* LLPanelAvatar::createPanelAvatarPicks(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelPicks = new LLPanelAvatarPicks("Picks", LLRect(), self);
	return self->mPanelPicks;
}

void* LLPanelAvatar::createPanelAvatarClassified(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelClassified = new LLPanelAvatarClassified("Classified", LLRect(), self);
	return self->mPanelClassified;
}

void* LLPanelAvatar::createPanelAvatarFirstLife(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelFirstLife = new LLPanelAvatarFirstLife("1st Life", LLRect(), self);
	return self->mPanelFirstLife;
}

void* LLPanelAvatar::createPanelAvatarNotes(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelNotes = new LLPanelAvatarNotes("My Notes", LLRect(),self);
	return self->mPanelNotes;
}
