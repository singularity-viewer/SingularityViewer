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
	if(mAvatarID != avatar_id)
	{
		if(mAvatarID.notNull())
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
		mAvatarID = avatar_id;
		if(mAvatarID.notNull())
			LLAvatarPropertiesProcessor::getInstance()->addObserver(mAvatarID, this);
	}
	
}
// virtual
LLPanelAvatarTab::~LLPanelAvatarTab()
{
	if(mAvatarID.notNull())
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
}

void LLPanelAvatarSecondLife::refresh()
{
	updatePartnerName();
}

void LLPanelAvatarSecondLife::updatePartnerName()
{
	if (mPartnerID.notNull())
	{
		std::string name;
		if (LLAvatarNameCache::getPNSName(mPartnerID, name))
			childSetTextArg("partner_edit", "[NAME]", name);
		childSetEnabled("partner_info", TRUE);
	}
}

//-----------------------------------------------------------------------------
// clearControls()
// Empty the data out of the controls, since we have to wait for new
// data off the network.
//-----------------------------------------------------------------------------
void LLPanelAvatarSecondLife::clearControls()
{
	LLTextureCtrl*	image_ctrl = getChild<LLTextureCtrl>("img");
	if(image_ctrl)
	{
		image_ctrl->setImageAssetID(LLUUID::null);
	}
	childSetValue("about", "");
	childSetValue("born", "");
	childSetValue("acct", "");

	childSetTextArg("partner_edit", "[NAME]", LLStringUtil::null);

	mPartnerID = LLUUID::null;
	
	LLScrollListCtrl*	group_list = getChild<LLScrollListCtrl>("groups"); 
	if(group_list)
	{
		group_list->deleteAllItems();
	}
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
			args["[AGEVERIFICATION]"] = " ";
			
			getChild<LLUICtrl>("acct")->setValue(getString("CaptionTextAcctInfo", args));

			getChild<LLTextureCtrl>("img")->setImageAssetID(pAvatarData->image_id);

			// Show avatar age in days.
			{
				using namespace boost::gregorian;
				int year, month, day;
				sscanf(pAvatarData->born_on.c_str(),"%d/%d/%d",&month,&day,&year);
				std::ostringstream born_on;
				born_on << pAvatarData->born_on << " (" << day_clock::local_day() - date(year, month, day) << ")";
				childSetValue("born", born_on.str());
			}

			bool allow_publish = (pAvatarData->flags & AVATAR_ALLOW_PUBLISH);
			childSetValue("allow_publish", allow_publish);

			setPartnerID(pAvatarData->partner_id);
			updatePartnerName();
		}
	}
	else if(type == APT_GROUPS)
	{
		const LLAvatarGroups* pAvatarGroups = static_cast<const LLAvatarGroups*>( data );
		if(pAvatarGroups && pAvatarGroups->avatar_id == mAvatarID && pAvatarGroups->avatar_id.notNull())
		{
			LLScrollListCtrl*	group_list = getChild<LLScrollListCtrl>("groups");
// 			if(group_list)
//			{
// 				group_list->deleteAllItems();
//			}
			if (0 == pAvatarGroups->group_list.size())
			{
				group_list->setCommentText(getString("None"));
			}

			for(LLAvatarGroups::group_list_t::const_iterator it = pAvatarGroups->group_list.begin();
				it != pAvatarGroups->group_list.end(); ++it)
			{
				// Is this really necessary?  Remove existing entry if it exists.
				// TODO: clear the whole list when a request for data is made
				if (group_list)
				{
					S32 index = group_list->getItemIndex(it->group_id);
					if ( index >= 0 )
					{
						group_list->deleteSingleItem(index);
					}
				}

				LLSD row;
				row["id"] = it->group_id;
				row["columns"][0]["value"] = it->group_id.notNull() ? it->group_name : "";
				row["columns"][0]["font"] = "SANSSERIF_SMALL";
				LLGroupData *group_data = NULL;

				if (pAvatarGroups->avatar_id == pAvatarGroups->agent_id) // own avatar
				{
					// Search for this group in the agent's groups list
					LLDynamicArray<LLGroupData>::iterator i;

					for (i = gAgent.mGroups.begin(); i != gAgent.mGroups.end(); i++)
					{
						if (i->mID == it->group_id)
						{
							group_data = &*i;
							break;
						}
					}
					// Set normal color if not found or if group is visible in profile
					if (group_data)
					{
						std::string font_style = group_data->mListInProfile ? "BOLD" : "NORMAL";
						if(group_data->mID == gAgent.getGroupID())
							font_style.append("|ITALIC");
						row["columns"][0]["font-style"] = font_style;
					}
					else
						row["columns"][0]["font-style"] = "NORMAL";
				}
				
				if (group_list)
				{
					group_list->addElement(row,ADD_SORTED);
				}
			}
		}
	}
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

// static
void LLPanelAvatarFirstLife::onClickImage(void* data)
{
	LLPanelAvatarFirstLife* self = (LLPanelAvatarFirstLife*)data;
	
	LLTextureCtrl*	image_ctrl = self->getChild<LLTextureCtrl>("img");
	if(image_ctrl)
	{ 
		LLUUID mUUID = image_ctrl->getImageAssetID();
		llinfos << "LLPanelAvatarFirstLife::onClickImage" << llendl;
		if(!LLPreview::show(mUUID))
		{
			// There isn't one, so make a new preview
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
			rect.translate( left - rect.mLeft, rect.mTop - top ); // Changed to avoid textures being sunken below the window border.
			LLPreviewTexture* preview = new LLPreviewTexture("preview task texture",
													 rect,
													 std::string("Profile First Life Picture"),
													 mUUID);
			preview->setFocus(TRUE);
			//preview->mIsCopyable=FALSE;
			//preview->canSaveAs
		}
	
	}
}

// virtual
void LLPanelAvatarFirstLife::processProperties(void* data, EAvatarProcessorType type)
{
	if(type == APT_PROPERTIES)
	{
		const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>( data );
		if (pAvatarData && (mAvatarID == pAvatarData->avatar_id) && (pAvatarData->avatar_id != LLUUID::null))
		{
			// Teens don't get these
			childSetValue("about", pAvatarData->fl_about_text);
			getChild<LLTextureCtrl>("img")->setImageAssetID(pAvatarData->fl_image_id);
		}
	}
}

// static
void LLPanelAvatarSecondLife::onClickImage(void* data)
{
	LLPanelAvatarSecondLife* self = (LLPanelAvatarSecondLife*)data;
	LLNameEditor* name_ctrl = self->getChild<LLNameEditor>("dnname");
	if(name_ctrl)
	{
		std::string name_text = name_ctrl->getText();	
	
		LLTextureCtrl*	image_ctrl = self->getChild<LLTextureCtrl>("img");
		if(image_ctrl)
		{ 
			LLUUID mUUID = image_ctrl->getImageAssetID();
			llinfos << "LLPanelAvatarSecondLife::onClickImage" << llendl;
			if(!LLPreview::show(mUUID))
			{
				// There isn't one, so make a new preview
				S32 left, top;
				gFloaterView->getNewFloaterPosition(&left, &top);
				LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
				rect.translate( left - rect.mLeft, rect.mTop - top ); // Changed to avoid textures being sunken below the window border.
				LLPreviewTexture* preview = new LLPreviewTexture("preview task texture",
														 rect,
														 std::string("Profile Picture: ") +	name_text,
														 mUUID
														 );
				preview->setFocus(TRUE);
				
				//preview->mIsCopyable=FALSE;
			}
			/*open_texture(LLUUID::null,//image_ctrl->getImageAssetID(),
				std::string("Profile Picture: ") +
				name_text+
				"and image id is "+
				image_ctrl->getImageAssetID().asString()
				, FALSE, image_ctrl->getImageAssetID(), TRUE);*/
		}
	}

	
}

// static
void LLPanelAvatarSecondLife::onDoubleClickGroup(void* data)
{
	LLPanelAvatarSecondLife* self = (LLPanelAvatarSecondLife*)data;
	
	LLScrollListCtrl*	group_list =  self->getChild<LLScrollListCtrl>("groups"); 
	if(group_list)
	{
		LLScrollListItem* item = group_list->getFirstSelected();
		if (item)
		{
			LLGroupActions::show(item->getUUID());
		}
	}
}

// static
void LLPanelAvatarSecondLife::onClickPublishHelp(void *)
{
	LLNotificationsUtil::add("ClickPublishHelpAvatar");
}

// static
void LLPanelAvatarSecondLife::onClickPartnerHelp(void *)
{
	LLNotificationsUtil::add("ClickPartnerHelpAvatar", LLSD(), LLSD(), onClickPartnerHelpLoadURL);
}

// static 
bool LLPanelAvatarSecondLife::onClickPartnerHelpLoadURL(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		LLWeb::loadURL("http://secondlife.com/partner");
	}
	return false;
}

// static
void LLPanelAvatarSecondLife::onClickPartnerInfo(void *data)
{
	LLPanelAvatarSecondLife* self = (LLPanelAvatarSecondLife*) data;
	LLAvatarActions::showProfile(self->mPartnerID);
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

BOOL LLPanelAvatarSecondLife::postBuild(void)
{
	childSetEnabled("born", FALSE);
	childSetEnabled("partner_edit", FALSE);
	childSetAction("partner_help",onClickPartnerHelp,this);
	childSetAction("partner_info", onClickPartnerInfo, this);
	childSetEnabled("partner_info", mPartnerID.notNull());
	
	childSetAction("?",onClickPublishHelp,this);
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
	getChild<LLUICtrl>("GroupInvite_Button")->setCommitCallback(boost::bind(LLAvatarActions::inviteToGroup, boost::bind(&LLPanelAvatar::getAvatarID, pa)));

	getChild<LLUICtrl>("Add Friend...")->setCommitCallback(boost::bind(LLAvatarActions::requestFriendshipDialog, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
	getChild<LLUICtrl>("Pay...")->setCommitCallback(boost::bind(LLAvatarActions::pay, boost::bind(&LLPanelAvatar::getAvatarID, pa)));
	getChild<LLUICtrl>("Mute")->setCommitCallback(boost::bind(LLAvatarActions::toggleBlock, boost::bind(&LLPanelAvatar::getAvatarID, pa)));

	getChild<LLUICtrl>("Offer Teleport...")->setCommitCallback(boost::bind(static_cast<void(*)(const LLUUID&)>(LLAvatarActions::offerTeleport), boost::bind(&LLPanelAvatar::getAvatarID, pa)));

	getChild<LLScrollListCtrl>("groups")->setDoubleClickCallback(boost::bind(&LLPanelAvatarSecondLife::onDoubleClickGroup,this));
	
	childSetAction("bigimg", onClickImage, this);
	
	getChild<LLTextureCtrl>("img")->setFallbackImageName("default_profile_picture.j2c");

	return TRUE;
}

BOOL LLPanelAvatarFirstLife::postBuild(void)
{
	BOOL own_avatar = (getPanelAvatar()->getAvatarID() == gAgent.getID() );
	enableControls(own_avatar);

	getChild<LLTextureCtrl>("img")->setFallbackImageName("default_profile_picture.j2c");

	childSetAction("flbigimg", onClickImage, this);
	return TRUE;
}

BOOL LLPanelAvatarNotes::postBuild(void)
{
	getChild<LLUICtrl>("notes edit")->setCommitCallback(boost::bind(&LLPanelAvatar::sendAvatarNotesUpdate, getPanelAvatar()));
	
	LLTextEditor*	te = getChild<LLTextEditor>("notes edit");
	if(te) te->setCommitOnFocusLost(TRUE);
	return TRUE;
}

BOOL LLPanelAvatarWeb::postBuild(void)
{
	LLLineEditor* url_edit = getChild<LLLineEditor>("url_edit");
	url_edit->setKeystrokeCallback(boost::bind(&LLPanelAvatarWeb::onURLKeystroke,this,_1));
	url_edit->setCommitCallback(boost::bind(&LLPanelAvatarWeb::onCommitURL,this,_2));

	getChild<LLUICtrl>("load")->setCommitCallback(boost::bind(&LLPanelAvatarWeb::onCommitLoad, this, _2));

	childSetAction("web_profile_help",onClickWebProfileHelp,this);

	mWebBrowser = getChild<LLMediaCtrl>("profile_html");
	mWebBrowser->addObserver(this);

	// links open in internally 
	//mWebBrowser->setOpenInExternalBrowser( false );

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

BOOL LLPanelAvatarClassified::postBuild(void)
{
	childSetAction("New...",onClickNew,this);
	childSetAction("Delete...",onClickDelete,this);
	return TRUE;
}

BOOL LLPanelAvatarPicks::postBuild(void)
{
	childSetAction("New...",onClickNew,this);
	childSetAction("Delete...",onClickDelete,this);
	
	//For pick import and export - RK
	childSetAction("Import...",onClickImport,this);
	childSetAction("Export...",onClickExport,this);
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
	};
}

void LLPanelAvatarWeb::refresh()
{
	if (mNavigateTo != "")
	{
		llinfos << "Loading " << mNavigateTo << llendl;
		mWebBrowser->navigateTo( mNavigateTo );
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
	bool have_url = !mHome.empty();
	
	childSetText("url_edit", mHome);
	childSetEnabled("load", mHome.length() > 0);

	if (have_url
		&& gSavedSettings.getBOOL("AutoLoadWebProfiles"))
	{
		if (changed_url)
		{
			load(mHome);
		}
	}
	else
	{
		childSetVisible("profile_html",false);
		childSetVisible("status_text", false);
	}
		BOOL own_avatar = (getPanelAvatar()->getAvatarID() == gAgent.getID() );
	childSetVisible("status_text",!own_avatar && !mHome.empty());
}


void LLPanelAvatarWeb::onCommitURL(const LLSD& value)
{
	load(value.asString());
}

// static
void LLPanelAvatarWeb::onClickWebProfileHelp(void *)
{
	LLNotificationsUtil::add("ClickWebProfileHelpAvatar");
}

void LLPanelAvatarWeb::load(std::string url)
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

void LLPanelAvatarWeb::onURLKeystroke(LLLineEditor* editor)
{
	LLSD::String url = editor->getText();
	childSetEnabled("load", url.length() > 0);
	return;
}

void LLPanelAvatarWeb::onCommitLoad(const LLSD& value)
{
	LLSD::String valstr = value.asString();
	LLSD::String urlstr = childGetText("url_edit");
	if (valstr == "") // load url string into browser panel
	{
		load(urlstr);
	}
	else if (valstr == "open") // open in user's external browser
	{
		if (!urlstr.empty())
		{
			LLWeb::loadURLExternal(urlstr);
		}
	}
	else if (valstr == "home") // reload profile owner's home page
	{
		if (!mHome.empty())
		{
			load(mHome);
		}
	}
}



void LLPanelAvatarWeb::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
			childSetText("status_text", self->getStatusText() );
		break;
		
		case MEDIA_EVENT_LOCATION_CHANGED:
			childSetText("url_edit", self->getLocation() );
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
	S32 t;
	for(t=0;t<mWantToCount;t++)
	{
		if(mWantToCheck[t])mWantToCheck[t]->setEnabled(self);
	}
	for(t=0;t<mSkillsCount;t++)
	{
		if(mSkillsCheck[t])mSkillsCheck[t]->setEnabled(self);
	}

	if (mWantToEdit) mWantToEdit->setEnabled(self);
	if (mSkillsEdit) mSkillsEdit->setEnabled(self);
	childSetEnabled("languages_edit",self);
}

void LLPanelAvatarAdvanced::setWantSkills(U32 want_to_mask, const std::string& want_to_text,
										  U32 skills_mask, const std::string& skills_text,
										  const std::string& languages_text)
{
	for(int id =0;id<mWantToCount;id++)
	{
		mWantToCheck[id]->set( want_to_mask & 1<<id );
	}
	for(int id =0;id<mSkillsCount;id++)
	{
		mSkillsCheck[id]->set( skills_mask & 1<<id );
	}
	if (mWantToEdit && mSkillsEdit)
	{
		mWantToEdit->setText( want_to_text );
		mSkillsEdit->setText( skills_text );
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
		for(int t=0;t<mWantToCount;t++)
		{
			if(mWantToCheck[t]->get())
				*want_to_mask |= 1<<t;
		}
	}
	if (skills_mask)
	{
		*skills_mask = 0;
		for(int t=0;t<mSkillsCount;t++)
		{
			if(mSkillsCheck[t]->get())
				*skills_mask |= 1<<t;
		}
	}
	if (mWantToEdit)
	{
		want_to_text = mWantToEdit->getText();
	}

	if (mSkillsEdit)
	{
		skills_text = mSkillsEdit->getText();
	}

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
	childSetText("notes edit", getString("Loading"));
	childSetEnabled("notes edit", false);
}


//-----------------------------------------------------------------------------
// LLPanelAvatarClassified()
//-----------------------------------------------------------------------------
LLPanelAvatarClassified::LLPanelAvatarClassified(const std::string& name, const LLRect& rect,
									   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar)
{
}


void LLPanelAvatarClassified::refresh()
{
	BOOL self = (gAgent.getID() == getPanelAvatar()->getAvatarID());
	
	LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");
	
	S32 tab_count = tabs ? tabs->getTabCount() : 0;

	bool allow_new = tab_count < MAX_CLASSIFIEDS;
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
	allow_new &= !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
// [/RLVa:KB]
	bool allow_delete = (tab_count > 0);
	bool show_help = (tab_count == 0);

	// *HACK: Don't allow making new classifieds from inside the directory.
	// The logic for save/don't save when closing is too hairy, and the 
	// directory is conceptually read-only. JC
	bool in_directory = false;
	LLView* view = this;
	while (view)
	{
		if (view->getName() == "directory")
		{
			in_directory = true;
			break;
		}
		view = view->getParent();
	}
	childSetEnabled("New...", self && !in_directory && allow_new);
	childSetVisible("New...", !in_directory);
	childSetEnabled("Delete...", self && !in_directory && allow_delete);
	childSetVisible("Delete...", !in_directory);
	childSetVisible("classified tab",!show_help);

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
		LLPanelClassified* panel = (LLPanelClassified*)tabs->getPanelByIndex(i);
		if (!panel->canClose())
		{
			return FALSE;
		}
	}
	return TRUE;
}

BOOL LLPanelAvatarClassified::titleIsValid()
{
	LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");
	if ( tabs )
	{
		LLPanelClassified* panel = (LLPanelClassified*)tabs->getCurrentPanel();
		if ( panel )
		{
			if ( ! panel->titleIsValid() )
			{
				return FALSE;
			};
		};
	};

	return TRUE;
}

void LLPanelAvatarClassified::apply()
{
	LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");
	for (S32 i = 0; i < tabs->getTabCount(); i++)
	{
		LLPanelClassified* panel = (LLPanelClassified*)tabs->getPanelByIndex(i);
		panel->apply();
	}
}


void LLPanelAvatarClassified::deleteClassifiedPanels()
{
	LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");
	if (tabs)
	{
		tabs->deleteAllTabs();
	}

	childSetVisible("New...", false);
	childSetVisible("Delete...", false);
	childSetVisible("loading_text", true);
}

// virtual
void LLPanelAvatarClassified::processProperties(void* data, EAvatarProcessorType type)
{
	if(type == APT_CLASSIFIEDS)
	{
		LLAvatarClassifieds* c_info = static_cast<LLAvatarClassifieds*>(data);
		if(c_info && mAvatarID == c_info->target_id)
		{
			LLTabContainer* tabs = getChild<LLTabContainer>("classified tab");
			
			for(LLAvatarClassifieds::classifieds_list_t::iterator it = c_info->classifieds_list.begin();
				it != c_info->classifieds_list.end(); ++it)
			{
				LLPanelClassified* panel_classified = new LLPanelClassified(false, false);

				panel_classified->setClassifiedID(it->classified_id);

				// This will request data from the server when the pick is first drawn.
				panel_classified->markForServerRequest();

				// The button should automatically truncate long names for us
				if(tabs)
				{
					tabs->addTabPanel(panel_classified, it->name);
				}
			}

			// Make sure somebody is highlighted.  This works even if there
			// are no tabs in the container.
			if(tabs)
			{
				tabs->selectFirstTab();
			}

			childSetVisible("New...", true);
			childSetVisible("Delete...", true);
			childSetVisible("loading_text", false);
		}
	}
}

// Create a new classified panel.  It will automatically handle generating
// its own id when it's time to save.
// static
void LLPanelAvatarClassified::onClickNew(void* data)
{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		return;
	}
// [/RLVa:KB]
	LLPanelAvatarClassified* self = (LLPanelAvatarClassified*)data;

	LLNotificationsUtil::add("AddClassified", LLSD(), LLSD(), boost::bind(&LLPanelAvatarClassified::callbackNew, self, _1, _2));
		
}

bool LLPanelAvatarClassified::callbackNew(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		LLPanelClassified* panel_classified = new LLPanelClassified(false, false);
		panel_classified->initNewClassified();
		LLTabContainer*	tabs = getChild<LLTabContainer>("classified tab");
		if(tabs)
		{
			tabs->addTabPanel(panel_classified, panel_classified->getClassifiedName());
			tabs->selectLastTab();
		}
	}
	return false;
}


// static
void LLPanelAvatarClassified::onClickDelete(void* data)
{
	LLPanelAvatarClassified* self = (LLPanelAvatarClassified*)data;

	LLTabContainer*	tabs = self->getChild<LLTabContainer>("classified tab");
	LLPanelClassified* panel_classified = NULL;
	if(tabs)
	{
		panel_classified = (LLPanelClassified*)tabs->getCurrentPanel();
	}
	if (!panel_classified) return;

	LLSD args;
	args["NAME"] = panel_classified->getClassifiedName();
	LLNotificationsUtil::add("DeleteClassified", args, LLSD(), boost::bind(&LLPanelAvatarClassified::callbackDelete, self, _1, _2));
		
}


bool  LLPanelAvatarClassified::callbackDelete(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLTabContainer*	tabs = getChild<LLTabContainer>("classified tab");
	LLPanelClassified* panel_classified=NULL;
	if(tabs)
	{
		panel_classified = (LLPanelClassified*)tabs->getCurrentPanel();
	}

	if (!panel_classified) return false;

	if (0 == option)
	{
		LLAvatarPropertiesProcessor::getInstance()->sendClassifiedDelete(panel_classified->getClassifiedID());

		if(tabs)
		{
			tabs->removeTabPanel(panel_classified);
		}
		delete panel_classified;
		panel_classified = NULL;
	}
	return false;
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
	BOOL self = (gAgent.getID() == getPanelAvatar()->getAvatarID());
	LLTabContainer*	tabs = getChild<LLTabContainer>("picks tab");
	S32 tab_count = tabs ? tabs->getTabCount() : 0;
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
	childSetEnabled("New...", self && tab_count < MAX_AVATAR_PICKS && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) );
// [/RLVa:KB]
	//childSetEnabled("New...",    self && tab_count < MAX_AVATAR_PICKS);
	childSetEnabled("Delete...", self && tab_count > 0);
	childSetVisible("New...",    self && getPanelAvatar()->isEditable());
	childSetVisible("Delete...", self && getPanelAvatar()->isEditable());

	//For pick import/export - RK
	childSetVisible("Import...", self && getPanelAvatar()->isEditable());
	childSetEnabled("Export...", self && tab_count > 0);
	childSetVisible("Export...", self && getPanelAvatar()->isEditable());

	if (!isDataRequested())
	{
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarPicksRequest(mAvatarID);
		setDataRequested(true);
	}
}


void LLPanelAvatarPicks::deletePickPanels()
{
	LLTabContainer* tabs = getChild<LLTabContainer>("picks tab");
	if(tabs)
	{
		tabs->deleteAllTabs();
	}

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
	if(type == APT_PICKS)
	{
		LLAvatarPicks* picks = static_cast<LLAvatarPicks*>(data);

		//llassert_always(picks->target_id != gAgent.getID());
		//llassert_always(mAvatarID != gAgent.getID());

		if(picks && mAvatarID == picks->target_id)
		{
			LLTabContainer* tabs =  getChild<LLTabContainer>("picks tab");

			// Clear out all the old panels.  We'll replace them with the correct
			// number of new panels.
			deletePickPanels();

			for(LLAvatarPicks::picks_list_t::iterator it = picks->picks_list.begin();
				it != picks->picks_list.end(); ++it)
			{
				LLPanelPick* panel_pick = new LLPanelPick();
				panel_pick->setPickID(it->first, mAvatarID);

				// This will request data from the server when the pick is first
				// drawn.
				panel_pick->markForServerRequest();

				// The button should automatically truncate long names for us
				if(tabs)
				{
					llinfos << "Adding tab for " << mAvatarID << " " << ((mAvatarID == gAgent.getID()) ? "Self" : "Other") << ": '" << it->second << "'" << llendl;
					tabs->addTabPanel(panel_pick, it->second);
				}
			}

			// Make sure somebody is highlighted.  This works even if there
			// are no tabs in the container.
			if(tabs)
			{
				tabs->selectFirstTab();
			}

			childSetVisible("New...", true);
			childSetVisible("Delete...", true);
			childSetVisible("loading_text", false);

			//For pick import and export - RK
			childSetVisible("Import...", true);
			childSetVisible("Export...", true);
		}
	}
}

// Create a new pick panel.  It will automatically handle generating
// its own id when it's time to save.
// static
void LLPanelAvatarPicks::onClickNew(void* data)
{
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		return;
	}
// [/RLVa:KB]
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	LLPanelPick* panel_pick = new LLPanelPick();
	LLTabContainer* tabs =  self->getChild<LLTabContainer>("picks tab");

	panel_pick->initNewPick();
	if(tabs)
	{
		tabs->addTabPanel(panel_pick, panel_pick->getPickName());
		tabs->selectLastTab();
	}
}

//Pick import and export - RK
// static
void LLPanelAvatarPicks::onClickImport(void* data)
{
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	self->mPanelPick = new LLPanelPick();
	self->mPanelPick->importNewPick(&LLPanelAvatarPicks::onClickImport_continued, data);
}

// static
void LLPanelAvatarPicks::onClickImport_continued(void* data, bool import)
{
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	LLTabContainer* tabs = self->getChild<LLTabContainer>("picks tab");
	if(tabs && import && self->mPanelPick)
	{
		tabs->addTabPanel(self->mPanelPick, self->mPanelPick->getPickName());
		tabs->selectLastTab();
	}
}

// static
void LLPanelAvatarPicks::onClickExport(void* data)
{
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	LLTabContainer* tabs =  self->getChild<LLTabContainer>("picks tab");
	LLPanelPick* panel_pick = tabs?(LLPanelPick*)tabs->getCurrentPanel():NULL;

	if (!panel_pick) return;

	panel_pick->exportPick();
}


// static
void LLPanelAvatarPicks::onClickDelete(void* data)
{
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	LLTabContainer* tabs =  self->getChild<LLTabContainer>("picks tab");
	LLPanelPick* panel_pick = tabs?(LLPanelPick*)tabs->getCurrentPanel():NULL;

	if (!panel_pick) return;

	LLSD args;
	args["PICK"] = panel_pick->getPickName();

	LLNotificationsUtil::add("DeleteAvatarPick", args, LLSD(),
									boost::bind(&LLPanelAvatarPicks::callbackDelete, self, _1, _2));
}


// static
bool LLPanelAvatarPicks::callbackDelete(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLTabContainer* tabs = getChild<LLTabContainer>("picks tab");
	LLPanelPick* panel_pick = tabs ? (LLPanelPick*)tabs->getCurrentPanel() : NULL;
	LLMessageSystem* msg = gMessageSystem;

	if (!panel_pick) return false;

	if (0 == option)
	{
		// If the viewer has a hacked god-mode, then this call will
		// fail.
		if(gAgent.isGodlike())
		{
			msg->newMessage("PickGodDelete");			
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgent.getID());
			msg->addUUID("SessionID", gAgent.getSessionID());
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
		

		if(tabs)
		{
			tabs->removeTabPanel(panel_pick);
		}
		delete panel_pick;
		panel_pick = NULL;
	}
	return false;
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

BOOL LLPanelAvatar::postBuild(void)
{
	mTab = getChild<LLTabContainer>("tab");
	getChild<LLUICtrl>("Kick")->setCommitCallback(boost::bind(LLAvatarActions::kick, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	getChild<LLUICtrl>("Freeze")->setCommitCallback(boost::bind(LLAvatarActions::freeze, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	getChild<LLUICtrl>("Unfreeze")->setCommitCallback(boost::bind(LLAvatarActions::unfreeze, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	getChild<LLUICtrl>("csr_btn")->setCommitCallback(boost::bind(LLAvatarActions::csr, boost::bind(&LLPanelAvatar::getAvatarID, this)));
	childSetAction("OK", onClickOK, this);
	childSetAction("Cancel", onClickCancel, this);

	childSetAction("copy_key",onClickGetKey,this);
	getChildView("web_profile")->setVisible(!gSavedSettings.getString("WebProfileURL").empty());

	if(mTab && !sAllowFirstLife)
	{
		LLPanel* panel = mTab->getPanelByName("1st Life");
		if (panel) mTab->removeTabPanel(panel);

		panel = mTab->getPanelByName("WebProfile");
		if (panel) mTab->removeTabPanel(panel);
	}
	childSetVisible("Kick",FALSE);
	childSetEnabled("Kick",FALSE);
	childSetVisible("Freeze",FALSE);
	childSetEnabled("Freeze",FALSE);
	childSetVisible("Unfreeze",FALSE);
	childSetEnabled("Unfreeze",FALSE);
	childSetVisible("csr_btn", FALSE);
	childSetEnabled("csr_btn", FALSE);

	//This text never changes. We simply toggle visibility.
	childSetVisible("online_yes", FALSE);
	childSetColor("online_yes",LLColor4::green);
	childSetValue("online_yes","Currently Online");

	return TRUE;
}


LLPanelAvatar::~LLPanelAvatar()
{
	LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID,this);
	sAllPanels.remove(this);
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
		&& (LLAvatarTracker::instance().isBuddyOnline( mAvatarID )))
	{
		online_status = ONLINE_STATUS_YES;
	}
	
	if(mPanelSecondLife)
	mPanelSecondLife->childSetVisible("online_yes", online_status == ONLINE_STATUS_YES);

	// Since setOnlineStatus gets called after setAvatarID
	// need to make sure that "Offer Teleport" doesn't get set
	// to TRUE again for yourself
	if (mAvatarID != gAgent.getID())
	{
		childSetVisible("Offer Teleport...",TRUE);
		childSetVisible("Find on Map", true);
	}

	BOOL in_prelude = gAgent.inPrelude();
	if(gAgent.isGodlike())
	{
		childSetEnabled("Offer Teleport...", TRUE);
		childSetToolTip("Offer Teleport...", getString("TeleportGod"));
	}
	else if (in_prelude)
	{
		childSetEnabled("Offer Teleport...",FALSE);
		childSetToolTip("Offer Teleport...", getString("TeleportPrelude"));
	}
	else
	{
		childSetEnabled("Offer Teleport...", TRUE /*(online_status == ONLINE_STATUS_YES)*/);
		childSetToolTip("Offer Teleport...", getString("TeleportNormal"));
	}

	// Note: we don't always know online status, so always allow gods to try to track
	childSetEnabled("Find on Map", gAgent.isGodlike() || is_agent_mappable(mAvatarID));
	if (!mIsFriend)
	{
		childSetToolTip("Find on Map", getString("ShowOnMapNonFriend"));
	}
	else if (ONLINE_STATUS_YES != online_status)
	{
		childSetToolTip("Find on Map", getString("ShowOnMapFriendOffline"));
	}
	else
	{
		childSetToolTip("Find on Map", getString("ShowOnMapFriendOnline"));
	}
}

void LLPanelAvatar::onAvatarNameResponse(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	std::string name;
	if (gSavedSettings.getBOOL("SinguCompleteNameProfiles"))
		name = av_name.getCompleteName();
	else
		LLAvatarNameCache::getPNSName(av_name, name);
	getChild<LLLineEditor>("dnname")->setText(name);
}

void LLPanelAvatar::setAvatarID(const LLUUID &avatar_id)
{
	if (avatar_id.isNull()) return;

	//BOOL avatar_changed = FALSE;
	if (avatar_id != mAvatarID)
	{
		//avatar_changed = TRUE;
		if(mAvatarID.notNull())
		{
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
		}
		mAvatarID = avatar_id;
	}

	LLAvatarPropertiesProcessor::getInstance()->addObserver(mAvatarID, this);

	// Determine if we have their calling card.
	mIsFriend = LLAvatarActions::isFriend(mAvatarID);

	// setOnlineStatus uses mIsFriend
	setOnlineStatus(ONLINE_STATUS_NO);

	BOOL own_avatar = (mAvatarID == gAgent.getID() );

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

	LLAvatarNameCache::get(avatar_id, boost::bind(&LLPanelAvatar::onAvatarNameResponse, this, _1, _2));

	LLNameEditor* key_edit = getChild<LLNameEditor>("avatar_key");
	if(key_edit)
	{
		key_edit->setText(mAvatarID.asString());
	}
// 	if (avatar_changed)
	{
		// While we're waiting for data off the network, clear out the
		// old data.
		if(mPanelSecondLife) mPanelSecondLife->clearControls();

		if(mPanelPicks) mPanelPicks->deletePickPanels();
		if(mPanelPicks) mPanelPicks->setDataRequested(false);

		if(mPanelClassified) mPanelClassified->deleteClassifiedPanels();
		if(mPanelClassified) mPanelClassified->setDataRequested(false);

		if(mPanelNotes) mPanelNotes->clearControls();
		if(mPanelNotes) mPanelNotes->setDataRequested(false);
		mHaveNotes = false;
		mLastNotes.clear();

		// Request just the first two pages of data.  The picks,
		// classifieds, and notes will be requested when that panel
		// is made visible. JC
		sendAvatarPropertiesRequest();

		if (own_avatar)
		{
			if (mAllowEdit)
			{
				// OK button disabled until properties data arrives
				childSetVisible("OK", true);
				childSetEnabled("OK", false);
				childSetVisible("Cancel",TRUE);
				childSetEnabled("Cancel",TRUE);
			}
			else
			{
				childSetVisible("OK",FALSE);
				childSetEnabled("OK",FALSE);
				childSetVisible("Cancel",FALSE);
				childSetEnabled("Cancel",FALSE);
			}
			childSetVisible("Instant Message...",FALSE);
			childSetEnabled("Instant Message...",FALSE);
			childSetVisible("GroupInvite_Button",FALSE);
			childSetEnabled("GroupInvite_Button",FALSE);
			childSetVisible("Mute",FALSE);
			childSetEnabled("Mute",FALSE);
			childSetVisible("Offer Teleport...",FALSE);
			childSetEnabled("Offer Teleport...",FALSE);
			childSetVisible("Find on Map",FALSE);
			childSetEnabled("Find on Map",FALSE);
			childSetVisible("Add Friend...",FALSE);
			childSetEnabled("Add Friend...",FALSE);
			childSetVisible("Pay...",FALSE);
			childSetEnabled("Pay...",FALSE);
		}
		else
		{
			childSetVisible("OK",FALSE);
			childSetEnabled("OK",FALSE);

			childSetVisible("Cancel",FALSE);
			childSetEnabled("Cancel",FALSE);

			childSetVisible("Instant Message...",TRUE);
			childSetEnabled("Instant Message...",FALSE);
			childSetVisible("GroupInvite_Button",TRUE);
			childSetEnabled("GroupInvite_Button",FALSE);
			childSetVisible("Mute",TRUE);
			childSetEnabled("Mute",FALSE);

			childSetVisible("Add Friend...", true);
			childSetEnabled("Add Friend...", !mIsFriend);
			childSetVisible("Pay...",TRUE);
			childSetEnabled("Pay...",FALSE);
		}
		LLNameEditor* avatar_key = getChild<LLNameEditor>("avatar_key");
		if (avatar_key)
		{
			avatar_key->setText(avatar_id.asString());
		}
	}
	
	bool is_god = gAgent.isGodlike();
	childSetVisible("Kick", is_god);
	childSetEnabled("Kick", is_god);
	childSetVisible("Freeze", is_god);
	childSetEnabled("Freeze", is_god);
	childSetVisible("Unfreeze", is_god);
	childSetEnabled("Unfreeze", is_god);
	childSetVisible("csr_btn", is_god);
	childSetEnabled("csr_btn", is_god);
}


void LLPanelAvatar::resetGroupList()
{
	// only get these updates asynchronously via the group floater, which works on the agent only
	if (mAvatarID != gAgent.getID())
	{
		return;
	}
		
	if (mPanelSecondLife)
	{
		LLScrollListCtrl* group_list = mPanelSecondLife->getChild<LLScrollListCtrl>("groups");
		if (group_list)
		{
			const LLUUID selected_id	= group_list->getSelectedValue();
			const S32	selected_idx	= group_list->getFirstSelectedIndex();
			const S32	scroll_pos		= group_list->getScrollPos();

			group_list->deleteAllItems();
			
			S32 count = gAgent.mGroups.count();
			LLUUID id;
			
			for(S32 i = 0; i < count; ++i)
			{
				LLGroupData group_data = gAgent.mGroups.get(i);
				id = group_data.mID;
				std::string group_string;
				/* Show group title?  DUMMY_POWER for Don Grep
				   if(group_data.mOfficer)
				   {
				   group_string = "Officer of ";
				   }
				   else
				   {
				   group_string = "Member of ";
				   }
				*/

				group_string += group_data.mName;

				LLSD row;

				row["id"] = id ;
				row["columns"][0]["value"] = group_string;
				row["columns"][0]["font"] = "SANSSERIF_SMALL";
				std::string font_style = group_data.mListInProfile ? "BOLD" : "NORMAL";
				if(group_data.mID == gAgent.getGroupID())
					font_style.append("|ITALIC");
				row["columns"][0]["font-style"] = font_style;
				row["columns"][0]["width"] = 0;
				group_list->addElement(row,ADD_SORTED);
			}
			if(selected_id.notNull())
				group_list->selectByValue(selected_id);
			if(selected_idx!=group_list->getFirstSelectedIndex()) //if index changed then our stored pos is pointless.
				group_list->scrollToShowSelected();
			else
				group_list->setScrollPos(scroll_pos);
		}
	}
}

//static
void LLPanelAvatar::onClickGetKey(void *userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*)userdata;
	LLUUID agent_id = self->getAvatarID();

	llinfos << "Copy agent id: " << agent_id << llendl;

	gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(agent_id.asString()));
}

// static
void LLPanelAvatar::onClickOK(void *userdata)
{
	LLPanelAvatar *self = (LLPanelAvatar *)userdata;

	// JC: Only save the data if we actually got the original
	// properties.  Otherwise we might save blanks into
	// the database.
	if (self
		&& self->mHaveProperties)
	{
		self->sendAvatarPropertiesUpdate();

		LLTabContainer* tabs = self->getChild<LLTabContainer>("tab");
		if ( tabs->getCurrentPanel() != self->mPanelClassified )
		{
			self->mPanelClassified->apply();

			LLFloaterAvatarInfo *infop = LLFloaterAvatarInfo::getInstance(self->mAvatarID);
			if (infop)
			{
				infop->close();
			}
		}
		else
		{
			if ( self->mPanelClassified->titleIsValid() )
			{
				self->mPanelClassified->apply();

				LLFloaterAvatarInfo *infop = LLFloaterAvatarInfo::getInstance(self->mAvatarID);
				if (infop)
				{
					infop->close();
				}
			}
		}
	}
}

// static
void LLPanelAvatar::onClickCancel(void *userdata)
{
	LLPanelAvatar *self = (LLPanelAvatar *)userdata;

	if (self)
	{
		LLFloaterAvatarInfo *infop;
		if ((infop = LLFloaterAvatarInfo::getInstance(self->mAvatarID)))
		{
			infop->close();
		}
		else
		{
			// We're in the Search directory and are cancelling an edit
			// to our own profile, so reset.
			self->sendAvatarPropertiesRequest();
		}
	}
}


void LLPanelAvatar::sendAvatarPropertiesRequest()
{
	lldebugs << "LLPanelAvatar::sendAvatarPropertiesRequest()" << llendl; 

	LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(mAvatarID);
}

void LLPanelAvatar::sendAvatarNotesUpdate()
{
	std::string notes = mPanelNotes->childGetValue("notes edit").asString();

	if (!mHaveNotes
		&& (notes.empty() || notes == getString("Loading")))
	{
		// no notes from server and no user updates
		return;
	}
	if (notes == mLastNotes)
	{
		// Avatar notes unchanged
		return;
	}

	LLAvatarPropertiesProcessor::getInstance()->sendNotes(mAvatarID,notes);
}

// virtual
void LLPanelAvatar::processProperties(void* data, EAvatarProcessorType type)
{
	if(type == APT_PROPERTIES)
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
			
			
			bool online = (pAvatarData->flags & AVATAR_ONLINE);
						
			EOnlineStatus online_status = (online) ? ONLINE_STATUS_YES : ONLINE_STATUS_NO;
			
			setOnlineStatus(online_status);
			
			childSetValue("about", pAvatarData->about_text);
		}
	}
	else if(type == APT_NOTES)
	{
		const LLAvatarNotes* pAvatarNotes = static_cast<const LLAvatarNotes*>( data );
		if (pAvatarNotes && (mAvatarID == pAvatarNotes->target_id) && (pAvatarNotes->target_id != LLUUID::null))
		{
			childSetValue("notes edit", pAvatarNotes->notes);
			childSetEnabled("notes edit", true);
			mHaveNotes = true;
			mLastNotes = pAvatarNotes->notes;
		}
	}
}

// Don't enable the OK button until you actually have the data.
// Otherwise you will write blanks back into the database.
void LLPanelAvatar::enableOKIfReady()
{
	if(mHaveProperties && childIsVisible("OK"))
	{
		childSetEnabled("OK", TRUE);
	}
	else
	{
		childSetEnabled("OK", FALSE);
	}
}

void LLPanelAvatar::sendAvatarPropertiesUpdate()
{
	llinfos << "Sending avatarinfo update" << llendl;
	BOOL allow_publish = FALSE;
	BOOL mature = FALSE;
	if (LLPanelAvatar::sAllowFirstLife)
	{
		allow_publish = childGetValue("allow_publish");
		//A profile should never be mature.
		mature = FALSE;
	}

	LLUUID first_life_image_id;
	std::string first_life_about_text;
	if (mPanelFirstLife)
	{
		first_life_about_text = mPanelFirstLife->childGetValue("about").asString();
		LLTextureCtrl*	image_ctrl = mPanelFirstLife->getChild<LLTextureCtrl>("img");
		if(image_ctrl)
		{
			first_life_image_id = image_ctrl->getImageAssetID();
		}
	}

	std::string about_text = mPanelSecondLife->childGetValue("about").asString();

	LLAvatarData avatar_data;
	avatar_data.image_id = mPanelSecondLife->getChild<LLTextureCtrl>("img")->getImageAssetID();
	avatar_data.fl_image_id = first_life_image_id;
	avatar_data.about_text = about_text;
	avatar_data.fl_about_text = first_life_about_text;
	avatar_data.allow_publish = allow_publish;
	//avatar_data.mature = mature;
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
	if(mTab)
	{
		mTab->selectTab(tabnum);
	}
}

void LLPanelAvatar::selectTabByName(std::string tab_name)
{
	if (mTab)
	{
		if (tab_name.empty())
		{
			mTab->selectFirstTab();
		}
		else
		{
			mTab->selectTabByName(tab_name);
		}
	}
}

void*	LLPanelAvatar::createPanelAvatarSecondLife(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelSecondLife = new LLPanelAvatarSecondLife(std::string("2nd Life"),LLRect(),self);
	return self->mPanelSecondLife;
}

void*	LLPanelAvatar::createPanelAvatarWeb(void*	data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelWeb = new LLPanelAvatarWeb(std::string("Web"),LLRect(),self);
	return self->mPanelWeb;
}

void*	LLPanelAvatar::createPanelAvatarInterests(void*	data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelAdvanced = new LLPanelAvatarAdvanced(std::string("Interests"),LLRect(),self);
	return self->mPanelAdvanced;
}


void*	LLPanelAvatar::createPanelAvatarPicks(void*	data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelPicks = new LLPanelAvatarPicks(std::string("Picks"),LLRect(),self);
	return self->mPanelPicks;
}

void*	LLPanelAvatar::createPanelAvatarClassified(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelClassified = new LLPanelAvatarClassified(std::string("Classified"),LLRect(),self);
	return self->mPanelClassified;
}

void*	LLPanelAvatar::createPanelAvatarFirstLife(void*	data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelFirstLife = new LLPanelAvatarFirstLife(std::string("1st Life"), LLRect(), self);
	return self->mPanelFirstLife;
}

void*	LLPanelAvatar::createPanelAvatarNotes(void*	data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelNotes = new LLPanelAvatarNotes(std::string("My Notes"),LLRect(),self);
	return self->mPanelNotes;
}
