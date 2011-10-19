/** 
 * @file llprocesslauncher.cpp
 * @brief Utility class for launching, terminating, and tracking the state of processes.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include <cassert>
#include <apr_file_io.h>
#include <apr_thread_proc.h>
#include "llprocesslauncher.h"
#include "llaprpool.h"

#include <iostream>
#if LL_DARWIN || LL_LINUX
// not required or present on Win32
#include <sys/wait.h>
#endif

LLProcessLauncher::LLProcessLauncher()
{
#if LL_WINDOWS
	mProcessHandle = 0;
#else
	mProcessID = 0;
#endif
}

LLProcessLauncher::~LLProcessLauncher()
{
	kill();
}

void LLProcessLauncher::setExecutable(const std::string &executable)
{
	mExecutable = executable;
}

void LLProcessLauncher::setWorkingDirectory(const std::string &dir)
{
	mWorkingDir = dir;
}

const std::string& LLProcessLauncher::getExecutable() const
{
	return mExecutable;
}

void LLProcessLauncher::clearArguments()
{
	mLaunchArguments.clear();
}

void LLProcessLauncher::addArgument(const std::string &arg)
{
	mLaunchArguments.push_back(arg);
}

void LLProcessLauncher::addArgument(const char *arg)
{
	mLaunchArguments.push_back(std::string(arg));
}

#if LL_WINDOWS

int LLProcessLauncher::launch(void)
{
	// If there was already a process associated with this object, kill it.
	kill();
	orphan();

	int result = 0;
	
	PROCESS_INFORMATION pinfo;
	STARTUPINFOA sinfo;
	memset(&sinfo, 0, sizeof(sinfo));
	
	std::string args = mExecutable;
	for(int i = 0; i < (int)mLaunchArguments.size(); i++)
	{
		args += " ";
		args += mLaunchArguments[i];
	}
	LL_INFOS("Plugin") << "Executable: " << mExecutable << " arguments: " << args << LL_ENDL;
	
	// So retarded.  Windows requires that the second parameter to CreateProcessA be a writable (non-const) string...
	char *args2 = new char[args.size() + 1];
	strcpy(args2, args.c_str());

	const char * working_directory = 0;
	if(!mWorkingDir.empty()) working_directory = mWorkingDir.c_str();
	if( ! CreateProcessA( NULL, args2, NULL, NULL, FALSE, 0, NULL, working_directory, &sinfo, &pinfo ) )
	{
		result = GetLastError();

		LPTSTR error_str = 0;
		if(
			FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				result,
				0,
				(LPTSTR)&error_str,
				0,
				NULL) 
			!= 0) 
		{
			char message[256];
			wcstombs(message, error_str, 256);
			message[255] = 0;
			llwarns << "CreateProcessA failed: " << message << llendl;
			LocalFree(error_str);
		}

		if(result == 0)
		{
			// Make absolutely certain we return a non-zero value on failure.
			result = -1;
		}
	}
	else
	{
		// foo = pinfo.dwProcessId; // get your pid here if you want to use it later on
		// CloseHandle(pinfo.hProcess); // stops leaks - nothing else
		mProcessHandle = pinfo.hProcess;
		CloseHandle(pinfo.hThread); // stops leaks - nothing else
	}		
	
	delete[] args2;
	
	return result;
}

bool LLProcessLauncher::isRunning(void)
{
	if(mProcessHandle != 0)		
	{
		DWORD waitresult = WaitForSingleObject(mProcessHandle, 0);
		if(waitresult == WAIT_OBJECT_0)
		{
			// the process has completed.
			mProcessHandle = 0;
		}			
	}

	return (mProcessHandle != 0);
}

bool LLProcessLauncher::kill(void)
{
	bool result = true;
	
	if(mProcessHandle != 0)
	{
		TerminateProcess(mProcessHandle,0);

		if(isRunning())
		{
			result = false;
		}
	}
	
	return result;
}

void LLProcessLauncher::orphan(void)
{
	// Forget about the process
	mProcessHandle = 0;
}

// static 
void LLProcessLauncher::reap(void)
{
	// No actions necessary on Windows.
}

#else // Mac and linux

#include <signal.h>
#include <fcntl.h>
#include <errno.h>

static std::list<pid_t> sZombies;

// Attempt to reap a process ID -- returns true if the process has exited and been reaped, false otherwise.
static bool reap_pid(pid_t pid)
{
	bool result = false;
	
	pid_t wait_result = ::waitpid(pid, NULL, WNOHANG);
	if(wait_result == pid)
	{
		result = true;
	}
	else if(wait_result == -1)
	{
		if(errno == ECHILD)
		{
			// No such process -- this may mean we're ignoring SIGCHILD.
			result = true;
		}
	}
	
	return result;
}

#if LL_DEBUG
// Define this to create a temporary pipe(2) between parent and child process, so
// that the child process can report error messages that it encounters when
// trying to execve(2). Most notably failing to start due to missing libraries
// or undefined symbols.
#define DEBUG_PIPE_CHILD_ERROR_REPORTING 1
#endif

#ifdef DEBUG_PIPE_CHILD_ERROR_REPORTING

// Called by child process.
static void write_pipe(apr_file_t* out, char const* message)
{
	apr_size_t const bytes_to_write = strlen(message) + 1;	// +1 for the length byte.
	assert(bytes_to_write < 256);
	char buf[256];
	strncpy(buf + 1, message, sizeof(buf) - 1);
	*reinterpret_cast<unsigned char*>(buf) = bytes_to_write - 1;

	apr_size_t bytes_written;
	apr_status_t status = apr_file_write_full(out, buf, bytes_to_write, &bytes_written);
	if (status != APR_SUCCESS)
	{
		std::cerr << "apr_file_write_full: " << apr_strerror(status, buf, sizeof(buf)) << std::endl;
	}
	else if (bytes_written != bytes_to_write)
	{
		std::cerr << "apr_file_write_full: bytes_written (" << bytes_written << ") != bytes_to_write (" << bytes_to_write << ")!" << std::endl;
	}
	status = apr_file_flush(out);
	if (status != APR_SUCCESS)
	{
		std::cerr << "apr_file_flush: " << apr_strerror(status, buf, sizeof(buf)) << std::endl;
	}

#ifdef _DEBUG
	std::cerr << "apr_file_write_full: Wrote " << bytes_written << " bytes to the pipe." << std::endl;
#endif
}

// Called by parent process.
static std::string read_pipe(apr_file_t* in, bool timeout_ok = false)
{
	char buf[256];
	unsigned char bytes_to_read;
	apr_size_t bytes_read;
	apr_status_t status = apr_file_read_full(in, &bytes_to_read, 1, &bytes_read);
	if (status != APR_SUCCESS)
	{
		if (APR_STATUS_IS_TIMEUP(status) && timeout_ok)
		{
			return "TIMEOUT";
		}
		llwarns << "apr_file_read_full: " << apr_strerror(status, buf, sizeof(buf)) << llendl;
		assert(APR_STATUS_IS_EOF(status));
		return "END OF FILE";
	}
	assert(bytes_read == 1);
	status = apr_file_read_full(in, buf, bytes_to_read, &bytes_read);
	if (status != APR_SUCCESS)
	{
		llwarns << "apr_file_read_full: " << apr_strerror(status, buf, sizeof(buf)) << llendl;
		assert(status == APR_SUCCESS);	// Fail
	}
	assert(bytes_read == bytes_to_read);

	std::string received(buf, bytes_read);
	llinfos << "Received: \"" << received << "\" (bytes read: " << bytes_read << ")" << llendl;
	return received;
}

#endif // DEBUG_PIPE_CHILD_ERROR_REPORTING

int LLProcessLauncher::launch(void)
{
	// If there was already a process associated with this object, kill it.
	kill();
	orphan();
	
	int result = 0;
	int current_wd = -1;
	
	// create an argv vector for the child process
	const char ** fake_argv = new const char *[mLaunchArguments.size() + 2];  // 1 for the executable path, 1 for the NULL terminator

	int i = 0;
	
	// add the executable path
	fake_argv[i++] = mExecutable.c_str();
	
	// and any arguments
	for(int j=0; j < mLaunchArguments.size(); j++)
		fake_argv[i++] = mLaunchArguments[j].c_str();
	
	// terminate with a null pointer
	fake_argv[i] = NULL;
	
	if(!mWorkingDir.empty())
	{
		// save the current working directory
		current_wd = ::open(".", O_RDONLY);
	
		// and change to the one the child will be executed in
		if (::chdir(mWorkingDir.c_str()))
		{
			// chdir failed
		}
	}
		
	pid_t id;
	{
#ifdef DEBUG_PIPE_CHILD_ERROR_REPORTING
		// Set up a pipe to the child process for error reporting.
		apr_file_t* in;
		apr_file_t* out;
		LLAPRPool pool;
		pool.create();
#if(APR_VERSION_MAJOR==1 && APR_VERSION_MINOR>=3 || APR_VERSION_MAJOR>1)
		apr_status_t status = apr_file_pipe_create_ex(&in, &out, APR_FULL_BLOCK, pool());
#else
		apr_status_t status = apr_file_pipe_create(&in, &out, pool());
#endif
		assert(status == APR_SUCCESS);
		bool success = (status == APR_SUCCESS);
		if (success)
		{
			apr_interval_time_t const timeout = 10000000;	// 10 seconds.
			status = apr_file_pipe_timeout_set(in, timeout);
			assert(status == APR_SUCCESS);
			success = (status == APR_SUCCESS);
		}
#endif // DEBUG_PIPE_CHILD_ERROR_REPORTING

		// flush all buffers before the child inherits them
		::fflush(NULL);

		id = vfork();
		if (id == 0)
		{
			// child process

#ifdef DEBUG_PIPE_CHILD_ERROR_REPORTING
			// Tell parent process we're about to call execv.
			write_pipe(out, "CALLING EXECV");
#ifdef _DEBUG
			char const* display = getenv("DISPLAY");
			std::cerr << "Calling ::execv(\"" << mExecutable << '"';
			for(int j = 0; j < i; ++j)
              std::cerr << ", \"" << fake_argv[j] << '"';
			std::cerr << ") with DISPLAY=\"" << (display ? display : "NULL") << '"' << std::endl;
#endif
#endif // DEBUG_PIPE_CHILD_ERROR_REPORTING

			::execv(mExecutable.c_str(), (char * const *)fake_argv);

#ifdef DEBUG_PIPE_CHILD_ERROR_REPORTING
			status = APR_FROM_OS_ERROR(apr_get_os_error());
			char message[256];
			char errbuf[128];
			apr_strerror(status, errbuf, sizeof(errbuf));
			snprintf(message, sizeof(message), "Child process: execv: %s: %s", mExecutable.c_str(), errbuf);
			write_pipe(out, message);
#ifdef _DEBUG
			std::cerr << "::execv() failed." << std::endl;
#endif
#endif // DEBUG_PIPE_CHILD_ERROR_REPORTING

			// If we reach this point, the exec failed.
			// Use _exit() instead of exit() per the vfork man page.
			_exit(0);
		}

		// parent process

#ifdef DEBUG_PIPE_CHILD_ERROR_REPORTING
		// Close unused pipe end.
		apr_file_close(out);

		if (success)
		{
			// Attempt to do error reporting.
			std::string message = read_pipe(in);
			success = (message == "CALLING EXECV");
			assert(success);
			if (success)
			{
				status = apr_file_pipe_timeout_set(in, 2000000);	// Only wait 2 seconds.
				message = read_pipe(in, true);
				if (message != "TIMEOUT" && message != "END OF FILE")
				{
					// Most likely execv failed.
					llwarns << message << llendl;
					assert(false);	// Fail in debug mode.
				}
			}
		}

		// Clean up.
		apr_file_close(in);
#endif // DEBUG_PIPE_CHILD_ERROR_REPORTING

	}

	if(current_wd >= 0)
	{
		// restore the previous working directory
		if (::fchdir(current_wd))
		{
			// chdir failed
		}
		::close(current_wd);
	}
	
	delete[] fake_argv;
	
	mProcessID = id;
	
	// At this point, the child process will have been created (since that's how vfork works -- the child borrowed our execution context until it forked)
	// If the process doesn't exist at this point, the exec failed.
	if(!isRunning())
	{
		result = -1;
	}
	
	return result;
}

bool LLProcessLauncher::isRunning(void)
{
	if(mProcessID != 0)
	{
		// Check whether the process has exited, and reap it if it has.
		if(reap_pid(mProcessID))
		{
			// the process has exited.
			mProcessID = 0;
		}
	}
	
	return (mProcessID != 0);
}

bool LLProcessLauncher::kill(void)
{
	bool result = true;
	
	if(mProcessID != 0)
	{
		// Try to kill the process.  We'll do approximately the same thing whether the kill returns an error or not, so we ignore the result.
		(void)::kill(mProcessID, SIGTERM);
		
		// This will have the side-effect of reaping the zombie if the process has exited.
		if(isRunning())
		{
			result = false;
		}
	}
	
	return result;
}

void LLProcessLauncher::orphan(void)
{
	// Disassociate the process from this object
	if(mProcessID != 0)
	{	
		// We may still need to reap the process's zombie eventually
		sZombies.push_back(mProcessID);
	
		mProcessID = 0;
	}
}

// static 
void LLProcessLauncher::reap(void)
{
	// Attempt to real all saved process ID's.
	
	std::list<pid_t>::iterator iter = sZombies.begin();
	while(iter != sZombies.end())
	{
		if(reap_pid(*iter))
		{
			iter = sZombies.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}

#endif
