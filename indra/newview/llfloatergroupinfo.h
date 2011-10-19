/** 
 * @file llfloatergroupinfo.h
 * @brief LLFloaterGroupInfo class definition
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

/**
 * Floater used both for display of group information and for
 * creating new groups.
 */

#ifndef LL_LLFLOATERGROUPINFO_H
#define LL_LLFLOATERGROUPINFO_H

#include "llfloater.h"
#include "lluuid.h"

class LLPanelGroup;
struct LLOfferInfo;

class LLFloaterGroupInfo
: public LLFloater
{
public:
	virtual ~LLFloaterGroupInfo();

	static void showCreateGroup(void *);
	static void showMyGroupInfo(void *);
	static void showFromUUID(const LLUUID &group_id,
							 const std::string& tab_name = std::string());
	static void closeCreateGroup();
	static void closeGroup(const LLUUID& group_id);
	static void refreshGroup(const LLUUID& group_id);

	static void showNotice(const std::string& subject,
						   const std::string& message,
						   const LLUUID& group_id,
						   const bool& has_inventory,
						   const std::string& inventory_name,
						   LLOfferInfo* inventory_offer);

	LLUUID		getGroupID() { return mGroupID;}

	void selectTabByName(std::string tab_name);

	// This allow us to block the user from closing the floater
	// if there is information that needs to be applied.
	virtual BOOL canClose();
protected:
	LLFloaterGroupInfo(const std::string& name, const LLRect &rect, const std::string& title, const LLUUID& group_id = LLUUID::null, const std::string& tab_name = std::string());

private:
	static void callbackLoadGroupName(const LLUUID& id, const std::string& full_name, bool is_group);
	static std::map<LLUUID, LLFloaterGroupInfo*> sInstances;

	LLUUID			mGroupID;
	LLPanelGroup*	mPanelGroupp;
};

#endif
