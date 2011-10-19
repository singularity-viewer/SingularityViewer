/**
 * @file aifilepicker.cpp
 * @brief Implementation of AIFilePicker
 *
 * Copyright (c) 2010, Aleric Inglewood.
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
 *   04/12/2010
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "linden_common.h"
#include "lltrans.h"
#include "llpluginclassmedia.h"
#include "llpluginmessageclasses.h"
#include "llsdserialize.h"
#include "aifilepicker.h"
#if LL_WINDOWS
#include "llviewerwindow.h"
#endif
#if LL_GTK && LL_X11
#include "llwindowsdl.h"
#endif

enum filepicker_state_type {
	AIFilePicker_initialize_plugin = AIStateMachine::max_state,
	AIFilePicker_plugin_running,
	AIFilePicker_canceled,
	AIFilePicker_done
};

char const* AIFilePicker::state_str_impl(state_type run_state) const
{
	switch(run_state)
	{
		AI_CASE_RETURN(AIFilePicker_initialize_plugin);
		AI_CASE_RETURN(AIFilePicker_plugin_running);
		AI_CASE_RETURN(AIFilePicker_canceled);
		AI_CASE_RETURN(AIFilePicker_done);
	}
	return "UNKNOWN STATE";
}

AIFilePicker::AIFilePicker(void) : mPluginManager(NULL), mAutoKill(false), mCanceled(false)
{
}

// static
AITHREADSAFESIMPLE(AIFilePicker::context_map_type, AIFilePicker::sContextMap, );

// static
void AIFilePicker::store_folder(std::string const& context, std::string const& folder)
{
	if (!folder.empty())
	{
		LL_DEBUGS("Plugin") << "AIFilePicker::mContextMap[\"" << context << "\"] = \"" << folder << '"' << LL_ENDL;
		AIAccess<context_map_type>(sContextMap)->operator[](context) = folder;
	}
}

// static
std::string AIFilePicker::get_folder(std::string const& default_path, std::string const& context)
{
	AIAccess<context_map_type> wContextMap(sContextMap);
	context_map_type::iterator iter = wContextMap->find(context);
	if (iter != wContextMap->end() && !iter->second.empty())
	{
		return iter->second;
	}
	else if (!default_path.empty())
	{
		LL_DEBUGS("Plugin") << "context \"" << context << "\" not found. Returning default_path \"" << default_path << "\"." << LL_ENDL;
		return default_path;
	}
	else if ((iter = wContextMap->find("savefile")) != wContextMap->end() && !iter->second.empty())
	{
		LL_DEBUGS("Plugin") << "context \"" << context << "\" not found and default_path empty. Returning context \"savefile\": \"" << iter->second << "\"." << LL_ENDL;
		return iter->second;
	}
	else
	{
		// This is the last resort when all else failed. Open the file chooser in directory 'home'.
#if LL_WINDOWS
		char const* envname = "USERPROFILE";
#else
		char const* envname = "HOME";
#endif
		char const* home = getenv(envname);
		if (!home || home[0] == '\0')
		{
#if LL_WINDOWS
			// On windows, the filepicker window won't pop up at all if we pass an empty string.
			home = "C:\\";
#else
			home = "/";
#endif
			LL_DEBUGS("Plugin") << "context \"" << context << "\" not found, default_path empty, context \"savefile\" not found "
			  "and environment variable \"$" << envname << "\" empty! Returning \"" << home << "\"." << LL_ENDL;
		}
		else
		{
			LL_DEBUGS("Plugin") << "context \"" << context << "\" not found, default_path empty and context \"savefile\" not found. "
			  "Returning environment variable \"$" << envname << "\" (" << home << ")." << LL_ENDL;
		}
		return home;
	}
}

void AIFilePicker::open(ELoadFilter filter, std::string const& default_path, std::string const& context, bool multiple)
{
	mContext = context;
	mFolder = AIFilePicker::get_folder(default_path, context);
	mOpenType = multiple ? load_multiple : load;
	switch(filter)
	{
	  case DF_DIRECTORY:
		  mFilter = "directory";
		  break;
	  case FFLOAD_ALL:
		  mFilter = "all";
		  break;
	  case FFLOAD_WAV:
		  mFilter = "wav";
		  break;
	  case FFLOAD_IMAGE:
		  mFilter = "image";
		  break;
	  case FFLOAD_ANIM:
		  mFilter = "anim";
		  break;
#ifdef _CORY_TESTING
	  case FFLOAD_GEOMETRY:
		  mFilter = "geometry";
		  break;
#endif
	  case FFLOAD_XML:
		  mFilter = "xml";
		  break;
	  case FFLOAD_SLOBJECT:
		  mFilter = "slobject";
		  break;
	  case FFLOAD_RAW:
		  mFilter = "raw";
		  break;
	  case FFLOAD_INVGZ:
		  mFilter = "invgz";
		  break;
	  case FFLOAD_AO:
		  mFilter = "ao";
		  break;
	  case FFLOAD_BLACKLIST:
		  mFilter = "blacklist";
		  break;
	}
}

void AIFilePicker::open(std::string const& filename, ESaveFilter filter, std::string const& default_path, std::string const& context)
{
	mFilename = filename;
	mContext = context;
	mFolder = AIFilePicker::get_folder(default_path, context);
	mOpenType = save;
	switch(filter)
	{
		case FFSAVE_ALL:
			mFilter = "all";
			break;
		case FFSAVE_WAV:
			mFilter = "wav";
			break;
		case FFSAVE_TGA:
			mFilter = "tga";
			break;
		case FFSAVE_BMP:
			mFilter = "bmp";
			break;
		case FFSAVE_AVI:
			mFilter = "avi";
			break;
		case FFSAVE_ANIM:
			mFilter = "anim";
			break;
#ifdef _CORY_TESTING
		case FFSAVE_GEOMETRY:
			mFilter = "geometry";
			break;
#endif
		case FFSAVE_XML:
			mFilter = "xml";
			break;
		case FFSAVE_COLLADA:
			mFilter = "collada";
			break;
		case FFSAVE_RAW:
			mFilter = "raw";
			break;
		case FFSAVE_J2C:
			mFilter = "j2c";
			break;
		case FFSAVE_PNG:
			mFilter = "png";
			break;
		case FFSAVE_JPEG:
			mFilter = "jpeg";
			break;
		case FFSAVE_ANIMATN:
			mFilter = "animatn";
			break;
		case FFSAVE_OGG:
			mFilter = "ogg";
			break;
		case FFSAVE_NOTECARD:
			mFilter = "notecard";
			break;
		case FFSAVE_GESTURE:
			mFilter = "gesture";
			break;
		case FFSAVE_LSL:
			mFilter = "lsl";
			break;
		case FFSAVE_SHAPE:
			mFilter = "shape";
			break;
		case FFSAVE_SKIN:
			mFilter = "skin";
			break;
		case FFSAVE_HAIR:
			mFilter = "hair";
			break;
		case FFSAVE_EYES:
			mFilter = "eyes";
			break;
		case FFSAVE_SHIRT:
			mFilter = "shirt";
			break;
		case FFSAVE_PANTS:
			mFilter = "pants";
			break;
		case FFSAVE_SHOES:
			mFilter = "shoes";
			break;
		case FFSAVE_SOCKS:
			mFilter = "socks";
			break;
		case FFSAVE_JACKET:
			mFilter = "jacket";
			break;
		case FFSAVE_GLOVES:
			mFilter = "gloves";
			break;
		case FFSAVE_UNDERSHIRT:
			mFilter = "undershirt";
			break;
		case FFSAVE_UNDERPANTS:
			mFilter = "underpants";
			break;
		case FFSAVE_SKIRT:
			mFilter = "skirt";
			break;
		case FFSAVE_INVGZ:
			mFilter = "invgz";
			break;
		case FFSAVE_LANDMARK:
			mFilter = "landmark";
			break;
		case FFSAVE_AO:
			mFilter = "ao";
			break;
		case FFSAVE_BLACKLIST:
			mFilter = "blacklist";
			break;
		case FFSAVE_PHYSICS:
			mFilter = "physics";
			break;
	}
}

void AIFilePicker::initialize_impl(void)
{
	mCanceled = false;
	if (mFilter.empty())
	{
		llwarns << "Calling AIFilePicker::initialize_impl() with empty mFilter. Call open before calling run!" << llendl;
		abort();
		return;
	}
	mPluginManager = new LLViewerPluginManager;
	if (!mPluginManager)
	{
		abort();
		return;
	}
	LLPluginClassBasic* plugin = mPluginManager->createPlugin<AIPluginFilePicker>(this);
	if (!plugin)
	{
		abort();
		return;
	}
	set_state(AIFilePicker_initialize_plugin);
}

void AIFilePicker::multiplex_impl(void)
{
	mPluginManager->update();										// Give the plugin some CPU for it's messages.
	LLPluginClassBasic* plugin = mPluginManager->getPlugin();
	if (!plugin || plugin->isPluginExited())
	{
		// This happens when there was a problem with the plugin (ie, it crashed).
		abort();
		return;
	}
	switch (mRunState)
	{
		case AIFilePicker_initialize_plugin:
		{
			if (!plugin->isPluginRunning())
			{
				break;												// Still initializing.
			}

			// Send initialization message.
			LLPluginMessage initialization_message(LLPLUGIN_MESSAGE_CLASS_BASIC, "initialization");
			static char const* key_str[] = {
				"all_files", "sound_files", "animation_files", "image_files", "save_file_verb",
				"targa_image_files", "bitmap_image_files", "avi_movie_file", "xaf_animation_file",
				"xml_file", "raw_file", "compressed_image_files", "load_file_verb", "load_files",
				"choose_the_directory"
			};
			LLSD dictionary;
			for (int key = 0; key < sizeof(key_str) / sizeof(key_str[0]); ++key)
			{
				dictionary[key_str[key]] = LLTrans::getString(key_str[key]);
			}
			initialization_message.setValueLLSD("dictionary", dictionary);
#if LL_WINDOWS || (LL_GTK && LL_X11)
			std::ostringstream window_id_str;
#if LL_WINDOWS
			unsigned long window_id = (unsigned long)gViewerWindow->getPlatformWindow();
#else
			unsigned long window_id = LLWindowSDL::get_SDL_XWindowID();
#endif
			if (window_id != 0)
			{
				window_id_str << std::hex << "0x" << window_id;
				initialization_message.setValue("window_id", window_id_str.str());
			}
			else
			{
				LL_WARNS("Plugin") << "Couldn't get xwid to use for transient." << LL_ENDL;
			}
#endif // LL_WINDOWS || (LL_GTK && LL_X11)
			plugin->sendMessage(initialization_message);

			// Send open message.
			LLPluginMessage open_message(LLPLUGIN_MESSAGE_CLASS_BASIC, "open");
			open_message.setValue("type", (mOpenType == save) ? "save" : (mOpenType == load) ? "load" : "load_multiple");
			open_message.setValue("filter", mFilter);
			if (mOpenType == save) open_message.setValue("default", mFilename);
			open_message.setValue("folder", mFolder);
			open_message.setValue("gorgon", "block");		// Don't expect HeartBeat messages after an "open".
			plugin->sendMessage(open_message);

			set_state(AIFilePicker_plugin_running);
			break;
		}
		case AIFilePicker_plugin_running:
		{
			// Users are slow, no need to look for new messages from the plugin all too often.
			yield_ms(250);
			break;
		}
		case AIFilePicker_canceled:
		{
			mCanceled = true;
			finish();
			break;
		}
		case AIFilePicker_done:
		{
			// Store folder of first filename as context.
			AIFilePicker::store_folder(mContext, getFolder());
			finish();
		}
	}
}

void AIFilePicker::abort_impl(void)
{
}

void AIFilePicker::finish_impl(void)
{
	if (mPluginManager)
	{
		mPluginManager->destroyPlugin();
		mPluginManager = NULL;
	}
	mFilter.clear();		// Check that open is called before calling run (again).
	if (mAutoKill)
	{
		// The default behavior is to delete the plugin. This can be overridden in
		// the callback by calling run() again.
		kill();
	}
}

// This function is called when a new message is received from the plugin.
// We get here when calling mPluginManager->update() in the first line of
// AIFilePicker::multiplex_impl.
//
// Note that we can't call finish() or abort() directly in this function,
// as that deletes mPluginManager and we're using the plugin manager
// right now (to receive this message)!
void AIFilePicker::receivePluginMessage(const LLPluginMessage &message)
{
	std::string message_class = message.getClass();

	if (message_class == LLPLUGIN_MESSAGE_CLASS_BASIC)
	{
		std::string message_name = message.getName();
		if (message_name == "canceled")
		{
			LL_DEBUGS("Plugin") << "received message \"canceled\"" << LL_ENDL;
			set_state(AIFilePicker_canceled);
		}
		else if (message_name == "done")
		{
			LL_DEBUGS("Plugin") << "received message \"done\"" << LL_ENDL;
			LLSD filenames = message.getValueLLSD("filenames");
			mFilenames.clear();
			for(LLSD::array_iterator filename = filenames.beginArray(); filename != filenames.endArray(); ++filename)
			{
				mFilenames.push_back(*filename);
			}
			set_state(AIFilePicker_done);
		}
		else
		{
			LL_WARNS("Plugin") << "Unknown " << message_class << " class message: " << message_name << LL_ENDL;
		}
	}
}

std::string const& AIFilePicker::getFilename(void) const
{
	// Only call this function after the AIFilePicker finished successfully without being canceled.
	llassert_always(!mFilenames.empty());
	// This function returns the first filename in the case that more than one was selected.
	return mFilenames[0];
}

std::string AIFilePicker::getFolder(void) const
{
	// Return the folder of the first filename.
	return gDirUtilp->getDirName(getFilename());
}

// static
bool AIFilePicker::loadFile(std::string const& filename)
{
	LLSD data;
	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename);
	llifstream file(filepath);
	if (file.is_open())
	{
		llinfos << "Loading filepicker context file at \"" << filepath << "\"." << llendl;
		LLSDSerialize::fromXML(data, file);
	}

	if (!data.isMap())
	{
		llinfos << "File missing, ill-formed, or simply undefined; not changing the file (" << filepath << ")." << llendl;
		return false;
	}

	AIAccess<context_map_type> wContextMap(sContextMap);

	for (LLSD::map_const_iterator iter = data.beginMap(); iter != data.endMap(); ++iter)
	{
		wContextMap->insert(context_map_type::value_type(iter->first, iter->second.asString()));
	}

	return true;
}

// static
bool AIFilePicker::saveFile(std::string const& filename)
{
	AIAccess<context_map_type> wContextMap(sContextMap);
	if (wContextMap->empty())
		return false;

	LLSD context;

	for (context_map_type::iterator iter = wContextMap->begin(); iter != wContextMap->end(); ++iter)
	{
		context[iter->first] = iter->second;
	}

	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename);
	llofstream file;
	file.open(filepath.c_str());
	if (!file.is_open()) 
	{
		llwarns << "Unable to open filepicker context file for save: \"" << filepath << "\"." << llendl;
		return false;
	}

	LLSDSerialize::toPrettyXML(context, file);

	file.close();
	llinfos << "Saved default paths to \"" << filepath << "\"." << llendl;

	return true;
}

