/**
 * @file hippofloaterxml.cpp
 * @author Mana Janus
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.	Terms of
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

#include "hippofloaterxml.h"

#include <llchat.h>
#include <llcombobox.h>
#include <llfloater.h>
#include <llradiogroup.h>
#include <lluictrlfactory.h>

#include "llviewerwindow.h"


#define CHANNEL 427169570


// ********************************************************************
// floater implementation class

class HippoFloaterXmlImpl : public LLFloater
{
	public:
		HippoFloaterXmlImpl(const std::string &name, const std::string &xml);
		virtual ~HippoFloaterXmlImpl();

		BOOL postBuild();
		void onClose(bool quitting);

		static bool execute(LLUICtrl *ctrl,
							const std::string &cmds, std::string::size_type &offset,
							std::string &response);

	private:
		std::string mName;
		bool mIsNotifyOnClose;
};


// ********************************************************************
// global floater data

typedef std::map<std::string, HippoFloaterXmlImpl*> FloaterMap;
static FloaterMap gInstances;


// ********************************************************************
// floater XML descriptions

class XmlData
{
	public:
		explicit XmlData(int numParts) :
			mData(numParts), mNumParts(numParts), mPartsReceived(0)
		{
		}

		int getNumParts() const { return mNumParts; }
		bool isComplete() const { return (mPartsReceived >= mNumParts); }

		void add(int index, const std::string &data)
		{
			--index;  // 1 <= index <= mNumParts
			if ((index >= 0) && (index < mNumParts)) {
				if (mData[index].empty()) mPartsReceived++;
				mData[index] = data;
			}
		}

		void get(std::string &xml) const
		{
			xml = "";
			for (int i=0; i<mNumParts; i++)
				xml += mData[i];
		}

		void reinit(int numParts)
		{
			mNumParts = numParts;
			mPartsReceived = 0;
			mData.reserve(numParts);
			for (int i=0; i<mNumParts; i++)
				mData[i].clear();
		}

		static XmlData *getInstance(const std::string &floaterName, int numParts);
		static void release(const std::string &floaterName);

	private:
		std::vector<std::string> mData;
		int mNumParts;
		int mPartsReceived;
};

typedef std::map<std::string, XmlData*> XmlDataMap;
static XmlDataMap gXmlData;

XmlData *XmlData::getInstance(const std::string &floaterName, int numParts)
{
	XmlDataMap::iterator it = gXmlData.find(floaterName);
	if (it == gXmlData.end()) {
		XmlData *data = new XmlData(numParts);
		gXmlData[floaterName] = data;
		return data;
	} else {
		XmlData *data = it->second;
		if (data->getNumParts() != numParts)
			data->reinit(numParts);
		return data;
	}
}

void XmlData::release(const std::string &floaterName)
{
	XmlDataMap::iterator it = gXmlData.find(floaterName);
	if (it != gXmlData.end()) {
		delete gXmlData[floaterName];
		gXmlData.erase(it);
	}
}


// ********************************************************************
// create HippoFloaterXmlImpl

HippoFloaterXmlImpl::HippoFloaterXmlImpl(const std::string &name, const std::string &xml) :
	mName(name), mIsNotifyOnClose(false)
{
	gInstances[mName] = this;
	LLUICtrlFactory::getInstance()->buildFloaterFromBuffer(this, xml);
}

HippoFloaterXmlImpl::~HippoFloaterXmlImpl()
{
	FloaterMap::iterator it = gInstances.find(mName);
	if (it != gInstances.end())
		gInstances.erase(it);
}

BOOL HippoFloaterXmlImpl::postBuild()
{
	LLRect rect = getRect();
	if ((rect.mLeft == 0) && (rect.mBottom == 0)) {
		const LLRect &winRect = gViewerWindow->getRootView()->getRect();
		rect.setCenterAndSize((winRect.getWidth()+1)>>1, (winRect.getHeight()+1)>>1,
							   rect.getWidth(), rect.getHeight());
		setRect(rect);
	}
	return true;
}


// ********************************************************************
// static function declarations

static bool cmdGetToken(const std::string &cmds, std::string::size_type &offset, std::string &token);

// defined in llchatbar.cpp, but not declared in any header
void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel);


// ********************************************************************
// execute commands static

void HippoFloaterXml::execute(const std::string &cmds)
{
	std::string::size_type offset = 0;

	std::string floaterName;
	if (!cmdGetToken(cmds, offset, floaterName)) return;

	HippoFloaterXmlImpl *floater;
	FloaterMap::iterator it = gInstances.find(floaterName);
	if (it != gInstances.end())
		floater = it->second;
	else
		floater = 0;

	std::string token;
	while (cmdGetToken(cmds, offset, token)) {

		if (token == "{") {
			if (floater) {
				std::string response;
				if (!floater->execute(floater, cmds, offset, response))
					break;
				if (!response.empty())
					send_chat_from_viewer(response, CHAT_TYPE_WHISPER, CHANNEL);
			}
		} else

		if (token == "create") {
			if (floater) delete floater;
			std::string nStr, mStr;
			if (!cmdGetToken(cmds, offset, nStr)) break;
			if (!cmdGetToken(cmds, offset, mStr)) break;
			if (mStr != "/") break;
			if (!cmdGetToken(cmds, offset, mStr)) break;
			int n = strtol(nStr.c_str(), 0, 10);
			int m = strtol(mStr.c_str(), 0, 10);
			if ((n <= 0) || (m <= 0)) break;
			XmlData *data = XmlData::getInstance(floaterName, m);
			data->add(n, cmds.substr(offset+1));
			if (data->isComplete()) {
				std::string xml;
				data->get(xml);
				XmlData::release(floaterName);
				new HippoFloaterXmlImpl(floaterName, xml);
			}
			break;
		} else

		if (token == "show") {
			if (floater) floater->setVisible(true);
		} else

		if (token == "hide") {
			if (floater) floater->setVisible(false);
		} else

		if (token == "destroy") {
			if (floater) delete floater;
			floater = 0;
		}
	}
}


// ********************************************************************
// generic notification callbacks

static void notify(LLUICtrl *ctrl, void *data)
{
	std::string msg = "NOTIFY:";
	msg += ctrl->getName();
	msg += ':';
	msg += ctrl->getValue().asString();
	send_chat_from_viewer(msg, CHAT_TYPE_WHISPER, CHANNEL);
}

static void notify(void *data)
{
	notify(static_cast<LLUICtrl*>(data), 0);
}

void HippoFloaterXmlImpl::onClose(bool quitting)
{
	if (mIsNotifyOnClose)
		send_chat_from_viewer("NOTIFY:" + mName + ":closed",
							  CHAT_TYPE_WHISPER, CHANNEL);
	LLFloater::onClose(quitting);
}


// ********************************************************************
// execute commands on instance

bool HippoFloaterXmlImpl::execute(LLUICtrl *ctrl,
								  const std::string &cmds, std::string::size_type &offset,
								  std::string &response)
{
	std::string token;

	while (cmdGetToken(cmds, offset, token)) {

		if (token == "}") {
			return true;
		} else	{
			std::string key = token;
			if (key == "getValue") {
				response += "VALUE:" + ctrl->getName() + ':' + ctrl->getValue().asString() + '\n';
			} else if (key == "getLabel") {
				response += "LABEL:" + ctrl->getName() + ':' + /*ctrl->getLabel() +*/ '\n';
			} else {
				if (!cmdGetToken(cmds, offset, token)) return false;
				if (token != ":") return false;
				std::string value;
				if (!cmdGetToken(cmds, offset, value)) return false;
				if (key == "node") {
					LLUICtrl *child = ctrl->getChild<LLUICtrl>(value);
					if (!child) return false;
					if (!cmdGetToken(cmds, offset, token)) return false;
					if (token != "{") return false;
					if (!execute(child, cmds, offset, response))
						return false;
				} else if (key == "setValue") {
					ctrl->setValue(value);
				} else if (key == "setLabel") {
					/*ctrl->setLabel(value);*/
				} else if (key == "setVisible") {
					ctrl->setVisible(value != "0");
				} else if (key == "notify") {
					bool set = (value != "0");
					if (HippoFloaterXmlImpl *floater = dynamic_cast<HippoFloaterXmlImpl*>(ctrl)) {
						floater->mIsNotifyOnClose = set;
					} else if (LLButton *button = dynamic_cast<LLButton*>(ctrl)) {
						if (set)
							button->setClickedCallback(notify, ctrl);
						else
							button->setClickedCallback(0, 0);
					} else {
						if (set)
							ctrl->setCommitCallback(notify);
						else
							ctrl->setCommitCallback(0);
					}
				}
			}
		}
	}
	return false;
}


// ********************************************************************
// parsing tools

static bool cmdGetToken(const std::string &cmds, std::string::size_type &offset, std::string &token)
{
	token = "";
	std::string::size_type size = cmds.size();
	char ch;
	while ((offset < size) && isspace(cmds[offset])) offset++;
	if (offset >= size) return false;
	ch = cmds[offset];
	if (ch == '\'') {
		ch = cmds[++offset];
		while ((offset < size) && (ch != '\'')) {
			if (ch == '&') ch = cmds[++offset];
			token += ch;
			ch = cmds[++offset];
		}
		offset++;
	} else if (isdigit(ch)) {
		while ((offset < size) && isdigit(ch)) {
			token += ch;
			ch = cmds[++offset];
		}
	} else if (isalpha(ch)) {
		while ((offset < size) && (isalnum(ch) || (ch == '_'))) {
			token += ch;
			ch = cmds[++offset];
		}
	} else {
		token += ch;
		offset++;
	}
	return (token != "");
}

