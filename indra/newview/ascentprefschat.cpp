/** 
 * @file ascentprefssys.cpp
 * @Ascent Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Henri Beauchamp.
 * Rewritten in its entirety 2010 Hg Beeks. 
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

//File include
#include "ascentprefschat.h"
#include "llagent.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llradiogroup.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "lgghunspell_wrapper.h"


LLPrefsAscentChat::LLPrefsAscentChat()
{
    LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_ascent_chat.xml");

    childSetCommitCallback("SpellBase", onSpellBaseComboBoxCommit, this);
    childSetAction("EmSpell_EditCustom", onSpellEditCustom, this);
    childSetAction("EmSpell_GetMore", onSpellGetMore, this);
    childSetAction("EmSpell_Add", onSpellAdd, this);
    childSetAction("EmSpell_Remove", onSpellRemove, this);

    childSetCommitCallback("time_format_combobox", onCommitTimeDate, this);
    childSetCommitCallback("date_format_combobox", onCommitTimeDate, this);

    childSetCommitCallback("AscentInstantMessageResponseAnyone", onCommitAutoResponse, this);
    childSetCommitCallback("AscentInstantMessageResponseFriends", onCommitAutoResponse, this);
    childSetCommitCallback("AscentInstantMessageResponseMuted", onCommitAutoResponse, this);
    childSetCommitCallback("AscentInstantMessageShowOnTyping", onCommitAutoResponse, this);
    childSetCommitCallback("AscentInstantMessageShowResponded", onCommitAutoResponse, this);
    childSetCommitCallback("AscentInstantMessageResponseRepeat", onCommitAutoResponse, this);
    childSetCommitCallback("AscentInstantMessageResponseItem", onCommitAutoResponse, this);
    childSetCommitCallback("im_response", onCommitAutoResponse, this);

    childSetCommitCallback("KeywordsOn", onCommitKeywords, this);
    childSetCommitCallback("KeywordsList", onCommitKeywords, this);
    childSetCommitCallback("KeywordsSound", onCommitKeywords, this);
    childSetCommitCallback("KeywordsInChat", onCommitKeywords, this);
    childSetCommitCallback("KeywordsInIM", onCommitKeywords, this);
    childSetCommitCallback("KeywordsChangeColor", onCommitKeywords, this);
    childSetCommitCallback("KeywordsColor", onCommitKeywords, this);
    childSetCommitCallback("KeywordsPlaySound", onCommitKeywords, this);

    refreshValues();
    refresh();
}

LLPrefsAscentChat::~LLPrefsAscentChat()
{
}

//static
void LLPrefsAscentChat::onSpellAdd(void* data)
{
    LLPrefsAscentChat* self = (LLPrefsAscentChat*)data;

    if(self)
    {
        glggHunSpell->addButton(self->childGetValue("EmSpell_Avail").asString());
    }

    self->refresh();
}

//static
void LLPrefsAscentChat::onSpellRemove(void* data)
{
    LLPrefsAscentChat* self = (LLPrefsAscentChat*)data;

    if(self)
    {
        glggHunSpell->removeButton(self->childGetValue("EmSpell_Installed").asString());
    }

    self->refresh();
}

//static
void LLPrefsAscentChat::onSpellGetMore(void* data)
{
    glggHunSpell->getMoreButton(data);
}

//static
void LLPrefsAscentChat::onSpellEditCustom(void* data)
{
    glggHunSpell->editCustomButton();
}

//static
void LLPrefsAscentChat::onSpellBaseComboBoxCommit(LLUICtrl* ctrl, void* userdata)
{
    LLComboBox* box = (LLComboBox*)ctrl;

    if (box)
    {
        glggHunSpell->newDictSelection(box->getValue().asString());
    }
}

//static
void LLPrefsAscentChat::onCommitTimeDate(LLUICtrl* ctrl, void* userdata)
{
    LLPrefsAscentChat* self = (LLPrefsAscentChat*)userdata;

    LLComboBox* combo = (LLComboBox*)ctrl;
    if (ctrl->getName() == "time_format_combobox")
    {
        self->tempTimeFormat = combo->getCurrentIndex();
    }
    else if (ctrl->getName() == "date_format_combobox")
    {
        self->tempDateFormat = combo->getCurrentIndex();
    }

    std::string short_date, long_date, short_time, long_time, timestamp;

    if (self->tempTimeFormat == 0)
    {
        short_time = "%H:%M";
        long_time  = "%H:%M:%S";
        timestamp  = " %H:%M:%S";
    }
    else
    {
        short_time = "%I:%M %p";
        long_time  = "%I:%M:%S %p";
        timestamp  = " %I:%M %p";
    }

    if (self->tempDateFormat == 0)
    {
        short_date = "%Y-%m-%d";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
    else if (self->tempDateFormat == 1)
    {
        short_date = "%d/%m/%Y";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
    else
    {
        short_date = "%m/%d/%Y";
        long_date  = "%A, %B %d %Y";
        timestamp  = "%a %b %d %Y" + timestamp;
    }

    gSavedSettings.setString("ShortDateFormat", short_date);
    gSavedSettings.setString("LongDateFormat",  long_date);
    gSavedSettings.setString("ShortTimeFormat", short_time);
    gSavedSettings.setString("LongTimeFormat",  long_time);
    gSavedSettings.setString("TimestampFormat", timestamp);
}

//static
void LLPrefsAscentChat::onCommitAutoResponse(LLUICtrl* ctrl, void* user_data)
{
    LLPrefsAscentChat* self = (LLPrefsAscentChat*)user_data;	

    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseAnyone",  self->childGetValue("AscentInstantMessageResponseAnyone"));
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseFriends", self->childGetValue("AscentInstantMessageResponseFriends"));
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseMuted",   self->childGetValue("AscentInstantMessageResponseMuted"));
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageShowOnTyping",    self->childGetValue("AscentInstantMessageShowOnTyping"));
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageShowResponded",   self->childGetValue("AscentInstantMessageShowResponded"));
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseRepeat",  self->childGetValue("AscentInstantMessageResponseRepeat"));
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseItem",    self->childGetValue("AscentInstantMessageResponseItem"));
    gSavedPerAccountSettings.setString("AscentInstantMessageResponse",      self->childGetValue("im_response"));
}

//static
void LLPrefsAscentChat::onCommitKeywords(LLUICtrl* ctrl, void* user_data)
{
    LLPrefsAscentChat* self = (LLPrefsAscentChat*)user_data;

    if (ctrl->getName() == "KeywordsOn")
    {
        bool enabled = self->childGetValue("KeywordsOn").asBoolean();
        self->childSetEnabled("KeywordsList",        enabled);
        self->childSetEnabled("KeywordsInChat",      enabled);
        self->childSetEnabled("KeywordsInIM",        enabled);
        self->childSetEnabled("KeywordsChangeColor", enabled);
        self->childSetEnabled("KeywordsColor",       enabled);
        self->childSetEnabled("KeywordsPlaySound",   enabled);
        self->childSetEnabled("KeywordsSound",       enabled);
    }

    gSavedPerAccountSettings.setBOOL("KeywordsOn",          self->childGetValue("KeywordsOn"));
    gSavedPerAccountSettings.setString("KeywordsList",      self->childGetValue("KeywordsList"));
    gSavedPerAccountSettings.setBOOL("KeywordsInChat",      self->childGetValue("KeywordsInChat"));
    gSavedPerAccountSettings.setBOOL("KeywordsInIM",        self->childGetValue("KeywordsInIM"));
    gSavedPerAccountSettings.setBOOL("KeywordsChangeColor", self->childGetValue("KeywordsChangeColor"));
    gSavedPerAccountSettings.setColor4("KeywordsColor",     self->childGetValue("KeywordsColor"));
    gSavedPerAccountSettings.setBOOL("KeywordsPlaySound",   self->childGetValue("KeywordsPlaySound"));
    gSavedPerAccountSettings.setString("KeywordsSound",     self->childGetValue("KeywordsSound"));
}

// Store current settings for cancel
void LLPrefsAscentChat::refreshValues()
{
    //Chat/IM -----------------------------------------------------------------------------
    mWoLfVerticalIMTabs             = gSavedSettings.getBOOL("WoLfVerticalIMTabs");
    mIMAnnounceIncoming             = gSavedSettings.getBOOL("AscentInstantMessageAnnounceIncoming");
    mHideTypingNotification         = gSavedSettings.getBOOL("AscentHideTypingNotification");
    mShowGroupNameInChatIM          = gSavedSettings.getBOOL("OptionShowGroupNameInChatIM");
    mPlayTypingSound                = gSavedSettings.getBOOL("PlayTypingSound");
    mHideNotificationsInChat        = gSavedSettings.getBOOL("HideNotificationsInChat");
    mEnableMUPose                   = gSavedSettings.getBOOL("AscentAllowMUpose");
    mEnableOOCAutoClose             = gSavedSettings.getBOOL("AscentAutoCloseOOC");
    mLinksForChattingObjects        = gSavedSettings.getU32("LinksForChattingObjects");
    mSecondsInChatAndIMs            = gSavedSettings.getBOOL("SecondsInChatAndIMs");

    std::string format = gSavedSettings.getString("ShortTimeFormat");
    if (format.find("%p") == -1)
    {
        mTimeFormat = 0;
    }
    else
    {
        mTimeFormat = 1;
    }

    format = gSavedSettings.getString("ShortDateFormat");
    if (format.find("%m/%d/%") != -1)
    {
        mDateFormat = 2;
    }
    else if (format.find("%d/%m/%") != -1)
    {
        mDateFormat = 1;
    }
    else
    {
        mDateFormat = 0;
    }

    tempTimeFormat = mTimeFormat;
    tempDateFormat = mDateFormat;

    mIMResponseAnyone               = gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseAnyone");
    mIMResponseFriends              = gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseFriends");
    mIMResponseMuted                = gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseMuted");
    mIMShowOnTyping                 = gSavedPerAccountSettings.getBOOL("AscentInstantMessageShowOnTyping");
    mIMShowResponded                = gSavedPerAccountSettings.getBOOL("AscentInstantMessageShowResponded");
    mIMResponseRepeat               = gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseRepeat");
    mIMResponseItem                 = gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseItem");
    mIMResponseText                 = gSavedPerAccountSettings.getString("AscentInstantMessageResponse");

    //Spam --------------------------------------------------------------------------------
    mBlockChatSpam                  = gSavedSettings.getBOOL("SGBlockChatSpam");
    mChatSpamCount                  = gSavedSettings.getU32("SGChatSpamCount");
    mChatSpamTime                   = gSavedSettings.getF32("SGChatSpamTime");
    mBlockDialogSpam                = gSavedSettings.getBOOL("SGBlockDialogSpam");
    mBlockCardSpam                  = gSavedSettings.getBOOL("SGBlockCardSpam");
    mSpamCount                      = gSavedSettings.getU32("SGSpamCount");
    mSpamTime                       = gSavedSettings.getF32("SGSpamTime");

    //Text Options ------------------------------------------------------------------------
    mSpellDisplay                   = gSavedSettings.getBOOL("SpellDisplay");
    mKeywordsOn                     = gSavedPerAccountSettings.getBOOL("KeywordsOn");
    mKeywordsList                   = gSavedPerAccountSettings.getString("KeywordsList");
    mKeywordsInChat                 = gSavedPerAccountSettings.getBOOL("KeywordsInChat");
    mKeywordsInIM                   = gSavedPerAccountSettings.getBOOL("KeywordsInIM");
    mKeywordsChangeColor            = gSavedPerAccountSettings.getBOOL("KeywordsChangeColor");
    mKeywordsColor                  = gSavedPerAccountSettings.getColor4("KeywordsColor");
    mKeywordsPlaySound              = gSavedPerAccountSettings.getBOOL("KeywordsPlaySound");
    mKeywordsSound                  = static_cast<LLUUID>(gSavedPerAccountSettings.getString("KeywordsSound"));
}

// Update controls based on current settings
void LLPrefsAscentChat::refresh()
{
    //Chat --------------------------------------------------------------------------------
    // time format combobox
    LLComboBox* combo = getChild<LLComboBox>("time_format_combobox");
    if (combo)
    {
        combo->setCurrentByIndex(mTimeFormat);
    }

    // date format combobox
    combo = getChild<LLComboBox>("date_format_combobox");
    if (combo)
    {
        combo->setCurrentByIndex(mDateFormat);
    }

    childSetValue("AscentInstantMessageResponseAnyone",  mIMResponseAnyone);
    childSetValue("AscentInstantMessageResponseFriends", mIMResponseFriends);
    childSetValue("AscentInstantMessageResponseMuted",   mIMResponseMuted);
    childSetValue("AscentInstantMessageShowOnTyping",    mIMShowOnTyping);
    childSetValue("AscentInstantMessageShowResponded",   mIMShowResponded);
    childSetValue("AscentInstantMessageResponseRepeat",  mIMResponseRepeat);
    childSetValue("AscentInstantMessageResponseItem",    mIMResponseItem);

    LLWString auto_response = utf8str_to_wstring( gSavedPerAccountSettings.getString("AscentInstantMessageResponse") );
    LLWStringUtil::replaceChar(auto_response, '^', '\n');
    LLWStringUtil::replaceChar(auto_response, '%', ' ');
    childSetText("im_response", wstring_to_utf8str(auto_response));

    //Text Options ------------------------------------------------------------------------
    combo = getChild<LLComboBox>("SpellBase");

    if (combo != NULL) 
    {
        combo->removeall();
        std::vector<std::string> names = glggHunSpell->getDicts();

        for (int i = 0; i < (int)names.size(); i++) 
        {
            combo->add(names[i]);
        }

        combo->setSimple(gSavedSettings.getString("SpellBase"));
    }

    combo = getChild<LLComboBox>("EmSpell_Avail");

    if (combo != NULL) 
    {
        combo->removeall();

        combo->add("");
        std::vector<std::string> names = glggHunSpell->getAvailDicts();

        for (int i = 0; i < (int)names.size(); i++) 
        {
            combo->add(names[i]);
        }

        combo->setSimple(std::string(""));
    }

    combo = getChild<LLComboBox>("EmSpell_Installed");

    if (combo != NULL) 
    {
        combo->removeall();

        combo->add("");
        std::vector<std::string> names = glggHunSpell->getInstalledDicts();

        for (int i = 0; i < (int)names.size(); i++) 
        {
            combo->add(names[i]);
        }

        combo->setSimple(std::string(""));
    }

    childSetEnabled("KeywordsList",        mKeywordsOn);
    childSetEnabled("KeywordsInChat",      mKeywordsOn);
    childSetEnabled("KeywordsInIM",        mKeywordsOn);
    childSetEnabled("KeywordsChangeColor", mKeywordsOn);
    childSetEnabled("KeywordsColor",       mKeywordsOn);
    childSetEnabled("KeywordsPlaySound",   mKeywordsOn);
    childSetEnabled("KeywordsSound",       mKeywordsOn);

    childSetValue("KeywordsOn",          mKeywordsOn);
    childSetValue("KeywordsList",        mKeywordsList);
    childSetValue("KeywordsInChat",      mKeywordsInChat);
    childSetValue("KeywordsInIM",        mKeywordsInIM);
    childSetValue("KeywordsChangeColor", mKeywordsChangeColor);

    LLColorSwatchCtrl* colorctrl = getChild<LLColorSwatchCtrl>("KeywordsColor");
    colorctrl->set(LLColor4(mKeywordsColor),TRUE);

    childSetValue("KeywordsPlaySound",   mKeywordsPlaySound);
    childSetValue("KeywordsSound",       mKeywordsSound);
}

// Reset settings to local copy
void LLPrefsAscentChat::cancel()
{
    //Chat/IM -----------------------------------------------------------------------------
    gSavedSettings.setBOOL("WoLfVerticalIMTabs",                   mWoLfVerticalIMTabs);
    gSavedSettings.setBOOL("AscentInstantMessageAnnounceIncoming", mIMAnnounceIncoming);
    gSavedSettings.setBOOL("AscentHideTypingNotification",         mHideTypingNotification);
    gSavedSettings.setBOOL("OptionShowGroupNameInChatIM",          mShowGroupNameInChatIM);
    gSavedSettings.setBOOL("PlayTypingSound",                      mPlayTypingSound);
    gSavedSettings.setBOOL("HideNotificationsInChat",              mHideNotificationsInChat);
    gSavedSettings.setBOOL("AscentAllowMUpose",                    mEnableMUPose);
    gSavedSettings.setBOOL("AscentAutoCloseOOC",                   mEnableOOCAutoClose);
    gSavedSettings.setU32("LinksForChattingObjects",               mLinksForChattingObjects);
    gSavedSettings.setBOOL("SecondsInChatAndIMs",                  mSecondsInChatAndIMs);

    std::string short_date, long_date, short_time, long_time, timestamp;

    if (mTimeFormat == 0)
    {
        short_time = "%H:%M";
        long_time  = "%H:%M:%S";
        timestamp  = " %H:%M:%S";
    }
    else
    {
        short_time = "%I:%M %p";
        long_time  = "%I:%M:%S %p";
        timestamp  = " %I:%M %p";
    }

    if (mDateFormat == 0)
    {
        short_date = "%Y-%m-%d";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
    else if (mDateFormat == 1)
    {
        short_date = "%d/%m/%Y";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
    else
    {
        short_date = "%m/%d/%Y";
        long_date  = "%A, %B %d %Y";
        timestamp  = "%a %b %d %Y" + timestamp;
    }

    gSavedSettings.setString("ShortDateFormat", short_date);
    gSavedSettings.setString("LongDateFormat",  long_date);
    gSavedSettings.setString("ShortTimeFormat", short_time);
    gSavedSettings.setString("LongTimeFormat",  long_time);
    gSavedSettings.setString("TimestampFormat", timestamp);

    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseAnyone",  mIMResponseAnyone);
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseFriends", mIMResponseFriends);
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseMuted",   mIMResponseMuted);
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageShowOnTyping",    mIMShowOnTyping);
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageShowResponded",   mIMShowResponded);
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseRepeat",  mIMResponseRepeat);
    gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseItem",    mIMResponseItem);
    gSavedPerAccountSettings.setString("AscentInstantMessageResponse",      mIMResponseText);

    //Spam --------------------------------------------------------------------------------
    gSavedSettings.setBOOL("SGBlockChatSpam",   mBlockChatSpam);
    gSavedSettings.setU32("SGChatSpamCount",    mChatSpamCount);
    gSavedSettings.setF32("SGChatSpamTime",     mChatSpamTime);
    gSavedSettings.setBOOL("SGBlockDialogSpam", mBlockDialogSpam);
    gSavedSettings.setBOOL("SGBlockCardSpam",   mBlockCardSpam);
    gSavedSettings.setU32("SGSpamCount",        mSpamCount);
    gSavedSettings.setF32("SGSpamTime",         mSpamTime);

    //Text Options ------------------------------------------------------------------------
    gSavedSettings.setBOOL("SpellDisplay",                  mSpellDisplay);
    gSavedPerAccountSettings.setBOOL("KeywordsOn",          mKeywordsOn);
    gSavedPerAccountSettings.setString("KeywordsList",      mKeywordsList);
    gSavedPerAccountSettings.setBOOL("KeywordsInChat",      mKeywordsInChat);
    gSavedPerAccountSettings.setBOOL("KeywordsInIM",        mKeywordsInIM);
    gSavedPerAccountSettings.setBOOL("KeywordsChangeColor", mKeywordsChangeColor);
    gSavedPerAccountSettings.setColor4("KeywordsColor",     mKeywordsColor);
    gSavedPerAccountSettings.setBOOL("KeywordsPlaySound",   mKeywordsPlaySound);
    gSavedPerAccountSettings.setString("KeywordsSound",     mKeywordsSound.asString());
}

// Update local copy so cancel has no effect
void LLPrefsAscentChat::apply()
{
    gSavedPerAccountSettings.setString("AscentInstantMessageResponse", childGetValue("im_response"));

    refreshValues();
    refresh();
}
