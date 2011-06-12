/** 
 * @file legacy.cpp
 * @brief Helper stubs to keep the picker files as much as possible equal to their original.
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

#include "basic_plugin_base.h"		// For PLS_INFOS etc.
#include "legacy.h"

// Translation map.
translation_map_type translation_map;

namespace translation
{
	std::string getString(char const* key)
	{
		translation_map_type::iterator iter = translation_map.find(key);
		return (iter != translation_map.end()) ? iter->second : key;
	}

	void add(std::string const& key, std::string const& translation)
	{
		PLS_DEBUGS << "Adding translation \"" << key << "\" --> \"" << translation << "\"" << PLS_ENDL;
		translation_map[key] = translation;
	}
}

#if LL_GTK
namespace LLWindowSDL {
	bool ll_try_gtk_init(void)
	{
		static BOOL done_gtk_diag = FALSE;
		static BOOL gtk_is_good = FALSE;
		static BOOL done_setlocale = FALSE;
		static BOOL tried_gtk_init = FALSE;

		if (!done_setlocale)
		{
			PLS_INFOS << "Starting GTK Initialization." << PLS_ENDL;
			//maybe_lock_display();
			gtk_disable_setlocale();
			//maybe_unlock_display();
			done_setlocale = TRUE;
		}

		if (!tried_gtk_init)
		{
			tried_gtk_init = TRUE;
			if (!g_thread_supported ()) g_thread_init (NULL);
			//maybe_lock_display();
			gtk_is_good = gtk_init_check(NULL, NULL);
			//maybe_unlock_display();
			if (!gtk_is_good)
				PLS_WARNS << "GTK Initialization failed." << PLS_ENDL;
		}
		if (gtk_is_good && !done_gtk_diag)
		{
			PLS_INFOS << "GTK Initialized." << PLS_ENDL;
			PLS_INFOS << "- Compiled against GTK version "
				<< GTK_MAJOR_VERSION << "."
				<< GTK_MINOR_VERSION << "."
				<< GTK_MICRO_VERSION << PLS_ENDL;
			PLS_INFOS << "- Running against GTK version "
				<< gtk_major_version << "."
				<< gtk_minor_version << "."
				<< gtk_micro_version << PLS_ENDL;
			//maybe_lock_display();
			const gchar* gtk_warning = gtk_check_version(
				GTK_MAJOR_VERSION,
				GTK_MINOR_VERSION,
				GTK_MICRO_VERSION);
			//maybe_unlock_display();
			if (gtk_warning)
			{
				PLS_WARNS << "- GTK COMPATIBILITY WARNING: " << gtk_warning << PLS_ENDL;
				gtk_is_good = FALSE;
			}
			else
			{
				PLS_INFOS << "- GTK version is good." << PLS_ENDL;
			}
			done_gtk_diag = TRUE;
		}
		return gtk_is_good;
	}
}
#endif

