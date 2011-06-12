# -*- cmake -*-

include(Boost)

set(LLVFS_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/llvfs
    )

set(LLVFS_LIBRARIES
    llvfs
    ${BOOST_REGEX_LIBRARY}
    )
