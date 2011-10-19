/** 
 * @file lldirpicker.cpp
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

#include "linden_common.h"
#include "lldirpicker.h"
#include "llerror.h"
#include "basic_plugin_base.h"      // For PLS_INFOS etc.

//
// Globals
//

LLDirPicker LLDirPicker::sInstance;

#if LL_WINDOWS
#include <shlobj.h>
#endif

//
// Implementation
//
#if LL_WINDOWS

LLDirPicker::LLDirPicker() 
{
}

LLDirPicker::~LLDirPicker()
{
	// nothing
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}
	return (uMsg == BFFM_VALIDATEFAILED);
}

BOOL LLDirPicker::getDir(std::string const& folder)
{
	if( mLocked )
	{
		return FALSE;
	}
	BOOL success = FALSE;

	BROWSEINFO bi;
	memset(&bi, 0, sizeof(bi));

	bi.ulFlags   = BIF_USENEWUI;
	bi.hwndOwner = NULL;//GetCurrentProcess();
	bi.lpszTitle = NULL;
	llutf16string tstring = utf8str_to_utf16str(folder);
	bi.lParam = (LPARAM)tstring.c_str();
	bi.lpfn = &BrowseCallbackProc;

	::OleInitialize(NULL);

	PLS_FLUSH;
	LPITEMIDLIST pIDL = ::SHBrowseForFolder(&bi);

	if(pIDL != NULL)
	{
	  WCHAR buffer[_MAX_PATH] = {'\0'};

	  if(::SHGetPathFromIDList(pIDL, buffer) != 0)
	  {
			// Set the string value.

			mDir = utf16str_to_utf8str(llutf16string(buffer));
			 success = TRUE;
	  }

	  // free the item id list
	  CoTaskMemFree(pIDL);
	}

	::OleUninitialize();

	return success;
}

std::string LLDirPicker::getDirName()
{
	return mDir;
}

/////////////////////////////////////////////DARWIN
#elif LL_DARWIN

LLDirPicker::LLDirPicker() 
{
	reset();

	memset(&mNavOptions, 0, sizeof(mNavOptions));
	OSStatus	error = NavGetDefaultDialogCreationOptions(&mNavOptions);
	if (error == noErr)
	{
		mNavOptions.modality = kWindowModalityAppModal;
	}
}

LLDirPicker::~LLDirPicker()
{
	// nothing
}

//static
pascal void LLDirPicker::doNavCallbackEvent(NavEventCallbackMessage callBackSelector,
										 NavCBRecPtr callBackParms, void* callBackUD)
{
	switch(callBackSelector)
	{
		case kNavCBStart:
		{
			if (!sInstance.mFileName) break;
 
			OSStatus error = noErr; 
			AEDesc theLocation = {typeNull, NULL};
			FSSpec outFSSpec;
			
			//Convert string to a FSSpec
			FSRef myFSRef;
			
			const char* folder = sInstance.mFileName->c_str();
			
			error = FSPathMakeRef ((UInt8*)folder,	&myFSRef, 	NULL); 
			
			if (error != noErr) break;

			error = FSGetCatalogInfo (&myFSRef, kFSCatInfoNone, NULL, NULL, &outFSSpec, NULL);

			if (error != noErr) break;
	
			error = AECreateDesc(typeFSS, &outFSSpec, sizeof(FSSpec), &theLocation);

			if (error != noErr) break;

			error = NavCustomControl(callBackParms->context,
							kNavCtlSetLocation, (void*)&theLocation);

		}
	}
}

OSStatus	LLDirPicker::doNavChooseDialog()
{
	OSStatus		error = noErr;
	NavDialogRef	navRef = NULL;
	NavReplyRecord	navReply;

	memset(&navReply, 0, sizeof(navReply));
	
	// NOTE: we are passing the address of a local variable here.  
	//   This is fine, because the object this call creates will exist for less than the lifetime of this function.
	//   (It is destroyed by NavDialogDispose() below.)

	error = NavCreateChooseFolderDialog(&mNavOptions, &doNavCallbackEvent, NULL, NULL, &navRef);

	if (error == noErr)
	{
		PLS_FLUSH;
		error = NavDialogRun(navRef);
	}

	if (error == noErr)
		error = NavDialogGetReply(navRef, &navReply);

	if (navRef)
		NavDialogDispose(navRef);

	if (error == noErr && navReply.validRecord)
	{	
		FSRef		fsRef;
		AEKeyword	theAEKeyword;
		DescType	typeCode;
		Size		actualSize = 0;
		char		path[LL_MAX_PATH];		 /*Flawfinder: ignore*/
		
		memset(&fsRef, 0, sizeof(fsRef));
		error = AEGetNthPtr(&navReply.selection, 1, typeFSRef, &theAEKeyword, &typeCode, &fsRef, sizeof(fsRef), &actualSize);
		
		if (error == noErr)
			error = FSRefMakePath(&fsRef, (UInt8*) path, sizeof(path));
		
		if (error == noErr)
			mDir = path;
	}
	
	return error;
}

BOOL LLDirPicker::getDir(std::string const& folder)
{
	if( mLocked ) return FALSE;
	BOOL success = FALSE;
	OSStatus	error = noErr;
	
	mFileName = const_cast<std::string*>(&folder);
	
//	mNavOptions.saveFileName 

	{
		error = doNavChooseDialog();
	}
	if (error == noErr)
	{
		if (mDir.length() >  0)
			success = true;
	}

	return success;
}

std::string LLDirPicker::getDirName()
{
	return mDir;
}

void LLDirPicker::reset()
{
	mLocked = FALSE;
	mDir.clear();
}

#elif LL_LINUX || LL_SOLARIS

LLDirPicker::LLDirPicker() 
{
	reset();
}

LLDirPicker::~LLDirPicker()
{
}


void LLDirPicker::reset()
{
	LLFilePickerBase::reset();
}

BOOL LLDirPicker::getDir(std::string const& folder)
{
	reset();
	GtkWindow* picker = buildFilePicker(false, true, folder);
	if (picker)
	{
	   gtk_window_set_title(GTK_WINDOW(picker), LLTrans::getString("choose_the_directory").c_str());
	   gtk_widget_show_all(GTK_WIDGET(picker));
	   PLS_FLUSH;
	   gtk_main();
	   return (!getFirstFile().empty());
	}
	return FALSE;
}

std::string LLDirPicker::getDirName()
{
	return getFirstFile();
}

#else // not implemented

LLDirPicker::LLDirPicker() 
{
	reset();
}

LLDirPicker::~LLDirPicker()
{
}


void LLDirPicker::reset()
{
}

BOOL LLDirPicker::getDir(std::string* folder)
{
	return FALSE;
}

std::string LLDirPicker::getDirName()
{
	return "";
}

#endif
