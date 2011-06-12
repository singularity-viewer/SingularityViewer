/** 
 * @file llthread.cpp
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "llapr.h"

#include "apr_portable.h"

#include "llthread.h"

#include "lltimer.h"

#if LL_LINUX || LL_SOLARIS
#include <sched.h>
#endif

//----------------------------------------------------------------------------
// Usage:
// void run_func(LLThread* thread)
// {
// }
// LLThread* thread = new LLThread();
// thread->run(run_func);
// ...
// thread->setQuitting();
// while(!timeout)
// {
//   if (thread->isStopped())
//   {
//     delete thread;
//     break;
//   }
// }
// 
//----------------------------------------------------------------------------

//
// Handed to the APR thread creation function
//
void *APR_THREAD_FUNC LLThread::staticRun(apr_thread_t *apr_threadp, void *datap)
{
#ifdef CWDEBUG
  	debug::init_thread();
#endif

	LLThread *threadp = (LLThread *)datap;

	// Set thread state to running
	threadp->mStatus = RUNNING;

	// Create a thread local data.
	AIThreadLocalData::create(threadp);

	// Run the user supplied function
	threadp->run();

	llinfos << "LLThread::staticRun() Exiting: " << threadp->mName << llendl;
	
	// We're done with the run function, this thread is done executing now.
	threadp->mStatus = STOPPED;

	return NULL;
}


LLThread::LLThread(std::string const& name) :
	mPaused(FALSE),
	mName(name),
	mAPRThreadp(NULL),
	mStatus(STOPPED),
	mThreadLocalData(NULL)
{
	mRunCondition = new LLCondition;
}


LLThread::~LLThread()
{
	shutdown();
}

void LLThread::shutdown()
{
	// Warning!  If you somehow call the thread destructor from itself,
	// the thread will die in an unclean fashion!
	if (mAPRThreadp)
	{
		if (!isStopped())
		{
			// The thread isn't already stopped
			// First, set the flag that indicates that we're ready to die
			setQuitting();

			llinfos << "LLThread::~LLThread() Killing thread " << mName << " Status: " << mStatus << llendl;
			// Now wait a bit for the thread to exit
			// It's unclear whether I should even bother doing this - this destructor
			// should netver get called unless we're already stopped, really...
			S32 counter = 0;
			const S32 MAX_WAIT = 600;
			while (counter < MAX_WAIT)
			{
				if (isStopped())
				{
					break;
				}
				// Sleep for a tenth of a second
				ms_sleep(100);
				yield();
				counter++;
			}
		}

		if (!isStopped())
		{
			// This thread just wouldn't stop, even though we gave it time
			llwarns << "LLThread::shutdown() exiting thread before clean exit!" << llendl;
			return;
		}
		mAPRThreadp = NULL;
	}

	delete mRunCondition;
}

void LLThread::start()
{
	apr_thread_create(&mAPRThreadp, NULL, staticRun, (void *)this, tldata().mRootPool());

	// We won't bother joining
	apr_thread_detach(mAPRThreadp);
}

//============================================================================
// Called from MAIN THREAD.

// Request that the thread pause/resume.
// The thread will pause when (and if) it calls checkPause()
void LLThread::pause()
{
	if (!mPaused)
	{
		// this will cause the thread to stop execution as soon as checkPause() is called
		mPaused = 1;		// Does not need to be atomic since this is only set/unset from the main thread
	}	
}

void LLThread::unpause()
{
	if (mPaused)
	{
		mPaused = 0;
	}

	wake(); // wake up the thread if necessary
}

// virtual predicate function -- returns true if the thread should wake up, false if it should sleep.
bool LLThread::runCondition(void)
{
	// by default, always run.  Handling of pause/unpause is done regardless of this function's result.
	return true;
}

//============================================================================
// Called from run() (CHILD THREAD).
// Stop thread execution if requested until unpaused.
void LLThread::checkPause()
{
	mRunCondition->lock();

	// This is in a while loop because the pthread API allows for spurious wakeups.
	while(shouldSleep())
	{
		mRunCondition->wait(); // unlocks mRunCondition
		// mRunCondition is locked when the thread wakes up
	}
	
 	mRunCondition->unlock();
}

//============================================================================

void LLThread::setQuitting()
{
	mRunCondition->lock();
	if (mStatus == RUNNING)
	{
		mStatus = QUITTING;
	}
	mRunCondition->unlock();
	wake();
}

// static
U32 LLThread::currentID()
{
	return (U32)apr_os_thread_current();
}

// static
void LLThread::yield()
{
#if LL_LINUX || LL_SOLARIS
	sched_yield(); // annoyingly, apr_thread_yield  is a noop on linux...
#else
	apr_thread_yield();
#endif
}

void LLThread::wake()
{
	mRunCondition->lock();
	if(!shouldSleep())
	{
		mRunCondition->signal();
	}
	mRunCondition->unlock();
}

void LLThread::wakeLocked()
{
	if(!shouldSleep())
	{
		mRunCondition->signal();
	}
}

#ifdef SHOW_ASSERT
// This allows the use of llassert(is_main_thread()) to assure the current thread is the main thread.
static apr_os_thread_t main_thread_id;
LL_COMMON_API bool is_main_thread() { return apr_os_thread_equal(main_thread_id, apr_os_thread_current()); }
#endif

// The thread private handle to access the AIThreadLocalData instance.
apr_threadkey_t* AIThreadLocalData::sThreadLocalDataKey;

//static
void AIThreadLocalData::init(void)
{
	// Only do this once.
	if (sThreadLocalDataKey)
	{
		return;
	}

	apr_status_t status = apr_threadkey_private_create(&sThreadLocalDataKey, &AIThreadLocalData::destroy, AIAPRRootPool::get()());
	ll_apr_assert_status(status);   // Or out of memory, or system-imposed limit on the
	                                // total number of keys per process {PTHREAD_KEYS_MAX}
									// has been exceeded.

	// Create the thread-local data for the main thread (this function is called by the main thread).
	AIThreadLocalData::create(NULL);

#ifdef SHOW_ASSERT
	// This function is called by the main thread.
	main_thread_id = apr_os_thread_current();
#endif
}

// This is called once for every thread when the thread is destructed.
//static
void AIThreadLocalData::destroy(void* thread_local_data)
{
	delete reinterpret_cast<AIThreadLocalData*>(thread_local_data);
}

//static
void AIThreadLocalData::create(LLThread* threadp)
{
	AIThreadLocalData* new_tld = new AIThreadLocalData;
	if (threadp)
	{
		threadp->mThreadLocalData = new_tld;
	}
	apr_status_t status = apr_threadkey_private_set(new_tld, sThreadLocalDataKey);
	llassert_always(status == APR_SUCCESS);
}

//static
AIThreadLocalData& AIThreadLocalData::tldata(void)
{
	if (!sThreadLocalDataKey)
		AIThreadLocalData::init();

	void* data;
	apr_status_t status = apr_threadkey_private_get(&data, sThreadLocalDataKey);
	llassert_always(status == APR_SUCCESS);
	return *static_cast<AIThreadLocalData*>(data);
}

//============================================================================

bool LLMutexBase::isLocked()
{
  	if (!tryLock())
	{
		return true;
	}
	apr_thread_mutex_unlock(mAPRMutexp);
	return false;
}

//============================================================================

LLCondition::LLCondition(AIAPRPool& parent) : LLMutex(parent)
{
	apr_thread_cond_create(&mAPRCondp, mPool());
}

LLCondition::~LLCondition()
{
	apr_thread_cond_destroy(mAPRCondp);
	mAPRCondp = NULL;
}

void LLCondition::wait()
{
	apr_thread_cond_wait(mAPRCondp, mAPRMutexp);
}

void LLCondition::signal()
{
	apr_thread_cond_signal(mAPRCondp);
}

void LLCondition::broadcast()
{
	apr_thread_cond_broadcast(mAPRCondp);
}

//============================================================================

//----------------------------------------------------------------------------

//static
LLMutex* LLThreadSafeRefCount::sMutex = 0;

//static
void LLThreadSafeRefCount::initThreadSafeRefCount()
{
	if (!sMutex)
	{
		sMutex = new LLMutex;
	}
}

//static
void LLThreadSafeRefCount::cleanupThreadSafeRefCount()
{
	delete sMutex;
	sMutex = NULL;
}
	

//----------------------------------------------------------------------------

LLThreadSafeRefCount::LLThreadSafeRefCount() :
	mRef(0)
{
}

LLThreadSafeRefCount::~LLThreadSafeRefCount()
{ 
	if (mRef != 0)
	{
		llerrs << "deleting non-zero reference" << llendl;
	}
}

//============================================================================

LLResponder::~LLResponder()
{
}

//============================================================================
