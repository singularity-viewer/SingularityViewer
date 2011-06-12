/** 
 * @file llpanelnetwork.h
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

#ifndef LL_LLPANELNETWORK_H
#define LL_LLPANELNETWORK_H

#include "llpanel.h"

class AIDirPicker;

class LLPanelNetwork : public LLPanel
{
public:
	LLPanelNetwork();
	~LLPanelNetwork();

	BOOL postBuild();

	void apply();
	void cancel();

private:
	static void onClickClearCache(void*);
	static void onClickSetCache(void*);
	static void onClickSetCache_continued(void* user_data, AIDirPicker* dirpicker);
	static void onClickResetCache(void*);
	static void onCommitPort(LLUICtrl* ctrl, void*);
	static void onCommitSocks5ProxyEnabled(LLUICtrl* ctrl, void* data);
	static void onClickTestProxy(void* user_data);
	static void onSocksSettingsModified(LLUICtrl* ctrl, void* data);
	static void onSocksAuthChanged(LLUICtrl* ctrl, void* data);
	static void updateProxyEnabled(LLPanelNetwork * self, bool enabled, std::string authtype);
	
	static bool sSocksSettingsChanged;

};

#endif
