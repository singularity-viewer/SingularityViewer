/** 
 * @file llpaneldirfind.h
 * @brief The "Find All" panel in the Find directory.
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

#ifndef LL_LLPANELDIRFIND_H
#define LL_LLPANELDIRFIND_H

#include "llpaneldirbrowser.h"
#include "llmediactrl.h"

class LLUICtrl;
class LLLineEditor;
class LLPanelDirFindAll;
class LLFloaterDirectory;

// This class in an abstract base class for all new style search widgets.  It contains a pointer to a web browser control
// 
class LLPanelDirFind
:	public LLPanelDirBrowser,
	public LLViewerMediaObserver
{
public:
	LLPanelDirFind(const std::string& name, LLFloaterDirectory* floater, const std::string& browser_name);
	/*virtual*/ ~LLPanelDirFind();

	/*virtual*/ void draw();
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void handleVisibilityChange(BOOL new_visibility);

	// Pure virtual.  Must be implemented
	virtual void search(const std::string& search_text) = 0;

	virtual void navigateToDefaultPage();
	void focus();

	static std::string buildSearchURL(const std::string& search_text, const std::string& collection, bool inc_pg, bool inc_mature, bool inc_adult, bool is_web);
	static std::string getSearchURLSuffix(bool inc_pg, bool inc_mature, bool inc_adult, bool is_web);

private:
	static void onClickBack( void* data );
	static void onClickForward( void* data );
	static void onClickHome( void* data );
	static void onClickRefresh( void* data );
	static void onClickSearch( void* data );
	static void onCommitSearch(LLUICtrl*, void* data);
	static void onClickHelp( void* data );

	// inherited from LLViewerMediaObserver
	/*virtual*/ void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event);

protected:
	LLMediaCtrl* mWebBrowser;
	std::string mBrowserName;
};

class LLPanelDirFindAllInterface
{
public:
	static LLPanelDirFindAll* create(LLFloaterDirectory* floater);
	static void search(LLPanelDirFindAll* panel, const std::string& search_text);
	static void focus(LLPanelDirFindAll* panel);
};


class LLPanelDirFindAllOld : public LLPanelDirBrowser
{
public:
	LLPanelDirFindAllOld(const std::string& name, LLFloaterDirectory* floater);
	/*virtual*/ ~LLPanelDirFindAllOld();

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void draw();

	static void onClickSearch(void *userdata);
	static void onCommitScope(LLUICtrl* ctrl, void* data);
	static void onKeystrokeName(LLLineEditor* line, void* data);
};

#endif
