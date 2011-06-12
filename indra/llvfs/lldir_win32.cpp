/** 
 * @file lldir_win32.cpp
 * @brief Implementation of directory utilities for windows
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#if LL_WINDOWS

#include "linden_common.h"

#include "lldir_win32.h"
#include "llerror.h"
#include "llrand.h"		// for gLindenLabRandomNumber
#include "shlobj.h"

#include <direct.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

// Utility stuff to get versions of the sh
#define PACKVERSION(major,minor) MAKELONG(minor,major)
DWORD GetDllVersion(LPCTSTR lpszDllName);

LLDir_Win32::LLDir_Win32()
{
	mDirDelimiter = "\\";

	WCHAR w_str[MAX_PATH];

	// Application Data is where user settings go
	SHGetSpecialFolderPath(NULL, w_str, CSIDL_APPDATA, TRUE);

	mOSUserDir = utf16str_to_utf8str(llutf16string(w_str));

	// We want cache files to go on the local disk, even if the
	// user is on a network with a "roaming profile".
	//
	// On XP this is:
	//   C:\Docments and Settings\James\Local Settings\Application Data
	// On Vista this is:
	//   C:\Users\James\AppData\Local
	//
	// We used to store the cache in AppData\Roaming, and the installer
	// cleans up that version on upgrade.  JC
	SHGetSpecialFolderPath(NULL, w_str, CSIDL_LOCAL_APPDATA, TRUE);
	mOSCacheDir = utf16str_to_utf8str(llutf16string(w_str));

	if (GetTempPath(MAX_PATH, w_str))
	{
		if (wcslen(w_str))	/* Flawfinder: ignore */ 
		{
			w_str[wcslen(w_str)-1] = '\0'; /* Flawfinder: ignore */ // remove trailing slash
		}
		mTempDir = utf16str_to_utf8str(llutf16string(w_str));
	}
	else
	{
		mTempDir = mOSUserDir;
	}

//	fprintf(stderr, "mTempDir = <%s>",mTempDir);

	// Set working directory, for LLDir::getWorkingDir()
	GetCurrentDirectory(MAX_PATH, w_str);
	mWorkingDir = utf16str_to_utf8str(llutf16string(w_str));

	// Set the executable directory
	S32 size = GetModuleFileName(NULL, w_str, MAX_PATH);
	if (size)
	{
		w_str[size] = '\0';
		mExecutablePathAndName = utf16str_to_utf8str(llutf16string(w_str));
		S32 path_end = mExecutablePathAndName.find_last_of('\\');
		if (path_end != std::string::npos)
		{
			mExecutableDir = mExecutablePathAndName.substr(0, path_end);
			mExecutableFilename = mExecutablePathAndName.substr(path_end+1, std::string::npos);
		}
		else
		{
			mExecutableFilename = mExecutablePathAndName;
		}

	}
	else
	{
		fprintf(stderr, "Couldn't get APP path, assuming current directory!\n");
		mExecutableDir = mWorkingDir;
		// Assume it's the current directory
	}

	// mAppRODataDir = ".";	

	// Determine the location of the App-Read-Only-Data
	// Try the working directory then the exe's dir.
	mAppRODataDir = mWorkingDir;	


//	if (mExecutableDir.find("indra") == std::string::npos)
	
	// *NOTE:Mani - It is a mistake to put viewer specific code in
	// the LLDir implementation. The references to 'skins' and 
	// 'llplugin' need to go somewhere else.
	// alas... this also gets called during static initialization 
	// time due to the construction of gDirUtil in lldir.cpp.
	if(! LLFile::isdir(mAppRODataDir + mDirDelimiter + "skins"))
	{
		// What? No skins in the working dir?
		// Try the executable's directory.
		mAppRODataDir = mExecutableDir;
	}

	llinfos << "mAppRODataDir = " << mAppRODataDir << llendl;

	mSkinBaseDir = mAppRODataDir + mDirDelimiter + "skins";

	// Build the default cache directory
	mDefaultCacheDir = buildSLOSCacheDir();
	
	// Make sure it exists
	int res = LLFile::mkdir(mDefaultCacheDir);
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_CACHE dir " << mDefaultCacheDir << llendl;
		}
	}

	mLLPluginDir = mExecutableDir + mDirDelimiter + "llplugin";
}

LLDir_Win32::~LLDir_Win32()
{
}

// Implementation

void LLDir_Win32::initAppDirs(const std::string &app_name,
							  const std::string& app_read_only_data_dir)
{
	// Allow override so test apps can read newview directory
	if (!app_read_only_data_dir.empty())
	{
		mAppRODataDir = app_read_only_data_dir;
		mSkinBaseDir = mAppRODataDir + mDirDelimiter + "skins";
	}
	mAppName = app_name;
	mOSUserAppDir = mOSUserDir;
	mOSUserAppDir += "\\";
	mOSUserAppDir += app_name;

	int res = LLFile::mkdir(mOSUserAppDir);
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create app user dir " << mOSUserAppDir << llendl;
			llwarns << "Default to base dir" << mOSUserDir << llendl;
			mOSUserAppDir = mOSUserDir;
		}
	}
	//dumpCurrentDirectories();

	res = LLFile::mkdir(getExpandedFilename(LL_PATH_LOGS,""));
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_LOGS dir " << getExpandedFilename(LL_PATH_LOGS,"") << llendl;
		}
	}
	
	res = LLFile::mkdir(getExpandedFilename(LL_PATH_USER_SETTINGS,""));
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_USER_SETTINGS dir " << getExpandedFilename(LL_PATH_USER_SETTINGS,"") << llendl;
		}
	}
	
	res = LLFile::mkdir(getExpandedFilename(LL_PATH_CACHE,""));
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_CACHE dir " << getExpandedFilename(LL_PATH_CACHE,"") << llendl;
		}
	}
	
	mCAFile = getExpandedFilename(LL_PATH_APP_SETTINGS, "CA.pem");
}

U32 LLDir_Win32::countFilesInDir(const std::string &dirname, const std::string &mask)
{
	HANDLE count_search_h;
	U32 file_count;

	file_count = 0;

	WIN32_FIND_DATA FileData;

	llutf16string pathname = utf8str_to_utf16str(dirname);
	pathname += utf8str_to_utf16str(mask);
	
	if ((count_search_h = FindFirstFile(pathname.c_str(), &FileData)) != INVALID_HANDLE_VALUE)   
	{
		file_count++;

		while (FindNextFile(count_search_h, &FileData))
		{
			file_count++;
		}
		   
		FindClose(count_search_h);
	}

	return (file_count);
}

std::string LLDir_Win32::getCurPath()
{
	WCHAR w_str[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, w_str);

	return utf16str_to_utf8str(llutf16string(w_str));
}


BOOL LLDir_Win32::fileExists(const std::string &filename) const
{
	llstat stat_data;
	// Check the age of the file
	// Now, we see if the files we've gathered are recent...
	int res = LLFile::stat(filename, &stat_data);
	if (!res)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


/*virtual*/ std::string LLDir_Win32::getLLPluginLauncher()
{
	return gDirUtilp->getExecutableDir() + gDirUtilp->getDirDelimiter() +
		"SLPlugin.exe";
}

/*virtual*/ std::string LLDir_Win32::getLLPluginFilename(std::string base_name)
{
	return gDirUtilp->getLLPluginDir() + gDirUtilp->getDirDelimiter() +
		base_name + ".dll";
}


#if 0
// Utility function to get version number of a DLL

#define PACKVERSION(major,minor) MAKELONG(minor,major)

DWORD GetDllVersion(LPCTSTR lpszDllName)
{

    HINSTANCE hinstDll;
    DWORD dwVersion = 0;

    hinstDll = LoadLibrary(lpszDllName);	/* Flawfinder: ignore */ 
	
    if(hinstDll)
    {
        DLLGETVERSIONPROC pDllGetVersion;

        pDllGetVersion = (DLLGETVERSIONPROC) GetProcAddress(hinstDll, "DllGetVersion");

/*Because some DLLs might not implement this function, you
  must test for it explicitly. Depending on the particular 
  DLL, the lack of a DllGetVersion function can be a useful
  indicator of the version.
*/
        if(pDllGetVersion)
        {
            DLLVERSIONINFO dvi;
            HRESULT hr;

            ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);

            hr = (*pDllGetVersion)(&dvi);

            if(SUCCEEDED(hr))
            {
                dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
            }
        }
        
        FreeLibrary(hinstDll);
    }
    return dwVersion;
}
#endif

#endif


