/** 
 * @file lltexturefetch.h
 * @brief Object for managing texture fetches.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLTEXTUREFETCH_H
#define LL_LLTEXTUREFETCH_H

#include "lldir.h"
#include "llimage.h"
#include "lluuid.h"
#include "llworkerthread.h"
#include "llcurl.h"
#include "lltextureinfo.h"
#include "llapr.h"

class LLViewerTexture;
class LLTextureFetchWorker;
class HTTPGetResponder;
class LLTextureCache;
class LLImageDecodeThread;
class LLHost;
#if HTTP_METRICS
class LLViewerAssetStats;
#endif

// Interface class
class LLTextureFetch : public LLWorkerThread
{
	friend class LLTextureFetchWorker;
	friend class HTTPGetResponder;
	
public:
	LLTextureFetch(LLTextureCache* cache, LLImageDecodeThread* imagedecodethread, bool threaded, bool qa_mode = false);
	~LLTextureFetch();

	class TFRequest;
	
	/*virtual*/ S32 update(U32 max_time_ms);	
	void shutDownTextureCacheThread() ; //called in the main thread after the TextureCacheThread shuts down.
	void shutDownImageDecodeThread() ;  //called in the main thread after the ImageDecodeThread shuts down.

	bool createRequest(const std::string& url, const LLUUID& id, const LLHost& host, F32 priority,
					   S32 w, S32 h, S32 c, S32 discard, bool needs_aux, bool can_use_http);
	void deleteRequest(const LLUUID& id, bool cancel);
	bool getRequestFinished(const LLUUID& id, S32& discard_level,
							LLPointer<LLImageRaw>& raw, LLPointer<LLImageRaw>& aux);
	bool updateRequestPriority(const LLUUID& id, F32 priority);

	bool receiveImageHeader(const LLHost& host, const LLUUID& id, U8 codec, U16 packets, U32 totalbytes, U16 data_size, U8* data);
	bool receiveImagePacket(const LLHost& host, const LLUUID& id, U16 packet_num, U16 data_size, U8* data);

	void setTextureBandwidth(F32 bandwidth) { mTextureBandwidth = bandwidth; }
	F32 getTextureBandwidth() { return mTextureBandwidth; }
	
	// Debug
	BOOL isFromLocalCache(const LLUUID& id);
	S32 getFetchState(const LLUUID& id, F32& decode_progress_p, F32& requested_priority_p,
					  U32& fetch_priority_p, F32& fetch_dtime_p, F32& request_dtime_p, bool& can_use_http);
	void dump();
	S32 getNumRequests() ;
	S32 getNumHTTPRequests() ;
	U32 getTotalNumHTTPRequests() ;
	
	// Public for access by callbacks
    S32 getPending();
	void lockQueue() { mQueueMutex.lock(); }
	void unlockQueue() { mQueueMutex.unlock(); }
	LLTextureFetchWorker* getWorker(const LLUUID& id);
	LLTextureFetchWorker* getWorkerAfterLock(const LLUUID& id);

	LLTextureInfo* getTextureInfo() { return &mTextureInfo; }

#if HTTP_METRICS
	// Commands available to other threads to control metrics gathering operations.
	void commandSetRegion(U64 region_handle);
	void commandSendMetrics(const std::string & caps_url,
							const LLUUID & session_id,
							const LLUUID & agent_id,
							LLViewerAssetStats * main_stats);
	void commandDataBreak();

	LLCurlRequest & getCurlRequest()	{ return *mCurlGetRequest; }

	bool isQAMode() const				{ return mQAMode; }

	// Curl POST counter maintenance
	inline void incrCurlPOSTCount()		{ mCurlPOSTRequestCount++; }
	inline void decrCurlPOSTCount()		{ mCurlPOSTRequestCount--; }
#endif

protected:
	void addToNetworkQueue(LLTextureFetchWorker* worker);
	void removeFromNetworkQueue(LLTextureFetchWorker* worker, bool cancel);
	void addToHTTPQueue(const LLUUID& id);
	void removeFromHTTPQueue(const LLUUID& id, S32 received_size = 0);
	void removeRequest(LLTextureFetchWorker* worker, bool cancel);

	// Overrides from the LLThread tree
	bool runCondition();

private:
	void sendRequestListToSimulators();
	/*virtual*/ void startThread(void);
	/*virtual*/ void endThread(void);
	/*virtual*/ void threadedUpdate(void);
	void commonUpdate();

#if HTTP_METRICS
	// Metrics command helpers
	/**
	 * Enqueues a command request at the end of the command queue
	 * and wakes up the thread as needed.
	 *
	 * Takes ownership of the TFRequest object.
	 *
	 * Method locks the command queue.
	 */
	void cmdEnqueue(TFRequest *);

	/**
	 * Returns the first TFRequest object in the command queue or
	 * NULL if none is present.
	 *
	 * Caller acquires ownership of the object and must dispose of it.
	 *
	 * Method locks the command queue.
	 */
	TFRequest * cmdDequeue();

	/**
	 * Processes the first command in the queue disposing of the
	 * request on completion.  Successive calls are needed to perform
	 * additional commands.
	 *
	 * Method locks the command queue.
	 */
	void cmdDoWork();
#endif
	
public:
	LLUUID mDebugID;
	S32 mDebugCount;
	BOOL mDebugPause;
	S32 mPacketCount;
	S32 mBadPacketCount;
	
private:
	LLMutex mQueueMutex;        //to protect mRequestMap only
	LLMutex mNetworkQueueMutex; //to protect mNetworkQueue, mHTTPTextureQueue and mCancelQueue.

	LLTextureCache* mTextureCache;
	LLImageDecodeThread* mImageDecodeThread;
	LLCurlRequest* mCurlGetRequest;
	
	// Map of all requests by UUID
	typedef std::map<LLUUID,LLTextureFetchWorker*> map_t;
	map_t mRequestMap;

	// Set of requests that require network data
	typedef std::set<LLUUID> queue_t;
	queue_t mNetworkQueue;
	queue_t mHTTPTextureQueue;
	typedef std::map<LLHost,std::set<LLUUID> > cancel_queue_t;
	cancel_queue_t mCancelQueue;
	F32 mTextureBandwidth;
	F32 mMaxBandwidth;
	LLTextureInfo mTextureInfo;

	U32 mHTTPTextureBits;

	//debug use
	U32 mTotalHTTPRequests ;

#if HTTP_METRICS
	// Out-of-band cross-thread command queue.  This command queue
	// is logically tied to LLQueuedThread's list of
	// QueuedRequest instances and so must be covered by the
	// same locks.
	typedef std::vector<TFRequest *> command_queue_t;
	command_queue_t mCommands;

	// If true, modifies some behaviors that help with QA tasks.
	const bool mQAMode;

	// Count of POST requests outstanding.  We maintain the count
	// indirectly in the CURL request responder's ctor and dtor and
	// use it when determining whether or not to sleep the thread.  Can't
	// use the LLCurl module's request counter as it isn't thread compatible.
	// *NOTE:  Don't mix Atomic and static, apr_initialize must be called first.
	LLAtomic32<S32> mCurlPOSTRequestCount;

public:
	// A probabilistically-correct indicator that the current
	// attempt to log metrics follows a break in the metrics stream
	// reporting due to either startup or a problem POSTing data.
	static volatile bool svMetricsDataBreak;
#endif
};

#endif // LL_LLTEXTUREFETCH_H

