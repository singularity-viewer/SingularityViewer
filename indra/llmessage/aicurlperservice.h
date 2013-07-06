/**
 * @file aicurlperservice.h
 * @brief Definition of class AIPerService
 *
 * Copyright (c) 2012, 2013, Aleric Inglewood.
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
 *   04/11/2012
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   06/04/2013
 *   Renamed AIPrivate::PerHostRequestQueue[Ptr] to AIPerHostRequestQueue[Ptr]
 *   to allow public access.
 *
 *   09/04/2013
 *   Renamed everything "host" to "service" and use "hostname:port" as key
 *   instead of just "hostname".
 */

#ifndef AICURLPERSERVICE_H
#define AICURLPERSERVICE_H

#include "llerror.h"		// llassert
#include <string>
#include <deque>
#include <map>
#include <iterator>
#include <algorithm>
#include <boost/intrusive_ptr.hpp>
#include "aithreadsafe.h"
#include "aiaverage.h"

class AICurlEasyRequest;
class AIPerService;
class AIServiceBar;

namespace AICurlPrivate {
namespace curlthread { class MultiHandle; }

class RefCountedThreadSafePerService;
class ThreadSafeBufferedCurlEasyRequest;

// Forward declaration of BufferedCurlEasyRequestPtr (see aicurlprivate.h).
typedef boost::intrusive_ptr<ThreadSafeBufferedCurlEasyRequest> BufferedCurlEasyRequestPtr;

} // namespace AICurlPrivate

// AIPerService objects are created by the curl thread and destructed by the main thread.
// We need locking.
typedef AIThreadSafeSimpleDC<AIPerService> threadsafe_PerService;
typedef AIAccessConst<AIPerService> PerService_crat;
typedef AIAccess<AIPerService> PerService_rat;
typedef AIAccess<AIPerService> PerService_wat;

// We can't put threadsafe_PerService in a std::map because you can't copy a mutex.
// Therefore, use an intrusive pointer for the threadsafe type.
typedef boost::intrusive_ptr<AICurlPrivate::RefCountedThreadSafePerService> AIPerServicePtr;

//-----------------------------------------------------------------------------
//

enum AICapabilityType {		// {Capabilities} [Responders]
  cap_texture	= 0,		// GetTexture [HTTPGetResponder]
  cap_inventory	= 1,		// { FetchInventory2, FetchLib2 } [LLInventoryModel::fetchInventoryResponder], { FetchInventoryDescendents2, FetchLibDescendents2 } [LLInventoryModelFetchDescendentsResponder]
  cap_mesh		= 2,		// GetMesh [LLMeshSkinInfoResponder, LLMeshDecompositionResponder, LLMeshPhysicsShapeResponder, LLMeshHeaderResponder, LLMeshLODResponder]
  cap_other		= 3,		// All other capabilities

  number_of_capability_types = 4
};

static U32 const approved_mask = 3;		// The mask of cap_texture OR-ed with the mask of cap_inventory.

//-----------------------------------------------------------------------------
// AIPerService

// This class provides a static interface to create and maintain instances of AIPerService objects,
// so that at any moment there is at most one instance per service (hostname:port).
// Those instances then are used to queue curl requests when the maximum number of connections
// for that service already have been reached. And to keep track of the bandwidth usage, and the
// number of queued requests in the pipeline, for this service.
class AIPerService {
  public:
	typedef std::map<std::string, AIPerServicePtr> instance_map_type;
	typedef AIThreadSafeSimpleDC<instance_map_type> threadsafe_instance_map_type;
	typedef AIAccess<instance_map_type> instance_map_rat;
	typedef AIAccess<instance_map_type> instance_map_wat;

  private:
	static threadsafe_instance_map_type sInstanceMap;	// Map of AIPerService instances with the canonical hostname:port as key.

	friend class AIThreadSafeSimpleDC<AIPerService>;	// threadsafe_PerService
	AIPerService(void);

  public:
	typedef instance_map_type::iterator iterator;
	typedef instance_map_type::const_iterator const_iterator;

	// Utility function; extract canonical (lowercase) hostname and port from url.
	static std::string extract_canonical_servicename(std::string const& url);

	// Return (possibly create) a unique instance for the given hostname:port combination.
	static AIPerServicePtr instance(std::string const& servicename);

	// Release instance (object will be deleted if this was the last instance).
	static void release(AIPerServicePtr& instance);

	// Remove everything. Called upon viewer exit.
	static void purge(void);

	// Make a copy of the instanceMap and then run 'action(per_service)' on each AIPerService object.
	template<class Action>
	static void copy_forEach(Action const& action);

  private:
	static U16 const ctf_empty = 1;
	static U16 const ctf_full = 2;
	static U16 const ctf_starvation = 4;
	// Flags used by the HTTP debug console.
	static U16 const ctf_success = 8;
	static U16 const ctf_progress_mask = 0x70;
	static U16 const ctf_progress_shift = 4;
	static U16 const ctf_grey = 0x80;

	struct CapabilityType {
	  typedef std::deque<AICurlPrivate::BufferedCurlEasyRequestPtr> queued_request_type;

	  queued_request_type mQueuedRequests;		// Waiting (throttled) requests.
	  U16 mApprovedRequests;					// The number of approved requests for this CT by approveHTTPRequestFor that were not added to the command queue yet.
	  U16 mQueuedCommands;						// Number of add commands (minus remove commands), for this service, in the command queue.
	  U16 mAdded;								// Number of active easy handles with this service.
	  U16 mFlags;								// ctf_empty: Set to true when the queue becomes precisely empty.
	  											// ctf_full : Set to true when the queue is popped and then still isn't empty;
												// ctf_starvation: Set to true when the queue was about to be popped but was already empty.
												// ctf_success: Set to true when a curl request finished successfully.
	  U32 mDownloading;							// The number of active easy handles with this service for which data was received.
	  U16 mMaxPipelinedRequests;				// The maximum number of accepted requests for this service and (approved) capability type, that didn't finish yet.
	  U16 mConcurrentConnections;				// The maximum number of allowed concurrent connections to the service of this capability type.

	  // Declare, not define, constructor and destructor - in order to avoid instantiation of queued_request_type from header.
	  CapabilityType(void);
	  ~CapabilityType();

	  S32 pipelined_requests(void) const { return mApprovedRequests + mQueuedCommands + mQueuedRequests.size() + mAdded; }
	};

	friend class AIServiceBar;
	CapabilityType mCapabilityType[number_of_capability_types];

	AIAverage mHTTPBandwidth;					// Keeps track on number of bytes received for this service in the past second.
	int mConcurrentConnections;					// The maximum number of allowed concurrent connections to this service.
	int mApprovedRequests;						// The number of approved requests for this service by approveHTTPRequestFor that were not added to the command queue yet.
	int mTotalAdded;							// Number of active easy handles with this service.
	int mEstablishedConnections;				// Number of connected sockets to this service.

	U32 mUsedCT;								// Bit mask with one bit per capability type. A '1' means the capability was in use since the last resetUsedCT().
	U32 mCTInUse;								// Bit mask with one bit per capability type. A '1' means the capability is in use right now.

	// Helper struct, used in the static resetUsed.
	struct ResetUsed { void operator()(instance_map_type::value_type const& service) const; };

	void redivide_connections(void);
	void mark_inuse(AICapabilityType capability_type)
	{
	  U32 bit = CT2mask(capability_type);
	  if ((mCTInUse & bit) == 0)			// If this CT went from unused to used
	  {
		mCTInUse |= bit;
		mUsedCT |= bit;
		if (mUsedCT != bit)					// and more than one CT use this service.
		{
		  redivide_connections();
		}
	  }
	}
	void mark_unused(AICapabilityType capability_type)
	{
	  U32 bit = CT2mask(capability_type);
	  if ((mCTInUse & bit) != 0)			// If this CT went from used to unused
	  {
		mCTInUse &= ~bit;
		if (mCTInUse && mUsedCT != bit)		// and more than one CT use this service, and at least one is in use.
		{
		  redivide_connections();
		}
	  }
	}
  public:
	int connection_established(void) { mEstablishedConnections++; return mEstablishedConnections; }
	int connection_closed(void) { mEstablishedConnections--; return mEstablishedConnections; }

	static bool is_approved(AICapabilityType capability_type) { return (((U32)1 << capability_type) & approved_mask); }
	static U32 CT2mask(AICapabilityType capability_type) { return (U32)1 << capability_type; }
	void resetUsedCt(void) { mUsedCT = mCTInUse; }
	bool is_used(AICapabilityType capability_type) const { return (mUsedCT & CT2mask(capability_type)); }
	bool is_inuse(AICapabilityType capability_type) const { return (mCTInUse & CT2mask(capability_type)); }

	static void resetUsed(void) { copy_forEach(ResetUsed()); }
	U32 is_used(void) const { return mUsedCT; }						// Non-zero if this service was used for any capability type.
	U32 is_inuse(void) const { return mCTInUse; }					// Non-zero if this service is in use for any capability type.

	// Global administration of the total number of queued requests of all services combined.
  private:
	struct TotalQueued {
		S32 approved;							// The sum of mQueuedRequests.size() of all AIPerService::CapabilityType objects of approved types.
		bool empty;								// Set to true when approved becomes precisely zero as the result of popping any queue.
		bool full;								// Set to true when approved is still larger than zero after popping any queue.
		bool starvation;						// Set to true when any queue was about to be popped when approved was already zero.
		TotalQueued(void) : approved(0), empty(false), full(false), starvation(false) { }
	};
	static AIThreadSafeSimpleDC<TotalQueued> sTotalQueued;
	typedef AIAccessConst<TotalQueued> TotalQueued_crat;
	typedef AIAccess<TotalQueued> TotalQueued_rat;
	typedef AIAccess<TotalQueued> TotalQueued_wat;
  public:
	static S32 total_approved_queue_size(void) { return TotalQueued_rat(sTotalQueued)->approved; }

	// Global administration of the maximum number of pipelined requests of all services combined.
  private:
	struct MaxPipelinedRequests {
		S32 threshold;							// The maximum total number of accepted requests that didn't finish yet.
		U64 last_increment;						// Last time that sMaxPipelinedRequests was incremented.
		U64 last_decrement;						// Last time that sMaxPipelinedRequests was decremented.
		MaxPipelinedRequests(void) : threshold(32), last_increment(0), last_decrement(0) { }
	};
	static AIThreadSafeSimpleDC<MaxPipelinedRequests> sMaxPipelinedRequests;
	typedef AIAccessConst<MaxPipelinedRequests> MaxPipelinedRequests_crat;
	typedef AIAccess<MaxPipelinedRequests> MaxPipelinedRequests_rat;
	typedef AIAccess<MaxPipelinedRequests> MaxPipelinedRequests_wat;
  public:
	static void setMaxPipelinedRequests(S32 threshold) { MaxPipelinedRequests_wat(sMaxPipelinedRequests)->threshold = threshold; }
	static void incrementMaxPipelinedRequests(S32 increment) { MaxPipelinedRequests_wat(sMaxPipelinedRequests)->threshold += increment; }

	// Global administration of throttle fraction (which is the same for all services).
  private:
	struct ThrottleFraction {
	  U32 fraction;								// A value between 0 and 1024: each service is throttled when it uses more than max_bandwidth * (sThrottleFraction/1024) bandwidth.
	  AIAverage average;						// Average of fraction over 25 * 40ms = 1 second.
	  U64 last_add;								// Last time that faction was added to average.
	  ThrottleFraction(void) : fraction(1024), average(25), last_add(0) { }
	};
	static AIThreadSafeSimpleDC<ThrottleFraction> sThrottleFraction;
	typedef AIAccessConst<ThrottleFraction> ThrottleFraction_crat;
	typedef AIAccess<ThrottleFraction> ThrottleFraction_rat;
	typedef AIAccess<ThrottleFraction> ThrottleFraction_wat;

	static LLAtomicU32 sHTTPThrottleBandwidth125;			// HTTPThrottleBandwidth times 125 (in bytes/s).
	static bool sNoHTTPBandwidthThrottling;					// Global override to disable bandwidth throttling.

  public:
	void added_to_command_queue(AICapabilityType capability_type) { ++mCapabilityType[capability_type].mQueuedCommands; mark_inuse(capability_type); }
	void removed_from_command_queue(AICapabilityType capability_type) { --mCapabilityType[capability_type].mQueuedCommands; llassert(mCapabilityType[capability_type].mQueuedCommands >= 0); }
	void added_to_multi_handle(AICapabilityType capability_type);									// Called when an easy handle for this service has been added to the multi handle.
	void removed_from_multi_handle(AICapabilityType capability_type, bool downloaded_something, bool success);	// Called when an easy handle for this service is removed again from the multi handle.
	void download_started(AICapabilityType capability_type) { ++mCapabilityType[capability_type].mDownloading; }
	bool throttled(AICapabilityType capability_type) const;		// Returns true if the maximum number of allowed requests for this service/capability type have been added to the multi handle.
	bool nothing_added(AICapabilityType capability_type) const { return mCapabilityType[capability_type].mAdded == 0; }

	bool queue(AICurlEasyRequest const& easy_request, AICapabilityType capability_type, bool force_queuing = true);	// Add easy_request to the queue if queue is empty or force_queuing.
	bool cancel(AICurlEasyRequest const& easy_request, AICapabilityType capability_type);							// Remove easy_request from the queue (if it's there).

    void add_queued_to(AICurlPrivate::curlthread::MultiHandle* mh, bool only_this_service = false);
														// Add queued easy handle (if any) to the multi handle. The request is removed from the queue,
														// followed by either a call to added_to_multi_handle() or to queue() to add it back.

	S32 pipelined_requests(AICapabilityType capability_type) const { return mCapabilityType[capability_type].pipelined_requests(); }

	AIAverage& bandwidth(void) { return mHTTPBandwidth; }
	AIAverage const& bandwidth(void) const { return mHTTPBandwidth; }

	static void setNoHTTPBandwidthThrottling(bool nb) { sNoHTTPBandwidthThrottling = nb; }
	static void setHTTPThrottleBandwidth(F32 max_kbps) { sHTTPThrottleBandwidth125 = 125.f * max_kbps; }
	static size_t getHTTPThrottleBandwidth125(void) { return sHTTPThrottleBandwidth125; }
	static F32 throttleFraction(void) { return ThrottleFraction_wat(sThrottleFraction)->fraction / 1024.f; }

	// Called when CurlConcurrentConnectionsPerService changes.
	static void adjust_concurrent_connections(int increment);

	// A helper class to decrement mApprovedRequests after requests approved by approveHTTPRequestFor were handled.
	class Approvement : public LLThreadSafeRefCount {
	  private:
		AIPerServicePtr mPerServicePtr;
		AICapabilityType mCapabilityType;
		bool mHonored;
	  public:
		Approvement(AIPerServicePtr const& per_service, AICapabilityType capability_type) : mPerServicePtr(per_service), mCapabilityType(capability_type), mHonored(false) { }
		~Approvement() { if (!mHonored) not_honored(); }
		void honored(void);
		void not_honored(void);
	};

	// The two following functions are static and have the AIPerService object passed
	// as first argument as an AIPerServicePtr because that avoids the need of having
	// the AIPerService object locked for the whole duration of the call.
	// The functions only lock it when access is required.

	// Returns approvement if curl can handle another request for this service.
	// Should return NULL if the maximum allowed HTTP bandwidth is reached, or when
	// the latency between request and actual delivery becomes too large.
	static Approvement* approveHTTPRequestFor(AIPerServicePtr const& per_service, AICapabilityType capability_type);
	// Return true if too much bandwidth is being used.
	static bool checkBandwidthUsage(AIPerServicePtr const& per_service, U64 sTime_40ms);

  private:
	// Disallow copying.
	AIPerService(AIPerService const&);
};

namespace AICurlPrivate {

class RefCountedThreadSafePerService : public threadsafe_PerService {
  public:
	RefCountedThreadSafePerService(void) : mReferenceCount(0) { }
	bool exactly_two_left(void) const { return mReferenceCount == 2; }

  private:
	// Used by AIPerServicePtr. Object is deleted when reference count reaches zero.
	LLAtomicU32 mReferenceCount;

	friend void intrusive_ptr_add_ref(RefCountedThreadSafePerService* p);
	friend void intrusive_ptr_release(RefCountedThreadSafePerService* p);
};

extern U16 CurlConcurrentConnectionsPerService;

} // namespace AICurlPrivate

template<class Action>
void AIPerService::copy_forEach(Action const& action)
{
  // Make a copy so we don't need to keep the lock on sInstanceMap for too long.
  std::vector<std::pair<instance_map_type::key_type, instance_map_type::mapped_type> > current_services;
  {
	instance_map_rat instance_map_r(sInstanceMap);
	std::copy(instance_map_r->begin(), instance_map_r->end(), std::back_inserter(current_services));
  }
  // Apply the functor on each of the services.
  std::for_each(current_services.begin(), current_services.end(), action);
}

#endif // AICURLPERSERVICE_H
