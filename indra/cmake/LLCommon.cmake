# -*- cmake -*-

include(APR)
include(Boost)
include(EXPAT)
include(ZLIB)

set(LLCOMMON_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/cwdebug
    ${LIBS_OPEN_DIR}/llcommon
    ${APR_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    )

set(LLCOMMON_LIBRARIES llcommon)
