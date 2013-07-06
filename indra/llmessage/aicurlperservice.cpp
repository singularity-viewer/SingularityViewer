/**
 * @file aiperservice.cpp
 * @brief Implementation of AIPerService
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
 *   Renamed AICurlPrivate::PerHostRequestQueue[Ptr] to AIPerHostRequestQueue[Ptr]
 *   to allow public access.
 *
 *   09/04/2013
 *   Renamed everything "host" to "service" and use "hostname:port" as key
 *   instead of just "hostname".
 */

#include "sys.h"
#include "aicurlperservice.h"
#include "aicurlthread.h"
#include "llcontrol.h"

AIPerService::threadsafe_instance_map_type AIPerService::sInstanceMap;
AIThreadSafeSimpleDC<AIPerService::TotalQueued> AIPerService::sTotalQueued;

#undef AICurlPrivate

namespace AICurlPrivate {

// Cached value of CurlConcurrentConnectionsPerService.
U16 CurlConcurrentConnectionsPerService;

// Friend functions of RefCountedThreadSafePerService

void intrusive_ptr_add_ref(RefCountedThreadSafePerService* per_service)
{
  per_service->mReferenceCount++;
}

void intrusive_ptr_release(RefCountedThreadSafePerService* per_service)
{
  if (--per_service->mReferenceCount == 0)
  {
    delete per_service;
  }
}

} // namespace AICurlPrivate

using namespace AICurlPrivate;

AIPerService::AIPerService(void) :
		mHTTPBandwidth(25),	// 25 = 1000 ms / 40 ms.
		mConcurrentConnections(CurlConcurrentConnectionsPerService),
		mApprovedRequests(0),
		mTotalAdded(0),
		mEstablishedConnections(0),
		mUsedCT(0),
		mCTInUse(0)
{
}

AIPerService::CapabilityType::CapabilityType(void) :
  		mApprovedRequests(0),
		mQueuedCommands(0),
		mAdded(0),
		mFlags(0),
		mDownloading(0),
		mMaxPipelinedRequests(CurlConcurrentConnectionsPerService),
		mConcurrentConnections(CurlConcurrentConnectionsPerService)
{
}

AIPerService::CapabilityType::~CapabilityType()
{
}

// Fake copy constructor.
AIPerService::AIPerService(AIPerService const&) : mHTTPBandwidth(0)
{
}

// url must be of the form
// (see http://www.ietf.org/rfc/rfc3986.txt Appendix A for definitions not given here):
//
// url			= sheme ":" hier-part [ "?" query ] [ "#" fragment ]
// hier-part	= "//" authority path-abempty
// authority     = [ userinfo "@" ] host [ ":" port ]
// path-abempty  = *( "/" segment )
//
// That is, a hier-part of the form '/ path-absolute', '/ path-rootless' or
// '/ path-empty' is NOT allowed here. This should be safe because we only
// call this function for curl access, any file access would use APR.
//
// However, as a special exception, this function allows:
//
// url			= authority path-abempty
//
// without the 'sheme ":" "//"' parts.
//
// As follows from the ABNF (see RFC, Appendix A):
// - authority is either terminated by a '/' or by the end of the string because
//   neither userinfo, host nor port may contain a '/'.
// - userinfo does not contain a '@', and if it exists, is always terminated by a '@'.
// - port does not contain a ':', and if it exists is always prepended by a ':'.
//
// This function also needs to deal with full paths, in which case it should return
// an empty string.
//
// Full paths can have the form: "/something..."
//                           or  "C:\something..."
//               and maybe even  "C:/something..."
//
// The first form leads to an empty string being returned because the '/' signals the
// end of the authority and we'll return immediately.
// The second one will abort when hitting the backslash because that is an illegal
// character in an url (before the first '/' anyway).
// The third will abort because "C:" would be the hostname and a colon in the hostname
// is not legal.
//
//static
std::string AIPerService::extract_canonical_servicename(std::string const& url)
{
  char const* p = url.data();
  char const* const end = p + url.size();
  char const* sheme_colon = NULL;
  char const* sheme_slash = NULL;
  char const* first_ampersand = NULL;
  char const* port_colon = NULL;
  std::string servicename;
  char const* hostname = p;                 // Default in the case there is no "sheme://userinfo@".
  while (p < end)
  {
    int c = *p;
    if (c == ':')
    {
      if (!port_colon && LLStringOps::isDigit(p[1]))
      {
        port_colon = p;
      }
      else if (!sheme_colon && !sheme_slash && !first_ampersand && !port_colon)
      {
        // Found a colon before any slash or ampersand: this has to be the colon between the sheme and the hier-part.
        sheme_colon = p;
      }
    }
    else if (c == '/')
    {
      if (!sheme_slash && sheme_colon && sheme_colon == p - 1 && !first_ampersand && p[1] == '/')
      {
        // Found the first '/' in the first occurance of the sequence "://".
        sheme_slash = p;
        hostname = ++p + 1;                 // Point hostname to the start of the authority, the default when there is no "userinfo@" part.
        servicename.clear();                // Remove the sheme.
      }
      else
      {
        // Found a slash that is not part of the "sheme://" string. Signals end of authority.
        // We're done.
        if (hostname < sheme_colon)
        {
          // This happens when windows filenames are passed to this function of the form "C:/..."
          servicename.clear();
        }
        break;
      }
    }
    else if (c == '@')
    {
      if (!first_ampersand)
      {
        first_ampersand = p;
        hostname = p + 1;
        servicename.clear();                // Remove the "userinfo@"
      }
    }
    else if (c == '\\')
    {
      // Found a backslash, which is an illegal character for an URL. This is a windows path... reject it.
      servicename.clear();
      break;
    }
    if (p >= hostname)
    {
      // Convert hostname to lowercase in a way that we compare two hostnames equal iff libcurl does.
#if APR_CHARSET_EBCDIC
#error Not implemented
#else
      if (c >= 'A' && c <= 'Z')
        c += ('a' - 'A');
#endif
      servicename += c;
    }
    ++p;
  }
  // Strip of any trailing ":80".
  if (p - 3 == port_colon && p[-1] == '0' && p[-2] == '8')
  {
    return servicename.substr(0, p - hostname - 3);
  }
  return servicename;
}

//static
AIPerServicePtr AIPerService::instance(std::string const& servicename)
{
  llassert(!servicename.empty());
  instance_map_wat instance_map_w(sInstanceMap);
  AIPerService::iterator iter = instance_map_w->find(servicename);
  if (iter == instance_map_w->end())
  {
	iter = instance_map_w->insert(instance_map_type::value_type(servicename, new RefCountedThreadSafePerService)).first;
	Dout(dc::curlio, "Created new service \"" << servicename << "\" [" << (void*)&*PerService_rat(*iter->second) << "]");
  }
  // Note: the creation of AIPerServicePtr MUST be protected by the lock on sInstanceMap (see release()).
  return iter->second;
}

//static
void AIPerService::release(AIPerServicePtr& instance)
{
  if (instance->exactly_two_left())		// Being 'instance' and the one in sInstanceMap.
  {
	// The viewer can be have left main() we can't access the global sInstanceMap anymore.
	if (LLApp::isStopped())
	{
	  return;
	}
	instance_map_wat instance_map_w(sInstanceMap);
	// It is possible that 'exactly_two_left' is not up to date anymore.
	// Therefore, recheck the condition now that we have locked sInstanceMap.
	if (!instance->exactly_two_left())
	{
	  // Some other thread added this service in the meantime.
	  return;
	}
#ifdef SHOW_ASSERT
	{
	  // The reference in the map is the last one; that means there can't be any curl easy requests queued for this service.
	  PerService_rat per_service_r(*instance);
	  for (int i = 0; i < number_of_capability_types; ++i)
	  {
	  	llassert(per_service_r->mCapabilityType[i].mQueuedRequests.empty());
	  }
	}
#endif
	// Find the service and erase it from the map.
	iterator const end = instance_map_w->end();
	for(iterator iter = instance_map_w->begin(); iter != end; ++iter)
	{
	  if (instance == iter->second)
	  {
		instance_map_w->erase(iter);
		instance.reset();
		return;
	  }
	}
	// We should always find the service.
	llassert(false);
  }
  instance.reset();
}

void AIPerService::redivide_connections(void)
{
  // Priority order.
  static AICapabilityType order[number_of_capability_types] = { cap_inventory, cap_texture, cap_mesh, cap_other };
  // Count the number of capability types that are currently in use and store the types in an array.
  AICapabilityType used_order[number_of_capability_types];
  int number_of_capability_types_in_use = 0;
  for (int i = 0; i < number_of_capability_types; ++i)
  {
	U32 const mask = CT2mask(order[i]);
	if ((mCTInUse & mask))
	{
	  used_order[number_of_capability_types_in_use++] = order[i];
	}
	else
	{
	  // Give every other type (that is not in use) one connection, so they can be used (at which point they'll get more).
	  mCapabilityType[order[i]].mConcurrentConnections = 1;
	}
  }
  // Keep one connection in reserve for currently unused capability types (that have been used before).
  int reserve = (mUsedCT != mCTInUse) ? 1 : 0;
  // Distribute (mConcurrentConnections - reserve) over number_of_capability_types_in_use.
  U16 max_connections_per_CT = (mConcurrentConnections - reserve) / number_of_capability_types_in_use + 1;
  // The first count CTs get max_connections_per_CT connections.
  int count = (mConcurrentConnections - reserve) % number_of_capability_types_in_use;
  for(int i = 1, j = 0;; --i)
  {
	while (j < count)
	{
	  mCapabilityType[used_order[j++]].mConcurrentConnections = max_connections_per_CT;
	}
	if (i == 0)
	{
	  break;
	}
	// Finish the loop till all used CTs are assigned.
	count = number_of_capability_types_in_use;
	// Never assign 0 as maximum.
	if (max_connections_per_CT > 1)
	{
	  // The remaining CTs get one connection less so that the sum of all assigned connections is mConcurrentConnections - reserve.
	  --max_connections_per_CT;
	}
  }
}

bool AIPerService::throttled(AICapabilityType capability_type) const
{
  return mTotalAdded >= mConcurrentConnections ||
		 mCapabilityType[capability_type].mAdded >= mCapabilityType[capability_type].mConcurrentConnections;
}

void AIPerService::added_to_multi_handle(AICapabilityType capability_type)
{
  ++mCapabilityType[capability_type].mAdded;
  ++mTotalAdded;
}

void AIPerService::removed_from_multi_handle(AICapabilityType capability_type, bool downloaded_something, bool success)
{
  CapabilityType& ct(mCapabilityType[capability_type]);
  llassert(mTotalAdded > 0 && ct.mAdded > 0);
  bool done = --ct.mAdded == 0;
  if (downloaded_something)
  {
	llassert(ct.mDownloading > 0);
	--ct.mDownloading;
  }
  --mTotalAdded;
  if (done && ct.pipelined_requests() == 0)
  {
	mark_unused(capability_type);
  }
  if (success)
  {
	ct.mFlags |= ctf_success;
  }
}

// Returns true if the request was queued.
bool AIPerService::queue(AICurlEasyRequest const& easy_request, AICapabilityType capability_type, bool force_queuing)
{
  CapabilityType::queued_request_type& queued_requests(mCapabilityType[capability_type].mQueuedRequests);
  bool needs_queuing = force_queuing || !queued_requests.empty();
  if (needs_queuing)
  {
	queued_requests.push_back(easy_request.get_ptr());
	if (is_approved(capability_type))
	{
	  TotalQueued_wat(sTotalQueued)->approved++;
	}
  }
  return needs_queuing;
}

bool AIPerService::cancel(AICurlEasyRequest const& easy_request, AICapabilityType capability_type)
{
  CapabilityType::queued_request_type::iterator const end = mCapabilityType[capability_type].mQueuedRequests.end();
  CapabilityType::queued_request_type::iterator cur = std::find(mCapabilityType[capability_type].mQueuedRequests.begin(), end, easy_request.get_ptr());

  if (cur == end)
	return false;		// Not found.

  // We can't use erase because that uses assignment to move elements,
  // because it isn't thread-safe. Therefore, move the element that we found to 
  // the back with swap (could just swap with the end immediately, but I don't
  // want to break the order in which requests where added). Swap is also not
  // thread-safe, but OK here because it only touches the objects in the deque,
  // and the deque is protected by the lock on the AIPerService object.
  CapabilityType::queued_request_type::iterator prev = cur;
  while (++cur != end)
  {
	prev->swap(*cur);				// This is safe,
	prev = cur;
  }
  mCapabilityType[capability_type].mQueuedRequests.pop_back();		// if this is safe.
  if (is_approved(capability_type))
  {
	TotalQueued_wat total_queued_w(sTotalQueued);
	llassert(total_queued_w->approved > 0);
	total_queued_w->approved--;
  }
  return true;
}

void AIPerService::add_queued_to(curlthread::MultiHandle* multi_handle, bool only_this_service)
{
  U32 success = 0;									// The CTs that we successfully added a request for from the queue.
  bool success_this_pass = false;
  int i = 0;
  // The first pass we only look at CTs with 0 requests added to the multi handle. Subsequent passes only non-zero ones.
  for (int pass = 0;; ++i)
  {
	if (i == number_of_capability_types)
	{
	  i = 0;
	  // Keep trying until we couldn't add anything anymore.
	  if (pass++ && !success_this_pass)
	  {
		// Done.
		break;
	  }
	  success_this_pass = false;
	}
	CapabilityType& ct(mCapabilityType[i]);
	if (!pass != !ct.mAdded)						// Does mAdded match what we're looking for (first mAdded == 0, then mAdded != 0)?
	{
	  continue;
	}
	if (multi_handle->added_maximum())
	{
	  // We hit the maximum number of global connections. Abort every attempt to add anything.
	  only_this_service = true;
	  break;
	}
	if (mTotalAdded >= mConcurrentConnections)
	{
	  // We hit the maximum number of connections for this service. Abort any attempt to add anything to this service.
	  break;
	}
	if (ct.mAdded >= ct.mConcurrentConnections)
	{
	  // We hit the maximum number of connections for this capability type. Try the next one.
	  continue;
	}
	U32 mask = CT2mask((AICapabilityType)i);
	if (ct.mQueuedRequests.empty())					// Is there anything in the queue (left) at all?
	{
	  // We could add a new request, but there is none in the queue!
	  // Note that if this service does not serve this capability type,
	  // then obviously this queue was empty; however, in that case
	  // this variable will never be looked at, so it's ok to set it.
	  ct.mFlags |= ((success & mask) ? ctf_empty : ctf_starvation);
	}
	else
	{
	  // Attempt to add the front of the queue.
	  if (!multi_handle->add_easy_request(ct.mQueuedRequests.front(), true))
	  {
		// If that failed then we got throttled on bandwidth because the maximum number of connections were not reached yet.
		// Therefore this will keep failing for this service, we abort any additional attempt to add something for this service.
		break;
	  }
	  // Request was added, remove it from the queue.
	  ct.mQueuedRequests.pop_front();
	  // Mark that at least one request of this CT was successfully added.
	  success |= mask;
	  success_this_pass = true;
	  // Update approved count.
	  if (is_approved((AICapabilityType)i))
	  {
		TotalQueued_wat total_queued_w(sTotalQueued);
		llassert(total_queued_w->approved > 0);
		total_queued_w->approved--;
	  }
	}
  }

  size_t queuedapproved_size = 0;
  for (int i = 0; i < number_of_capability_types; ++i)
  {
	CapabilityType& ct(mCapabilityType[i]);
	U32 mask = CT2mask((AICapabilityType)i);
	// Add up the size of all queues with approved requests.
	if ((approved_mask & mask))
	{
	  queuedapproved_size += ct.mQueuedRequests.size();
	}
	// Skip CTs that we didn't add anything for.
	if (!(success & mask))
	{
	  continue;
	}
	if (!ct.mQueuedRequests.empty())
	{
	  // We obtained one or more requests from the queue, and even after that there was at least one more request in the queue of this CT.
	  ct.mFlags |= ctf_full;
	}
  }

  // Update the flags of sTotalQueued.
  {
	TotalQueued_wat total_queued_w(sTotalQueued);
	if (total_queued_w->approved == 0)
	{
	  if ((success & approved_mask))
	  {
		// We obtained an approved request from the queue, and after that there were no more requests in any (approved) queue.
		total_queued_w->empty = true;
	  }
	  else
	  {
		// Every queue of every approved CT is empty!
		total_queued_w->starvation = true;
	  }
	}
	else if ((success & approved_mask))
	{
	  // We obtained an approved request from the queue, and even after that there was at least one more request in some (approved) queue.
	  total_queued_w->full = true;
	}
  }
 
  // Don't try other services if anything was added successfully.
  if (success || only_this_service)
  {
	return;
  }

  // Nothing from this service could be added, try other services.
  instance_map_wat instance_map_w(sInstanceMap);
  for (iterator service = instance_map_w->begin(); service != instance_map_w->end(); ++service)
  {
	PerService_wat per_service_w(*service->second);
	if (&*per_service_w == this)
	{
	  continue;
	}
	per_service_w->add_queued_to(multi_handle, true);
  }
}

//static
void AIPerService::purge(void)
{
  instance_map_wat instance_map_w(sInstanceMap);
  for (iterator service = instance_map_w->begin(); service != instance_map_w->end(); ++service)
  {
	Dout(dc::curl, "Purging queues of service \"" << service->first << "\".");
	PerService_wat per_service_w(*service->second);
	TotalQueued_wat total_queued_w(sTotalQueued);
	for (int i = 0; i < number_of_capability_types; ++i)
	{
	  size_t s = per_service_w->mCapabilityType[i].mQueuedRequests.size();
	  per_service_w->mCapabilityType[i].mQueuedRequests.clear();
	  if (is_approved((AICapabilityType)i))
	  {
		llassert(total_queued_w->approved >= s);
		total_queued_w->approved -= s;
	  }
	}
  }
}

//static
void AIPerService::adjust_concurrent_connections(int increment)
{
  instance_map_wat instance_map_w(sInstanceMap);
  for (AIPerService::iterator iter = instance_map_w->begin(); iter != instance_map_w->end(); ++iter)
  {
	PerService_wat per_service_w(*iter->second);
	U16 old_concurrent_connections = per_service_w->mConcurrentConnections;
	int new_concurrent_connections = llclamp(old_concurrent_connections + increment, 1, (int)CurlConcurrentConnectionsPerService);
	per_service_w->mConcurrentConnections = (U16)new_concurrent_connections;
	increment = per_service_w->mConcurrentConnections - old_concurrent_connections;
	for (int i = 0; i < number_of_capability_types; ++i)
	{
	  per_service_w->mCapabilityType[i].mMaxPipelinedRequests = llmax(per_service_w->mCapabilityType[i].mMaxPipelinedRequests + increment, 0);
	  int new_concurrent_connections_per_capability_type =
		  llclamp((new_concurrent_connections * per_service_w->mCapabilityType[i].mConcurrentConnections + old_concurrent_connections / 2) / old_concurrent_connections, 1, new_concurrent_connections);
	  per_service_w->mCapabilityType[i].mConcurrentConnections = (U16)new_concurrent_connections_per_capability_type;
	}
  }
}

void AIPerService::ResetUsed::operator()(AIPerService::instance_map_type::value_type const& service) const
{
  PerService_wat(*service.second)->resetUsedCt();
}

void AIPerService::Approvement::honored(void)
{
  if (!mHonored)
  {
	mHonored = true;
	PerService_wat per_service_w(*mPerServicePtr);
	llassert(per_service_w->mCapabilityType[mCapabilityType].mApprovedRequests > 0 && per_service_w->mApprovedRequests > 0);
	per_service_w->mCapabilityType[mCapabilityType].mApprovedRequests--;
	per_service_w->mApprovedRequests--;
  }
}

void AIPerService::Approvement::not_honored(void)
{
  honored();
  llwarns << "Approvement for has not been honored." << llendl;
}

