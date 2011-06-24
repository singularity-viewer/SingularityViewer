/**
 * @file llsavedlogins.cpp
 * @brief Manages a list of previous successful logins
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2009, Linden Research, Inc.
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
#include "llsavedlogins.h"
#include "llxorcipher.h"
#include "llsdserialize.h"
#include "hippogridmanager.h"

//---------------------------------------------------------------------------
// LLSavedLoginEntry methods
//---------------------------------------------------------------------------

LLSavedLoginEntry::LLSavedLoginEntry(const LLSD& entry_data)
{
	if (entry_data.isUndefined() || !entry_data.isMap())
	{
		throw std::invalid_argument("Cannot create a null login entry.");
	}
	if (!entry_data.has("firstname"))
	{
		throw std::invalid_argument("Missing firstname key.");
	}
	if (!entry_data.has("lastname"))
	{
		throw std::invalid_argument("Missing lastname key.");
	}
	if (!entry_data.has("password"))
	{
		throw std::invalid_argument("Missing password key.");
	}
	if (!entry_data.has("grid"))
	{
		throw std::invalid_argument("Missing grid key.");
	}
	if (!entry_data.get("firstname").isString())
	{
		throw std::invalid_argument("firstname key is not string.");
	}
	if (!entry_data.get("lastname").isString())
	{
		throw std::invalid_argument("lastname key is not string.");
	}
	if (!(entry_data.get("password").isUndefined() || entry_data.get("password").isBinary()))
	{
		throw std::invalid_argument("password key is neither blank nor binary.");
	}
	if (!entry_data.get("grid").isString())
	{
		throw std::invalid_argument("grid key is not string.");
	}
	mEntry = entry_data;
}

LLSavedLoginEntry::LLSavedLoginEntry(const std::string& firstname,
									 const std::string& lastname,
									 const std::string& password,
                                     const std::string& grid)
{
	mEntry.clear();
	mEntry.insert("firstname", LLSD(firstname));
	mEntry.insert("lastname", LLSD(lastname));
	mEntry.insert("grid", LLSD(grid));
	setPassword(password);
}

LLSD LLSavedLoginEntry::asLLSD() const
{
	return mEntry;
}

const std::string LLSavedLoginEntry::getPassword() const
{
	return (mEntry.has("password") ? decryptPassword(mEntry.get("password")) : std::string());
}
void LLSavedLoginEntry::setPassword(const std::string& value)
{
	mEntry.insert("password", encryptPassword(value));
}

const std::string LLSavedLoginEntry::decryptPassword(const LLSD& pwdata)
{
	std::string pw = "";

	if (pwdata.isBinary() && pwdata.asBinary().size() == PASSWORD_HASH_LENGTH+1)
	{
		LLSD::Binary buffer = pwdata.asBinary();

		LLXORCipher cipher(gMACAddress, 6);
		cipher.decrypt(&buffer[0], PASSWORD_HASH_LENGTH);

		buffer[PASSWORD_HASH_LENGTH] = '\0';
		if (LLStringOps::isHexString(std::string(reinterpret_cast<const char*>(&buffer[0]), PASSWORD_HASH_LENGTH)))
		{
			pw.assign(reinterpret_cast<char*>(&buffer[0]));
		}
	}

	return pw;
}

const LLSD LLSavedLoginEntry::encryptPassword(const std::string& password)
{
	LLSD pwdata;

	if (password.size() == PASSWORD_HASH_LENGTH && LLStringOps::isHexString(password))
	{
		LLSD::Binary buffer(PASSWORD_HASH_LENGTH+1);
		LLStringUtil::copy(reinterpret_cast<char*>(&buffer[0]), password.c_str(), PASSWORD_HASH_LENGTH+1);
		buffer[PASSWORD_HASH_LENGTH] = '\0';
		LLXORCipher cipher(gMACAddress, 6);
		cipher.encrypt(&buffer[0], PASSWORD_HASH_LENGTH);
		pwdata.assign(buffer);
	}

	return pwdata;
}

//---------------------------------------------------------------------------
// LLSavedLogins methods
//---------------------------------------------------------------------------

LLSavedLogins::LLSavedLogins()
{
}

LLSavedLogins::LLSavedLogins(const LLSD& history_data)
{
	if (!history_data.isArray()) throw std::invalid_argument("Invalid history data.");
	for (LLSD::array_const_iterator i = history_data.beginArray();
		 i != history_data.endArray(); ++i)
	{
	  	// Put the last used grids first.
		if (!i->isUndefined()) mEntries.push_front(LLSavedLoginEntry(*i));
	}
}

LLSD LLSavedLogins::asLLSD() const
{
	LLSD output;
	for (LLSavedLoginsList::const_iterator i = mEntries.begin();
		 i != mEntries.end(); ++i)
	{
		output.insert(0, i->asLLSD());
	}
	return output;
}

void LLSavedLogins::addEntry(const LLSavedLoginEntry& entry)
{
	mEntries.push_back(entry);
}

void LLSavedLogins::deleteEntry(const std::string& firstname,
				const std::string& lastname, const std::string& grid)
{
	for (LLSavedLoginsList::iterator i = mEntries.begin();
		 i != mEntries.end();)
	{
		if (i->getFirstName() == firstname &&
			i->getLastName() == lastname &&
		    i->getGrid() == grid)
		{
			i = mEntries.erase(i);
		}
		else
		{
			++i;
		}
	}
}

LLSavedLogins LLSavedLogins::loadFile(const std::string& filepath)
{
	LLSavedLogins hist;
	LLSD data;

	llifstream file(filepath);

	if (file.is_open())
	{
		llinfos << "Loading login history file at " << filepath << llendl;
		LLSDSerialize::fromXML(data, file);
	}

	if (data.isUndefined())
	{
		llinfos << "Login History File \"" << filepath << "\" is missing, "
		    "ill-formed, or simply undefined; not loading the file." << llendl;
	}
	else
	{
		try
		{
			hist = LLSavedLogins(data);
		}
		catch(std::invalid_argument& error)
		{
			llwarns << "Login History File \"" << filepath << "\" is ill-formed (" <<
			        error.what() << "); not loading the file." << llendl;
		}
	}

	return hist;
}

bool LLSavedLogins::saveFile(const LLSavedLogins& history, const std::string& filepath)
{
	llofstream out(filepath);
	if (!out.good())
	{
		llwarns << "Unable to open \"" << filepath << "\" for output." << llendl;
		return false;
	}

	LLSDSerialize::toPrettyXML(history.asLLSD(), out);

	out.close();
	return true;
}
