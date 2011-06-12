# -*- cmake -*-
include(Prebuilt)

set(Boost_FIND_QUIETLY ON)
set(Boost_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindBoost)

  set(BOOST_FILESYSTEM_LIBRARY boost_filesystem-mt)
  set(BOOST_PROGRAM_OPTIONS_LIBRARY boost_program_options-mt)
  set(BOOST_REGEX_LIBRARY boost_regex-mt)
  set(BOOST_SYSTEM_LIBRARY boost_system-mt)  
else (STANDALONE)
  use_prebuilt_binary(boost)
  set(Boost_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)

  if (WINDOWS)
    set(BOOST_VERSION 1_45)

	# SNOW-788
	# 00-Common.cmake alreay sets MSVC_SUFFIX to be correct for the VS we are using eg VC71, VC80, VC90 etc
	# The precompiled boost libs for VC71 use a different suffix to VS80 and VS90
	# This code should ensure the cmake rules are valid for any VS being used in future as long as the approprate
	# boost libs are avaiable - RC.
	
	if (MSVC71)
		set(BOOST_OPTIM_SUFFIX mt-s)
	    set(BOOST_DEBUG_SUFFIX mt-sgd)
	else (MSVC71)
	    set(BOOST_OPTIM_SUFFIX mt)
	    set(BOOST_DEBUG_SUFFIX mt-gd)
	endif (MSVC71)
		
	set(BOOST_PROGRAM_OPTIONS_LIBRARY 
          optimized libboost_program_options-vc${MSVC_SUFFIX}-${BOOST_OPTIM_SUFFIX}-${BOOST_VERSION}
          debug libboost_program_options-vc${MSVC_SUFFIX}-${BOOST_DEBUG_SUFFIX}-${BOOST_VERSION})
    set(BOOST_REGEX_LIBRARY
          optimized libboost_regex-vc${MSVC_SUFFIX}-${BOOST_OPTIM_SUFFIX}-${BOOST_VERSION}
          debug libboost_regex-vc${MSVC_SUFFIX}-${BOOST_DEBUG_SUFFIX}-${BOOST_VERSION})
	
  elseif (DARWIN)
    set(BOOST_FILESYSTEM_LIBRARY boost_filesystem-mt)
    set(BOOST_PROGRAM_OPTIONS_LIBRARY boost_program_options-mt)
    set(BOOST_REGEX_LIBRARY boost_regex-mt)
    set(BOOST_SYSTEM_LIBRARY boost_system-mt)  
  elseif (LINUX)
    set(BOOST_FILESYSTEM_LIBRARY boost_filesystem-mt)
    set(BOOST_PROGRAM_OPTIONS_LIBRARY boost_program_options-mt)
    set(BOOST_REGEX_LIBRARY boost_regex-mt)
    set(BOOST_SYSTEM_LIBRARY boost_system-mt)  
  endif (WINDOWS)
endif (STANDALONE)
