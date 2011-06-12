/**
 * @file aidirpicker.h
 * @brief Directory picker State machine
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
 *   10/05/2011
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIDIRPICKER_H
#define AIDIRPICKER_H

#include "aifilepicker.h"

// A directory picker state machine.
//
// Before calling run(), call open() to pass needed parameters.
//
// When the state machine finishes, call hasDirname to check
// whether or not getDirname will be valid.
//
// Objects of this type can be reused multiple times, see
// also the documentation of AIStateMachine.
class AIDirPicker : protected AIFilePicker {
public:
	// Allow to pass the arguments to open upon creation.
	//
	// The starting directory that the user will be in when the directory picker opens
	// will be the same as the directory used the last time the directory picker was
	// opened with the same context. If the directory picker was never opened before
	// with the given context, the starting directory will be set to default_path
	// unless that is the empty string, in which case it will be equal to the
	// directory used the last time the file- or dirpicker was opened with context
	// "openfile".
	AIDirPicker(std::string const& default_path = "", std::string const& context = "openfile") { open(default_path, context); }

	// This should only be called if you want to re-use the AIDirPicker after it finished
	// (from the callback function, followed by a call to 'run'). Normally it is not used.
	void open(std::string const& default_path = "", std::string const& context = "openfile") { AIFilePicker::open(DF_DIRECTORY, default_path, context); }

	bool hasDirname(void) const { return hasFilename(); }
	std::string const& getDirname(void) const { return getFilename(); }

public:
	// Basically all public members of AIStateMachine could made accessible here,
	// but I don't think others will ever be needed (not even these, actually).
	using AIStateMachine::state_type;
	using AIFilePicker::isCanceled;
	using AIStateMachine::run;

protected:
	// Call finish() (or abort()), not delete.
	/*virtual*/ ~AIDirPicker() { LL_DEBUGS("Plugin") << "Calling AIDirPicker::~AIDirPicker()" << LL_ENDL; }
};

#endif
