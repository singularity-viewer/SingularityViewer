/** 
 * @file legacy.h
 * @brief Declarations of legacy.cpp.
 *
 * Copyright (c) 2011, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   11/05/2011
 *   - Initial version, written by Aleric Inglewood @ SL
 */

#include <map>
#include <string>

#ifdef LL_WINDOWS
#include <windows.h>
#include <WinUser.h>
#include <commdlg.h>
#endif

#include "stdtypes.h"				// BOOL

// Translation map.
typedef std::map<std::string, std::string> translation_map_type;
extern translation_map_type translation_map;

namespace translation
{
	std::string getString(char const* key);
	void add(std::string const& key, std::string const& translation);
}

#if LL_GTK
namespace LLWindowSDL {
	bool ll_try_gtk_init(void);
}
#endif

// A temporary hack to minimize the number of changes from the original llfilepicker.cpp.
#define LLTrans translation

#if LL_DARWIN
#include <Carbon/Carbon.h>

// AssertMacros.h does bad things.
#undef verify
#undef check
#undef require

#include "llstring.h"
#endif

// mostly for Linux, possible on others
#if LL_GTK
# include "gtk/gtk.h"
#endif // LL_GTK

// also mostly for Linux, for some X11-specific filepicker usability tweaks
#if LL_X11
#include "SDL/SDL_syswm.h"
#endif

