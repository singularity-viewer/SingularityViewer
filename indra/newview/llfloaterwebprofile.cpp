/**
 * @file llfloaterwebprofile.cpp
 * @brief Avatar profile floater.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llfloaterwebprofile.h"

#include "lluictrlfactory.h"
#include "llviewercontrol.h"

LLFloaterWebProfile::LLFloaterWebProfile(const Params& key) :
	LLFloaterWebContent(key)
{
}

void LLFloaterWebProfile::onOpen()
{
	Params p(mKey);
	p.show_chrome(false).
		window_class("profile");
	mKey = p;
	LLFloaterWebContent::onOpen();
	applyPreferredRect();
}

// virtual
void LLFloaterWebProfile::handleReshape(const LLRect& new_rect, bool by_user)
{
	lldebugs << "handleReshape: " << new_rect << llendl;

	if (by_user && !isMinimized())
	{
		lldebugs << "Storing new rect" << llendl;
		gSavedSettings.setRect("WebProfileFloaterRect", new_rect);
	}

	LLFloaterWebContent::handleReshape(new_rect, by_user);
}

// Singu Note: this was copied from LLFloaterWebContent::showInstance
//static
void LLFloaterWebProfile::showInstance(const std::string& window_class, Params& p)
{
	p.window_class(window_class);

	LLSD key = p;

	instance_iter it = beginInstances();
	for(;it!=endInstances();++it)
	{
		if(it->mKey["window_class"].asString() == window_class)
		{
			if(it->matchesKey(key))
			{
				it->mKey = key;
				it->setKey(p.id());
				it->mAgeTimer.reset();
				it->open();
				return;
			}
		}
	}
	LLFloaterWebContent* old_inst = getInstance(p.id());
	if(old_inst)
	{
		llwarns << "Replacing unexpected duplicate floater: " << p.id() << llendl;
		old_inst->mKey = key;
		old_inst->mAgeTimer.reset();
		old_inst->open();
	}
	assert(!old_inst);

	if(!old_inst)
		LLUICtrlFactory::getInstance()->buildFloater(LLFloaterWebProfile::create(p), "floater_web_content.xml");
}

LLFloater* LLFloaterWebProfile::create(const LLSD& key)
{
	LLFloaterWebContent::Params p(key);
	preCreate(p);
	return new LLFloaterWebProfile(p);
}

void LLFloaterWebProfile::applyPreferredRect()
{
	const LLRect preferred_rect = gSavedSettings.getRect("WebProfileFloaterRect");
	lldebugs << "Applying preferred rect: " << preferred_rect << llendl;

	// Don't override position that may have been set by floater stacking code.
	// Singu Note: We do floater stacking here, actually
	int left, top;
	gFloaterView->getNewFloaterPosition(&left, &top);
	LLRect new_rect = getRect();
	new_rect.setLeftTopAndSize(
		left, top, //new_rect.mLeft, new_rect.mTop,
		preferred_rect.getWidth(), preferred_rect.getHeight());
	setShape(new_rect);
}
