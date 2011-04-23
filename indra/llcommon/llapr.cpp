/** 
 * @file llapr.cpp
 * @author Phoenix
 * @date 2004-11-28
 * @brief Helper functions for using the apache portable runtime library.
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

apr_pool_t *gAPRPoolp = NULL; // Global APR memory pool
apr_thread_mutex_t *gLogMutexp = NULL;
apr_thread_mutex_t *gCallStacksLogMutexp = NULL;

const S32 FULL_VOLATILE_APR_POOL = 1024 ; //number of references to LLVolatileAPRPool

void ll_init_apr()
{
	if (!gAPRPoolp)
	{
		// Initialize APR and create the global pool
		apr_initialize();
		apr_pool_create(&gAPRPoolp, NULL);
		
		// Initialize the logging mutex
		apr_thread_mutex_create(&gLogMutexp, APR_THREAD_MUTEX_UNNESTED, gAPRPoolp);
		apr_thread_mutex_create(&gCallStacksLogMutexp, APR_THREAD_MUTEX_UNNESTED, gAPRPoolp);

		// Initialize thread-local APR pool support.
		LLVolatileAPRPool::initLocalAPRFilePool();
	}
}


void ll_cleanup_apr()
{
	LL_INFOS("APR") << "Cleaning up APR" << LL_ENDL;

	if (gLogMutexp)
	{
		// Clean up the logging mutex

		// All other threads NEED to be done before we clean up APR, so this is okay.
		apr_thread_mutex_destroy(gLogMutexp);
		gLogMutexp = NULL;
	}
	if (gCallStacksLogMutexp)
	{
		// Clean up the logging mutex

		// All other threads NEED to be done before we clean up APR, so this is okay.
		apr_thread_mutex_destroy(gCallStacksLogMutexp);
		gCallStacksLogMutexp = NULL;
	}
	if (gAPRPoolp)
	{
		apr_pool_destroy(gAPRPoolp);
		gAPRPoolp = NULL;
	}
	apr_terminate();
}

//
//
//LLAPRPool
//
LLAPRPool::LLAPRPool(apr_pool_t *parent, apr_size_t size, BOOL releasePoolFlag) 	
	: mParent(parent),
	mReleasePoolFlag(releasePoolFlag),
	mMaxSize(size),
	mPool(NULL)
{	
	createAPRPool() ;
}

LLAPRPool::~LLAPRPool() 
{
	releaseAPRPool() ;
}

void LLAPRPool::createAPRPool()
{
	if(mPool)
	{
		return ;
	}

	mStatus = apr_pool_create(&mPool, mParent);
	ll_apr_warn_status(mStatus) ;

	if(mMaxSize > 0) //size is the number of blocks (which is usually 4K), NOT bytes.
	{
		apr_allocator_t *allocator = apr_pool_allocator_get(mPool); 
		if (allocator) 
		{ 
			apr_allocator_max_free_set(allocator, mMaxSize) ;
		}
	}
}

void LLAPRPool::releaseAPRPool()
{
	if(!mPool)
	{
		return ;
	}

	if(!mParent || mReleasePoolFlag)
	{
		apr_pool_destroy(mPool) ;
		mPool = NULL ;
	}
}

apr_pool_t* LLAPRPool::getAPRPool() 
{
	if(!mPool)
	{
		createAPRPool() ;
	}
	
	return mPool ; 
}
LLVolatileAPRPool::LLVolatileAPRPool(apr_pool_t *parent, apr_size_t size, BOOL releasePoolFlag) 
				  : LLAPRPool(parent, size, releasePoolFlag)
{
	mNumActiveRef = 0 ;
	mNumTotalRef = 0 ;
}

apr_pool_t* LLVolatileAPRPool::getVolatileAPRPool() 
{
	mNumTotalRef++ ;
	mNumActiveRef++ ;
	return getAPRPool() ;
}

void LLVolatileAPRPool::clearVolatileAPRPool() 
{
	if(mNumActiveRef > 0)
	{
		mNumActiveRef--;
		if(mNumActiveRef < 1)
		{
			if(isFull()) 
			{
				mNumTotalRef = 0 ;

				//destroy the apr_pool.
				releaseAPRPool() ;
			}
			else 
			{
				//This does not actually free the memory, 
				//it just allows the pool to re-use this memory for the next allocation. 
				apr_pool_clear(mPool) ;
			}
		}
	}
	else
	{
		llassert_always(mNumActiveRef > 0) ;
	}

	//paranoia check if the pool is jammed.
	//will remove the check before going to release.
	llassert_always(mNumTotalRef < (FULL_VOLATILE_APR_POOL << 2)) ;
}

BOOL LLVolatileAPRPool::isFull()
{
	return mNumTotalRef > FULL_VOLATILE_APR_POOL ;
}

#ifdef SHOW_ASSERT
// This allows the use of llassert(is_main_thread()) to assure the current thread is the main thread.
static void* gIsMainThread;
bool is_main_thread() { return gIsMainThread == LLVolatileAPRPool::getLocalAPRFilePool(); }
#endif

// The thread private handle to access the LocalAPRFilePool.
apr_threadkey_t* LLVolatileAPRPool::sLocalAPRFilePoolKey;

// This should be called exactly once, before the first call to createLocalAPRFilePool.
// static
void LLVolatileAPRPool::initLocalAPRFilePool()
{
	apr_status_t status = apr_threadkey_private_create(&sLocalAPRFilePoolKey, &destroyLocalAPRFilePool, gAPRPoolp);
	ll_apr_assert_status(status);			// Or out of memory, or system-imposed limit on the
							// total number of keys per process {PTHREAD_KEYS_MAX}
							// has been exceeded.
	// Create the thread-local pool for the main thread (this function is called by the main thread).
	createLocalAPRFilePool();
#ifdef SHOW_ASSERT
	gIsMainThread = getLocalAPRFilePool();
#endif
}

// This should be called once for every thread, before it uses getLocalAPRFilePool.
// static
void LLVolatileAPRPool::createLocalAPRFilePool()
{
	void* thread_local_data = new LLVolatileAPRPool;
	apr_status_t status = apr_threadkey_private_set(thread_local_data, sLocalAPRFilePoolKey);
	llassert_always(status == APR_SUCCESS);
}

// This is called once for every thread when the thread is destructed.
// static
void LLVolatileAPRPool::destroyLocalAPRFilePool(void* thread_local_data)
{
	delete reinterpret_cast<LLVolatileAPRPool*>(thread_local_data);
}

// static
LLVolatileAPRPool* LLVolatileAPRPool::getLocalAPRFilePool()
{
	void* thread_local_data;
	apr_status_t status = apr_threadkey_private_get(&thread_local_data, sLocalAPRFilePoolKey);
	llassert_always(status == APR_SUCCESS);
	return reinterpret_cast<LLVolatileAPRPool*>(thread_local_data);
}

//---------------------------------------------------------------------
//
// LLScopedLock
//
LLScopedLock::LLScopedLock(apr_thread_mutex_t* mutex) : mMutex(mutex)
{
	if(mutex)
	{
		if(ll_apr_warn_status(apr_thread_mutex_lock(mMutex)))
		{
			mLocked = false;
		}
		else
		{
			mLocked = true;
		}
	}
	else
	{
		mLocked = false;
	}
}

LLScopedLock::~LLScopedLock()
{
	unlock();
}

void LLScopedLock::unlock()
{
	if(mLocked)
	{
		if(!ll_apr_warn_status(apr_thread_mutex_unlock(mMutex)))
		{
			mLocked = false;
		}
	}
}

//---------------------------------------------------------------------

bool ll_apr_warn_status(apr_status_t status)
{
	if(APR_SUCCESS == status) return false;
	char buf[MAX_STRING];	/* Flawfinder: ignore */
	apr_strerror(status, buf, MAX_STRING);
	LL_WARNS("APR") << "APR: " << buf << LL_ENDL;
	return true;
}

void ll_apr_assert_status(apr_status_t status)
{
	llassert(ll_apr_warn_status(status) == false);
}

//---------------------------------------------------------------------
//
// LLAPRFile functions
//
LLAPRFile::LLAPRFile()
	: mFile(NULL),
	  mCurrentFilePoolp(NULL)
{
}

LLAPRFile::LLAPRFile(const std::string& filename, apr_int32_t flags, access_t access_type)
	: mFile(NULL),
	  mCurrentFilePoolp(NULL)
{
	open(filename, flags, access_type);
}

LLAPRFile::~LLAPRFile()
{
	close() ;
}

apr_status_t LLAPRFile::close() 
{
	apr_status_t ret = APR_SUCCESS ;
	if(mFile)
	{
		ret = apr_file_close(mFile);
		mFile = NULL ;
	}

	if(mCurrentFilePoolp)
	{
		mCurrentFilePoolp->clearVolatileAPRPool() ;
		mCurrentFilePoolp = NULL ;
	}

	return ret ;
}

apr_status_t LLAPRFile::open(std::string const& filename, apr_int32_t flags, access_t access_type, S32* sizep)
{
	llassert_always(!mFile);
	llassert_always(!mCurrentFilePoolp);

	// Access the pool and increment it's reference count.
	// The reference count of LLVolatileAPRPool objects will be decremented
	// again in LLAPRFile::close by calling mCurrentFilePoolp->clearVolatileAPRPool().
	apr_pool_t* pool;
	if (access_type == local)
	{
	  	// Use a "volatile" thread-local pool.
		mCurrentFilePoolp = LLVolatileAPRPool::getLocalAPRFilePool();
		pool = mCurrentFilePoolp->getVolatileAPRPool();
	}
	else
	{
	  	llassert(is_main_thread());
		pool = gAPRPoolp;
	}
	apr_status_t s = apr_file_open(&mFile, filename.c_str(), flags, APR_OS_DEFAULT, pool);
	if (s != APR_SUCCESS || !mFile)
	{
		mFile = NULL ;
		close() ;
		if (sizep)
		{
			*sizep = 0;
		}
		return s;
	}

	if (sizep)
	{
		S32 file_size = 0;
		apr_off_t offset = 0;
		if (apr_file_seek(mFile, APR_END, &offset) == APR_SUCCESS)
		{
			llassert_always(offset <= 0x7fffffff);
			file_size = (S32)offset;
			offset = 0;
			apr_file_seek(mFile, APR_SET, &offset);
		}
		*sizep = file_size;
	}

	return s;
}

// File I/O
S32 LLAPRFile::read(void *buf, S32 nbytes)
{
	//llassert_always(mFile); (ASC-TUDCC) -HgB
	if(!mFile) 
	{
		llwarns << "apr mFile is removed by somebody else. Can not read." << llendl ;
		return 0;
	}
	
	apr_size_t sz = nbytes;
	apr_status_t s = apr_file_read(mFile, buf, &sz);
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return 0;
	}
	else
	{
		llassert_always(sz <= 0x7fffffff);
		return (S32)sz;
	}
}

S32 LLAPRFile::write(const void *buf, S32 nbytes)
{
	// llassert_always(mFile); (ASC-TUDCC) -HgB
	if(!mFile) 
	{
		llwarns << "apr mFile is removed by somebody else. Can not write." << llendl ;
		return 0;
	}
	
	apr_size_t sz = nbytes;
	apr_status_t s = apr_file_write(mFile, buf, &sz);
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return 0;
	}
	else
	{
		llassert_always(sz <= 0x7fffffff);
		return (S32)sz;
	}
}

S32 LLAPRFile::seek(apr_seek_where_t where, S32 offset)
{
	return LLAPRFile::seek(mFile, where, offset) ;
}

//
//*******************************************************************************************************************************
//static components of LLAPRFile
//

// Used in the static functions below.
class LLScopedVolatileAPRFilePool {
private:
  	LLVolatileAPRPool* mPool;
	apr_pool_t* apr_pool;
public:
	LLScopedVolatileAPRFilePool() : mPool(LLVolatileAPRPool::getLocalAPRFilePool()), apr_pool(mPool->getVolatileAPRPool()) { }
	~LLScopedVolatileAPRFilePool() { mPool->clearVolatileAPRPool(); }
	operator apr_pool_t*() const { return apr_pool; }
};

//static
S32 LLAPRFile::seek(apr_file_t* file_handle, apr_seek_where_t where, S32 offset)
{
	if(!file_handle)
	{
		return -1 ;
	}

	apr_status_t s;
	apr_off_t apr_offset;
	if (offset >= 0)
	{
		apr_offset = (apr_off_t)offset;
		s = apr_file_seek(file_handle, where, &apr_offset);
	}
	else
	{
		apr_offset = 0;
		s = apr_file_seek(file_handle, APR_END, &apr_offset);
	}
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return -1;
	}
	else
	{
		llassert_always(apr_offset <= 0x7fffffff);
		return (S32)apr_offset;
	}
}

//static
S32 LLAPRFile::readEx(const std::string& filename, void *buf, S32 offset, S32 nbytes)
{
	apr_file_t* file_handle;
	LLScopedVolatileAPRFilePool pool;
	apr_status_t s = apr_file_open(&file_handle, filename.c_str(), APR_READ|APR_BINARY, APR_OS_DEFAULT, pool);
	if (s != APR_SUCCESS || !file_handle)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " while attempting to open file \"" << filename << '"' << LL_ENDL;
		return 0;
	}

	S32 off;
	if (offset < 0)
		off = LLAPRFile::seek(file_handle, APR_END, 0);
	else
		off = LLAPRFile::seek(file_handle, APR_SET, offset);
	
	apr_size_t bytes_read;
	if (off < 0)
	{
		bytes_read = 0;
	}
	else
	{
		bytes_read = nbytes ;		
		apr_status_t s = apr_file_read(file_handle, buf, &bytes_read);
		if (s != APR_SUCCESS)
		{
			LL_WARNS("APR") << " Attempting to read filename: " << filename << LL_ENDL;
			ll_apr_warn_status(s);
			bytes_read = 0;
		}
		else
		{
			llassert_always(bytes_read <= 0x7fffffff);		
		}
	}
	
	apr_file_close(file_handle);

	return (S32)bytes_read;
}

//static
S32 LLAPRFile::writeEx(const std::string& filename, void *buf, S32 offset, S32 nbytes)
{
	apr_int32_t flags = APR_CREATE|APR_WRITE|APR_BINARY;
	if (offset < 0)
	{
		flags |= APR_APPEND;
		offset = 0;
	}
	
	apr_file_t* file_handle;
	LLScopedVolatileAPRFilePool pool;
	apr_status_t s = apr_file_open(&file_handle, filename.c_str(), flags, APR_OS_DEFAULT, pool);
	if (s != APR_SUCCESS || !file_handle)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " while attempting to open file \"" << filename << '"' << LL_ENDL;
		return 0;
	}

	if (offset > 0)
	{
		offset = LLAPRFile::seek(file_handle, APR_SET, offset);
	}
	
	apr_size_t bytes_written;
	if (offset < 0)
	{
		bytes_written = 0;
	}
	else
	{
		bytes_written = nbytes ;		
		apr_status_t s = apr_file_write(file_handle, buf, &bytes_written);
		if (s != APR_SUCCESS)
		{
			LL_WARNS("APR") << " Attempting to write filename: " << filename << LL_ENDL;
			ll_apr_warn_status(s);
			bytes_written = 0;
		}
		else
		{
			llassert_always(bytes_written <= 0x7fffffff);
		}
	}

	apr_file_close(file_handle);

	return (S32)bytes_written;
}

//static
bool LLAPRFile::remove(const std::string& filename)
{
	apr_status_t s;

	LLScopedVolatileAPRFilePool pool;
	s = apr_file_remove(filename.c_str(), pool);

	if (s != APR_SUCCESS)
	{
#if LL_WINDOWS
		if (s != APR_ENOENT && s != APR_OS_START_SYSERR + ERROR_FILE_NOT_FOUND)
#else
		if (s != APR_ENOENT)
#endif
		{
			ll_apr_warn_status(s);
			LL_WARNS("APR") << " Attempting to remove filename: " << filename << LL_ENDL;
		}
		return false;
	}
	return true;
}

//static
bool LLAPRFile::rename(const std::string& filename, const std::string& newname)
{
	apr_status_t s;

	LLScopedVolatileAPRFilePool pool;
	s = apr_file_rename(filename.c_str(), newname.c_str(), pool);
	
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to rename filename: " << filename << LL_ENDL;
		return false;
	}
	return true;
}

//static
bool LLAPRFile::isExist(const std::string& filename, apr_int32_t flags)
{
	apr_file_t* file_handle;
	apr_status_t s;

	LLScopedVolatileAPRFilePool pool;
	s = apr_file_open(&file_handle, filename.c_str(), flags, APR_OS_DEFAULT, pool);

	if (s != APR_SUCCESS || !file_handle)
	{
		return false;
	}
	else
	{
		apr_file_close(file_handle);
		return true;
	}
}

//static
S32 LLAPRFile::size(const std::string& filename)
{
	apr_file_t* file_handle;
	apr_finfo_t info;
	apr_status_t s;
	
	LLScopedVolatileAPRFilePool pool;
	s = apr_file_open(&file_handle, filename.c_str(), APR_READ, APR_OS_DEFAULT, pool);
	
	if (s != APR_SUCCESS || !file_handle)
	{		
		return 0;
	}
	else
	{
		apr_status_t s = apr_file_info_get(&info, APR_FINFO_SIZE, file_handle);

		apr_file_close(file_handle) ;
		
		if (s == APR_SUCCESS)
		{
			return (S32)info.size;
		}
		else
		{
			return 0;
		}
	}
}

//static
bool LLAPRFile::makeDir(const std::string& dirname)
{
	apr_status_t s;

	LLScopedVolatileAPRFilePool pool;
	s = apr_dir_make(dirname.c_str(), APR_FPROT_OS_DEFAULT, pool);
		
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " while attempting to make directory: " << dirname << LL_ENDL;
		return false;
	}
	return true;
}

//static
bool LLAPRFile::removeDir(const std::string& dirname)
{
	apr_status_t s;

	LLScopedVolatileAPRFilePool pool;
	s = apr_file_remove(dirname.c_str(), pool);
	
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to remove directory: " << dirname << LL_ENDL;
		return false;
	}
	return true;
}
//
//end of static components of LLAPRFile
//*******************************************************************************************************************************
//
