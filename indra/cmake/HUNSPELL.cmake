# -*- cmake -*-
include(Prebuilt)

if (STANDALONE)
	include(FindHunSpell)
else (STANDALONE)
    use_prebuilt_binary(hunspell)
	
	set(HUNSPELL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/hunspell)

	if (LINUX OR DARWIN)
	    set(HUNSPELL_LIBRARY hunspell-1.2)
	else (LINUX OR DARWIN)
	    set(HUNSPELL_LIBRARY libhunspell)
	endif (LINUX OR DARWIN)
endif (STANDALONE)
