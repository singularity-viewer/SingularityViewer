/** 
 * @file llsavedsettingsglue.cpp
 * @author James Cook
 * @brief LLSavedSettingsGlue class implementation
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

#include "llsavedsettingsglue.h"

#include "lluictrl.h"

#include "llviewercontrol.h"
/*
void LLSavedSettingsGlue::setBOOL(LLUICtrl* ctrl, void* data)
{
	const char* name = (const char*)data;
	LLSD value = ctrl->getValue();
	gSavedSettings.setBOOL(name, value.asBoolean());
}

void LLSavedSettingsGlue::setS32(LLUICtrl* ctrl, void* data)
{
	const char* name = (const char*)data;
	LLSD value = ctrl->getValue();
	gSavedSettings.setS32(name, value.asInteger());
}

void LLSavedSettingsGlue::setF32(LLUICtrl* ctrl, void* data)
{
	const char* name = (const char*)data;
	LLSD value = ctrl->getValue();
	gSavedSettings.setF32(name, (F32)value.asReal());
}

void LLSavedSettingsGlue::setU32(LLUICtrl* ctrl, void* data)
{
	const char* name = (const char*)data;
	LLSD value = ctrl->getValue();
	gSavedSettings.setU32(name, (U32)value.asInteger());
}

void LLSavedSettingsGlue::setString(LLUICtrl* ctrl, void* data)
{
	const char* name = (const char*)data;
	LLSD value = ctrl->getValue();
	gSavedSettings.setString(name, value.asString());
}
*/

/*
//Begin Ascent SavedSettings/PerAccountSettings handling

//Get
LLControlVariable *LLSavedSettingsGlue::getControl(const std::string &name)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		return gSavedSettings.getControl(name);
	else
		return gSavedPerAccountSettings.getControl(name);
}
BOOL LLSavedSettingsGlue::getBOOL(const std::string &name)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		return gSavedSettings.getBOOL(name);
	else
		return gSavedPerAccountSettings.getBOOL(name);
}

S32 LLSavedSettingsGlue::getS32(const std::string &name)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		return gSavedSettings.getS32(name);
	else
		return gSavedPerAccountSettings.getS32(name);
}

F32 LLSavedSettingsGlue::getF32(const std::string &name)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		return gSavedSettings.getF32(name);
	else
		return gSavedPerAccountSettings.getF32(name);
}

U32 LLSavedSettingsGlue::getU32(const std::string &name)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		return gSavedSettings.getU32(name);
	else
		return gSavedPerAccountSettings.getU32(name);
}

std::string LLSavedSettingsGlue::getString(const std::string &name)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		return gSavedSettings.getString(name);
	else
		return gSavedPerAccountSettings.getString(name);
}

LLColor4 LLSavedSettingsGlue::getColor4(const std::string &name)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		return gSavedSettings.getColor4(name);
	else
		return gSavedPerAccountSettings.getColor4(name);
}

//Set

void LLSavedSettingsGlue::setBOOL(const std::string &name, BOOL value)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		gSavedSettings.setBOOL(name, value);
	else
		gSavedPerAccountSettings.setBOOL(name, value);
}

void LLSavedSettingsGlue::setS32(const std::string &name, S32 value)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		gSavedSettings.setS32(name, value);
	else
		gSavedPerAccountSettings.setS32(name, value);
}

void LLSavedSettingsGlue::setF32(const std::string &name, F32 value)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		gSavedSettings.setF32(name, value);
	else
		gSavedPerAccountSettings.setF32(name, value);
}

void LLSavedSettingsGlue::setU32(const std::string &name, U32 value)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		gSavedSettings.setU32(name, value);
	else
		gSavedPerAccountSettings.setU32(name, value);
}

void LLSavedSettingsGlue::setString(const std::string &name, std::string value)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		gSavedSettings.setString(name, value);
	else
		gSavedPerAccountSettings.setString(name, value);
}

void LLSavedSettingsGlue::setColor4(const std::string &name, LLColor4 value)
{
	if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
		gSavedSettings.setColor4(name, value);
	else
		gSavedPerAccountSettings.setColor4(name, value);
}*/
