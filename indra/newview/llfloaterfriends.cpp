/** 
 * @file llfloaterfriends.cpp
 * @author Phoenix
 * @date 2005-01-13
 * @brief Implementation of the friends floater
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloaterfriends.h"

#include <sstream>

#include "lldir.h"

#include "llagent.h"
#include "llappviewer.h"	// for gLastVersionChannel

// [Ansariel: Display name support]
#include "llavatarname.h"
#include "llavatarnamecache.h"
// [/Ansariel: Display name support]

#include "llfloateravatarpicker.h"
#include "llviewerwindow.h"
#include "llbutton.h"
#include "llfloateravatarinfo.h"
#include "llinventorymodel.h"
#include "llnamelistctrl.h"
#include "llnotificationsutil.h"
#include "llresmgr.h"
#include "llimview.h"
#include "lluictrlfactory.h"
#include "llmenucommands.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "lleventtimer.h"
#include "lltextbox.h"
#include "llvoiceclient.h"

#include "llsdserialize.h"
#include "statemachine/aifilepicker.h"

#include "llviewermenufile.h"
#include "llviewermenu.h"
#include "llviewernetwork.h"
#include "hippogridmanager.h"

#include "llchat.h"
#include "llfloaterchat.h"

// <dogmode> stuff for Contact groups
#include "ascentfloatercontactgroups.h"

//Maximum number of people you can select to do an operation on at once.
#define MAX_FRIEND_SELECT 20
#define DEFAULT_PERIOD 5.0
#define RIGHTS_CHANGE_TIMEOUT 5.0
#define OBSERVER_TIMEOUT 0.5

#define ONLINE_SIP_ICON_NAME "slim_icon_16_viewer.tga"

// simple class to observe the calling cards.
class LLLocalFriendsObserver : public LLFriendObserver, public LLEventTimer
{
public: 
	LLLocalFriendsObserver(LLPanelFriends* floater) : mFloater(floater), LLEventTimer(OBSERVER_TIMEOUT)
	{
		mEventTimer.stop();
	}
	virtual ~LLLocalFriendsObserver()
	{
		mFloater = NULL;
	}
	virtual void changed(U32 mask)
	{
		// events can arrive quickly in bulk - we need not process EVERY one of them -
		// so we wait a short while to let others pile-in, and process them in aggregate.
		mEventTimer.start();

		// save-up all the mask-bits which have come-in
		mMask |= mask;
	}
	virtual BOOL tick()
	{
		mFloater->populateContactGroupSelect();
		mFloater->updateFriends(mMask);

		mEventTimer.stop();
		mMask = 0;

		return FALSE;
	}
	
protected:
	LLPanelFriends* mFloater;
	U32 mMask;
};

LLPanelFriends::LLPanelFriends() :
	LLPanel(),
	LLEventTimer(DEFAULT_PERIOD),
	mObserver(NULL),
	mShowMaxSelectWarning(TRUE),
	mAllowRightsChange(TRUE),
	mNumRightsChanged(0),
	mNumOnline(0)
{
	mEventTimer.stop();
	mObserver = new LLLocalFriendsObserver(this);
	LLAvatarTracker::instance().addObserver(mObserver);
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->addObserver(mObserver);
}

LLPanelFriends::~LLPanelFriends()
{
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->removeObserver(mObserver);
	LLAvatarTracker::instance().removeObserver(mObserver);
	delete mObserver;
}

BOOL LLPanelFriends::tick()
{
	mEventTimer.stop();
	mPeriod = DEFAULT_PERIOD;
	mAllowRightsChange = TRUE;
	updateFriends(LLFriendObserver::ADD);
	return FALSE;
}

void LLPanelFriends::updateFriends(U32 changed_mask)
{
	LLUUID selected_id;
	LLCtrlListInterface *friends_list = childGetListInterface("friend_list");
	if (!friends_list) return;
	LLCtrlScrollInterface *friends_scroll = childGetScrollInterface("friend_list");
	if (!friends_scroll) return;
	
	// We kill the selection warning, otherwise we'll spam with warning popups
	// if the maximum amount of friends are selected
	mShowMaxSelectWarning = false;

	LLDynamicArray<LLUUID> selected_friends = getSelectedIDs();
	if(changed_mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE))
	{
		refreshNames(changed_mask);
	}
	else if(changed_mask & LLFriendObserver::POWERS)
	{
		--mNumRightsChanged;
		if(mNumRightsChanged > 0)
		{
			mPeriod = RIGHTS_CHANGE_TIMEOUT;	
			mEventTimer.start();
			mAllowRightsChange = FALSE;
		}
		else
		{
			tick();
		}
	}
	if(selected_friends.size() > 0)
	{
		// only non-null if friends was already found. This may fail,
		// but we don't really care here, because refreshUI() will
		// clean up the interface.
		friends_list->setCurrentByID(selected_id);
		for(LLDynamicArray<LLUUID>::iterator itr = selected_friends.begin(); itr != selected_friends.end(); ++itr)
		{
			friends_list->setSelectedByValue(*itr, true);
		}
	}

	refreshUI();
	mShowMaxSelectWarning = true;
}

// <dogmode>
// Contact search and group system.
// 09/05/2010 - Charley Levenque
std::string LLPanelFriends::cleanFileName(std::string filename)
{
	std::string invalidChars = "\"\'\\/?*:<>|";
	S32 position = filename.find_first_of(invalidChars);
	while (position != filename.npos)
	{
		filename[position] = '_';
		position = filename.find_first_of(invalidChars, position);
	}
	return filename;
}

void LLPanelFriends::populateContactGroupSelect()
{
	LLComboBox* combo = getChild<LLComboBox>("buddy_group_combobox");

	if (combo)
	{
		combo->removeall();
		combo->add("All", ADD_BOTTOM);

		LLSD groups = gSavedPerAccountSettings.getLLSD("AscentContactGroups");

		S32 count = groups["ASC_MASTER_GROUP_LIST"].size();
		S32 index;
		for (index = 0; index < count; index++)
		{
			combo->addSimpleElement(groups["ASC_MASTER_GROUP_LIST"][index].asString(), ADD_BOTTOM);
		}
	}
	else
	{
		LLChat msg("Null combo");
		LLFloaterChat::addChat(msg);
	}
}

void LLPanelFriends::setContactGroup(std::string contact_grp)
{
	LLChat msg("Group set to " + contact_grp);
	LLFloaterChat::addChat(msg);
	refreshNames(LLFriendObserver::ADD);
	refreshUI();
	categorizeContacts();
}

void LLPanelFriends::categorizeContacts()
{
	LLSD contact_groups = gSavedPerAccountSettings.getLLSD("AscentContactGroups");
	std::string group_name = "All";

	LLComboBox* combo = getChild<LLComboBox>("buddy_group_combobox");
	if (combo)
	{
		group_name = combo->getValue().asString();

		if (group_name != "All")
		{
			std::vector<LLScrollListItem*> vFriends = mFriendsList->getAllData(); // all of it.
			for (std::vector<LLScrollListItem*>::iterator itr = vFriends.begin(); itr != vFriends.end(); ++itr)
			{
				BOOL show_entry = false;//contact_groups[group_name].has((*itr)->getUUID().asString());

				S32 count = contact_groups[group_name].size();
				int i;
				for(i = 0; i < count; i++)
				{
					if (contact_groups[group_name][i].asString() == (*itr)->getUUID().asString())
					{
						show_entry = true;
						break;
					}
				}

				if (!show_entry)
				{
					LLChat msg("False: contact_groups['" + group_name + "'].has('" + (*itr)->getUUID().asString() + "');");
					LLFloaterChat::addChat(msg);
					mFriendsList->deleteItems((*itr)->getValue());
				}
				else
				{
					LLChat msg("True: contact_groups['" + group_name + "'].has('" + (*itr)->getUUID().asString() + "');");
					LLFloaterChat::addChat(msg);
				}
			}
		}
		else
		{
			LLChat msg("Group set to all.");
			LLFloaterChat::addChat(msg);
		}

		refreshUI();
	}
	else
	{
		LLChat msg("Null combo.");
		LLFloaterChat::addChat(msg);
	}
}

void LLPanelFriends::filterContacts()
{
	std::string friend_name;
	std::string search_name;

	search_name = LLPanelFriends::getChild<LLLineEditor>("buddy_search_lineedit")->getValue().asString();

	if ((search_name != "" /*&& search_name != mLastContactSearch*/))
	{	
		mLastContactSearch = search_name;
		refreshNames(LLFriendObserver::ADD);

		std::vector<LLScrollListItem*> vFriends = mFriendsList->getAllData(); // all of it.
		for (std::vector<LLScrollListItem*>::iterator itr = vFriends.begin(); itr != vFriends.end(); ++itr)
		{
			friend_name = utf8str_tolower((*itr)->getColumn(LIST_FRIEND_NAME)->getValue().asString());
			BOOL show_entry = (friend_name.find(utf8str_tolower(search_name)) != std::string::npos);

			if (!show_entry)
			{
				mFriendsList->deleteItems((*itr)->getValue());
			}
		}

		refreshUI();
	}
	else if (search_name == "" && search_name != mLastContactSearch) refreshNames(LLFriendObserver::ADD);
}

void LLPanelFriends::onContactSearchKeystroke(LLLineEditor* caller, void* user_data)
{
	if (caller)
	{
		LLPanelFriends* panelp = (LLPanelFriends*)caller->getParent();
		if (panelp)
		{
			panelp->filterContacts();
		}
	}
}

void LLPanelFriends::onChangeContactGroup(LLUICtrl* ctrl, void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	if(panelp)
	{
		LLComboBox* combo = panelp->getChild<LLComboBox>("buddy_group_combobox");
		panelp->setContactGroup(combo->getValue().asString());
	}
}
// --

// virtual
BOOL LLPanelFriends::postBuild()
{
	mFriendsList = getChild<LLScrollListCtrl>("friend_list");
	mFriendsList->setMaxSelectable(MAX_FRIEND_SELECT);
	mFriendsList->setMaximumSelectCallback(onMaximumSelect);
	mFriendsList->setCommitOnSelectionChange(TRUE);
	childSetCommitCallback("friend_list", onSelectName, this);
	childSetCommitCallback("buddy_group_combobox", onChangeContactGroup, this);
	childSetDoubleClickCallback("friend_list", onClickIM);

	// <dogmode>
	// Contact search and group system.
	// 09/05/2010 - Charley Levenque
	LLLineEditor* contact = getChild<LLLineEditor>("buddy_search_lineedit");
	if (contact)
	{
		contact->setKeystrokeCallback(&onContactSearchKeystroke);
	}

	getChild<LLTextBox>("s_num")->setValue("0");
	getChild<LLTextBox>("f_num")->setValue(llformat("%d", mFriendsList->getItemCount()));

	U32 changed_mask = LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE;
	refreshNames(changed_mask);

	childSetAction("im_btn", onClickIM, this);
	childSetAction("assign_btn", onClickAssign, this);
	childSetAction("expand_collapse_btn", onClickExpand, this);
	childSetAction("profile_btn", onClickProfile, this);
	childSetAction("offer_teleport_btn", onClickOfferTeleport, this);
	childSetAction("pay_btn", onClickPay, this);
	childSetAction("add_btn", onClickAddFriend, this);
	childSetAction("remove_btn", onClickRemove, this);
	//childSetAction("export_btn", onClickExport, this); Making Dummy View -HgB
	//childSetAction("import_btn", onClickImport, this); Making Dummy View -HgB

	setDefaultBtn("im_btn");

	updateFriends(LLFriendObserver::ADD);
	refreshUI();

	// primary sort = online status, secondary sort = name
	mFriendsList->sortByColumn(std::string("friend_name"), TRUE);
	mFriendsList->sortByColumn(std::string("icon_online_status"), FALSE);

	updateColumns(this);

	return TRUE;
}

BOOL LLPanelFriends::addFriend(const LLUUID& agent_id)
{
	LLAvatarTracker& at = LLAvatarTracker::instance();
	const LLRelationship* relationInfo = at.getBuddyInfo(agent_id);
	if(!relationInfo) return FALSE;

	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(agent_id);
	bool isOnline = relationInfo->isOnline();

	std::string fullname;
	// [Ansariel: Display name support]
	//BOOL have_name = gCacheName->getFullName(agent_id, fullname);
	LLAvatarName avatar_name;
	BOOL have_name;
	if (LLAvatarNameCache::get(agent_id, &avatar_name))
	{
		static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
		switch (phoenix_name_system)
		{
			case 0 : fullname = avatar_name.getLegacyName(); break;
			case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
			case 2 : fullname = avatar_name.mDisplayName; break;
			default : fullname = avatar_name.getCompleteName(); break;
		}

		have_name = TRUE;
	}
	else have_name = FALSE;
	// [/Ansariel: Display name support]
	
	LLSD element;
	element["id"] = agent_id;
	LLSD& friend_column = element["columns"][LIST_FRIEND_NAME];
	friend_column["column"] = "friend_name";
	friend_column["value"] = fullname;
	friend_column["font"] = "SANSSERIF";
	friend_column["font-style"] = "NORMAL";	

	LLSD& online_status_column = element["columns"][LIST_ONLINE_STATUS];
	online_status_column["column"] = "icon_online_status";
	online_status_column["type"] = "icon";

	if (isOnline)
	{
		mNumOnline++;
		friend_column["font-style"] = "BOLD";	
		online_status_column["value"] = "icon_avatar_online.tga";
	}
	else if(isOnlineSIP)
	{
		mNumOnline++;
		friend_column["font-style"] = "BOLD";	
		online_status_column["value"] = ONLINE_SIP_ICON_NAME;
	}

	LLSD& online_column = element["columns"][LIST_VISIBLE_ONLINE];
	online_column["column"] = "icon_visible_online";
	online_column["type"] = "checkbox";
	online_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS);

	LLSD& visible_map_column = element["columns"][LIST_VISIBLE_MAP];
	visible_map_column["column"] = "icon_visible_map";
	visible_map_column["type"] = "checkbox";
	visible_map_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION);

	LLSD& edit_my_object_column = element["columns"][LIST_EDIT_MINE];
	edit_my_object_column["column"] = "icon_edit_mine";
	edit_my_object_column["type"] = "checkbox";
	edit_my_object_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);
	
	LLSD& see_online_them_column = element["columns"][LIST_VISIBLE_ONLINE_THEIRS];
	see_online_them_column["column"] = "icon_visible_online_theirs";
	see_online_them_column["type"] = "checkbox";
	see_online_them_column["enabled"] = "";
	see_online_them_column["value"] = relationInfo->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS);



	LLSD& mapstalk_them_column = element["columns"][LIST_VISIBLE_MAP_THEIRS];
	mapstalk_them_column["column"] = "icon_visible_map_theirs";
	mapstalk_them_column["type"] = "checkbox";
	mapstalk_them_column["enabled"] = "";
	mapstalk_them_column["value"] = relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION);

	LLSD& edit_their_object_column = element["columns"][LIST_EDIT_THEIRS];
	edit_their_object_column["column"] = "icon_edit_theirs";
	edit_their_object_column["type"] = "checkbox";
	edit_their_object_column["enabled"] = "";
	edit_their_object_column["value"] = relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& update_gen_column = element["columns"][LIST_FRIEND_UPDATE_GEN];
	update_gen_column["column"] = "friend_last_update_generation";
	update_gen_column["value"] = have_name ? relationInfo->getChangeSerialNum() : -1;

	mFriendsList->addElement(element, ADD_BOTTOM);
	return have_name;
}

// propagate actual relationship to UI.
// Does not resort the UI list because it can be called frequently. JC
BOOL LLPanelFriends::updateFriendItem(const LLUUID& agent_id, const LLRelationship* info)
{
	if (!info) return FALSE;
	LLScrollListItem* itemp = mFriendsList->getItem(agent_id);
	if (!itemp) return FALSE;
	
	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(itemp->getUUID());
	bool isOnline = info->isOnline();

	std::string fullname;	
	// [Ansariel: Display name support]
	//BOOL have_name = gCacheName->getFullName(agent_id, fullname);
	LLAvatarName avatar_name;
	BOOL have_name;
	if (LLAvatarNameCache::get(agent_id, &avatar_name))
	{
		static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
		switch (phoenix_name_system)
		{
			case 0 : fullname = avatar_name.getLegacyName(); break;
			case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
			case 2 : fullname = avatar_name.mDisplayName; break;
			default : fullname = avatar_name.getCompleteName(); break;
		}

		have_name = TRUE;
	}
	else have_name = FALSE;
	// [/Ansariel: Display name support]

	// Name of the status icon to use
	std::string statusIcon;
	
	if(isOnline)
	{
		mNumOnline++;
		statusIcon = "icon_avatar_online.tga";
	}
	else if(isOnlineSIP)
	{
		mNumOnline++;
		statusIcon = ONLINE_SIP_ICON_NAME;
	}
	else
	{
		mNumOnline--;
	}

	itemp->getColumn(LIST_ONLINE_STATUS)->setValue(statusIcon);
	
	itemp->getColumn(LIST_FRIEND_NAME)->setValue(fullname);
	// render name of online friends in bold text
	((LLScrollListText*)itemp->getColumn(LIST_FRIEND_NAME))->setFontStyle((isOnline || isOnlineSIP) ? LLFontGL::BOLD : LLFontGL::NORMAL);	
	itemp->getColumn(LIST_VISIBLE_ONLINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
	itemp->getColumn(LIST_VISIBLE_MAP)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_MINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));
	//Notes in the original code imply this may not always work. Good to know. -HgB
	itemp->getColumn(LIST_VISIBLE_ONLINE_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS));
	

	itemp->getColumn(LIST_VISIBLE_MAP_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS));

	S32 change_generation = have_name ? info->getChangeSerialNum() : -1;
	itemp->getColumn(LIST_FRIEND_UPDATE_GEN)->setValue(change_generation);

	// enable this item, in case it was disabled after user input
	itemp->setEnabled(TRUE);

	// Do not resort, this function can be called frequently.
	return have_name;
}

void LLPanelFriends::refreshRightsChangeList()
{

	LLDynamicArray<LLUUID> friends = getSelectedIDs();
	
	S32 num_selected = friends.size();
	bool can_offer_teleport = num_selected >= 1;
	bool selected_friends_online = true;

	/*LLTextBox* processing_label = getChild<LLTextBox>("process_rights_label");

	if(!mAllowRightsChange)
	{
		if(processing_label)
		{
			processing_label->setVisible(true);
			// ignore selection for now
			friends.clear();
			num_selected = 0;
		}
	}
	else if(processing_label)
	{
		processing_label->setVisible(false);
	} Making Dummy View -HgB */
	const LLRelationship* friend_status = NULL;
	for(LLDynamicArray<LLUUID>::iterator itr = friends.begin(); itr != friends.end(); ++itr)
	{
		friend_status = LLAvatarTracker::instance().getBuddyInfo(*itr);
		if (friend_status)
		{
			if(!friend_status->isOnline())
			{
				can_offer_teleport = false;
				selected_friends_online = false;
			}
		}
		else // missing buddy info, don't allow any operations
		{
			can_offer_teleport = false;
		}
	}

	//Stuff for the online/total/select counts.
	
	getChild<LLTextBox>("s_num")->setValue(llformat("%d", num_selected));
	getChild<LLTextBox>("f_num")->setValue(llformat("%d / %d", mNumOnline, mFriendsList->getItemCount()));
	
	if (num_selected == 0)  // nothing selected
	{
		childSetEnabled("im_btn", FALSE);
		childSetEnabled("assign_btn", FALSE);
		childSetEnabled("offer_teleport_btn", FALSE);
	}
	else // we have at least one friend selected...
	{
		// only allow IMs to groups when everyone in the group is online
		// to be consistent with context menus in inventory and because otherwise
		// offline friends would be silently dropped from the session
		childSetEnabled("im_btn", selected_friends_online || num_selected == 1);
		childSetEnabled("assign_btn", num_selected == 1);
		childSetEnabled("offer_teleport_btn", can_offer_teleport);
	}
}

struct SortFriendsByID
{
	bool operator() (const LLScrollListItem* const a, const LLScrollListItem* const b) const
	{
		return a->getValue().asUUID() < b->getValue().asUUID();
	}
};

void LLPanelFriends::refreshNames(U32 changed_mask)
{
	LLDynamicArray<LLUUID> selected_ids = getSelectedIDs();	
	S32 pos = mFriendsList->getScrollPos();	
	
	// get all buddies we know about
	LLAvatarTracker::buddy_map_t all_buddies;
	LLAvatarTracker::instance().copyBuddyList(all_buddies);

	BOOL have_names = TRUE;

	if(changed_mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE))
	{
		have_names &= refreshNamesSync(all_buddies);
	}

	if(changed_mask & LLFriendObserver::ONLINE)
	{
		have_names &= refreshNamesPresence(all_buddies);
	}

	if (!have_names)
	{
		mEventTimer.start();
	}
	// Changed item in place, need to request sort and update columns
	// because we might have changed data in a column on which the user
	// has already sorted. JC
	mFriendsList->sortItems();

	// re-select items
	mFriendsList->selectMultiple(selected_ids);
	mFriendsList->setScrollPos(pos);
}

BOOL LLPanelFriends::refreshNamesSync(const LLAvatarTracker::buddy_map_t & all_buddies)
{
	mNumOnline = 0;
	mFriendsList->deleteAllItems();

	BOOL have_names = TRUE;
	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();

	for(; buddy_it != all_buddies.end(); ++buddy_it)
	{
		have_names &= addFriend(buddy_it->first);
	}

	return have_names;
}

BOOL LLPanelFriends::refreshNamesPresence(const LLAvatarTracker::buddy_map_t & all_buddies)
{

	std::vector<LLScrollListItem*> items = mFriendsList->getAllData();
	std::sort(items.begin(), items.end(), SortFriendsByID());

	LLAvatarTracker::buddy_map_t::const_iterator buddy_it  = all_buddies.begin();
	std::vector<LLScrollListItem*>::const_iterator item_it = items.begin();
	BOOL have_names = TRUE;

	while(true)
	{
		if(item_it == items.end() || buddy_it == all_buddies.end())
		{
			break;
		}

		const LLUUID & buddy_uuid = buddy_it->first;
		const LLUUID & item_uuid  = (*item_it)->getValue().asUUID();
		if(item_uuid == buddy_uuid)
		{
			const LLRelationship* info = buddy_it->second;
			if (!info) 
			{	
				++item_it;
				continue;
			}
			
			S32 last_change_generation = (*item_it)->getColumn(LIST_FRIEND_UPDATE_GEN)->getValue().asInteger();
			if (last_change_generation < info->getChangeSerialNum())
			{
				// update existing item in UI
				have_names &= updateFriendItem(buddy_it->first, info);
			}

			++buddy_it;
			++item_it;
		}
		else if(item_uuid < buddy_uuid)
		{
			++item_it;
		}
		else //if(item_uuid > buddy_uuid)
		{
			++buddy_it;
		}
	}

	return have_names;
}
// meal disk
void LLPanelFriends::refreshUI()
{	
	BOOL single_selected = FALSE;
	BOOL multiple_selected = FALSE;
	int num_selected = mFriendsList->getAllSelected().size();
	if(num_selected > 0)
	{
		single_selected = TRUE;
		if(num_selected > 1)
		{
			multiple_selected = TRUE;		
		}
	}

	//Options that can only be performed with one friend selected
	childSetEnabled("profile_btn", single_selected && !multiple_selected);
	childSetEnabled("pay_btn", single_selected && !multiple_selected);
	childSetEnabled("assign_btn", single_selected && !multiple_selected);

	//Options that can be performed with up to MAX_FRIEND_SELECT friends selected
	//(single_selected will always be true in this situations)
	childSetEnabled("remove_btn", single_selected);
	childSetEnabled("im_btn", single_selected);
	//childSetEnabled("friend_rights", single_selected); Making Dummy View -HgB

	refreshRightsChangeList();
}

LLDynamicArray<LLUUID> LLPanelFriends::getSelectedIDs()
{
	LLUUID selected_id;
	LLDynamicArray<LLUUID> friend_ids;
	std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
	for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		friend_ids.push_back((*itr)->getUUID());
	}
	return friend_ids;
}

// static 
void LLPanelFriends::onSelectName(LLUICtrl* ctrl, void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	if(panelp)
	{
		panelp->refreshUI();
		// check to see if rights have changed
		panelp->applyRightsToFriends();
	}
}

//static
void LLPanelFriends::onMaximumSelect(void* user_data)
{
	LLSD args;
	args["MAX_SELECT"] = llformat("%d", MAX_FRIEND_SELECT);
	LLNotificationsUtil::add("MaxListSelectMessage", args);
};

// static
void LLPanelFriends::onClickProfile(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	//llinfos << "LLPanelFriends::onClickProfile()" << llendl;
	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() > 0)
	{
		LLUUID agent_id = ids[0];
		BOOL online;
		online = LLAvatarTracker::instance().isBuddyOnline(agent_id);
		LLFloaterAvatarInfo::showFromFriend(agent_id, online);
	}
}

// static
void LLPanelFriends::onClickAssign(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;
	if (panelp)
	{
		LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
		ASFloaterContactGroups::show(ids);
	}
}

// static
void LLPanelFriends::onClickExpand(void* user_data)
{
	BOOL collapsed = gSavedSettings.getBOOL("ContactListCollapsed");
	gSavedSettings.setBOOL("ContactListCollapsed", !collapsed);
	updateColumns(user_data);
}

void LLPanelFriends::updateColumns(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;
	if (panelp)
	{
		LLButton* expand_button = panelp->getChild<LLButton>("expand_collapse_btn");
		LLScrollListCtrl* list = panelp->getChild<LLScrollListCtrl>("friend_list");
		//llinfos << "Refreshing UI" << llendl;
		S32 width = 22;
		std::string button = ">";
		if (gSavedSettings.getBOOL("ContactListCollapsed"))
		{
			width = 0;
			button = "<";
		}
		expand_button->setLabel(button);
		LLScrollListColumn* column = list->getColumn(5);
		list->updateStaticColumnWidth(column, width);
		column->setWidth(width);
		column = list->getColumn(6);
		list->updateStaticColumnWidth(column, width);
		column->setWidth(width);
		column = list->getColumn(7);
		list->updateStaticColumnWidth(column, width);
		column->setWidth(width);
		list->updateLayout();
		if (!gSavedSettings.getBOOL("ContactListCollapsed"))
		{
			panelp->updateFriends(LLFriendObserver::ADD);
		}
	}
}

void LLPanelFriends::onClickIM(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	//llinfos << "LLPanelFriends::onClickIM()" << llendl;
	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() > 0)
	{
		if(ids.size() == 1)
		{
			LLUUID agent_id = ids[0];
			const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(agent_id);
			// [Ansariel: Display name support]
			//std::string fullname;
			//if(info && gCacheName->getFullName(agent_id, fullname))
			//{
			//	gIMMgr->setFloaterOpen(TRUE);
			//	gIMMgr->addSession(fullname, IM_NOTHING_SPECIAL, agent_id);
			//}
			LLAvatarName avatar_name;
			if (info && LLAvatarNameCache::get(agent_id, &avatar_name))
			{
				gIMMgr->setFloaterOpen(TRUE);
				gIMMgr->addSession(avatar_name.getLegacyName(),IM_NOTHING_SPECIAL,agent_id);
			}
			// [/Ansariel: Display name support]
		}
		else
		{
			gIMMgr->setFloaterOpen(TRUE);
			gIMMgr->addSession("Friends Conference", IM_SESSION_CONFERENCE_START, ids[0], ids);
		}
		make_ui_sound("UISndStartIM");
	}
}

// static
void LLPanelFriends::requestFriendship(const LLUUID& target_id, const std::string& target_name, const std::string& message)
{
	LLUUID calling_card_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	send_improved_im(target_id,
					 target_name,
					 message,
					 IM_ONLINE,
					 IM_FRIENDSHIP_OFFERED,
					 calling_card_folder_id);
}

// static
bool LLPanelFriends::callbackAddFriendWithMessage(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		requestFriendship(notification["payload"]["id"].asUUID(), 
		    notification["payload"]["name"].asString(),
		    response["message"].asString());
	}
	return false;
}

bool LLPanelFriends::callbackAddFriend(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		// Servers older than 1.25 require the text of the message to be the
		// calling card folder ID for the offering user. JC
		LLUUID calling_card_folder_id = 
			gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		std::string message = calling_card_folder_id.asString();
		requestFriendship(notification["payload"]["id"].asUUID(), 
		    notification["payload"]["name"].asString(),
		    message);
	}
    return false;
}

// static
void LLPanelFriends::onPickAvatar(const std::vector<std::string>& names,
									const std::vector<LLUUID>& ids,
									void* )
{
	if (names.empty()) return;
	if (ids.empty()) return;
	requestFriendshipDialog(ids[0], names[0]);
}

// static
void LLPanelFriends::requestFriendshipDialog(const LLUUID& id,
											   const std::string& name)
{
	if(id == gAgentID)
	{
		LLNotificationsUtil::add("AddSelfFriend");
		return;
	}

	LLSD args;
	args["NAME"] = name;
	LLSD payload;
	payload["id"] = id;
	payload["name"] = name;
    // Look for server versions like: Second Life Server 1.24.4.95600
	if (gLastVersionChannel.find(" 1.24.") != std::string::npos)
	{
		// Old and busted server version, doesn't support friend
		// requests with messages.
    	LLNotificationsUtil::add("AddFriend", args, payload, &callbackAddFriend);
	}
	else
	{
    	LLNotificationsUtil::add("AddFriendWithMessage", args, payload, &callbackAddFriendWithMessage);
	}
}

// static
void LLPanelFriends::onClickAddFriend(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;
	LLFloater* root_floater = gFloaterView->getParentFloater(panelp);
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(onPickAvatar, user_data, FALSE, TRUE);
	if (root_floater)
	{
		root_floater->addDependentFloater(picker);
	}
}

// static
void LLPanelFriends::onClickRemove(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	//llinfos << "LLPanelFriends::onClickRemove()" << llendl;
	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	LLSD args;
	if(ids.size() > 0)
	{
		std::string msgType = "RemoveFromFriends";
		if(ids.size() == 1)
		{
			LLUUID agent_id = ids[0];
			// [Ansariel: Display name support]
			//std::string first, last;
			//if(gCacheName->getName(agent_id, first, last))
			//{
			//	args["FIRST_NAME"] = first;
			//	args["LAST_NAME"] = last;	
			//}

			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(agent_id, &avatar_name))
			{
				std::string fullname;
				static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
				switch (phoenix_name_system)
				{
					case 0 : fullname = avatar_name.getLegacyName(); break;
					case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
					case 2 : fullname = avatar_name.mDisplayName; break;
					default : fullname = avatar_name.getCompleteName(); break;
				}
				
				args["NAME"] = fullname;
			}
			// [/Ansariel: Display name support]
		}
		else
		{
			msgType = "RemoveMultipleFromFriends";
		}
		LLSD payload;

		for (LLDynamicArray<LLUUID>::iterator it = ids.begin();
			it != ids.end();
			++it)
		{
			payload["ids"].append(*it);
		}

		LLNotifications::instance().add(msgType,
			args,
			payload,
			&handleRemove);
	}
}

void LLPanelFriends::onClickExport(void* user_data)
{
	std::string agn;
	gAgent.getName(agn);
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(agn + ".friendlist", FFSAVE_ALL);
	filepicker->run(boost::bind(&LLPanelFriends::onClickExport_continued, user_data, filepicker));
}

void LLPanelFriends::onClickExport_continued(void* user_data, AIFilePicker* filepicker)
{
	if(!filepicker->hasFilename())
	{
		// User canceled save.
		return;
	}
	std::string const filename = filepicker->getFilename();
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;
	std::vector<LLScrollListItem*> selected = panelp->mFriendsList->getAllData();//->getAllSelected();

	LLSD llsd;
	//U32 count = 0;
	//std::string friendlist;
	for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		LLSD fraind;
		std::string friend_name = (*itr)->getColumn(LIST_FRIEND_NAME)->getValue().asString();
		std::string friend_uuid = (*itr)->getUUID().asString();
		bool show_online_status = (*itr)->getColumn(LIST_VISIBLE_ONLINE)->getValue().asBoolean();
		bool show_map_location = (*itr)->getColumn(LIST_VISIBLE_MAP)->getValue().asBoolean();
		bool allow_modify_objects = (*itr)->getColumn(LIST_EDIT_MINE)->getValue().asBoolean();
		fraind["name"] = friend_name;
		fraind["see_online"] = show_online_status;
		fraind["can_map"] = show_map_location;
		fraind["can_mod"] = allow_modify_objects;
		//friendlist += friend_name+"|"+friend_uuid+"|"+show_online_status+"|"+show_map_location+"|"+allow_modify_objects+"\n";
		llsd[friend_uuid] = fraind;
		//count += 1;
	}

	std::string grid_uri = gHippoGridManager->getConnectedGrid()->getLoginUri();
	//LLStringUtil::toLower(uris[0]);
	llsd["GRID"] = grid_uri;

	llofstream export_file;
	export_file.open(filename);
	LLSDSerialize::toPrettyXML(llsd, export_file);
	export_file.close();
}

bool LLPanelFriends::merging;

void LLPanelFriends::onClickImport(void* user_data)
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open();
	filepicker->run(boost::bind(&LLPanelFriends::onClickImport_filepicker_continued, filepicker));
}

//THIS CODE IS DESIGNED SO THAT EXP/IMP BETWEEN GRIDS WILL FAIL
//because assuming someone having the same name on another grid is the same person is generally a bad idea
//i might add the option to query the user as to intelligently detecting matching names on a alternative grid
// jcool410
void LLPanelFriends::onClickImport_filepicker_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
		return;

	std::string filename = filepicker->getFilename();
	llifstream importer(filename);
	LLSD data;
	LLSDSerialize::fromXMLDocument(data, importer);
	if(data.has("GRID"))
	{
		std::string grid = gHippoGridManager->getConnectedGrid()->getLoginUri();
		if(grid != data["GRID"])return;
		data.erase("GRID");
	}

#if LL_WINDOWS
	std::string file = filename.substr(filename.find_last_of("\\")+1);
#else
	std::string file = filename.substr(filename.find_last_of("/")+1);
#endif

	std::string importstate = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "friendimportstate.dat");
	
	llifstream stateload(importstate);
	LLSD importstatellsd;
	LLSDSerialize::fromXMLDocument(importstatellsd, stateload);


	//LLMessageSystem* msg = gMessageSystem;
	LLSD newdata;

	LLSD::map_const_iterator iter;
	for(iter = data.beginMap(); iter != data.endMap(); ++iter)
	{//first= var second = val
		LLSD content = iter->second;
		if(!content.has("name"))continue;
		if(!content.has("see_online"))continue;
		if(!content.has("can_map"))continue;
		if(!content.has("can_mod"))continue;

		LLUUID agent_id = LLUUID(iter->first);
		if(merging && importstatellsd.has(agent_id.asString()))continue;//dont need to request what weve already requested from another list import and have not got a reply yet

		std::string agent_name = content["name"];
		if(!is_agent_friend(agent_id))//dont need to request what we have
		{
			if(merging)importstatellsd[agent_id.asString()] = content;//MERGEEEE
			requestFriendship(agent_id, agent_name, "Imported from "+file);
			newdata[iter->first] = iter->second;
		}else 
		{
			//data.erase(iter->first);
			//--iter;//god help us all
		}
	}
	data = newdata;
	newdata = LLSD();

	if(!merging)
	{
		merging = true;//this hack is to support importing multiple account lists without spamming users but considering LLs fail in forcing silent declines
		importstatellsd = data;
	}

	llofstream export_file;
	export_file.open(importstate);
	LLSDSerialize::toPrettyXML(importstatellsd, export_file);
	export_file.close();
}

void LLPanelFriends::FriendImportState(LLUUID id, bool accepted)
{
	std::string importstate = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "friendimportstate.dat");
	if(LLFile::isfile(importstate))
	{
		llifstream importer(importstate);
		LLSD data;
		LLSDSerialize::fromXMLDocument(data, importer);

		if(!data.has(id.asString()))return;

		LLSD user = data[id.asString()];
		if(accepted)
		{
			BOOL can_map = user["can_map"].asBoolean();
			BOOL can_mod = user["can_mod"].asBoolean();
			BOOL see_online = user["see_online"].asBoolean();
			S32 rights = 0;
			if(can_map)rights |= LLRelationship::GRANT_MAP_LOCATION;
			if(can_mod)rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
			if(see_online)rights |= LLRelationship::GRANT_ONLINE_STATUS;
			if(is_agent_friend(id))//is this legit shit yo
			{
				const LLRelationship* friend_status = LLAvatarTracker::instance().getBuddyInfo(id);
				if(friend_status)
				{
					S32 tr = friend_status->getRightsGrantedTo();
					if(tr != rights)
					{
						LLMessageSystem* msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_GrantUserRights);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUID(_PREHASH_AgentID, gAgent.getID());
						msg->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
						msg->nextBlockFast(_PREHASH_Rights);
						msg->addUUID(_PREHASH_AgentRelated, id);
						msg->addS32(_PREHASH_RelatedRights, rights);
						gAgent.sendReliableMessage();
					}
				}
			}
		}
		data.erase(id.asString());//if they declined then we need to forget about it, if they accepted it is done

		llofstream export_file;
		export_file.open(importstate);
		LLSDSerialize::toPrettyXML(data, export_file);
		export_file.close();
	}
}

// static
void LLPanelFriends::onClickOfferTeleport(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() > 0)
	{	
		handle_lure(ids);
	}
}

// static
void LLPanelFriends::onClickPay(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() == 1)
	{	
		handle_pay_by_id(ids[0]);
	}
}

void LLPanelFriends::confirmModifyRights(rights_map_t& ids, EGrantRevoke command)
{
	if (ids.empty()) return;
	
	LLSD args;
	if(ids.size() > 0)
	{
		rights_map_t* rights = new rights_map_t(ids);

		// for single friend, show their name
		if(ids.size() == 1)
		{
			LLUUID agent_id = ids.begin()->first;
			//std::string first, last;
			//if(gCacheName->getName(agent_id, first, last))
			//{
			//	args["FIRST_NAME"] = first;
			//	args["LAST_NAME"] = last;	
			//}

			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(agent_id, &avatar_name))
			{
				std::string fullname;
				static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
				switch (phoenix_name_system)
				{
					case 0 : fullname = avatar_name.getLegacyName(); break;
					case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
					case 2 : fullname = avatar_name.mDisplayName; break;
					default : fullname = avatar_name.getCompleteName(); break;
				}
				
				args["NAME"] = fullname;
			}

			if (command == GRANT)
			{
				LLNotificationsUtil::add("GrantModifyRights", 
					args, 
					LLSD(), 
					boost::bind(&LLPanelFriends::modifyRightsConfirmation, this, _1, _2, rights));
			}
			else
			{
				LLNotificationsUtil::add("RevokeModifyRights", 
					args, 
					LLSD(), 
					boost::bind(&LLPanelFriends::modifyRightsConfirmation, this, _1, _2, rights));
			}
		}
		else
		{
			if (command == GRANT)
			{
				LLNotificationsUtil::add("GrantModifyRightsMultiple", 
					args, 
					LLSD(), 
					boost::bind(&LLPanelFriends::modifyRightsConfirmation, this, _1, _2, rights));
			}
			else
			{
				LLNotificationsUtil::add("RevokeModifyRightsMultiple", 
					args, 
					LLSD(), 
					boost::bind(&LLPanelFriends::modifyRightsConfirmation, this, _1, _2, rights));
			}
		}
	}
}

bool LLPanelFriends::modifyRightsConfirmation(const LLSD& notification, const LLSD& response, rights_map_t* rights)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(0 == option)
	{
		sendRightsGrant(*rights);
	}
	else
	{
		// need to resync view with model, since user cancelled operation
		rights_map_t::iterator rights_it;
		for (rights_it = rights->begin(); rights_it != rights->end(); ++rights_it)
		{
			const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(rights_it->first);
			updateFriendItem(rights_it->first, info);
		}
	}
	refreshUI();

	delete rights;
	return false;
}

void LLPanelFriends::applyRightsToFriends()
{
	BOOL rights_changed = FALSE;

	// store modify rights separately for confirmation
	rights_map_t rights_updates;

	BOOL need_confirmation = FALSE;
	EGrantRevoke confirmation_type = GRANT;

	// this assumes that changes only happened to selected items
	std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
	for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		LLUUID id = (*itr)->getValue();
		const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(id);
		if (buddy_relationship == NULL) continue;

		bool show_online_staus = (*itr)->getColumn(LIST_VISIBLE_ONLINE)->getValue().asBoolean();
		bool show_map_location = (*itr)->getColumn(LIST_VISIBLE_MAP)->getValue().asBoolean();
		bool allow_modify_objects = (*itr)->getColumn(LIST_EDIT_MINE)->getValue().asBoolean();

		S32 rights = buddy_relationship->getRightsGrantedTo();
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS) != show_online_staus)
		{
			rights_changed = TRUE;
			if(show_online_staus) 
			{
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
			}
			else 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights &= ~LLRelationship::GRANT_ONLINE_STATUS;
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
				// propagate rights constraint to UI
				(*itr)->getColumn(LIST_VISIBLE_MAP)->setValue(FALSE);
			}
		}
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION) != show_map_location)
		{
			rights_changed = TRUE;
			if(show_map_location) 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights |= LLRelationship::GRANT_MAP_LOCATION;
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
				(*itr)->getColumn(LIST_VISIBLE_ONLINE)->setValue(TRUE);
			}
			else 
			{
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
			}
		}
		
		// now check for change in modify object rights, which requires confirmation
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS) != allow_modify_objects)
		{
			rights_changed = TRUE;
			need_confirmation = TRUE;

			if(allow_modify_objects)
			{
				rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = GRANT;
			}
			else
			{
				rights &= ~LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = REVOKE;
			}
		}

		if (rights_changed)
		{
			rights_updates.insert(std::make_pair(id, rights));
			// disable these ui elements until response from server
			// to avoid race conditions
			(*itr)->setEnabled(FALSE);
		}
	}

	// separately confirm grant and revoke of modify rights
	if (need_confirmation)
	{
		confirmModifyRights(rights_updates, confirmation_type);
	}
	else
	{
		sendRightsGrant(rights_updates);
	}
}

void LLPanelFriends::sendRightsGrant(rights_map_t& ids)
{
	if (ids.empty()) return;

	LLMessageSystem* msg = gMessageSystem;

	// setup message header
	msg->newMessageFast(_PREHASH_GrantUserRights);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgent.getID());
	msg->addUUID(_PREHASH_SessionID, gAgent.getSessionID());

	rights_map_t::iterator id_it;
	rights_map_t::iterator end_it = ids.end();
	for(id_it = ids.begin(); id_it != end_it; ++id_it)
	{
		msg->nextBlockFast(_PREHASH_Rights);
		msg->addUUID(_PREHASH_AgentRelated, id_it->first);
		msg->addS32(_PREHASH_RelatedRights, id_it->second);
	}

	mNumRightsChanged = ids.size();
	gAgent.sendReliableMessage();
}



// static
bool LLPanelFriends::handleRemove(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	const LLSD& ids = notification["payload"]["ids"];
	for(LLSD::array_const_iterator itr = ids.beginArray(); itr != ids.endArray(); ++itr)
	{
		LLUUID id = itr->asUUID();
		const LLRelationship* ip = LLAvatarTracker::instance().getBuddyInfo(id);
		if(ip)
		{			
			switch(option)
			{
			case 0: // YES
				if( ip->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
				{
					LLAvatarTracker::instance().empower(id, FALSE);
					LLAvatarTracker::instance().notifyObservers();
				}
				LLAvatarTracker::instance().terminateBuddy(id);
				LLAvatarTracker::instance().notifyObservers();
				gInventory.addChangedMask(LLInventoryObserver::LABEL | LLInventoryObserver::CALLING_CARD, LLUUID::null);
				gInventory.notifyObservers();
				break;

			case 1: // NO
			default:
				llinfos << "No removal performed." << llendl;
				break;
			}
		}
		
	}
	return false;
}
