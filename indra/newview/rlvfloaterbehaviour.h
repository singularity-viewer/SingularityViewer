/** 
 *
 * Copyright (c) 2009-2010, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU General Public License, version 2.0, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the GPL can be found in doc/GPL-license.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/gpl-2.0.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#ifndef RLV_FLOATER_BEHAVIOUR
#define RLV_FLOATER_BEHAVIOUR

#include "llfloater.h"
#include "rlvcommon.h"

// ============================================================================

class RlvFloaterBehaviour :
	public LLFloater,
	public LLFloaterSingleton<RlvFloaterBehaviour>,
	public RlvBehaviourObserver
{
	friend class LLUISingleton<RlvFloaterBehaviour, VisibilityPolicy<LLFloater> >;
public:
	virtual ~RlvFloaterBehaviour() {}

	/*
	 * LLFloater overrides
	 */
public:
	/*virtual*/ BOOL canClose();
	/*virtual*/ void onOpen();
	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ BOOL postBuild();

	/*
	 * RlvBehaviourObserver overrides
	 */
public:
	/*virtual*/ void changed(const RlvCommand& rlvCmd, bool fInternal);

	/*
	 * Member functions
	 */
public:
	static void show(void*);
	void onAvatarNameLookup(const LLUUID& uuid);
protected:
	void refreshAll();
private:
	RlvFloaterBehaviour(const LLSD& key = LLSD());

public:
	std::map<LLUUID,boost::signals2::connection> m_PendingLookup;	//Have to hold a signal so we can remove it later.
};

// ============================================================================

#endif // RLV_FLOATER_BEHAVIOUR
