/** 
 * @file llnameeditor.h
 * @brief display and refresh a name from the name cache
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

#ifndef LL_LLNAMEEDITOR_H
#define LL_LLNAMEEDITOR_H

#include "lllineeditor.h"
#include "llnameui.h"

class LLNameEditor
:	public LLLineEditor
,	public LLNameUI
{
	bool mClickForProfile;
public:
	LLNameEditor(const std::string& name, const LLRect& rect,
		const LLUUID& name_id = LLUUID::null,
		bool is_group = false,
		const std::string& loading = LLStringUtil::null,
		bool rlv_sensitive = false,
		bool click_for_profile = true,
		const LLFontGL* glfont = nullptr,
		S32 max_text_length = 254);

	BOOL handleMouseDown(S32 x, S32 y, MASK mask) override final;
	BOOL handleRightMouseDown(S32 x, S32 y, MASK mask) override final;
	BOOL handleHover(S32 x, S32 y, MASK mask) override final;

	LLXMLNodePtr getXML(bool save_children = true) const override final;
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	void setValue(const LLSD& value) override final { LLNameUI::setValue(value); }
	LLSD getValue() const override final { return LLNameUI::getValue(); }

	void displayAsLink(bool link) override final;
	void setText(const std::string& text) override final;
};

#endif
