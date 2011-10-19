/** 
 * @file llfloaterdisplayname.cpp
 * @author Leyla Farazha
 * @brief Implementation of the LLFloaterDisplayName class.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h"
#include "llfloater.h"

#include "llnotificationsutil.h"
#include "llviewerdisplayname.h"

#include "llnotify.h"
#include "llfloaterdisplayname.h"
#include "llavatarnamecache.h"
#include "lluictrlfactory.h"

#include "llagent.h"


LLFloaterDisplayName* LLFloaterDisplayName::sInstance = NULL;

LLFloaterDisplayName::LLFloaterDisplayName()
: LLFloater(std::string("Display Names Floater"))
{
	LLFloaterDisplayName::sInstance = this;
}

// virtual
LLFloaterDisplayName::~LLFloaterDisplayName()
{

	LLFloaterDisplayName::sInstance = NULL;

}

BOOL LLFloaterDisplayName::postBuild()
{
	childSetAction("reset_btn", onReset, this);
	childSetAction("cancel_btn", onCancel, this);
	childSetAction("save_btn", onSave, this);
	
	center();

	return TRUE;
}

void LLFloaterDisplayName::show()
{
	if (LLFloaterDisplayName::sInstance)
	{
		LLFloaterDisplayName::sInstance->open();		/*Flawfinder: ignore*/
		return;
	}


	LLFloaterDisplayName *self = new LLFloaterDisplayName();

	// Builds and adds to gFloaterView
	LLUICtrlFactory::getInstance()->buildFloater(self, "floater_display_name.xml");

	// Fix up rectangle

	self->open();	/*Flawfinder: ignore*/
}


void LLFloaterDisplayName::onOpen()
{
	getChild<LLUICtrl>("display_name_editor")->clear();
	getChild<LLUICtrl>("display_name_confirm")->clear();

	LLAvatarName av_name;
	LLAvatarNameCache::get(gAgent.getID(), &av_name);

	F64 now_secs = LLDate::now().secondsSinceEpoch();

	if (now_secs < av_name.mNextUpdate)
	{
		// ...can't update until some time in the future
		F64 next_update_local_secs =
			av_name.mNextUpdate ;
		LLDate next_update_local(next_update_local_secs);
		// display as "July 18 12:17 PM"
//		std::string next_update_string =
//		next_update_local.toHTTPDateString("%B %d %I:%M %p");
		std::string year = next_update_local.asString().substr(0,4);
		std::string month = next_update_local.asString().substr(5,2);
		std::string day = next_update_local.asString().substr(8,2);
		std::string hour = next_update_local.asString().substr(11,2);
		std::string minute = next_update_local.asString().substr(14,2);
		std::string second = next_update_local.asString().substr(17,2);
		std::string next_update_string_date = 
			llformat("%s/%s/%s",year.c_str(),month.c_str(),
			day.c_str());
		std::string next_update_string_time = 
			llformat("%s:%s:%s",hour.c_str(),minute.c_str(),
			second.c_str());
		getChild<LLUICtrl>("lockout_text")->setTextArg("[DATE]",
			next_update_string_date);
		getChild<LLUICtrl>("lockout_text")->setTextArg("[TIME]",
			next_update_string_time);
		getChild<LLUICtrl>("lockout_text")->setVisible(true);
		getChild<LLUICtrl>("no_lockout_text")->setVisible(false);
		getChild<LLUICtrl>("save_btn")->setEnabled(false);
		getChild<LLUICtrl>("display_name_editor")->setEnabled(false);
		getChild<LLUICtrl>("display_name_confirm")->setEnabled(false);
		getChild<LLUICtrl>("cancel_btn")->setFocus(TRUE);
		
	}
	else
	{
		getChild<LLUICtrl>("lockout_text")->setVisible(false);
		getChild<LLUICtrl>("no_lockout_text")->setVisible(true);
		getChild<LLUICtrl>("save_btn")->setEnabled(true);
		getChild<LLUICtrl>("display_name_editor")->setEnabled(true);
		getChild<LLUICtrl>("display_name_confirm")->setEnabled(true);

	}
}


void LLFloaterDisplayName::onCacheSetName(bool success,
										  const std::string& reason,
										  const LLSD& content)
{
	if (success)
	{
		// Inform the user that the change took place, but will take a while
		// to percolate.
		LLSD args;
		args["DISPLAY_NAME"] = content["display_name"];
		LLNotificationsUtil::add("SetDisplayNameSuccess", args);
		
		// Re-fetch my name, as it may have been sanitized by the service
		//LLAvatarNameCache::get(getAvatarId(),
		//	boost::bind(&LLPanelMyProfileEdit::onNameCache, this, _1, _2));
		return;
	}

	// Request failed, notify the user
	std::string error_tag = content["error_tag"].asString();
	llinfos << "set name failure error_tag " << error_tag << llendl;

	// We might have a localized string for this message
	// error_args will usually be empty from the server.
	if (!error_tag.empty()
		&& LLNotifications::getInstance()->templateExists(error_tag))
	{
		LLNotifications::instance().add(error_tag);
		return;
	}

	// The server error might have a localized message for us
	std::string lang_code = LLUI::getLanguage();
	LLSD error_desc = content["error_description"];
	if (error_desc.has( lang_code ))
	{
		LLSD args;
		args["MESSAGE"] = error_desc[lang_code].asString();
		LLNotificationsUtil::add("GenericAlert", args);
		return;
	}

	// No specific error, throw a generic one
	LLNotificationsUtil::add("SetDisplayNameFailedGeneric");
}

void LLFloaterDisplayName::onCancel(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;
	self->setVisible(false);
}

void LLFloaterDisplayName::onReset(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;
	if (LLAvatarNameCache::useDisplayNames())
	{
		LLViewerDisplayName::set("",
			boost::bind(&LLFloaterDisplayName::onCacheSetName, self, _1, _2, _3));
	}	
	else
	{
		LLNotificationsUtil::add("SetDisplayNameFailedGeneric");
	}
	
	self->setVisible(false);
}


void LLFloaterDisplayName::onSave(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;
	std::string display_name_utf8 = self->getChild<LLUICtrl>("display_name_editor")->getValue().asString();
	std::string display_name_confirm = self->getChild<LLUICtrl>("display_name_confirm")->getValue().asString();

	if (display_name_utf8.compare(display_name_confirm))
	{
		LLNotificationsUtil::add("SetDisplayNameMismatch");
		return;
	}

	const U32 DISPLAY_NAME_MAX_LENGTH = 31; // characters, not bytes
	LLWString display_name_wstr = utf8string_to_wstring(display_name_utf8);
	if (display_name_wstr.size() > DISPLAY_NAME_MAX_LENGTH)
	{
		LLSD args;
		args["LENGTH"] = llformat("%d", DISPLAY_NAME_MAX_LENGTH);
		LLNotificationsUtil::add("SetDisplayNameFailedLength", args);
		return;
	}
	
	if (LLAvatarNameCache::useDisplayNames())
	{
		LLViewerDisplayName::set(display_name_utf8,
			boost::bind(&LLFloaterDisplayName::onCacheSetName, self, _1, _2, _3));	
	}
	else
	{
		LLNotificationsUtil::add("SetDisplayNameFailedGeneric");
	}

	self->setVisible(false);
}


