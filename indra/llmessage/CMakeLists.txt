# -*- cmake -*-

project(llmessage)

include(00-Common)
include(LLCommon)
include(LLMath)
include(LLMessage)
include(LLVFS)
include(LLAddBuildTest)
include(Tut)

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    )

set(llmessage_SOURCE_FILES
    llares.cpp
    llassetstorage.cpp
    llavatarnamecache.cpp
    llblowfishcipher.cpp
    llbuffer.cpp
    llbufferstream.cpp
    llcachename.cpp
    llchainio.cpp
    llcircuit.cpp
    llclassifiedflags.cpp
    llcurl.cpp
    lldatapacker.cpp
    lldispatcher.cpp
    llfiltersd2xmlrpc.cpp
    llhost.cpp
    llhttpassetstorage.cpp
    llhttpclient.cpp
    llhttpclientadapter.cpp
    llhttpnode.cpp
    llhttpsender.cpp
    llinstantmessage.cpp
    lliobuffer.cpp
    lliohttpserver.cpp
    lliopipe.cpp
    lliosocket.cpp
    llioutil.cpp
    llmail.cpp
    llmessagebuilder.cpp
    llmessageconfig.cpp
    llmessagelog.cpp
    llmessagereader.cpp
    llmessagetemplate.cpp
    llmessagetemplateparser.cpp
    llmessagethrottle.cpp
    llmime.cpp
    llnamevalue.cpp
    llnullcipher.cpp
    llpacketack.cpp
    llpacketbuffer.cpp
    llpacketring.cpp
    llpartdata.cpp
    llpumpio.cpp
    llregionpresenceverifier.cpp
    llsdappservices.cpp
    llsdhttpserver.cpp
    llsdmessagebuilder.cpp
    llsdmessagereader.cpp
    llsdrpcclient.cpp
    llsdrpcserver.cpp
    llservicebuilder.cpp
    llservice.cpp
    llstoredmessage.cpp
    lltemplatemessagebuilder.cpp
    lltemplatemessagedispatcher.cpp
    lltemplatemessagereader.cpp
    llthrottle.cpp
    lltransfermanager.cpp
    lltransfersourceasset.cpp
    lltransfersourcefile.cpp
    lltransfertargetfile.cpp
    lltransfertargetvfile.cpp
    lltrustedmessageservice.cpp
    llurlrequest.cpp
    lluseroperation.cpp
    llxfer.cpp
    llxfer_file.cpp
    llxfermanager.cpp
    llxfer_mem.cpp
    llxfer_vfile.cpp
    llxorcipher.cpp
    machine.cpp
    message.cpp
    message_prehash.cpp
    message_string_table.cpp
    net.cpp
    partsyspacket.cpp
    patch_code.cpp
    patch_dct.cpp
    patch_idct.cpp
    llsocks5.cpp
    sound_ids.cpp
    )

set(llmessage_HEADER_FILES
    CMakeLists.txt

    llares.h
    llassetstorage.h
    llavatarnamecache.h
    llblowfishcipher.h
    llbuffer.h
    llbufferstream.h
    llcachename.h
    llchainio.h
    llcipher.h
    llcircuit.h
    llclassifiedflags.h
    llcurl.h
    lldatapacker.h
    lldbstrings.h
    lldispatcher.h
    lleventflags.h
    llfiltersd2xmlrpc.h
    llfollowcamparams.h
    llhost.h
    llhttpassetstorage.h
    llhttpclient.h
    llhttpclientinterface.h
    llhttpclientadapter.h
    llhttpnode.h
    llhttpnodeadapter.h
    llhttpsender.h
    llinstantmessage.h
    llinvite.h
    lliobuffer.h
    lliohttpserver.h
    lliopipe.h
    lliosocket.h
    llioutil.h
    llloginflags.h
    llmail.h
    llmessagebuilder.h
    llmessageconfig.h
    llmessagelog.h
    llmessagereader.h
    llmessagetemplate.h
    llmessagetemplateparser.h
    llmessagethrottle.h
    llmime.h
    llmsgvariabletype.h
    llnamevalue.h
    llnullcipher.h
    llpacketack.h
    llpacketbuffer.h
    llpacketring.h
    llpartdata.h
    llpumpio.h
    llqueryflags.h
    llregionflags.h
    llregionhandle.h
    llregionpresenceverifier.h
    llsdappservices.h
    llsdhttpserver.h
    llsdmessagebuilder.h
    llsdmessagereader.h
    llsdrpcclient.h
    llsdrpcserver.h
    llservice.h
    llservicebuilder.h
    llstoredmessage.h
    lltaskname.h
    llteleportflags.h
    lltemplatemessagebuilder.h
    lltemplatemessagedispatcher.h
    lltemplatemessagereader.h
    llthrottle.h
    lltransfermanager.h
    lltransfersourceasset.h
    lltransfersourcefile.h
    lltransfertargetfile.h
    lltransfertargetvfile.h
    lltrustedmessageservice.h
    llurlrequest.h
    lluseroperation.h
    llvehicleparams.h
    llxfer.h
    llxfermanager.h
    llxfer_file.h
    llxfer_mem.h
    llxfer_vfile.h
    llxorcipher.h
    machine.h
    mean_collision_data.h
    message.h
    message_prehash.h
    net.h
    partsyspacket.h
    patch_code.h
    patch_dct.h
    sound_ids.h
    llsocks5.h
    )

set_source_files_properties(${llmessage_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llmessage_SOURCE_FILES ${llmessage_HEADER_FILES})

add_library (llmessage ${llmessage_SOURCE_FILES})
add_dependencies(llmessage prepare)
target_link_libraries(
    llmessage
    ${CURL_LIBRARIES}
    ${CARES_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CRYPTO_LIBRARIES}
    ${XMLRPCEPI_LIBRARIES}
    )

IF (LL_TESTS)
    # When building the viewer the tests links against the shared objects. 
    # These can not be found when we try to run the tests, so we had to disable them, for the viewer build.
    # TODO: Can someone with viewer knowledge figure out how to make these find the correct so.
    #ADD_BUILD_TEST(llhttpclientadapter llmessage)
    ADD_BUILD_TEST(llhttpclientadapter llmessage)
    ADD_BUILD_TEST(lltrustedmessageservice llmessage)
    ADD_BUILD_TEST(lltemplatemessagedispatcher llmessage)
ENDIF (LL_TESTS)

