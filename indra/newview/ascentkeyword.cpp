/** 
 * @file ascentprefssys.cpp
 * @Ascent Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 * 
 * Copyright (c) 2011, Tigh MacFanatic.
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

#include "ascentkeyword.h"
#include "llviewercontrol.h"
#include "llui.h"
#include <boost/regex.hpp>


BOOL AscentKeyword::hasKeyword(std::string msg,int source)
{
	static const LLCachedControl<bool> mKeywordsOn(gSavedPerAccountSettings,"KeywordsOn", false);
	static const LLCachedControl<bool> mKeywordsInChat(gSavedPerAccountSettings,"KeywordsInChat", false);
	static const LLCachedControl<bool> mKeywordsInIM(gSavedPerAccountSettings, "KeywordsInIM", false);

	if (mKeywordsOn)
    {
	    if ((source == 1) && (mKeywordsInChat))
        {
		    return containsKeyWord(msg);
        }

	    if ((source == 2) && (mKeywordsInIM))
        {
		    return containsKeyWord(msg);
        }
    }

    return FALSE;
}

bool AscentKeyword::containsKeyWord(std::string source)
{
	static const LLCachedControl<std::string> mKeywordsList(gSavedPerAccountSettings, "KeywordsList", "");
	static const LLCachedControl<bool> mKeywordsPlaySound(gSavedPerAccountSettings, "KeywordsPlaySound", false);
	static const LLCachedControl<std::string> mKeywordsSound(gSavedPerAccountSettings, "KeywordsSound", "");

    std::string s = mKeywordsList;
	LLStringUtil::toLower(s);
	LLStringUtil::toLower(source);
	boost::regex re(",");
	boost::sregex_token_iterator i(s.begin(), s.end(), re, -1);
	boost::sregex_token_iterator j;

	while(i != j)
	{
		if(source.find( *i++) != std::string::npos)
		{
			if (mKeywordsPlaySound)
            {
				LLUI::sAudioCallback(LLUUID(mKeywordsSound));
            }

			return true;
		}
	}

    return false;
}
