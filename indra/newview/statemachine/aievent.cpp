/**
 * @file aievent.cpp
 * @brief Implementation of AIEvent.
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

#include "linden_common.h"
#include "aievent.h"
#include "aistatemachine.h"
#include <map>

// Additional information stored per registered statemachine in AIRegisteredStateMachines std::map.
struct AIRSData {
	bool mOneShot;
	AIRSData(void) { }
	AIRSData(bool one_shot) : mOneShot(one_shot) { }
};

// A list of all statemachines registered for a particular event, and an API to work on it.
struct AIRegisteredStateMachines {
	typedef std::map<AIStateMachine*, AIRSData> rsm_type;
	rsm_type mRegisteredStateMachines;
	void Register(AIStateMachine* statemachine, bool one_shot) { mRegisteredStateMachines[statemachine] = one_shot; }
	void Unregister(AIStateMachine* statemachine) { mRegisteredStateMachines.erase(statemachine); }
	void trigger(void);
};

// Inline this, because it's only called from one place.
inline void AIRegisteredStateMachines::trigger(void)
{
	rsm_type::iterator sm = mRegisteredStateMachines.begin();
	while(sm != mRegisteredStateMachines.end())
	{
		sm->first->cont();				// This is safe, cont() does never access mRegisteredStateMachines.
		if (sm->second.mOneShot)
			mRegisteredStateMachines.erase(sm++);
		else
			++sm;
	}
}

// A list (array) with all AIRegisteredStateMachines maps, one for each event type.
struct AIRegisteredStateMachinesList {
	AIThreadSafeSimple<AIRegisteredStateMachines> mRegisteredStateMachinesList[AIEvent::number_of_events];
	AIRegisteredStateMachinesList(void);
	AIThreadSafeSimple<AIRegisteredStateMachines>& operator[](AIEvent::AIEvents event) { return mRegisteredStateMachinesList[event]; }
};

AIRegisteredStateMachinesList::AIRegisteredStateMachinesList(void)
{
	for (int event = 0; event < AIEvent::number_of_events; ++event)
	{
		new (&mRegisteredStateMachinesList[event]) AIRegisteredStateMachines;
	}
}

// Instantiate the list with all AIRegisteredStateMachines maps.
static AIRegisteredStateMachinesList registered_statemachines_list;

//-----------------------------------------------------------------------------
// External API starts here.
// Each function obtains access to the thread-safe AIRegisteredStateMachines that belongs
// to the given event (locking it's mutex) and then calls the corresponding method.

// Register a statemachine for notification of event.
// static
void AIEvent::Register(AIEvents event, AIStateMachine* statemachine, bool one_shot)
{
	statemachine->idle();
	AIAccess<AIRegisteredStateMachines> registered_statemachines_w(registered_statemachines_list[event]);
	registered_statemachines_w->Register(statemachine, one_shot);
}

// Unregister a statemachine for notification of event.
// static
void AIEvent::Unregister(AIEvents event, AIStateMachine* statemachine)
{
	AIAccess<AIRegisteredStateMachines> registered_statemachines_w(registered_statemachines_list[event]);
	registered_statemachines_w->Unregister(statemachine);
}

// Trigger event.
// static
void AIEvent::trigger(AIEvents event)
{
	AIAccess<AIRegisteredStateMachines> registered_statemachines_w(registered_statemachines_list[event]);
	registered_statemachines_w->trigger();
}

