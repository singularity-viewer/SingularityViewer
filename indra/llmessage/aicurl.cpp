/**
 * @file aicurl.cpp
 * @brief Implementation of AICurl.
 *
 * Copyright (c) 2012, Aleric Inglewood.
 * Copyright (C) 2010, Linden Research, Inc.
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
 *   17/03/2012
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   20/03/2012
 *   Added copyright notice for Linden Lab for those parts that were
 *   copied or derived from llcurl.cpp. The code of those parts are
 *   already in their own llcurl.cpp, so they do not ever need to
 *   even look at this file; the reason I added the copyright notice
 *   is to make clear that I am not the author of 100% of this code
 *   and hence I cannot change the license of it.
 */

#include "linden_common.h"

#if LL_WINDOWS
#include <winsock2.h> //remove classic winsock from windows.h
#endif
#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>	// OPENSSL_THREADS
#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include "aicurl.h"
#include "llbufferstream.h"
#include "llsdserialize.h"
#include "aithreadsafe.h"
#include "llqueuedthread.h"
#include "lltimer.h"		// ms_sleep
#include "llproxy.h"
#include "llhttpstatuscodes.h"
#include "aihttpheaders.h"
#include "aihttptimeoutpolicy.h"
#include "aicurleasyrequeststatemachine.h"
#include "aicurlperservice.h"

//==================================================================================
// Debug Settings
//

bool gNoVerifySSLCert;

//==================================================================================
// Local variables.
//

namespace {

struct CertificateAuthority {
  std::string file;
  std::string path;
};

AIThreadSafeSimpleDC<CertificateAuthority> gCertificateAuthority;
typedef AIAccess<CertificateAuthority> CertificateAuthority_wat;
typedef AIAccessConst<CertificateAuthority> CertificateAuthority_rat;

enum gSSLlib_type {
  ssl_unknown,
  ssl_openssl,
  ssl_gnutls,
  ssl_nss
};

// No locking needed: initialized before threads are created, and subsequently only read.
gSSLlib_type gSSLlib;
bool gSetoptParamsNeedDup;

} // namespace

// See http://www.openssl.org/docs/crypto/threads.html:
// CRYPTO_THREADID and associated functions were introduced in OpenSSL 1.0.0 to replace
// (actually, deprecate) the previous CRYPTO_set_id_callback(), CRYPTO_get_id_callback(),
// and CRYPTO_thread_id() functions which assumed thread IDs to always be represented by
// 'unsigned long'.
#define HAVE_CRYPTO_THREADID (OPENSSL_VERSION_NUMBER >= (1 << 28))

//-----------------------------------------------------------------------------------
// Needed for thread-safe openSSL operation.

// Must be defined in global namespace.
struct CRYPTO_dynlock_value
{
  AIRWLock rwlock;
};

namespace {

AIRWLock* ssl_rwlock_array;

// OpenSSL locking function.
void ssl_locking_function(int mode, int n, char const* file, int line)
{
  if ((mode & CRYPTO_LOCK))
  {
	if ((mode & CRYPTO_READ))
	  ssl_rwlock_array[n].rdlock();
	else
	  ssl_rwlock_array[n].wrlock();
  }
  else
  {
    if ((mode & CRYPTO_READ))
	  ssl_rwlock_array[n].rdunlock();
	else
	  ssl_rwlock_array[n].wrunlock();
  }
}

#if HAVE_CRYPTO_THREADID
// OpenSSL uniq id function.
void ssl_id_function(CRYPTO_THREADID* thread_id)
{
#if LL_WINDOWS || LL_DARWIN	// apr_os_thread_current() returns a pointer,
  CRYPTO_THREADID_set_pointer(thread_id, apr_os_thread_current());
#else			// else it returns an unsigned long.
  CRYPTO_THREADID_set_numeric(thread_id, apr_os_thread_current());
#endif
}
#endif // HAVE_CRYPTO_THREADID

// OpenSSL allocate and initialize dynamic crypto lock.
CRYPTO_dynlock_value* ssl_dyn_create_function(char const* file, int line)
{
  return new CRYPTO_dynlock_value;
}

// OpenSSL destroy dynamic crypto lock.
void ssl_dyn_destroy_function(CRYPTO_dynlock_value* l, char const* file, int line)
{
  delete l;
}

// OpenSSL dynamic locking function.
void ssl_dyn_lock_function(int mode, CRYPTO_dynlock_value* l, char const* file, int line)
{
  if ((mode & CRYPTO_LOCK))
  {
	if ((mode & CRYPTO_READ))
	  l->rwlock.rdlock();
	else
	  l->rwlock.wrlock();
  }
  else
  {
    if ((mode & CRYPTO_READ))
	  l->rwlock.rdunlock();
	else
	  l->rwlock.wrunlock();
  }
}

typedef void (*ssl_locking_function_type)(int, int, char const*, int);
#if HAVE_CRYPTO_THREADID
typedef void (*ssl_id_function_type)(CRYPTO_THREADID*);
#else
typedef unsigned long (*ulong_thread_id_function_type)(void);
#endif
typedef CRYPTO_dynlock_value* (*ssl_dyn_create_function_type)(char const*, int);
typedef void (*ssl_dyn_destroy_function_type)(CRYPTO_dynlock_value*, char const*, int);
typedef void (*ssl_dyn_lock_function_type)(int, CRYPTO_dynlock_value*, char const*, int);

ssl_locking_function_type     old_ssl_locking_function;
#if HAVE_CRYPTO_THREADID
ssl_id_function_type          old_ssl_id_function;
#else
ulong_thread_id_function_type old_ulong_thread_id_function;
#endif
ssl_dyn_create_function_type  old_ssl_dyn_create_function;
ssl_dyn_destroy_function_type old_ssl_dyn_destroy_function;
ssl_dyn_lock_function_type    old_ssl_dyn_lock_function;

#if LL_WINDOWS
static unsigned long __cdecl apr_os_thread_current_wrapper()
{
	return (unsigned long)apr_os_thread_current();
}
#endif

// Set for openssl-1.0.1...1.0.1c.
static bool need_renegotiation_hack = false;

// Initialize OpenSSL library for thread-safety.
void ssl_init(void)
{
  // The version identifier format is: MMNNFFPPS: major minor fix patch status.
  int const compiled_openSSL_major = (OPENSSL_VERSION_NUMBER >> 28) & 0xff;
  int const compiled_openSSL_minor = (OPENSSL_VERSION_NUMBER >> 20) & 0xff;
  unsigned long const ssleay = SSLeay();
  int const linked_openSSL_major = (ssleay >> 28) & 0xff;
  int const linked_openSSL_minor = (ssleay >> 20) & 0xff;
  // Check if dynamically loaded version is compatible with the one we compiled against.
  // As off version 1.0.0 also minor versions are compatible.
  if (linked_openSSL_major != compiled_openSSL_major ||
	  (linked_openSSL_major == 0 && linked_openSSL_minor != compiled_openSSL_minor))
  {
	llerrs << "The viewer was compiled against " << OPENSSL_VERSION_TEXT <<
	    " but linked against " << SSLeay_version(SSLEAY_VERSION) <<
		". Those versions are not compatible." << llendl;
  }
  // Static locks vector.
  ssl_rwlock_array = new AIRWLock[CRYPTO_num_locks()];
  // Static locks callbacks.
  old_ssl_locking_function = CRYPTO_get_locking_callback();
#if HAVE_CRYPTO_THREADID
  old_ssl_id_function = CRYPTO_THREADID_get_callback();
#else
  old_ulong_thread_id_function = CRYPTO_get_id_callback();
#endif
  CRYPTO_set_locking_callback(&ssl_locking_function);
  // Setting this avoids the need for a thread-local error number facility, which is hard to check.
#if HAVE_CRYPTO_THREADID
  CRYPTO_THREADID_set_callback(&ssl_id_function);
#else
#if LL_WINDOWS
  CRYPTO_set_id_callback(&apr_os_thread_current_wrapper);
#else
  CRYPTO_set_id_callback(&apr_os_thread_current);
#endif
#endif
  // Dynamic locks callbacks.
  old_ssl_dyn_create_function = CRYPTO_get_dynlock_create_callback();
  old_ssl_dyn_lock_function = CRYPTO_get_dynlock_lock_callback();
  old_ssl_dyn_destroy_function = CRYPTO_get_dynlock_destroy_callback();
  CRYPTO_set_dynlock_create_callback(&ssl_dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(&ssl_dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(&ssl_dyn_destroy_function);
  need_renegotiation_hack = (0x10001000UL <= ssleay);
  llinfos << "Successful initialization of " <<
	  SSLeay_version(SSLEAY_VERSION) << " (0x" << std::hex << SSLeay() << std::dec << ")." << llendl;
}

// Cleanup OpenSSL library thread-safety.
void ssl_cleanup(void)
{
  // Dynamic locks callbacks.
  CRYPTO_set_dynlock_destroy_callback(old_ssl_dyn_destroy_function);
  CRYPTO_set_dynlock_lock_callback(old_ssl_dyn_lock_function);
  CRYPTO_set_dynlock_create_callback(old_ssl_dyn_create_function);
  // Static locks callbacks.
#if HAVE_CRYPTO_THREADID
  CRYPTO_THREADID_set_callback(old_ssl_id_function);
#else
  CRYPTO_set_id_callback(old_ulong_thread_id_function);
#endif
  CRYPTO_set_locking_callback(old_ssl_locking_function);
  // Static locks vector.
  delete [] ssl_rwlock_array;
}

} // namespace openSSL
//-----------------------------------------------------------------------------------

static unsigned int encoded_version(int major, int minor, int patch)
{
  return (major << 16) | (minor << 8) | patch;
}

//==================================================================================
// External API
//

#undef AICurlPrivate

namespace AICurlInterface {

//static
LLAtomicU32 Stats::easy_calls;
LLAtomicU32 Stats::easy_errors;
LLAtomicU32 Stats::easy_init_calls;
LLAtomicU32 Stats::easy_init_errors;
LLAtomicU32 Stats::easy_cleanup_calls;
LLAtomicU32 Stats::multi_calls;
LLAtomicU32 Stats::multi_errors;
LLAtomicU32 Stats::running_handles;
LLAtomicU32 Stats::AICurlEasyRequest_count;
LLAtomicU32 Stats::AICurlEasyRequestStateMachine_count;
LLAtomicU32 Stats::BufferedCurlEasyRequest_count;
LLAtomicU32 Stats::ResponderBase_count;
LLAtomicU32 Stats::ThreadSafeBufferedCurlEasyRequest_count;
LLAtomicU32 Stats::status_count[100];
LLAtomicU32 Stats::llsd_body_count;
LLAtomicU32 Stats::llsd_body_parse_error;
LLAtomicU32 Stats::raw_body_count;

// Called from BufferedCurlEasyRequest::setStatusAndReason.
// The only allowed values for 'status' are S <= status < S+20, where S={100,200,300,400,500}.
U32 Stats::status2index(U32 status)
{
  return (status - 100) / 100 * 20 + status % 100;							// Returns 0..99 (for status 100..519).
}

U32 Stats::index2status(U32 index)
{
  return 100 + (index / 20) * 100 + index % 20;
}

// MAIN-THREAD
void initCurl(void)
{
  DoutEntering(dc::curl, "AICurlInterface::initCurl()");

  llassert(LLThread::getRunning() == 0);		// We must not call curl_global_init unless we are the only thread.
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK)
  {
	llerrs << "curl_global_init(CURL_GLOBAL_ALL) failed." << llendl;
  }

  // Print version and do some feature sanity checks.
  {
	curl_version_info_data* version_info = curl_version_info(CURLVERSION_NOW);

	llassert_always(version_info->age >= 0);
	if (version_info->age < 1)
	{
	  llwarns << "libcurl's age is 0; no ares support." << llendl;
	}
	llassert_always((version_info->features & CURL_VERSION_SSL));	// SSL support, added in libcurl 7.10.
	if (!(version_info->features & CURL_VERSION_ASYNCHDNS))			// Asynchronous name lookups (added in libcurl 7.10.7).
	{
	  llwarns << "libcurl was not compiled with support for asynchronous name lookups!" << llendl;
	}
	if (!version_info->ssl_version)
	{
	  llerrs << "This libcurl has no SSL support!" << llendl;
	}

	llinfos << "Successful initialization of libcurl " <<
		version_info->version << " (0x" << std::hex << version_info->version_num << std::dec << "), (" <<
	    version_info->ssl_version;
	if (version_info->libz_version)
	{
	  llcont << ", libz/" << version_info->libz_version;
	}
	llcont << ")." << llendl;

	// Detect SSL library used.
	gSSLlib = ssl_unknown;
	std::string ssl_version(version_info->ssl_version);
	if (ssl_version.find("OpenSSL") != std::string::npos)
	  gSSLlib = ssl_openssl;										// See http://www.openssl.org/docs/crypto/threads.html#DESCRIPTION
	else if (ssl_version.find("GnuTLS") != std::string::npos)
	  gSSLlib = ssl_gnutls;											// See http://www.gnu.org/software/gnutls/manual/html_node/Thread-safety.html
	else if (ssl_version.find("NSS") != std::string::npos)
	  gSSLlib = ssl_nss;											// Supposedly thread-safe without any requirements.

	// Set up thread-safety requirements of underlaying SSL library.
	// See http://curl.haxx.se/libcurl/c/libcurl-tutorial.html
	switch (gSSLlib)
	{
	  case ssl_unknown:
	  {
		llerrs << "Unknown SSL library \"" << version_info->ssl_version << "\", required actions for thread-safe handling are unknown! Bailing out." << llendl;
	  }
	  case ssl_openssl:
	  {
#ifndef OPENSSL_THREADS
		llerrs << "OpenSSL was not configured with thread support! Bailing out." << llendl;
#endif
		ssl_init();
	  }
	  case ssl_gnutls:
	  {
		// I don't think we ever get here, do we? --Aleric
		llassert_always(gSSLlib != ssl_gnutls);
		// If we do, then didn't curl_global_init already call gnutls_global_init?
		// It seems there is nothing to do for us here.
	  }
	  case ssl_nss:
	  {
	    break;	// No requirements.
	  }
	}

	// Before version 7.17.0, strings were not copied. Instead the user was forced keep them available until libcurl no longer needed them.
	gSetoptParamsNeedDup = (version_info->version_num < encoded_version(7, 17, 0));
	if (gSetoptParamsNeedDup)
	{
	  llwarns << "Your libcurl version is too old." << llendl;
	}
	llassert_always(!gSetoptParamsNeedDup);		// Might add support later.
  }
}

// MAIN-THREAD
void shutdownCurl(void)
{
  using namespace AICurlPrivate;

  DoutEntering(dc::curl, "AICurlInterface::shutdownCurl()");

  BufferedCurlEasyRequest::shutdown();
}

// MAIN-THREAD
void cleanupCurl(void)
{
  using namespace AICurlPrivate;

  DoutEntering(dc::curl, "AICurlInterface::cleanupCurl()");

  stopCurlThread();
  if (CurlMultiHandle::getTotalMultiHandles() != 0)
	llwarns << "Not all CurlMultiHandle objects were destroyed!" << llendl;
  gMainThreadEngine.flush();			// Not really related to curl, but why not.
  gStateMachineThreadEngine.flush();
  clearCommandQueue();
  Stats::print();
  ssl_cleanup();

  llassert(LLThread::getRunning() <= (curlThreadIsRunning() ? 1 : 0));		// We must not call curl_global_cleanup unless we are the only thread left.
  curl_global_cleanup();
}

// THREAD-SAFE
std::string getVersionString(void)
{
  // libcurl is thread safe, no locking needed.
  return curl_version();
}

// THREAD-SAFE
void setCAFile(std::string const& file)
{
  CertificateAuthority_wat CertificateAuthority_w(gCertificateAuthority);
  CertificateAuthority_w->file = file;
}

// This function is not called from anywhere, but provided as part of AICurlInterface because setCAFile is.
// THREAD-SAFE
void setCAPath(std::string const& path)
{
  CertificateAuthority_wat CertificateAuthority_w(gCertificateAuthority);
  CertificateAuthority_w->path = path;
}

// THREAD-SAFE
U32 getNumHTTPRunning(void)
{
  return Stats::running_handles;
}

//static
void Stats::print(void)
{
  int const easy_handles = easy_init_calls - easy_init_errors - easy_cleanup_calls;
  llinfos_nf << "============ CURL  STATS ============" << llendl;
  llinfos_nf << "  Curl multi       errors/calls      : " << std::dec << multi_errors << "/" << multi_calls << llendl;
  llinfos_nf << "  Curl easy        errors/calls      : " << std::dec << easy_errors << "/" << easy_calls << llendl;
  llinfos_nf << "  curl_easy_init() errors/calls      : " << std::dec << easy_init_errors << "/" << easy_init_calls << llendl;
  llinfos_nf << "  Current number of curl easy handles: " << std::dec << easy_handles << llendl;
#ifdef DEBUG_CURLIO
  llinfos_nf << "  Current number of BufferedCurlEasyRequest objects: " << BufferedCurlEasyRequest_count << llendl;
  llinfos_nf << "  Current number of ThreadSafeBufferedCurlEasyRequest objects: " << ThreadSafeBufferedCurlEasyRequest_count << llendl;
  llinfos_nf << "  Current number of AICurlEasyRequest objects: " << AICurlEasyRequest_count << llendl;
  llinfos_nf << "  Current number of AICurlEasyRequestStateMachine objects: " << AICurlEasyRequestStateMachine_count << llendl;
#endif
  llinfos_nf << "  Current number of Responders: " << ResponderBase_count << llendl;
  llinfos_nf << "  Received HTTP bodies   LLSD / LLSD parse errors / non-LLSD: " << llsd_body_count << "/" << llsd_body_parse_error << "/" << raw_body_count << llendl;
  llinfos_nf << "  Received HTTP status codes: status (count) [...]: ";
  bool first = true;
  for (U32 index = 0; index < 100; ++index)
  {
	if (status_count[index] > 0)
	{
	  if (!first)
	  {
		llcont << ", ";
	  }
	  else
	  {
		first = false;
	  }
	  llcont << index2status(index) << " (" << status_count[index] << ')';
	}
  }
  llcont << llendl;
  llinfos_nf << "========= END OF CURL STATS =========" << llendl;
  // Leak tests.
  // There is one easy handle per CurlEasyHandle, and BufferedCurlEasyRequest is derived from that.
  // It is not allowed to create CurlEasyHandle (or CurlEasyRequest) directly, only by creating a BufferedCurlEasyRequest,
  // therefore the number of existing easy handles must equal the number of BufferedCurlEasyRequest objects.
  llassert(easy_handles == BufferedCurlEasyRequest_count);
  // Even more strict, BufferedCurlEasyRequest may not be created directly either, only as
  // base class of ThreadSafeBufferedCurlEasyRequest.
  llassert(BufferedCurlEasyRequest_count == ThreadSafeBufferedCurlEasyRequest_count);
  // Each AICurlEasyRequestStateMachine has one AICurlEasyRequest member.
  llassert(AICurlEasyRequest_count >= AICurlEasyRequestStateMachine_count);
  // AIFIXME: is this really always the case? And why?
  llassert(easy_handles <= S32(ResponderBase_count));
}

} // namespace AICurlInterface
//==================================================================================

//==================================================================================
// Local implementation.
//

namespace AICurlPrivate {

using AICurlInterface::Stats;

#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
// CURLOPT_DEBUGFUNCTION function.
extern int debug_callback(CURL*, curl_infotype infotype, char* buf, size_t size, void* user_ptr);
#endif

// THREAD-SAFE
void handle_multi_error(CURLMcode code) 
{
  Stats::multi_errors++;
  llinfos << "curl multi error detected: " << curl_multi_strerror(code) <<
	  "; (errors/calls = " << Stats::multi_errors << "/" << Stats::multi_calls << ")" << llendl;
}

//=============================================================================
// AICurlEasyRequest (base classes)
//

//-----------------------------------------------------------------------------
// CurlEasyHandle

// THREAD-SAFE
//static
void CurlEasyHandle::handle_easy_error(CURLcode code)
{
  char* error_buffer = LLThreadLocalData::tldata().mCurlErrorBuffer;
  llinfos << "curl easy error detected: " << curl_easy_strerror(code);
  if (error_buffer && *error_buffer != '\0')
  {
	llcont << ": " << error_buffer;
  }
  Stats::easy_errors++;
  llcont << "; (errors/calls = " << Stats::easy_errors << "/" << Stats::easy_calls << ")" << llendl;
}

// Throws AICurlNoEasyHandle.
CurlEasyHandle::CurlEasyHandle(void) : mActiveMultiHandle(NULL), mErrorBuffer(NULL), mQueuedForRemoval(false)
#ifdef DEBUG_CURLIO
	, mDebug(false)
#endif
#ifdef SHOW_ASSERT
	, mRemovedPerCommand(true)
#endif
{
  mEasyHandle = curl_easy_init();
#if 0
  // Fake curl_easy_init() failures: throw once every 10 times (for debugging purposes).
  static int count = 0;
  if (mEasyHandle && (++count % 10) == 5)
  {
    curl_easy_cleanup(mEasyHandle);
	mEasyHandle = NULL;
  }
#endif
  Stats::easy_init_calls++;
  if (!mEasyHandle)
  {
	Stats::easy_init_errors++;
	throw AICurlNoEasyHandle("curl_easy_init() returned NULL");
  }
}

#if 0 // Not used
CurlEasyHandle::CurlEasyHandle(CurlEasyHandle const& orig) : mActiveMultiHandle(NULL), mErrorBuffer(NULL)
#ifdef SHOW_ASSERT
		, mRemovedPerCommand(true)
#endif
{
  mEasyHandle = curl_easy_duphandle(orig.mEasyHandle);
  Stats::easy_init_calls++;
  if (!mEasyHandle)
  {
	Stats::easy_init_errors++;
	throw AICurlNoEasyHandle("curl_easy_duphandle() returned NULL");
  }
}
#endif

CurlEasyHandle::~CurlEasyHandle()
{
  llassert(!mActiveMultiHandle);
  curl_easy_cleanup(mEasyHandle);
  Stats::easy_cleanup_calls++;
#ifdef DEBUG_CURLIO
  if (mDebug)
  {
	debug_curl_remove_easy(mEasyHandle);
  }
#endif
}

//static
char* CurlEasyHandle::getTLErrorBuffer(void)
{
  LLThreadLocalData& tldata = LLThreadLocalData::tldata();
  if (!tldata.mCurlErrorBuffer)
  {
	tldata.mCurlErrorBuffer = new char[CURL_ERROR_SIZE];
  }
  return tldata.mCurlErrorBuffer;
}

void CurlEasyHandle::setErrorBuffer(void) const
{
  char* error_buffer = getTLErrorBuffer();
  if (mErrorBuffer != error_buffer)
  {
	mErrorBuffer = error_buffer;
	CURLcode res = curl_easy_setopt(mEasyHandle, CURLOPT_ERRORBUFFER, error_buffer);
	if (res != CURLE_OK)
	{
	  llwarns << "curl_easy_setopt(" << (void*)mEasyHandle << "CURLOPT_ERRORBUFFER, " << (void*)error_buffer << ") failed with error " << res << llendl;
	  mErrorBuffer = NULL;
	}
  }
  if (mErrorBuffer)
  {
	mErrorBuffer[0] = '\0';
  }
}

CURLcode CurlEasyHandle::getinfo_priv(CURLINFO info, void* data) const
{
  setErrorBuffer();
  return check_easy_code(curl_easy_getinfo(mEasyHandle, info, data));
}

char* CurlEasyHandle::escape(char* url, int length)
{
  return curl_easy_escape(mEasyHandle, url, length);
}

char* CurlEasyHandle::unescape(char* url, int inlength , int* outlength)
{
  return curl_easy_unescape(mEasyHandle, url, inlength, outlength);
}

CURLcode CurlEasyHandle::perform(void)
{
  llassert(!mActiveMultiHandle);
  setErrorBuffer();
  return check_easy_code(curl_easy_perform(mEasyHandle));
}

CURLcode CurlEasyHandle::pause(int bitmask)
{
  setErrorBuffer();
  return check_easy_code(curl_easy_pause(mEasyHandle, bitmask));
}

CURLMcode CurlEasyHandle::add_handle_to_multi(AICurlEasyRequest_wat& curl_easy_request_w, CURLM* multi)
{
  llassert_always(!mActiveMultiHandle && multi);
  mActiveMultiHandle = multi;
  CURLMcode res = check_multi_code(curl_multi_add_handle(multi, mEasyHandle));
  added_to_multi_handle(curl_easy_request_w);
  return res;
}

CURLMcode CurlEasyHandle::remove_handle_from_multi(AICurlEasyRequest_wat& curl_easy_request_w, CURLM* multi)
{
  llassert_always(mActiveMultiHandle && mActiveMultiHandle == multi);
  mActiveMultiHandle = NULL;
  CURLMcode res = check_multi_code(curl_multi_remove_handle(multi, mEasyHandle));
  removed_from_multi_handle(curl_easy_request_w);
  mPostField = NULL;
  return res;
}

void intrusive_ptr_add_ref(ThreadSafeBufferedCurlEasyRequest* threadsafe_curl_easy_request)
{
  threadsafe_curl_easy_request->mReferenceCount++;
}

void intrusive_ptr_release(ThreadSafeBufferedCurlEasyRequest* threadsafe_curl_easy_request)
{
  if (--threadsafe_curl_easy_request->mReferenceCount == 0)
  {
	delete threadsafe_curl_easy_request;
  }
}

CURLcode CurlEasyHandle::setopt(CURLoption option, long parameter)
{
  llassert((CURLOPTTYPE_LONG  <= option && option < CURLOPTTYPE_LONG  + 1000) ||
		   (sizeof(curl_off_t) == sizeof(long) &&
			CURLOPTTYPE_OFF_T <= option && option < CURLOPTTYPE_OFF_T + 1000));
  llassert(!mActiveMultiHandle);
  setErrorBuffer();
  return check_easy_code(curl_easy_setopt(mEasyHandle, option, parameter));
}

// The standard requires that sizeof(long) < sizeof(long long), so it's safe to overload like this.
// We assume that one of them is 64 bit, the size of curl_off_t.
CURLcode CurlEasyHandle::setopt(CURLoption option, long long parameter)
{
  llassert(sizeof(curl_off_t) == sizeof(long long) &&
		   CURLOPTTYPE_OFF_T <= option && option < CURLOPTTYPE_OFF_T + 1000);
  llassert(!mActiveMultiHandle);
  setErrorBuffer();
  return check_easy_code(curl_easy_setopt(mEasyHandle, option, parameter));
}

CURLcode CurlEasyHandle::setopt(CURLoption option, void const* parameter)
{
  llassert(CURLOPTTYPE_OBJECTPOINT <= option && option < CURLOPTTYPE_OBJECTPOINT + 1000);
  setErrorBuffer();
  return check_easy_code(curl_easy_setopt(mEasyHandle, option, parameter));
}

#define DEFINE_FUNCTION_SETOPT1(function_type, opt1) \
	CURLcode CurlEasyHandle::setopt(CURLoption option, function_type parameter) \
	{ \
	  llassert(option == opt1); \
	  setErrorBuffer(); \
	  return check_easy_code(curl_easy_setopt(mEasyHandle, option, parameter)); \
	}

#define DEFINE_FUNCTION_SETOPT3(function_type, opt1, opt2, opt3) \
	CURLcode CurlEasyHandle::setopt(CURLoption option, function_type parameter) \
	{ \
	  llassert(option == opt1 || option == opt2 || option == opt3); \
	  setErrorBuffer(); \
	  return check_easy_code(curl_easy_setopt(mEasyHandle, option, parameter)); \
	}

#define DEFINE_FUNCTION_SETOPT4(function_type, opt1, opt2, opt3, opt4) \
	CURLcode CurlEasyHandle::setopt(CURLoption option, function_type parameter) \
	{ \
	  llassert(option == opt1 || option == opt2 || option == opt3 || option == opt4); \
	  setErrorBuffer(); \
	  return check_easy_code(curl_easy_setopt(mEasyHandle, option, parameter)); \
	}

DEFINE_FUNCTION_SETOPT1(curl_debug_callback, CURLOPT_DEBUGFUNCTION)
DEFINE_FUNCTION_SETOPT4(curl_write_callback, CURLOPT_HEADERFUNCTION, CURLOPT_WRITEFUNCTION, CURLOPT_INTERLEAVEFUNCTION, CURLOPT_READFUNCTION)
//DEFINE_FUNCTION_SETOPT1(curl_read_callback, CURLOPT_READFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_ssl_ctx_callback, CURLOPT_SSL_CTX_FUNCTION)
DEFINE_FUNCTION_SETOPT3(curl_conv_callback, CURLOPT_CONV_FROM_NETWORK_FUNCTION, CURLOPT_CONV_TO_NETWORK_FUNCTION, CURLOPT_CONV_FROM_UTF8_FUNCTION)
#if 0 // Not used by the viewer.
DEFINE_FUNCTION_SETOPT1(curl_progress_callback, CURLOPT_PROGRESSFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_seek_callback, CURLOPT_SEEKFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_ioctl_callback, CURLOPT_IOCTLFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_sockopt_callback, CURLOPT_SOCKOPTFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_opensocket_callback, CURLOPT_OPENSOCKETFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_closesocket_callback, CURLOPT_CLOSESOCKETFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_sshkeycallback, CURLOPT_SSH_KEYFUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_chunk_bgn_callback, CURLOPT_CHUNK_BGN_FUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_chunk_end_callback, CURLOPT_CHUNK_END_FUNCTION)
DEFINE_FUNCTION_SETOPT1(curl_fnmatch_callback, CURLOPT_FNMATCH_FUNCTION)
#endif

//-----------------------------------------------------------------------------
// CurlEasyRequest

void CurlEasyRequest::setoptString(CURLoption option, std::string const& value)
{
  llassert(!gSetoptParamsNeedDup);
  setopt(option, value.c_str());
}

void CurlEasyRequest::setPut(U32 size, bool keepalive)
{
  DoutCurl("PUT size is " << size << " bytes.");
  mContentLength = size;

  // The server never replies with 100-continue, so suppress the "Expect: 100-continue" header that libcurl adds by default.
  addHeader("Expect:");
  if (size > 0 && keepalive)
  {
	addHeader("Connection: keep-alive");
	addHeader("Keep-alive: 300");
  }
  setopt(CURLOPT_UPLOAD, 1);
  setopt(CURLOPT_INFILESIZE, size);
}

void CurlEasyRequest::setPost(AIPostFieldPtr const& postdata, U32 size, bool keepalive)
{
  llassert_always(postdata->data());

  DoutCurl("POST size is " << size << " bytes: \"" << libcwd::buf2str(postdata->data(), size) << "\".");
  setPostField(postdata);		// Make sure the data stays around until we don't need it anymore.

  setPost_raw(size, postdata->data(), keepalive);
}

void CurlEasyRequest::setPost_raw(U32 size, char const* data, bool keepalive)
{
  if (!data)
  {
	// data == NULL when we're going to read the data using CURLOPT_READFUNCTION.
	DoutCurl("POST size is " << size << " bytes.");
  }
  mContentLength = size;

  // The server never replies with 100-continue, so suppress the "Expect: 100-continue" header that libcurl adds by default.
  addHeader("Expect:");
  if (size > 0 && keepalive)
  {
	addHeader("Connection: keep-alive");
	addHeader("Keep-alive: 300");
  }
  setopt(CURLOPT_POSTFIELDSIZE, size);
  setopt(CURLOPT_POSTFIELDS, data);			// Implies CURLOPT_POST
}

//static
size_t CurlEasyRequest::headerCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  ThreadSafeBufferedCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mHeaderCallback(ptr, size, nmemb, self->mHeaderCallbackUserData);
}

void CurlEasyRequest::setHeaderCallback(curl_write_callback callback, void* userdata)
{
  mHeaderCallback = callback;
  mHeaderCallbackUserData = userdata;
  setopt(CURLOPT_HEADERFUNCTION, callback ? &CurlEasyRequest::headerCallback : NULL);
  setopt(CURLOPT_WRITEHEADER, userdata ? this : NULL);
}

//static
size_t CurlEasyRequest::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  ThreadSafeBufferedCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mWriteCallback(ptr, size, nmemb, self->mWriteCallbackUserData);
}

void CurlEasyRequest::setWriteCallback(curl_write_callback callback, void* userdata)
{
  mWriteCallback = callback;
  mWriteCallbackUserData = userdata;
  setopt(CURLOPT_WRITEFUNCTION, callback ? &CurlEasyRequest::writeCallback : NULL);
  setopt(CURLOPT_WRITEDATA, userdata ? this : NULL);
}

//static
size_t CurlEasyRequest::readCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  ThreadSafeBufferedCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mReadCallback(ptr, size, nmemb, self->mReadCallbackUserData);
}

void CurlEasyRequest::setReadCallback(curl_read_callback callback, void* userdata)
{
  mReadCallback = callback;
  mReadCallbackUserData = userdata;
  setopt(CURLOPT_READFUNCTION, callback ? &CurlEasyRequest::readCallback : NULL);
  setopt(CURLOPT_READDATA, userdata ? this : NULL);
}

//static
CURLcode CurlEasyRequest::SSLCtxCallback(CURL* curl, void* sslctx, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  ThreadSafeBufferedCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mSSLCtxCallback(curl, sslctx, self->mSSLCtxCallbackUserData);
}

void CurlEasyRequest::setSSLCtxCallback(curl_ssl_ctx_callback callback, void* userdata)
{
  mSSLCtxCallback = callback;
  mSSLCtxCallbackUserData = userdata;
  setopt(CURLOPT_SSL_CTX_FUNCTION, callback ? &CurlEasyRequest::SSLCtxCallback : NULL);
  setopt(CURLOPT_SSL_CTX_DATA, this);
}

#define llmaybewarns lllog(LLApp::isExiting() ? LLError::LEVEL_INFO : LLError::LEVEL_WARN, NULL, NULL, false, true)

static size_t noHeaderCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  llmaybewarns << "Calling noHeaderCallback(); curl session aborted." << llendl;
  return 0;							// Cause a CURLE_WRITE_ERROR
}

static size_t noWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  llmaybewarns << "Calling noWriteCallback(); curl session aborted." << llendl;
  return 0;							// Cause a CURLE_WRITE_ERROR
}

static size_t noReadCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  llmaybewarns << "Calling noReadCallback(); curl session aborted." << llendl;
  return CURL_READFUNC_ABORT;		// Cause a CURLE_ABORTED_BY_CALLBACK
}

static CURLcode noSSLCtxCallback(CURL* curl, void* sslctx, void* parm)
{
  llmaybewarns << "Calling noSSLCtxCallback(); curl session aborted." << llendl;
  return CURLE_ABORTED_BY_CALLBACK;
}

void CurlEasyRequest::revokeCallbacks(void)
{
  if (mHeaderCallback == &noHeaderCallback &&
	  mWriteCallback == &noWriteCallback &&
	  mReadCallback == &noReadCallback &&
	  mSSLCtxCallback == &noSSLCtxCallback)
  {
	// Already revoked.
	return;
  }
  mHeaderCallback = &noHeaderCallback;
  mWriteCallback = &noWriteCallback;
  mReadCallback = &noReadCallback;
  mSSLCtxCallback = &noSSLCtxCallback;
  if (active() && !no_warning())
  {
	llwarns << "Revoking callbacks on a still active CurlEasyRequest object!" << llendl;
  }
  curl_easy_setopt(getEasyHandle(), CURLOPT_HEADERFUNCTION, &noHeaderCallback);
  curl_easy_setopt(getEasyHandle(), CURLOPT_WRITEHEADER, &noWriteCallback);
  curl_easy_setopt(getEasyHandle(), CURLOPT_READFUNCTION, &noReadCallback);
  curl_easy_setopt(getEasyHandle(), CURLOPT_SSL_CTX_FUNCTION, &noSSLCtxCallback);
}

CurlEasyRequest::~CurlEasyRequest()
{
  // If the CurlEasyRequest object is destructed then we need to revoke all callbacks, because
  // we can't set the lock anymore, and neither will mHeaderCallback, mWriteCallback etc,
  // be available anymore.
  send_handle_events_to(NULL);
  revokeCallbacks();
  if (mPerServicePtr)
  {
	 AIPerService::release(mPerServicePtr);
  }
  // This wasn't freed yet if the request never finished.
  curl_slist_free_all(mHeaders);
}

void CurlEasyRequest::resetState(void)
{
  // This function should not revoke the event call backs!
  revokeCallbacks();
  reset();
  curl_slist_free_all(mHeaders);
  mHeaders = NULL;
  mTimeoutPolicy = NULL;
  mTimeout = NULL;
  mHandleEventsTarget = NULL;
  mResult = CURLE_FAILED_INIT;
  applyDefaultOptions();
}

void CurlEasyRequest::addHeader(char const* header)
{
  llassert(!mTimeoutPolicy);	// Cannot add a header after calling finalizeRequest.
  mHeaders = curl_slist_append(mHeaders, header);
}

void CurlEasyRequest::addHeaders(AIHTTPHeaders const& headers)
{
  llassert(!mTimeoutPolicy);	// Cannot add headers after calling finalizeRequest.
  headers.append_to(mHeaders);
}

void CurlEasyRequest::applyProxySettings(void)
{
  LLProxy& proxy = *LLProxy::getInstance();

  // Do a faster unlocked check to see if we are supposed to proxy.
  if (proxy.HTTPProxyEnabled())
  {
	// We think we should proxy, read lock the shared proxy members.
	LLProxy::Shared_crat proxy_r(proxy.shared_lockobj());

	// Now test again to verify that the proxy wasn't disabled between the first check and the lock.
	if (proxy.HTTPProxyEnabled())
	{
	  setopt(CURLOPT_PROXY, proxy.getHTTPProxy(proxy_r).getIPString().c_str());
	  setopt(CURLOPT_PROXYPORT, proxy.getHTTPProxy(proxy_r).getPort());

	  if (proxy.getHTTPProxyType(proxy_r) == LLPROXY_SOCKS)
	  {
		setopt(CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
		if (proxy.getSelectedAuthMethod(proxy_r) == METHOD_PASSWORD)
		{
		  std::string auth_string = proxy.getSocksUser(proxy_r) + ":" + proxy.getSocksPwd(proxy_r);
		  setopt(CURLOPT_PROXYUSERPWD, auth_string.c_str());
		}
	  }
	  else
	  {
		setopt(CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
	  }
	}
  }
}

//static
CURLcode CurlEasyRequest::curlCtxCallback(CURL* curl, void* sslctx, void* parm)
{
  DoutEntering(dc::curl, "CurlEasyRequest::curlCtxCallback((CURL*)" << (void*)curl << ", " << sslctx << ", " << parm << ")");
  SSL_CTX* ctx = (SSL_CTX*)sslctx;
  // Turn off TLS v1.1 (which is not supported anyway by Linden Lab) because otherwise we fail to connect.
  // Also turn off SSL v2, which is highly broken and strongly discouraged[1].
  // [1] http://www.openssl.org/docs/ssl/SSL_CTX_set_options.html#SECURE_RENEGOTIATION
  long options = SSL_OP_NO_SSLv2;
#ifdef SSL_OP_NO_TLSv1_1	// Only defined for openssl version 1.0.1 and up.
  if (need_renegotiation_hack)
  {
	// This option disables openssl to use TLS version 1.1.
	// The Linden Lab servers don't support TLS versions later than 1.0, and libopenssl-1.0.1-beta1 up till and including
	// libopenssl-1.0.1c have a bug (or feature?) where (re)negotiation fails (see http://rt.openssl.org/Ticket/Display.html?id=2828),
	// causing the connection to fail completely without this hack.
	// For a commandline test of the same, observe the difference between:
	// openssl s_client            -connect login.agni.lindenlab.com:443 -CAfile packaged/app_settings/CA.pem -debug
	// which gets no response from the server after sending the initial data, and
	// openssl s_client -no_tls1_1 -connect login.agni.lindenlab.com:443 -CAfile packaged/app_settings/CA.pem -debug
	// which finishes the negotiation and ends with 'Verify return code: 0 (ok)'
	options |= SSL_OP_NO_TLSv1_1;
  }
#else
  // This is expected when you compile against the headers of a version < 1.0.1 and then link at runtime with version >= 1.0.1.
  // Don't do that.
  llassert_always(!need_renegotiation_hack);
#endif
  SSL_CTX_set_options(ctx, options);
  return CURLE_OK;
}

void CurlEasyRequest::applyDefaultOptions(void)
{
  CertificateAuthority_rat CertificateAuthority_r(gCertificateAuthority);
  setoptString(CURLOPT_CAINFO, CertificateAuthority_r->file);
  setSSLCtxCallback(&curlCtxCallback, NULL);
  setopt(CURLOPT_NOSIGNAL, 1);
  // Cache DNS look ups an hour. If we set it smaller we risk frequent connect timeouts in cases where DNS look ups are slow.
  setopt(CURLOPT_DNS_CACHE_TIMEOUT, 3600);
  // Only resolve to IPV4.
  // Rationale: if a host resolves to both, ipv4 and ipv6, then this stops libcurl from
  // using the ipv6 address. If we don't do that then libcurl first attempts to connect
  // to the ipv4 IP number (using only HALF the connect timeout we passed to it!) and if
  // that fails try the ipv6 IP number, which then most likely fails with "network unreachable".
  // Then libcurl immediately returns with just the ipv6 error as result masking the real problem.
  // Since the viewer doesn't support IPv6 at least for UDP services, and there are no
  // transition plans to IPv6 anywhere at this moment, the easiest way to get rid of this
  // problem is by simply not falling back to ipv6.
  setopt(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
  // Disable SSL/TLS session caching; some servers (aka id.secondlife.com) refuse connections when session ids are enabled.
  setopt(CURLOPT_SSL_SESSIONID_CACHE, 0);
  // Set the CURL options for either SOCKS or HTTP proxy.
  applyProxySettings();
  // Cause libcurl to print all it's I/O traffic on the debug channel.
  Debug(
	if (dc::curlio.is_on())
	{
	  setopt(CURLOPT_VERBOSE, 1);
	  setopt(CURLOPT_DEBUGFUNCTION, &debug_callback);
	  setopt(CURLOPT_DEBUGDATA, this);
	}
  );
}

void CurlEasyRequest::finalizeRequest(std::string const& url, AIHTTPTimeoutPolicy const& policy, AICurlEasyRequestStateMachine* state_machine)
{
  DoutCurlEntering("CurlEasyRequest::finalizeRequest(\"" << url << "\", " << policy.name() << ", " << (void*)state_machine << ")");
  llassert(!mTimeoutPolicy);		// May only call finalizeRequest once!
  mResult = CURLE_FAILED_INIT;		// General error code; the final result code is stored here by MultiHandle::check_msg_queue when msg is CURLMSG_DONE.
#ifdef SHOW_ASSERT
  // Do a sanity check on the headers.
  int content_type_count = 0;
  for (curl_slist* list = mHeaders; list; list = list->next)
  {
	if (strncmp(list->data, "Content-Type:", 13) == 0)
	{
	  ++content_type_count;
	}
  }
  if (content_type_count > 1)
  {
	llwarns << "Requesting: \"" << url << "\": " << content_type_count << " Content-Type: headers!" << llendl;
  }
#endif
  setopt(CURLOPT_HTTPHEADER, mHeaders);
  setoptString(CURLOPT_URL, url);
  llassert(!mPerServicePtr);
  mLowercaseServicename = AIPerService::extract_canonical_servicename(url);
  mTimeoutPolicy = &policy;
  state_machine->setTotalDelayTimeout(policy.getTotalDelay());
  // The following line is a bit tricky: we store a pointer to the object without increasing its reference count.
  // Of course we could increment the reference count, but that doesn't help much: if then this pointer would
  // get "lost" we'd have a memory leak. Either way we must make sure that it is impossible that this pointer
  // will be used if the object is deleted [In fact, since this is finalizeRequest() and not addRequest(),
  // incrementing the reference counter would be wrong: if addRequest() is never called then the object is
  // destroyed shortly after and this pointer is never even used.]
  // This pointer is used in MultiHandle::check_msg_queue, which means that addRequest() was called and
  // the reference counter was increased and the object is being kept alive, see the comments above
  // command_queue in aicurlthread.cpp. In fact, this object survived until MultiHandle::add_easy_request
  // was called and is kept alive by MultiHandle::mAddedEasyRequests. The only way to get deleted after
  // that is when MultiHandle::remove_easy_request is called, which first removes the easy handle from
  // the multi handle. So that it's (hopefully) no longer possible that info_read() in
  // MultiHandle::check_msg_queue returns this easy handle, after the object is destroyed by deleting
  // it from MultiHandle::mAddedEasyRequests.
  setopt(CURLOPT_PRIVATE, get_lockobj());
}

// AIFIXME: Doing this only when it is actually being added assures that the first curl easy handle that is
// // being added for a particular host will be the one getting extra 'DNS lookup' connect time.
// // However, if another curl easy handle for the same host is added immediately after, it will
// // get less connect time, while it still (also) has to wait for this DNS lookup.
void CurlEasyRequest::set_timeout_opts(void)
{
  setopt(CURLOPT_CONNECTTIMEOUT, mTimeoutPolicy->getConnectTimeout(getLowercaseHostname()));
  setopt(CURLOPT_TIMEOUT, mTimeoutPolicy->getCurlTransaction());
}

void CurlEasyRequest::create_timeout_object(void)
{
  ThreadSafeBufferedCurlEasyRequest* lockobj = NULL;
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
  lockobj = static_cast<BufferedCurlEasyRequest*>(this)->get_lockobj();
#endif
  mTimeout = new curlthread::HTTPTimeout(mTimeoutPolicy, lockobj);
}

LLPointer<curlthread::HTTPTimeout>& CurlEasyRequest::get_timeout_object(void)
{
  if (mTimeoutIsOrphan)
  {
	mTimeoutIsOrphan = false;
	llassert_always(mTimeout);
  }
  else
  {
	create_timeout_object();
  }
  return mTimeout;
}

void CurlEasyRequest::print_curl_timings(void) const
{
  double t;
  getinfo(CURLINFO_NAMELOOKUP_TIME, &t);
  DoutCurl("CURLINFO_NAMELOOKUP_TIME = " << t);
  getinfo(CURLINFO_CONNECT_TIME, &t);
  DoutCurl("CURLINFO_CONNECT_TIME = " << t);
  getinfo(CURLINFO_APPCONNECT_TIME, &t);
  DoutCurl("CURLINFO_APPCONNECT_TIME = " << t);
  getinfo(CURLINFO_PRETRANSFER_TIME, &t);
  DoutCurl("CURLINFO_PRETRANSFER_TIME = " << t);
  getinfo(CURLINFO_STARTTRANSFER_TIME, &t);
  DoutCurl("CURLINFO_STARTTRANSFER_TIME = " << t);
}

void CurlEasyRequest::getTransferInfo(AITransferInfo* info)
{
  // Curl explicitly demands a double for these info's.
  double size, total_time, speed;
  getinfo(CURLINFO_SIZE_DOWNLOAD, &size);
  getinfo(CURLINFO_TOTAL_TIME, &total_time);
  getinfo(CURLINFO_SPEED_DOWNLOAD, &speed);
  // Convert to F64.
  info->mSizeDownload = size;
  info->mTotalTime = total_time;
  info->mSpeedDownload = speed;
}

void CurlEasyRequest::getResult(CURLcode* result, AITransferInfo* info)
{
  *result = mResult;
  if (info && mResult != CURLE_FAILED_INIT)
  {
	getTransferInfo(info);
  }
}

void CurlEasyRequest::added_to_multi_handle(AICurlEasyRequest_wat& curl_easy_request_w)
{
  if (mHandleEventsTarget)
	mHandleEventsTarget->added_to_multi_handle(curl_easy_request_w);
}

void CurlEasyRequest::finished(AICurlEasyRequest_wat& curl_easy_request_w)
{
  if (mHandleEventsTarget)
	mHandleEventsTarget->finished(curl_easy_request_w);
}

void CurlEasyRequest::removed_from_multi_handle(AICurlEasyRequest_wat& curl_easy_request_w)
{
  if (mHandleEventsTarget)
	mHandleEventsTarget->removed_from_multi_handle(curl_easy_request_w);
}

void CurlEasyRequest::bad_file_descriptor(AICurlEasyRequest_wat& curl_easy_request_w)
{
  if (mHandleEventsTarget)
	mHandleEventsTarget->bad_file_descriptor(curl_easy_request_w);
}

#ifdef SHOW_ASSERT
void CurlEasyRequest::queued_for_removal(AICurlEasyRequest_wat& curl_easy_request_w)
{
  if (mHandleEventsTarget)
	mHandleEventsTarget->queued_for_removal(curl_easy_request_w);
}
#endif

AIPerServicePtr CurlEasyRequest::getPerServicePtr(void)
{
  if (!mPerServicePtr)
  {
	// mPerServicePtr is really just a speed-up cache.
	// The reason we can cache it is because mLowercaseServicename is only set
	// in finalizeRequest which may only be called once: it never changes.
	mPerServicePtr = AIPerService::instance(mLowercaseServicename);
  }
  return mPerServicePtr;
}

bool CurlEasyRequest::removeFromPerServiceQueue(AICurlEasyRequest const& easy_request, AICapabilityType capability_type) const
{
  // Note that easy_request (must) represent(s) this object; it's just passed for convenience.
  return mPerServicePtr && PerService_wat(*mPerServicePtr)->cancel(easy_request, capability_type);
}

std::string CurlEasyRequest::getLowercaseHostname(void) const
{
  return mLowercaseServicename.substr(0, mLowercaseServicename.find_last_of(':'));
}

//-----------------------------------------------------------------------------
// BufferedCurlEasyRequest

static int const HTTP_REDIRECTS_DEFAULT = 10;

LLChannelDescriptors const BufferedCurlEasyRequest::sChannels;
LLMutex BufferedCurlEasyRequest::sResponderCallbackMutex;
bool BufferedCurlEasyRequest::sShuttingDown = false;
AIAverage BufferedCurlEasyRequest::sHTTPBandwidth(25);

BufferedCurlEasyRequest::BufferedCurlEasyRequest() :
	mRequestTransferedBytes(0), mTotalRawBytes(0), mStatus(HTTP_INTERNAL_ERROR_OTHER), mBufferEventsTarget(NULL), mCapabilityType(number_of_capability_types)
{
  AICurlInterface::Stats::BufferedCurlEasyRequest_count++;
}

#define llmaybeerrs lllog(LLApp::isRunning() ? LLError::LEVEL_ERROR : LLError::LEVEL_WARN, NULL, NULL, false, true)

BufferedCurlEasyRequest::~BufferedCurlEasyRequest()
{
  send_buffer_events_to(NULL);
  revokeCallbacks();
  if (mResponder)
  {	
	// If the responder is still alive, then that means that BufferedCurlEasyRequest::processOutput was
	// never called, which means that the removed_from_multi_handle event never happened.
	// This is definitely an internal error as it can only happen when libcurl is too slow,
	// in which case AICurlEasyRequestStateMachine::mTimer times out, but that already
	// calls BufferedCurlEasyRequest::timed_out().
	llmaybeerrs << "Calling ~BufferedCurlEasyRequest() with active responder!" << llendl;
	if (!LLApp::isRunning())
	{
	  // It might happen if some BufferedCurlEasyRequest escaped clean up somehow :/
	  mResponder = NULL;
	}
	else
	{
	  // User chose to continue.
	  timed_out();
	}
  }
  --AICurlInterface::Stats::BufferedCurlEasyRequest_count;
}

void BufferedCurlEasyRequest::timed_out(void)
{
  mResponder->finished(CURLE_OK, HTTP_INTERNAL_ERROR_CURL_LOCKUP, "Request timeout, aborted.", sChannels, mOutput);
  if (mResponder->needsHeaders())
  {
	send_buffer_events_to(NULL);	// Revoke buffer events: we send them to the responder.
  }
  mResponder = NULL;
}

void BufferedCurlEasyRequest::bad_socket(void)
{
  mResponder->finished(CURLE_OK, HTTP_INTERNAL_ERROR_CURL_BADSOCKET, "File descriptor went bad! Aborted.", sChannels, mOutput);
  if (mResponder->needsHeaders())
  {
	send_buffer_events_to(NULL);	// Revoke buffer events: we send them to the responder.
  }
  mResponder = NULL;
}

#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
static AIPerServicePtr sConnections[64];

void BufferedCurlEasyRequest::connection_established(int connectionnr)
{
  PerService_rat per_service_r(*mPerServicePtr);
  int n = per_service_r->connection_established();
  llassert(sConnections[connectionnr] == NULL);		// Only one service can use a connection at a time.
  llassert_always(connectionnr < 64);
  sConnections[connectionnr] = mPerServicePtr;
  Dout(dc::curlio, (void*)get_lockobj() << " Connection established (#" << connectionnr << "). Now " << n << " connections [" << (void*)&*per_service_r << "].");
  llassert(sConnections[connectionnr] != NULL);
}

void BufferedCurlEasyRequest::connection_closed(int connectionnr)
{
  if (sConnections[connectionnr] == NULL)
  {
	Dout(dc::curlio, "Closing connection that never connected (#" << connectionnr << ").");
	return;
  }
  PerService_rat per_service_r(*sConnections[connectionnr]);
  int n = per_service_r->connection_closed();
  sConnections[connectionnr] = NULL;
  Dout(dc::curlio, (void*)get_lockobj() << " Connection closed (#" << connectionnr << "); " << n << " connections remaining [" << (void*)&*per_service_r << "].");
}
#endif

void BufferedCurlEasyRequest::resetState(void)
{
  llassert(!mResponder);

  // Call base class implementation.
  CurlEasyRequest::resetState();

  // Reset local variables.
  mOutput.reset();
  mInput.reset();
  mRequestTransferedBytes = 0;
  mTotalRawBytes = 0;
  mBufferEventsTarget = NULL;
  mStatus = HTTP_INTERNAL_ERROR_OTHER;
}

void BufferedCurlEasyRequest::print_diagnostics(CURLcode code)
{
  char* eff_url;
  getinfo(CURLINFO_EFFECTIVE_URL, &eff_url);
  if (code == CURLE_OPERATION_TIMEDOUT)
  {
	// mTimeout SHOULD always be set, but I see no reason not to test it, as
	// this is far from the code that guaranteeds that it is set.
	if (mTimeout)
	{
	  mTimeout->print_diagnostics(this, eff_url);
	}
  }
  else
  {
	llwarns << "Curl returned error code " << code << " (" << curl_easy_strerror(code) << ") for HTTP request to \"" << eff_url << "\"." << llendl;
  }
}

ThreadSafeBufferedCurlEasyRequest* BufferedCurlEasyRequest::get_lockobj(void)
{
  return static_cast<ThreadSafeBufferedCurlEasyRequest*>(AIThreadSafeSimple<BufferedCurlEasyRequest>::wrapper_cast(this));
}

ThreadSafeBufferedCurlEasyRequest const* BufferedCurlEasyRequest::get_lockobj(void) const
{
  return static_cast<ThreadSafeBufferedCurlEasyRequest const*>(AIThreadSafeSimple<BufferedCurlEasyRequest>::wrapper_cast(this));
}

void BufferedCurlEasyRequest::prepRequest(AICurlEasyRequest_wat& curl_easy_request_w, AIHTTPHeaders const& headers, LLHTTPClient::ResponderPtr responder)
{
  mInput.reset(new LLBufferArray);
  mInput->setThreaded(true);
  mLastRead = NULL;

  mOutput.reset(new LLBufferArray);
  mOutput->setThreaded(true);

  ThreadSafeBufferedCurlEasyRequest* lockobj = get_lockobj();
  curl_easy_request_w->setWriteCallback(&curlWriteCallback, lockobj);
  curl_easy_request_w->setReadCallback(&curlReadCallback, lockobj);
  curl_easy_request_w->setHeaderCallback(&curlHeaderCallback, lockobj);

  bool allow_cookies = headers.hasHeader("Cookie");
  // Allow up to ten redirects.
  if (responder->followRedir())
  {
	curl_easy_request_w->setopt(CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_request_w->setopt(CURLOPT_MAXREDIRS, HTTP_REDIRECTS_DEFAULT);
	// This is needed (at least) for authentication after temporary redirection
	// to id.secondlife.com for marketplace.secondlife.com.
	allow_cookies = true;
  }
  if (responder->forbidReuse())
  {
	curl_easy_request_w->setopt(CURLOPT_FORBID_REUSE, 1);
  }
  if (allow_cookies)
  {
	// Given an empty or non-existing file or by passing the empty string (""),
	// this option will enable cookies for this curl handle, making it understand
	// and parse received cookies and then use matching cookies in future requests.
	curl_easy_request_w->setopt(CURLOPT_COOKIEFILE, "");
  }

  // Keep responder alive.
  mResponder = responder;
  // Cache capability type, because it will be needed even after the responder was removed.
  mCapabilityType = responder->capability_type();

  // Send header events to responder if needed.
  if (mResponder->needsHeaders())
  {
	  send_buffer_events_to(mResponder.get());
  }

  // Add extra headers.
  curl_easy_request_w->addHeaders(headers);
}

//-----------------------------------------------------------------------------
// CurlMultiHandle

LLAtomicU32 CurlMultiHandle::sTotalMultiHandles;

CurlMultiHandle::CurlMultiHandle(void)
{
  DoutEntering(dc::curl, "CurlMultiHandle::CurlMultiHandle() [" << (void*)this << "].");
  mMultiHandle = curl_multi_init();
  Stats::multi_calls++;
  if (!mMultiHandle)
  {
	Stats::multi_errors++;
	throw AICurlNoMultiHandle("curl_multi_init() returned NULL");
  }
  sTotalMultiHandles++;
}

CurlMultiHandle::~CurlMultiHandle()
{
  curl_multi_cleanup(mMultiHandle);
  Stats::multi_calls++;
#ifdef CWDEBUG
  int total = --sTotalMultiHandles;
  Dout(dc::curl, "Called CurlMultiHandle::~CurlMultiHandle() [" << (void*)this << "], " << total << " remaining.");
#else
  --sTotalMultiHandles;
#endif
}

} // namespace AICurlPrivate

#if LL_LINUX
extern "C" {

// Keep linker happy.
const SSL_METHOD *SSLv2_client_method(void)
{
  // Never used.
  llassert_always(false);
  return NULL;
}

}
#endif

