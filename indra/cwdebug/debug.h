// slviewer -- Second Life Viewer Source Code
//
//! @file debug.h
//! @brief This file contains the declaration of debug related macros, objects and functions.
//
// Copyright (C) 2008, by
//
// Carlo Wood, Run on IRC <carlo@alinoe.com>
// RSA-1024 0x624ACAD5 1997-01-26                    Sign & Encrypt
// Fingerprint16 = 32 EC A7 B6 AC DB 65 A6  F6 F6 55 DD 1C DC FF 61
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef DEBUG_H
#define DEBUG_H

#ifndef CWDEBUG

#ifndef DOXYGEN         // No need to document this.  See http://libcwd.sourceforge.net/ for more info.

#include <iostream>
#include <cstdlib>      // std::exit, EXIT_FAILURE

#define AllocTag1(p)
#define AllocTag2(p, desc)
#define AllocTag_dynamic_description(p, x)
#define AllocTag(p, x)
#define Debug(x)
#define Dout(a, b)
#define DoutEntering(a, b)
#define DoutFatal(a, b) LibcwDoutFatal(::std, , a, b)
#define ForAllDebugChannels(STATEMENT)
#define ForAllDebugObjects(STATEMENT)
#define LibcwDebug(dc_namespace, x)
#define LibcwDout(a, b, c, d)
#define LibcwDoutFatal(a, b, c, d) do { ::std::cerr << d << ::std::endl; ::std::exit(EXIT_FAILURE); } while (1)
#define NEW(x) new x
#define CWDEBUG_ALLOC 0
#define CWDEBUG_MAGIC 0
#define CWDEBUG_LOCATION 0
#define CWDEBUG_LIBBFD 0
#define CWDEBUG_DEBUG 0
#define CWDEBUG_DEBUGOUTPUT 0
#define CWDEBUG_DEBUGM 0
#define CWDEBUG_DEBUGT 0
#define CWDEBUG_MARKER 0

#define BACKTRACE do { } while(0)

#endif // !DOXYGEN

#include <cassert>
#ifdef DEBUG
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif

#else // CWDEBUG

//! Assert \a x, if debugging is turned on.
#define ASSERT(x) LIBCWD_ASSERT(x)
#define builtin_return_address(addr) ((char*)__builtin_return_address(addr) + libcwd::builtin_return_address_offset)

#ifndef DEBUGCHANNELS
//! @brief The namespace in which the \c dc namespace is declared.
//
// <A HREF="http://libcwd.sourceforge.net/">Libcwd</A> demands that this macro is defined
// before <libcwd/debug.h> is included and must be the name of the namespace containing
// the \c dc (Debug Channels) namespace.
//
// @sa debug::channels::dc

#define DEBUGCHANNELS ::debug::channels
#endif
#include <libcwd/debug.h>
#include <boost/shared_array.hpp>
#if CWDEBUG_LOCATION
#include <execinfo.h>		// Needed for 'backtrace'.
#endif

#define CWD_API __attribute__ ((visibility("default")))

//! Debug specific code.
namespace debug {

void CWD_API init(void);                // Initialize debugging code, called once from main.
void CWD_API init_thread(void);         // Initialize debugging code, called once for each thread.

//! @brief Debug Channels (dc) namespace.
//
// @sa debug::channels::dc
namespace channels {	// namespace DEBUGCHANNELS

//! The namespace containing the actual debug channels.
namespace dc {
using namespace libcwd::channels::dc;
using libcwd::channel_ct;

#ifndef DOXYGEN         // Doxygen bug causes a warning here.
// Add the declaration of new debug channels here
// and add their definition in a custom debug.cc file.
extern CWD_API channel_ct viewer;	// The normal logging output of the viewer (normally to stderr).
extern CWD_API channel_ct primbackup;
extern CWD_API channel_ct gtk;
extern CWD_API channel_ct sdl;
extern CWD_API channel_ct backtrace;
extern CWD_API channel_ct statemachine;

#endif

} // namespace dc
} // namespace DEBUGCHANNELS

#if CWDEBUG_LOCATION
std::string call_location(void const* return_addr);
#endif

//! @brief Interface for marking scopes of invisible memory allocations.
//
// Creation of the object does nothing, you have to explicitly call
// InvisibleAllocations::on.  Destruction of the object automatically
// cancels any call to \c on of this object.  This makes it exception-
// (stack unwinding) and recursive-safe.
struct InvisibleAllocations {
  int M_on;             //!< The number of times that InvisibleAllocations::on() was called.
  //! Constructor.
  InvisibleAllocations() : M_on(0) { }
  //! Destructor.
  ~InvisibleAllocations() { while (M_on > 0) off(); }
  //! Set invisible allocations on. Can be called recursively.
  void on(void) { libcwd::set_invisible_on(); ++M_on; }
  //! Cancel one call to on().
  void off(void) { assert(M_on > 0); --M_on; libcwd::set_invisible_off(); }
};

//! @brief Interface for marking scopes with indented debug output.
//
// Creation of the object increments the debug indentation. Destruction
// of the object automatically decrements the indentation again.
struct Indent {
  int M_indent;                 //!< The extra number of spaces that were added to the indentation.
  //! Construct an Indent object.
  Indent(int indent) : M_indent(indent) { if (M_indent > 0) libcwd::libcw_do.inc_indent(M_indent); }
  //! Destructor.
  ~Indent() { if (M_indent > 0) libcwd::libcw_do.dec_indent(M_indent); }
};

// A class streambuf that splits what is written to two streambufs.
class TeeBuf : public std::streambuf {
  private:
    std::streambuf* M_sb1;
    std::streambuf* M_sb2;

  public:
    TeeBuf(std::streambuf* sb1, std::streambuf* sb2) : M_sb1(sb1), M_sb2(sb2) { }

  protected:
    virtual int sync(void) { M_sb2->pubsync(); return M_sb1->pubsync(); }
    virtual std::streamsize xsputn(char_type const* p, std::streamsize n) { M_sb2->sputn(p, n); return M_sb1->sputn(p, n); }
    virtual int_type overflow(int_type c = traits_type::eof()) { M_sb2->sputc(c); return M_sb1->sputc(c); }
};

// An ostream that passes what is written to it on to two other ostreams.
class TeeStream : public std::ostream {
  private:
    TeeBuf M_teebuf;
  public:
    TeeStream(std::ostream& os1, std::ostream& os2) : std::ostream(&M_teebuf), M_teebuf(os1.rdbuf(), os2.rdbuf()) { }
};

#if CWDEBUG_LOCATION
class BackTrace {
  private:
    boost::shared_array<void*> M_buffer;
    int M_frames;
  public:
    BackTrace(void** buffer, int frames) : M_buffer(new void* [frames]), M_frames(frames) { std::memcpy(M_buffer.get(), buffer, sizeof(void*) * frames); }

    friend bool operator<(BackTrace const& bt1, BackTrace const& bt2)
    {
      if (bt1.M_frames != bt2.M_frames)
	return bt1.M_frames < bt2.M_frames;
      for (int frame = 0; frame < bt1.M_frames; ++frame)
	if (bt1.M_buffer[frame] < bt2.M_buffer[frame])
	  return true;
        else if (bt1.M_buffer[frame] > bt2.M_buffer[frame])
	  return true;
      return false;
    }

  void dump_backtrace(void) const;

  int frames(void) const { return M_frames; }
  boost::shared_array<void*> const& buffer(void) const { return M_buffer; }
};
extern std::vector<BackTrace> backtraces;
extern pthread_mutex_t backtrace_mutex;
#define BACKTRACE do { \
    using namespace debug; \
    void* buffer[32]; \
    int frames = backtrace(buffer, 32); \
    size_t size; \
    { \
      pthread_mutex_lock(&backtrace_mutex); \
      backtraces.push_back(BackTrace(buffer, frames)); \
      size = backtraces.size(); \
      pthread_mutex_unlock(&backtrace_mutex); \
    } \
    Dout(dc::backtrace, "Stored backtrace #" << size); \
  } while(0)
#else
#define BACKTRACE do { } while(0)
#endif // CWDEBUG_LOCATION

} // namespace debug

//! Debugging macro.
//
// Print "Entering " << \a data to channel \a cntrl and increment
// debugging output indentation until the end of the current scope.
#define DoutEntering(cntrl, data) \
  int __slviewer_debug_indentation = 2;                                                                      \
  {                                                                                                                     \
    LIBCWD_TSD_DECLARATION;                                                                                             \
    if (LIBCWD_DO_TSD_MEMBER_OFF(::libcwd::libcw_do) < 0)                                                               \
    {                                                                                                                   \
      ::libcwd::channel_set_bootstrap_st __libcwd_channel_set(LIBCWD_DO_TSD(::libcwd::libcw_do) LIBCWD_COMMA_TSD);      \
      bool on;                                                                                                          \
      {                                                                                                                 \
        using namespace LIBCWD_DEBUGCHANNELS;                                                                           \
        on = (__libcwd_channel_set|cntrl).on;                                                                           \
      }                                                                                                                 \
      if (on)                                                                                                           \
        Dout(cntrl, "Entering " << data);                                                                               \
      else                                                                                                              \
        __slviewer_debug_indentation = 0;                                                                    \
    }                                                                                                                   \
  }                                                                                                                     \
  debug::Indent __slviewer_debug_indent(__slviewer_debug_indentation);

#endif // CWDEBUG

#include "debug_ostream_operators.h"

#endif // DEBUG_H
