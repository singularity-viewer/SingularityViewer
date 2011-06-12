/**
* @file lltranslate.cpp
* @brief Functions for translating text via Google Translate.
*
* $LicenseInfo:firstyear=2009&license=viewergpl$
*
* Copyright (c) 2009, Linden Research, Inc.
*
* Second Life Viewer Source Code
* The source code in this file ("Source Code") is provided by Linden Lab
* to you under the terms of the GNU General Public License, version 2.0
* ("GPL"), unless you have obtained a separate licensing agreement
* ("Other License"), formally executed by you and Linden Lab. Terms of
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

#include "llbufferstream.h"
#include "lltranslate.h"
#include "llui.h"
#include "llversionviewer.h"
#include "llweb.h"

// <edit>
#include "llviewercontrol.h"
// </edit>

// These two are concatenated with the language specifiers to form a complete Google Translate URL
const char* LLTranslate::m_GoogleURL = "http://ajax.googleapis.com/ajax/services/language/translate?v=1.0&q=";
const char* LLTranslate::m_GoogleLangSpec = "&langpair=";
float LLTranslate::m_GoogleTimeout = 10;

LLSD LLTranslate::m_Header;
// These constants are for the GET header.
const char* LLTranslate::m_AcceptHeader = "Accept";
const char* LLTranslate::m_AcceptType = "text/plain";
const char* LLTranslate::m_AgentHeader = "User-Agent";

// These constants are in the JSON returned from Google
const char* LLTranslate::m_GoogleData = "responseData";
const char* LLTranslate::m_GoogleTranslation = "translatedText";
const char* LLTranslate::m_GoogleLanguage = "detectedSourceLanguage";

//static
void LLTranslate::translateMessage(LLHTTPClient::ResponderPtr &result, const std::string &fromLang, const std::string &toLang, const std::string &mesg)
{
	std::string url;
	getTranslateUrl(url, fromLang, toLang, mesg);

//<edit>
    std::string user_agent = gCurrentVersion;
//</edit>

	if (!m_Header.size())
	{
		m_Header.insert(m_AcceptHeader, LLSD(m_AcceptType));
		m_Header.insert(m_AgentHeader, LLSD(user_agent));
	}

	LLHTTPClient::get(url, result, m_Header, m_GoogleTimeout);
}

//static
void LLTranslate::getTranslateUrl(std::string &translateUrl, const std::string &fromLang, const std::string &toLang, const std::string &mesg)
{
	std::string escaped_mesg = LLWeb::curlEscape(mesg);

	translateUrl = m_GoogleURL
		+ escaped_mesg + m_GoogleLangSpec
		+ fromLang // 'from' language; empty string for auto
		+ "%7C" // |
		+ toLang; // 'to' language
}

//static
void LLTranslate::stringReplaceAll(std::string& context, const std::string& from, const std::string& to)
{
	size_t lookHere = 0;
	size_t foundHere;

	while((foundHere = context.find(from, lookHere))
		!= std::string::npos) {
			context.replace(foundHere, from.size(), to);
			lookHere = foundHere + to.size();
	}
}

//static
BOOL LLTranslate::parseGoogleTranslate(const std::string result, std::string &translation, std::string &detectedLanguage)
{
	Json::Value root;
	Json::Reader reader;
	BOOL parsingSuccessful = reader.parse(result, root );
	if ( !parsingSuccessful )
	{
		LL_WARNS("JSON") << reader.getFormatedErrorMessages() << LL_ENDL;
		return FALSE;
	}

	translation = root[m_GoogleData].get(m_GoogleTranslation, "").asString();
	detectedLanguage = root[m_GoogleData].get(m_GoogleLanguage, "").asString();
	return TRUE;
}

//static
std::string LLTranslate::getTranslateLanguage()
{
	std::string language = "en";
	if (LLUI::sConfigGroup)
	{
		language = LLUI::sConfigGroup->getString("TranslateLanguage");
		if (language.empty() || language == "default")
		{
		  language = LLUI::getLanguage();
		}
	}
	language = language.substr(0,2);
	return language;
}

