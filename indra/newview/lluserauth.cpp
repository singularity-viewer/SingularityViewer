/** 
 * @file lluserauth.cpp
 * @brief LLUserAuth class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lluserauth.h"

#include <sstream>
#include <iterator>

#include "lldir.h"
#include "llversionviewer.h"
#include "llappviewer.h"
#include "llviewerbuild.h"
#include "llviewercontrol.h"
#include "llxmlrpctransaction.h"
#include "llsdutil.h"

// NOTE: MUST include these after otherincludes since queue gets redefined!?!!
#include <curl/curl.h>
#include <xmlrpc-epi/xmlrpc.h>


// Don't define PLATFORM_STRING for unknown platforms - they need
// to get added to the login cgi script, so we want this to cause an
// error if we get compiled for a different platform.
// *FIX: This is misreporting on linux. Change this so that linux is
// in fact reporting linux.
#if LL_WINDOWS || LL_LINUX  
static const char* PLATFORM_STRING = "Win";
#elif LL_DARWIN
static const char* PLATFORM_STRING = "Mac";
#elif LL_LINUX
static const char* PLATFORM_STRING = "Lnx";
#elif LL_SOLARIS
static const char* PLATFORM_STRING = "Sol";
#else
#error("Unknown platform defined!")
#endif


LLUserAuth::LLUserAuth() :
	mTransaction(NULL),
	mLastTransferRateBPS(0),
	mResult(LLSD())
{
	mAuthResponse = E_NO_RESPONSE_YET;
}

LLUserAuth::~LLUserAuth()
{
	reset();
}

void LLUserAuth::reset()
{
	delete mTransaction;
	mTransaction = NULL;
	mResponses.clear();
	mOptions.clear();
	mResult.clear();
}


void LLUserAuth::authenticate(
	const std::string& auth_uri,
	const std::string& method,
	const std::string& firstname,
	const std::string& lastname,
	LLUUID web_login_key,
	const std::string& start,
	BOOL skip_optional,
	BOOL accept_tos,
	BOOL accept_critical_message,
	BOOL last_exec_froze, 
	const std::vector<const char*>& requested_options,
	const std::string& hashed_mac,
	const std::string& hashed_volume_serial)
{
	LL_INFOS2("AppInit", "Authentication") << "Authenticating: " << firstname << " " << lastname << ", "
			<< /*dpasswd.c_str() <<*/ LL_ENDL;
	std::ostringstream option_str;
	option_str << "Options: ";
	std::ostream_iterator<const char*> appender(option_str, ", ");
	std::copy(requested_options.begin(), requested_options.end(), appender);
	option_str << "END";
	
	LL_INFOS2("AppInit", "Authentication") << option_str.str() << LL_ENDL;

	mAuthResponse = E_NO_RESPONSE_YET;
	//mDownloadTimer.reset();
	
	// create the request
	XMLRPC_REQUEST request = XMLRPC_RequestNew();
	XMLRPC_RequestSetMethodName(request, method.c_str());
	XMLRPC_RequestSetRequestType(request, xmlrpc_request_call);

	// stuff the parameters
	XMLRPC_VALUE params = XMLRPC_CreateVector(NULL, xmlrpc_vector_struct);
	XMLRPC_VectorAppendString(params, "first", firstname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "last", lastname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "web_login_key", web_login_key.getString().c_str(), 0);
	XMLRPC_VectorAppendString(params, "start", start.c_str(), 0);
	// <edit>
	//XMLRPC_VectorAppendString(params, "version", gCurrentVersion.c_str(), 0); // Includes channel name
	//XMLRPC_VectorAppendString(params, "channel", gSavedSettings.getString("VersionChannelName").c_str(), 0);
	
	//WOW NEIL YOU ARE SO AWESOME!!
	
	XMLRPC_VectorAppendString(params, "version", std::string(
		gSavedSettings.getString("SpecifiedChannel") + " " +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionMaj")) + "." +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionMin")) + "." +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionPatch")) + "." +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionBuild"))
	).c_str(), 0); // Includes channel name
	
	XMLRPC_VectorAppendString(params, "channel", gSavedSettings.getString("SpecifiedChannel").c_str(), 0);
	XMLRPC_VectorAppendString(params, "platform", PLATFORM_STRING, 0);
	// </edit>
	// <edit>
	if(gSavedSettings.getBOOL("SpecifyMAC"))
		XMLRPC_VectorAppendString(params, "mac", gSavedSettings.getString("SpecifiedMAC").c_str(), 0);
	else
	// </edit>
	XMLRPC_VectorAppendString(params, "mac", hashed_mac.c_str(), 0);
	// A bit of security through obscurity: id0 is volume_serial
	// <edit>
	if(gSavedSettings.getBOOL("SpecifyID0"))
		XMLRPC_VectorAppendString(params, "id0", gSavedSettings.getString("SpecifiedID0").c_str(), 0);
	else
	// </edit>
	XMLRPC_VectorAppendString(params, "id0", hashed_volume_serial.c_str(), 0);
	if (skip_optional)
	{
		XMLRPC_VectorAppendString(params, "skipoptional", "true", 0);
	}
	if (accept_tos)
	{
		XMLRPC_VectorAppendString(params, "agree_to_tos", "true", 0);
	}
	if (accept_critical_message)
	{
		XMLRPC_VectorAppendString(params, "read_critical", "true", 0);
	}
	XMLRPC_VectorAppendInt(params, "last_exec_event", (int) last_exec_froze);

	// append optional requests in an array
	XMLRPC_VALUE options = XMLRPC_CreateVector("options", xmlrpc_vector_array);
	std::vector<const char*>::const_iterator it = requested_options.begin();
	std::vector<const char*>::const_iterator end = requested_options.end();
	for( ; it < end; ++it)
	{
		XMLRPC_VectorAppendString(options, NULL, (*it), 0);
	}
	XMLRPC_AddValueToVector(params, options);

	// put the parameters on the request
	XMLRPC_RequestSetData(request, params);

	mTransaction = new LLXMLRPCTransaction(auth_uri, request);
	
	XMLRPC_RequestFree(request, 1);

	LL_INFOS2("AppInit", "Authentication") << "LLUserAuth::authenticate: uri=" << auth_uri << LL_ENDL;
}



// Legacy version of constructor

// passwd is already MD5 hashed by the time we get to it.
void LLUserAuth::authenticate(
	const std::string& auth_uri,
	const std::string& method,
	const std::string& firstname,
	const std::string& lastname,
	const std::string& passwd,
	const std::string& start,
	BOOL skip_optional,
	BOOL accept_tos,
	BOOL accept_critical_message,
	BOOL last_exec_froze, 
	const std::vector<const char*>& requested_options,
	const std::string& hashed_mac,
	const std::string& hashed_volume_serial)
{
	std::string dpasswd("$1$");
	dpasswd.append(passwd);
	LL_INFOS2("AppInit", "Authentication") << "Authenticating: " << firstname << " " << lastname << ", "
			<< /*dpasswd.c_str() <<*/ LL_ENDL;
	std::ostringstream option_str;
	option_str << "Options: ";
	std::ostream_iterator<const char*> appender(option_str, ", ");
	std::copy(requested_options.begin(), requested_options.end(), appender);
	option_str << "END";

	LL_INFOS2("AppInit", "Authentication") << option_str.str().c_str() << LL_ENDL;

	mAuthResponse = E_NO_RESPONSE_YET;
	//mDownloadTimer.reset();
	
	// create the request
	XMLRPC_REQUEST request = XMLRPC_RequestNew();
	XMLRPC_RequestSetMethodName(request, method.c_str());
	XMLRPC_RequestSetRequestType(request, xmlrpc_request_call);

	// stuff the parameters
	XMLRPC_VALUE params = XMLRPC_CreateVector(NULL, xmlrpc_vector_struct);
	XMLRPC_VectorAppendString(params, "first", firstname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "last", lastname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "passwd", dpasswd.c_str(), 0);
	XMLRPC_VectorAppendString(params, "start", start.c_str(), 0);
	// <edit>
	//XMLRPC_VectorAppendString(params, "version", gCurrentVersion.c_str(), 0); // Includes channel name
	//XMLRPC_VectorAppendString(params, "channel", gSavedSettings.getString("VersionChannelName").c_str(), 0);
	
	//WOW NEIL YOU ARE SO AWESOME!!
	
	XMLRPC_VectorAppendString(params, "version", std::string(
		gSavedSettings.getString("SpecifiedChannel") + " " +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionMaj")) + "." +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionMin")) + "." +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionPatch")) + "." +
		llformat("%d", gSavedSettings.getU32("SpecifiedVersionBuild"))
	).c_str(), 0); // Includes channel name
	
	XMLRPC_VectorAppendString(params, "channel", gSavedSettings.getString("SpecifiedChannel").c_str(), 0);
	// </edit>
	XMLRPC_VectorAppendString(params, "platform", PLATFORM_STRING, 0);
	// <edit>
	if(gSavedSettings.getBOOL("SpecifyMAC"))
		XMLRPC_VectorAppendString(params, "mac", gSavedSettings.getString("SpecifiedMAC").c_str(), 0);
	else
	// </edit>
	XMLRPC_VectorAppendString(params, "mac", hashed_mac.c_str(), 0);
	// A bit of security through obscurity: id0 is volume_serial
	// <edit>
	if(gSavedSettings.getBOOL("SpecifyID0"))
		XMLRPC_VectorAppendString(params, "id0", gSavedSettings.getString("SpecifiedID0").c_str(), 0);
	else
	// </edit>
	XMLRPC_VectorAppendString(params, "id0", hashed_volume_serial.c_str(), 0);
	if (skip_optional)
	{
		XMLRPC_VectorAppendString(params, "skipoptional", "true", 0);
	}
	if (accept_tos)
	{
		XMLRPC_VectorAppendString(params, "agree_to_tos", "true", 0);
	}
	if (accept_critical_message)
	{
		XMLRPC_VectorAppendString(params, "read_critical", "true", 0);
	}
	XMLRPC_VectorAppendInt(params, "last_exec_event", (int) last_exec_froze);

	// append optional requests in an array
	XMLRPC_VALUE options = XMLRPC_CreateVector("options", xmlrpc_vector_array);
	std::vector<const char*>::const_iterator it = requested_options.begin();
	std::vector<const char*>::const_iterator end = requested_options.end();
	for( ; it < end; ++it)
	{
		XMLRPC_VectorAppendString(options, NULL, (*it), 0);
	}
	XMLRPC_AddValueToVector(params, options);

	// put the parameters on the request
	XMLRPC_RequestSetData(request, params);

	mTransaction = new LLXMLRPCTransaction(auth_uri, request);
	
	XMLRPC_RequestFree(request, 1);

	LL_INFOS2("AppInit", "Authentication") << "LLUserAuth::authenticate: uri=" << auth_uri << LL_ENDL;
}


LLUserAuth::UserAuthcode LLUserAuth::authResponse()
{
	if (!mTransaction)
	{
		return mAuthResponse;
	}
	
	bool done = mTransaction->process();

	if (!done) {
		if (LLXMLRPCTransaction::StatusDownloading == mTransaction->status(0))
		{
			mAuthResponse = E_DOWNLOADING;
		}
		
		return mAuthResponse;
	}
	
	
	mLastTransferRateBPS = mTransaction->transferRate();

	int result;
	mTransaction->status(&result);
	mErrorMessage = mTransaction->statusMessage();
	
	// if curl was ok, parse the download area.
	switch (result)
	{
	case CURLE_OK:
		mAuthResponse = parseResponse();
		break;
	case CURLE_COULDNT_RESOLVE_HOST:
		mAuthResponse = E_COULDNT_RESOLVE_HOST;
		break;
	case CURLE_SSL_PEER_CERTIFICATE:
		mAuthResponse = E_SSL_PEER_CERTIFICATE;
		break;
	case CURLE_SSL_CACERT:
		mAuthResponse = E_SSL_CACERT;
		break;
	case CURLE_SSL_CONNECT_ERROR:
		mAuthResponse = E_SSL_CONNECT_ERROR;
		break;
	default:
		mAuthResponse = E_UNHANDLED_ERROR;
		break;
	}
	
	LL_INFOS2("AppInit", "Authentication") << "Processed response: " << result << LL_ENDL;

	delete mTransaction;
	mTransaction = NULL;
	
	return mAuthResponse;
}

static void parseOptionInto(
	const std::string& id, XMLRPC_VALUE option, LLUserAuth::options_t& options)
{
	std::string key;
	std::string val;
	XMLRPC_VALUE_TYPE_EASY type;
	XMLRPC_VALUE row = XMLRPC_VectorRewind(option);
	while(row)
	{
		XMLRPC_VALUE opt  = XMLRPC_VectorRewind(row);
		LLUserAuth::response_t responses;
		while(opt)
		{
			key.assign(XMLRPC_GetValueID(opt));
			//llinfos << "option key: " << key << llendl;
			type = XMLRPC_GetValueTypeEasy(opt);
			if(xmlrpc_type_string == type)
			{
				val.assign(XMLRPC_GetValueString(opt));
				//llinfos << "string val: " << val << llendl;
			}
			else if(xmlrpc_type_int == type)
			{
				val = llformat("%d", XMLRPC_GetValueInt(opt));
				//llinfos << "int val: " << val << llendl;
			}
			else if(xmlrpc_type_double == type)
			{
				val = llformat("%g", XMLRPC_GetValueDouble(opt));
				//llinfos << "double val: " << val << llendl;
			}
			else
			{	// Can't understand the type
				val = "???";
				//llinfos << "unknown value type: " << type << llendl;
			}

			responses.insert(LLUserAuth::response_t::value_type(key, val));
			opt = XMLRPC_VectorNext(row);
		}
		options.push_back(responses);
		row = XMLRPC_VectorNext(option);
	}
}

LLUserAuth::UserAuthcode LLUserAuth::parseResponse()
{
	// The job of this function is to parse sCurlDownloadArea and
	// extract every member into either the mResponses or
	// mOptions. For now, we will only be looking at mResponses, which
	// will all be string => string pairs.
	UserAuthcode rv = E_UNHANDLED_ERROR;
	XMLRPC_REQUEST response = mTransaction->response();
	if(!response) return rv;

	// clear out any old parsing
	mResponses.clear();
	mOptions.clear();

	// Now, parse everything
	std::string key;
	std::string val;
	XMLRPC_VALUE param = NULL;
	XMLRPC_VALUE current = NULL;
	XMLRPC_VALUE_TYPE_EASY type;
	param = XMLRPC_RequestGetData(response);
	if(!param) goto exit;
	current = XMLRPC_VectorRewind(param);
	while(current)
	{
		key.assign(XMLRPC_GetValueID(current));
		lldebugs << "key: " << key << llendl;
		type = XMLRPC_GetValueTypeEasy(current);
		if(xmlrpc_type_string == type)
		{
			val.assign(XMLRPC_GetValueString(current));
			lldebugs << "val: " << val << llendl;
			mResponses.insert(response_t::value_type(key, val));
		}
		else if(xmlrpc_type_int == type)
		{
			val = llformat( "%d", XMLRPC_GetValueInt(current));
			lldebugs << "val: " << val << llendl;
			mResponses.insert(response_t::value_type(key, val));
		}
		else if(xmlrpc_type_array == type)
		{
			options_t options;
			parseOptionInto(key, current, options);
			mOptions.insert(all_options_t::value_type(key, options));
		}
		else
		{
			// whoops - bad response
			llwarns << "Unhandled xmlrpc type, key, value: " << type << " "
					<< key << " " << val << "." << llendl;
			rv = E_UNHANDLED_ERROR;
			break;
		}
		current = XMLRPC_VectorNext(param);
		rv = E_OK;
	}

 exit:
	return rv;
}

const std::string& LLUserAuth::getResponse(const std::string& key) const
{
	response_t::const_iterator it = mResponses.find(key);
	if(it != mResponses.end())
	{
		return((*it).second);
	}
	return LLStringUtil::null;
}

BOOL LLUserAuth::getOptions(const std::string& key, options_t& options) const
{
	all_options_t::const_iterator it = mOptions.find(key);
	if(it != mOptions.end())
	{
		// found the option set, copy them onto the container.
		std::back_insert_iterator<options_t> ii(options);
		std::copy((*it).second.begin(), (*it).second.end(), ii);
		return TRUE;
	}
	return FALSE;
}



