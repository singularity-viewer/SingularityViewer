// slviewer -- Second Life Viewer Source Code
//
//! @file debug.cc
//! @brief This file contains the definitions of debug related objects and functions.
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

#ifndef USE_PCH
#include "sys.h"                        // Needed for platform-specific code
#endif

#ifdef CWDEBUG

#ifndef USE_PCH
#include <cctype>                       // Needed for std::isprint
#include <iomanip>                      // Needed for setfill and setw
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include "debug.h"
#ifdef USE_LIBCW
#include <libcw/memleak.h>		// memleak_filter
#endif
#include <sys/types.h>
#include <unistd.h>
#endif // USE_PCH

#define BACKTRACE_AQUIRE_LOCK           libcwd::_private_::mutex_tct<libcwd::_private_::backtrace_instance>::lock()
#define BACKTRACE_RELEASE_LOCK          libcwd::_private_::mutex_tct<libcwd::_private_::backtrace_instance>::unlock()

namespace debug {

#if CWDEBUG_LOCATION
void BackTrace::dump_backtrace(void) const
{
  for (int frame = 0; frame < frames(); ++frame)
  {
    libcwd::location_ct loc((char*)buffer()[frame] + libcwd::builtin_return_address_offset);
    Dout(dc::notice|continued_cf, '#' << std::left << std::setw(3) << frame <<
	std::left << std::setw(16) << buffer()[frame] << ' ' << loc << "\n                  in ");
    char const* mangled_function_name = loc.mangled_function_name();
    if (mangled_function_name != libcwd::unknown_function_c)
    {
      std::string demangled_function_name;
      libcwd::demangle_symbol(mangled_function_name, demangled_function_name);
      Dout(dc::finish, demangled_function_name);
    }
    else
      Dout(dc::finish, mangled_function_name);
  }
}
#endif // CWDEBUG_LOCATION

#if CWDEBUG_ALLOC && CWDEBUG_LOCATION
typedef std::map<BackTrace, int, std::less<BackTrace>, libcwd::_private_::internal_allocator> backtrace_map_t;
backtrace_map_t* backtrace_map;
static int total_calls = 0;
static int number_of_stack_traces = 0;

void my_backtrace_hook(void** buffer, int frames LIBCWD_COMMA_TSD_PARAM)
{
  ++total_calls;

  backtrace_map_t::iterator iter;

  set_alloc_checking_off(__libcwd_tsd);
  {
    BackTrace backtrace(buffer, frames);
    std::pair<backtrace_map_t::iterator, bool> res = backtrace_map->insert(backtrace_map_t::value_type(backtrace, 0));
    if (res.second)
      ++number_of_stack_traces;
    ++res.first->second;
    iter = res.first;
  }
  set_alloc_checking_on(__libcwd_tsd);
#if 0
  // Dump the stack trace.
  iter->first.dump_backtrace();
#endif
}

void start_recording_backtraces(void)
{
  BACKTRACE_AQUIRE_LOCK;
  libcwd::backtrace_hook = my_backtrace_hook;
  BACKTRACE_RELEASE_LOCK;
  //Debug(dc::malloc.on());
  LIBCWD_TSD_DECLARATION;
  set_alloc_checking_off(__libcwd_tsd);
  backtrace_map = new backtrace_map_t;
  set_alloc_checking_on(__libcwd_tsd);
}

struct Compare {
  bool operator()(backtrace_map_t::const_iterator const& iter1, backtrace_map_t::const_iterator const& iter2)
  {
    return iter1->second > iter2->second;
  }
};

void stop_recording_backtraces(void)
{
  //Debug(dc::malloc.off());
  BACKTRACE_AQUIRE_LOCK;
  libcwd::backtrace_hook = NULL;

  if (!backtrace_map)
  {
    Dout(dc::notice, "Not recording; call cwdebug_start() first.");
    return;
  }

  Dout(dc::notice, "Total number of calls: " << total_calls);
  Dout(dc::notice, "Number of different stack traces: " << number_of_stack_traces);
  Dout(dc::notice, "First 10 stack traces:");
  std::list<backtrace_map_t::const_iterator> entries;
  for (backtrace_map_t::const_iterator iter = backtrace_map->begin(); iter != backtrace_map->end(); ++iter)
    entries.push_back(iter);
  entries.sort(Compare());
  int count = 0;
  for (std::list<backtrace_map_t::const_iterator>::iterator iter = entries.begin(); iter != entries.end(); ++iter, ++count)
  {
    Dout(dc::notice, "Used: " << (*iter)->second);
    // Dump the stack trace.
    (*iter)->first.dump_backtrace();
    if (count == 10)
      break;
  }

  // Clear all data.
  LIBCWD_TSD_DECLARATION;
  set_alloc_checking_off(__libcwd_tsd);
  delete backtrace_map;
  set_alloc_checking_on(__libcwd_tsd);
  backtrace_map = NULL;
  total_calls = 0;
  number_of_stack_traces = 0;

  BACKTRACE_RELEASE_LOCK;
}
#endif // CWDEBUG_ALLOC && CWDEBUG_LOCATION

  namespace channels {	// namespace DEBUGCHANNELS
    namespace dc {

#ifndef DOXYGEN
#define DDCN(x) (x)
#endif
      // Add new debug channels here.

      channel_ct viewer DDCN("VIEWER");		//!< This debug channel is used for the normal debugging out of the viewer.
      channel_ct primbackup DDCN("PRIMBACKUP");	//!< This debug channel is used for output related to primbackup.
      channel_ct gtk DDCN("GTK");		//!< This debug channel is used for output related to gtk.
      channel_ct sdl DDCN("SDL");		//!< This debug channel is used for output related to sdl locking.
      channel_ct backtrace DDCN("BACKTRACE");	//!< This debug channel is used for backtraces.
      channel_ct statemachine DDCN("STATEMACHINE");	//!< This debug channel is used class AIStateMachine.

    } // namespace dc
  } // namespace DEBUGCHANNELS

  // Anonymous namespace, this map and its initialization functions are private to this file
  // for Thead-safeness reasons.
  namespace {

    /*! @brief The type of rcfile_dc_states.
     * @internal
     */
    typedef std::map<std::string, bool> rcfile_dc_states_type;

    /*! @brief Map containing the default debug channel states used at the start of each new thread.
     * @internal
     *
     * The first thread calls main, which calls debug::init which will initialize this
     * map with all debug channel labels and whether or not they were turned on in the
     * rcfile or not.
     */
    rcfile_dc_states_type rcfile_dc_states;

    /*! @brief Set the default state of debug channel \a dc_label.
     * @internal
     *
     * This function is called once for each debug channel.
     */
    void set_state(char const* dc_label, bool is_on)
    {
      std::pair<rcfile_dc_states_type::iterator, bool> res =
          rcfile_dc_states.insert(rcfile_dc_states_type::value_type(std::string(dc_label), is_on));
      if (!res.second)
        Dout(dc::warning, "Calling set_state() more than once for the same label!");
      return;
    }

    /*! @brief Save debug channel states.
     * @internal
     *
     * One time initialization function of rcfile_dc_state.
     * This must be called from debug::init after reading the rcfile.
     */
    void save_dc_states(void)
    {
      // We may only call this function once: it reflects the states as stored
      // in the rcfile and that won't change.  Therefore it is not needed to
      // lock `rcfile_dc_states', it is only written to by the first thread
      // (once, via main -> init) when there are no other threads yet.
      static bool second_time = false;
      if (second_time)
      {
        Dout(dc::warning, "Calling save_dc_states() more than once!");
	return;
      }
      second_time = true;
      ForAllDebugChannels( set_state(debugChannel.get_label(), debugChannel.is_on()) );
    }

  } // anonymous namespace

  /*! @brief Returns the the original state of a debug channel.
   * @internal
   *
   * For a given \a dc_label, which must be the exact name (<tt>channel_ct::get_label</tt>) of an
   * existing debug channel, this function returns \c true when the corresponding debug channel was
   * <em>on</em> at the startup of the application, directly after reading the libcwd runtime
   * configuration file (.libcwdrc).
   *
   * If the label/channel did not exist at the start of the application, it will return \c false
   * (note that libcwd disallows adding debug channels to modules - so this would probably
   * a bug).
   */
  bool is_on_in_rcfile(char const* dc_label)
  {
    rcfile_dc_states_type::const_iterator iter = rcfile_dc_states.find(std::string(dc_label));
    if (iter == rcfile_dc_states.end())
    {
      Dout(dc::warning, "is_on_in_rcfile(\"" << dc_label << "\"): \"" << dc_label << "\" is an unknown label!");
      return false;
    }
    return iter->second;
  }

#if LIBCWD_THREAD_SAFE
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  // I can cause I'm the maintainer of libcwd ;).
  libcwd::_private_::pthread_lock_interface_ct cout_mutex(&mutex);
  libcwd::_private_::lock_interface_base_ct* cout_mutex_ptr(&cout_mutex);
#endif

  /*! @brief Initialize debugging code from new threads.
   *
   * This function needs to be called at the start of each new thread,
   * because a new thread starts in a completely reset state.
   *
   * The function turns on all debug channels that were turned on
   * after reading the rcfile at the start of the application.
   * Furthermore it initializes the debug ostream, its mutex and the
   * margin of the default debug object (Dout).
   */
  void init_thread(void)
  {
    // Turn on all debug channels that are turned on as per rcfile configuration.
    ForAllDebugChannels(
        if (!debugChannel.is_on() && is_on_in_rcfile(debugChannel.get_label()))
	  debugChannel.on();
    );

    // Turn on debug output.
    Debug( libcw_do.on() );
    static std::ofstream debug_file;
    static bool debug_out_opened = false;
    if (!debug_out_opened)
    {
      debug_out_opened = true;		// Thread-safe, cause the main thread calls this first while it's still alone.
      std::ostringstream os;
      os << "debug.out." << getpid();
      debug_file.open(os.str().c_str());
    }
    static debug::TeeStream debug_stream(std::cout, debug_file);
#if LIBCWD_THREAD_SAFE
    Debug( libcw_do.set_ostream(&debug_stream, cout_mutex_ptr) );
#else
    Debug( libcw_do.set_ostream(&debug_stream) );
#endif

    static bool first_thread = true;
    if (!first_thread)			// So far, the application has only one thread.  So don't add a thread id.
    {
      // Set the thread id in the margin.
      char margin[18];
      sprintf(margin, "0x%-14lx ", pthread_self());
      Debug( libcw_do.margin().assign(margin, 17) );
    }
    first_thread = false;
  }

  /*! @brief Initialize debugging code from main.
   *
   * This function initializes the debug code.
   */
  void init(void)
  {
#if CWDEBUG_ALLOC && defined(USE_LIBCW)
    // Tell the memory leak detector which parts of the code are
    // expected to leak so that we won't get an alarm for those.
    {
      std::vector<std::pair<std::string, std::string> > hide_list;
      hide_list.push_back(std::pair<std::string, std::string>("libdl.so.2", "_dlerror_run"));
      hide_list.push_back(std::pair<std::string, std::string>("libstdc++.so.6", "__cxa_get_globals"));
      // The following is actually necessary because of a bug in glibc
      // (see http://sources.redhat.com/bugzilla/show_bug.cgi?id=311).
      hide_list.push_back(std::pair<std::string, std::string>("libc.so.6", "dl_open_worker"));
      memleak_filter().hide_functions_matching(hide_list);
    }
    {
      std::vector<std::string> hide_list;
      // Also because of http://sources.redhat.com/bugzilla/show_bug.cgi?id=311
      hide_list.push_back(std::string("ld-linux.so.2"));
      memleak_filter().hide_objectfiles_matching(hide_list);
    }
    memleak_filter().set_flags(libcwd::show_objectfile|libcwd::show_function);
#endif

    // The following call allocated the filebuf's of cin, cout, cerr, wcin, wcout and wcerr.
    // Because this causes a memory leak being reported, make them invisible.
    Debug(set_invisible_on());

    // You want this, unless you mix streams output with C output.
    // Read  http://gcc.gnu.org/onlinedocs/libstdc++/27_io/howto.html#8 for an explanation.
    //std::ios::sync_with_stdio(false);

    // Cancel previous call to set_invisible_on.
    Debug(set_invisible_off());

    // This will warn you when you are using header files that do not belong to the
    // shared libcwd object that you linked with.
    Debug( check_configuration() );

    Debug(
      libcw_do.on();		// Show which rcfile we are reading!
      ForAllDebugChannels(
        while (debugChannel.is_on())
	  debugChannel.off()	// Print as little as possible though.
      );
      read_rcfile();		// Put 'silent = on' in the rcfile to suppress most of the output here.
      libcw_do.off()
    );
    save_dc_states();

    init_thread();
  }

#if CWDEBUG_LOCATION
  /*! @brief Return call location.
   *
   * @param return_addr The return address of the call.
   */
  std::string call_location(void const* return_addr)
  {
    libcwd::location_ct loc((char*)return_addr + libcwd::builtin_return_address_offset);
    std::ostringstream convert;
    convert << loc;
    return convert.str();
  }

  std::vector<BackTrace> __attribute__ ((visibility("default"))) backtraces;
  pthread_mutex_t __attribute__ ((visibility("default"))) backtrace_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

} // namespace debug

#if CWDEBUG_ALLOC
// These can be called from gdb.
void cwdebug_start()
{
  debug::start_recording_backtraces();
}

void cwdebug_stop()
{
  debug::stop_recording_backtraces();
}
#endif

#if CWDEBUG_LOCATION
void cwdebug_backtrace(int n)
{
  if (0 < n && n <= debug::backtraces.size())
  {
    Dout(dc::backtrace, "Backtrace #" << n << ":");
    debug::backtraces[n - 1].dump_backtrace();
  }
  else
    std::cout << "No such backtrace nr. Max is " << debug::backtraces.size() << std::endl;
}
#endif

#endif // CWDEBUG
