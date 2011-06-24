# -*- cmake -*-

project(basic_plugin_base)

include(00-Common)
include(LLCommon)
include(LLPlugin)
include(Linking)
include(PluginAPI)

include_directories(
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
)

### basic_plugin_base

if(NOT WORD_SIZE EQUAL 32)
	if(WINDOWS)
		add_definitions(/FIXED:NO)
	else(WINDOWS) # not windows therefore gcc LINUX and DARWIN
		add_definitions(-fPIC)
	endif(WINDOWS)
endif (NOT WORD_SIZE EQUAL 32)

set(basic_plugin_base_SOURCE_FILES
    basic_plugin_base.cpp
)

set(basic_plugin_base_HEADER_FILES
    CMakeLists.txt

    basic_plugin_base.h
)

add_library(basic_plugin_base
    ${basic_plugin_base_SOURCE_FILES}
)

add_dependencies(basic_plugin_base prepare)
