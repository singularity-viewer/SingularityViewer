/**
 * @file aievent.h
 * @brief Collect viewer events for statemachines.
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

#ifndef AIEVENT_H
#define AIEVENT_H

class AIStateMachine;

class AIEvent {
  public:
	enum AIEvents {
		LLInventoryModel_mIsAgentInvUsable_true,		// LLInventoryModel::mIsAgentInvUsable was set to true.
		number_of_events
	};

	/**
	 * Register a statemachine for the given event.
	 *
	 * If the event happens then statemachine->cont() is called.
	 * 
	 * @param event the event that we want to be notified of.
	 * @param statemachine the statemachine.
	 * @param one_shot if true then AIEvent::Unregister is called automatically when the event occured once.
	 */
	static void Register(AIEvents event, AIStateMachine* statemachine, bool one_shot = true);

	/**
	 * Unregister a previously registered event.
	 *
	 * This may be called for already unregistered events.
	 * This should be called from the destructor of a statemachine for any event it registers,
	 * as well as when it doesn't need the event anymore (in the case of non- one shot events).
	 *
	 * @param event the event we want to no longer be notified off.
	 * @param statemachine the statemachine.
	 */
	static void Unregister(AIEvents event, AIStateMachine* statemachine);

	/**
	 * Trigger the event.
	 *
	 * This is called by the viewer code when the event happens and
	 * causes statemachine->cont() to be called for every registered
	 * statemachine.
	 *
	 * @param event the event that just happened.
	 */
	static void trigger(AIEvents event);
};

#endif
