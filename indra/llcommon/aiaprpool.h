/**
 * @file aiaprpool.h
 * @brief Implementation of AIAPRPool.
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
 *   04/04/2010
 *   - Initial version, written by Aleric Inglewood @ SL
 *
 *   10/11/2010
 *   - Changed filename, class names and license to a more
 *     company-neutral format.
 *   - Added APR_HAS_THREADS #if's to allow creation and destruction
 *     of subpools by threads other than the parent pool owner.
 */

#ifndef AIAPRPOOL_H
#define AIAPRPOOL_H

#ifdef LL_WINDOWS
#include <ws2tcpip.h>		// Needed before including apr_portable.h
#endif

#include "apr_portable.h"
#include "apr_pools.h"
#include "llerror.h"

extern void ll_init_apr();

/**
 * @brief A wrapper around the APR memory pool API.
 *
 * Usage of this class should be restricted to passing it to libapr-1 function calls that need it.
 *
 */
class LL_COMMON_API AIAPRPool
{
protected:
	apr_pool_t* mPool;					//!< Pointer to the underlaying pool. NULL if not initialized.
	AIAPRPool* mParent;			//!< Pointer to the parent pool, if any. Only valid when mPool is non-zero.
	apr_os_thread_t mOwner;				//!< The thread that owns this memory pool. Only valid when mPool is non-zero.

public:
	//! Construct an uninitialized (destructed) pool.
	AIAPRPool(void) : mPool(NULL) { }

    //! Construct a subpool from an existing pool.
	// This is not a copy-constructor, this class doesn't have one!
	AIAPRPool(AIAPRPool& parent) : mPool(NULL) { create(parent); }

	//! Destruct the memory pool (free all of it's subpools and allocated memory).
	~AIAPRPool() { destroy(); }

protected:
	// Create a pool that is allocated from the Operating System. Only used by AIAPRRootPool.
	AIAPRPool(int) : mPool(NULL), mParent(NULL), mOwner(apr_os_thread_current())
	{
		apr_status_t const apr_pool_create_status = apr_pool_create(&mPool, NULL);
		llassert_always(apr_pool_create_status == APR_SUCCESS);
		llassert(mPool);
		apr_pool_cleanup_register(mPool, this, &s_plain_cleanup, &apr_pool_cleanup_null);
	}

public:
	//! Create a subpool from parent. May only be called for an uninitialized/destroyed pool.
	// The default parameter causes the root pool of the current thread to be used.
	void create(AIAPRPool& parent = *static_cast<AIAPRPool*>(NULL));

	//! Destroy the (sub)pool, if any.
	void destroy(void);

	// Use some safebool idiom (http://www.artima.com/cppsource/safebool.html) rather than operator bool.
	typedef apr_pool_t* const AIAPRPool::* const bool_type;
	//! Return true if the pool is initialized.
	operator bool_type() const { return mPool ? &AIAPRPool::mPool : 0; }

	// Painful, but we have to either provide access to this, or wrap
	// every APR function call that needs a apr_pool_t* to be passed.
	// NEVER destroy a pool that is returned by this function!
	apr_pool_t* operator()(void) const
	{
		llassert(mPool);
		llassert(apr_os_thread_equal(mOwner, apr_os_thread_current()));
		return mPool;
	}

	// Free all memory without destructing the pool.
	void clear(void)
	{
		llassert(mPool);
		llassert(apr_os_thread_equal(mOwner, apr_os_thread_current()));
		apr_pool_clear(mPool);
	}

// These methods would make this class 'complete' (as wrapper around the libapr
// pool functions), but we don't use memory pools in the viewer (only when
// we are forced to pass one to a libapr call), so don't define them in order
// not to encourage people to use them.
#if 0
	void* palloc(size_t size)
	{
		llassert(mPool);
		llassert(apr_os_thread_equal(mOwner, apr_os_thread_current()));
		return apr_palloc(mPool, size);
	}
	void* pcalloc(size_t size)
	{
		llassert(mPool);
		llassert(apr_os_thread_equal(mOwner, apr_os_thread_current()));
		return apr_pcalloc(mPool, size);
	}
#endif

private:
	bool parent_is_being_destructed(void);
	static apr_status_t s_plain_cleanup(void* userdata) { return static_cast<AIAPRPool*>(userdata)->plain_cleanup(); }

	apr_status_t plain_cleanup(void)
	{
		if (mPool && 						// We are not being destructed,
			parent_is_being_destructed())	// but our parent is.
		  // This means the pool is being destructed recursively by libapr
		  // because one of it's parents is being destructed.
		{
			mPool = NULL;	// Stop destroy() from destructing the pool again.
		}
		return APR_SUCCESS;
	}
};

class AIAPRInitialization
{
public:
	AIAPRInitialization(void);
};

/**
 * @brief Root memory pool (allocates memory from the operating system).
 *
 * This class should only be used by AIThreadLocalData and AIThreadSafeSimpleDCRootPool_pbase
 * (and LLMutexRootPool when APR_HAS_THREADS isn't defined).
 */
class LL_COMMON_API AIAPRRootPool : public AIAPRInitialization, public AIAPRPool
{
private:
	friend class AIThreadLocalData;
	friend class AIThreadSafeSimpleDCRootPool_pbase;
#if !APR_HAS_THREADS
	friend class LLMutexRootPool;
#endif
	//! Construct a root memory pool.
	//  Should only be used by AIThreadLocalData and AIThreadSafeSimpleDCRootPool_pbase.
	AIAPRRootPool(void);
	~AIAPRRootPool();

private:
	// Keep track of how many root pools exist and when the last one is destructed.
	static bool sCountInitialized;
	static apr_uint32_t volatile sCount;

public:
	// Return a global root pool that is independent of AIThreadLocalData.
	// Normally you should not use this. Only use for early initialization
	// (before main) and deinitialization (after main).
	static AIAPRRootPool& get(void);

#if APR_POOL_DEBUG
	void grab_ownership(void)
	{
		// You need a patched libapr to use this.
		// See http://web.archiveorange.com/archive/v/5XO9y2zoxUOMt6Gmi1OI
		apr_pool_owner_set(mPool);
	}
#endif

private:
	// Used for constructing the Special Global Root Pool (returned by AIAPRRootPool::get).
	// It is the same as the default constructor but omits to increment sCount. As a result,
	// we must be sure that at least one other AIAPRRootPool is created before termination
	// of the application (which is the case: we create one AIAPRRootPool per thread).
	AIAPRRootPool(int) : AIAPRInitialization(), AIAPRPool(0) { }
};

//! Volatile memory pool
//
// 'Volatile' APR memory pool which normally only clears memory,
// and does not destroy the pool (the same pool is reused) for
// greater efficiency. However, as a safe guard the apr pool
// is destructed every FULL_VOLATILE_APR_POOL uses to allow
// the system memory to be allocated more efficiently and not
// get scattered through RAM.
//
class LL_COMMON_API AIVolatileAPRPool : protected AIAPRPool
{
public:
	AIVolatileAPRPool(void) : mNumActiveRef(0), mNumTotalRef(0) { }

	apr_pool_t* getVolatileAPRPool(void)
	{
		if (!mPool) create();
		++mNumActiveRef;
		++mNumTotalRef;
		return AIAPRPool::operator()();
	}
	void clearVolatileAPRPool(void);

	bool isOld(void) const { return mNumTotalRef > FULL_VOLATILE_APR_POOL; }
	bool isUnused() const { return mNumActiveRef == 0; }

private:
	S32 mNumActiveRef;	// Number of active uses of the pool.
	S32 mNumTotalRef;	// Number of total uses of the pool since last creation.

	// Maximum number of references to AIVolatileAPRPool until the pool is recreated.
	static S32 const FULL_VOLATILE_APR_POOL = 1024;
};

#endif // AIAPRPOOL_H
