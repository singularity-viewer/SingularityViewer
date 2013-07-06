/** 
 * @file llxmlrpcresponder.h
 * @brief LLXMLRPCResponder and related class header file
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
 * Copyright (c) 2012, Aleric Inglewood.
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

#ifndef LLXMLRPCRESPONDER_H
#define LLXMLRPCRESPONDER_H

#include <string>
#include "llcurl.h"
#include "llhttpstatuscodes.h"

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy XMLRPCResponder_timeout;

typedef struct _xmlrpc_request* XMLRPC_REQUEST;
typedef struct _xmlrpc_value* XMLRPC_VALUE;
	// foward decl of types from xmlrpc.h (this usage is type safe)

class LLXMLRPCValue
	// a c++ wrapper around XMLRPC_VALUE
{
public:
	LLXMLRPCValue()						: mV(NULL) { }
	LLXMLRPCValue(XMLRPC_VALUE value)	: mV(value) { }
	
	bool isValid() const;
	
	std::string	asString()	const;
	int			asInt()		const;
	bool		asBool()	const;
	double		asDouble()	const;

	LLXMLRPCValue operator[](const char*) const;
	
	LLXMLRPCValue rewind();
	LLXMLRPCValue next();

	static LLXMLRPCValue createArray();
	static LLXMLRPCValue createStruct();

	void append(LLXMLRPCValue&);
	void appendString(const std::string&);
	void appendInt(int);
	void appendBool(bool);
	void appendDouble(double);
	void appendValue(LLXMLRPCValue&);

	void append(const char*, LLXMLRPCValue&);
	void appendString(const char*, const std::string&);
	void appendInt(const char*, int);
	void appendBool(const char*, bool);
	void appendDouble(const char*, double);
	void appendValue(const char*, LLXMLRPCValue&);
	
	void cleanup();
		// only call this on the top level created value
	
	XMLRPC_VALUE getValue() const;
	
private:
	XMLRPC_VALUE mV;
};

class XMLRPCResponder : public LLHTTPClient::LegacyPolledResponder {
private:
	AITransferInfo mTransferInfo;
	S32 mBufferSize;
	bool mReceivedHTTPHeader;
	XMLRPC_REQUEST mResponse;

public:
	XMLRPCResponder(void) : mBufferSize(0), mReceivedHTTPHeader(false), mResponse(NULL) { }

	// Accessors.
	F64 transferRate(void) const;
	bool is_downloading(void) const { return mReceivedHTTPHeader; }
	XMLRPC_REQUEST response(void) const { return mResponse; }
	LLXMLRPCValue responseValue(void) const;

	/*virtual*/ bool needsHeaders(void) const { return true; }
	/*virtual*/ bool forbidReuse(void) const { return true; }
	/*virtual*/ void received_HTTP_header(void) { mReceivedHTTPHeader = true; LLHTTPClient::ResponderBase::received_HTTP_header(); }
	/*virtual*/ void completed_headers(U32 status, std::string const& reason, AITransferInfo* info);
	/*virtual*/ void completedRaw(U32 status, std::string const& reason, LLChannelDescriptors const& channels, buffer_ptr_t const& buffer);
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return XMLRPCResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "XMLRPCResponder"; }
};

#endif // LLXMLRPCRESPONDER_H
