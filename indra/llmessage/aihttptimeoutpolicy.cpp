/**
 * @file aihttptimeoutpolicy.cpp
 * @brief Implementation of AIHTTPTimeoutPolicy
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
 *   24/08/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "sys.h"
#include "aihttptimeoutpolicy.h"
#define NOMINMAX
#include "llerror.h"
#include "lldefs.h"
#include "v3math.h"
#include <vector>

//!
// Timing of a HTML connection.
//
// Request call
//     |
//     v                                                       ... <--low speed time--> ...                  ... <--low speed time--> ...
//     <--request queued--><--DNS lookup--><--connect margin--><--data transfer to server--><--reply delay--><--data transfer from server-->
//                         <------------------------------------------curl transaction----------------------------------------------------->
//     <--------------------------------------------------------------total delay---------------------------------------------------------->
//                                                                                                                                         |
//                                                                                                                                         v
//                                                                                                                                     finished
// For now, low speed limit is the same for up and download: usually download is (much) higher, but we have to take into account that
// there might also be multiple downloads at the same time, more than simultaneous uploads.

// Absolute maxima (min/max range):
// These values are intuitively determined and rather arbitrary.

namespace {

U16 const ABS_min_DNS_lookup = 0;						// Rationale: If a FAST roundtrip is demanded, then setting the DNS lookup grace time
														// at 0 seconds will not make a connection fail when the lookup takes 1 second, it
														// just means that no EXTRA time is added to the connect time.
U16 const ABS_max_DNS_lookup = 300;						// Waiting longer than 5 minutes never makes sense.

U16 const ABS_min_connect_time = 1;						// Can't demand 0 seconds, and we deal with integer numbers here.
U16 const ABS_max_connect_time = 30;					// Making a TCP/IP connection should REALLY succeed within 30 seconds or we rather try again.

U16 const ABS_min_reply_delay = 1;						// Can't demand 0 seconds, and we deal with integer numbers here.
U16 const ABS_max_reply_delay = 120;					// If the server needs more than 2 minutes to find the reply then something just HAS to be wrong :/.

U16 const ABS_min_low_speed_time = 4;					// Intuitively, I think it makes no sense to average a download speed over less than 4 seconds.
U16 const ABS_max_low_speed_time = 120;					// Averaging it over a time considerably larger than the normal timeout periods makes no sense either.

U32 const ABS_min_low_speed_limit = 1;					// In case you don't want to timeout when there is any data received at all.
U32 const ABS_max_low_speed_limit = 1000000;			// This limit is almost certainly higher than the maximum speed you get from the server.

U16 const ABS_min_transaction = 60;						// This is an absurd low value for experimentation. In reality, you should control
														// termination of really slow connections through the low_speed settings.
U16 const ABS_max_transaction = 1200;					// Insane long time. Downloads a texture of 4 MB at 3.5 kB/s. Textures are compressed though ;).

U16 const ABS_min_total_delay = 60;						// This is an absurd low value for experimentation. In reality, you should control
														// termination of really slow connections through the low_speed settings.
U16 const ABS_max_total_delay = 3000;					// Insane long time, for when someone wants to be ABSOLUTELY sure this isn't the bottleneck.

using namespace AIHTTPTimeoutPolicyOperators;

// Default policy values.
U16 const AITP_default_DNS_lookup_grace = 60;			// Allow for 60 seconds long DNS look ups.
U16 const AITP_default_maximum_connect_time = 10;		// Allow the SSL/TLS connection through a proxy, including handshakes, to take up to 10 seconds.
U16 const AITP_default_maximum_reply_delay = 60;		// Allow the server 60 seconds to do whatever it has to do before starting to send data.
U16 const AITP_default_low_speed_time = 30;				// If a transfer speed drops below AITP_default_low_speed_limit bytes/s for 30 seconds, terminate the transfer.
U32 const AITP_default_low_speed_limit = 7000;			// In bytes per second (use for CURLOPT_LOW_SPEED_LIMIT).
U16 const AITP_default_maximum_curl_transaction = 300;	// Allow large files to be transfered over slow connections.
U16 const AITP_default_maximum_total_delay = 600;		// Avoid "leaking" by terminating anything that wasn't completed after 10 minutes.

} // namespace

AIHTTPTimeoutPolicy& AIHTTPTimeoutPolicy::operator=(AIHTTPTimeoutPolicy const& rhs)
{
  // You're not allowed to assign to a policy that is based on another policy.
  llassert(!mBase);  
  mDNSLookupGrace = rhs.mDNSLookupGrace;
  mMaximumConnectTime = rhs.mMaximumConnectTime;
  mMaximumReplyDelay = rhs.mMaximumReplyDelay;
  mLowSpeedTime = rhs.mLowSpeedTime;
  mLowSpeedLimit = rhs.mLowSpeedLimit;
  mMaximumCurlTransaction = rhs.mMaximumCurlTransaction;
  mMaximumTotalDelay = rhs.mMaximumTotalDelay;
  changed();
  return *this;
}

AIHTTPTimeoutPolicy::AIHTTPTimeoutPolicy(char const* name,
										 U16 dns_lookup_grace, U16 subsequent_connects, U16 reply_delay,
										 U16 low_speed_time, U32 low_speed_limit,
										 U16 curl_transaction, U16 total_delay) :
	mName(name),
	mBase(NULL),
	mDNSLookupGrace(dns_lookup_grace),
	mMaximumConnectTime(subsequent_connects),
	mMaximumReplyDelay(reply_delay),
	mLowSpeedTime(low_speed_time),
	mLowSpeedLimit(low_speed_limit),
	mMaximumCurlTransaction(curl_transaction),
	mMaximumTotalDelay(total_delay)
{
  sanity_checks();
}

struct PolicyOp {
  PolicyOp* mNext;
  PolicyOp(void) : mNext(NULL) { }
  PolicyOp(PolicyOp& op) : mNext(&op) { }
  virtual void perform(AIHTTPTimeoutPolicy* policy) const { }
  void nextOp(AIHTTPTimeoutPolicy* policy) const { if (mNext) mNext->perform(policy); }
};

class AIHTTPTimeoutPolicyBase : public AIHTTPTimeoutPolicy {
  private:
	std::vector<AIHTTPTimeoutPolicy*> mDerived;	// Policies derived from this one.
	PolicyOp const* mOp;						// Operator we applied to base to get ourselves.

  public:
	AIHTTPTimeoutPolicyBase(U16 dns_lookup_grace, U16 subsequent_connects, U16 reply_delay,
							U16 low_speed_time, U32 low_speed_limit,
							U16 curl_transaction, U16 total_delay) :
		AIHTTPTimeoutPolicy(NULL, dns_lookup_grace, subsequent_connects, reply_delay, low_speed_time, low_speed_limit, curl_transaction, total_delay),
		mOp(NULL) { }

	// Derive base from base.
	AIHTTPTimeoutPolicyBase(AIHTTPTimeoutPolicyBase& rhs, PolicyOp& op) : AIHTTPTimeoutPolicy(rhs), mOp(&op) { rhs.derived(this); mOp->perform(this); }

	// Called for every derived policy.
	void derived(AIHTTPTimeoutPolicy* derived) { mDerived.push_back(derived); }

	// Called when our base changed.
	/*virtual*/ void base_changed(void);

	// Called when we ourselves changed.
	/*virtual*/ void changed(void);

	// Provide public acces to sDebugSettingsCurlTimeout for this compilation unit.
	static AIHTTPTimeoutPolicyBase& getDebugSettingsCurlTimeout(void) { return sDebugSettingsCurlTimeout; }

  protected:
	friend void AIHTTPTimeoutPolicy::setDefaultCurlTimeout(AIHTTPTimeoutPolicy const& timeout);
	AIHTTPTimeoutPolicyBase& operator=(AIHTTPTimeoutPolicy const& rhs);
};

void AIHTTPTimeoutPolicyBase::base_changed(void)
{
  AIHTTPTimeoutPolicy::base_changed();
  if (mOp)
	mOp->perform(this);
  changed();
}

void AIHTTPTimeoutPolicyBase::changed(void)
{
  for (std::vector<AIHTTPTimeoutPolicy*>::iterator iter = mDerived.begin(); iter != mDerived.end(); ++iter)
	(*iter)->base_changed();
}

void AIHTTPTimeoutPolicy::changed(void)
{
  Dout(dc::notice, "Policy \"" << mName << "\" changed into: DNSLookup: " << mDNSLookupGrace << "; Connect: " << mMaximumConnectTime <<
	  "; ReplyDelay: " << mMaximumReplyDelay << "; LowSpeedTime: " << mLowSpeedTime << "; LowSpeedLimit: " << mLowSpeedLimit <<
	  "; MaxTransaction: " << mMaximumCurlTransaction << "; MaxTotalDelay:" << mMaximumTotalDelay);
}

AIHTTPTimeoutPolicy::AIHTTPTimeoutPolicy(AIHTTPTimeoutPolicy& base) :
	mName(NULL),
	mBase(static_cast<AIHTTPTimeoutPolicyBase*>(&base)),
	mDNSLookupGrace(base.mDNSLookupGrace),
	mMaximumConnectTime(base.mMaximumConnectTime),
	mMaximumReplyDelay(base.mMaximumReplyDelay),
	mLowSpeedTime(base.mLowSpeedTime),
	mLowSpeedLimit(base.mLowSpeedLimit),
	mMaximumCurlTransaction(base.mMaximumCurlTransaction),
	mMaximumTotalDelay(base.mMaximumTotalDelay)
{
}

AIHTTPTimeoutPolicyBase& AIHTTPTimeoutPolicyBase::operator=(AIHTTPTimeoutPolicy const& rhs)
{
  AIHTTPTimeoutPolicy::operator=(rhs);
  return *this;
}

AIHTTPTimeoutPolicy::AIHTTPTimeoutPolicy(char const* name, AIHTTPTimeoutPolicyBase& base) :
	mName(name),
	mBase(&base),
	mDNSLookupGrace(mBase->mDNSLookupGrace),
	mMaximumConnectTime(mBase->mMaximumConnectTime),
	mMaximumReplyDelay(mBase->mMaximumReplyDelay),
	mLowSpeedTime(mBase->mLowSpeedTime),
	mLowSpeedLimit(mBase->mLowSpeedLimit),
	mMaximumCurlTransaction(mBase->mMaximumCurlTransaction),
	mMaximumTotalDelay(mBase->mMaximumTotalDelay)
{
  sNameMap.insert(namemap_t::value_type(name, this));
  // Register for changes to the base policy.
  mBase->derived(this);
}

void AIHTTPTimeoutPolicy::base_changed(void)
{
  // The same as *this = *mBase; but can't use operator= because of an assert that checks that mBase is not set.
  mDNSLookupGrace = mBase->mDNSLookupGrace;
  mMaximumConnectTime = mBase->mMaximumConnectTime;
  mMaximumReplyDelay = mBase->mMaximumReplyDelay;
  mLowSpeedTime = mBase->mLowSpeedTime;
  mLowSpeedLimit = mBase->mLowSpeedLimit;
  mMaximumCurlTransaction = mBase->mMaximumCurlTransaction;
  mMaximumTotalDelay = mBase->mMaximumTotalDelay;
  changed();
}

//static
void AIHTTPTimeoutPolicy::setDefaultCurlTimeout(AIHTTPTimeoutPolicy const& timeout)
{
  sDebugSettingsCurlTimeout = timeout;
  llinfos << "CurlTimeout Debug Settings now"
	  ": DNSLookup: " << sDebugSettingsCurlTimeout.mDNSLookupGrace <<
	  "; Connect: " << sDebugSettingsCurlTimeout.mMaximumConnectTime <<
	  "; ReplyDelay: " << sDebugSettingsCurlTimeout.mMaximumReplyDelay <<
	  "; LowSpeedTime: " << sDebugSettingsCurlTimeout.mLowSpeedTime <<
	  "; LowSpeedLimit: " << sDebugSettingsCurlTimeout.mLowSpeedLimit <<
	  "; MaxTransaction: " << sDebugSettingsCurlTimeout.mMaximumCurlTransaction <<
	  "; MaxTotalDelay: " << sDebugSettingsCurlTimeout.mMaximumTotalDelay << llendl;
  if (sDebugSettingsCurlTimeout.mDNSLookupGrace < AITP_default_DNS_lookup_grace)
  {
	llwarns << "CurlTimeoutDNSLookup (" << sDebugSettingsCurlTimeout.mDNSLookupGrace << ") is lower than the built-in default value (" << AITP_default_DNS_lookup_grace << ")." << llendl;
  }
  if (sDebugSettingsCurlTimeout.mMaximumConnectTime < AITP_default_maximum_connect_time)
  {
	llwarns << "CurlTimeoutConnect (" << sDebugSettingsCurlTimeout.mMaximumConnectTime << ") is lower than the built-in default value (" << AITP_default_maximum_connect_time << ")." << llendl;
  }
  if (sDebugSettingsCurlTimeout.mMaximumReplyDelay < AITP_default_maximum_reply_delay)
  {
	llwarns << "CurlTimeoutReplyDelay (" << sDebugSettingsCurlTimeout.mMaximumReplyDelay << ") is lower than the built-in default value (" << AITP_default_maximum_reply_delay << ")." << llendl;
  }
  if (sDebugSettingsCurlTimeout.mLowSpeedTime < AITP_default_low_speed_time)
  {
	llwarns << "CurlTimeoutLowSpeedTime (" << sDebugSettingsCurlTimeout.mLowSpeedTime << ") is lower than the built-in default value (" << AITP_default_low_speed_time << ")." << llendl;
  }
  if (sDebugSettingsCurlTimeout.mLowSpeedLimit > AITP_default_low_speed_limit)
  {
	llwarns << "CurlTimeoutLowSpeedLimit (" << sDebugSettingsCurlTimeout.mLowSpeedLimit << ") is higher than the built-in default value (" << AITP_default_low_speed_limit << ")." << llendl;
  }
  if (sDebugSettingsCurlTimeout.mMaximumCurlTransaction < AITP_default_maximum_curl_transaction)
  {
	llwarns << "CurlTimeoutMaxTransaction (" << sDebugSettingsCurlTimeout.mMaximumCurlTransaction << ") is lower than the built-in default value (" << AITP_default_maximum_curl_transaction<< ")." << llendl;
  }
  if (sDebugSettingsCurlTimeout.mMaximumTotalDelay < AITP_default_maximum_total_delay)
  {
	llwarns << "CurlTimeoutMaxTotalDelay (" << sDebugSettingsCurlTimeout.mMaximumTotalDelay << ") is lower than the built-in default value (" << AITP_default_maximum_total_delay << ")." << llendl;
  }
}

//static
AIHTTPTimeoutPolicy const& AIHTTPTimeoutPolicy::getDebugSettingsCurlTimeout(void)
{
  return sDebugSettingsCurlTimeout;
}

#ifdef SHOW_ASSERT
#include "aithreadid.h"
static AIThreadID curlthread(AIThreadID::none);		// Initialized by getConnectTimeout.
#endif

static std::set<std::string> gSeenHostnames;

U16 AIHTTPTimeoutPolicy::getConnectTimeout(std::string const& hostname) const
{
#ifdef SHOW_ASSERT
  // Only the CURL-THREAD may access gSeenHostnames.
  if (curlthread.is_no_thread())
	curlthread.reset();
  llassert(curlthread.equals_current_thread());
#endif

  U16 connect_timeout = mMaximumConnectTime;
  // Add the hostname to the list of seen hostnames, if not already there.
  if (gSeenHostnames.insert(hostname).second)
	connect_timeout += mDNSLookupGrace;			// If the host is not in the list, increase the connect timeout with mDNSLookupGrace.
  return connect_timeout;
}

//static
bool AIHTTPTimeoutPolicy::connect_timed_out(std::string const& hostname)
{
  llassert(curlthread.equals_current_thread());

  // This is called when a connect to hostname timed out on connect.
  // If the hostname is currently in the list, remove it and return true
  // so that subsequent connects will get more time to connect.
  // Otherwise return false.
  return gSeenHostnames.erase(hostname) > 0;
}

//=======================================================================================================
// Start of policy operation definitions.

namespace AIHTTPTimeoutPolicyOperators {

// Note: Policies are applied in the order First(x, Second(y, Third(z))) etc,
// where the last (Third) has the highest priority.
// For example: Transaction(5, Connect(40)) would first enforce a transaction time of 5 seconds,
// and then a connect time of 40 seconds, even if that would mean increasing the transaction
// time again.

struct DNS : PolicyOp {
  int mSeconds;
  DNS(int seconds) : mSeconds(seconds) { }
  DNS(int seconds, PolicyOp& op) : PolicyOp(op), mSeconds(seconds) { }
  static void fix(AIHTTPTimeoutPolicy* policy);
  static U16 min(void) { return ABS_min_DNS_lookup; }
  static U16 max(void) { return ABS_max_DNS_lookup; }
  virtual void perform(AIHTTPTimeoutPolicy* policy) const;
};

struct Connect : PolicyOp {
  int mSeconds;
  Connect(int seconds) : mSeconds(seconds) { }
  Connect(int seconds, PolicyOp& op) : PolicyOp(op), mSeconds(seconds) { }
  static void fix(AIHTTPTimeoutPolicy* policy);
  static U16 min(void) { return ABS_min_connect_time; }
  static U16 max(void) { return ABS_max_connect_time; }
  virtual void perform(AIHTTPTimeoutPolicy* policy) const;
};

struct Reply : PolicyOp {
  int mSeconds;
  Reply(int seconds) : mSeconds(seconds) { }
  Reply(int seconds, PolicyOp& op) : PolicyOp(op), mSeconds(seconds) { }
  static void fix(AIHTTPTimeoutPolicy* policy);
  static U16 min(void) { return ABS_min_reply_delay; }
  static U16 max(void) { return ABS_max_reply_delay; }
  virtual void perform(AIHTTPTimeoutPolicy* policy) const;
};

struct Speed : PolicyOp {
  int mSeconds;
  int mRate;
  Speed(int seconds, int rate) : mSeconds(seconds), mRate(rate) { }
  Speed(int seconds, int rate, PolicyOp& op) : PolicyOp(op), mSeconds(seconds), mRate(rate) { }
  static void fix(AIHTTPTimeoutPolicy* policy);
  static U16 min(void) { return ABS_min_low_speed_time; }
  static U16 max(AIHTTPTimeoutPolicy const* policy) { return llmin(ABS_max_low_speed_time, (U16)(policy->mMaximumCurlTransaction / 2)); }
  static U32 lmin(void) { return ABS_min_low_speed_limit; }
  static U32 lmax(void) { return ABS_max_low_speed_limit; }
  virtual void perform(AIHTTPTimeoutPolicy* policy) const;
};

struct Transaction : PolicyOp {
  int mSeconds;
  Transaction(int seconds) : mSeconds(seconds) { }
  Transaction(int seconds, PolicyOp& op) : PolicyOp(op), mSeconds(seconds) { }
  static void fix(AIHTTPTimeoutPolicy* policy);
  static U16 min(AIHTTPTimeoutPolicy const* policy) { return llmax(ABS_min_transaction, (U16)(policy->mMaximumConnectTime + policy->mMaximumReplyDelay + 4 * policy->mLowSpeedTime)); }
  static U16 max(void) { return ABS_max_transaction; }
  virtual void perform(AIHTTPTimeoutPolicy* policy) const;
};

struct Total : PolicyOp {
  int mSeconds;
  Total(int seconds) : mSeconds(seconds) { }
  Total(int seconds, PolicyOp& op) : PolicyOp(op), mSeconds(seconds) { }
  static void fix(AIHTTPTimeoutPolicy* policy);
  static U16 min(AIHTTPTimeoutPolicy const* policy) { return llmax(ABS_min_total_delay, (U16)(policy->mMaximumCurlTransaction + 1)); }
  static U16 max(void) { return ABS_max_total_delay; }
  virtual void perform(AIHTTPTimeoutPolicy* policy) const;
};

void DNS::perform(AIHTTPTimeoutPolicy* policy) const
{
  policy->mDNSLookupGrace = mSeconds;
  fix(policy);
  nextOp(policy);
}

void Connect::perform(AIHTTPTimeoutPolicy* policy) const
{
  policy->mMaximumConnectTime = mSeconds;
  fix(policy);
  nextOp(policy);
}

void Reply::perform(AIHTTPTimeoutPolicy* policy) const
{
  policy->mMaximumReplyDelay = mSeconds;
  fix(policy);
  nextOp(policy);
}

void Speed::perform(AIHTTPTimeoutPolicy* policy) const
{
  policy->mLowSpeedTime = mSeconds;
  policy->mLowSpeedLimit = mRate;
  fix(policy);
  nextOp(policy);
}

void Transaction::perform(AIHTTPTimeoutPolicy* policy) const
{
  policy->mMaximumCurlTransaction = mSeconds;
  fix(policy);
  nextOp(policy);
}

void Total::perform(AIHTTPTimeoutPolicy* policy) const
{
  policy->mMaximumTotalDelay = mSeconds;
  fix(policy);
  nextOp(policy);
}

void DNS::fix(AIHTTPTimeoutPolicy* policy)
{
  if (policy->mDNSLookupGrace > max())
  {
	policy->mDNSLookupGrace = max();
  }
  else if (policy->mDNSLookupGrace < min())
  {
	policy->mDNSLookupGrace = min();
  }
}

void Connect::fix(AIHTTPTimeoutPolicy* policy)
{
  bool changed = false;
  if (policy->mMaximumConnectTime > max())
  {
	policy->mMaximumConnectTime = max();
	changed = true;
  }
  else if (policy->mMaximumConnectTime < min())
  {
	policy->mMaximumConnectTime = min();
	changed = true;
  }
  if (changed)
  {
	// Transaction limits depend on Connect.
	Transaction::fix(policy);
  }
}

void Reply::fix(AIHTTPTimeoutPolicy* policy)
{
  bool changed = false;
  if (policy->mMaximumReplyDelay > max())
  {
	policy->mMaximumReplyDelay = max();
	changed = true;
  }
  else if (policy->mMaximumReplyDelay < min())
  {
	policy->mMaximumReplyDelay = min();
	changed = true;
  }
  if (changed)
  {
	// Transaction limits depend on Reply.
	Transaction::fix(policy);
  }
}

void Speed::fix(AIHTTPTimeoutPolicy* policy)
{
  bool changed = false;
  if (policy->mLowSpeedTime > ABS_max_low_speed_time)
  {
	policy->mLowSpeedTime = ABS_max_low_speed_time;
	changed = true;
  }
  else if (policy->mLowSpeedTime != 0 && policy->mLowSpeedTime < min())
  {
	policy->mLowSpeedTime = min();
	changed = true;
  }
  if (changed)
  {
	// Transaction limits depend on Speed time.
	Transaction::fix(policy);
  }
  if (policy->mLowSpeedTime > max(policy))
  {
	policy->mLowSpeedTime = max(policy);
  }
  if (policy->mLowSpeedLimit > lmax())
  {
	policy->mLowSpeedLimit = lmax();
  }
  else if (policy->mLowSpeedLimit != 0 && policy->mLowSpeedLimit < lmin())
  {
	policy->mLowSpeedLimit = lmin();
  }
}

void Transaction::fix(AIHTTPTimeoutPolicy* policy)
{
  bool changed = false;
  if (policy->mMaximumCurlTransaction > max())
  {
	policy->mMaximumCurlTransaction = max();
	changed = true;
  }
  else if (policy->mMaximumCurlTransaction < ABS_min_transaction)
  {
	policy->mMaximumCurlTransaction = ABS_min_transaction;
	changed = true;
  }
  if (changed)
  {
	// Totals minimum limit depends on Transaction.
	Total::fix(policy);
	// Transaction limits depend on Connect, Reply and Speed time.
	if (policy->mMaximumCurlTransaction < min(policy))
	{
	  // We need to achieve the following (from Transaction::min()):
	  // policy->mMaximumCurlTransaction >= policy->mMaximumConnectTime + policy->mMaximumReplyDelay + 4 * policy->mLowSpeedTime

	  // There isn't a single way to fix this, so we just do something randomly intuitive.
	  // We consider the vector space <connect_time, reply_delay, low_speed_time>;
	  // In other words, we need to compare with the dot product of <1, 1, 4>.
	  LLVector3 const ref(1, 1, 4);

	  // The shortest allowed vector is:
	  LLVector3 const vec_min(ABS_min_connect_time, ABS_min_reply_delay, ABS_min_low_speed_time);

	  // Initialize the result vector to (0, 0, 0) (in the vector space with shifted origin).
	  LLVector3 vec_res;

	  // Check if there is a solution at all:
	  if (policy->mMaximumCurlTransaction > ref * vec_min)					// Is vec_min small enough?
	  {
		// The current point is:
		LLVector3 vec_cur(policy->mMaximumConnectTime, policy->mMaximumReplyDelay, policy->mLowSpeedTime);

		// The default point is:
		LLVector3 vec_def(AITP_default_maximum_connect_time, AITP_default_maximum_reply_delay, AITP_default_low_speed_time);

		// Move the origin.
		vec_cur -= vec_min;
		vec_def -= vec_min;

		// Normalize the default vector (we only need it's direction).
		vec_def.normalize();

		// Project the current vector onto the default vector (dp = default projection):
		LLVector3 vec_dp = vec_def * (vec_cur * vec_def);

		// Check if the projection is a solution and choose the vectors between which the result lays.
		LLVector3 a;			// vec_min is too small (a = (0, 0, 0) which corresponds to vec_min).
		LLVector3 b = vec_cur;	// vec_cur is too large.
		if (policy->mMaximumCurlTransaction > ref * (vec_dp + vec_min))		// Is vec_dp small enough too?
		{
		  a = vec_dp;	// New lower bound.
		}
		else
		{
		  b = vec_dp;	// New upper bound.
		}
		// Find vec_res = a + lambda * (b - a), where 0 <= lambda <= 1, such that
		// policy->mMaximumCurlTransaction == ref * (vec_res + vec_min).
		//
		// Note that ref * (b - a) must be non-zero because if it wasn't then changing lambda wouldn't have
		// any effect on right-hand side of the equation (ref * (vec_res + vec_min)) which in contradiction
		// with the fact that a is a solution and b is not.
		F32 lambda = (policy->mMaximumCurlTransaction - ref * (a + vec_min)) / (ref * (b - a));
		vec_res = a + lambda * (b - a);
	  }

	  // Shift origin back and fill in the result.
	  vec_res += vec_min;
	  policy->mMaximumConnectTime = vec_res[VX];
	  policy->mMaximumReplyDelay = vec_res[VY];
	  policy->mLowSpeedTime = vec_res[VZ];
	}
  }
  if (policy->mMaximumCurlTransaction < min(policy))
  {
	policy->mMaximumCurlTransaction = min(policy);
  }
}

void Total::fix(AIHTTPTimeoutPolicy* policy)
{
  bool changed = false;
  if (policy->mMaximumTotalDelay > max())
  {
	policy->mMaximumTotalDelay = max();
	changed = true;
  }
  else if (policy->mMaximumTotalDelay < ABS_min_total_delay)
  {
	policy->mMaximumTotalDelay = ABS_min_total_delay;
	changed = true;
  }
  if (changed)
  {
	// Totals minimum limit depends on Transaction.
	// We have to correct mMaximumCurlTransaction such that (from Total::min)
	// mMaximumTotalDelay >= llmax((int)ABS_min_total_delay, policy->mMaximumCurlTransaction + 1)
	if (policy->mMaximumTotalDelay < policy->mMaximumCurlTransaction + 1)
	{
	  policy->mMaximumCurlTransaction = policy->mMaximumTotalDelay - 1;
	}
  }
  if (policy->mMaximumTotalDelay < min(policy))
  {
	policy->mMaximumTotalDelay = min(policy);
  }
}

} // namespace AIHTTPTimeoutPolicyOperators

void AIHTTPTimeoutPolicy::sanity_checks(void) const
{
  // Sanity checks.
  llassert(            DNS::min() <= mDNSLookupGrace         &&         mDNSLookupGrace <= DNS::max());
  llassert(        Connect::min() <= mMaximumConnectTime     &&     mMaximumConnectTime <= Connect::max());
  llassert(          Reply::min() <= mMaximumReplyDelay      &&      mMaximumReplyDelay <= Reply::max());
  llassert(mLowSpeedTime == 0   ||
	                (Speed::min() <= mLowSpeedTime           &&           mLowSpeedTime <= Speed::max(this)));
  llassert(mLowSpeedLimit == 0  ||
	               (Speed::lmin() <= mLowSpeedLimit          &&          mLowSpeedLimit <= Speed::lmax()));
  llassert(Transaction::min(this) <= mMaximumCurlTransaction && mMaximumCurlTransaction <= Transaction::max());
  llassert(      Total::min(this) <= mMaximumTotalDelay      &&      mMaximumTotalDelay <= Total::max());
}

//=======================================================================================================
// Start of policy definitions.

// Policy with hardcoded default values.
AIHTTPTimeoutPolicyBase HTTPTimeoutPolicy_default(
		AITP_default_DNS_lookup_grace,
		AITP_default_maximum_connect_time,
		AITP_default_maximum_reply_delay,
		AITP_default_low_speed_time,
		AITP_default_low_speed_limit,
		AITP_default_maximum_curl_transaction,
		AITP_default_maximum_total_delay);

//static. Initialized here, but shortly overwritten by Debug Settings (except for the crash logger, in which case these are the actual values).
AIHTTPTimeoutPolicyBase AIHTTPTimeoutPolicy::sDebugSettingsCurlTimeout(
		AITP_default_DNS_lookup_grace,
		AITP_default_maximum_connect_time,
		AITP_default_maximum_reply_delay,
		AITP_default_low_speed_time,
		AITP_default_low_speed_limit,
		AITP_default_maximum_curl_transaction,
		AITP_default_maximum_total_delay);

// Note: All operator objects (Transaction, Connect, etc) must be globals (not temporaries)!
// To enforce this they are passes as reference to non-const (but will never be changed).

// Used for baseCapabilitiesComplete - which is retried many times, and needs to be rather "fast".
// Most connection succeed under 5 seconds.
Connect connectOp5s(5);
AIHTTPTimeoutPolicyBase connect_5s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	connectOp5s
	);

// Some services connects rather slow. We need to enforce that connect timeouts never drop below 10 seconds for the policies that shorten the transaction time.
Connect connectOp10s(10);
AIHTTPTimeoutPolicyBase connect_10s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	connectOp10s
	);

// The texture/mesh service suffers often from timeouts, give them a much longer connect timeout than normal.
Connect connectOp30s(30);
AIHTTPTimeoutPolicyBase connect_30s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	connectOp30s
	);

// Extreme measures.
Connect connectOp60s(60);
AIHTTPTimeoutPolicyBase connect_60s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	connectOp60s
	);

// This used to be '18 seconds'. Pass connectOp5s or it would become 3 seconds (which is working already pretty good actually).
Transaction transactionOp18s(18, connectOp5s);
AIHTTPTimeoutPolicyBase transfer_18s_connect_5s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	transactionOp18s
	);

// This used to be '18 seconds'. Pass connectOp10s or it would become 3 seconds.
Transaction transactionOp22s(22, connectOp10s);
AIHTTPTimeoutPolicyBase transfer_22s_connect_10s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	transactionOp22s
	);

// This used to be '30 seconds'. Pass connectOp10s or it would become 3 seconds.
Transaction transactionOp30s(30, connectOp10s);
AIHTTPTimeoutPolicyBase transfer_30s_connect_10s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	transactionOp30s
	);

// This used to be '300 seconds'. We derive this from the hardcoded result so users can't mess with it.
Transaction transactionOp300s(300);
AIHTTPTimeoutPolicyBase transfer_300s(HTTPTimeoutPolicy_default,
	transactionOp300s
	);

// This used to be a call to setopt(CURLOPT_CONNECTTIMEOUT, 40L) with the remark 'Be a little impatient about establishing connections.'
Connect connectOp40s(40);
AIHTTPTimeoutPolicyBase connect_40s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	connectOp40s
	);

// This used to be FETCHING_TIMEOUT (for HTTP textures), being a 15 second timeout from start of request till finishing receiving all data.
// That seems way to demanding however; lets use a 15 second reply delay demand instead.
Reply replyOp15s(15);
AIHTTPTimeoutPolicyBase reply_15s(AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout(),
	replyOp15s
	);

// End of policy definitions.
//=======================================================================================================

bool validateCurlTimeoutDNSLookup(LLSD const& newvalue)
{
  U32 new_value = newvalue.asInteger();
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::DNS op(new_value);
  op.perform(&timeout);
  return timeout.getDNSLookup() == new_value;
}

bool handleCurlTimeoutDNSLookup(LLSD const& newvalue)
{
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::DNS op(newvalue.asInteger());
  op.perform(&timeout);
  AIHTTPTimeoutPolicy::setDefaultCurlTimeout(timeout);
  return true;
}

bool validateCurlTimeoutConnect(LLSD const& newvalue)
{
  U32 new_value = newvalue.asInteger();
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Connect op(new_value);
  op.perform(&timeout);
  return timeout.getConnect() == new_value;
}

bool handleCurlTimeoutConnect(LLSD const& newvalue)
{
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Connect op(newvalue.asInteger());
  op.perform(&timeout);
  AIHTTPTimeoutPolicy::setDefaultCurlTimeout(timeout);
  return true;
}

bool validateCurlTimeoutReplyDelay(LLSD const& newvalue)
{
  U32 new_value = newvalue.asInteger();
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Reply op(new_value);
  op.perform(&timeout);
  return timeout.getReplyDelay() == new_value;
}

bool handleCurlTimeoutReplyDelay(LLSD const& newvalue)
{
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Reply op(newvalue.asInteger());
  op.perform(&timeout);
  AIHTTPTimeoutPolicy::setDefaultCurlTimeout(timeout);
  return true;
}

bool validateCurlTimeoutLowSpeedLimit(LLSD const& newvalue)
{
  U32 new_value = newvalue.asInteger();
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Speed op(timeout.getLowSpeedTime(), new_value);
  op.perform(&timeout);
  return timeout.getLowSpeedLimit() == new_value;
}

bool handleCurlTimeoutLowSpeedLimit(LLSD const& newvalue)
{
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Speed op(timeout.getLowSpeedTime(), newvalue.asInteger());
  op.perform(&timeout);
  AIHTTPTimeoutPolicy::setDefaultCurlTimeout(timeout);
  return true;
}

bool validateCurlTimeoutLowSpeedTime(LLSD const& newvalue)
{
  U32 new_value = newvalue.asInteger();
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Speed op(new_value, timeout.getLowSpeedLimit());
  op.perform(&timeout);
  return timeout.getLowSpeedTime() == new_value;
}

bool handleCurlTimeoutLowSpeedTime(LLSD const& newvalue)
{
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Speed op(newvalue.asInteger(), timeout.getLowSpeedLimit());
  op.perform(&timeout);
  AIHTTPTimeoutPolicy::setDefaultCurlTimeout(timeout);
  return true;
}

bool validateCurlTimeoutMaxTransaction(LLSD const& newvalue)
{
  U32 new_value = newvalue.asInteger();
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Transaction op(new_value);
  op.perform(&timeout);
  return timeout.getCurlTransaction() == new_value;
}

bool handleCurlTimeoutMaxTransaction(LLSD const& newvalue)
{
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Transaction op(newvalue.asInteger());
  op.perform(&timeout);
  AIHTTPTimeoutPolicy::setDefaultCurlTimeout(timeout);
  return true;
}

bool validateCurlTimeoutMaxTotalDelay(LLSD const& newvalue)
{
  U32 new_value = newvalue.asInteger();
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Total op(new_value);
  op.perform(&timeout);
  return timeout.getTotalDelay() == new_value;
}

bool handleCurlTimeoutMaxTotalDelay(LLSD const& newvalue)
{
  AIHTTPTimeoutPolicyBase timeout = AIHTTPTimeoutPolicyBase::getDebugSettingsCurlTimeout();
  AIHTTPTimeoutPolicyOperators::Total op(newvalue.asInteger());
  op.perform(&timeout);
  AIHTTPTimeoutPolicy::setDefaultCurlTimeout(timeout);
  return true;
}

//static
AIHTTPTimeoutPolicy::namemap_t AIHTTPTimeoutPolicy::sNameMap;

//static
AIHTTPTimeoutPolicy const* AIHTTPTimeoutPolicy::getTimeoutPolicyByName(std::string const& name)
{
  namemap_t::iterator iter = sNameMap.find(name);
  if (iter == sNameMap.end())
  {
	if (!name.empty())
	{
	  llwarns << "Cannot find AIHTTPTimeoutPolicy with name \"" << name << "\"." << llendl;
	}
	return &sDebugSettingsCurlTimeout;
  }
  return iter->second;
}

//=======================================================================================================
// Start of Responder timeout policy list.

// Note: to find the actual responder class back, search for the name listed here but with upper case first character.
// For example, if the actual responder class is LLAccountingCostResponder then the name used here is accountingCostResponder.

#undef P
#define P(n)		AIHTTPTimeoutPolicy n##_timeout(#n)
#define P2(n, b)	AIHTTPTimeoutPolicy n##_timeout(#n, b)

// Policy name									Policy
P(accountingCostResponder);
P(agentStateResponder);
P(appearanceChangeMetricsResponder);
P(assetUploadResponder);
P(assetReportHandler);
P(asyncConsoleResponder);
P(avatarPickerResponder);
P(authHandler);
P(avatarNameResponder);
P2(baseCapabilitiesComplete,					transfer_18s_connect_5s);
P(blockingLLSDPost);
P(blockingLLSDGet);
P(blockingRawGet);
P(charactersResponder);
P(checkAgentAppearanceServiceResponder);
P(classifiedStatsResponder);
P(consoleResponder);
P2(crashLoggerResponder,						transfer_22s_connect_10s);
P(createInventoryCategoryResponder);
P(emeraldDicDownloader);
P(environmentApplyResponder);
P(environmentRequestResponder);
P(estateChangeInfoResponder);
P(eventPollResponder);
P(fetchInventoryResponder);
P(fetchScriptLimitsAttachmentInfoResponder);
P(fetchScriptLimitsRegionDetailsResponder);
P(fetchScriptLimitsRegionInfoResponder);
P(fetchScriptLimitsRegionSummaryResponder);
P(fnPtrResponder);
P2(gamingDataReceived,							transfer_22s_connect_10s);
P2(groupMemberDataResponder,					transfer_300s);
P2(groupProposalBallotResponder,				transfer_300s);
P(homeLocationResponder);
P2(HTTPGetResponder,							reply_15s);
P(iamHereLogin);
P(iamHere);
P(iamHereVoice);
P2(inventoryModelFetchDescendentsResponder,		transfer_300s);
P(inventoryModelFetchItemResponder);
P(lcl_responder);
P(MPImportGetResponder);
P(MPImportPostResponder);
P(mapLayerResponder);
P2(maturityPreferences,							transfer_30s_connect_10s);
P(mediaDataClientResponder);
P(mediaTypeResponder);
P2(meshDecompositionResponder,					connect_30s);
P2(meshHeaderResponder,							connect_30s);
P2(meshLODResponder,							connect_30s);
P2(meshPhysicsShapeResponder,					connect_30s);
P2(meshSkinInfoResponder,						connect_30s);
P(mimeDiscoveryResponder);
P(moderationResponder);
P(navMeshRebakeResponder);
P(navMeshResponder);
P(navMeshStatusResponder);
P(newAgentInventoryVariablePriceResponder);
P(objectCostResponder);
P(objectLinksetsResponder);
P(physicsFlagsResponder);
P(placeAvatarTeleportResponder);
P(productInfoRequestResponder);
P(regionResponder);
P(remoteParcelRequestResponder);
P(requestAgentUpdateAppearance);
P(responderIgnore);
P(sessionInviteResponder);
P(setDisplayNameResponder);
P2(simulatorFeaturesReceived,					transfer_22s_connect_10s);
P(startConferenceChatResponder);
P2(startGroupVoteResponder,						transfer_300s);
P(terrainLinksetsResponder);
P(translationReceiver);
P(uploadModelPremissionsResponder);
P(userReportResponder);
P(verifiedDestinationResponder);
P(viewerChatterBoxInvitationAcceptResponder);
P(viewerMediaOpenIDResponder);
P(viewerMediaWebProfileResponder);
P(viewerStatsResponder);
P(vivoxVoiceAccountProvisionResponder);
P(vivoxVoiceClientCapResponder);
P(voiceCallCapResponder);
P(webProfileResponders);
P(wholeModelFeeResponder);
P(wholeModelUploadResponder);
P2(XMLRPCResponder,								connect_40s);

