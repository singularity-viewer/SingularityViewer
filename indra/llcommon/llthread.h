/** 
 * @file llthread.h
 * @brief Base classes for thread, mutex and condition handling.
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

#ifndef LL_LLTHREAD_H
#define LL_LLTHREAD_H

#include "llapr.h"
#include "llapp.h"
#include "llmemory.h"

#include "apr_thread_cond.h"
#include "aiaprpool.h"

#ifdef SHOW_ASSERT
extern LL_COMMON_API bool is_main_thread(void);
#endif

class LLThread;
class LLMutex;
class LLCondition;

class LL_COMMON_API AIThreadLocalData
{
private:
	static apr_threadkey_t* sThreadLocalDataKey;

public:
	// Thread-local memory pool.
	AIAPRRootPool mRootPool;
	AIVolatileAPRPool mVolatileAPRPool;

	static void init(void);
	static void destroy(void* thread_local_data);
	static void create(LLThread* pthread);
	static AIThreadLocalData& tldata(void);
};

class LL_COMMON_API LLThread
{
public:
	typedef enum e_thread_status
	{
		STOPPED = 0,	// The thread is not running.  Not started, or has exited its run function
		RUNNING = 1,	// The thread is currently running
		QUITTING= 2 	// Someone wants this thread to quit
	} EThreadStatus;

	LLThread(std::string const& name);
	virtual ~LLThread(); // Warning!  You almost NEVER want to destroy a thread unless it's in the STOPPED state.
	virtual void shutdown(); // stops the thread
	
	bool isQuitting() const { return (QUITTING == mStatus); }
	bool isStopped() const { return (STOPPED == mStatus); }
	
	static U32 currentID(); // Return ID of current thread
	static void yield(); // Static because it can be called by the main thread, which doesn't have an LLThread data structure.
	
public:
	// PAUSE / RESUME functionality. See source code for important usage notes.
	// Called from MAIN THREAD.
	void pause();
	void unpause();
	bool isPaused() { return isStopped() || mPaused == TRUE; }
	
	// Cause the thread to wake up and check its condition
	void wake();

	// Same as above, but to be used when the condition is already locked.
	void wakeLocked();

	// Called from run() (CHILD THREAD). Pause the thread if requested until unpaused.
	void checkPause();

	// this kicks off the apr thread
	void start(void);

	// Return thread-local data for the current thread.
	static AIThreadLocalData& tldata(void) { return AIThreadLocalData::tldata(); }

private:
	BOOL				mPaused;
	
	// static function passed to APR thread creation routine
	static void *APR_THREAD_FUNC staticRun(apr_thread_t *apr_threadp, void *datap);

protected:
	std::string			mName;
	LLCondition*		mRunCondition;

	apr_thread_t		*mAPRThreadp;
	EThreadStatus		mStatus;

	friend void AIThreadLocalData::create(LLThread* threadp);
	AIThreadLocalData*  mThreadLocalData;

	void setQuitting();
	
	// virtual function overridden by subclass -- this will be called when the thread runs
	virtual void run(void) = 0; 
	
	// virtual predicate function -- returns true if the thread should wake up, false if it should sleep.
	virtual bool runCondition(void);

	// Lock/Unlock Run Condition -- use around modification of any variable used in runCondition()
	inline void lockData();
	inline void unlockData();
	
	// This is the predicate that decides whether the thread should sleep.  
	// It should only be called with mRunCondition locked, since the virtual runCondition() function may need to access
	// data structures that are thread-unsafe.
	bool shouldSleep(void) { return (mStatus == RUNNING) && (isPaused() || (!runCondition())); }

	// To avoid spurious signals (and the associated context switches) when the condition may or may not have changed, you can do the following:
	// mRunCondition->lock();
	// if(!shouldSleep())
	//     mRunCondition->signal();
	// mRunCondition->unlock();
};

//============================================================================

class LL_COMMON_API LLMutexBase
{
public:
	void lock() { apr_thread_mutex_lock(mAPRMutexp); }
	void unlock() { apr_thread_mutex_unlock(mAPRMutexp); }
	// Returns true if lock was obtained successfully.
	bool tryLock() { return !APR_STATUS_IS_EBUSY(apr_thread_mutex_trylock(mAPRMutexp)); }

	bool isLocked(); 	// non-blocking, but does do a lock/unlock so not free

protected:
	// mAPRMutexp is initialized and uninitialized in the derived class.
	apr_thread_mutex_t* mAPRMutexp;
};

class LL_COMMON_API LLMutex : public LLMutexBase
{
public:
	LLMutex(AIAPRPool& parent = LLThread::tldata().mRootPool) : mPool(parent)
	{
		apr_thread_mutex_create(&mAPRMutexp, APR_THREAD_MUTEX_UNNESTED, mPool());
	}
	~LLMutex()
	{
		llassert(!isLocked()); // better not be locked!
		apr_thread_mutex_destroy(mAPRMutexp);
		mAPRMutexp = NULL;
	}

protected:
	AIAPRPool mPool;
private:
	// Disable copy construction, as si teh bomb!!! -SG
	LLMutex(const LLMutex&);
	LLMutex& operator=(const LLMutex&);
};

#if APR_HAS_THREADS
// No need to use a root pool in this case.
typedef LLMutex LLMutexRootPool;
#else  // APR_HAS_THREADS
class LL_COMMON_API LLMutexRootPool : public LLMutexBase
{
public:
	LLMutexRootPool(void)
	{
		apr_thread_mutex_create(&mAPRMutexp, APR_THREAD_MUTEX_UNNESTED, mRootPool());
	}
	~LLMutexRootPool()
	{
#if APR_POOL_DEBUG
		// It is allowed to destruct root pools from a different thread.
		mRootPool.grab_ownership();
#endif
		llassert(!isLocked()); // better not be locked!
		apr_thread_mutex_destroy(mAPRMutexp);
		mAPRMutexp = NULL;
	}

protected:
	AIAPRRootPool mRootPool;
};
#endif // APR_HAS_THREADS

// Actually a condition/mutex pair (since each condition needs to be associated with a mutex).
class LL_COMMON_API LLCondition : public LLMutex
{
public:
	LLCondition(AIAPRPool& parent = LLThread::tldata().mRootPool);
	~LLCondition();
	
	void wait();		// blocks
	void signal();
	void broadcast();
	
protected:
	apr_thread_cond_t *mAPRCondp;
};

class LL_COMMON_API LLMutexLock
{
public:
	LLMutexLock(LLMutexBase* mutex)
	{
		mMutex = mutex;
		mMutex->lock();
	}
	~LLMutexLock()
	{
		mMutex->unlock();
	}
private:
	LLMutexBase* mMutex;
};

class AIRWLock
{
public:
	AIRWLock(AIAPRPool& parent = LLThread::tldata().mRootPool) :
		mWriterWaitingMutex(parent), mNoHoldersCondition(parent), mHoldersCount(0), mWriterIsWaiting(false) { }

private:
	LLMutex mWriterWaitingMutex;		//!< This mutex is locked while some writer is waiting for access.
	LLCondition mNoHoldersCondition;	//!< Access control for mHoldersCount. Condition true when there are no more holders.
	int mHoldersCount;					//!< Number of readers or -1 if a writer locked this object.
	// This is volatile because we read it outside the critical area of mWriterWaitingMutex, at [1].
	// That means that other threads can change it while we are already in the (inlined) function rdlock.
	// Without volatile, the following assembly would fail:
	// register x = mWriterIsWaiting;
	// /* some thread changes mWriterIsWaiting */
	// if (x ...
	// However, because the function is fuzzy to begin with (we don't mind that this race
	// condition exists) it would work fine without volatile. So, basically it's just here
	// out of principle ;).	-- Aleric
    bool volatile mWriterIsWaiting;		//!< True when there is a writer waiting for write access.

public:
	void rdlock(bool high_priority = false)
	{
		// Give a writer a higher priority (kinda fuzzy).
		if (mWriterIsWaiting && !high_priority)	// [1] If there is a writer interested,
		{
			mWriterWaitingMutex.lock();			// [2] then give it precedence and wait here.
			// If we get here then the writer got it's access; mHoldersCount == -1.
			mWriterWaitingMutex.unlock();
		}
		mNoHoldersCondition.lock();				// [3] Get exclusive access to mHoldersCount.
		while (mHoldersCount == -1)				// [4]
		{
		  	mNoHoldersCondition.wait();			// [5] Wait till mHoldersCount is (or just was) 0.
		}
		++mHoldersCount;						// One more reader.
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
	void rdunlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		if (--mHoldersCount == 0)				// Was this the last reader?
		{
			mNoHoldersCondition.signal();		// Tell waiting threads, see [5], [6] and [7].
		}
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
	void wrlock(void)
	{
		mWriterWaitingMutex.lock();				// Block new readers, see [2],
		mWriterIsWaiting = true;				// from this moment on, see [1].
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		while (mHoldersCount != 0)				// Other readers or writers have this lock?
		{
		  	mNoHoldersCondition.wait();			// [6] Wait till mHoldersCount is (or just was) 0.
		}
		mWriterIsWaiting = false;				// Stop checking the lock for new readers, see [1].
		mWriterWaitingMutex.unlock();			// Release blocked readers, they will still hang at [3].
		mHoldersCount = -1;						// We are a writer now (will cause a hang at [5], see [4]).
		mNoHoldersCondition.unlock();			// Release lock on mHolders (readers go from [3] to [5]).
	}
	void wrunlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		mHoldersCount = 0;						// We have no writer anymore.
		mNoHoldersCondition.signal();			// Tell waiting threads, see [5], [6] and [7].
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
	void rd2wrlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount. Blocks new readers at [3].
		if (--mHoldersCount > 0)				// Any other reads left?
		{
			mWriterWaitingMutex.lock();			// Block new readers, see [2],
			mWriterIsWaiting = true;			// from this moment on, see [1].
			while (mHoldersCount != 0)			// Other readers (still) have this lock?
			{
				mNoHoldersCondition.wait();		// [7] Wait till mHoldersCount is (or just was) 0.
			}
			mWriterIsWaiting = false;			// Stop checking the lock for new readers, see [1].
			mWriterWaitingMutex.unlock();		// Release blocked readers, they will still hang at [3].
		}
		mHoldersCount = -1;						// We are a writer now (will cause a hang at [5], see [4]).
		mNoHoldersCondition.unlock();			// Release lock on mHolders (readers go from [3] to [5]).
	}
	void wr2rdlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		mHoldersCount = 1;						// Turn writer into a reader.
		mNoHoldersCondition.signal();			// Tell waiting readers, see [5].
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
};

//============================================================================

void LLThread::lockData()
{
	mRunCondition->lock();
}

void LLThread::unlockData()
{
	mRunCondition->unlock();
}

//============================================================================

// see llmemory.h for LLPointer<> definition

class LL_COMMON_API LLThreadSafeRefCount
{
public:
	static void initThreadSafeRefCount(); // creates sMutex
	static void cleanupThreadSafeRefCount(); // destroys sMutex
	
private:
	static LLMutex* sMutex;

private:
	LLThreadSafeRefCount(const LLThreadSafeRefCount&); // not implemented
	LLThreadSafeRefCount&operator=(const LLThreadSafeRefCount&); // not implemented

protected:
	virtual ~LLThreadSafeRefCount(); // use unref()
	
public:
	LLThreadSafeRefCount();
	
	void ref()
	{
		if (sMutex) sMutex->lock();
		mRef++; 
		if (sMutex) sMutex->unlock();
	} 

	S32 unref()
	{
		llassert(mRef >= 1);
		if (sMutex) sMutex->lock();
		S32 res = --mRef;
		if (sMutex) sMutex->unlock();
		if (0 == res) 
		{
			delete this; 
			return 0;
		}
		return res;
	}	
	S32 getNumRefs() const
	{
		return mRef;
	}

private: 
	S32	mRef; 
};

//============================================================================

// Simple responder for self destructing callbacks
// Pure virtual class
class LL_COMMON_API LLResponder : public LLThreadSafeRefCount
{
protected:
	virtual ~LLResponder();
public:
	virtual void completed(bool success) = 0;
};

//============================================================================

#endif // LL_LLTHREAD_H
