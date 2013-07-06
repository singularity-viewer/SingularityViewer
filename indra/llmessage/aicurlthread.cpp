/**
 * @file aicurlthread.cpp
 * @brief Implementation of AICurl, curl thread functions.
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

#include "linden_common.h"
#include "aicurlthread.h"
#include "aihttptimeoutpolicy.h"
#include "aihttptimeout.h"
#include "aicurlperservice.h"
#include "aiaverage.h"
#include "aicurltimer.h"
#include "lltimer.h"		// ms_sleep, get_clock_count
#include "llhttpstatuscodes.h"
#include "llbuffer.h"
#include "llcontrol.h"
#include <sys/types.h>
#if !LL_WINDOWS
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <deque>
#include <cctype>

// On linux, add -DDEBUG_WINDOWS_CODE_ON_LINUX to test the windows code used in this file.
#if !defined(DEBUG_WINDOWS_CODE_ON_LINUX) || !defined(LL_LINUX) || defined(LL_RELEASE)
#undef DEBUG_WINDOWS_CODE_ON_LINUX
#define DEBUG_WINDOWS_CODE_ON_LINUX 0
#endif

#if DEBUG_WINDOWS_CODE_ON_LINUX

struct windows_fd_set {
  unsigned int fd_count;
  curl_socket_t fd_array[64];
};

unsigned int const not_found  = (unsigned int)-1;

static inline unsigned int find_fd(curl_socket_t s, windows_fd_set const* fsp)
{
  for (unsigned int i = 0; i < fsp->fd_count; ++i)
  {
	if (fsp->fd_array[i] == s)
	  return i;
  }
  return not_found;
}

static int windows_select(int, windows_fd_set* readfds, windows_fd_set* writefds, windows_fd_set*, timeval* val)
{
  fd_set r;
  fd_set w;
  FD_ZERO(&r);
  FD_ZERO(&w);
  int mfd = -1;
  if (readfds)
  {
	for (int i  = 0; i < readfds->fd_count; ++i)
	{
	  int fd = readfds->fd_array[i];
	  FD_SET(fd, &r);
	  mfd = llmax(mfd, fd);
	}
  }
  if (writefds)
  {
	for (int i  = 0; i < writefds->fd_count; ++i)
	{
	  int fd = writefds->fd_array[i];
	  FD_SET(fd, &w);
	  mfd = llmax(mfd, fd);
	}
  }
  int nfds = select(mfd + 1, readfds ? &r : NULL, writefds ? &w : NULL, NULL, val);
  if (readfds)
  {
	unsigned int fd_count = 0;
	for (int i  = 0; i < readfds->fd_count; ++i)
	{
	  if (FD_ISSET(readfds->fd_array[i], &r))
		readfds->fd_array[fd_count++] = readfds->fd_array[i];
	}
	readfds->fd_count = fd_count;
  }
  if (writefds)
  {
	unsigned int fd_count = 0;
	for (int i  = 0; i < writefds->fd_count; ++i)
	{
	  if (FD_ISSET(writefds->fd_array[i], &w))
		writefds->fd_array[fd_count++] = writefds->fd_array[i];
	}
	writefds->fd_count = fd_count;
  }
  return nfds;
}

#undef FD_SETSIZE
#undef FD_ZERO
#undef FD_ISSET
#undef FD_SET
#undef FD_CLR

int const FD_SETSIZE  = sizeof(windows_fd_set::fd_array) / sizeof(curl_socket_t);

static void FD_ZERO(windows_fd_set* fsp)
{
  fsp->fd_count = 0;
}

static bool FD_ISSET(curl_socket_t s, windows_fd_set const* fsp)
{
  return find_fd(s, fsp) != not_found;
}

static void FD_SET(curl_socket_t s, windows_fd_set* fsp)
{
  llassert(!FD_ISSET(s, fsp));
  fsp->fd_array[fsp->fd_count++] = s;
}

static void FD_CLR(curl_socket_t s, windows_fd_set* fsp)
{
  unsigned int i = find_fd(s, fsp);
  llassert(i != not_found);
  fsp->fd_array[i] = fsp->fd_array[--(fsp->fd_count)];
}

#define fd_set windows_fd_set
#define select windows_select

int WSAGetLastError(void)
{
  return errno;
}

typedef char* LPTSTR;

bool FormatMessage(int, void*, int e, int, LPTSTR error_str_ptr, int, void*)
{
  char* error_str = *(LPTSTR*)error_str_ptr;
  error_str = strerror(e);
  return true;
}

void LocalFree(LPTSTR)
{
}

int const FORMAT_MESSAGE_ALLOCATE_BUFFER = 0;
int const FORMAT_MESSAGE_FROM_SYSTEM = 0;
int const FORMAT_MESSAGE_IGNORE_INSERTS = 0;
int const INVALID_SOCKET = -1;
int const SOCKET_ERROR = -1;
int const WSAEWOULDBLOCK = EWOULDBLOCK;

int closesocket(curl_socket_t fd)
{
  return close(fd);
}

int const FIONBIO = 0;

int ioctlsocket(int fd, int, unsigned long* nonblocking_enable)
{
  int res = fcntl(fd, F_GETFL, 0);
  llassert_always(res != -1);
  if (*nonblocking_enable)
	res |= O_NONBLOCK;
  else
	res &= ~O_NONBLOCK;
  return fcntl(fd, F_SETFD, res);
}

#endif // DEBUG_WINDOWS_CODE_ON_LINUX

#define WINDOWS_CODE (LL_WINDOWS || DEBUG_WINDOWS_CODE_ON_LINUX)

#undef AICurlPrivate

namespace AICurlPrivate {

enum command_st {
  cmd_none,
  cmd_add,
  cmd_boost,
  cmd_remove
};

class Command {
  public:
	Command(void) : mCommand(cmd_none) { }
	Command(AICurlEasyRequest const& easy_request, command_st command) : mCurlEasyRequest(easy_request.get_ptr()), mCommand(command) { }

	command_st command(void) const { return mCommand; }
	BufferedCurlEasyRequestPtr const& easy_request(void) const { return mCurlEasyRequest; }

	bool operator==(AICurlEasyRequest const& easy_request) const { return mCurlEasyRequest == easy_request.get_ptr(); }

	void reset(void);

  private:
	BufferedCurlEasyRequestPtr mCurlEasyRequest;
	command_st mCommand;
};

void Command::reset(void)
{
  mCurlEasyRequest.reset();
  mCommand = cmd_none;
}

// The following two globals have separate locks for speed considerations (in order not
// to block the main thread unnecessarily) but have the following correlation:
//
// MAIN-THREAD (AICurlEasyRequest::addRequest)
// * command_queue locked
//   - A non-active (mActiveMultiHandle is NULL) ThreadSafeBufferedCurlEasyRequest (by means of an AICurlEasyRequest pointing to it) is added to command_queue with as command cmd_add.
// * command_queue unlocked
//
// If at this point addRequest is called again, then it is detected that the last command added to the queue
// for this ThreadSafeBufferedCurlEasyRequest is cmd_add.
//
// CURL-THREAD (AICurlThread::wakeup):
// * command_queue locked
//   * command_being_processed is write-locked
//     - command_being_processed is assigned the value of the command in the queue.
//   * command_being_processed is unlocked
//   - The command is removed from command_queue
// * command_queue unlocked
//
// If at this point addRequest is called again, then it is detected that command_being_processed adds the same ThreadSafeBufferedCurlEasyRequest.
//
// * command_being_processed is read-locked
//   - mActiveMultiHandle is set to point to the curl multi handle
//   - The easy handle is added to the multi handle
// * command_being_processed is write-locked
//   - command_being_processed is reset
// * command_being_processed is unlocked
//
// If at this point addRequest is called again, then it is detected that the ThreadSafeBufferedCurlEasyRequest is active.

struct command_queue_st {
  std::deque<Command> commands;		// The commands
  size_t size;						// Number of add commands in the queue minus the number of remove commands.
};

// Multi-threaded queue for passing Command objects from the main-thread to the curl-thread.
AIThreadSafeSimpleDC<command_queue_st> command_queue;			// Fills 'size' with zero, because it's a global.
typedef AIAccess<command_queue_st> command_queue_wat;
typedef AIAccess<command_queue_st> command_queue_rat;

AIThreadSafeDC<Command> command_being_processed;
typedef AIWriteAccess<Command> command_being_processed_wat;
typedef AIReadAccess<Command> command_being_processed_rat;

namespace curlthread {
// All functions in this namespace are only run by the curl thread, unless they are marked with MAIN-THREAD.

//-----------------------------------------------------------------------------
// PollSet

int const empty = 0x1;
int const complete = 0x2;

enum refresh_t {
  not_complete_not_empty = 0,
  complete_not_empty = complete,
  empty_and_complete = complete|empty
};

// A class with info for each socket that is in use by curl.
class CurlSocketInfo
{
  public:
	CurlSocketInfo(MultiHandle& multi_handle, ASSERT_ONLY(CURL* easy,) curl_socket_t s, int action, ThreadSafeBufferedCurlEasyRequest* lockobj);
	~CurlSocketInfo();

	void set_action(int action);
	void mark_dead(void) { set_action(CURL_POLL_NONE); mDead = true; }
	curl_socket_t getSocketFd(void) const { return mSocketFd; }
	AICurlEasyRequest& getEasyRequest(void) { return mEasyRequest; }

  private:
	MultiHandle& mMultiHandle;
	curl_socket_t mSocketFd;
	int mAction;
	bool mDead;
	AICurlEasyRequest mEasyRequest;
	LLPointer<HTTPTimeout> mTimeout;
};

class PollSet
{
  public:
	PollSet(void);

	// Add/remove a filedescriptor to/from mFileDescriptors.
	void add(CurlSocketInfo* sp);
	void remove(CurlSocketInfo* sp);

	// Copy mFileDescriptors to an internal fd_set that is returned by access().
	// Returns if all fds could be copied (complete) and/or if the resulting fd_set is empty.
	refresh_t refresh(void);

	// Return a pointer to the underlaying fd_set.
	fd_set* access(void) { return &mFdSet; }

#if !WINDOWS_CODE
	// Return the largest fd set in mFdSet by refresh.
	curl_socket_t get_max_fd(void) const { return mMaxFdSet; }
#endif

	// Return a pointer to the corresponding CurlSocketInfo if a filedescriptor is set in mFileDescriptors, or NULL if s is not set.
	CurlSocketInfo* contains(curl_socket_t s) const;

	// Return true if a filedescriptor is set in mFdSet.
	bool is_set(curl_socket_t s) const;

	// Clear filedescriptor in mFdSet.
	void clr(curl_socket_t fd);

	// Iterate over all file descriptors that were set by refresh and are still set in mFdSet.
	void reset(void);				// Reset the iterator.
	curl_socket_t get(void) const;	// Return next filedescriptor, or CURL_SOCKET_BAD when there are no more.
	                   				// Only valid if reset() was called after the last call to refresh().
	void next(void);				// Advance to next filedescriptor.

  private:
	CurlSocketInfo** mFileDescriptors;
	int mNrFds;						// The number of filedescriptors in the array.
	int mNext;						// The index of the first file descriptor to start copying, the next call to refresh().

	fd_set mFdSet;					// Output variable for select(). (Re)initialized by calling refresh().

#if !WINDOWS_CODE
	curl_socket_t mMaxFd;			// The largest filedescriptor in the array, or CURL_SOCKET_BAD when it is empty.
	curl_socket_t mMaxFdSet;		// The largest filedescriptor set in mFdSet by refresh(), or CURL_SOCKET_BAD when it was empty.
	std::vector<curl_socket_t> mCopiedFileDescriptors;	// Filedescriptors copied by refresh to mFdSet.
	std::vector<curl_socket_t>::iterator mIter;			// Index into mCopiedFileDescriptors for next(); loop variable.
#else
	unsigned int mIter;				// Index into fd_set::fd_array.
#endif
};

// A PollSet can store at least 1024 filedescriptors, or FD_SETSIZE if that is larger than 1024 [MAXSIZE].
// The number of stored filedescriptors is mNrFds [0 <= mNrFds <= MAXSIZE].
// The largest filedescriptor is stored is mMaxFd, which is -1 iff mNrFds == 0.
// The file descriptors are stored contiguous in mFileDescriptors[i], with 0 <= i < mNrFds.
// File descriptors with the highest priority should be stored first (low index).
//
// mNext is an index into mFileDescriptors that is copied first, the next call to refresh().
// It is set to 0 when mNrFds < FD_SETSIZE, even if mNrFds == 0.
//
// After a call to refresh():
//
// mFdSet has bits set for at most FD_SETSIZE - 1 filedescriptors, copied from mFileDescriptors starting
// at index mNext (wrapping around to 0). If mNrFds < FD_SETSIZE then mNext is reset to 0 before copying starts.
// If mNrFds >= FD_SETSIZE then mNext is set to the next filedescriptor that was not copied (otherwise it is left at 0).
//
// mMaxFdSet is the largest filedescriptor in mFdSet or -1 if it is empty.

static size_t const MAXSIZE = llmax(1024, FD_SETSIZE);

// Create an empty PollSet.
PollSet::PollSet(void) : mFileDescriptors(new CurlSocketInfo* [MAXSIZE]),
                         mNrFds(0), mNext(0)
#if !WINDOWS_CODE
						 , mMaxFd(-1), mMaxFdSet(-1)
#endif
{
  FD_ZERO(&mFdSet);
}

// Add filedescriptor s to the PollSet.
void PollSet::add(CurlSocketInfo* sp)
{
  llassert_always(mNrFds < (int)MAXSIZE);
  mFileDescriptors[mNrFds++] = sp;
#if !WINDOWS_CODE
  mMaxFd = llmax(mMaxFd, sp->getSocketFd());
#endif
}

// Remove filedescriptor s from the PollSet.
void PollSet::remove(CurlSocketInfo* sp)
{
  // The number of open filedescriptors is relatively small,
  // and on top of that we rather do something CPU intensive
  // than bandwidth intensive (lookup table). Hence that this
  // is a linear search in an array containing just the open
  // filedescriptors. Then, since we are reading this memory
  // page anyway, we might as well write to it without losing
  // much clock cycles. Therefore, shift the whole vector
  // back, keeping it compact and keeping the filedescriptors
  // in the same order (which is supposedly their priority).
  //
  // The general case is where mFileDescriptors contains sp at an index
  // between 0 and mNrFds:
  //                              mNrFds = 6
  //                                v
  // index: 0   1   2   3   4   5
  //        a   b   c   s   d   e

  // This function should never be called unless sp is actually in mFileDescriptors,
  // as a result of a previous call to PollSet::add(sp).
  llassert(mNrFds > 0);

  // Correct mNrFds for when the descriptor is removed.
  // Make i 'point' to the last entry.
  int i = --mNrFds;
  //                       i = NrFds = 5
  //                            v
  // index: 0   1   2   3   4   5
  //        a   b   c   s   d   e
  curl_socket_t const s = sp->getSocketFd();
  CurlSocketInfo* cur = mFileDescriptors[i];		// cur = 'e'
#if !WINDOWS_CODE
  curl_socket_t max = -1;
#endif
  while (cur != sp)
  {
	llassert(i > 0);
	CurlSocketInfo* next = mFileDescriptors[--i];	// next = 'd'
	mFileDescriptors[i] = cur;					// Overwrite 'd' with 'e'.
#if !WINDOWS_CODE
	max = llmax(max, cur->getSocketFd());		// max is the maximum value in 'i' or higher.
#endif
	cur = next;									// cur = 'd'
	//                        i  NrFds = 5
	//                        v   v
	// index: 0   1   2   3   4
	//        a   b   c   s   e					// cur = 'd'
	//
	// Next loop iteration: next = 'sp', overwrite 'sp' with 'd', cur = 'sp'; loop terminates.
	//                    i      NrFds = 5
	//                    v       v
	// index: 0   1   2   3   4
	//        a   b   c   d   e					// cur = 'sp'
  }
  llassert(cur == sp);
  // At this point i was decremented once more and points to the element before the old s.
  //                i          NrFds = 5
  //                v           v
  // index: 0   1   2   3   4
  //        a   b   c   d   e					// max = llmax('d', 'e')

  // If mNext pointed to an element before sp, it should be left alone. Otherwise, if mNext pointed
  // to sp it must now point to 'd', or if it pointed beyond 'sp' it must be decremented by 1.
  if (mNext > i)								// i is where s was.
	--mNext;

#if !WINDOWS_CODE
  // If s was the largest file descriptor, we have to update mMaxFd.
  if (s == mMaxFd)
  {
	while (i > 0)
	{
	  CurlSocketInfo* next = mFileDescriptors[--i];
	  max = llmax(max, next->getSocketFd());
	}
	mMaxFd = max;
	llassert(mMaxFd < s);
	llassert((mMaxFd == -1) == (mNrFds == 0));
  }
#endif

  // ALSO make sure that s is no longer set in mFdSet, or we might confuse libcurl by
  // calling curl_multi_socket_action for a socket that it told us to remove.
#if !WINDOWS_CODE
  clr(s);
#else
  // We have to use a custom implementation here, because we don't want to invalidate mIter.
  // This is the same algorithm as above, but with mFdSet.fd_count instead of mNrFds,
  // mFdSet.fd_array instead of mFileDescriptors and mIter instead of mNext.
  if (FD_ISSET(s, &mFdSet))
  {
	llassert(mFdSet.fd_count > 0);
	unsigned int i = --mFdSet.fd_count;
	curl_socket_t cur = mFdSet.fd_array[i];
	while (cur != s)
	{
	  llassert(i > 0);
	  curl_socket_t next = mFdSet.fd_array[--i];
	  mFdSet.fd_array[i] = cur;
	  cur = next;
	}
	if (mIter > i)
	  --mIter;
	llassert(mIter <= mFdSet.fd_count);
  }
#endif
}

CurlSocketInfo* PollSet::contains(curl_socket_t fd) const
{
  for (int i = 0; i < mNrFds; ++i)
	if (mFileDescriptors[i]->getSocketFd() == fd)
	  return mFileDescriptors[i];
  return NULL;
}

inline bool PollSet::is_set(curl_socket_t fd) const
{
  return FD_ISSET(fd, &mFdSet);
}

inline void PollSet::clr(curl_socket_t fd)
{
  FD_CLR(fd, &mFdSet);
}

// This function fills mFdSet with at most FD_SETSIZE - 1 filedescriptors,
// starting at index mNext (updating mNext when not all could be added),
// and updates mMaxFdSet to be the largest fd added to mFdSet, or -1 if it's empty.
refresh_t PollSet::refresh(void)
{
  FD_ZERO(&mFdSet);
#if !WINDOWS_CODE
  mCopiedFileDescriptors.clear();
#endif

  if (mNrFds == 0)
  {
#if !WINDOWS_CODE
	mMaxFdSet = -1;
#endif
	return empty_and_complete;
  }

  llassert_always(mNext < mNrFds);

  // Test if mNrFds is larger than or equal to FD_SETSIZE; equal, because we reserve one
  // filedescriptor for the wakeup fd: we copy maximal FD_SETSIZE - 1 filedescriptors.
  // If not then we're going to copy everything so that we can save on CPU cycles
  // by not calculating mMaxFdSet here.
  if (mNrFds >= FD_SETSIZE)
  {
	llwarns << "PollSet::reset: More than FD_SETSIZE (" << FD_SETSIZE << ") file descriptors active!" << llendl;
#if !WINDOWS_CODE
	// Calculate mMaxFdSet.
	// Run over FD_SETSIZE - 1 elements, starting at mNext, wrapping to 0 when we reach the end.
	int max = -1, i = mNext, count = 0;
	while (++count < FD_SETSIZE) { max = llmax(max, mFileDescriptors[i]->getSocketFd()); if (++i == mNrFds) i = 0; }
	mMaxFdSet = max;
#endif
  }
  else
  {
	mNext = 0;				// Start at the beginning if we copy everything anyway.
#if !WINDOWS_CODE
	mMaxFdSet = mMaxFd;
#endif
  }
  int count = 0;
  int i = mNext;
  for(;;)
  {
	if (++count == FD_SETSIZE)
	{
	  mNext = i;
	  return not_complete_not_empty;
	}
	FD_SET(mFileDescriptors[i]->getSocketFd(), &mFdSet);
#if !WINDOWS_CODE
	mCopiedFileDescriptors.push_back(mFileDescriptors[i]->getSocketFd());
#endif
	if (++i == mNrFds)
	{
	  // If we reached the end and start at the beginning, then we copied everything.
	  if (mNext == 0)
		break;
	  // We can only come here if mNrFds >= FD_SETSIZE, hence we can just
	  // wrap around and terminate on count reaching FD_SETSIZE.
	  i = 0;
	}
  }
  return complete_not_empty;
}

// The API reset(), get() and next() allows one to run over all filedescriptors
// in mFdSet that are set. This works by running only over the filedescriptors
// that were set initially (by the call to refresh()) and then checking if that
// filedescriptor is (still) set in mFdSet.
//
// A call to reset() resets mIter to the beginning, so that get() returns
// the first filedescriptor that is still set. A call to next() progresses
// the iterator to the next set filedescriptor. If get() return -1, then there
// were no more filedescriptors set.
//
// Note that one should never call next() unless get() didn't return -1, so
// the call sequence is:
// refresh();
// /* reset some or all bits in mFdSet */
// reset();
// while (get() != CURL_SOCKET_BAD) // next();
//
// Note also that this API is only used by MergeIterator, which wraps it
// and provides a different API to use.

void PollSet::reset(void)
{
#if WINDOWS_CODE
  mIter = 0;
#else
  if (mCopiedFileDescriptors.empty())
	mIter = mCopiedFileDescriptors.end();
  else
  {
	mIter = mCopiedFileDescriptors.begin();
	if (!FD_ISSET(*mIter, &mFdSet))
	  next();
  }
#endif
}

inline curl_socket_t PollSet::get(void) const
{
#if WINDOWS_CODE
  return (mIter >= mFdSet.fd_count) ? CURL_SOCKET_BAD : mFdSet.fd_array[mIter];
#else
  return (mIter == mCopiedFileDescriptors.end()) ? CURL_SOCKET_BAD : *mIter;
#endif
}

void PollSet::next(void)
{
#if WINDOWS_CODE
  llassert(mIter < mFdSet.fd_count);
  ++mIter;
#else
  llassert(mIter != mCopiedFileDescriptors.end());	// Only call next() if the last call to get() didn't return -1.
  while (++mIter != mCopiedFileDescriptors.end() && !FD_ISSET(*mIter, &mFdSet));
#endif
}

//-----------------------------------------------------------------------------
// MergeIterator
//
// This class takes two PollSet's and allows one to run over all filedescriptors
// that are set in one or both poll sets, returning each filedescriptor only
// once, by calling next() until it returns false.

class MergeIterator
{
  public:
	MergeIterator(PollSet* readPollSet, PollSet* writePollSet);

	bool next(curl_socket_t& fd_out, int& ev_bitmask_out);

  private:
	PollSet* mReadPollSet;
	PollSet* mWritePollSet;
};

MergeIterator::MergeIterator(PollSet* readPollSet, PollSet* writePollSet) :
    mReadPollSet(readPollSet), mWritePollSet(writePollSet)
{
  mReadPollSet->reset();
  mWritePollSet->reset();
}

bool MergeIterator::next(curl_socket_t& fd_out, int& ev_bitmask_out)
{
  curl_socket_t rfd = mReadPollSet->get();
  curl_socket_t wfd = mWritePollSet->get();

  if (rfd == CURL_SOCKET_BAD && wfd == CURL_SOCKET_BAD)
	return false;

  if (rfd == wfd)
  {
	fd_out = rfd;
	ev_bitmask_out = CURL_CSELECT_IN | CURL_CSELECT_OUT;
	mReadPollSet->next();
  }
  else if (wfd == CURL_SOCKET_BAD || (rfd != CURL_SOCKET_BAD && rfd < wfd))	// Use and increment smaller one, unless it's CURL_SOCKET_BAD.
  {
	fd_out = rfd;
	ev_bitmask_out = CURL_CSELECT_IN;
	mReadPollSet->next();
	if (wfd != CURL_SOCKET_BAD && mWritePollSet->is_set(rfd))
	{
	  ev_bitmask_out |= CURL_CSELECT_OUT;
	  mWritePollSet->clr(rfd);
	}
  }
  else
  {
	fd_out = wfd;
	ev_bitmask_out = CURL_CSELECT_OUT;
	mWritePollSet->next();
	if (rfd != CURL_SOCKET_BAD && mReadPollSet->is_set(wfd))
	{
	  ev_bitmask_out |= CURL_CSELECT_IN;
	  mReadPollSet->clr(wfd);
	}
  }

  return true;
}

//-----------------------------------------------------------------------------
// CurlSocketInfo

#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
#undef AI_CASE_RETURN
#define AI_CASE_RETURN(x) case x: return #x;
static char const* action_str(int action)
{
  switch(action)
  {
	AI_CASE_RETURN(CURL_POLL_NONE);
	AI_CASE_RETURN(CURL_POLL_IN);
	AI_CASE_RETURN(CURL_POLL_OUT);
	AI_CASE_RETURN(CURL_POLL_INOUT);
	AI_CASE_RETURN(CURL_POLL_REMOVE);
  }
  return "<unknown action>";
}

struct DebugFdSet {
  int nfds;
  fd_set* fdset;
  DebugFdSet(int n, fd_set* p) : nfds(n), fdset(p) { }
};

std::ostream& operator<<(std::ostream& os, DebugFdSet const& s)
{
  if (!s.fdset)
	return os << "NULL";
  bool first = true;
  os << '{';
  for (int fd = 0; fd < s.nfds; ++fd)
  {
	if (FD_ISSET(fd, s.fdset))
	{
	  if (!first)
		os << ", ";
	  os << fd;
	  first = false;
	}
  }
  os << '}';
  return os;
}
#endif

CurlSocketInfo::CurlSocketInfo(MultiHandle& multi_handle, ASSERT_ONLY(CURL* easy,) curl_socket_t s, int action, ThreadSafeBufferedCurlEasyRequest* lockobj) :
    mMultiHandle(multi_handle), mSocketFd(s), mAction(CURL_POLL_NONE), mDead(false), mEasyRequest(lockobj)
{
  llassert(*AICurlEasyRequest_wat(*mEasyRequest) == easy);
  mMultiHandle.assign(s, this);
  llassert(!mMultiHandle.mReadPollSet->contains(s));
  llassert(!mMultiHandle.mWritePollSet->contains(s));
  set_action(action);
  // Create a new HTTPTimeout object and keep a pointer to it in the corresponding CurlEasyRequest object.
  // The reason for this seemingly redundant storage (we could just store it directly in the CurlEasyRequest
  // and not in CurlSocketInfo) is because in the case of a redirection there exist temporarily two
  // CurlSocketInfo objects for a request and we need upload_finished() to be called on the HTTPTimeout
  // object related to THIS CurlSocketInfo.
  AICurlEasyRequest_wat easy_request_w(*lockobj);
  mTimeout = easy_request_w->get_timeout_object();
}

CurlSocketInfo::~CurlSocketInfo()
{
  set_action(CURL_POLL_NONE);
}

void CurlSocketInfo::set_action(int action)
{
  if (mDead)
  {
	return;
  }

  Dout(dc::curl, "CurlSocketInfo::set_action(" << action_str(mAction) << " --> " << action_str(action) << ") [" << (void*)mEasyRequest.get_ptr().get() << "]");
  int toggle_action = mAction ^ action; 
  mAction = action;
  if ((toggle_action & CURL_POLL_IN))
  {
	if ((action & CURL_POLL_IN))
	  mMultiHandle.mReadPollSet->add(this);
	else
	  mMultiHandle.mReadPollSet->remove(this);
  }
  if ((toggle_action & CURL_POLL_OUT))
  {
	if ((action & CURL_POLL_OUT))
	{
	  mMultiHandle.mWritePollSet->add(this);
	  if (mTimeout)
	  {
		  // Note that this detection normally doesn't work because mTimeout will be zero.
		  // However, it works in the case of a redirect - and then we need it.
		  mTimeout->upload_starting();			// Update timeout administration.
	  }
	}
	else
	{
	  mMultiHandle.mWritePollSet->remove(this);

	  // The following is a bit of a hack, needed because of the lack of proper timeout callbacks in libcurl.
	  // The removal of CURL_POLL_OUT could be part of the SSL handshake, therefore check if we're already connected:
	  AICurlEasyRequest_wat curl_easy_request_w(*mEasyRequest);
	  double pretransfer_time;
	  curl_easy_request_w->getinfo(CURLINFO_PRETRANSFER_TIME, &pretransfer_time);
	  if (pretransfer_time > 0)
	  {
		// If CURL_POLL_OUT is removed and CURLINFO_PRETRANSFER_TIME is already set, then we have nothing more to send apparently.
		mTimeout->upload_finished();		// Update timeout administration.
	  }
	}
  }
}

//-----------------------------------------------------------------------------
// AICurlThread

class AICurlThread : public LLThread
{
  public:
	static AICurlThread* sInstance;
	LLMutex mWakeUpMutex;		// Set while a thread is waking up the curl thread.
	LLMutex mWakeUpFlagMutex;	// Set when the curl thread is sleeping (in or about to enter select()).
	bool mWakeUpFlag;			// Protected by mWakeUpFlagMutex.

  public:
	// MAIN-THREAD
	AICurlThread(void);
	virtual ~AICurlThread();

	// MAIN-THREAD
	void wakeup_thread(bool stop_thread = false);

	// MAIN-THREAD
	apr_status_t join_thread(void);

  protected:
	virtual void run(void);
	void wakeup(AICurlMultiHandle_wat const& multi_handle_w);
	void process_commands(AICurlMultiHandle_wat const& multi_handle_w);

  private:
	// MAIN-THREAD
	void create_wakeup_fds(void);
	void cleanup_wakeup_fds(void);

	curl_socket_t mWakeUpFd_in;
	curl_socket_t mWakeUpFd;

	int mZeroTimeout;

	volatile bool mRunning;
};

// Only the main thread is accessing this.
AICurlThread* AICurlThread::sInstance = NULL;

// MAIN-THREAD
AICurlThread::AICurlThread(void) : LLThread("AICurlThread"),
    mWakeUpFd_in(CURL_SOCKET_BAD),
	mWakeUpFd(CURL_SOCKET_BAD),
	mZeroTimeout(0), mWakeUpFlag(false), mRunning(true)
{
  create_wakeup_fds();
  sInstance = this;
}

// MAIN-THREAD
AICurlThread::~AICurlThread()
{
  sInstance = NULL;
  cleanup_wakeup_fds();
}

#if LL_WINDOWS
static std::string formatWSAError(int e = WSAGetLastError())
{
	std::ostringstream r;
	LPTSTR error_str = 0;
	r << e;
	if(FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, e, 0, (LPTSTR)&error_str, 0, NULL))
	{
		r << " " << utf16str_to_utf8str(error_str);
		LocalFree(error_str);
	}
	else
	{
		r << " Unknown WinSock error";
	}
	return r.str();
}
#elif WINDOWS_CODE
static std::string formatWSAError(int e = errno)
{
	return strerror(e);
}
#endif

#if LL_WINDOWS
 /* Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>
 * This code is Free Software.  It may be copied freely, in original or 
 * modified form, subject only to the restrictions that (1) the author is
 * relieved from all responsibilities for any use for any purpose, and (2)
 * this copyright notice must be retained, unchanged, in its entirety.  If
 * for any reason the author might be held responsible for any consequences
 * of copying or use, license is withheld.  
 */
static int dumb_socketpair(SOCKET socks[2], bool make_overlapped)
{
    union {
       struct sockaddr_in inaddr;
       struct sockaddr addr;
    } a;
    SOCKET listener;
    int e;
    socklen_t addrlen = sizeof(a.inaddr);
    DWORD flags = (make_overlapped ? WSA_FLAG_OVERLAPPED : 0);
    int reuse = 1;

    if (socks == 0) {
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) 
        return SOCKET_ERROR;

    memset(&a, 0, sizeof(a));
    a.inaddr.sin_family = AF_INET;
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_port = 0; 

    socks[0] = socks[1] = INVALID_SOCKET;
    do {
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, 
               (char*) &reuse, (socklen_t) sizeof(reuse)) == -1)
            break;
        if  (bind(listener, &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        memset(&a, 0, sizeof(a));
        if  (getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR)
            break;
        // win32 getsockname may only set the port number, p=0.0005.
        // ( http://msdn.microsoft.com/library/ms738543.aspx ):
        a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.inaddr.sin_family = AF_INET;

        if (listen(listener, 1) == SOCKET_ERROR)
            break;

        socks[0] = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
        if (socks[0] == INVALID_SOCKET)
            break;
        if (connect(socks[0], &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        socks[1] = accept(listener, NULL, NULL);
        if (socks[1] == INVALID_SOCKET)
            break;

        closesocket(listener);
        return 0;

    } while (0);

    e = WSAGetLastError();
    closesocket(listener);
    closesocket(socks[0]);
    closesocket(socks[1]);
    WSASetLastError(e);
    return SOCKET_ERROR;
}
#elif WINDOWS_CODE
int dumb_socketpair(int socks[2], int dummy)
{
    (void) dummy;
    return socketpair(AF_LOCAL, SOCK_STREAM, 0, socks);
}
#endif

// MAIN-THREAD
void AICurlThread::create_wakeup_fds(void)
{
#if WINDOWS_CODE
	//SGTODO
	curl_socket_t socks[2];
	if (dumb_socketpair(socks, false) == SOCKET_ERROR)
	{
		llerrs << "Failed to generate wake-up socket pair" << formatWSAError() << llendl;
		return;
	}
	u_long nonblocking_enable = TRUE;
	int error = ioctlsocket(socks[0], FIONBIO, &nonblocking_enable);
	if(error)
	{
		llerrs << "Failed to set wake-up socket nonblocking: " << formatWSAError() << llendl;
	}
	llassert(nonblocking_enable);
	error = ioctlsocket(socks[1], FIONBIO, &nonblocking_enable);
	if(error)
	{
		llerrs << "Failed to set wake-up input socket nonblocking: " << formatWSAError() << llendl;
	}
	mWakeUpFd = socks[0];
	mWakeUpFd_in = socks[1];
#else
  int pipefd[2];
  if (pipe(pipefd))
  {
	llerrs << "Failed to create wakeup pipe: " << strerror(errno) << llendl;
  }
  int const flags = O_NONBLOCK;
  for (int i = 0; i < 2; ++i)
  {
	if (fcntl(pipefd[i], F_SETFL, flags))
	{
	  llerrs << "Failed to set pipe to non-blocking: " << strerror(errno) << llendl;
	}
  }
  mWakeUpFd = pipefd[0];		// Read-end of the pipe.
  mWakeUpFd_in = pipefd[1];		// Write-end of the pipe.
#endif
}

// MAIN-THREAD
void AICurlThread::cleanup_wakeup_fds(void)
{
#if WINDOWS_CODE
	//SGTODO
	if (mWakeUpFd != CURL_SOCKET_BAD)
	{
		int error = closesocket(mWakeUpFd);
		if (error)
		{
			llwarns << "Error closing wake-up socket" << formatWSAError() << llendl;
		}
	}
	if (mWakeUpFd_in != CURL_SOCKET_BAD)
	{
		int error = closesocket(mWakeUpFd_in);
		if (error)
		{
			llwarns << "Error closing wake-up input socket" << formatWSAError() << llendl;
		}
	}
#else
  if (mWakeUpFd_in != CURL_SOCKET_BAD)
	close(mWakeUpFd_in);
  if (mWakeUpFd != CURL_SOCKET_BAD)
	close(mWakeUpFd);
#endif
}

// OTHER THREADS
void AICurlThread::wakeup_thread(bool stop_thread)
{
  DoutEntering(dc::curl, "AICurlThread::wakeup_thread");

  // If we are already exiting the viewer then return immediately.
  if (!mRunning)
	return;

  // Last time we are run?
  if (stop_thread)
	mRunning = false;			// Thread-safe because all other threads were already stopped.

  // Note, we do not want this function to be blocking the calling thread; therefore we only use tryLock()s.

  // Stop two threads running the following code concurrently.
  if (!mWakeUpMutex.tryLock())
  {
	// If we failed to obtain mWakeUpMutex then another thread is (or was) in AICurlThread::wakeup_thread,
	// or curl was holding the lock for a micro second at the start of process_commands.
	// In the first case, curl might or might not yet have been woken up because of that, but if it was
	// then it could not have started processing the commands yet, because it needs to obtain mWakeUpMutex
	// between being woken up and processing the commands.
	// Either way, the command that this thread called this function for was already in the queue (it's
	// added before this function is called) but the command(s) that another thread called this function
	// for were not processed yet. Hence, it's safe to exit here as our command(s) will be processed too.
	return;
  }

  // Try if curl thread is still awake and if so, pass the new commands directly.
  if (mWakeUpFlagMutex.tryLock())
  {
	mWakeUpFlag = true;
	mWakeUpFlagMutex.unlock();
	mWakeUpMutex.unlock();
	return;
  }

#if WINDOWS_CODE
  //SGTODO
  int len = send(mWakeUpFd_in, "!", 1, 0);
  if (len == SOCKET_ERROR)
  {
	  llerrs << "Send to wake-up socket failed: " << formatWSAError() << llendl;
  }
  llassert_always(len == 1);
  //SGTODO: handle EAGAIN if needed
#else
  // If write() is interrupted by a signal before it writes any data, it shall return -1 with errno set to [EINTR].
  // If write() is interrupted by a signal after it successfully writes some data, it shall return the number of bytes written.
  // Write requests to a pipe or FIFO shall be handled in the same way as a regular file with the following exceptions:
  // If the O_NONBLOCK flag is set, write() requests shall be handled differently, in the following ways:
  //   A write request for {PIPE_BUF} or fewer bytes shall have the following effect:
  //     if there is sufficient space available in the pipe, write() shall transfer all the data and return the number
  //     of bytes requested. Otherwise, write() shall transfer no data and return -1 with errno set to [EAGAIN].
  ssize_t len;
  do
  {
    len = write(mWakeUpFd_in, "!", 1);
    if (len == -1 && errno == EAGAIN)
	{
	  mWakeUpMutex.unlock();
	  return;		// Unread characters are still in the pipe, so no need to add more.
	}
  }
  while(len == -1 && errno == EINTR);
  if (len == -1)
  {
	llerrs << "write(3) to mWakeUpFd_in: " << strerror(errno) << llendl;
  }
  llassert_always(len == 1);
#endif

  // Release the lock here and not sooner, for the sole purpose of making sure
  // that not two threads execute the above code concurrently. If the above code
  // is thread-safe (maybe it is?) then we could release this lock arbitrarily
  // sooner indeed - or even not lock it at all.
  mWakeUpMutex.unlock();
}

apr_status_t AICurlThread::join_thread(void)
{
	apr_status_t retval = APR_SUCCESS;
	if (sInstance)
	{
		apr_thread_join(&retval, sInstance->mAPRThreadp);
		delete sInstance;
	}
	return retval;
}

void AICurlThread::wakeup(AICurlMultiHandle_wat const& multi_handle_w)
{
  DoutEntering(dc::curl, "AICurlThread::wakeup");

#if WINDOWS_CODE
  //SGTODO
  char buf[256];
  bool got_data = false;
  for(;;)
  {
	int len = recv(mWakeUpFd, buf, sizeof(buf), 0);
	if (len > 0)
	{
	  // Data was read from the pipe.
	  got_data = true;
	  if (len < sizeof(buf))
		break;
	}
	else if (len == SOCKET_ERROR)
	{
	  // An error occurred.
	  if (errno == EWOULDBLOCK)
	  {
		if (got_data)
		  break;
		// There was no data, even though select() said so. If this ever happens at all(?), lets just return and enter select() again.
		return;
	  }
	  else if (errno == EINTR)
	  {
		continue;
	  }
	  else
	  {
		llerrs << "read(3) from mWakeUpFd: " << formatWSAError() << llendl;
		return;
	  }
	}
	else
	{
	  // pipe(2) returned 0.
	  llwarns << "read(3) from mWakeUpFd returned 0, indicating that the pipe on the other end was closed! Shutting down curl thread." << llendl;
	  closesocket(mWakeUpFd);
	  mWakeUpFd = CURL_SOCKET_BAD;
	  mRunning = false;
	  return;
	}
  }
#else
  // If a read() is interrupted by a signal before it reads any data, it shall return -1 with errno set to [EINTR].
  // If a read() is interrupted by a signal after it has successfully read some data, it shall return the number of bytes read.
  // When attempting to read from an empty pipe or FIFO:
  // If no process has the pipe open for writing, read() shall return 0 to indicate end-of-file.
  // If some process has the pipe open for writing and O_NONBLOCK is set, read() shall return -1 and set errno to [EAGAIN].
  char buf[256];
  bool got_data = false;
  for(;;)
  {
	ssize_t len = read(mWakeUpFd, buf, sizeof(buf));
	if (len > 0)
	{
	  // Data was read from the pipe.
	  got_data = true;
	  if (len < sizeof(buf))
		break;
	}
	else if (len == -1)
	{
	  // An error occurred.
	  if (errno == EAGAIN)
	  {
		if (got_data)
		  break;
		// There was no data, even though select() said so. If this ever happens at all(?), lets just return and enter select() again.
		return;
	  }
	  else if (errno == EINTR)
	  {
		continue;
	  }
	  else
	  {
		llerrs << "read(3) from mWakeUpFd: " << strerror(errno) << llendl;
		return;
	  }
	}
	else
	{
	  // pipe(2) returned 0.
	  llwarns << "read(3) from mWakeUpFd returned 0, indicating that the pipe on the other end was closed! Shutting down curl thread." << llendl;
	  close(mWakeUpFd);
	  mWakeUpFd = CURL_SOCKET_BAD;
	  mRunning = false;
	  return;
	}
  }
#endif
  // Data was received on mWakeUpFd. This means that the main-thread added one
  // or more commands to the command queue and called wakeUpCurlThread().
  process_commands(multi_handle_w);
}

void AICurlThread::process_commands(AICurlMultiHandle_wat const& multi_handle_w)
{
  DoutEntering(dc::curl, "AICurlThread::process_commands(void)");

  // Block here until the thread that woke us up released mWakeUpMutex.
  // This is necessary to make sure that a third thread added commands
  // too then either it will signal us later, or we process those commands
  // now, too.
  mWakeUpMutex.lock();
  // Note that if at THIS point another thread tries to obtain mWakeUpMutex in AICurlThread::wakeup_thread
  // and fails, it is ok that it leaves that function without waking us up too: we're awake and
  // about to process any commands!
  mWakeUpMutex.unlock();

  // If we get here then the main thread called wakeup_thread() recently.
  for(;;)
  {
	// Access command_queue, and move command to command_being_processed.
	{
	  command_queue_wat command_queue_w(command_queue);
	  if (command_queue_w->commands.empty())
	  {
		mWakeUpFlagMutex.lock();
		mWakeUpFlag = false;
		mWakeUpFlagMutex.unlock();
		break;
	  }
	  // Move the next command from the queue into command_being_processed.
	  command_st command;
	  {
		command_being_processed_wat command_being_processed_w(command_being_processed);
		*command_being_processed_w = command_queue_w->commands.front();
		command = command_being_processed_w->command();
	  }
	  // Update the size: the number netto number of pending requests in the command queue.
	  command_queue_w->commands.pop_front();
	  if (command == cmd_add)
	  {
		command_queue_w->size--;
	  }
	  else if (command == cmd_remove)
	  {
		command_queue_w->size++;
	  }
	}
	// Access command_being_processed only.
	{
	  command_being_processed_rat command_being_processed_r(command_being_processed);
	  AICapabilityType capability_type;
	  AIPerServicePtr per_service;
	  {
		AICurlEasyRequest_wat easy_request_w(*command_being_processed_r->easy_request());
		capability_type = easy_request_w->capability_type();
		per_service = easy_request_w->getPerServicePtr();
	  }
	  switch(command_being_processed_r->command())
	  {
		case cmd_none:
		case cmd_boost:	// FIXME: future stuff
		  break;
		case cmd_add:
		{
		  multi_handle_w->add_easy_request(AICurlEasyRequest(command_being_processed_r->easy_request()), false);
		  PerService_wat(*per_service)->removed_from_command_queue(capability_type);
		  break;
		}
		case cmd_remove:
		{
		  PerService_wat(*per_service)->added_to_command_queue(capability_type);		// Not really, but this has the same effect as 'removed a remove command'.
		  multi_handle_w->remove_easy_request(AICurlEasyRequest(command_being_processed_r->easy_request()), true);
		  break;
		}
	  }
	  // Done processing.
	  command_being_processed_wat command_being_processed_w(command_being_processed_r);
	  command_being_processed_w->reset();		// This destroys the CurlEasyRequest in case of a cmd_remove.
	}
  }
}

// Return true if fd is a 'bad' socket.
static bool is_bad(curl_socket_t fd, bool for_writing)
{
  fd_set tmp;
  FD_ZERO(&tmp);
  FD_SET(fd, &tmp);
  fd_set* readfds = for_writing ? NULL : &tmp;
  fd_set* writefds = for_writing ? &tmp : NULL;
#if !WINDOWS_CODE
  int nfds = fd + 1;
#else
  int nfds = 64;
#endif
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 10;
  int ret = select(nfds, readfds, writefds, NULL, &timeout);
  return ret == -1;
}

// The main loop of the curl thread.
void AICurlThread::run(void)
{
  DoutEntering(dc::curl, "AICurlThread::run()");

  {
	AICurlMultiHandle_wat multi_handle_w(AICurlMultiHandle::getInstance());
	while(mRunning)
	{
	  // If mRunning is true then we can only get here if mWakeUpFd != CURL_SOCKET_BAD.
	  llassert(mWakeUpFd != CURL_SOCKET_BAD);
	  // Process every command in command_queue before filling the fd_set passed to select().
	  for(;;)
	  {
		mWakeUpFlagMutex.lock();
		if (mWakeUpFlag)
		{
		  mWakeUpFlagMutex.unlock();
		  process_commands(multi_handle_w);
		  continue;
		}
		break;
	  }
	  // wakeup_thread() is also called after setting mRunning to false.
	  if (!mRunning)
	  {
		mWakeUpFlagMutex.unlock();
		break;
	  }

	  // If we get here then mWakeUpFlag has been false since we grabbed the lock.
	  // We're now entering select(), during which the main thread will write to the pipe/socket
	  // to wake us up, because it can't get the lock.

	  // Copy the next batch of file descriptors from the PollSets mFileDescriptors into their mFdSet.
	  multi_handle_w->mReadPollSet->refresh();
	  refresh_t wres = multi_handle_w->mWritePollSet->refresh();
	  // Add wake up fd if any, and pass NULL to select() if a set is empty.
	  fd_set* read_fd_set = multi_handle_w->mReadPollSet->access();
	  FD_SET(mWakeUpFd, read_fd_set);
	  fd_set* write_fd_set = ((wres & empty)) ? NULL : multi_handle_w->mWritePollSet->access();
	  // Calculate nfds (ignored on windows).
#if !WINDOWS_CODE
	  curl_socket_t const max_rfd = llmax(multi_handle_w->mReadPollSet->get_max_fd(), mWakeUpFd);
	  curl_socket_t const max_wfd = multi_handle_w->mWritePollSet->get_max_fd();
	  int nfds = llmax(max_rfd, max_wfd) + 1;
	  llassert(1 <= nfds && nfds <= FD_SETSIZE);
	  llassert((max_rfd == -1) == (read_fd_set == NULL) &&
			   (max_wfd == -1) == (write_fd_set == NULL));	// Needed on Windows.
	  llassert((max_rfd == -1 || multi_handle_w->mReadPollSet->is_set(max_rfd)) &&
			   (max_wfd == -1 || multi_handle_w->mWritePollSet->is_set(max_wfd)));
#else
	  int nfds = 64;
#endif
	  int ready = 0;
	  struct timeval timeout;
	  // Update AICurlTimer::sTime_1ms.
	  AICurlTimer::sTime_1ms = get_clock_count() * AICurlTimer::sClockWidth_1ms;
	  Dout(dc::curl, "AICurlTimer::sTime_1ms = " << AICurlTimer::sTime_1ms);
	  // Get the time in ms that libcurl wants us to wait for socket actions - at most - before proceeding.
	  long timeout_ms = multi_handle_w->getTimeout();
	  // Set libcurl_timeout iff the shortest timeout is that of libcurl.
	  bool libcurl_timeout = timeout_ms == 0 || (timeout_ms > 0 && !AICurlTimer::expiresBefore(timeout_ms));
	  // If no curl timeout is set, sleep at most 4 seconds.
	  if (LL_UNLIKELY(timeout_ms < 0))
		timeout_ms = 4000;
	  // Check if some AICurlTimer expires first.
	  if (AICurlTimer::expiresBefore(timeout_ms))
	  {
		timeout_ms = AICurlTimer::nextExpiration();
	  }
	  // If we have to continue immediately, then just set a zero timeout, but only for 100 calls on a row;
	  // after that start sleeping 1ms and later even 10ms (this should never happen).
	  if (LL_UNLIKELY(timeout_ms <= 0))
	  {
		if (mZeroTimeout >= 1000)
		{
		  if (mZeroTimeout % 10000 == 0)
			llwarns << "Detected " << mZeroTimeout << " zero-timeout calls of select() by curl thread (more than 101 seconds)!" << llendl;
		  timeout_ms = 10;
		}
		else if (mZeroTimeout >= 100)
		  timeout_ms = 1;
		else
		  timeout_ms = 0;
	  }
	  else
	  {
		if (LL_UNLIKELY(mZeroTimeout >= 10000))
		  llinfos << "Timeout of select() call by curl thread reset (to " << timeout_ms << " ms)." << llendl;
		mZeroTimeout = 0;
	  }
	  timeout.tv_sec = timeout_ms / 1000;
	  timeout.tv_usec = (timeout_ms % 1000) * 1000;
#ifdef CWDEBUG
#ifdef DEBUG_CURLIO
	  Dout(dc::curl|flush_cf|continued_cf, "select(" << nfds << ", " << DebugFdSet(nfds, read_fd_set) << ", " << DebugFdSet(nfds, write_fd_set) << ", NULL, timeout = " << timeout_ms << " ms) = ");
#else
	  static int last_nfds = -1;
	  static long last_timeout_ms = -1;
	  static int same_count = 0;
	  bool same = (nfds == last_nfds && timeout_ms == last_timeout_ms);
	  if (!same)
	  {
		if (same_count > 1)
		  Dout(dc::curl, "Last select() call repeated " << same_count << " times.");
		Dout(dc::curl|flush_cf|continued_cf, "select(" << nfds << ", ..., timeout = " << timeout_ms << " ms) = ");
		same_count = 1;
	  }
	  else
	  {
		++same_count;
	  }
#endif
#endif
	  ready = select(nfds, read_fd_set, write_fd_set, NULL, &timeout);
	  mWakeUpFlagMutex.unlock();
#ifdef CWDEBUG
#ifdef DEBUG_CURLIO
	  Dout(dc::finish|cond_error_cf(ready == -1), ready);
#else
	  static int last_ready = -2;
	  static int last_errno = 0;
	  if (!same)
		Dout(dc::finish|cond_error_cf(ready == -1), ready);
	  else if (ready != last_ready || (ready == -1 && errno != last_errno))
	  {
		if (same_count > 1)
		  Dout(dc::curl, "Last select() call repeated " << same_count << " times.");
		Dout(dc::curl|cond_error_cf(ready == -1), "select(" << last_nfds << ", ..., timeout = " << last_timeout_ms << " ms) = " << ready);
		same_count = 1;
	  }
	  last_nfds = nfds;
	  last_timeout_ms = timeout_ms;
	  last_ready = ready;
	  if (ready == -1)
		last_errno = errno;
#endif
#endif
	  // Select returns the total number of bits set in each of the fd_set's (upon return),
	  // or -1 when an error occurred. A value of 0 means that a timeout occurred.
	  if (ready == -1)
	  {
		llwarns << "select() failed: " << errno << ", " << strerror(errno) << llendl;
		if (errno == EBADF)
		{
		  // Somewhere (fmodex?) one of our file descriptors was closed. Try to recover by finding out which.
		  llassert_always(!is_bad(mWakeUpFd, false));		// We can't recover from this.
		  PollSet* found = NULL;
		  // Run over all read file descriptors.
		  multi_handle_w->mReadPollSet->refresh();
		  multi_handle_w->mReadPollSet->reset();
		  curl_socket_t fd;
		  while ((fd = multi_handle_w->mReadPollSet->get()) != CURL_SOCKET_BAD)
		  {
			if (is_bad(fd, false))
			{
			  found = multi_handle_w->mReadPollSet;
			  break;
			}
			multi_handle_w->mReadPollSet->next();
		  }
		  if (!found)
		  {
			// Try all write file descriptors.
			refresh_t wres = multi_handle_w->mWritePollSet->refresh();
			if (!(wres & empty))
			{
			  multi_handle_w->mWritePollSet->reset();
			  while ((fd = multi_handle_w->mWritePollSet->get()) != CURL_SOCKET_BAD)
			  {
				if (is_bad(fd, true))
				{
				  found = multi_handle_w->mWritePollSet;
				  break;
				}
				multi_handle_w->mWritePollSet->next();
			  }
			}
		  }
		  llassert_always(found);	// It makes no sense to continue if we can't recover.
		  // Find the corresponding CurlSocketInfo
		  CurlSocketInfo* sp = found->contains(fd);
		  llassert_always(sp);		// fd was just *read* from this sp.
		  sp->mark_dead();													// Make sure it's never used again.
		  AICurlEasyRequest_wat curl_easy_request_w(*sp->getEasyRequest());
		  curl_easy_request_w->pause(CURLPAUSE_ALL);						// Keep libcurl at bay.
		  curl_easy_request_w->bad_file_descriptor(curl_easy_request_w);	// Make the main thread cleanly terminate this transaction.
		}
		continue;
	  }
	  // Update the clocks.
	  AICurlTimer::sTime_1ms = get_clock_count() * AICurlTimer::sClockWidth_1ms;
	  Dout(dc::curl, "AICurlTimer::sTime_1ms = " << AICurlTimer::sTime_1ms);
	  HTTPTimeout::sTime_10ms = AICurlTimer::sTime_1ms / 10;
	  if (ready == 0)
	  {
		if (libcurl_timeout)
		{
		  multi_handle_w->socket_action(CURL_SOCKET_TIMEOUT, 0);
		}
		else
		{
		  // Update MultiHandle::mTimeout because next loop we need to sleep timeout_ms shorter.
		  multi_handle_w->update_timeout(timeout_ms);
		  Dout(dc::curl, "MultiHandle::mTimeout set to " << multi_handle_w->getTimeout() << " ms.");
		}
		// Handle timers.
		if (AICurlTimer::expiresBefore(1))
		{
		  AICurlTimer::handleExpiration();
		}
		// Handle stalling transactions.
		multi_handle_w->handle_stalls();
	  }
	  else
	  {
		if (multi_handle_w->mReadPollSet->is_set(mWakeUpFd))
		{
		  // Process commands from main-thread. This can add or remove filedescriptors from the poll sets.
		  wakeup(multi_handle_w);
		  --ready;
		}
		// Handle all active filedescriptors.
		MergeIterator iter(multi_handle_w->mReadPollSet, multi_handle_w->mWritePollSet);
		curl_socket_t fd;
		int ev_bitmask;
		while (ready > 0 && iter.next(fd, ev_bitmask))
		{
		  ready -= (ev_bitmask == (CURL_CSELECT_IN|CURL_CSELECT_OUT)) ? 2 : 1;
		  // This can cause libcurl to do callbacks and remove filedescriptors, causing us to reset their bits in the poll sets.
		  multi_handle_w->socket_action(fd, ev_bitmask);
		  llassert(ready >= 0);
		}
		// Note that ready is not necessarily 0 here, because it's possible
		// that libcurl removed file descriptors which we subsequently
		// didn't handle.
	  }
	  multi_handle_w->check_msg_queue();
	}
	// Clear the queued requests.
	AIPerService::purge();
  }
  AICurlMultiHandle::destroyInstance();
}

//-----------------------------------------------------------------------------
// MultiHandle

LLAtomicU32 MultiHandle::sTotalAdded;

MultiHandle::MultiHandle(void) : mTimeout(-1), mReadPollSet(NULL), mWritePollSet(NULL)
{
  mReadPollSet = new PollSet;
  mWritePollSet = new PollSet;
  check_multi_code(curl_multi_setopt(mMultiHandle, CURLMOPT_SOCKETFUNCTION, &MultiHandle::socket_callback));
  check_multi_code(curl_multi_setopt(mMultiHandle, CURLMOPT_SOCKETDATA, this));
  check_multi_code(curl_multi_setopt(mMultiHandle, CURLMOPT_TIMERFUNCTION, &MultiHandle::timer_callback));
  check_multi_code(curl_multi_setopt(mMultiHandle, CURLMOPT_TIMERDATA, this));
}

MultiHandle::~MultiHandle()
{
  llinfos << "Destructing MultiHandle with " << mAddedEasyRequests.size() << " active curl easy handles." << llendl;

  // This thread was terminated.
  // Curl demands that all handles are removed from the multi session handle before calling curl_multi_cleanup.
  for(addedEasyRequests_type::iterator iter = mAddedEasyRequests.begin(); iter != mAddedEasyRequests.end(); iter = mAddedEasyRequests.begin())
  {
	finish_easy_request(*iter, CURLE_GOT_NOTHING);	// Error code is not used anyway.
	remove_easy_request(*iter);
  }
  delete mWritePollSet;
  delete mReadPollSet;
}

void MultiHandle::handle_stalls(void)
{
  for(addedEasyRequests_type::iterator iter = mAddedEasyRequests.begin(); iter != mAddedEasyRequests.end();)
  {
	if (AICurlEasyRequest_wat(**iter)->has_stalled())
	{
	  Dout(dc::curl, "MultiHandle::handle_stalls(): Easy request stalled! [" << (void*)iter->get_ptr().get() << "]");
	  finish_easy_request(*iter, CURLE_OPERATION_TIMEDOUT);
	  remove_easy_request(iter++, false);
	}
	else
	  ++iter;
  }
}

//static
int MultiHandle::socket_callback(CURL* easy, curl_socket_t s, int action, void* userp, void* socketp)
{
#ifdef CWDEBUG
  ThreadSafeBufferedCurlEasyRequest* lockobj = NULL;
  curl_easy_getinfo(easy, CURLINFO_PRIVATE, &lockobj);
  DoutEntering(dc::curl, "MultiHandle::socket_callback((CURL*)" << (void*)easy << ", " << s <<
	  ", " << action_str(action) << ", " << (void*)userp << ", " << (void*)socketp << ") [CURLINFO_PRIVATE = " << (void*)lockobj << "]");
#endif
  MultiHandle& self = *static_cast<MultiHandle*>(userp);
  CurlSocketInfo* sock_info = static_cast<CurlSocketInfo*>(socketp);
  if (action == CURL_POLL_REMOVE)
  {
	delete sock_info;
  }
  else
  {
	if (!sock_info)
	{
	  ThreadSafeBufferedCurlEasyRequest* ptr;
	  CURLcode rese = curl_easy_getinfo(easy, CURLINFO_PRIVATE, &ptr);
	  llassert_always(rese == CURLE_OK);
	  sock_info = new CurlSocketInfo(self, ASSERT_ONLY(easy,) s, action, ptr);
	}
	else
	{
	  sock_info->set_action(action);
	}
  }
  return 0;
}

//static
int MultiHandle::timer_callback(CURLM* multi, long timeout_ms, void* userp)
{
  MultiHandle& self = *static_cast<MultiHandle*>(userp);
  llassert(multi == self.mMultiHandle);
  self.mTimeout = timeout_ms;
  Dout(dc::curl, "MultiHandle::timer_callback(): timeout set to " << timeout_ms << " ms.");
  return 0;
}

CURLMcode MultiHandle::socket_action(curl_socket_t sockfd, int ev_bitmask)
{
  int running_handles;
  CURLMcode res;
  do
  {
    res = check_multi_code(curl_multi_socket_action(mMultiHandle, sockfd, ev_bitmask, &running_handles));
  }
  while(res == CURLM_CALL_MULTI_PERFORM);
  llassert(mAddedEasyRequests.size() >= (size_t)running_handles);
  AICurlInterface::Stats::running_handles = running_handles;
  return res;
}

CURLMcode MultiHandle::assign(curl_socket_t sockfd, void* sockptr)
{
  return check_multi_code(curl_multi_assign(mMultiHandle, sockfd, sockptr));
}

CURLMsg const* MultiHandle::info_read(int* msgs_in_queue) const
{
  CURLMsg const* ret = curl_multi_info_read(mMultiHandle, msgs_in_queue);
  // NULL could be an error, but normally it isn't, so don't print anything and
  // never increment Stats::multi_errors. However, lets just increment multi_calls
  // when it certainly wasn't an error...
  if (ret)
	AICurlInterface::Stats::multi_calls++;
  return ret;
}

U32 curl_max_total_concurrent_connections = 32;						// Initialized on start up by startCurlThread().

bool MultiHandle::add_easy_request(AICurlEasyRequest const& easy_request, bool from_queue)
{
  bool throttled = true;		// Default.
  AICapabilityType capability_type;
  AIPerServicePtr per_service;
  {
	AICurlEasyRequest_wat curl_easy_request_w(*easy_request);
	capability_type = curl_easy_request_w->capability_type();
	per_service = curl_easy_request_w->getPerServicePtr();
	if (!from_queue)
	{
	  // Add the request to the back of a non-empty queue.
	  PerService_wat per_service_w(*per_service);
	  if (per_service_w->queue(easy_request, capability_type, false))
	  {
		// The queue was not empty, therefore the request was queued.
#ifdef SHOW_ASSERT
		// Not active yet, but it's no longer an error if next we try to remove the request.
		curl_easy_request_w->mRemovedPerCommand = false;
#endif
		// This is a fail-safe. Normally, if there is anything in the queue then things should
		// be running (normally an attempt is made to add from the queue whenever a request
		// finishes). However, it CAN happen on occassion that things get 'stuck' with
		// nothing running, so nothing will ever finish and therefore the queue would never
		// be checked. Only do this when there is indeed nothing running (added) though.
		if (per_service_w->nothing_added(capability_type))
		{
		  per_service_w->add_queued_to(this);
		}
		return true;
	  }
	}
	bool too_much_bandwidth = !curl_easy_request_w->approved() && AIPerService::checkBandwidthUsage(per_service, get_clock_count() * HTTPTimeout::sClockWidth_40ms);
	PerService_wat per_service_w(*per_service);
	if (!too_much_bandwidth && sTotalAdded < curl_max_total_concurrent_connections && !per_service_w->throttled(capability_type))
	{
	  curl_easy_request_w->set_timeout_opts();
	  if (curl_easy_request_w->add_handle_to_multi(curl_easy_request_w, mMultiHandle) == CURLM_OK)
	  {
		per_service_w->added_to_multi_handle(capability_type);	// (About to be) added to mAddedEasyRequests.
		throttled = false;						// Fall through...
	  }
	}
  } // Release the lock on easy_request.
  if (!throttled)
  {												// ... to here.
#ifdef SHOW_ASSERT
	std::pair<addedEasyRequests_type::iterator, bool> res =
#endif
		mAddedEasyRequests.insert(easy_request);
	llassert(res.second);						// May not have been added before.
	sTotalAdded++;
	llassert(sTotalAdded == mAddedEasyRequests.size());
	Dout(dc::curl, "MultiHandle::add_easy_request: Added AICurlEasyRequest " << (void*)easy_request.get_ptr().get() <<
		"; now processing " << mAddedEasyRequests.size() << " easy handles [running_handles = " << AICurlInterface::Stats::running_handles << "].");
	return true;
  }
  if (from_queue)
  {
	// Throttled. Do not add to queue, because it is already in the queue.
	return false;
  }
  // The request could not be added, we have to queue it.
  PerService_wat(*per_service)->queue(easy_request, capability_type);
#ifdef SHOW_ASSERT
  // Not active yet, but it's no longer an error if next we try to remove the request.
  AICurlEasyRequest_wat(*easy_request)->mRemovedPerCommand = false;
#endif
  return true;
}

CURLMcode MultiHandle::remove_easy_request(AICurlEasyRequest const& easy_request, bool as_per_command)
{
  AICurlEasyRequest_wat easy_request_w(*easy_request);
  addedEasyRequests_type::iterator iter = mAddedEasyRequests.find(easy_request);
  if (iter == mAddedEasyRequests.end())
  {
	// The request could be queued.
#ifdef SHOW_ASSERT
	bool removed =
#endif
	easy_request_w->removeFromPerServiceQueue(easy_request, easy_request_w->capability_type());
#ifdef SHOW_ASSERT
	if (removed)
	{
	  // Now a second remove command would be an error again.
	  AICurlEasyRequest_wat(*easy_request)->mRemovedPerCommand = true;
	}
#endif
	return (CURLMcode)-2;				// Was already removed before, or never added (queued).
  }
  return remove_easy_request(iter, as_per_command);
}

CURLMcode MultiHandle::remove_easy_request(addedEasyRequests_type::iterator const& iter, bool as_per_command)
{
  CURLMcode res;
  AICapabilityType capability_type;
  AIPerServicePtr per_service;
  {
	AICurlEasyRequest_wat curl_easy_request_w(**iter);
	bool downloaded_something = curl_easy_request_w->received_data();
	bool success = curl_easy_request_w->success();
	res = curl_easy_request_w->remove_handle_from_multi(curl_easy_request_w, mMultiHandle);
	capability_type = curl_easy_request_w->capability_type();
	per_service = curl_easy_request_w->getPerServicePtr();
	PerService_wat(*per_service)->removed_from_multi_handle(capability_type, downloaded_something, success);		// (About to be) removed from mAddedEasyRequests.
#ifdef SHOW_ASSERT
	curl_easy_request_w->mRemovedPerCommand = as_per_command;
#endif
  }
#if CWDEBUG
  ThreadSafeBufferedCurlEasyRequest* lockobj = iter->get_ptr().get();
#endif
  mAddedEasyRequests.erase(iter);
  --sTotalAdded;
  llassert(sTotalAdded == mAddedEasyRequests.size());
#if CWDEBUG
  Dout(dc::curl, "MultiHandle::remove_easy_request: Removed AICurlEasyRequest " << (void*)lockobj <<
	  "; now processing " << mAddedEasyRequests.size() << " easy handles [running_handles = " << AICurlInterface::Stats::running_handles << "].");
#endif

  // Attempt to add a queued request, if any.
  PerService_wat(*per_service)->add_queued_to(this);
  return res;
}

void MultiHandle::check_msg_queue(void)
{
  CURLMsg const* msg;
  int msgs_left;
  while ((msg = info_read(&msgs_left)))
  {
	if (msg->msg == CURLMSG_DONE)
	{
	  CURL* easy = msg->easy_handle;
	  ThreadSafeBufferedCurlEasyRequest* ptr;
	  CURLcode rese = curl_easy_getinfo(easy, CURLINFO_PRIVATE, &ptr);
	  llassert_always(rese == CURLE_OK);
	  AICurlEasyRequest easy_request(ptr);
	  llassert(*AICurlEasyRequest_wat(*easy_request) == easy);
	  // Store result and trigger events for the easy request.
	  finish_easy_request(easy_request, msg->data.result);
	  // This invalidates msg, but not easy_request.
	  CURLMcode res = remove_easy_request(easy_request);
	  // This should hold, I think, because the handles are obviously ok and
	  // the only error we could get is when remove_easy_request() was already
	  // called before (by this thread); but if that was the case then the easy
	  // handle should not have been be returned by info_read()...
	  llassert(res == CURLM_OK);
	  // Nevertheless, if it was already removed then just ignore it.
	  if (res == CURLM_OK)
	  {
	  }
	  else if (res == -2)
	  {
		llwarns << "Curl easy handle returned by curl_multi_info_read() that is not (anymore) in MultiHandle::mAddedEasyRequests!?!" << llendl;
	  }
	  // Destruction of easy_request at this point, causes the CurlEasyRequest to be deleted.
	}
  }
}

void MultiHandle::finish_easy_request(AICurlEasyRequest const& easy_request, CURLcode result)
{
  AICurlEasyRequest_wat curl_easy_request_w(*easy_request);
  // Final body bandwidth update.
  curl_easy_request_w->update_body_bandwidth();
  // Store the result in the easy handle.
  curl_easy_request_w->storeResult(result);
#ifdef CWDEBUG
  char* eff_url;
  curl_easy_request_w->getinfo(CURLINFO_EFFECTIVE_URL, &eff_url);
  double namelookup_time, connect_time, appconnect_time, pretransfer_time, starttransfer_time;
  curl_easy_request_w->getinfo(CURLINFO_NAMELOOKUP_TIME, &namelookup_time);
  curl_easy_request_w->getinfo(CURLINFO_CONNECT_TIME, &connect_time);
  curl_easy_request_w->getinfo(CURLINFO_APPCONNECT_TIME, &appconnect_time);
  curl_easy_request_w->getinfo(CURLINFO_PRETRANSFER_TIME, &pretransfer_time);
  curl_easy_request_w->getinfo(CURLINFO_STARTTRANSFER_TIME, &starttransfer_time);
  // If appconnect_time is almost equal to connect_time, then it was just set because this is a connection re-use.
  if (appconnect_time - connect_time <= 1e-6)
  {
	appconnect_time = 0;
  }
  // If connect_time is almost equal to namelookup_time, then it was just set because it was already connected.
  if (connect_time - namelookup_time <= 1e-6)
  {
	connect_time = 0;
  }
  // If namelookup_time is less than 500 microseconds, then it's very likely just a DNS cache lookup.
  if (namelookup_time < 500e-6)
  {
	namelookup_time = 0;
  }
  Dout(dc::curl|continued_cf, "Finished: " << eff_url << " (" << curl_easy_strerror(result));
  if (result != CURLE_OK)
  {
	long os_error;
	curl_easy_request_w->getinfo(CURLINFO_OS_ERRNO, &os_error);
	if (os_error)
	{
#if WINDOWS_CODE
	  Dout(dc::continued, ": " << formatWSAError(os_error));
#else
	  Dout(dc::continued, ": " << strerror(os_error));
#endif
	}
  }
  Dout(dc::continued, "); ");
  if (namelookup_time)
  {
    Dout(dc::continued, "namelookup time: " << namelookup_time << ", ");
  }
  if (connect_time)
  {
    Dout(dc::continued, "connect_time: " << connect_time << ", ");
  }
  if (appconnect_time)
  {
	Dout(dc::continued, "appconnect_time: " << appconnect_time << ", ");
  }
  Dout(dc::finish, "pretransfer_time: " << pretransfer_time << ", starttransfer_time: " << starttransfer_time <<
	  ". [CURLINFO_PRIVATE = " << (void*)easy_request.get_ptr().get() << "]");
#endif
  // Signal that this easy handle finished.
  curl_easy_request_w->done(curl_easy_request_w, result);
}

} // namespace curlthread
} // namespace AICurlPrivate

//static
void AICurlMultiHandle::destroyInstance(void)
{
  LLThreadLocalData& tldata = LLThreadLocalData::tldata();
  Dout(dc::curl, "Destroying AICurlMultiHandle [" << (void*)tldata.mCurlMultiHandle << "] for thread \"" << tldata.mName << "\".");
  delete tldata.mCurlMultiHandle;
  tldata.mCurlMultiHandle = NULL;
}

//=============================================================================
// MAIN-THREAD (needing to access the above declarations).

//static
AICurlMultiHandle& AICurlMultiHandle::getInstance(void)
{
  LLThreadLocalData& tldata = LLThreadLocalData::tldata();
  if (!tldata.mCurlMultiHandle)
  {
	tldata.mCurlMultiHandle = new AICurlMultiHandle;
	Dout(dc::curl, "Created AICurlMultiHandle [" << (void*)tldata.mCurlMultiHandle << "] for thread \"" << tldata.mName << "\".");
  }
  return *static_cast<AICurlMultiHandle*>(tldata.mCurlMultiHandle);
}

namespace AICurlPrivate {

bool curlThreadIsRunning(void)
{
  using curlthread::AICurlThread;
  return AICurlThread::sInstance && !AICurlThread::sInstance->isStopped();
}

void wakeUpCurlThread(void)
{
  using curlthread::AICurlThread;
  if (AICurlThread::sInstance)
	AICurlThread::sInstance->wakeup_thread();
}

void stopCurlThread(void)
{
  using curlthread::AICurlThread;
  if (AICurlThread::sInstance)
  {
	AICurlThread::sInstance->wakeup_thread(true);
	int count = 401;
	while(--count && !AICurlThread::sInstance->isStopped())
	{
	  ms_sleep(10);
	}
	if (AICurlThread::sInstance->isStopped())
	{
	  // isStopped() returns true somewhere at the end of run(),
	  // but that doesn't mean the thread really stopped: it still
	  // needs to destroy it's static variables.
	  // If we don't join here, then there is a chance that the
	  // curl thread will crash when using globals that we (the
	  // main thread) will have destroyed before it REALLY finished.
	  AICurlThread::sInstance->join_thread();	// Wait till it is REALLY done.
	}
	llinfos << "Curl thread" << (curlThreadIsRunning() ? " not" : "") << " stopped after " << ((400 - count) * 10) << "ms." << llendl;
  }
}

void clearCommandQueue(void)
{
	// Clear the command queue now in order to avoid the global deinitialization order fiasco.
	command_queue_wat command_queue_w(command_queue);
	command_queue_w->commands.clear();
	command_queue_w->size = 0;
}

//-----------------------------------------------------------------------------
// BufferedCurlEasyRequest

void BufferedCurlEasyRequest::setStatusAndReason(U32 status, std::string const& reason)
{
  mStatus = status;
  mReason = reason;
  if (status >= 100 && status < 600 && (status % 100) < 20)
  {
	// Only count statistic for sane values.
	AICurlInterface::Stats::status_count[AICurlInterface::Stats::status2index(mStatus)]++;
  }

  // Sanity check. If the server replies with a redirect status then we better have that option turned on!
  if ((status >= 300 && status < 400) && mResponder && !mResponder->redirect_status_ok())
  {
	llerrs << "Received " << status << " (" << reason << ") for responder \"" << mResponder->getName() << "\" which has no followRedir()!" << llendl;
  }
}

void BufferedCurlEasyRequest::processOutput(void)
{
  U32 responseCode = 0;	
  std::string responseReason;
  
  CURLcode code;
  AITransferInfo info;
  getResult(&code, &info);
  if (code == CURLE_OK && !is_internal_http_error(mStatus))
  {
	getinfo(CURLINFO_RESPONSE_CODE, &responseCode);
	// If getResult code is CURLE_OK then we should have decoded the first header line ourselves.
	llassert(responseCode == mStatus);
	if (responseCode == mStatus)
	  responseReason = mReason;
	else
	  responseReason = "Unknown reason.";
  }
  else
  {
	responseReason = (code == CURLE_OK) ? mReason : std::string(curl_easy_strerror(code));
	switch (code)
	{
	  case CURLE_FAILED_INIT:
		responseCode = HTTP_INTERNAL_ERROR_OTHER;
		break;
	  case CURLE_OPERATION_TIMEDOUT:
		responseCode = HTTP_INTERNAL_ERROR_CURL_TIMEOUT;
		break;
	  case CURLE_WRITE_ERROR:
		responseCode = HTTP_INTERNAL_ERROR_LOW_SPEED;
		break;
	  default:
		responseCode = HTTP_INTERNAL_ERROR_CURL_OTHER;
		break;
	}
	if (responseCode == HTTP_INTERNAL_ERROR_LOW_SPEED)
	{
		// Rewrite error to something understandable.
		responseReason = llformat("Connection to \"%s\" stalled: download speed dropped below %u bytes/s for %u seconds (up till that point, %s received a total of %lu bytes). "
			"To change these values, go to Advanced --> Debug Settings and change CurlTimeoutLowSpeedLimit and CurlTimeoutLowSpeedTime respectively.",
			mResponder->getURL().c_str(), mResponder->getHTTPTimeoutPolicy().getLowSpeedLimit(), mResponder->getHTTPTimeoutPolicy().getLowSpeedTime(),
			mResponder->getName(), mTotalRawBytes);
	}
	setopt(CURLOPT_FRESH_CONNECT, TRUE);
  }

  if (code != CURLE_OK)
  {
	print_diagnostics(code);
  }
  sResponderCallbackMutex.lock();
  if (!sShuttingDown)
  {
	if (mBufferEventsTarget)
	{
	  // Only the responder registers for these events.
	  llassert(mBufferEventsTarget == mResponder.get());
	  // Allow clients to parse result codes and headers before we attempt to parse
	  // the body and provide completed/result/error calls.
	  mBufferEventsTarget->completed_headers(responseCode, responseReason, (code == CURLE_FAILED_INIT) ? NULL : &info);
	}
	mResponder->finished(code, responseCode, responseReason, sChannels, mOutput);
  }
  sResponderCallbackMutex.unlock();
  mResponder = NULL;

  resetState();
}

//static
void BufferedCurlEasyRequest::shutdown(void)
{
  sResponderCallbackMutex.lock();
  sShuttingDown = true;
  sResponderCallbackMutex.unlock();
}

void BufferedCurlEasyRequest::received_HTTP_header(void)
{
  if (mBufferEventsTarget)
	mBufferEventsTarget->received_HTTP_header();
}

void BufferedCurlEasyRequest::received_header(std::string const& key, std::string const& value)
{
  if (mBufferEventsTarget)
	mBufferEventsTarget->received_header(key, value);
}

void BufferedCurlEasyRequest::completed_headers(U32 status, std::string const& reason, AITransferInfo* info)
{
  if (mBufferEventsTarget)
	mBufferEventsTarget->completed_headers(status, reason, info);
}

//static
size_t BufferedCurlEasyRequest::curlWriteCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
  ThreadSafeBufferedCurlEasyRequest* lockobj = static_cast<ThreadSafeBufferedCurlEasyRequest*>(user_data);

  // We need to lock the curl easy request object too, because that lock is used
  // to make sure that callbacks and destruction aren't done simultaneously.
  AICurlEasyRequest_wat self_w(*lockobj);

  S32 bytes = size * nmemb;		// The amount to write.
  // BufferedCurlEasyRequest::setBodyLimit is never called, so buffer_w->mBodyLimit is infinite.
  //S32 bytes = llmin(size * nmemb, buffer_w->mBodyLimit); buffer_w->mBodyLimit -= bytes;
  self_w->getOutput()->append(sChannels.in(), (U8 const*)data, bytes);
  // Update HTTP bandwith.
  self_w->update_body_bandwidth();
  // Update timeout administration.
  if (self_w->httptimeout()->data_received(bytes))					// Update timeout administration.
  {
	// Transfer timed out. Return 0 which will abort with error CURLE_WRITE_ERROR.
	return 0;
  }
  return bytes;
}

void BufferedCurlEasyRequest::update_body_bandwidth(void)
{
  double size_download;	// Total amount of raw bytes received so far (ie. still compressed, 'bytes' is uncompressed).
  getinfo(CURLINFO_SIZE_DOWNLOAD, &size_download);
  size_t total_raw_bytes = size_download;
  size_t raw_bytes = total_raw_bytes - mTotalRawBytes;
  if (mTotalRawBytes == 0 && total_raw_bytes > 0)
  {
	// Update service/capability type administration for the HTTP Debug Console.
	PerService_wat per_service_w(*mPerServicePtr);
	per_service_w->download_started(mCapabilityType);
  }
  mTotalRawBytes = total_raw_bytes;
  // Note that in some cases (like HTTP_PARTIAL_CONTENT), the output of CURLINFO_SIZE_DOWNLOAD lags
  // behind and will return 0 the first time, and the value of the previous chunk the next time.
  // The last call from MultiHandle::finish_easy_request corrects this, in that case.
  if (raw_bytes > 0)
  {
	U64 const sTime_40ms = curlthread::HTTPTimeout::sTime_10ms >> 2;
	AIAverage& http_bandwidth(PerService_wat(*getPerServicePtr())->bandwidth());
	http_bandwidth.addData(raw_bytes, sTime_40ms);
	sHTTPBandwidth.addData(raw_bytes, sTime_40ms);
  }
}

//static
size_t BufferedCurlEasyRequest::curlReadCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
  ThreadSafeBufferedCurlEasyRequest* lockobj = static_cast<ThreadSafeBufferedCurlEasyRequest*>(user_data);

  // We need to lock the curl easy request object too, because that lock is used
  // to make sure that callbacks and destruction aren't done simultaneously.
  AICurlEasyRequest_wat self_w(*lockobj);

  S32 bytes = size * nmemb;		// The maximum amount to read.
  self_w->mLastRead = self_w->getInput()->readAfter(sChannels.out(), self_w->mLastRead, (U8*)data, bytes);
  self_w->mRequestTransferedBytes += bytes;		// Accumulate data sent to the server.
  llassert(self_w->mRequestTransferedBytes <= self_w->mContentLength);	// Content-Length should always be known, and we should never be sending more.
  // Timeout administration.
  if (self_w->httptimeout()->data_sent(bytes, self_w->mRequestTransferedBytes >= self_w->mContentLength))
  {
	// Transfer timed out. Return CURL_READFUNC_ABORT which will abort with error CURLE_ABORTED_BY_CALLBACK.
	return CURL_READFUNC_ABORT;
  }
  return bytes;					// Return the amount actually read (might be lowered by readAfter()).
}

//static
size_t BufferedCurlEasyRequest::curlHeaderCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
  ThreadSafeBufferedCurlEasyRequest* lockobj = static_cast<ThreadSafeBufferedCurlEasyRequest*>(user_data);

  // We need to lock the curl easy request object, because that lock is used
  // to make sure that callbacks and destruction aren't done simultaneously.
  AICurlEasyRequest_wat self_w(*lockobj);

  // This used to be headerCallback() in llurlrequest.cpp.

  char const* const header_line = static_cast<char const*>(data);
  size_t const header_len = size * nmemb;
  if (!header_len)
  {
	return header_len;
  }
  std::string header(header_line, header_len);
  bool being_redirected = false;
  bool done = false;
  if (!LLStringUtil::_isASCII(header))
  {
	done = true;
  }
  // Per HTTP spec the first header line must be the status line.
  else if (header.substr(0, 5) == "HTTP/")
  {
	std::string::iterator const begin = header.begin();
	std::string::iterator const end = header.end();
	std::string::iterator pos1 = std::find(begin, end, ' ');
	if (pos1 != end) ++pos1;
	std::string::iterator pos2 = std::find(pos1, end, ' ');
	if (pos2 != end) ++pos2;
	std::string::iterator pos3 = std::find(pos2, end, '\r');
	U32 status = 0;
	std::string reason;
	if (pos3 != end && LLStringOps::isDigit(*pos1))
	{
	  status = atoi(&header_line[pos1 - begin]);
	  reason.assign(pos2, pos3);
	}
	if (!(status >= 100 && status < 600 && (status % 100) < 20))	// Sanity check on the decoded status.
	{
	  if (status == 0)
	  {
		reason = "Header parse error.";
		llwarns << "Received broken header line from server: \"" << header << "\"" << llendl;
	  }
	  else
	  {
		reason = "Unexpected HTTP status.";
		llwarns << "Received unexpected status value from server (" << status << "): \"" << header << "\"" << llendl;
	  }
	  // Either way, this status value is not understood (or taken into account).
	  // Set it to internal error so that the rest of code treats it as an error.
	  status = HTTP_INTERNAL_ERROR_OTHER;
	}
	self_w->received_HTTP_header();
	self_w->setStatusAndReason(status, reason);
	done = true;
	if (status >= 300 && status < 400)
	{
	  // Timeout administration needs to know if we're being redirected.
	  being_redirected = true;
	}
  }
  // Update HTTP bandwidth.
  U64 const sTime_40ms = curlthread::HTTPTimeout::sTime_10ms >> 2;
  AIAverage& http_bandwidth(PerService_wat(*self_w->getPerServicePtr())->bandwidth());
  http_bandwidth.addData(header_len, sTime_40ms);
  sHTTPBandwidth.addData(header_len, sTime_40ms);
  // Update timeout administration. This must be done after the status is already known.
  if (self_w->httptimeout()->data_received(header_len/*,*/ ASSERT_ONLY_COMMA(self_w->upload_error_status())))
  {
	// Transfer timed out. Return 0 which will abort with error CURLE_WRITE_ERROR.
	return 0;
  }
  if (being_redirected)
  {
	  // Call this after data_received(), because that might reset mBeingRedirected if it causes a late- upload_finished!
	  // Ie, when the upload finished was not detected and this is a redirect header then the call to data_received()
	  // will call upload_finished() which sets HTTPTimeout::mUploadFinished (and resets HTTPTimeout::mBeingRedirected),
	  // after which we set HTTPTimeout::mBeingRedirected here because we ARE being redirected.
	  self_w->httptimeout()->being_redirected();
  }
  if (done)
  {
	return header_len;
  }

  std::string::iterator sep = std::find(header.begin(), header.end(), ':');

  if (sep != header.end())
  {
	std::string key(header.begin(), sep);
	std::string value(sep + 1, header.end());

	key = utf8str_tolower(utf8str_trim(key));
	value = utf8str_trim(value);

	self_w->received_header(key, value);
  }
  else
  {
	LLStringUtil::trim(header);
	if (!header.empty())
	{
	  llwarns << "Unable to parse header: " << header << llendl;
	}
  }

  return header_len;
}

#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
int debug_callback(CURL* handle, curl_infotype infotype, char* buf, size_t size, void* user_ptr)
{
  BufferedCurlEasyRequest* request = (BufferedCurlEasyRequest*)user_ptr;
  if (infotype == CURLINFO_HEADER_OUT && size >= 5 && (strncmp(buf, "GET ", 4) == 0 || strncmp(buf, "HEAD ", 5) == 0))
  {
	request->mDebugIsHeadOrGetMethod = true;
  }
  if (infotype == CURLINFO_TEXT)
  {
	if (!strncmp(buf, "STATE: WAITCONNECT => ", 22))
	{
	  if (buf[22] == 'P' || buf[22] == 'D')		// PROTOCONNECT or DO.
	  {
		int n = size - 1;
		while (buf[n] != ')')
		{
		  llassert(n > 56);
		  --n;
		}
		int connectionnr = 0;
		int factor = 1;
		do
		{
		  llassert(n > 56);
		  --n;
		  connectionnr += factor * (buf[n] - '0');
		  factor *= 10;
		}
		while(buf[n - 1] != '#');
		// A new connection was established.
		request->connection_established(connectionnr);
	  }
	  else
	  {
	  	llassert(buf[22] == 'C');				// COMPLETED (connection failure).
	  }
	}
	else if (!strncmp(buf, "Closing connection", 18))
	{
	  int n = size - 1;
	  while (!std::isdigit(buf[n]))
	  {
		llassert(n > 20);
		--n;
	  }
	  int connectionnr = 0;
	  int factor = 1;
	  do
	  {
		llassert(n > 19);
		connectionnr += factor * (buf[n] - '0');
		factor *= 10;
		--n;
	  }
	  while(buf[n] != '#');
	  // A connection was closed.
	  request->connection_closed(connectionnr);
	}
  }

#ifdef DEBUG_CURLIO
  if (!debug_curl_print_debug(handle))
  {
	return 0;
  }
#endif

#ifdef CWDEBUG
  using namespace ::libcwd;
  std::ostringstream marker;
  marker << (void*)request->get_lockobj() << ' ';
  libcw_do.push_marker();
  libcw_do.marker().assign(marker.str().data(), marker.str().size());
  if (!debug::channels::dc::curlio.is_on())
	debug::channels::dc::curlio.on();
  LibcwDoutScopeBegin(LIBCWD_DEBUGCHANNELS, libcw_do, dc::curlio|cond_nonewline_cf(infotype == CURLINFO_TEXT))
#else
  if (infotype == CURLINFO_TEXT)
  {
	while (size > 0 && (buf[size - 1] == '\r' ||  buf[size - 1] == '\n'))
	  --size;
  }
  LibcwDoutScopeBegin(LIBCWD_DEBUGCHANNELS, libcw_do, dc::curlio)
#endif
  switch (infotype)
  {
	case CURLINFO_TEXT:
	  LibcwDoutStream << "* ";
	  break;
	case CURLINFO_HEADER_IN:
	  LibcwDoutStream << "H> ";
	  break;
	case CURLINFO_HEADER_OUT:
	  LibcwDoutStream << "H< ";
	  break;
	case CURLINFO_DATA_IN:
	  LibcwDoutStream << "D> ";
	  break;
	case CURLINFO_DATA_OUT:
	  LibcwDoutStream << "D< ";
	  break;
	case CURLINFO_SSL_DATA_IN:
	  LibcwDoutStream << "S> ";
	  break;
	case CURLINFO_SSL_DATA_OUT:
	  LibcwDoutStream << "S< ";
	  break;
	default:
	  LibcwDoutStream << "?? ";
  }
  if (infotype == CURLINFO_TEXT)
	LibcwDoutStream.write(buf, size);
  else if (infotype == CURLINFO_HEADER_IN || infotype == CURLINFO_HEADER_OUT)
	LibcwDoutStream << libcwd::buf2str(buf, size);
  else if (infotype == CURLINFO_DATA_IN)
  {
	LibcwDoutStream << size << " bytes";
	bool finished = false;
	size_t i = 0;
	while (i < size)
	{
	  char c = buf[i];
	  if (!('0' <= c && c <= '9') && !('a' <= c && c <= 'f'))
	  {
		if (0 < i && i + 1 < size && buf[i] == '\r' && buf[i + 1] == '\n')
		{
		  // Binary output: "[0-9a-f]*\r\n ...binary data..."
		  LibcwDoutStream << ": \"" << libcwd::buf2str(buf, i + 2) << "\"...";
		  finished = true;
		}
		break;
	  }
	  ++i;
	}
	if (!finished && size > 9 && buf[0] == '<')
	{
	  // Human readable output: html, xml or llsd.
	  if (!strncmp(buf, "<!DOCTYPE", 9) || !strncmp(buf, "<?xml", 5) || !strncmp(buf, "<llsd>", 6))
	  {
		LibcwDoutStream << ": \"" << libcwd::buf2str(buf, size) << '"';
		finished = true;
	  }
	}
	if (!finished)
	{
	  // Unknown format. Only print the first and last 20 characters.
	  if (size > 40UL)
	  {
		LibcwDoutStream << ": \"" << libcwd::buf2str(buf, 20) << "\"...\"" << libcwd::buf2str(&buf[size - 20], 20) << '"';
	  }
	  else
	  {
		LibcwDoutStream << ": \"" << libcwd::buf2str(buf, size) << '"';
	  }
	}
  }
  else if (infotype == CURLINFO_DATA_OUT)
	LibcwDoutStream << size << " bytes: \"" << libcwd::buf2str(buf, size) << '"';
  else
	LibcwDoutStream << size << " bytes";
  LibcwDoutScopeEnd;
#ifdef CWDEBUG
  libcw_do.pop_marker();
#endif
  return 0;
}
#endif // defined(CWDEBUG) || defined(DEBUG_CURLIO)

} // namespace AICurlPrivate

//-----------------------------------------------------------------------------
// AICurlEasyRequest

void AICurlEasyRequest::addRequest(void)
{
  using namespace AICurlPrivate;

  {
	// Write-lock the command queue.
	command_queue_wat command_queue_w(command_queue);
#ifdef SHOW_ASSERT
	// This debug code checks if we aren't calling addRequest() twice for the same object.
	// That means that the main thread already called (and finished, this is also the
	// main thread) this function, which also follows from that we just locked command_queue.
	// That leaves three options: It's still in the queue, or it was removed and is currently
	// processed by the curl thread with again two options: either it was already added
	// to the multi session handle or not yet.

	// Find the last command added.
	command_st cmd = cmd_none;
	for (std::deque<Command>::iterator iter = command_queue_w->commands.begin(); iter != command_queue_w->commands.end(); ++iter)
	{
	  if (*iter == *this)
	  {
		cmd = iter->command();
		break;
	  }
	}
	llassert(cmd == cmd_none || cmd == cmd_remove);	// Not in queue, or last command was to remove it.
	if (cmd == cmd_none)
	{
	  // Read-lock command_being_processed.
	  command_being_processed_rat command_being_processed_r(command_being_processed);
	  if (*command_being_processed_r == *this)
	  {
		// May not be in-between being removed from the command queue but not added to the multi session handle yet.
		llassert(command_being_processed_r->command() == cmd_remove);
	  }
	  else
	  {
		// May not already be added to the multi session handle.
		llassert(!AICurlEasyRequest_wat(*get())->active());
	  }
	}
#endif
	// Add a command to add the new request to the multi session to the command queue.
	command_queue_w->commands.push_back(Command(*this, cmd_add));
	command_queue_w->size++;
	AICurlEasyRequest_wat curl_easy_request_w(*get());
	PerService_wat(*curl_easy_request_w->getPerServicePtr())->added_to_command_queue(curl_easy_request_w->capability_type());
	curl_easy_request_w->add_queued();
  }
  // Something was added to the queue, wake up the thread to get it.
  wakeUpCurlThread();
}

void AICurlEasyRequest::removeRequest(void)
{
  using namespace AICurlPrivate;

  {
	// Write-lock the command queue.
	command_queue_wat command_queue_w(command_queue);
#ifdef SHOW_ASSERT
	// This debug code checks if we aren't calling removeRequest() twice for the same object.
	// That means that the thread calling this function already finished it, following from that
	// we just locked command_queue.
	// That leaves three options: It's still in the queue, or it was removed and is currently
	// processed by the curl thread with again two options: either it was already removed
	// from the multi session handle or not yet.

	// Find the last command added.
	command_st cmd = cmd_none;
	for (std::deque<Command>::iterator iter = command_queue_w->commands.begin(); iter != command_queue_w->commands.end(); ++iter)
	{
	  if (*iter == *this)
	  {
		cmd = iter->command();
		break;
	  }
	}
	llassert(cmd == cmd_none || cmd != cmd_remove);	// Not in queue, or last command was not a remove command.
	if (cmd == cmd_none)
	{
	  // Read-lock command_being_processed.
	  command_being_processed_rat command_being_processed_r(command_being_processed);
	  if (*command_being_processed_r == *this)
	  {
		// May not be in-between being removed from the command queue but not removed from the multi session handle yet.
		llassert(command_being_processed_r->command() != cmd_remove);
	  }
	  else
	  {
		// May not already have been removed from multi session handle as per command from the main thread (through this function thus).
		{
		  AICurlEasyRequest_wat curl_easy_request_w(*get());
		  llassert(curl_easy_request_w->active() || !curl_easy_request_w->mRemovedPerCommand);
		}
	  }
	}
	{
	  AICurlEasyRequest_wat curl_easy_request_w(*get());
	  // As soon as the lock on the command queue is released, it could be picked up by
	  // the curl thread and executed. At that point it (already) demands that the easy
	  // request either timed out or is finished. So, to avoid race conditions that already
	  // has to be true right now. The call to queued_for_removal() checks this.
	  curl_easy_request_w->queued_for_removal(curl_easy_request_w);
	}
#endif
	// Add a command to remove this request from the multi session to the command queue.
	command_queue_w->commands.push_back(Command(*this, cmd_remove));
	command_queue_w->size--;
	AICurlEasyRequest_wat curl_easy_request_w(*get());
	PerService_wat(*curl_easy_request_w->getPerServicePtr())->removed_from_command_queue(curl_easy_request_w->capability_type());	// Note really, but this has the same effect as 'added a remove command'.
	// Suppress warning that would otherwise happen if the callbacks are revoked before the curl thread removed the request.
	curl_easy_request_w->remove_queued();
  }
  // Something was added to the queue, wake up the thread to get it.
  wakeUpCurlThread();
}

//-----------------------------------------------------------------------------

namespace AICurlInterface {

LLControlGroup* sConfigGroup;

void startCurlThread(LLControlGroup* control_group)
{
  using namespace AICurlPrivate;
  using namespace AICurlPrivate::curlthread;

  llassert(is_main_thread());

  // Cache Debug Settings.
  sConfigGroup = control_group;
  curl_max_total_concurrent_connections = sConfigGroup->getU32("CurlMaxTotalConcurrentConnections");
  CurlConcurrentConnectionsPerService = (U16)sConfigGroup->getU32("CurlConcurrentConnectionsPerService");
  gNoVerifySSLCert = sConfigGroup->getBOOL("NoVerifySSLCert");
  AIPerService::setMaxPipelinedRequests(curl_max_total_concurrent_connections);
  AIPerService::setHTTPThrottleBandwidth(sConfigGroup->getF32("HTTPThrottleBandwidth"));

  AICurlThread::sInstance = new AICurlThread;
  AICurlThread::sInstance->start();
}

bool handleCurlMaxTotalConcurrentConnections(LLSD const& newvalue)
{
  using namespace AICurlPrivate;
  using namespace AICurlPrivate::curlthread;

  U32 old = curl_max_total_concurrent_connections;
  curl_max_total_concurrent_connections = newvalue.asInteger();
  AIPerService::incrementMaxPipelinedRequests(curl_max_total_concurrent_connections - old);
  llinfos << "CurlMaxTotalConcurrentConnections set to " << curl_max_total_concurrent_connections << llendl;
  return true;
}

bool handleCurlConcurrentConnectionsPerService(LLSD const& newvalue)
{
  using namespace AICurlPrivate;

  U16 new_concurrent_connections = (U16)newvalue.asInteger();
  U16 const maxCurlConcurrentConnectionsPerService = 32;
  if (new_concurrent_connections < 1 || new_concurrent_connections > maxCurlConcurrentConnectionsPerService)
  {
	sConfigGroup->setU32("CurlConcurrentConnectionsPerService", static_cast<U32>((new_concurrent_connections < 1) ? 1 : maxCurlConcurrentConnectionsPerService));
  }
  else
  {
	int increment = new_concurrent_connections - CurlConcurrentConnectionsPerService;
	CurlConcurrentConnectionsPerService = new_concurrent_connections;
	AIPerService::adjust_concurrent_connections(increment);
	llinfos << "CurlConcurrentConnectionsPerService set to " << CurlConcurrentConnectionsPerService << llendl;
  }
  return true;
}

bool handleNoVerifySSLCert(LLSD const& newvalue)
{
  gNoVerifySSLCert = newvalue.asBoolean();
  return true;
}

U32 getNumHTTPCommands(void)
{
  using namespace AICurlPrivate;

  command_queue_rat command_queue_r(command_queue);
  return command_queue_r->size;
}

U32 getNumHTTPQueued(void)
{
  return AIPerService::total_approved_queue_size();
}

U32 getNumHTTPAdded(void)
{
  return AICurlPrivate::curlthread::MultiHandle::total_added_size();
}

U32 getMaxHTTPAdded(void)
{
  return AICurlPrivate::curlthread::curl_max_total_concurrent_connections;
}

size_t getHTTPBandwidth(void)
{
  using namespace AICurlPrivate;

  U64 const sTime_40ms = get_clock_count() * curlthread::HTTPTimeout::sClockWidth_40ms;
  return BufferedCurlEasyRequest::sHTTPBandwidth.truncateData(sTime_40ms);
}

} // namespace AICurlInterface

// Global AIPerService members.
AIThreadSafeSimpleDC<AIPerService::MaxPipelinedRequests> AIPerService::sMaxPipelinedRequests;
AIThreadSafeSimpleDC<AIPerService::ThrottleFraction> AIPerService::sThrottleFraction;
LLAtomicU32 AIPerService::sHTTPThrottleBandwidth125(250000);
bool AIPerService::sNoHTTPBandwidthThrottling;

// Return Approvement if we want at least one more HTTP request for this service.
//
// It's OK if this function is a bit fuzzy, but we don't want it to return
// approvement a hundred times on a row when it is called in a tight loop.
// Hence the following consideration:
//
// If this function returns non-NULL, a Approvement object was created and
// the corresponding AIPerService::CapabilityType::mApprovedRequests was
// incremented. The Approvement object is passed to LLHTTPClient::request,
// and once the request is added to the command queue, used to update the counters.
//
// It is therefore guaranteed that in one loop of LLTextureFetchWorker::doWork,
// or LLInventoryModelBackgroundFetch::bulkFetch (the two functions currently
// calling this function) this function will stop returning aprovement once we
// reached the threshold of "pipelined" requests (the sum of approved requests,
// requests in the command queue, the ones throttled and queued in
// AIPerService::mQueuedRequests and the already running requests
// (in MultiHandle::mAddedEasyRequests)).
//
// A request has two types of reasons why it can be throttled:
// 1) The number of connections.
// 2) Bandwidth usage.
// And three levels where each can occur:
// a) Global
// b) Service
// c) Capability Type (CT)
// Currently, not all of those are in use. The ones that are used are:
//
//                               | Global | Service |   CT
//                               +--------+---------+--------
// 1) The number of connections  |   X    |    X    |    X
// 2) Bandwidth usage            |   X    |         |
//
// Pre-approved requests have the bandwidth tested here, and the
// connections tested in the curl thread, right before they are
// added to the multi handle.
//
// The "pipeline" is as follows:
//
// <approvement>				// If the number of requests in the pipeline is less than a threshold
//       |          			// and the global bandwidth usage is not too large.
//       V
// <command queue>
//       |
//       V
//  <CT queue>
//       |
//       V
// <added to multi handle>		// If the number of connections at all three levels allow it.
//       |
//       V
// <removed from multi handle>
//
// Every time this function is called, but not more often than once every 40 ms, the state
// of the CT queue is checked to be starvation, empty or full. If it is starvation
// then the threshold for allowed number of connections is incremented by one,
// if it is empty then nother is done and when it is full then the threshold is
// decremented by one.
//
// Starvation means that we could add a request from the queue to the multi handle,
// but the queue was empty. Empty means that after adding one or more requests to the
// multi handle the queue became empty, and full means that after adding one of more
// requests to the multi handle the queue still wasn't empty (see AIPerService::add_queued_to).
//
//static
AIPerService::Approvement* AIPerService::approveHTTPRequestFor(AIPerServicePtr const& per_service, AICapabilityType capability_type)
{
  using namespace AICurlPrivate;
  using namespace AICurlPrivate::curlthread;

  // Do certain things at most once every 40ms.
  U64 const sTime_40ms = get_clock_count() * HTTPTimeout::sClockWidth_40ms;							// Time in 40ms units.

  // Cache all sTotalQueued info.
  bool starvation, decrement_threshold;
  S32 total_approved_queuedapproved_or_added = MultiHandle::total_added_size();
  {
	TotalQueued_wat total_queued_w(sTotalQueued);
	total_approved_queuedapproved_or_added += total_queued_w->approved;
	starvation = total_queued_w->starvation;
	decrement_threshold = total_queued_w->full && !total_queued_w->empty;
	total_queued_w->starvation = total_queued_w->empty = total_queued_w->full = false;				// Reset flags.
  }

  // Whether or not we're going to approve a new request, decrement the global threshold first, when appropriate.

  if (decrement_threshold)
  {
	MaxPipelinedRequests_wat max_pipelined_requests_w(sMaxPipelinedRequests);
	if (max_pipelined_requests_w->threshold > (S32)curl_max_total_concurrent_connections &&
		sTime_40ms > max_pipelined_requests_w->last_decrement)
	{
	  // Decrement the threshold because since the last call to this function at least one curl request finished
	  // and was replaced with another request from the queue, but the queue never ran empty: we have too many
	  // queued requests.
	  max_pipelined_requests_w->threshold--;
	  // Do this at most once every 40 ms.
	  max_pipelined_requests_w->last_decrement = sTime_40ms;
	}
  }

  // Check if it's ok to get a new request for this particular capability type and update the per-capability-type threshold.

  bool reject, equal, increment_threshold;
  {
	PerService_wat per_service_w(*per_service);		// Keep this lock for the duration of accesses to ct.
	CapabilityType& ct(per_service_w->mCapabilityType[capability_type]);
	S32 const pipelined_requests_per_capability_type = ct.pipelined_requests();
	reject = pipelined_requests_per_capability_type >= (S32)ct.mMaxPipelinedRequests;
	equal = pipelined_requests_per_capability_type == ct.mMaxPipelinedRequests;
	increment_threshold = ct.mFlags & ctf_starvation;
	decrement_threshold = (ct.mFlags & (ctf_empty | ctf_full)) == ctf_full;
	ct.mFlags &= ~(ctf_empty|ctf_full|ctf_starvation);
	if (decrement_threshold)
	{
	  if ((int)ct.mMaxPipelinedRequests > ct.mConcurrentConnections)
	  {
		ct.mMaxPipelinedRequests--;
	  }
	}
	else if (increment_threshold && reject)
	{
	  if ((int)ct.mMaxPipelinedRequests < 2 * ct.mConcurrentConnections)
	  {
		ct.mMaxPipelinedRequests++;
		// Immediately take the new threshold into account.
		reject = !equal;
	  }
	}
	if (!reject)
	{
	  // Before releasing the lock on per_service, stop other threads from getting a
	  // too small value from pipelined_requests() and approving too many requests.
	  ct.mApprovedRequests++;
	  per_service_w->mApprovedRequests++;
	}
	total_approved_queuedapproved_or_added += per_service_w->mApprovedRequests;
  }
  if (reject)
  {
	// Too many request for this service already.
	return NULL;
  }
  
  // Throttle on bandwidth usage.
  if (checkBandwidthUsage(per_service, sTime_40ms))
  {
	// Too much bandwidth is being used, either in total or for this service.
	PerService_wat per_service_w(*per_service);
	per_service_w->mCapabilityType[capability_type].mApprovedRequests--;		// Not approved after all.
	per_service_w->mApprovedRequests--;
	return NULL;
  }

  // Check if it's ok to get a new request based on the total number of requests and increment the threshold if appropriate.

  S32 const pipelined_requests = command_queue_rat(command_queue)->size + total_approved_queuedapproved_or_added;
  // We can't take the command being processed (command_being_processed) into account without
  // introducing relatively long waiting times for some mutex (namely between when the command
  // is moved from command_queue to command_being_processed, till it's actually being added to
  // mAddedEasyRequests). The whole purpose of command_being_processed is to reduce the time
  // that things are locked to micro seconds, so we'll just accept an off-by-one fuzziness
  // here instead.

  // The maximum number of requests that may be queued in command_queue is equal to the total number of requests
  // that may exist in the pipeline minus the number approved requests not yet added to the command queue, minus the
  // number of requests queued in AIPerService objects, minus the number of already running requests
  // (excluding non-approved requests queued in their CT queue).
  MaxPipelinedRequests_wat max_pipelined_requests_w(sMaxPipelinedRequests);
  reject = pipelined_requests >= max_pipelined_requests_w->threshold;
  equal = pipelined_requests == max_pipelined_requests_w->threshold;
  increment_threshold = starvation;
  if (increment_threshold && reject)
  {
	if (max_pipelined_requests_w->threshold < 2 * (S32)curl_max_total_concurrent_connections &&
		sTime_40ms > max_pipelined_requests_w->last_increment)
	{
	  max_pipelined_requests_w->threshold++;
	  max_pipelined_requests_w->last_increment = sTime_40ms;
	  // Immediately take the new threshold into account.
	  reject = !equal;
	}
  }
  if (reject)
  {
	PerService_wat per_service_w(*per_service);
	per_service_w->mCapabilityType[capability_type].mApprovedRequests--;		// Not approved after all.
	per_service_w->mApprovedRequests--;
	return NULL;
  }
  return new Approvement(per_service, capability_type);
}

bool AIPerService::checkBandwidthUsage(AIPerServicePtr const& per_service, U64 sTime_40ms)
{
  if (sNoHTTPBandwidthThrottling)
	return false;

  using namespace AICurlPrivate;

  // Truncate the sums to the last second, and get their value.
  size_t const max_bandwidth = AIPerService::getHTTPThrottleBandwidth125();
  size_t const total_bandwidth = BufferedCurlEasyRequest::sHTTPBandwidth.truncateData(sTime_40ms);		// Bytes received in the past second.
  size_t const service_bandwidth = PerService_wat(*per_service)->bandwidth().truncateData(sTime_40ms);	// Idem for just this service.
  ThrottleFraction_wat throttle_fraction_w(sThrottleFraction);
  if (sTime_40ms > throttle_fraction_w->last_add)
  {
	throttle_fraction_w->average.addData(throttle_fraction_w->fraction, sTime_40ms);
	// Only add throttle_fraction_w->fraction once every 40 ms at most.
	// It's ok to ignore other values in the same 40 ms because the value only changes on the scale of 1 second.
	throttle_fraction_w->last_add = sTime_40ms;
  }
  double fraction_avg = throttle_fraction_w->average.getAverage(1024.0);	// throttle_fraction_w->fraction averaged over the past second, or 1024 if there is no data.

  // Adjust the fraction based on total bandwidth usage.
  if (total_bandwidth == 0)
	throttle_fraction_w->fraction = 1024;
  else
  {
	// This is the main formula. It can be made plausible by assuming
	// an equilibrium where total_bandwidth == max_bandwidth and
	// thus fraction == fraction_avg for more than a second.
	//
	// Then, more bandwidth is being used (for example because another
	// service starts downloading). Assuming that all services that use
	// a significant portion of the bandwidth, the new service included,
	// must be throttled (all using the same bandwidth; note that the
	// new service is immediately throttled at the same value), then
	// the limit should be reduced linear with the fraction:
	// max_bandwidth / total_bandwidth.
	//
	// For example, let max_bandwidth be 1. Let there be two throttled
	// services, each using 0.5 (fraction_avg = 1024/2). Let the new
	// service use what it can: also 0.5 - then without reduction the
	// total_bandwidth would become 1.5, and fraction would
	// become (1024/2) * 1/1.5 = 1024/3: from 2 to 3 services.
	//
	// In reality, total_bandwidth would rise linear from 1.0 to 1.5 in
	// one second if the throttle fraction wasn't changed. However it is
	// changed here. The end result is that any change more or less
	// linear fades away in one second.
	throttle_fraction_w->fraction = llmin(1024., fraction_avg * max_bandwidth / total_bandwidth + 0.5);
  }
  if (total_bandwidth > max_bandwidth)
  {
	throttle_fraction_w->fraction *= 0.95;
  }

  // Throttle this service if it uses too much bandwidth.
  // Warning: this requires max_bandwidth * 1024 to fit in a size_t.
  // On 32 bit that means that HTTPThrottleBandwidth must be less than 33554 kbps.
  return (service_bandwidth > (max_bandwidth * throttle_fraction_w->fraction / 1024));
}

