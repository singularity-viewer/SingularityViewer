# -*- cmake -*-

# cmake_minimum_required should appear before any
# other commands to guarantee full compatibility
# with the version specified

cmake_minimum_required(VERSION 2.6.2 FATAL_ERROR)

# Eventually the third-party support modules (cmake/*.cmake) should
# know the full path to all libraries.  Until that happens we need
# per-configuration link directory "libraries/<arch>/lib/debug" for
# Debug and "libraries/<arch>/lib/release" for Release,
# RelWithDebInfo, and MinSizeRel.  CMake 2.6 does not directly support
# per-configuration link directory specification.  However, we can set
# CMP0003 to OLD and link to one library (apr) on a per-configuration
# basis to convince CMake to add the proper link directory.  This line
# can be removed when we use full paths for all libraries.
cmake_policy(SET CMP0003 OLD)

set(ROOT_PROJECT_NAME "PartyHat" CACHE STRING
    "The root project/makefile/solution name. Defaults to SecondLife.")
project(${ROOT_PROJECT_NAME})

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(Variables)

# Load versions now.  Install locations need them.
include(Versions)

include(UnixInstall)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ReleaseSSE2 CACHE STRING
      "Build type.  One of: Debug Release ReleaseSSE2 RelWithDebInfo" FORCE)
endif (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

# Create a 'prepare' target in which to perform setup actions.  This
# must be the first target created so other targets can depend on it.
if(NOT STANDALONE)
  # We prepare prebuilt binaries when not building standalone.
  set(prepare_depends ${CMAKE_BINARY_DIR}/prepare/prebuilt)
endif(NOT STANDALONE)
add_custom_target(prepare DEPENDS ${prepare_depends})

add_subdirectory(cmake)
add_subdirectory(${LIBS_OPEN_PREFIX}cwdebug)
add_subdirectory(${LIBS_OPEN_PREFIX}llaudio)
add_subdirectory(${LIBS_OPEN_PREFIX}llcharacter)
add_subdirectory(${LIBS_OPEN_PREFIX}llcommon)
add_subdirectory(${LIBS_OPEN_PREFIX}llimage)
add_subdirectory(${LIBS_OPEN_PREFIX}libopenjpeg)
add_subdirectory(${LIBS_OPEN_PREFIX}llimagej2coj)
add_subdirectory(${LIBS_OPEN_PREFIX}llinventory)
add_subdirectory(${LIBS_OPEN_PREFIX}llmath)
add_subdirectory(${LIBS_OPEN_PREFIX}llmessage)
add_subdirectory(${LIBS_OPEN_PREFIX}llprimitive)
add_subdirectory(${LIBS_OPEN_PREFIX}llrender)
add_subdirectory(${LIBS_OPEN_PREFIX}llvfs)
add_subdirectory(${LIBS_OPEN_PREFIX}llwindow)
add_subdirectory(${LIBS_OPEN_PREFIX}llxml)

if (EXISTS ${LIBS_CLOSED_DIR}llkdu AND NOT STANDALONE)
  add_subdirectory(${LIBS_CLOSED_PREFIX}llkdu)
endif (EXISTS ${LIBS_CLOSED_DIR}llkdu AND NOT STANDALONE)

add_subdirectory(${LIBS_OPEN_PREFIX}lscript)

if (WINDOWS AND EXISTS ${LIBS_CLOSED_DIR}copy_win_scripts)
  add_subdirectory(${LIBS_CLOSED_PREFIX}copy_win_scripts)
endif (WINDOWS AND EXISTS ${LIBS_CLOSED_DIR}copy_win_scripts)

add_custom_target(viewer)
if (VIEWER)
  add_subdirectory(${LIBS_OPEN_PREFIX}llcrashlogger)
  add_subdirectory(${LIBS_OPEN_PREFIX}llplugin)
  add_subdirectory(${LIBS_OPEN_PREFIX}llui)

  # viewer plugins directory
  add_subdirectory(${LIBS_OPEN_PREFIX}plugins)

  # llplugin testbed code (is this the right way to include it?)
  #if (NOT LINUX)
  #  add_subdirectory(${VIEWER_PREFIX}test_apps/llplugintest)
  #endif (NOT LINUX)

  if (LINUX)
    add_subdirectory(${VIEWER_PREFIX}linux_crash_logger)
    add_dependencies(viewer linux-crash-logger-strip-target)
  elseif (DARWIN)
    add_subdirectory(${VIEWER_PREFIX}mac_crash_logger)
	#add_subdirectory(${VIEWER_PREFIX}mac_updater)
    add_dependencies(viewer mac-crash-logger)
	#add_dependencies(viewer mac-updater)
  elseif (WINDOWS)
    add_subdirectory(${VIEWER_PREFIX}win_crash_logger)
    # cmake EXISTS requires an absolute path, see indra/cmake/Variables.cmake
    if (EXISTS ${VIEWER_DIR}win_setup)
      add_subdirectory(${VIEWER_DIR}win_setup)
    endif (EXISTS ${VIEWER_DIR}win_setup)
    add_subdirectory(${VIEWER_PREFIX}win_updater)
    add_dependencies(viewer windows-updater)
    add_dependencies(viewer windows-crash-logger)
  elseif (SOLARIS)
    add_subdirectory(solaris_crash_logger)
    add_dependencies(viewer solaris-crash-logger)
  endif (LINUX)

  add_subdirectory(${VIEWER_PREFIX}newview/statemachine)
  add_subdirectory(${VIEWER_PREFIX}newview)
  add_dependencies(viewer secondlife-bin)
endif (VIEWER)

# Linux builds the viewer and server in 2 separate projects
# In order for ./develop.py build server to work on linux, 
# the viewer project needs a server target.
# This is not true for mac and windows.
if (LINUX) 
  add_custom_target(server)
endif (LINUX)
if (SERVER)
  if (NOT LINUX)
    add_custom_target(server)
  endif (NOT LINUX)
  include(${SERVER_PREFIX}Server.cmake)
endif (SERVER)

# Windows builds include tools like VFS tool
if (SERVER)
  if (WINDOWS)
    add_subdirectory(${SERVER_PREFIX}tools)
  endif (WINDOWS)
endif (SERVER)

# Configure prebuilt binary download.  This must be done last so that
# all subdirectories have a chance to list the packages they need.
# We need scp for proprietary downloads.
if(INSTALL_PROPRIETARY)
  include(FindSCP)
endif(INSTALL_PROPRIETARY)

# The use_prebuilt_binary macro in cmake/Prebuilt.cmake records
# packages in the PREBUILT property of the 'prepare' target.
get_property(PREBUILT_PACKAGES TARGET prepare PROPERTY PREBUILT)

# Create a script to download the needed binaries.
configure_file(${CMAKE_SOURCE_DIR}/cmake/DownloadPrebuilt.cmake.in
               ${CMAKE_BINARY_DIR}/DownloadPrebuilt.cmake @ONLY)

# Drive the download script at build time.  Depend on 'install.xml'
# to aqcuire new binaries when needed.
add_custom_command(
  COMMENT "Obtaining prebuilt binaries..."
  OUTPUT ${CMAKE_BINARY_DIR}/prepare/prebuilt
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/DownloadPrebuilt.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/../install.xml
          ${CMAKE_BINARY_DIR}/DownloadPrebuilt.cmake
  )
