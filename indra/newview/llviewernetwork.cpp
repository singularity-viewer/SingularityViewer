/**
 * @file llviewernetwork.cpp
 * @author James Cook, Richard Nelson
 * @brief Networking constants and globals for viewer.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2010, Linden Research, Inc.
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

#include "llviewernetwork.h"
#include "llviewercontrol.h"

#include "hippogridmanager.h"

unsigned char gMACAddress[MAC_ADDRESS_BYTES];		/* Flawfinder: ignore */


const std::string LLViewerLogin::getCurrentGridURI() {
	return gHippoGridManager->getConnectedGrid()->getLoginUri();
}

void LLViewerLogin::getLoginURIs(std::vector<std::string>& uris) const
{
	uris.push_back(gHippoGridManager->getConnectedGrid()->getLoginUri());
}

const std::string &LLViewerLogin::getGridLabel() const
{
	return gHippoGridManager->getConnectedGrid()->getGridName();
}

const std::string &LLViewerLogin::getLoginPage() const
{
	return gHippoGridManager->getConnectedGrid()->getLoginPage();
}

const std::string &LLViewerLogin::getHelperURI() const
{
	return gHippoGridManager->getConnectedGrid()->getHelperUri();
	}

bool LLViewerLogin::isOpenSimulator()
{
	return gHippoGridManager->getConnectedGrid()->isOpenSimulator();
}

bool LLViewerLogin::isSecondLife()
{
	return gHippoGridManager->getConnectedGrid()->isSecondLife();
}

bool LLViewerLogin::isInProductionGrid()
{
	return true;
}

