# -*- cmake -*-

project(basic_plugin_filepicker)

include(00-Common)
include(LLCommon)
include(LLPlugin)
include(Linking)
include(PluginAPI)
include(BasicPluginBase)
include(UI)
include(LLWindow)
include(LLMath)

include_directories(
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${BASIC_PLUGIN_BASE_INCLUDE_DIRS}
	${LLUI_INCLUDE_DIRS}
	${LLWINDOW_INCLUDE_DIRS}
	${LLMATH_INCLUDE_DIRS}
)

### basic_plugin_filepicker

if(NOT WORD_SIZE EQUAL 32)
	if(WINDOWS)
		add_definitions(/FIXED:NO)
	else(WINDOWS) # not windows therefore gcc LINUX and DARWIN
		add_definitions(-fPIC)
	endif(WINDOWS)
endif (NOT WORD_SIZE EQUAL 32)

set(basic_plugin_filepicker_SOURCE_FILES
	basic_plugin_filepicker.cpp
	legacy.cpp
	llfilepicker.cpp
	lldirpicker.cpp
    )

set(basic_plugin_filepicker_HEADER_FILES
	legacy.h
	llfilepicker.h
	lldirpicker.h
   )

set_source_files_properties(${basic_plugin_filepicker_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND basic_plugin_filepicker_SOURCE_FILES ${basic_plugin_filepicker_HEADER_FILES})

add_library(basic_plugin_filepicker
    SHARED
    ${basic_plugin_filepicker_SOURCE_FILES}
)

if (WINDOWS)
  set(WINDOWS_API_LIBRARIES
      ole32
	  comdlg32
      )
else (WINDOWS)
  set(WINDOWS_API_LIBRARIES "")
endif (WINDOWS)

target_link_libraries(basic_plugin_filepicker
  ${LLPLUGIN_LIBRARIES}
  ${LLCOMMON_LIBRARIES}
  ${BASIC_PLUGIN_BASE_LIBRARIES}
  ${UI_LIBRARIES}
  ${WINDOWS_API_LIBRARIES}
)

add_dependencies(basic_plugin_filepicker
  ${LLPLUGIN_LIBRARIES}
  ${LLCOMMON_LIBRARIES}
  ${BASIC_PLUGIN_BASE_LIBRARIES}
)

if (WINDOWS)
  set_target_properties(
    basic_plugin_filepicker
    PROPERTIES
    LINK_FLAGS "/MANIFEST:NO"
    )
endif (WINDOWS)

if (DARWIN)
  # Don't prepend 'lib' to the executable name, and don't embed a full path in the library's install name
  set_target_properties(
    basic_plugin_filepicker
    PROPERTIES
    PREFIX ""
    BUILD_WITH_INSTALL_RPATH 1
    INSTALL_NAME_DIR "@executable_path"
    LINK_FLAGS "-exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/../base_basic/basic_plugin_base.exp"
  )

endif (DARWIN)
