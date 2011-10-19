/** 
 * @file LLPanelWeb.cpp
 * @brief Network preferences panel
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

// file include
#include "llpanelweb.h"

// project includes
#include "llcheckboxctrl.h"
#include "llnotificationsutil.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerwindow.h"
#include "llpluginclassmedia.h"

// helper functions for getting/freeing the web browser media
// if creating/destroying these is too slow, we'll need to create
// a static member and update all our static callbacks
viewer_media_t get_web_media()
{

	viewer_media_t media_source = LLViewerMedia::newMediaImpl("", LLUUID::null, 0, 0, 0, 0, "text/html");

	return media_source;
}

LLPanelWeb::LLPanelWeb()
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_web.xml");
}

BOOL LLPanelWeb::postBuild()
{
	childSetAction("clear_cache", onClickClearCache, this);
	childSetCommitCallback("web_proxy_enabled", onCommitWebProxyEnabled, this);

	std::string value = gSavedSettings.getBOOL("UseExternalBrowser") ? "external" : "internal";
	childSetValue("use_external_browser", value);

	childSetValue("cookies_enabled", gSavedSettings.getBOOL("CookiesEnabled"));

	childSetValue("web_proxy_enabled", gSavedSettings.getBOOL("BrowserProxyEnabled"));
	childSetValue("web_proxy_editor", gSavedSettings.getString("BrowserProxyAddress"));
	childSetValue("web_proxy_port", gSavedSettings.getS32("BrowserProxyPort"));

	childSetEnabled("proxy_text_label", gSavedSettings.getBOOL("BrowserProxyEnabled"));
	childSetEnabled("web_proxy_editor", gSavedSettings.getBOOL("BrowserProxyEnabled"));
	childSetEnabled("web_proxy_port", gSavedSettings.getBOOL("BrowserProxyEnabled"));

	return TRUE;
}



LLPanelWeb::~LLPanelWeb()
{
	// Children all cleaned up by default view destructor.
}

void LLPanelWeb::apply()
{
	gSavedSettings.setBOOL("CookiesEnabled", childGetValue("cookies_enabled"));
	gSavedSettings.setBOOL("BrowserProxyEnabled", childGetValue("web_proxy_enabled"));
	gSavedSettings.setString("BrowserProxyAddress", childGetValue("web_proxy_editor"));
	gSavedSettings.setS32("BrowserProxyPort", childGetValue("web_proxy_port"));

	bool value = childGetValue("use_external_browser").asString() == "external" ? true : false;
	gSavedSettings.setBOOL("UseExternalBrowser", value);
	
	viewer_media_t media_source = get_web_media();
	if (media_source && media_source->hasMedia())
	{
		media_source->getMediaPlugin()->enable_cookies(childGetValue("cookies_enabled"));

		bool proxy_enable = childGetValue("web_proxy_enabled");
		std::string proxy_address = childGetValue("web_proxy_editor");
		int proxy_port = childGetValue("web_proxy_port");
		media_source->getMediaPlugin()->proxy_setup(proxy_enable, proxy_address, proxy_port);
	}
}

void LLPanelWeb::cancel()
{
}

// static
void LLPanelWeb::onClickClearCache(void*)
{
	LLNotificationsUtil::add("ConfirmClearBrowserCache", LLSD(), LLSD(), callback_clear_browser_cache);
}

//static
bool LLPanelWeb::callback_clear_browser_cache(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if ( option == 0 ) // YES
	{
		viewer_media_t media_source = get_web_media();
		if (media_source && media_source->hasMedia())
			media_source->getMediaPlugin()->clear_cache();
	}
	return false;
}

// static
void LLPanelWeb::onClickClearCookies(void*)
{
	LLNotificationsUtil::add("ConfirmClearCookies", LLSD(), LLSD(), callback_clear_cookies);
}

//static
bool LLPanelWeb::callback_clear_cookies(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if ( option == 0 ) // YES
	{
		viewer_media_t media_source = get_web_media();
		if (media_source && media_source->hasMedia())
			media_source->getMediaPlugin()->clear_cookies();
	}
	return false;
}

// static
void LLPanelWeb::onCommitCookies(LLUICtrl* ctrl, void* data)
{
  LLPanelWeb* self = (LLPanelWeb*)data;
  LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;

  if (!self || !check) return;

  viewer_media_t media_source = get_web_media();
		if (media_source && media_source->hasMedia())
	  media_source->getMediaPlugin()->enable_cookies(check->get());
}
// static
void LLPanelWeb::onCommitWebProxyEnabled(LLUICtrl* ctrl, void* data)
{
	LLPanelWeb* self = (LLPanelWeb*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;

	if (!self || !check) return;
	self->childSetEnabled("web_proxy_editor", check->get());
	self->childSetEnabled("web_proxy_port", check->get());
	self->childSetEnabled("proxy_text_label", check->get());


}
