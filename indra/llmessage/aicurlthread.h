/**
 * @file aicurlthread.h
 * @brief Thread safe wrapper for libcurl.
 *
 * Copyright (c) 2012, Aleric Inglewood.
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
 *   28/04/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AICURLTHREAD_H
#define AICURLTHREAD_H

#include "aicurl.h"
#include <vector>

#undef AICurlPrivate

namespace AICurlPrivate {
namespace curlthread {

extern U32 curl_max_total_concurrent_connections;

class PollSet;

// For ordering a std::set with AICurlEasyRequest objects.
struct AICurlEasyRequestCompare {
  bool operator()(AICurlEasyRequest const& h1, AICurlEasyRequest const& h2) { return h1.get() < h2.get(); }
};

//-----------------------------------------------------------------------------
// MultiHandle

// This class adds member functions that will only be called from the AICurlThread thread.
// This class guarantees that all added easy handles will be removed from the multi handle
// before the multi handle is cleaned up, as is required by libcurl.
class MultiHandle : public CurlMultiHandle
{
  public:
	MultiHandle(void);
	~MultiHandle();

	// Add/remove an easy handle to/from a multi session.
	bool add_easy_request(AICurlEasyRequest const& easy_request, bool from_queue);
	CURLMcode remove_easy_request(AICurlEasyRequest const& easy_request, bool as_per_command = false);

	// Reads/writes available data from a particular socket (non-blocking).
	CURLMcode socket_action(curl_socket_t sockfd, int ev_bitmask);

	// Set data to association with an internal socket.
	CURLMcode assign(curl_socket_t sockfd, void* sockptr);

	// Read multi stack informationals.
	CURLMsg const* info_read(int* msgs_in_queue) const;

  private:
	typedef std::set<AICurlEasyRequest, AICurlEasyRequestCompare> addedEasyRequests_type;
	addedEasyRequests_type mAddedEasyRequests;	// All easy requests currently added to the multi handle.
	long mTimeout;								// The last timeout in ms as set by the callback CURLMOPT_TIMERFUNCTION.
	static LLAtomicU32 sTotalAdded;				// The (sum of the) size of mAddedEasyRequests (of every MultiHandle, but there is only one).

  private:
	// Store result and trigger events for easy request.
	void finish_easy_request(AICurlEasyRequest const& easy_request, CURLcode result);
	// Remove easy request at iter (must exist).
	// Note that it's possible that a new request from a AIPerService::mQueuedRequests is inserted before iter.
	CURLMcode remove_easy_request(addedEasyRequests_type::iterator const& iter, bool as_per_command);

    static int socket_callback(CURL* easy, curl_socket_t s, int action, void* userp, void* socketp);
    static int timer_callback(CURLM* multi, long timeout_ms, void* userp);

  public:
	// Returns how long to wait for socket action before calling socket_action(CURL_SOCKET_TIMEOUT, 0), in ms.
	int getTimeout(void) const { return mTimeout; }

	// We slept delta_ms instead of mTimeout ms. Update mTimeout to be the remaining time.
	void update_timeout(long delta_ms) { mTimeout -= delta_ms; }

	// This is called before sleeping, after calling (one or more times) socket_action.
	void check_msg_queue(void);

	// Called from the main loop every time select() timed out.
	void handle_stalls(void);

	// Return the total number of added curl requests.
	static U32 total_added_size(void) { return sTotalAdded; }

	// Return true if we reached the global maximum number of connections.
	static bool added_maximum(void) { return sTotalAdded >= curl_max_total_concurrent_connections; }

  public:
	//-----------------------------------------------------------------------------
	// Curl socket administration:

	PollSet* mReadPollSet;
	PollSet* mWritePollSet;
};

} // namespace curlthread
} // namespace AICurlPrivate

// Thread safe, noncopyable curl multi handle.
// This class wraps MultiHandle for thread-safety.
// AIThreadSafeSingleThreadDC cannot be copied, but that is OK as we don't need that (or want that);
// this class provides a thread-local singleton (exactly one instance per thread), and because it
// can't be copied, that guarantees that the CURLM* handle is never used concurrently, which is
// not allowed by libcurl.
class AICurlMultiHandle : public AIThreadSafeSingleThreadDC<AICurlPrivate::curlthread::MultiHandle>, public LLThreadLocalDataMember {
  public:
	static AICurlMultiHandle& getInstance(void);
	static void destroyInstance(void);
  private:
	// Use getInstance().
	AICurlMultiHandle(void) { }
};

typedef AISTAccessConst<AICurlPrivate::curlthread::MultiHandle> AICurlMultiHandle_rat;
typedef AISTAccess<AICurlPrivate::curlthread::MultiHandle> AICurlMultiHandle_wat;

#define AICurlPrivate DONTUSE_AICurlPrivate

#endif
