/**
 * @file aifetchinventoryfolder.h
 * @brief Fetch an inventory folder
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
 *   19/05/2011
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIFETCHINVENTORYFOLDER_H
#define AIFETCHINVENTORYFOLDER_H

#include "aistatemachine.h"
#include "lluuid.h"
#include <map>

// An inventory folder fetch state machine.
//
// Before calling run(), call fetch() to pass needed parameters.
//
// When the state machine finishes, call aborted() to check
// whether or not the statemachine succeeded in fetching
// the folder or not.
//
// Objects of this type can be reused multiple times, see
// also the documentation of AIStateMachine.
class AIFetchInventoryFolder : public AIStateMachine {
  private:
	std::string mFolderName;		//!< Input variable.
	bool mCreate;					//!< Input variable: create mFolderName if it doesn't exist.
	bool mFetchContents;			//!< Input variable: fetch contents before finishing.
	LLUUID mParentFolder;			//!< Input variable: the UUID of the parent folder.
	LLUUID mFolderUUID;				//!< Input and/or output variable.
	bool mExists;					//!< Output variable: true if the folder exists.
	bool mCreated;					//!< Output variable: true if mFolderName didn't exist and was created by this object.

	bool mNeedNotifyObservers;

  public:
	AIFetchInventoryFolder(void) : mCreate(false), mFetchContents(false), mExists(false), mCreated(false)
        { Dout(dc::statemachine, "Calling AIFetchInventoryFolder constructor [" << (void*)this << "]"); }

	/**
	 * @brief Fetch an inventory folder by name, optionally creating it.
	 *
	 * Upon successful finish (aborted() returns false), exists() will return true
	 * if the folder exists; created() will return true if it was created;
	 * UUID() will return the UUID of the folder. It will then also be possible
	 * to scan over all folders (Category) of this folder. If fetch_contents
	 * is set, you will also be able to scan over the contents of the folder
	 * upon successful finish.
	 *
	 * @param parentUUID The UUID of the parent. Passing gInventory.getRootFolderID(), or a null ID, will assume a root folder.
	 * @param foldername The name of the folder.
	 * @param create if set, create the folder if it doesn't exists yet.
	 * @param fetch_contents if set, fetch the contents before finishing.
	 */
	void fetch(LLUUID const& parentUUID, std::string const& foldername, bool create = false, bool fetch_contents = true)
	{
	  mParentFolder = parentUUID;
	  mCreate = create;
	  mFetchContents = fetch_contents;
	  if (mFolderName != foldername)
	  {
		mFolderName = foldername;
		mFolderUUID.setNull();
	  }
	}

	/**
	 * @brief Fetch an inventory folder by name, optionally creating it.
	 *
	 * Upon successful finish (aborted() returns false), exists() will return
	 * true if the folder exists; created() will return true if it was created;
	 * UUID() will return the UUID of the folder. It will then also be possible
	 * to scan over all folders (Category) of this folder. If fetch_contents
	 * is set, you will also be able to scan over the contents of the folder
	 * upon successful finish.
	 *
	 * @param foldername The name of the folder.
	 * @param create if set, create the folder if it doesn't exists yet.
	 * @param fetch_contents if set, fetch the contents before finishing.
	 */
	void fetch(std::string const& foldername, bool create = false, bool fetch_contents = true);

	/**
	 * @brief Fetch an inventory folder by UUID.
	 *
	 * Upon successful finish (aborted() returns false), exists() will return true
	 * if the folder exists; it will then be possible to scan over all folders (Category)
	 * of this folder. If fetch_contents is set, you will also be able to scan over
	 * the contents of the folder upon successful finish.
	 *
	 * @param folderUUID The UUID of the folder.
	 * @param fetch_contents if set, fetch the contents before finishing.
	 */
	void fetch(LLUUID const& folderUUID, bool fetch_contents = true)
	{
	  mFetchContents = fetch_contents;
	  if (mFolderUUID != folderUUID)
	  {
		mFolderName.clear();
		mFolderUUID = folderUUID;
	  }
	}

	std::string const& name(void) const { return mFolderName; }
	bool exists(void) const { return mExists; }
	bool created(void) const { return mCreated; }
	LLUUID const& UUID(void) const { llassert(mExists || mFolderUUID.isNull()); return mFolderUUID; }

  protected:
	// Call finish() (or abort()), not delete.
	/*virtual*/ ~AIFetchInventoryFolder() { Dout(dc::statemachine, "Calling ~AIFetchInventoryFolder() [" << (void*)this << "]"); }

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

#endif
