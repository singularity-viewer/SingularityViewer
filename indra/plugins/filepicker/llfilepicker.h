/** 
 * @file llfilepicker.h
 * @brief OS-specific file picker
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

// OS specific file selection dialog. This is implemented as a
// singleton class, so call the instance() method to get the working
// instance. When you call getMultipleLoadFile(), it locks the picker
// until you iterate to the end of the list of selected files with
// getNextFile() or call reset().

#ifndef LL_LLFILEPICKER_H
#define LL_LLFILEPICKER_H

#include "legacy.h"
#include <vector>

// This class is used as base class of a singleton and is therefore not
// allowed to have any static members or static local variables!
class LLFilePickerBase
{
public:
	enum ELoadFilter
	{
		FFLOAD_ALL = 1,
		FFLOAD_WAV = 2,
		FFLOAD_IMAGE = 3,
		FFLOAD_ANIM = 4,
#ifdef _CORY_TESTING
		FFLOAD_GEOMETRY = 5,
#endif
		FFLOAD_XML = 6,
		FFLOAD_SLOBJECT = 7,
		FFLOAD_RAW = 8,
		// <edit>
		FFLOAD_INVGZ = 9,
		FFLOAD_AO = 10,
		FFLOAD_BLACKLIST = 11
		// </edit>
	};

	enum ESaveFilter
	{
		FFSAVE_ALL = 1,
		FFSAVE_WAV = 3,
		FFSAVE_TGA = 4,
		FFSAVE_BMP = 5,
		FFSAVE_AVI = 6,
		FFSAVE_ANIM = 7,
#ifdef _CORY_TESTING
		FFSAVE_GEOMETRY = 8,
#endif
		FFSAVE_XML = 9,
		FFSAVE_COLLADA = 10,
		FFSAVE_RAW = 11,
		FFSAVE_J2C = 12,
		FFSAVE_PNG = 13,
		FFSAVE_JPEG = 14,
		// <edit>
		FFSAVE_ANIMATN = 15,
		FFSAVE_OGG = 16,
		FFSAVE_NOTECARD = 17,
		FFSAVE_GESTURE = 18,
		FFSAVE_LSL = 19,
		// good grief
		FFSAVE_SHAPE = 20,
		FFSAVE_SKIN = 21,
		FFSAVE_HAIR = 22,
		FFSAVE_EYES = 23,
		FFSAVE_SHIRT = 24,
		FFSAVE_PANTS = 25,
		FFSAVE_SHOES = 26,
		FFSAVE_SOCKS = 27,
		FFSAVE_JACKET = 28,
		FFSAVE_GLOVES = 29,
		FFSAVE_UNDERSHIRT = 30,
		FFSAVE_UNDERPANTS = 31,
		FFSAVE_SKIRT = 32,
		FFSAVE_INVGZ = 33,
		FFSAVE_LANDMARK = 34,
		FFSAVE_AO = 35,
		FFSAVE_BLACKLIST = 36,
		FFSAVE_PHYSICS = 37,
		// </edit>
	};

	// open the dialog. This is a modal operation
	bool getSaveFile(ESaveFilter filter, std::string const& filename, std::string const& folder);
	bool getLoadFile(ELoadFilter filter, std::string const& folder);
	bool getMultipleLoadFiles(ELoadFilter filter, std::string const& folder);

	// Get the filename(s) found. getFirstFile() sets the pointer to
	// the start of the structure and allows the start of iteration.
	const std::string getFirstFile();

	// getNextFile() increments the internal representation and
	// returns the next file specified by the user. Returns NULL when
	// no more files are left. Further calls to getNextFile() are
	// undefined.
	const std::string getNextFile();

	// This utility function extracts the current file name without
	// doing any incrementing.
	const std::string getCurFile();

	// Returns the index of the current file.
	S32 getCurFileNum() const { return mCurrentFile; }

	S32 getFileCount() const { return (S32)mFiles.size(); }

	// See llvfs/lldir.h : getBaseFileName and getDirName to extract base or directory names
	
	// clear any lists of buffers or whatever, and make sure the file
	// picker isn't locked.
	void reset();

private:
	enum
	{
		SINGLE_FILENAME_BUFFER_SIZE = 1024,
		//FILENAME_BUFFER_SIZE = 65536
		FILENAME_BUFFER_SIZE = 65000
	};
	
#if LL_WINDOWS
	OPENFILENAMEW mOFN;				// for open and save dialogs
	WCHAR mFilesW[FILENAME_BUFFER_SIZE];

public:
	void setWindowID(unsigned long window_id) { mOFN.hwndOwner = (HWND)window_id; }

private:
	bool setupFilter(ELoadFilter filter);
#endif	// LL_WINDOWS

#if LL_DARWIN
	NavDialogCreationOptions mNavOptions;
	
	OSStatus doNavChooseDialog(ELoadFilter filter);
	OSStatus doNavSaveDialog(ESaveFilter filter, const std::string& filename);
	static Boolean navOpenFilterProc(AEDesc *theItem, void *info, void *callBackUD, NavFilterModes filterMode);
    static pascal void LLFilePickerBase::doNavCallbackEvent(NavEventCallbackMessage callBackSelector, NavCBRecPtr callBackParms, void* callBackUD);
	ELoadFilter getLoadFilter(void) const { return mLoadFilter; }
	std::string const& getFolder(void) const { return mFolder; }
#endif	// LL_DARWIN

#if LL_GTK
	static void add_to_selectedfiles(gpointer data, gpointer user_data);
	static void chooser_responder(GtkWidget *widget, gint response, gpointer user_data);
	// we remember the last path that was accessed for a particular usage
	std::map <std::string, std::string> mContextToPathMap;
	std::string mCurContextName;
#if LL_X11
	Window mX11WindowID;
#endif

public:
	std::map <std::string, std::string>& get_ContextToPathMap(void) { return mContextToPathMap; }
#if LL_X11
	void setWindowID(unsigned long window_id) { mX11WindowID = (Window)window_id; }
#endif

#endif	// LL_GTK
#if !LL_WINDOWS && !(LL_GTK && LL_X11)
	void setWindowID(unsigned long window_id) { PLS_WARNS << "Calling unimplemented LLFilePickerBase::setWindowID" << PLS_ENDL; }
#endif

private:
	std::vector<std::string> mFiles;
	S32 mCurrentFile;
	bool mLocked;
	bool mMultiFile;
#if LL_DARWIN
	ELoadFilter mLoadFilter;
	std::string mFolder;
#endif

protected:
#if LL_GTK
	GtkWindow* buildFilePicker(bool is_save, bool is_folder, std::string const& folder);
#endif

protected:
	LLFilePickerBase();
};

// True singleton, private constructors (and no friends).
class LLFilePicker : public LLFilePickerBase
{
public:
	// calling this before main() is undefined
	static LLFilePicker& instance( void ) { return sInstance; }

private:
	static LLFilePicker sInstance;

	LLFilePicker() { }
};

#endif
