# -*- cmake -*-

include(BasicPluginBase)

set(MEDIA_PLUGIN_BASE_INCLUDE_DIRS ${LIBS_OPEN_DIR}/plugins/base_media ${BASIC_PLUGIN_BASE_INCLUDE_DIRS})
set(MEDIA_PLUGIN_BASE_LIBRARIES media_plugin_base ${BASIC_PLUGIN_BASE_LIBRARIES})
