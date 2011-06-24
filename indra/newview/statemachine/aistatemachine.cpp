/**
 * @file aistatemachine.cpp
 * @brief Implementation of AIStateMachine
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
 *   01/03/2010
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "linden_common.h"

#include <algorithm>

#include "llcallbacklist.h"
#include "llcontrol.h"
#include "llfasttimer.h"
#include "aithreadsafe.h"
#include "aistatemachine.h"

extern LLControlGroup gSavedSettings;

// Local variables.
namespace {
  struct QueueElementComp;

  class QueueElement {
	private:
	  AIStateMachine* mStateMachine;
	  U64 mRuntime;

	public:
	  QueueElement(AIStateMachine* statemachine) : mStateMachine(statemachine), mRuntime(0) { }
	  friend bool operator==(QueueElement const& e1, QueueElement const& e2) { return e1.mStateMachine == e2.mStateMachine; }
	  friend struct QueueElementComp;

	  AIStateMachine& statemachine(void) const { return *mStateMachine; }
	  void add(U64 count) { mRuntime += count; }
  };

  struct QueueElementComp {
	bool operator()(QueueElement const& e1, QueueElement const& e2) const { return e1.mRuntime < e2.mRuntime; }
  };

  typedef std::vector<QueueElement> active_statemachines_type;
  static active_statemachines_type active_statemachines;
  typedef std::vector<AIStateMachine*> continued_statemachines_type;
  struct cscm_type
  {
	continued_statemachines_type continued_statemachines;
	bool calling_mainloop;
  };
  static AITHREADSAFE(cscm_type, continued_statemachines_and_calling_mainloop, );
}

// static
AITHREADSAFESIMPLE(U64, AIStateMachine::sMaxCount, );

void AIStateMachine::updateSettings(void)
{
  Dout(dc::statemachine, "Initializing AIStateMachine::sMaxCount");
  *AIAccess<U64>(sMaxCount) = LLFastTimer::countsPerSecond() * gSavedSettings.getU32("StateMachineMaxTime") / 1000;
}

//----------------------------------------------------------------------------
//
// Public methods
//

void AIStateMachine::run(AIStateMachine* parent, state_type new_parent_state, bool abort_parent)
{
  DoutEntering(dc::statemachine, "AIStateMachine::run(" << (void*)parent << ", " << (parent ? parent->state_str(new_parent_state) : "NA") << ", " << abort_parent << ") [" << (void*)this << "]");
  // Must be the first time we're being run.
  llassert(!mParent);
  llassert(!mCallback);
  // Can only be run when in this state.
  llassert(mState == bs_initialize);
  // If a parent is provided, it must be running.
  llassert(!parent || parent->mState == bs_run);

  mParent = parent;
  mNewParentState = new_parent_state;
  mAbortParent = abort_parent;

  cont();
}

void AIStateMachine::run(callback_type::signal_type::slot_type const& slot)
{
  DoutEntering(dc::statemachine, "AIStateMachine::run(<slot>) [" << (void*)this << "]");
  // Must be the first time we're being run.
  llassert(!mParent);
  llassert(!mCallback);
  // Can only be run when in this state.
  llassert(mState == bs_initialize);

  mCallback = new callback_type(slot);

  cont(); 
}

void AIStateMachine::idle(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::idle() [" << (void*)this << "]");
  llassert(!mIdle);
  mIdle = true;
  mSleep = 0;
}

void AIStateMachine::cont(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::cont() [" << (void*)this << "]");
  llassert(mIdle);
  mIdle = false;
  if (mQueued)
	return;
  AIWriteAccess<cscm_type> cscm_w(continued_statemachines_and_calling_mainloop);
  cscm_w->continued_statemachines.push_back(this);
  if (!cscm_w->calling_mainloop)
  {
	Dout(dc::statemachine, "Adding AIStateMachine::mainloop to gIdleCallbacks");
	cscm_w->calling_mainloop = true;
	gIdleCallbacks.addFunction(&AIStateMachine::mainloop);
  }
  mQueued = true;
}

void AIStateMachine::set_state(state_type state)
{
  DoutEntering(dc::statemachine, "AIStateMachine::set_state(" << state_str(state) << ") [" << (void*)this << "]");
  llassert(mState == bs_run);
  if (mRunState != state)
  {
	mRunState = state;
	Dout(dc::statemachine, "mRunState set to " << state_str(mRunState));
  }
  if (mIdle)
	cont();
}

void AIStateMachine::abort(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::abort() [" << (void*)this << "]");
  llassert(mState == bs_run);
  mState = bs_abort;
  abort_impl();
  mAborted = true;
  finish();
}

void AIStateMachine::finish(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::finish() [" << (void*)this << "]");
  llassert(mState == bs_run || mState == bs_abort);
  // It is possible that mIdle is false when abort or finish was called from
  // outside multiplex_impl. However, that only may be done by the main thread.
  llassert(!mIdle || is_main_thread());
  if (!mIdle)
	idle();
  mState = bs_finish;
  finish_impl();
  // Did finish_impl call kill()? Then that is only the default. Remember it.
  bool default_delete = (mState == bs_killed);
  mState = bs_finish;
  if (mParent)
  {
	// It is possible that the parent is not running when the parent is in fact aborting and called
	// abort on this object from it's abort_impl function. It that case we don't want to recursively
	// call abort again (or change it's state).
	if (mParent->running())
	{
	  if (mAborted && mAbortParent)
	  {
		mParent->abort();
	  }
	  else
	  {
		mParent->set_state(mNewParentState);
	  }
	}
	mParent = NULL;
  }
  // Set this already to bs_initialize now, so that (bool)*this evaluates to true.
  mState = bs_initialize;
  if (mCallback)
  {
	mCallback->callback(!mAborted);			// This can/may call kill(), in which case the whole AIStateMachine will be deleted from the mainloop.
	delete mCallback;
	mCallback = NULL;
  }
  // Restore the request for deletion if we weren't started again from the callback.
  if (default_delete && mState == bs_initialize)
	mState = bs_killed;
}

void AIStateMachine::kill(void)
{
  // Should only be called from finish().
  llassert(mIdle && (mState == bs_initialize || mState == bs_finish));
  if (mState == bs_initialize)
  {
	// Bump the statemachine onto the active statemachine list, or else it won't be deleted.
	cont();
	idle();
  }
  mState = bs_killed;
}

// Return stringified 'state'.
char const* AIStateMachine::state_str(state_type state)
{
  if (state >= min_state && state < max_state)
  {
	switch (state)
	{
	  AI_CASE_RETURN(bs_initialize);
	  AI_CASE_RETURN(bs_run);
	  AI_CASE_RETURN(bs_abort);
	  AI_CASE_RETURN(bs_finish);
	  AI_CASE_RETURN(bs_killed);
	}
  }
  return state_str_impl(state);
}

//----------------------------------------------------------------------------
//
// Private methods
//

void AIStateMachine::multiplex(U64 current_time)
{
  // Return immediately when this state machine is sleeping.
  // A negative value of mSleep means we're counting frames,
  // a positive value means we're waiting till a certain
  // amount of time has passed.
  if (mSleep != 0)
  {
	if (mSleep < 0)
	{
	  if (++mSleep)
		return;
	}
	else
	{
	  if (current_time < (U64)mSleep)
		return;
	  mSleep = 0;
	}
  }

  DoutEntering(dc::statemachine, "AIStateMachine::multiplex() [" << (void*)this << "] [with state: " << state_str(mState == bs_run ? mRunState : mState) << "]");
  llassert(mState == bs_initialize || mState == bs_run);

  // Real state machine starts here.
  if (mState == bs_initialize)
  {
	mAborted = false;
	mState = bs_run;
	initialize_impl();
	if (mAborted || mState != bs_run)
	  return;
  }
  multiplex_impl();
}

// static
void AIStateMachine::mainloop(void*)
{
  LLFastTimer t(LLFastTimer::FTM_STATEMACHINE);
  // Add continued state machines.
  {
	AIReadAccess<cscm_type> cscm_r(continued_statemachines_and_calling_mainloop);
	bool nonempty = false;
	for (continued_statemachines_type::const_iterator iter = cscm_r->continued_statemachines.begin(); iter != cscm_r->continued_statemachines.end(); ++iter)
	{
	  nonempty = true;
	  active_statemachines.push_back(QueueElement(*iter));
	  Dout(dc::statemachine, "Adding " << (void*)*iter << " to active_statemachines");
	  (*iter)->mQueued = false;
	}
	if (nonempty)
	  AIWriteAccess<cscm_type>(cscm_r)->continued_statemachines.clear();
  }
  llassert(!active_statemachines.empty());
  // Run one or more state machines.
  U64 total_clocks = 0;
  U64 max_count = *AIAccess<U64>(sMaxCount);
  for (active_statemachines_type::iterator iter = active_statemachines.begin(); iter != active_statemachines.end(); ++iter)
  {
	AIStateMachine& statemachine(iter->statemachine());
	if (!statemachine.mIdle)
	{
	  U64 start = LLFastTimer::getCPUClockCount64();
	  iter->statemachine().multiplex(start);
	  U64 delta = LLFastTimer::getCPUClockCount64() - start;
	  iter->add(delta);
	  total_clocks += delta;
	  if (total_clocks >= max_count)
	  {
#ifndef LL_RELEASE_FOR_DOWNLOAD
		llwarns << "AIStateMachine::mainloop did run for " << (total_clocks * 1000 / LLFastTimer::countsPerSecond()) << " ms." << llendl;
#endif
		std::sort(active_statemachines.begin(), active_statemachines.end(), QueueElementComp());
		break;
	  }
	}
  }
  // Remove idle state machines from the loop.
  active_statemachines_type::iterator iter = active_statemachines.begin();
  while (iter != active_statemachines.end())
  {
	AIStateMachine& statemachine(iter->statemachine());
	if (statemachine.mIdle)
	{
	  Dout(dc::statemachine, "Erasing " << (void*)&statemachine << " from active_statemachines");
	  iter = active_statemachines.erase(iter);
	  if (statemachine.mState == bs_killed)
	  {
	  	Dout(dc::statemachine, "Deleting " << (void*)&statemachine);
		delete &statemachine;
	  }
	}
	else
	{
	  llassert(statemachine.mState == bs_run || statemachine.mState == bs_initialize);
	  ++iter;
	}
  }
  if (active_statemachines.empty())
  {
	// If this was the last state machine, remove mainloop from the IdleCallbacks.
	AIReadAccess<cscm_type> cscm_r(continued_statemachines_and_calling_mainloop);
	if (cscm_r->continued_statemachines.empty() && cscm_r->calling_mainloop)
	{
	  Dout(dc::statemachine, "Removing AIStateMachine::mainloop from gIdleCallbacks");
	  AIWriteAccess<cscm_type>(cscm_r)->calling_mainloop = false;
	  gIdleCallbacks.deleteFunction(&AIStateMachine::mainloop);
	}
  }
}
