/**
 * @file aifilepicker.h
 * @brief File picker State machine
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
 *   02/12/2010
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIFILEPICKER_H
#define AIFILEPICKER_H

#include "aistatemachine.h"
#include "llpluginclassmedia.h"
#include "llviewerpluginmanager.h"
#include <vector>

enum ELoadFilter
{
	DF_DIRECTORY,
	FFLOAD_ALL,
	FFLOAD_WAV,
	FFLOAD_IMAGE,
	FFLOAD_ANIM,
	FFLOAD_XML,
	FFLOAD_SLOBJECT,
	FFLOAD_RAW,
	FFLOAD_INVGZ,
	FFLOAD_AO,
	FFLOAD_BLACKLIST
};

enum ESaveFilter
{
	FFSAVE_ALL,
	FFSAVE_WAV,
	FFSAVE_TGA,
	FFSAVE_BMP,
	FFSAVE_AVI,
	FFSAVE_ANIM,
	FFSAVE_XML,
	FFSAVE_COLLADA,
	FFSAVE_RAW,
	FFSAVE_J2C,
	FFSAVE_PNG,
	FFSAVE_JPEG,
	FFSAVE_ANIMATN,
	FFSAVE_OGG,
	FFSAVE_NOTECARD,
	FFSAVE_GESTURE,
	FFSAVE_LSL,
	FFSAVE_SHAPE,
	FFSAVE_SKIN,
	FFSAVE_HAIR,
	FFSAVE_EYES,
	FFSAVE_SHIRT,
	FFSAVE_PANTS,
	FFSAVE_SHOES,
	FFSAVE_SOCKS,
	FFSAVE_JACKET,
	FFSAVE_GLOVES,
	FFSAVE_UNDERSHIRT,
	FFSAVE_UNDERPANTS,
	FFSAVE_SKIRT,
	FFSAVE_INVGZ,
	FFSAVE_LANDMARK,
	FFSAVE_AO,
	FFSAVE_BLACKLIST,
	FFSAVE_PHYSICS
};

/*

  AIFilePicker is an AIStateMachine, that has a LLViewerPluginManager (mPluginManager)
  to manage a plugin that runs the actual filepicker in a separate process.

  The relationship with the plugin is as follows:

new AIFilePicker

  AIFilePicker::mPluginManager = new LLViewerPluginManager
  
                                             LLPluginProcessParentOwner                  LLPluginMessagePipeOwner
                                                     |                                            | LLPluginMessagePipeOwner::mOwner = LLPluginProcessParentOwner
                                                     |                                            | LLPluginMessagePipeOwner::mMessagePipe = LLPluginMessagePipe
                                                     |                                            | LLPluginMessagePipeOwner::receiveMessageRaw calls
                                                     v                                            |   LLPluginProcessParent::receiveMessageRaw
                                             LLPluginClassBasic                                   v
                                                     | LLPluginClassBasic::mPlugin = new LLPluginProcessParent ---> new LLPluginMessagePipe
                                                     |   LLPluginProcessParent::mOwner = (LLPluginProcessParentOwner*)LLPluginClassBasic
                                                     |   LLPluginProcessParent::sendMessage calls
                                                     |     LLPluginMessagePipeOwner::writeMessageRaw calls
                                                     |     mMessagePipe->LLPluginMessagePipe::addMessage
                                                     |   LLPluginProcessParent::receiveMessageRaw calls
                                                     |     LLPluginProcessParent::receiveMessage calls
                                                     |     LLPluginProcessParentOwner::receivePluginMessage == AIPluginFilePicker::receivePluginMessage
                                                     |
                                                     | LLPluginClassBasic::sendMessage calls mPlugin->LLPluginProcessParent::sendMessage
                                                     |
                                                     v
    LLViewerPluginManager::mPluginBase = new AIPluginFilePicker
      AIPluginFilePicker::receivePluginMessage calls
        AIFilePicker::receivePluginMessage

      AIPluginFilePicker::mStateMachine = AIFilePicker

  Where the entry point to send messages to the plugin is LLPluginClassBasic::sendMessage,
  and the end point for messages received from the plugin is AIFilePicker::receivePluginMessage.

  Termination happens by receiving the "canceled" or "done" message,
  which sets the state to AIFilePicker_canceled or AIFilePicker_done
  respectively, causing a call to AIStateMachine::finish(), which calls
  AIFilePicker::finish_impl which destroys the plugin (mPluginBase),
  the plugin manager (mPluginManager) and calls AIStateMachine::kill()
  causing the AIFilePicker to be deleted.

*/

// A file picker state machine.
//
// Before calling run(), call open() to pass needed parameters.
//
// When the state machine finishes, call isCanceled to check
// whether or not getFilename and getFolder will be valid.
//
// Objects of this type can be reused multiple times, see
// also the documentation of AIStateMachine.
class AIFilePicker : public AIStateMachine {
public:
	AIFilePicker(void);

	// Create a dynamically created AIFilePicker object.
	static AIFilePicker* create(bool auto_kill = true) { AIFilePicker* filepicker = new AIFilePicker; filepicker->mAutoKill = auto_kill; return filepicker; }

	// The starting directory that the user will be in when the file picker opens
	// will be the same as the directory used the last time the file picker was
	// opened with the same context. If the file picker was never opened before
	// with the given context, the starting directory will be set to default_path
	// unless that is the empty string, in which case it will be equal to the
	// directory used the last time the filepicker was opened with context "savefile".
	void open(std::string const& filename, ESaveFilter filter = FFSAVE_ALL, std::string const& default_path = "", std::string const& context = "savefile");
	void open(ELoadFilter filter = FFLOAD_ALL, std::string const& default_path = "", std::string const& context = "openfile", bool multiple = false);

	bool isCanceled(void) const { return mCanceled; }
	bool hasFilename(void) const { return *this && !mCanceled; }
	std::string const& getFilename(void) const;
	std::string getFolder(void) const;
	std::vector<std::string> const& getFilenames(void) const { return mFilenames; }

	// Load the sContextMap from disk.
	static bool loadFile(std::string const& filename);
	// Save the sContextMap to disk.
	static bool saveFile(std::string const& filename);

private:
	friend class AIPluginFilePicker;

	// This is called from AIPluginFilePicker::receivePluginMessage, see below.
    void receivePluginMessage(LLPluginMessage const& message);

public:
	enum open_type { save, load, load_multiple };

private:
	LLPointer<LLViewerPluginManager> mPluginManager;				//!< Pointer to the plugin manager.
	typedef std::map<std::string, std::string> context_map_type;	//!< Type of mContextMap.
	static AIThreadSafeSimple<context_map_type> sContextMap;		//!< Map context (ie, "snapshot" or "image") to last used folder.
	std::string mContext;											//!< Some key to indicate the context (remembers the folder per key).
	bool mAutoKill;													//!< True if the default behavior is to delete itself after being finished.

	// Input variables (cache variable between call to open and run).
	open_type mOpenType;					//!< Set to whether opening a filepicker to select for saving one file, for loading one file, or loading multiple files.
	std::string mFilter;					//!< A keyword indicating which file types (extensions) we want to see.
	std::string mFilename;					//!< When saving: proposed filename.
	std::string mFolder;					//!< Initial folder to start in.
	// Output variables:
	bool mCanceled;							//!< True if the file picker was canceled or closed.
	std::vector<std::string> mFilenames;	//!< Filesnames.

	// Store a folder for the given context.
	static void store_folder(std::string const& context, std::string const& folder);
	// Return the last folder stored for 'context', or default_path if none, or context "savefile" if empty, or $HOME if none.
	static std::string get_folder(std::string const& default_path, std::string const& context);

protected:
	// Call finish() (or abort()), not delete.
	/*virtual*/ ~AIFilePicker() { LL_DEBUGS("Plugin") << "Calling AIFilePicker::~AIFilePicker()" << LL_ENDL; }

	// Handle initializing the object.
	/*virtual*/ void initialize_impl(void);

	// Handle mRunState.
	/*virtual*/ void multiplex_impl(void);

	// Handle aborting from current bs_run state.
	/*virtual*/ void abort_impl(void);

	// Handle cleaning up from initialization (or post abort) state.
	/*virtual*/ void finish_impl(void);

	// Implemenation of state_str for run states.
	/*virtual*/ char const* state_str_impl(state_type run_state) const;
};

// Viewer-side helper class for objects with a lifetime equal to the
// plugin child process (SLPlugin). Don't use directly.
class AIPluginFilePicker : public LLPluginClassBasic {
	LOG_CLASS(AIPluginFilePicker);
public:
	AIPluginFilePicker(AIFilePicker* state_machine) : mStateMachine(state_machine) { }
	/*virtual*/ ~AIPluginFilePicker() { LL_DEBUGS("Plugin") << "Calling AIPluginFilePicker::~AIPluginFilePicker()" << LL_ENDL; }

	static std::string launcher_name(void) { return gDirUtilp->getLLPluginLauncher(); }
	static char const* plugin_basename(void) { return "basic_plugin_filepicker"; }

	/*virtual*/ void receivePluginMessage(LLPluginMessage const& message) { mStateMachine->receivePluginMessage(message); }
	/*virtual*/ void receivedShutdown(void) { /* Nothing -- we terminate on deletion (from AIStateMachine::mainloop) */ }

private:
	AIFilePicker* mStateMachine;
};

#endif
