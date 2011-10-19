/** 
 * @file llweb.cpp
 * @brief Functions dealing with web browsers
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llweb.h"

#include "llviewerwindow.h"
#include "llwindow.h"

#include "llviewercontrol.h"
#include "llfloatermediabrowser.h"

// static
void LLWeb::initClass()
{
	LLAlertDialog::setURLLoader(&sAlertURLLoader);
}

// static
void LLWeb::loadURL(const std::string& url)
{
	if (gSavedSettings.getBOOL("UseExternalBrowser"))
	{
		loadURLExternal(url);
	}
	else
	{
		LLFloaterMediaBrowser::showInstance(url);
	}
}


// static
void LLWeb::loadURLExternal(const std::string& url)
{
	std::string escaped_url = escapeURL(url);
	gViewerWindow->getWindow()->spawnWebBrowser(escaped_url,true);
}


// static
std::string LLWeb::curlEscape(const std::string& url)
{
	static const char hex[] = "0123456789ABCDEF";
	std::stringstream escaped_url;
	for (std::string::const_iterator iter = url.begin(); iter != url.end(); ++iter)
	{
		switch(*iter) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			case 'a': case 'b': case 'c': case 'd': case 'e':
			case 'f': case 'g': case 'h': case 'i': case 'j':
			case 'k': case 'l': case 'm': case 'n': case 'o':
			case 'p': case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D': case 'E':
			case 'F': case 'G': case 'H': case 'I': case 'J':
			case 'K': case 'L': case 'M': case 'N': case 'O':
			case 'P': case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
				escaped_url << (*iter);
				break;
			default:
				unsigned char c = (unsigned char)(*iter);
				escaped_url << '%' << hex[c>>4] << hex[c&0xF];
		}
	}
	return escaped_url.str();
}

// static
std::string LLWeb::escapeURL(const std::string& url)
{
	// The CURL curl_escape() function escapes colons, slashes,
	// and all characters but A-Z and 0-9.  Do a cheesy mini-escape.
	std::string escaped_url;
	S32 len = url.length();
	for (S32 i = 0; i < len; i++)
	{
		char c = url[i];
		if (c == ' ')
		{
			escaped_url += "%20";
		}
		else if (c == '\\')
		{
			escaped_url += "%5C";
		}
		else
		{
			escaped_url += c;
		}
	}
	return escaped_url;
}

// virtual
void LLWeb::URLLoader::load(const std::string& url)
{
	loadURL(url);
}

// static
LLWeb::URLLoader LLWeb::sAlertURLLoader;
