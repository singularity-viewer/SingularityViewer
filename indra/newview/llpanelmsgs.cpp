/** 
 * @file llpanelmsgs.cpp
 * @brief Message popup preferences panel
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llpanelmsgs.h"

#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llfirstuse.h"

//-----------------------------------------------------------------------------
LLPanelMsgs::LLPanelMsgs() : 
	LLPanel("Messages Panel")
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_popups.xml");
}


LLPanelMsgs::~LLPanelMsgs()
{ }

//-----------------------------------------------------------------------------
// postBuild()
//-----------------------------------------------------------------------------
BOOL LLPanelMsgs::postBuild()
{
	childSetAction("enable_popup", onClickEnablePopup, this);
	childSetAction("reset_dialogs_btn", onClickResetDialogs, this);
	childSetAction("skip_dialogs_btn", onClickSkipDialogs, this);
	buildLists();

	childSetValue("accept_new_inventory", gSavedSettings.getBOOL("AutoAcceptNewInventory"));
	childSetValue("show_new_inventory", gSavedSettings.getBOOL("ShowNewInventory"));
	childSetValue("show_in_inventory", gSavedSettings.getBOOL("ShowInInventory"));

	return TRUE;
}

void LLPanelMsgs::buildLists()
{
	LLScrollListCtrl& disabled_popups = getChildRef<LLScrollListCtrl>("disabled_popups");
	LLScrollListCtrl& enabled_popups = getChildRef<LLScrollListCtrl>("enabled_popups");

	disabled_popups.deleteAllItems();
	enabled_popups.deleteAllItems();

	for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
		iter != LLNotifications::instance().templatesEnd();
		++iter)
	{
		LLNotificationTemplatePtr templatep = iter->second;
		LLNotificationFormPtr formp = templatep->mForm;

		LLNotificationForm::EIgnoreType ignore = formp->getIgnoreType();
		if (ignore == LLNotificationForm::IGNORE_NO)
				continue;

		LLSD row;
		row["columns"][0]["value"] = formp->getIgnoreMessage();
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		row["columns"][0]["width"] = 300;

		LLScrollListItem* item = NULL;

		bool show_popup = gSavedSettings.getWarning(templatep->mName);
		if (!show_popup)
		{
			if (ignore == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
			{
				LLSD last_response = LLUI::sConfigGroup->getLLSD("Default" + templatep->mName);
				if (!last_response.isUndefined())
				{
					for (LLSD::map_const_iterator it = last_response.beginMap();
						it != last_response.endMap();
						++it)
					{
						if (it->second.asBoolean())
						{
							row["columns"][1]["value"] = formp->getElement(it->first)["ignore"].asString();
							break;
						}
					}
				}
				row["columns"][1]["font"] = "SANSSERIF_SMALL";
				row["columns"][1]["width"] = 160;
			}
			item = disabled_popups.addElement(row,
							   ADD_SORTED);
		}
		else
		{
			item = enabled_popups.addElement(row,
							  ADD_SORTED);
		}

		if (item)
		{
			item->setUserdata((void*)&iter->first);
		}
	}
}

void LLPanelMsgs::draw()
{
	LLScrollListCtrl& disabled_popups = getChildRef<LLScrollListCtrl>("disabled_popups");

	if (disabled_popups.getFirstSelected())
	{
		childEnable("enable_popup");
	}
	else
	{
		childDisable("enable_popup");
	}

	LLPanel::draw();
}


void LLPanelMsgs::apply()
{
	gSavedSettings.setBOOL("AutoAcceptNewInventory", childGetValue("accept_new_inventory"));
	gSavedSettings.setBOOL("ShowNewInventory", childGetValue("show_new_inventory"));
	gSavedSettings.setBOOL("ShowInInventory", childGetValue("show_in_inventory"));
}

void LLPanelMsgs::cancel()
{
}

void LLPanelMsgs::resetAllIgnored()
{
	for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
		iter != LLNotifications::instance().templatesEnd();
		++iter)
	{
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(iter->first, TRUE);
		}
	}
}

void LLPanelMsgs::setAllIgnored()
{
	for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
		iter != LLNotifications::instance().templatesEnd();
		++iter)
	{
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(iter->first, FALSE);
		}
	}
}

//static 
void LLPanelMsgs::onClickEnablePopup(void* user_data)
{
	LLPanelMsgs* panelp = (LLPanelMsgs*)user_data;

	LLScrollListCtrl& disabled_popups = panelp->getChildRef<LLScrollListCtrl>("disabled_popups");

	std::vector<LLScrollListItem*> items = disabled_popups.getAllSelected();
	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate(*(std::string*)((*itor)->getUserdata()));
		gSavedSettings.setWarning(templatep->mName, TRUE);
	}

	panelp->buildLists();
}

bool callback_reset_dialogs(const LLSD& notification, const LLSD& response, LLPanelMsgs* panelp)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		if ( panelp )
		{
			panelp->resetAllIgnored();
			LLFirstUse::resetFirstUse();
			panelp->buildLists();
		}
	}
	return false;
}

// static
void LLPanelMsgs::onClickResetDialogs(void* user_data)
{
	LLNotificationsUtil::add("ResetShowNextTimeDialogs", LLSD(), LLSD(), boost::bind(&callback_reset_dialogs, _1, _2, (LLPanelMsgs*)user_data));
}

bool callback_skip_dialogs(const LLSD& notification, const LLSD& response, LLPanelMsgs* panelp)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		if ( panelp )
		{
			panelp->setAllIgnored();
			LLFirstUse::disableFirstUse();
			panelp->buildLists();
		}
	}
	return false;
}

// static
void LLPanelMsgs::onClickSkipDialogs(void* user_data)
{
	LLNotificationsUtil::add("SkipShowNextTimeDialogs", LLSD(), LLSD(), boost::bind(&callback_skip_dialogs, _1, _2, (LLPanelMsgs*)user_data));
}
